#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <stddef.h>
#include <string.h>

#include <kan/api_common/alignment.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/hash_storage.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/reflection/field_visibility_iterator.h>
#include <kan/reflection/markup.h>
#include <kan/reflection/patch.h>
#include <kan/serialization/binary.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (serialization_binary);

/// \brief Type used for binary script data store.
typedef kan_instance_size_t script_size_t;

enum script_command_type_t
{
    SCRIPT_COMMAND_BLOCK = 0u,
    SCRIPT_COMMAND_STRING,
    SCRIPT_COMMAND_INTERNED_STRING,
    SCRIPT_COMMAND_BLOCK_DYNAMIC_ARRAY,
    SCRIPT_COMMAND_STRING_DYNAMIC_ARRAY,
    SCRIPT_COMMAND_INTERNED_STRING_DYNAMIC_ARRAY,
    SCRIPT_COMMAND_STRUCT_DYNAMIC_ARRAY,
    SCRIPT_COMMAND_PATCH_DYNAMIC_ARRAY,
    SCRIPT_COMMAND_PATCH,
};

struct script_command_block_t
{
    script_size_t size;
};

struct script_command_struct_dynamic_array_t
{
    kan_interned_string_t type_name;
};

#define SCRIPT_NO_CONDITION UINT32_MAX

struct script_command_t
{
    enum script_command_type_t type;
    script_size_t condition_index;
    script_size_t offset;

    union
    {
        struct script_command_block_t block;
        struct script_command_struct_dynamic_array_t struct_dynamic_array;
    };
};

struct script_condition_t
{
    struct kan_reflection_field_t *condition_value_field;
    script_size_t absolute_source_offset;
    kan_instance_size_t condition_values_count;
    kan_reflection_visibility_size_t *condition_values;
    script_size_t parent_condition_index;
};

struct script_t
{
    kan_instance_size_t conditions_count;
    kan_instance_size_t commands_count;

    // First conditions, then commands.
    void *data[];
};

struct script_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t type_name;
    struct kan_atomic_int_t script_generation_lock;
    struct script_t *script;
};

struct interned_string_lookup_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t type_name;
    struct kan_atomic_int_t interned_string_absolute_positions_generation_lock;
    struct kan_atomic_int_t interned_string_absolute_positions_generated;
    kan_instance_size_t interned_string_absolute_positions_count;
    script_size_t *interned_string_absolute_positions;
};

struct script_storage_t
{
    kan_reflection_registry_t registry;

    struct kan_atomic_int_t script_storage_lock;
    struct kan_hash_storage_t script_storage;

    struct kan_atomic_int_t interned_string_lookup_storage_lock;
    struct kan_hash_storage_t interned_string_lookup_storage;
};

struct interned_string_registry_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t value;
    script_size_t index;
};

struct interned_string_registry_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t index_to_value;

    bool load_only;
    struct kan_atomic_int_t store_lock;
    struct kan_hash_storage_t value_to_index;
};

struct interned_string_registry_reader_t
{
    struct interned_string_registry_t *registry;
    struct kan_stream_t *stream;

    kan_serialized_size_t strings_total;
    kan_serialized_size_t strings_read;
};

struct interned_string_registry_writer_t
{
    struct interned_string_registry_t *registry;
    struct kan_stream_t *stream;

    kan_serialized_size_t strings_total;
    kan_serialized_size_t strings_written;
};

enum serialization_condition_value_t
{
    SERIALIZATION_CONDITION_NOT_CALCULATED = 0,
    SERIALIZATION_CONDITION_FAILED,
    SERIALIZATION_CONDITION_PASSED,
};

struct script_state_dynamic_array_suffix_t
{
    script_size_t items_total;
    script_size_t items_processed;
};

struct script_state_patch_read_suffix_t
{
    script_size_t blocks_total;
    script_size_t blocks_processed;
    script_size_t section_id_bound;
};

struct script_state_patch_write_suffix_t
{
    kan_reflection_patch_iterator_t current_iterator;
    kan_reflection_patch_iterator_t end_iterator;
};

struct script_state_patch_suffix_t
{
    kan_interned_string_t type_name;

    union
    {
        struct script_state_patch_read_suffix_t read;
        struct script_state_patch_write_suffix_t write;
    };
};

struct script_state_patch_dynamic_array_suffix_t
{
    struct script_state_dynamic_array_suffix_t array;
    struct script_state_patch_suffix_t current_patch;
};

struct script_state_t
{
    struct script_t *script;
    void *instance;

    enum serialization_condition_value_t *condition_values;
    script_size_t command_to_process_index;
    bool condition_checked;
    bool suffix_initialized;

    union
    {
        struct script_state_dynamic_array_suffix_t suffix_dynamic_array;
        struct script_state_patch_suffix_t suffix_patch;
        struct script_state_patch_dynamic_array_suffix_t suffix_patch_dynamic_array;
    };
};

struct patch_section_state_info_t
{
    enum kan_reflection_patch_section_type_t type;
    const struct kan_reflection_field_t *source_field;

    union
    {
        kan_reflection_patch_builder_section_t read_section;
    };
};

struct serialization_common_state_t
{
    struct script_storage_t *script_storage;
    struct interned_string_registry_t *optional_string_registry;
    struct kan_stream_t *stream;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct script_state_t)
    struct kan_dynamic_array_t script_state_stack;

    kan_instance_size_t patch_section_map_size;
    struct patch_section_state_info_t *patch_section_map;
    struct patch_section_state_info_t *last_patch_section_state;
};

struct serialization_read_state_t
{
    struct serialization_common_state_t common;

    kan_memory_size_t buffer_size;
    void *buffer;

    kan_reflection_patch_builder_t patch_builder;
    kan_allocation_group_t child_allocation_group;
};

struct serialization_write_state_t
{
    struct serialization_common_state_t common;
};

static kan_allocation_group_t script_storage_allocation_group;
static kan_allocation_group_t script_allocation_group;
static kan_allocation_group_t interned_string_lookup_allocation_group;
static kan_allocation_group_t script_generation_allocation_group;
static kan_allocation_group_t interned_string_lookup_generation_allocation_group;
static kan_allocation_group_t interned_string_registry_allocation_group;
static kan_allocation_group_t interned_string_registry_read_allocation_group;
static kan_allocation_group_t interned_string_registry_write_allocation_group;
static kan_allocation_group_t serialization_allocation_group;

static kan_interned_string_t interned_invalid_patch_type_t;

static bool statics_initialized = false;
static struct kan_atomic_int_t statics_initialization_lock = {.value = 0};

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        KAN_ATOMIC_INT_SCOPED_LOCK (&statics_initialization_lock)
        if (!statics_initialized)
        {
            script_storage_allocation_group =
                kan_allocation_group_get_child (kan_allocation_group_root (), "serialization_binary_script_storage");
            script_allocation_group = kan_allocation_group_get_child (script_storage_allocation_group, "script");
            interned_string_lookup_allocation_group =
                kan_allocation_group_get_child (script_storage_allocation_group, "interned_string_lookup");
            script_generation_allocation_group =
                kan_allocation_group_get_child (script_storage_allocation_group, "script_generation");
            interned_string_lookup_generation_allocation_group =
                kan_allocation_group_get_child (script_storage_allocation_group, "interned_string_lookup_generation");
            interned_string_registry_allocation_group = kan_allocation_group_get_child (
                kan_allocation_group_root (), "serialization_binary_interned_string_registry");
            interned_string_registry_read_allocation_group =
                kan_allocation_group_get_child (interned_string_registry_allocation_group, "read");
            interned_string_registry_write_allocation_group =
                kan_allocation_group_get_child (interned_string_registry_allocation_group, "write");
            serialization_allocation_group =
                kan_allocation_group_get_child (kan_allocation_group_root (), "serialization_binary");

            interned_invalid_patch_type_t = kan_string_intern ("invalid_patch_type_t");
            statics_initialized = true;
        }
    }
}

static struct script_node_t *script_storage_get_script_internal (struct script_storage_t *storage,
                                                                 kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&storage->script_storage, KAN_HASH_OBJECT_POINTER (type_name));
    struct script_node_t *node = (struct script_node_t *) bucket->first;
    const struct script_node_t *node_end = (struct script_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->type_name == type_name)
        {
            return node;
        }

        node = (struct script_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static struct script_node_t *script_storage_get_or_create_script (struct script_storage_t *storage,
                                                                  kan_interned_string_t type_name)
{
    KAN_ATOMIC_INT_SCOPED_LOCK (&storage->script_storage_lock)
    struct script_node_t *node = script_storage_get_script_internal (storage, type_name);

    if (!node)
    {
        ensure_statics_initialized ();
        node = (struct script_node_t *) kan_allocate_batched (script_allocation_group, sizeof (struct script_node_t));
        node->node.hash = KAN_HASH_OBJECT_POINTER (type_name);
        node->type_name = type_name;
        node->script_generation_lock = kan_atomic_int_init (0);
        node->script = NULL;

        kan_hash_storage_update_bucket_count_default (&storage->script_storage,
                                                      KAN_SERIALIZATION_BINARY_SCRIPT_INITIAL_BUCKETS);
        kan_hash_storage_add (&storage->script_storage, &node->node);
    }

    return node;
}

struct script_condition_temporary_node_t
{
    struct script_condition_temporary_node_t *next;
    struct script_condition_t condition;
};

struct script_command_temporary_node_t
{
    struct script_command_temporary_node_t *next;
    struct script_command_t command;
};

struct generation_temporary_state_t
{
    struct script_storage_t *storage;
    const struct kan_reflection_struct_t *struct_data;

    kan_instance_size_t conditions_count;
    kan_instance_size_t commands_count;

    struct script_condition_temporary_node_t *first_condition;
    struct script_condition_temporary_node_t *last_condition;

    struct script_command_temporary_node_t *first_command;
    struct script_command_temporary_node_t *last_command;

    struct kan_stack_group_allocator_t temporary_allocator;
};

static inline struct script_condition_t build_condition_from_reflection (struct kan_reflection_field_t *source_field)
{
    return (struct script_condition_t) {
        .condition_value_field = source_field->visibility_condition_field,
        .absolute_source_offset = (script_size_t) source_field->visibility_condition_field->offset,
        .condition_values_count = (script_size_t) source_field->visibility_condition_values_count,
        .condition_values = source_field->visibility_condition_values,
        .parent_condition_index = SCRIPT_NO_CONDITION,
    };
}

static inline script_size_t find_condition (struct generation_temporary_state_t *state,
                                            struct script_condition_t condition)
{
    struct script_condition_temporary_node_t *other_condition_node = state->first_condition;
    script_size_t condition_index = 0u;

    while (other_condition_node)
    {
        if (other_condition_node->condition.condition_value_field == condition.condition_value_field &&
            other_condition_node->condition.absolute_source_offset == condition.absolute_source_offset &&
            other_condition_node->condition.condition_values_count == condition.condition_values_count &&
            // We can just compare values pointers, because we are pointing to reflection data.
            other_condition_node->condition.condition_values == condition.condition_values &&
            other_condition_node->condition.parent_condition_index == condition.parent_condition_index)
        {
            return condition_index;
        }

        other_condition_node = other_condition_node->next;
        ++condition_index;
    }

    return SCRIPT_NO_CONDITION;
}

static inline void add_condition (struct generation_temporary_state_t *state, struct script_condition_t condition)
{
    struct script_condition_temporary_node_t *new_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &state->temporary_allocator, struct script_condition_temporary_node_t);
    new_node->next = NULL;
    new_node->condition = condition;

    if (state->last_condition)
    {
        state->last_condition->next = new_node;
        state->last_condition = new_node;
    }
    else
    {
        state->first_condition = new_node;
        state->last_condition = new_node;
    }

    ++state->conditions_count;
}

static inline struct script_command_t build_block_command (script_size_t condition_index,
                                                           script_size_t offset,
                                                           script_size_t size)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_BLOCK,
        .condition_index = condition_index,
        .offset = offset,
        .block =
            {
                .size = size,
            },
    };
}

static inline struct script_command_t build_string_command (script_size_t condition_index, script_size_t offset)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_STRING,
        .condition_index = condition_index,
        .offset = offset,
    };
}

static inline struct script_command_t build_interned_string_command (script_size_t condition_index,
                                                                     script_size_t offset)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_INTERNED_STRING,
        .condition_index = condition_index,
        .offset = offset,
    };
}

static inline struct script_command_t build_block_dynamic_array_command (script_size_t condition_index,
                                                                         script_size_t offset,
                                                                         script_size_t item_size)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_BLOCK_DYNAMIC_ARRAY,
        .condition_index = condition_index,
        .offset = offset,
    };
}

