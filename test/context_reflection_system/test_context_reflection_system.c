#include <test_context_reflection_system_api.h>

#include <stddef.h>

#include <kan/context/reflection_system.h>
#include <kan/memory/allocation.h>
#include <kan/testing/testing.h>

struct first_static_struct_t
{
    int a;
    int b;
};

struct second_static_struct_t
{
    float x;
    float y;
};

enum static_enum_t
{
    STATIC_ENUM_A = 0,
    STATIC_ENUM_B,
};

TEST_CONTEXT_REFLECTION_SYSTEM_API void static_function (int a, int b, int c)
{
}

struct check_generated_config_t
{
    uint64_t enums_to_check_count;
    kan_interned_string_t *enums_to_check;
    uint64_t structs_to_check_count;
    kan_interned_string_t *structs_to_check;
    uint64_t functions_to_check_count;
    kan_interned_string_t *functions_to_check;
};

struct check_generated_system_t
{
    kan_context_handle_t context;
    kan_allocation_group_t group;
    uint64_t generated_calls_count;
    struct check_generated_config_t *config;
};

TEST_CONTEXT_REFLECTION_SYSTEM_API kan_context_system_handle_t
check_generated_system_create (kan_allocation_group_t group, void *user_config)
{
    struct check_generated_system_t *system = kan_allocate_general (group, sizeof (struct check_generated_system_t),
                                                                    _Alignof (struct check_generated_system_t));
    system->group = group;
    system->generated_calls_count = 0u;
    system->config = (struct check_generated_config_t *) user_config;
    return (kan_context_system_handle_t) system;
}

static void check_generated_system_received_reflection (kan_context_system_handle_t handle,
                                                        kan_reflection_registry_t registry,
                                                        kan_reflection_migration_seed_t migration_seed,
                                                        kan_reflection_struct_migrator_t migrator)
{
    struct check_generated_system_t *system = (struct check_generated_system_t *) handle;
    ++system->generated_calls_count;

    for (uint64_t index = 0u; index < system->config->enums_to_check_count; ++index)
    {
        const struct kan_reflection_enum_t *enum_data =
            kan_reflection_registry_query_enum (registry, system->config->enums_to_check[index]);
        KAN_TEST_CHECK (enum_data)
    }

    for (uint64_t index = 0u; index < system->config->structs_to_check_count; ++index)
    {
        const struct kan_reflection_struct_t *struct_data =
            kan_reflection_registry_query_struct (registry, system->config->structs_to_check[index]);
        KAN_TEST_CHECK (struct_data)
    }

    for (uint64_t index = 0u; index < system->config->functions_to_check_count; ++index)
    {
        const struct kan_reflection_function_t *function_data =
            kan_reflection_registry_query_function (registry, system->config->functions_to_check[index]);
        KAN_TEST_CHECK (function_data)
    }
}

TEST_CONTEXT_REFLECTION_SYSTEM_API void check_generated_system_connect (kan_context_system_handle_t handle,
                                                                        kan_context_handle_t context)
{
    struct check_generated_system_t *system = (struct check_generated_system_t *) handle;
    system->context = context;

    kan_context_system_handle_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);

    if (reflection_system == KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        return;
    }

    kan_reflection_system_connect_on_generated (reflection_system, handle, check_generated_system_received_reflection);
}

TEST_CONTEXT_REFLECTION_SYSTEM_API void check_generated_system_init (kan_context_system_handle_t handle)
{
}

TEST_CONTEXT_REFLECTION_SYSTEM_API void check_generated_system_shutdown (kan_context_system_handle_t handle)
{
    struct check_generated_system_t *system = (struct check_generated_system_t *) handle;
    KAN_TEST_CHECK (system->generated_calls_count == 1u)
}

TEST_CONTEXT_REFLECTION_SYSTEM_API void check_generated_system_disconnect (kan_context_system_handle_t handle)
{
    struct check_generated_system_t *system = (struct check_generated_system_t *) handle;
    kan_context_system_handle_t reflection_system =
        kan_context_query (system->context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);

    if (reflection_system == KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        return;
    }

    kan_reflection_system_disconnect_on_generated (reflection_system, handle);
}

TEST_CONTEXT_REFLECTION_SYSTEM_API void check_generated_system_destroy (kan_context_system_handle_t handle)
{
    struct check_generated_system_t *system = (struct check_generated_system_t *) handle;
    kan_free_general (system->group, system, sizeof (struct check_generated_system_t));
}

