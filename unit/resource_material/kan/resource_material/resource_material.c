#include <string.h>

#include <qsort.h>

#include <kan/api_common/mute_warnings.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/render_pipeline_language/parser.h>
#include <kan/resource_material/resource_material.h>
#include <kan/resource_pipeline/resource_pipeline.h>

KAN_LOG_DEFINE_CATEGORY (resource_material_compilation);

KAN_REFLECTION_STRUCT_META (kan_resource_material_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t kan_resource_material_resource_type_meta = {
    .root = KAN_FALSE,
};

static enum kan_resource_compile_result_t kan_resource_material_compile (struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (kan_resource_material_t)
RESOURCE_MATERIAL_API struct kan_resource_compilable_meta_t kan_resource_material_compilable_meta = {
    .output_type_name = "kan_resource_material_compiled_t",
    .configuration_type_name = NULL,
    // No state as material compilation just spawns byproducts that need to be compiled separately later.
    .state_type_name = NULL,
    .functor = kan_resource_material_compile,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_t, sources)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_sources_reference_meta = {
    .type = NULL, // Null means third party.
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

KAN_REFLECTION_STRUCT_META (kan_resource_material_meta_compiled_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t kan_resource_material_meta_compiled_resource_type_meta =
    {
        .root = KAN_FALSE,
};

KAN_REFLECTION_STRUCT_META (kan_resource_material_pipeline_compiled_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t
    kan_resource_material_pipeline_compiled_resource_type_meta = {
        .root = KAN_FALSE,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_pass_compiled_t, pipeline)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_pass_compiled_pipeline_reference_meta =
    {
        .type = "kan_resource_material_pipeline_compiled_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

KAN_REFLECTION_STRUCT_META (kan_resource_material_compiled_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t kan_resource_material_compiled_resource_type_meta = {
    .root = KAN_FALSE,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_compiled_t, meta)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_compiled_meta_reference_meta = {
    .type = "kan_resource_material_meta_compiled_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

struct kan_resource_material_shader_source_t
{
    kan_interned_string_t source;
};

KAN_REFLECTION_STRUCT_META (kan_resource_material_shader_source_t)
RESOURCE_MATERIAL_API struct kan_resource_byproduct_type_meta_t
    kan_resource_material_shader_source_byproduct_type_meta = {
        .hash = NULL,
        .is_equal = NULL,
        .move = NULL,
        .reset = NULL,
};

static enum kan_resource_compile_result_t kan_resource_material_shader_source_compile (
    struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (kan_resource_material_shader_source_t)
RESOURCE_MATERIAL_API struct kan_resource_compilable_meta_t kan_resource_material_shader_source_compilable_meta = {
    .output_type_name = "kan_resource_material_shader_source_compiled_t",
    .configuration_type_name = NULL,
    // No state as render pipeline language does not support step by step parsing right now.
    .state_type_name = NULL,
    .functor = kan_resource_material_shader_source_compile,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_shader_source_t, source)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_shader_source_source_reference_meta = {
    .type = NULL, // Null means third party.
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW,
};

struct kan_resource_material_shader_source_compiled_t
{
    struct kan_rpl_intermediate_t intermediate;
};

KAN_REFLECTION_STRUCT_META (kan_resource_material_shader_source_compiled_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t
    kan_resource_material_shader_source_compiled_resource_type_meta = {
        .root = KAN_FALSE,
};

RESOURCE_MATERIAL_API void kan_resource_material_shader_source_compiled_init (
    struct kan_resource_material_shader_source_compiled_t *instance)
{
    kan_rpl_intermediate_init (&instance->intermediate);
}

RESOURCE_MATERIAL_API void kan_resource_material_shader_source_compiled_shutdown (
    struct kan_resource_material_shader_source_compiled_t *instance)
{
    kan_rpl_intermediate_shutdown (&instance->intermediate);
}

struct kan_resource_material_meta_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t sources;

    struct kan_resource_material_options_t options;

    // Excluded from reflection in order to be ignored for byproduct replacement search.
    KAN_REFLECTION_IGNORE
    kan_interned_string_t source_material;
};

KAN_REFLECTION_STRUCT_META (kan_resource_material_meta_t)
RESOURCE_MATERIAL_API struct kan_resource_byproduct_type_meta_t kan_resource_material_meta_byproduct_type_meta = {
    .hash = NULL,
    .is_equal = NULL,
    .move = NULL,
    .reset = NULL,
};

static enum kan_resource_compile_result_t kan_resource_material_meta_compile (
    struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (kan_resource_material_meta_t)
RESOURCE_MATERIAL_API struct kan_resource_compilable_meta_t kan_resource_material_meta_compilable_meta = {
    .output_type_name = "kan_resource_material_meta_compiled_t",
    .configuration_type_name = NULL,
    // No state as render pipeline language does not support step by step emission right now.
    .state_type_name = NULL,
    .functor = kan_resource_material_meta_compile,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_meta_t, sources)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_meta_sources_reference_meta = {
    .type = "kan_resource_material_shader_source_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED,
};

RESOURCE_MATERIAL_API void kan_resource_material_meta_init (struct kan_resource_material_meta_t *instance)
{
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_resource_material_options_init (&instance->options);
    instance->source_material = NULL;
}

RESOURCE_MATERIAL_API void kan_resource_material_meta_shutdown (struct kan_resource_material_meta_t *instance)
{
    kan_dynamic_array_shutdown (&instance->sources);
    kan_resource_material_options_shutdown (&instance->options);
}

struct kan_resource_material_pipeline_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_entry_point_t)
    struct kan_dynamic_array_t entry_points;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t sources;

    struct kan_resource_material_options_t options;

    // Excluded from reflection in order to be ignored for byproduct replacement search.
    KAN_REFLECTION_IGNORE
    kan_interned_string_t source_material;

    // Excluded from reflection in order to be ignored for byproduct replacement search.
    KAN_REFLECTION_IGNORE
    kan_interned_string_t source_pass;
};

KAN_REFLECTION_STRUCT_META (kan_resource_material_pipeline_t)
RESOURCE_MATERIAL_API struct kan_resource_byproduct_type_meta_t kan_resource_material_pipeline_byproduct_type_meta = {
    .hash = NULL,
    .is_equal = NULL,
    .move = NULL,
    .reset = NULL,
};

static enum kan_resource_compile_result_t kan_resource_material_pipeline_compile (
    struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (kan_resource_material_pipeline_t)
RESOURCE_MATERIAL_API struct kan_resource_compilable_meta_t kan_resource_material_pipeline_compilable_meta = {
    .output_type_name = "kan_resource_material_pipeline_compiled_t",
    .configuration_type_name = "kan_resource_material_platform_configuration_t",
    // No state as render pipeline language does not support step by step emission right now.
    .state_type_name = NULL,
    .functor = kan_resource_material_pipeline_compile,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_pipeline_t, sources)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_pipeline_sources_reference_meta = {
    .type = "kan_resource_material_shader_source_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED,
};

RESOURCE_MATERIAL_API void kan_resource_material_pipeline_init (struct kan_resource_material_pipeline_t *instance)
{
    kan_dynamic_array_init (&instance->entry_points, 0u, sizeof (struct kan_rpl_entry_point_t),
                            _Alignof (struct kan_rpl_entry_point_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_resource_material_options_init (&instance->options);
    instance->source_material = NULL;
    instance->source_pass = NULL;
}

RESOURCE_MATERIAL_API void kan_resource_material_pipeline_shutdown (struct kan_resource_material_pipeline_t *instance)
{
    kan_dynamic_array_shutdown (&instance->entry_points);
    kan_dynamic_array_shutdown (&instance->sources);
    kan_resource_material_options_shutdown (&instance->options);
}

static kan_bool_t append_options (struct kan_resource_material_options_t *target,
                                  const struct kan_resource_material_options_t *source)
{
    kan_dynamic_array_set_capacity (&target->flags, target->flags.size + source->flags.size);
    kan_dynamic_array_set_capacity (&target->counts, target->counts.size + source->counts.size);

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) source->flags.size; ++index)
    {
        const struct kan_resource_material_flag_option_t *input =
            &((struct kan_resource_material_flag_option_t *) source->flags.data)[index];

        for (kan_loop_size_t target_index = 0u; target_index < (kan_loop_size_t) target->flags.size; ++target_index)
        {
            if (((struct kan_resource_material_flag_option_t *) target->flags.data)[target_index].name == input->name)
            {
                KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                         "Unable to append flag option \"%s\" as its value is already present.", input->name)
                return KAN_FALSE;
            }
        }

        struct kan_resource_material_flag_option_t *output = kan_dynamic_array_add_last (&target->flags);
        KAN_ASSERT (output)
        output->name = input->name;
        output->value = input->value;
    }

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) source->counts.size; ++index)
    {
        const struct kan_resource_material_count_option_t *input =
            &((struct kan_resource_material_count_option_t *) source->counts.data)[index];

        for (kan_loop_size_t target_index = 0u; target_index < (kan_loop_size_t) target->counts.size; ++target_index)
        {
            if (((struct kan_resource_material_count_option_t *) target->counts.data)[target_index].name == input->name)
            {
                KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                         "Unable to append count option \"%s\" as its value is already present.", input->name)
                return KAN_FALSE;
            }
        }

        struct kan_resource_material_count_option_t *output = kan_dynamic_array_add_last (&target->counts);
        KAN_ASSERT (output)
        output->name = input->name;
        output->value = input->value;
    }

    return KAN_TRUE;
}

static void sort_options (struct kan_resource_material_options_t *options)
{
    {
        struct kan_resource_material_flag_option_t temporary;

        KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#define AT_INDEX(INDEX) (((struct kan_resource_material_flag_option_t *) options->flags.data)[INDEX])
#define LESS(first_index, second_index) strcmp (AT_INDEX (first_index).name, AT_INDEX (second_index).name) < 0
#define SWAP(first_index, second_index)                                                                                \
    temporary = AT_INDEX (first_index), AT_INDEX (first_index) = AT_INDEX (second_index),                              \
    AT_INDEX (second_index) = temporary
        QSORT (options->flags.size, LESS, SWAP);
#undef LESS
#undef SWAP
#undef AT_INDEX
        KAN_MUTE_THIRD_PARTY_WARNINGS_END
    }

    {
        struct kan_resource_material_count_option_t temporary;

        KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#define AT_INDEX(INDEX) (((struct kan_resource_material_count_option_t *) options->counts.data)[INDEX])
#define LESS(first_index, second_index) strcmp (AT_INDEX (first_index).name, AT_INDEX (second_index).name) < 0
#define SWAP(first_index, second_index)                                                                                \
    temporary = AT_INDEX (first_index), AT_INDEX (first_index) = AT_INDEX (second_index),                              \
    AT_INDEX (second_index) = temporary
        QSORT (options->counts.size, LESS, SWAP);
#undef LESS
#undef SWAP
#undef AT_INDEX
        KAN_MUTE_THIRD_PARTY_WARNINGS_END
    }
}

static enum kan_resource_compile_result_t kan_resource_material_compile (struct kan_resource_compile_state_t *state)
{
    const struct kan_resource_material_t *input = state->input_instance;
    struct kan_resource_material_compiled_t *output = state->output_instance;

    if (input->sources.size == 0u)
    {
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to compile material \"%s\" as it has no sources.", state->name)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    kan_allocation_group_t main_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "material_compilation");
    kan_allocation_group_stack_push (main_allocation_group);

    kan_bool_t successful = KAN_TRUE;
    struct kan_dynamic_array_t sources;

    kan_dynamic_array_init (&sources, input->sources.size, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), main_allocation_group);
    kan_interned_string_t interned_kan_resource_material_shader_source_t =
        kan_string_intern ("kan_resource_material_shader_source_t");
    struct kan_resource_material_shader_source_t shader_source_byproduct;

    for (kan_loop_size_t index = 0u; index < input->sources.size && successful; ++index)
    {
        kan_interned_string_t source = ((kan_interned_string_t *) input->sources.data)[index];
        shader_source_byproduct.source = source;
        kan_interned_string_t registered_name = state->register_byproduct (
            state->interface_user_data, interned_kan_resource_material_shader_source_t, &shader_source_byproduct);

        if (!registered_name)
        {
            KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                     "Failed to register source byproduct for material \"%s\" for source \"%s\".", state->name, source)
            successful = KAN_FALSE;
        }

        *(kan_interned_string_t *) kan_dynamic_array_add_last (&sources) = registered_name;
    }

    if (successful)
    {
        kan_interned_string_t temporary;

        KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#define AT_INDEX(INDEX) (((kan_interned_string_t *) sources.data)[INDEX])
#define LESS(first_index, second_index) strcmp (AT_INDEX (first_index), AT_INDEX (second_index))
#define SWAP(first_index, second_index)                                                                                \
    temporary = AT_INDEX (first_index), AT_INDEX (first_index) = AT_INDEX (second_index),                              \
    AT_INDEX (second_index) = temporary
        QSORT (sources.size, LESS, SWAP);
#undef LESS
#undef SWAP
#undef AT_INDEX
        KAN_MUTE_THIRD_PARTY_WARNINGS_END
    }

    struct kan_resource_material_meta_t meta_byproduct;
    kan_resource_material_meta_init (&meta_byproduct);

    if (successful)
    {
        meta_byproduct.source_material = state->name;
        kan_dynamic_array_set_capacity (&meta_byproduct.sources, sources.size);
        meta_byproduct.sources.size = meta_byproduct.sources.capacity;
        memcpy (meta_byproduct.sources.data, sources.data, sources.size * sizeof (kan_interned_string_t));

        if (!append_options (&meta_byproduct.options, &input->global_options))
        {
            KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                     "Failed to append options for meta byproduct for material \"%s\".", state->name)
            successful = KAN_FALSE;
        }
    }

    if (successful)
    {
        sort_options (&meta_byproduct.options);
        output->meta = state->register_byproduct (state->interface_user_data,
                                                  kan_string_intern ("kan_resource_material_meta_t"), &meta_byproduct);

        if (!output->meta)
        {
            KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                     "Failed to register meta byproduct for material \"%s\".", state->name)
            successful = KAN_FALSE;
        }
    }

    kan_resource_material_meta_shutdown (&meta_byproduct);
    if (successful)
    {
        kan_dynamic_array_set_capacity (&output->passes, input->passes.size);
        kan_interned_string_t interned_kan_resource_material_pipeline_t =
            kan_string_intern ("kan_resource_material_pipeline_t");

        struct kan_resource_material_pipeline_t pipeline_byproduct;
        kan_resource_material_pipeline_init (&pipeline_byproduct);

        for (kan_loop_size_t index = 0u; index < input->passes.size && successful; ++index)
        {
            struct kan_resource_material_pass_t *source_pass =
                &((struct kan_resource_material_pass_t *) input->passes.data)[index];

            if (source_pass->entry_points.size == 0u)
            {
                KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                         "Material \"%s\" has pass \"%s\" without entry points.", state->name, source_pass->name)
                successful = KAN_FALSE;
                break;
            }

            struct kan_resource_material_pass_compiled_t *target_pass = kan_dynamic_array_add_last (&output->passes);
            KAN_ASSERT (target_pass)
            target_pass->name = source_pass->name;

            pipeline_byproduct.source_material = state->name;
            pipeline_byproduct.source_pass = source_pass->name;

            kan_dynamic_array_set_capacity (&pipeline_byproduct.entry_points, source_pass->entry_points.size);
            pipeline_byproduct.entry_points.size = pipeline_byproduct.entry_points.capacity;
            memcpy (pipeline_byproduct.entry_points.data, source_pass->entry_points.data,
                    pipeline_byproduct.entry_points.size * sizeof (struct kan_rpl_entry_point_t));

            {
                struct kan_rpl_entry_point_t temporary;

                KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#define AT_INDEX(INDEX) (((struct kan_rpl_entry_point_t *) pipeline_byproduct.entry_points.data)[INDEX])
#define LESS(first_index, second_index) AT_INDEX (first_index).stage < AT_INDEX (second_index).stage
#define SWAP(first_index, second_index)                                                                                \
    temporary = AT_INDEX (first_index), AT_INDEX (first_index) = AT_INDEX (second_index),                              \
    AT_INDEX (second_index) = temporary
                QSORT (pipeline_byproduct.entry_points.size, LESS, SWAP);
#undef LESS
#undef SWAP
#undef AT_INDEX
                KAN_MUTE_THIRD_PARTY_WARNINGS_END
            }

            kan_dynamic_array_set_capacity (&pipeline_byproduct.sources, sources.size);
            pipeline_byproduct.sources.size = pipeline_byproduct.sources.capacity;
            memcpy (pipeline_byproduct.sources.data, sources.data, sources.size * sizeof (kan_interned_string_t));

            if (!append_options (&pipeline_byproduct.options, &input->global_options))
            {
                KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                         "Failed to append global options for pass byproduct for material \"%s\" for pass \"%s\".",
                         state->name, source_pass->name)
                successful = KAN_FALSE;
                break;
            }

            if (!append_options (&pipeline_byproduct.options, &source_pass->options))
            {
                KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                         "Failed to append pass options for pass byproduct for material \"%s\" for pass \"%s\".",
                         state->name, source_pass->name)
                successful = KAN_FALSE;
                break;
            }

            sort_options (&pipeline_byproduct.options);
            target_pass->pipeline = state->register_byproduct (
                state->interface_user_data, interned_kan_resource_material_pipeline_t, &pipeline_byproduct);

            if (!target_pass->pipeline)
            {
                KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                         "Failed to register pass byproduct for material \"%s\" for pass \"%s\".", state->name,
                         source_pass->name)
                successful = KAN_FALSE;
                break;
            }
        }

        kan_resource_material_pipeline_shutdown (&pipeline_byproduct);
    }

    kan_dynamic_array_shutdown (&sources);
    kan_allocation_group_stack_pop ();
    return successful ? KAN_RESOURCE_PIPELINE_COMPILE_FINISHED : KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
}

