#include <string.h>

#include <qsort.h>

#include <kan/api_common/mute_warnings.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/render_pipeline_language/parser.h>
#include <kan/resource_material/resource_material.h>
#include <kan/resource_pipeline/resource_pipeline.h>

KAN_LOG_DEFINE_CATEGORY (resource_material_compilation);

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_pass_t, name)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_pass_name_reference_meta = {
    .type = "kan_resource_render_pass_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW,
};

KAN_REFLECTION_STRUCT_META (kan_resource_material_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t kan_resource_material_resource_type_meta = {
    .root = KAN_FALSE,
};

static enum kan_resource_compile_result_t kan_resource_material_compile (struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (kan_resource_material_t)
RESOURCE_MATERIAL_API struct kan_resource_compilable_meta_t kan_resource_material_compilable_meta = {
    .output_type_name = "kan_resource_material_compiled_t",
    .configuration_type_name = "kan_resource_material_platform_configuration_t",
    // No state as material compilation just spawns byproducts that need to be compiled separately later.
    .state_type_name = NULL,
    .functor = kan_resource_material_compile,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_t, sources)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_sources_reference_meta = {
    .type = NULL, // Null means third party.
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

KAN_REFLECTION_STRUCT_META (kan_resource_material_pipeline_family_compiled_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t
    kan_resource_material_pipeline_family_compiled_resource_type_meta = {
        .root = KAN_FALSE,
};

KAN_REFLECTION_STRUCT_META (kan_resource_material_pipeline_compiled_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t
    kan_resource_material_pipeline_compiled_resource_type_meta = {
        .root = KAN_FALSE,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_pass_variant_compiled_t, name)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_pass_compiled_name_reference_meta = {
    .type = "kan_resource_render_pass_compiled_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_pass_variant_compiled_t, pipeline)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_pass_compiled_pipeline_reference_meta =
    {
        .type = "kan_resource_material_pipeline_compiled_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

KAN_REFLECTION_STRUCT_META (kan_resource_material_compiled_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t kan_resource_material_compiled_resource_type_meta = {
    .root = KAN_FALSE,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_compiled_t, pipeline_family)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_compiled_family_reference_meta = {
    .type = "kan_resource_material_pipeline_family_compiled_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

struct kan_resource_material_pipeline_family_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t sources;

    struct kan_resource_rpl_options_t options;

    // Excluded from reflection in order to be ignored for byproduct replacement search.
    KAN_REFLECTION_IGNORE
    kan_interned_string_t source_material;
};

/// \details Custom move is needed because we're using fields ignored in reflection for compilation logs.
static void kan_resource_material_pipeline_family_move (void *target_void, void *source_void)
{
    struct kan_resource_material_pipeline_family_t *target = target_void;
    struct kan_resource_material_pipeline_family_t *source = source_void;

    kan_dynamic_array_init_move (&target->sources, &source->sources);
    kan_dynamic_array_init_move (&target->options.flags, &source->options.flags);
    kan_dynamic_array_init_move (&target->options.counts, &source->options.counts);
    target->source_material = source->source_material;
}

KAN_REFLECTION_STRUCT_META (kan_resource_material_pipeline_family_t)
RESOURCE_MATERIAL_API struct kan_resource_byproduct_type_meta_t
    kan_resource_material_pipeline_family_byproduct_type_meta = {
        .hash = NULL,
        .is_equal = NULL,
        .move = kan_resource_material_pipeline_family_move,
        .reset = NULL,
};

static enum kan_resource_compile_result_t kan_resource_material_pipeline_family_compile (
    struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (kan_resource_material_pipeline_family_t)
RESOURCE_MATERIAL_API struct kan_resource_compilable_meta_t kan_resource_material_pipeline_family_compilable_meta = {
    .output_type_name = "kan_resource_material_pipeline_family_compiled_t",
    .configuration_type_name = NULL,
    // No state as render pipeline language does not support step by step emission right now.
    .state_type_name = NULL,
    .functor = kan_resource_material_pipeline_family_compile,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_pipeline_family_t, sources)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t
    kan_resource_material_pipeline_family_sources_reference_meta = {
        .type = "kan_resource_rpl_source_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED,
};

RESOURCE_MATERIAL_API void kan_resource_material_pipeline_family_init (
    struct kan_resource_material_pipeline_family_t *instance)
{
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_resource_rpl_options_init (&instance->options);
    instance->source_material = NULL;
}

RESOURCE_MATERIAL_API void kan_resource_material_pipeline_family_shutdown (
    struct kan_resource_material_pipeline_family_t *instance)
{
    kan_dynamic_array_shutdown (&instance->sources);
    kan_resource_rpl_options_shutdown (&instance->options);
}

struct kan_resource_material_pipeline_t
{
    kan_interned_string_t family;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_entry_point_t)
    struct kan_dynamic_array_t entry_points;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t sources;

    struct kan_resource_rpl_options_t instance_options;

    // Excluded from reflection in order to be ignored for byproduct replacement search.
    KAN_REFLECTION_IGNORE
    kan_interned_string_t source_material;

    // Excluded from reflection in order to be ignored for byproduct replacement search.
    KAN_REFLECTION_IGNORE
    kan_interned_string_t source_pass;

    // Excluded from reflection in order to be ignored for byproduct replacement search.
    KAN_REFLECTION_IGNORE
    kan_instance_size_t source_pass_variant_index;
};

/// \details Custom move is needed because we're using fields ignored in reflection for compilation logs.
static void kan_resource_material_pipeline_move (void *target_void, void *source_void)
{
    struct kan_resource_material_pipeline_t *target = target_void;
    struct kan_resource_material_pipeline_t *source = source_void;

    target->family = source->family;
    kan_dynamic_array_init_move (&target->entry_points, &source->entry_points);
    kan_dynamic_array_init_move (&target->sources, &source->sources);
    kan_dynamic_array_init_move (&target->instance_options.flags, &source->instance_options.flags);
    kan_dynamic_array_init_move (&target->instance_options.counts, &source->instance_options.counts);
    target->source_material = source->source_material;
    target->source_pass = source->source_pass;
    target->source_pass_variant_index = source->source_pass_variant_index;
}

KAN_REFLECTION_STRUCT_META (kan_resource_material_pipeline_t)
RESOURCE_MATERIAL_API struct kan_resource_byproduct_type_meta_t kan_resource_material_pipeline_byproduct_type_meta = {
    .hash = NULL,
    .is_equal = NULL,
    .move = kan_resource_material_pipeline_move,
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

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_pipeline_t, family)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_pipeline_family_reference_meta = {
    .type = "kan_resource_material_pipeline_family_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_pipeline_t, sources)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_pipeline_sources_reference_meta = {
    .type = "kan_resource_rpl_source_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED,
};

RESOURCE_MATERIAL_API void kan_resource_material_pipeline_init (struct kan_resource_material_pipeline_t *instance)
{
    instance->family = NULL;
    kan_dynamic_array_init (&instance->entry_points, 0u, sizeof (struct kan_rpl_entry_point_t),
                            _Alignof (struct kan_rpl_entry_point_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_resource_rpl_options_init (&instance->instance_options);
    instance->source_material = NULL;
    instance->source_pass = NULL;
}

RESOURCE_MATERIAL_API void kan_resource_material_pipeline_shutdown (struct kan_resource_material_pipeline_t *instance)
{
    kan_dynamic_array_shutdown (&instance->entry_points);
    kan_dynamic_array_shutdown (&instance->sources);
    kan_resource_rpl_options_shutdown (&instance->instance_options);
}

static kan_bool_t append_options (struct kan_resource_rpl_options_t *target,
                                  const struct kan_resource_rpl_options_t *source)
{
    kan_dynamic_array_set_capacity (&target->flags, target->flags.size + source->flags.size);
    kan_dynamic_array_set_capacity (&target->counts, target->counts.size + source->counts.size);

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) source->flags.size; ++index)
    {
        const struct kan_resource_rpl_flag_option_t *input =
            &((struct kan_resource_rpl_flag_option_t *) source->flags.data)[index];

        for (kan_loop_size_t target_index = 0u; target_index < (kan_loop_size_t) target->flags.size; ++target_index)
        {
            if (((struct kan_resource_rpl_flag_option_t *) target->flags.data)[target_index].name == input->name)
            {
                KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                         "Unable to append flag option \"%s\" as its value is already present.", input->name)
                return KAN_FALSE;
            }
        }

        struct kan_resource_rpl_flag_option_t *output = kan_dynamic_array_add_last (&target->flags);
        KAN_ASSERT (output)
        output->name = input->name;
        output->value = input->value;
    }

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) source->counts.size; ++index)
    {
        const struct kan_resource_rpl_count_option_t *input =
            &((struct kan_resource_rpl_count_option_t *) source->counts.data)[index];

        for (kan_loop_size_t target_index = 0u; target_index < (kan_loop_size_t) target->counts.size; ++target_index)
        {
            if (((struct kan_resource_rpl_count_option_t *) target->counts.data)[target_index].name == input->name)
            {
                KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                         "Unable to append count option \"%s\" as its value is already present.", input->name)
                return KAN_FALSE;
            }
        }

        struct kan_resource_rpl_count_option_t *output = kan_dynamic_array_add_last (&target->counts);
        KAN_ASSERT (output)
        output->name = input->name;
        output->value = input->value;
    }

    return KAN_TRUE;
}

static void sort_options (struct kan_resource_rpl_options_t *options)
{
    {
        struct kan_resource_rpl_flag_option_t temporary;

        KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#define AT_INDEX(INDEX) (((struct kan_resource_rpl_flag_option_t *) options->flags.data)[INDEX])
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
        struct kan_resource_rpl_count_option_t temporary;

        KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#define AT_INDEX(INDEX) (((struct kan_resource_rpl_count_option_t *) options->counts.data)[INDEX])
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

KAN_REFLECTION_IGNORE
struct material_pass_registration_context_t
{
    struct kan_resource_compile_state_t *state;
    const struct kan_resource_material_pass_t *material_pass;
    struct kan_dynamic_array_t *material_sources;
    struct kan_resource_material_pipeline_t *pipeline_byproduct;

    kan_interned_string_t interned_kan_resource_rpl_source_t;
    kan_interned_string_t interned_kan_resource_material_pipeline_t;
};

static void sort_source_list (struct kan_dynamic_array_t *sources)
{
    kan_interned_string_t temporary;

    KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#define AT_INDEX(INDEX) (((kan_interned_string_t *) sources->data)[INDEX])
#define LESS(first_index, second_index) strcmp (AT_INDEX (first_index), AT_INDEX (second_index))
#define SWAP(first_index, second_index)                                                                                \
    temporary = AT_INDEX (first_index), AT_INDEX (first_index) = AT_INDEX (second_index),                              \
    AT_INDEX (second_index) = temporary
    QSORT (sources->size, LESS, SWAP);
#undef LESS
#undef SWAP
#undef AT_INDEX
    KAN_MUTE_THIRD_PARTY_WARNINGS_END
}

static kan_bool_t material_register_pass_variant (struct material_pass_registration_context_t *context,
                                                  struct kan_resource_render_pass_variant_description_t *pass_variant,
                                                  kan_instance_size_t pass_variant_index)
{
    struct kan_resource_compile_state_t *state = context->state;
    const struct kan_resource_material_pass_t *material_pass = context->material_pass;
    struct kan_dynamic_array_t *material_sources = context->material_sources;
    struct kan_resource_material_pipeline_t *pipeline_byproduct = context->pipeline_byproduct;
    const struct kan_resource_material_t *input = state->input_instance;
    struct kan_resource_material_compiled_t *output = state->output_instance;

    struct kan_resource_material_pass_variant_compiled_t *target_variant =
        kan_dynamic_array_add_last (&output->pass_variants);

    if (!target_variant)
    {
        kan_dynamic_array_set_capacity (&output->pass_variants, output->pass_variants.capacity * 2u);
        target_variant = kan_dynamic_array_add_last (&output->pass_variants);
    }

    KAN_ASSERT (target_variant)
    target_variant->name = material_pass->name;
    target_variant->variant_index = pass_variant_index;

    pipeline_byproduct->family = output->pipeline_family;
    pipeline_byproduct->source_material = state->name;
    pipeline_byproduct->source_pass = material_pass->name;
    pipeline_byproduct->source_pass_variant_index = pass_variant_index;

    kan_dynamic_array_set_capacity (&pipeline_byproduct->entry_points, material_pass->entry_points.size);
    pipeline_byproduct->entry_points.size = pipeline_byproduct->entry_points.capacity;
    memcpy (pipeline_byproduct->entry_points.data, material_pass->entry_points.data,
            pipeline_byproduct->entry_points.size * sizeof (struct kan_rpl_entry_point_t));

    {
        struct kan_rpl_entry_point_t temporary;

        KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#define AT_INDEX(INDEX) (((struct kan_rpl_entry_point_t *) pipeline_byproduct->entry_points.data)[INDEX])
#define LESS(first_index, second_index) AT_INDEX (first_index).stage < AT_INDEX (second_index).stage
#define SWAP(first_index, second_index)                                                                                \
    temporary = AT_INDEX (first_index), AT_INDEX (first_index) = AT_INDEX (second_index),                              \
    AT_INDEX (second_index) = temporary
        QSORT (pipeline_byproduct->entry_points.size, LESS, SWAP);
#undef LESS
#undef SWAP
#undef AT_INDEX
        KAN_MUTE_THIRD_PARTY_WARNINGS_END
    }

    kan_dynamic_array_set_capacity (&pipeline_byproduct->sources,
                                    material_sources->size + (pass_variant ? pass_variant->sources.size : 0u));

    pipeline_byproduct->sources.size = material_sources->size;
    memcpy (pipeline_byproduct->sources.data, material_sources->data,
            material_sources->size * sizeof (kan_interned_string_t));

    // Append pass parameter set code if it exists.
    if (pass_variant)
    {
        struct kan_resource_rpl_source_t code_source_byproduct;

        for (kan_loop_size_t source_index = 0u; source_index < pass_variant->sources.size; ++source_index)
        {
            const kan_interned_string_t source_name =
                ((kan_interned_string_t *) pass_variant->sources.data)[source_index];

            // Check that this source is not already included into material, which can happen for utility sources with
            // various structures and helpers. We do check before registering byproduct, because byproduct registration
            // would be slower than gains from using search on sorted sources instead of raw sources.
            kan_bool_t already_added = KAN_FALSE;

            for (kan_loop_size_t material_source_index = 0u; material_source_index < input->sources.size;
                 ++material_source_index)
            {
                if (((kan_interned_string_t *) input->sources.data)[material_source_index] == source_name)
                {
                    already_added = KAN_TRUE;
                    break;
                }
            }

            if (!already_added)
            {
                code_source_byproduct.source = source_name;
                kan_interned_string_t source_registered_name = state->register_byproduct (
                    state->interface_user_data, context->interned_kan_resource_rpl_source_t, &code_source_byproduct);

                if (!source_registered_name)
                {
                    KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                             "Failed to register source byproduct for material \"%s\" for source \"%s\" (for pass "
                             "\"%s\" for variant %lu).",
                             state->name, source_name, material_pass->name, (unsigned long) pass_variant_index)
                    return KAN_FALSE;
                }

                *(kan_interned_string_t *) kan_dynamic_array_add_last (&pipeline_byproduct->sources) =
                    source_registered_name;
            }
        }

        // We need to sort source list again in order to make it possible for
        // pipelines to predictably replace each other.
        sort_source_list (&pipeline_byproduct->sources);

        if (!append_options (&pipeline_byproduct->instance_options, &pass_variant->instance_options))
        {
            KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                     "Failed to append pass instance options for pass byproduct for material \"%s\" for pass \"%s\" "
                     "for variant %lu (from variant options list).",
                     state->name, material_pass->name, (unsigned long) pass_variant_index)
            return KAN_FALSE;
        }
    }

    if (!append_options (&pipeline_byproduct->instance_options, &material_pass->options))
    {
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to append pass instance options for pass byproduct for material \"%s\" for pass \"%s\" for "
                 "variant %lu (from material options list).",
                 state->name, material_pass->name, (unsigned long) pass_variant_index)
        return KAN_FALSE;
    }

    sort_options (&pipeline_byproduct->instance_options);
    target_variant->pipeline = state->register_byproduct (
        state->interface_user_data, context->interned_kan_resource_material_pipeline_t, pipeline_byproduct);

    if (!target_variant->pipeline)
    {
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to register pass byproduct for material \"%s\" for pass \"%s\" for variant %lu.", state->name,
                 material_pass->name, (unsigned long) pass_variant_index)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static enum kan_resource_compile_result_t kan_resource_material_compile (struct kan_resource_compile_state_t *state)
{
    const struct kan_resource_material_t *input = state->input_instance;
    struct kan_resource_material_compiled_t *output = state->output_instance;
    const struct kan_resource_material_platform_configuration_t *configuration = state->platform_configuration;
    KAN_ASSERT (configuration)

    if (input->sources.size == 0u)
    {
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to compile material \"%s\" as it has no sources.", state->name)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    kan_interned_string_t interned_kan_resource_rpl_source_t = kan_string_intern ("kan_resource_rpl_source_t");
    kan_interned_string_t interned_kan_resource_render_pass_t = kan_string_intern ("kan_resource_render_pass_t");

    kan_allocation_group_t main_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "material_compilation");
    kan_allocation_group_stack_push (main_allocation_group);

    kan_bool_t successful = KAN_TRUE;
    struct kan_dynamic_array_t sources;

    kan_dynamic_array_init (&sources, input->sources.size, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), main_allocation_group);
    struct kan_resource_rpl_source_t code_source_byproduct;

    for (kan_loop_size_t index = 0u; index < input->sources.size && successful; ++index)
    {
        kan_interned_string_t source = ((kan_interned_string_t *) input->sources.data)[index];
        code_source_byproduct.source = source;
        kan_interned_string_t registered_name = state->register_byproduct (
            state->interface_user_data, interned_kan_resource_rpl_source_t, &code_source_byproduct);

        if (!registered_name)
        {
            KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                     "Failed to register source byproduct for material \"%s\" for source \"%s\".", state->name, source)
            successful = KAN_FALSE;
        }

        *(kan_interned_string_t *) kan_dynamic_array_add_last (&sources) = registered_name;
    }

    sort_source_list (&sources);
    struct kan_resource_material_pipeline_family_t meta_byproduct;
    kan_resource_material_pipeline_family_init (&meta_byproduct);

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
        output->pipeline_family = state->register_byproduct (
            state->interface_user_data, kan_string_intern ("kan_resource_material_pipeline_family_t"), &meta_byproduct);

        if (!output->pipeline_family)
        {
            KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                     "Failed to register family byproduct for material \"%s\".", state->name)
            successful = KAN_FALSE;
        }
    }

    kan_resource_material_pipeline_family_shutdown (&meta_byproduct);
    if (successful)
    {
        kan_dynamic_array_set_capacity (&output->pass_variants, input->passes.size);
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

            const struct kan_resource_render_pass_t *pass = NULL;
            for (kan_loop_size_t dependency_index = 0u; dependency_index < state->dependencies_count;
                 ++dependency_index)
            {
                if (state->dependencies[dependency_index].name == source_pass->name &&
                    state->dependencies[dependency_index].type == interned_kan_resource_render_pass_t)
                {
                    pass = state->dependencies[dependency_index].data;
                    break;
                }
            }

            if (!pass)
            {
                KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                         "Material \"%s\" has pass \"%s\", but there is not render graph pass resource with this name.",
                         state->name, source_pass->name)
                successful = KAN_FALSE;
                break;
            }

            if (!kan_resource_material_platform_configuration_is_pass_supported (configuration, pass))
            {
                // Pass is not supported in current compilation context, therefore we can just skip it.
                continue;
            }

            struct material_pass_registration_context_t context = {
                .state = state,
                .material_pass = source_pass,
                .material_sources = &sources,
                .pipeline_byproduct = &pipeline_byproduct,
                .interned_kan_resource_rpl_source_t = interned_kan_resource_rpl_source_t,
                .interned_kan_resource_material_pipeline_t = interned_kan_resource_material_pipeline_t,
            };

            if (pass->variants.size == 0u)
            {
                // Special case -- pass with no variants is technically just a pass
                // with one variant with no customization on it at all.
                successful &= material_register_pass_variant (&context, NULL, 0u);
            }
            else
            {
                for (kan_loop_size_t variant_index = 0u; variant_index < pass->variants.size; ++variant_index)
                {
                    successful &= material_register_pass_variant (
                        &context,
                        &((struct kan_resource_render_pass_variant_description_t *) pass->variants.data)[variant_index],
                        (kan_instance_size_t) variant_index);
                }
            }
        }

        kan_resource_material_pipeline_shutdown (&pipeline_byproduct);
        kan_dynamic_array_set_capacity (&output->pass_variants, output->pass_variants.size);
    }

    kan_dynamic_array_shutdown (&sources);
    kan_allocation_group_stack_pop ();
    return successful ? KAN_RESOURCE_PIPELINE_COMPILE_FINISHED : KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
}

