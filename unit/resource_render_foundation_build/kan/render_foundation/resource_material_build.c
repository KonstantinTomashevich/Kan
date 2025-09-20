#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <string.h>

#include <kan/log/logging.h>
#include <kan/render_foundation/resource_material_build.h>
#include <kan/render_foundation/resource_render_pass_build.h>
#include <kan/resource_pipeline/meta.h>

KAN_LOG_DEFINE_CATEGORY (resource_render_foundation_material);
KAN_USE_STATIC_INTERNED_IDS

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_pass_header_t, name)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_reference_meta_t
    kan_resource_material_pass_header_reference_name = {
        .type_name = "kan_resource_render_pass_header_t",
        .flags = 0u,
};

void kan_resource_material_pass_header_init (struct kan_resource_material_pass_header_t *instance)
{
    instance->name = NULL;
    kan_dynamic_array_init (&instance->entry_points, 0u, sizeof (struct kan_rpl_entry_point_t),
                            alignof (struct kan_rpl_entry_point_t), kan_allocation_group_stack_get ());
    kan_resource_rpl_options_init (&instance->options);
    kan_dynamic_array_init (&instance->tags, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
}

void kan_resource_material_pass_header_shutdown (struct kan_resource_material_pass_header_t *instance)
{
    kan_dynamic_array_shutdown (&instance->entry_points);
    kan_resource_rpl_options_shutdown (&instance->options);
    kan_dynamic_array_shutdown (&instance->tags);
}

KAN_REFLECTION_STRUCT_META (kan_resource_material_header_t)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_type_meta_t kan_resource_material_header_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_header_t, sources)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_reference_meta_t
    kan_resource_material_header_reference_sources = {
        .type_name = "kan_resource_rpl_source_t",
        .flags = 0u,
};

void kan_resource_material_header_init (struct kan_resource_material_header_t *instance)
{
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_resource_rpl_options_init (&instance->global_options);
    kan_dynamic_array_init (&instance->passes, 0u, sizeof (struct kan_resource_material_pass_header_t),
                            alignof (struct kan_resource_material_pass_header_t), kan_allocation_group_stack_get ());
}

void kan_resource_material_header_shutdown (struct kan_resource_material_header_t *instance)
{
    kan_dynamic_array_shutdown (&instance->sources);
    kan_resource_rpl_options_shutdown (&instance->global_options);
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->passes, kan_resource_material_pass_header)
}

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_pipeline_transient_t, pipeline_name)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_reference_meta_t
    kan_resource_material_pipeline_transient_reference_pipeline_name = {
        .type_name = "kan_resource_rpl_pipeline_t",
        .flags = 0u,
};

KAN_REFLECTION_STRUCT_META (kan_resource_material_transient_t)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_type_meta_t kan_resource_material_transient_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_transient_t, sources)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_reference_meta_t
    kan_resource_material_transient_reference_sources = {
        .type_name = "kan_resource_rpl_source_t",
        .flags = 0u,
};

void kan_resource_material_transient_init (struct kan_resource_material_transient_t *instance)
{
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_resource_rpl_options_init (&instance->global_options);
    kan_dynamic_array_init (&instance->pipelines, 0u, sizeof (struct kan_resource_material_pipeline_transient_t),
                            alignof (struct kan_resource_material_pipeline_transient_t),
                            kan_allocation_group_stack_get ());
}

void kan_resource_material_transient_shutdown (struct kan_resource_material_transient_t *instance)
{
    kan_dynamic_array_shutdown (&instance->sources);
    kan_resource_rpl_options_shutdown (&instance->global_options);
    kan_dynamic_array_shutdown (&instance->pipelines);
}

static enum kan_resource_build_rule_result_t material_transient_build (
    struct kan_resource_build_rule_context_t *context);

KAN_REFLECTION_STRUCT_META (kan_resource_material_transient_t)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_build_rule_t kan_resource_material_transient_build_rule = {
    .primary_input_type = "kan_resource_material_header_t",
    .platform_configuration_type = "kan_resource_render_code_platform_configuration_t",
    .secondary_types_count = 1u,
    .secondary_types = (const char *[]) {"kan_resource_render_pass_header_t"},
    .functor = material_transient_build,
    .version = CUSHION_START_NS_X64,
};