static inline struct script_command_t build_string_dynamic_array_command (script_size_t condition_index,
                                                                          script_size_t offset)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_STRING_DYNAMIC_ARRAY,
        .condition_index = condition_index,
        .offset = offset,
    };
}

static inline struct script_command_t build_interned_string_dynamic_array_command (script_size_t condition_index,
                                                                                   script_size_t offset)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_INTERNED_STRING_DYNAMIC_ARRAY,
        .condition_index = condition_index,
        .offset = offset,
    };
}

static inline struct script_command_t build_struct_dynamic_array_command (script_size_t condition_index,
                                                                          script_size_t offset,
                                                                          kan_interned_string_t type_name)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_STRUCT_DYNAMIC_ARRAY,
        .condition_index = condition_index,
        .offset = offset,
        .struct_dynamic_array =
            {
                .type_name = type_name,
            },
    };
}

static inline struct script_command_t build_patch_dynamic_array_command (script_size_t condition_index,
                                                                         script_size_t offset)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_PATCH_DYNAMIC_ARRAY,
        .condition_index = condition_index,
        .offset = offset,
    };
}

static inline struct script_command_t build_patch_command (script_size_t condition_index, script_size_t offset)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_PATCH,
        .condition_index = condition_index,
        .offset = offset,
    };
}

static inline void add_command (struct generation_temporary_state_t *state, struct script_command_t command)
{
    // We merge block commands in order to perform serialization more effectively.
    if (command.type == SCRIPT_COMMAND_BLOCK && state->last_command &&
        state->last_command->command.type == SCRIPT_COMMAND_BLOCK &&
        state->last_command->command.condition_index == command.condition_index &&
        state->last_command->command.offset + state->last_command->command.block.size == command.offset)
    {
        state->last_command->command.block.size += command.block.size;
        return;
    }

    struct script_command_temporary_node_t *new_node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&state->temporary_allocator, struct script_command_temporary_node_t);

    new_node->next = NULL;
    new_node->command.type = command.type;
    new_node->command.condition_index = command.condition_index;
    new_node->command.offset = command.offset;

    switch (command.type)
    {
    case SCRIPT_COMMAND_BLOCK:
        new_node->command.block = command.block;
        break;

    case SCRIPT_COMMAND_STRING:
    case SCRIPT_COMMAND_INTERNED_STRING:
    case SCRIPT_COMMAND_BLOCK_DYNAMIC_ARRAY:
    case SCRIPT_COMMAND_STRING_DYNAMIC_ARRAY:
    case SCRIPT_COMMAND_INTERNED_STRING_DYNAMIC_ARRAY:
    case SCRIPT_COMMAND_PATCH_DYNAMIC_ARRAY:
    case SCRIPT_COMMAND_PATCH:
        break;

    case SCRIPT_COMMAND_STRUCT_DYNAMIC_ARRAY:
        new_node->command.struct_dynamic_array = command.struct_dynamic_array;
        break;
    }

    if (state->last_command)
    {
        state->last_command->next = new_node;
        state->last_command = new_node;
    }
    else
    {
        state->first_command = new_node;
        state->last_command = new_node;
    }

    ++state->commands_count;
}

static void script_storage_ensure_script_generated (struct script_storage_t *storage, struct script_node_t *node);

static inline void add_struct_commands (struct generation_temporary_state_t *state,
                                        kan_interned_string_t type_name,
                                        kan_instance_size_t field_offset,
                                        script_size_t condition_index)
{
    struct script_node_t *script_node = script_storage_get_or_create_script (state->storage, type_name);
    script_storage_ensure_script_generated (state->storage, script_node);

    const script_size_t condition_index_offset = state->conditions_count;
    const struct script_condition_t *condition = (const struct script_condition_t *) script_node->script->data;

    for (script_size_t index = 0u; index < script_node->script->conditions_count; ++index, ++condition)
    {
        struct script_condition_t new_condition = *condition;
        new_condition.absolute_source_offset += (script_size_t) field_offset;

        if (new_condition.parent_condition_index == SCRIPT_NO_CONDITION)
        {
            new_condition.parent_condition_index = condition_index;
        }
        else
        {
            new_condition.parent_condition_index += condition_index_offset;
        }

        add_condition (state, new_condition);
    }

    const struct script_command_t *command = (const struct script_command_t *) condition;
    for (script_size_t index = 0u; index < script_node->script->commands_count; ++index, ++command)
    {
        struct script_command_t new_command = *command;
        if (new_command.condition_index == SCRIPT_NO_CONDITION)
        {
            new_command.condition_index = condition_index;
        }
        else
        {
            new_command.condition_index += condition_index_offset;
        }

        new_command.offset += (script_size_t) field_offset;
        add_command (state, new_command);
    }
}

static_assert (sizeof (enum kan_reflection_archetype_t) == sizeof (kan_reflection_enum_size_t),
               "Enums have expected size and we do not risk breaking binary serialization.");

static inline void add_field_to_commands (struct generation_temporary_state_t *state,
                                          struct kan_reflection_field_t *field,
                                          script_size_t condition_index)
{
    switch (field->archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_FLOATING:
    case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
    case KAN_REFLECTION_ARCHETYPE_ENUM:
        add_command (state,
                     build_block_command (condition_index, (script_size_t) field->offset, (script_size_t) field->size));
        break;

    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        add_command (state, build_string_command (condition_index, (script_size_t) field->offset));
        break;

    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        add_command (state, build_interned_string_command (condition_index, (script_size_t) field->offset));
        break;

    case KAN_REFLECTION_ARCHETYPE_STRUCT:
        add_struct_commands (state, field->archetype_struct.type_name, (script_size_t) field->offset, condition_index);
        break;

    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        switch (field->archetype_inline_array.item_archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
            add_command (state, build_block_command (condition_index, (script_size_t) field->offset,
                                                     (script_size_t) (field->archetype_inline_array.item_size *
                                                                      field->archetype_inline_array.item_count)));
            break;

        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            for (kan_loop_size_t index = 0u; index < field->archetype_inline_array.item_count; ++index)
            {
                add_command (state,
                             build_string_command (
                                 condition_index,
                                 (script_size_t) (field->offset + field->archetype_inline_array.item_size * index)));
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            for (kan_loop_size_t index = 0u; index < field->archetype_inline_array.item_count; ++index)
            {
                add_command (state,
                             build_interned_string_command (
                                 condition_index,
                                 (script_size_t) (field->offset + field->archetype_inline_array.item_size * index)));
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
            for (kan_loop_size_t index = 0u; index < field->archetype_inline_array.item_count; ++index)
            {
                add_struct_commands (state, field->archetype_inline_array.item_archetype_struct.type_name,
                                     (script_size_t) (field->offset + field->archetype_inline_array.item_size * index),
                                     condition_index);
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                     "Struct \"%s\" contains field \"%s\" that is an inline array of external pointers and cannot be "
                     "serialized.",
                     state->struct_data->name, field->name)
            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                     "Struct \"%s\" contains field \"%s\" that is an inline array of struct pointers and cannot be "
                     "serialized.",
                     state->struct_data->name, field->name)
            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            KAN_ASSERT (false)
            break;

        case KAN_REFLECTION_ARCHETYPE_PATCH:
            for (kan_loop_size_t index = 0u; index < field->archetype_inline_array.item_count; ++index)
            {
                add_command (state,
                             build_patch_command (
                                 condition_index,
                                 (script_size_t) (field->offset + field->archetype_inline_array.item_size * index)));
            }

            break;
        }

        break;

    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        switch (field->archetype_dynamic_array.item_archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
            add_command (state,
                         build_block_dynamic_array_command (condition_index, (script_size_t) field->offset,
                                                            (script_size_t) field->archetype_dynamic_array.item_size));
            break;

        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            add_command (state, build_string_dynamic_array_command (condition_index, (script_size_t) field->offset));
            break;

        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            add_command (state,
                         build_interned_string_dynamic_array_command (condition_index, (script_size_t) field->offset));
            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
            add_command (state, build_struct_dynamic_array_command (
                                    condition_index, (script_size_t) field->offset,
                                    field->archetype_dynamic_array.item_archetype_struct.type_name));
            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                     "Struct \"%s\" contains field \"%s\" that is a dynamic array of struct pointers and cannot be "
                     "serialized.",
                     state->struct_data->name, field->name)
            break;

        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                     "Struct \"%s\" contains field \"%s\" that is a dynamic array of external pointers and cannot be "
                     "serialized.",
                     state->struct_data->name, field->name)
            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            KAN_ASSERT (false)
            break;

        case KAN_REFLECTION_ARCHETYPE_PATCH:
            add_command (state, build_patch_dynamic_array_command (condition_index, (script_size_t) field->offset));
            break;
        }

        break;

    case KAN_REFLECTION_ARCHETYPE_PATCH:
        add_command (state, build_patch_command (condition_index, (script_size_t) field->offset));
        break;

    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                 "Struct \"%s\" contains field \"%s\" that is an external pointer and cannot be serialized.",
                 state->struct_data->name, field->name)
        break;

    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                 "Struct \"%s\" contains field \"%s\" that is a struct pointer and cannot be serialized.",
                 state->struct_data->name, field->name)
        break;
    }
}

static void script_storage_ensure_script_generated (struct script_storage_t *storage, struct script_node_t *node)
{
    if (node->script)
    {
        return;
    }

    KAN_ATOMIC_INT_SCOPED_LOCK (&node->script_generation_lock);
    if (node->script)
    {
        return;
    }

    ensure_statics_initialized ();
    struct generation_temporary_state_t state;
    state.storage = storage;

    state.struct_data = kan_reflection_registry_query_struct (storage->registry, node->type_name);
    KAN_ASSERT (state.struct_data)

    state.conditions_count = 0u;
    state.commands_count = 0u;

    state.first_condition = NULL;
    state.last_condition = NULL;

    state.first_command = NULL;
    state.last_command = NULL;

    kan_stack_group_allocator_init (&state.temporary_allocator, script_generation_allocation_group,
                                    KAN_SERIALIZATION_BINARY_GENERATION_INITIAL_STACK);

    for (kan_loop_size_t field_index = 0u; field_index < state.struct_data->fields_count; ++field_index)
    {
        struct kan_reflection_field_t *field = &state.struct_data->fields[field_index];
        script_size_t condition_index = SCRIPT_NO_CONDITION;

        if (field->visibility_condition_field)
        {
            const struct script_condition_t condition = build_condition_from_reflection (field);
            condition_index = find_condition (&state, condition);

            if (condition_index == SCRIPT_NO_CONDITION)
            {
                add_condition (&state, condition);
                condition_index = state.conditions_count - 1u;
            }
        }

        add_field_to_commands (&state, field, condition_index);
    }

    static_assert (alignof (struct script_t) == alignof (struct script_condition_t), "Script parts alignment match.");
    static_assert (alignof (struct script_t) == alignof (struct script_command_t), "Script parts alignment match.");

    struct script_t *script = (struct script_t *) kan_allocate_general (
        script_allocation_group,
        sizeof (struct script_t) + state.conditions_count * sizeof (struct script_condition_t) +
            state.commands_count * sizeof (struct script_command_t),
        alignof (struct script_t));

    script->conditions_count = state.conditions_count;
    script->commands_count = state.commands_count;

    struct script_condition_t *condition_output = (struct script_condition_t *) script->data;
    struct script_condition_temporary_node_t *condition = state.first_condition;

    while (condition)
    {
        *condition_output = condition->condition;
        ++condition_output;
        condition = condition->next;
    }

    struct script_command_t *command_output = (struct script_command_t *) condition_output;
    struct script_command_temporary_node_t *command = state.first_command;

    while (command)
    {
        command_output->type = command->command.type;
        command_output->condition_index = command->command.condition_index;
        command_output->offset = command->command.offset;

        switch (command->command.type)
        {
        case SCRIPT_COMMAND_BLOCK:
            command_output->block = command->command.block;
            break;

        case SCRIPT_COMMAND_STRING:
        case SCRIPT_COMMAND_INTERNED_STRING:
        case SCRIPT_COMMAND_BLOCK_DYNAMIC_ARRAY:
        case SCRIPT_COMMAND_STRING_DYNAMIC_ARRAY:
        case SCRIPT_COMMAND_INTERNED_STRING_DYNAMIC_ARRAY:
        case SCRIPT_COMMAND_PATCH_DYNAMIC_ARRAY:
        case SCRIPT_COMMAND_PATCH:
            break;

        case SCRIPT_COMMAND_STRUCT_DYNAMIC_ARRAY:
            command_output->struct_dynamic_array = command->command.struct_dynamic_array;
            break;
        }

        ++command_output;
        command = command->next;
    }

    node->script = script;
    kan_stack_group_allocator_shutdown (&state.temporary_allocator);
}

