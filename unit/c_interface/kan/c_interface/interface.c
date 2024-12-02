#include <string.h>

#include <kan/c_interface/builder.h>
#include <kan/c_interface/interface.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/hash_storage.h>
#include <kan/error/critical.h>
#include <kan/memory/allocation.h>

static kan_bool_t allocation_group_ready = KAN_FALSE;
static kan_allocation_group_t allocation_group;

kan_allocation_group_t kan_c_interface_allocation_group (void)
{
    if (!allocation_group_ready)
    {
        allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "c_interface");
        allocation_group_ready = KAN_TRUE;
    }

    return allocation_group;
}

struct string_encoding_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t string;
    kan_serialized_size_t value;
};

struct string_encoding_context_t
{
    struct kan_hash_storage_t hash_storage;
    struct kan_dynamic_array_t order;
    kan_bool_t serialized;
};

static struct string_encoding_node_t *string_encoding_node_create (void)
{
    return kan_allocate_batched (kan_c_interface_allocation_group (), sizeof (struct string_encoding_node_t));
}

static void string_encoding_node_destroy (struct string_encoding_node_t *node)
{
    kan_free_batched (kan_c_interface_allocation_group (), node);
}

static kan_serialized_size_t encode_interned_string (struct string_encoding_context_t *context,
                                                     kan_interned_string_t string)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&context->hash_storage, KAN_HASH_OBJECT_POINTER (string));
    struct string_encoding_node_t *node = (struct string_encoding_node_t *) bucket->first;
    const struct string_encoding_node_t *end =
        (struct string_encoding_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->string == string)
        {
            return node->value;
        }

        node = (struct string_encoding_node_t *) node->node.list_node.next;
    }

    KAN_ASSERT (!context->serialized)
    struct string_encoding_node_t *new_node = string_encoding_node_create ();
    new_node->node.hash = KAN_HASH_OBJECT_POINTER (string);
    new_node->string = string;
    new_node->value = context->order.size;

    kan_hash_storage_update_bucket_count_default (&context->hash_storage,
                                                  KAN_C_INTERFACE_STRING_ENCODING_INITIAL_BUCKETS);
    kan_hash_storage_add (&context->hash_storage, &new_node->node);

    kan_interned_string_t *in_order = kan_dynamic_array_add_last (&context->order);
    if (!in_order)
    {
        kan_dynamic_array_set_capacity (&context->order, context->order.size * 2u);
        in_order = kan_dynamic_array_add_last (&context->order);
    }

    *in_order = string;
    return new_node->value;
}

static void meta_attachment_add_to_encoding_context (struct string_encoding_context_t *context,
                                                     const struct kan_c_meta_attachment_t *attachment)
{
    for (kan_loop_size_t meta_index = 0u; meta_index < attachment->meta_count; ++meta_index)
    {
        struct kan_c_meta_t *meta = &attachment->meta_array[meta_index];
        encode_interned_string (context, meta->name);

        if (meta->type == KAN_C_META_STRING)
        {
            encode_interned_string (context, meta->string_value);
        }
    }
}

static void variable_add_to_encoding_context (struct string_encoding_context_t *context,
                                              const struct kan_c_variable_t *variable)
{
    encode_interned_string (context, variable->name);
    encode_interned_string (context, variable->type.name);
    meta_attachment_add_to_encoding_context (context, &variable->meta);
}

static void c_interface_add_to_encoding_context (struct string_encoding_context_t *context,
                                                 const struct kan_c_interface_t *interface)
{
    for (kan_loop_size_t enum_index = 0u; enum_index < interface->enums_count; ++enum_index)
    {
        struct kan_c_enum_t *enum_info = &interface->enums[enum_index];
        encode_interned_string (context, enum_info->name);
        meta_attachment_add_to_encoding_context (context, &enum_info->meta);

        for (kan_loop_size_t value_index = 0u; value_index < enum_info->values_count; ++value_index)
        {
            struct kan_c_enum_value_t *value = &enum_info->values[value_index];
            encode_interned_string (context, value->name);
            meta_attachment_add_to_encoding_context (context, &value->meta);
        }
    }

    for (kan_loop_size_t struct_index = 0u; struct_index < interface->structs_count; ++struct_index)
    {
        struct kan_c_struct_t *struct_info = &interface->structs[struct_index];
        encode_interned_string (context, struct_info->name);
        meta_attachment_add_to_encoding_context (context, &struct_info->meta);

        for (kan_loop_size_t field_index = 0u; field_index < struct_info->fields_count; ++field_index)
        {
            variable_add_to_encoding_context (context, &struct_info->fields[field_index]);
        }
    }

    for (kan_loop_size_t function_index = 0u; function_index < interface->functions_count; ++function_index)
    {
        struct kan_c_function_t *function_info = &interface->functions[function_index];
        encode_interned_string (context, function_info->name);
        encode_interned_string (context, function_info->return_type.name);
        meta_attachment_add_to_encoding_context (context, &function_info->meta);

        for (kan_loop_size_t argument_index = 0u; argument_index < function_info->arguments_count; ++argument_index)
        {
            variable_add_to_encoding_context (context, &function_info->arguments[argument_index]);
        }
    }

    for (kan_loop_size_t symbol_index = 0u; symbol_index < interface->symbols_count; ++symbol_index)
    {
        variable_add_to_encoding_context (context, &interface->symbols[symbol_index]);
    }
}

