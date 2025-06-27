#include <context_update_system_api.h>

#include <stddef.h>
#include <string.h>

#include <kan/api_common/min_max.h>
#include <kan/container/dynamic_array.h>
#include <kan/context/all_system_names.h>
#include <kan/context/update_system.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/reflection/markup.h>

KAN_LOG_DEFINE_CATEGORY (update_system);

struct update_connection_request_t
{
    struct update_connection_request_t *next;
    kan_context_system_t system;
    kan_context_update_run_t functor;

    bool added;
    bool proxy;
    kan_instance_size_t dependencies_left;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_context_system_t)
    struct kan_dynamic_array_t dependencies;
};

struct update_callable_t
{
    kan_context_system_t system;
    kan_context_update_run_t functor;
};

struct update_system_t
{
    kan_context_t context;
    kan_allocation_group_t group;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct update_callable_t)
    struct kan_dynamic_array_t update_sequence;

    kan_instance_size_t connection_request_count;
    struct update_connection_request_t *first_connection_request;

    kan_cpu_section_t update_section;
};

CONTEXT_UPDATE_SYSTEM_API kan_context_system_t update_system_create (kan_allocation_group_t group, void *user_config)
{
    struct update_system_t *system =
        kan_allocate_general (group, sizeof (struct update_system_t), _Alignof (struct update_system_t));
    system->group = group;
    kan_dynamic_array_init (&system->update_sequence, 0u, sizeof (struct update_callable_t),
                            _Alignof (struct update_callable_t), group);
    system->connection_request_count = 0u;
    system->first_connection_request = NULL;
    system->update_section = kan_cpu_section_get ("context_update_system");
    return KAN_HANDLE_SET (kan_context_system_t, system);
}

CONTEXT_UPDATE_SYSTEM_API void update_system_connect (kan_context_system_t handle, kan_context_t context)
{
    struct update_system_t *system = KAN_HANDLE_GET (handle);
    system->context = context;
}

static void visit_to_generate_update_sequence (struct update_system_t *system,
                                               struct update_connection_request_t *request)
{
    if (request->dependencies_left > 0u || request->added)
    {
        return;
    }

    request->added = true;
    if (!request->proxy)
    {
        struct update_callable_t *callable = kan_dynamic_array_add_last (&system->update_sequence);
        KAN_ASSERT (callable);

        *callable = (struct update_callable_t) {
            .system = request->system,
            .functor = request->functor,
        };
    }

    struct update_connection_request_t *other_request = system->first_connection_request;
    while (other_request)
    {
        for (kan_loop_size_t index = 0u; index < other_request->dependencies.size; ++index)
        {
            if (KAN_HANDLE_IS_EQUAL (((kan_context_system_t *) other_request->dependencies.data)[index],
                                     request->system))
            {
                --other_request->dependencies_left;
                visit_to_generate_update_sequence (system, other_request);
                break;
            }
        }

        other_request = other_request->next;
    }
}

CONTEXT_UPDATE_SYSTEM_API void update_system_init (kan_context_system_t handle)
{
    struct update_system_t *system = KAN_HANDLE_GET (handle);
    if (!system->first_connection_request)
    {
        return;
    }

    kan_dynamic_array_set_capacity (&system->update_sequence, system->connection_request_count);
    struct update_connection_request_t *request = system->first_connection_request;

    // Calculate dependency left counts. We do it here so we wouldn't count unregistered systems.
    // It is okay to loop over everything as there shouldn't be lots of context systems.
    while (request)
    {
        for (kan_loop_size_t index = 0u; index < request->dependencies.size; ++index)
        {
            const kan_context_system_t dependency = ((kan_context_system_t *) request->dependencies.data)[index];
            struct update_connection_request_t *other_request = system->first_connection_request;

            while (other_request)
            {
                if (KAN_HANDLE_IS_EQUAL (other_request->system, dependency))
                {
                    ++request->dependencies_left;
                }

                other_request = other_request->next;
            }
        }

        request = request->next;
    }

    request = system->first_connection_request;
    while (request)
    {
        visit_to_generate_update_sequence (system, request);
        request = request->next;
    }

    bool any_not_added = false;
    while (system->first_connection_request)
    {
        struct update_connection_request_t *next = system->first_connection_request->next;
        if (!system->first_connection_request->added && !system->first_connection_request->proxy)
        {
            any_not_added = true;
        }

        kan_dynamic_array_shutdown (&system->first_connection_request->dependencies);
        kan_free_batched (system->group, system->first_connection_request);
        system->first_connection_request = next;
    }

    if (any_not_added)
    {
        KAN_LOG (update_system, KAN_LOG_ERROR,
                 "Unable to generate full update graph due to cycles or incorrect dependencies.")
    }

    kan_dynamic_array_set_capacity (&system->update_sequence, system->update_sequence.size);
}

CONTEXT_UPDATE_SYSTEM_API void update_system_shutdown (kan_context_system_t handle) {}

CONTEXT_UPDATE_SYSTEM_API void update_system_disconnect (kan_context_system_t handle) {}

