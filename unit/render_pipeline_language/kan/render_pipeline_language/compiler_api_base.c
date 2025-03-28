#define KAN_RPL_COMPILER_IMPLEMENTATION
#include <kan/render_pipeline_language/compiler_internal.h>

KAN_LOG_DEFINE_CATEGORY (rpl_compiler_context);
KAN_LOG_DEFINE_CATEGORY (rpl_compiler_instance);

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

    case KAN_RPL_META_VARIABLE_TYPE_I1:
        return "KAN_RPL_META_VARIABLE_TYPE_I1";

    case KAN_RPL_META_VARIABLE_TYPE_I2:
        return "KAN_RPL_META_VARIABLE_TYPE_I2";

    case KAN_RPL_META_VARIABLE_TYPE_I3:
        return "KAN_RPL_META_VARIABLE_TYPE_I3";

    case KAN_RPL_META_VARIABLE_TYPE_I4:
        return "KAN_RPL_META_VARIABLE_TYPE_I4";

    case KAN_RPL_META_VARIABLE_TYPE_F3X3:
        return "KAN_RPL_META_VARIABLE_TYPE_F3X3";

    case KAN_RPL_META_VARIABLE_TYPE_F4X4:
        return "KAN_RPL_META_VARIABLE_TYPE_F4X4";
    }

    KAN_ASSERT (KAN_FALSE)
    return "<unknown>";
}

void kan_rpl_meta_parameter_init (struct kan_rpl_meta_parameter_t *instance)
{
    instance->name = NULL;
    instance->type = KAN_RPL_META_VARIABLE_TYPE_F1;
    instance->offset = 0u;
    instance->total_item_count = 0u;
    kan_dynamic_array_init (&instance->meta, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
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
                            _Alignof (kan_interned_string_t), STATICS.rpl_meta_allocation_group);
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
    instance->type = KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE;
    instance->main_size = 0u;
    instance->tail_item_size = 0u;
    kan_dynamic_array_init (&instance->attributes, 0u, sizeof (struct kan_rpl_meta_attribute_t),
                            _Alignof (struct kan_rpl_meta_attribute_t), STATICS.rpl_meta_allocation_group);
    kan_dynamic_array_init (&instance->main_parameters, 0u, sizeof (struct kan_rpl_meta_parameter_t),
                            _Alignof (struct kan_rpl_meta_parameter_t), STATICS.rpl_meta_allocation_group);

    instance->tail_name = NULL;
    kan_dynamic_array_init (&instance->tail_item_parameters, 0u, sizeof (struct kan_rpl_meta_parameter_t),
                            _Alignof (struct kan_rpl_meta_parameter_t), STATICS.rpl_meta_allocation_group);
}

