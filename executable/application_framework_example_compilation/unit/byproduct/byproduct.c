#include <application_framework_example_compilation_byproduct_api.h>

#include <qsort.h>

#include <kan/api_common/alignment.h>
#include <kan/container/dynamic_array.h>
#include <kan/context/application_framework_system.h>
#include <kan/hash/hash.h>
#include <kan/log/logging.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

// \c_interface_scanner_disable
KAN_LOG_DEFINE_CATEGORY (application_framework_example_compilation_byproduct);
// \c_interface_scanner_enable

// Raw shader object just stores plain names of shader code sources.

struct shader_object_t
{
    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t sources;
};

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void shader_object_init (struct shader_object_t *instance)
{
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
}

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void shader_object_shutdown (struct shader_object_t *instance)
{
    kan_dynamic_array_shutdown (&instance->sources);
}

// \meta reflection_struct_meta = "shader_object_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_resource_type_meta_t
    shader_object_resource_type_meta = {
        .root = KAN_FALSE,
};

static enum kan_resource_compile_result_t shader_object_compile (struct kan_resource_compile_state_t *state);

// \meta reflection_struct_meta = "shader_object_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_compilable_meta_t
    shader_object_compilable_meta = {
        .output_type_name = "shader_object_compiled_t",
        .configuration_type_name = NULL,
        .state_type_name = NULL,
        .functor = shader_object_compile,
};

// \meta reflection_struct_field_meta = "shader_object_t.sources"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_reference_meta_t
    shader_object_sources_reference_meta = {
        .type = NULL, // Null means third party.
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW,
};

// Shader object byproducts are created for every source separately.
// We don't want to parse one shader code several times, therefore we use byproducts to merge everything.

struct shader_object_source_byproduct_t
{
    kan_interned_string_t source;
};

static kan_hash_t shader_object_source_byproduct_hash (void *instance)
{
    return (kan_hash_t) ((struct shader_object_source_byproduct_t *) instance)->source;
}

static kan_bool_t shader_object_source_byproduct_is_equal (const void *first, const void *second)
{
    return ((struct shader_object_source_byproduct_t *) first)->source ==
           ((struct shader_object_source_byproduct_t *) second)->source;
}

static void shader_object_source_byproduct_move (void *to, void *from)
{
    struct shader_object_source_byproduct_t *target = to;
    struct shader_object_source_byproduct_t *source = from;
    target->source = source->source;
}

// \meta reflection_struct_meta = "shader_object_source_byproduct_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_byproduct_type_meta_t
    shader_object_source_byproduct_byproduct_type_meta = {
        .hash = shader_object_source_byproduct_hash,
        .is_equal = shader_object_source_byproduct_is_equal,
        .move = shader_object_source_byproduct_move,
        .reset = NULL,
};

static enum kan_resource_compile_result_t shader_object_source_byproduct_compile (
    struct kan_resource_compile_state_t *state);

// \meta reflection_struct_meta = "shader_object_source_byproduct_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_compilable_meta_t
    shader_object_source_byproduct_compilable_meta = {
        .output_type_name = "shader_object_source_byproduct_compiled_t",
        .configuration_type_name = NULL,
        .state_type_name = NULL,
        .functor = shader_object_source_byproduct_compile,
};

// \meta reflection_struct_field_meta = "shader_object_source_byproduct_t.source"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_reference_meta_t
    shader_object_source_byproduct_source_meta = {
        .type = NULL, // Null means third party.
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW,
};

// Compiled shader object source byproduct contains data parsed from individual sources.
// It would be empty in the example for simplicity.

struct shader_object_source_byproduct_compiled_t
{
    kan_memory_size_t stub;
};

// \meta reflection_struct_meta = "shader_object_source_byproduct_compiled_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_resource_type_meta_t
    shader_object_source_byproduct_compiled_meta = {
        .root = KAN_FALSE,
};