static struct interned_string_lookup_node_t *script_storage_get_interned_string_lookup_internal (
    struct script_storage_t *storage, kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&storage->interned_string_lookup_storage, KAN_HASH_OBJECT_POINTER (type_name));
    struct interned_string_lookup_node_t *node = (struct interned_string_lookup_node_t *) bucket->first;
    const struct interned_string_lookup_node_t *node_end =
        (struct interned_string_lookup_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->type_name == type_name)
        {
            return node;
        }

        node = (struct interned_string_lookup_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static struct interned_string_lookup_node_t *script_storage_get_or_create_interned_string_lookup (
    struct script_storage_t *storage, kan_interned_string_t type_name)
{
    KAN_ATOMIC_INT_SCOPED_LOCK (&storage->interned_string_lookup_storage_lock)
    struct interned_string_lookup_node_t *node =
        script_storage_get_interned_string_lookup_internal (storage, type_name);

    if (!node)
    {
        ensure_statics_initialized ();
        node = (struct interned_string_lookup_node_t *) kan_allocate_batched (
            interned_string_lookup_allocation_group, sizeof (struct interned_string_lookup_node_t));

        node->node.hash = KAN_HASH_OBJECT_POINTER (type_name);
        node->type_name = type_name;
        node->interned_string_absolute_positions_generation_lock = kan_atomic_int_init (0);
        node->interned_string_absolute_positions_generated = kan_atomic_int_init (0);
        node->interned_string_absolute_positions_count = 0u;
        node->interned_string_absolute_positions = NULL;

        kan_hash_storage_update_bucket_count_default (&storage->interned_string_lookup_storage,
                                                      KAN_SERIALIZATION_BINARY_INTERNED_STRING_BUCKETS);
        kan_hash_storage_add (&storage->interned_string_lookup_storage, &node->node);
    }

    return node;
}

static inline void add_to_script_size_array (struct kan_dynamic_array_t *array, script_size_t value)
{
    void *spot = kan_dynamic_array_add_last (array);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (array, array->capacity * 2u);
        spot = kan_dynamic_array_add_last (array);
    }

    KAN_ASSERT (spot)
    *(script_size_t *) spot = value;
}

static void script_storage_ensure_interned_string_lookup_generated (struct script_storage_t *storage,
                                                                    struct interned_string_lookup_node_t *node);

static inline void add_struct_interned_string_lookup (struct kan_dynamic_array_t *temporary_array,
                                                      struct script_storage_t *storage,
                                                      kan_interned_string_t type_name,
                                                      script_size_t offset)
{
    struct interned_string_lookup_node_t *other_node =
        script_storage_get_or_create_interned_string_lookup (storage, type_name);
    script_storage_ensure_interned_string_lookup_generated (storage, other_node);

    for (kan_loop_size_t index = 0u; index < other_node->interned_string_absolute_positions_count; ++index)
    {
        add_to_script_size_array (temporary_array, other_node->interned_string_absolute_positions[index] + offset);
    }
}

static inline bool error_if_struct_has_interned_strings (struct script_storage_t *storage,
                                                         kan_interned_string_t type_name)
{
    const struct kan_reflection_struct_t *struct_data =
        kan_reflection_registry_query_struct (storage->registry, type_name);
    KAN_ASSERT (struct_data)

    for (kan_loop_size_t field_index = 0u; field_index < struct_data->fields_count; ++field_index)
    {
        if (struct_data->fields[field_index].archetype == KAN_REFLECTION_ARCHETYPE_INTERNED_STRING ||
            (struct_data->fields[field_index].archetype == KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY &&
             struct_data->fields[field_index].archetype_inline_array.item_archetype ==
                 KAN_REFLECTION_ARCHETYPE_INTERNED_STRING))
        {
            KAN_LOG (serialization_binary, KAN_LOG_ERROR, "    (field path for the error below) %s.%s",
                     struct_data->name, struct_data->fields[field_index].name)
            return true;
        }
    }

    return false;
}

static void script_storage_ensure_interned_string_lookup_generated (struct script_storage_t *storage,
                                                                    struct interned_string_lookup_node_t *node)
{
    if (kan_atomic_int_get (&node->interned_string_absolute_positions_generated))
    {
        return;
    }

    KAN_ATOMIC_INT_SCOPED_LOCK (&node->interned_string_absolute_positions_generation_lock)
    if (kan_atomic_int_get (&node->interned_string_absolute_positions_generated))
    {
        return;
    }

    ensure_statics_initialized ();
    struct kan_dynamic_array_t temporary_array;
    kan_dynamic_array_init (&temporary_array, KAN_SERIALIZATION_BINARY_INTERNED_STRING_CAPACITY, sizeof (script_size_t),
                            alignof (script_size_t), interned_string_lookup_generation_allocation_group);

    const struct kan_reflection_struct_t *struct_data =
        kan_reflection_registry_query_struct (storage->registry, node->type_name);
    KAN_ASSERT (struct_data)

    for (kan_loop_size_t field_index = 0u; field_index < struct_data->fields_count; ++field_index)
    {
        struct kan_reflection_field_t *field = &struct_data->fields[field_index];
        if (field->visibility_condition_field)
        {
            switch (field->archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                         "Struct \"%s\" contains field \"%s\" that is an interned string and has visibility condition "
                         "(for example, part of a union). Interned string lookup sequence was requested for this "
                         "struct (possibly due to usage in patches), but this field cannot be safely included due to"
                         "the fact that visibility condition is not guarantied to be stored inside patch.",
                         struct_data->name, field->name)
                break;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
            {
                if (error_if_struct_has_interned_strings (storage, field->archetype_struct.type_name))
                {
                    KAN_LOG (
                        serialization_binary, KAN_LOG_ERROR,
                        "Struct \"%s\" contains field \"%s\" that is a struct with interned string and has visibility "
                        "condition (for example, part of a union). Interned string lookup sequence was requested for "
                        "this struct (possibly due to usage in patches), but this field cannot be safely included due "
                        "to the fact that visibility condition is not guarantied to be stored inside patch.",
                        struct_data->name, field->name)
                }

                break;
            }

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                switch (field->archetype_inline_array.item_archetype)
                {
                case KAN_REFLECTION_ARCHETYPE_STRUCT:
                    if (error_if_struct_has_interned_strings (
                            storage, field->archetype_inline_array.item_archetype_struct.type_name))
                    {
                        KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                                 "Struct \"%s\" contains field \"%s\" that is a struct with interned string and has "
                                 "visibility condition (for example, part of a union). Interned string lookup sequence "
                                 "was requested for his struct (possibly due to usage in patches), but this field "
                                 "cannot be safely included due to the fact that visibility condition is not "
                                 "guarantied to be stored inside patch.",
                                 struct_data->name, field->name)
                    }

                    break;

                case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                    KAN_LOG (
                        serialization_binary, KAN_LOG_ERROR,
                        "Struct \"%s\" contains field \"%s\" that is an interned string array and has visibility "
                        "condition (for example, part of a union). Interned string lookup sequence was requested for "
                        "this struct (possibly due to usage in patches), but this field cannot be safely included due "
                        "to the fact that visibility condition is not guarantied to be stored inside patch.",
                        struct_data->name, field->name)

                    break;

                case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                case KAN_REFLECTION_ARCHETYPE_FLOATING:
                case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
                case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
                case KAN_REFLECTION_ARCHETYPE_ENUM:
                case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                case KAN_REFLECTION_ARCHETYPE_PATCH:
                    break;
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_FLOATING:
            case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_PATCH:
                break;
            }

            continue;
        }

        KAN_ASSERT (field->offset < UINT32_MAX)
        switch (field->archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            add_to_script_size_array (&temporary_array, (script_size_t) field->offset);
            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
        {
            add_struct_interned_string_lookup (&temporary_array, storage, field->archetype_struct.type_name,
                                               (script_size_t) field->offset);
            break;
        }

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            switch (field->archetype_inline_array.item_archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                for (kan_loop_size_t index = 0u; index < field->archetype_inline_array.item_count; ++index)
                {
                    add_struct_interned_string_lookup (
                        &temporary_array, storage, field->archetype_inline_array.item_archetype_struct.type_name,
                        (script_size_t) (field->offset + index * field->archetype_inline_array.item_size));
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                for (kan_loop_size_t index = 0u; index < field->archetype_inline_array.item_count; ++index)
                {
                    add_to_script_size_array (&temporary_array,
                                              (script_size_t) (field->offset + index * sizeof (kan_interned_string_t)));
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_FLOATING:
            case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_PATCH:
                break;
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_PATCH:
            break;
        }
    }

    node->interned_string_absolute_positions_count = (kan_instance_size_t) temporary_array.size;
    if (node->interned_string_absolute_positions_count > 0u)
    {
        node->interned_string_absolute_positions = kan_allocate_general (
            interned_string_lookup_allocation_group,
            sizeof (script_size_t) * node->interned_string_absolute_positions_count, alignof (script_size_t));

        memcpy (node->interned_string_absolute_positions, temporary_array.data,
                sizeof (script_size_t) * node->interned_string_absolute_positions_count);
    }

    kan_atomic_int_set (&node->interned_string_absolute_positions_generated, 1);
    kan_dynamic_array_shutdown (&temporary_array);
}

static struct interned_string_registry_t *interned_string_registry_create (bool load_only)
{
    ensure_statics_initialized ();
    struct interned_string_registry_t *registry = (struct interned_string_registry_t *) kan_allocate_general (
        interned_string_registry_allocation_group, sizeof (struct interned_string_registry_t),
        alignof (struct interned_string_registry_t));

    kan_dynamic_array_init (
        &registry->index_to_value,
        KAN_SERIALIZATION_BINARY_INTERNED_REGISTRY_BUCKETS * KAN_CONTAINER_HASH_STORAGE_DEFAULT_LOAD_FACTOR,
        sizeof (kan_interned_string_t), alignof (kan_interned_string_t), interned_string_registry_allocation_group);

    registry->load_only = load_only;
    if (!registry->load_only)
    {
        registry->store_lock = kan_atomic_int_init (0);
        kan_hash_storage_init (&registry->value_to_index, interned_string_registry_allocation_group,
                               KAN_SERIALIZATION_BINARY_INTERNED_REGISTRY_BUCKETS);
    }

    return registry;
}

static kan_serialized_size_t interned_string_registry_add_string_internal (struct interned_string_registry_t *registry,
                                                                           kan_interned_string_t interned_string)
{
    KAN_ASSERT (registry->index_to_value.size <= UINT32_MAX)
    const script_size_t index = (kan_serialized_size_t) registry->index_to_value.size;
    void *spot = kan_dynamic_array_add_last (&registry->index_to_value);

    if (!spot)
    {
        kan_dynamic_array_set_capacity (&registry->index_to_value, registry->index_to_value.capacity * 2u);
        spot = kan_dynamic_array_add_last (&registry->index_to_value);
    }

    KAN_ASSERT (spot)
    *(kan_interned_string_t *) spot = interned_string;

    if (!registry->load_only)
    {
        ensure_statics_initialized ();
        struct interned_string_registry_node_t *node = kan_allocate_batched (
            interned_string_registry_allocation_group, sizeof (struct interned_string_registry_node_t));

        node->node.hash = KAN_HASH_OBJECT_POINTER (interned_string);
        node->value = interned_string;
        node->index = index;

        kan_hash_storage_update_bucket_count_default (&registry->value_to_index,
                                                      KAN_SERIALIZATION_BINARY_INTERNED_REGISTRY_BUCKETS);
        kan_hash_storage_add (&registry->value_to_index, &node->node);
    }

    return index;
}

static kan_serialized_size_t interned_string_registry_store_string (struct interned_string_registry_t *registry,
                                                                    kan_interned_string_t interned_string)
{
    KAN_ASSERT (!registry->load_only)
    KAN_ATOMIC_INT_SCOPED_LOCK (&registry->store_lock)

    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&registry->value_to_index, KAN_HASH_OBJECT_POINTER (interned_string));
    struct interned_string_registry_node_t *node = (struct interned_string_registry_node_t *) bucket->first;
    const struct interned_string_registry_node_t *node_end =
        (struct interned_string_registry_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->value == interned_string)
        {
            return node->index;
        }

        node = (struct interned_string_registry_node_t *) node->node.list_node.next;
    }

    return interned_string_registry_add_string_internal (registry, interned_string);
}

static kan_interned_string_t interned_string_registry_load_string (struct interned_string_registry_t *registry,
                                                                   script_size_t index)
{
    KAN_ASSERT (registry->load_only || kan_atomic_int_get (&registry->store_lock) == 0)
    if (index >= registry->index_to_value.size)
    {
        KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                 "Encountered interned string index that is too big for the registry: %u.", (unsigned) index);
        return NULL;
    }

    return ((kan_interned_string_t *) registry->index_to_value.data)[index];
}

