#define _CRT_SECURE_NO_WARNINGS

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <kan/c_interface/file.h>
#include <kan/container/hash_storage.h>
#include <kan/container/trivial_string_buffer.h>
#include <kan/file_system/stream.h>

#define RETURN_CODE_SUCCESS 0
#define RETURN_CODE_INVALID_ARGUMENTS (-1)
#define RETURN_CODE_INPUT_IO_ERROR (-2)
#define RETURN_CODE_NOT_ENOUGH_DATA_FOR_DYNAMIC_ARRAY (-3)
#define RETURN_CODE_UNKNOWN_SIZE_FIELD (-4)
#define RETURN_CODE_UNKNOWN_VISIBILITY_FIELD (-5)
#define RETURN_CODE_INVALID_PARSABLE_META_TARGET (-6)
#define RETURN_CODE_UNABLE_TO_OPEN_OUTPUT (-7)
#define RETURN_CODE_IO_OUTPUT_FAILED (-8)

#define OUTPUT_BUFFER_INITIAL_CAPACITY 524288u // 512 kilobytes

#define MAX_STRUCT_NAME_LENGTH 128u

// Global state.

static struct
{
    const char *module_name;
    const char *output_file_path;

    uint64_t input_files_count;
    char **input_files;
} arguments;

static struct
{
    struct kan_c_interface_file_t *input_files;
    struct kan_trivial_string_buffer_t output_buffer;
} io = {.input_files = NULL, .output_buffer = {0u, 0u, KAN_ALLOCATION_GROUP_IGNORE, NULL}};

static struct
{
    kan_interned_string_t reflection_flags;
    kan_interned_string_t reflection_ignore_enum;
    kan_interned_string_t reflection_ignore_enum_value;
    kan_interned_string_t reflection_ignore_struct;
    kan_interned_string_t reflection_ignore_struct_field;
    kan_interned_string_t reflection_external_pointer;
    kan_interned_string_t reflection_dynamic_array_type;
    kan_interned_string_t reflection_size_field;
    kan_interned_string_t reflection_visibility_condition_field;
    kan_interned_string_t reflection_visibility_condition_values;
    kan_interned_string_t reflection_enum_meta;
    kan_interned_string_t reflection_enum_value_meta;
    kan_interned_string_t reflection_struct_meta;
    kan_interned_string_t reflection_struct_field_meta;

    kan_interned_string_t type_char;
    kan_interned_string_t type_interned_string;
    kan_interned_string_t type_dynamic_array;
    kan_interned_string_t type_patch;
} interned;

struct functor_registry_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
};

static struct kan_hash_storage_t functor_registry;

static int current_error_code = RETURN_CODE_SUCCESS;

static const char *helper_macros =
    "#define ARCHETYPE_SELECTION_HELPER_GENERIC(EXPRESSION) \\\n"
    "    _Generic ((EXPRESSION),\\\n"
    "        int8_t: KAN_REFLECTION_ARCHETYPE_SIGNED_INT, \\\n"
    "        int16_t: KAN_REFLECTION_ARCHETYPE_SIGNED_INT, \\\n"
    "        int32_t: KAN_REFLECTION_ARCHETYPE_SIGNED_INT, \\\n"
    "        int64_t: KAN_REFLECTION_ARCHETYPE_SIGNED_INT, \\\n"
    "        uint8_t: KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT, \\\n"
    "        uint16_t: KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT, \\\n"
    "        uint32_t: KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT, \\\n"
    "        uint64_t: KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT, \\\n"
    "        float: KAN_REFLECTION_ARCHETYPE_FLOATING, \\\n"
    "        double: KAN_REFLECTION_ARCHETYPE_FLOATING, \\\n"
    "        default: KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT)\n\n"
    "#define ARCHETYPE_SELECTION_HELPER(STRUCTURE, FIELD) \\\n"
    "    ARCHETYPE_SELECTION_HELPER_GENERIC(((struct STRUCTURE *) NULL)->FIELD)\n\n"
    "#define SIZE_OF_FIELD(STRUCTURE, FIELD) sizeof (((struct STRUCTURE *) NULL)->FIELD)\n\n"
    "#if defined(_WIN32)\n"
    "#    define EXPORT_THIS __declspec(dllexport)\n"
    "#else\n"
    "#    define EXPORT_THIS\n"
    "#endif\n\n";

// Helper functions.

static void shutdown (void)
{
    if (io.input_files)
    {
        for (uint64_t index = 0u; index < arguments.input_files_count; ++index)
        {
            kan_c_interface_file_shutdown (&io.input_files[index]);
        }

        kan_free_general (KAN_ALLOCATION_GROUP_IGNORE, io.input_files,
                          arguments.input_files_count * sizeof (struct kan_c_interface_file_t));
    }

    if (io.output_buffer.buffer != NULL)
    {
        kan_trivial_string_buffer_shutdown (&io.output_buffer);
    }

    struct kan_bd_list_node_t *functor_node = functor_registry.items.first;
    while (functor_node)
    {
        struct kan_bd_list_node_t *next = functor_node->next;
        kan_free_batched (KAN_ALLOCATION_GROUP_IGNORE, functor_node);
        functor_node = next;
    }

    kan_hash_storage_shutdown (&functor_registry);
}

static kan_bool_t is_flags (const struct kan_c_meta_attachment_t *meta)
{
    for (uint64_t index = 0u; index < meta->meta_count; ++index)
    {
        if (meta->meta_array[index].type == KAN_C_META_MARKER &&
            meta->meta_array[index].name == interned.reflection_flags)
        {
            return KAN_TRUE;
        }
    }

    return KAN_FALSE;
}

static kan_bool_t is_enum_ignored (const struct kan_c_enum_t *enum_data)
{
    for (uint64_t index = 0u; index < enum_data->meta.meta_count; ++index)
    {
        if (enum_data->meta.meta_array[index].type == KAN_C_META_MARKER &&
            enum_data->meta.meta_array[index].name == interned.reflection_ignore_enum)
        {
            return KAN_TRUE;
        }
    }

    return KAN_FALSE;
}

static kan_bool_t is_enum_value_ignored (const struct kan_c_enum_value_t *enum_value_data)
{
    for (uint64_t index = 0u; index < enum_value_data->meta.meta_count; ++index)
    {
        if (enum_value_data->meta.meta_array[index].type == KAN_C_META_MARKER &&
            enum_value_data->meta.meta_array[index].name == interned.reflection_ignore_enum_value)
        {
            return KAN_TRUE;
        }
    }

    return KAN_FALSE;
}

static kan_bool_t is_struct_ignored (const struct kan_c_struct_t *struct_data)
{
    for (uint64_t index = 0u; index < struct_data->meta.meta_count; ++index)
    {
        if (struct_data->meta.meta_array[index].type == KAN_C_META_MARKER &&
            struct_data->meta.meta_array[index].name == interned.reflection_ignore_struct)
        {
            return KAN_TRUE;
        }
    }

    return KAN_FALSE;
}

static kan_bool_t is_struct_field_ignored (const struct kan_c_variable_t *struct_field_data)
{
    for (uint64_t index = 0u; index < struct_field_data->meta.meta_count; ++index)
    {
        if (struct_field_data->meta.meta_array[index].type == KAN_C_META_MARKER &&
            struct_field_data->meta.meta_array[index].name == interned.reflection_ignore_struct_field)
        {
            return KAN_TRUE;
        }
    }

    return KAN_FALSE;
}