static enum kan_resource_compile_result_t kan_resource_material_shader_source_compile (
    struct kan_resource_compile_state_t *state)
{
    const struct kan_resource_material_shader_source_t *input = state->input_instance;
    struct kan_resource_material_shader_source_compiled_t *output = state->output_instance;

    KAN_ASSERT (state->dependencies_count == 1u)
    KAN_ASSERT (state->dependencies->type == NULL)

    if (state->dependencies->data_size_if_third_party == 0u)
    {
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to compile \"%s\" as source \"%s\" loaded data is empty.", state->name,
                 state->dependencies->name)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    kan_rpl_parser_t parser = kan_rpl_parser_create (state->name);
    if (!kan_rpl_parser_add_source_char_sequence (
            parser, state->dependencies->data,
            ((char *) state->dependencies->data) + state->dependencies->data_size_if_third_party, input->source))
    {
        kan_rpl_parser_destroy (parser);
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to compile \"%s\" as source \"%s\" cannot be properly parsed.", state->name,
                 state->dependencies->name)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    if (!kan_rpl_parser_build_intermediate (parser, &output->intermediate))
    {
        kan_rpl_parser_destroy (parser);
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to compile \"%s\" as source \"%s\" export to intermediate format failed.", state->name,
                 state->dependencies->name)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    kan_rpl_parser_destroy (parser);
    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

static kan_bool_t add_sources_to_compiler_context (kan_rpl_compiler_context_t compiler_context,
                                                   struct kan_resource_compile_state_t *state)
{
    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) state->dependencies_count; ++index)
    {
        struct kan_resource_compilation_dependency_t *dependency = &state->dependencies[index];
        KAN_ASSERT (dependency->type == kan_string_intern ("kan_resource_material_shader_source_compiled_t"))
        const struct kan_resource_material_shader_source_compiled_t *source = dependency->data;

        if (!kan_rpl_compiler_context_use_module (compiler_context, &source->intermediate))
        {
            return KAN_FALSE;
        }
    }

    return KAN_TRUE;
}