static inline void script_state_go_to_next_command (struct script_state_t *state)
{
    ++state->command_to_process_index;
    state->condition_checked = false;
    state->suffix_initialized = false;
}

static void calculate_condition (void *instance,
                                 enum serialization_condition_value_t *condition_array,
                                 script_size_t condition_index,
                                 struct script_condition_t *script_conditions)
{
    KAN_ASSERT (condition_array[condition_index] == SERIALIZATION_CONDITION_NOT_CALCULATED)
    bool condition_valid = true;
    const script_size_t parent_condition = script_conditions[condition_index].parent_condition_index;

    if (parent_condition != SCRIPT_NO_CONDITION)
    {
        if (condition_array[parent_condition] == SERIALIZATION_CONDITION_NOT_CALCULATED)
        {
            calculate_condition (instance, condition_array, parent_condition, script_conditions);
        }

        if (condition_array[parent_condition] == SERIALIZATION_CONDITION_FAILED)
        {
            condition_valid = false;
        }
    }

    condition_valid &= kan_reflection_check_visibility (
        script_conditions[condition_index].condition_value_field,
        script_conditions[condition_index].condition_values_count, script_conditions[condition_index].condition_values,
        ((const uint8_t *) instance) + script_conditions[condition_index].absolute_source_offset);

    if (condition_valid)
    {
        condition_array[condition_index] = SERIALIZATION_CONDITION_PASSED;
    }
    else
    {
        condition_array[condition_index] = SERIALIZATION_CONDITION_FAILED;
    }
}

static inline void serialization_common_state_init (
    struct serialization_common_state_t *state,
    struct kan_stream_t *stream,
    kan_serialization_binary_script_storage_t script_storage,
    kan_serialization_interned_string_registry_t interned_string_registry)
{
    state->script_storage = KAN_HANDLE_GET (script_storage);
    KAN_ASSERT (state->script_storage != NULL)
    state->optional_string_registry = KAN_HANDLE_GET (interned_string_registry);
    state->stream = stream;

    kan_dynamic_array_init (&state->script_state_stack, 4u, sizeof (struct script_state_t),
                            alignof (struct script_state_t), serialization_allocation_group);

    state->patch_section_map_size = 0u;
    state->patch_section_map = NULL;
    state->last_patch_section_state = NULL;
}

static inline void serialization_common_state_push_script_state (
    struct serialization_common_state_t *serialization_state,
    struct script_t *script,
    void *instance,
    bool calculate_conditions)
{
    struct script_state_t *script_state =
        (struct script_state_t *) kan_dynamic_array_add_last (&serialization_state->script_state_stack);
    if (!script_state)
    {
        kan_dynamic_array_set_capacity (&serialization_state->script_state_stack,
                                        serialization_state->script_state_stack.capacity * 2u);
    }

    KAN_ASSERT (script_state)
    script_state->script = script;
    script_state->instance = instance;

    if (script->conditions_count > 0u)
    {
        ensure_statics_initialized ();
        script_state->condition_values = kan_allocate_general (
            serialization_allocation_group, sizeof (enum serialization_condition_value_t) * script->conditions_count,
            alignof (enum serialization_condition_value_t));

        if (calculate_conditions)
        {
            for (script_size_t index = 0u; index < script->conditions_count; ++index)
            {
                script_state->condition_values[index] = SERIALIZATION_CONDITION_NOT_CALCULATED;
                calculate_condition (instance, script_state->condition_values, index,
                                     (struct script_condition_t *) script->data);
            }
        }
        else
        {
            for (script_size_t index = 0u; index < script->conditions_count; ++index)
            {
                script_state->condition_values[index] = SERIALIZATION_CONDITION_NOT_CALCULATED;
            }
        }
    }
    else
    {
        script_state->condition_values = NULL;
    }

    script_state->command_to_process_index = 0u;
    script_state->suffix_initialized = false;
}

static inline void serialization_common_state_pop_script_state (
    struct serialization_common_state_t *serialization_state)
{
    KAN_ASSERT (serialization_state->script_state_stack.size > 0u)
    struct script_state_t *last_state =
        &((struct script_state_t *)
              serialization_state->script_state_stack.data)[serialization_state->script_state_stack.size - 1u];

    kan_free_general (serialization_allocation_group, last_state->condition_values,
                      sizeof (enum serialization_condition_value_t) * last_state->script->conditions_count);

    kan_dynamic_array_remove_swap_at (&serialization_state->script_state_stack,
                                      serialization_state->script_state_stack.size - 1u);
}

static inline void serialization_common_state_shutdown (struct serialization_common_state_t *state)
{
    while (state->script_state_stack.size > 0u)
    {
        serialization_common_state_pop_script_state (state);
    }

    kan_dynamic_array_shutdown (&state->script_state_stack);
    if (state->patch_section_map)
    {
        kan_free_general (serialization_allocation_group, state->patch_section_map,
                          sizeof (struct patch_section_state_info_t) * state->patch_section_map_size);
    }
}

kan_serialization_binary_script_storage_t kan_serialization_binary_script_storage_create (
    kan_reflection_registry_t registry)
{
    ensure_statics_initialized ();
    struct script_storage_t *script_storage = (struct script_storage_t *) kan_allocate_general (
        script_storage_allocation_group, sizeof (struct script_storage_t), alignof (struct script_storage_t));

    script_storage->registry = registry;
    script_storage->script_storage_lock = kan_atomic_int_init (0);

    kan_hash_storage_init (&script_storage->script_storage, script_allocation_group,
                           KAN_SERIALIZATION_BINARY_SCRIPT_INITIAL_BUCKETS);

    script_storage->interned_string_lookup_storage_lock = kan_atomic_int_init (0);
    kan_hash_storage_init (&script_storage->interned_string_lookup_storage, interned_string_lookup_allocation_group,
                           KAN_SERIALIZATION_BINARY_INTERNED_STRING_BUCKETS);

    return KAN_HANDLE_SET (kan_serialization_binary_script_storage_t, script_storage);
}

void kan_serialization_binary_script_storage_destroy (kan_serialization_binary_script_storage_t storage)
{
    ensure_statics_initialized ();
    struct script_storage_t *script_storage = KAN_HANDLE_GET (storage);
    struct script_node_t *script_node = (struct script_node_t *) script_storage->script_storage.items.first;

    while (script_node)
    {
        struct script_node_t *next = (struct script_node_t *) script_node->node.list_node.next;
        if (script_node->script)
        {
            kan_free_general (script_allocation_group, script_node->script,
                              sizeof (struct script_t) +
                                  sizeof (struct script_condition_t) * script_node->script->conditions_count +
                                  sizeof (struct script_command_t) * script_node->script->commands_count);
        }

        kan_free_batched (script_allocation_group, script_node);
        script_node = next;
    }

    struct interned_string_lookup_node_t *lookup_node =
        (struct interned_string_lookup_node_t *) script_storage->interned_string_lookup_storage.items.first;

    while (lookup_node)
    {
        struct interned_string_lookup_node_t *next =
            (struct interned_string_lookup_node_t *) lookup_node->node.list_node.next;

        if (lookup_node->interned_string_absolute_positions)
        {
            kan_free_general (interned_string_lookup_allocation_group, lookup_node->interned_string_absolute_positions,
                              sizeof (script_size_t) * lookup_node->interned_string_absolute_positions_count);
        }

        kan_free_batched (interned_string_lookup_allocation_group, lookup_node);
        lookup_node = next;
    }

    kan_hash_storage_shutdown (&script_storage->script_storage);
    kan_hash_storage_shutdown (&script_storage->interned_string_lookup_storage);
    kan_free_general (script_storage_allocation_group, script_storage, sizeof (struct script_storage_t));
}

kan_serialization_interned_string_registry_t kan_serialization_interned_string_registry_create_empty (void)
{
    return KAN_HANDLE_SET (kan_serialization_interned_string_registry_t, interned_string_registry_create (false));
}

void kan_serialization_interned_string_registry_destroy (kan_serialization_interned_string_registry_t registry)
{
    struct interned_string_registry_t *data = KAN_HANDLE_GET (registry);
    kan_dynamic_array_shutdown (&data->index_to_value);

    if (!data->load_only)
    {
        struct interned_string_registry_node_t *node =
            (struct interned_string_registry_node_t *) data->value_to_index.items.first;

        while (node)
        {
            struct interned_string_registry_node_t *next =
                (struct interned_string_registry_node_t *) node->node.list_node.next;
            kan_free_batched (interned_string_registry_allocation_group, node);
            node = next;
        }

        kan_hash_storage_shutdown (&data->value_to_index);
    }

    kan_free_general (interned_string_registry_allocation_group, data, sizeof (struct interned_string_registry_t));
}

kan_serialization_interned_string_registry_reader_t kan_serialization_interned_string_registry_reader_create (
    struct kan_stream_t *stream, bool load_only_registry)
{
    ensure_statics_initialized ();
    struct interned_string_registry_reader_t *reader =
        (struct interned_string_registry_reader_t *) kan_allocate_general (
            interned_string_registry_read_allocation_group, sizeof (struct interned_string_registry_reader_t),
            alignof (struct interned_string_registry_reader_t));

    reader->registry = interned_string_registry_create (load_only_registry);
    reader->stream = stream;
    KAN_ASSERT (kan_stream_is_readable (stream));

    if (reader->stream->operations->read (reader->stream, sizeof (kan_serialized_size_t), &reader->strings_total) !=
        sizeof (kan_serialized_size_t))
    {
        reader->strings_total = 0u;
        KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                 "Failed to initialize interned string registry reader: unable to read strings count from stream.")
    }

    reader->strings_read = 0u;
    kan_dynamic_array_set_capacity (&reader->registry->index_to_value, reader->strings_total);
    return KAN_HANDLE_SET (kan_serialization_interned_string_registry_reader_t, reader);
}

enum kan_serialization_state_t kan_serialization_interned_string_registry_reader_step (
    kan_serialization_interned_string_registry_reader_t reader)
{
    struct interned_string_registry_reader_t *data = KAN_HANDLE_GET (reader);
    if (data->strings_read >= data->strings_total)
    {
        return KAN_SERIALIZATION_FINISHED;
    }

    kan_serialized_size_t string_length;
    if (data->stream->operations->read (data->stream, sizeof (kan_serialized_size_t), &string_length) !=
        sizeof (kan_serialized_size_t))
    {
        return KAN_SERIALIZATION_FAILED;
    }

    if (string_length == 0u)
    {
        interned_string_registry_add_string_internal (data->registry, NULL);
        ++data->strings_read;
        return data->strings_read >= data->strings_total ? KAN_SERIALIZATION_FINISHED : KAN_SERIALIZATION_IN_PROGRESS;
    }

#define MAX_SIZE_ON_STACK 1023u
    ensure_statics_initialized ();
    char read_buffer_on_stack[MAX_SIZE_ON_STACK + 1u];
    char *read_buffer;

    if (string_length >= MAX_SIZE_ON_STACK)
    {
        read_buffer =
            kan_allocate_general (interned_string_registry_read_allocation_group, string_length + 1u, alignof (char));
    }
    else
    {
        read_buffer = read_buffer_on_stack;
    }
#undef MAX_SIZE_ON_STACK

    const kan_serialized_size_t read =
        (kan_serialized_size_t) data->stream->operations->read (data->stream, string_length, read_buffer);
    if (read == string_length)
    {
        read_buffer[string_length] = '\0';
        interned_string_registry_add_string_internal (data->registry, kan_string_intern (read_buffer));
        ++data->strings_read;
    }

    if (read_buffer != read_buffer_on_stack)
    {
        kan_free_general (interned_string_registry_read_allocation_group, read_buffer, string_length + 1u);
    }

    if (read == string_length)
    {
        return data->strings_read >= data->strings_total ? KAN_SERIALIZATION_FINISHED : KAN_SERIALIZATION_IN_PROGRESS;
    }
    else
    {
        return KAN_SERIALIZATION_FAILED;
    }
}

kan_serialization_interned_string_registry_t kan_serialization_interned_string_registry_reader_get (
    kan_serialization_interned_string_registry_reader_t reader)
{
    struct interned_string_registry_reader_t *data = KAN_HANDLE_GET (reader);
    return KAN_HANDLE_SET (kan_serialization_interned_string_registry_t, data->registry);
}

void kan_serialization_interned_string_registry_reader_destroy (
    kan_serialization_interned_string_registry_reader_t reader)
{
    kan_free_general (interned_string_registry_read_allocation_group, KAN_HANDLE_GET (reader),
                      sizeof (struct interned_string_registry_reader_t));
}