bool is_pass_variant_supported (const struct kan_resource_render_pass_variant_header_t *pass_variant,
                                const struct kan_resource_material_pass_header_t *material_pass)
{
    for (kan_loop_size_t required_index = 0u; required_index < pass_variant->required_tags.size; ++required_index)
    {
        bool is_supported = false;
        kan_interned_string_t required = ((kan_interned_string_t *) pass_variant->required_tags.data)[required_index];

        for (kan_loop_size_t supported_index = 0u; supported_index < material_pass->tags.size; ++supported_index)
        {
            kan_interned_string_t supported = ((kan_interned_string_t *) material_pass->tags.data)[supported_index];

            if (supported == required)
            {
                is_supported = true;
                break;
            }
        }

        if (!is_supported)
        {
            return false;
        }
    }

    return true;
}

static enum kan_resource_build_rule_result_t material_transient_build (
    struct kan_resource_build_rule_context_t *context)
{
    kan_static_interned_ids_ensure_initialized ();
    const struct kan_resource_material_header_t *input = context->primary_input;
    struct kan_resource_material_transient_t *output = context->primary_output;
    const struct kan_resource_render_code_platform_configuration_t *configuration = context->platform_configuration;

    kan_dynamic_array_set_capacity (&output->sources, input->sources.size);
    output->sources.size = input->sources.size;
    memcpy (output->sources.data, input->sources.data, sizeof (kan_interned_string_t) * input->sources.size);

    kan_resource_rpl_options_shutdown (&output->global_options);
    kan_resource_rpl_options_init_copy (&output->global_options, &input->global_options);
    // Reserve allocation for at least 1 pipeline per pass.
    kan_dynamic_array_set_capacity (&output->pipelines, input->passes.size);

    struct kan_resource_rpl_pipeline_header_t pipeline_header;
    kan_resource_rpl_pipeline_header_init (&pipeline_header);
    CUSHION_DEFER { kan_resource_rpl_pipeline_header_shutdown (&pipeline_header); }

    bool successful = true;
    char name_buffer[KAN_RESOURCE_RF_PIPELINE_MAX_NAME_LENGTH];

