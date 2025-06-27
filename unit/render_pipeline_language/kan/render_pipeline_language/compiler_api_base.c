#define KAN_RPL_COMPILER_IMPLEMENTATION
#include <kan/render_pipeline_language/compiler_internal.h>

KAN_LOG_DEFINE_CATEGORY (rpl_compiler_context);
KAN_LOG_DEFINE_CATEGORY (rpl_compiler_instance);

const char *kan_rpl_meta_attribute_item_format_to_string (enum kan_rpl_meta_attribute_item_format_t format)
{
    switch (format)
    {
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_16:
        return "KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_16";

    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32:
        return "KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32";

    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_8:
        return "KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_8";

    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_16:
        return "KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_16";

    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_8:
        return "KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_8";

    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_16:
        return "KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_16";

    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_8:
        return "KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_8";

    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_16:
        return "KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_16";

    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_32:
        return "KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_32";

    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_8:
        return "KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_8";

    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_16:
        return "KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_16";

    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_32:
        return "KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_32";
    }

    KAN_ASSERT (false)
    return "<unknown>";
}

kan_instance_size_t kan_rpl_meta_attribute_item_format_get_size (enum kan_rpl_meta_attribute_item_format_t format)
{
    switch (format)
    {
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_16:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_16:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_16:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_16:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_16:
        return sizeof (uint16_t);

    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_32:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_32:
        return sizeof (uint32_t);

    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_8:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_8:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_8:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_8:
        return sizeof (uint8_t);
    }

    KAN_ASSERT (false)
    return 0u;
}

kan_instance_size_t kan_rpl_meta_attribute_item_format_get_alignment (enum kan_rpl_meta_attribute_item_format_t format)
{
    switch (format)
    {
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_16:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_16:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_16:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_16:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_16:
        return alignof (uint16_t);

    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_32:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_32:
        return alignof (uint32_t);

    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_8:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_8:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_8:
    case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_8:
        return alignof (uint8_t);
    }

    KAN_ASSERT (false)
    return 0u;
}

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_attribute_init (struct kan_rpl_meta_attribute_t *instance)
{
    instance->name = NULL;
    instance->location = 0u;
    instance->offset = 0u;
    instance->class = KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_1;
    instance->item_format = KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32;
    kan_dynamic_array_init (&instance->meta, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            STATICS.rpl_meta_allocation_group);
}

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_attribute_init_copy (struct kan_rpl_meta_attribute_t *instance,
                                                                    const struct kan_rpl_meta_attribute_t *copy_from)
{
    instance->name = copy_from->name;
    instance->location = copy_from->location;
    instance->offset = copy_from->offset;
    instance->class = copy_from->class;
    instance->item_format = copy_from->item_format;

    kan_dynamic_array_init (&instance->meta, copy_from->meta.size, sizeof (kan_interned_string_t),
                            alignof (kan_interned_string_t), STATICS.rpl_meta_allocation_group);
    instance->meta.size = copy_from->meta.size;

    if (instance->meta.size > 0u)
    {
        memcpy (instance->meta.data, copy_from->meta.data, copy_from->meta.size * copy_from->meta.item_size);
    }
}

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_attribute_shutdown (struct kan_rpl_meta_attribute_t *instance)
{
    kan_dynamic_array_shutdown (&instance->meta);
}

void kan_rpl_meta_attribute_source_init (struct kan_rpl_meta_attribute_source_t *instance)
{
    instance->name = NULL;
    instance->rate = KAN_RPL_META_ATTRIBUTE_SOURCE_RATE_VERTEX;
    instance->binding = 0u;
    instance->block_size = 0u;
    kan_dynamic_array_init (&instance->attributes, 0u, sizeof (struct kan_rpl_meta_attribute_t),
                            alignof (struct kan_rpl_meta_attribute_t), STATICS.rpl_meta_allocation_group);
}

