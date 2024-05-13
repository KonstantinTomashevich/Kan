#include <stddef.h>
#include <stdlib.h>

#include <kan/context/application_framework_system.h>
#include <kan/context/application_system.h>
#include <kan/context/update_system.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/memory/allocation.h>

struct application_framework_system_t
{
    kan_context_handle_t context;
    kan_allocation_group_t group;

    uint64_t outer_arguments_count;
    char **outer_arguments;
    char *outer_auto_build_and_hot_reload_command;

    kan_bool_t exit_requested;
    int exit_code;

    uint64_t min_frame_time_ns;
    kan_application_system_event_iterator_t event_iterator;
    kan_cpu_section_t update_section;
};

kan_context_system_handle_t application_framework_system_create (kan_allocation_group_t group, void *user_config)
{
    struct application_framework_system_t *system = kan_allocate_general (
        group, sizeof (struct application_framework_system_t), _Alignof (struct application_framework_system_t));
    system->group = group;

    if (user_config)
    {
        struct kan_application_framework_system_config_t *config = user_config;
        system->outer_arguments_count = config->arguments_count;
        system->outer_arguments = config->arguments;
        system->outer_auto_build_and_hot_reload_command = config->auto_build_and_hot_reload_command;
    }
    else
    {
        system->outer_arguments_count = 0u;
        system->outer_arguments = NULL;
        system->outer_auto_build_and_hot_reload_command = NULL;
    }

    system->exit_requested = KAN_FALSE;
    system->exit_code = 0;
    system->min_frame_time_ns = KAN_APPLICATION_FRAMEWORK_DEFAULT_MIN_FRAME_TIME_NS;
    system->update_section = kan_cpu_section_get ("context_application_framework_system_update");
    return (kan_context_system_handle_t) system;
}

static void application_framework_system_update (kan_context_system_handle_t handle)
{
    struct application_framework_system_t *framework_system = (struct application_framework_system_t *) handle;
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, framework_system->update_section);

    kan_context_system_handle_t application_system =
        kan_context_query (framework_system->context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);

    if (application_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        const struct kan_platform_application_event_t *event;
        while (
            (event = kan_application_system_event_iterator_get (application_system, framework_system->event_iterator)))
        {
            if (event->type == KAN_PLATFORM_APPLICATION_EVENT_TYPE_QUIT)
            {
                if (!framework_system->exit_requested)
                {
                    framework_system->exit_requested = KAN_TRUE;
                    framework_system->exit_code = 0;
                }
            }
            else if (event->type == KAN_PLATFORM_APPLICATION_EVENT_TYPE_WINDOW_FOCUS_GAINED)
            {
                if (!framework_system->exit_requested && framework_system->outer_auto_build_and_hot_reload_command)
                {
                    system (framework_system->outer_auto_build_and_hot_reload_command);
                }
            }

            framework_system->event_iterator =
                kan_application_system_event_iterator_advance (framework_system->event_iterator);
        }
    }

    kan_cpu_section_execution_shutdown (&execution);
}

void application_framework_system_connect (kan_context_system_handle_t handle, kan_context_handle_t context)
{
    struct application_framework_system_t *system = (struct application_framework_system_t *) handle;
    system->context = context;

    kan_context_system_handle_t update_system = kan_context_query (system->context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
    if (update_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        kan_update_system_connect_on_run (update_system, handle, application_framework_system_update, 0u, NULL);
    }
}

void application_framework_system_init (kan_context_system_handle_t handle)
{
    struct application_framework_system_t *system = (struct application_framework_system_t *) handle;
    kan_context_system_handle_t application_system =
        kan_context_query (system->context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);

    if (application_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        system->event_iterator = kan_application_system_event_iterator_create (application_system);
    }
}

void application_framework_system_shutdown (kan_context_system_handle_t handle)
{
    struct application_framework_system_t *system = (struct application_framework_system_t *) handle;
    kan_context_system_handle_t application_system =
        kan_context_query (system->context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);

    if (application_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        kan_application_system_event_iterator_destroy (application_system, system->event_iterator);
    }
}

void application_framework_system_disconnect (kan_context_system_handle_t handle)
{
    struct application_framework_system_t *system = (struct application_framework_system_t *) handle;
    kan_context_system_handle_t update_system = kan_context_query (system->context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);

    if (update_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        kan_update_system_disconnect_on_run (update_system, handle);
    }
}

void application_framework_system_destroy (kan_context_system_handle_t handle)
{
    struct application_framework_system_t *system = (struct application_framework_system_t *) handle;
    kan_free_general (system->group, system, sizeof (struct application_framework_system_t));
}

APPLICATION_FRAMEWORK_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (
    application_framework_system_t) = {
    .name = "application_framework_system_t",
    .create = application_framework_system_create,
    .connect = application_framework_system_connect,
    .connected_init = application_framework_system_init,
    .connected_shutdown = application_framework_system_shutdown,
    .disconnect = application_framework_system_disconnect,
    .destroy = application_framework_system_destroy,
};

uint64_t kan_application_framework_system_get_arguments_count (kan_context_system_handle_t application_framework_system)
{
    struct application_framework_system_t *system =
        (struct application_framework_system_t *) application_framework_system;
    return system->outer_arguments_count;
}

char **kan_application_framework_system_get_arguments (kan_context_system_handle_t application_framework_system)
{
    struct application_framework_system_t *system =
        (struct application_framework_system_t *) application_framework_system;
    return system->outer_arguments;
}

uint64_t kan_application_framework_get_min_frame_time_ns (kan_context_system_handle_t application_framework_system)
{
    struct application_framework_system_t *system =
        (struct application_framework_system_t *) application_framework_system;
    return system->min_frame_time_ns;
}

void kan_application_framework_set_min_frame_time_ns (kan_context_system_handle_t application_framework_system,
                                                      uint64_t min_frame_time_ns)
{
    struct application_framework_system_t *system =
        (struct application_framework_system_t *) application_framework_system;
    system->min_frame_time_ns = min_frame_time_ns;
}

void kan_application_framework_system_request_exit (kan_context_system_handle_t application_framework_system,
                                                    int exit_code)
{
    struct application_framework_system_t *system =
        (struct application_framework_system_t *) application_framework_system;

    if (!system->exit_requested)
    {
        system->exit_requested = KAN_TRUE;
        system->exit_code = exit_code;
    }
}

kan_bool_t kan_application_framework_system_is_exit_requested (kan_context_system_handle_t application_framework_system,
                                                               int *exit_code_output)
{
    struct application_framework_system_t *system =
        (struct application_framework_system_t *) application_framework_system;

    if (system->exit_requested)
    {
        *exit_code_output = system->exit_code;
    }

    return system->exit_requested;
}
