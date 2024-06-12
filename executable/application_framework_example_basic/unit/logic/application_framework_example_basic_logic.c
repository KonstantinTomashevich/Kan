#include <application_framework_example_basic_logic_api.h>

#include <stdio.h>
#include <string.h>

#include <kan/context/application_framework_system.h>
#include <kan/context/application_system.h>
#include <kan/log/logging.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>
#include <kan/universe_time/universe_time.h>

// \c_interface_scanner_disable
KAN_LOG_DEFINE_CATEGORY (application_framework_example_basic_logic_test_mode);
// \c_interface_scanner_enable

struct test_data_type_t
{
    uint64_t x;
    uint64_t y;
};

_Static_assert (_Alignof (struct test_data_type_t) == _Alignof (uint64_t), "Alignment has expected value.");

// \meta reflection_struct_meta = "test_data_type_t"
APPLICATION_FRAMEWORK_EXAMPLE_BASIC_LOGIC_API struct kan_resource_pipeline_resource_type_meta_t test_data_type_meta = {
    .root = KAN_TRUE,
    .compilation_output_type_name = NULL,
    .compile = NULL,
};

struct test_singleton_t
{
    kan_application_system_window_handle_t window_handle;
    kan_bool_t test_request_added;
    uint64_t test_request_id;
};

APPLICATION_FRAMEWORK_EXAMPLE_BASIC_LOGIC_API void test_singleton_init (struct test_singleton_t *instance)
{
    instance->window_handle = KAN_INVALID_APPLICATION_SYSTEM_WINDOW_HANDLE;
    instance->test_request_added = KAN_FALSE;
}

struct test_mutator_state_t
{
    struct kan_repository_singleton_write_query_t write__test_singleton;
    struct kan_repository_singleton_read_query_t read__kan_time_singleton;

    struct kan_repository_singleton_read_query_t read__kan_resource_provider_singleton;
    struct kan_repository_indexed_insert_query_t insert__kan_resource_request;
    struct kan_repository_indexed_value_read_query_t read_value__kan_resource_request__request_id;
    struct kan_repository_indexed_value_read_query_t
        read_value__resource_provider_container_test_data_type__container_id;

    kan_context_system_handle_t application_system_handle;
    kan_context_system_handle_t application_framework_system_handle;

    kan_bool_t test_mode;
    kan_bool_t test_passed;
    kan_bool_t test_asset_loaded;
    uint64_t test_frames_count;
};