    for (kan_loop_size_t pass_index = 0u; pass_index < input->passes.size; ++pass_index)
    {
        const struct kan_resource_material_pass_header_t *pass_header =
            &((struct kan_resource_material_pass_header_t *) input->passes.data)[pass_index];

        const struct kan_resource_render_pass_header_t *pass = NULL;
        struct kan_resource_build_rule_secondary_node_t *secondary = context->secondary_input_first;

        while (secondary)
        {
            if (secondary->name == pass_header->name)
            {
                pass = secondary->data;
                break;
            }

            secondary = secondary->next;
        }

        KAN_ASSERT (pass)
        if (!kan_resource_render_code_platform_configuration_is_pass_supported (configuration, pass))
        {
            continue;
        }

        for (kan_loop_size_t variant_index = 0u; variant_index < pass->variants.size; ++variant_index)
        {
            const struct kan_resource_render_pass_variant_header_t *variant_header =
                &((struct kan_resource_render_pass_variant_header_t *) pass->variants.data)[variant_index];

            if (!is_pass_variant_supported (variant_header, pass_header))
            {
                continue;
            }

            pipeline_header.type = KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC;
            kan_dynamic_array_set_capacity (&pipeline_header.entry_points, pass_header->entry_points.size);

            for (kan_loop_size_t point_index = 0u; point_index < pass_header->entry_points.size; ++point_index)
            {
                const struct kan_rpl_entry_point_t *point =
                    &((struct kan_rpl_entry_point_t *) pass_header->entry_points.data)[point_index];
                bool disabled = false;

                for (kan_loop_size_t stage_index = 0u; stage_index < variant_header->disabled_stages.size;
                     ++stage_index)
                {
                    enum kan_rpl_pipeline_stage_t disabled_stage =
                        ((enum kan_rpl_pipeline_stage_t *) variant_header->disabled_stages.data)[stage_index];

                    if (disabled_stage == point->stage)
                    {
                        disabled = true;
                        break;
                    }
                }

                if (!disabled)
                {
                    *(struct kan_rpl_entry_point_t *) kan_dynamic_array_add_last (&pipeline_header.entry_points) =
                        *point;
                }
            }

            kan_dynamic_array_set_capacity (&pipeline_header.sources,
                                            variant_header->sources.size + input->sources.size);
            pipeline_header.sources.size = pipeline_header.sources.capacity;

            if (variant_header->sources.size > 0u)
            {
                memcpy (pipeline_header.sources.data, variant_header->sources.data,
                        sizeof (kan_interned_string_t) * variant_header->sources.size);
            }

            if (input->sources.size > 0u)
            {
                memcpy (pipeline_header.sources.data + sizeof (kan_interned_string_t) * variant_header->sources.size,
                        input->sources.data, sizeof (kan_interned_string_t) * input->sources.size);
            }

            kan_resource_rpl_options_shutdown (&pipeline_header.global_options);
            kan_resource_rpl_options_init_copy (&pipeline_header.global_options, &input->global_options);

            kan_resource_rpl_options_shutdown (&pipeline_header.instance_options);
            kan_resource_rpl_options_init_copy (&pipeline_header.instance_options, &variant_header->instance_options);
            kan_resource_rpl_options_append (&pipeline_header.instance_options, &pass_header->options);

            struct kan_resource_material_pipeline_transient_t *transient =
                kan_dynamic_array_add_last (&output->pipelines);

            if (!transient)
            {
                kan_dynamic_array_set_capacity (&output->pipelines, output->pipelines.size * 2u);
                transient = kan_dynamic_array_add_last (&output->pipelines);
            }

            transient->pass_name = pass_header->name;
            transient->variant_name = variant_header->name;

            snprintf (name_buffer, sizeof (name_buffer), "%s_pass_%s_variant_%s", context->primary_name,
                      pass_header->name, variant_header->name);
            transient->pipeline_name = kan_string_intern (name_buffer);

            if (!context->produce_secondary_output (context->interface,
                                                    KAN_STATIC_INTERNED_ID_GET (kan_resource_rpl_pipeline_header_t),
                                                    transient->pipeline_name, &pipeline_header))
            {
                KAN_LOG (resource_render_foundation_material, KAN_LOG_ERROR,
                         "Failed to produce pipeline \"%s\" for material \"%s\".", transient->pipeline_name,
                         context->primary_name)
                successful = false;
            }
        }

        if (pass->variants.size == 0u)
        {
            pipeline_header.type = KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC;
            kan_dynamic_array_set_capacity (&pipeline_header.entry_points, pass_header->entry_points.size);
            memcpy (pipeline_header.entry_points.data, pass_header->entry_points.data,
                    sizeof (struct kan_rpl_entry_point_t) * pass_header->entry_points.size);

            kan_dynamic_array_set_capacity (&pipeline_header.sources, input->sources.size);
            pipeline_header.sources.size = pipeline_header.sources.capacity;
            memcpy (pipeline_header.sources.data, input->sources.data,
                    sizeof (kan_interned_string_t) * input->sources.size);

            kan_resource_rpl_options_shutdown (&pipeline_header.global_options);
            kan_resource_rpl_options_init_copy (&pipeline_header.global_options, &input->global_options);

            kan_resource_rpl_options_shutdown (&pipeline_header.instance_options);
            kan_resource_rpl_options_init_copy (&pipeline_header.instance_options, &pass_header->options);

            struct kan_resource_material_pipeline_transient_t *transient =
                kan_dynamic_array_add_last (&output->pipelines);

            if (!transient)
            {
                kan_dynamic_array_set_capacity (&output->pipelines, output->pipelines.size * 2u);
                transient = kan_dynamic_array_add_last (&output->pipelines);
            }

            transient->pass_name = pass_header->name;
            transient->variant_name = NULL;

            snprintf (name_buffer, sizeof (name_buffer), "%s_pass_%s_default", context->primary_name,
                      pass_header->name);
            transient->pipeline_name = kan_string_intern (name_buffer);

            if (!context->produce_secondary_output (context->interface,
                                                    KAN_STATIC_INTERNED_ID_GET (kan_resource_rpl_pipeline_header_t),
                                                    transient->pipeline_name, &pipeline_header))
            {
                KAN_LOG (resource_render_foundation_material, KAN_LOG_ERROR,
                         "Failed to produce pipeline \"%s\" for material \"%s\".", transient->pipeline_name,
                         context->primary_name)
                successful = false;
            }
        }
    }

    return successful ? KAN_RESOURCE_BUILD_RULE_SUCCESS : KAN_RESOURCE_BUILD_RULE_FAILURE;
}

static enum kan_resource_build_rule_result_t material_build (struct kan_resource_build_rule_context_t *context);

KAN_REFLECTION_STRUCT_META (kan_resource_material_t)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_build_rule_t kan_resource_material_build_rule = {
    .primary_input_type = "kan_resource_material_transient_t",
    .platform_configuration_type = "kan_resource_render_code_platform_configuration_t",
    .secondary_types_count = 2u,
    .secondary_types = (const char *[]) {"kan_resource_rpl_pipeline_t", "kan_resource_rpl_source_t"},
    .functor = material_build,
    .version = CUSHION_START_NS_X64,
};

static enum kan_resource_build_rule_result_t material_build (struct kan_resource_build_rule_context_t *context)
{
    kan_static_interned_ids_ensure_initialized ();
    const struct kan_resource_material_transient_t *input = context->primary_input;
    struct kan_resource_material_t *output = context->primary_output;
    const struct kan_resource_render_code_platform_configuration_t *configuration = context->platform_configuration;