static enum kan_resource_compile_result_t shader_object_source_byproduct_compile (
    struct kan_resource_compile_state_t *state)
{
    // As this is only an example, not real material compiler, we just fill the stub here.
    ((struct shader_object_source_byproduct_compiled_t *) state->output_instance)->stub = 0u;
    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

// Compiled shader object is almost the same as regular shader object,
// but it references byproducts instead of code sources.

struct shader_object_compiled_t
{
    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t sources;
};

_Static_assert (_Alignof (struct shader_object_compiled_t) <= _Alignof (kan_memory_size_t),
                "Alignment has expected value.");

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void shader_object_compiled_init (
    struct shader_object_compiled_t *instance)
{
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
}

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void shader_object_compiled_shutdown (
    struct shader_object_compiled_t *instance)
{
    kan_dynamic_array_shutdown (&instance->sources);
}

// \meta reflection_struct_meta = "shader_object_compiled_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_resource_type_meta_t
    shader_object_compiled_resource_type_meta = {
        .root = KAN_FALSE,
};

// \meta reflection_struct_field_meta = "shader_object_compiled_t.sources"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_reference_meta_t
    shader_object_compiled_sources_reference_meta = {
        .type = "shader_object_source_byproduct_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED};

// As mentioned above, shader object compilation translates third party source links into byproducts.
static enum kan_resource_compile_result_t shader_object_compile (struct kan_resource_compile_state_t *state)
{
    struct shader_object_t *input = state->input_instance;
    struct shader_object_t *output = state->output_instance;

    kan_dynamic_array_set_capacity (&output->sources, input->sources.size);
    struct shader_object_source_byproduct_t byproduct;
    const kan_interned_string_t byproduct_type_name = kan_string_intern ("shader_object_source_byproduct_t");

    for (kan_loop_size_t index = 0u; index < input->sources.size; ++index)
    {
        byproduct.source = ((kan_interned_string_t *) input->sources.data)[index];
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&output->sources) =
            state->register_byproduct (state->interface_user_data, byproduct_type_name, &byproduct);
    }

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

    /// \meta reflection_dynamic_array_type = "struct material_pass_option_t"
    struct kan_dynamic_array_t options;
};

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void material_pass_init (struct material_pass_t *instance)
{
    kan_dynamic_array_init (&instance->options, 0u, sizeof (struct material_pass_option_t),
                            _Alignof (struct material_pass_option_t), kan_allocation_group_stack_get ());
}

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void material_pass_shutdown (struct material_pass_t *instance)
{
    kan_dynamic_array_shutdown (&instance->options);
}

struct material_t
{
    kan_interned_string_t shader;