static kan_bool_t is_forced_external_pointer (const struct kan_c_variable_t *field_data)
{
    for (uint64_t index = 0u; index < field_data->meta.meta_count; ++index)
    {
        if (field_data->meta.meta_array[index].type == KAN_C_META_MARKER &&
            field_data->meta.meta_array[index].name == interned.reflection_external_pointer)
        {
            return KAN_TRUE;
        }
    }

    return KAN_FALSE;
}

static void add_array_bootstrap_common_internals (enum kan_c_archetype_t archetype,
                                                  kan_interned_string_t type_name,
                                                  uint64_t pointer_level,
                                                  const struct kan_c_variable_t *field_data)
{
    if (pointer_level > 0u)
    {
        const kan_bool_t forced_external_pointer = is_forced_external_pointer (field_data);
        if (forced_external_pointer || archetype == KAN_C_ARCHETYPE_ENUM ||
            (archetype == KAN_C_ARCHETYPE_BASIC && type_name != interned.type_char))
        {
            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     "            .item_archetype = "
                                                     "KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER,\n");
            kan_trivial_string_buffer_append_string (&io.output_buffer, "            .item_size = sizeof (void *),\n");
        }
        else if (archetype == KAN_C_ARCHETYPE_BASIC && type_name == interned.type_char)
        {
            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     "            .item_archetype = "
                                                     "KAN_REFLECTION_ARCHETYPE_STRING_POINTER,\n");
            kan_trivial_string_buffer_append_string (&io.output_buffer, "            .item_size = sizeof (void *),\n");
        }
        else
        {
            KAN_ASSERT (archetype == KAN_C_ARCHETYPE_STRUCT)
            kan_trivial_string_buffer_append_string (
                &io.output_buffer, "            .item_archetype = KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER,\n");
            kan_trivial_string_buffer_append_string (&io.output_buffer, "            .item_size = sizeof (void *),\n");

            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     "            .item_archetype_struct_pointer = {.type_name = "
                                                     "kan_string_intern "
                                                     "(\"");
            kan_trivial_string_buffer_append_string (&io.output_buffer, type_name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "\")},\n");
        }
    }
    else
    {
        switch (archetype)
        {
        case KAN_C_ARCHETYPE_BASIC:
            if (type_name == interned.type_interned_string)
            {
                kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                         "            .item_archetype = "
                                                         "KAN_REFLECTION_ARCHETYPE_INTERNED_STRING,\n");
            }
            else if (type_name == interned.type_patch)
            {
                kan_trivial_string_buffer_append_string (
                    &io.output_buffer, "            .item_archetype = KAN_REFLECTION_ARCHETYPE_PATCH,\n");
            }
            else
            {
                kan_trivial_string_buffer_append_string (
                    &io.output_buffer, "            .item_archetype = ARCHETYPE_SELECTION_HELPER_GENERIC (*(");
                kan_trivial_string_buffer_append_string (&io.output_buffer, type_name);
                kan_trivial_string_buffer_append_string (&io.output_buffer, " *) NULL),\n");
            }

            kan_trivial_string_buffer_append_string (&io.output_buffer, "            .item_size = sizeof (");
            kan_trivial_string_buffer_append_string (&io.output_buffer, type_name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "),\n");
            break;

        case KAN_C_ARCHETYPE_ENUM:
            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     "            .item_archetype = "
                                                     "KAN_REFLECTION_ARCHETYPE_ENUM,\n");
            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     "            .item_archetype_enum = {.type_name = \"");
            kan_trivial_string_buffer_append_string (&io.output_buffer, type_name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "\"},\n");
            kan_trivial_string_buffer_append_string (&io.output_buffer, "            .item_size = sizeof (int),\n");
            break;

        case KAN_C_ARCHETYPE_STRUCT:
            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     "            .item_archetype = "
                                                     "KAN_REFLECTION_ARCHETYPE_STRUCT,\n");
            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     "            .item_archetype_struct = {.type_name = \"");
            kan_trivial_string_buffer_append_string (&io.output_buffer, type_name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "\"},\n");
            kan_trivial_string_buffer_append_string (&io.output_buffer, "            .item_size = sizeof (struct ");
            kan_trivial_string_buffer_append_string (&io.output_buffer, type_name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "),\n");
            break;
        }
    }
}

static kan_bool_t is_functor_registered (kan_interned_string_t functor_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&functor_registry, (uint64_t) functor_name);
    struct functor_registry_node_t *node = (struct functor_registry_node_t *) bucket->first;
    const struct functor_registry_node_t *node_end =
        (struct functor_registry_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == functor_name)
        {
            return KAN_TRUE;
        }

        node = (struct functor_registry_node_t *) node->node.list_node.next;
    }

    return KAN_FALSE;
}

static void register_functor (kan_interned_string_t functor_name)
{
    if (is_functor_registered (functor_name))
    {
        return;
    }

    struct functor_registry_node_t *node =
        kan_allocate_batched (KAN_ALLOCATION_GROUP_IGNORE, sizeof (struct functor_registry_node_t));

    node->node.hash = (uint64_t) functor_name;
    node->name = functor_name;

    if (functor_registry.items.size >= functor_registry.bucket_count * KAN_REFLECTION_GENERATOR_FUNCTORS_LOAD_FACTOR)
    {
        kan_hash_storage_set_bucket_count (&functor_registry, functor_registry.bucket_count * 2u);
    }

    kan_hash_storage_add (&functor_registry, &node->node);
}

static inline kan_interned_string_t build_init_functor_name (const struct kan_c_struct_t *struct_data)
{
    char value[MAX_STRUCT_NAME_LENGTH + 14u] = "init_functor_";
    strcpy (value + 13u, struct_data->name);
    return kan_string_intern (value);
}

static void add_init_functor (const struct kan_c_struct_t *struct_data, kan_interned_string_t backend_function_name)
{
    const kan_interned_string_t functor_name = build_init_functor_name (struct_data);

    kan_trivial_string_buffer_append_string (&io.output_buffer, "static void ");
    kan_trivial_string_buffer_append_string (&io.output_buffer, functor_name);
    kan_trivial_string_buffer_append_string (
        &io.output_buffer, " (kan_reflection_functor_user_data_t user_data, void *generic_instance)\n{\n    ");

    kan_trivial_string_buffer_append_string (&io.output_buffer, backend_function_name);
    kan_trivial_string_buffer_append_string (&io.output_buffer, " ((struct ");
    kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
    kan_trivial_string_buffer_append_string (&io.output_buffer, " *) generic_instance);\n}\n\n");

    register_functor (functor_name);
}

static inline kan_interned_string_t build_shutdown_functor_name (const struct kan_c_struct_t *struct_data)
{
    char value[MAX_STRUCT_NAME_LENGTH + 18u] = "shutdown_functor_";
    strcpy (value + 17u, struct_data->name);
    return kan_string_intern (value);
}

static void add_shutdown_functor (const struct kan_c_struct_t *struct_data, kan_interned_string_t backend_function_name)
{
    const kan_interned_string_t functor_name = build_shutdown_functor_name (struct_data);

    kan_trivial_string_buffer_append_string (&io.output_buffer, "static void ");
    kan_trivial_string_buffer_append_string (&io.output_buffer, functor_name);
    kan_trivial_string_buffer_append_string (
        &io.output_buffer, " (kan_reflection_functor_user_data_t user_data, void *generic_instance)\n{\n    ");

    kan_trivial_string_buffer_append_string (&io.output_buffer, backend_function_name);
    kan_trivial_string_buffer_append_string (&io.output_buffer, " ((struct ");
    kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
    kan_trivial_string_buffer_append_string (&io.output_buffer, " *) generic_instance);\n}\n\n");

    register_functor (functor_name);
}

