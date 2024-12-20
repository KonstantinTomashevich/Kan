#include <application_framework_example_compilation_basic_api.h>

#include <kan/container/dynamic_array.h>
#include <kan/context/all_system_names.h>
#include <kan/context/application_framework_system.h>
#include <kan/context/application_system.h>
#include <kan/log/logging.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

// \c_interface_scanner_disable
KAN_LOG_DEFINE_CATEGORY (application_framework_example_compilation_basic);
// \c_interface_scanner_enable

struct root_config_t
{
    kan_interned_string_t required_sum;
};

_Static_assert (_Alignof (struct root_config_t) <= _Alignof (kan_memory_size_t), "Alignment has expected value.");

// \meta reflection_struct_meta = "root_config_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BASIC_API struct kan_resource_resource_type_meta_t
    root_config_resource_type_meta = {
        .root = KAN_TRUE,
};

// \meta reflection_struct_field_meta = "root_config_t.required_sum"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BASIC_API struct kan_resource_reference_meta_t
    root_config_required_sum_reference_meta = {
        .type = "sum_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

struct sum_compiled_t
{
    kan_serialized_size_t value;
};

_Static_assert (_Alignof (struct sum_compiled_t) <= _Alignof (kan_memory_size_t), "Alignment has expected value.");

// \meta reflection_struct_meta = "sum_compiled_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BASIC_API struct kan_resource_resource_type_meta_t
    sum_compiled_resource_type_meta = {
        .root = KAN_FALSE,
};

struct sum_t
{
    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t records;

    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t sums;
};

_Static_assert (_Alignof (struct sum_t) == _Alignof (kan_memory_size_t), "Alignment has expected value.");

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BASIC_API void sum_init (struct sum_t *sum)
{
    kan_dynamic_array_init (&sum->records, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&sum->sums, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
}

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BASIC_API void sum_shutdown (struct sum_t *sum)
{
    kan_dynamic_array_shutdown (&sum->records);
    kan_dynamic_array_shutdown (&sum->sums);
}

static enum kan_resource_compile_result_t sum_compile (struct kan_resource_compile_state_t *state);

// \meta reflection_struct_meta = "sum_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BASIC_API struct kan_resource_resource_type_meta_t sum_resource_type_meta = {
    .root = KAN_FALSE,
};

// \meta reflection_struct_meta = "sum_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BASIC_API struct kan_resource_compilable_meta_t sum_compilable_meta = {
    .output_type_name = "sum_compiled_t",
    .configuration_type_name = NULL,
    .state_type_name = NULL,
    .functor = sum_compile,
};

// \meta reflection_struct_field_meta = "sum_t.records"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BASIC_API struct kan_resource_reference_meta_t sum_records_reference_meta = {
    .type = "record_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW,
};

// \meta reflection_struct_field_meta = "sum_t.sums"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BASIC_API struct kan_resource_reference_meta_t
    sum_required_sum_reference_meta = {
        .type = "sum_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED,
};

struct record_t
{
    kan_serialized_size_t value;
};

_Static_assert (_Alignof (struct record_t) <= _Alignof (kan_memory_size_t), "Alignment has expected value.");

// \meta reflection_struct_meta = "record_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BASIC_API struct kan_resource_resource_type_meta_t record_resource_type_meta =
    {
        .root = KAN_FALSE,
};

static enum kan_resource_compile_result_t sum_compile (struct kan_resource_compile_state_t *state)
{
    struct sum_compiled_t *compiled = (struct sum_compiled_t *) state->output_instance;
    compiled->value = 0u;

    const kan_interned_string_t interned_record_t = kan_string_intern ("record_t");
    const kan_interned_string_t interned_sum_compiled_t = kan_string_intern ("sum_compiled_t");

    // All dependencies are already collected from input into array,
    // therefore we're taking shortcut and not reading input.
    for (kan_instance_size_t dependency_index = 0u; dependency_index < state->dependencies_count; ++dependency_index)
    {
        struct kan_resource_compilation_dependency_t *dependency = &state->dependencies[dependency_index];
        if (dependency->type == interned_record_t)
        {
            const struct record_t *dependency_record = dependency->data;
            compiled->value += dependency_record->value;
        }
        else if (dependency->type == interned_sum_compiled_t)
        {
            const struct sum_compiled_t *dependency_sum = dependency->data;
            compiled->value += dependency_sum->value;
        }
        else
        {
            KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR,
                     "Compilation error: unknown dependency type \"%s\".", dependency->type)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }
    }

    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

struct basic_test_singleton_t
{
    kan_bool_t checked_entries;
    kan_bool_t requested_loaded_data;
    kan_bool_t loaded_test_data;
    kan_resource_request_id_t test_request_id;
};

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BASIC_API void basic_test_singleton_init (
    struct basic_test_singleton_t *instance)
{
    instance->checked_entries = KAN_FALSE;
    instance->requested_loaded_data = KAN_FALSE;
    instance->loaded_test_data = KAN_FALSE;
}

struct basic_mutator_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (basic_mutator)
    KAN_UP_BIND_STATE (basic_mutator, state)

    kan_context_system_t application_framework_system_handle;
};

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BASIC_API void kan_universe_mutator_deploy_basic_mutator (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct basic_mutator_state_t *state)
{
    kan_context_t context = kan_universe_get_context (universe);
    state->application_framework_system_handle =
        kan_context_query (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME);
}

