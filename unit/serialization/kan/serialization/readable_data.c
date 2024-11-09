#define _CRT_SECURE_NO_WARNINGS

#include <string.h>

#include <kan/api_common/min_max.h>
#include <kan/container/dynamic_array.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/readable_data/readable_data.h>
#include <kan/reflection/field_visibility_iterator.h>
#include <kan/reflection/patch.h>
#include <kan/serialization/readable_data.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (serialization_readable_data);

enum reader_block_type_t
{
    READER_BLOCK_TYPE_STRUCT = 0u,
    READER_BLOCK_TYPE_PATCH,
    READER_BLOCK_TYPE_PATCH_SUB_STRUCT,
};

struct reader_struct_block_state_t
{
    void *instance;
    const struct kan_reflection_struct_t *type;
};

struct reader_patch_block_state_t
{
    kan_reflection_patch_t *patch_output;
    const struct kan_reflection_struct_t *type;
};

struct reader_patch_sub_struct_block_state_t
{
    kan_instance_size_t offset;
    kan_instance_size_t size_with_padding;
    const struct kan_reflection_struct_t *struct_type;
};

struct reader_block_state_t
{
    enum reader_block_type_t type;
    union
    {
        struct reader_struct_block_state_t struct_state;
        struct reader_patch_block_state_t patch_state;
        struct reader_patch_sub_struct_block_state_t patch_sub_struct_state;
    };
};

struct reader_state_t
{
    kan_readable_data_parser_t parser;
    kan_reflection_registry_t registry;
    kan_reflection_patch_builder_t patch_builder;
    kan_allocation_group_t child_allocation_group;

    /// \meta reflection_dynamic_array_type = "struct reader_block_state_t"
    struct kan_dynamic_array_t block_state_stack;
};

enum writer_block_type_t
{
    WRITER_BLOCK_TYPE_STRUCT = 0u,
    WRITER_BLOCK_TYPE_PATCH_SUB_STRUCT,
    WRITER_BLOCK_TYPE_PATCH,
};

struct writer_struct_block_state_t
{
    struct kan_reflection_visibility_iterator_t field_iterator;

    kan_instance_size_t suffix_next_index_to_write;
};

struct writer_patch_sub_struct_block_state_t
{
    const void *imaginary_instance;
    kan_instance_size_t scope_begin;
    kan_instance_size_t scope_end;

    const struct kan_reflection_field_t *current_field;
    const struct kan_reflection_field_t *end_field;

    kan_instance_size_t suffix_next_index_to_write;
};

struct writer_patch_block_state_t
{
    kan_reflection_patch_t patch;
    kan_reflection_patch_iterator_t current_iterator;
    kan_reflection_patch_iterator_t end_iterator;
};

struct writer_block_state_t
{
    enum writer_block_type_t type;
    union
    {
        struct writer_struct_block_state_t struct_state;
        struct writer_patch_sub_struct_block_state_t patch_sub_struct_state;
        struct writer_patch_block_state_t patch_state;
    };
};

struct writer_state_t
{
    kan_readable_data_emitter_t emitter;
    kan_reflection_registry_t registry;

    /// \meta reflection_dynamic_array_type = "struct writer_block_state_t"
    struct kan_dynamic_array_t block_state_stack;
};

static kan_allocation_group_t serialization_allocation_group;

static kan_interned_string_t interned_string_patch_type_field;

static kan_bool_t statics_initialized = KAN_FALSE;
static struct kan_atomic_int_t statics_initialization_lock = {.value = 0};

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        kan_atomic_int_lock (&statics_initialization_lock);
        if (!statics_initialized)
        {
            serialization_allocation_group =
                kan_allocation_group_get_child (kan_allocation_group_root (), "serialization_readable_data");

            interned_string_patch_type_field = kan_string_intern ("__type");

            statics_initialized = KAN_TRUE;
        }

        kan_atomic_int_unlock (&statics_initialization_lock);
    }
}

static inline void reader_state_push (struct reader_state_t *reader_state, struct reader_block_state_t block_state)
{
    void *spot = kan_dynamic_array_add_last (&reader_state->block_state_stack);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (&reader_state->block_state_stack,
                                        reader_state->block_state_stack.capacity * 2u);
        spot = kan_dynamic_array_add_last (&reader_state->block_state_stack);
    }

    KAN_ASSERT (spot)
    struct reader_block_state_t *output = (struct reader_block_state_t *) spot;
    output->type = block_state.type;

    switch (block_state.type)
    {
    case READER_BLOCK_TYPE_STRUCT:
        output->struct_state = block_state.struct_state;
        break;

    case READER_BLOCK_TYPE_PATCH:
        output->patch_state = block_state.patch_state;
        break;

    case READER_BLOCK_TYPE_PATCH_SUB_STRUCT:
        output->patch_sub_struct_state = block_state.patch_sub_struct_state;
        break;
    }
}

static inline void reader_state_pop (struct reader_state_t *reader_state)
{
    struct reader_block_state_t *top_state =
        &((struct reader_block_state_t *)
              reader_state->block_state_stack.data)[reader_state->block_state_stack.size - 1u];

    switch (top_state->type)
    {
    case READER_BLOCK_TYPE_STRUCT:
    case READER_BLOCK_TYPE_PATCH_SUB_STRUCT:
        break;

    case READER_BLOCK_TYPE_PATCH:
    {
        if (top_state->patch_state.type)
        {
            *top_state->patch_state.patch_output = kan_reflection_patch_builder_build (
                reader_state->patch_builder, reader_state->registry, top_state->patch_state.type);
        }
        else
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Unable to deserialize patch as it didn't specify its type through \"%s\" field.",
                     interned_string_patch_type_field)
            *top_state->patch_state.patch_output = KAN_HANDLE_SET_INVALID (kan_reflection_patch_t);
        }

        break;
    }
    }

    kan_dynamic_array_remove_swap_at (&reader_state->block_state_stack, reader_state->block_state_stack.size - 1u);
}

static inline void writer_state_push (struct writer_state_t *writer_state, struct writer_block_state_t block_state)
{
    void *spot = kan_dynamic_array_add_last (&writer_state->block_state_stack);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (&writer_state->block_state_stack,
                                        writer_state->block_state_stack.capacity * 2u);
        spot = kan_dynamic_array_add_last (&writer_state->block_state_stack);
    }

    KAN_ASSERT (spot)
    struct writer_block_state_t *output = (struct writer_block_state_t *) spot;
    output->type = block_state.type;

    switch (block_state.type)
    {
    case WRITER_BLOCK_TYPE_STRUCT:
        output->struct_state = block_state.struct_state;
        break;

    case WRITER_BLOCK_TYPE_PATCH_SUB_STRUCT:
        output->patch_sub_struct_state = block_state.patch_sub_struct_state;
        break;

    case WRITER_BLOCK_TYPE_PATCH:
        output->patch_state = block_state.patch_state;
        break;
    }
}

static inline void writer_state_pop (struct writer_state_t *writer_state)
{
    kan_dynamic_array_remove_swap_at (&writer_state->block_state_stack, writer_state->block_state_stack.size - 1u);
}

kan_serialization_rd_reader_t kan_serialization_rd_reader_create (
    struct kan_stream_t *stream,
    void *instance,
    kan_interned_string_t type_name,
    kan_reflection_registry_t reflection_registry,
    kan_allocation_group_t deserialized_string_allocation_group)
{
    ensure_statics_initialized ();
    KAN_ASSERT (kan_stream_is_readable (stream))

    struct reader_state_t *reader_state = (struct reader_state_t *) kan_allocate_general (
        serialization_allocation_group, sizeof (struct reader_state_t), _Alignof (struct reader_state_t));

    reader_state->parser = kan_readable_data_parser_create (stream);
    reader_state->registry = reflection_registry;
    reader_state->patch_builder = kan_reflection_patch_builder_create ();
    reader_state->child_allocation_group = deserialized_string_allocation_group;

    kan_dynamic_array_init (&reader_state->block_state_stack, KAN_SERIALIZATION_RD_READER_STACK_INITIAL_CAPACITY,
                            sizeof (struct reader_block_state_t), _Alignof (struct reader_block_state_t),
                            serialization_allocation_group);

    struct reader_block_state_t root_state = {
        .type = READER_BLOCK_TYPE_STRUCT,
        .struct_state =
            {
                .instance = instance,
                .type = kan_reflection_registry_query_struct (reflection_registry, type_name),
            },
    };

    KAN_ASSERT (root_state.struct_state.type)
    reader_state_push (reader_state, root_state);
    return KAN_HANDLE_SET (kan_serialization_rd_reader_t, reader_state);
}

static inline kan_bool_t extract_output_target_parts (const struct kan_readable_data_event_t *parsed_event,
                                                      size_t *output_target_parts_count,
                                                      kan_interned_string_t *output_target_parts)
{
    const char *pointer = parsed_event->output_target.identifier;
    const char *part_begin = pointer;
    *output_target_parts_count = 0u;

    while (*pointer)
    {
        if (*pointer == '.')
        {
            if (*output_target_parts_count == KAN_SERIALIZATION_RD_MAX_PARTS_IN_OUTPUT_TARGET)
            {
                return KAN_FALSE;
            }

            output_target_parts[*output_target_parts_count] = kan_char_sequence_intern (part_begin, pointer);
            ++pointer;
            part_begin = pointer;
            ++*output_target_parts_count;
        }
        else
        {
            ++pointer;
        }
    }

    if (*part_begin)
    {
        if (*output_target_parts_count == KAN_SERIALIZATION_RD_MAX_PARTS_IN_OUTPUT_TARGET)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Encountered output target with more parts (identifiers separated by dot) that allowed: \"%s\"",
                     parsed_event->output_target.identifier)
            return KAN_FALSE;
        }

        output_target_parts[*output_target_parts_count] = kan_char_sequence_intern (part_begin, pointer);
        ++*output_target_parts_count;
    }

    return *output_target_parts_count > 0u;
}

static inline kan_interned_string_t extract_struct_type_name (struct reader_block_state_t *top_state)
{
    switch (top_state->type)
    {
    case READER_BLOCK_TYPE_STRUCT:
        return top_state->struct_state.type->name;

    case READER_BLOCK_TYPE_PATCH:
        if (!top_state->patch_state.type)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Caught attempt to fill patch without specifying its type first!")
            return NULL;
        }

        return top_state->patch_state.type->name;

    case READER_BLOCK_TYPE_PATCH_SUB_STRUCT:
        return top_state->patch_sub_struct_state.struct_type->name;
    }

    KAN_ASSERT (KAN_FALSE)
    return NULL;
}

static inline kan_bool_t ensure_output_target_is_not_array_element (
    const struct kan_readable_data_event_t *parsed_event)
{
    if (parsed_event->output_target.array_index != KAN_READABLE_DATA_ARRAY_INDEX_NONE)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Elemental setter attempts to set value at path \"%s\" with index %llu, but target field is not an "
                 "array.",
                 parsed_event->output_target.identifier, (unsigned long long) parsed_event->output_target.array_index)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline kan_bool_t ensure_setter_single_value (const struct kan_readable_data_event_t *parsed_event)
{
    if (parsed_event->setter_value_first->next)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Elemental setter attempts to set multiple values at path \"%s\" (index -- %d), but target supports"
                 " only one value.",
                 parsed_event->output_target.identifier,
                 parsed_event->output_target.array_index == KAN_READABLE_DATA_ARRAY_INDEX_NONE ?
                     (int) -1 :
                     (int) parsed_event->output_target.array_index)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline void copy_parsed_string (struct reader_state_t *reader_state, const char *parsed_string, void *address)
{
    const kan_instance_size_t length = (kan_instance_size_t) strlen (parsed_string);
    char *string = kan_allocate_general (reader_state->child_allocation_group, length + 1u, _Alignof (char));
    memcpy (string, parsed_string, length);
    string[length] = '\0';
    *((char **) address) = string;
}

static inline kan_bool_t find_enum_value (const struct kan_reflection_enum_t *enum_data,
                                          const char *parsed_string,
                                          kan_reflection_enum_size_t *output)
{
    const kan_interned_string_t value_name = kan_string_intern (parsed_string);
    for (size_t index = 0u; index < enum_data->values_count; ++index)
    {
        if (enum_data->values[index].name == value_name)
        {
            *output = enum_data->values[index].value;
            return KAN_TRUE;
        }
    }

    KAN_LOG (serialization_readable_data, KAN_LOG_ERROR, "Unable to find value \"%s\" of enum \"%s\"!", value_name,
             enum_data->name)
    return KAN_FALSE;
}