    /// \meta reflection_dynamic_array_type = "struct material_pass_t"
    struct kan_dynamic_array_t passes;
};

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void material_init (struct material_t *instance)
{
    kan_dynamic_array_init (&instance->passes, 0u, sizeof (struct material_pass_t), _Alignof (struct material_pass_t),
                            kan_allocation_group_stack_get ());
}

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void material_shutdown (struct material_t *instance)
{
    for (kan_loop_size_t index = 0u; index < instance->passes.size; ++index)
    {
        material_pass_shutdown (&((struct material_pass_t *) instance->passes.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->passes);
}

// \meta reflection_struct_meta = "material_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_resource_type_meta_t
    material_resource_type_meta = {
        .root = KAN_TRUE,
};

static enum kan_resource_compile_result_t material_compile (struct kan_resource_compile_state_t *state);

// \meta reflection_struct_meta = "material_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_compilable_meta_t material_compilable_meta =
    {
        .output_type_name = "material_compiled_t",
        .configuration_type_name = NULL,
        .state_type_name = NULL,
        .functor = material_compile,
};

// \meta reflection_struct_field_meta = "material_t.shader"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_reference_meta_t
    material_sources_reference_meta = {
        .type = "shader_object_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED,
};

// Pipeline instance byproduct contains all the information required to compile render pipeline:
// both list of compiled shader object byproducts that provide parsed data and options for generation.

struct pipeline_instance_byproduct_t
{
    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t sources;

    /// \meta reflection_dynamic_array_type = "struct material_pass_option_t"
    struct kan_dynamic_array_t options;
};

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void pipeline_instance_byproduct_init (
    struct pipeline_instance_byproduct_t *instance)
{
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->options, 0u, sizeof (struct material_pass_option_t),
                            _Alignof (struct material_pass_option_t), kan_allocation_group_stack_get ());
}

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void pipeline_instance_byproduct_shutdown (
    struct pipeline_instance_byproduct_t *instance)
{
    kan_dynamic_array_shutdown (&instance->sources);
    kan_dynamic_array_shutdown (&instance->options);
}

static kan_hash_t pipeline_instance_byproduct_hash (void *instance)
{
    struct pipeline_instance_byproduct_t *data = instance;
    kan_hash_t hash = 0u;

#define APPEND                                                                                                         \
    if (hash == 0u)                                                                                                    \
    {                                                                                                                  \
        hash = sub_hash;                                                                                               \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        hash = kan_hash_combine (hash, sub_hash);                                                                      \
    }

    for (kan_loop_size_t index = 0u; index < data->sources.size; ++index)
    {
        const kan_hash_t sub_hash = (kan_hash_t) ((kan_interned_string_t *) data->sources.data)[index];
        APPEND
    }

    for (kan_loop_size_t index = 0u; index < data->options.size; ++index)
    {
        struct material_pass_option_t *option = &((struct material_pass_option_t *) data->options.data)[index];
        const kan_hash_t sub_hash = kan_hash_combine ((kan_hash_t) option->name, (kan_hash_t) option->value);
        APPEND
    }

#undef APPEND
    return hash;
}

static kan_bool_t pipeline_instance_byproduct_is_equal (const void *first, const void *second)
{
    const struct pipeline_instance_byproduct_t *data_first = first;
    const struct pipeline_instance_byproduct_t *data_second = second;

    if (data_first->sources.size != data_second->sources.size || data_first->options.size != data_second->options.size)
    {
        return KAN_FALSE;
    }

    for (kan_loop_size_t index = 0u; index < data_first->sources.size; ++index)
    {
        if (((kan_interned_string_t *) data_first->sources.data)[index] !=
            ((kan_interned_string_t *) data_second->sources.data)[index])
        {
            return KAN_FALSE;
        }
    }

    for (kan_loop_size_t index = 0u; index < data_first->options.size; ++index)
    {
        struct material_pass_option_t *option_first =
            &((struct material_pass_option_t *) data_first->options.data)[index];
        struct material_pass_option_t *option_second =
            &((struct material_pass_option_t *) data_second->options.data)[index];

        if (option_first->name != option_second->name || option_first->value != option_second->value)
        {
            return KAN_FALSE;
        }
    }

    return KAN_TRUE;
}

static void pipeline_instance_byproduct_move (void *to, void *from)
{
    struct pipeline_instance_byproduct_t *target = to;
    struct pipeline_instance_byproduct_t *source = from;

    target->sources = source->sources;
    source->sources.size = 0u;
    source->sources.capacity = 0u;
    source->sources.data = NULL;

    target->options = source->options;
    source->options.size = 0u;
    source->options.capacity = 0u;
    source->options.data = NULL;
}

static void pipeline_instance_byproduct_reset (void *byproduct)
{
    struct pipeline_instance_byproduct_t *data = byproduct;
    data->sources.size = 0u;
    data->options.size = 0u;
}

// \meta reflection_struct_meta = "pipeline_instance_byproduct_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_byproduct_type_meta_t
    pipeline_instance_byproduct_byproduct_type_meta = {
        .hash = pipeline_instance_byproduct_hash,
        .is_equal = pipeline_instance_byproduct_is_equal,
        .move = pipeline_instance_byproduct_move,
        .reset = pipeline_instance_byproduct_reset,
};

static enum kan_resource_compile_result_t pipeline_instance_byproduct_compile (
    struct kan_resource_compile_state_t *state);

// \meta reflection_struct_meta = "pipeline_instance_byproduct_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_compilable_meta_t
    pipeline_instance_byproduct_compilable_meta = {
        .output_type_name = "pipeline_instance_byproduct_compiled_t",
        .configuration_type_name = "pipeline_instance_platform_configuration_t",
        .state_type_name = NULL,
        .functor = pipeline_instance_byproduct_compile,
};

// \meta reflection_struct_field_meta = "pipeline_instance_byproduct_t.sources"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_reference_meta_t
    pipeline_instance_byproduct_source_meta = {
        .type = "shader_object_source_byproduct_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED,
};

// Pipeline instance compilation configuration.
// As it is only example stub, we just use simple enum.

enum pipeline_instance_platform_format_t
{
    PIPELINE_INSTANCE_PLATFORM_FORMAT_UNKNOWN = 0u,
    PIPELINE_INSTANCE_PLATFORM_FORMAT_SPIRV,
};

struct pipeline_instance_platform_configuration_t
{
    enum pipeline_instance_platform_format_t format;
};

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void pipeline_instance_platform_configuration_init (
    struct pipeline_instance_platform_configuration_t *instance)
{
    instance->format = PIPELINE_INSTANCE_PLATFORM_FORMAT_UNKNOWN;
}

// Compiled pipeline instance byproduct contains pipeline compiled code.
// It would be empty in the example for simplicity.

struct pipeline_instance_byproduct_compiled_t
{
    enum pipeline_instance_platform_format_t format;
};

// \meta reflection_struct_meta = "pipeline_instance_byproduct_compiled_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_resource_type_meta_t
    pipeline_instance_byproduct_compiled_meta = {
        .root = KAN_FALSE,
};

static enum kan_resource_compile_result_t pipeline_instance_byproduct_compile (
    struct kan_resource_compile_state_t *state)
{
    KAN_ASSERT (state->platform_configuration)
    struct pipeline_instance_platform_configuration_t *configuration = state->platform_configuration;

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
    /// \meta reflection_dynamic_array_type = "struct material_pass_compiled_t"
    struct kan_dynamic_array_t passes;
};

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void material_compiled_init (
    struct material_compiled_t *instance)
{
    kan_dynamic_array_init (&instance->passes, 0u, sizeof (struct material_pass_compiled_t),
                            _Alignof (struct material_pass_compiled_t), kan_allocation_group_stack_get ());
}

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void material_compiled_shutdown (
    struct material_compiled_t *instance)
{
    kan_dynamic_array_shutdown (&instance->passes);
}

// \meta reflection_struct_meta = "material_compiled_t"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_resource_type_meta_t
    material_compiled_resource_type_meta = {
        .root = KAN_TRUE,
};

// \meta reflection_struct_field_meta = "material_pass_compiled_t.pipeline_instance"
APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API struct kan_resource_reference_meta_t
    material_pass_compiled_t_pipeline_instance_meta = {
        .type = "pipeline_instance_byproduct_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

static enum kan_resource_compile_result_t material_compile (struct kan_resource_compile_state_t *state)
{
    struct material_t *source = state->input_instance;
    struct material_compiled_t *target = state->output_instance;

    // We only expect one dependency -- compiled shader object.
    KAN_ASSERT (state->dependencies_count == 1u)
    const struct shader_object_compiled_t *shader_object = state->dependencies[0u].data;

    kan_allocation_group_t temporary_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "material_compilation");
    struct kan_dynamic_array_t sorted_sources;

    kan_dynamic_array_init (&sorted_sources, shader_object->sources.size, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), temporary_allocation_group);

    for (kan_loop_size_t copy_index = 0u; copy_index < shader_object->sources.size; ++copy_index)
    {
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&sorted_sources) =
            ((kan_interned_string_t *) shader_object->sources.data)[copy_index];
    }

