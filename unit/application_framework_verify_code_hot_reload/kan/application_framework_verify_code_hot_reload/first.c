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
};

struct struct_that_will_be_deleted_t
{
    kan_instance_size_t id;
};

struct verify_code_hot_reload_mutator_state_t
{
    struct kan_repository_singleton_write_query_t write__verify_code_hot_test_singleton;
    struct kan_repository_indexed_value_read_query_t read_value__struct_that_will_be_deleted__id;
    struct kan_repository_indexed_insert_query_t insert__some_shared_struct;
    struct kan_repository_indexed_insert_query_t insert__struct_that_will_be_deleted;

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
    KAN_LOG (application_framework_verify_code_hot_reload, KAN_LOG_INFO, "Deployed first stage.")
}

APPLICATION_FRAMEWORK_VERIFY_CODE_HOT_RELOAD_API void kan_universe_mutator_execute_verify_code_hot_reload (
    kan_cpu_job_t job, struct verify_code_hot_reload_mutator_state_t *state)
{
    struct kan_repository_singleton_write_access_t singleton_write_access =
        kan_repository_singleton_write_query_execute (&state->write__verify_code_hot_test_singleton);
    struct verify_code_hot_test_singleton_t *singleton =
        (struct verify_code_hot_test_singleton_t *) kan_repository_singleton_write_access_resolve (
            &singleton_write_access);

    if (singleton->test_frame == 10u)
    {
        // Insert some data for migration.
        struct kan_repository_indexed_insertion_package_t package =
            kan_repository_indexed_insert_query_execute (&state->insert__some_shared_struct);

        struct some_shared_struct_t *first_data =
            (struct some_shared_struct_t *) kan_repository_indexed_insertion_package_get (&package);
        first_data->x = 11u;
        first_data->y = 15u;
        kan_repository_indexed_insertion_package_submit (&package);

        package = kan_repository_indexed_insert_query_execute (&state->insert__struct_that_will_be_deleted);

        struct struct_that_will_be_deleted_t *second_data =
            (struct struct_that_will_be_deleted_t *) kan_repository_indexed_insertion_package_get (&package);
        second_data->id = 49u;
        kan_repository_indexed_insertion_package_submit (&package);
    }

    if (singleton->test_frame == 15u &&
        // If reload was somehow skipped, request it again.
        kan_precise_time_get_elapsed_nanoseconds () - singleton->reload_request_time > 500000000u)
    {
        // Initiate hot reload.
        KAN_ASSERT (kan_application_framework_system_get_arguments_count (state->application_framework_system_handle) ==
                    5)
        char **arguments = kan_application_framework_system_get_arguments (state->application_framework_system_handle);

        const char *cmake = arguments[1u];
        const char *build_directory = arguments[2u];
        const char *target = arguments[3u];
        const char *config = arguments[4u];

#define COMMAND_BUFFER_SIZE 4096u
        char command_buffer[COMMAND_BUFFER_SIZE];
        snprintf (command_buffer, COMMAND_BUFFER_SIZE, "\"%s\" \"%s\" -DKAN_APPLICATION_FRAMEWORK_CHRV_SECOND_PASS=ON",
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
    else if (singleton->test_frame < 15u)
    {
        ++singleton->test_frame;
    }

    kan_repository_singleton_write_access_close (&singleton_write_access);
    kan_cpu_job_release (job);
}
