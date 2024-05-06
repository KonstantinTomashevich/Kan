#include <application_framework_example_logic_api.h>

#include <stdio.h>

#include <kan/context/application_framework_system.h>
#include <kan/context/application_system.h>
#include <kan/platform/precise_time.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

struct test_data_type_t
{
    uint64_t x;
    uint64_t y;
};

// \meta reflection_struct_meta = "test_data_type_t"
APPLICATION_FRAMEWORK_EXAMPLE_LOGIC_API struct kan_resource_provider_type_meta_t second_resource_type_meta = {0u};

struct plain_update_state_t
{
    uint64_t stub;
};

APPLICATION_FRAMEWORK_EXAMPLE_LOGIC_API void kan_universe_scheduler_execute_plain_update (
    kan_universe_scheduler_interface_t interface, struct plain_update_state_t *state)
{
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern ("update"));
    kan_universe_scheduler_interface_update_all_children (interface);
}

struct test_singleton_t
{
    kan_application_system_window_handle_t window_handle;
};

APPLICATION_FRAMEWORK_EXAMPLE_LOGIC_API void test_singleton_init (struct test_singleton_t *instance)
{
    instance->window_handle = KAN_INVALID_APPLICATION_SYSTEM_WINDOW_HANDLE;
}

struct test_mutator_state_t
{
    struct kan_repository_singleton_write_query_t write__test_singleton;

    kan_context_system_handle_t application_system_handle;
    kan_application_system_event_iterator_t event_iterator;
    kan_context_system_handle_t application_framework_system_handle;
};

APPLICATION_FRAMEWORK_EXAMPLE_LOGIC_API void kan_universe_mutator_deploy_test_mutator (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct test_mutator_state_t *state)
{
    kan_context_handle_t context = kan_universe_get_context (universe);
    state->application_system_handle = kan_context_query (context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);
    state->application_framework_system_handle =
        kan_context_query (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME);

    if (state->application_system_handle != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        state->event_iterator = kan_application_system_event_iterator_create (state->application_system_handle);
    }
}

APPLICATION_FRAMEWORK_EXAMPLE_LOGIC_API void kan_universe_mutator_execute_test_mutator (
    kan_cpu_job_t job, struct test_mutator_state_t *state)
{
    const struct kan_platform_application_event_t *event;
    while (
        (event = kan_application_system_event_iterator_get (state->application_system_handle, state->event_iterator)))
    {
        if (event->type == KAN_PLATFORM_APPLICATION_EVENT_TYPE_QUIT)
        {
            if (state->application_framework_system_handle != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
            {
                kan_application_framework_system_request_exit (state->application_framework_system_handle, 0);
            }
        }

        state->event_iterator = kan_application_system_event_iterator_advance (state->event_iterator);
    }

    kan_repository_singleton_write_access_t write_access =
        kan_repository_singleton_write_query_execute (&state->write__test_singleton);
    struct test_singleton_t *singleton =
        (struct test_singleton_t *) kan_repository_singleton_write_access_resolve (write_access);

    if (singleton->window_handle == KAN_INVALID_APPLICATION_SYSTEM_WINDOW_HANDLE)
    {
        singleton->window_handle =
            kan_application_system_window_create (state->application_system_handle, "Title placeholder", 600u, 400u,
                                                  KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_VULKAN);
         kan_application_system_window_raise (state->application_system_handle, singleton->window_handle);
    }

#define TITLE_BUFFER_SIZE 256u
    char buffer[TITLE_BUFFER_SIZE];
    snprintf (buffer, TITLE_BUFFER_SIZE, "Seconds from startup: %llu.",
              (unsigned long long) (kan_platform_get_elapsed_nanoseconds () / 1000000000ull));
    kan_application_system_window_set_title (state->application_system_handle, singleton->window_handle, buffer);
#undef TITLE_BUFFER_SIZE

    kan_repository_singleton_write_access_close (write_access);

    kan_cpu_job_release (job);
}

APPLICATION_FRAMEWORK_EXAMPLE_LOGIC_API void kan_universe_mutator_undeploy_test_mutator (
    struct test_mutator_state_t *state)
{
    if (state->application_system_handle != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        kan_application_system_event_iterator_destroy (state->application_system_handle, state->event_iterator);
    }
}