static const char *get_registered_init_functor_name_or_null_name (const struct kan_c_struct_t *struct_data)
{
    const kan_interned_string_t functor_name = build_init_functor_name (struct_data);
    if (is_functor_registered (functor_name))
    {
        return functor_name;
    }

    return "NULL";
}

static const char *get_registered_shutdown_functor_name_or_null_name (const struct kan_c_struct_t *struct_data)
{
    const kan_interned_string_t functor_name = build_shutdown_functor_name (struct_data);
    if (is_functor_registered (functor_name))
    {
        return functor_name;
    }

    return "NULL";
}

// Execution mainline functions.

static void add_header (void)
{
    kan_trivial_string_buffer_append_string (&io.output_buffer,
                                             "// Autogenerated by reflection_generator tool, do not modify manually.\n"
                                             "#include <stddef.h>\n\n"
                                             "#include <kan/api_common/bool.h>\n"
                                             "#include <kan/api_common/mute_third_party_warnings.h>\n"
                                             "#include <kan/container/interned_string.h>\n"
                                             "#include <kan/reflection/generated_reflection.h>\n"
                                             "#include <kan/reflection/registry.h>\n\n");

    kan_trivial_string_buffer_append_string (&io.output_buffer, helper_macros);
}

static void add_includes (void)
{
    kan_trivial_string_buffer_append_string (&io.output_buffer, "// Section: input includes.\n\n");
    for (uint64_t index = 0u; index < arguments.input_files_count; ++index)
    {
        if (kan_c_interface_file_should_have_includable_object (&io.input_files[index]))
        {
            kan_trivial_string_buffer_append_string (&io.output_buffer, "// Includable object from \"");
            kan_trivial_string_buffer_append_string (&io.output_buffer, io.input_files[index].source_file_path);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "\"\n\n");

            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     "// We're muting warnings because currently includable objects "
                                                     "contain some issues like additional \";\".\n");
            kan_trivial_string_buffer_append_string (&io.output_buffer, "KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN\n\n");

            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     io.input_files[index].optional_includable_object);

            kan_trivial_string_buffer_append_string (&io.output_buffer, "KAN_MUTE_THIRD_PARTY_WARNINGS_END\n\n");

            kan_trivial_string_buffer_append_string (&io.output_buffer, "// End of includable object from \"");
            kan_trivial_string_buffer_append_string (&io.output_buffer, io.input_files[index].source_file_path);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "\"\n\n");
        }
        else
        {
            kan_trivial_string_buffer_append_string (&io.output_buffer, "#include \"");
            kan_trivial_string_buffer_append_string (&io.output_buffer, io.input_files[index].source_file_path);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "\"\n\n");
        }
    }
}

static void add_variables (void)
{
    kan_trivial_string_buffer_append_string (&io.output_buffer, "// Section: reflection data global variables.\n\n");

    for (uint64_t file_index = 0u; file_index < arguments.input_files_count; ++file_index)
    {
        const struct kan_c_interface_t *interface = io.input_files[file_index].interface;

        kan_trivial_string_buffer_append_string (&io.output_buffer, "// Variables from \"");
        kan_trivial_string_buffer_append_string (&io.output_buffer, io.input_files[file_index].source_file_path);
        kan_trivial_string_buffer_append_string (&io.output_buffer, "\"\n");

        for (uint64_t enum_index = 0u; enum_index < interface->enums_count; ++enum_index)
        {
            const struct kan_c_enum_t *enum_data = &interface->enums[enum_index];
            if (is_enum_ignored (enum_data))
            {
                continue;
            }

            unsigned long values_count = 0u;
            for (uint64_t value_index = 0u; value_index < enum_data->values_count; ++value_index)
            {
                if (!is_enum_value_ignored (&enum_data->values[value_index]))
                {
                    ++values_count;
                }
            }

            if (values_count == 0u)
            {
                continue;
            }

            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     "static struct kan_reflection_enum_value_t reflection_");
            kan_trivial_string_buffer_append_string (&io.output_buffer, enum_data->name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "_values[");
            kan_trivial_string_buffer_append_unsigned_long (&io.output_buffer, values_count);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "];\n");

            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     "static struct kan_reflection_enum_t reflection_");
            kan_trivial_string_buffer_append_string (&io.output_buffer, enum_data->name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "_data;\n");
        }

        for (uint64_t struct_index = 0u; struct_index < interface->structs_count; ++struct_index)
        {
            const struct kan_c_struct_t *struct_data = &interface->structs[struct_index];
            if (is_struct_ignored (struct_data))
            {
                continue;
            }

            unsigned long fields_count = 0u;
            for (uint64_t field_index = 0u; field_index < struct_data->fields_count; ++field_index)
            {
                const struct kan_c_variable_t *field_data = &struct_data->fields[field_index];
                if (is_struct_field_ignored (field_data))
                {
                    continue;
                }

                ++fields_count;
                for (uint64_t meta_index = 0u; meta_index < field_data->meta.meta_count; ++meta_index)
                {
                    if (field_data->meta.meta_array[meta_index].name == interned.reflection_visibility_condition_values)
                    {
                        kan_trivial_string_buffer_append_string (&io.output_buffer, "static int64_t reflection_");
                        kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
                        kan_trivial_string_buffer_append_string (&io.output_buffer, "_field_");
                        kan_trivial_string_buffer_append_string (&io.output_buffer, field_data->name);
                        kan_trivial_string_buffer_append_string (&io.output_buffer, "_visibility_values[] = {");

                        if (field_data->meta.meta_array[meta_index].type == KAN_C_META_INTEGER)
                        {
                            kan_trivial_string_buffer_append_signed_long (
                                &io.output_buffer, (long) field_data->meta.meta_array[meta_index].integer_value);
                        }
                        else if (field_data->meta.meta_array[meta_index].type == KAN_C_META_STRING)
                        {
                            kan_trivial_string_buffer_append_string (
                                &io.output_buffer, field_data->meta.meta_array[meta_index].string_value);
                        }

                        kan_trivial_string_buffer_append_string (&io.output_buffer, "};\n");
                        break;
                    }
                }
            }

            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     "static struct kan_reflection_field_t reflection_");
            kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "_fields[");
            kan_trivial_string_buffer_append_unsigned_long (&io.output_buffer, fields_count);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "u];\n");

            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     "static struct kan_reflection_struct_t reflection_");
            kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "_data;\n");
        }

        kan_trivial_string_buffer_append_string (&io.output_buffer, "// End of variables from \"");
        kan_trivial_string_buffer_append_string (&io.output_buffer, io.input_files[file_index].source_file_path);
        kan_trivial_string_buffer_append_string (&io.output_buffer, "\"\n\n");
    }
}