static kan_bool_t apply_options_to_compiler_context (kan_rpl_compiler_context_t compiler_context,
                                                     const struct kan_resource_material_options_t *options)
{
    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) options->flags.size; ++index)
    {
        struct kan_resource_material_flag_option_t *option =
            &((struct kan_resource_material_flag_option_t *) options->flags.data)[index];

        if (!kan_rpl_compiler_context_set_option_flag (compiler_context, option->name, option->value))
        {
            return KAN_FALSE;
        }
    }

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) options->counts.size; ++index)
    {
        struct kan_resource_material_count_option_t *option =
            &((struct kan_resource_material_count_option_t *) options->counts.data)[index];

        if (!kan_rpl_compiler_context_set_option_count (compiler_context, option->name, option->value))
        {
            return KAN_FALSE;
        }
    }

    return KAN_TRUE;
}

static enum kan_resource_compile_result_t kan_resource_material_meta_compile (
    struct kan_resource_compile_state_t *state)
{
    const struct kan_resource_material_meta_t *input = state->input_instance;
    struct kan_resource_material_meta_compiled_t *output = state->output_instance;

    kan_rpl_compiler_context_t compiler_context =
        // Currently, all materials use graphics pipelines.
        kan_rpl_compiler_context_create (KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC, input->source_material);