static kan_bool_t is_entry_exists (struct basic_mutator_state_t *state,
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

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BASIC_API void kan_universe_mutator_execute_basic_mutator (
    kan_cpu_job_t job, struct basic_mutator_state_t *state)
{
    KAN_UP_SINGLETON_READ (provider_singleton, kan_resource_provider_singleton_t)
    KAN_UP_SINGLETON_WRITE (test_singleton, basic_test_singleton_t)
    {
        if (!provider_singleton->scan_done)
        {
            KAN_UP_MUTATOR_RETURN;
        }

        if (!test_singleton->checked_entries)
        {
            kan_bool_t everything_ok = KAN_TRUE;
            if (!is_entry_exists (state, kan_string_intern ("root_config_t"), kan_string_intern ("root_config")))
            {
                KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR, "\"root_config\" not found!")
                everything_ok = KAN_FALSE;
            }

            const kan_bool_t in_compiled_mode =
                is_entry_exists (state, kan_string_intern ("sum_compiled_t"), kan_string_intern ("sum_1_2_3"));

            if (in_compiled_mode)
            {
                if (is_entry_exists (state, kan_string_intern ("record_t"), kan_string_intern ("record_1")))
                {
                    KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR,
                             "\"record_1\" found in compiled mode!")
                    everything_ok = KAN_FALSE;
                }

                if (is_entry_exists (state, kan_string_intern ("record_t"), kan_string_intern ("record_2")))
                {
                    KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR,
                             "\"record_2\" found in compiled mode!")
                    everything_ok = KAN_FALSE;
                }

                if (is_entry_exists (state, kan_string_intern ("record_t"), kan_string_intern ("record_3")))
                {
                    KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR,
                             "\"record_3\" found in compiled mode!")
                    everything_ok = KAN_FALSE;
                }

                if (is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_1_2")))
                {
                    KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR,
                             "\"sum_1_2\" found in compiled mode!")
                    everything_ok = KAN_FALSE;
                }

                if (is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_1_2_3")))
                {
                    KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR,
                             "\"sum_1_2_3\" found in compiled mode!")
                    everything_ok = KAN_FALSE;
                }

                if (is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_1_3")))
                {
                    KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR,
                             "\"sum_1_3\" found in compiled mode!")
                    everything_ok = KAN_FALSE;
                }

                if (is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_2_3")))
                {
                    KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR,
                             "\"sum_2_3\" found in compiled mode!")
                    everything_ok = KAN_FALSE;
                }
            }
            else
            {
                if (!is_entry_exists (state, kan_string_intern ("record_t"), kan_string_intern ("record_1")))
                {
                    KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR, "\"record_1\" not found!")
                    everything_ok = KAN_FALSE;
                }

                if (!is_entry_exists (state, kan_string_intern ("record_t"), kan_string_intern ("record_2")))
                {
                    KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR, "\"record_2\" not found!")
                    everything_ok = KAN_FALSE;
                }

                if (!is_entry_exists (state, kan_string_intern ("record_t"), kan_string_intern ("record_3")))
                {
                    KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR, "\"record_3\" not found!")
                    everything_ok = KAN_FALSE;
                }

                if (!is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_1_2")))
                {
                    KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR, "\"sum_1_2\" not found!")
                    everything_ok = KAN_FALSE;
                }

                if (!is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_1_2_3")))
                {
                    KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR, "\"sum_1_2_3\" not found!")
                    everything_ok = KAN_FALSE;
                }

                if (!is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_1_3")))
                {
                    KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR, "\"sum_1_3\" not found!")
                    everything_ok = KAN_FALSE;
                }

                if (!is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_2_3")))
                {
                    KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR, "\"sum_2_3\" not found!")
                    everything_ok = KAN_FALSE;
                }
            }

            if (everything_ok)
            {
                test_singleton->checked_entries = KAN_TRUE;
            }
            else if (KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
            {
                kan_application_framework_system_request_exit (state->application_framework_system_handle, 1);
            }
        }

        if (test_singleton->checked_entries && !test_singleton->loaded_test_data &&
            !test_singleton->requested_loaded_data)
        {
            KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
            {
                request->request_id = kan_next_resource_request_id (provider_singleton);
                request->type = kan_string_intern ("sum_compiled_t");
                request->name = kan_string_intern ("sum_1_2_3");
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
                    KAN_UP_VALUE_READ (view, resource_provider_container_sum_compiled_t, container_id,
                                       &request->provided_container_id)
                    {
                        struct sum_compiled_t *loaded_resource =
                            (struct sum_compiled_t *) ((struct kan_resource_container_view_t *) view)->data_begin;

                        if (loaded_resource->value == 111u)
                        {
                            test_singleton->loaded_test_data = KAN_TRUE;
                        }
                        else
                        {
                            KAN_LOG (application_framework_example_compilation_basic, KAN_LOG_ERROR,
                                     "\"sum_1_2_3\" has incorrect data %llu.",
                                     (unsigned long long) loaded_resource->value)

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
