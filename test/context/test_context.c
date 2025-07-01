#include <test_context_api.h>

#include <kan/context/context.h>
#include <kan/memory/allocation.h>
#include <kan/testing/testing.h>

struct first_independent_system_t
{
    kan_context_t context;
    kan_allocation_group_t group;
    bool initialized;
    bool second_connected;
};

TEST_CONTEXT_API kan_context_system_t first_independent_system_create (kan_allocation_group_t group, void *user_config)
{
    struct first_independent_system_t *system = kan_allocate_general (group, sizeof (struct first_independent_system_t),
                                                                      alignof (struct first_independent_system_t));
    system->group = group;
    system->initialized = false;
    system->second_connected = false;
    return KAN_HANDLE_SET (kan_context_system_t, system);
}

TEST_CONTEXT_API void first_independent_system_connect (kan_context_system_t handle, kan_context_t context)
{
    struct first_independent_system_t *system = KAN_HANDLE_GET (handle);
    system->context = context;
    KAN_TEST_CHECK (!system->initialized)
}

TEST_CONTEXT_API void first_independent_system_init (kan_context_system_t handle)
{
    struct first_independent_system_t *system = KAN_HANDLE_GET (handle);
    KAN_TEST_CHECK (!system->initialized)
    system->initialized = true;
}

TEST_CONTEXT_API void first_independent_system_shutdown (kan_context_system_t handle)
{
    struct first_independent_system_t *system = KAN_HANDLE_GET (handle);
    KAN_TEST_CHECK (system->initialized)
    system->initialized = false;
}

TEST_CONTEXT_API void first_independent_system_disconnect (kan_context_system_t handle)
{
    struct first_independent_system_t *system = KAN_HANDLE_GET (handle);
    KAN_TEST_CHECK (!system->initialized)
}

TEST_CONTEXT_API void first_independent_system_destroy (kan_context_system_t handle)
{
    struct first_independent_system_t *system = KAN_HANDLE_GET (handle);
    KAN_TEST_CHECK (!system->second_connected)
    kan_free_general (system->group, system, sizeof (struct first_independent_system_t));
}

TEST_CONTEXT_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (first_independent_system_t) = {
    .name = "first_independent_system_t",
    .create = first_independent_system_create,
    .connect = first_independent_system_connect,
    .connected_init = first_independent_system_init,
    .connected_shutdown = first_independent_system_shutdown,
    .disconnect = first_independent_system_disconnect,
    .destroy = first_independent_system_destroy,
};

struct second_independent_system_t
{
    kan_context_t context;
    kan_allocation_group_t group;
    bool initialized;
};

TEST_CONTEXT_API kan_context_system_t second_independent_system_create (kan_allocation_group_t group, void *user_config)
{
    struct second_independent_system_t *system = kan_allocate_general (
        group, sizeof (struct second_independent_system_t), alignof (struct second_independent_system_t));
    system->group = group;
    system->initialized = false;
    return KAN_HANDLE_SET (kan_context_system_t, system);
}

TEST_CONTEXT_API void second_independent_system_connect (kan_context_system_t handle, kan_context_t context)
{
    struct second_independent_system_t *system = KAN_HANDLE_GET (handle);
    system->context = context;

    struct first_independent_system_t *first =
        KAN_HANDLE_GET (kan_context_query (context, "first_independent_system_t"));

    if (first)
    {
        first->second_connected = true;
    }
}

TEST_CONTEXT_API void second_independent_system_init (kan_context_system_t handle)
{
    struct second_independent_system_t *system = KAN_HANDLE_GET (handle);
    system->initialized = true;
}

TEST_CONTEXT_API void second_independent_system_shutdown (kan_context_system_t handle)
{
    struct second_independent_system_t *system = KAN_HANDLE_GET (handle);
    system->initialized = false;
}

TEST_CONTEXT_API void second_independent_system_disconnect (kan_context_system_t handle)
{
    struct second_independent_system_t *system = KAN_HANDLE_GET (handle);
    struct first_independent_system_t *first =
        KAN_HANDLE_GET (kan_context_query (system->context, "first_independent_system_t"));

    if (first)
    {
        first->second_connected = false;
    }
}