static kan_bool_t serialize_encoded_strings (struct string_encoding_context_t *context, struct kan_stream_t *stream)
{
    KAN_ASSERT (context->order.size < UINT32_MAX)
    const kan_instance_size_t count = context->order.size;

    if (stream->operations->write (stream, sizeof (kan_instance_size_t), &count) != sizeof (kan_instance_size_t))
    {
        return KAN_FALSE;
    }

    kan_interned_string_t *strings = (kan_interned_string_t *) context->order.data;
    for (kan_loop_size_t string_index = 0u; string_index < context->order.size; ++string_index)
    {
        const kan_instance_size_t length = (kan_instance_size_t) strlen (strings[string_index]);
        KAN_ASSERT (length < KAN_C_INTERFACE_ENCODED_STRING_MAX_LENGTH)
        _Static_assert (KAN_C_INTERFACE_ENCODED_STRING_MAX_LENGTH <= UINT8_MAX,
                        "Max length of encoded string fits into byte.");
        const uint8_t length_byte = (uint8_t) length;

        if (stream->operations->write (stream, sizeof (uint8_t), &length_byte) != sizeof (uint8_t) ||
            stream->operations->write (stream, length, strings[string_index]) != length)
        {
            return KAN_FALSE;
        }
    }

    context->serialized = KAN_TRUE;
    return KAN_TRUE;
}

static kan_bool_t meta_attachment_serialize (struct kan_c_meta_attachment_t *attachment,
                                             struct string_encoding_context_t *context,
                                             struct kan_stream_t *stream)
{
    KAN_ASSERT (attachment->meta_count < UINT8_MAX)
    const uint8_t count = (uint8_t) attachment->meta_count;

    if (stream->operations->write (stream, sizeof (uint8_t), &count) != sizeof (uint8_t))
    {
        return KAN_FALSE;
    }

    for (kan_loop_size_t meta_index = 0u; meta_index < attachment->meta_count; ++meta_index)
    {
        struct kan_c_meta_t *meta = &attachment->meta_array[meta_index];
        const kan_serialized_size_t encoded_name = encode_interned_string (context, meta->name);
        const uint8_t type_byte = (uint8_t) meta->type;

        if (stream->operations->write (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                sizeof (kan_serialized_size_t) ||
            stream->operations->write (stream, sizeof (uint8_t), &type_byte) != sizeof (uint8_t))
        {
            return KAN_FALSE;
        }

        switch (meta->type)
        {
        case KAN_C_META_MARKER:
            break;

        case KAN_C_META_INTEGER:
            if (stream->operations->write (stream, sizeof (int64_t), &meta->integer_value) != sizeof (int64_t))
            {
                return KAN_FALSE;
            }

            break;

        case KAN_C_META_STRING:
        {
            const kan_serialized_size_t encoded_value = encode_interned_string (context, meta->string_value);
            if (stream->operations->write (stream, sizeof (kan_serialized_size_t), &encoded_value) !=
                sizeof (kan_serialized_size_t))
            {
                return KAN_FALSE;
            }

            break;
        }
        }
    }

    return KAN_TRUE;
}

static kan_bool_t type_serialize (struct kan_c_type_t *type,
                                  struct string_encoding_context_t *context,
                                  struct kan_stream_t *stream)
{
    const kan_serialized_size_t encoded_name = encode_interned_string (context, type->name);
    const uint8_t archetype_byte = (uint8_t) type->archetype;

