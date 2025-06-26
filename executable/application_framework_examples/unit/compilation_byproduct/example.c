#include <application_framework_examples_compilation_byproduct_api.h>

#include <string.h>

#include <qsort.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/mute_warnings.h>
#include <kan/container/dynamic_array.h>
#include <kan/context/all_system_names.h>
#include <kan/context/application_framework_system.h>
#include <kan/log/logging.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/universe/macro.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

#include <examples/platform_configuration.h>

KAN_LOG_DEFINE_CATEGORY (application_framework_examples_compilation_byproduct);

// Shader source byproducts are created for every source separately.
// We don't want to parse one shader code several times, therefore we use byproducts to merge everything.

struct shader_source_byproduct_t
{
    kan_interned_string_t source;
};

KAN_REFLECTION_STRUCT_META (shader_source_byproduct_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API struct kan_resource_byproduct_type_meta_t
    shader_source_byproduct_byproduct_type_meta = {
        .hash = NULL,
        .is_equal = NULL,
        .move = NULL,
        .reset = NULL,
};

static enum kan_resource_compile_result_t shader_source_byproduct_compile (struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (shader_source_byproduct_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API struct kan_resource_compilable_meta_t
    shader_source_byproduct_compilable_meta = {
        .output_type_name = "shader_source_byproduct_compiled_t",
        .configuration_type_name = NULL,
        .state_type_name = NULL,
        .functor = shader_source_byproduct_compile,
};

KAN_REFLECTION_STRUCT_FIELD_META (shader_source_byproduct_t, source)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API struct kan_resource_reference_meta_t
    shader_source_byproduct_source_meta = {
        .type = NULL, // Null means third party.
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW,
};

// Compiled shader source byproduct contains data parsed from individual sources.
// It would be empty in the example for simplicity.

struct shader_source_byproduct_compiled_t
{
    kan_memory_size_t stub;
};

KAN_REFLECTION_STRUCT_META (shader_source_byproduct_compiled_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API struct kan_resource_resource_type_meta_t
    shader_source_byproduct_compiled_meta = {
        .root = KAN_FALSE,
};

static enum kan_resource_compile_result_t shader_source_byproduct_compile (struct kan_resource_compile_state_t *state)
{
    // As this is only an example, not real material compiler, we just fill the stub here.
    ((struct shader_source_byproduct_compiled_t *) state->output_instance)->stub = 0u;
    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

// Raw material references shader object and defines passes with shader compilation options.

struct material_pass_option_t
{
    kan_interned_string_t name;
    kan_instance_size_t value;
};

struct material_pass_t
{
    kan_interned_string_t name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct material_pass_option_t)
    struct kan_dynamic_array_t options;
};

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API void material_pass_init (struct material_pass_t *instance)
{
    kan_dynamic_array_init (&instance->options, 0u, sizeof (struct material_pass_option_t),
                            _Alignof (struct material_pass_option_t), kan_allocation_group_stack_get ());
}

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API void material_pass_shutdown (struct material_pass_t *instance)
{
    kan_dynamic_array_shutdown (&instance->options);
}

struct material_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t shader_sources;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct material_pass_t)
    struct kan_dynamic_array_t passes;
};

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API void material_init (struct material_t *instance)
{
    kan_dynamic_array_init (&instance->shader_sources, 0u, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->passes, 0u, sizeof (struct material_pass_t), _Alignof (struct material_pass_t),
                            kan_allocation_group_stack_get ());
}

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API void material_shutdown (struct material_t *instance)
{
    for (kan_loop_size_t index = 0u; index < instance->passes.size; ++index)
    {
        material_pass_shutdown (&((struct material_pass_t *) instance->passes.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->shader_sources);
    kan_dynamic_array_shutdown (&instance->passes);
}

KAN_REFLECTION_STRUCT_META (material_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API struct kan_resource_resource_type_meta_t
    material_resource_type_meta = {
        .root = KAN_TRUE,
};

static enum kan_resource_compile_result_t material_compile (struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (material_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API struct kan_resource_compilable_meta_t
    material_compilable_meta = {
        .output_type_name = "material_compiled_t",
        .configuration_type_name = NULL,
        .state_type_name = NULL,
        .functor = material_compile,
};

// Pipeline instance byproduct contains all the information required to compile render pipeline:
// both list of compiled shader object byproducts that provide parsed data and options for generation.

struct pipeline_instance_byproduct_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t sources;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct material_pass_option_t)
    struct kan_dynamic_array_t options;
};

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API void pipeline_instance_byproduct_init (
    struct pipeline_instance_byproduct_t *instance)
{
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->options, 0u, sizeof (struct material_pass_option_t),
                            _Alignof (struct material_pass_option_t), kan_allocation_group_stack_get ());
}

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API void pipeline_instance_byproduct_shutdown (
    struct pipeline_instance_byproduct_t *instance)
{
    kan_dynamic_array_shutdown (&instance->sources);
    kan_dynamic_array_shutdown (&instance->options);
}

KAN_REFLECTION_STRUCT_META (pipeline_instance_byproduct_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API struct kan_resource_byproduct_type_meta_t
    pipeline_instance_byproduct_byproduct_type_meta = {
        .hash = NULL,
        .is_equal = NULL,
        .move = NULL,
        .reset = NULL,
};

static enum kan_resource_compile_result_t pipeline_instance_byproduct_compile (
    struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (pipeline_instance_byproduct_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API struct kan_resource_compilable_meta_t
    pipeline_instance_byproduct_compilable_meta = {
        .output_type_name = "pipeline_instance_byproduct_compiled_t",
        .configuration_type_name = "pipeline_instance_platform_configuration_t",
        .state_type_name = NULL,
        .functor = pipeline_instance_byproduct_compile,
};

KAN_REFLECTION_STRUCT_FIELD_META (pipeline_instance_byproduct_t, sources)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API struct kan_resource_reference_meta_t
    pipeline_instance_byproduct_source_meta = {
        .type = "shader_source_byproduct_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED,
};

// Compiled pipeline instance byproduct contains pipeline compiled code.
// It would be empty in the example for simplicity.

struct pipeline_instance_byproduct_compiled_t
{
    enum pipeline_instance_platform_format_t format;
};

KAN_REFLECTION_STRUCT_META (pipeline_instance_byproduct_compiled_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API struct kan_resource_resource_type_meta_t
    pipeline_instance_byproduct_compiled_meta = {
        .root = KAN_FALSE,
};

static enum kan_resource_compile_result_t pipeline_instance_byproduct_compile (
    struct kan_resource_compile_state_t *state)
{
    KAN_ASSERT (state->platform_configuration)
    const struct pipeline_instance_platform_configuration_t *configuration = state->platform_configuration;

    // As this is only an example, not real material compiler, we just fill the stub here.
    ((struct pipeline_instance_byproduct_compiled_t *) state->output_instance)->format = configuration->format;
    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

// Compiled material has no reference to initial shader object and only stores passes, but passes also store only
// pass name and compiled pipeline instance byproduct reference.

struct material_pass_compiled_t
{
    kan_interned_string_t name;
    kan_interned_string_t pipeline_instance;
};

struct material_compiled_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct material_pass_compiled_t)
    struct kan_dynamic_array_t passes;
};

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API void material_compiled_init (
    struct material_compiled_t *instance)
{
    kan_dynamic_array_init (&instance->passes, 0u, sizeof (struct material_pass_compiled_t),
                            _Alignof (struct material_pass_compiled_t), kan_allocation_group_stack_get ());
}

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API void material_compiled_shutdown (
    struct material_compiled_t *instance)
{
    kan_dynamic_array_shutdown (&instance->passes);
}

KAN_REFLECTION_STRUCT_META (material_compiled_t)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API struct kan_resource_resource_type_meta_t
    material_compiled_resource_type_meta = {
        .root = KAN_TRUE,
};

KAN_REFLECTION_STRUCT_FIELD_META (material_pass_compiled_t, pipeline_instance)
APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API struct kan_resource_reference_meta_t
    material_pass_compiled_t_pipeline_instance_meta = {
        .type = "pipeline_instance_byproduct_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

static enum kan_resource_compile_result_t material_compile (struct kan_resource_compile_state_t *state)
{
    const struct material_t *source = state->input_instance;
    struct material_compiled_t *target = state->output_instance;

    kan_allocation_group_t temporary_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "material_compilation");

    struct kan_dynamic_array_t sources;
    kan_dynamic_array_init (&sources, source->shader_sources.size, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), temporary_allocation_group);

    struct shader_source_byproduct_t shader_source;
    const kan_interned_string_t shader_source_type_name = kan_string_intern ("shader_source_byproduct_t");

    for (kan_loop_size_t index = 0u; index < source->shader_sources.size; ++index)
    {
        shader_source.source = ((kan_interned_string_t *) source->shader_sources.data)[index];
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&sources) =
            state->register_byproduct (state->interface_user_data, shader_source_type_name, &shader_source);
    }

    // Sort shader sources to ensure stable order of sources for pipeline instance.
    {
        kan_interned_string_t temporary;

#define AT_INDEX(INDEX) (((kan_interned_string_t *) sources.data)[INDEX])
#define LESS(first_index, second_index)                                                                                \
    __CUSHION_PRESERVE__ strcmp (AT_INDEX (first_index), AT_INDEX (second_index)) < 0
#define SWAP(first_index, second_index)                                                                                \
    __CUSHION_PRESERVE__                                                                                               \
    temporary = AT_INDEX (first_index), AT_INDEX (first_index) = AT_INDEX (second_index),                              \
    AT_INDEX (second_index) = temporary

        QSORT (sources.size, LESS, SWAP);
#undef LESS
#undef SWAP
#undef AT_INDEX
    }

    kan_dynamic_array_set_capacity (&target->passes, source->passes.size);
    const kan_interned_string_t pipeline_instance_type_name = kan_string_intern ("pipeline_instance_byproduct_t");
    struct pipeline_instance_byproduct_t pipeline_instance;
    pipeline_instance_byproduct_init (&pipeline_instance);

    for (kan_loop_size_t pass_index = 0u; pass_index < source->passes.size; ++pass_index)
    {
        struct material_pass_t *source_pass = &((struct material_pass_t *) source->passes.data)[pass_index];
        struct material_pass_compiled_t *target_pass = kan_dynamic_array_add_last (&target->passes);
        target_pass->name = source_pass->name;

        kan_dynamic_array_set_capacity (&pipeline_instance.sources, sources.size);
        for (kan_loop_size_t copy_index = 0u; copy_index < sources.size; ++copy_index)
        {
            *(kan_interned_string_t *) kan_dynamic_array_add_last (&pipeline_instance.sources) =
                ((kan_interned_string_t *) sources.data)[copy_index];
        }

        kan_dynamic_array_set_capacity (&pipeline_instance.options, source_pass->options.size);
        for (kan_loop_size_t copy_index = 0u; copy_index < source_pass->options.size; ++copy_index)
        {
            *(struct material_pass_option_t *) kan_dynamic_array_add_last (&pipeline_instance.options) =
                ((struct material_pass_option_t *) source_pass->options.data)[copy_index];
        }

        {
            struct material_pass_option_t temporary;

#define AT_INDEX(INDEX) (((struct material_pass_option_t *) pipeline_instance.options.data)[INDEX])
#define LESS(first_index, second_index) __CUSHION_PRESERVE__ AT_INDEX (first_index).name < AT_INDEX (second_index).name
#define SWAP(first_index, second_index)                                                                                \
    __CUSHION_PRESERVE__                                                                                               \
    temporary = AT_INDEX (first_index), AT_INDEX (first_index) = AT_INDEX (second_index),                              \
    AT_INDEX (second_index) = temporary

            QSORT (pipeline_instance.options.size, LESS, SWAP);
#undef LESS
#undef SWAP
#undef AT_INDEX
        }

        target_pass->pipeline_instance =
            state->register_byproduct (state->interface_user_data, pipeline_instance_type_name, &pipeline_instance);
    }

    kan_dynamic_array_shutdown (&sources);
    pipeline_instance_byproduct_shutdown (&pipeline_instance);
    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

struct example_compilation_byproduct_singleton_t
{
    kan_bool_t checked_entries;
    kan_bool_t loaded_data;
    kan_bool_t data_valid;
    kan_resource_request_id_t material_1_request_id;
    kan_resource_request_id_t material_2_request_id;
    kan_resource_request_id_t material_3_request_id;
    kan_resource_request_id_t material_4_request_id;
    kan_resource_request_id_t any_pipeline_request_id;
};

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API void example_compilation_byproduct_singleton_init (
    struct example_compilation_byproduct_singleton_t *instance)
{
    instance->checked_entries = KAN_FALSE;
    instance->loaded_data = KAN_FALSE;
    instance->data_valid = KAN_FALSE;
    instance->material_1_request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
    instance->material_2_request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
    instance->material_3_request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
    instance->material_4_request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
    instance->any_pipeline_request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
}

struct compilation_byproduct_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (compilation_byproduct)
    KAN_UM_BIND_STATE (compilation_byproduct, state)

    kan_context_system_t application_framework_system_handle;
};

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API KAN_UM_MUTATOR_DEPLOY (compilation_byproduct)
{
    kan_context_t context = kan_universe_get_context (universe);
    state->application_framework_system_handle =
        kan_context_query (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME);
}

static kan_bool_t is_entry_exists (struct compilation_byproduct_state_t *state,
                                   kan_interned_string_t type,
                                   kan_interned_string_t name)
{
    KAN_UML_VALUE_READ (entry, kan_resource_native_entry_t, name, &name)
    {
        if (entry->type == type)
        {
            return KAN_TRUE;
        }
    }

    return KAN_FALSE;
}

static kan_bool_t is_any_entry_exists (struct compilation_byproduct_state_t *state, kan_interned_string_t type)
{
    KAN_UML_VALUE_READ (entry, kan_resource_native_entry_t, type, &type) { return KAN_TRUE; }
    return KAN_FALSE;
}

static void check_entries (struct compilation_byproduct_state_t *state,
                           struct example_compilation_byproduct_singleton_t *singleton)
{
    kan_bool_t everything_ok = KAN_TRUE;
    const kan_bool_t in_compiled_mode =
        is_entry_exists (state, kan_string_intern ("material_compiled_t"), kan_string_intern ("material_1"));

    if (is_any_entry_exists (state, kan_string_intern ("shader_source_byproduct_compiled_t")))
    {
        KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                 "Found \"shader_source_byproduct_compiled_t\" which are unexpected!")
        everything_ok = KAN_FALSE;
    }

    if (is_any_entry_exists (state, kan_string_intern ("shader_object_compiled_t")))
    {
        KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                 "Found \"shader_object_compiled_t\" which are unexpected!")
        everything_ok = KAN_FALSE;
    }

    if (in_compiled_mode)
    {
        if (!is_any_entry_exists (state, kan_string_intern ("pipeline_instance_byproduct_compiled_t")))
        {
            KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                     "Not found any \"pipeline_instance_byproduct_compiled_t\" which are expected!")
            everything_ok = KAN_FALSE;
        }

        if (is_any_entry_exists (state, kan_string_intern ("material_t")))
        {
            KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                     "Found \"material_t\" which are unexpected!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_compiled_t"), kan_string_intern ("material_1")))
        {
            KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_1\" of type \"material_compiled_t\"!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_compiled_t"), kan_string_intern ("material_2")))
        {
            KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_2\" of type \"material_compiled_t\"!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_compiled_t"), kan_string_intern ("material_3")))
        {
            KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_3\" of type \"material_compiled_t\"!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_compiled_t"), kan_string_intern ("material_4")))
        {
            KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_4\" of type \"material_compiled_t\"!")
            everything_ok = KAN_FALSE;
        }
    }
    else
    {
        if (is_any_entry_exists (state, kan_string_intern ("pipeline_instance_byproduct_compiled_t")))
        {
            KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                     "Found \"shader_source_byproduct_compiled_t\" which are unexpected!")
            everything_ok = KAN_FALSE;
        }

        if (is_any_entry_exists (state, kan_string_intern ("material_compiled_t")))
        {
            KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                     "Found \"material_compiled_t\" which are unexpected!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_t"), kan_string_intern ("material_1")))
        {
            KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_1\" of type \"material_t\"!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_t"), kan_string_intern ("material_2")))
        {
            KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_2\" of type \"material_t\"!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_t"), kan_string_intern ("material_3")))
        {
            KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_3\" of type \"material_t\"!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_t"), kan_string_intern ("material_4")))
        {
            KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_4\" of type \"material_t\"!")
            everything_ok = KAN_FALSE;
        }
    }

    if (everything_ok)
    {
        singleton->checked_entries = KAN_TRUE;
    }
    else if (KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
    {
        kan_application_framework_system_request_exit (state->application_framework_system_handle, 1);
    }
}