static inline kan_bool_t read_to_signed_integer (struct reader_state_t *reader_state,
                                                 const struct kan_readable_data_event_t *parsed_event,
                                                 const struct kan_readable_data_value_node_t *source_node,
                                                 kan_instance_size_t integer_size,
                                                 void *address)
{
    if (parsed_event->type != KAN_READABLE_DATA_EVENT_ELEMENTAL_INTEGER_SETTER)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Elemental setter attempts to set value at path \"%s\" (index -- %llu), but it is an integer and "
                 "therefore only integer setters are allowed.",
                 parsed_event->output_target.identifier, (unsigned long long) parsed_event->output_target.array_index)
        return KAN_FALSE;
    }

    const kan_readable_data_signed_t value = source_node->integer;
    switch (integer_size)
    {
    case 1u:
        if (value < INT8_MIN || value > INT8_MAX)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Elemental setter attempts to set value at path \"%s\" (index -- %llu), but receiver is 8 bit "
                     "integer and cannot hold value %lld.",
                     parsed_event->output_target.identifier, (long long) parsed_event->output_target.array_index,
                     (long long) value)
            return KAN_FALSE;
        }

        *((int8_t *) address) = (int8_t) value;
        return KAN_TRUE;

    case 2u:
        if (value < INT16_MIN || value > INT16_MAX)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Elemental setter attempts to set value at path \"%s\" (index -- %llu), but receiver is 16 bit "
                     "integer and cannot hold value %lld.",
                     parsed_event->output_target.identifier, (long long) parsed_event->output_target.array_index,
                     (long long) value)
            return KAN_FALSE;
        }

        *((int16_t *) address) = (int16_t) value;
        return KAN_TRUE;

    case 4u:
        if (value < INT32_MIN || value > INT32_MAX)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Elemental setter attempts to set value at path \"%s\" (index -- %llu), but receiver is 32 bit "
                     "integer and cannot hold value %lld.",
                     parsed_event->output_target.identifier, (long long) parsed_event->output_target.array_index,
                     (long long) value)
            return KAN_FALSE;
        }

        *((int32_t *) address) = (int32_t) value;
        return KAN_TRUE;

    case 8u:
        *((int64_t *) address) = value;
        return KAN_TRUE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t read_to_unsigned_integer (struct reader_state_t *reader_state,
                                                   const struct kan_readable_data_event_t *parsed_event,
                                                   const struct kan_readable_data_value_node_t *source_node,
                                                   kan_instance_size_t integer_size,
                                                   void *address)
{
    if (parsed_event->type != KAN_READABLE_DATA_EVENT_ELEMENTAL_INTEGER_SETTER)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Elemental setter attempts to set value at path \"%s\" (index -- %llu), but it is an integer and "
                 "therefore only integer setters are allowed.",
                 parsed_event->output_target.identifier, (unsigned long long) parsed_event->output_target.array_index)
        return KAN_FALSE;
    }

    const kan_readable_data_signed_t value = source_node->integer;
    if (value < 0)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Elemental setter attempts to set value at path \"%s\" (index -- %llu), but receiver is an unsigned "
                 "integer and cannot hold value %lld.",
                 parsed_event->output_target.identifier, (long long) parsed_event->output_target.array_index,
                 (long long) value)
        return KAN_FALSE;
    }

    switch (integer_size)
    {
    case 1u:
        if (value > UINT8_MAX)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Elemental setter attempts to set value at path \"%s\" (index -- %llu), but receiver is 8 bit "
                     "unsigned integer and cannot hold value %lld.",
                     parsed_event->output_target.identifier, (long long) parsed_event->output_target.array_index,
                     (long long) value)
            return KAN_FALSE;
        }

        *((uint8_t *) address) = (uint8_t) value;
        return KAN_TRUE;

    case 2u:
        if (value > INT16_MAX)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Elemental setter attempts to set value at path \"%s\" (index -- %llu), but receiver is 16 bit "
                     "unsigned integer and cannot hold value %lld.",
                     parsed_event->output_target.identifier, (long long) parsed_event->output_target.array_index,
                     (long long) value)
            return KAN_FALSE;
        }

        *((uint16_t *) address) = (uint16_t) value;
        return KAN_TRUE;

    case 4u:
        if ((uint32_t) value > UINT32_MAX)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Elemental setter attempts to set value at path \"%s\" (index -- %llu), but receiver is 32 bit "
                     "unsigned integer and cannot hold value %lld.",
                     parsed_event->output_target.identifier, (long long) parsed_event->output_target.array_index,
                     (long long) value)
            return KAN_FALSE;
        }

        *((uint32_t *) address) = (uint32_t) value;
        return KAN_TRUE;

    case 8u:
        *((uint64_t *) address) = (uint64_t) value;
        return KAN_TRUE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t read_to_floating (struct reader_state_t *reader_state,
                                           const struct kan_readable_data_event_t *parsed_event,
                                           const struct kan_readable_data_value_node_t *source_node,
                                           kan_instance_size_t integer_size,
                                           void *address)
{
    if (parsed_event->type != KAN_READABLE_DATA_EVENT_ELEMENTAL_FLOATING_SETTER)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Elemental setter attempts to set value at path \"%s\" (index -- %llu), but it is a floating number "
                 "and  therefore only floating setters are allowed.",
                 parsed_event->output_target.identifier, (unsigned long long) parsed_event->output_target.array_index)
        return KAN_FALSE;
    }

    const kan_readable_data_floating_t value = source_node->floating;
    switch (integer_size)
    {
    case 4u:
        *((float *) address) = (float) value;
        return KAN_TRUE;

    case 8u:
        *((double *) address) = value;
        return KAN_TRUE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t read_to_string (struct reader_state_t *reader_state,
                                         const struct kan_readable_data_event_t *parsed_event,
                                         const struct kan_readable_data_value_node_t *source_node,
                                         void *address)
{
    if (parsed_event->type == KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER)
    {
        copy_parsed_string (reader_state, source_node->identifier, address);
    }
    else if (parsed_event->type == KAN_READABLE_DATA_EVENT_ELEMENTAL_STRING_SETTER)
    {
        copy_parsed_string (reader_state, source_node->string, address);
    }
    else
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Elemental setter attempts to set value at path \"%s\" (index -- %llu), but it is a string and "
                 "therefore only string or identifier setters are allowed.",
                 parsed_event->output_target.identifier, (unsigned long long) parsed_event->output_target.array_index)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline kan_bool_t read_to_interned_string (struct reader_state_t *reader_state,
                                                  const struct kan_readable_data_event_t *parsed_event,
                                                  const struct kan_readable_data_value_node_t *source_node,
                                                  void *address)
{
    if (parsed_event->type == KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER)
    {
        *((kan_interned_string_t *) address) = kan_string_intern (source_node->identifier);
    }
    else if (parsed_event->type == KAN_READABLE_DATA_EVENT_ELEMENTAL_STRING_SETTER)
    {
        *((kan_interned_string_t *) address) = kan_string_intern (source_node->string);
    }
    else
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Elemental setter attempts to set value at path \"%s\" (index -- %llu), but it is an interned string "
                 "and therefore only string or identifier setters are allowed.",
                 parsed_event->output_target.identifier, (unsigned long long) parsed_event->output_target.array_index)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline kan_bool_t read_to_enum (struct reader_state_t *reader_state,
                                       const struct kan_readable_data_event_t *parsed_event,
                                       const struct kan_readable_data_value_node_t *source_node,
                                       kan_bool_t reading_packed_array_setter,
                                       kan_interned_string_t enum_name,
                                       void *address)
{
    if (parsed_event->type != KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Elemental setter attempts to set value at path \"%s\" (index -- %llu), but it is an enum and "
                 "therefore only identifier setters are allowed.",
                 parsed_event->output_target.identifier, (unsigned long long) parsed_event->output_target.array_index)
        return KAN_FALSE;
    }

    const struct kan_reflection_enum_t *enum_data =
        kan_reflection_registry_query_enum (reader_state->registry, enum_name);
    KAN_ASSERT (enum_data)

    if (enum_data->flags)
    {
        if (reading_packed_array_setter)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Elemental setter attempts to set value at path \"%s\" (index -- %llu), but it is an array of "
                     "bitflags and setting array of bitflags through one setter is not supported by syntax.",
                     parsed_event->output_target.identifier,
                     (unsigned long long) parsed_event->output_target.array_index)
            return KAN_FALSE;
        }

        KAN_ASSERT (source_node == parsed_event->setter_value_first)
        unsigned int *flags = (unsigned int *) address;
        *flags = 0u;
        struct kan_readable_data_value_node_t *node = parsed_event->setter_value_first;

        while (node)
        {
            kan_reflection_enum_size_t value_enum;
            if (!find_enum_value (enum_data, node->identifier, &value_enum))
            {
                return KAN_FALSE;
            }

            *flags |= (unsigned int) value_enum;
            node = node->next;
        }
    }
    else
    {
        if (!reading_packed_array_setter && !ensure_setter_single_value (parsed_event))
        {
            return KAN_FALSE;
        }

        int *value = (int *) address;
        kan_reflection_enum_size_t value_enum;

        if (!find_enum_value (enum_data, source_node->identifier, &value_enum))
        {
            return KAN_FALSE;
        }

        *value = (int) value_enum;
    }

    return KAN_TRUE;
}

static inline kan_loop_size_t calculate_values_count (const struct kan_readable_data_event_t *parsed_event)
{
    kan_loop_size_t count = 0u;
    const struct kan_readable_data_value_node_t *node = parsed_event->setter_value_first;

    while (node)
    {
        ++count;
        node = node->next;
    }

    return count;
}

static inline kan_bool_t read_elemental_setter_into_array_element (struct reader_state_t *reader_state,
                                                                   const struct kan_readable_data_event_t *parsed_event,
                                                                   const struct kan_reflection_field_t *array_field,
                                                                   void *array_address)
{
    KAN_ASSERT (array_field->archetype == KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY ||
                array_field->archetype == KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY)

    const enum kan_reflection_archetype_t item_archetype =
        array_field->archetype == KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY ?
            array_field->archetype_inline_array.item_archetype :
            array_field->archetype_dynamic_array.item_archetype;

    const kan_instance_size_t item_size = array_field->archetype == KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY ?
                                              array_field->archetype_inline_array.item_size :
                                              array_field->archetype_dynamic_array.item_size;

    switch (item_archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        return ensure_setter_single_value (parsed_event) &&
               read_to_signed_integer (reader_state, parsed_event, parsed_event->setter_value_first, item_size,
                                       array_address);

    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        return ensure_setter_single_value (parsed_event) &&
               read_to_unsigned_integer (reader_state, parsed_event, parsed_event->setter_value_first, item_size,
                                         array_address);

    case KAN_REFLECTION_ARCHETYPE_FLOATING:
        return ensure_setter_single_value (parsed_event) &&
               read_to_floating (reader_state, parsed_event, parsed_event->setter_value_first, item_size,
                                 array_address);

    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        return ensure_setter_single_value (parsed_event) &&
               read_to_string (reader_state, parsed_event, parsed_event->setter_value_first, array_address);

    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        return ensure_setter_single_value (parsed_event) &&
               read_to_interned_string (reader_state, parsed_event, parsed_event->setter_value_first, array_address);

    case KAN_REFLECTION_ARCHETYPE_ENUM:
    {
        const kan_interned_string_t enum_name = array_field->archetype == KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY ?
                                                    array_field->archetype_inline_array.item_archetype_enum.type_name :
                                                    array_field->archetype_dynamic_array.item_archetype_enum.type_name;

        return read_to_enum (reader_state, parsed_event, parsed_event->setter_value_first, KAN_FALSE, enum_name,
                             array_address);
    }

    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        KAN_ASSERT (KAN_FALSE)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
    case KAN_REFLECTION_ARCHETYPE_STRUCT:
    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
    case KAN_REFLECTION_ARCHETYPE_PATCH:
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Elemental setter attempts to set value at path \"%s[%llu]\" where target archetype does not supports "
                 "elemental setters.",
                 parsed_event->output_target.identifier, (unsigned long long) parsed_event->output_target.array_index)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t read_elemental_setter_node_into_packed_array (
    struct reader_state_t *reader_state,
    const struct kan_readable_data_event_t *parsed_event,
    struct kan_readable_data_value_node_t *node,
    enum kan_reflection_archetype_t item_archetype,
    kan_instance_size_t item_size,
    uint8_t *output,
    kan_interned_string_t enum_type)
{
    switch (item_archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        return read_to_signed_integer (reader_state, parsed_event, node, item_size, output);

    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        return read_to_unsigned_integer (reader_state, parsed_event, node, item_size, output);

    case KAN_REFLECTION_ARCHETYPE_FLOATING:
        return read_to_floating (reader_state, parsed_event, node, item_size, output);

    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        return read_to_string (reader_state, parsed_event, node, output);

    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        return read_to_interned_string (reader_state, parsed_event, node, output);

    case KAN_REFLECTION_ARCHETYPE_ENUM:
    {
        return read_to_enum (reader_state, parsed_event, node, KAN_TRUE, enum_type, output);
    }

    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        KAN_ASSERT (KAN_FALSE)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
    case KAN_REFLECTION_ARCHETYPE_STRUCT:
    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
    case KAN_REFLECTION_ARCHETYPE_PATCH:
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Elemental setter attempts to set value at path \"%s[%llu]\" where target archetype does not supports "
                 "elemental setters.",
                 parsed_event->output_target.identifier, (unsigned long long) parsed_event->output_target.array_index)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t read_patch_type (struct reader_state_t *reader_state,
                                          const struct kan_readable_data_event_t *parsed_event,
                                          struct reader_block_state_t *top_state)
{
    if (parsed_event->type != KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR, "Patch type setter must always be identifier setter.")
        return KAN_FALSE;
    }

    if (parsed_event->setter_value_first->next)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR, "Patch type cannot have more than one value!")
        return KAN_FALSE;
    }

    if (top_state->patch_state.type)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR, "Caught attempt to set patch type more than once!")
        return KAN_FALSE;
    }

    top_state->patch_state.type = kan_reflection_registry_query_struct (
        reader_state->registry, kan_string_intern (parsed_event->setter_value_first->identifier));

    if (!top_state->patch_state.type)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR, "Unable to find patch type \"%s\"!",
                 parsed_event->setter_value_first->identifier)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline void elemental_setter_item_post_read (struct reader_state_t *reader_state,
                                                    struct reader_block_state_t *top_state,
                                                    kan_instance_size_t absolute_offset,
                                                    kan_instance_size_t size_with_padding,
                                                    void *address)
{
    switch (top_state->type)
    {
    case READER_BLOCK_TYPE_STRUCT:
        break;

    case READER_BLOCK_TYPE_PATCH:
        kan_reflection_patch_builder_add_chunk (reader_state->patch_builder, absolute_offset, size_with_padding,
                                                address);
        break;

    case READER_BLOCK_TYPE_PATCH_SUB_STRUCT:
    {
        kan_instance_size_t adjusted_offset = absolute_offset;
        kan_instance_size_t adjusted_size = size_with_padding;

        if (adjusted_offset + adjusted_size == top_state->patch_sub_struct_state.struct_type->size)
        {
            adjusted_size = top_state->patch_sub_struct_state.size_with_padding - adjusted_offset;
        }

        adjusted_offset += top_state->patch_sub_struct_state.offset;
        kan_reflection_patch_builder_add_chunk (reader_state->patch_builder, adjusted_offset, adjusted_size, address);
        break;
    }
    }
}