    return stream->operations->write (stream, sizeof (kan_serialized_size_t), &encoded_name) ==
               sizeof (kan_serialized_size_t) &&
           stream->operations->write (stream, sizeof (uint8_t), &archetype_byte) == sizeof (uint8_t) &&
           stream->operations->write (stream, sizeof (kan_bool_t), &type->is_const) == sizeof (kan_bool_t) &&
           stream->operations->write (stream, sizeof (kan_bool_t), &type->is_array) == sizeof (kan_bool_t) &&
           stream->operations->write (stream, sizeof (uint8_t), &type->pointer_level) == sizeof (uint8_t);
}

static kan_bool_t c_interface_serialize (const struct kan_c_interface_t *interface,
                                         struct string_encoding_context_t *context,
                                         struct kan_stream_t *stream)
{
    KAN_ASSERT (interface->enums_count < UINT8_MAX)
    uint8_t count = (uint8_t) interface->enums_count;

    if (stream->operations->write (stream, sizeof (uint8_t), &count) != sizeof (uint8_t))
    {
        return KAN_FALSE;
    }

    for (kan_loop_size_t enum_index = 0u; enum_index < interface->enums_count; ++enum_index)
    {
        struct kan_c_enum_t *enum_info = &interface->enums[enum_index];
        kan_serialized_size_t encoded_name = encode_interned_string (context, enum_info->name);
        KAN_ASSERT (enum_info->values_count < UINT8_MAX)
        count = (uint8_t) enum_info->values_count;

        if (stream->operations->write (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                sizeof (kan_serialized_size_t) ||
            !meta_attachment_serialize (&enum_info->meta, context, stream) ||
            stream->operations->write (stream, sizeof (uint8_t), &count) != sizeof (uint8_t))
        {
            return KAN_FALSE;
        }

        for (kan_loop_size_t value_index = 0u; value_index < enum_info->values_count; ++value_index)
        {
            struct kan_c_enum_value_t *value = &enum_info->values[value_index];
            encoded_name = encode_interned_string (context, value->name);

            if (stream->operations->write (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                    sizeof (kan_serialized_size_t) ||
                !meta_attachment_serialize (&value->meta, context, stream))
            {
                return KAN_FALSE;
            }
        }
    }

    KAN_ASSERT (interface->structs_count < UINT8_MAX)
    count = (uint8_t) interface->structs_count;

    if (stream->operations->write (stream, sizeof (uint8_t), &interface->structs_count) != sizeof (uint8_t))
    {
        return KAN_FALSE;
    }

    for (kan_loop_size_t struct_index = 0u; struct_index < interface->structs_count; ++struct_index)
    {
        struct kan_c_struct_t *struct_info = &interface->structs[struct_index];
        kan_serialized_size_t encoded_name = encode_interned_string (context, struct_info->name);
        KAN_ASSERT (struct_info->fields_count < UINT8_MAX)
        count = (uint8_t) struct_info->fields_count;

        if (stream->operations->write (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                sizeof (kan_serialized_size_t) ||
            !meta_attachment_serialize (&struct_info->meta, context, stream) ||
            stream->operations->write (stream, sizeof (uint8_t), &count) != sizeof (uint8_t))
        {
            return KAN_FALSE;
        }

        for (kan_loop_size_t field_index = 0u; field_index < struct_info->fields_count; ++field_index)
        {
            struct kan_c_variable_t *field_info = &struct_info->fields[field_index];
            encoded_name = encode_interned_string (context, field_info->name);

            if (stream->operations->write (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                    sizeof (kan_serialized_size_t) ||
                !type_serialize (&field_info->type, context, stream) ||
                !meta_attachment_serialize (&field_info->meta, context, stream))
            {
                return KAN_FALSE;
            }
        }
    }

    KAN_ASSERT (interface->functions_count < UINT8_MAX)
    count = (uint8_t) interface->functions_count;

    if (stream->operations->write (stream, sizeof (uint8_t), &count) != sizeof (uint8_t))
    {
        return KAN_FALSE;
    }

    for (kan_loop_size_t function_index = 0u; function_index < interface->functions_count; ++function_index)
    {
        struct kan_c_function_t *function_info = &interface->functions[function_index];
        kan_serialized_size_t encoded_name = encode_interned_string (context, function_info->name);
        KAN_ASSERT (function_info->arguments_count < UINT8_MAX)
        count = (uint8_t) function_info->arguments_count;

        if (stream->operations->write (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                sizeof (kan_serialized_size_t) ||
            !type_serialize (&function_info->return_type, context, stream) ||
            !meta_attachment_serialize (&function_info->meta, context, stream) ||
            stream->operations->write (stream, sizeof (uint8_t), &count) != sizeof (uint8_t))
        {
            return KAN_FALSE;
        }

        for (kan_loop_size_t argument_index = 0u; argument_index < function_info->arguments_count; ++argument_index)
        {
            struct kan_c_variable_t *argument_info = &function_info->arguments[argument_index];
            encoded_name = encode_interned_string (context, argument_info->name);

            if (stream->operations->write (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                    sizeof (kan_serialized_size_t) ||
                !type_serialize (&argument_info->type, context, stream) ||
                !meta_attachment_serialize (&argument_info->meta, context, stream))
            {
                return KAN_FALSE;
            }
        }
    }

    KAN_ASSERT (interface->symbols_count < UINT8_MAX)
    count = (uint8_t) interface->symbols_count;

    if (stream->operations->write (stream, sizeof (uint8_t), &count) != sizeof (uint8_t))
    {
        return KAN_FALSE;
    }

    for (kan_loop_size_t symbol_index = 0u; symbol_index < interface->symbols_count; ++symbol_index)
    {
        struct kan_c_variable_t *symbol_info = &interface->symbols[symbol_index];
        const kan_serialized_size_t encoded_name = encode_interned_string (context, symbol_info->name);

        if (stream->operations->write (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                sizeof (kan_serialized_size_t) ||
            !type_serialize (&symbol_info->type, context, stream) ||
            !meta_attachment_serialize (&symbol_info->meta, context, stream))
        {
            return KAN_FALSE;
        }
    }

    return KAN_TRUE;
}

kan_bool_t kan_c_interface_serialize (const struct kan_c_interface_t *interface, struct kan_stream_t *stream)
{
    if (!kan_stream_is_writeable (stream))
    {
        return KAN_FALSE;
    }

    struct string_encoding_context_t encoding_context;
    kan_hash_storage_init (&encoding_context.hash_storage, kan_c_interface_allocation_group (),
                           KAN_C_INTERFACE_STRING_ENCODING_INITIAL_BUCKETS);
    kan_dynamic_array_init (&encoding_context.order, KAN_C_INTERFACE_STRING_ENCODING_INITIAL_CAPACITY,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_c_interface_allocation_group ());
    encoding_context.serialized = KAN_FALSE;

    c_interface_add_to_encoding_context (&encoding_context, interface);
    kan_bool_t result = KAN_TRUE;

    if ((result = serialize_encoded_strings (&encoding_context, stream)))
    {
        result = c_interface_serialize (interface, &encoding_context, stream);
    }

    struct kan_bd_list_node_t *node = encoding_context.hash_storage.items.first;
    while (node)
    {
        struct kan_bd_list_node_t *next = node->next;
        string_encoding_node_destroy ((struct string_encoding_node_t *) node);
        node = next;
    }

    kan_hash_storage_shutdown (&encoding_context.hash_storage);
    kan_dynamic_array_shutdown (&encoding_context.order);
    return result;
}

struct encoded_strings_info_t
{
    kan_instance_size_t count;
    kan_interned_string_t *strings;
};

static kan_bool_t deserialize_encoded_strings (struct kan_stream_t *stream, struct encoded_strings_info_t *output)
{
    output->count = 0u;
    output->strings = NULL;

    kan_instance_size_t serialized_count;
    if (stream->operations->read (stream, sizeof (kan_instance_size_t), &serialized_count) !=
        sizeof (kan_instance_size_t))
    {
        return KAN_FALSE;
    }

    output->count = serialized_count;
    if (output->count == 0u)
    {
        return KAN_TRUE;
    }

    output->strings =
        kan_allocate_general (kan_c_interface_allocation_group (), sizeof (kan_interned_string_t) * output->count,
                              _Alignof (kan_interned_string_t));
    char buffer[KAN_C_INTERFACE_ENCODED_STRING_MAX_LENGTH + 1u];

    for (kan_loop_size_t index = 0u; index < output->count; ++index)
    {
        uint8_t serialized_length;
        if (stream->operations->read (stream, sizeof (uint8_t), &serialized_length) == sizeof (uint8_t))
        {
            if (stream->operations->read (stream, serialized_length, buffer) == serialized_length)
            {
                buffer[serialized_length] = '\0';
                output->strings[index] = kan_string_intern (buffer);
                continue;
            }
        }

        // Failed, free memory and exit.
        kan_free_general (kan_c_interface_allocation_group (), output->strings,
                          sizeof (kan_interned_string_t) * output->count);
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static kan_bool_t meta_attachment_deserialize (struct kan_c_meta_attachment_t *attachment,
                                               struct encoded_strings_info_t *encoded_strings,
                                               struct kan_stream_t *stream)
{
    attachment->meta_count = 0u;
    attachment->meta_array = NULL;

    uint8_t count;
    if (stream->operations->read (stream, sizeof (uint8_t), &count) != sizeof (count))
    {
        return KAN_FALSE;
    }

    attachment->meta_count = count;
    if (attachment->meta_count == 0u)
    {
        attachment->meta_array = NULL;
        return KAN_TRUE;
    }

    attachment->meta_array =
        kan_allocate_general (kan_c_interface_allocation_group (),
                              sizeof (struct kan_c_meta_t) * attachment->meta_count, _Alignof (struct kan_c_meta_t));

    for (kan_loop_size_t meta_index = 0u; meta_index < attachment->meta_count; ++meta_index)
    {
        struct kan_c_meta_t *meta = &attachment->meta_array[meta_index];
        kan_serialized_size_t encoded_name;
        uint8_t encoded_type;

        if (stream->operations->read (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                sizeof (kan_serialized_size_t) ||
            stream->operations->read (stream, sizeof (uint8_t), &encoded_type) != sizeof (uint8_t))
        {
            return KAN_FALSE;
        }

        meta->name = encoded_strings->strings[encoded_name];
        meta->type = (enum kan_c_meta_type_t) encoded_type;

        switch (meta->type)
        {
        case KAN_C_META_MARKER:
            break;

        case KAN_C_META_INTEGER:
            if (stream->operations->read (stream, sizeof (int64_t), &meta->integer_value) != sizeof (int64_t))
            {
                return KAN_FALSE;
            }

            break;

        case KAN_C_META_STRING:
        {
            kan_serialized_size_t encoded_value;
            if (stream->operations->read (stream, sizeof (kan_serialized_size_t), &encoded_value) !=
                sizeof (kan_serialized_size_t))
            {
                return KAN_FALSE;
            }

            meta->string_value = encoded_strings->strings[encoded_value];
            break;
        }
        }
    }

    return KAN_TRUE;
}

static kan_bool_t type_deserialize (struct kan_c_type_t *type,
                                    struct encoded_strings_info_t *encoded_strings,
                                    struct kan_stream_t *stream)
{
    kan_serialized_size_t encoded_name;
    uint8_t archetype_byte;

    if (stream->operations->read (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
            sizeof (kan_serialized_size_t) ||
        stream->operations->read (stream, sizeof (uint8_t), &archetype_byte) != sizeof (uint8_t) ||
        stream->operations->read (stream, sizeof (kan_bool_t), &type->is_const) != sizeof (kan_bool_t) ||
        stream->operations->read (stream, sizeof (kan_bool_t), &type->is_array) != sizeof (kan_bool_t) ||
        stream->operations->read (stream, sizeof (uint8_t), &type->pointer_level) != sizeof (uint8_t))

    {
        return KAN_FALSE;
    }

    type->name = encoded_strings->strings[encoded_name];
    type->archetype = (enum kan_c_archetype_t) archetype_byte;
    return KAN_TRUE;
}

static kan_bool_t c_interface_deserialize (struct kan_c_interface_t *interface,
                                           struct encoded_strings_info_t *encoded_strings,
                                           struct kan_stream_t *stream)
{
    uint8_t count;
    if (stream->operations->read (stream, sizeof (uint8_t), &count) != sizeof (uint8_t))
    {
        return KAN_FALSE;
    }

    kan_c_interface_init_enums_array (interface, count);
    for (kan_loop_size_t enum_index = 0u; enum_index < interface->enums_count; ++enum_index)
    {
        struct kan_c_enum_t *enum_info = &interface->enums[enum_index];
        kan_serialized_size_t encoded_name;

        if (stream->operations->read (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                sizeof (kan_serialized_size_t) ||
            !meta_attachment_deserialize (&enum_info->meta, encoded_strings, stream) ||
            stream->operations->read (stream, sizeof (uint8_t), &count) != sizeof (uint8_t))
        {
            return KAN_FALSE;
        }

        enum_info->name = encoded_strings->strings[encoded_name];
        kan_c_enum_init_values_array (enum_info, count);

        for (kan_loop_size_t value_index = 0u; value_index < enum_info->values_count; ++value_index)
        {
            struct kan_c_enum_value_t *value_info = &enum_info->values[value_index];
            if (stream->operations->read (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                    sizeof (kan_serialized_size_t) ||
                !meta_attachment_deserialize (&value_info->meta, encoded_strings, stream))
            {
                return KAN_FALSE;
            }

            value_info->name = encoded_strings->strings[encoded_name];
        }
    }

    if (stream->operations->read (stream, sizeof (uint8_t), &count) != sizeof (uint8_t))
    {
        return KAN_FALSE;
    }

    kan_c_interface_init_structs_array (interface, count);
    for (kan_loop_size_t struct_index = 0u; struct_index < interface->structs_count; ++struct_index)
    {
        struct kan_c_struct_t *struct_info = &interface->structs[struct_index];
        kan_serialized_size_t encoded_name;

        if (stream->operations->read (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                sizeof (kan_serialized_size_t) ||
            !meta_attachment_deserialize (&struct_info->meta, encoded_strings, stream) ||
            stream->operations->read (stream, sizeof (uint8_t), &count) != sizeof (uint8_t))
        {
            return KAN_FALSE;
        }

        struct_info->name = encoded_strings->strings[encoded_name];
        kan_c_struct_init_fields_array (struct_info, count);

        for (kan_loop_size_t index = 0u; index < struct_info->fields_count; ++index)
        {
            struct kan_c_variable_t *info = &struct_info->fields[index];
            if (stream->operations->read (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                    sizeof (kan_serialized_size_t) ||
                !type_deserialize (&info->type, encoded_strings, stream) ||
                !meta_attachment_deserialize (&info->meta, encoded_strings, stream))
            {
                return KAN_FALSE;
            }

            info->name = encoded_strings->strings[encoded_name];
        }
    }

    if (stream->operations->read (stream, sizeof (uint8_t), &count) != sizeof (uint8_t))
    {
        return KAN_FALSE;
    }

    kan_c_interface_init_functions_array (interface, count);
    for (kan_loop_size_t function_index = 0u; function_index < interface->functions_count; ++function_index)
    {
        struct kan_c_function_t *function_info = &interface->functions[function_index];
        kan_serialized_size_t encoded_name;

        if (stream->operations->read (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                sizeof (kan_serialized_size_t) ||
            !type_deserialize (&function_info->return_type, encoded_strings, stream) ||
            !meta_attachment_deserialize (&function_info->meta, encoded_strings, stream) ||
            stream->operations->read (stream, sizeof (uint8_t), &count) != sizeof (uint8_t))
        {
            return KAN_FALSE;
        }

        function_info->name = encoded_strings->strings[encoded_name];
        kan_c_function_init_arguments_array (function_info, count);

        for (kan_loop_size_t index = 0u; index < function_info->arguments_count; ++index)
        {
            struct kan_c_variable_t *info = &function_info->arguments[index];
            if (stream->operations->read (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                    sizeof (kan_serialized_size_t) ||
                !type_deserialize (&info->type, encoded_strings, stream) ||
                !meta_attachment_deserialize (&info->meta, encoded_strings, stream))
            {
                return KAN_FALSE;
            }

            info->name = encoded_strings->strings[encoded_name];
        }
    }

    if (stream->operations->read (stream, sizeof (uint8_t), &count) != sizeof (uint8_t))
    {
        return KAN_FALSE;
    }

    kan_c_interface_init_symbols_array (interface, count);
    for (kan_loop_size_t symbol_index = 0u; symbol_index < interface->symbols_count; ++symbol_index)
    {
        struct kan_c_variable_t *symbol_info = &interface->symbols[symbol_index];
        kan_serialized_size_t encoded_name;

        if (stream->operations->read (stream, sizeof (kan_serialized_size_t), &encoded_name) !=
                sizeof (kan_serialized_size_t) ||
            !type_deserialize (&symbol_info->type, encoded_strings, stream) ||
            !meta_attachment_deserialize (&symbol_info->meta, encoded_strings, stream))
        {
            return KAN_FALSE;
        }

        symbol_info->name = encoded_strings->strings[encoded_name];
    }

    return KAN_TRUE;
}

struct kan_c_interface_t *kan_c_interface_deserialize (struct kan_stream_t *stream)
{
    struct kan_c_interface_t *interface = (struct kan_c_interface_t *) kan_allocate_batched (
        kan_c_interface_allocation_group (), sizeof (struct kan_c_interface_t));

    interface->enums_count = 0u;
    interface->enums = NULL;

    interface->structs_count = 0u;
    interface->structs = NULL;

    interface->functions_count = 0u;
    interface->functions = NULL;

    interface->symbols_count = 0u;
    interface->symbols = NULL;

    kan_bool_t result = KAN_TRUE;
    struct encoded_strings_info_t info;

    if (deserialize_encoded_strings (stream, &info))
    {
        result = c_interface_deserialize (interface, &info, stream);
        kan_free_general (kan_c_interface_allocation_group (), info.strings,
                          sizeof (kan_interned_string_t) * info.count);
    }
    else
    {
        result = KAN_FALSE;
    }

    if (result)
    {
        return interface;
    }

    kan_c_interface_destroy (interface);
    return NULL;
}

static void meta_attachment_shutdown (struct kan_c_meta_attachment_t *attachment)
{
    if (attachment->meta_array)
    {
        kan_free_general (kan_c_interface_allocation_group (), attachment->meta_array,
                          attachment->meta_count * sizeof (struct kan_c_meta_t));
    }
}

void kan_c_interface_destroy (struct kan_c_interface_t *interface)
{
    for (kan_loop_size_t enum_index = 0u; enum_index < interface->enums_count; ++enum_index)
    {
        struct kan_c_enum_t *enum_info = &interface->enums[enum_index];

        for (kan_loop_size_t value_index = 0u; value_index < enum_info->values_count; ++value_index)
        {
            meta_attachment_shutdown (&enum_info->values[value_index].meta);
        }

        if (enum_info->values)
        {
            kan_free_general (kan_c_interface_allocation_group (), enum_info->values,
                              enum_info->values_count * sizeof (struct kan_c_enum_value_t));
        }

        meta_attachment_shutdown (&enum_info->meta);
    }

    if (interface->enums)
    {
        kan_free_general (kan_c_interface_allocation_group (), interface->enums,
                          interface->enums_count * sizeof (struct kan_c_enum_t));
    }

    for (kan_loop_size_t struct_index = 0u; struct_index < interface->structs_count; ++struct_index)
    {
        struct kan_c_struct_t *struct_info = &interface->structs[struct_index];

        for (kan_loop_size_t field_index = 0u; field_index < struct_info->fields_count; ++field_index)
        {
            meta_attachment_shutdown (&struct_info->fields[field_index].meta);
        }

        if (struct_info->fields)
        {
            kan_free_general (kan_c_interface_allocation_group (), struct_info->fields,
                              struct_info->fields_count * sizeof (struct kan_c_variable_t));
        }

        meta_attachment_shutdown (&struct_info->meta);
    }

    if (interface->structs)
    {
        kan_free_general (kan_c_interface_allocation_group (), interface->structs,
                          interface->structs_count * sizeof (struct kan_c_struct_t));
    }

    for (kan_loop_size_t function_index = 0u; function_index < interface->functions_count; ++function_index)
    {
        struct kan_c_function_t *function_info = &interface->functions[function_index];

        for (kan_loop_size_t argument_index = 0u; argument_index < function_info->arguments_count; ++argument_index)
        {
            meta_attachment_shutdown (&function_info->arguments[argument_index].meta);
        }

        if (function_info->arguments)
        {
            kan_free_general (kan_c_interface_allocation_group (), function_info->arguments,
                              function_info->arguments_count * sizeof (struct kan_c_variable_t));
        }

        meta_attachment_shutdown (&function_info->meta);
    }

    if (interface->functions)
    {
        kan_free_general (kan_c_interface_allocation_group (), interface->functions,
                          interface->functions_count * sizeof (struct kan_c_function_t));
    }

    for (kan_loop_size_t index = 0u; index < interface->symbols_count; ++index)
    {
        meta_attachment_shutdown (&interface->symbols[index].meta);
    }

    if (interface->symbols)
    {
        kan_free_general (kan_c_interface_allocation_group (), interface->symbols,
                          interface->symbols_count * sizeof (struct kan_c_variable_t));
    }

    kan_free_batched (kan_c_interface_allocation_group (), interface);
}
