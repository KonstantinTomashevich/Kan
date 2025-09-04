#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <string.h>

#include <kan/log/logging.h>
#include <kan/render_foundation/resource_render_pass_build.h>
#include <kan/resource_pipeline/meta.h>

KAN_LOG_DEFINE_CATEGORY (resource_render_foundation_pass);
KAN_USE_STATIC_INTERNED_IDS

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_render_pass_variant_header_t, sources)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_reference_meta_t
    kan_resource_render_pass_variant_header_reference_sources = {
        .type_name = "kan_resource_rpl_source_t",
        .flags = KAN_RESOURCE_REFERENCE_META_NULLABLE,
};

void kan_resource_render_pass_variant_header_init (struct kan_resource_render_pass_variant_header_t *instance)
{
    instance->name = NULL;
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_resource_rpl_options_init (&instance->instance_options);
    kan_dynamic_array_init (&instance->disabled_stages, 0u, sizeof (enum kan_rpl_pipeline_stage_t),
                            alignof (enum kan_rpl_pipeline_stage_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->required_tags, 0u, sizeof (kan_interned_string_t),
                            alignof (kan_interned_string_t), kan_allocation_group_stack_get ());
}

void kan_resource_render_pass_variant_header_shutdown (struct kan_resource_render_pass_variant_header_t *instance)
{
    kan_dynamic_array_shutdown (&instance->sources);
    kan_resource_rpl_options_shutdown (&instance->instance_options);
    kan_dynamic_array_shutdown (&instance->disabled_stages);
    kan_dynamic_array_shutdown (&instance->required_tags);
}

KAN_REFLECTION_STRUCT_META (kan_resource_render_pass_header_t)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_type_meta_t kan_resource_render_pass_header_resource_type = {
    .flags = KAN_RESOURCE_TYPE_ROOT,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

void kan_resource_render_pass_header_init (struct kan_resource_render_pass_header_t *instance)
{
    instance->type = KAN_RENDER_PASS_GRAPHICS;
    kan_dynamic_array_init (&instance->required_tags, 0u, sizeof (kan_interned_string_t),
                            alignof (kan_interned_string_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->attachments, 0u, sizeof (struct kan_render_pass_attachment_t),
                            alignof (struct kan_render_pass_attachment_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->variants, 0u, sizeof (struct kan_resource_render_pass_variant_header_t),
                            alignof (struct kan_resource_render_pass_variant_header_t),
                            kan_allocation_group_stack_get ());
}

void kan_resource_render_pass_header_shutdown (struct kan_resource_render_pass_header_t *instance)
{
    kan_dynamic_array_shutdown (&instance->required_tags);
    kan_dynamic_array_shutdown (&instance->attachments);
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->variants, kan_resource_render_pass_variant_header)
}

static enum kan_resource_build_rule_result_t pass_build (struct kan_resource_build_rule_context_t *context);

KAN_REFLECTION_STRUCT_META (kan_resource_render_pass_t)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_build_rule_t kan_resource_rpl_source_build_rule = {
    .primary_input_type = "kan_resource_render_pass_header_t",
    .platform_configuration_type = "kan_resource_render_code_platform_configuration_t",
    .secondary_types_count = 1u,
    .secondary_types = (const char *[]) {"kan_resource_rpl_source_t"},
    .functor = pass_build,
    .version = CUSHION_START_NS_X64,
};

static enum kan_resource_build_rule_result_t pass_build (struct kan_resource_build_rule_context_t *context)
{
    kan_static_interned_ids_ensure_initialized ();
    const struct kan_resource_render_pass_header_t *input = context->primary_input;
    struct kan_resource_render_pass_t *output = context->primary_output;
    const struct kan_resource_render_code_platform_configuration_t *configuration = context->platform_configuration;

    if (!kan_resource_render_code_platform_configuration_is_pass_supported (configuration, input))
    {
        return KAN_RESOURCE_BUILD_RULE_UNSUPPORTED;
    }

    for (kan_loop_size_t required_index = 0u; required_index < input->required_tags.size; ++required_index)
    {
        bool is_supported = false;
        kan_interned_string_t required = ((kan_interned_string_t *) input->required_tags.data)[required_index];

        for (kan_loop_size_t supported_index = 0u; supported_index < configuration->supported_pass_tags.size;
             ++supported_index)
        {
            kan_interned_string_t supported =
                ((kan_interned_string_t *) configuration->supported_pass_tags.data)[supported_index];

            if (supported == required)
            {
                is_supported = true;
                break;
            }
        }

        if (!is_supported)
        {
            return KAN_RESOURCE_BUILD_RULE_UNSUPPORTED;
        }
    }

    output->type = input->type;
    kan_dynamic_array_set_capacity (&output->attachments, input->attachments.size);
    output->attachments.size = input->attachments.size;

    memcpy (output->attachments.data, input->attachments.data,
            sizeof (struct kan_render_pass_attachment_t) * output->attachments.size);
    kan_dynamic_array_set_capacity (&output->variants, input->variants.size);
    bool successful = true;

    for (kan_loop_size_t variant_index = 0u; variant_index < input->variants.size; ++variant_index)
    {
        const struct kan_resource_render_pass_variant_header_t *source =
            &((struct kan_resource_render_pass_variant_header_t *) input->variants.data)[variant_index];

        struct kan_resource_render_pass_variant_t *target = kan_dynamic_array_add_last (&output->variants);
        kan_allocation_group_stack_push (output->variants.allocation_group);
        kan_resource_render_pass_variant_init (target);
        kan_allocation_group_stack_pop ();

        target->name = source->name;
        char log_name_buffer[256u];
        snprintf (log_name_buffer, sizeof (log_name_buffer), "%s_variant_%s", context->primary_name, target->name);
        const kan_interned_string_t log_name = kan_string_intern (log_name_buffer);

        kan_rpl_compiler_context_t compiler_context =
            // Currently, all materials and passes use graphics pipelines.
            kan_rpl_compiler_context_create (KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC, log_name);

        CUSHION_DEFER { kan_rpl_compiler_context_destroy (compiler_context); }
        bool modules_used = true;

        for (kan_loop_size_t source_index = 0u; source_index < source->sources.size; ++source_index)
        {
            kan_interned_string_t source_name = ((kan_interned_string_t *) source->sources.data)[source_index];
            struct kan_resource_build_rule_secondary_node_t *secondary_node = context->secondary_input_first;

            while (secondary_node)
            {
                if (secondary_node->type == KAN_STATIC_INTERNED_ID_GET (kan_resource_rpl_source_t) &&
                    secondary_node->name == source_name)
                {
                    break;
                }

                secondary_node = secondary_node->next;
            }

            KAN_ASSERT (secondary_node)
            const struct kan_resource_rpl_source_t *source_data = secondary_node->data;

            if (!kan_rpl_compiler_context_use_module (compiler_context, &source_data->intermediate))
            {
                KAN_LOG (resource_render_foundation_pass, KAN_LOG_ERROR,
                         "Failed to use module \"%s\" while compiling variant \"%s\" of pass \"%s\".", source_name,
                         source->name, context->primary_name)
                modules_used = false;
            }
        }

        if (!modules_used)
        {
            successful = false;
            continue;
        }

        // We do not apply pass instance options as they should not affect meta anyway.
        // Also, we don't need entry points as we only need meta to be resolved.
        kan_rpl_compiler_instance_t compiler_instance = kan_rpl_compiler_context_resolve (compiler_context, 0u, NULL);

        if (!KAN_HANDLE_IS_VALID (compiler_instance))
        {
            KAN_LOG (resource_render_foundation_pass, KAN_LOG_ERROR,
                     "Failed to compile variant \"%s\" of pass \"%s\" for meta generation.", source->name,
                     context->primary_name)
            successful = false;
            continue;
        }

        CUSHION_DEFER { kan_rpl_compiler_instance_destroy (compiler_instance); }
        struct kan_rpl_meta_t meta;
        kan_rpl_meta_init (&meta);
        CUSHION_DEFER { kan_rpl_meta_shutdown (&meta); }

        // Emit full meta as we'd like to validate that variant source do not add anything unexpected.
        const bool emit_result =
            kan_rpl_compiler_instance_emit_meta (compiler_instance, &meta, KAN_RPL_META_EMISSION_FULL);

        if (!emit_result)
        {
            KAN_LOG (resource_render_foundation_pass, KAN_LOG_ERROR,
                     "Failed to emit meta for variant \"%s\" of pass \"%s\" for meta generation.", source->name,
                     context->primary_name)
            successful = false;
            continue;
        }

        if (meta.attribute_sources.size > 0u)
        {
            KAN_LOG (resource_render_foundation_pass, KAN_LOG_ERROR,
                     "Failed to build variant \"%s\" of pass \"%s\": source contains attribute sources, which is "
                     "forbidden for pass set layout sources.",
                     source->name, context->primary_name)
            successful = false;
            continue;
        }

        if (meta.set_material.buffers.size || meta.set_material.samplers.size > 0u || meta.set_object.buffers.size ||
            meta.set_object.samplers.size > 0u || meta.set_shared.buffers.size || meta.set_shared.samplers.size > 0u)
        {
            KAN_LOG (resource_render_foundation_pass, KAN_LOG_ERROR,
                     "Failed to build variant \"%s\" of pass \"%s\": source contains data for non-pass sets, which is "
                     "forbidden for pass set layout sources.",
                     source->name, context->primary_name)
            successful = false;
            continue;
        }

        // Color outputs are explicitly allowed as their structure usually depends on pass.

        kan_rpl_meta_set_bindings_shutdown (&target->pass_set_bindings);
        kan_rpl_meta_set_bindings_init_copy (&target->pass_set_bindings, &meta.set_pass);
    }

    return successful ? KAN_RESOURCE_BUILD_RULE_SUCCESS : KAN_RESOURCE_BUILD_RULE_FAILURE;
}