static inline kan_bool_t read_elemental_setter (struct reader_state_t *reader_state,
                                                const struct kan_readable_data_event_t *parsed_event,
                                                struct reader_block_state_t *top_state)
{
    size_t output_target_parts_count = 0u;
    kan_interned_string_t output_target_parts[KAN_SERIALIZATION_RD_MAX_PARTS_IN_OUTPUT_TARGET];

    if (!extract_output_target_parts (parsed_event, &output_target_parts_count, output_target_parts))
    {
        return KAN_FALSE;
    }

    if (output_target_parts_count == 1u && output_target_parts[0u] == interned_string_patch_type_field &&
        parsed_event->output_target.array_index == KAN_READABLE_DATA_ARRAY_INDEX_NONE &&
        top_state->type == READER_BLOCK_TYPE_PATCH)
    {
        return read_patch_type (reader_state, parsed_event, top_state);
    }

    const kan_interned_string_t struct_type_name = extract_struct_type_name (top_state);
    if (!struct_type_name)
    {
        return KAN_FALSE;
    }

    kan_instance_size_t absolute_offset;
    kan_instance_size_t size_with_padding;

    const struct kan_reflection_field_t *field =
        kan_reflection_registry_query_local_field (reader_state->registry, struct_type_name, output_target_parts_count,
                                                   output_target_parts, &absolute_offset, &size_with_padding);

