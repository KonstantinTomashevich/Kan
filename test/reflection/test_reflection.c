#include <memory.h>
#include <stddef.h>
#include <string.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/container/dynamic_array.h>
#include <kan/reflection/field_visibility_iterator.h>
#include <kan/reflection/generated_reflection.h>
#include <kan/reflection/migration.h>
#include <kan/reflection/patch.h>
#include <kan/reflection/registry.h>
#include <kan/testing/testing.h>

#include <generator_test_first.h>
#include <generator_test_second.h>

struct example_struct_meta_assembly_t
{
    kan_bool_t can_be_used_for_assembly;
};

struct example_field_meta_min_max_t
{
    int64_t min_value;
    int64_t max_value;
};

struct example_enum_meta_serialization_t
{
    uint8_t compress_to_bytes;
};

struct example_universal_meta_editor_t
{
    const char *display_name_key;
    const char *description_key;
    kan_bool_t hidden;
};

struct example_function_meta_editor_action_t
{
    const char *category;
    const char *display_name_key;
    const char *description_key;
};

struct example_argument_meta_min_max_t
{
    int64_t min_value;
    int64_t max_value;
};

static void function_call_functor_stub (kan_functor_user_data_t user_data,
                                        void *return_pointer,
                                        void *arguments_pointer)
{
}

KAN_TEST_CASE (registry)
{
    struct kan_reflection_enum_value_t first_enum_values[] = {
        {kan_string_intern ("F_FIRST"), 0},
        {kan_string_intern ("F_SECOND"), 1},
        {kan_string_intern ("F_THIRD"), 2},
    };

    struct kan_reflection_enum_t first_enum = {
        kan_string_intern ("first_t"),
        KAN_FALSE,
        3,
        first_enum_values,
    };

    struct example_enum_meta_serialization_t first_enum_serialization = {1u};
    struct example_universal_meta_editor_t first_enum_editor = {"first_t", "description", KAN_TRUE};

    struct kan_reflection_enum_value_t second_enum_values[] = {
        {kan_string_intern ("S_FIRST"), 1u << 0u},
        {kan_string_intern ("S_SECOND"), 1u << 1u},
        {kan_string_intern ("S_THIRD"), 1u << 2u},
    };

    struct kan_reflection_enum_t second_enum = {
        kan_string_intern ("second_t"),
        KAN_TRUE,
        3,
        second_enum_values,
    };

    struct example_enum_meta_serialization_t second_enum_serialization = {2u};
    struct example_universal_meta_editor_t second_enum_editor = {"second_t", "description", KAN_FALSE};

    struct kan_reflection_field_t first_struct_fields[] = {
        {
            kan_string_intern ("first"),
            0u,
            sizeof (int32_t),
            KAN_REFLECTION_ARCHETYPE_SIGNED_INT,
            .visibility_condition_field = NULL,
            .visibility_condition_values_count = 0u,
            .visibility_condition_values = NULL,
        },
        {
            kan_string_intern ("second"),
            sizeof (int32_t),
            sizeof (uint32_t),
            KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
            .visibility_condition_field = NULL,
            .visibility_condition_values_count = 0u,
            .visibility_condition_values = NULL,
        },
        {
            kan_string_intern ("third"),
            sizeof (int32_t) + sizeof (uint32_t),
            sizeof (kan_interned_string_t),
            KAN_REFLECTION_ARCHETYPE_INTERNED_STRING,
            .visibility_condition_field = NULL,
            .visibility_condition_values_count = 0u,
            .visibility_condition_values = NULL,
        },
    };

    struct kan_reflection_struct_t first_struct = {
        kan_string_intern ("struct_first_t"),
        16u,
        8u,
        NULL,
        NULL,
        0u,
        sizeof (first_struct_fields) / sizeof (struct kan_reflection_field_t),
        first_struct_fields,
    };

    struct example_struct_meta_assembly_t first_struct_assembly = {KAN_TRUE};
    struct example_universal_meta_editor_t first_struct_editor = {"struct_first_t", "description", KAN_FALSE};

    struct kan_reflection_field_t second_struct_fields[] = {
        {
            kan_string_intern ("first"),
            0u,
            sizeof (int32_t),
            KAN_REFLECTION_ARCHETYPE_SIGNED_INT,
            .visibility_condition_field = NULL,
            .visibility_condition_values_count = 0u,
            .visibility_condition_values = NULL,
        },
        {
            kan_string_intern ("second"),
            sizeof (int32_t),
            sizeof (uint32_t),
            KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
            .visibility_condition_field = NULL,
            .visibility_condition_values_count = 0u,
            .visibility_condition_values = NULL,
        },
        {
            kan_string_intern ("third"),
            sizeof (int32_t) + sizeof (uint32_t),
            sizeof (kan_interned_string_t),
            KAN_REFLECTION_ARCHETYPE_INTERNED_STRING,
            .visibility_condition_field = NULL,
            .visibility_condition_values_count = 0u,
            .visibility_condition_values = NULL,
        },
    };

    struct kan_reflection_struct_t second_struct = {
        kan_string_intern ("struct_second_t"),
        16u,
        8u,
        NULL,
        NULL,
        0u,
        sizeof (second_struct_fields) / sizeof (struct kan_reflection_field_t),
        second_struct_fields,
    };

    struct example_struct_meta_assembly_t second_struct_assembly = {KAN_FALSE};
    struct example_universal_meta_editor_t second_struct_editor = {"struct_second_t", "description", KAN_FALSE};

    struct example_field_meta_min_max_t first_struct_first_min_max = {-10, 10};
    struct example_field_meta_min_max_t second_struct_second_min_max = {0u, 16u};

    struct kan_reflection_argument_t first_function_arguments[] = {
        {
            .name = kan_string_intern ("x"),
            .size = sizeof (int32_t),
            .archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT,
        },
        {
            .name = kan_string_intern ("y"),
            .size = sizeof (uint32_t),
            .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
        },
    };

    struct kan_reflection_function_t first_function = {
        .name = kan_string_intern ("first_function"),
        .call = function_call_functor_stub,
        .call_user_data = 0u,
        .return_type = {.size = sizeof (int32_t), .archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT},
        .arguments_count = sizeof (first_function_arguments) / sizeof (struct kan_reflection_argument_t),
        .arguments = first_function_arguments,
    };

    struct example_argument_meta_min_max_t first_function_x_min_max = {-10, 10};
    struct example_argument_meta_min_max_t first_function_y_min_max = {0, 16};

    struct kan_reflection_argument_t second_function_arguments[] = {
        {
            .name = kan_string_intern ("parameter"),
            .size = sizeof (int32_t),
            .archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT,
        },
    };

    struct kan_reflection_function_t second_function = {
        .name = kan_string_intern ("second_function"),
        .call = function_call_functor_stub,
        .call_user_data = 0u,
        .return_type = {.size = sizeof (int32_t), .archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT},
        .arguments_count = sizeof (second_function_arguments) / sizeof (struct kan_reflection_argument_t),
        .arguments = second_function_arguments,
    };

    struct example_function_meta_editor_action_t second_function_editor_action = {
        .category = "Tools",
        .display_name_key = "Bake",
        .description_key = "Bake",
    };

    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_TEST_CHECK (!kan_reflection_registry_query_enum (registry, first_enum.name))
    KAN_TEST_CHECK (!kan_reflection_registry_query_enum (registry, second_enum.name))
    KAN_TEST_CHECK (!kan_reflection_registry_query_struct (registry, first_struct.name))
    KAN_TEST_CHECK (!kan_reflection_registry_query_struct (registry, second_struct.name))
    KAN_TEST_CHECK (!kan_reflection_registry_query_function (registry, first_function.name))
    KAN_TEST_CHECK (!kan_reflection_registry_query_function (registry, second_function.name))

    kan_reflection_registry_add_enum_meta (registry, kan_string_intern ("first_t"),
                                           kan_string_intern ("example_enum_meta_serialization_t"),
                                           &first_enum_serialization);
    KAN_TEST_CHECK (kan_reflection_registry_add_enum (registry, &first_enum))
    kan_reflection_registry_add_enum_meta (registry, first_enum.name,
                                           kan_string_intern ("example_universal_meta_editor_t"), &first_enum_editor);

    kan_reflection_registry_add_enum_meta (registry, second_enum.name,
                                           kan_string_intern ("example_universal_meta_editor_t"), &second_enum_editor);
    KAN_TEST_CHECK (kan_reflection_registry_add_enum (registry, &second_enum))
    kan_reflection_registry_add_enum_meta (registry, kan_string_intern ("second_t"),
                                           kan_string_intern ("example_enum_meta_serialization_t"),
                                           &second_enum_serialization);

    kan_reflection_registry_add_struct_meta (registry, kan_string_intern ("struct_first_t"),
                                             kan_string_intern ("example_struct_meta_assembly_t"),
                                             &first_struct_assembly);
    kan_reflection_registry_add_struct_field_meta (registry, first_struct.name, first_struct_fields[0u].name,
                                                   kan_string_intern ("example_field_meta_min_max_t"),
                                                   &first_struct_first_min_max);
    KAN_TEST_CHECK (kan_reflection_registry_add_struct (registry, &first_struct))
    kan_reflection_registry_add_struct_meta (
        registry, first_struct.name, kan_string_intern ("example_universal_meta_editor_t"), &first_struct_editor);

    kan_reflection_registry_add_struct_meta (
        registry, second_struct.name, kan_string_intern ("example_universal_meta_editor_t"), &second_struct_editor);
    KAN_TEST_CHECK (kan_reflection_registry_add_struct (registry, &second_struct))
    kan_reflection_registry_add_struct_meta (registry, kan_string_intern ("struct_second_t"),
                                             kan_string_intern ("example_struct_meta_assembly_t"),
                                             &second_struct_assembly);
    kan_reflection_registry_add_struct_field_meta (registry, second_struct.name, second_struct_fields[1u].name,
                                                   kan_string_intern ("example_field_meta_min_max_t"),
                                                   &second_struct_second_min_max);

    kan_reflection_registry_add_function_argument_meta (
        registry, first_function.name, first_function_arguments[0u].name,
        kan_string_intern ("example_argument_meta_min_max_t"), &first_function_x_min_max);
    KAN_TEST_CHECK (kan_reflection_registry_add_function (registry, &first_function))
    kan_reflection_registry_add_function_argument_meta (
        registry, first_function.name, first_function_arguments[1u].name,
        kan_string_intern ("example_argument_meta_min_max_t"), &first_function_y_min_max);

    KAN_TEST_CHECK (kan_reflection_registry_add_function (registry, &second_function))
    kan_reflection_registry_add_function_meta (registry, second_function.name,
                                               kan_string_intern ("example_function_meta_editor_action_t"),
                                               &second_function_editor_action);

    KAN_TEST_CHECK (kan_reflection_registry_query_enum (registry, first_enum.name) == &first_enum)

    struct kan_reflection_enum_meta_iterator_t enum_meta_iterator = kan_reflection_registry_query_enum_meta (
        registry, first_enum.name, kan_string_intern ("example_enum_meta_serialization_t"));
    KAN_TEST_CHECK (kan_reflection_enum_meta_iterator_get (&enum_meta_iterator) == &first_enum_serialization)
    kan_reflection_enum_meta_iterator_next (&enum_meta_iterator);
    KAN_TEST_CHECK (!kan_reflection_enum_meta_iterator_get (&enum_meta_iterator))

    enum_meta_iterator = kan_reflection_registry_query_enum_meta (
        registry, first_enum.name, kan_string_intern ("example_universal_meta_editor_t"));
    KAN_TEST_CHECK (kan_reflection_enum_meta_iterator_get (&enum_meta_iterator) == &first_enum_editor)
    kan_reflection_enum_meta_iterator_next (&enum_meta_iterator);
    KAN_TEST_CHECK (!kan_reflection_enum_meta_iterator_get (&enum_meta_iterator))

    enum_meta_iterator = kan_reflection_registry_query_enum_meta (registry, first_enum.name,
                                                                  kan_string_intern ("there_is_no_such_meta"));
    KAN_TEST_CHECK (!kan_reflection_enum_meta_iterator_get (&enum_meta_iterator))

    KAN_TEST_CHECK (kan_reflection_registry_query_enum (registry, second_enum.name) == &second_enum)

    enum_meta_iterator = kan_reflection_registry_query_enum_meta (
        registry, second_enum.name, kan_string_intern ("example_enum_meta_serialization_t"));
    KAN_TEST_CHECK (kan_reflection_enum_meta_iterator_get (&enum_meta_iterator) == &second_enum_serialization)
    kan_reflection_enum_meta_iterator_next (&enum_meta_iterator);
    KAN_TEST_CHECK (!kan_reflection_enum_meta_iterator_get (&enum_meta_iterator))

    enum_meta_iterator = kan_reflection_registry_query_enum_meta (
        registry, second_enum.name, kan_string_intern ("example_universal_meta_editor_t"));
    KAN_TEST_CHECK (kan_reflection_enum_meta_iterator_get (&enum_meta_iterator) == &second_enum_editor)
    kan_reflection_enum_meta_iterator_next (&enum_meta_iterator);
    KAN_TEST_CHECK (!kan_reflection_enum_meta_iterator_get (&enum_meta_iterator))

    enum_meta_iterator = kan_reflection_registry_query_enum_meta (registry, second_enum.name,
                                                                  kan_string_intern ("there_is_no_such_meta"));
    KAN_TEST_CHECK (!kan_reflection_enum_meta_iterator_get (&enum_meta_iterator))

    struct kan_reflection_enum_value_meta_iterator_t enum_value_meta_iterator =
        kan_reflection_registry_query_enum_value_meta (registry, second_enum.name, second_enum_values[1u].name,
                                                       kan_string_intern ("there_is_no_such_meta"));
    KAN_TEST_CHECK (!kan_reflection_enum_value_meta_iterator_get (&enum_value_meta_iterator))

    KAN_TEST_CHECK (kan_reflection_registry_query_struct (registry, first_struct.name) == &first_struct)

    struct kan_reflection_struct_meta_iterator_t struct_meta_iterator = kan_reflection_registry_query_struct_meta (
        registry, first_struct.name, kan_string_intern ("example_struct_meta_assembly_t"));
    KAN_TEST_CHECK (kan_reflection_struct_meta_iterator_get (&struct_meta_iterator) == &first_struct_assembly)
    kan_reflection_struct_meta_iterator_next (&struct_meta_iterator);
    KAN_TEST_CHECK (!kan_reflection_struct_meta_iterator_get (&struct_meta_iterator))

    struct_meta_iterator = kan_reflection_registry_query_struct_meta (
        registry, first_struct.name, kan_string_intern ("example_universal_meta_editor_t"));
    KAN_TEST_CHECK (kan_reflection_struct_meta_iterator_get (&struct_meta_iterator) == &first_struct_editor)
    kan_reflection_struct_meta_iterator_next (&struct_meta_iterator);
    KAN_TEST_CHECK (!kan_reflection_struct_meta_iterator_get (&struct_meta_iterator))

    struct_meta_iterator = kan_reflection_registry_query_struct_meta (registry, first_struct.name,
                                                                      kan_string_intern ("there_is_no_such_meta"));
    KAN_TEST_CHECK (!kan_reflection_struct_meta_iterator_get (&struct_meta_iterator))

    KAN_TEST_CHECK (kan_reflection_registry_query_struct (registry, second_struct.name) == &second_struct)

    struct_meta_iterator = kan_reflection_registry_query_struct_meta (
        registry, second_struct.name, kan_string_intern ("example_struct_meta_assembly_t"));
    KAN_TEST_CHECK (kan_reflection_struct_meta_iterator_get (&struct_meta_iterator) == &second_struct_assembly)
    kan_reflection_struct_meta_iterator_next (&struct_meta_iterator);
    KAN_TEST_CHECK (!kan_reflection_struct_meta_iterator_get (&struct_meta_iterator))

    struct_meta_iterator = kan_reflection_registry_query_struct_meta (
        registry, second_struct.name, kan_string_intern ("example_universal_meta_editor_t"));
    KAN_TEST_CHECK (kan_reflection_struct_meta_iterator_get (&struct_meta_iterator) == &second_struct_editor)
    kan_reflection_struct_meta_iterator_next (&struct_meta_iterator);
    KAN_TEST_CHECK (!kan_reflection_struct_meta_iterator_get (&struct_meta_iterator))

    struct_meta_iterator = kan_reflection_registry_query_struct_meta (registry, second_struct.name,
                                                                      kan_string_intern ("there_is_no_such_meta"));
    KAN_TEST_CHECK (!kan_reflection_struct_meta_iterator_get (&struct_meta_iterator))

    struct kan_reflection_struct_field_meta_iterator_t struct_field_meta_iterator =
        kan_reflection_registry_query_struct_field_meta (registry, first_struct.name, first_struct_fields[0u].name,
                                                         kan_string_intern ("example_field_meta_min_max_t"));
    KAN_TEST_CHECK (kan_reflection_struct_field_meta_iterator_get (&struct_field_meta_iterator) ==
                    &first_struct_first_min_max)
    kan_reflection_struct_field_meta_iterator_next (&struct_field_meta_iterator);
    KAN_TEST_CHECK (!kan_reflection_struct_field_meta_iterator_get (&struct_field_meta_iterator))

    struct_field_meta_iterator = kan_reflection_registry_query_struct_field_meta (
        registry, first_struct.name, first_struct_fields[1u].name, kan_string_intern ("example_field_meta_min_max_t"));
    KAN_TEST_CHECK (!kan_reflection_struct_field_meta_iterator_get (&struct_field_meta_iterator))

    struct_field_meta_iterator =
        kan_reflection_registry_query_struct_field_meta (registry, second_struct.name, second_struct_fields[0u].name,
                                                         kan_string_intern ("example_field_meta_min_max_t"));
    KAN_TEST_CHECK (!kan_reflection_struct_field_meta_iterator_get (&struct_field_meta_iterator))

    struct_field_meta_iterator =
        kan_reflection_registry_query_struct_field_meta (registry, second_struct.name, second_struct_fields[1u].name,
                                                         kan_string_intern ("example_field_meta_min_max_t"));
    KAN_TEST_CHECK (kan_reflection_struct_field_meta_iterator_get (&struct_field_meta_iterator) ==
                    &second_struct_second_min_max)
    kan_reflection_struct_field_meta_iterator_next (&struct_field_meta_iterator);
    KAN_TEST_CHECK (!kan_reflection_struct_field_meta_iterator_get (&struct_field_meta_iterator))

    KAN_TEST_CHECK (kan_reflection_registry_query_function (registry, first_function.name) == &first_function)
    KAN_TEST_CHECK (kan_reflection_registry_query_function (registry, second_function.name) == &second_function)
    KAN_TEST_CHECK (!kan_reflection_registry_query_function (registry, kan_string_intern ("unknown")))

    struct kan_reflection_function_meta_iterator_t function_meta_iterator =
        kan_reflection_registry_query_function_meta (registry, first_function.name,
                                                     kan_string_intern ("example_function_meta_editor_action_t"));
    KAN_TEST_CHECK (!kan_reflection_function_meta_iterator_get (&function_meta_iterator))

    function_meta_iterator = kan_reflection_registry_query_function_meta (
        registry, second_function.name, kan_string_intern ("example_function_meta_editor_action_t"));
    KAN_TEST_CHECK (kan_reflection_function_meta_iterator_get (&function_meta_iterator) ==
                    &second_function_editor_action)
    kan_reflection_function_meta_iterator_next (&function_meta_iterator);
    KAN_TEST_CHECK (!kan_reflection_function_meta_iterator_get (&function_meta_iterator))

    struct kan_reflection_function_argument_meta_iterator_t argument_meta_iterator =
        kan_reflection_registry_query_function_argument_meta (registry, first_function.name,
                                                              first_function_arguments[0u].name,
                                                              kan_string_intern ("example_argument_meta_min_max_t"));
    KAN_TEST_CHECK (kan_reflection_function_argument_meta_iterator_get (&argument_meta_iterator) ==
                    &first_function_x_min_max)
    kan_reflection_function_argument_meta_iterator_next (&argument_meta_iterator);
    KAN_TEST_CHECK (!kan_reflection_function_argument_meta_iterator_get (&argument_meta_iterator))

    argument_meta_iterator = kan_reflection_registry_query_function_argument_meta (
        registry, first_function.name, first_function_arguments[1u].name,
        kan_string_intern ("example_argument_meta_min_max_t"));
    KAN_TEST_CHECK (kan_reflection_function_argument_meta_iterator_get (&argument_meta_iterator) ==
                    &first_function_y_min_max)
    kan_reflection_function_argument_meta_iterator_next (&argument_meta_iterator);
    KAN_TEST_CHECK (!kan_reflection_function_argument_meta_iterator_get (&argument_meta_iterator))

    kan_reflection_registry_destroy (registry);
}