    {
        kan_interned_string_t temporary;

#define AT_INDEX(INDEX) (((kan_interned_string_t *) sorted_sources.data)[INDEX])
#define LESS(first_index, second_index) AT_INDEX (first_index) < AT_INDEX (second_index)
#define SWAP(first_index, second_index)                                                                                \
    temporary = AT_INDEX (first_index), AT_INDEX (first_index) = AT_INDEX (second_index),                              \
    AT_INDEX (second_index) = temporary
        QSORT (sorted_sources.size, LESS, SWAP);
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

        kan_dynamic_array_set_capacity (&pipeline_instance.sources, sorted_sources.size);
        for (kan_loop_size_t copy_index = 0u; copy_index < sorted_sources.size; ++copy_index)
        {
            *(kan_interned_string_t *) kan_dynamic_array_add_last (&pipeline_instance.sources) =
                ((kan_interned_string_t *) sorted_sources.data)[copy_index];
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
#define LESS(first_index, second_index) AT_INDEX (first_index).name < AT_INDEX (second_index).name
#define SWAP(first_index, second_index)                                                                                \
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

    kan_dynamic_array_shutdown (&sorted_sources);
    pipeline_instance_byproduct_shutdown (&pipeline_instance);
    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

struct byproduct_test_singleton_t
{
    kan_bool_t checked_entries;
    kan_bool_t loaded_data;
    kan_bool_t data_valid;
    kan_resource_request_id_t material_1_request_id;
    kan_resource_request_id_t material_2_request_id;
    kan_resource_request_id_t material_3_request_id;
    kan_resource_request_id_t material_4_request_id;
};

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void byproduct_test_singleton_init (
    struct byproduct_test_singleton_t *instance)
{
    instance->checked_entries = KAN_FALSE;
    instance->loaded_data = KAN_FALSE;
    instance->data_valid = KAN_FALSE;
    instance->material_1_request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
    instance->material_2_request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
    instance->material_3_request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
    instance->material_4_request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
}

struct byproduct_mutator_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (byproduct_mutator)
    KAN_UP_BIND_STATE (byproduct_mutator, state)

    kan_context_system_t application_framework_system_handle;
};

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void kan_universe_mutator_deploy_byproduct_mutator (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct byproduct_mutator_state_t *state)
{
    kan_context_t context = kan_universe_get_context (universe);
    state->application_framework_system_handle =
        kan_context_query (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME);
}

static kan_bool_t is_entry_exists (struct byproduct_mutator_state_t *state,
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

static kan_bool_t is_any_entry_exists (struct byproduct_mutator_state_t *state, kan_interned_string_t type)
{
    KAN_UP_VALUE_READ (entry, kan_resource_native_entry_t, type, &type)
    {
        KAN_UP_QUERY_RETURN_VALUE (kan_bool_t, KAN_TRUE);
    }

    return KAN_FALSE;
}

static void check_entries (struct byproduct_mutator_state_t *state, struct byproduct_test_singleton_t *test_singleton)
{
    kan_bool_t everything_ok = KAN_TRUE;
    const kan_bool_t in_compiled_mode =
        is_entry_exists (state, kan_string_intern ("material_compiled_t"), kan_string_intern ("material_1"));

    if (is_any_entry_exists (state, kan_string_intern ("shader_object_source_byproduct_compiled_t")))
    {
        KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                 "Found \"shader_object_source_byproduct_compiled_t\" which are unexpected!")
        everything_ok = KAN_FALSE;
    }

    if (is_any_entry_exists (state, kan_string_intern ("shader_object_compiled_t")))
    {
        KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                 "Found \"shader_object_compiled_t\" which are unexpected!")
        everything_ok = KAN_FALSE;
    }

    if (in_compiled_mode)
    {
        if (is_any_entry_exists (state, kan_string_intern ("shader_object_t")))
        {
            KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                     "Found \"shader_object_t\" which are unexpected!")
            everything_ok = KAN_FALSE;
        }

        if (!is_any_entry_exists (state, kan_string_intern ("pipeline_instance_byproduct_compiled_t")))
        {
            KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                     "Not found any \"pipeline_instance_byproduct_compiled_t\" which are expected!")
            everything_ok = KAN_FALSE;
        }