    if (!field)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Elemental setter attempts to set value at path \"%s\", but there is no field at given path.",
                 parsed_event->output_target.identifier)
        return KAN_FALSE;
    }

    uint64_t patch_single_value_read_buffer = 0u;
    void *address = NULL;

    switch (top_state->type)
    {
    case READER_BLOCK_TYPE_STRUCT:
        address = ((uint8_t *) top_state->struct_state.instance) + absolute_offset;
        break;

    case READER_BLOCK_TYPE_PATCH:
    case READER_BLOCK_TYPE_PATCH_SUB_STRUCT:
        address = &patch_single_value_read_buffer;
        break;
    }

    switch (field->archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        if (!ensure_setter_single_value (parsed_event) || !ensure_output_target_is_not_array_element (parsed_event) ||
            !read_to_signed_integer (reader_state, parsed_event, parsed_event->setter_value_first, field->size,
                                     address))
        {
            return KAN_FALSE;
        }

        elemental_setter_item_post_read (reader_state, top_state, absolute_offset, size_with_padding, address);
        return KAN_TRUE;

    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        if (!ensure_setter_single_value (parsed_event) || !ensure_output_target_is_not_array_element (parsed_event) ||
            !read_to_unsigned_integer (reader_state, parsed_event, parsed_event->setter_value_first, field->size,
                                       address))
        {
            return KAN_FALSE;
        }

        elemental_setter_item_post_read (reader_state, top_state, absolute_offset, size_with_padding, address);
        return KAN_TRUE;

    case KAN_REFLECTION_ARCHETYPE_FLOATING:
        if (!ensure_setter_single_value (parsed_event) || !ensure_output_target_is_not_array_element (parsed_event) ||
            !read_to_floating (reader_state, parsed_event, parsed_event->setter_value_first, field->size, address))
        {
            return KAN_FALSE;
        }

        elemental_setter_item_post_read (reader_state, top_state, absolute_offset, size_with_padding, address);
        return KAN_TRUE;

    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        if (top_state->type == READER_BLOCK_TYPE_PATCH || top_state->type == READER_BLOCK_TYPE_PATCH_SUB_STRUCT)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Elemental setter attempts to set value at path \"%s\" which is string, but setting strings is not"
                     " supported for patches.",
                     parsed_event->output_target.identifier)
            return KAN_FALSE;
        }

        return ensure_output_target_is_not_array_element (parsed_event) && ensure_setter_single_value (parsed_event) &&
               read_to_string (reader_state, parsed_event, parsed_event->setter_value_first, address);

    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        if (!ensure_output_target_is_not_array_element (parsed_event) || !ensure_setter_single_value (parsed_event) ||
            !read_to_interned_string (reader_state, parsed_event, parsed_event->setter_value_first, address))
        {
            return KAN_FALSE;
        }

        elemental_setter_item_post_read (reader_state, top_state, absolute_offset, size_with_padding, address);
        return KAN_TRUE;

    case KAN_REFLECTION_ARCHETYPE_ENUM:
        if (!ensure_output_target_is_not_array_element (parsed_event) ||
            !read_to_enum (reader_state, parsed_event, parsed_event->setter_value_first, KAN_FALSE,
                           field->archetype_enum.type_name, address))
        {
            return KAN_FALSE;
        }

        elemental_setter_item_post_read (reader_state, top_state, absolute_offset, size_with_padding, address);
        return KAN_TRUE;

    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        if (parsed_event->output_target.array_index == KAN_READABLE_DATA_ARRAY_INDEX_NONE)
        {
            const kan_instance_size_t count = calculate_values_count (parsed_event);
            if (count > field->archetype_inline_array.item_count)
            {
                KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                         "Elemental setter attempts to set value at path \"%s\", but provides %llu values which is "
                         "greater than inline array size %llu.",
                         parsed_event->output_target.identifier, (unsigned long long) count,
                         (unsigned long long) field->archetype_inline_array.item_count)
                return KAN_FALSE;
            }

            struct kan_readable_data_value_node_t *node = parsed_event->setter_value_first;
            kan_loop_size_t index = 0u;

            while (node)
            {
                void *output_address = NULL;
                switch (top_state->type)
                {
                case READER_BLOCK_TYPE_STRUCT:
                    output_address = ((uint8_t *) address) + field->archetype_inline_array.item_size * index;
                    break;
                case READER_BLOCK_TYPE_PATCH:
                case READER_BLOCK_TYPE_PATCH_SUB_STRUCT:
                    output_address = address;
                    break;
                }

                if (!read_elemental_setter_node_into_packed_array (
                        reader_state, parsed_event, node, field->archetype_inline_array.item_archetype,
                        field->archetype_inline_array.item_size, output_address,
                        field->archetype_inline_array.item_archetype_enum.type_name))
                {
                    return KAN_FALSE;
                }

                kan_instance_size_t item_offset = absolute_offset + field->archetype_inline_array.item_size * index;
                kan_instance_size_t item_size_with_padding_adjustment;

                if (index == field->archetype_inline_array.item_count - 1u)
                {
                    item_size_with_padding_adjustment =
                        size_with_padding - field->archetype_inline_array.item_size * index;
                }
                else
                {
                    item_size_with_padding_adjustment = field->archetype_inline_array.item_size;
                }

                elemental_setter_item_post_read (reader_state, top_state, item_offset,
                                                 item_size_with_padding_adjustment, output_address);

                ++index;
                node = node->next;
            }

            return KAN_TRUE;
        }
        else
        {
            if (parsed_event->output_target.array_index >= field->archetype_inline_array.item_count)
            {
                KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                         "Elemental setter attempts to set value at path \"%s[%llu]\", but given index is greater than "
                         "inline array size %llu.",
                         parsed_event->output_target.identifier,
                         (unsigned long long) parsed_event->output_target.array_index,
                         (unsigned long long) field->archetype_inline_array.item_count)
                return KAN_FALSE;
            }

            void *output_address = NULL;
            switch (top_state->type)
            {
            case READER_BLOCK_TYPE_STRUCT:
                output_address = ((uint8_t *) address) +
                                 field->archetype_inline_array.item_size * parsed_event->output_target.array_index;
                break;
            case READER_BLOCK_TYPE_PATCH:
            case READER_BLOCK_TYPE_PATCH_SUB_STRUCT:
                output_address = address;
                break;
            }

            if (!read_elemental_setter_into_array_element (reader_state, parsed_event, field, output_address))
            {
                return KAN_FALSE;
            }

            kan_instance_size_t item_offset =
                absolute_offset + field->archetype_inline_array.item_size * parsed_event->output_target.array_index;
            kan_instance_size_t item_size_with_padding_adjustment;

            if (parsed_event->output_target.array_index == field->archetype_inline_array.item_count - 1u)
            {
                item_size_with_padding_adjustment = size_with_padding - field->archetype_inline_array.item_size *
                                                                            parsed_event->output_target.array_index;
            }
            else
            {
                item_size_with_padding_adjustment = field->archetype_inline_array.item_size;
            }

            elemental_setter_item_post_read (reader_state, top_state, item_offset, item_size_with_padding_adjustment,
                                             output_address);
            return KAN_TRUE;
        }

    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        if (top_state->type == READER_BLOCK_TYPE_PATCH || top_state->type == READER_BLOCK_TYPE_PATCH_SUB_STRUCT)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Elemental setter attempts to set value at path \"%s\" which is a dynamic array, but setting "
                     "values of dynamic arrays is not supported for patches.",
                     parsed_event->output_target.identifier)
            return KAN_FALSE;
        }

        if (parsed_event->output_target.array_index == KAN_READABLE_DATA_ARRAY_INDEX_NONE)
        {
            const kan_loop_size_t count = calculate_values_count (parsed_event);
            struct kan_dynamic_array_t *dynamic_array = (struct kan_dynamic_array_t *) address;

            if (dynamic_array->size < count)
            {
                if (dynamic_array->capacity < count)
                {
                    kan_dynamic_array_set_capacity (dynamic_array, count);
                }

                dynamic_array->size = count;
            }

            struct kan_readable_data_value_node_t *node = parsed_event->setter_value_first;
            uint8_t *output = dynamic_array->data;

            while (node)
            {
                if (!read_elemental_setter_node_into_packed_array (
                        reader_state, parsed_event, node, field->archetype_dynamic_array.item_archetype,
                        dynamic_array->item_size, output, field->archetype_dynamic_array.item_archetype_enum.type_name))
                {
                    return KAN_FALSE;
                }

                output += dynamic_array->item_size;
                node = node->next;
            }

            return KAN_TRUE;
        }
        else
        {
            struct kan_dynamic_array_t *dynamic_array = (struct kan_dynamic_array_t *) address;
            if (dynamic_array->size <= parsed_event->output_target.array_index)
            {
                if (dynamic_array->capacity <= parsed_event->output_target.array_index)
                {
                    kan_dynamic_array_set_capacity (dynamic_array, parsed_event->output_target.array_index + 1u);
                }

                dynamic_array->size = parsed_event->output_target.array_index + 1u;
            }

            void *array_address =
                dynamic_array->data + dynamic_array->item_size * parsed_event->output_target.array_index;

            return read_elemental_setter_into_array_element (reader_state, parsed_event, field, array_address);
        }

    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
    case KAN_REFLECTION_ARCHETYPE_STRUCT:
    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
    case KAN_REFLECTION_ARCHETYPE_PATCH:
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Elemental setter attempts to set value at path \"%s\" where target archetype does not supports "
                 "elemental setters.",
                 parsed_event->output_target.identifier)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t structural_setter_open_struct (struct reader_state_t *reader_state,
                                                        struct reader_block_state_t *top_state,
                                                        kan_instance_size_t absolute_offset,
                                                        kan_instance_size_t size_with_padding,
                                                        kan_interned_string_t type_name)
{
    switch (top_state->type)
    {
    case READER_BLOCK_TYPE_STRUCT:
    {
        struct reader_block_state_t new_state = {
            .type = READER_BLOCK_TYPE_STRUCT,
            .struct_state =
                {
                    .instance = ((uint8_t *) top_state->struct_state.instance) + absolute_offset,
                    .type = kan_reflection_registry_query_struct (reader_state->registry, type_name),
                },
        };

        KAN_ASSERT (new_state.struct_state.type)
        if (new_state.struct_state.type->init)
        {
            new_state.struct_state.type->init (new_state.struct_state.type->functor_user_data,
                                               new_state.struct_state.instance);
        }

        reader_state_push (reader_state, new_state);
        return KAN_TRUE;
    }

    case READER_BLOCK_TYPE_PATCH:
    {
        struct reader_block_state_t new_state = {
            .type = READER_BLOCK_TYPE_PATCH_SUB_STRUCT,
            .patch_sub_struct_state =
                {
                    .offset = absolute_offset,
                    .size_with_padding = size_with_padding,
                    .struct_type = kan_reflection_registry_query_struct (reader_state->registry, type_name),
                },
        };

        reader_state_push (reader_state, new_state);
        return KAN_TRUE;
    }

    case READER_BLOCK_TYPE_PATCH_SUB_STRUCT:
    {
        struct reader_block_state_t new_state = {
            .type = READER_BLOCK_TYPE_PATCH_SUB_STRUCT,
            .patch_sub_struct_state =
                {
                    .offset = top_state->patch_sub_struct_state.offset + absolute_offset,
                    .size_with_padding = size_with_padding,
                    .struct_type = kan_reflection_registry_query_struct (reader_state->registry, type_name),
                },
        };

        reader_state_push (reader_state, new_state);
        return KAN_TRUE;
    }
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t read_structural_setter (struct reader_state_t *reader_state,
                                                 const struct kan_readable_data_event_t *parsed_event,
                                                 struct reader_block_state_t *top_state)
{
    size_t output_target_parts_count = 0u;
    kan_interned_string_t output_target_parts[KAN_SERIALIZATION_RD_MAX_PARTS_IN_OUTPUT_TARGET];

    if (!extract_output_target_parts (parsed_event, &output_target_parts_count, output_target_parts))
    {
        return KAN_FALSE;
    }

    const kan_interned_string_t struct_type_name = extract_struct_type_name (top_state);
    if (!struct_type_name)
    {
        return KAN_FALSE;
    }

    kan_instance_size_t absolute_offset;
    kan_instance_size_t size_with_padding;

    const struct kan_reflection_field_t *field =
        kan_reflection_registry_query_local_field (reader_state->registry, struct_type_name, output_target_parts_count,
                                                   output_target_parts, &absolute_offset, &size_with_padding);

    if (!field)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Structural setter attempts to set value at path \"%s\", but there is no field at given path.",
                 parsed_event->output_target.identifier)
        return KAN_FALSE;
    }

    switch (field->archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_FLOATING:
    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
    case KAN_REFLECTION_ARCHETYPE_ENUM:
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Structural setter attempts to set value at path \"%s\", but field has elemental archetype.",
                 parsed_event->output_target.identifier)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Structural setter attempts to set value at path \"%s\" which holds external pointer, but external "
                 "pointers are not supported.",
                 parsed_event->output_target.identifier)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_STRUCT:
        if (parsed_event->output_target.array_index != KAN_READABLE_DATA_ARRAY_INDEX_NONE)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Structural setter attempts to set value at path \"%s[%llu]\" which holds struct, but structs "
                     "can not be indexed.",
                     parsed_event->output_target.identifier,
                     (unsigned long long) parsed_event->output_target.array_index)
            return KAN_FALSE;
        }

        return structural_setter_open_struct (reader_state, top_state, absolute_offset, size_with_padding,
                                              field->archetype_struct.type_name);

    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Structural setter attempts to set value at path \"%s\" which holds struct pointer, but struct "
                 "pointers are not supported.",
                 parsed_event->output_target.identifier)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        if (parsed_event->output_target.array_index == KAN_READABLE_DATA_ARRAY_INDEX_NONE)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Structural setter attempts to set value at path \"%s\" which holds inline array, but "
                     "inline arrays must be indexed.",
                     parsed_event->output_target.identifier)
            return KAN_FALSE;
        }

        if (parsed_event->output_target.array_index >= field->archetype_inline_array.item_count)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Structural setter attempts to set value at path \"%s[%llu]\" which holds inline array, but "
                     "inline array size is only %llu",
                     parsed_event->output_target.identifier,
                     (unsigned long long) parsed_event->output_target.array_index,
                     (unsigned long long) field->archetype_inline_array.item_count)
            return KAN_FALSE;
        }

        switch (field->archetype_inline_array.item_archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Structural setter attempts to set value at path \"%s[%llu]\", but field has elemental archetype.",
                     parsed_event->output_target.identifier,
                     (unsigned long long) parsed_event->output_target.array_index)
            return KAN_FALSE;

        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Structural setter attempts to set value at path \"%s[%llu]\" which holds external pointer, but "
                     "external pointers are not supported.",
                     parsed_event->output_target.identifier,
                     (unsigned long long) parsed_event->output_target.array_index)
            return KAN_FALSE;

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
        {
            const kan_instance_size_t item_offset =
                field->archetype_inline_array.item_size * parsed_event->output_target.array_index;

            const kan_instance_size_t item_size_with_padding =
                parsed_event->output_target.array_index == field->archetype_inline_array.item_size - 1u ?
                    size_with_padding - item_offset :
                    field->archetype_inline_array.item_size;

            return structural_setter_open_struct (reader_state, top_state, absolute_offset + item_offset,
                                                  item_size_with_padding, field->archetype_struct.type_name);
        }

        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Structural setter attempts to set value at path \"%s[%llu]\" which holds struct pointer, but "
                     "struct pointers are not supported.",
                     parsed_event->output_target.identifier,
                     (unsigned long long) parsed_event->output_target.array_index)
            return KAN_FALSE;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            KAN_ASSERT (KAN_FALSE)
            return KAN_FALSE;

        case KAN_REFLECTION_ARCHETYPE_PATCH:
        {
            if (top_state->type != READER_BLOCK_TYPE_STRUCT)
            {
                KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                         "Structural setter attempts to set value at path \"%s[%llu]\" which holds patch, but "
                         "patches can not be stored inside other patches.",
                         parsed_event->output_target.identifier,
                         (unsigned long long) parsed_event->output_target.array_index)
                return KAN_FALSE;
            }

            struct reader_block_state_t new_state = {
                .type = READER_BLOCK_TYPE_PATCH,
                .patch_state = {
                    .patch_output =
                        (kan_reflection_patch_t *) (((uint8_t *) top_state->struct_state.instance) + absolute_offset +
                                                    sizeof (kan_reflection_patch_t) *
                                                        parsed_event->output_target.array_index),
                    .type = NULL,
                }};

            reader_state_push (reader_state, new_state);
            return KAN_TRUE;
        }
        }

        break;

    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
    {
        if (parsed_event->output_target.array_index == KAN_READABLE_DATA_ARRAY_INDEX_NONE)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Structural setter attempts to set value at path \"%s\" which holds dynamic array, but "
                     "dynamic arrays must be indexed.",
                     parsed_event->output_target.identifier)
            return KAN_FALSE;
        }

        if (top_state->type != READER_BLOCK_TYPE_STRUCT)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Structural setter attempts to set value at path \"%s[%llu]\" which holds dynamic array, but "
                     "dynamic arrays can only receive values when not inside patch",
                     parsed_event->output_target.identifier,
                     (unsigned long long) parsed_event->output_target.array_index)
            return KAN_FALSE;
        }

        struct kan_dynamic_array_t *dynamic_array =
            (struct kan_dynamic_array_t *) (((uint8_t *) top_state->struct_state.instance) + absolute_offset);

        if (dynamic_array->capacity < parsed_event->output_target.array_index + 1u)
        {
            kan_dynamic_array_set_capacity (dynamic_array, (parsed_event->output_target.array_index + 1u) * 2u);
        }

        if (dynamic_array->size < parsed_event->output_target.array_index + 1u)
        {
            dynamic_array->size = parsed_event->output_target.array_index + 1u;
        }

        void *array_address =
            ((uint8_t *) dynamic_array->data) + dynamic_array->item_size * parsed_event->output_target.array_index;

        switch (field->archetype_dynamic_array.item_archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Structural setter attempts to set value at path \"%s[%llu]\", but field has elemental archetype.",
                     parsed_event->output_target.identifier,
                     (unsigned long long) parsed_event->output_target.array_index)
            return KAN_FALSE;

        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Structural setter attempts to set value at path \"%s[%llu]\" which holds external pointer, but "
                     "external pointers are not supported.",
                     parsed_event->output_target.identifier,
                     (unsigned long long) parsed_event->output_target.array_index)
            return KAN_FALSE;

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
        {
            struct reader_block_state_t new_state = {
                .type = READER_BLOCK_TYPE_STRUCT,
                .struct_state =
                    {
                        .instance = array_address,
                        .type = kan_reflection_registry_query_struct (
                            reader_state->registry, field->archetype_dynamic_array.item_archetype_struct.type_name),
                    },
            };

            KAN_ASSERT (new_state.struct_state.type)
            if (new_state.struct_state.type->init)
            {
                new_state.struct_state.type->init (new_state.struct_state.type->functor_user_data, array_address);
            }

            reader_state_push (reader_state, new_state);
            return KAN_TRUE;
        }

        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Structural setter attempts to set value at path \"%s[%llu]\" which holds struct pointer, but "
                     "struct pointers are not supported.",
                     parsed_event->output_target.identifier,
                     (unsigned long long) parsed_event->output_target.array_index)
            return KAN_FALSE;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            KAN_ASSERT (KAN_FALSE)
            return KAN_FALSE;

        case KAN_REFLECTION_ARCHETYPE_PATCH:
        {
            struct reader_block_state_t new_state = {.type = READER_BLOCK_TYPE_PATCH,
                                                     .patch_state = {
                                                         .patch_output = array_address,
                                                         .type = NULL,
                                                     }};

            reader_state_push (reader_state, new_state);
            return KAN_TRUE;
        }
        }
    }

    case KAN_REFLECTION_ARCHETYPE_PATCH:
    {
        if (parsed_event->output_target.array_index != KAN_READABLE_DATA_ARRAY_INDEX_NONE)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Structural setter attempts to set value at path \"%s[%llu]\" which holds patch, but patches "
                     "can not be indexed.",
                     parsed_event->output_target.identifier,
                     (unsigned long long) parsed_event->output_target.array_index)
            return KAN_FALSE;
        }

        if (top_state->type != READER_BLOCK_TYPE_STRUCT)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                     "Structural setter attempts to set value at path \"%s\" which holds patch, but "
                     "patches can not be stored inside other patches.",
                     parsed_event->output_target.identifier)
            return KAN_FALSE;
        }

        struct reader_block_state_t new_state = {
            .type = READER_BLOCK_TYPE_PATCH,
            .patch_state = {
                .patch_output =
                    (kan_reflection_patch_t *) (((uint8_t *) top_state->struct_state.instance) + absolute_offset),
                .type = NULL,
            }};

        reader_state_push (reader_state, new_state);
        return KAN_TRUE;
    }
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t read_array_appender (struct reader_state_t *reader_state,
                                              const struct kan_readable_data_event_t *parsed_event,
                                              struct reader_block_state_t *top_state)
{
    if (parsed_event->output_target.array_index != KAN_READABLE_DATA_ARRAY_INDEX_NONE)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Array appender attempts to set value at path \"%s[%llu]\", but array appenders can not have indices "
                 "as arrays of arrays are not supported.",
                 parsed_event->output_target.identifier, (unsigned long long) parsed_event->output_target.array_index)
        return KAN_FALSE;
    }

    if (top_state->type != READER_BLOCK_TYPE_STRUCT)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Array appender attempts to set value at path \"%s\", but array appenders inside patches are not "
                 "supported.",
                 parsed_event->output_target.identifier)
        return KAN_FALSE;
    }

    size_t output_target_parts_count = 0u;
    kan_interned_string_t output_target_parts[KAN_SERIALIZATION_RD_MAX_PARTS_IN_OUTPUT_TARGET];

    if (!extract_output_target_parts (parsed_event, &output_target_parts_count, output_target_parts))
    {
        return KAN_FALSE;
    }

    kan_instance_size_t absolute_offset;
    kan_instance_size_t size_with_padding;

    const struct kan_reflection_field_t *field = kan_reflection_registry_query_local_field (
        reader_state->registry, top_state->struct_state.type->name, output_target_parts_count, output_target_parts,
        &absolute_offset, &size_with_padding);

    if (!field)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Array appender attempts to set value at path \"%s\", but there is no field at given path.",
                 parsed_event->output_target.identifier)
        return KAN_FALSE;
    }

    if (field->archetype != KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Array appender attempts to set value at path \"%s\", but given field is not a dynamic array.",
                 parsed_event->output_target.identifier)
        return KAN_FALSE;
    }

    struct kan_dynamic_array_t *dynamic_array =
        (struct kan_dynamic_array_t *) (((uint8_t *) top_state->struct_state.instance) + absolute_offset);

    void *spot = kan_dynamic_array_add_last (dynamic_array);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (dynamic_array, KAN_MAX (1u, dynamic_array->capacity * 2u));
        spot = kan_dynamic_array_add_last (dynamic_array);
    }

    KAN_ASSERT (spot)
    switch (field->archetype_dynamic_array.item_archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_FLOATING:
    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
    case KAN_REFLECTION_ARCHETYPE_ENUM:
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Array appender attempts to set value at path \"%s\" which holds elemental value, but array appenders "
                 "can only be used with arrays of structs or patches.",
                 parsed_event->output_target.identifier)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Array appender attempts to set value at path \"%s\" which holds external pointer, but array appenders"
                 " can only be used with arrays of structs or patches.",
                 parsed_event->output_target.identifier)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_STRUCT:
    {
        struct reader_block_state_t new_state = {
            .type = READER_BLOCK_TYPE_STRUCT,
            .struct_state =
                {
                    .instance = spot,
                    .type = kan_reflection_registry_query_struct (
                        reader_state->registry, field->archetype_dynamic_array.item_archetype_struct.type_name),
                },
        };

        KAN_ASSERT (new_state.struct_state.type)
        if (new_state.struct_state.type->init)
        {
            new_state.struct_state.type->init (new_state.struct_state.type->functor_user_data, spot);
        }

        reader_state_push (reader_state, new_state);
        return KAN_TRUE;
    }

    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Array appender attempts to set value at path \"%s\" which holds struct pointer, but array appenders"
                 " can only be used with arrays of structs or patches.",
                 parsed_event->output_target.identifier)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        KAN_ASSERT (KAN_FALSE)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_PATCH:
    {
        struct reader_block_state_t new_state = {.type = READER_BLOCK_TYPE_PATCH,
                                                 .patch_state = {
                                                     .patch_output = spot,
                                                     .type = NULL,
                                                 }};

        reader_state_push (reader_state, new_state);
        return KAN_TRUE;
    }
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