kan_serialization_interned_string_registry_writer_t kan_serialization_interned_string_registry_writer_create (
    struct kan_stream_t *stream, kan_serialization_interned_string_registry_t registry)
{
    ensure_statics_initialized ();
    struct interned_string_registry_writer_t *writer =
        (struct interned_string_registry_writer_t *) kan_allocate_general (
            interned_string_registry_write_allocation_group, sizeof (struct interned_string_registry_writer_t),
            alignof (struct interned_string_registry_writer_t));

    KAN_ASSERT (KAN_HANDLE_IS_VALID (registry))
    writer->registry = KAN_HANDLE_GET (registry);
    KAN_ASSERT (kan_stream_is_writeable (stream))
    writer->stream = stream;

    KAN_ASSERT (writer->registry->index_to_value.size <= UINT32_MAX)
    writer->strings_total = (kan_serialized_size_t) writer->registry->index_to_value.size;
    writer->strings_written = 0u;

    if (writer->stream->operations->write (writer->stream, sizeof (kan_serialized_size_t), &writer->strings_total) !=
        sizeof (kan_serialized_size_t))
    {
        writer->strings_total = 0u;
        KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                 "Failed to initialize interned string registry writer: unable to write strings count into stream.")
    }

    return KAN_HANDLE_SET (kan_serialization_interned_string_registry_writer_t, writer);
}

enum kan_serialization_state_t kan_serialization_interned_string_registry_writer_step (
    kan_serialization_interned_string_registry_writer_t writer)
{
    struct interned_string_registry_writer_t *data = KAN_HANDLE_GET (writer);
    if (data->strings_written >= data->strings_total)
    {
        return KAN_SERIALIZATION_FINISHED;
    }

    kan_interned_string_t string =
        ((kan_interned_string_t *) data->registry->index_to_value.data)[data->strings_written];

    // Replacement for the writing. NULL interned strings are acceptable, but we cannot serialize them.
    // Therefore, we store empty strings that will be converted to NULLs on read.
    if (!string)
    {
        string = "";
    }

    kan_serialized_size_t string_length = (kan_serialized_size_t) strlen (string);
    if (data->stream->operations->write (data->stream, sizeof (kan_serialized_size_t), &string_length) !=
        sizeof (kan_serialized_size_t))
    {
        return KAN_SERIALIZATION_FAILED;
    }

    if (string_length > 0u)
    {
        if (data->stream->operations->write (data->stream, string_length, string) != string_length)
        {
            return KAN_SERIALIZATION_FAILED;
        }
    }

    ++data->strings_written;
    return data->strings_written >= data->strings_total ? KAN_SERIALIZATION_FINISHED : KAN_SERIALIZATION_IN_PROGRESS;
}

void kan_serialization_interned_string_registry_writer_destroy (
    kan_serialization_interned_string_registry_writer_t writer)
{
    kan_free_general (interned_string_registry_write_allocation_group, KAN_HANDLE_GET (writer),
                      sizeof (struct interned_string_registry_writer_t));
}

kan_serialization_binary_reader_t kan_serialization_binary_reader_create (
    struct kan_stream_t *stream,
    void *instance,
    kan_interned_string_t type_name,
    kan_serialization_binary_script_storage_t script_storage,
    kan_serialization_interned_string_registry_t interned_string_registry,
    kan_allocation_group_t deserialized_string_allocation_group)
{
    ensure_statics_initialized ();
    KAN_ASSERT (kan_stream_is_readable (stream))

    struct serialization_read_state_t *state = (struct serialization_read_state_t *) kan_allocate_general (
        serialization_allocation_group, sizeof (struct serialization_read_state_t),
        alignof (struct serialization_read_state_t));

    serialization_common_state_init (&state->common, stream, script_storage, interned_string_registry);
    struct script_node_t *script_node = script_storage_get_or_create_script (state->common.script_storage, type_name);
    script_storage_ensure_script_generated (state->common.script_storage, script_node);
    serialization_common_state_push_script_state (&state->common, script_node->script, instance, false);

    state->buffer_size = 0u;
    state->buffer = NULL;
    state->patch_builder = KAN_HANDLE_SET_INVALID (kan_reflection_patch_builder_t);
    state->child_allocation_group = deserialized_string_allocation_group;
    return KAN_HANDLE_SET (kan_serialization_binary_reader_t, state);
}

static inline void ensure_read_buffer_size (struct serialization_read_state_t *state, kan_memory_size_t required_size)
{
    if (state->buffer_size < required_size)
    {
        if (state->buffer)
        {
            kan_free_general (serialization_allocation_group, state->buffer, state->buffer_size);
        }

        state->buffer_size = kan_apply_alignment (required_size, alignof (kan_memory_size_t));
        state->buffer =
            kan_allocate_general (serialization_allocation_group, state->buffer_size, alignof (kan_memory_size_t));
    }
}

static inline bool read_string_to_buffer (struct serialization_read_state_t *state,
                                          kan_instance_size_t *string_length_output)
{
    kan_serialized_size_t string_length;
    if (state->common.stream->operations->read (state->common.stream, sizeof (kan_serialized_size_t), &string_length) !=
        sizeof (kan_serialized_size_t))
    {
        return false;
    }

    ensure_read_buffer_size (state, string_length);
    if (state->common.stream->operations->read (state->common.stream, string_length, state->buffer) != string_length)
    {
        return false;
    }

    *string_length_output = string_length;
    return true;
}

static inline bool read_string_to_new_allocation (struct serialization_read_state_t *state, char **string_output)
{
    kan_serialized_size_t string_length;
    if (state->common.stream->operations->read (state->common.stream, sizeof (kan_serialized_size_t), &string_length) !=
        sizeof (kan_serialized_size_t))
    {
        return false;
    }

    char *string_memory = kan_allocate_general (state->child_allocation_group, string_length + 1u, alignof (char));
    if (state->common.stream->operations->read (state->common.stream, string_length, string_memory) != string_length)
    {
        return false;
    }

    string_memory[string_length] = '\0';
    *string_output = string_memory;
    return true;
}

static inline bool read_interned_string_stateless (struct kan_stream_t *stream,
                                                   struct interned_string_registry_t *string_registry,
                                                   kan_interned_string_t *output)
{
    if (string_registry)
    {
        script_size_t index;
        if (stream->operations->read (stream, sizeof (kan_serialized_size_t), &index) != sizeof (kan_serialized_size_t))
        {
            return false;
        }

        *output = interned_string_registry_load_string (string_registry, index);
        return true;
    }
    else
    {
        kan_serialized_size_t string_length;
        if (stream->operations->read (stream, sizeof (kan_serialized_size_t), &string_length) !=
            sizeof (kan_serialized_size_t))
        {
            return false;
        }

        if (string_length == 0u)
        {
            *output = NULL;
            return true;
        }

        char *string_memory = kan_allocate_general (serialization_allocation_group, string_length, alignof (char));
        if (stream->operations->read (stream, string_length, string_memory) != string_length)
        {
            kan_free_general (serialization_allocation_group, string_memory, string_length);
            return false;
        }

        *output = kan_char_sequence_intern (string_memory, string_memory + string_length);
        kan_free_general (serialization_allocation_group, string_memory, string_length);
        return true;
    }
}

static inline bool read_interned_string (struct serialization_read_state_t *state, kan_interned_string_t *output)
{
    if (state->common.optional_string_registry)
    {
        script_size_t index;
        if (state->common.stream->operations->read (state->common.stream, sizeof (kan_serialized_size_t), &index) !=
            sizeof (kan_serialized_size_t))
        {
            return false;
        }

        *output = interned_string_registry_load_string (state->common.optional_string_registry, index);
        return true;
    }
    else
    {
        kan_instance_size_t length;
        if (!read_string_to_buffer (state, &length))
        {
            return false;
        }

        *output = kan_char_sequence_intern ((const char *) state->buffer, (const char *) state->buffer + length);
        return true;
    }
}

static inline bool read_array_or_patch_size (struct serialization_read_state_t *state, kan_serialized_size_t *output)
{
    return state->common.stream->operations->read (state->common.stream, sizeof (kan_serialized_size_t), output) ==
           sizeof (kan_serialized_size_t);
}

static inline bool ensure_dynamic_array_read_suffix_ready (struct serialization_read_state_t *state,
                                                           struct script_state_t *top_state,
                                                           struct kan_dynamic_array_t *array)
{
    if (!top_state->suffix_initialized)
    {
        if (!read_array_or_patch_size (state, &top_state->suffix_dynamic_array.items_total))
        {
            return false;
        }

        top_state->suffix_dynamic_array.items_processed = 0u;
        kan_dynamic_array_set_capacity (array, top_state->suffix_dynamic_array.items_total);
        array->size = 0u;
        top_state->suffix_initialized = true;
    }

    return true;
}

static inline void ensure_patch_section_map_is_ready (struct serialization_common_state_t *state,
                                                      kan_instance_size_t id_bound)
{
    state->last_patch_section_state = NULL;
    if (state->patch_section_map_size < id_bound)
    {
        if (state->patch_section_map)
        {
            kan_free_general (serialization_allocation_group, state->patch_section_map,
                              sizeof (struct patch_section_state_info_t) * state->patch_section_map_size);
        }

        state->patch_section_map_size = id_bound;
        state->patch_section_map = kan_allocate_general (
            serialization_allocation_group, sizeof (struct patch_section_state_info_t) * state->patch_section_map_size,
            alignof (struct patch_section_state_info_t));
    }

    for (kan_loop_size_t index = 0u; index < id_bound; ++index)
    {
        state->patch_section_map[index].source_field = NULL;
        state->patch_section_map[index].type = KAN_REFLECTION_PATCH_SECTION_TYPE_DYNAMIC_ARRAY_SET;
        state->patch_section_map[index].read_section = KAN_REFLECTION_PATCH_BUILDER_SECTION_ROOT;
    }
}

static inline bool init_patch_read_suffix (struct serialization_read_state_t *state,
                                           struct script_state_patch_suffix_t *suffix)
{
    if (!read_interned_string (state, &suffix->type_name))
    {
        return false;
    }

    if (suffix->type_name != interned_invalid_patch_type_t)
    {
        if (!read_array_or_patch_size (state, &suffix->read.blocks_total) ||
            !read_array_or_patch_size (state, &suffix->read.section_id_bound))
        {
            return false;
        }

        suffix->read.blocks_processed = 0u;
    }
    else
    {
        suffix->read.blocks_processed = 0u;
        suffix->read.blocks_total = 0u;
        suffix->read.section_id_bound = 0u;
    }

    ensure_patch_section_map_is_ready (&state->common, suffix->read.section_id_bound);
    return true;
}

static inline kan_loop_size_t upper_or_equal_bound_index (const script_size_t *positions,
                                                          kan_instance_size_t positions_count,
                                                          script_size_t value)
{
    kan_loop_size_t first = 0u;
    kan_loop_size_t last = positions_count;

    while (first < last)
    {
        kan_loop_size_t middle = (first + last) / 2u;
        if (positions[middle] == value)
        {
            return middle;
        }
        else if (positions[middle] < value)
        {
            first = middle + 1u;
        }
        else
        {
            last = middle;
        }
    }

    return first;
}

struct patch_section_info_t
{
    kan_reflection_patch_serializable_section_id_t parent_id;
    kan_reflection_patch_serializable_section_id_t my_id;
    enum kan_reflection_patch_section_type_t type;
    kan_serialized_size_t source_offset;
};

struct patch_chunk_info_t
{
    kan_serialized_size_t offset;
    kan_serialized_size_t size;
};

static inline kan_interned_string_t extract_parent_patch_section_struct_type (
    struct patch_section_state_info_t *parent_state, struct script_state_patch_suffix_t *suffix)
{
    if (parent_state)
    {
        switch (parent_state->type)
        {
        case KAN_REFLECTION_PATCH_SECTION_TYPE_DYNAMIC_ARRAY_SET:
        case KAN_REFLECTION_PATCH_SECTION_TYPE_DYNAMIC_ARRAY_APPEND:
            KAN_ASSERT (parent_state->source_field->archetype == KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY)
            if (parent_state->source_field->archetype_dynamic_array.item_archetype == KAN_REFLECTION_ARCHETYPE_STRUCT)
            {
                return parent_state->source_field->archetype_dynamic_array.item_archetype_struct.type_name;
            }

            return NULL;
        }

        return NULL;
    }
    else
    {
        return suffix->type_name;
    }
}