static void add_functors (void)
{
    kan_trivial_string_buffer_append_string (&io.output_buffer, "// Section: reflection global functors.\n\n");

    for (uint64_t file_index = 0u; file_index < arguments.input_files_count; ++file_index)
    {
        const struct kan_c_interface_t *interface = io.input_files[file_index].interface;

        kan_trivial_string_buffer_append_string (&io.output_buffer, "// Functors from \"");
        kan_trivial_string_buffer_append_string (&io.output_buffer, io.input_files[file_index].source_file_path);
        kan_trivial_string_buffer_append_string (&io.output_buffer, "\"\n\n");

        for (uint64_t struct_index = 0u; struct_index < interface->structs_count; ++struct_index)
        {
            const struct kan_c_struct_t *struct_data = &interface->structs[struct_index];
            if (is_struct_ignored (struct_data))
            {
                continue;
            }

            uint64_t struct_data_name_length = strlen (struct_data->name);
            if (struct_data_name_length > 2u && struct_data->name[struct_data_name_length - 2u] == '_' &&
                struct_data->name[struct_data_name_length - 1u] == 't')
            {
                struct_data_name_length -= 2u;
            }

            kan_bool_t init_found = KAN_FALSE;
            kan_bool_t shutdown_found = KAN_FALSE;

            KAN_ASSERT (struct_data_name_length < MAX_STRUCT_NAME_LENGTH)
            char init_function_name_value[MAX_STRUCT_NAME_LENGTH + 6u];
            strncpy (init_function_name_value, struct_data->name, struct_data_name_length);
            strcpy (init_function_name_value + struct_data_name_length, "_init");
            kan_interned_string_t init_function_name = kan_string_intern (init_function_name_value);

            char shutdown_function_name_value[MAX_STRUCT_NAME_LENGTH + 10u];
            strncpy (shutdown_function_name_value, struct_data->name, struct_data_name_length);
            strcpy (shutdown_function_name_value + struct_data_name_length, "_shutdown");
            kan_interned_string_t shutdown_function_name = kan_string_intern (shutdown_function_name_value);

            for (uint64_t function_index = 0u; function_index < interface->functions_count; ++function_index)
            {
                struct kan_c_function_t *function = &interface->functions[function_index];
                if (!init_found && function->name == init_function_name && function->arguments_count == 1u &&
                    function->arguments[0u].type.pointer_level == 1u &&
                    function->arguments[0u].type.archetype == KAN_C_ARCHETYPE_STRUCT &&
                    function->arguments[0u].type.name == struct_data->name)
                {
                    add_init_functor (struct_data, init_function_name);
                    init_found = KAN_TRUE;
                }
                else if (!shutdown_found && function->name == shutdown_function_name &&
                         function->arguments_count == 1u && function->arguments[0u].type.pointer_level == 1u &&
                         function->arguments[0u].type.archetype == KAN_C_ARCHETYPE_STRUCT &&
                         function->arguments[0u].type.name == struct_data->name)
                {
                    add_shutdown_functor (struct_data, shutdown_function_name);
                    shutdown_found = KAN_TRUE;
                }

                if (init_found && shutdown_found)
                {
                    break;
                }
            }
        }

        kan_trivial_string_buffer_append_string (&io.output_buffer, "// End of functors from \"");
        kan_trivial_string_buffer_append_string (&io.output_buffer, io.input_files[file_index].source_file_path);
        kan_trivial_string_buffer_append_string (&io.output_buffer, "\"\n\n");
    }
}