static kan_bool_t apply_options_to_compiler_context (kan_rpl_compiler_context_t compiler_context,
                                                     enum kan_rpl_option_target_scope_t target_scope,
                                                     const struct kan_resource_rpl_options_t *options)
{
    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) options->flags.size; ++index)
    {
        struct kan_resource_rpl_flag_option_t *option =
            &((struct kan_resource_rpl_flag_option_t *) options->flags.data)[index];

        if (!kan_rpl_compiler_context_set_option_flag (compiler_context, target_scope, option->name, option->value))
        {
            return KAN_FALSE;
        }
    }

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) options->counts.size; ++index)
    {
        struct kan_resource_rpl_count_option_t *option =
            &((struct kan_resource_rpl_count_option_t *) options->counts.data)[index];

        if (!kan_rpl_compiler_context_set_option_count (compiler_context, target_scope, option->name, option->value))
        {
            return KAN_FALSE;
        }
    }

    return KAN_TRUE;
}

static enum kan_resource_compile_result_t kan_resource_material_pipeline_family_compile (
    struct kan_resource_compile_state_t *state)
{
    const struct kan_resource_material_pipeline_family_t *input = state->input_instance;
    struct kan_resource_material_pipeline_family_compiled_t *output = state->output_instance;

    kan_rpl_compiler_context_t compiler_context =
        // Currently, all materials use graphics pipelines.
        kan_rpl_compiler_context_create (KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC, input->source_material);

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) state->dependencies_count; ++index)
    {
        struct kan_resource_compilation_dependency_t *dependency = &state->dependencies[index];
        KAN_ASSERT (dependency->type == kan_string_intern ("kan_resource_rpl_source_compiled_t"))
        const struct kan_resource_rpl_source_compiled_t *source = dependency->data;

        if (!kan_rpl_compiler_context_use_module (compiler_context, &source->intermediate))
        {
            kan_rpl_compiler_context_destroy (compiler_context);
            KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                     "Failed to resolve meta for \"%s\" (material \"%s\"): failed to use modules.", state->name,
                     input->source_material)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }
    }

    if (!apply_options_to_compiler_context (compiler_context, KAN_RPL_OPTION_TARGET_SCOPE_GLOBAL, &input->options))
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

    struct kan_rpl_meta_t meta;
    kan_rpl_meta_init (&meta);

    if (!kan_rpl_compiler_instance_emit_meta (compiler_instance, &meta, KAN_RPL_META_EMISSION_FULL))
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
    kan_bool_t meta_valid = KAN_TRUE;

    if (meta.set_pass.buffers.size > 0u || meta.set_pass.samplers.size > 0u)
    {
        KAN_LOG (
            resource_material_compilation, KAN_LOG_ERROR,
            "Produced incorrect meta for \"%s\" (material \"%s\"): meta has entries in pass set, but it should've been "
            "compiled from pass-agnostic sources. That means that source list contains pass set, but it shouldn't.",
            state->name, input->source_material)
        meta_valid = KAN_FALSE;
    }

    kan_dynamic_array_set_capacity (&output->vertex_attribute_buffers, meta.attribute_buffers.size);
    for (kan_loop_size_t index = 0u; index < meta.attribute_buffers.size; ++index)
    {
        const struct kan_rpl_meta_buffer_t *source =
            &((struct kan_rpl_meta_buffer_t *) meta.attribute_buffers.data)[index];

        switch (source->type)
        {
        case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
        {
            struct kan_rpl_meta_buffer_t *target = kan_dynamic_array_add_last (&output->vertex_attribute_buffers);
            KAN_ASSERT (target)
            kan_rpl_meta_buffer_init_copy (target, source);
            break;
        }

        case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
            if (output->has_instanced_attribute_buffer)
            {
                KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                         "Produced incorrect meta for \"%s\" (material \"%s\"): meta has several instanced attribute "
                         "buffers, but it is not supported by materials right now.",
                         state->name, input->source_material)
                meta_valid = KAN_FALSE;
            }
            else
            {
                output->has_instanced_attribute_buffer = KAN_TRUE;
                kan_rpl_meta_buffer_shutdown (&output->instanced_attribute_buffer);
                kan_rpl_meta_buffer_init_copy (&output->instanced_attribute_buffer, source);
            }

            break;

        case KAN_RPL_BUFFER_TYPE_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
        case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
        case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
            KAN_ASSERT (KAN_FALSE)
            break;
        }
    }

    for (kan_loop_size_t index = 0u; index < meta.set_material.images.size; ++index)
    {
        struct kan_rpl_meta_image_t *image = &((struct kan_rpl_meta_image_t *) meta.set_material.images.data)[index];
        if (image->image_array_size > 1u)
        {
            KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                     "Produced incorrect meta for \"%s\" (material \"%s\"): meta has image array \"%s\" in material "
                     "set, but image arrays are not yet supported on material level.",
                     state->name, input->source_material, image->name)
            meta_valid = KAN_FALSE;
        }
    }

    kan_rpl_meta_set_bindings_shutdown (&output->set_material);
    kan_rpl_meta_set_bindings_init_copy (&output->set_material, &meta.set_material);

    kan_rpl_meta_set_bindings_shutdown (&output->set_object);
    kan_rpl_meta_set_bindings_init_copy (&output->set_object, &meta.set_object);

    kan_rpl_meta_set_bindings_shutdown (&output->set_shared);
    kan_rpl_meta_set_bindings_init_copy (&output->set_shared, &meta.set_shared);

    kan_rpl_meta_shutdown (&meta);
    return meta_valid ? KAN_RESOURCE_PIPELINE_COMPILE_FINISHED : KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
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

    const struct kan_resource_material_pipeline_family_t *family = NULL;
    kan_interned_string_t interned_kan_resource_rpl_source_compiled_t =
        kan_string_intern ("kan_resource_rpl_source_compiled_t");
    kan_interned_string_t interned_kan_resource_material_pipeline_family_t =
        kan_string_intern ("kan_resource_material_pipeline_family_t");

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) state->dependencies_count; ++index)
    {
        struct kan_resource_compilation_dependency_t *dependency = &state->dependencies[index];
        if (dependency->type == interned_kan_resource_rpl_source_compiled_t)
        {
            const struct kan_resource_rpl_source_compiled_t *source = dependency->data;

            if (!kan_rpl_compiler_context_use_module (compiler_context, &source->intermediate))
            {
                kan_rpl_compiler_context_destroy (compiler_context);
                KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                         "Failed to compile pipeline for \"%s\" (material \"%s\", pass \"%s\"): failed to use modules.",
                         state->name, input->source_material, input->source_pass)
                return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
            }
        }
        else if (dependency->type == interned_kan_resource_material_pipeline_family_t)
        {
            family = dependency->data;
        }
        else
        {
            KAN_ASSERT (KAN_FALSE)
        }
    }

    if (!family)
    {
        kan_rpl_compiler_context_destroy (compiler_context);
        KAN_LOG (
            resource_material_compilation, KAN_LOG_ERROR,
            "Failed to compile pipeline for \"%s\" (material \"%s\", pass \"%s\", variant %lu): unable to find family.",
            state->name, input->source_material, input->source_pass, (unsigned long) input->source_pass_variant_index)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    if (!apply_options_to_compiler_context (compiler_context, KAN_RPL_OPTION_TARGET_SCOPE_GLOBAL, &family->options))
    {
        kan_rpl_compiler_context_destroy (compiler_context);
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to compile pipeline for \"%s\" (material \"%s\", pass \"%s\", variant %lu): failed to set "
                 "global options.",
                 state->name, input->source_material, input->source_pass,
                 (unsigned long) input->source_pass_variant_index)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    if (!apply_options_to_compiler_context (compiler_context, KAN_RPL_OPTION_TARGET_SCOPE_INSTANCE,
                                            &input->instance_options))
    {
        kan_rpl_compiler_context_destroy (compiler_context);
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to compile pipeline for \"%s\" (material \"%s\", pass \"%s\", variant %lu): failed to set "
                 "instance options.",
                 state->name, input->source_material, input->source_pass,
                 (unsigned long) input->source_pass_variant_index)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    // We don't need entry points as we only need meta to be resolved.
    kan_rpl_compiler_instance_t compiler_instance = kan_rpl_compiler_context_resolve (
        compiler_context, input->entry_points.size, (struct kan_rpl_entry_point_t *) input->entry_points.data);

    if (!KAN_HANDLE_IS_VALID (compiler_instance))
    {
        kan_rpl_compiler_context_destroy (compiler_context);
        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to compile pipeline for \"%s\" (material \"%s\", pass \"%s\", variant %lu): failed "
                 "compilation resolve.",
                 state->name, input->source_material, input->source_pass,
                 (unsigned long) input->source_pass_variant_index)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    struct kan_rpl_meta_t meta;
    kan_rpl_meta_init (&meta);

    if (!kan_rpl_compiler_instance_emit_meta (
            compiler_instance, &meta, KAN_RPL_META_EMISSION_SKIP_ATTRIBUTE_BUFFERS | KAN_RPL_META_EMISSION_SKIP_SETS))
    {
        kan_rpl_compiler_instance_destroy (compiler_instance);
        kan_rpl_compiler_context_destroy (compiler_context);
        kan_rpl_meta_shutdown (&meta);

        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to resolve meta for \"%s\" (material \"%s\", pass \"%s\", variant %lu): failed to emit meta.",
                 state->name, input->source_material, input->source_pass,
                 (unsigned long) input->source_pass_variant_index)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    if (meta.pipeline_type != KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC)
    {
        kan_rpl_compiler_instance_destroy (compiler_instance);
        kan_rpl_compiler_context_destroy (compiler_context);
        kan_rpl_meta_shutdown (&meta);

        KAN_LOG (resource_material_compilation, KAN_LOG_ERROR,
                 "Failed to resolve meta for \"%s\" (material \"%s\", pass \"%s\", variant %lu): pipeline type is not "
                 "graphics classic, other types are not supported.",
                 state->name, input->source_material, input->source_pass,
                 (unsigned long) input->source_pass_variant_index)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    output->pipeline_settings = meta.graphics_classic_settings;
    kan_dynamic_array_shutdown (&output->color_outputs);
    kan_dynamic_array_init_move (&output->color_outputs, &meta.color_outputs);
    output->color_blend_constants = meta.color_blend_constants;

    kan_rpl_meta_shutdown (&meta);
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
        KAN_LOG (
            resource_material_compilation, KAN_LOG_ERROR,
            "Failed to compile pipeline for \"%s\" (material \"%s\", pass \"%s\", variant %lu): failed to emit code.",
            state->name, input->source_material, input->source_pass, (unsigned long) input->source_pass_variant_index)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    kan_rpl_compiler_instance_destroy (compiler_instance);
    kan_rpl_compiler_context_destroy (compiler_context);
    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

void kan_resource_material_pass_init (struct kan_resource_material_pass_t *instance)
{
    instance->name = NULL;
    kan_dynamic_array_init (&instance->entry_points, 0u, sizeof (struct kan_rpl_entry_point_t),
                            _Alignof (struct kan_rpl_entry_point_t), kan_allocation_group_stack_get ());
    kan_resource_rpl_options_init (&instance->options);
}

void kan_resource_material_pass_shutdown (struct kan_resource_material_pass_t *instance)
{
    kan_dynamic_array_shutdown (&instance->entry_points);
    kan_resource_rpl_options_shutdown (&instance->options);
}

void kan_resource_material_init (struct kan_resource_material_t *instance)
{
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_resource_rpl_options_init (&instance->global_options);
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
    kan_resource_rpl_options_shutdown (&instance->global_options);
    kan_dynamic_array_shutdown (&instance->passes);
}

void kan_resource_material_platform_configuration_init (struct kan_resource_material_platform_configuration_t *instance)
{
    instance->code_format = KAN_RENDER_CODE_FORMAT_SPIRV;
    kan_dynamic_array_init (&instance->supported_pass_tags, 0u, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), kan_allocation_group_stack_get ());
}

void kan_resource_material_platform_configuration_shutdown (
    struct kan_resource_material_platform_configuration_t *instance)
{
    kan_dynamic_array_shutdown (&instance->supported_pass_tags);
}

kan_bool_t kan_resource_material_platform_configuration_is_pass_supported (
    const struct kan_resource_material_platform_configuration_t *configuration,
    const struct kan_resource_render_pass_t *pass)
{
    for (kan_loop_size_t required_index = 0u; required_index < pass->required_tags.size; ++required_index)
    {
        const kan_interned_string_t required_tag = ((kan_interned_string_t *) pass->required_tags.data)[required_index];
        kan_bool_t found = KAN_FALSE;

        for (kan_loop_size_t supported_index = 0u; supported_index < configuration->supported_pass_tags.size;
             ++supported_index)
        {
            const kan_interned_string_t supported_tag =
                ((kan_interned_string_t *) configuration->supported_pass_tags.data)[supported_index];

            if (supported_tag == required_tag)
            {
                found = KAN_TRUE;
                break;
            }
        }

        if (!found)
        {
            return KAN_FALSE;
        }
    }

    return KAN_TRUE;
}

void kan_resource_material_pipeline_family_compiled_init (
    struct kan_resource_material_pipeline_family_compiled_t *instance)
{
    kan_dynamic_array_init (&instance->vertex_attribute_buffers, 0u, sizeof (struct kan_rpl_meta_buffer_t),
                            _Alignof (struct kan_rpl_meta_buffer_t), kan_allocation_group_stack_get ());

    instance->has_instanced_attribute_buffer = KAN_FALSE;
    kan_rpl_meta_buffer_init (&instance->instanced_attribute_buffer);

    kan_rpl_meta_set_bindings_init (&instance->set_material);
    kan_rpl_meta_set_bindings_init (&instance->set_object);
    kan_rpl_meta_set_bindings_init (&instance->set_shared);
}

void kan_resource_material_pipeline_family_compiled_shutdown (
    struct kan_resource_material_pipeline_family_compiled_t *instance)
{
    for (kan_loop_size_t index = 0u; index < instance->vertex_attribute_buffers.size; ++index)
    {
        kan_rpl_meta_buffer_shutdown (
            &((struct kan_rpl_meta_buffer_t *) instance->vertex_attribute_buffers.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->vertex_attribute_buffers);
    kan_rpl_meta_buffer_shutdown (&instance->instanced_attribute_buffer);
    kan_rpl_meta_set_bindings_shutdown (&instance->set_material);
    kan_rpl_meta_set_bindings_shutdown (&instance->set_object);
    kan_rpl_meta_set_bindings_shutdown (&instance->set_shared);
}

void kan_resource_material_pipeline_compiled_init (struct kan_resource_material_pipeline_compiled_t *instance)
{
    kan_dynamic_array_init (&instance->entry_points, 0u, sizeof (struct kan_rpl_entry_point_t),
                            _Alignof (struct kan_rpl_entry_point_t), kan_allocation_group_stack_get ());

    instance->code_format = KAN_RENDER_CODE_FORMAT_SPIRV;
    instance->pipeline_settings = kan_rpl_graphics_classic_pipeline_settings_default ();

    kan_dynamic_array_init (&instance->color_outputs, 0u, sizeof (struct kan_rpl_meta_color_output_t),
                            _Alignof (struct kan_rpl_meta_color_output_t), kan_allocation_group_stack_get ());

    instance->color_blend_constants = (struct kan_rpl_color_blend_constants_t) {
        .r = 0.0f,
        .g = 0.0f,
        .b = 0.0f,
        .a = 0.0f,
    };

    kan_dynamic_array_init (&instance->code, 0u, sizeof (uint8_t), _Alignof (uint8_t),
                            kan_allocation_group_stack_get ());
}

void kan_resource_material_pipeline_compiled_shutdown (struct kan_resource_material_pipeline_compiled_t *instance)
{
    kan_dynamic_array_shutdown (&instance->entry_points);
    kan_dynamic_array_shutdown (&instance->color_outputs);
    kan_dynamic_array_shutdown (&instance->code);
}

void kan_resource_material_compiled_init (struct kan_resource_material_compiled_t *instance)
{
    instance->pipeline_family = NULL;
    kan_dynamic_array_init (&instance->pass_variants, 0u, sizeof (struct kan_resource_material_pass_variant_compiled_t),
                            _Alignof (struct kan_resource_material_pass_variant_compiled_t),
                            kan_allocation_group_stack_get ());
}

void kan_resource_material_compiled_shutdown (struct kan_resource_material_compiled_t *instance)
{
    kan_dynamic_array_shutdown (&instance->pass_variants);
}