void kan_rpl_meta_attribute_source_init_copy (struct kan_rpl_meta_attribute_source_t *instance,
                                              const struct kan_rpl_meta_attribute_source_t *copy_from)
{
    instance->name = copy_from->name;
    instance->rate = copy_from->rate;
    instance->binding = copy_from->binding;
    instance->block_size = copy_from->block_size;

    kan_dynamic_array_init (&instance->attributes, copy_from->attributes.size, sizeof (struct kan_rpl_meta_attribute_t),
                            alignof (struct kan_rpl_meta_attribute_t), STATICS.rpl_meta_allocation_group);

    for (kan_loop_size_t index = 0u; index < copy_from->attributes.size; ++index)
    {
        kan_rpl_meta_attribute_init_copy (kan_dynamic_array_add_last (&instance->attributes),
                                          &((struct kan_rpl_meta_attribute_t *) copy_from->attributes.data)[index]);
    }
}

void kan_rpl_meta_attribute_source_shutdown (struct kan_rpl_meta_attribute_source_t *instance)
{
    for (kan_loop_size_t attribute_index = 0u; attribute_index < instance->attributes.size; ++attribute_index)
    {
        kan_rpl_meta_attribute_shutdown (
            &((struct kan_rpl_meta_attribute_t *) instance->attributes.data)[attribute_index]);
    }

    kan_dynamic_array_shutdown (&instance->attributes);
}

const char *kan_rpl_meta_variable_type_to_string (enum kan_rpl_meta_variable_type_t type)
{
    switch (type)
    {
    case KAN_RPL_META_VARIABLE_TYPE_F1:
        return "KAN_RPL_META_VARIABLE_TYPE_F1";

    case KAN_RPL_META_VARIABLE_TYPE_F2:
        return "KAN_RPL_META_VARIABLE_TYPE_F2";

    case KAN_RPL_META_VARIABLE_TYPE_F3:
        return "KAN_RPL_META_VARIABLE_TYPE_F3";

    case KAN_RPL_META_VARIABLE_TYPE_F4:
        return "KAN_RPL_META_VARIABLE_TYPE_F4";

    case KAN_RPL_META_VARIABLE_TYPE_U1:
        return "KAN_RPL_META_VARIABLE_TYPE_U1";

    case KAN_RPL_META_VARIABLE_TYPE_U2:
        return "KAN_RPL_META_VARIABLE_TYPE_U2";

    case KAN_RPL_META_VARIABLE_TYPE_U3:
        return "KAN_RPL_META_VARIABLE_TYPE_U3";

    case KAN_RPL_META_VARIABLE_TYPE_U4:
        return "KAN_RPL_META_VARIABLE_TYPE_U4";

    case KAN_RPL_META_VARIABLE_TYPE_S1:
        return "KAN_RPL_META_VARIABLE_TYPE_S1";

    case KAN_RPL_META_VARIABLE_TYPE_S2:
        return "KAN_RPL_META_VARIABLE_TYPE_S2";

    case KAN_RPL_META_VARIABLE_TYPE_S3:
        return "KAN_RPL_META_VARIABLE_TYPE_S3";

    case KAN_RPL_META_VARIABLE_TYPE_S4:
        return "KAN_RPL_META_VARIABLE_TYPE_S4";

    case KAN_RPL_META_VARIABLE_TYPE_F3X3:
        return "KAN_RPL_META_VARIABLE_TYPE_F3X3";

    case KAN_RPL_META_VARIABLE_TYPE_F4X4:
        return "KAN_RPL_META_VARIABLE_TYPE_F4X4";
    }

    KAN_ASSERT (false)
    return "<unknown>";
}

void kan_rpl_meta_parameter_init (struct kan_rpl_meta_parameter_t *instance)
{
    instance->name = NULL;
    instance->type = KAN_RPL_META_VARIABLE_TYPE_F1;
    instance->offset = 0u;
    instance->total_item_count = 0u;
    kan_dynamic_array_init (&instance->meta, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            STATICS.rpl_meta_allocation_group);
}