TEST_CONTEXT_API void second_independent_system_destroy (kan_context_system_t handle)
{
    struct second_independent_system_t *system = KAN_HANDLE_GET (handle);
    kan_free_general (system->group, system, sizeof (struct second_independent_system_t));
}

TEST_CONTEXT_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (second_independent_system_t) = {
    .name = "second_independent_system_t",
    .create = second_independent_system_create,
    .connect = second_independent_system_connect,
    .connected_init = second_independent_system_init,
    .connected_shutdown = second_independent_system_shutdown,
    .disconnect = second_independent_system_disconnect,
    .destroy = second_independent_system_destroy,
};

struct system_with_dependencies_t
{
    kan_context_t context;
    kan_allocation_group_t group;
    bool initialized;
    bool first_used;
    bool second_used;
};

TEST_CONTEXT_API kan_context_system_t system_with_dependencies_create (kan_allocation_group_t group, void *user_config)
{
    struct system_with_dependencies_t *system = kan_allocate_general (group, sizeof (struct system_with_dependencies_t),
                                                                      alignof (struct system_with_dependencies_t));
    system->group = group;
    system->initialized = false;
    system->first_used = false;
    system->second_used = false;
    return KAN_HANDLE_SET (kan_context_system_t, system);
}

TEST_CONTEXT_API void system_with_dependencies_connect (kan_context_system_t handle, kan_context_t context)
{
    struct system_with_dependencies_t *system = KAN_HANDLE_GET (handle);
    system->context = context;
}

TEST_CONTEXT_API void system_with_dependencies_init (kan_context_system_t handle)
{
    struct system_with_dependencies_t *system = KAN_HANDLE_GET (handle);

    struct first_independent_system_t *first =
        KAN_HANDLE_GET (kan_context_query (system->context, "first_independent_system_t"));

    if (first)
    {
        system->first_used = true;
        KAN_TEST_CHECK (first->initialized)
    }

    struct second_independent_system_t *second =
        KAN_HANDLE_GET (kan_context_query (system->context, "second_independent_system_t"));

    if (second)
    {
        system->second_used = true;
        KAN_TEST_CHECK (second->initialized)
    }

    system->initialized = true;
}

TEST_CONTEXT_API void system_with_dependencies_shutdown (kan_context_system_t handle)
{
    struct system_with_dependencies_t *system = KAN_HANDLE_GET (handle);

    struct first_independent_system_t *first =
        KAN_HANDLE_GET (kan_context_query (system->context, "first_independent_system_t"));

    if (first)
    {
        KAN_TEST_CHECK (first->initialized)
    }

    struct second_independent_system_t *second =
        KAN_HANDLE_GET (kan_context_query (system->context, "second_independent_system_t"));

    if (second)
    {
        KAN_TEST_CHECK (second->initialized)
    }

    system->initialized = false;
}

TEST_CONTEXT_API void system_with_dependencies_disconnect (kan_context_system_t handle) {}

TEST_CONTEXT_API void system_with_dependencies_destroy (kan_context_system_t handle)
{
    struct system_with_dependencies_t *system = KAN_HANDLE_GET (handle);
    kan_free_general (system->group, system, sizeof (struct system_with_dependencies_t));
}

TEST_CONTEXT_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (system_with_dependencies_t) = {
    .name = "system_with_dependencies_t",
    .create = system_with_dependencies_create,
    .connect = system_with_dependencies_connect,
    .connected_init = system_with_dependencies_init,
    .connected_shutdown = system_with_dependencies_shutdown,
    .disconnect = system_with_dependencies_disconnect,
    .destroy = system_with_dependencies_destroy,
};

KAN_TEST_CASE (no_systems)
{
    kan_context_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);
    kan_context_assembly (context);
    KAN_TEST_CHECK (!KAN_HANDLE_IS_VALID (kan_context_query (context, "first_independent_system_t")))
    KAN_TEST_CHECK (!KAN_HANDLE_IS_VALID (kan_context_query (context, "second_independent_system_t")))
    KAN_TEST_CHECK (!KAN_HANDLE_IS_VALID (kan_context_query (context, "system_with_dependencies_t")))
    kan_context_destroy (context);
}