APPLICATION_FRAMEWORK_EXAMPLE_BASIC_LOGIC_API void kan_universe_mutator_deploy_test_mutator (
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

    if (state->application_framework_system_handle != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
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
    kan_repository_singleton_write_access_t write_access =
        kan_repository_singleton_write_query_execute (&state->write__test_singleton);
    struct test_singleton_t *singleton =
        (struct test_singleton_t *) kan_repository_singleton_write_access_resolve (write_access);

    if (singleton->window_handle == KAN_INVALID_APPLICATION_SYSTEM_WINDOW_HANDLE)
    {
        singleton->window_handle = kan_application_system_window_create (
            state->application_system_handle, "Title placeholder", 600u, 400u,
            KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_VULKAN | KAN_PLATFORM_WINDOW_FLAG_RESIZABLE);
        kan_application_system_window_raise (state->application_system_handle, singleton->window_handle);
    }

    if (!singleton->test_request_added)
    {
        kan_repository_singleton_read_access_t provider_access =
            kan_repository_singleton_read_query_execute (&state->read__kan_resource_provider_singleton);

        const struct kan_resource_provider_singleton_t *provider =
            kan_repository_singleton_read_access_resolve (provider_access);

        struct kan_repository_indexed_insertion_package_t package =
            kan_repository_indexed_insert_query_execute (&state->insert__kan_resource_request);
        struct kan_resource_request_t *request = kan_repository_indexed_insertion_package_get (&package);

        request->request_id = kan_next_resource_request_id (provider);
        request->type = kan_string_intern ("test_data_type_t");
        request->name = kan_string_intern ("test");
        request->priority = 0u;
        singleton->test_request_id = request->request_id;
        kan_repository_indexed_insertion_package_submit (&package);

        kan_repository_singleton_read_access_close (provider_access);
        singleton->test_request_added = KAN_TRUE;
    }

    uint64_t x = 0;
    uint64_t y = 0;

    struct kan_repository_indexed_value_read_cursor_t request_cursor = kan_repository_indexed_value_read_query_execute (
        &state->read_value__kan_resource_request__request_id, &singleton->test_request_id);

    struct kan_repository_indexed_value_read_access_t request_access =
        kan_repository_indexed_value_read_cursor_next (&request_cursor);

    const struct kan_resource_request_t *request = kan_repository_indexed_value_read_access_resolve (&request_access);
    KAN_ASSERT (request)

    if (request->provided_container_id != KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)
    {
        state->test_asset_loaded = KAN_TRUE;
        struct kan_repository_indexed_value_read_cursor_t container_cursor =
            kan_repository_indexed_value_read_query_execute (
                &state->read_value__resource_provider_container_test_data_type__container_id,
                &request->provided_container_id);

        struct kan_repository_indexed_value_read_access_t container_access =
            kan_repository_indexed_value_read_cursor_next (&container_cursor);

        const struct kan_resource_container_view_t *view =
            kan_repository_indexed_value_read_access_resolve (&container_access);
        KAN_ASSERT (view)

        if (view)
        {
            struct test_data_type_t *loaded_resource = (struct test_data_type_t *) view->data_begin;
            x = loaded_resource->x;
            y = loaded_resource->y;

            if (x != 3u || y != 5u)
            {
                state->test_passed = KAN_FALSE;
                KAN_LOG (application_framework_example_basic_logic_test_mode, KAN_LOG_INFO, "Unexpected x or y.")
            }

            kan_repository_indexed_value_read_access_close (&container_access);
        }

        kan_repository_indexed_value_read_cursor_close (&container_cursor);
    }

    kan_repository_indexed_value_read_access_close (&request_access);
    kan_repository_indexed_value_read_cursor_close (&request_cursor);

    kan_repository_singleton_read_access_t time_access =
        kan_repository_singleton_read_query_execute (&state->read__kan_time_singleton);
    const struct kan_time_singleton_t *time = kan_repository_singleton_read_access_resolve (time_access);

#define TITLE_BUFFER_SIZE 256u
    char buffer[TITLE_BUFFER_SIZE];
    snprintf (buffer, TITLE_BUFFER_SIZE, "Visual time: %.3f seconds. Visual delta: %.3f seconds. X: %llu. Y: %llu.",
              (float) (time->visual_time_ns) / 1e9f, (float) (time->visual_delta_ns) / 1e9f, (unsigned long long) x,
              (unsigned long long) y);
    kan_application_system_window_set_title (state->application_system_handle, singleton->window_handle, buffer);
#undef TITLE_BUFFER_SIZE

    kan_repository_singleton_read_access_close (time_access);
    kan_repository_singleton_write_access_close (write_access);

    if (state->test_mode)
    {
        if (30u < ++state->test_frames_count)
        {
            KAN_LOG (application_framework_example_basic_logic_test_mode, KAN_LOG_INFO, "Shutting down...")
            if (!state->test_asset_loaded)
            {
                state->test_passed = KAN_FALSE;
                KAN_LOG (application_framework_example_basic_logic_test_mode, KAN_LOG_ERROR, "Failed to load asset.")
            }

            KAN_ASSERT (state->application_framework_system_handle != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
            if (state->test_passed)
            {
                kan_application_framework_system_request_exit (state->application_framework_system_handle, 0);
            }
            else
            {
                kan_application_framework_system_request_exit (state->application_framework_system_handle, -1);
            }
        }
    }

    kan_cpu_job_release (job);
}