    kan_rpl_compiler_context_t compiler_context =
        kan_rpl_compiler_context_create (KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC, context->primary_name);
    CUSHION_DEFER { kan_rpl_compiler_context_destroy (compiler_context); }

    struct kan_resource_build_rule_secondary_node_t *secondary = context->secondary_input_first;
    bool modules_used = true;

    while (secondary)
    {
        if (secondary->type == KAN_STATIC_INTERNED_ID_GET (kan_resource_rpl_source_t))
        {
            for (kan_loop_size_t index = 0u; index < input->sources.size; ++index)
            {
                kan_interned_string_t source = ((kan_interned_string_t *) input->sources.data)[index];
                if (source == secondary->name)
                {
                    const struct kan_resource_rpl_source_t *data = secondary->data;
                    if (!kan_rpl_compiler_context_use_module (compiler_context, &data->intermediate))
                    {
                        KAN_LOG (resource_render_foundation_material, KAN_LOG_ERROR,
                                 "Failed to use source \"%s\" while trying to emit meta for material \"%s\".",
                                 secondary->name, context->primary_name)
                        modules_used = false;
                    }

                    break;
                }
            }
        }

        secondary = secondary->next;
    }

    if (!modules_used)
    {
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    if (!kan_resource_rpl_options_apply (&input->global_options, compiler_context, KAN_RPL_OPTION_TARGET_SCOPE_GLOBAL))
    {
        KAN_LOG (resource_render_foundation_material, KAN_LOG_ERROR,
                 "Failed to apply global options while trying to emit meta for material \"%s\".", context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    kan_rpl_compiler_instance_t compiler_instance = kan_rpl_compiler_context_resolve (compiler_context, 0u, NULL);
    if (!KAN_HANDLE_IS_VALID (compiler_instance))
    {
        KAN_LOG (resource_render_foundation_material, KAN_LOG_ERROR,
                 "Failed to resolve compile context while trying to emit meta for material \"%s\".",
                 context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    CUSHION_DEFER { kan_rpl_compiler_instance_destroy (compiler_instance); }
    struct kan_rpl_meta_t meta;
    kan_rpl_meta_init (&meta);
    CUSHION_DEFER { kan_rpl_meta_shutdown (&meta); }

    if (!kan_rpl_compiler_instance_emit_meta (compiler_instance, &meta, KAN_RPL_META_EMISSION_FULL))
    {
        KAN_LOG (resource_render_foundation_material, KAN_LOG_ERROR, "Failed to emit meta for material \"%s\".",
                 context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    bool meta_valid = true;
    if (meta.set_pass.buffers.size > 0u || meta.set_pass.samplers.size > 0u)
    {
        KAN_LOG (
            resource_render_foundation_material, KAN_LOG_ERROR,
            "Produced incorrect meta for material \"%s\": meta has entries in pass set, but it should've been compiled "
            "from pass-agnostic sources. That means that source list contains pass set, but it shouldn't.",
            context->primary_name)
        meta_valid = false;
    }

    kan_dynamic_array_set_capacity (&output->vertex_attribute_sources, meta.attribute_sources.size);
    for (kan_loop_size_t index = 0u; index < meta.attribute_sources.size; ++index)
    {
        const struct kan_rpl_meta_attribute_source_t *source =
            &((struct kan_rpl_meta_attribute_source_t *) meta.attribute_sources.data)[index];

        switch (source->rate)
        {
        case KAN_RPL_META_ATTRIBUTE_SOURCE_RATE_VERTEX:
        {
            struct kan_rpl_meta_attribute_source_t *target =
                kan_dynamic_array_add_last (&output->vertex_attribute_sources);
            KAN_ASSERT (target)
            kan_rpl_meta_attribute_source_init_copy (target, source);
            break;
        }

        case KAN_RPL_META_ATTRIBUTE_SOURCE_RATE_INSTANCE:
            if (output->has_instanced_attribute_source)
            {
                KAN_LOG (resource_render_foundation_material, KAN_LOG_ERROR,
                         "Produced incorrect meta for material \"%s\": meta has several instanced attribute sources, "
                         "but it is not supported by materials right now.",
                         context->primary_name)
                meta_valid = false;
            }
            else
            {
                output->has_instanced_attribute_source = true;
                kan_rpl_meta_attribute_source_shutdown (&output->instanced_attribute_source);
                kan_rpl_meta_attribute_source_init_copy (&output->instanced_attribute_source, source);

                for (kan_loop_size_t attribute_index = 0u; attribute_index < source->attributes.size; ++attribute_index)
                {
                    const struct kan_rpl_meta_attribute_t *attribute =
                        &((struct kan_rpl_meta_attribute_t *) source->attributes.data)[attribute_index];

                    switch (attribute->item_format)
                    {
                    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_16:
                    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_8:
                    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_16:
                    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_8:
                    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_16:
                    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_8:
                    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_16:
                    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_8:
                    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_16:
                        KAN_LOG (resource_render_foundation_material, KAN_LOG_ERROR,
                                 "Produced incorrect meta for material \"%s\": instanced attribute source has "
                                 "attribute \"%s\" with item format \"%s\", which is not currently supported. "
                                 "Currently, only 32-bit items are supported for instanced attributes for material "
                                 "code simplification.",
                                 context->primary_name, attribute->name,
                                 kan_rpl_meta_attribute_item_format_to_string (attribute->item_format))
                        meta_valid = false;
                        break;

                    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32:
                    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_32:
                    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_32:
                        break;
                    }
                }
            }

            break;
        }
    }

    output->push_constant_size = meta.push_constant_size;
    for (kan_loop_size_t index = 0u; index < meta.set_material.images.size; ++index)
    {
        struct kan_rpl_meta_image_t *image = &((struct kan_rpl_meta_image_t *) meta.set_material.images.data)[index];
        if (image->image_array_size > 1u)
        {
            KAN_LOG (resource_render_foundation_material, KAN_LOG_ERROR,
                     "Produced incorrect meta for material \"%s\": meta has image array \"%s\" in material set, but "
                     "image arrays are not yet supported on material level.",
                     context->primary_name, image->name)
            meta_valid = false;
        }
    }

    kan_rpl_meta_set_bindings_shutdown (&output->set_material);
    kan_rpl_meta_set_bindings_init_copy (&output->set_material, &meta.set_material);

    kan_rpl_meta_set_bindings_shutdown (&output->set_object);
    kan_rpl_meta_set_bindings_init_copy (&output->set_object, &meta.set_object);

    kan_rpl_meta_set_bindings_shutdown (&output->set_shared);
    kan_rpl_meta_set_bindings_init_copy (&output->set_shared, &meta.set_shared);
    kan_dynamic_array_set_capacity (&output->pipelines, input->pipelines.size);

    for (kan_loop_size_t pipeline_index = 0u; pipeline_index < input->pipelines.size; ++pipeline_index)
    {
        const struct kan_resource_material_pipeline_transient_t *source =
            &((struct kan_resource_material_pipeline_transient_t *) input->pipelines.data)[pipeline_index];
        struct kan_resource_material_pipeline_t *target = kan_dynamic_array_add_last (&output->pipelines);

        kan_allocation_group_stack_push (output->pipelines.allocation_group);
        kan_resource_material_pipeline_init (target);
        kan_allocation_group_stack_pop ();

        target->pass_name = source->pass_name;
        target->variant_name = source->variant_name;

        secondary = context->secondary_input_first;
        const struct kan_resource_rpl_pipeline_t *built = NULL;

        while (secondary)
        {
            if (secondary->type == KAN_STATIC_INTERNED_ID_GET (kan_resource_rpl_pipeline_t) &&
                secondary->name == source->pipeline_name)
            {
                built = secondary->data;
                break;
            }

            secondary = secondary->next;
        }

        KAN_ASSERT (built)
        kan_dynamic_array_set_capacity (&target->entry_points, built->entry_points.size);
        target->entry_points.size = built->entry_points.size;
        memcpy (target->entry_points.data, built->entry_points.data,
                sizeof (struct kan_rpl_entry_point_t) * built->entry_points.size);
        target->code_format = configuration->code_format;

        target->pipeline_settings = built->meta.graphics_classic_settings;
        kan_dynamic_array_set_capacity (&target->color_outputs, built->meta.color_outputs.size);
        target->color_outputs.size = built->meta.color_outputs.size;
        memcpy (target->color_outputs.data, built->meta.color_outputs.data,
                sizeof (struct kan_rpl_meta_color_output_t) * built->meta.color_outputs.size);

        target->color_blend_constants = built->meta.color_blend_constants;
        kan_dynamic_array_set_capacity (&target->code, built->code.size * built->code.item_size);
        target->code.size = target->code.capacity;
        memcpy (target->code.data, built->code.data, target->code.size);
    }

    return meta_valid ? KAN_RESOURCE_BUILD_RULE_SUCCESS : KAN_RESOURCE_BUILD_RULE_FAILURE;
}
