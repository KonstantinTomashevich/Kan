#include <application_framework_example_compilation_state_api.h>

#include <kan/container/dynamic_array.h>
#include <kan/context/application_framework_system.h>
#include <kan/log/logging.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

// \c_interface_scanner_disable
KAN_LOG_DEFINE_CATEGORY (application_framework_example_compilation_state);
// \c_interface_scanner_enable

struct numbers_t
{
    /// \meta reflection_dynamic_array_type = "kan_serialized_size_t"
    struct kan_dynamic_array_t items;
};

_Static_assert (_Alignof (struct numbers_t) <= _Alignof (kan_memory_size_t), "Alignment has expected value.");

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_STATE_API void numbers_init (struct numbers_t *numbers)
{
    kan_dynamic_array_init (&numbers->items, 0u, sizeof (kan_serialized_size_t), _Alignof (kan_serialized_size_t),
                            kan_allocation_group_stack_get ());
}

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_STATE_API void numbers_shutdown (struct numbers_t *numbers)
{
    kan_dynamic_array_shutdown (&numbers->items);
}

// \meta reflection_struct_meta = "numbers_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_STATE_API struct kan_resource_resource_type_meta_t
    numbers_resource_type_meta = {
        .root = KAN_TRUE,
};

static enum kan_resource_compile_result_t numbers_compile (struct kan_resource_compile_state_t *state);

// \meta reflection_struct_meta = "numbers_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_STATE_API struct kan_resource_compilable_meta_t numbers_compilable_meta = {
    .output_type_name = "numbers_compiled_t",
    .configuration_type_name = NULL,
    .state_type_name = "numbers_compilation_state_t",
    .functor = numbers_compile,
};

struct numbers_compilation_state_t
{
    kan_instance_size_t item_index;
};

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_STATE_API void numbers_compilation_state_init (
    struct numbers_compilation_state_t *instance)
{
    instance->item_index = 0u;
}

struct numbers_compiled_t
{
    kan_instance_size_t sum;
};

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_STATE_API void numbers_compiled_init (struct numbers_compiled_t *instance)
{
    instance->sum = 0u;
}

// \meta reflection_struct_meta = "numbers_compiled_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_STATE_API struct kan_resource_resource_type_meta_t
    numbers_compiled_resource_type_meta = {
        .root = KAN_TRUE,
};

static enum kan_resource_compile_result_t numbers_compile (struct kan_resource_compile_state_t *state)
{
    struct numbers_t *input = state->input_instance;
    struct numbers_compiled_t *output = state->output_instance;
    struct numbers_compilation_state_t *user_state = state->user_state;

    if (user_state->item_index >= input->items.size)
    {
        return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
    }

    output->sum += ((kan_serialized_size_t *) input->items.data)[user_state->item_index];
    ++user_state->item_index;

    // Just for the sake of testing, we return after every operation, not after deadline.
    return KAN_RESOURCE_PIPELINE_COMPILE_IN_PROGRESS;
}

struct state_test_singleton_t
{
    kan_bool_t checked_entries;
    kan_bool_t requested_loaded_data;
    kan_bool_t loaded_test_data;
    kan_resource_request_id_t test_request_id;
};

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_STATE_API void state_test_singleton_init (
    struct state_test_singleton_t *instance)
{
    instance->checked_entries = KAN_FALSE;
    instance->requested_loaded_data = KAN_FALSE;
    instance->loaded_test_data = KAN_FALSE;
}

struct state_mutator_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (state_mutator)
    KAN_UP_BIND_STATE (state_mutator, state)

    kan_context_system_t application_framework_system_handle;
};

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_STATE_API void kan_universe_mutator_deploy_state_mutator (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct state_mutator_state_t *state)
{
    kan_context_t context = kan_universe_get_context (universe);
    state->application_framework_system_handle =
        kan_context_query (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME);
}

static kan_bool_t is_entry_exists (struct state_mutator_state_t *state,
                                   kan_interned_string_t type,
                                   kan_interned_string_t name)
{
    KAN_UP_VALUE_READ (entry, kan_resource_native_entry_t, name, &name)
    {
        if (entry->type == type)
        {
            KAN_UP_QUERY_RETURN_VALUE (kan_bool_t, KAN_TRUE);
        }
    }

    return KAN_FALSE;
}

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_STATE_API void kan_universe_mutator_execute_state_mutator (
    kan_cpu_job_t job, struct state_mutator_state_t *state)
{
    KAN_UP_SINGLETON_READ (provider_singleton, kan_resource_provider_singleton_t)
    KAN_UP_SINGLETON_WRITE (test_singleton, state_test_singleton_t)
    {
        if (!provider_singleton->scan_done)
        {
            KAN_UP_MUTATOR_RETURN;
        }

        if (!test_singleton->checked_entries)
        {
            if (!is_entry_exists (state, kan_string_intern ("numbers_compiled_t"), kan_string_intern ("data")))
            {
                // We're in development mode and there is no runtime compilation as of now. Just exit, then.
                if (KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
                {
                    kan_application_framework_system_request_exit (state->application_framework_system_handle, 0);
                }

                KAN_UP_MUTATOR_RETURN;
            }

            test_singleton->checked_entries = KAN_TRUE;
        }

        if (test_singleton->checked_entries && !test_singleton->loaded_test_data &&
            !test_singleton->requested_loaded_data)
        {
            KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
            {
                request->request_id = kan_next_resource_request_id (provider_singleton);
                request->type = kan_string_intern ("numbers_compiled_t");
                request->name = kan_string_intern ("data");
                request->priority = 0u;
                test_singleton->test_request_id = request->request_id;
            }

            test_singleton->requested_loaded_data = KAN_TRUE;
        }

        if (test_singleton->checked_entries && !test_singleton->loaded_test_data &&
            test_singleton->requested_loaded_data)
        {
            KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &test_singleton->test_request_id)
            {
                if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
                {
                    KAN_UP_VALUE_READ (view, resource_provider_container_numbers_compiled_t, container_id,
                                       &request->provided_container_id)
                    {
                        struct numbers_compiled_t *loaded_resource =
                            (struct numbers_compiled_t *) ((struct kan_resource_container_view_t *) view)->data_begin;

                        if (loaded_resource->sum == 55u)
                        {
                            test_singleton->loaded_test_data = KAN_TRUE;
                        }
                        else
                        {
                            KAN_LOG (application_framework_example_compilation_state, KAN_LOG_ERROR,
                                     "\"data\" has incorrect data %llu.", (unsigned long long) loaded_resource->sum)

                            if (KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
                            {
                                kan_application_framework_system_request_exit (
                                    state->application_framework_system_handle, 1);
                            }
                        }
                    }
                }
            }
        }

        if (test_singleton->checked_entries && test_singleton->loaded_test_data &&
            KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
        {
            kan_application_framework_system_request_exit (state->application_framework_system_handle, 0);
        }
    }

    KAN_UP_MUTATOR_RETURN;
}
