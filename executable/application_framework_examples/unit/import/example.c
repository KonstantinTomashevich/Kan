#include <application_framework_examples_import_api.h>

#include <stdio.h>
#include <string.h>

#include <kan/context/all_system_names.h>
#include <kan/context/application_framework_system.h>
#include <kan/context/application_system.h>
#include <kan/log/logging.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>
#include <kan/universe_time/universe_time.h>

#include <examples/icon.h>

KAN_LOG_DEFINE_CATEGORY (application_framework_examples_import);

struct example_import_singleton_t
{
    kan_application_system_window_t window_handle;
    kan_bool_t test_request_added;
    kan_resource_request_id_t test_request_id;
    kan_bool_t test_asset_loaded;
};

APPLICATION_FRAMEWORK_EXAMPLES_IMPORT_API void example_import_singleton_init (
    struct example_import_singleton_t *instance)
{
    instance->window_handle = KAN_HANDLE_SET_INVALID (kan_application_system_window_t);
    instance->test_request_added = KAN_FALSE;
    instance->test_asset_loaded = KAN_FALSE;
}

struct import_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (import)
    KAN_UP_BIND_STATE (import, state)

    kan_context_system_t application_system_handle;
    kan_context_system_t application_framework_system_handle;

    kan_bool_t test_mode;
    kan_bool_t test_passed;
    kan_instance_size_t test_frames_count;
};

APPLICATION_FRAMEWORK_EXAMPLES_IMPORT_API void kan_universe_mutator_deploy_import (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct import_state_t *state)
{
    kan_context_t context = kan_universe_get_context (universe);
    state->application_system_handle = kan_context_query (context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);
    state->application_framework_system_handle =
        kan_context_query (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
    {
        state->test_mode =
            kan_application_framework_system_get_arguments_count (state->application_framework_system_handle) == 2 &&
            strcmp (kan_application_framework_system_get_arguments (state->application_framework_system_handle)[1],
                    "--test") == 0;
    }
    else
    {
        state->test_mode = KAN_FALSE;
    }

    state->test_passed = KAN_TRUE;
    state->test_frames_count = 0u;
}

APPLICATION_FRAMEWORK_EXAMPLES_IMPORT_API void kan_universe_mutator_execute_import (kan_cpu_job_t job,
                                                                                    struct import_state_t *state)
{
    KAN_UP_SINGLETON_WRITE (singleton, example_import_singleton_t)
    {
        if (!KAN_HANDLE_IS_VALID (singleton->window_handle))
        {
            singleton->window_handle =
                kan_application_system_window_create (state->application_system_handle, "Title (expect icon too)", 600u,
                                                      400u, KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_VULKAN);
            kan_application_system_window_raise (state->application_system_handle, singleton->window_handle);
        }

        if (!singleton->test_request_added)
        {
            KAN_UP_SINGLETON_READ (provider, kan_resource_provider_singleton_t)
            {
                KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
                {
                    request->request_id = kan_next_resource_request_id (provider);
                    request->type = kan_string_intern ("icon_t");
                    request->name = kan_string_intern ("orange_icon");
                    request->priority = 0u;
                    singleton->test_request_id = request->request_id;
                }
            }

            singleton->test_request_added = KAN_TRUE;
        }

        KAN_UP_EVENT_FETCH (event, kan_resource_request_updated_event_t)
        {
            if (KAN_TYPED_ID_32_IS_EQUAL (event->request_id, singleton->test_request_id))
            {
                KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &singleton->test_request_id)
                {
                    KAN_ASSERT (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
                    singleton->test_asset_loaded = KAN_TRUE;

                    KAN_UP_VALUE_READ (view, KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (icon_t), container_id,
                                       &request->provided_container_id)
                    {
                        const struct icon_t *icon = KAN_RESOURCE_PROVIDER_CONTAINER_GET (icon_t, view);
                        kan_application_system_window_set_icon (
                            state->application_system_handle, singleton->window_handle,
                            KAN_PLATFORM_PIXEL_FORMAT_RGBA32, (kan_platform_visual_size_t) icon->width,
                            (kan_platform_visual_size_t) icon->height, icon->pixels.data);
                    }
                }
            }
        }

        if (state->test_mode)
        {
            if (30u < ++state->test_frames_count)
            {
                KAN_LOG (application_framework_examples_import, KAN_LOG_INFO, "Shutting down...")
                if (!singleton->test_asset_loaded)
                {
                    state->test_passed = KAN_FALSE;
                    KAN_LOG (application_framework_examples_import, KAN_LOG_ERROR, "Failed to load asset.")
                }

                KAN_ASSERT (KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
                if (state->test_passed)
                {
                    kan_application_framework_system_request_exit (state->application_framework_system_handle, 0);
                }
                else
                {
                    kan_application_framework_system_request_exit (state->application_framework_system_handle, 1);
                }
            }
        }
    }

    KAN_UP_MUTATOR_RETURN;
}