void kan_rpl_meta_parameter_init_copy (struct kan_rpl_meta_parameter_t *instance,
                                       const struct kan_rpl_meta_parameter_t *copy_from)
{
    instance->name = copy_from->name;
    instance->type = copy_from->type;
    instance->offset = copy_from->offset;
    instance->total_item_count = copy_from->total_item_count;

    kan_dynamic_array_init (&instance->meta, copy_from->meta.size, sizeof (kan_interned_string_t),
                            alignof (kan_interned_string_t), STATICS.rpl_meta_allocation_group);
    instance->meta.size = copy_from->meta.size;

    if (instance->meta.size > 0u)
    {
        memcpy (instance->meta.data, copy_from->meta.data, copy_from->meta.size * copy_from->meta.item_size);
    }
}

void kan_rpl_meta_parameter_shutdown (struct kan_rpl_meta_parameter_t *instance)
{
    kan_dynamic_array_shutdown (&instance->meta);
}

void kan_rpl_meta_buffer_init (struct kan_rpl_meta_buffer_t *instance)
{
    instance->name = NULL;
    instance->binding = 0u;
    instance->type = KAN_RPL_BUFFER_TYPE_UNIFORM;
    instance->main_size = 0u;
    instance->tail_item_size = 0u;
    kan_dynamic_array_init (&instance->main_parameters, 0u, sizeof (struct kan_rpl_meta_parameter_t),
                            alignof (struct kan_rpl_meta_parameter_t), STATICS.rpl_meta_allocation_group);

    instance->tail_name = NULL;
    kan_dynamic_array_init (&instance->tail_item_parameters, 0u, sizeof (struct kan_rpl_meta_parameter_t),
                            alignof (struct kan_rpl_meta_parameter_t), STATICS.rpl_meta_allocation_group);
}

void kan_rpl_meta_buffer_init_copy (struct kan_rpl_meta_buffer_t *instance,
                                    const struct kan_rpl_meta_buffer_t *copy_from)
{
    instance->name = copy_from->name;
    instance->binding = copy_from->binding;
    instance->type = copy_from->type;
    instance->main_size = copy_from->main_size;
    instance->tail_item_size = copy_from->tail_item_size;

    kan_dynamic_array_init (&instance->main_parameters, copy_from->main_parameters.size,
                            sizeof (struct kan_rpl_meta_parameter_t), alignof (struct kan_rpl_meta_parameter_t),
                            STATICS.rpl_meta_allocation_group);

    for (kan_loop_size_t index = 0u; index < copy_from->main_parameters.size; ++index)
    {
        kan_rpl_meta_parameter_init_copy (
            kan_dynamic_array_add_last (&instance->main_parameters),
            &((struct kan_rpl_meta_parameter_t *) copy_from->main_parameters.data)[index]);
    }

    instance->tail_name = copy_from->tail_name;
    kan_dynamic_array_init (&instance->tail_item_parameters, copy_from->tail_item_parameters.size,
                            sizeof (struct kan_rpl_meta_parameter_t), alignof (struct kan_rpl_meta_parameter_t),
                            STATICS.rpl_meta_allocation_group);

    for (kan_loop_size_t index = 0u; index < copy_from->tail_item_parameters.size; ++index)
    {
        kan_rpl_meta_parameter_init_copy (
            kan_dynamic_array_add_last (&instance->tail_item_parameters),
            &((struct kan_rpl_meta_parameter_t *) copy_from->tail_item_parameters.data)[index]);
    }
}

void kan_rpl_meta_buffer_shutdown (struct kan_rpl_meta_buffer_t *instance)
{
    for (kan_loop_size_t parameter_index = 0u; parameter_index < instance->main_parameters.size; ++parameter_index)
    {
        kan_rpl_meta_parameter_shutdown (
            &((struct kan_rpl_meta_parameter_t *) instance->main_parameters.data)[parameter_index]);
    }

    for (kan_loop_size_t parameter_index = 0u; parameter_index < instance->tail_item_parameters.size; ++parameter_index)
    {
        kan_rpl_meta_parameter_shutdown (
            &((struct kan_rpl_meta_parameter_t *) instance->tail_item_parameters.data)[parameter_index]);
    }

    kan_dynamic_array_shutdown (&instance->main_parameters);
    kan_dynamic_array_shutdown (&instance->tail_item_parameters);
}