static void add_bootstrap (void)
{
    kan_trivial_string_buffer_append_string (&io.output_buffer,
                                             "// Section: bootstrap that fills all the data.\n\n"
                                             "static kan_bool_t bootstrap_done = KAN_FALSE;\n\n"
                                             "static void ensure_reflection_is_ready (void)\n"
                                             "{\n"
                                             "    if (bootstrap_done)\n"
                                             "    {\n"
                                             "        return;\n"
                                             "    }\n\n");

    for (uint64_t file_index = 0u; file_index < arguments.input_files_count; ++file_index)
    {
        const struct kan_c_interface_t *interface = io.input_files[file_index].interface;

        kan_trivial_string_buffer_append_string (&io.output_buffer, "    // Data from \"");
        kan_trivial_string_buffer_append_string (&io.output_buffer, io.input_files[file_index].source_file_path);
        kan_trivial_string_buffer_append_string (&io.output_buffer, "\"\n\n");

        for (uint64_t enum_index = 0u; enum_index < interface->enums_count; ++enum_index)
        {
            const struct kan_c_enum_t *enum_data = &interface->enums[enum_index];
            if (is_enum_ignored (enum_data))
            {
                continue;
            }

            unsigned long output_value_index = 0u;
            for (uint64_t value_index = 0u; value_index < enum_data->values_count; ++value_index)
            {
                const struct kan_c_enum_value_t *value_data = &enum_data->values[value_index];
                if (is_enum_value_ignored (value_data))
                {
                    continue;
                }

                kan_trivial_string_buffer_append_string (&io.output_buffer, "    reflection_");
                kan_trivial_string_buffer_append_string (&io.output_buffer, enum_data->name);
                kan_trivial_string_buffer_append_string (&io.output_buffer, "_values[");
                kan_trivial_string_buffer_append_unsigned_long (&io.output_buffer, output_value_index);
                kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                         "u] = (struct kan_reflection_enum_value_t) {\n");
                kan_trivial_string_buffer_append_string (&io.output_buffer, "        .name = kan_string_intern (\"");
                kan_trivial_string_buffer_append_string (&io.output_buffer, value_data->name);
                kan_trivial_string_buffer_append_string (&io.output_buffer, "\"),\n");
                kan_trivial_string_buffer_append_string (&io.output_buffer, "        .value = (int64_t) ");
                kan_trivial_string_buffer_append_string (&io.output_buffer, value_data->name);
                kan_trivial_string_buffer_append_string (&io.output_buffer, ",\n");
                kan_trivial_string_buffer_append_string (&io.output_buffer, "    };\n\n");

                ++output_value_index;
            }

            kan_trivial_string_buffer_append_string (&io.output_buffer, "    reflection_");
            kan_trivial_string_buffer_append_string (&io.output_buffer, enum_data->name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "_data = (struct kan_reflection_enum_t) {\n");
            kan_trivial_string_buffer_append_string (&io.output_buffer, "        .name = kan_string_intern (\"");
            kan_trivial_string_buffer_append_string (&io.output_buffer, enum_data->name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "\"),\n");
            kan_trivial_string_buffer_append_string (&io.output_buffer, "        .flags = ");
            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     is_flags (&enum_data->meta) ? "KAN_TRUE" : "KAN_FALSE");
            kan_trivial_string_buffer_append_string (&io.output_buffer, ",\n");
            kan_trivial_string_buffer_append_string (&io.output_buffer, "        .values_count = ");
            kan_trivial_string_buffer_append_unsigned_long (&io.output_buffer, output_value_index);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "u,\n");
            kan_trivial_string_buffer_append_string (&io.output_buffer, "        .values = reflection_");
            kan_trivial_string_buffer_append_string (&io.output_buffer, enum_data->name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "_values,\n");
            kan_trivial_string_buffer_append_string (&io.output_buffer, "    };\n\n");
        }

        for (uint64_t struct_index = 0u; struct_index < interface->structs_count; ++struct_index)
        {
            const struct kan_c_struct_t *struct_data = &interface->structs[struct_index];
            if (is_struct_ignored (struct_data))
            {
                continue;
            }

            unsigned long output_field_index = 0u;
            for (uint64_t field_index = 0u; field_index < struct_data->fields_count; ++field_index)
            {
                const struct kan_c_variable_t *field_data = &struct_data->fields[field_index];
                if (is_struct_field_ignored (field_data))
                {
                    continue;
                }

                kan_trivial_string_buffer_append_string (&io.output_buffer, "    reflection_");
                kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
                kan_trivial_string_buffer_append_string (&io.output_buffer, "_fields[");
                kan_trivial_string_buffer_append_unsigned_long (&io.output_buffer, output_field_index);
                kan_trivial_string_buffer_append_string (&io.output_buffer, "u] = (struct kan_reflection_field_t) {\n");
                kan_trivial_string_buffer_append_string (&io.output_buffer, "        .name = kan_string_intern (\"");
                kan_trivial_string_buffer_append_string (&io.output_buffer, field_data->name);
                kan_trivial_string_buffer_append_string (&io.output_buffer, "\"),\n");
                kan_trivial_string_buffer_append_string (&io.output_buffer, "        .offset = offsetof (struct ");
                kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
                kan_trivial_string_buffer_append_string (&io.output_buffer, ", ");
                kan_trivial_string_buffer_append_string (&io.output_buffer, field_data->name);
                kan_trivial_string_buffer_append_string (&io.output_buffer, "),\n");
                kan_trivial_string_buffer_append_string (&io.output_buffer, "        .size = SIZE_OF_FIELD (");
                kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
                kan_trivial_string_buffer_append_string (&io.output_buffer, ", ");
                kan_trivial_string_buffer_append_string (&io.output_buffer, field_data->name);
                kan_trivial_string_buffer_append_string (&io.output_buffer, "),\n");

                if (field_data->type.is_array)
                {
                    kan_trivial_string_buffer_append_string (
                        &io.output_buffer, "        .archetype = KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY,\n");
                    kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                             "        .archetype_inline_array = {\n");
                    add_array_bootstrap_common_internals (field_data->type.archetype, field_data->type.name,
                                                          field_data->type.pointer_level, field_data);
                    kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                             "            .item_count = SIZE_OF_FIELD (");
                    kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
                    kan_trivial_string_buffer_append_string (&io.output_buffer, ", ");
                    kan_trivial_string_buffer_append_string (&io.output_buffer, field_data->name);
                    kan_trivial_string_buffer_append_string (&io.output_buffer, ") / sizeof (");

                    if (field_data->type.pointer_level > 0u)
                    {
                        kan_trivial_string_buffer_append_string (&io.output_buffer, "void *");
                    }
                    else
                    {
                        switch (field_data->type.archetype)
                        {
                        case KAN_C_ARCHETYPE_BASIC:
                            break;

                        case KAN_C_ARCHETYPE_ENUM:
                            kan_trivial_string_buffer_append_string (&io.output_buffer, "enum ");
                            break;

                        case KAN_C_ARCHETYPE_STRUCT:
                            kan_trivial_string_buffer_append_string (&io.output_buffer, "struct ");
                            break;
                        }

                        kan_trivial_string_buffer_append_string (&io.output_buffer, field_data->type.name);
                    }

                    kan_trivial_string_buffer_append_string (&io.output_buffer, "),\n");

#define NO_SIZE_FIELD ~0u
                    uint64_t size_field_index = NO_SIZE_FIELD;

                    for (uint64_t meta_index = 0u; meta_index < field_data->meta.meta_count; ++meta_index)
                    {
                        if (field_data->meta.meta_array[meta_index].name == interned.reflection_size_field &&
                            field_data->meta.meta_array[meta_index].type == KAN_C_META_STRING)
                        {
                            uint64_t index_with_ignored_skipped = 0u;
                            for (uint64_t search_field_index = 0u; search_field_index < struct_data->fields_count;
                                 ++search_field_index)
                            {
                                if (!is_struct_field_ignored (&struct_data->fields[search_field_index]))
                                {
                                    if (struct_data->fields[search_field_index].name ==
                                        field_data->meta.meta_array[meta_index].string_value)
                                    {
                                        size_field_index = index_with_ignored_skipped;
                                        break;
                                    }

                                    ++index_with_ignored_skipped;
                                }
                            }

                            if (size_field_index == NO_SIZE_FIELD)
                            {
                                fprintf (stderr, "Unable to find size field %s for %s::%s.\n",
                                         field_data->meta.meta_array[meta_index].string_value, struct_data->name,
                                         field_data->name);
                                current_error_code = RETURN_CODE_UNKNOWN_SIZE_FIELD;
                            }

                            break;
                        }
                    }

                    if (size_field_index == NO_SIZE_FIELD)
                    {
                        kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                                 "            .size_field = NULL,\n");
                    }
                    else
                    {
                        kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                                 "            .size_field = reflection_");
                        kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
                        kan_trivial_string_buffer_append_string (&io.output_buffer, "_fields + ");
                        kan_trivial_string_buffer_append_unsigned_long (&io.output_buffer,
                                                                        (unsigned long) size_field_index);
                        kan_trivial_string_buffer_append_string (&io.output_buffer, "u,\n");
                    }

