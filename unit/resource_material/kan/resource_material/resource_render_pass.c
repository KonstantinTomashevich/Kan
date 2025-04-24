#include <string.h>

#include <qsort.h>

#include <kan/api_common/mute_warnings.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/render_pipeline_language/parser.h>
#include <kan/resource_material/resource_material.h>
#include <kan/resource_material/resource_render_pass.h>
#include <kan/resource_pipeline/resource_pipeline.h>

KAN_LOG_DEFINE_CATEGORY (resource_pass_compilation);

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_render_pass_variant_description_t, sources)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_render_pass_pass_set_source_reference_meta = {
    .type = NULL, // Null means third party.
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

KAN_REFLECTION_STRUCT_META (kan_resource_render_pass_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t kan_resource_render_pass_resource_type_meta = {
    .root = KAN_TRUE,
};

static enum kan_resource_compile_result_t kan_resource_render_pass_compile (struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (kan_resource_render_pass_t)
RESOURCE_MATERIAL_API struct kan_resource_compilable_meta_t kan_resource_render_pass_compilable_meta = {
    .output_type_name = "kan_resource_render_pass_compiled_t",
    .configuration_type_name = "kan_resource_material_platform_configuration_t",
    // No state as render_pass compilation just spawns byproducts that need to be compiled separately later.
    .state_type_name = NULL,
    .functor = kan_resource_render_pass_compile,
};

KAN_REFLECTION_STRUCT_META (kan_resource_render_pass_compiled_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t kan_resource_render_pass_compiled_resource_type_meta = {
    .root = KAN_TRUE,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_render_pass_compiled_t, variants)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_render_pass_compiled_variant_reference_meta = {
    .type = "kan_resource_render_pass_variant_compiled_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

struct kan_resource_render_pass_variant_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t sources;

    KAN_REFLECTION_IGNORE
    kan_interned_string_t source_pass;

    KAN_REFLECTION_IGNORE
    kan_instance_size_t source_variant_index;
};

RESOURCE_MATERIAL_API void kan_resource_render_pass_variant_init (struct kan_resource_render_pass_variant_t *instance)
{
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
}

RESOURCE_MATERIAL_API void kan_resource_render_pass_variant_shutdown (
    struct kan_resource_render_pass_variant_t *instance)
{
    kan_dynamic_array_shutdown (&instance->sources);
}

/// \details Custom move is needed because we're using fields ignored in reflection for compilation logs.
static void kan_resource_render_pass_variant_move (void *target_void, void *source_void)
{
    struct kan_resource_render_pass_variant_t *target = target_void;
    struct kan_resource_render_pass_variant_t *source = source_void;

    kan_dynamic_array_init_move (&target->sources, &source->sources);
    target->source_pass = source->source_pass;
    target->source_variant_index = source->source_variant_index;
}

KAN_REFLECTION_STRUCT_META (kan_resource_render_pass_variant_t)
RESOURCE_MATERIAL_API struct kan_resource_byproduct_type_meta_t kan_resource_render_pass_variant_byproduct_type_meta = {
    .hash = NULL,
    .is_equal = NULL,
    .move = kan_resource_render_pass_variant_move,
    .reset = NULL,
};

static enum kan_resource_compile_result_t kan_resource_render_pass_variant_compile (
    struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (kan_resource_render_pass_variant_t)
RESOURCE_MATERIAL_API struct kan_resource_compilable_meta_t kan_resource_render_pass_variant_compilable_meta = {
    .output_type_name = "kan_resource_render_pass_variant_compiled_t",
    .configuration_type_name = NULL,
    // No state as render pipeline language does not support step by step parsing right now.
    .state_type_name = NULL,
    .functor = kan_resource_render_pass_variant_compile,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_render_pass_variant_t, sources)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_render_pass_variant_source_reference_meta = {
    .type = "kan_resource_rpl_source_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED,
};

KAN_REFLECTION_STRUCT_META (kan_resource_render_pass_variant_compiled_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t
    kan_resource_render_pass_variant_compiled_resource_type_meta = {
        .root = KAN_FALSE,
};

static enum kan_resource_compile_result_t kan_resource_render_pass_compile (struct kan_resource_compile_state_t *state)
{
    const struct kan_resource_render_pass_t *input = state->input_instance;
    struct kan_resource_render_pass_compiled_t *output = state->output_instance;
    const struct kan_resource_material_platform_configuration_t *configuration = state->platform_configuration;
    KAN_ASSERT (configuration)

    output->supported = kan_resource_material_platform_configuration_is_pass_supported (configuration, input);
    if (!output->supported)
    {
        // Pass is not supported, but this is okay. It will just be left empty in resources.
        return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
    }

    output->type = input->type;
    kan_dynamic_array_set_capacity (&output->attachments, input->attachments.size);
    output->attachments.size = output->attachments.capacity;
    memcpy (output->attachments.data, input->attachments.data,
            sizeof (struct kan_render_pass_attachment_t) * input->attachments.size);

    kan_interned_string_t interned_kan_resource_rpl_source_t = kan_string_intern ("kan_resource_rpl_source_t");
    kan_interned_string_t interned_kan_resource_render_pass_variant_t =
        kan_string_intern ("kan_resource_render_pass_variant_t");

    kan_dynamic_array_set_capacity (&output->variants, input->variants.size);
    kan_bool_t successful = KAN_TRUE;

    struct kan_resource_render_pass_variant_t variant_byproduct;
    kan_resource_render_pass_variant_init (&variant_byproduct);

    for (kan_loop_size_t variant_index = 0u; variant_index < input->variants.size && successful; ++variant_index)
    {
        const struct kan_resource_render_pass_variant_description_t *variant_description =
            &((struct kan_resource_render_pass_variant_description_t *) input->variants.data)[variant_index];

        kan_dynamic_array_set_capacity (&variant_byproduct.sources, variant_description->sources.size);
        variant_byproduct.source_pass = state->name;
        variant_byproduct.source_variant_index = (kan_instance_size_t) variant_index;
        // All options in variant must be instance options, therefore there is no sense to add them into byproduct:
        // they cannot technically affect the layout.

        for (kan_loop_size_t source_index = 0u; source_index < variant_description->sources.size && successful;
             ++source_index)
        {
            const kan_interned_string_t source_name =
                ((kan_interned_string_t *) variant_description->sources.data)[source_index];
            struct kan_resource_rpl_source_t code_source_byproduct;
            code_source_byproduct.source = source_name;

            kan_interned_string_t source_registered_name = state->register_byproduct (
                state->interface_user_data, interned_kan_resource_rpl_source_t, &code_source_byproduct);

            if (!source_registered_name)
            {
                KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                         "Failed to register source byproduct for pass \"%s\" for source \"%s\".", state->name,
                         source_name)
                successful = KAN_FALSE;
                break;
            }

            *(kan_interned_string_t *) kan_dynamic_array_add_last (&variant_byproduct.sources) = source_registered_name;
        }

        if (!successful)
        {
            break;
        }

        // No need to sort sources as we're using unique byproducts for pass variants anyway.
        char name_buffer[KAN_RESOURCE_MATERIAL_PASS_VARIANT_MAX_NAME_LENGTH];
        snprintf (name_buffer, KAN_RESOURCE_MATERIAL_PASS_VARIANT_MAX_NAME_LENGTH, "%s_variant_%lu", state->name,
                  (unsigned long) variant_index);

        kan_interned_string_t registered_variant_name =
            state->register_unique_byproduct (state->interface_user_data, interned_kan_resource_render_pass_variant_t,
                                              kan_string_intern (name_buffer), &variant_byproduct);

        if (!registered_variant_name)
        {
            KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                     "Failed to register variant byproduct for pass \"%s\" for variant %lu.", state->name,
                     (unsigned long) variant_index)
            successful = KAN_FALSE;
            break;
        }

        *(kan_interned_string_t *) kan_dynamic_array_add_last (&output->variants) = registered_variant_name;
    }

    kan_resource_render_pass_variant_shutdown (&variant_byproduct);
    return successful ? KAN_RESOURCE_PIPELINE_COMPILE_FINISHED : KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
}

static enum kan_resource_compile_result_t kan_resource_render_pass_variant_compile (
    struct kan_resource_compile_state_t *state)
{
    const struct kan_resource_render_pass_variant_t *input = state->input_instance;
    struct kan_resource_render_pass_variant_compiled_t *output = state->output_instance;
    const kan_interned_string_t interned_kan_resource_rpl_source_compiled_t =
        kan_string_intern ("kan_resource_rpl_source_compiled_t");

    kan_rpl_compiler_context_t compiler_context =
        // Currently, all materials and passes use graphics pipelines.
        kan_rpl_compiler_context_create (KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC, state->name);

    for (kan_loop_size_t index = 0u; index < state->dependencies_count; ++index)
    {
        if (state->dependencies[index].type == interned_kan_resource_rpl_source_compiled_t)
        {
            const struct kan_resource_rpl_source_compiled_t *source = state->dependencies[index].data;
            if (!kan_rpl_compiler_context_use_module (compiler_context, &source->intermediate))
            {
                kan_rpl_compiler_context_destroy (compiler_context);
                KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                         "Failed to compile pass \"%s\" variant %lu set layout: failed to use module.",
                         input->source_pass, (unsigned long) input->source_variant_index)
                return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
            }
        }
        else
        {
            // Unexpected dependency.
            KAN_ASSERT (KAN_FALSE)
        }
    }

    // We don't need entry points as we only need meta to be resolved.
    kan_rpl_compiler_instance_t compiler_instance = kan_rpl_compiler_context_resolve (compiler_context, 0u, NULL);

    if (!KAN_HANDLE_IS_VALID (compiler_instance))
    {
        kan_rpl_compiler_context_destroy (compiler_context);
        KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                 "Failed to compile pass \"%s\" variant %lu set layout: failed compilation resolve.",
                 input->source_pass, (unsigned long) input->source_variant_index)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    struct kan_rpl_meta_t meta;
    kan_rpl_meta_init (&meta);
    const kan_bool_t emit_result =
        kan_rpl_compiler_instance_emit_meta (compiler_instance, &meta, KAN_RPL_META_EMISSION_FULL);

    kan_rpl_compiler_instance_destroy (compiler_instance);
    kan_rpl_compiler_context_destroy (compiler_context);

    if (!emit_result)
    {
        kan_rpl_meta_shutdown (&meta);
        KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                 "Failed to compile pass \"%s\" variant %lu set layout: failed to emit meta.", input->source_pass,
                 (unsigned long) input->source_variant_index)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    if (meta.attribute_sources.size > 0u)
    {
        kan_rpl_meta_shutdown (&meta);
        KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                 "Failed to compile pass \"%s\" variant %lu set layout: source contains attribute sources, which is "
                 "forbidden for pass set layout sources.",
                 input->source_pass, (unsigned long) input->source_variant_index)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    if (meta.set_material.buffers.size || meta.set_material.samplers.size > 0u || meta.set_object.buffers.size ||
        meta.set_object.samplers.size > 0u || meta.set_shared.buffers.size || meta.set_shared.samplers.size > 0u)
    {
        kan_rpl_meta_shutdown (&meta);
        KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                 "Failed to compile pass \"%s\" variant %lu set layout: source contains data for non-pass sets, "
                 "which is forbidden for pass set layout sources.",
                 input->source_pass, (unsigned long) input->source_variant_index)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    // Color outputs are explicitly allowed as their structure usually depends on pass.

    kan_rpl_meta_set_bindings_shutdown (&output->pass_set_bindings);
    kan_rpl_meta_set_bindings_init_copy (&output->pass_set_bindings, &meta.set_pass);
    kan_rpl_meta_shutdown (&meta);
    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

void kan_resource_render_pass_variant_description_init (struct kan_resource_render_pass_variant_description_t *instance)
{
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_resource_rpl_options_init (&instance->instance_options);
    kan_dynamic_array_init (&instance->disabled_stages, 0u, sizeof (enum kan_rpl_pipeline_stage_t),
                            _Alignof (enum kan_rpl_pipeline_stage_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->required_tags, 0u, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), kan_allocation_group_stack_get ());
}

void kan_resource_render_pass_variant_description_shutdown (
    struct kan_resource_render_pass_variant_description_t *instance)
{
    kan_dynamic_array_shutdown (&instance->sources);
    kan_resource_rpl_options_shutdown (&instance->instance_options);
    kan_dynamic_array_shutdown (&instance->disabled_stages);
    kan_dynamic_array_shutdown (&instance->required_tags);
}

void kan_resource_render_pass_init (struct kan_resource_render_pass_t *instance)
{
    instance->type = KAN_RENDER_PASS_GRAPHICS;
    kan_dynamic_array_init (&instance->required_tags, 0u, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->attachments, 0u, sizeof (struct kan_render_pass_attachment_t),
                            _Alignof (struct kan_render_pass_attachment_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->variants, 0u, sizeof (struct kan_resource_render_pass_variant_description_t),
                            _Alignof (struct kan_resource_render_pass_variant_description_t),
                            kan_allocation_group_stack_get ());
}

void kan_resource_render_pass_shutdown (struct kan_resource_render_pass_t *instance)
{
    for (kan_loop_size_t index = 0u; index < instance->variants.size; ++index)
    {
        kan_resource_render_pass_variant_description_shutdown (
            &((struct kan_resource_render_pass_variant_description_t *) instance->variants.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->required_tags);
    kan_dynamic_array_shutdown (&instance->attachments);
    kan_dynamic_array_shutdown (&instance->variants);
}

void kan_resource_render_pass_variant_compiled_init (struct kan_resource_render_pass_variant_compiled_t *instance)
{
    kan_rpl_meta_set_bindings_init (&instance->pass_set_bindings);
}

void kan_resource_render_pass_variant_compiled_shutdown (struct kan_resource_render_pass_variant_compiled_t *instance)
{
    kan_rpl_meta_set_bindings_shutdown (&instance->pass_set_bindings);
}

void kan_resource_render_pass_compiled_init (struct kan_resource_render_pass_compiled_t *instance)
{
    instance->supported = KAN_FALSE;
    instance->type = KAN_RENDER_PASS_GRAPHICS;
    kan_dynamic_array_init (&instance->attachments, 0u, sizeof (struct kan_render_pass_attachment_t),
                            _Alignof (struct kan_render_pass_attachment_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->variants, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
}

void kan_resource_render_pass_compiled_shutdown (struct kan_resource_render_pass_compiled_t *instance)
{
    kan_dynamic_array_shutdown (&instance->attachments);
    kan_dynamic_array_shutdown (&instance->variants);
}