void kan_rpl_meta_set_bindings_init (struct kan_rpl_meta_set_bindings_t *instance)
{
    kan_dynamic_array_init (&instance->buffers, 0u, sizeof (struct kan_rpl_meta_buffer_t),
                            alignof (struct kan_rpl_meta_buffer_t), STATICS.rpl_meta_allocation_group);
    kan_dynamic_array_init (&instance->samplers, 0u, sizeof (struct kan_rpl_meta_sampler_t),
                            alignof (struct kan_rpl_meta_sampler_t), STATICS.rpl_meta_allocation_group);
    kan_dynamic_array_init (&instance->images, 0u, sizeof (struct kan_rpl_meta_image_t),
                            alignof (struct kan_rpl_meta_image_t), STATICS.rpl_meta_allocation_group);
}

void kan_rpl_meta_set_bindings_init_copy (struct kan_rpl_meta_set_bindings_t *instance,
                                          const struct kan_rpl_meta_set_bindings_t *copy_from)
{
    kan_dynamic_array_init (&instance->buffers, copy_from->buffers.size, sizeof (struct kan_rpl_meta_buffer_t),
                            alignof (struct kan_rpl_meta_buffer_t), STATICS.rpl_meta_allocation_group);

    for (kan_loop_size_t index = 0u; index < copy_from->buffers.size; ++index)
    {
        kan_rpl_meta_buffer_init_copy (kan_dynamic_array_add_last (&instance->buffers),
                                       &((struct kan_rpl_meta_buffer_t *) copy_from->buffers.data)[index]);
    }

    kan_dynamic_array_init (&instance->samplers, copy_from->samplers.size, sizeof (struct kan_rpl_meta_sampler_t),
                            alignof (struct kan_rpl_meta_sampler_t), STATICS.rpl_meta_allocation_group);
    instance->samplers.size = copy_from->samplers.size;

    if (instance->samplers.size > 0u)
    {
        memcpy (instance->samplers.data, copy_from->samplers.data,
                copy_from->samplers.size * copy_from->samplers.item_size);
    }

    kan_dynamic_array_init (&instance->images, copy_from->images.size, sizeof (struct kan_rpl_meta_image_t),
                            alignof (struct kan_rpl_meta_image_t), STATICS.rpl_meta_allocation_group);
    instance->images.size = copy_from->images.size;

    if (instance->images.size > 0u)
    {
        memcpy (instance->images.data, copy_from->images.data, copy_from->images.size * copy_from->images.item_size);
    }
}