static void insert_missing_requests (struct compilation_byproduct_state_t *state,
                                     struct example_compilation_byproduct_singleton_t *singleton,
                                     const struct kan_resource_provider_singleton_t *provider_singleton)
{
    if (!KAN_TYPED_ID_32_IS_VALID (singleton->material_1_request_id))
    {
        KAN_UMO_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (provider_singleton);
            request->type = kan_string_intern ("material_compiled_t");
            request->name = kan_string_intern ("material_1");
            request->priority = 0u;
            singleton->material_1_request_id = request->request_id;
        }
    }

    if (!KAN_TYPED_ID_32_IS_VALID (singleton->material_2_request_id))
    {
        KAN_UMO_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (provider_singleton);
            request->type = kan_string_intern ("material_compiled_t");
            request->name = kan_string_intern ("material_2");
            request->priority = 0u;
            singleton->material_2_request_id = request->request_id;
        }
    }

    if (!KAN_TYPED_ID_32_IS_VALID (singleton->material_3_request_id))
    {
        KAN_UMO_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (provider_singleton);
            request->type = kan_string_intern ("material_compiled_t");
            request->name = kan_string_intern ("material_3");
            request->priority = 0u;
            singleton->material_3_request_id = request->request_id;
        }
    }

    if (!KAN_TYPED_ID_32_IS_VALID (singleton->material_4_request_id))
    {
        KAN_UMO_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (provider_singleton);
            request->type = kan_string_intern ("material_compiled_t");
            request->name = kan_string_intern ("material_4");
            request->priority = 0u;
            singleton->material_4_request_id = request->request_id;
        }
    }

    if (!KAN_TYPED_ID_32_IS_VALID (singleton->any_pipeline_request_id))
    {
        const kan_interned_string_t type = kan_string_intern ("pipeline_instance_byproduct_compiled_t");
        KAN_UML_VALUE_READ (entry, kan_resource_native_entry_t, type, &type)
        {
            KAN_UMO_INDEXED_INSERT (request, kan_resource_request_t)
            {
                request->request_id = kan_next_resource_request_id (provider_singleton);
                request->type = type;
                request->name = entry->name;
                request->priority = 0u;
                singleton->any_pipeline_request_id = request->request_id;
            }

            break;
        }
    }
}