        if (is_any_entry_exists (state, kan_string_intern ("material_t")))
        {
            KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                     "Found \"material_t\" which are unexpected!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_compiled_t"), kan_string_intern ("material_1")))
        {
            KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_1\" of type \"material_compiled_t\"!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_compiled_t"), kan_string_intern ("material_2")))
        {
            KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_2\" of type \"material_compiled_t\"!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_compiled_t"), kan_string_intern ("material_3")))
        {
            KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_3\" of type \"material_compiled_t\"!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_compiled_t"), kan_string_intern ("material_4")))
        {
            KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_4\" of type \"material_compiled_t\"!")
            everything_ok = KAN_FALSE;
        }
    }
    else
    {
        if (is_any_entry_exists (state, kan_string_intern ("pipeline_instance_byproduct_compiled_t")))
        {
            KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                     "Found \"shader_object_source_byproduct_compiled_t\" which are unexpected!")
            everything_ok = KAN_FALSE;
        }

        if (is_any_entry_exists (state, kan_string_intern ("material_compiled_t")))
        {
            KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                     "Found \"material_compiled_t\" which are unexpected!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_t"), kan_string_intern ("material_1")))
        {
            KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_1\" of type \"material_t\"!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_t"), kan_string_intern ("material_2")))
        {
            KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_2\" of type \"material_t\"!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_t"), kan_string_intern ("material_3")))
        {
            KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_3\" of type \"material_t\"!")
            everything_ok = KAN_FALSE;
        }

        if (!is_entry_exists (state, kan_string_intern ("material_t"), kan_string_intern ("material_4")))
        {
            KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                     "Unable to find \"material_4\" of type \"material_t\"!")
            everything_ok = KAN_FALSE;
        }

        // There is no runtime compilation yet, we have nothing to do after this check.
        if (everything_ok && KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
        {
            kan_application_framework_system_request_exit (state->application_framework_system_handle, 0);
            return;
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

static void insert_missing_requests (struct byproduct_mutator_state_t *state,
                                     struct byproduct_test_singleton_t *test_singleton,
                                     const struct kan_resource_provider_singleton_t *provider_singleton)
{
    if (!KAN_TYPED_ID_32_IS_VALID (test_singleton->material_1_request_id))
    {
        KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (provider_singleton);
            request->type = kan_string_intern ("material_compiled_t");
            request->name = kan_string_intern ("material_1");
            request->priority = 0u;
            test_singleton->material_1_request_id = request->request_id;
        }
    }

    if (!KAN_TYPED_ID_32_IS_VALID (test_singleton->material_2_request_id))
    {
        KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (provider_singleton);
            request->type = kan_string_intern ("material_compiled_t");
            request->name = kan_string_intern ("material_2");
            request->priority = 0u;
            test_singleton->material_2_request_id = request->request_id;
        }
    }

    if (!KAN_TYPED_ID_32_IS_VALID (test_singleton->material_3_request_id))
    {
        KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (provider_singleton);
            request->type = kan_string_intern ("material_compiled_t");
            request->name = kan_string_intern ("material_3");
            request->priority = 0u;
            test_singleton->material_3_request_id = request->request_id;
        }
    }

    if (!KAN_TYPED_ID_32_IS_VALID (test_singleton->material_4_request_id))
    {
        KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (provider_singleton);
            request->type = kan_string_intern ("material_compiled_t");
            request->name = kan_string_intern ("material_4");
            request->priority = 0u;
            test_singleton->material_4_request_id = request->request_id;
        }
    }
}