void kan_rpl_meta_set_bindings_shutdown (struct kan_rpl_meta_set_bindings_t *instance)
{
    for (kan_loop_size_t index = 0u; index < instance->buffers.size; ++index)
    {
        kan_rpl_meta_buffer_shutdown (&((struct kan_rpl_meta_buffer_t *) instance->buffers.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->buffers);
    kan_dynamic_array_shutdown (&instance->samplers);
    kan_dynamic_array_shutdown (&instance->images);
}

void kan_rpl_meta_init (struct kan_rpl_meta_t *instance)
{
    instance->pipeline_type = KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC;
    instance->graphics_classic_settings = kan_rpl_graphics_classic_pipeline_settings_default ();

    kan_dynamic_array_init (&instance->attribute_sources, 0u, sizeof (struct kan_rpl_meta_attribute_source_t),
                            alignof (struct kan_rpl_meta_attribute_source_t), STATICS.rpl_meta_allocation_group);
    instance->push_constant_size = 0u;

    kan_rpl_meta_set_bindings_init (&instance->set_pass);
    kan_rpl_meta_set_bindings_init (&instance->set_material);
    kan_rpl_meta_set_bindings_init (&instance->set_object);
    kan_rpl_meta_set_bindings_init (&instance->set_shared);

    kan_dynamic_array_init (&instance->color_outputs, 0u, sizeof (struct kan_rpl_meta_color_output_t),
                            alignof (struct kan_rpl_meta_color_output_t), STATICS.rpl_meta_allocation_group);

    instance->color_blend_constants.r = 0.0f;
    instance->color_blend_constants.g = 0.0f;
    instance->color_blend_constants.b = 0.0f;
    instance->color_blend_constants.a = 0.0f;
}

void kan_rpl_meta_init_copy (struct kan_rpl_meta_t *instance, const struct kan_rpl_meta_t *copy_from)
{
    instance->pipeline_type = copy_from->pipeline_type;
    switch (instance->pipeline_type)
    {
    case KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC:
        // No unions, can copy everything directly.
        instance->graphics_classic_settings = copy_from->graphics_classic_settings;
        break;
    }

    kan_dynamic_array_init (&instance->attribute_sources, copy_from->attribute_sources.size,
                            sizeof (struct kan_rpl_meta_attribute_source_t),
                            alignof (struct kan_rpl_meta_attribute_source_t), STATICS.rpl_meta_allocation_group);

    for (kan_loop_size_t index = 0u; index < copy_from->attribute_sources.size; ++index)
    {
        kan_rpl_meta_attribute_source_init_copy (
            kan_dynamic_array_add_last (&instance->attribute_sources),
            &((struct kan_rpl_meta_attribute_source_t *) copy_from->attribute_sources.data)[index]);
    }

    instance->push_constant_size = copy_from->push_constant_size;
    kan_rpl_meta_set_bindings_init_copy (&instance->set_pass, &copy_from->set_pass);
    kan_rpl_meta_set_bindings_init_copy (&instance->set_material, &copy_from->set_material);
    kan_rpl_meta_set_bindings_init_copy (&instance->set_object, &copy_from->set_object);
    kan_rpl_meta_set_bindings_init_copy (&instance->set_shared, &copy_from->set_shared);

    kan_dynamic_array_init (&instance->color_outputs, copy_from->color_outputs.size,
                            sizeof (struct kan_rpl_meta_color_output_t), alignof (struct kan_rpl_meta_color_output_t),
                            STATICS.rpl_meta_allocation_group);
    instance->color_outputs.size = copy_from->color_outputs.size;

    if (instance->color_outputs.size > 0u)
    {
        memcpy (instance->color_outputs.data, copy_from->color_outputs.data,
                copy_from->color_outputs.size * copy_from->color_outputs.item_size);
    }

    instance->color_blend_constants = copy_from->color_blend_constants;
}

void kan_rpl_meta_shutdown (struct kan_rpl_meta_t *instance)
{
    for (kan_loop_size_t index = 0u; index < instance->attribute_sources.size; ++index)
    {
        kan_rpl_meta_attribute_source_shutdown (
            &((struct kan_rpl_meta_attribute_source_t *) instance->attribute_sources.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->attribute_sources);
    kan_rpl_meta_set_bindings_shutdown (&instance->set_pass);
    kan_rpl_meta_set_bindings_shutdown (&instance->set_material);
    kan_rpl_meta_set_bindings_shutdown (&instance->set_object);
    kan_rpl_meta_set_bindings_shutdown (&instance->set_shared);
    kan_dynamic_array_shutdown (&instance->color_outputs);
}

kan_rpl_compiler_context_t kan_rpl_compiler_context_create (enum kan_rpl_pipeline_type_t pipeline_type,
                                                            kan_interned_string_t log_name)
{
    kan_rpl_compiler_ensure_statics_initialized ();
    struct rpl_compiler_context_t *instance =
        kan_allocate_general (STATICS.rpl_compiler_context_allocation_group, sizeof (struct rpl_compiler_context_t),
                              alignof (struct rpl_compiler_context_t));

    instance->pipeline_type = pipeline_type;
    instance->log_name = log_name;

    kan_dynamic_array_init (&instance->option_values, 0u, sizeof (struct rpl_compiler_context_option_value_t),
                            alignof (struct rpl_compiler_context_option_value_t),
                            STATICS.rpl_compiler_allocation_group);
    kan_dynamic_array_init (&instance->modules, 0u, sizeof (struct kan_rpl_intermediate_t *),
                            alignof (struct kan_rpl_intermediate_t *), STATICS.rpl_compiler_allocation_group);
    kan_stack_group_allocator_init (&instance->resolve_allocator, STATICS.rpl_compiler_context_allocation_group,
                                    KAN_RPL_COMPILER_CONTEXT_RESOLVE_STACK);

    return KAN_HANDLE_SET (kan_rpl_compiler_context_t, instance);
}

bool kan_rpl_compiler_context_use_module (kan_rpl_compiler_context_t compiler_context,
                                          const struct kan_rpl_intermediate_t *intermediate_reference)
{
    struct rpl_compiler_context_t *instance = KAN_HANDLE_GET (compiler_context);
    for (kan_loop_size_t module_index = 0u; module_index < instance->modules.size; ++module_index)
    {
        if (((struct kan_rpl_intermediate_t **) instance->modules.data)[module_index] == intermediate_reference)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Caught attempt to use module \"%s\" twice.",
                     instance->log_name, intermediate_reference->log_name)
            return true;
        }
    }

    for (kan_loop_size_t new_option_index = 0u; new_option_index < intermediate_reference->options.size;
         ++new_option_index)
    {
        struct kan_rpl_option_t *new_option =
            &((struct kan_rpl_option_t *) intermediate_reference->options.data)[new_option_index];

        for (kan_loop_size_t old_option_index = 0u; old_option_index < instance->option_values.size; ++old_option_index)
        {
            struct rpl_compiler_context_option_value_t *old_option =
                &((struct rpl_compiler_context_option_value_t *) instance->option_values.data)[old_option_index];

            if (old_option->name == new_option->name)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING,
                         "[%s] Unable to use module \"%s\" as it contains option \"%s\" which is already declared in "
                         "other used module.",
                         instance->log_name, intermediate_reference->log_name, new_option->name)
                return false;
            }
        }
    }

    const struct kan_rpl_intermediate_t **spot = kan_dynamic_array_add_last (&instance->modules);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (&instance->modules, KAN_MAX (1u, instance->modules.size * 2u));
        spot = kan_dynamic_array_add_last (&instance->modules);
        KAN_ASSERT (spot)
    }

    *spot = intermediate_reference;
    kan_dynamic_array_set_capacity (&instance->option_values,
                                    instance->option_values.size + intermediate_reference->options.size);

    for (kan_loop_size_t new_option_index = 0u; new_option_index < intermediate_reference->options.size;
         ++new_option_index)
    {
        struct kan_rpl_option_t *new_option =
            &((struct kan_rpl_option_t *) intermediate_reference->options.data)[new_option_index];

        struct rpl_compiler_context_option_value_t *option_value =
            kan_dynamic_array_add_last (&instance->option_values);
        KAN_ASSERT (option_value)

        option_value->name = new_option->name;
        option_value->scope = new_option->scope;
        option_value->source_module = intermediate_reference;
        option_value->source_option = new_option;

        switch (new_option->type)
        {
        case KAN_RPL_OPTION_TYPE_FLAG:
            option_value->value.type = COMPILE_TIME_EVALUATION_VALUE_TYPE_BOOLEAN;
            option_value->value.boolean_value = new_option->flag_default_value;
            break;

        case KAN_RPL_OPTION_TYPE_UINT:
            option_value->value.type = COMPILE_TIME_EVALUATION_VALUE_TYPE_UINT;
            option_value->value.uint_value = new_option->uint_default_value;
            break;

        case KAN_RPL_OPTION_TYPE_SINT:
            option_value->value.type = COMPILE_TIME_EVALUATION_VALUE_TYPE_SINT;
            option_value->value.sint_value = new_option->sint_default_value;
            break;

        case KAN_RPL_OPTION_TYPE_FLOAT:
            option_value->value.type = COMPILE_TIME_EVALUATION_VALUE_TYPE_FLOAT;
            option_value->value.float_value = new_option->float_default_value;
            break;

        case KAN_RPL_OPTION_TYPE_ENUM:
            option_value->value.type = COMPILE_TIME_EVALUATION_VALUE_TYPE_STRING;
            option_value->value.string_value =
                ((kan_interned_string_t *)
                     intermediate_reference->string_lists_storage.data)[new_option->enum_values.list_index];
            break;
        }
    }

    return true;
}

