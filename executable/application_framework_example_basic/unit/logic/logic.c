#include <application_framework_example_basic_logic_api.h>

#include <stdio.h>
#include <string.h>

#include <kan/context/all_system_names.h>
#include <kan/context/application_framework_system.h>
#include <kan/context/application_system.h>
#include <kan/log/logging.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>
#include <kan/universe_time/universe_time.h>

// \c_interface_scanner_disable
KAN_LOG_DEFINE_CATEGORY (application_framework_example_basic_logic_test_mode);
// \c_interface_scanner_enable

struct test_data_type_t
{
    kan_serialized_size_t x;
    kan_serialized_size_t y;
};

_Static_assert (_Alignof (struct test_data_type_t) <= _Alignof (kan_memory_size_t), "Alignment has expected value.");

// \meta reflection_struct_meta = "test_data_type_t"
APPLICATION_FRAMEWORK_EXAMPLE_BASIC_LOGIC_API struct kan_resource_resource_type_meta_t test_data_type_meta = {
    .root = KAN_TRUE,
};

struct test_singleton_t
{
    kan_application_system_window_t window_handle;
    kan_bool_t test_request_added;
    kan_resource_request_id_t test_request_id;
};

APPLICATION_FRAMEWORK_EXAMPLE_BASIC_LOGIC_API void test_singleton_init (struct test_singleton_t *instance)
{
    instance->window_handle = KAN_HANDLE_SET_INVALID (kan_application_system_window_t);
    instance->test_request_added = KAN_FALSE;
}

struct test_mutator_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (test_mutator)
    KAN_UP_BIND_STATE (test_mutator, state)

    kan_context_system_t application_system_handle;
    kan_context_system_t application_framework_system_handle;

    kan_bool_t test_mode;
    kan_bool_t test_passed;
    kan_bool_t test_asset_loaded;
    kan_instance_size_t test_frames_count;
};

APPLICATION_FRAMEWORK_EXAMPLE_BASIC_LOGIC_API void kan_universe_mutator_deploy_test_mutator (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct test_mutator_state_t *state)
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
    state->test_asset_loaded = KAN_FALSE;
    state->test_frames_count = 0u;
}

APPLICATION_FRAMEWORK_EXAMPLE_BASIC_LOGIC_API void kan_universe_mutator_execute_test_mutator (
    kan_cpu_job_t job, struct test_mutator_state_t *state)
{
    KAN_UP_SINGLETON_WRITE (singleton, test_singleton_t)
    {
        if (!KAN_HANDLE_IS_VALID (singleton->window_handle))
        {
            singleton->window_handle =
                kan_application_system_window_create (state->application_system_handle, "Title placeholder", 600u, 400u,
                                                      KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_VULKAN);
            kan_application_system_window_raise (state->application_system_handle, singleton->window_handle);
        }

        if (!singleton->test_request_added)
        {
            KAN_UP_SINGLETON_READ (provider, kan_resource_provider_singleton_t)
            {
                KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
                {
                    request->request_id = kan_next_resource_request_id (provider);
                    request->type = kan_string_intern ("test_data_type_t");
                    request->name = kan_string_intern ("test");
                    request->priority = 0u;
                    singleton->test_request_id = request->request_id;
                }
            }

            singleton->test_request_added = KAN_TRUE;
        }

        kan_instance_size_t x = 0;
        kan_instance_size_t y = 0;

        KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &singleton->test_request_id)
        {
            if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
            {
                state->test_asset_loaded = KAN_TRUE;
                KAN_UP_VALUE_READ (view, resource_provider_container_test_data_type_t, container_id,
                                   &request->provided_container_id)
                {
                    struct test_data_type_t *loaded_resource =
                        (struct test_data_type_t *) ((struct kan_resource_container_view_t *) view)->data_begin;

                    x = loaded_resource->x;
                    y = loaded_resource->y;

                    if (x != 3u || y != 5u)
                    {
                        state->test_passed = KAN_FALSE;
                        KAN_LOG (application_framework_example_basic_logic_test_mode, KAN_LOG_INFO,
                                 "Unexpected x or y.")
                    }
                }
            }
        }

#define TITLE_BUFFER_SIZE 256u
        char buffer[TITLE_BUFFER_SIZE];

        KAN_UP_SINGLETON_READ (time, kan_time_singleton_t)
        {
            snprintf (buffer, TITLE_BUFFER_SIZE,
                      "Visual time: %.3f seconds. Visual delta: %.3f seconds. X: %llu. Y: %llu.",
                      (float) (time->visual_time_ns) / 1e9f, (float) (time->visual_delta_ns) / 1e9f,
                      (unsigned long long) x, (unsigned long long) y);
        }

        kan_application_system_window_set_title (state->application_system_handle, singleton->window_handle, buffer);
#undef TITLE_BUFFER_SIZE

        if (state->test_mode)
        {
            if (30u < ++state->test_frames_count)
            {
                KAN_LOG (application_framework_example_basic_logic_test_mode, KAN_LOG_INFO, "Shutting down...")
                if (!state->test_asset_loaded)
                {
                    state->test_passed = KAN_FALSE;
                    KAN_LOG (application_framework_example_basic_logic_test_mode, KAN_LOG_ERROR,
                             "Failed to load asset.")
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
