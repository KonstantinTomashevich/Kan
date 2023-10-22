#include <memory.h>
#include <stddef.h>

#include <kan/reflection/field_visibility_iterator.h>
#include <kan/reflection/patch.h>
#include <kan/reflection/registry.h>
#include <kan/testing/testing.h>

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
        sizeof (second_struct_fields) / sizeof (struct kan_reflection_field_t),
        second_struct_fields,
    };

    struct example_struct_meta_assembly_t second_struct_assembly = {KAN_FALSE};
    struct example_universal_meta_editor_t second_struct_editor = {"struct_second_t", "description", KAN_FALSE};

    struct example_field_meta_min_max_t first_struct_first_min_max = {-10, 10};
    struct example_field_meta_min_max_t second_struct_second_min_max = {0u, 16u};

    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_TEST_CHECK (!kan_reflection_registry_query_enum (registry, first_enum.name))
    KAN_TEST_CHECK (!kan_reflection_registry_query_enum (registry, second_enum.name))
    KAN_TEST_CHECK (!kan_reflection_registry_query_struct (registry, first_struct.name))
    KAN_TEST_CHECK (!kan_reflection_registry_query_struct (registry, second_struct.name))

    KAN_TEST_CHECK (kan_reflection_registry_add_enum_meta (registry, kan_string_intern ("first_t"),
                                                           kan_string_intern ("example_enum_meta_serialization_t"),
                                                           &first_enum_serialization))
    KAN_TEST_CHECK (kan_reflection_registry_add_enum (registry, &first_enum))
    KAN_TEST_CHECK (kan_reflection_registry_add_enum_meta (
        registry, first_enum.name, kan_string_intern ("example_universal_meta_editor_t"), &first_enum_editor))

    KAN_TEST_CHECK (kan_reflection_registry_add_enum_meta (
        registry, second_enum.name, kan_string_intern ("example_universal_meta_editor_t"), &second_enum_editor))
    KAN_TEST_CHECK (kan_reflection_registry_add_enum (registry, &second_enum))
    KAN_TEST_CHECK (kan_reflection_registry_add_enum_meta (registry, kan_string_intern ("second_t"),
                                                           kan_string_intern ("example_enum_meta_serialization_t"),
                                                           &second_enum_serialization))

    KAN_TEST_CHECK (kan_reflection_registry_add_struct_meta (registry, kan_string_intern ("struct_first_t"),
                                                             kan_string_intern ("example_struct_meta_assembly_t"),
                                                             &first_struct_assembly))
    KAN_TEST_CHECK (kan_reflection_registry_add_struct_field_meta (
        registry, first_struct.name, first_struct_fields[0u].name, kan_string_intern ("example_field_meta_min_max_t"),
        &first_struct_first_min_max))
    KAN_TEST_CHECK (kan_reflection_registry_add_struct (registry, &first_struct))
    KAN_TEST_CHECK (kan_reflection_registry_add_struct_meta (
        registry, first_struct.name, kan_string_intern ("example_universal_meta_editor_t"), &first_struct_editor))

    KAN_TEST_CHECK (kan_reflection_registry_add_struct_meta (
        registry, second_struct.name, kan_string_intern ("example_universal_meta_editor_t"), &second_struct_editor))
    KAN_TEST_CHECK (kan_reflection_registry_add_struct (registry, &second_struct))
    KAN_TEST_CHECK (kan_reflection_registry_add_struct_meta (registry, kan_string_intern ("struct_second_t"),
                                                             kan_string_intern ("example_struct_meta_assembly_t"),
                                                             &second_struct_assembly))
    KAN_TEST_CHECK (kan_reflection_registry_add_struct_field_meta (
        registry, second_struct.name, second_struct_fields[1u].name, kan_string_intern ("example_field_meta_min_max_t"),
        &second_struct_second_min_max))

    KAN_TEST_CHECK (kan_reflection_registry_query_enum (registry, first_enum.name) == &first_enum)
    KAN_TEST_CHECK (kan_reflection_registry_query_enum_meta (registry, first_enum.name,
                                                             kan_string_intern ("example_enum_meta_serialization_t")) ==
                    &first_enum_serialization)
    KAN_TEST_CHECK (kan_reflection_registry_query_enum_meta (registry, first_enum.name,
                                                             kan_string_intern ("example_universal_meta_editor_t")) ==
                    &first_enum_editor)
    KAN_TEST_CHECK (kan_reflection_registry_query_enum_meta (registry, first_enum.name,
                                                             kan_string_intern ("there_is_no_such_meta")) == NULL)

    KAN_TEST_CHECK (kan_reflection_registry_query_enum (registry, second_enum.name) == &second_enum)
    KAN_TEST_CHECK (kan_reflection_registry_query_enum_meta (registry, second_enum.name,
                                                             kan_string_intern ("example_enum_meta_serialization_t")) ==
                    &second_enum_serialization)
    KAN_TEST_CHECK (kan_reflection_registry_query_enum_meta (registry, second_enum.name,
                                                             kan_string_intern ("example_universal_meta_editor_t")) ==
                    &second_enum_editor)
    KAN_TEST_CHECK (kan_reflection_registry_query_enum_meta (registry, second_enum.name,
                                                             kan_string_intern ("there_is_no_such_meta")) == NULL)
    KAN_TEST_CHECK (kan_reflection_registry_query_enum_value_meta (registry, second_enum.name,
                                                                   second_enum_values[1u].name,
                                                                   kan_string_intern ("there_is_no_such_meta")) == NULL)

    KAN_TEST_CHECK (kan_reflection_registry_query_struct (registry, first_struct.name) == &first_struct)
    KAN_TEST_CHECK (kan_reflection_registry_query_struct_meta (registry, first_struct.name,
                                                               kan_string_intern ("example_struct_meta_assembly_t")) ==
                    &first_struct_assembly)
    KAN_TEST_CHECK (kan_reflection_registry_query_struct_meta (registry, first_struct.name,
                                                               kan_string_intern ("example_universal_meta_editor_t")) ==
                    &first_struct_editor)
    KAN_TEST_CHECK (kan_reflection_registry_query_struct_meta (registry, first_struct.name,
                                                               kan_string_intern ("there_is_no_such_meta")) == NULL)

    KAN_TEST_CHECK (kan_reflection_registry_query_struct (registry, second_struct.name) == &second_struct)
    KAN_TEST_CHECK (kan_reflection_registry_query_struct_meta (registry, second_struct.name,
                                                               kan_string_intern ("example_struct_meta_assembly_t")) ==
                    &second_struct_assembly)
    KAN_TEST_CHECK (kan_reflection_registry_query_struct_meta (registry, second_struct.name,
                                                               kan_string_intern ("example_universal_meta_editor_t")) ==
                    &second_struct_editor)
    KAN_TEST_CHECK (kan_reflection_registry_query_struct_meta (registry, second_struct.name,
                                                               kan_string_intern ("there_is_no_such_meta")) == NULL)

    KAN_TEST_CHECK (kan_reflection_registry_query_struct_field_meta (
                        registry, first_struct.name, first_struct_fields[0u].name,
                        kan_string_intern ("example_field_meta_min_max_t")) == &first_struct_first_min_max)
    KAN_TEST_CHECK (
        kan_reflection_registry_query_struct_field_meta (registry, first_struct.name, first_struct_fields[1u].name,
                                                         kan_string_intern ("example_field_meta_min_max_t")) == NULL)

    KAN_TEST_CHECK (
        kan_reflection_registry_query_struct_field_meta (registry, second_struct.name, second_struct_fields[0u].name,
                                                         kan_string_intern ("example_field_meta_min_max_t")) == NULL)
    KAN_TEST_CHECK (kan_reflection_registry_query_struct_field_meta (
                        registry, second_struct.name, second_struct_fields[1u].name,
                        kan_string_intern ("example_field_meta_min_max_t")) == &second_struct_second_min_max)

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
            .offset = sizeof (void *),
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
        sizeof (second_struct_fields) / sizeof (struct kan_reflection_field_t),
        second_struct_fields,
    };

    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_TEST_CHECK (kan_reflection_registry_add_struct (registry, &first_struct));
    KAN_TEST_CHECK (kan_reflection_registry_add_struct (registry, &second_struct));

    uint64_t absolute_offset;
    kan_interned_string_t first_first_path[] = {first_struct.name, first_struct_fields[0u].name};
    KAN_TEST_CHECK (kan_reflection_registry_query_local_field (registry, 2u, first_first_path, &absolute_offset) ==
                    &first_struct_fields[0u])
    KAN_TEST_CHECK (absolute_offset == first_struct_fields[0u].offset)

    kan_interned_string_t first_second_path[] = {first_struct.name, first_struct_fields[1u].name};
    KAN_TEST_CHECK (kan_reflection_registry_query_local_field (registry, 2u, first_second_path, &absolute_offset) ==
                    &first_struct_fields[1u])
    KAN_TEST_CHECK (absolute_offset == first_struct_fields[1u].offset)

    kan_interned_string_t first_unknown_path[] = {first_struct.name, kan_string_intern ("unknown")};
    KAN_TEST_CHECK (kan_reflection_registry_query_local_field (registry, 2u, first_unknown_path, &absolute_offset) ==
                    NULL)
    KAN_TEST_CHECK (absolute_offset == 0u)

    kan_interned_string_t second_second_third_path[] = {second_struct.name, second_struct_fields[1u].name,
                                                        first_struct_fields[2u].name};
    KAN_TEST_CHECK (kan_reflection_registry_query_local_field (registry, 3u, second_second_third_path,
                                                               &absolute_offset) == &first_struct_fields[2u])
    KAN_TEST_CHECK (absolute_offset == second_struct_fields[1u].offset + first_struct_fields[2u].offset)

    kan_interned_string_t second_first_third_path[] = {second_struct.name, second_struct_fields[0u].name,
                                                       first_struct_fields[2u].name};
    KAN_TEST_CHECK (
        kan_reflection_registry_query_local_field (registry, 3u, second_first_third_path, &absolute_offset) == NULL)
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
    int64_t first_switch_values[] = {-1, 1};
    int64_t second_switch_values[] = {-2, 2};

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
                 .items_count = sizeof (((struct patch_outer_t *) NULL)->inner) / sizeof (struct patch_inner_t),
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
    KAN_TEST_ASSERT (first_to_second != KAN_REFLECTION_INVALID_PATCH)

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
    KAN_TEST_ASSERT (second_to_third != KAN_REFLECTION_INVALID_PATCH)
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

    KAN_TEST_ASSERT (iterator != end)
    struct kan_reflection_patch_chunk_info_t chunk = kan_reflection_patch_iterator_get (iterator);
    KAN_TEST_CHECK (chunk.offset == 0u)
    KAN_TEST_CHECK (chunk.size == sizeof (double) + sizeof (struct patch_inner_t))
    iterator = kan_reflection_patch_iterator_next (iterator);
    KAN_TEST_CHECK (iterator == end)

    iterator = kan_reflection_patch_begin (second_to_third);
    end = kan_reflection_patch_end (second_to_third);

    KAN_TEST_ASSERT (iterator != end)
    chunk = kan_reflection_patch_iterator_get (iterator);
    KAN_TEST_CHECK (chunk.offset == sizeof (double) + sizeof (struct patch_inner_t))
    KAN_TEST_CHECK (chunk.size == sizeof (double) + sizeof (struct patch_inner_t))
    iterator = kan_reflection_patch_iterator_next (iterator);
    KAN_TEST_CHECK (iterator == end)

    // Patches will be automatically destroyed with owning registry.
    kan_reflection_registry_destroy (registry);
}