void kan_rpl_meta_buffer_init_copy (struct kan_rpl_meta_buffer_t *instance,
                                    const struct kan_rpl_meta_buffer_t *copy_from)
{
    instance->name = copy_from->name;
    instance->binding = copy_from->binding;
    instance->type = copy_from->type;
    instance->main_size = copy_from->main_size;
    instance->tail_item_size = copy_from->tail_item_size;

    kan_dynamic_array_init (&instance->attributes, copy_from->attributes.size, sizeof (struct kan_rpl_meta_attribute_t),
                            _Alignof (struct kan_rpl_meta_attribute_t), STATICS.rpl_meta_allocation_group);
    instance->attributes.size = copy_from->attributes.size;

    if (instance->attributes.size > 0u)
    {
        memcpy (instance->attributes.data, copy_from->attributes.data,
                copy_from->attributes.size * copy_from->attributes.item_size);
    }

    kan_dynamic_array_init (&instance->main_parameters, copy_from->main_parameters.size,
                            sizeof (struct kan_rpl_meta_parameter_t), _Alignof (struct kan_rpl_meta_parameter_t),
                            STATICS.rpl_meta_allocation_group);

    for (kan_loop_size_t index = 0u; index < copy_from->main_parameters.size; ++index)
    {
        kan_rpl_meta_parameter_init_copy (
            kan_dynamic_array_add_last (&instance->main_parameters),
            &((struct kan_rpl_meta_parameter_t *) copy_from->main_parameters.data)[index]);
    }

    instance->tail_name = copy_from->tail_name;
    kan_dynamic_array_init (&instance->tail_item_parameters, copy_from->tail_item_parameters.size,
                            sizeof (struct kan_rpl_meta_parameter_t), _Alignof (struct kan_rpl_meta_parameter_t),
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

    kan_dynamic_array_shutdown (&instance->attributes);
    kan_dynamic_array_shutdown (&instance->main_parameters);
    kan_dynamic_array_shutdown (&instance->tail_item_parameters);
}

void kan_rpl_meta_set_bindings_init (struct kan_rpl_meta_set_bindings_t *instance)
{
    kan_dynamic_array_init (&instance->buffers, 0u, sizeof (struct kan_rpl_meta_buffer_t),
                            _Alignof (struct kan_rpl_meta_buffer_t), STATICS.rpl_meta_allocation_group);
    kan_dynamic_array_init (&instance->samplers, 0u, sizeof (struct kan_rpl_meta_sampler_t),
                            _Alignof (struct kan_rpl_meta_sampler_t), STATICS.rpl_meta_allocation_group);
    kan_dynamic_array_init (&instance->images, 0u, sizeof (struct kan_rpl_meta_image_t),
                            _Alignof (struct kan_rpl_meta_image_t), STATICS.rpl_meta_allocation_group);
}

void kan_rpl_meta_set_bindings_init_copy (struct kan_rpl_meta_set_bindings_t *instance,
                                          const struct kan_rpl_meta_set_bindings_t *copy_from)
{
    kan_dynamic_array_init (&instance->buffers, copy_from->buffers.size, sizeof (struct kan_rpl_meta_buffer_t),
                            _Alignof (struct kan_rpl_meta_buffer_t), STATICS.rpl_meta_allocation_group);

    for (kan_loop_size_t index = 0u; index < copy_from->buffers.size; ++index)
    {
        kan_rpl_meta_buffer_init_copy (kan_dynamic_array_add_last (&instance->buffers),
                                       &((struct kan_rpl_meta_buffer_t *) copy_from->buffers.data)[index]);
    }

    kan_dynamic_array_init (&instance->samplers, copy_from->samplers.size, sizeof (struct kan_rpl_meta_sampler_t),
                            _Alignof (struct kan_rpl_meta_sampler_t), STATICS.rpl_meta_allocation_group);
    instance->samplers.size = copy_from->samplers.size;

    if (instance->samplers.size > 0u)
    {
        memcpy (instance->samplers.data, copy_from->samplers.data,
                copy_from->samplers.size * copy_from->samplers.item_size);
    }
    
    kan_dynamic_array_init (&instance->images, copy_from->images.size, sizeof (struct kan_rpl_meta_image_t),
                            _Alignof (struct kan_rpl_meta_image_t), STATICS.rpl_meta_allocation_group);
    instance->images.size = copy_from->images.size;
    
    if (instance->images.size > 0u)
    {
        memcpy (instance->images.data, copy_from->images.data,
                copy_from->images.size * copy_from->images.item_size);
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

    kan_dynamic_array_init (&instance->attribute_buffers, 0u, sizeof (struct kan_rpl_meta_buffer_t),
                            _Alignof (struct kan_rpl_meta_buffer_t), STATICS.rpl_meta_allocation_group);
    kan_rpl_meta_set_bindings_init (&instance->set_pass);
    kan_rpl_meta_set_bindings_init (&instance->set_material);
    kan_rpl_meta_set_bindings_init (&instance->set_object);
    kan_rpl_meta_set_bindings_init (&instance->set_shared);

    kan_dynamic_array_init (&instance->color_outputs, 0u, sizeof (struct kan_rpl_meta_color_output_t),
                            _Alignof (struct kan_rpl_meta_color_output_t), STATICS.rpl_meta_allocation_group);

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

    kan_dynamic_array_init (&instance->attribute_buffers, copy_from->attribute_buffers.size,
                            sizeof (struct kan_rpl_meta_buffer_t), _Alignof (struct kan_rpl_meta_buffer_t),
                            STATICS.rpl_meta_allocation_group);

    for (kan_loop_size_t index = 0u; index < copy_from->attribute_buffers.size; ++index)
    {
        kan_rpl_meta_buffer_init_copy (kan_dynamic_array_add_last (&instance->attribute_buffers),
                                       &((struct kan_rpl_meta_buffer_t *) copy_from->attribute_buffers.data)[index]);
    }

    kan_rpl_meta_set_bindings_init_copy (&instance->set_pass, &copy_from->set_pass);
    kan_rpl_meta_set_bindings_init_copy (&instance->set_material, &copy_from->set_material);
    kan_rpl_meta_set_bindings_init_copy (&instance->set_object, &copy_from->set_object);
    kan_rpl_meta_set_bindings_init_copy (&instance->set_shared, &copy_from->set_shared);

    kan_dynamic_array_init (&instance->color_outputs, copy_from->color_outputs.size,
                            sizeof (struct kan_rpl_meta_color_output_t), _Alignof (struct kan_rpl_meta_color_output_t),
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
    for (kan_loop_size_t index = 0u; index < instance->attribute_buffers.size; ++index)
    {
        kan_rpl_meta_buffer_shutdown (&((struct kan_rpl_meta_buffer_t *) instance->attribute_buffers.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->attribute_buffers);
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
                              _Alignof (struct rpl_compiler_context_t));

    instance->pipeline_type = pipeline_type;
    instance->log_name = log_name;

    kan_dynamic_array_init (&instance->option_values, 0u, sizeof (struct rpl_compiler_context_option_value_t),
                            _Alignof (struct rpl_compiler_context_option_value_t),
                            STATICS.rpl_compiler_allocation_group);
    kan_dynamic_array_init (&instance->modules, 0u, sizeof (struct kan_rpl_intermediate_t *),
                            _Alignof (struct kan_rpl_intermediate_t *), STATICS.rpl_compiler_allocation_group);
    kan_stack_group_allocator_init (&instance->resolve_allocator, STATICS.rpl_compiler_context_allocation_group,
                                    KAN_RPL_COMPILER_CONTEXT_RESOLVE_STACK);

    kan_trivial_string_buffer_init (&instance->name_generation_buffer, STATICS.rpl_compiler_context_allocation_group,
                                    KAN_RPL_COMPILER_INSTANCE_MAX_FLAT_NAME_LENGTH);
    return KAN_HANDLE_SET (kan_rpl_compiler_context_t, instance);
}

kan_bool_t kan_rpl_compiler_context_use_module (kan_rpl_compiler_context_t compiler_context,
                                                const struct kan_rpl_intermediate_t *intermediate_reference)
{
    struct rpl_compiler_context_t *instance = KAN_HANDLE_GET (compiler_context);
    for (kan_loop_size_t module_index = 0u; module_index < instance->modules.size; ++module_index)
    {
        if (((struct kan_rpl_intermediate_t **) instance->modules.data)[module_index] == intermediate_reference)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Caught attempt to use module \"%s\" twice.",
                     instance->log_name, intermediate_reference->log_name)
            return KAN_TRUE;
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
                return KAN_FALSE;
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
        option_value->type = new_option->type;

        switch (new_option->type)
        {
        case KAN_RPL_OPTION_TYPE_FLAG:
            option_value->flag_value = new_option->flag_default_value;
            break;

        case KAN_RPL_OPTION_TYPE_COUNT:
            option_value->count_value = new_option->count_default_value;
            break;
        }
    }

    return KAN_TRUE;
}

static inline kan_bool_t match_target_scope (enum kan_rpl_option_target_scope_t target_scope,
                                             enum kan_rpl_option_scope_t real_scope)
{
    switch (target_scope)
    {
    case KAN_RPL_OPTION_TARGET_SCOPE_ANY:
        return KAN_TRUE;

    case KAN_RPL_OPTION_TARGET_SCOPE_GLOBAL:
        return real_scope == KAN_RPL_OPTION_SCOPE_GLOBAL;

    case KAN_RPL_OPTION_TARGET_SCOPE_INSTANCE:
        return real_scope == KAN_RPL_OPTION_SCOPE_INSTANCE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
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

    KAN_ASSERT (KAN_FALSE)
    return "<unknown>";
}

kan_bool_t kan_rpl_compiler_context_set_option_flag (kan_rpl_compiler_context_t compiler_context,
                                                     enum kan_rpl_option_target_scope_t target_scope,
                                                     kan_interned_string_t name,
                                                     kan_bool_t value)
{
    struct rpl_compiler_context_t *instance = KAN_HANDLE_GET (compiler_context);
    for (kan_loop_size_t index = 0u; index < instance->option_values.size; ++index)
    {
        struct rpl_compiler_context_option_value_t *option =
            &((struct rpl_compiler_context_option_value_t *) instance->option_values.data)[index];

        if (option->name == name)
        {
            if (option->type != KAN_RPL_OPTION_TYPE_FLAG)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not a flag.", instance->log_name,
                         name)
                return KAN_FALSE;
            }

            if (!match_target_scope (target_scope, option->scope))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not in %s scope.",
                         instance->log_name, name, get_target_scope_name (target_scope))
                return KAN_FALSE;
            }

            option->flag_value = value;
            return KAN_TRUE;
        }
    }

    KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Unable to find flag option \"%s\".", instance->log_name, name)
    return KAN_FALSE;
}

kan_bool_t kan_rpl_compiler_context_set_option_count (kan_rpl_compiler_context_t compiler_context,
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
            if (option->type != KAN_RPL_OPTION_TYPE_COUNT)
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not a count.",
                         instance->log_name, name)
                return KAN_FALSE;
            }

            if (!match_target_scope (target_scope, option->scope))
            {
                KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Option \"%s\" is not in %s scope.",
                         instance->log_name, name, get_target_scope_name (target_scope))
                return KAN_FALSE;
            }

            option->count_value = value;
            return KAN_TRUE;
        }
    }

    KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Unable to find count option \"%s\".", instance->log_name,
             name)
    return KAN_FALSE;
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
    kan_trivial_string_buffer_shutdown (&instance->name_generation_buffer);
    kan_free_general (STATICS.rpl_compiler_context_allocation_group, instance, sizeof (struct rpl_compiler_context_t));
}