static inline bool match_target_scope (enum kan_rpl_option_target_scope_t target_scope,
                                       enum kan_rpl_option_scope_t real_scope)
{
    switch (target_scope)
    {
    case KAN_RPL_OPTION_TARGET_SCOPE_ANY:
        return true;

    case KAN_RPL_OPTION_TARGET_SCOPE_GLOBAL:
        return real_scope == KAN_RPL_OPTION_SCOPE_GLOBAL;

    case KAN_RPL_OPTION_TARGET_SCOPE_INSTANCE:
        return real_scope == KAN_RPL_OPTION_SCOPE_INSTANCE;
    }

    KAN_ASSERT (false)
    return false;
}

static inline const char *get_target_scope_name (enum kan_rpl_option_target_scope_t target_scope)
{
    switch (target_scope)
    {
    case KAN_RPL_OPTION_TARGET_SCOPE_ANY:
        return "any";

    case KAN_RPL_OPTION_TARGET_SCOPE_GLOBAL:
        return "global";

    case KAN_RPL_OPTION_TARGET_SCOPE_INSTANCE:
        return "instance";
    }

    KAN_ASSERT (false)
    return "<unknown>";
}

bool kan_rpl_compiler_context_set_option_flag (kan_rpl_compiler_context_t compiler_context,
                                               enum kan_rpl_option_target_scope_t target_scope,
                                               kan_interned_string_t name,
                                               bool value)
{
    struct rpl_compiler_context_t *instance = KAN_HANDLE_GET (compiler_context);
    for (kan_loop_size_t index = 0u; index < instance->option_values.size; ++index)
    {
        struct rpl_compiler_context_option_value_t *option =
            &((struct rpl_compiler_context_option_value_t *) instance->option_values.data)[index];

        if (option->name == name)
        {
            if (option->value.type != COMPILE_TIME_EVALUATION_VALUE_TYPE_BOOLEAN)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not a flag.", instance->log_name,
                         name)
                return false;
            }

            if (!match_target_scope (target_scope, option->scope))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not in %s scope.",
                         instance->log_name, name, get_target_scope_name (target_scope))
                return false;
            }

            option->value.boolean_value = value;
            return true;
        }
    }

    KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Unable to find flag option \"%s\".", instance->log_name, name)
    return false;
}