enum kan_serialization_state_t kan_serialization_rd_reader_step (kan_serialization_rd_reader_t reader)
{
    struct reader_state_t *reader_state = KAN_HANDLE_GET (reader);
    KAN_ASSERT (reader_state->block_state_stack.size > 0u)

    const enum kan_readable_data_parser_response_t response = kan_readable_data_parser_step (reader_state->parser);
    switch (response)
    {
    case KAN_READABLE_DATA_PARSER_RESPONSE_NEW_EVENT:
        break;

    case KAN_READABLE_DATA_PARSER_RESPONSE_FAILED:
        return KAN_SERIALIZATION_FAILED;

    case KAN_READABLE_DATA_PARSER_RESPONSE_COMPLETED:
        if (reader_state->block_state_stack.size > 1u)
        {
            KAN_LOG (
                serialization_readable_data, KAN_LOG_ERROR,
                "Readable data parser completed parsing input, but more than one serialization state is on the stack."
                "This can only happen if not all readable data blocks are closed, but parser skipped it somehow.")
            return KAN_SERIALIZATION_FAILED;
        }

        return KAN_SERIALIZATION_FINISHED;
    }

    const struct kan_readable_data_event_t *parsed_event =
        kan_readable_data_parser_get_last_event (reader_state->parser);

    struct reader_block_state_t *top_state =
        &((struct reader_block_state_t *)
              reader_state->block_state_stack.data)[reader_state->block_state_stack.size - 1u];

    switch (parsed_event->type)
    {
    case KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER:
    case KAN_READABLE_DATA_EVENT_ELEMENTAL_STRING_SETTER:
    case KAN_READABLE_DATA_EVENT_ELEMENTAL_INTEGER_SETTER:
    case KAN_READABLE_DATA_EVENT_ELEMENTAL_FLOATING_SETTER:
        if (!read_elemental_setter (reader_state, parsed_event, top_state))
        {
            return KAN_SERIALIZATION_FAILED;
        }

        break;

    case KAN_READABLE_DATA_EVENT_STRUCTURAL_SETTER_BEGIN:
        if (!read_structural_setter (reader_state, parsed_event, top_state))
        {
            return KAN_SERIALIZATION_FAILED;
        }

        break;

    case KAN_READABLE_DATA_EVENT_ARRAY_APPENDER_BEGIN:
        if (!read_array_appender (reader_state, parsed_event, top_state))
        {
            return KAN_SERIALIZATION_FAILED;
        }

        break;

    case KAN_READABLE_DATA_EVENT_BLOCK_END:
        reader_state_pop (reader_state);
        break;
    }

    return KAN_SERIALIZATION_IN_PROGRESS;
}

void kan_serialization_rd_reader_destroy (kan_serialization_rd_reader_t reader)
{
    struct reader_state_t *reader_state = KAN_HANDLE_GET (reader);
    kan_readable_data_parser_destroy (reader_state->parser);
    kan_reflection_patch_builder_destroy (reader_state->patch_builder);
    kan_dynamic_array_shutdown (&reader_state->block_state_stack);
    kan_free_general (serialization_allocation_group, reader_state, sizeof (struct reader_state_t));
}

kan_serialization_rd_writer_t kan_serialization_rd_writer_create (struct kan_stream_t *stream,
                                                                  const void *instance,
                                                                  kan_interned_string_t type_name,
                                                                  kan_reflection_registry_t reflection_registry)
{
    ensure_statics_initialized ();
    KAN_ASSERT (kan_stream_is_writeable (stream))

    struct writer_state_t *writer_state = (struct writer_state_t *) kan_allocate_general (
        serialization_allocation_group, sizeof (struct writer_state_t), _Alignof (struct writer_state_t));

    writer_state->emitter = kan_readable_data_emitter_create (stream);
    writer_state->registry = reflection_registry;
    kan_dynamic_array_init (&writer_state->block_state_stack, KAN_SERIALIZATION_RD_WRITER_STACK_INITIAL_CAPACITY,
                            sizeof (struct writer_block_state_t), _Alignof (struct writer_block_state_t),
                            serialization_allocation_group);

    struct writer_block_state_t root_state;
    root_state.type = WRITER_BLOCK_TYPE_STRUCT;

    kan_reflection_visibility_iterator_init (&root_state.struct_state.field_iterator,
                                             kan_reflection_registry_query_struct (writer_state->registry, type_name),
                                             instance);

    root_state.struct_state.suffix_next_index_to_write = 0u;
    writer_state_push (writer_state, root_state);
    return KAN_HANDLE_SET (kan_serialization_rd_writer_t, writer_state);
}

static inline kan_bool_t emit_structural_setter_begin (struct writer_state_t *writer_state,
                                                       const char *name,
                                                       kan_instance_size_t array_index)
{
    struct kan_readable_data_event_t event;
    event.type = KAN_READABLE_DATA_EVENT_STRUCTURAL_SETTER_BEGIN;
    event.output_target.identifier = name;
    event.output_target.array_index = array_index;
    return kan_readable_data_emitter_step (writer_state->emitter, &event);
}

static inline kan_bool_t emit_array_appender_begin (struct writer_state_t *writer_state, const char *name)
{
    struct kan_readable_data_event_t event;
    event.type = KAN_READABLE_DATA_EVENT_ARRAY_APPENDER_BEGIN;
    event.output_target.identifier = name;
    event.output_target.array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE;
    return kan_readable_data_emitter_step (writer_state->emitter, &event);
}

static inline kan_bool_t emit_block_end (struct writer_state_t *writer_state)
{
    struct kan_readable_data_event_t event;
    event.type = KAN_READABLE_DATA_EVENT_BLOCK_END;
    return kan_readable_data_emitter_step (writer_state->emitter, &event);
}

static inline int64_t extract_signed_integer_value (kan_instance_size_t size, const void *address)
{
    switch (size)
    {
    case 1u:
        return *((int8_t *) address);

    case 2u:
        return *((int16_t *) address);

    case 4u:
        return *((int32_t *) address);

    case 8u:
        return *((int64_t *) address);
    }

    KAN_ASSERT (KAN_FALSE)
    return 0;
}

static inline kan_bool_t emit_single_signed_integer_setter (struct writer_state_t *writer_state,
                                                            kan_interned_string_t name,
                                                            kan_instance_size_t array_index,
                                                            kan_instance_size_t size,
                                                            const void *address)
{
    struct kan_readable_data_event_t event;
    event.type = KAN_READABLE_DATA_EVENT_ELEMENTAL_INTEGER_SETTER;
    event.output_target.identifier = name;
    event.output_target.array_index = array_index;

    struct kan_readable_data_value_node_t value_node = {
        .next = NULL,
        .integer = extract_signed_integer_value (size, address),
    };

    event.setter_value_first = &value_node;
    return kan_readable_data_emitter_step (writer_state->emitter, &event);
}

static inline int64_t extract_unsigned_integer_value (kan_instance_size_t size, const void *address)
{
    switch (size)
    {
    case 1u:
        return (int64_t) * ((uint8_t *) address);

    case 2u:
        return (int64_t) * ((uint16_t *) address);

    case 4u:
        return (int64_t) * ((uint32_t *) address);

    case 8u:
        KAN_ASSERT (*((uint64_t *) address) <= INT64_MAX)
        return (int64_t) * ((uint64_t *) address);
    }

    KAN_ASSERT (KAN_FALSE)
    return 0u;
}

static inline kan_bool_t emit_single_unsigned_integer_setter (struct writer_state_t *writer_state,
                                                              kan_interned_string_t name,
                                                              kan_instance_size_t array_index,
                                                              kan_instance_size_t size,
                                                              const void *address)
{
    struct kan_readable_data_event_t event;
    event.type = KAN_READABLE_DATA_EVENT_ELEMENTAL_INTEGER_SETTER;
    event.output_target.identifier = name;
    event.output_target.array_index = array_index;

    struct kan_readable_data_value_node_t value_node = {
        .next = NULL,
        .integer = extract_unsigned_integer_value (size, address),
    };

    event.setter_value_first = &value_node;
    return kan_readable_data_emitter_step (writer_state->emitter, &event);
}

static inline kan_readable_data_floating_t extract_floating_value (kan_instance_size_t size, const void *address)
{
    switch (size)
    {
    case 4u:
        return *((float *) address);

    case 8u:
        return (kan_readable_data_floating_t) * ((double *) address);
    }

    KAN_ASSERT (KAN_FALSE)
    return (kan_readable_data_floating_t) 0.0;
}

static inline kan_bool_t emit_single_floating_setter (struct writer_state_t *writer_state,
                                                      kan_interned_string_t name,
                                                      kan_instance_size_t array_index,
                                                      kan_instance_size_t size,
                                                      const void *address)
{
    struct kan_readable_data_event_t event;
    event.type = KAN_READABLE_DATA_EVENT_ELEMENTAL_FLOATING_SETTER;
    event.output_target.identifier = name;
    event.output_target.array_index = array_index;

    struct kan_readable_data_value_node_t value_node = {
        .next = NULL,
        .floating = extract_floating_value (size, address),
    };

    event.setter_value_first = &value_node;
    return kan_readable_data_emitter_step (writer_state->emitter, &event);
}