// \c_interface_scanner_disable
TEST_CONTEXT_REFLECTION_SYSTEM_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (
    check_generated_system_t) = {
    .name = "check_generated_system_t",
    .create = check_generated_system_create,
    .connect = check_generated_system_connect,
    .connected_init = check_generated_system_init,
    .connected_shutdown = check_generated_system_shutdown,
    .disconnect = check_generated_system_disconnect,
    .destroy = check_generated_system_destroy,
};
// \c_interface_scanner_enable

struct check_populate_system_t
{
    kan_context_handle_t context;
    kan_allocation_group_t group;
    uint64_t populate_calls_count;
};

TEST_CONTEXT_REFLECTION_SYSTEM_API kan_context_system_handle_t
check_populate_system_create (kan_allocation_group_t group, void *user_config)
{
    struct check_populate_system_t *system = kan_allocate_general (group, sizeof (struct check_populate_system_t),
                                                                   _Alignof (struct check_populate_system_t));
    system->group = group;
    system->populate_calls_count = 0u;
    return (kan_context_system_handle_t) system;
}

static struct kan_reflection_field_t populate_test_struct_fields[2u];
static struct kan_reflection_struct_t populate_test_struct;

static void check_populate_system_populate (kan_context_system_handle_t handle, kan_reflection_registry_t registry)
{
    struct check_populate_system_t *system = (struct check_populate_system_t *) handle;
    ++system->populate_calls_count;

    populate_test_struct = (struct kan_reflection_struct_t) {
        .name = kan_string_intern ("populate_test_struct_t"),
        .size = 16u,
        .alignment = 8u,
        .init = NULL,
        .shutdown = NULL,
        .functor_user_data = 0u,
        .fields_count = 2u,
        .fields = populate_test_struct_fields,
    };

    populate_test_struct_fields[0u] = (struct kan_reflection_field_t) {
        .name = kan_string_intern ("a"),
        .offset = 0u,
        .size = 8u,
        .archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT,
        .visibility_condition_field = NULL,
        .visibility_condition_values_count = 0u,
        .visibility_condition_values = NULL,
    };

    populate_test_struct_fields[1u] = (struct kan_reflection_field_t) {
        .name = kan_string_intern ("b"),
        .offset = 8u,
        .size = 8u,
        .archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT,
        .visibility_condition_field = NULL,
        .visibility_condition_values_count = 0u,
        .visibility_condition_values = NULL,
    };

    kan_reflection_registry_add_struct (registry, &populate_test_struct);
}

TEST_CONTEXT_REFLECTION_SYSTEM_API void check_populate_system_connect (kan_context_system_handle_t handle,
                                                                       kan_context_handle_t context)
{
    struct check_populate_system_t *system = (struct check_populate_system_t *) handle;
    system->context = context;

    kan_context_system_handle_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);

    if (reflection_system == KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        return;
    }

    kan_reflection_system_connect_on_populate (reflection_system, handle, check_populate_system_populate);
}

TEST_CONTEXT_REFLECTION_SYSTEM_API void check_populate_system_init (kan_context_system_handle_t handle)
{
}

TEST_CONTEXT_REFLECTION_SYSTEM_API void check_populate_system_shutdown (kan_context_system_handle_t handle)
{
    struct check_populate_system_t *system = (struct check_populate_system_t *) handle;
    KAN_TEST_CHECK (system->populate_calls_count == 1u)
}

TEST_CONTEXT_REFLECTION_SYSTEM_API void check_populate_system_disconnect (kan_context_system_handle_t handle)
{
    struct check_populate_system_t *system = (struct check_populate_system_t *) handle;
    kan_context_system_handle_t reflection_system =
        kan_context_query (system->context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);

    if (reflection_system == KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        return;
    }

    kan_reflection_system_disconnect_on_populate (reflection_system, handle);
}

TEST_CONTEXT_REFLECTION_SYSTEM_API void check_populate_system_destroy (kan_context_system_handle_t handle)
{
    struct check_populate_system_t *system = (struct check_populate_system_t *) handle;
    kan_free_general (system->group, system, sizeof (struct check_populate_system_t));
}

// \c_interface_scanner_disable
TEST_CONTEXT_REFLECTION_SYSTEM_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (
    check_populate_system_t) = {
    .name = "check_populate_system_t",
    .create = check_populate_system_create,
    .connect = check_populate_system_connect,
    .connected_init = check_populate_system_init,
    .connected_shutdown = check_populate_system_shutdown,
    .disconnect = check_populate_system_disconnect,
    .destroy = check_populate_system_destroy,
};
// \c_interface_scanner_enable