CONTEXT_UPDATE_SYSTEM_API void update_system_destroy (kan_context_system_t handle)
{
    struct update_system_t *system = KAN_HANDLE_GET (handle);
    KAN_ASSERT (!system->first_connection_request)
    KAN_ASSERT (system->update_sequence.size == 0u)
    kan_dynamic_array_shutdown (&system->update_sequence);
    kan_free_general (system->group, system, sizeof (struct update_system_t));
}

CONTEXT_UPDATE_SYSTEM_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (update_system_t) = {
    .name = KAN_CONTEXT_UPDATE_SYSTEM_NAME,
    .create = update_system_create,
    .connect = update_system_connect,
    .connected_init = update_system_init,
    .connected_shutdown = update_system_shutdown,
    .disconnect = update_system_disconnect,
    .destroy = update_system_destroy,
};

void kan_update_system_connect_on_run (kan_context_system_t update_system,
                                       kan_context_system_t other_system,
                                       kan_context_update_run_t functor,
                                       kan_instance_size_t dependencies_count,
                                       kan_context_system_t *dependencies,
                                       kan_instance_size_t dependency_of_count,
                                       kan_context_system_t *dependency_of)
{
    struct update_system_t *system = KAN_HANDLE_GET (update_system);
    struct update_connection_request_t *request = system->first_connection_request;

    while (request)
    {
        if (KAN_HANDLE_IS_EQUAL (request->system, other_system))
        {
            if (!request->proxy)
            {
                KAN_LOG (update_system, KAN_LOG_ERROR, "Caught attempt to register the same system twice.")
                return;
            }

            break;
        }

        request = request->next;
    }

    if (!request)
    {
        request = kan_allocate_batched (system->group, sizeof (struct update_connection_request_t));
        request->dependencies_left = 0u;
        kan_dynamic_array_init (&request->dependencies, 0u, sizeof (kan_context_system_t),
                                _Alignof (kan_context_system_t), system->group);
    }

    request->system = other_system;
    request->functor = functor;

    if (dependencies_count > 0u)
    {
        if (request->dependencies.capacity < request->dependencies.size + dependencies_count)
        {
            kan_dynamic_array_set_capacity (
                &request->dependencies,
                KAN_MAX (request->dependencies.size + dependencies_count, request->dependencies.capacity * 2u));
        }

        for (kan_loop_size_t index = 0u; index < dependencies_count; ++index)
        {
            if (KAN_HANDLE_IS_VALID (dependencies[index]))
            {
                *(kan_context_system_t *) kan_dynamic_array_add_last (&request->dependencies) = dependencies[index];
            }
        }
    }

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) dependency_of_count; ++index)
    {
        const kan_context_system_t dependency_of_system = dependency_of[index];
        if (!KAN_HANDLE_IS_VALID (dependency_of_system))
        {
            continue;
        }

        struct update_connection_request_t *other_request = system->first_connection_request;
        while (other_request)
        {
            if (KAN_HANDLE_IS_EQUAL (other_request->system, dependency_of_system))
            {
                break;
            }

            other_request = other_request->next;
        }

        if (!other_request)
        {
            other_request = kan_allocate_batched (system->group, sizeof (struct update_connection_request_t));
            other_request->system = dependency_of_system;
            other_request->functor = NULL;
            other_request->added = false;
            other_request->proxy = true;
            other_request->dependencies_left = 0u;
            kan_dynamic_array_init (&other_request->dependencies, 0u, sizeof (kan_context_system_t),
                                    _Alignof (kan_context_system_t), system->group);
        }

        kan_context_system_t *spot = kan_dynamic_array_add_last (&other_request->dependencies);
        if (!spot)
        {
            kan_dynamic_array_set_capacity (&other_request->dependencies,
                                            KAN_MAX (1u, other_request->dependencies.capacity * 2u));
            spot = kan_dynamic_array_add_last (&other_request->dependencies);
        }

        *spot = other_system;
    }

    request->added = false;
    request->proxy = false;

    request->next = system->first_connection_request;
    system->first_connection_request = request;
    ++system->connection_request_count;
}

void kan_update_system_disconnect_on_run (kan_context_system_t update_system, kan_context_system_t other_system)
{
    struct update_system_t *system = KAN_HANDLE_GET (update_system);
    // Check that we're not in connection phase.
    KAN_ASSERT (!system->first_connection_request)

    for (kan_loop_size_t index = 0u; index < system->update_sequence.size; ++index)
    {
        struct update_callable_t *callable = &((struct update_callable_t *) system->update_sequence.data)[index];
        if (KAN_HANDLE_IS_EQUAL (callable->system, other_system))
        {
            kan_dynamic_array_remove_at (&system->update_sequence, index);
            return;
        }
    }
}

void kan_update_system_run (kan_context_system_t update_system)
{
    struct update_system_t *system = KAN_HANDLE_GET (update_system);
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->update_section);

    for (kan_loop_size_t index = 0u; index < system->update_sequence.size; ++index)
    {
        struct update_callable_t *callable = &((struct update_callable_t *) system->update_sequence.data)[index];
        callable->functor (callable->system);
    }

    kan_cpu_section_execution_shutdown (&execution);
}