    if (!add_sources_to_compiler_context (compiler_context, state))
    {
        kan_rpl_compiler_context_destroy (compiler_context);
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to resolve meta for \"%s\" (material \"%s\"): failed to use modules.", state->name,
                 input->source_material)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    if (!apply_options_to_compiler_context (compiler_context, &input->options))
    {
        kan_rpl_compiler_context_destroy (compiler_context);
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to resolve meta for \"%s\" (material \"%s\"): failed to set options.", state->name,
                 input->source_material)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    // We don't need entry points as we only need meta to be resolved.
    kan_rpl_compiler_instance_t compiler_instance = kan_rpl_compiler_context_resolve (compiler_context, 0u, NULL);

    if (!KAN_HANDLE_IS_VALID (compiler_instance))
    {
        kan_rpl_compiler_context_destroy (compiler_context);
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to resolve meta for \"%s\" (material \"%s\"): failed compilation resolve.", state->name,
                 input->source_material)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    if (!kan_rpl_compiler_instance_emit_meta (compiler_instance, &output->meta))
    {
        kan_rpl_compiler_instance_destroy (compiler_instance);
        kan_rpl_compiler_context_destroy (compiler_context);
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to resolve meta for \"%s\" (material \"%s\"): failed to emit meta.", state->name,
                 input->source_material)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    kan_rpl_compiler_instance_destroy (compiler_instance);
    kan_rpl_compiler_context_destroy (compiler_context);
    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

static enum kan_resource_compile_result_t kan_resource_material_pipeline_compile (
    struct kan_resource_compile_state_t *state)
{
    const struct kan_resource_material_pipeline_t *input = state->input_instance;
    struct kan_resource_material_pipeline_compiled_t *output = state->output_instance;
    const struct kan_resource_material_platform_configuration_t *configuration = state->platform_configuration;
    KAN_ASSERT (configuration)