static void check_if_requests_are_loaded (struct byproduct_mutator_state_t *state,
                                          struct byproduct_test_singleton_t *test_singleton)
{
    test_singleton->loaded_data = KAN_TRUE;
    KAN_UP_VALUE_READ (request_1, kan_resource_request_t, request_id, &test_singleton->material_1_request_id)
    {
        test_singleton->loaded_data &= KAN_TYPED_ID_32_IS_VALID (request_1->provided_container_id);
    }

    KAN_UP_VALUE_READ (request_2, kan_resource_request_t, request_id, &test_singleton->material_2_request_id)
    {
        test_singleton->loaded_data &= KAN_TYPED_ID_32_IS_VALID (request_2->provided_container_id);
    }

    KAN_UP_VALUE_READ (request_3, kan_resource_request_t, request_id, &test_singleton->material_3_request_id)
    {
        test_singleton->loaded_data &= KAN_TYPED_ID_32_IS_VALID (request_3->provided_container_id);
    }

    KAN_UP_VALUE_READ (request_4, kan_resource_request_t, request_id, &test_singleton->material_4_request_id)
    {
        test_singleton->loaded_data &= KAN_TYPED_ID_32_IS_VALID (request_4->provided_container_id);
    }
}

static void validate_material (struct byproduct_mutator_state_t *state,
                               struct byproduct_test_singleton_t *test_singleton,
                               kan_resource_request_id_t request_id,
                               kan_interned_string_t *output_visible_world_pipeline,
                               kan_interned_string_t *output_shadow_pipeline)
{
    KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &request_id)
    {
        KAN_UP_VALUE_READ (view, resource_provider_container_material_compiled_t, container_id,
                           &request->provided_container_id)
        {
            kan_memory_size_t offset = offsetof (struct kan_resource_container_view_t, data_begin);
            offset = kan_apply_alignment (offset, _Alignof (struct material_compiled_t));
            struct material_compiled_t *material = (struct material_compiled_t *) (((uint8_t *) view) + offset);

            if (material->passes.size != 2u)
            {
                KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                         "Expected 2 passes in material \"%s\", but got %lu!", request->name,
                         (unsigned long) material->passes.size)
                test_singleton->data_valid = KAN_FALSE;
                KAN_UP_QUERY_RETURN_VOID;
            }

            struct material_pass_compiled_t *first_pass =
                &((struct material_pass_compiled_t *) material->passes.data)[0u];

            if (first_pass->name != kan_string_intern ("visible_world"))
            {
                KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                         "Expected pass 0 of material \"%s\" to be named \"visible world\", but got \"%s\".",
                         request->name, first_pass->name)
                test_singleton->data_valid = KAN_FALSE;
                KAN_UP_QUERY_RETURN_VOID;
            }

            struct material_pass_compiled_t *second_pass =
                &((struct material_pass_compiled_t *) material->passes.data)[1u];

            if (second_pass->name != kan_string_intern ("shadow"))
            {
                KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                         "Expected pass 0 of material \"%s\" to be named \"shadow\", but got \"%s\".", request->name,
                         second_pass->name)
                test_singleton->data_valid = KAN_FALSE;
                KAN_UP_QUERY_RETURN_VOID;
            }

            *output_visible_world_pipeline = first_pass->pipeline_instance;
            *output_shadow_pipeline = second_pass->pipeline_instance;

            if (*output_visible_world_pipeline == *output_shadow_pipeline)
            {
                KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                         "Pass 0 and pass 1 pipelines of \"%s\" are equal, but must be different.", request->name)
                test_singleton->data_valid = KAN_FALSE;
            }
        }
    }
}