static void check_if_requests_are_loaded (struct compilation_byproduct_state_t *state,
                                          struct example_compilation_byproduct_singleton_t *singleton)
{
    singleton->loaded_data = KAN_TRUE;
    KAN_UMI_VALUE_READ_REQUIRED (request_1, kan_resource_request_t, request_id, &singleton->material_1_request_id)
    singleton->loaded_data &= KAN_TYPED_ID_32_IS_VALID (request_1->provided_container_id);

    KAN_UMI_VALUE_READ_REQUIRED (request_2, kan_resource_request_t, request_id, &singleton->material_2_request_id)
    singleton->loaded_data &= KAN_TYPED_ID_32_IS_VALID (request_2->provided_container_id);

    KAN_UMI_VALUE_READ_REQUIRED (request_3, kan_resource_request_t, request_id, &singleton->material_3_request_id)
    singleton->loaded_data &= KAN_TYPED_ID_32_IS_VALID (request_3->provided_container_id);

    KAN_UMI_VALUE_READ_REQUIRED (request_4, kan_resource_request_t, request_id, &singleton->material_4_request_id)
    singleton->loaded_data &= KAN_TYPED_ID_32_IS_VALID (request_4->provided_container_id);

    if (KAN_TYPED_ID_32_IS_VALID (singleton->any_pipeline_request_id))
    {
        KAN_UMI_VALUE_READ_REQUIRED (pipeline_request, kan_resource_request_t, request_id,
                                     &singleton->any_pipeline_request_id)
        singleton->loaded_data &= KAN_TYPED_ID_32_IS_VALID (pipeline_request->provided_container_id);
    }
}

