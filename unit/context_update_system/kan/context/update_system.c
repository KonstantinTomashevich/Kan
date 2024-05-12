#include <context_update_system_api.h>

#include <stddef.h>
#include <string.h>

#include <kan/container/dynamic_array.h>
#include <kan/context/update_system.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>

KAN_LOG_DEFINE_CATEGORY (update_system);

struct update_connection_request_t
{
    struct update_connection_request_t *next;
    kan_context_system_handle_t system;
    kan_context_update_run_t functor;
    kan_bool_t added;
    uint64_t dependencies_count;
    uint64_t dependencies_left;
    kan_context_system_handle_t *dependencies;
};

struct update_callable_t
{
    kan_context_system_handle_t system;
    kan_context_update_run_t functor;
};

struct update_system_t
{
    kan_context_handle_t context;
    kan_allocation_group_t group;

    /// \meta reflection_dynamic_array_type = "struct update_callable_t"
    struct kan_dynamic_array_t update_sequence;

    uint64_t connection_request_count;
    struct update_connection_request_t *first_connection_request;

    kan_cpu_section_t update_section;
};

CONTEXT_UPDATE_SYSTEM_API kan_context_system_handle_t update_system_create (kan_allocation_group_t group,
                                                                            void *user_config)
{
    struct update_system_t *system =
        kan_allocate_general (group, sizeof (struct update_system_t), _Alignof (struct update_system_t));
    system->group = group;
    kan_dynamic_array_init (&system->update_sequence, 0u, sizeof (struct update_callable_t),
                            _Alignof (struct update_callable_t), group);
    system->connection_request_count = 0u;
    system->first_connection_request = NULL;
    system->update_section = kan_cpu_section_get ("context_update_system");
    return (kan_context_system_handle_t) system;
}

CONTEXT_UPDATE_SYSTEM_API void update_system_connect (kan_context_system_handle_t handle, kan_context_handle_t context)
{
    struct update_system_t *system = (struct update_system_t *) handle;
    system->context = context;
}

static void visit_to_generate_update_sequence (struct update_system_t *system,
                                               struct update_connection_request_t *request)
{
    if (request->dependencies_left > 0u || request->added)
    {
        return;
    }

    request->added = KAN_TRUE;
    struct update_callable_t *callable = kan_dynamic_array_add_last (&system->update_sequence);
    KAN_ASSERT (callable);

    *callable = (struct update_callable_t) {
        .system = request->system,
        .functor = request->functor,
    };

    struct update_connection_request_t *other_request = system->first_connection_request;
    while (other_request)
    {
        for (uint64_t index = 0u; index < other_request->dependencies_count; ++index)
        {
            if (other_request->dependencies[index] == request->system)
            {
                --other_request->dependencies_left;
                visit_to_generate_update_sequence (system, other_request);
                break;
            }
        }

        other_request = other_request->next;
    }
}

CONTEXT_UPDATE_SYSTEM_API void update_system_init (kan_context_system_handle_t handle)
{
    struct update_system_t *system = (struct update_system_t *) handle;
    if (!system->first_connection_request)
    {
        return;
    }

    kan_dynamic_array_set_capacity (&system->update_sequence, system->connection_request_count);
    struct update_connection_request_t *request = system->first_connection_request;

    while (request)
    {
        visit_to_generate_update_sequence (system, request);
        request = request->next;
    }

    kan_bool_t any_not_added = KAN_FALSE;
    while (system->first_connection_request)
    {
        struct update_connection_request_t *next = system->first_connection_request->next;
        if (!system->first_connection_request->added)
        {
            any_not_added = KAN_TRUE;
        }

        if (system->first_connection_request->dependencies)
        {
            kan_free_general (
                system->group, system->first_connection_request->dependencies,
                sizeof (kan_context_system_handle_t) * system->first_connection_request->dependencies_count);
        }

        kan_free_batched (system->group, system->first_connection_request);
        system->first_connection_request = next;
    }

    if (any_not_added)
    {
        KAN_LOG (update_system, KAN_LOG_ERROR,
                 "Unable to generate full update graph due to cycles or incorrect dependencies.")
    }
}

CONTEXT_UPDATE_SYSTEM_API void update_system_shutdown (kan_context_system_handle_t handle)
{
}

CONTEXT_UPDATE_SYSTEM_API void update_system_disconnect (kan_context_system_handle_t handle)
{
}

CONTEXT_UPDATE_SYSTEM_API void update_system_destroy (kan_context_system_handle_t handle)
{
    struct update_system_t *system = (struct update_system_t *) handle;
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

void kan_update_system_connect_on_run (kan_context_system_handle_t update_system,
                                       kan_context_system_handle_t other_system,
                                       kan_context_update_run_t functor,
                                       uint64_t dependencies_count,
                                       kan_context_system_handle_t *dependencies)
{
    struct update_system_t *system = (struct update_system_t *) update_system;
    struct update_connection_request_t *request =
        kan_allocate_batched (system->group, sizeof (struct update_connection_request_t));
    request->system = other_system;
    request->functor = functor;
    request->dependencies_count = dependencies_count;
    request->dependencies_left = dependencies_count;

    if (request->dependencies_count > 0u)
    {
        request->dependencies =
            kan_allocate_general (system->group, sizeof (kan_context_system_handle_t) * dependencies_count,
                                  _Alignof (kan_context_system_handle_t));
        memcpy (request->dependencies, dependencies, sizeof (kan_context_system_handle_t) * dependencies_count);
    }
    else
    {
        request->dependencies = NULL;
    }

    request->added = KAN_FALSE;
    request->next = system->first_connection_request;
    system->first_connection_request = request;
    ++system->connection_request_count;
}

void kan_update_system_disconnect_on_run (kan_context_system_handle_t update_system,
                                          kan_context_system_handle_t other_system)
{
    struct update_system_t *system = (struct update_system_t *) update_system;
    // Check that we're not in connection phase.
    KAN_ASSERT (!system->first_connection_request)

    for (uint64_t index = 0u; index < system->update_sequence.size; ++index)
    {
        struct update_callable_t *callable = &((struct update_callable_t *) system->update_sequence.data)[index];
        if (callable->system == other_system)
        {
            kan_dynamic_array_remove_at (&system->update_sequence, index);
            return;
        }
    }
}

void kan_update_system_run (kan_context_system_handle_t update_system)
{
    struct update_system_t *system = (struct update_system_t *) update_system;
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->update_section);

    for (uint64_t index = 0u; index < system->update_sequence.size; ++index)
    {
        struct update_callable_t *callable = &((struct update_callable_t *) system->update_sequence.data)[index];
        callable->functor (callable->system);
    }

    kan_cpu_section_execution_shutdown (&execution);
}