static inline bool is_patch_section_represents_interned_string_array (struct patch_section_state_info_t *state)
{
    if (state)
    {
        switch (state->type)
        {
        case KAN_REFLECTION_PATCH_SECTION_TYPE_DYNAMIC_ARRAY_SET:
        case KAN_REFLECTION_PATCH_SECTION_TYPE_DYNAMIC_ARRAY_APPEND:
            KAN_ASSERT (state->source_field->archetype == KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY)
            return state->source_field->archetype_dynamic_array.item_archetype ==
                   KAN_REFLECTION_ARCHETYPE_INTERNED_STRING;
        }
    }

    return false;
}

static inline const struct kan_reflection_field_t *find_source_field_for_child_patch_section (
    struct serialization_common_state_t *state,
    struct patch_section_state_info_t *parent_state,
    struct script_state_patch_suffix_t *suffix,
    kan_instance_size_t offset_in_parent)
{
    kan_interned_string_t parent_type_name = extract_parent_patch_section_struct_type (parent_state, suffix);
    KAN_ASSERT (parent_type_name)
    const struct kan_reflection_struct_t *parent_struct =
        kan_reflection_registry_query_struct (state->script_storage->registry, parent_type_name);

    return kan_reflection_registry_query_local_field_by_offset (state->script_storage->registry, parent_type_name,
                                                                offset_in_parent % parent_struct->size, NULL);
}

static inline bool read_patch_block (struct serialization_read_state_t *state,
                                     struct script_state_patch_suffix_t *suffix)
{
    bool is_data_chunk;
    if (state->common.stream->operations->read (state->common.stream, sizeof (bool), &is_data_chunk) != sizeof (bool))
    {
        return false;
    }

    if (is_data_chunk)
    {
        struct patch_chunk_info_t block_info;
        if (state->common.stream->operations->read (state->common.stream, sizeof (struct patch_chunk_info_t),
                                                    &block_info) != sizeof (struct patch_chunk_info_t))
        {
            return false;
        }

        if (block_info.size == 0u)
        {
            return true;
        }

        const kan_reflection_patch_builder_section_t section =
            state->common.last_patch_section_state ? state->common.last_patch_section_state->read_section :
                                                     KAN_REFLECTION_PATCH_BUILDER_SECTION_ROOT;

        script_size_t current_offset = block_info.offset;
        const script_size_t end_offset = block_info.offset + block_info.size;

        // Check special case: section that is an array of interned strings.
        if (is_patch_section_represents_interned_string_array (state->common.last_patch_section_state))
        {
            // Otherwise patch is malformed.
            KAN_ASSERT (block_info.size % sizeof (kan_interned_string_t) == 0u)

            while (current_offset < end_offset)
            {
                kan_interned_string_t string;
                if (!read_interned_string (state, &string))
                {
                    return false;
                }

                kan_reflection_patch_builder_add_chunk (state->patch_builder, section, current_offset,
                                                        sizeof (kan_interned_string_t), &string);
                current_offset += sizeof (kan_interned_string_t);
            }

            return true;
        }

        kan_interned_string_t parent_struct_type_name =
            extract_parent_patch_section_struct_type (state->common.last_patch_section_state, suffix);

        // Check special case: section does not contain structs and therefore can be read directly.
        if (!parent_struct_type_name)
        {
            const script_size_t size = end_offset - current_offset;
            ensure_read_buffer_size (state, size);

            if (state->common.stream->operations->read (state->common.stream, size, state->buffer) != size)
            {
                return false;
            }

            kan_reflection_patch_builder_add_chunk (state->patch_builder, section, current_offset, size, state->buffer);
            return true;
        }

        struct interned_string_lookup_node_t *interned_string_lookup_node =
            script_storage_get_or_create_interned_string_lookup (state->common.script_storage, parent_struct_type_name);
        script_storage_ensure_interned_string_lookup_generated (state->common.script_storage,
                                                                interned_string_lookup_node);

        const struct kan_reflection_struct_t *struct_type =
            kan_reflection_registry_query_struct (state->common.script_storage->registry, parent_struct_type_name);
        KAN_ASSERT (struct_type)

        while (current_offset < end_offset)
        {
            script_size_t local_current_offset = current_offset % struct_type->size;
            const script_size_t local_end_offset =
                KAN_MIN (local_current_offset + (end_offset - current_offset), struct_type->size);

            kan_loop_size_t next_interned_string_index = upper_or_equal_bound_index (
                interned_string_lookup_node->interned_string_absolute_positions,
                interned_string_lookup_node->interned_string_absolute_positions_count, local_current_offset);

            while (local_current_offset < local_end_offset)
            {
                script_size_t serialized_block_end = local_end_offset;
                if (next_interned_string_index < interned_string_lookup_node->interned_string_absolute_positions_count)
                {
                    script_size_t next_string_offset =
                        interned_string_lookup_node->interned_string_absolute_positions[next_interned_string_index];

                    if (next_string_offset == local_current_offset)
                    {
                        kan_interned_string_t string;
                        if (!read_interned_string (state, &string))
                        {
                            return false;
                        }

                        kan_reflection_patch_builder_add_chunk (state->patch_builder, section, current_offset,
                                                                sizeof (kan_interned_string_t), &string);

                        local_current_offset += sizeof (kan_interned_string_t);
                        current_offset += sizeof (kan_interned_string_t);
                        ++next_interned_string_index;
                        continue;
                    }
                    else if (next_string_offset < serialized_block_end)
                    {
                        KAN_ASSERT (next_string_offset > local_current_offset)
                        serialized_block_end = next_string_offset;
                    }
                }

                const script_size_t size = serialized_block_end - local_current_offset;
                ensure_read_buffer_size (state, size);

                if (state->common.stream->operations->read (state->common.stream, size, state->buffer) != size)
                {
                    return false;
                }

                kan_reflection_patch_builder_add_chunk (state->patch_builder, section, current_offset, size,
                                                        state->buffer);
                local_current_offset = serialized_block_end;
                current_offset += size;
            }
        }

        kan_loop_size_t next_interned_string_index = upper_or_equal_bound_index (
            interned_string_lookup_node->interned_string_absolute_positions,
            interned_string_lookup_node->interned_string_absolute_positions_count, current_offset);

        while (current_offset < end_offset)
        {
            script_size_t serialized_block_end = end_offset;
            if (next_interned_string_index < interned_string_lookup_node->interned_string_absolute_positions_count)
            {
                script_size_t next_string_offset =
                    interned_string_lookup_node->interned_string_absolute_positions[next_interned_string_index];

                if (next_string_offset == current_offset)
                {
                    kan_interned_string_t string;
                    if (!read_interned_string (state, &string))
                    {
                        return false;
                    }

                    kan_reflection_patch_builder_add_chunk (state->patch_builder, section, current_offset,
                                                            sizeof (kan_interned_string_t), &string);

                    current_offset += sizeof (kan_interned_string_t);
                    ++next_interned_string_index;
                    continue;
                }
                else if (next_string_offset < serialized_block_end)
                {
                    KAN_ASSERT (next_string_offset > current_offset)
                    serialized_block_end = next_string_offset;
                }
            }

            const script_size_t size = serialized_block_end - current_offset;
            ensure_read_buffer_size (state, size);

            if (state->common.stream->operations->read (state->common.stream, size, state->buffer) != size)
            {
                return false;
            }

            kan_reflection_patch_builder_add_chunk (state->patch_builder, section, current_offset, size, state->buffer);
            current_offset = serialized_block_end;
        }
    }
    else
    {
        struct patch_section_info_t section_info;
        if (state->common.stream->operations->read (state->common.stream, sizeof (struct patch_section_info_t),
                                                    &section_info) != sizeof (struct patch_section_info_t))
        {
            return false;
        }

        struct patch_section_state_info_t *parent_state =
            KAN_TYPED_ID_32_IS_VALID (section_info.parent_id) ?
                &state->common.patch_section_map[KAN_TYPED_ID_32_GET (section_info.parent_id)] :
                NULL;
        KAN_ASSERT (!parent_state || parent_state->source_field)

        struct patch_section_state_info_t *my_state =
            &state->common.patch_section_map[KAN_TYPED_ID_32_GET (section_info.my_id)];

        my_state->source_field = find_source_field_for_child_patch_section (&state->common, parent_state, suffix,
                                                                            section_info.source_offset);

        my_state->type = section_info.type;
        my_state->read_section = kan_reflection_patch_builder_add_section (
            state->patch_builder, parent_state ? parent_state->read_section : KAN_REFLECTION_PATCH_BUILDER_SECTION_ROOT,
            section_info.type, section_info.source_offset);
        state->common.last_patch_section_state = my_state;
    }

    return true;
}

enum kan_serialization_state_t kan_serialization_binary_reader_step (kan_serialization_binary_reader_t reader)
{
    struct serialization_read_state_t *state = KAN_HANDLE_GET (reader);
    if (state->common.script_state_stack.size == 0u)
    {
        return KAN_SERIALIZATION_FINISHED;
    }

    struct script_state_t *top_state =
        &((struct script_state_t *) state->common.script_state_stack.data)[state->common.script_state_stack.size - 1u];
    KAN_ASSERT (top_state->command_to_process_index < top_state->script->commands_count)

    struct script_condition_t *script_conditions = (struct script_condition_t *) top_state->script->data;
    struct script_command_t *script_commands =
        (struct script_command_t *) (script_conditions + top_state->script->conditions_count);

    struct script_command_t *command_to_process = script_commands + top_state->command_to_process_index;
    bool should_be_processed = true;

    if (command_to_process->condition_index != SCRIPT_NO_CONDITION && !top_state->condition_checked)
    {
        if (top_state->condition_values[command_to_process->condition_index] == SERIALIZATION_CONDITION_NOT_CALCULATED)
        {
            calculate_condition (top_state->instance, top_state->condition_values, command_to_process->condition_index,
                                 script_conditions);
        }

        should_be_processed =
            top_state->condition_values[command_to_process->condition_index] == SERIALIZATION_CONDITION_PASSED;
        top_state->condition_checked = true;
    }