struct check_generation_iterate_system_t
{
    kan_context_handle_t context;
    kan_allocation_group_t group;
    uint64_t generation_last_iteration;
};

TEST_CONTEXT_REFLECTION_SYSTEM_API kan_context_system_handle_t
check_generation_iterate_system_create (kan_allocation_group_t group, void *user_config)
{
    struct check_generation_iterate_system_t *system = kan_allocate_general (
        group, sizeof (struct check_generation_iterate_system_t), _Alignof (struct check_generation_iterate_system_t));
    system->group = group;
    system->generation_last_iteration = 0u;
    return (kan_context_system_handle_t) system;
}

static struct kan_reflection_field_t generation_iterate_test_struct_fields[2u];
static struct kan_reflection_struct_t generation_iterate_test_struct;

static void check_generation_iterate_system_generation_iterate (kan_context_system_handle_t handle,
                                                                kan_reflection_registry_t registry,
                                                                kan_reflection_system_generation_iterator_t iterator,
                                                                uint64_t iteration_index)
{
    struct check_generation_iterate_system_t *system = (struct check_generation_iterate_system_t *) handle;
    system->generation_last_iteration = iteration_index;

    if (iteration_index == 0u)
    {
        generation_iterate_test_struct = (struct kan_reflection_struct_t) {
            .name = kan_string_intern ("generation_iterate_test_struct_t"),
            .size = 16u,
            .alignment = 8u,
            .init = NULL,
            .shutdown = NULL,
            .functor_user_data = 0u,
            .fields_count = 2u,
            .fields = generation_iterate_test_struct_fields,
        };

        generation_iterate_test_struct_fields[0u] = (struct kan_reflection_field_t) {
            .name = kan_string_intern ("x"),
            .offset = 0u,
            .size = 8u,
            .archetype = KAN_REFLECTION_ARCHETYPE_FLOATING,
            .visibility_condition_field = NULL,
            .visibility_condition_values_count = 0u,
            .visibility_condition_values = NULL,
        };

        generation_iterate_test_struct_fields[1u] = (struct kan_reflection_field_t) {
            .name = kan_string_intern ("y"),
            .offset = 8u,
            .size = 8u,
            .archetype = KAN_REFLECTION_ARCHETYPE_FLOATING,
            .visibility_condition_field = NULL,
            .visibility_condition_values_count = 0u,
            .visibility_condition_values = NULL,
        };

        kan_reflection_system_generation_iterator_add_struct (iterator, &generation_iterate_test_struct);
    }
    else
    {
        KAN_TEST_CHECK (kan_reflection_system_generation_iterator_next_added_struct (iterator) ==
                        kan_string_intern ("generation_iterate_test_struct_t"));
        KAN_TEST_CHECK (kan_reflection_system_generation_iterator_next_added_struct (iterator) == NULL);
    }
}

TEST_CONTEXT_REFLECTION_SYSTEM_API void check_generation_iterate_system_connect (kan_context_system_handle_t handle,
                                                                                 kan_context_handle_t context)
{
    struct check_generation_iterate_system_t *system = (struct check_generation_iterate_system_t *) handle;
    system->context = context;

    kan_context_system_handle_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);

    if (reflection_system == KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        return;
    }

    kan_reflection_system_connect_on_generation_iterate (reflection_system, handle,
                                                         check_generation_iterate_system_generation_iterate);
}

TEST_CONTEXT_REFLECTION_SYSTEM_API void check_generation_iterate_system_init (kan_context_system_handle_t handle)
{
}

TEST_CONTEXT_REFLECTION_SYSTEM_API void check_generation_iterate_system_shutdown (kan_context_system_handle_t handle)
{
    struct check_generation_iterate_system_t *system = (struct check_generation_iterate_system_t *) handle;
    KAN_TEST_CHECK (system->generation_last_iteration == 1u)
}

TEST_CONTEXT_REFLECTION_SYSTEM_API void check_generation_iterate_system_disconnect (kan_context_system_handle_t handle)
{
    struct check_generation_iterate_system_t *system = (struct check_generation_iterate_system_t *) handle;
    kan_context_system_handle_t reflection_system =
        kan_context_query (system->context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);

    if (reflection_system == KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        return;
    }

    kan_reflection_system_disconnect_on_generation_iterate (reflection_system, handle);
}