static inline kan_bool_t emit_single_enum_setter (struct writer_state_t *writer_state,
                                                  kan_interned_string_t name,
                                                  kan_instance_size_t array_index,
                                                  kan_interned_string_t type_name,
                                                  const void *address)
{
    const struct kan_reflection_enum_t *enum_data =
        kan_reflection_registry_query_enum (writer_state->registry, type_name);
    KAN_ASSERT (enum_data)

    struct kan_readable_data_event_t event;
    event.type = KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER;
    event.output_target.identifier = name;
    event.output_target.array_index = array_index;

    if (enum_data->flags)
    {
#define MAX_VALUE_NODES (sizeof (kan_reflection_enum_size_t) * 8u)
        struct kan_readable_data_value_node_t value_nodes[MAX_VALUE_NODES];
        kan_loop_size_t value_nodes_count = 0u;
        event.setter_value_first = &value_nodes[0u];
        kan_interned_string_t none_name = NULL;
        const unsigned int enum_value = *(unsigned int *) address;

        for (kan_loop_size_t value_index = 0u; value_index < enum_data->values_count; ++value_index)
        {
            const struct kan_reflection_enum_value_t *value_data = &(enum_data->values[value_index]);
            if (value_data->value == 0)
            {
                none_name = value_data->name;
            }
            else
            {
                unsigned int unsigned_value = (unsigned int) value_data->value;
                if (enum_value & unsigned_value)
                {
                    if (value_nodes_count < MAX_VALUE_NODES)
                    {
                        value_nodes[value_nodes_count].next = &value_nodes[value_nodes_count];
                        value_nodes[value_nodes_count].identifier = value_data->name;
                        ++value_nodes_count;
                    }
                    else
                    {
                        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                                 "Unable to save value of enum in field \"%s\" as it has too many values selected.",
                                 name)
                        return KAN_FALSE;
                    }
                }
            }
        }

        if (value_nodes_count == 0u)
        {
            if (none_name)
            {
                value_nodes[0u].next = NULL;
                value_nodes[0u].identifier = none_name;
            }
            else
            {
                // No value to set, skip event emission. It is correct case as bitflag default value should be
                // "no selected values".
                return KAN_TRUE;
            }
        }
        else
        {
            value_nodes[value_nodes_count - 1u].next = NULL;
        }

        return kan_readable_data_emitter_step (writer_state->emitter, &event);
#undef MAX_VALUE_NODES
    }
    else
    {
        const int enum_value = *(int *) address;
        for (kan_loop_size_t value_index = 0u; value_index < enum_data->values_count; ++value_index)
        {
            const struct kan_reflection_enum_value_t *value_data = &(enum_data->values[value_index]);
            if ((int) value_data->value == enum_value)
            {
                struct kan_readable_data_value_node_t value_node;
                value_node.next = NULL;
                value_node.identifier = value_data->name;
                event.setter_value_first = &value_node;
                return kan_readable_data_emitter_step (writer_state->emitter, &event);
            }
        }

        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Unable to save value of enum in field \"%s\" as there is no appropriate name for value %d.", name,
                 enum_value)
        return KAN_FALSE;
    }
}

static inline kan_bool_t emit_single_string_setter (struct writer_state_t *writer_state,
                                                    kan_interned_string_t name,
                                                    kan_instance_size_t array_index,
                                                    const void *address)
{
    if (!*(char **) address)
    {
        // Just skip empty strings, they should be nullified by initializers either way.
        return KAN_TRUE;
    }

    struct kan_readable_data_event_t event;
    event.type = KAN_READABLE_DATA_EVENT_ELEMENTAL_STRING_SETTER;
    event.output_target.identifier = name;
    event.output_target.array_index = array_index;

    struct kan_readable_data_value_node_t value_node = {
        .next = NULL,
        .string = *(char **) address,
    };

    event.setter_value_first = &value_node;
    return kan_readable_data_emitter_step (writer_state->emitter, &event);
}

static inline kan_bool_t enter_struct (struct writer_state_t *writer_state,
                                       kan_interned_string_t name,
                                       kan_instance_size_t array_index,
                                       kan_interned_string_t type_name,
                                       const void *address,
                                       kan_bool_t as_array_appender)
{
    if (as_array_appender)
    {
        if (!emit_array_appender_begin (writer_state, name))
        {
            return KAN_FALSE;
        }
    }
    else
    {
        if (!emit_structural_setter_begin (writer_state, name, array_index))
        {
            return KAN_FALSE;
        }
    }

    struct writer_block_state_t new_block_state;
    new_block_state.type = WRITER_BLOCK_TYPE_STRUCT;

    kan_reflection_visibility_iterator_init (&new_block_state.struct_state.field_iterator,
                                             kan_reflection_registry_query_struct (writer_state->registry, type_name),
                                             address);

    new_block_state.struct_state.suffix_next_index_to_write = 0u;
    writer_state_push (writer_state, new_block_state);
    return KAN_TRUE;
}

static inline kan_bool_t enter_patch (struct writer_state_t *writer_state,
                                      kan_interned_string_t name,
                                      kan_instance_size_t array_index,
                                      const void *address,
                                      kan_bool_t as_array_appender)
{
    kan_reflection_patch_t patch = *(kan_reflection_patch_t *) address;
    if (KAN_HANDLE_IS_VALID (patch))
    {
        if (as_array_appender)
        {
            if (!emit_array_appender_begin (writer_state, name))
            {
                return KAN_FALSE;
            }
        }
        else
        {
            if (!emit_structural_setter_begin (writer_state, name, array_index))
            {
                return KAN_FALSE;
            }
        }

        writer_state_push (writer_state,
                           (struct writer_block_state_t) {.type = WRITER_BLOCK_TYPE_PATCH,
                                                          .patch_state = {
                                                              .patch = patch,
                                                              .current_iterator = kan_reflection_patch_begin (patch),
                                                              .end_iterator = kan_reflection_patch_end (patch),
                                                          }});

        const struct kan_reflection_struct_t *patch_struct = kan_reflection_patch_get_type (patch);
        KAN_ASSERT (patch_struct)

        struct kan_readable_data_event_t event;
        event.type = KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER;
        event.output_target.identifier = interned_string_patch_type_field;
        event.output_target.array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE;

        struct kan_readable_data_value_node_t value_node = {
            .next = NULL,
            .identifier = patch_struct->name,
        };

        event.setter_value_first = &value_node;
        if (!kan_readable_data_emitter_step (writer_state->emitter, &event))
        {
            return KAN_FALSE;
        }
    }

    return KAN_TRUE;
}