    if (should_be_processed)
    {
        uint8_t *address = ((uint8_t *) top_state->instance) + command_to_process->offset;
        switch (command_to_process->type)
        {
        case SCRIPT_COMMAND_BLOCK:
            if (state->common.stream->operations->read (state->common.stream, command_to_process->block.size,
                                                        address) != command_to_process->block.size)
            {
                return KAN_SERIALIZATION_FAILED;
            }

            script_state_go_to_next_command (top_state);
            break;

        case SCRIPT_COMMAND_STRING:
        {
            char *string;
            if (!read_string_to_new_allocation (state, &string))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            *((char **) address) = string;
            script_state_go_to_next_command (top_state);
            break;
        }

        case SCRIPT_COMMAND_INTERNED_STRING:
            if (!read_interned_string (state, (kan_interned_string_t *) address))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            script_state_go_to_next_command (top_state);
            break;

        case SCRIPT_COMMAND_BLOCK_DYNAMIC_ARRAY:
        {
            script_size_t size;
            if (!read_array_or_patch_size (state, &size))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            kan_dynamic_array_set_capacity (array, size);
            array->size = size;

            if (state->common.stream->operations->read (state->common.stream, array->size * array->item_size,
                                                        array->data) != array->size * array->item_size)
            {
                return KAN_SERIALIZATION_FAILED;
            }

            script_state_go_to_next_command (top_state);
            break;
        }

        case SCRIPT_COMMAND_STRING_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!ensure_dynamic_array_read_suffix_ready (state, top_state, array))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            if (top_state->suffix_dynamic_array.items_processed < top_state->suffix_dynamic_array.items_total)
            {
                if (!read_string_to_new_allocation (
                        state, ((char **) array->data) + top_state->suffix_dynamic_array.items_processed))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                ++top_state->suffix_dynamic_array.items_processed;
                ++array->size;
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_INTERNED_STRING_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!ensure_dynamic_array_read_suffix_ready (state, top_state, array))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            if (top_state->suffix_dynamic_array.items_processed < top_state->suffix_dynamic_array.items_total)
            {
                if (!read_interned_string (state, ((kan_interned_string_t *) array->data) +
                                                      top_state->suffix_dynamic_array.items_processed))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                ++top_state->suffix_dynamic_array.items_processed;
                ++array->size;
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_STRUCT_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!ensure_dynamic_array_read_suffix_ready (state, top_state, array))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            if (top_state->suffix_dynamic_array.items_processed < top_state->suffix_dynamic_array.items_total)
            {
                struct script_node_t *script_node = script_storage_get_or_create_script (
                    state->common.script_storage, command_to_process->struct_dynamic_array.type_name);
                script_storage_ensure_script_generated (state->common.script_storage, script_node);

                const struct kan_reflection_struct_t *child_struct_data = kan_reflection_registry_query_struct (
                    state->common.script_storage->registry, command_to_process->struct_dynamic_array.type_name);
                KAN_ASSERT (child_struct_data)

                uint8_t *instance_address =
                    ((uint8_t *) array->data) + array->item_size * top_state->suffix_dynamic_array.items_processed;

                if (child_struct_data->init)
                {
                    child_struct_data->init (child_struct_data->functor_user_data, instance_address);
                }

                serialization_common_state_push_script_state (&state->common, script_node->script, instance_address,
                                                              false);

                ++top_state->suffix_dynamic_array.items_processed;
                ++array->size;
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_PATCH_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!top_state->suffix_initialized)
            {
                if (!read_array_or_patch_size (state, &top_state->suffix_patch_dynamic_array.array.items_total))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                if (top_state->suffix_patch_dynamic_array.array.items_total > 0u)
                {
                    if (!init_patch_read_suffix (state, &top_state->suffix_patch_dynamic_array.current_patch))
                    {
                        return KAN_SERIALIZATION_FAILED;
                    }

                    if (!KAN_HANDLE_IS_VALID (state->patch_builder))
                    {
                        state->patch_builder = kan_reflection_patch_builder_create ();
                    }
                }

                top_state->suffix_patch_dynamic_array.array.items_processed = 0u;
                kan_dynamic_array_set_capacity (array, top_state->suffix_patch_dynamic_array.array.items_total);
                array->size = 0u;
                top_state->suffix_initialized = true;
            }

            if (top_state->suffix_patch_dynamic_array.array.items_processed <
                top_state->suffix_patch_dynamic_array.array.items_total)
            {
                if (top_state->suffix_patch_dynamic_array.current_patch.read.blocks_processed <
                    top_state->suffix_patch_dynamic_array.current_patch.read.blocks_total)
                {
                    if (!read_patch_block (state, &top_state->suffix_patch_dynamic_array.current_patch))
                    {
                        return KAN_SERIALIZATION_FAILED;
                    }

                    ++top_state->suffix_patch_dynamic_array.current_patch.read.blocks_processed;
                }

                if (top_state->suffix_patch_dynamic_array.current_patch.read.blocks_processed >=
                    top_state->suffix_patch_dynamic_array.current_patch.read.blocks_total)
                {
                    kan_reflection_patch_t *patch =
                        (kan_reflection_patch_t *) (((uint8_t *) array->data) +
                                                    sizeof (kan_reflection_patch_t) *
                                                        top_state->suffix_patch_dynamic_array.array.items_processed);

                    if (top_state->suffix_patch_dynamic_array.current_patch.type_name != interned_invalid_patch_type_t)
                    {
                        *patch = kan_reflection_patch_builder_build (
                            state->patch_builder, state->common.script_storage->registry,
                            kan_reflection_registry_query_struct (
                                state->common.script_storage->registry,
                                top_state->suffix_patch_dynamic_array.current_patch.type_name));
                    }
                    else
                    {
                        *patch = KAN_HANDLE_SET_INVALID (kan_reflection_patch_t);
                    }

                    ++top_state->suffix_patch_dynamic_array.array.items_processed;
                    ++array->size;

                    if (top_state->suffix_patch_dynamic_array.array.items_processed <
                        top_state->suffix_patch_dynamic_array.array.items_total)
                    {
                        if (!init_patch_read_suffix (state, &top_state->suffix_patch_dynamic_array.current_patch))
                        {
                            return KAN_SERIALIZATION_FAILED;
                        }
                    }
                }
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_PATCH:
        {
            if (!top_state->suffix_initialized)
            {
                if (!init_patch_read_suffix (state, &top_state->suffix_patch))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                if (!KAN_HANDLE_IS_VALID (state->patch_builder))
                {
                    state->patch_builder = kan_reflection_patch_builder_create ();
                }

                top_state->suffix_initialized = true;
            }

            if (top_state->suffix_patch.read.blocks_processed < top_state->suffix_patch.read.blocks_total)
            {
                if (!read_patch_block (state, &top_state->suffix_patch))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                ++top_state->suffix_patch.read.blocks_processed;
            }

            if (top_state->suffix_patch.read.blocks_processed >= top_state->suffix_patch.read.blocks_total)
            {
                kan_reflection_patch_t *patch = (kan_reflection_patch_t *) address;
                if (top_state->suffix_patch.type_name != interned_invalid_patch_type_t)
                {
                    *patch = kan_reflection_patch_builder_build (
                        state->patch_builder, state->common.script_storage->registry,
                        kan_reflection_registry_query_struct (state->common.script_storage->registry,
                                                              top_state->suffix_patch.type_name));
                }
                else
                {
                    *patch = KAN_HANDLE_SET_INVALID (kan_reflection_patch_t);
                }

                script_state_go_to_next_command (top_state);
            }

            break;
        }
        }
    }
    else
    {
        script_state_go_to_next_command (top_state);
    }

    while (true)
    {
        // Update top state pointer in case if new state was pushed on top.
        top_state = &((struct script_state_t *)
                          state->common.script_state_stack.data)[state->common.script_state_stack.size - 1u];

        if (top_state->command_to_process_index >= top_state->script->commands_count)
        {
            serialization_common_state_pop_script_state (&state->common);
            if (state->common.script_state_stack.size == 0u)
            {
                return KAN_SERIALIZATION_FINISHED;
            }
        }
        else
        {
            break;
        }
    }

    return KAN_SERIALIZATION_IN_PROGRESS;
}

void kan_serialization_binary_reader_destroy (kan_serialization_binary_reader_t reader)
{
    struct serialization_read_state_t *state = KAN_HANDLE_GET (reader);
    serialization_common_state_shutdown (&state->common);

    if (state->buffer)
    {
        kan_free_general (serialization_allocation_group, state->buffer, state->buffer_size);
    }

    if (KAN_HANDLE_IS_VALID (state->patch_builder))
    {
        kan_reflection_patch_builder_destroy (state->patch_builder);
    }

    kan_free_general (serialization_allocation_group, state, sizeof (struct serialization_read_state_t));
}

kan_serialization_binary_writer_t kan_serialization_binary_writer_create (
    struct kan_stream_t *stream,
    const void *instance,
    kan_interned_string_t type_name,
    kan_serialization_binary_script_storage_t script_storage,
    kan_serialization_interned_string_registry_t interned_string_registry)
{
    ensure_statics_initialized ();
    KAN_ASSERT (kan_stream_is_writeable (stream))

    struct serialization_write_state_t *state = (struct serialization_write_state_t *) kan_allocate_general (
        serialization_allocation_group, sizeof (struct serialization_write_state_t),
        alignof (struct serialization_write_state_t));

    serialization_common_state_init (&state->common, stream, script_storage, interned_string_registry);
    struct script_node_t *script_node = script_storage_get_or_create_script (state->common.script_storage, type_name);
    script_storage_ensure_script_generated (state->common.script_storage, script_node);
    serialization_common_state_push_script_state (&state->common, script_node->script, (void *) instance, true);

    return KAN_HANDLE_SET (kan_serialization_binary_writer_t, state);
}

static inline bool write_string_stateless (struct kan_stream_t *stream, const char *string_input)
{
    const kan_instance_size_t string_length_wide = (kan_instance_size_t) strlen (string_input);
    KAN_ASSERT (string_length_wide <= UINT32_MAX)
    const kan_serialized_size_t string_length = (kan_serialized_size_t) string_length_wide;

    if (stream->operations->write (stream, sizeof (kan_serialized_size_t), &string_length) !=
        sizeof (kan_serialized_size_t))
    {
        return false;
    }

    if (stream->operations->write (stream, string_length, string_input) != string_length)
    {
        return false;
    }

    return true;
}

static inline bool write_string (struct serialization_write_state_t *state, const char *string_input)
{
    return write_string_stateless (state->common.stream, string_input);
}

static inline bool write_interned_string_stateless (struct kan_stream_t *stream,
                                                    struct interned_string_registry_t *string_registry,
                                                    kan_interned_string_t input)
{
    if (string_registry)
    {
        const script_size_t index = interned_string_registry_store_string (string_registry, input);
        return stream->operations->write (stream, sizeof (kan_serialized_size_t), &index) ==
               sizeof (kan_serialized_size_t);
    }

    // Having NULL interned strings as no-option value is totally valid.
    if (!input)
    {
        kan_serialized_size_t size = 0u;
        if (stream->operations->write (stream, sizeof (kan_serialized_size_t), &size) != sizeof (kan_serialized_size_t))
        {
            return false;
        }

        return true;
    }

    return write_string_stateless (stream, input);
}

static inline bool write_interned_string (struct serialization_write_state_t *state, kan_interned_string_t input)
{
    return write_interned_string_stateless (state->common.stream, state->common.optional_string_registry, input);
}

static inline bool write_array_or_patch_size (struct serialization_write_state_t *state, kan_serialized_size_t input)
{
    return state->common.stream->operations->write (state->common.stream, sizeof (kan_serialized_size_t), &input) ==
           sizeof (kan_serialized_size_t);
}

static inline bool ensure_dynamic_array_write_suffix_ready (struct serialization_write_state_t *state,
                                                            struct script_state_t *top_state,
                                                            struct kan_dynamic_array_t *array)
{
    if (!top_state->suffix_initialized)
    {
        top_state->suffix_dynamic_array.items_total = (kan_serialized_size_t) array->size;
        if (!write_array_or_patch_size (state, top_state->suffix_dynamic_array.items_total))
        {
            return false;
        }

        top_state->suffix_dynamic_array.items_processed = 0u;
        top_state->suffix_initialized = true;
    }

    return true;
}

static inline bool init_patch_write_suffix (struct serialization_write_state_t *state,
                                            kan_reflection_patch_t patch,
                                            struct script_state_patch_suffix_t *suffix)
{
    if (KAN_HANDLE_IS_VALID (patch) && kan_reflection_patch_get_type (patch))
    {
        if (!write_interned_string (state, kan_reflection_patch_get_type (patch)->name) ||
            !write_array_or_patch_size (state, (kan_serialized_size_t) kan_reflection_patch_get_chunks_count (patch)) ||
            !write_array_or_patch_size (state,
                                        (kan_serialized_size_t) kan_reflection_patch_get_section_id_bound (patch)))
        {
            return false;
        }

        suffix->type_name = kan_reflection_patch_get_type (patch)->name;
        suffix->write.current_iterator = kan_reflection_patch_begin (patch);
        suffix->write.end_iterator = kan_reflection_patch_end (patch);

        ensure_patch_section_map_is_ready (&state->common, kan_reflection_patch_get_section_id_bound (patch));
    }
    else
    {
        if (!write_interned_string (state, interned_invalid_patch_type_t))
        {
            return false;
        }

        suffix->type_name = interned_invalid_patch_type_t;
        suffix->write.current_iterator = KAN_HANDLE_SET_INVALID (kan_reflection_patch_iterator_t);
        suffix->write.end_iterator = KAN_HANDLE_SET_INVALID (kan_reflection_patch_iterator_t);
        ensure_patch_section_map_is_ready (&state->common, 0u);
    }

    return true;
}

static inline bool write_patch_block (struct serialization_write_state_t *state,
                                      struct script_state_patch_suffix_t *suffix)
{
    struct kan_reflection_patch_node_info_t node = kan_reflection_patch_iterator_get (suffix->write.current_iterator);
    if (state->common.stream->operations->write (state->common.stream, sizeof (bool), &node.is_data_chunk) !=
        sizeof (bool))
    {
        return false;
    }