    output->code_format = configuration->code_format;
    kan_dynamic_array_set_capacity (&output->entry_points, input->entry_points.size);
    output->entry_points.size = output->entry_points.capacity;
    memcpy (output->entry_points.data, input->entry_points.data,
            sizeof (struct kan_rpl_entry_point_t) * input->entry_points.size);

    kan_rpl_compiler_context_t compiler_context =
        // Currently, all materials use graphics pipelines.
        kan_rpl_compiler_context_create (KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC, input->source_material);

    if (!add_sources_to_compiler_context (compiler_context, state))
    {
        kan_rpl_compiler_context_destroy (compiler_context);
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to compile pipeline for \"%s\" (material \"%s\", pass \"%s\"): failed to use modules.",
                 state->name, input->source_material, input->source_pass)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    if (!apply_options_to_compiler_context (compiler_context, &input->options))
    {
        kan_rpl_compiler_context_destroy (compiler_context);
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to compile pipeline for \"%s\" (material \"%s\", pass \"%s\"): failed to set options.",
                 state->name, input->source_material, input->source_pass)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    // We don't need entry points as we only need meta to be resolved.
    kan_rpl_compiler_instance_t compiler_instance = kan_rpl_compiler_context_resolve (
        compiler_context, input->entry_points.size, (struct kan_rpl_entry_point_t *) input->entry_points.data);

