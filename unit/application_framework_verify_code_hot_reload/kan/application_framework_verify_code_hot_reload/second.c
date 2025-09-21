#include <application_framework_verify_code_hot_reload_api.h>

#include <stdlib.h>

#include <kan/context/all_system_names.h>
#include <kan/context/application_framework_system.h>
#include <kan/context/hot_reload_coordination_system.h>
#include <kan/log/logging.h>
#include <kan/precise_time/precise_time.h>
#include <kan/universe/macro.h>
#include <kan/universe/universe.h>

KAN_LOG_DEFINE_CATEGORY (application_framework_verify_code_hot_reload);

struct verify_code_hot_reload_scheduler_state_t
{
    kan_instance_size_t stub;
};

APPLICATION_FRAMEWORK_VERIFY_CODE_HOT_RELOAD_API KAN_UM_SCHEDULER_EXECUTE (verify_code_hot_reload)
{
    // We need to close all accesses before running pipelines.
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern ("verify_code_hot_reload_update"));
}

struct verify_code_hot_reload_singleton_t
{
    kan_instance_size_t test_frame;
};

APPLICATION_FRAMEWORK_VERIFY_CODE_HOT_RELOAD_API void verify_code_hot_reload_singleton_init (
    struct verify_code_hot_reload_singleton_t *instance)
{
    instance->test_frame = 0u;
}

struct verify_code_hot_hot_reload_second_stage_singleton_t
{
    bool want_to_hot_reload;
};

APPLICATION_FRAMEWORK_VERIFY_CODE_HOT_RELOAD_API void verify_code_hot_hot_reload_second_stage_singleton_init (
    struct verify_code_hot_hot_reload_second_stage_singleton_t *instance)
{
    instance->want_to_hot_reload = false;
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

struct verify_code_hot_reload_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (verify_code_hot_reload)
    KAN_UM_BIND_STATE (verify_code_hot_reload, state)

    kan_context_system_t application_framework_system_handle;
    kan_context_system_t hot_reload_coordination_system_handle;
};

APPLICATION_FRAMEWORK_VERIFY_CODE_HOT_RELOAD_API KAN_UM_MUTATOR_DEPLOY (verify_code_hot_reload)
{
    kan_context_t context = kan_universe_get_context (universe);
    state->application_framework_system_handle =
        kan_context_query (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME);
    state->hot_reload_coordination_system_handle =
        kan_context_query (context, KAN_CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_NAME);
    KAN_LOG (application_framework_verify_code_hot_reload, KAN_LOG_INFO, "Deployed second stage.")
}

APPLICATION_FRAMEWORK_VERIFY_CODE_HOT_RELOAD_API KAN_UM_MUTATOR_EXECUTE (verify_code_hot_reload)
{
    KAN_UMI_SINGLETON_WRITE (singleton, verify_code_hot_reload_singleton_t)
    KAN_UMI_SINGLETON_WRITE (second_singleton, verify_code_hot_hot_reload_second_stage_singleton_t)

    if (second_singleton->want_to_hot_reload)
    {
        if (kan_hot_reload_coordination_system_is_executing (state->hot_reload_coordination_system_handle))
        {
            KAN_ASSERT (
                kan_application_framework_system_get_arguments_count (state->application_framework_system_handle) == 5)

            char **arguments =
                kan_application_framework_system_get_arguments (state->application_framework_system_handle);

            const char *cmake = arguments[1u];
            const char *build_directory = arguments[2u];
            const char *target = arguments[3u];
            const char *config = arguments[4u];

#define COMMAND_BUFFER_SIZE 4096u
            char command_buffer[COMMAND_BUFFER_SIZE];
            snprintf (command_buffer, COMMAND_BUFFER_SIZE,
                      "\"%s\" \"%s\" -DKAN_APPLICATION_FRAMEWORK_CHRV_SECOND_PASS=OFF", cmake, build_directory);
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
            }
#undef COMMAND_BUFFER_SIZE

            second_singleton->want_to_hot_reload = false;
            kan_hot_reload_coordination_system_finish (state->hot_reload_coordination_system_handle);
        }
        else if (!kan_hot_reload_coordination_system_is_scheduled (state->hot_reload_coordination_system_handle))
        {
            kan_hot_reload_coordination_system_schedule (state->hot_reload_coordination_system_handle);
        }

        return;
    }

    if (singleton->test_frame == 0u)
    {
        second_singleton->want_to_hot_reload = true;
        singleton->test_frame = 1u;
        return;
    }

    if (singleton->test_frame == 20u)
    {
        kan_loop_size_t count = 0u;
        KAN_UML_SEQUENCE_READ (shared_struct, some_shared_struct_t)
        {
            if (shared_struct->x != 11u || shared_struct->y != 15u || shared_struct->z != 42u)
            {
                KAN_LOG (application_framework_verify_code_hot_reload, KAN_LOG_ERROR,
                         "Shared struct is not as expected.")
                kan_application_framework_system_request_exit (state->application_framework_system_handle, -1);
            }

            ++count;
        }

        if (count == 0u)
        {
            KAN_LOG (application_framework_verify_code_hot_reload, KAN_LOG_ERROR, "Unable to find shared struct.")
            kan_application_framework_system_request_exit (state->application_framework_system_handle, -1);
        }
        else if (count > 1u)
        {
            KAN_LOG (application_framework_verify_code_hot_reload, KAN_LOG_ERROR,
                     "Multiple instances of shared struct.")
            kan_application_framework_system_request_exit (state->application_framework_system_handle, -1);
        }

        KAN_UML_SEQUENCE_READ (should_not_be_here, struct_that_will_be_added_t)
        {
            KAN_LOG (application_framework_verify_code_hot_reload, KAN_LOG_ERROR, "Found instance of added struct.")
            kan_application_framework_system_request_exit (state->application_framework_system_handle, -1);
        }
    }

    if (singleton->test_frame >= 16u)
    {
        if (singleton->test_frame > 30u)
        {
            kan_application_framework_system_request_exit (state->application_framework_system_handle, 0);
        }

        ++singleton->test_frame;
    }
}
