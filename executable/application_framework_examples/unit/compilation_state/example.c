#include <application_framework_examples_compilation_state_api.h>

#include <kan/container/dynamic_array.h>
#include <kan/context/all_system_names.h>
#include <kan/context/application_framework_system.h>
#include <kan/log/logging.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

KAN_LOG_DEFINE_CATEGORY (application_framework_examples_compilation_state);

struct numbers_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_serialized_size_t)
    struct kan_dynamic_array_t items;
};

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_STATE_API void numbers_init (struct numbers_t *numbers)
{
    kan_dynamic_array_init (&numbers->items, 0u, sizeof (kan_serialized_size_t), _Alignof (kan_serialized_size_t),
                            kan_allocation_group_stack_get ());
}

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_STATE_API void numbers_shutdown (struct numbers_t *numbers)
{
    kan_dynamic_array_shutdown (&numbers->items);
}

KAN_REFLECTION_STRUCT_META (numbers_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_STATE_API struct kan_resource_resource_type_meta_t
    numbers_resource_type_meta = {
        .root = KAN_TRUE,
};

static enum kan_resource_compile_result_t numbers_compile (struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (numbers_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_STATE_API struct kan_resource_compilable_meta_t numbers_compilable_meta = {
    .output_type_name = "numbers_compiled_t",
    .configuration_type_name = NULL,
    .state_type_name = "numbers_compilation_state_t",
    .functor = numbers_compile,
};

struct numbers_compilation_state_t
{
    kan_instance_size_t item_index;
};

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_STATE_API void numbers_compilation_state_init (
    struct numbers_compilation_state_t *instance)
{
    instance->item_index = 0u;
}

struct numbers_compiled_t
{
    kan_instance_size_t sum;
};

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_STATE_API void numbers_compiled_init (struct numbers_compiled_t *instance)
{
    instance->sum = 0u;
}

KAN_REFLECTION_STRUCT_META (numbers_compiled_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_STATE_API struct kan_resource_resource_type_meta_t
    numbers_compiled_resource_type_meta = {
        .root = KAN_TRUE,
};

static enum kan_resource_compile_result_t numbers_compile (struct kan_resource_compile_state_t *state)
{
    const struct numbers_t *input = state->input_instance;
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

struct compilation_state_singleton_t
{
    kan_bool_t requested_loaded_data;
    kan_bool_t loaded_test_data;
    kan_resource_request_id_t test_request_id;
};

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_STATE_API void compilation_state_singleton_init (
    struct compilation_state_singleton_t *instance)
{
    instance->requested_loaded_data = KAN_FALSE;
    instance->loaded_test_data = KAN_FALSE;
}

struct compilation_state_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (compilation_state)
    KAN_UP_BIND_STATE (compilation_state, state)

    kan_context_system_t application_framework_system_handle;
};

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_STATE_API void kan_universe_mutator_deploy_compilation_state (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct compilation_state_state_t *state)
{
    kan_context_t context = kan_universe_get_context (universe);
    state->application_framework_system_handle =
        kan_context_query (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME);
}

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_STATE_API void kan_universe_mutator_execute_compilation_state (
    kan_cpu_job_t job, struct compilation_state_state_t *state)
{
    KAN_UP_SINGLETON_READ (provider_singleton, kan_resource_provider_singleton_t)
    KAN_UP_SINGLETON_WRITE (singleton, compilation_state_singleton_t)
    {
        if (!provider_singleton->scan_done)
        {
            KAN_UP_MUTATOR_RETURN;
        }

        if (!singleton->loaded_test_data && !singleton->requested_loaded_data)
        {
            KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
            {
                request->request_id = kan_next_resource_request_id (provider_singleton);
                request->type = kan_string_intern ("numbers_compiled_t");
                request->name = kan_string_intern ("data");
                request->priority = 0u;
                singleton->test_request_id = request->request_id;
            }

            singleton->requested_loaded_data = KAN_TRUE;
        }

        if (!singleton->loaded_test_data && singleton->requested_loaded_data)
        {
            KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &singleton->test_request_id)
            {
                if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
                {
                    KAN_UP_VALUE_READ (view, KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (numbers_compiled_t),
                                       container_id, &request->provided_container_id)
                    {
                        const struct numbers_compiled_t *loaded_resource =
                            KAN_RESOURCE_PROVIDER_CONTAINER_GET (numbers_compiled_t, view);

                        if (loaded_resource->sum == 55u)
                        {
                            singleton->loaded_test_data = KAN_TRUE;
                        }
                        else
                        {
                            KAN_LOG (application_framework_examples_compilation_state, KAN_LOG_ERROR,
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

        if (singleton->loaded_test_data && KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
        {
            kan_application_framework_system_request_exit (state->application_framework_system_handle, 0);
        }
    }

    KAN_UP_MUTATOR_RETURN;
}