static inline kan_bool_t can_pack_array (enum kan_reflection_archetype_t archetype, kan_instance_size_t item_count)
{
    if (item_count > KAN_SERIALIZATION_RD_WRITE_MAX_PACKED_ARRAY_SIZE)
    {
        return KAN_FALSE;
    }

    switch (archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_FLOATING:
    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        return KAN_TRUE;

    // We do not pack enum arrays, because otherwise they'll look like bitflags.
    case KAN_REFLECTION_ARCHETYPE_ENUM:
    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
    case KAN_REFLECTION_ARCHETYPE_STRUCT:
    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
    case KAN_REFLECTION_ARCHETYPE_PATCH:
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        KAN_ASSERT (KAN_FALSE)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t write_packed_array (struct writer_state_t *writer_state,
                                             kan_interned_string_t identifier,
                                             enum kan_reflection_archetype_t archetype,
                                             kan_instance_size_t item_size,
                                             kan_instance_size_t item_count,
                                             const void *begin)
{
    if (item_count == 0u)
    {
        return KAN_TRUE;
    }

    const void *end = ((const uint8_t *) begin) + item_size * item_count;
    struct kan_readable_data_value_node_t value_nodes[KAN_SERIALIZATION_RD_WRITE_MAX_PACKED_ARRAY_SIZE];
    kan_loop_size_t value_nodes_count = 0u;

    struct kan_readable_data_event_t event;
    event.output_target.identifier = identifier;
    event.output_target.array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE;

    switch (archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        event.type = KAN_READABLE_DATA_EVENT_ELEMENTAL_INTEGER_SETTER;

        while (begin < end)
        {
            KAN_ASSERT (value_nodes_count < KAN_SERIALIZATION_RD_WRITE_MAX_PACKED_ARRAY_SIZE)
            value_nodes[value_nodes_count].next = &value_nodes[value_nodes_count + 1u];
            value_nodes[value_nodes_count].integer = extract_signed_integer_value (item_size, begin);
            ++value_nodes_count;
            begin = ((uint8_t *) begin) + item_size;
        }

        value_nodes[value_nodes_count - 1u].next = NULL;
        return kan_readable_data_emitter_step (writer_state->emitter, &event);

    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        event.type = KAN_READABLE_DATA_EVENT_ELEMENTAL_INTEGER_SETTER;

        while (begin < end)
        {
            KAN_ASSERT (value_nodes_count < KAN_SERIALIZATION_RD_WRITE_MAX_PACKED_ARRAY_SIZE)
            value_nodes[value_nodes_count].next = &value_nodes[value_nodes_count + 1u];
            value_nodes[value_nodes_count].integer = extract_unsigned_integer_value (item_size, begin);
            ++value_nodes_count;
            begin = ((uint8_t *) begin) + item_size;
        }

        value_nodes[value_nodes_count - 1u].next = NULL;
        return kan_readable_data_emitter_step (writer_state->emitter, &event);

    case KAN_REFLECTION_ARCHETYPE_FLOATING:
        event.type = KAN_READABLE_DATA_EVENT_ELEMENTAL_FLOATING_SETTER;

        while (begin < end)
        {
            KAN_ASSERT (value_nodes_count < KAN_SERIALIZATION_RD_WRITE_MAX_PACKED_ARRAY_SIZE)
            value_nodes[value_nodes_count].next = &value_nodes[value_nodes_count + 1u];
            value_nodes[value_nodes_count].floating = extract_floating_value (item_size, begin);
            ++value_nodes_count;
            begin = ((uint8_t *) begin) + item_size;
        }

        value_nodes[value_nodes_count - 1u].next = NULL;
        return kan_readable_data_emitter_step (writer_state->emitter, &event);

    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        event.type = KAN_READABLE_DATA_EVENT_ELEMENTAL_STRING_SETTER;

        while (begin < end)
        {
            KAN_ASSERT (value_nodes_count < KAN_SERIALIZATION_RD_WRITE_MAX_PACKED_ARRAY_SIZE)
            value_nodes[value_nodes_count].next = &value_nodes[value_nodes_count + 1u];
            value_nodes[value_nodes_count].string = *(char **) begin;
            ++value_nodes_count;
            begin = ((uint8_t *) begin) + item_size;
        }

        value_nodes[value_nodes_count - 1u].next = NULL;
        return kan_readable_data_emitter_step (writer_state->emitter, &event);

    case KAN_REFLECTION_ARCHETYPE_ENUM:
    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
    case KAN_REFLECTION_ARCHETYPE_STRUCT:
    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
    case KAN_REFLECTION_ARCHETYPE_PATCH:
    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        KAN_ASSERT (KAN_FALSE)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

// Returning bool for convenient chaining.
static inline kan_bool_t writer_struct_block_state_advance (struct writer_block_state_t *top_state)
{
    kan_reflection_visibility_iterator_advance (&top_state->struct_state.field_iterator);
    top_state->struct_state.suffix_next_index_to_write = 0u;
    return KAN_TRUE;
}

static inline kan_bool_t writer_step_struct (struct writer_state_t *writer_state,
                                             struct writer_block_state_t *top_state)
{
    if (top_state->struct_state.field_iterator.field == top_state->struct_state.field_iterator.field_end)
    {
        return KAN_TRUE;
    }

    const void *struct_instance = top_state->struct_state.field_iterator.context;
    const struct kan_reflection_field_t *current_field = top_state->struct_state.field_iterator.field;
    const uint8_t *address = ((const uint8_t *) struct_instance) + current_field->offset;

    switch (current_field->archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        return emit_single_signed_integer_setter (writer_state, current_field->name, KAN_READABLE_DATA_ARRAY_INDEX_NONE,
                                                  current_field->size, address) &&
               writer_struct_block_state_advance (top_state);

    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        return emit_single_unsigned_integer_setter (writer_state, current_field->name,
                                                    KAN_READABLE_DATA_ARRAY_INDEX_NONE, current_field->size, address) &&
               writer_struct_block_state_advance (top_state);

    case KAN_REFLECTION_ARCHETYPE_FLOATING:
        return emit_single_floating_setter (writer_state, current_field->name, KAN_READABLE_DATA_ARRAY_INDEX_NONE,
                                            current_field->size, address) &&
               writer_struct_block_state_advance (top_state);

    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        return emit_single_string_setter (writer_state, current_field->name, KAN_READABLE_DATA_ARRAY_INDEX_NONE,
                                          address) &&
               writer_struct_block_state_advance (top_state);

    case KAN_REFLECTION_ARCHETYPE_ENUM:
        return emit_single_enum_setter (writer_state, current_field->name, KAN_READABLE_DATA_ARRAY_INDEX_NONE,
                                        current_field->archetype_enum.type_name, address) &&
               writer_struct_block_state_advance (top_state);

    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Found field \"%s\" that is an external pointer. External pointers in serializable structs are "
                 "not supported.",
                 current_field->name)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_STRUCT:
        return enter_struct (writer_state, current_field->name, KAN_READABLE_DATA_ARRAY_INDEX_NONE,
                             current_field->archetype_struct.type_name, address, KAN_FALSE) &&
               writer_struct_block_state_advance (top_state);

    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Found field \"%s\" that is a struct pointer. Struct pointers in serializable structs are not "
                 "supported.",
                 current_field->name)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        if (can_pack_array (current_field->archetype_inline_array.item_archetype,
                            current_field->archetype_inline_array.item_count))
        {
            return write_packed_array (writer_state, current_field->name,
                                       current_field->archetype_inline_array.item_archetype,
                                       current_field->archetype_inline_array.item_size,
                                       current_field->archetype_inline_array.item_count, address) &&
                   writer_struct_block_state_advance (top_state);
        }
        else if (top_state->struct_state.suffix_next_index_to_write < current_field->archetype_inline_array.item_count)
        {
            const kan_instance_size_t index_to_write = top_state->struct_state.suffix_next_index_to_write++;
            const void *array_address = address + index_to_write * current_field->archetype_inline_array.item_size;

            switch (current_field->archetype_inline_array.item_archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                return emit_single_signed_integer_setter (writer_state, current_field->name, index_to_write,
                                                          current_field->archetype_inline_array.item_size,
                                                          array_address);

            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                return emit_single_unsigned_integer_setter (writer_state, current_field->name, index_to_write,
                                                            current_field->archetype_inline_array.item_size,
                                                            array_address);

            case KAN_REFLECTION_ARCHETYPE_FLOATING:
                return emit_single_floating_setter (writer_state, current_field->name, index_to_write,
                                                    current_field->archetype_inline_array.item_size, array_address);

            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                return emit_single_string_setter (writer_state, current_field->name, index_to_write, array_address);

            case KAN_REFLECTION_ARCHETYPE_ENUM:
                return emit_single_enum_setter (writer_state, current_field->name, index_to_write,
                                                current_field->archetype_inline_array.item_archetype_enum.type_name,
                                                array_address);

            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                         "Found field \"%s\" that is an array of external pointers. External pointers in serializable "
                         "structs are not supported.",
                         current_field->name)
                return KAN_FALSE;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                return enter_struct (writer_state, current_field->name, index_to_write,
                                     current_field->archetype_inline_array.item_archetype_struct.type_name,
                                     array_address, KAN_FALSE);

            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                         "Found field \"%s\" that is an array of struct pointers. Struct pointers in serializable "
                         "structs are not supported.",
                         current_field->name)
                return KAN_FALSE;

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                KAN_ASSERT (KAN_FALSE)
                return KAN_FALSE;

            case KAN_REFLECTION_ARCHETYPE_PATCH:
                return enter_patch (writer_state, current_field->name, index_to_write, array_address, KAN_FALSE);
            }
        }
        else
        {
            writer_struct_block_state_advance (top_state);
            return KAN_TRUE;
        }

    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
    {
        struct kan_dynamic_array_t *dynamic_array = (struct kan_dynamic_array_t *) address;
        if (dynamic_array->size == 0u)
        {
            writer_struct_block_state_advance (top_state);
            return KAN_TRUE;
        }

        if (can_pack_array (current_field->archetype_dynamic_array.item_archetype, dynamic_array->size))
        {
            return write_packed_array (writer_state, current_field->name,
                                       current_field->archetype_dynamic_array.item_archetype, dynamic_array->item_size,
                                       dynamic_array->size, dynamic_array->data) &&
                   writer_struct_block_state_advance (top_state);
        }
        else if (top_state->struct_state.suffix_next_index_to_write < dynamic_array->size)
        {
            const kan_instance_size_t index_to_write = top_state->struct_state.suffix_next_index_to_write++;
            const void *array_address =
                ((const uint8_t *) dynamic_array->data) + index_to_write * dynamic_array->item_size;

            switch (current_field->archetype_dynamic_array.item_archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                return emit_single_signed_integer_setter (writer_state, current_field->name, index_to_write,
                                                          dynamic_array->item_size, array_address);

            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                return emit_single_unsigned_integer_setter (writer_state, current_field->name, index_to_write,
                                                            dynamic_array->item_size, array_address);

            case KAN_REFLECTION_ARCHETYPE_FLOATING:
                return emit_single_floating_setter (writer_state, current_field->name, index_to_write,
                                                    dynamic_array->item_size, array_address);

            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                return emit_single_string_setter (writer_state, current_field->name, index_to_write, array_address);

            case KAN_REFLECTION_ARCHETYPE_ENUM:
                return emit_single_enum_setter (writer_state, current_field->name, index_to_write,
                                                current_field->archetype_dynamic_array.item_archetype_enum.type_name,
                                                array_address);

            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                         "Found field \"%s\" that is an array of external pointers. External pointers in serializable "
                         "structs are not supported.",
                         current_field->name)
                return KAN_FALSE;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                return enter_struct (writer_state, current_field->name, index_to_write,
                                     current_field->archetype_dynamic_array.item_archetype_struct.type_name,
                                     array_address, KAN_TRUE);

            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                         "Found field \"%s\" that is an array of struct pointers. Struct pointers in serializable "
                         "structs are not supported.",
                         current_field->name)
                return KAN_FALSE;

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                KAN_ASSERT (KAN_FALSE)
                return KAN_FALSE;

            case KAN_REFLECTION_ARCHETYPE_PATCH:
                return enter_patch (writer_state, current_field->name, index_to_write, array_address, KAN_TRUE);
            }
        }
        else
        {
            writer_struct_block_state_advance (top_state);
            return KAN_TRUE;
        }
    }

    case KAN_REFLECTION_ARCHETYPE_PATCH:
        return enter_patch (writer_state, current_field->name, KAN_READABLE_DATA_ARRAY_INDEX_NONE, address,
                            KAN_FALSE) &&
               writer_struct_block_state_advance (top_state);
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline const struct kan_reflection_field_t *find_first_first_field_for_patch_sub_struct (
    const struct kan_reflection_struct_t *type, kan_loop_size_t data_begin)
{
    if (data_begin == 0u)
    {
        return &type->fields[0u];
    }

    kan_loop_size_t first = 0u;
    kan_loop_size_t last = type->fields_count;

    while (first < last)
    {
        const kan_loop_size_t middle = (first + last) / 2u;
        const kan_loop_size_t middle_field_end = type->fields[middle].offset + type->fields[middle].size;

        if (middle_field_end <= data_begin)
        {
            first = middle + 1u;
        }
        else
        {
            last = middle;
        }
    }

    return &type->fields[first];
}

static inline const struct kan_reflection_field_t *find_first_end_field_for_patch_sub_struct (
    const struct kan_reflection_struct_t *type, kan_loop_size_t data_end)
{
    if (data_end >= type->size)
    {
        return &type->fields[type->fields_count];
    }

    kan_loop_size_t first = 0u;
    kan_loop_size_t last = type->fields_count;

    while (first < last)
    {
        const kan_loop_size_t middle = (first + last) / 2u;
        const kan_loop_size_t middle_field_begin = type->fields[middle].offset;

        if (middle_field_begin < data_end)
        {
            first = middle + 1u;
        }
        else
        {
            last = middle;
        }
    }

    return &type->fields[first];
}

// Returning bool for convenient chaining.
static inline kan_bool_t writer_patch_sub_struct_block_state_advance (struct writer_block_state_t *top_state)
{
    ++top_state->patch_sub_struct_state.current_field;
    top_state->patch_sub_struct_state.suffix_next_index_to_write = 0u;
    return KAN_TRUE;
}

static inline kan_bool_t enter_patch_sub_struct (struct writer_state_t *writer_state,
                                                 struct writer_block_state_t *top_state,
                                                 kan_interned_string_t block_name,
                                                 kan_instance_size_t array_index,
                                                 kan_instance_size_t offset,
                                                 kan_instance_size_t size,
                                                 kan_interned_string_t type_name)
{
    if (!emit_structural_setter_begin (writer_state, block_name, array_index))
    {
        return KAN_FALSE;
    }

    struct writer_block_state_t new_block_state;
    new_block_state.type = WRITER_BLOCK_TYPE_PATCH_SUB_STRUCT;

    if (top_state->patch_sub_struct_state.scope_begin > offset)
    {
        new_block_state.patch_sub_struct_state.scope_begin = top_state->patch_sub_struct_state.scope_begin - offset;
    }
    else
    {
        new_block_state.patch_sub_struct_state.scope_begin = 0u;
    }

    const kan_instance_size_t left_of_data = top_state->patch_sub_struct_state.scope_end - offset;
    const kan_instance_size_t left_of_struct = size - new_block_state.patch_sub_struct_state.scope_begin;

    new_block_state.patch_sub_struct_state.scope_end =
        new_block_state.patch_sub_struct_state.scope_begin + KAN_MIN (left_of_data, left_of_struct);
    new_block_state.patch_sub_struct_state.imaginary_instance =
        ((const uint8_t *) top_state->patch_sub_struct_state.imaginary_instance) + offset;

    const struct kan_reflection_struct_t *type =
        kan_reflection_registry_query_struct (writer_state->registry, type_name);

    new_block_state.patch_sub_struct_state.current_field =
        find_first_first_field_for_patch_sub_struct (type, new_block_state.patch_sub_struct_state.scope_begin);

    new_block_state.patch_sub_struct_state.end_field =
        find_first_end_field_for_patch_sub_struct (type, new_block_state.patch_sub_struct_state.scope_end);

    new_block_state.patch_sub_struct_state.suffix_next_index_to_write = 0u;

    writer_patch_sub_struct_block_state_advance (top_state);
    writer_state_push (writer_state, new_block_state);
    return KAN_TRUE;
}

static inline kan_bool_t writer_step_patch_sub_struct (struct writer_state_t *writer_state,
                                                       struct writer_block_state_t *top_state)
{
    if (top_state->patch_sub_struct_state.current_field == top_state->patch_sub_struct_state.end_field)
    {
        return KAN_TRUE;
    }