TEST_CONTEXT_REFLECTION_SYSTEM_API void check_generation_iterate_system_destroy (kan_context_system_handle_t handle)
{
    struct check_generation_iterate_system_t *system = (struct check_generation_iterate_system_t *) handle;
    kan_free_general (system->group, system, sizeof (struct check_generation_iterate_system_t));
}

// \c_interface_scanner_disable
TEST_CONTEXT_REFLECTION_SYSTEM_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (
    check_generation_iterate_system_t) = {
    .name = "check_generation_iterate_system_t",
    .create = check_generation_iterate_system_create,
    .connect = check_generation_iterate_system_connect,
    .connected_init = check_generation_iterate_system_init,
    .connected_shutdown = check_generation_iterate_system_shutdown,
    .disconnect = check_generation_iterate_system_disconnect,
    .destroy = check_generation_iterate_system_destroy,
};
// \c_interface_scanner_enable

KAN_TEST_CASE (only_statics)
{
    kan_context_handle_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);

    struct check_generated_config_t check_config = {
        .enums_to_check_count = 1u,
        .enums_to_check = (kan_interned_string_t[]) {kan_string_intern ("static_enum_t")},
        .structs_to_check_count = 2u,
        .structs_to_check = (kan_interned_string_t[]) {kan_string_intern ("first_static_struct_t"),
                                                       kan_string_intern ("second_static_struct_t")},
        .functions_to_check_count = 1u,
        .functions_to_check = (kan_interned_string_t[]) {kan_string_intern ("static_function")},
    };

    KAN_TEST_CHECK (kan_context_request_system (context, "check_generated_system_t", &check_config))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    kan_context_assembly (context);
    kan_context_destroy (context);
}

KAN_TEST_CASE (check_populate)
{
    kan_context_handle_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);

    struct check_generated_config_t check_config = {
        .enums_to_check_count = 1u,
        .enums_to_check = (kan_interned_string_t[]) {kan_string_intern ("static_enum_t")},
        .structs_to_check_count = 3u,
        .structs_to_check = (kan_interned_string_t[]) {kan_string_intern ("first_static_struct_t"),
                                                       kan_string_intern ("second_static_struct_t"),
                                                       kan_string_intern ("populate_test_struct_t")},
        .functions_to_check_count = 1u,
        .functions_to_check = (kan_interned_string_t[]) {kan_string_intern ("static_function")},
    };

    KAN_TEST_CHECK (kan_context_request_system (context, "check_generated_system_t", &check_config))
    KAN_TEST_CHECK (kan_context_request_system (context, "check_populate_system_t", NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    kan_context_assembly (context);
    kan_context_destroy (context);
}

KAN_TEST_CASE (check_iterate)
{
    kan_context_handle_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);

    struct check_generated_config_t check_config = {
        .enums_to_check_count = 1u,
        .enums_to_check = (kan_interned_string_t[]) {kan_string_intern ("static_enum_t")},
        .structs_to_check_count = 3u,
        .structs_to_check = (kan_interned_string_t[]) {kan_string_intern ("first_static_struct_t"),
                                                       kan_string_intern ("second_static_struct_t"),
                                                       kan_string_intern ("generation_iterate_test_struct_t")},
        .functions_to_check_count = 1u,
        .functions_to_check = (kan_interned_string_t[]) {kan_string_intern ("static_function")},
    };

    KAN_TEST_CHECK (kan_context_request_system (context, "check_generated_system_t", &check_config))
    KAN_TEST_CHECK (kan_context_request_system (context, "check_generation_iterate_system_t", NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    kan_context_assembly (context);
    kan_context_destroy (context);
}

KAN_TEST_CASE (combined)
{
    kan_context_handle_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);

    struct check_generated_config_t check_config = {
        .enums_to_check_count = 1u,
        .enums_to_check = (kan_interned_string_t[]) {kan_string_intern ("static_enum_t")},
        .structs_to_check_count = 4u,
        .structs_to_check = (kan_interned_string_t[]) {kan_string_intern ("first_static_struct_t"),
                                                       kan_string_intern ("second_static_struct_t"),
                                                       kan_string_intern ("populate_test_struct_t"),
                                                       kan_string_intern ("generation_iterate_test_struct_t")},
        .functions_to_check_count = 1u,
        .functions_to_check = (kan_interned_string_t[]) {kan_string_intern ("static_function")},
    };

    KAN_TEST_CHECK (kan_context_request_system (context, "check_generated_system_t", &check_config))
    KAN_TEST_CHECK (kan_context_request_system (context, "check_populate_system_t", NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, "check_generation_iterate_system_t", NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    kan_context_assembly (context);
    kan_context_destroy (context);
}
