#include <application_framework_examples_compilation_basic_api.h>

#include <kan/container/dynamic_array.h>
#include <kan/context/all_system_names.h>
#include <kan/context/application_framework_system.h>
#include <kan/log/logging.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/universe/macro.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

KAN_LOG_DEFINE_CATEGORY (application_framework_examples_compilation_basic);

struct root_config_t
{
    kan_interned_string_t required_sum;
};

KAN_REFLECTION_STRUCT_META (root_config_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BASIC_API struct kan_resource_resource_type_meta_t
    root_config_resource_type_meta = {
        .root = true,
};

KAN_REFLECTION_STRUCT_FIELD_META (root_config_t, required_sum)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BASIC_API struct kan_resource_reference_meta_t
    root_config_required_sum_reference_meta = {
        .type = "sum_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

struct sum_compiled_t
{
    kan_serialized_size_t value;
};

KAN_REFLECTION_STRUCT_META (sum_compiled_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BASIC_API struct kan_resource_resource_type_meta_t
    sum_compiled_resource_type_meta = {
        .root = false,
};

struct sum_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t records;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t sums;
};

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BASIC_API void sum_init (struct sum_t *sum)
{
    kan_dynamic_array_init (&sum->records, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&sum->sums, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
}

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BASIC_API void sum_shutdown (struct sum_t *sum)
{
    kan_dynamic_array_shutdown (&sum->records);
    kan_dynamic_array_shutdown (&sum->sums);
}

static enum kan_resource_compile_result_t sum_compile (struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (sum_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BASIC_API struct kan_resource_resource_type_meta_t sum_resource_type_meta = {
    .root = false,
};

KAN_REFLECTION_STRUCT_META (sum_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BASIC_API struct kan_resource_compilable_meta_t sum_compilable_meta = {
    .output_type_name = "sum_compiled_t",
    .configuration_type_name = NULL,
    .state_type_name = NULL,
    .functor = sum_compile,
};

KAN_REFLECTION_STRUCT_FIELD_META (sum_t, records)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BASIC_API struct kan_resource_reference_meta_t sum_records_reference_meta = {
    .type = "record_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW,
};

KAN_REFLECTION_STRUCT_FIELD_META (sum_t, sums)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BASIC_API struct kan_resource_reference_meta_t
    sum_required_sum_reference_meta = {
        .type = "sum_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED,
};

struct record_t
{
    kan_serialized_size_t value;
};

KAN_REFLECTION_STRUCT_META (record_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BASIC_API struct kan_resource_resource_type_meta_t
    record_resource_type_meta = {
        .root = false,
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
            KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR,
                     "Compilation error: unknown dependency type \"%s\".", dependency->type)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }
    }

    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

struct example_compilation_basic_singleton_t
{
    bool checked_entries;
    bool requested_loaded_data;
    bool loaded_test_data;
    kan_resource_request_id_t test_request_id;
};

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BASIC_API void example_compilation_basic_singleton_init (
    struct example_compilation_basic_singleton_t *instance)
{
    instance->checked_entries = false;
    instance->requested_loaded_data = false;
    instance->loaded_test_data = false;
}

struct compilation_basic_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (compilation_basic)
    KAN_UM_BIND_STATE (compilation_basic, state)

    kan_context_system_t application_framework_system_handle;
};

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BASIC_API KAN_UM_MUTATOR_DEPLOY (compilation_basic)
{
    kan_context_t context = kan_universe_get_context (universe);
    state->application_framework_system_handle =
        kan_context_query (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME);
}

static bool is_entry_exists (struct compilation_basic_state_t *state,
                             kan_interned_string_t type,
                             kan_interned_string_t name)
{
    KAN_UML_VALUE_READ (entry, kan_resource_native_entry_t, name, &name)
    {
        if (entry->type == type)
        {
            return true;
        }
    }

    return false;
}

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BASIC_API KAN_UM_MUTATOR_EXECUTE (compilation_basic)
{
    KAN_UMI_SINGLETON_READ (provider_singleton, kan_resource_provider_singleton_t)
    KAN_UMI_SINGLETON_WRITE (singleton, example_compilation_basic_singleton_t)

    if (!provider_singleton->scan_done)
    {
        return;
    }

    if (!singleton->checked_entries)
    {
        bool everything_ok = true;
        if (!is_entry_exists (state, kan_string_intern ("root_config_t"), kan_string_intern ("root_config")))
        {
            KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR, "\"root_config\" not found!")
            everything_ok = false;
        }

        const bool in_compiled_mode =
            is_entry_exists (state, kan_string_intern ("sum_compiled_t"), kan_string_intern ("sum_1_2_3"));

        if (in_compiled_mode)
        {
            if (is_entry_exists (state, kan_string_intern ("record_t"), kan_string_intern ("record_1")))
            {
                KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR,
                         "\"record_1\" found in compiled mode!")
                everything_ok = false;
            }

            if (is_entry_exists (state, kan_string_intern ("record_t"), kan_string_intern ("record_2")))
            {
                KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR,
                         "\"record_2\" found in compiled mode!")
                everything_ok = false;
            }

            if (is_entry_exists (state, kan_string_intern ("record_t"), kan_string_intern ("record_3")))
            {
                KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR,
                         "\"record_3\" found in compiled mode!")
                everything_ok = false;
            }

            if (is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_1_2")))
            {
                KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR,
                         "\"sum_1_2\" found in compiled mode!")
                everything_ok = false;
            }

            if (is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_1_2_3")))
            {
                KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR,
                         "\"sum_1_2_3\" found in compiled mode!")
                everything_ok = false;
            }

            if (is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_1_3")))
            {
                KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR,
                         "\"sum_1_3\" found in compiled mode!")
                everything_ok = false;
            }

            if (is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_2_3")))
            {
                KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR,
                         "\"sum_2_3\" found in compiled mode!")
                everything_ok = false;
            }
        }
        else
        {
            if (!is_entry_exists (state, kan_string_intern ("record_t"), kan_string_intern ("record_1")))
            {
                KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR, "\"record_1\" not found!")
                everything_ok = false;
            }

            if (!is_entry_exists (state, kan_string_intern ("record_t"), kan_string_intern ("record_2")))
            {
                KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR, "\"record_2\" not found!")
                everything_ok = false;
            }

            if (!is_entry_exists (state, kan_string_intern ("record_t"), kan_string_intern ("record_3")))
            {
                KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR, "\"record_3\" not found!")
                everything_ok = false;
            }

            if (!is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_1_2")))
            {
                KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR, "\"sum_1_2\" not found!")
                everything_ok = false;
            }

            if (!is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_1_2_3")))
            {
                KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR, "\"sum_1_2_3\" not found!")
                everything_ok = false;
            }

            if (!is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_1_3")))
            {
                KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR, "\"sum_1_3\" not found!")
                everything_ok = false;
            }

            if (!is_entry_exists (state, kan_string_intern ("sum_t"), kan_string_intern ("sum_2_3")))
            {
                KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR, "\"sum_2_3\" not found!")
                everything_ok = false;
            }
        }

        if (everything_ok)
        {
            singleton->checked_entries = true;
        }
        else if (KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
        {
            kan_application_framework_system_request_exit (state->application_framework_system_handle, 1);
        }
    }

    if (singleton->checked_entries && !singleton->loaded_test_data && !singleton->requested_loaded_data)
    {
        KAN_UMO_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (provider_singleton);
            request->type = kan_string_intern ("sum_compiled_t");
            request->name = kan_string_intern ("sum_1_2_3");
            request->priority = 0u;
            singleton->test_request_id = request->request_id;
        }

        singleton->requested_loaded_data = true;
    }

    if (singleton->checked_entries && !singleton->loaded_test_data && singleton->requested_loaded_data)
    {
        KAN_UMI_VALUE_READ_REQUIRED (request, kan_resource_request_t, request_id, &singleton->test_request_id)
        if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
        {
            KAN_UMI_VALUE_READ_REQUIRED (view, KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (sum_compiled_t), container_id,
                                         &request->provided_container_id)

            const struct sum_compiled_t *loaded_resource = KAN_RESOURCE_PROVIDER_CONTAINER_GET (sum_compiled_t, view);
            if (loaded_resource->value == 111u)
            {
                singleton->loaded_test_data = true;
            }
            else
            {
                KAN_LOG (application_framework_examples_compilation_basic, KAN_LOG_ERROR,
                         "\"sum_1_2_3\" has incorrect data %llu.", (unsigned long long) loaded_resource->value)

                if (KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
                {
                    kan_application_framework_system_request_exit (state->application_framework_system_handle, 1);
                }
            }
        }
    }

    if (singleton->checked_entries && singleton->loaded_test_data &&
        KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
    {
        kan_application_framework_system_request_exit (state->application_framework_system_handle, 0);
    }
}