#undef NO_SIZE_FIELD
                    kan_trivial_string_buffer_append_string (&io.output_buffer, "        },\n");
                }
                else if (field_data->type.pointer_level > 0)
                {
                    const kan_bool_t forced_external_pointer = is_forced_external_pointer (field_data);
                    switch (field_data->type.archetype)
                    {
                    case KAN_C_ARCHETYPE_BASIC:
                        if (!forced_external_pointer && field_data->type.name == interned.type_char)
                        {
                            kan_trivial_string_buffer_append_string (
                                &io.output_buffer, "        .archetype = KAN_REFLECTION_ARCHETYPE_STRING_POINTER,\n");
                        }
                        else
                        {
                            kan_trivial_string_buffer_append_string (
                                &io.output_buffer, "        .archetype = KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER,\n");
                        }

                        break;

                    case KAN_C_ARCHETYPE_ENUM:
                        // Currently pointers to enums are processed as externals.
                        kan_trivial_string_buffer_append_string (
                            &io.output_buffer, "        .archetype = KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER,\n");
                        break;

                    case KAN_C_ARCHETYPE_STRUCT:
                        if (forced_external_pointer)
                        {
                            kan_trivial_string_buffer_append_string (
                                &io.output_buffer, "        .archetype = KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER,\n");
                        }
                        else
                        {
                            kan_trivial_string_buffer_append_string (
                                &io.output_buffer, "        .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER,\n");
                            kan_trivial_string_buffer_append_string (
                                &io.output_buffer,
                                "        .archetype_struct_pointer = {.type_name = kan_string_intern (\"");
                            kan_trivial_string_buffer_append_string (&io.output_buffer, field_data->type.name);
                            kan_trivial_string_buffer_append_string (&io.output_buffer, "\")},\n");
                        }

                        break;
                    }
                }
                else
                {
                    switch (field_data->type.archetype)
                    {
                    case KAN_C_ARCHETYPE_BASIC:
                        if (field_data->type.name == interned.type_interned_string)
                        {
                            kan_trivial_string_buffer_append_string (
                                &io.output_buffer, "        .archetype = KAN_REFLECTION_ARCHETYPE_INTERNED_STRING,\n");
                        }
                        else if (field_data->type.name == interned.type_patch)
                        {
                            kan_trivial_string_buffer_append_string (
                                &io.output_buffer, "        .archetype = KAN_REFLECTION_ARCHETYPE_PATCH,\n");
                        }
                        else
                        {
                            kan_trivial_string_buffer_append_string (
                                &io.output_buffer, "        .archetype = ARCHETYPE_SELECTION_HELPER (");
                            kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
                            kan_trivial_string_buffer_append_string (&io.output_buffer, ", ");
                            kan_trivial_string_buffer_append_string (&io.output_buffer, field_data->name);
                            kan_trivial_string_buffer_append_string (&io.output_buffer, "),\n");
                        }

                        break;

                    case KAN_C_ARCHETYPE_ENUM:
                        kan_trivial_string_buffer_append_string (
                            &io.output_buffer, "        .archetype = KAN_REFLECTION_ARCHETYPE_ENUM,\n");
                        kan_trivial_string_buffer_append_string (
                            &io.output_buffer, "        .archetype_enum = {.type_name = kan_string_intern (\"");
                        kan_trivial_string_buffer_append_string (&io.output_buffer, field_data->type.name);
                        kan_trivial_string_buffer_append_string (&io.output_buffer, "\")},\n");
                        break;

                    case KAN_C_ARCHETYPE_STRUCT:
                        if (field_data->type.name == interned.type_dynamic_array)
                        {
                            kan_trivial_string_buffer_append_string (
                                &io.output_buffer, "        .archetype = KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY,\n");
                            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                                     "        .archetype_dynamic_array = {\n");

                            enum kan_c_archetype_t dynamic_array_item_archetype = KAN_C_ARCHETYPE_BASIC;
                            kan_interned_string_t dynamic_array_item_type_name = NULL;
                            uint8_t dynamic_array_item_pointer_level = 0u;

                            for (uint64_t meta_index = 0u; meta_index < field_data->meta.meta_count; ++meta_index)
                            {
                                if (field_data->meta.meta_array[meta_index].name ==
                                        interned.reflection_dynamic_array_type &&
                                    field_data->meta.meta_array[meta_index].type == KAN_C_META_STRING)
                                {
                                    const char *string_pointer =
                                        (char *) field_data->meta.meta_array[meta_index].string_value;

                                    while (*string_pointer == ' ')
                                    {
                                        ++string_pointer;
                                    }

                                    if (strncmp (string_pointer, "enum ", 5u) == 0)
                                    {
                                        dynamic_array_item_archetype = KAN_C_ARCHETYPE_ENUM;
                                        string_pointer += 5u;
                                    }
                                    else if (strncmp (string_pointer, "struct ", 7u) == 0)
                                    {
                                        dynamic_array_item_archetype = KAN_C_ARCHETYPE_STRUCT;
                                        string_pointer += 7u;
                                    }

                                    while (*string_pointer == ' ')
                                    {
                                        ++string_pointer;
                                    }

                                    const char *type_begin = string_pointer;
                                    while (*string_pointer && *string_pointer != ' ' && *string_pointer != '*')
                                    {
                                        ++string_pointer;
                                    }

                                    const char *type_end = string_pointer;
                                    dynamic_array_item_type_name = kan_char_sequence_intern (type_begin, type_end);

                                    while (*string_pointer)
                                    {
                                        if (*string_pointer == '*')
                                        {
                                            ++dynamic_array_item_pointer_level;
                                        }

                                        ++string_pointer;
                                    }

                                    break;
                                }
                            }

                            if (dynamic_array_item_type_name != NULL)
                            {
                                add_array_bootstrap_common_internals (dynamic_array_item_archetype,
                                                                      dynamic_array_item_type_name,
                                                                      dynamic_array_item_pointer_level, field_data);
                            }
                            else
                            {
                                fprintf (stderr, "No dynamic array meta for %s::%s.\n", struct_data->name,
                                         field_data->name);
                                current_error_code = RETURN_CODE_NOT_ENOUGH_DATA_FOR_DYNAMIC_ARRAY;
                            }

                            kan_trivial_string_buffer_append_string (&io.output_buffer, "        },\n");
                        }
                        else
                        {
                            kan_trivial_string_buffer_append_string (
                                &io.output_buffer, "        .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT,\n");
                            kan_trivial_string_buffer_append_string (
                                &io.output_buffer, "        .archetype_struct = {.type_name = kan_string_intern (\"");
                            kan_trivial_string_buffer_append_string (&io.output_buffer, field_data->type.name);
                            kan_trivial_string_buffer_append_string (&io.output_buffer, "\")},\n");
                        }

                        break;
                    }
                }

#define NO_VISIBILITY_FIELD ~0u
                uint64_t visibility_condition_field_index = NO_VISIBILITY_FIELD;
                for (uint64_t meta_index = 0u; meta_index < field_data->meta.meta_count; ++meta_index)
                {
                    if (field_data->meta.meta_array[meta_index].name ==
                            interned.reflection_visibility_condition_field &&
                        field_data->meta.meta_array[meta_index].type == KAN_C_META_STRING)
                    {
                        uint64_t index_with_ignored_skipped = 0u;
                        for (uint64_t search_field_index = 0u; search_field_index < struct_data->fields_count;
                             ++search_field_index)
                        {
                            if (!is_struct_field_ignored (&struct_data->fields[search_field_index]))
                            {
                                if (struct_data->fields[search_field_index].name ==
                                    field_data->meta.meta_array[meta_index].string_value)
                                {
                                    visibility_condition_field_index = index_with_ignored_skipped;
                                    break;
                                }

                                ++index_with_ignored_skipped;
                            }
                        }

                        if (visibility_condition_field_index == NO_VISIBILITY_FIELD)
                        {
                            fprintf (stderr, "Unable to find visibility field %s for %s::%s.\n",
                                     field_data->meta.meta_array[meta_index].string_value, struct_data->name,
                                     field_data->name);
                            current_error_code = RETURN_CODE_UNKNOWN_VISIBILITY_FIELD;
                        }
                    }
                }

                if (visibility_condition_field_index == NO_VISIBILITY_FIELD)
                {
                    kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                             "        .visibility_condition_field = NULL,\n");
                    kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                             "        .visibility_condition_values_count = 0u,\n");
                    kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                             "        .visibility_condition_values = NULL,\n");
                }
                else
                {
                    kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                             "        .visibility_condition_field = reflection_");
                    kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
                    kan_trivial_string_buffer_append_string (&io.output_buffer, "_fields + ");
                    kan_trivial_string_buffer_append_unsigned_long (&io.output_buffer,
                                                                    (unsigned long) visibility_condition_field_index);
                    kan_trivial_string_buffer_append_string (&io.output_buffer, "u,\n");

                    kan_trivial_string_buffer_append_string (
                        &io.output_buffer, "        .visibility_condition_values_count = sizeof (reflection_");
                    kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
                    kan_trivial_string_buffer_append_string (&io.output_buffer, "_field_");
                    kan_trivial_string_buffer_append_string (&io.output_buffer, field_data->name);
                    kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                             "_visibility_values) / sizeof (int64_t),\n");

                    kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                             "        .visibility_condition_values = reflection_");
                    kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
                    kan_trivial_string_buffer_append_string (&io.output_buffer, "_field_");
                    kan_trivial_string_buffer_append_string (&io.output_buffer, field_data->name);
                    kan_trivial_string_buffer_append_string (&io.output_buffer, "_visibility_values,\n");
                }
