#include <application_framework_verify_code_hot_reload_api.h>

#include <stdlib.h>

#include <kan/context/all_system_names.h>
#include <kan/context/application_framework_system.h>
#include <kan/log/logging.h>
#include <kan/precise_time/precise_time.h>
#include <kan/universe/universe.h>

KAN_LOG_DEFINE_CATEGORY (application_framework_verify_code_hot_reload);

struct verify_code_hot_reload_scheduler_state_t
{
    kan_instance_size_t stub;
};

APPLICATION_FRAMEWORK_VERIFY_CODE_HOT_RELOAD_API void kan_universe_scheduler_execute_verify_code_hot_reload (
    kan_universe_scheduler_interface_t interface, struct verify_code_hot_reload_scheduler_state_t *state)
{
    // We need to close all accesses before running pipelines.
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern ("verify_code_hot_reload_update"));
}

struct verify_code_hot_test_singleton_t
{
    kan_instance_size_t test_frame;
    kan_time_size_t reload_request_time;
};

APPLICATION_FRAMEWORK_VERIFY_CODE_HOT_RELOAD_API void verify_code_hot_test_singleton_init (
    struct verify_code_hot_test_singleton_t *instance)
{
    instance->test_frame = 0u;
    instance->reload_request_time = 0u;
}

struct some_shared_struct_t
{
    kan_instance_size_t x;
    kan_instance_size_t y;
    kan_instance_size_t z;
};

APPLICATION_FRAMEWORK_VERIFY_CODE_HOT_RELOAD_API void some_shared_struct_init (struct some_shared_struct_t *instance)
{
    instance->x = 0u;
    instance->y = 0u;
    instance->z = 42u;
}

struct struct_that_will_be_added_t
{
    kan_instance_size_t id_some;
};

struct verify_code_hot_reload_mutator_state_t
{
    struct kan_repository_singleton_write_query_t write__verify_code_hot_test_singleton;
    struct kan_repository_indexed_sequence_read_query_t read_sequence__some_shared_struct;
    struct kan_repository_indexed_value_read_query_t read_value__struct_that_will_be_added__id_some;
    struct kan_repository_indexed_sequence_read_query_t read_sequence__struct_that_will_be_added;

    kan_context_system_t application_framework_system_handle;
};

APPLICATION_FRAMEWORK_VERIFY_CODE_HOT_RELOAD_API void kan_universe_mutator_deploy_verify_code_hot_reload (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct verify_code_hot_reload_mutator_state_t *state)
{
    kan_context_t context = kan_universe_get_context (universe);
    state->application_framework_system_handle =
        kan_context_query (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME);
    KAN_LOG (application_framework_verify_code_hot_reload, KAN_LOG_INFO, "Deployed second stage.")
}

APPLICATION_FRAMEWORK_VERIFY_CODE_HOT_RELOAD_API void kan_universe_mutator_execute_verify_code_hot_reload (
    kan_cpu_job_t job, struct verify_code_hot_reload_mutator_state_t *state)
{
    struct kan_repository_singleton_write_access_t singleton_write_access =
        kan_repository_singleton_write_query_execute (&state->write__verify_code_hot_test_singleton);
    struct verify_code_hot_test_singleton_t *singleton =
        (struct verify_code_hot_test_singleton_t *) kan_repository_singleton_write_access_resolve (
            &singleton_write_access);

    if (singleton->test_frame == 20u)
    {
        struct kan_repository_indexed_sequence_read_cursor_t cursor =
            kan_repository_indexed_sequence_read_query_execute (&state->read_sequence__some_shared_struct);

        struct kan_repository_indexed_sequence_read_access_t access =
            kan_repository_indexed_sequence_read_cursor_next (&cursor);

        const struct some_shared_struct_t *shared_struct =
            kan_repository_indexed_sequence_read_access_resolve (&access);

        if (!shared_struct || shared_struct->x != 11u || shared_struct->y != 15u || shared_struct->z != 42u)
        {
            KAN_LOG (application_framework_verify_code_hot_reload, KAN_LOG_ERROR, "Shared struct is not as expected.")
            kan_application_framework_system_request_exit (state->application_framework_system_handle, -1);
        }

        kan_repository_indexed_sequence_read_access_close (&access);
        access = kan_repository_indexed_sequence_read_cursor_next (&cursor);

        if (kan_repository_indexed_sequence_read_access_resolve (&access))
        {
            KAN_LOG (application_framework_verify_code_hot_reload, KAN_LOG_ERROR,
                     "Multiple instances of shared struct.")
            kan_application_framework_system_request_exit (state->application_framework_system_handle, -1);
        }

        kan_repository_indexed_sequence_read_cursor_close (&cursor);
        cursor = kan_repository_indexed_sequence_read_query_execute (&state->read_sequence__struct_that_will_be_added);
        access = kan_repository_indexed_sequence_read_cursor_next (&cursor);

        if (kan_repository_indexed_sequence_read_access_resolve (&access))
        {
            KAN_LOG (application_framework_verify_code_hot_reload, KAN_LOG_ERROR, "Found instance of added struct.")
            kan_application_framework_system_request_exit (state->application_framework_system_handle, -1);
        }

        kan_repository_indexed_sequence_read_cursor_close (&cursor);
    }

    if (singleton->test_frame == 0u &&
        // If reload was somehow skipped, request it again.
        kan_precise_time_get_elapsed_nanoseconds () - singleton->reload_request_time > 500000000u)
    {
        // Started in second state, reload back to first.
        KAN_ASSERT (kan_application_framework_system_get_arguments_count (state->application_framework_system_handle) ==
                    5)
        char **arguments = kan_application_framework_system_get_arguments (state->application_framework_system_handle);

        const char *cmake = arguments[1u];
        const char *build_directory = arguments[2u];
        const char *target = arguments[3u];
        const char *config = arguments[4u];

#define COMMAND_BUFFER_SIZE 4096u
        char command_buffer[COMMAND_BUFFER_SIZE];
        snprintf (command_buffer, COMMAND_BUFFER_SIZE, "\"%s\" \"%s\" -DKAN_APPLICATION_FRAMEWORK_CHRV_SECOND_PASS=OFF",
                  cmake, build_directory);
        int result = system (command_buffer);

        if (result != 0)
        {
            KAN_LOG (application_framework_verify_code_hot_reload, KAN_LOG_ERROR, "Failed to regenerate CMake.")
            kan_application_framework_system_request_exit (state->application_framework_system_handle, -1);
        }

        if (result == 0)
        {
            snprintf (command_buffer, COMMAND_BUFFER_SIZE, "\"%s\" --build \"%s\" --target \"%s\" --config \"%s\"",
                      cmake, build_directory, target, config);
            result = system (command_buffer);

            if (result != 0)
            {
                KAN_LOG (application_framework_verify_code_hot_reload, KAN_LOG_ERROR, "Failed to rebuild plugins.")
                kan_application_framework_system_request_exit (state->application_framework_system_handle, -1);
            }
            else
            {
                singleton->reload_request_time = kan_precise_time_get_elapsed_nanoseconds ();
            }
        }

#undef COMMAND_BUFFER_SIZE
    }
    else if (singleton->test_frame >= 15u)
    {
        if (singleton->test_frame > 30u)
        {
            kan_application_framework_system_request_exit (state->application_framework_system_handle, 0);
        }

        ++singleton->test_frame;
    }

    kan_repository_singleton_write_access_close (&singleton_write_access);
    kan_cpu_job_release (job);
}