static void validate_material (struct compilation_byproduct_state_t *state,
                               struct example_compilation_byproduct_singleton_t *singleton,
                               kan_resource_request_id_t request_id,
                               kan_interned_string_t *output_visible_world_pipeline,
                               kan_interned_string_t *output_shadow_pipeline)
{
    KAN_UMI_VALUE_READ_REQUIRED (request, kan_resource_request_t, request_id, &request_id)
    KAN_UMI_VALUE_READ_REQUIRED (view, KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (material_compiled_t), container_id,
                                 &request->provided_container_id)

    const struct material_compiled_t *material = KAN_RESOURCE_PROVIDER_CONTAINER_GET (material_compiled_t, view);
    if (material->passes.size != 2u)
    {
        KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                 "Expected 2 passes in material \"%s\", but got %lu!", request->name,
                 (unsigned long) material->passes.size)
        singleton->data_valid = KAN_FALSE;
        return;
    }

    struct material_pass_compiled_t *first_pass = &((struct material_pass_compiled_t *) material->passes.data)[0u];

    if (first_pass->name != kan_string_intern ("visible_world"))
    {
        KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                 "Expected pass 0 of material \"%s\" to be named \"visible world\", but got \"%s\".", request->name,
                 first_pass->name)
        singleton->data_valid = KAN_FALSE;
        return;
    }

    struct material_pass_compiled_t *second_pass = &((struct material_pass_compiled_t *) material->passes.data)[1u];

    if (second_pass->name != kan_string_intern ("shadow"))
    {
        KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                 "Expected pass 0 of material \"%s\" to be named \"shadow\", but got \"%s\".", request->name,
                 second_pass->name)
        singleton->data_valid = KAN_FALSE;
        return;
    }

    *output_visible_world_pipeline = first_pass->pipeline_instance;
    *output_shadow_pipeline = second_pass->pipeline_instance;

    if (*output_visible_world_pipeline == *output_shadow_pipeline)
    {
        KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                 "Pass 0 and pass 1 pipelines of \"%s\" are equal, but must be different.", request->name)
        singleton->data_valid = KAN_FALSE;
    }
}