static void validate_loaded_data (struct byproduct_mutator_state_t *state,
                                  struct byproduct_test_singleton_t *test_singleton)
{
    test_singleton->data_valid = KAN_TRUE;
    kan_interned_string_t material_1_visible_world_pipeline = NULL;
    kan_interned_string_t material_1_shadow_pipeline = NULL;
    kan_interned_string_t material_2_visible_world_pipeline = NULL;
    kan_interned_string_t material_2_shadow_pipeline = NULL;
    kan_interned_string_t material_3_visible_world_pipeline = NULL;
    kan_interned_string_t material_3_shadow_pipeline = NULL;
    kan_interned_string_t material_4_visible_world_pipeline = NULL;
    kan_interned_string_t material_4_shadow_pipeline = NULL;

    validate_material (state, test_singleton, test_singleton->material_1_request_id, &material_1_visible_world_pipeline,
                       &material_1_shadow_pipeline);

    validate_material (state, test_singleton, test_singleton->material_2_request_id, &material_2_visible_world_pipeline,
                       &material_2_shadow_pipeline);

    validate_material (state, test_singleton, test_singleton->material_3_request_id, &material_3_visible_world_pipeline,
                       &material_3_shadow_pipeline);

    validate_material (state, test_singleton, test_singleton->material_4_request_id, &material_4_visible_world_pipeline,
                       &material_4_shadow_pipeline);