KAN_TEST_CASE (only_first)
{
    kan_context_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);
    KAN_TEST_CHECK (kan_context_request_system (context, "first_independent_system_t", NULL))
    kan_context_assembly (context);

    struct first_independent_system_t *first =
        KAN_HANDLE_GET (kan_context_query (context, "first_independent_system_t"));
    KAN_TEST_ASSERT (first)
    KAN_TEST_CHECK (first->initialized)
    KAN_TEST_CHECK (!first->second_connected)

    KAN_TEST_CHECK (!KAN_HANDLE_IS_VALID (kan_context_query (context, "second_independent_system_t")))
    KAN_TEST_CHECK (!KAN_HANDLE_IS_VALID (kan_context_query (context, "system_with_dependencies_t")))
    kan_context_destroy (context);
}

KAN_TEST_CASE (only_second)
{
    kan_context_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);
    KAN_TEST_CHECK (kan_context_request_system (context, "second_independent_system_t", NULL))
    kan_context_assembly (context);

    struct second_independent_system_t *second =
        KAN_HANDLE_GET (kan_context_query (context, "second_independent_system_t"));
    KAN_TEST_ASSERT (second)
    KAN_TEST_CHECK (second->initialized)

    KAN_TEST_CHECK (!KAN_HANDLE_IS_VALID (kan_context_query (context, "first_independent_system_t")))
    KAN_TEST_CHECK (!KAN_HANDLE_IS_VALID (kan_context_query (context, "system_with_dependencies_t")))
    kan_context_destroy (context);
}

KAN_TEST_CASE (only_with_dependencies)
{
    kan_context_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);
    KAN_TEST_CHECK (kan_context_request_system (context, "system_with_dependencies_t", NULL))
    kan_context_assembly (context);

    struct system_with_dependencies_t *system =
        KAN_HANDLE_GET (kan_context_query (context, "system_with_dependencies_t"));
    KAN_TEST_ASSERT (system)
    KAN_TEST_CHECK (system->initialized)
    KAN_TEST_CHECK (!system->first_used)
    KAN_TEST_CHECK (!system->second_used)

    KAN_TEST_CHECK (!KAN_HANDLE_IS_VALID (kan_context_query (context, "first_independent_system_t")))
    KAN_TEST_CHECK (!KAN_HANDLE_IS_VALID (kan_context_query (context, "second_independent_system_t")))
    kan_context_destroy (context);
}

KAN_TEST_CASE (first_and_second)
{
    kan_context_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);
    KAN_TEST_CHECK (kan_context_request_system (context, "first_independent_system_t", NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, "second_independent_system_t", NULL))
    kan_context_assembly (context);

    struct first_independent_system_t *first =
        KAN_HANDLE_GET (kan_context_query (context, "first_independent_system_t"));
    KAN_TEST_ASSERT (first)
    KAN_TEST_CHECK (first->initialized)
    KAN_TEST_CHECK (first->second_connected)

    struct second_independent_system_t *second =
        KAN_HANDLE_GET (kan_context_query (context, "second_independent_system_t"));
    KAN_TEST_ASSERT (second)
    KAN_TEST_CHECK (second->initialized)

    KAN_TEST_CHECK (!KAN_HANDLE_IS_VALID (kan_context_query (context, "system_with_dependencies_t")))
    kan_context_destroy (context);
}

KAN_TEST_CASE (all)
{
    kan_context_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);
    KAN_TEST_CHECK (kan_context_request_system (context, "first_independent_system_t", NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, "second_independent_system_t", NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, "system_with_dependencies_t", NULL))
    kan_context_assembly (context);

    struct system_with_dependencies_t *system =
        KAN_HANDLE_GET (kan_context_query (context, "system_with_dependencies_t"));
    KAN_TEST_ASSERT (system)
    KAN_TEST_CHECK (system->initialized)
    KAN_TEST_CHECK (system->first_used)
    KAN_TEST_CHECK (system->second_used)

    KAN_TEST_CHECK (KAN_HANDLE_IS_VALID (kan_context_query (context, "first_independent_system_t")))
    KAN_TEST_CHECK (KAN_HANDLE_IS_VALID (kan_context_query (context, "second_independent_system_t")))
    kan_context_destroy (context);
}