static void validate_loaded_data (struct compilation_byproduct_state_t *state,
                                  struct example_compilation_byproduct_singleton_t *singleton)
{
    singleton->data_valid = KAN_TRUE;
    kan_interned_string_t material_1_visible_world_pipeline = NULL;
    kan_interned_string_t material_1_shadow_pipeline = NULL;
    kan_interned_string_t material_2_visible_world_pipeline = NULL;
    kan_interned_string_t material_2_shadow_pipeline = NULL;
    kan_interned_string_t material_3_visible_world_pipeline = NULL;
    kan_interned_string_t material_3_shadow_pipeline = NULL;
    kan_interned_string_t material_4_visible_world_pipeline = NULL;
    kan_interned_string_t material_4_shadow_pipeline = NULL;

    validate_material (state, singleton, singleton->material_1_request_id, &material_1_visible_world_pipeline,
                       &material_1_shadow_pipeline);

    validate_material (state, singleton, singleton->material_2_request_id, &material_2_visible_world_pipeline,
                       &material_2_shadow_pipeline);

    validate_material (state, singleton, singleton->material_3_request_id, &material_3_visible_world_pipeline,
                       &material_3_shadow_pipeline);

    validate_material (state, singleton, singleton->material_4_request_id, &material_4_visible_world_pipeline,
                       &material_4_shadow_pipeline);