    const struct kan_reflection_field_t *current_field = top_state->patch_sub_struct_state.current_field;
    if (current_field->visibility_condition_field)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Found field \"%s\" that has condition while serializing patch. Fields with visibility conditions "
                 "aren't supported for patch textual serialization due to possibility of incomplete or broken data.",
                 current_field->name)
        return KAN_FALSE;
    }

    const uint8_t *address =
        ((const uint8_t *) top_state->patch_sub_struct_state.imaginary_instance) + current_field->offset;

    switch (current_field->archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        KAN_ASSERT (current_field->offset >= top_state->patch_sub_struct_state.scope_begin)
        KAN_ASSERT (current_field->offset + current_field->size <= top_state->patch_sub_struct_state.scope_end)
        return emit_single_signed_integer_setter (writer_state, current_field->name, KAN_READABLE_DATA_ARRAY_INDEX_NONE,
                                                  current_field->size, address) &&
               writer_patch_sub_struct_block_state_advance (top_state);

    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        KAN_ASSERT (current_field->offset >= top_state->patch_sub_struct_state.scope_begin)
        KAN_ASSERT (current_field->offset + current_field->size <= top_state->patch_sub_struct_state.scope_end)
        return emit_single_unsigned_integer_setter (writer_state, current_field->name,
                                                    KAN_READABLE_DATA_ARRAY_INDEX_NONE, current_field->size, address) &&
               writer_patch_sub_struct_block_state_advance (top_state);

    case KAN_REFLECTION_ARCHETYPE_FLOATING:
        KAN_ASSERT (current_field->offset >= top_state->patch_sub_struct_state.scope_begin)
        KAN_ASSERT (current_field->offset + current_field->size <= top_state->patch_sub_struct_state.scope_end)
        return emit_single_floating_setter (writer_state, current_field->name, KAN_READABLE_DATA_ARRAY_INDEX_NONE,
                                            current_field->size, address) &&
               writer_patch_sub_struct_block_state_advance (top_state);

    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        KAN_ASSERT (current_field->offset >= top_state->patch_sub_struct_state.scope_begin)
        KAN_ASSERT (current_field->offset + current_field->size <= top_state->patch_sub_struct_state.scope_end)
        return emit_single_string_setter (writer_state, current_field->name, KAN_READABLE_DATA_ARRAY_INDEX_NONE,
                                          address) &&
               writer_patch_sub_struct_block_state_advance (top_state);

    case KAN_REFLECTION_ARCHETYPE_ENUM:
        KAN_ASSERT (current_field->offset >= top_state->patch_sub_struct_state.scope_begin)
        KAN_ASSERT (current_field->offset + current_field->size <= top_state->patch_sub_struct_state.scope_end)
        return emit_single_enum_setter (writer_state, current_field->name, KAN_READABLE_DATA_ARRAY_INDEX_NONE,
                                        current_field->archetype_enum.type_name, address) &&
               writer_patch_sub_struct_block_state_advance (top_state);

    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Found field \"%s\" that is an external pointer. External pointers in serializable structs are "
                 "not supported.",
                 current_field->name)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_STRUCT:
        return enter_patch_sub_struct (writer_state, top_state, current_field->name, KAN_READABLE_DATA_ARRAY_INDEX_NONE,
                                       current_field->offset, current_field->size,
                                       current_field->archetype_struct.type_name);

    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Found field \"%s\" that is a struct pointer. Struct pointers in serializable structs are not "
                 "supported.",
                 current_field->name)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        if (can_pack_array (current_field->archetype_inline_array.item_archetype,
                            current_field->archetype_inline_array.item_count) &&
            current_field->offset >= top_state->patch_sub_struct_state.scope_begin &&
            current_field->offset + current_field->size <= top_state->patch_sub_struct_state.scope_end)
        {
            return write_packed_array (writer_state, current_field->name,
                                       current_field->archetype_inline_array.item_archetype,
                                       current_field->archetype_inline_array.item_size,
                                       current_field->archetype_inline_array.item_count, address) &&
                   writer_patch_sub_struct_block_state_advance (top_state);
        }
        else if (top_state->patch_sub_struct_state.suffix_next_index_to_write <
                 current_field->archetype_inline_array.item_count)
        {
            if (top_state->patch_sub_struct_state.suffix_next_index_to_write == 0u &&
                current_field->offset < top_state->patch_sub_struct_state.scope_begin)
            {
                // Offset initial index if necessary.
                top_state->patch_sub_struct_state.suffix_next_index_to_write =
                    (top_state->patch_sub_struct_state.scope_begin - current_field->offset) /
                    current_field->archetype_inline_array.item_size;
            }

            const kan_instance_size_t index_to_write = top_state->patch_sub_struct_state.suffix_next_index_to_write++;
            const kan_instance_size_t item_offset = index_to_write * current_field->archetype_inline_array.item_size;
            const kan_instance_size_t begin_offset = current_field->offset + item_offset;

            if (begin_offset >= top_state->patch_sub_struct_state.scope_end)
            {
                writer_patch_sub_struct_block_state_advance (top_state);
                return KAN_TRUE;
            }

            const uint8_t *array_address = address + item_offset;
            switch (current_field->archetype_inline_array.item_archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                return emit_single_signed_integer_setter (writer_state, current_field->name, index_to_write,
                                                          current_field->archetype_inline_array.item_size,
                                                          array_address);

            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                return emit_single_unsigned_integer_setter (writer_state, current_field->name, index_to_write,
                                                            current_field->archetype_inline_array.item_size,
                                                            array_address);

            case KAN_REFLECTION_ARCHETYPE_FLOATING:
                return emit_single_floating_setter (writer_state, current_field->name, index_to_write,
                                                    current_field->archetype_inline_array.item_size, array_address);

            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                return emit_single_string_setter (writer_state, current_field->name, index_to_write, array_address);

            case KAN_REFLECTION_ARCHETYPE_ENUM:
                return emit_single_enum_setter (writer_state, current_field->name, index_to_write,
                                                current_field->archetype_inline_array.item_archetype_enum.type_name,
                                                array_address);

            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                         "Found field \"%s\" that is an array of external pointers. External pointers in serializable "
                         "structs are not supported.",
                         current_field->name)
                return KAN_FALSE;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                return enter_patch_sub_struct (writer_state, top_state, current_field->name, index_to_write,
                                               begin_offset, current_field->archetype_inline_array.item_size,
                                               current_field->archetype_inline_array.item_archetype_struct.type_name);

            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                         "Found field \"%s\" that is an array of struct pointers. Struct pointers in serializable "
                         "structs are not supported.",
                         current_field->name)
                return KAN_FALSE;

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                KAN_ASSERT (KAN_FALSE)
                return KAN_FALSE;

            case KAN_REFLECTION_ARCHETYPE_PATCH:
                KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                         "Found field \"%s\" that is a patch array. Patches inside patches are not supported.",
                         current_field->name)
                return KAN_FALSE;
            }
        }
        else
        {
            writer_patch_sub_struct_block_state_advance (top_state);
            return KAN_TRUE;
        }

    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Found field \"%s\" that is a dynamic array. Dynamic arrays inside patches are not supported.",
                 current_field->name)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_PATCH:
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR,
                 "Found field \"%s\" that is a patch. Patches inside patches are not supported.", current_field->name)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline kan_bool_t writer_step_patch (struct writer_state_t *writer_state, struct writer_block_state_t *top_state)
{
    if (KAN_HANDLE_IS_EQUAL (top_state->patch_state.current_iterator, top_state->patch_state.end_iterator))
    {
        return KAN_TRUE;
    }

    struct kan_reflection_patch_chunk_info_t chunk =
        kan_reflection_patch_iterator_get (top_state->patch_state.current_iterator);
    top_state->patch_state.current_iterator =
        kan_reflection_patch_iterator_next (top_state->patch_state.current_iterator);

    struct writer_block_state_t new_state;
    new_state.type = WRITER_BLOCK_TYPE_PATCH_SUB_STRUCT;
    new_state.patch_sub_struct_state.imaginary_instance = ((const uint8_t *) chunk.data) - chunk.offset;
    new_state.patch_sub_struct_state.scope_begin = chunk.offset;
    new_state.patch_sub_struct_state.scope_end = chunk.offset + chunk.size;

    const struct kan_reflection_struct_t *patch_type = kan_reflection_patch_get_type (top_state->patch_state.patch);
    new_state.patch_sub_struct_state.current_field =
        find_first_first_field_for_patch_sub_struct (patch_type, new_state.patch_sub_struct_state.scope_begin);

    new_state.patch_sub_struct_state.end_field =
        find_first_end_field_for_patch_sub_struct (patch_type, new_state.patch_sub_struct_state.scope_end);

    new_state.patch_sub_struct_state.suffix_next_index_to_write = 0u;
    writer_state_push (writer_state, new_state);
    return KAN_TRUE;
}

enum kan_serialization_state_t kan_serialization_rd_writer_step (kan_serialization_rd_writer_t writer)
{
    struct writer_state_t *writer_state = KAN_HANDLE_GET (writer);
    if (writer_state->block_state_stack.size == 0u)
    {
        return KAN_SERIALIZATION_FINISHED;
    }

    struct writer_block_state_t *top_state =
        &((struct writer_block_state_t *)
              writer_state->block_state_stack.data)[writer_state->block_state_stack.size - 1u];

    switch (top_state->type)
    {
    case WRITER_BLOCK_TYPE_STRUCT:
        if (!writer_step_struct (writer_state, top_state))
        {
            return KAN_SERIALIZATION_FAILED;
        }

        break;

    case WRITER_BLOCK_TYPE_PATCH_SUB_STRUCT:
        if (!writer_step_patch_sub_struct (writer_state, top_state))
        {
            return KAN_SERIALIZATION_FAILED;
        }

        break;

    case WRITER_BLOCK_TYPE_PATCH:
        if (!writer_step_patch (writer_state, top_state))
        {
            return KAN_SERIALIZATION_FAILED;
        }

        break;
    }

    while (writer_state->block_state_stack.size > 0u)
    {
        top_state = &((struct writer_block_state_t *)
                          writer_state->block_state_stack.data)[writer_state->block_state_stack.size - 1u];
        kan_bool_t popped = KAN_FALSE;

        switch (top_state->type)
        {
        case WRITER_BLOCK_TYPE_STRUCT:
            if (top_state->struct_state.field_iterator.field == top_state->struct_state.field_iterator.field_end)
            {
                writer_state_pop (writer_state);
                popped = KAN_TRUE;

                // Do not emit block end if it was the root state.
                if (writer_state->block_state_stack.size > 0u)
                {
                    if (!emit_block_end (writer_state))
                    {
                        return KAN_SERIALIZATION_FAILED;
                    }
                }
            }

            break;

        case WRITER_BLOCK_TYPE_PATCH_SUB_STRUCT:
            if (top_state->patch_sub_struct_state.current_field == top_state->patch_sub_struct_state.end_field)
            {
                writer_state_pop (writer_state);
                popped = KAN_TRUE;

                KAN_ASSERT (writer_state->block_state_stack.size > 0u)
                struct writer_block_state_t *state_below =
                    &((struct writer_block_state_t *)
                          writer_state->block_state_stack.data)[writer_state->block_state_stack.size - 1u];

                // Do not emit block end for patch root sub structs.
                if (state_below->type != WRITER_BLOCK_TYPE_PATCH)
                {
                    if (!emit_block_end (writer_state))
                    {
                        return KAN_SERIALIZATION_FAILED;
                    }
                }
            }

            break;

        case WRITER_BLOCK_TYPE_PATCH:
            if (KAN_HANDLE_IS_EQUAL (top_state->patch_state.current_iterator, top_state->patch_state.end_iterator))
            {
                writer_state_pop (writer_state);
                popped = KAN_TRUE;

                if (!emit_block_end (writer_state))
                {
                    return KAN_SERIALIZATION_FAILED;
                }
            }

            break;
        }

        if (!popped)
        {
            break;
        }
    }

    return writer_state->block_state_stack.size == 0u ? KAN_SERIALIZATION_FINISHED : KAN_SERIALIZATION_IN_PROGRESS;
}

void kan_serialization_rd_writer_destroy (kan_serialization_rd_writer_t writer)
{
    struct writer_state_t *writer_state = KAN_HANDLE_GET (writer);
    kan_readable_data_emitter_destroy (writer_state->emitter);
    kan_dynamic_array_shutdown (&writer_state->block_state_stack);
    kan_free_general (serialization_allocation_group, writer_state, sizeof (struct writer_state_t));
}

#define TYPE_HEADER_PREFIX "//! "
#define TYPE_HEADER_PREFIX_LENGTH 4u

kan_bool_t kan_serialization_rd_read_type_header (struct kan_stream_t *stream, kan_interned_string_t *type_name_output)
{
    KAN_ASSERT (kan_stream_is_readable (stream))
    char buffer[KAN_SERIALIZATION_RD_HEADER_MAX_TYPE_LENGTH];

    if (stream->operations->read (stream, TYPE_HEADER_PREFIX_LENGTH, buffer) != TYPE_HEADER_PREFIX_LENGTH)
    {
        return KAN_FALSE;
    }

    if (strncmp (buffer, TYPE_HEADER_PREFIX, TYPE_HEADER_PREFIX_LENGTH) != 0)
    {
        KAN_LOG (serialization_readable_data, KAN_LOG_ERROR, "Failed to read type header: unexpected prefix.")
        return KAN_FALSE;
    }

    char *output = buffer;
    char *output_end = buffer + KAN_SERIALIZATION_RD_HEADER_MAX_TYPE_LENGTH;

    while (KAN_TRUE)
    {
        if (stream->operations->read (stream, 1u, output) != 1u)
        {
            return KAN_FALSE;
        }

        if (*output == '\n')
        {
            *type_name_output = kan_char_sequence_intern (buffer, output);
            break;
        }

        ++output;
        if (output == output_end)
        {
            KAN_LOG (serialization_readable_data, KAN_LOG_ERROR, "Failed to read type header: type name is too long.")
            return KAN_FALSE;
        }
    }

    return KAN_TRUE;
}

kan_bool_t kan_serialization_rd_write_type_header (struct kan_stream_t *stream, kan_interned_string_t type_name)
{
    KAN_ASSERT (kan_stream_is_writeable (stream))
    if (stream->operations->write (stream, TYPE_HEADER_PREFIX_LENGTH, TYPE_HEADER_PREFIX) != TYPE_HEADER_PREFIX_LENGTH)
    {
        return KAN_FALSE;
    }

    const kan_instance_size_t type_name_length = (kan_instance_size_t) strlen (type_name);
    KAN_ASSERT (type_name_length <= KAN_SERIALIZATION_RD_HEADER_MAX_TYPE_LENGTH)

    if (stream->operations->write (stream, type_name_length, type_name) != type_name_length)
    {
        return KAN_FALSE;
    }

    if (stream->operations->write (stream, 1u, "\n") != 1u)
    {
        return KAN_FALSE;
    }

    return KAN_TRUE;
}