#undef NO_VISIBILITY_FIELD

                kan_trivial_string_buffer_append_string (&io.output_buffer, "    };\n\n");
                ++output_field_index;
            }

            kan_trivial_string_buffer_append_string (&io.output_buffer, "    reflection_");
            kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "_data = (struct kan_reflection_struct_t) {\n");

            kan_trivial_string_buffer_append_string (&io.output_buffer, "        .name = kan_string_intern (\"");
            kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "\"),\n");

            kan_trivial_string_buffer_append_string (&io.output_buffer, "        .size = sizeof (struct ");
            kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "),\n");

            kan_trivial_string_buffer_append_string (&io.output_buffer, "        .alignment = _Alignof (struct ");
            kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "),\n");

            uint64_t struct_data_name_length = strlen (struct_data->name);
            if (struct_data_name_length > 2u && struct_data->name[struct_data_name_length - 2u] == '_' &&
                struct_data->name[struct_data_name_length - 1u] == 't')
            {
                struct_data_name_length -= 2u;
            }

            kan_trivial_string_buffer_append_string (&io.output_buffer, "        .init = ");
            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     get_registered_init_functor_name_or_null_name (struct_data));
            kan_trivial_string_buffer_append_string (&io.output_buffer, ",\n");

            kan_trivial_string_buffer_append_string (&io.output_buffer, "        .shutdown = ");
            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     get_registered_shutdown_functor_name_or_null_name (struct_data));
            kan_trivial_string_buffer_append_string (&io.output_buffer, ",\n");

            kan_trivial_string_buffer_append_string (&io.output_buffer, "        .functor_user_data = 0u,\n");
            kan_trivial_string_buffer_append_string (&io.output_buffer, "        .fields_count = ");
            kan_trivial_string_buffer_append_unsigned_long (&io.output_buffer, output_field_index);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "u,\n");

            kan_trivial_string_buffer_append_string (&io.output_buffer, "        .fields = reflection_");
            kan_trivial_string_buffer_append_string (&io.output_buffer, struct_data->name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "_fields,\n");

            kan_trivial_string_buffer_append_string (&io.output_buffer, "    };\n\n");
        }

        kan_trivial_string_buffer_append_string (&io.output_buffer, "    // End of data from \"");
        kan_trivial_string_buffer_append_string (&io.output_buffer, io.input_files[file_index].source_file_path);
        kan_trivial_string_buffer_append_string (&io.output_buffer, "\"\n\n");
    }

    kan_trivial_string_buffer_append_string (&io.output_buffer,
                                             "    bootstrap_done = KAN_TRUE;\n"
                                             "}\n\n");
}

void add_registrar (void)
{
    kan_trivial_string_buffer_append_string (&io.output_buffer,
                                             "// Section: registration function.\n\n"
                                             "EXPORT_THIS void KAN_REFLECTION_UNIT_REGISTRAR_NAME (");
    kan_trivial_string_buffer_append_string (&io.output_buffer, arguments.module_name);
    kan_trivial_string_buffer_append_string (&io.output_buffer,
                                             ") (kan_reflection_registry_t registry)\n"
                                             "{\n"
                                             "    ensure_reflection_is_ready ();\n\n");

    for (uint64_t file_index = 0u; file_index < arguments.input_files_count; ++file_index)
    {
        const struct kan_c_interface_t *interface = io.input_files[file_index].interface;

        kan_trivial_string_buffer_append_string (&io.output_buffer, "    // Data from \"");
        kan_trivial_string_buffer_append_string (&io.output_buffer, io.input_files[file_index].source_file_path);
        kan_trivial_string_buffer_append_string (&io.output_buffer, "\"\n");

        for (uint64_t enum_index = 0u; enum_index < interface->enums_count; ++enum_index)
        {
            if (is_enum_ignored (&interface->enums[enum_index]))
            {
                continue;
            }

            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     "    kan_reflection_registry_add_enum (registry, &reflection_");
            kan_trivial_string_buffer_append_string (&io.output_buffer, interface->enums[enum_index].name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "_data);\n");
        }

        for (uint64_t struct_index = 0u; struct_index < interface->structs_count; ++struct_index)
        {
            if (is_struct_ignored (&interface->structs[struct_index]))
            {
                continue;
            }

            kan_trivial_string_buffer_append_string (&io.output_buffer,
                                                     "    kan_reflection_registry_add_struct (registry, &reflection_");
            kan_trivial_string_buffer_append_string (&io.output_buffer, interface->structs[struct_index].name);
            kan_trivial_string_buffer_append_string (&io.output_buffer, "_data);\n");
        }

        for (uint64_t symbol_index = 0u; symbol_index < interface->symbols_count; ++symbol_index)
        {
            const struct kan_c_variable_t *symbol_data = &interface->symbols[symbol_index];
            for (uint64_t meta_index = 0u; meta_index < symbol_data->meta.meta_count; ++meta_index)
            {
                const struct kan_c_meta_t *meta = &symbol_data->meta.meta_array[meta_index];
                if (meta->type == KAN_C_META_STRING)
                {
                    if (meta->name == interned.reflection_enum_meta)
                    {
                        kan_trivial_string_buffer_append_string (
                            &io.output_buffer,
                            "    kan_reflection_registry_add_enum_meta (registry, kan_string_intern (\"");
                        kan_trivial_string_buffer_append_string (&io.output_buffer, meta->string_value);
                        kan_trivial_string_buffer_append_string (&io.output_buffer, "\"), kan_string_intern (\"");
                        kan_trivial_string_buffer_append_string (&io.output_buffer, symbol_data->type.name);
                        kan_trivial_string_buffer_append_string (&io.output_buffer, "\"), &");
                        kan_trivial_string_buffer_append_string (&io.output_buffer, symbol_data->name);
                        kan_trivial_string_buffer_append_string (&io.output_buffer, ");\n");
                        break;
                    }
                    else if (meta->name == interned.reflection_enum_value_meta)
                    {
                        char *separator = strchr (meta->string_value, '.');
                        if (separator == NULL || separator[1u] == '\0')
                        {
                            fprintf (
                                stderr,
                                "Unable to find type-value separator '.' in enum value meta specifier \"%s\" on symbol "
                                "\"%s\".\n",
                                meta->string_value, symbol_data->name);
                            current_error_code = RETURN_CODE_INVALID_PARSABLE_META_TARGET;
                        }
                        else
                        {
                            kan_trivial_string_buffer_append_string (
                                &io.output_buffer,
                                "    kan_reflection_registry_add_enum_value_meta (registry, kan_string_intern (\"");
                            kan_trivial_string_buffer_append_char_sequence (&io.output_buffer, meta->string_value,
                                                                            separator - meta->string_value);
                            kan_trivial_string_buffer_append_string (&io.output_buffer, "\"), kan_string_intern (\"");
                            kan_trivial_string_buffer_append_string (&io.output_buffer, separator + 1u);
                            kan_trivial_string_buffer_append_string (&io.output_buffer, "\"), kan_string_intern (\"");
                            kan_trivial_string_buffer_append_string (&io.output_buffer, symbol_data->type.name);
                            kan_trivial_string_buffer_append_string (&io.output_buffer, "\"), &");
                            kan_trivial_string_buffer_append_string (&io.output_buffer, symbol_data->name);
                            kan_trivial_string_buffer_append_string (&io.output_buffer, ");\n");
                        }

                        break;
                    }
                    else if (meta->name == interned.reflection_struct_meta)
                    {
                        kan_trivial_string_buffer_append_string (
                            &io.output_buffer,
                            "    kan_reflection_registry_add_struct_meta (registry, kan_string_intern (\"");
                        kan_trivial_string_buffer_append_string (&io.output_buffer, meta->string_value);
                        kan_trivial_string_buffer_append_string (&io.output_buffer, "\"), kan_string_intern (\"");
                        kan_trivial_string_buffer_append_string (&io.output_buffer, symbol_data->type.name);
                        kan_trivial_string_buffer_append_string (&io.output_buffer, "\"), &");
                        kan_trivial_string_buffer_append_string (&io.output_buffer, symbol_data->name);
                        kan_trivial_string_buffer_append_string (&io.output_buffer, ");\n");
                        break;
                    }
                    else if (meta->name == interned.reflection_struct_field_meta)
                    {
                        char *separator = strchr (meta->string_value, '.');
                        if (separator == NULL || separator[1u] == '\0')
                        {
                            fprintf (stderr,
                                     "Unable to find type-value separator '.' in struct field meta specifier \"%s\" on "
                                     "symbol "
                                     "\"%s\".\n",
                                     meta->string_value, symbol_data->name);
                            current_error_code = RETURN_CODE_INVALID_PARSABLE_META_TARGET;
                        }
                        else
                        {
                            kan_trivial_string_buffer_append_string (
                                &io.output_buffer,
                                "    kan_reflection_registry_add_struct_field_meta (registry, kan_string_intern (\"");
                            kan_trivial_string_buffer_append_char_sequence (&io.output_buffer, meta->string_value,
                                                                            separator - meta->string_value);
                            kan_trivial_string_buffer_append_string (&io.output_buffer, "\"), kan_string_intern (\"");
                            kan_trivial_string_buffer_append_string (&io.output_buffer, separator + 1u);
                            kan_trivial_string_buffer_append_string (&io.output_buffer, "\"), kan_string_intern (\"");
                            kan_trivial_string_buffer_append_string (&io.output_buffer, symbol_data->type.name);
                            kan_trivial_string_buffer_append_string (&io.output_buffer, "\"), &");
                            kan_trivial_string_buffer_append_string (&io.output_buffer, symbol_data->name);
                            kan_trivial_string_buffer_append_string (&io.output_buffer, ");\n");
                        }

                        break;
                    }
                }
            }
        }

        kan_trivial_string_buffer_append_string (&io.output_buffer, "    // End of data from \"");
        kan_trivial_string_buffer_append_string (&io.output_buffer, io.input_files[file_index].source_file_path);
        kan_trivial_string_buffer_append_string (&io.output_buffer, "\"\n\n");
    }

    kan_trivial_string_buffer_append_string (&io.output_buffer, "}\n\n");
}