    if (material_1_visible_world_pipeline == material_2_visible_world_pipeline)
    {
        KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                 "\"material_1\" and \"material_2\" visible world pipelines are equal, but shouldn't.")
        singleton->data_valid = KAN_FALSE;
    }

    if (material_1_visible_world_pipeline == material_3_visible_world_pipeline)
    {
        KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                 "\"material_1\" and \"material_3\" visible world pipelines are equal, but shouldn't.")
        singleton->data_valid = KAN_FALSE;
    }

    if (material_1_shadow_pipeline != material_2_shadow_pipeline)
    {
        KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                 "\"material_1\" and \"material_2\" shadow pipelines are not equal, but should.")
        singleton->data_valid = KAN_FALSE;
    }

    if (material_3_visible_world_pipeline != material_4_visible_world_pipeline)
    {
        KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                 "\"material_3\" and \"material_4\" visible world pipelines are not equal, but should.")
        singleton->data_valid = KAN_FALSE;
    }

    if (material_3_shadow_pipeline == material_4_shadow_pipeline)
    {
        KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                 "\"material_3\" and \"material_4\" shadow pipelines are equal, but shouldn't.")
        singleton->data_valid = KAN_FALSE;
    }

    if (material_1_shadow_pipeline == material_3_shadow_pipeline)
    {
        KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                 "\"material_1\" and \"material_3\" shadow pipelines are equal, but shouldn't.")
        singleton->data_valid = KAN_FALSE;
    }

    // Check random pipeline instance that it was compiled with expected format from configuration.
    if (KAN_TYPED_ID_32_IS_VALID (singleton->any_pipeline_request_id))
    {
        KAN_UMI_VALUE_READ_REQUIRED (request, kan_resource_request_t, request_id, &singleton->any_pipeline_request_id)
        KAN_UMI_VALUE_READ_REQUIRED (view,
                                     KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (pipeline_instance_byproduct_compiled_t),
                                     container_id, &request->provided_container_id)

        const struct pipeline_instance_byproduct_compiled_t *pipeline =
            KAN_RESOURCE_PROVIDER_CONTAINER_GET (pipeline_instance_byproduct_compiled_t, view);

        if (pipeline->format != PIPELINE_INSTANCE_PLATFORM_FORMAT_SPIRV)
        {
            KAN_LOG (application_framework_examples_compilation_byproduct, KAN_LOG_ERROR,
                     "Expected any pipeline to have format from default configuration, but pipeline \"%s\" "
                     "received format %lu!",
                     request->name, (unsigned long) pipeline->format)
            singleton->data_valid = KAN_FALSE;
        }
    }
}

APPLICATION_FRAMEWORK_EXAMPLES_COMPILATION_BYPRODUCT_API KAN_UM_MUTATOR_EXECUTE (compilation_byproduct)
{
    KAN_UMI_SINGLETON_READ (provider_singleton, kan_resource_provider_singleton_t)
    KAN_UMI_SINGLETON_WRITE (singleton, example_compilation_byproduct_singleton_t)

    if (!provider_singleton->scan_done)
    {
        return;
    }

    if (!singleton->checked_entries)
    {
        check_entries (state, singleton);
        return;
    }

    insert_missing_requests (state, singleton, provider_singleton);
    check_if_requests_are_loaded (state, singleton);

    if (singleton->loaded_data)
    {
        validate_loaded_data (state, singleton);
    }

    if (singleton->checked_entries && singleton->loaded_data &&
        KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
    {
        kan_application_framework_system_request_exit (state->application_framework_system_handle,
                                                       singleton->data_valid ? 0 : 1);
    }
}