    if (material_1_visible_world_pipeline == material_2_visible_world_pipeline)
    {
        KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                 "\"material_1\" and \"material_2\" visible world pipelines are equal, but shouldn't.")
        test_singleton->data_valid = KAN_FALSE;
    }

    if (material_1_visible_world_pipeline == material_3_visible_world_pipeline)
    {
        KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                 "\"material_1\" and \"material_3\" visible world pipelines are equal, but shouldn't.")
        test_singleton->data_valid = KAN_FALSE;
    }

    if (material_1_shadow_pipeline != material_2_shadow_pipeline)
    {
        KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                 "\"material_1\" and \"material_2\" shadow pipelines are not equal, but should.")
        test_singleton->data_valid = KAN_FALSE;
    }

    if (material_3_visible_world_pipeline != material_4_visible_world_pipeline)
    {
        KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                 "\"material_3\" and \"material_4\" visible world pipelines are not equal, but should.")
        test_singleton->data_valid = KAN_FALSE;
    }

    if (material_3_shadow_pipeline == material_4_shadow_pipeline)
    {
        KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                 "\"material_3\" and \"material_4\" shadow pipelines are equal, but shouldn't.")
        test_singleton->data_valid = KAN_FALSE;
    }

    if (material_1_shadow_pipeline == material_3_shadow_pipeline)
    {
        KAN_LOG (application_framework_example_compilation_byproduct, KAN_LOG_ERROR,
                 "\"material_1\" and \"material_3\" shadow pipelines are equal, but shouldn't.")
        test_singleton->data_valid = KAN_FALSE;
    }

    // TODO: Somehow augment test to check pipeline format to ensure that they've received accurate configuration?
}

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_BYPRODUCT_API void kan_universe_mutator_execute_byproduct_mutator (
    kan_cpu_job_t job, struct byproduct_mutator_state_t *state)
{
    KAN_UP_SINGLETON_READ (provider_singleton, kan_resource_provider_singleton_t)
    KAN_UP_SINGLETON_WRITE (test_singleton, byproduct_test_singleton_t)
    {
        if (!provider_singleton->scan_done)
        {
            KAN_UP_MUTATOR_RETURN;
        }

        if (!test_singleton->checked_entries)
        {
            check_entries (state, test_singleton);
            KAN_UP_MUTATOR_RETURN;
        }

        insert_missing_requests (state, test_singleton, provider_singleton);
        check_if_requests_are_loaded (state, test_singleton);

        if (test_singleton->loaded_data)
        {
            validate_loaded_data (state, test_singleton);
        }

        if (test_singleton->checked_entries && test_singleton->loaded_data &&
            KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
        {
            kan_application_framework_system_request_exit (state->application_framework_system_handle,
                                                           test_singleton->data_valid ? 0 : 1);
        }
    }

    KAN_UP_MUTATOR_RETURN;
}