KAN_TEST_CASE (query_local_field)
{
    struct kan_reflection_field_t first_struct_fields[] = {
        {
            kan_string_intern ("first"),
            0u,
            sizeof (int32_t),
            KAN_REFLECTION_ARCHETYPE_SIGNED_INT,
            .visibility_condition_field = NULL,
            .visibility_condition_values_count = 0u,
            .visibility_condition_values = NULL,
        },
        {
            kan_string_intern ("second"),
            sizeof (int32_t),
            sizeof (uint32_t),
            KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
            .visibility_condition_field = NULL,
            .visibility_condition_values_count = 0u,
            .visibility_condition_values = NULL,
        },
        {
            kan_string_intern ("third"),
            sizeof (int32_t) + sizeof (uint32_t),
            sizeof (kan_interned_string_t),
            KAN_REFLECTION_ARCHETYPE_INTERNED_STRING,
            .visibility_condition_field = NULL,
            .visibility_condition_values_count = 0u,
            .visibility_condition_values = NULL,
        },
    };

    struct kan_reflection_struct_t first_struct = {
        kan_string_intern ("first_t"),
        16u,
        8u,
        NULL,
        NULL,
        0u,
        sizeof (first_struct_fields) / sizeof (struct kan_reflection_field_t),
        first_struct_fields,
    };

    struct kan_reflection_field_t second_struct_fields[] = {
        {
            .name = kan_string_intern ("first"),
            .offset = 0u,
            .size = sizeof (void *),
            .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER,
            .archetype_struct_pointer = {first_struct.name},
            .visibility_condition_field = NULL,
            .visibility_condition_values_count = 0u,
            .visibility_condition_values = NULL,
        },
        {
            .name = kan_string_intern ("second"),
            .offset = kan_apply_alignment (sizeof (void *), first_struct.alignment),
            .size = first_struct.size,
            .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT,
            .archetype_struct = {first_struct.name},
            .visibility_condition_field = NULL,
            .visibility_condition_values_count = 0u,
            .visibility_condition_values = NULL,
        },
    };

    struct kan_reflection_struct_t second_struct = {
        kan_string_intern ("second_t"),
        24u,
        8u,
        NULL,
        NULL,
        0u,
        sizeof (second_struct_fields) / sizeof (struct kan_reflection_field_t),
        second_struct_fields,
    };

    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_TEST_CHECK (kan_reflection_registry_add_struct (registry, &first_struct))
    KAN_TEST_CHECK (kan_reflection_registry_add_struct (registry, &second_struct))

    kan_instance_size_t absolute_offset;
    kan_instance_size_t size_with_padding;
    kan_interned_string_t first_first_path[] = {first_struct_fields[0u].name};
    KAN_TEST_CHECK (kan_reflection_registry_query_local_field (registry, first_struct.name, 1u, first_first_path,
                                                               &absolute_offset,
                                                               &size_with_padding) == &first_struct_fields[0u])
    KAN_TEST_CHECK (absolute_offset == first_struct_fields[0u].offset)
    KAN_TEST_CHECK (size_with_padding == first_struct_fields[0u].size)

    kan_interned_string_t first_second_path[] = {first_struct_fields[1u].name};
    KAN_TEST_CHECK (kan_reflection_registry_query_local_field (registry, first_struct.name, 1u, first_second_path,
                                                               &absolute_offset,
                                                               &size_with_padding) == &first_struct_fields[1u])
    KAN_TEST_CHECK (absolute_offset == first_struct_fields[1u].offset)
    KAN_TEST_CHECK (size_with_padding == first_struct_fields[1u].size)

    kan_interned_string_t first_unknown_path[] = {kan_string_intern ("unknown")};
    KAN_TEST_CHECK (kan_reflection_registry_query_local_field (registry, first_struct.name, 1u, first_unknown_path,
                                                               &absolute_offset, &size_with_padding) == NULL)
    KAN_TEST_CHECK (absolute_offset == 0u)

    kan_interned_string_t second_second_third_path[] = {second_struct_fields[1u].name, first_struct_fields[2u].name};
    KAN_TEST_CHECK (kan_reflection_registry_query_local_field (registry, second_struct.name, 2u,
                                                               second_second_third_path, &absolute_offset,
                                                               &size_with_padding) == &first_struct_fields[2u])
    KAN_TEST_CHECK (absolute_offset == second_struct_fields[1u].offset + first_struct_fields[2u].offset)

    // Padding size is different due to different sizes on interned string pointers on different platforms.
    if (sizeof (void *) == 4u)
    {
        KAN_TEST_CHECK (size_with_padding == sizeof (void *) + sizeof (uint32_t))
    }
    else if (sizeof (void *) == 8u)
    {
        KAN_TEST_CHECK (size_with_padding == sizeof (void *))
    }

    kan_interned_string_t second_first_third_path[] = {second_struct_fields[0u].name, first_struct_fields[2u].name};
    KAN_TEST_CHECK (kan_reflection_registry_query_local_field (registry, second_struct.name, 2u,
                                                               second_first_third_path, &absolute_offset,
                                                               &size_with_padding) == NULL)
    KAN_TEST_CHECK (absolute_offset == 0u)

    kan_reflection_registry_destroy (registry);
}