    if (!KAN_HANDLE_IS_VALID (compiler_instance))
    {
        kan_rpl_compiler_context_destroy (compiler_context);
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to compile pipeline for \"%s\" (material \"%s\", pass \"%s\"): failed compilation resolve.",
                 state->name, input->source_material, input->source_pass)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    kan_bool_t emit_result = KAN_FALSE;
    switch (configuration->code_format)
    {
    case KAN_RENDER_CODE_FORMAT_SPIRV:
    {
        struct kan_dynamic_array_t spirv_code;
        emit_result = kan_rpl_compiler_instance_emit_spirv (
            compiler_instance, &spirv_code,
            kan_allocation_group_get_child (kan_allocation_group_root (), "material_pipeline_compilation"));

        if (emit_result)
        {
            KAN_ASSERT (spirv_code.size > 0u)
            kan_dynamic_array_set_capacity (&output->code, spirv_code.size * spirv_code.item_size);
            output->code.size = output->code.capacity;
            memcpy (output->code.data, spirv_code.data, output->code.size);
        }

        kan_dynamic_array_shutdown (&spirv_code);
        break;
    }
    }

    if (!emit_result)
    {
        kan_rpl_compiler_instance_destroy (compiler_instance);
        kan_rpl_compiler_context_destroy (compiler_context);
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to compile pipeline for \"%s\" (material \"%s\", pass \"%s\"): failed to emit code.",
                 state->name, input->source_material, input->source_pass)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    kan_rpl_compiler_instance_destroy (compiler_instance);
    kan_rpl_compiler_context_destroy (compiler_context);
    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

void kan_resource_material_options_init (struct kan_resource_material_options_t *instance)
{
    kan_dynamic_array_init (&instance->flags, 0u, sizeof (struct kan_resource_material_flag_option_t),
                            _Alignof (struct kan_resource_material_flag_option_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->counts, 0u, sizeof (struct kan_resource_material_count_option_t),
                            _Alignof (struct kan_resource_material_count_option_t), kan_allocation_group_stack_get ());
}

void kan_resource_material_options_shutdown (struct kan_resource_material_options_t *instance)
{
    kan_dynamic_array_shutdown (&instance->flags);
    kan_dynamic_array_shutdown (&instance->counts);
}

void kan_resource_material_pass_init (struct kan_resource_material_pass_t *instance)
{
    instance->name = NULL;
    kan_dynamic_array_init (&instance->entry_points, 0u, sizeof (struct kan_rpl_entry_point_t),
                            _Alignof (struct kan_rpl_entry_point_t), kan_allocation_group_stack_get ());
    kan_resource_material_options_init (&instance->options);
}

void kan_resource_material_pass_shutdown (struct kan_resource_material_pass_t *instance)
{
    kan_dynamic_array_shutdown (&instance->entry_points);
    kan_resource_material_options_shutdown (&instance->options);
}

void kan_resource_material_init (struct kan_resource_material_t *instance)
{
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_resource_material_options_init (&instance->global_options);
    kan_dynamic_array_init (&instance->passes, 0u, sizeof (struct kan_resource_material_pass_t),
                            _Alignof (struct kan_resource_material_pass_t), kan_allocation_group_stack_get ());
}

void kan_resource_material_shutdown (struct kan_resource_material_t *instance)
{
    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) instance->passes.size; ++index)
    {
        kan_resource_material_pass_shutdown (&((struct kan_resource_material_pass_t *) instance->passes.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->sources);
    kan_resource_material_options_shutdown (&instance->global_options);
    kan_dynamic_array_shutdown (&instance->passes);
}

void kan_resource_material_platform_configuration_init (struct kan_resource_material_platform_configuration_t *instance)
{
    instance->code_format = KAN_RENDER_CODE_FORMAT_SPIRV;
}

void kan_resource_material_pipeline_compiled_init (struct kan_resource_material_pipeline_compiled_t *instance)
{
    instance->code_format = KAN_RENDER_CODE_FORMAT_SPIRV;
    kan_dynamic_array_init (&instance->entry_points, 0u, sizeof (struct kan_rpl_entry_point_t),
                            _Alignof (struct kan_rpl_entry_point_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->code, 0u, sizeof (uint8_t), _Alignof (uint8_t),
                            kan_allocation_group_stack_get ());
}

void kan_resource_material_pipeline_compiled_shutdown (struct kan_resource_material_pipeline_compiled_t *instance)
{
    kan_dynamic_array_shutdown (&instance->entry_points);
    kan_dynamic_array_shutdown (&instance->code);
}

void kan_resource_material_meta_compiled_init (struct kan_resource_material_meta_compiled_t *instance)
{
    kan_rpl_meta_init (&instance->meta);
}

void kan_resource_material_meta_compiled_shutdown (struct kan_resource_material_meta_compiled_t *instance)
{
    kan_rpl_meta_shutdown (&instance->meta);
}

void kan_resource_material_compiled_init (struct kan_resource_material_compiled_t *instance)
{
    instance->meta = NULL;
    kan_dynamic_array_init (&instance->passes, 0u, sizeof (struct kan_resource_material_pass_compiled_t),
                            _Alignof (struct kan_resource_material_pass_compiled_t), kan_allocation_group_stack_get ());
}

void kan_resource_material_compiled_shutdown (struct kan_resource_material_compiled_t *instance)
{
    kan_dynamic_array_shutdown (&instance->passes);
}