bool kan_rpl_compiler_context_set_option_uint (kan_rpl_compiler_context_t compiler_context,
                                               enum kan_rpl_option_target_scope_t target_scope,
                                               kan_interned_string_t name,
                                               kan_rpl_unsigned_int_literal_t value)
{
    struct rpl_compiler_context_t *instance = KAN_HANDLE_GET (compiler_context);
    for (kan_loop_size_t index = 0u; index < instance->option_values.size; ++index)
    {
        struct rpl_compiler_context_option_value_t *option =
            &((struct rpl_compiler_context_option_value_t *) instance->option_values.data)[index];

        if (option->name == name)
        {
            if (option->value.type != COMPILE_TIME_EVALUATION_VALUE_TYPE_UINT)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not an uint.",
                         instance->log_name, name)
                return false;
            }

            if (!match_target_scope (target_scope, option->scope))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not in %s scope.",
                         instance->log_name, name, get_target_scope_name (target_scope))
                return false;
            }

            option->value.uint_value = value;
            return true;
        }
    }

    KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Unable to find uint option \"%s\".", instance->log_name, name)
    return false;
}

bool kan_rpl_compiler_context_set_option_sint (kan_rpl_compiler_context_t compiler_context,
                                               enum kan_rpl_option_target_scope_t target_scope,
                                               kan_interned_string_t name,
                                               kan_rpl_signed_int_literal_t value)
{
    struct rpl_compiler_context_t *instance = KAN_HANDLE_GET (compiler_context);
    for (kan_loop_size_t index = 0u; index < instance->option_values.size; ++index)
    {
        struct rpl_compiler_context_option_value_t *option =
            &((struct rpl_compiler_context_option_value_t *) instance->option_values.data)[index];

        if (option->name == name)
        {
            if (option->value.type != COMPILE_TIME_EVALUATION_VALUE_TYPE_SINT)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not a sint.", instance->log_name,
                         name)
                return false;
            }

            if (!match_target_scope (target_scope, option->scope))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not in %s scope.",
                         instance->log_name, name, get_target_scope_name (target_scope))
                return false;
            }

            option->value.sint_value = value;
            return true;
        }
    }

    KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Unable to find sint option \"%s\".", instance->log_name, name)
    return false;
}