struct struct_with_union_t
{
    uint32_t before;
    uint32_t switch_value;

    union
    {
        uint64_t first;
        uint64_t second;
    };

    uint32_t after;
};

KAN_TEST_CASE (field_visibility_iterator)
{
    kan_reflection_visibility_size_t first_switch_values[] = {-1, 1};
    kan_reflection_visibility_size_t second_switch_values[] = {-2, 2};

    struct kan_reflection_field_t struct_with_union_fields[] = {
        {.name = kan_string_intern ("before"),
         .offset = offsetof (struct struct_with_union_t, before),
         .size = sizeof (((struct struct_with_union_t *) NULL)->before),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("switch_value"),
         .offset = offsetof (struct struct_with_union_t, switch_value),
         .size = sizeof (((struct struct_with_union_t *) NULL)->switch_value),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("first"),
         .offset = offsetof (struct struct_with_union_t, first),
         .size = sizeof (((struct struct_with_union_t *) NULL)->first),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = &struct_with_union_fields[1u],
         .visibility_condition_values_count = 2u,
         .visibility_condition_values = first_switch_values},
        {.name = kan_string_intern ("second"),
         .offset = offsetof (struct struct_with_union_t, second),
         .size = sizeof (((struct struct_with_union_t *) NULL)->second),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = &struct_with_union_fields[1u],
         .visibility_condition_values_count = 2u,
         .visibility_condition_values = second_switch_values},
        {.name = kan_string_intern ("after"),
         .offset = offsetof (struct struct_with_union_t, after),
         .size = sizeof (((struct struct_with_union_t *) NULL)->after),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
    };

    struct kan_reflection_struct_t struct_with_union = {
        kan_string_intern ("struct_with_union_t"),
        sizeof (struct struct_with_union_t),
        _Alignof (struct struct_with_union_t),
        NULL,
        NULL,
        0u,
        sizeof (struct_with_union_fields) / sizeof (struct kan_reflection_field_t),
        struct_with_union_fields,
    };

    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_TEST_CHECK (kan_reflection_registry_add_struct (registry, &struct_with_union))

    struct struct_with_union_t struct_first_first_value = {
        .before = 12u, .switch_value = (uint32_t) first_switch_values[0u], .first = 42u, .after = 17u};
    struct struct_with_union_t struct_first_second_value = {
        .before = 12u, .switch_value = (uint32_t) first_switch_values[1u], .first = 42u, .after = 17u};
    struct struct_with_union_t struct_second_first_value = {
        .before = 12u, .switch_value = (uint32_t) second_switch_values[0u], .second = 42u, .after = 17u};
    struct struct_with_union_t struct_second_second_value = {
        .before = 12u, .switch_value = (uint32_t) second_switch_values[1u], .second = 42u, .after = 17u};

    struct kan_reflection_visibility_iterator_t iterator;

    kan_reflection_visibility_iterator_init (&iterator, &struct_with_union, &struct_first_first_value);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[0u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[1u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[2u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[4u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == iterator.field_end)

    kan_reflection_visibility_iterator_init (&iterator, &struct_with_union, &struct_first_second_value);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[0u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[1u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[2u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[4u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == iterator.field_end)

    kan_reflection_visibility_iterator_init (&iterator, &struct_with_union, &struct_second_first_value);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[0u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[1u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[3u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[4u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == iterator.field_end)

    kan_reflection_visibility_iterator_init (&iterator, &struct_with_union, &struct_second_second_value);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[0u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[1u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[3u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == &struct_with_union_fields[4u])
    kan_reflection_visibility_iterator_advance (&iterator);
    KAN_TEST_CHECK (iterator.field == iterator.field_end)

    kan_reflection_registry_destroy (registry);
}

struct patch_inner_t
{
    uint64_t first;
    uint64_t second;
};

struct patch_outer_t
{
    double before;
    struct patch_inner_t inner[2u];
    double after;
};

static kan_bool_t is_patch_outer_equal (const struct patch_outer_t *first, const struct patch_outer_t *second)
{
    return first->before == second->before && first->inner[0].first == second->inner[0].first &&
           first->inner[0].second == second->inner[0].second && first->inner[1].first == second->inner[1].first &&
           first->inner[1].second == second->inner[1].second && first->after == second->after;
}

KAN_TEST_CASE (patch)
{
    struct kan_reflection_field_t patch_inner_fields[] = {
        {.name = kan_string_intern ("first"),
         .offset = offsetof (struct patch_inner_t, first),
         .size = sizeof (((struct patch_inner_t *) NULL)->first),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("second"),
         .offset = offsetof (struct patch_inner_t, second),
         .size = sizeof (((struct patch_inner_t *) NULL)->second),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
    };

    struct kan_reflection_struct_t patch_inner = {
        kan_string_intern ("patch_inner_t"),
        sizeof (struct patch_inner_t),
        _Alignof (struct patch_inner_t),
        NULL,
        NULL,
        0u,
        sizeof (patch_inner_fields) / sizeof (struct kan_reflection_field_t),
        patch_inner_fields,
    };

    struct kan_reflection_field_t patch_outer_fields[] = {
        {.name = kan_string_intern ("before"),
         .offset = offsetof (struct patch_outer_t, before),
         .size = sizeof (((struct patch_outer_t *) NULL)->before),
         .archetype = KAN_REFLECTION_ARCHETYPE_FLOATING,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("inner"),
         .offset = offsetof (struct patch_outer_t, inner),
         .size = sizeof (((struct patch_outer_t *) NULL)->inner),
         .archetype = KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY,
         .archetype_inline_array =
             {
                 .item_archetype = KAN_REFLECTION_ARCHETYPE_STRUCT,
                 .item_size = sizeof (struct patch_inner_t),
                 .item_archetype_struct = {kan_string_intern ("patch_inner_t")},
                 .item_count = sizeof (((struct patch_outer_t *) NULL)->inner) / sizeof (struct patch_inner_t),
                 .size_field = NULL,
             },
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("after"),
         .offset = offsetof (struct patch_outer_t, after),
         .size = sizeof (((struct patch_outer_t *) NULL)->after),
         .archetype = KAN_REFLECTION_ARCHETYPE_FLOATING,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
    };

    struct kan_reflection_struct_t patch_outer = {
        kan_string_intern ("patch_outer_t"),
        sizeof (struct patch_outer_t),
        _Alignof (struct patch_outer_t),
        NULL,
        NULL,
        0u,
        sizeof (patch_outer_fields) / sizeof (struct kan_reflection_field_t),
        patch_outer_fields,
    };

    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_TEST_CHECK (kan_reflection_registry_add_struct (registry, &patch_inner))
    KAN_TEST_CHECK (kan_reflection_registry_add_struct (registry, &patch_outer))

    struct patch_outer_t first = {
        .before = 1.0,
        .inner =
            {
                {
                    .first = 1u,
                    .second = 2u,
                },
                {
                    .first = 3u,
                    .second = 4u,
                },
            },
        .after = 1.0,
    };

    kan_reflection_patch_builder_t patch_builder = kan_reflection_patch_builder_create ();
    struct patch_outer_t second = first;
    second.before = 2.0;
    second.inner[0u].first = 5u;
    second.inner[0u].second = 6u;

    kan_reflection_patch_builder_add_chunk (patch_builder, offsetof (struct patch_outer_t, before),
                                            sizeof (first.before), &second.before);
    kan_reflection_patch_builder_add_chunk (
        patch_builder, offsetof (struct patch_outer_t, inner) + offsetof (struct patch_inner_t, first),
        sizeof (second.inner[0u].first), &second.inner[0u].first);
    kan_reflection_patch_builder_add_chunk (
        patch_builder, offsetof (struct patch_outer_t, inner) + offsetof (struct patch_inner_t, second),
        sizeof (second.inner[0u].second), &second.inner[0u].second);

    kan_reflection_patch_t first_to_second = kan_reflection_patch_builder_build (patch_builder, registry, &patch_outer);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (first_to_second))

    struct patch_outer_t third = second;
    third.after = 3.0;
    third.inner[1u].first = 7u;
    third.inner[1u].second = 8u;

    kan_reflection_patch_builder_add_chunk (patch_builder, offsetof (struct patch_outer_t, after), sizeof (first.after),
                                            &third.after);
    kan_reflection_patch_builder_add_chunk (
        patch_builder,
        offsetof (struct patch_outer_t, inner) + sizeof (struct patch_inner_t) + offsetof (struct patch_inner_t, first),
        sizeof (third.inner[1u].first), &third.inner[1u].first);
    kan_reflection_patch_builder_add_chunk (patch_builder,
                                            offsetof (struct patch_outer_t, inner) + sizeof (struct patch_inner_t) +
                                                offsetof (struct patch_inner_t, second),
                                            sizeof (third.inner[1u].second), &third.inner[1u].second);

    kan_reflection_patch_t second_to_third = kan_reflection_patch_builder_build (patch_builder, registry, &patch_outer);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (second_to_third))
    kan_reflection_patch_builder_destroy (patch_builder);

    KAN_TEST_CHECK (!is_patch_outer_equal (&first, &second))
    KAN_TEST_CHECK (!is_patch_outer_equal (&second, &third))

    struct patch_outer_t test = first;
    KAN_TEST_CHECK (is_patch_outer_equal (&first, &test))

    kan_reflection_patch_apply (first_to_second, &test);
    KAN_TEST_CHECK (is_patch_outer_equal (&second, &test))

    kan_reflection_patch_apply (second_to_third, &test);
    KAN_TEST_CHECK (is_patch_outer_equal (&third, &test))

    kan_reflection_patch_iterator_t iterator = kan_reflection_patch_begin (first_to_second);
    kan_reflection_patch_iterator_t end = kan_reflection_patch_end (first_to_second);

    KAN_TEST_ASSERT (!KAN_HANDLE_IS_EQUAL (iterator, end))
    struct kan_reflection_patch_chunk_info_t chunk = kan_reflection_patch_iterator_get (iterator);
    KAN_TEST_CHECK (chunk.offset == 0u)
    KAN_TEST_CHECK (chunk.size == sizeof (double) + sizeof (struct patch_inner_t))
    iterator = kan_reflection_patch_iterator_next (iterator);
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (iterator, end))

    iterator = kan_reflection_patch_begin (second_to_third);
    end = kan_reflection_patch_end (second_to_third);

    KAN_TEST_ASSERT (!KAN_HANDLE_IS_EQUAL (iterator, end))
    chunk = kan_reflection_patch_iterator_get (iterator);
    KAN_TEST_CHECK (chunk.offset == sizeof (double) + sizeof (struct patch_inner_t))
    KAN_TEST_CHECK (chunk.size == sizeof (double) + sizeof (struct patch_inner_t))
    iterator = kan_reflection_patch_iterator_next (iterator);
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (iterator, end))

    // Patches will be automatically destroyed with owning registry.
    kan_reflection_registry_destroy (registry);
}

enum first_enum_source_t
{
    FIRST_ENUM_SOURCE_HELLO = 0,
    FIRST_ENUM_SOURCE_WORLD
};

enum first_enum_target_t
{
    FIRST_ENUM_TARGET_HELLO = 0,
    FIRST_ENUM_TARGET_REFLECTION,
    FIRST_ENUM_TARGET_WORLD
};

enum second_enum_source_t
{
    SECOND_ENUM_SOURCE_FIRST = 1u << 0u,
    SECOND_ENUM_SOURCE_SECOND = 1u << 1u,
    SECOND_ENUM_SOURCE_THIRD = 1u << 2u,
};

enum second_enum_target_t
{
    SECOND_ENUM_TARGET_FIRST = 1u << 0u,
    SECOND_ENUM_TARGET_SECOND = 1u << 1u,
    SECOND_ENUM_TARGET_THIRD = 1u << 2u,
};

struct same_source_t
{
    uint64_t first;
    enum second_enum_source_t second;
    uint64_t third;
};

struct same_target_t
{
    uint64_t first;
    enum second_enum_target_t second;
    uint64_t third;
};

struct cross_copy_source_t
{
    uint64_t a;
    uint64_t b;
    uint64_t c;
    uint64_t d;
    uint64_t e;
    uint64_t f;
};

struct cross_copy_target_t
{
    uint64_t d;
    uint64_t e;
    uint64_t f;
    uint64_t a;
    uint64_t b;
    uint64_t c;
};

struct nesting_source_t
{
    struct same_source_t same;
    struct cross_copy_source_t cross_copy;
};

struct nesting_target_t
{
    struct same_target_t same;
    struct cross_copy_target_t cross_copy;
};

struct migration_source_t
{
    struct nesting_source_t nesting_first;
    struct nesting_source_t nesting_to_drop;
    struct nesting_source_t nesting_second;

    enum first_enum_source_t enums_array_one[4u];
    enum second_enum_source_t enums_array_two[4u];
    uint32_t numeric_to_adapt;

    uint64_t selector;
    union
    {
        enum first_enum_source_t selection_first;
        enum second_enum_source_t selection_second;
    };

    kan_interned_string_t interned_string;
    char *owned_string;
    struct kan_dynamic_array_t dynamic_array;
};

struct migration_target_t
{
    struct nesting_target_t nesting_first;
    struct nesting_target_t nesting_second;
    struct nesting_target_t nesting_to_add;

    enum first_enum_target_t enums_array_one[4u];
    enum second_enum_target_t enums_array_two[4u];
    uint64_t numeric_to_adapt;

    uint64_t selector;
    union
    {
        enum first_enum_target_t selection_first;
        enum second_enum_target_t selection_second;
    };

    kan_interned_string_t interned_string;
    char *owned_string;
    struct kan_dynamic_array_t dynamic_array;
};

static struct migration_target_t construct_empty_migration_target (void)
{
    struct migration_target_t migration_target = {
        .nesting_first =
            {
                .same = {.first = 0u, .second = 0u, .third = 0u},
                .cross_copy = {.a = 0u, .b = 0u, .c = 0u, .d = 0u, .e = 0u, .f = 0u},
            },
        .nesting_second =
            {
                .same = {.first = 0u, .second = 0u, .third = 0u},
                .cross_copy = {.a = 0u, .b = 0u, .c = 0u, .d = 0u, .e = 0u, .f = 0u},
            },
        .nesting_to_add =
            {
                .same = {.first = 0u, .second = 0u, .third = 0u},
                .cross_copy = {.a = 0u, .b = 0u, .c = 0u, .d = 0u, .e = 0u, .f = 0u},
            },
        .enums_array_one = {FIRST_ENUM_TARGET_HELLO, FIRST_ENUM_TARGET_HELLO, FIRST_ENUM_TARGET_HELLO,
                            FIRST_ENUM_TARGET_HELLO},
        .enums_array_two = {0u, 0u, 0u, 0u},
        .numeric_to_adapt = 0u,
        .selector = 0u,
        .selection_first = FIRST_ENUM_TARGET_HELLO,
        .interned_string = NULL,
        .owned_string = NULL,
    };

    kan_dynamic_array_init (&migration_target.dynamic_array, 0u, sizeof (int), _Alignof (int),
                            KAN_ALLOCATION_GROUP_IGNORE);
    return migration_target;
}

static void check_nesting_migration_result (const struct nesting_source_t *source,
                                            const struct nesting_target_t *target)
{
    KAN_TEST_CHECK (source->same.first == target->same.first)
    KAN_TEST_CHECK ((int) source->same.second == (int) target->same.second)
    KAN_TEST_CHECK (source->same.third == target->same.third)

    KAN_TEST_CHECK (source->cross_copy.a == target->cross_copy.a)
    KAN_TEST_CHECK (source->cross_copy.b == target->cross_copy.b)
    KAN_TEST_CHECK (source->cross_copy.c == target->cross_copy.c)
    KAN_TEST_CHECK (source->cross_copy.d == target->cross_copy.d)
    KAN_TEST_CHECK (source->cross_copy.e == target->cross_copy.e)
    KAN_TEST_CHECK (source->cross_copy.f == target->cross_copy.f)
}

static void check_nesting_unchanged (const struct nesting_target_t *target)
{
    KAN_TEST_CHECK (target->same.first == 0u)
    KAN_TEST_CHECK (target->same.second == 0)
    KAN_TEST_CHECK (target->same.third == 0u)
    KAN_TEST_CHECK (target->cross_copy.a == 0u)
    KAN_TEST_CHECK (target->cross_copy.b == 0u)
    KAN_TEST_CHECK (target->cross_copy.c == 0u)
    KAN_TEST_CHECK (target->cross_copy.d == 0u)
    KAN_TEST_CHECK (target->cross_copy.e == 0u)
    KAN_TEST_CHECK (target->cross_copy.f == 0u)
}

static void check_first_enum_migration_result (enum first_enum_source_t source, enum first_enum_target_t target)
{
    switch (source)
    {
    case FIRST_ENUM_SOURCE_HELLO:
        KAN_TEST_CHECK (target == FIRST_ENUM_TARGET_HELLO)
        break;

    case FIRST_ENUM_SOURCE_WORLD:
        KAN_TEST_CHECK (target == FIRST_ENUM_TARGET_WORLD)
        break;

    default:
        KAN_TEST_CHECK (KAN_FALSE)
    }
}

static void check_generic_migration_result (const struct migration_source_t *source,
                                            const struct migration_target_t *target)
{
    check_nesting_migration_result (&source->nesting_first, &target->nesting_first);
    check_nesting_migration_result (&source->nesting_second, &target->nesting_second);
    check_nesting_unchanged (&target->nesting_to_add);

    check_first_enum_migration_result (source->enums_array_one[0u], target->enums_array_one[0u]);
    check_first_enum_migration_result (source->enums_array_one[1u], target->enums_array_one[1u]);
    check_first_enum_migration_result (source->enums_array_one[2u], target->enums_array_one[2u]);
    check_first_enum_migration_result (source->enums_array_one[3u], target->enums_array_one[3u]);

    KAN_TEST_CHECK ((int) source->enums_array_two[0u] == (int) target->enums_array_two[0u])
    KAN_TEST_CHECK ((int) source->enums_array_two[1u] == (int) target->enums_array_two[1u])
    KAN_TEST_CHECK ((int) source->enums_array_two[2u] == (int) target->enums_array_two[2u])
    KAN_TEST_CHECK ((int) source->enums_array_two[3u] == (int) target->enums_array_two[3u])

    KAN_TEST_CHECK ((uint64_t) source->numeric_to_adapt == target->numeric_to_adapt)
    KAN_TEST_CHECK (source->selector == target->selector)

    if (source->selector == 0u)
    {
        check_first_enum_migration_result (source->selection_first, target->selection_first);
    }
    else
    {
        KAN_TEST_CHECK ((int) source->selection_second == (int) target->selection_second)
    }

    KAN_TEST_CHECK (source->interned_string == target->interned_string)

    // String should be moved, so we can't check it.
    KAN_TEST_CHECK (!source->owned_string)

    KAN_TEST_CHECK (source->dynamic_array.capacity == target->dynamic_array.capacity)
    KAN_TEST_CHECK (source->dynamic_array.size == target->dynamic_array.size)

    const uint64_t min_size = KAN_MIN (source->dynamic_array.size, target->dynamic_array.size);
    for (kan_loop_size_t index = 0u; index < min_size; ++index)
    {
        check_first_enum_migration_result (((const enum first_enum_source_t *) source->dynamic_array.data)[index],
                                           ((const enum first_enum_target_t *) target->dynamic_array.data)[index]);
    }
}

KAN_TEST_CASE (migration)
{
    struct kan_reflection_enum_value_t first_enum_source_values[] = {
        {kan_string_intern ("FIRST_ENUM_HELLO"), FIRST_ENUM_SOURCE_HELLO},
        {kan_string_intern ("FIRST_ENUM_WORLD"), FIRST_ENUM_SOURCE_WORLD},
    };

    struct kan_reflection_enum_value_t first_enum_target_values[] = {
        {kan_string_intern ("FIRST_ENUM_HELLO"), FIRST_ENUM_TARGET_HELLO},
        {kan_string_intern ("FIRST_ENUM_REFLECTION"), FIRST_ENUM_TARGET_REFLECTION},
        {kan_string_intern ("FIRST_ENUM_WORLD"), FIRST_ENUM_TARGET_WORLD},
    };

    struct kan_reflection_enum_t first_enum_source = {
        .name = kan_string_intern ("first_enum_t"),
        .flags = KAN_FALSE,
        .values_count = sizeof (first_enum_source_values) / sizeof (struct kan_reflection_enum_value_t),
        .values = first_enum_source_values,
    };

    struct kan_reflection_enum_t first_enum_target = {
        .name = kan_string_intern ("first_enum_t"),
        .flags = KAN_FALSE,
        .values_count = sizeof (first_enum_target_values) / sizeof (struct kan_reflection_enum_value_t),
        .values = first_enum_target_values,
    };

    struct kan_reflection_enum_value_t second_enum_source_values[] = {
        {kan_string_intern ("SECOND_ENUM_FIRST"), SECOND_ENUM_SOURCE_FIRST},
        {kan_string_intern ("SECOND_ENUM_SECOND"), SECOND_ENUM_SOURCE_SECOND},
        {kan_string_intern ("SECOND_ENUM_THIRD"), SECOND_ENUM_SOURCE_THIRD},
    };

    struct kan_reflection_enum_value_t second_enum_target_values[] = {
        {kan_string_intern ("SECOND_ENUM_FIRST"), SECOND_ENUM_TARGET_FIRST},
        {kan_string_intern ("SECOND_ENUM_SECOND"), SECOND_ENUM_TARGET_SECOND},
        {kan_string_intern ("SECOND_ENUM_THIRD"), SECOND_ENUM_TARGET_THIRD},
    };

    struct kan_reflection_enum_t second_enum_source = {
        .name = kan_string_intern ("second_enum_t"),
        .flags = KAN_TRUE,
        .values_count = sizeof (second_enum_source_values) / sizeof (struct kan_reflection_enum_value_t),
        .values = second_enum_source_values,
    };

    struct kan_reflection_enum_t second_enum_target = {
        .name = kan_string_intern ("second_enum_t"),
        .flags = KAN_TRUE,
        .values_count = sizeof (second_enum_target_values) / sizeof (struct kan_reflection_enum_value_t),
        .values = second_enum_target_values,
    };

    struct kan_reflection_field_t same_source_fields[] = {
        {.name = kan_string_intern ("first"),
         .offset = offsetof (struct same_source_t, first),
         .size = sizeof (((struct same_source_t *) NULL)->first),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("second"),
         .offset = offsetof (struct same_source_t, second),
         .size = sizeof (((struct same_source_t *) NULL)->second),
         .archetype = KAN_REFLECTION_ARCHETYPE_ENUM,
         .archetype_enum = {.type_name = kan_string_intern ("second_enum_t")},
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("third"),
         .offset = offsetof (struct same_source_t, third),
         .size = sizeof (((struct same_source_t *) NULL)->third),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
    };

    struct kan_reflection_field_t same_target_fields[] = {
        {.name = kan_string_intern ("first"),
         .offset = offsetof (struct same_target_t, first),
         .size = sizeof (((struct same_target_t *) NULL)->first),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("second"),
         .offset = offsetof (struct same_target_t, second),
         .size = sizeof (((struct same_target_t *) NULL)->second),
         .archetype = KAN_REFLECTION_ARCHETYPE_ENUM,
         .archetype_enum = {.type_name = kan_string_intern ("second_enum_t")},
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("third"),
         .offset = offsetof (struct same_target_t, third),
         .size = sizeof (((struct same_target_t *) NULL)->third),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
    };

    struct kan_reflection_struct_t same_source = {
        .name = kan_string_intern ("same_t"),
        .size = sizeof (struct same_source_t),
        .alignment = _Alignof (struct same_source_t),
        .init = NULL,
        .shutdown = NULL,
        .functor_user_data = 0u,
        .fields_count = sizeof (same_source_fields) / sizeof (struct kan_reflection_field_t),
        .fields = same_source_fields,
    };

    struct kan_reflection_struct_t same_target = {
        .name = kan_string_intern ("same_t"),
        .size = sizeof (struct same_target_t),
        .alignment = _Alignof (struct same_target_t),
        .init = NULL,
        .shutdown = NULL,
        .functor_user_data = 0u,
        .fields_count = sizeof (same_target_fields) / sizeof (struct kan_reflection_field_t),
        .fields = same_target_fields,
    };

    struct kan_reflection_field_t cross_copy_source_fields[] = {
        {.name = kan_string_intern ("a"),
         .offset = offsetof (struct cross_copy_source_t, a),
         .size = sizeof (((struct cross_copy_source_t *) NULL)->a),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("b"),
         .offset = offsetof (struct cross_copy_source_t, b),
         .size = sizeof (((struct cross_copy_source_t *) NULL)->b),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("c"),
         .offset = offsetof (struct cross_copy_source_t, c),
         .size = sizeof (((struct cross_copy_source_t *) NULL)->c),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("d"),
         .offset = offsetof (struct cross_copy_source_t, d),
         .size = sizeof (((struct cross_copy_source_t *) NULL)->d),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("e"),
         .offset = offsetof (struct cross_copy_source_t, e),
         .size = sizeof (((struct cross_copy_source_t *) NULL)->e),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("f"),
         .offset = offsetof (struct cross_copy_source_t, f),
         .size = sizeof (((struct cross_copy_source_t *) NULL)->f),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
    };

    struct kan_reflection_field_t cross_copy_target_fields[] = {
        {.name = kan_string_intern ("d"),
         .offset = offsetof (struct cross_copy_target_t, d),
         .size = sizeof (((struct cross_copy_target_t *) NULL)->d),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("e"),
         .offset = offsetof (struct cross_copy_target_t, e),
         .size = sizeof (((struct cross_copy_target_t *) NULL)->e),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("f"),
         .offset = offsetof (struct cross_copy_target_t, f),
         .size = sizeof (((struct cross_copy_target_t *) NULL)->f),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("a"),
         .offset = offsetof (struct cross_copy_target_t, a),
         .size = sizeof (((struct cross_copy_target_t *) NULL)->a),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("b"),
         .offset = offsetof (struct cross_copy_target_t, b),
         .size = sizeof (((struct cross_copy_target_t *) NULL)->b),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("c"),
         .offset = offsetof (struct cross_copy_target_t, c),
         .size = sizeof (((struct cross_copy_target_t *) NULL)->c),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
    };

    struct kan_reflection_struct_t cross_copy_source = {
        .name = kan_string_intern ("cross_copy_t"),
        .size = sizeof (struct cross_copy_source_t),
        .alignment = _Alignof (struct cross_copy_source_t),
        .init = NULL,
        .shutdown = NULL,
        .functor_user_data = 0u,
        .fields_count = sizeof (cross_copy_source_fields) / sizeof (struct kan_reflection_field_t),
        .fields = cross_copy_source_fields,
    };

    struct kan_reflection_struct_t cross_copy_target = {
        .name = kan_string_intern ("cross_copy_t"),
        .size = sizeof (struct cross_copy_target_t),
        .alignment = _Alignof (struct cross_copy_target_t),
        .init = NULL,
        .shutdown = NULL,
        .functor_user_data = 0u,
        .fields_count = sizeof (cross_copy_target_fields) / sizeof (struct kan_reflection_field_t),
        .fields = cross_copy_target_fields,
    };

    struct kan_reflection_field_t nesting_source_fields[] = {
        {.name = kan_string_intern ("same"),
         .offset = offsetof (struct nesting_source_t, same),
         .size = sizeof (((struct nesting_source_t *) NULL)->same),
         .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT,
         .archetype_struct = {.type_name = kan_string_intern ("same_t")},
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("cross_copy"),
         .offset = offsetof (struct nesting_source_t, cross_copy),
         .size = sizeof (((struct nesting_source_t *) NULL)->cross_copy),
         .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT,
         .archetype_struct = {.type_name = kan_string_intern ("cross_copy_t")},
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
    };

    struct kan_reflection_field_t nesting_target_fields[] = {
        {.name = kan_string_intern ("same"),
         .offset = offsetof (struct nesting_target_t, same),
         .size = sizeof (((struct nesting_target_t *) NULL)->same),
         .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT,
         .archetype_struct = {.type_name = kan_string_intern ("same_t")},
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("cross_copy"),
         .offset = offsetof (struct nesting_target_t, cross_copy),
         .size = sizeof (((struct nesting_target_t *) NULL)->cross_copy),
         .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT,
         .archetype_struct = {.type_name = kan_string_intern ("cross_copy_t")},
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
    };

    struct kan_reflection_struct_t nesting_source = {
        .name = kan_string_intern ("nesting_t"),
        .size = sizeof (struct nesting_source_t),
        .alignment = _Alignof (struct nesting_source_t),
        .init = NULL,
        .shutdown = NULL,
        .functor_user_data = 0u,
        .fields_count = sizeof (nesting_source_fields) / sizeof (struct kan_reflection_field_t),
        .fields = nesting_source_fields,
    };

    struct kan_reflection_struct_t nesting_target = {
        .name = kan_string_intern ("nesting_t"),
        .size = sizeof (struct nesting_target_t),
        .alignment = _Alignof (struct nesting_target_t),
        .init = NULL,
        .shutdown = NULL,
        .functor_user_data = 0u,
        .fields_count = sizeof (nesting_target_fields) / sizeof (struct kan_reflection_field_t),
        .fields = nesting_target_fields,
    };

    kan_reflection_visibility_size_t first_selection_conditions[] = {(int64_t) 0u};
    kan_reflection_visibility_size_t second_selection_conditions[] = {(int64_t) 1u};

    struct kan_reflection_field_t migration_source_fields[] = {
        {.name = kan_string_intern ("nesting_first"),
         .offset = offsetof (struct migration_source_t, nesting_first),
         .size = sizeof (((struct migration_source_t *) NULL)->nesting_first),
         .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT,
         .archetype_struct = {.type_name = kan_string_intern ("nesting_t")},
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("nesting_to_drop"),
         .offset = offsetof (struct migration_source_t, nesting_to_drop),
         .size = sizeof (((struct migration_source_t *) NULL)->nesting_to_drop),
         .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT,
         .archetype_struct = {.type_name = kan_string_intern ("nesting_t")},
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("nesting_second"),
         .offset = offsetof (struct migration_source_t, nesting_second),
         .size = sizeof (((struct migration_source_t *) NULL)->nesting_second),
         .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT,
         .archetype_struct = {.type_name = kan_string_intern ("nesting_t")},
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("enums_array_one"),
         .offset = offsetof (struct migration_source_t, enums_array_one),
         .size = sizeof (((struct migration_source_t *) NULL)->enums_array_one),
         .archetype = KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY,
         .archetype_inline_array =
             {
                 .item_archetype = KAN_REFLECTION_ARCHETYPE_ENUM,
                 .item_size = sizeof (int),
                 .item_archetype_enum = {.type_name = kan_string_intern ("first_enum_t")},
                 .item_count = 4u,
                 .size_field = NULL,
             },
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("enums_array_two"),
         .offset = offsetof (struct migration_source_t, enums_array_two),
         .size = sizeof (((struct migration_source_t *) NULL)->enums_array_two),
         .archetype = KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY,
         .archetype_inline_array =
             {
                 .item_archetype = KAN_REFLECTION_ARCHETYPE_ENUM,
                 .item_size = sizeof (int),
                 .item_archetype_enum = {.type_name = kan_string_intern ("second_enum_t")},
                 .item_count = 4u,
                 .size_field = NULL,
             },
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("numeric_to_adapt"),
         .offset = offsetof (struct migration_source_t, numeric_to_adapt),
         .size = sizeof (((struct migration_source_t *) NULL)->numeric_to_adapt),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("selector"),
         .offset = offsetof (struct migration_source_t, selector),
         .size = sizeof (((struct migration_source_t *) NULL)->selector),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("selection_first"),
         .offset = offsetof (struct migration_source_t, selection_first),
         .size = sizeof (((struct migration_source_t *) NULL)->selection_first),
         .archetype = KAN_REFLECTION_ARCHETYPE_ENUM,
         .archetype_enum = {.type_name = kan_string_intern ("first_enum_t")},
         .visibility_condition_field = &migration_source_fields[6],
         .visibility_condition_values_count = 1u,
         .visibility_condition_values = first_selection_conditions},
        {.name = kan_string_intern ("selection_second"),
         .offset = offsetof (struct migration_source_t, selection_second),
         .size = sizeof (((struct migration_source_t *) NULL)->selection_second),
         .archetype = KAN_REFLECTION_ARCHETYPE_ENUM,
         .archetype_enum = {.type_name = kan_string_intern ("second_enum_t")},
         .visibility_condition_field = &migration_source_fields[6],
         .visibility_condition_values_count = 1u,
         .visibility_condition_values = second_selection_conditions},
        {.name = kan_string_intern ("interned_string"),
         .offset = offsetof (struct migration_source_t, interned_string),
         .size = sizeof (((struct migration_source_t *) NULL)->interned_string),
         .archetype = KAN_REFLECTION_ARCHETYPE_INTERNED_STRING,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("owned_string"),
         .offset = offsetof (struct migration_source_t, owned_string),
         .size = sizeof (((struct migration_source_t *) NULL)->owned_string),
         .archetype = KAN_REFLECTION_ARCHETYPE_STRING_POINTER,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("dynamic_array"),
         .offset = offsetof (struct migration_source_t, dynamic_array),
         .size = sizeof (((struct migration_source_t *) NULL)->dynamic_array),
         .archetype = KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY,
         .archetype_dynamic_array =
             {
                 .item_archetype = KAN_REFLECTION_ARCHETYPE_ENUM,
                 .item_size = sizeof (int),
                 .item_archetype_enum = {.type_name = kan_string_intern ("first_enum_t")},
             },
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
    };

    struct kan_reflection_field_t migration_target_fields[] = {
        {.name = kan_string_intern ("nesting_first"),
         .offset = offsetof (struct migration_target_t, nesting_first),
         .size = sizeof (((struct migration_target_t *) NULL)->nesting_first),
         .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT,
         .archetype_struct = {.type_name = kan_string_intern ("nesting_t")},
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("nesting_second"),
         .offset = offsetof (struct migration_target_t, nesting_second),
         .size = sizeof (((struct migration_target_t *) NULL)->nesting_second),
         .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT,
         .archetype_struct = {.type_name = kan_string_intern ("nesting_t")},
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("nesting_to_add"),
         .offset = offsetof (struct migration_target_t, nesting_to_add),
         .size = sizeof (((struct migration_target_t *) NULL)->nesting_to_add),
         .archetype = KAN_REFLECTION_ARCHETYPE_STRUCT,
         .archetype_struct = {.type_name = kan_string_intern ("nesting_t")},
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("enums_array_one"),
         .offset = offsetof (struct migration_target_t, enums_array_one),
         .size = sizeof (((struct migration_target_t *) NULL)->enums_array_one),
         .archetype = KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY,
         .archetype_inline_array =
             {
                 .item_archetype = KAN_REFLECTION_ARCHETYPE_ENUM,
                 .item_size = sizeof (int),
                 .item_archetype_enum = {.type_name = kan_string_intern ("first_enum_t")},
                 .item_count = 4u,
                 .size_field = NULL,
             },
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("enums_array_two"),
         .offset = offsetof (struct migration_target_t, enums_array_two),
         .size = sizeof (((struct migration_target_t *) NULL)->enums_array_two),
         .archetype = KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY,
         .archetype_inline_array =
             {
                 .item_archetype = KAN_REFLECTION_ARCHETYPE_ENUM,
                 .item_size = sizeof (int),
                 .item_archetype_enum = {.type_name = kan_string_intern ("second_enum_t")},
                 .item_count = 4u,
                 .size_field = NULL,
             },
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("numeric_to_adapt"),
         .offset = offsetof (struct migration_target_t, numeric_to_adapt),
         .size = sizeof (((struct migration_target_t *) NULL)->numeric_to_adapt),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("selector"),
         .offset = offsetof (struct migration_target_t, selector),
         .size = sizeof (((struct migration_target_t *) NULL)->selector),
         .archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("selection_first"),
         .offset = offsetof (struct migration_target_t, selection_first),
         .size = sizeof (((struct migration_target_t *) NULL)->selection_first),
         .archetype = KAN_REFLECTION_ARCHETYPE_ENUM,
         .archetype_enum = {.type_name = kan_string_intern ("first_enum_t")},
         .visibility_condition_field = &migration_target_fields[6],
         .visibility_condition_values_count = 1u,
         .visibility_condition_values = first_selection_conditions},
        {.name = kan_string_intern ("selection_second"),
         .offset = offsetof (struct migration_target_t, selection_second),
         .size = sizeof (((struct migration_target_t *) NULL)->selection_second),
         .archetype = KAN_REFLECTION_ARCHETYPE_ENUM,
         .archetype_enum = {.type_name = kan_string_intern ("second_enum_t")},
         .visibility_condition_field = &migration_target_fields[6],
         .visibility_condition_values_count = 1u,
         .visibility_condition_values = second_selection_conditions},
        {.name = kan_string_intern ("interned_string"),
         .offset = offsetof (struct migration_target_t, interned_string),
         .size = sizeof (((struct migration_target_t *) NULL)->interned_string),
         .archetype = KAN_REFLECTION_ARCHETYPE_INTERNED_STRING,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("owned_string"),
         .offset = offsetof (struct migration_target_t, owned_string),
         .size = sizeof (((struct migration_target_t *) NULL)->owned_string),
         .archetype = KAN_REFLECTION_ARCHETYPE_STRING_POINTER,
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
        {.name = kan_string_intern ("dynamic_array"),
         .offset = offsetof (struct migration_target_t, dynamic_array),
         .size = sizeof (((struct migration_target_t *) NULL)->dynamic_array),
         .archetype = KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY,
         .archetype_dynamic_array =
             {
                 .item_archetype = KAN_REFLECTION_ARCHETYPE_ENUM,
                 .item_size = sizeof (int),
                 .item_archetype_enum = {.type_name = kan_string_intern ("first_enum_t")},
             },
         .visibility_condition_field = NULL,
         .visibility_condition_values_count = 0u,
         .visibility_condition_values = NULL},
    };

    struct kan_reflection_struct_t migration_source = {
        .name = kan_string_intern ("migration_t"),
        .size = sizeof (struct migration_source_t),
        .alignment = _Alignof (struct migration_source_t),
        .init = NULL,
        .shutdown = NULL,
        .functor_user_data = 0u,
        .fields_count = sizeof (migration_source_fields) / sizeof (struct kan_reflection_field_t),
        .fields = migration_source_fields,
    };

    struct kan_reflection_struct_t migration_target = {
        .name = kan_string_intern ("migration_t"),
        .size = sizeof (struct migration_target_t),
        .alignment = _Alignof (struct migration_target_t),
        .init = NULL,
        .shutdown = NULL,
        .functor_user_data = 0u,
        .fields_count = sizeof (migration_target_fields) / sizeof (struct kan_reflection_field_t),
        .fields = migration_target_fields,
    };

    kan_reflection_registry_t source_registry = kan_reflection_registry_create ();
    kan_reflection_registry_t target_registry = kan_reflection_registry_create ();

    KAN_TEST_CHECK (kan_reflection_registry_add_enum (source_registry, &first_enum_source))
    KAN_TEST_CHECK (kan_reflection_registry_add_enum (target_registry, &first_enum_target))

    KAN_TEST_CHECK (kan_reflection_registry_add_enum (source_registry, &second_enum_source))
    KAN_TEST_CHECK (kan_reflection_registry_add_enum (target_registry, &second_enum_target))

    KAN_TEST_CHECK (kan_reflection_registry_add_struct (source_registry, &same_source))
    KAN_TEST_CHECK (kan_reflection_registry_add_struct (target_registry, &same_target))

    KAN_TEST_CHECK (kan_reflection_registry_add_struct (source_registry, &cross_copy_source))
    KAN_TEST_CHECK (kan_reflection_registry_add_struct (target_registry, &cross_copy_target))

    KAN_TEST_CHECK (kan_reflection_registry_add_struct (source_registry, &nesting_source))
    KAN_TEST_CHECK (kan_reflection_registry_add_struct (target_registry, &nesting_target))

    KAN_TEST_CHECK (kan_reflection_registry_add_struct (source_registry, &migration_source))
    KAN_TEST_CHECK (kan_reflection_registry_add_struct (target_registry, &migration_target))

    kan_reflection_migration_seed_t source_to_target_migration_seed =
        kan_reflection_migration_seed_build (source_registry, target_registry);

    const struct kan_reflection_enum_migration_seed_t *first_enum_seed =
        kan_reflection_migration_seed_get_for_enum (source_to_target_migration_seed, first_enum_source.name);
    KAN_TEST_ASSERT (first_enum_seed)
    KAN_TEST_CHECK (first_enum_seed->status == KAN_REFLECTION_MIGRATION_NEEDED)
    KAN_TEST_CHECK (first_enum_seed->value_remap[0u] == &first_enum_target_values[0u])
    KAN_TEST_CHECK (first_enum_seed->value_remap[1u] == &first_enum_target_values[2u])

    const struct kan_reflection_enum_migration_seed_t *second_enum_seed =
        kan_reflection_migration_seed_get_for_enum (source_to_target_migration_seed, second_enum_source.name);
    KAN_TEST_ASSERT (second_enum_seed)
    KAN_TEST_CHECK (second_enum_seed->status == KAN_REFLECTION_MIGRATION_NOT_NEEDED)

    const struct kan_reflection_struct_migration_seed_t *same_seed =
        kan_reflection_migration_seed_get_for_struct (source_to_target_migration_seed, same_source.name);
    KAN_TEST_ASSERT (same_seed)
    KAN_TEST_CHECK (same_seed->status == KAN_REFLECTION_MIGRATION_NOT_NEEDED)

    const struct kan_reflection_struct_migration_seed_t *cross_copy_seed =
        kan_reflection_migration_seed_get_for_struct (source_to_target_migration_seed, cross_copy_source.name);
    KAN_TEST_ASSERT (cross_copy_seed)
    KAN_TEST_CHECK (cross_copy_seed->status == KAN_REFLECTION_MIGRATION_NEEDED)
    KAN_TEST_CHECK (cross_copy_seed->field_remap[0u] == &cross_copy_target_fields[3u])
    KAN_TEST_CHECK (cross_copy_seed->field_remap[1u] == &cross_copy_target_fields[4u])
    KAN_TEST_CHECK (cross_copy_seed->field_remap[2u] == &cross_copy_target_fields[5u])
    KAN_TEST_CHECK (cross_copy_seed->field_remap[3u] == &cross_copy_target_fields[0u])
    KAN_TEST_CHECK (cross_copy_seed->field_remap[4u] == &cross_copy_target_fields[1u])
    KAN_TEST_CHECK (cross_copy_seed->field_remap[5u] == &cross_copy_target_fields[2u])

    const struct kan_reflection_struct_migration_seed_t *nesting_seed =
        kan_reflection_migration_seed_get_for_struct (source_to_target_migration_seed, nesting_source.name);
    KAN_TEST_ASSERT (nesting_seed)
    KAN_TEST_CHECK (nesting_seed->status == KAN_REFLECTION_MIGRATION_NEEDED)
    KAN_TEST_CHECK (nesting_seed->field_remap[0u] == &nesting_target_fields[0u])
    KAN_TEST_CHECK (nesting_seed->field_remap[1u] == &nesting_target_fields[1u])

    const struct kan_reflection_struct_migration_seed_t *migration_seed =
        kan_reflection_migration_seed_get_for_struct (source_to_target_migration_seed, migration_source.name);
    KAN_TEST_ASSERT (migration_seed)
    KAN_TEST_CHECK (migration_seed->status == KAN_REFLECTION_MIGRATION_NEEDED)
    KAN_TEST_CHECK (migration_seed->field_remap[0u] == &migration_target_fields[0u])
    KAN_TEST_CHECK (migration_seed->field_remap[1u] == NULL)
    KAN_TEST_CHECK (migration_seed->field_remap[2u] == &migration_target_fields[1u])
    KAN_TEST_CHECK (migration_seed->field_remap[3u] == &migration_target_fields[3u])
    KAN_TEST_CHECK (migration_seed->field_remap[4u] == &migration_target_fields[4u])
    KAN_TEST_CHECK (migration_seed->field_remap[5u] == &migration_target_fields[5u])
    KAN_TEST_CHECK (migration_seed->field_remap[6u] == &migration_target_fields[6u])
    KAN_TEST_CHECK (migration_seed->field_remap[7u] == &migration_target_fields[7u])
    KAN_TEST_CHECK (migration_seed->field_remap[8u] == &migration_target_fields[8u])
    KAN_TEST_CHECK (migration_seed->field_remap[9u] == &migration_target_fields[9u])
    KAN_TEST_CHECK (migration_seed->field_remap[10u] == &migration_target_fields[10u])
    KAN_TEST_CHECK (migration_seed->field_remap[11u] == &migration_target_fields[11u])

    kan_reflection_struct_migrator_t source_to_target_migrator =
        kan_reflection_struct_migrator_build (source_to_target_migration_seed);

    struct migration_source_t first_migration_source = {
        .nesting_first =
            {
                .same = {.first = 42u, .second = SECOND_ENUM_SOURCE_SECOND, .third = 13u},
                .cross_copy = {.a = 1u, .b = 2u, .c = 3u, .d = 4u, .e = 5u, .f = 6u},
            },
        .nesting_to_drop =
            {
                .same = {.first = 11u, .second = SECOND_ENUM_SOURCE_FIRST, .third = 99u},
                .cross_copy = {.a = 0u, .b = 1u, .c = 2u, .d = 3u, .e = 4u, .f = 5u},
            },
        .nesting_second =
            {
                .same = {.first = 167u, .second = SECOND_ENUM_SOURCE_FIRST, .third = 255u},
                .cross_copy = {.a = 11u, .b = 15u, .c = 22u, .d = 34u, .e = 37u, .f = 99u},
            },
        .enums_array_one = {FIRST_ENUM_SOURCE_HELLO, FIRST_ENUM_SOURCE_WORLD, FIRST_ENUM_SOURCE_WORLD,
                            FIRST_ENUM_SOURCE_HELLO},
        .enums_array_two = {SECOND_ENUM_SOURCE_FIRST, SECOND_ENUM_SOURCE_THIRD, SECOND_ENUM_SOURCE_SECOND,
                            SECOND_ENUM_SOURCE_FIRST | SECOND_ENUM_SOURCE_SECOND | SECOND_ENUM_SOURCE_THIRD},
        .numeric_to_adapt = 177756u,
        .selector = 0u,
        .selection_first = FIRST_ENUM_SOURCE_WORLD,
        .interned_string = kan_string_intern ("Hello, world!"),
        .owned_string = "Let's think it is owned string.",
    };

    // Just the same, but with own dynamic array and other selector.
    struct migration_source_t second_migration_source = first_migration_source;
    second_migration_source.selector = 1u;
    second_migration_source.selection_second = SECOND_ENUM_SOURCE_SECOND | SECOND_ENUM_SOURCE_THIRD;

    kan_dynamic_array_init (&first_migration_source.dynamic_array, 4u, sizeof (int), _Alignof (int),
                            KAN_ALLOCATION_GROUP_IGNORE);
    *(enum first_enum_source_t *) kan_dynamic_array_add_last (&first_migration_source.dynamic_array) =
        FIRST_ENUM_SOURCE_HELLO;
    *(enum first_enum_source_t *) kan_dynamic_array_add_last (&first_migration_source.dynamic_array) =
        FIRST_ENUM_SOURCE_WORLD;

    kan_dynamic_array_init (&second_migration_source.dynamic_array, 4u, sizeof (int), _Alignof (int),
                            KAN_ALLOCATION_GROUP_IGNORE);
    *(enum first_enum_source_t *) kan_dynamic_array_add_last (&second_migration_source.dynamic_array) =
        FIRST_ENUM_SOURCE_WORLD;
    *(enum first_enum_source_t *) kan_dynamic_array_add_last (&second_migration_source.dynamic_array) =
        FIRST_ENUM_SOURCE_HELLO;

    struct migration_target_t first_migration_target = construct_empty_migration_target ();
    kan_reflection_struct_migrator_migrate_instance (source_to_target_migrator, migration_source.name,
                                                     &first_migration_source, &first_migration_target);

    check_generic_migration_result (&first_migration_source, &first_migration_target);
    KAN_TEST_CHECK (strcmp (first_migration_target.interned_string, "Hello, world!") == 0)
    KAN_TEST_CHECK (strcmp (first_migration_target.owned_string, "Let's think it is owned string.") == 0)

    struct migration_target_t second_migration_target = construct_empty_migration_target ();
    kan_reflection_struct_migrator_migrate_instance (source_to_target_migrator, migration_source.name,
                                                     &second_migration_source, &second_migration_target);

    check_generic_migration_result (&second_migration_source, &second_migration_target);
    KAN_TEST_CHECK (strcmp (second_migration_target.interned_string, "Hello, world!") == 0)
    KAN_TEST_CHECK (strcmp (second_migration_target.owned_string, "Let's think it is owned string.") == 0)

    kan_reflection_patch_builder_t patch_builder = kan_reflection_patch_builder_create ();

    kan_reflection_patch_builder_add_chunk (patch_builder, offsetof (struct migration_source_t, nesting_first),
                                            sizeof (struct nesting_source_t), &first_migration_source.nesting_first);
    kan_reflection_patch_builder_add_chunk (patch_builder, offsetof (struct migration_source_t, nesting_to_drop),
                                            sizeof (struct nesting_source_t), &first_migration_source.nesting_to_drop);
    kan_reflection_patch_builder_add_chunk (patch_builder, offsetof (struct migration_source_t, nesting_second),
                                            sizeof (struct nesting_source_t), &first_migration_source.nesting_second);
    kan_reflection_patch_builder_add_chunk (patch_builder, offsetof (struct migration_source_t, enums_array_one),
                                            sizeof (enum first_enum_target_t) * 4u,
                                            first_migration_source.enums_array_one);
    kan_reflection_patch_builder_add_chunk (patch_builder, offsetof (struct migration_source_t, numeric_to_adapt),
                                            sizeof (uint32_t), &first_migration_source.numeric_to_adapt);
    kan_reflection_patch_builder_add_chunk (patch_builder, offsetof (struct migration_source_t, selector),
                                            sizeof (uint64_t), &first_migration_source.selector);
    kan_reflection_patch_builder_add_chunk (patch_builder, offsetof (struct migration_source_t, selection_first),
                                            sizeof (enum first_enum_source_t), &first_migration_source.selection_first);
    kan_reflection_patch_t patch =
        kan_reflection_patch_builder_build (patch_builder, source_registry, &migration_source);

    kan_reflection_patch_builder_destroy (patch_builder);

    kan_reflection_struct_migrator_migrate_patches (source_to_target_migrator, source_registry, target_registry);
    struct migration_target_t patch_target = construct_empty_migration_target ();
    kan_reflection_patch_apply (patch, &patch_target);

    check_nesting_migration_result (&first_migration_source.nesting_first, &patch_target.nesting_first);
    check_nesting_migration_result (&first_migration_source.nesting_second, &patch_target.nesting_second);
    check_nesting_unchanged (&patch_target.nesting_to_add);

    check_first_enum_migration_result (first_migration_source.enums_array_one[0u], patch_target.enums_array_one[0u]);
    check_first_enum_migration_result (first_migration_source.enums_array_one[1u], patch_target.enums_array_one[1u]);
    check_first_enum_migration_result (first_migration_source.enums_array_one[2u], patch_target.enums_array_one[2u]);
    check_first_enum_migration_result (first_migration_source.enums_array_one[3u], patch_target.enums_array_one[3u]);

    KAN_TEST_CHECK ((uint64_t) first_migration_source.numeric_to_adapt == patch_target.numeric_to_adapt)
    KAN_TEST_CHECK (first_migration_source.selector == patch_target.selector)
    check_first_enum_migration_result (first_migration_source.selection_first, patch_target.selection_first);

    kan_dynamic_array_shutdown (&first_migration_source.dynamic_array);
    kan_dynamic_array_shutdown (&first_migration_target.dynamic_array);
    kan_dynamic_array_shutdown (&second_migration_source.dynamic_array);
    kan_dynamic_array_shutdown (&second_migration_target.dynamic_array);

    kan_reflection_struct_migrator_destroy (source_to_target_migrator);
    kan_reflection_migration_seed_destroy (source_to_target_migration_seed);
    kan_reflection_registry_destroy (source_registry);
    kan_reflection_registry_destroy (target_registry);
}

KAN_REFLECTION_EXPECT_UNIT_REGISTRAR (test_reflection);

KAN_TEST_CASE (generated_reflection)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_reflection) (registry);

    // We do not check everything here, because it would be just copying and pasting generated code.
    // We check only the main aspects.

    const struct kan_reflection_struct_t *vector3_data =
        kan_reflection_registry_query_struct (registry, kan_string_intern ("vector3_t"));
    KAN_TEST_ASSERT (vector3_data)
    KAN_TEST_CHECK (vector3_data->size == sizeof (struct vector3_t))
    KAN_TEST_CHECK (vector3_data->alignment == _Alignof (struct vector3_t))
    KAN_TEST_CHECK (!vector3_data->init)
    KAN_TEST_CHECK (!vector3_data->shutdown)
    KAN_TEST_CHECK (vector3_data->fields_count == 3u)

    const struct kan_reflection_struct_t *vector4_data =
        kan_reflection_registry_query_struct (registry, kan_string_intern ("vector4_t"));
    KAN_TEST_ASSERT (vector4_data)
    KAN_TEST_CHECK (vector4_data->size == sizeof (struct vector4_t))
    KAN_TEST_CHECK (vector4_data->alignment == _Alignof (struct vector4_t))
    KAN_TEST_CHECK (!vector4_data->init)
    KAN_TEST_CHECK (!vector4_data->shutdown)
    KAN_TEST_CHECK (vector4_data->fields_count == 4u)

    KAN_TEST_CHECK (!kan_reflection_registry_query_struct (registry, kan_string_intern ("ignored_t")))
    KAN_TEST_CHECK (!kan_reflection_registry_query_enum (registry, kan_string_intern ("ignored_enum_t")))

    const struct kan_reflection_enum_t *some_flags_data =
        kan_reflection_registry_query_enum (registry, kan_string_intern ("some_flags_t"));
    KAN_TEST_ASSERT (some_flags_data)
    KAN_TEST_CHECK (some_flags_data->flags)
    KAN_TEST_CHECK (some_flags_data->values_count == 2u)

    const struct kan_reflection_enum_t *some_enum_data =
        kan_reflection_registry_query_enum (registry, kan_string_intern ("some_enum_t"));
    KAN_TEST_ASSERT (some_enum_data)
    KAN_TEST_CHECK (!some_enum_data->flags)
    KAN_TEST_CHECK (some_enum_data->values_count == 2u)

    const struct kan_reflection_struct_t *first_component_data =
        kan_reflection_registry_query_struct (registry, kan_string_intern ("first_component_t"));
    KAN_TEST_ASSERT (first_component_data)
    KAN_TEST_CHECK (first_component_data->init)
    KAN_TEST_CHECK (first_component_data->shutdown)

    const struct kan_reflection_struct_t *second_component_data =
        kan_reflection_registry_query_struct (registry, kan_string_intern ("second_component_t"));
    KAN_TEST_ASSERT (second_component_data)
    KAN_TEST_CHECK (second_component_data->init)
    KAN_TEST_CHECK (second_component_data->shutdown)

    const struct kan_reflection_struct_t *a_bit_of_everything_data =
        kan_reflection_registry_query_struct (registry, kan_string_intern ("a_bit_of_everything_t"));
    KAN_TEST_ASSERT (a_bit_of_everything_data)
    KAN_TEST_ASSERT (a_bit_of_everything_data->fields_count == 16u)

    KAN_TEST_CHECK (a_bit_of_everything_data->fields[0u].name == kan_string_intern ("some_enum"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[0u].size == sizeof (int))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[0u].offset == offsetof (struct a_bit_of_everything_t, some_enum))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[0u].archetype == KAN_REFLECTION_ARCHETYPE_ENUM)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[0u].archetype_enum.type_name == kan_string_intern ("some_enum_t"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[0u].visibility_condition_field == NULL)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[0u].visibility_condition_values_count == 0u)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[0u].visibility_condition_values == NULL)

    KAN_TEST_CHECK (a_bit_of_everything_data->fields[1u].name == kan_string_intern ("some_flags"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[1u].size == sizeof (int))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[1u].offset == offsetof (struct a_bit_of_everything_t, some_flags))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[1u].archetype == KAN_REFLECTION_ARCHETYPE_ENUM)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[1u].archetype_enum.type_name == kan_string_intern ("some_flags_t"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[1u].visibility_condition_field == NULL)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[1u].visibility_condition_values_count == 0u)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[1u].visibility_condition_values == NULL)

    KAN_TEST_CHECK (a_bit_of_everything_data->fields[2u].name == kan_string_intern ("uint32"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[2u].size == sizeof (uint32_t))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[2u].offset == offsetof (struct a_bit_of_everything_t, uint32))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[2u].archetype == KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[2u].visibility_condition_field == NULL)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[2u].visibility_condition_values_count == 0u)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[2u].visibility_condition_values == NULL)

    KAN_TEST_CHECK (a_bit_of_everything_data->fields[3u].name == kan_string_intern ("int32"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[3u].size == sizeof (int32_t))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[3u].offset == offsetof (struct a_bit_of_everything_t, int32))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[3u].archetype == KAN_REFLECTION_ARCHETYPE_SIGNED_INT)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[3u].visibility_condition_field == NULL)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[3u].visibility_condition_values_count == 0u)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[3u].visibility_condition_values == NULL)

    KAN_TEST_CHECK (a_bit_of_everything_data->fields[4u].name == kan_string_intern ("count"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[5u].name == kan_string_intern ("bytes"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[5u].archetype == KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[5u].archetype_inline_array.item_archetype ==
                    KAN_REFLECTION_ARCHETYPE_SIGNED_INT)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[5u].archetype_inline_array.item_size == sizeof (int8_t))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[5u].archetype_inline_array.item_count == 128u)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[5u].archetype_inline_array.size_field ==
                    &a_bit_of_everything_data->fields[4u])

    KAN_TEST_CHECK (a_bit_of_everything_data->fields[6u].name == kan_string_intern ("selector"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[7u].name == kan_string_intern ("first_selection"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[7u].visibility_condition_field ==
                    &a_bit_of_everything_data->fields[6u])
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[7u].visibility_condition_values_count == 2u)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[7u].visibility_condition_values[0u] == 0)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[7u].visibility_condition_values[1u] == 2)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[8u].name == kan_string_intern ("second_selection"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[8u].visibility_condition_field ==
                    &a_bit_of_everything_data->fields[6u])
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[8u].visibility_condition_values_count == 1u)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[8u].visibility_condition_values[0u] == 1)

    KAN_TEST_CHECK (a_bit_of_everything_data->fields[9u].name == kan_string_intern ("owned_string"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[9u].archetype == KAN_REFLECTION_ARCHETYPE_STRING_POINTER)

    KAN_TEST_CHECK (a_bit_of_everything_data->fields[10u].name == kan_string_intern ("interned_string"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[10u].archetype == KAN_REFLECTION_ARCHETYPE_INTERNED_STRING)

    KAN_TEST_CHECK (a_bit_of_everything_data->fields[11u].name == kan_string_intern ("patch"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[11u].archetype == KAN_REFLECTION_ARCHETYPE_PATCH)

    KAN_TEST_CHECK (a_bit_of_everything_data->fields[12u].name == kan_string_intern ("first_external_pointer"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[12u].archetype == KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER)

    KAN_TEST_CHECK (a_bit_of_everything_data->fields[13u].name == kan_string_intern ("struct_pointer"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[13u].archetype == KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[13u].archetype_struct_pointer.type_name ==
                    kan_string_intern ("first_component_t"))

    KAN_TEST_CHECK (a_bit_of_everything_data->fields[14u].name == kan_string_intern ("second_external_pointer"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[14u].archetype == KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER)

    KAN_TEST_CHECK (a_bit_of_everything_data->fields[15u].name == kan_string_intern ("dynamic_array"))
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[15u].archetype = KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[15u].archetype_dynamic_array.item_archetype ==
                    KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER)
    KAN_TEST_CHECK (a_bit_of_everything_data->fields[15u].archetype_dynamic_array.item_size ==
                    sizeof (struct second_component_t *))
    KAN_TEST_CHECK (
        a_bit_of_everything_data->fields[15u].archetype_dynamic_array.item_archetype_struct_pointer.type_name ==
        kan_string_intern ("second_component_t"))

    struct kan_reflection_struct_field_meta_iterator_t iterator = kan_reflection_registry_query_struct_field_meta (
        registry, kan_string_intern ("first_component_t"), kan_string_intern ("position"),
        kan_string_intern ("network_meta_t"));

    KAN_TEST_CHECK (kan_reflection_struct_field_meta_iterator_get (&iterator))
    kan_reflection_struct_field_meta_iterator_next (&iterator);
    KAN_TEST_CHECK (!kan_reflection_struct_field_meta_iterator_get (&iterator))

    iterator = kan_reflection_registry_query_struct_field_meta (registry, kan_string_intern ("second_component_t"),
                                                                kan_string_intern ("velocity"),
                                                                kan_string_intern ("network_meta_t"));

    KAN_TEST_CHECK (kan_reflection_struct_field_meta_iterator_get (&iterator))
    kan_reflection_struct_field_meta_iterator_next (&iterator);
    KAN_TEST_CHECK (!kan_reflection_struct_field_meta_iterator_get (&iterator))

    const struct kan_reflection_function_t *vector3_add_data =
        kan_reflection_registry_query_function (registry, kan_string_intern ("vector3_add"));
    KAN_TEST_ASSERT (vector3_add_data)
    KAN_TEST_CHECK (vector3_add_data->name == kan_string_intern ("vector3_add"))
    KAN_TEST_CHECK (vector3_add_data->return_type.archetype == KAN_REFLECTION_ARCHETYPE_STRUCT)
    KAN_TEST_CHECK (vector3_add_data->return_type.size == sizeof (struct vector3_t))
    KAN_TEST_CHECK (vector3_add_data->return_type.archetype_struct.type_name == kan_string_intern ("vector3_t"))
    KAN_TEST_ASSERT (vector3_add_data->arguments_count == 2u)
    KAN_TEST_CHECK (vector3_add_data->arguments[0u].archetype == KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER)
    KAN_TEST_CHECK (vector3_add_data->arguments[0u].size == sizeof (void *))
    KAN_TEST_CHECK (vector3_add_data->arguments[0u].archetype_struct_pointer.type_name ==
                    kan_string_intern ("vector3_t"))
    KAN_TEST_CHECK (vector3_add_data->arguments[1u].archetype == KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER)
    KAN_TEST_CHECK (vector3_add_data->arguments[1u].size == sizeof (void *))
    KAN_TEST_CHECK (vector3_add_data->arguments[1u].archetype_struct_pointer.type_name ==
                    kan_string_intern ("vector3_t"))

    struct vector3_t first_vector3 = {1.0f, 2.0f, 3.0f};
    struct vector3_t second_vector3 = {11.0f, 19.0f, -2.0f};
    struct vector3_t *vector3_arguments[] = {&first_vector3, &second_vector3};
    struct vector3_t vector3_result;
    vector3_add_data->call (vector3_add_data->call_user_data, &vector3_result, &vector3_arguments);

    KAN_TEST_CHECK (vector3_result.x == first_vector3.x + second_vector3.x)
    KAN_TEST_CHECK (vector3_result.y == first_vector3.y + second_vector3.y)
    KAN_TEST_CHECK (vector3_result.z == first_vector3.z + second_vector3.z)

    const struct kan_reflection_function_t *vector4_add_data =
        kan_reflection_registry_query_function (registry, kan_string_intern ("vector4_add"));
    KAN_TEST_ASSERT (vector4_add_data)

    struct vector4_t first_vector4 = {1.0f, 2.0f, 3.0f, -4.0f};
    struct vector4_t second_vector4 = {11.0f, 19.0f, -2.0f, 10.0f};
    struct vector4_t *vector4_arguments[] = {&first_vector4, &second_vector4};
    struct vector4_t vector4_result;
    vector4_add_data->call (vector4_add_data->call_user_data, &vector4_result, &vector4_arguments);

    KAN_TEST_CHECK (vector4_result.x == first_vector4.x + second_vector4.x)
    KAN_TEST_CHECK (vector4_result.y == first_vector4.y + second_vector4.y)
    KAN_TEST_CHECK (vector4_result.z == first_vector4.z + second_vector4.z)
    KAN_TEST_CHECK (vector4_result.w == first_vector4.w + second_vector4.w)

    struct kan_reflection_function_meta_iterator_t function_meta_iterator =
        kan_reflection_registry_query_function_meta (registry, kan_string_intern ("vector3_add"),
                                                     kan_string_intern ("function_script_graph_meta_t"));
    KAN_TEST_CHECK (kan_reflection_function_meta_iterator_get (&function_meta_iterator))
    kan_reflection_function_meta_iterator_next (&function_meta_iterator);
    KAN_TEST_CHECK (!kan_reflection_function_meta_iterator_get (&function_meta_iterator))

    function_meta_iterator = kan_reflection_registry_query_function_meta (
        registry, kan_string_intern ("vector4_add"), kan_string_intern ("function_script_graph_meta_t"));
    KAN_TEST_CHECK (kan_reflection_function_meta_iterator_get (&function_meta_iterator))
    kan_reflection_function_meta_iterator_next (&function_meta_iterator);
    KAN_TEST_CHECK (!kan_reflection_function_meta_iterator_get (&function_meta_iterator))

    function_meta_iterator = kan_reflection_registry_query_function_meta (registry, kan_string_intern ("vector4_add"),
                                                                          kan_string_intern ("some_unknown_meta_t"));
    KAN_TEST_CHECK (!kan_reflection_function_meta_iterator_get (&function_meta_iterator))

    struct kan_reflection_function_argument_meta_iterator_t argument_meta_iterator =
        kan_reflection_registry_query_function_argument_meta (registry, kan_string_intern ("vector3_add"),
                                                              kan_string_intern ("first"),
                                                              kan_string_intern ("some_meta_t"));
    KAN_TEST_CHECK (!kan_reflection_function_argument_meta_iterator_get (&argument_meta_iterator));

    kan_reflection_registry_destroy (registry);
}