int main (int argument_count, char **arguments_array)
{
    if (argument_count < 4)
    {
        fprintf (stderr,
                 "Unknown arguments. Expected arguments:\n"
                 "- module_name: name of the module to correctly generate export functions.\n"
                 "- output_file_path: path to file to write generated reflection code.\n"
                 "- all other arguments: paths to input c interface files.\n");
        return RETURN_CODE_INVALID_ARGUMENTS;
    }

    arguments.module_name = arguments_array[1u];
    arguments.output_file_path = arguments_array[2u];
    arguments.input_files_count = argument_count - 3u;
    arguments.input_files = arguments_array + 3u;

    interned.reflection_flags = kan_string_intern ("reflection_flags");
    interned.reflection_ignore_enum = kan_string_intern ("reflection_ignore_enum");
    interned.reflection_ignore_enum_value = kan_string_intern ("reflection_ignore_enum_value");
    interned.reflection_ignore_struct = kan_string_intern ("reflection_ignore_struct");
    interned.reflection_ignore_struct_field = kan_string_intern ("reflection_ignore_struct_field");
    interned.reflection_external_pointer = kan_string_intern ("reflection_external_pointer");
    interned.reflection_dynamic_array_type = kan_string_intern ("reflection_dynamic_array_type");
    interned.reflection_size_field = kan_string_intern ("reflection_size_field");
    interned.reflection_visibility_condition_field = kan_string_intern ("reflection_visibility_condition_field");
    interned.reflection_visibility_condition_values = kan_string_intern ("reflection_visibility_condition_values");
    interned.reflection_enum_meta = kan_string_intern ("reflection_enum_meta");
    interned.reflection_enum_value_meta = kan_string_intern ("reflection_enum_value_meta");
    interned.reflection_struct_meta = kan_string_intern ("reflection_struct_meta");
    interned.reflection_struct_field_meta = kan_string_intern ("reflection_struct_field_meta");

    interned.type_char = kan_string_intern ("char");
    interned.type_interned_string = kan_string_intern ("kan_interned_string_t");
    interned.type_dynamic_array = kan_string_intern ("kan_dynamic_array_t");
    interned.type_patch = kan_string_intern ("kan_reflection_patch_t");

    kan_hash_storage_init (&functor_registry, KAN_ALLOCATION_GROUP_IGNORE,
                           KAN_REFLECTION_GENERATOR_FUNCTORS_INITIAL_BUCKETS);

    io.input_files = kan_allocate_general (KAN_ALLOCATION_GROUP_IGNORE,
                                           sizeof (struct kan_c_interface_file_t) * arguments.input_files_count,
                                           _Alignof (struct kan_c_interface_file_t));

    for (uint64_t index = 0u; index < arguments.input_files_count; ++index)
    {
        kan_c_interface_file_init (&io.input_files[index]);
    }

    for (uint64_t index = 0u; index < arguments.input_files_count; ++index)
    {
        struct kan_stream_t *input_stream =
            kan_direct_file_stream_open_for_read (arguments.input_files[index], KAN_TRUE);

        if (!input_stream)
        {
            fprintf (stderr, "Unable to open input file \"%s\".\n", arguments.input_files[index]);
            shutdown ();
            return RETURN_CODE_INPUT_IO_ERROR;
        }

        if (!kan_c_interface_file_deserialize (&io.input_files[index], input_stream))
        {
            fprintf (stderr, "Unable to deserialize input file \"%s\".\n", arguments.input_files[index]);
            shutdown ();
            return RETURN_CODE_INPUT_IO_ERROR;
        }

        kan_direct_file_stream_close (input_stream);
    }

    kan_trivial_string_buffer_init (&io.output_buffer, KAN_ALLOCATION_GROUP_IGNORE, OUTPUT_BUFFER_INITIAL_CAPACITY);
    add_header ();
    add_includes ();
    add_variables ();
    add_functors ();

    add_bootstrap ();
    if (current_error_code != RETURN_CODE_SUCCESS)
    {
        shutdown ();
        return current_error_code;
    }

    add_registrar ();
    if (current_error_code != RETURN_CODE_SUCCESS)
    {
        shutdown ();
        return current_error_code;
    }

    struct kan_stream_t *output_stream = kan_direct_file_stream_open_for_write (arguments.output_file_path, KAN_TRUE);
    if (!output_stream)
    {
        fprintf (stderr, "Unable to open output file \"%s\".\n", arguments.output_file_path);
        return RETURN_CODE_UNABLE_TO_OPEN_OUTPUT;
    }

    if (output_stream->operations->write (output_stream, io.output_buffer.size, io.output_buffer.buffer) !=
        io.output_buffer.size)
    {
        fprintf (stderr, "Output failed, exiting...\n");
        kan_direct_file_stream_close (output_stream);
        shutdown ();
        return RETURN_CODE_IO_OUTPUT_FAILED;
    }

    kan_direct_file_stream_close (output_stream);
    shutdown ();
    return RETURN_CODE_SUCCESS;
}