    if (node.is_data_chunk)
    {
        struct patch_chunk_info_t block_info;
        block_info.offset = (kan_serialized_size_t) node.chunk_info.offset;
        block_info.size = (kan_serialized_size_t) node.chunk_info.size;

        if (state->common.stream->operations->write (state->common.stream, sizeof (struct patch_chunk_info_t),
                                                     &block_info) != sizeof (struct patch_chunk_info_t))
        {
            return false;
        }

        if (block_info.size == 0u)
        {
            return true;
        }

        script_size_t current_offset = block_info.offset;
        const script_size_t end_offset = block_info.offset + block_info.size;

        // Check special case: section that is an array of interned strings.
        if (is_patch_section_represents_interned_string_array (state->common.last_patch_section_state))
        {
            // Otherwise patch is malformed.
            KAN_ASSERT (block_info.size % sizeof (kan_interned_string_t) == 0u)

            while (current_offset < end_offset)
            {
                const uint8_t *data_begin =
                    ((const uint8_t *) node.chunk_info.data) + (current_offset - node.chunk_info.offset);

                if (!write_interned_string (state, *(kan_interned_string_t *) data_begin))
                {
                    return false;
                }

                current_offset += sizeof (kan_interned_string_t);
            }

            return true;
        }

        kan_interned_string_t parent_struct_type_name =
            extract_parent_patch_section_struct_type (state->common.last_patch_section_state, suffix);

        // Check special case: section does not contain structs and therefore can be read directly.
        if (!parent_struct_type_name)
        {
            const script_size_t size = end_offset - current_offset;
            if (state->common.stream->operations->write (state->common.stream, size, node.chunk_info.data) != size)
            {
                return false;
            }

            return true;
        }

        struct interned_string_lookup_node_t *interned_string_lookup_node =
            script_storage_get_or_create_interned_string_lookup (state->common.script_storage, parent_struct_type_name);
        script_storage_ensure_interned_string_lookup_generated (state->common.script_storage,
                                                                interned_string_lookup_node);

        const struct kan_reflection_struct_t *struct_type =
            kan_reflection_registry_query_struct (state->common.script_storage->registry, parent_struct_type_name);
        KAN_ASSERT (struct_type)

        while (current_offset < end_offset)
        {
            script_size_t local_current_offset = current_offset % struct_type->size;
            const script_size_t local_end_offset =
                KAN_MIN (local_current_offset + (end_offset - current_offset), struct_type->size);

            kan_loop_size_t next_interned_string_index = upper_or_equal_bound_index (
                interned_string_lookup_node->interned_string_absolute_positions,
                interned_string_lookup_node->interned_string_absolute_positions_count, local_current_offset);

            while (local_current_offset < local_end_offset)
            {
                script_size_t serialized_block_end = local_end_offset;
                const uint8_t *data_begin =
                    ((const uint8_t *) node.chunk_info.data) + (current_offset - node.chunk_info.offset);

                if (next_interned_string_index < interned_string_lookup_node->interned_string_absolute_positions_count)
                {
                    script_size_t next_string_offset =
                        interned_string_lookup_node->interned_string_absolute_positions[next_interned_string_index];

                    if (next_string_offset == local_current_offset)
                    {
                        if (!write_interned_string (state, *(kan_interned_string_t *) data_begin))
                        {
                            return false;
                        }

                        local_current_offset += sizeof (kan_interned_string_t);
                        current_offset += sizeof (kan_interned_string_t);
                        ++next_interned_string_index;
                        continue;
                    }
                    else if (next_string_offset < serialized_block_end)
                    {
                        KAN_ASSERT (next_string_offset > local_current_offset)
                        serialized_block_end = next_string_offset;
                    }
                }

                const script_size_t size = serialized_block_end - local_current_offset;
                if (state->common.stream->operations->write (state->common.stream, size, data_begin) != size)
                {
                    return false;
                }

                local_current_offset = serialized_block_end;
                current_offset += size;
            }
        }
    }
    else
    {
        struct patch_section_info_t section_info = {
            .parent_id = node.section_info.parent_section_id,
            .my_id = node.section_info.section_id,
            .type = node.section_info.type,
            .source_offset = node.section_info.source_offset_in_parent,
        };

        if (state->common.stream->operations->write (state->common.stream, sizeof (struct patch_section_info_t),
                                                     &section_info) != sizeof (struct patch_section_info_t))
        {
            return false;
        }

        struct patch_section_state_info_t *parent_state =
            KAN_TYPED_ID_32_IS_VALID (node.section_info.parent_section_id) ?
                &state->common.patch_section_map[KAN_TYPED_ID_32_GET (node.section_info.parent_section_id)] :
                NULL;
        KAN_ASSERT (!parent_state || parent_state->source_field)

        struct patch_section_state_info_t *my_state =
            &state->common.patch_section_map[KAN_TYPED_ID_32_GET (node.section_info.section_id)];

        my_state->source_field = find_source_field_for_child_patch_section (&state->common, parent_state, suffix,
                                                                            node.section_info.source_offset_in_parent);

        KAN_ASSERT (my_state->source_field)
        my_state->type = node.section_info.type;
        state->common.last_patch_section_state = my_state;
    }

    return true;
}

enum kan_serialization_state_t kan_serialization_binary_writer_step (kan_serialization_binary_writer_t writer)
{
    struct serialization_write_state_t *state = KAN_HANDLE_GET (writer);
    if (state->common.script_state_stack.size == 0u)
    {
        return KAN_SERIALIZATION_FINISHED;
    }

    struct script_state_t *top_state =
        &((struct script_state_t *) state->common.script_state_stack.data)[state->common.script_state_stack.size - 1u];
    KAN_ASSERT (top_state->command_to_process_index < top_state->script->commands_count)

    struct script_condition_t *script_conditions = (struct script_condition_t *) top_state->script->data;
    struct script_command_t *script_commands =
        (struct script_command_t *) (script_conditions + top_state->script->conditions_count);

    struct script_command_t *command_to_process = script_commands + top_state->command_to_process_index;
    bool should_be_processed = true;

    if (command_to_process->condition_index != SCRIPT_NO_CONDITION && !top_state->condition_checked)
    {
        KAN_ASSERT (top_state->condition_values[command_to_process->condition_index] !=
                    SERIALIZATION_CONDITION_NOT_CALCULATED)
        should_be_processed =
            top_state->condition_values[command_to_process->condition_index] == SERIALIZATION_CONDITION_PASSED;
        top_state->condition_checked = true;
    }

    if (should_be_processed)
    {
        uint8_t *address = ((uint8_t *) top_state->instance) + command_to_process->offset;
        switch (command_to_process->type)
        {
        case SCRIPT_COMMAND_BLOCK:
            if (state->common.stream->operations->write (state->common.stream, command_to_process->block.size,
                                                         address) != command_to_process->block.size)
            {
                return KAN_SERIALIZATION_FAILED;
            }

            script_state_go_to_next_command (top_state);
            break;

        case SCRIPT_COMMAND_STRING:
        {
            if (!write_string (state, *(char **) address))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            script_state_go_to_next_command (top_state);
            break;
        }

        case SCRIPT_COMMAND_INTERNED_STRING:
            if (!write_interned_string (state, *(kan_interned_string_t *) address))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            script_state_go_to_next_command (top_state);
            break;

        case SCRIPT_COMMAND_BLOCK_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!write_array_or_patch_size (state, (kan_serialized_size_t) array->size))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            if (state->common.stream->operations->write (state->common.stream, array->size * array->item_size,
                                                         array->data) != array->size * array->item_size)
            {
                return KAN_SERIALIZATION_FAILED;
            }

            script_state_go_to_next_command (top_state);
            break;
        }

        case SCRIPT_COMMAND_STRING_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!ensure_dynamic_array_write_suffix_ready (state, top_state, array))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            if (top_state->suffix_dynamic_array.items_processed < top_state->suffix_dynamic_array.items_total)
            {
                if (!write_string (state, *(((char **) array->data) + top_state->suffix_dynamic_array.items_processed)))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                ++top_state->suffix_dynamic_array.items_processed;
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_INTERNED_STRING_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!ensure_dynamic_array_write_suffix_ready (state, top_state, array))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            if (top_state->suffix_dynamic_array.items_processed < top_state->suffix_dynamic_array.items_total)
            {
                if (!write_interned_string (state, *(((kan_interned_string_t *) array->data) +
                                                     top_state->suffix_dynamic_array.items_processed)))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                ++top_state->suffix_dynamic_array.items_processed;
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_STRUCT_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!ensure_dynamic_array_write_suffix_ready (state, top_state, array))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            if (top_state->suffix_dynamic_array.items_processed < top_state->suffix_dynamic_array.items_total)
            {
                struct script_node_t *script_node = script_storage_get_or_create_script (
                    state->common.script_storage, command_to_process->struct_dynamic_array.type_name);
                script_storage_ensure_script_generated (state->common.script_storage, script_node);
                serialization_common_state_push_script_state (
                    &state->common, script_node->script,
                    ((uint8_t *) array->data) + array->item_size * top_state->suffix_dynamic_array.items_processed,
                    true);

                ++top_state->suffix_dynamic_array.items_processed;
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_PATCH_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!top_state->suffix_initialized)
            {
                top_state->suffix_dynamic_array.items_total = (script_size_t) array->size;
                if (!write_array_or_patch_size (state, top_state->suffix_patch_dynamic_array.array.items_total))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                if (array->size > 0u)
                {
                    if (!init_patch_write_suffix (state, *(kan_reflection_patch_t *) array->data,
                                                  &top_state->suffix_patch_dynamic_array.current_patch))
                    {
                        return KAN_SERIALIZATION_FAILED;
                    }
                }

                top_state->suffix_patch_dynamic_array.array.items_processed = 0u;
                top_state->suffix_initialized = true;
            }

            if (top_state->suffix_patch_dynamic_array.array.items_processed <
                top_state->suffix_patch_dynamic_array.array.items_total)
            {
                if (!KAN_HANDLE_IS_EQUAL (top_state->suffix_patch_dynamic_array.current_patch.write.current_iterator,
                                          top_state->suffix_patch_dynamic_array.current_patch.write.end_iterator))
                {
                    if (!write_patch_block (state, &top_state->suffix_patch_dynamic_array.current_patch))
                    {
                        return KAN_SERIALIZATION_FAILED;
                    }

                    top_state->suffix_patch_dynamic_array.current_patch.write.current_iterator =
                        kan_reflection_patch_iterator_next (
                            top_state->suffix_patch_dynamic_array.current_patch.write.current_iterator);
                }

                if (KAN_HANDLE_IS_EQUAL (top_state->suffix_patch_dynamic_array.current_patch.write.current_iterator,
                                         top_state->suffix_patch_dynamic_array.current_patch.write.end_iterator))
                {
                    ++top_state->suffix_patch_dynamic_array.array.items_processed;
                    if (top_state->suffix_patch_dynamic_array.array.items_processed <
                        top_state->suffix_patch_dynamic_array.array.items_total)
                    {
                        kan_reflection_patch_t next_patch = *(
                            kan_reflection_patch_t *) (((uint8_t *) array->data) +
                                                       sizeof (kan_reflection_patch_t) *
                                                           top_state->suffix_patch_dynamic_array.array.items_processed);

                        if (!init_patch_write_suffix (state, next_patch,
                                                      &top_state->suffix_patch_dynamic_array.current_patch))
                        {
                            return KAN_SERIALIZATION_FAILED;
                        }
                    }
                }
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_PATCH:
        {
            kan_reflection_patch_t patch = *(kan_reflection_patch_t *) address;
            if (!top_state->suffix_initialized)
            {
                if (!init_patch_write_suffix (state, patch, &top_state->suffix_patch))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                top_state->suffix_initialized = true;
            }

            if (!KAN_HANDLE_IS_EQUAL (top_state->suffix_patch.write.current_iterator,
                                      top_state->suffix_patch.write.end_iterator))
            {
                if (!write_patch_block (state, &top_state->suffix_patch))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                top_state->suffix_patch.write.current_iterator =
                    kan_reflection_patch_iterator_next (top_state->suffix_patch.write.current_iterator);
            }

            if (KAN_HANDLE_IS_EQUAL (top_state->suffix_patch.write.current_iterator,
                                     top_state->suffix_patch.write.end_iterator))
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }
        }
    }
    else
    {
        script_state_go_to_next_command (top_state);
    }

    while (true)
    {
        // Update top state pointer in case if new state was pushed on top.
        top_state = &((struct script_state_t *)
                          state->common.script_state_stack.data)[state->common.script_state_stack.size - 1u];

        if (top_state->command_to_process_index >= top_state->script->commands_count)
        {
            serialization_common_state_pop_script_state (&state->common);
            if (state->common.script_state_stack.size == 0u)
            {
                return KAN_SERIALIZATION_FINISHED;
            }
        }
        else
        {
            break;
        }
    }

    return KAN_SERIALIZATION_IN_PROGRESS;
}

void kan_serialization_binary_writer_destroy (kan_serialization_binary_writer_t writer)
{
    struct serialization_write_state_t *state = KAN_HANDLE_GET (writer);
    serialization_common_state_shutdown (&state->common);
    kan_free_general (serialization_allocation_group, state, sizeof (struct serialization_write_state_t));
}

bool kan_serialization_binary_read_type_header (struct kan_stream_t *stream,
                                                kan_interned_string_t *type_name_output,
                                                kan_serialization_interned_string_registry_t interned_string_registry)
{
    ensure_statics_initialized ();
    return read_interned_string_stateless (stream, KAN_HANDLE_GET (interned_string_registry), type_name_output);
}

bool kan_serialization_binary_write_type_header (struct kan_stream_t *stream,
                                                 kan_interned_string_t type_name,
                                                 kan_serialization_interned_string_registry_t interned_string_registry)
{
    return write_interned_string_stateless (stream, KAN_HANDLE_GET (interned_string_registry), type_name);
}