bool kan_rpl_compiler_context_set_option_float (kan_rpl_compiler_context_t compiler_context,
                                                enum kan_rpl_option_target_scope_t target_scope,
                                                kan_interned_string_t name,
                                                kan_rpl_floating_t value)
{
    struct rpl_compiler_context_t *instance = KAN_HANDLE_GET (compiler_context);
    for (kan_loop_size_t index = 0u; index < instance->option_values.size; ++index)
    {
        struct rpl_compiler_context_option_value_t *option =
            &((struct rpl_compiler_context_option_value_t *) instance->option_values.data)[index];

        if (option->name == name)
        {
            if (option->value.type != COMPILE_TIME_EVALUATION_VALUE_TYPE_FLOAT)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not a float.",
                         instance->log_name, name)
                return false;
            }

            if (!match_target_scope (target_scope, option->scope))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not in %s scope.",
                         instance->log_name, name, get_target_scope_name (target_scope))
                return false;
            }

            option->value.float_value = value;
            return true;
        }
    }

    KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Unable to find float option \"%s\".", instance->log_name,
             name)
    return false;
}

bool kan_rpl_compiler_context_set_option_enum (kan_rpl_compiler_context_t compiler_context,
                                               enum kan_rpl_option_target_scope_t target_scope,
                                               kan_interned_string_t name,
                                               kan_interned_string_t value)
{
    struct rpl_compiler_context_t *instance = KAN_HANDLE_GET (compiler_context);
    for (kan_loop_size_t index = 0u; index < instance->option_values.size; ++index)
    {
        struct rpl_compiler_context_option_value_t *option =
            &((struct rpl_compiler_context_option_value_t *) instance->option_values.data)[index];

        if (option->name == name)
        {
            if (option->value.type != COMPILE_TIME_EVALUATION_VALUE_TYPE_STRING)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not an enum.",
                         instance->log_name, name)
                return false;
            }

            if (!match_target_scope (target_scope, option->scope))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not in %s scope.",
                         instance->log_name, name, get_target_scope_name (target_scope))
                return false;
            }

            const kan_interned_string_t *first_value =
                &((kan_interned_string_t *)
                      option->source_module->string_lists_storage.data)[option->source_option->enum_values.list_index];
            const kan_interned_string_t *last_value = first_value + option->source_option->enum_values.list_size;
            const kan_interned_string_t *search_value = first_value;

            while (search_value != last_value)
            {
                if (*search_value == value)
                {
                    break;
                }

                ++search_value;
            }

            if (search_value == last_value)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING,
                         "[%s] Option \"%s\" enum has no requested value \"%s\".", instance->log_name, name, value)
                return false;
            }

            option->value.string_value = value;
            return true;
        }
    }

    KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Unable to find uint option \"%s\".", instance->log_name, name)
    return false;
}

void kan_rpl_compiler_instance_destroy (kan_rpl_compiler_instance_t compiler_instance)
{
    struct rpl_compiler_instance_t *instance = KAN_HANDLE_GET (compiler_instance);
    kan_stack_group_allocator_shutdown (&instance->resolve_allocator);
    kan_free_general (STATICS.rpl_compiler_instance_allocation_group, instance,
                      sizeof (struct rpl_compiler_instance_t));
}

void kan_rpl_compiler_context_destroy (kan_rpl_compiler_context_t compiler_context)
{
    struct rpl_compiler_context_t *instance = KAN_HANDLE_GET (compiler_context);
    kan_dynamic_array_shutdown (&instance->option_values);
    kan_dynamic_array_shutdown (&instance->modules);
    kan_stack_group_allocator_shutdown (&instance->resolve_allocator);
    kan_free_general (STATICS.rpl_compiler_context_allocation_group, instance, sizeof (struct rpl_compiler_context_t));
}
