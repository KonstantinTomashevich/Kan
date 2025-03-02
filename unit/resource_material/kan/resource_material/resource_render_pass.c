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

KAN_REFLECTION_STRUCT_META (kan_resource_render_pass_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t kan_resource_render_pass_resource_type_meta = {
    .root = KAN_TRUE,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_render_pass_t, pass_set_source)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_render_pass_pass_set_source_reference_meta = {
    .type = NULL, // Null means third party.
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW,
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

    if (input->pass_set_source)
    {
        // Technically, we should compile pass set source through source byproduct (the same as for materials).
        // But it would result in separation of pass data into two resources: pass resource and pass set resources.
        // And it would add unnecessary complications into pass loading routine.
        // But pass set sources are usually quite small, therefore it should be okay to compile them directly here.

        KAN_ASSERT (state->dependencies_count == 1u)
        KAN_ASSERT (state->dependencies->type == NULL)

        if (state->dependencies->data_size_if_third_party == 0u)
        {
            KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                     "Failed to compile pass \"%s\" set layout as source \"%s\" loaded data is empty.", state->name,
                     state->dependencies->name)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }

        kan_allocation_group_t temporary_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "pass_set_source_compilation");

        // The reason for allocation is the same as for material shader source.
        char *string_to_parse = kan_allocate_general (
            temporary_allocation_group, state->dependencies->data_size_if_third_party + 1u, _Alignof (char));
        memcpy (string_to_parse, state->dependencies->data, state->dependencies->data_size_if_third_party);
        string_to_parse[state->dependencies->data_size_if_third_party] = '\0';

        kan_rpl_parser_t parser = kan_rpl_parser_create (state->name);
        if (!kan_rpl_parser_add_source (parser, string_to_parse, input->pass_set_source))
        {
            kan_free_general (temporary_allocation_group, string_to_parse,
                              state->dependencies->data_size_if_third_party + 1u);
            kan_rpl_parser_destroy (parser);

            KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                     "Failed to compile pass \"%s\" set layout as source \"%s\" cannot be properly parsed.",
                     state->name, state->dependencies->name)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }

        kan_free_general (temporary_allocation_group, string_to_parse,
                          state->dependencies->data_size_if_third_party + 1u);
        struct kan_rpl_intermediate_t pass_set_intermediate;
        kan_rpl_intermediate_init (&pass_set_intermediate);

        if (!kan_rpl_parser_build_intermediate (parser, &pass_set_intermediate))
        {
            kan_rpl_parser_destroy (parser);
            KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                     "Failed to compile pass \"%s\" set layout as source \"%s\" export to intermediate format failed.",
                     state->name, state->dependencies->name)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }

        kan_rpl_parser_destroy (parser);
        if (pass_set_intermediate.options.size > 0u)
        {
            KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                     "Failed to compile pass \"%s\" set layout as source \"%s\" contains options which is not allowed "
                     "for pass set layout sources.",
                     state->name, state->dependencies->name)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }

        kan_rpl_compiler_context_t compiler_context =
            // Currently, all materials and passes use graphics pipelines.
            kan_rpl_compiler_context_create (KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC, state->name);

        if (!kan_rpl_compiler_context_use_module (compiler_context, &pass_set_intermediate))
        {
            kan_rpl_intermediate_shutdown (&pass_set_intermediate);
            kan_rpl_compiler_context_destroy (compiler_context);
            KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                     "Failed to compile pass \"%s\" set layout: failed to use module.", state->name)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }

        // We don't need entry points as we only need meta to be resolved.
        kan_rpl_compiler_instance_t compiler_instance = kan_rpl_compiler_context_resolve (compiler_context, 0u, NULL);

        if (!KAN_HANDLE_IS_VALID (compiler_instance))
        {
            kan_rpl_intermediate_shutdown (&pass_set_intermediate);
            kan_rpl_compiler_context_destroy (compiler_context);
            KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                     "Failed to compile pass \"%s\" set layout: failed compilation resolve.", state->name)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }

        struct kan_rpl_meta_t meta;
        kan_rpl_meta_init (&meta);
        const kan_bool_t emit_result =
            kan_rpl_compiler_instance_emit_meta (compiler_instance, &meta, KAN_RPL_META_EMISSION_FULL);

        kan_rpl_intermediate_shutdown (&pass_set_intermediate);
        kan_rpl_compiler_instance_destroy (compiler_instance);
        kan_rpl_compiler_context_destroy (compiler_context);

        if (!emit_result)
        {
            kan_rpl_meta_shutdown (&meta);
            KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                     "Failed to compile pass \"%s\" set layout: failed to emit meta.", state->name)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }

        if (meta.attribute_buffers.size > 0u)
        {
            kan_rpl_meta_shutdown (&meta);
            KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                     "Failed to compile pass \"%s\" set layout: source contains attribute buffers, which is forbidden "
                     "for pass set layout sources.",
                     state->name)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }

        if (meta.set_material.buffers.size || meta.set_material.samplers.size > 0u || meta.set_object.buffers.size ||
            meta.set_object.samplers.size > 0u || meta.set_unstable.buffers.size ||
            meta.set_unstable.samplers.size > 0u)
        {
            kan_rpl_meta_shutdown (&meta);
            KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                     "Failed to compile pass \"%s\" set layout: source contains data for non-pass sets, which is "
                     "forbidden for pass set layout sources.",
                     state->name)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }

        if (meta.color_outputs.size > 0u)
        {
            kan_rpl_meta_shutdown (&meta);
            KAN_LOG (resource_pass_compilation, KAN_LOG_ERROR,
                     "Failed to compile pass \"%s\" set layout: source contains color outputs, which is forbidden for "
                     "pass set layout sources.",
                     state->name)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }

        kan_rpl_meta_set_bindings_shutdown (&output->pass_set_bindings);
        kan_rpl_meta_set_bindings_init_copy (&output->pass_set_bindings, &meta.set_pass);
        kan_rpl_meta_shutdown (&meta);
    }

    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

void kan_resource_render_pass_init (struct kan_resource_render_pass_t *instance)
{
    instance->type = KAN_RENDER_PASS_GRAPHICS;
    kan_dynamic_array_init (&instance->required_tags, 0u, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), kan_allocation_group_stack_get ());
    instance->pass_set_source = NULL;
    kan_dynamic_array_init (&instance->attachments, 0u, sizeof (struct kan_render_pass_attachment_t),
                            _Alignof (struct kan_render_pass_attachment_t), kan_allocation_group_stack_get ());
}

void kan_resource_render_pass_shutdown (struct kan_resource_render_pass_t *instance)
{
    kan_dynamic_array_shutdown (&instance->required_tags);
    kan_dynamic_array_shutdown (&instance->attachments);
}

void kan_resource_render_pass_compiled_init (struct kan_resource_render_pass_compiled_t *instance)
{
    instance->supported = KAN_FALSE;
    instance->type = KAN_RENDER_PASS_GRAPHICS;
    kan_rpl_meta_set_bindings_init (&instance->pass_set_bindings);
    kan_dynamic_array_init (&instance->attachments, 0u, sizeof (struct kan_render_pass_attachment_t),
                            _Alignof (struct kan_render_pass_attachment_t), kan_allocation_group_stack_get ());
}

void kan_resource_render_pass_compiled_shutdown (struct kan_resource_render_pass_compiled_t *instance)
{
    kan_rpl_meta_set_bindings_shutdown (&instance->pass_set_bindings);
    kan_dynamic_array_shutdown (&instance->attachments);
}
