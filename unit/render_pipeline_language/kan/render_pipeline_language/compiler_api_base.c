#define KAN_RPL_COMPILER_IMPLEMENTATION
#include <kan/render_pipeline_language/compiler_internal.h>

KAN_LOG_DEFINE_CATEGORY (rpl_compiler_context);
KAN_LOG_DEFINE_CATEGORY (rpl_compiler_instance);

void kan_rpl_meta_parameter_init (struct kan_rpl_meta_parameter_t *instance)
{
    instance->name = NULL;
    instance->type = KAN_RPL_META_VARIABLE_TYPE_F1;
    instance->offset = 0u;
    instance->total_item_count = 0u;
    kan_dynamic_array_init (&instance->meta, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            STATICS.rpl_meta_allocation_group);
}

void kan_rpl_meta_parameter_shutdown (struct kan_rpl_meta_parameter_t *instance)
{
    kan_dynamic_array_shutdown (&instance->meta);
}

void kan_rpl_meta_buffer_init (struct kan_rpl_meta_buffer_t *instance)
{
    instance->name = NULL;
    instance->set = 0u;
    instance->binding = 0u;
    instance->stable_binding = KAN_TRUE;
    instance->type = KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE;
    instance->size = 0u;
    kan_dynamic_array_init (&instance->attributes, 0u, sizeof (struct kan_rpl_meta_attribute_t),
                            _Alignof (struct kan_rpl_meta_attribute_t), STATICS.rpl_meta_allocation_group);
    kan_dynamic_array_init (&instance->parameters, 0u, sizeof (struct kan_rpl_meta_parameter_t),
                            _Alignof (struct kan_rpl_meta_parameter_t), STATICS.rpl_meta_allocation_group);
}

void kan_rpl_meta_buffer_shutdown (struct kan_rpl_meta_buffer_t *instance)
{
    for (uint64_t parameter_index = 0u; parameter_index < instance->parameters.size; ++parameter_index)
    {
        kan_rpl_meta_parameter_shutdown (
            &((struct kan_rpl_meta_parameter_t *) instance->parameters.data)[parameter_index]);
    }

    kan_dynamic_array_shutdown (&instance->attributes);
    kan_dynamic_array_shutdown (&instance->parameters);
}

void kan_rpl_meta_init (struct kan_rpl_meta_t *instance)
{
    kan_rpl_compiler_ensure_statics_initialized ();
    instance->pipeline_type = KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC;
    instance->graphics_classic_settings = kan_rpl_graphics_classic_pipeline_settings_default ();

    kan_dynamic_array_init (&instance->buffers, 0u, sizeof (struct kan_rpl_meta_buffer_t),
                            _Alignof (struct kan_rpl_meta_buffer_t), STATICS.rpl_meta_allocation_group);
    kan_dynamic_array_init (&instance->samplers, 0u, sizeof (struct kan_rpl_meta_sampler_t),
                            _Alignof (struct kan_rpl_meta_sampler_t), STATICS.rpl_meta_allocation_group);
    kan_dynamic_array_init (&instance->color_outputs, 0u, sizeof (struct kan_rpl_meta_color_output_t),
                            _Alignof (struct kan_rpl_meta_color_output_t), STATICS.rpl_meta_allocation_group);

    instance->color_blend_constant_r = 0.0f;
    instance->color_blend_constant_g = 0.0f;
    instance->color_blend_constant_b = 0.0f;
    instance->color_blend_constant_a = 0.0f;
}

void kan_rpl_meta_shutdown (struct kan_rpl_meta_t *instance)
{
    for (uint64_t index = 0u; index < instance->buffers.size; ++index)
    {
        kan_rpl_meta_buffer_shutdown (&((struct kan_rpl_meta_buffer_t *) instance->buffers.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->buffers);
    kan_dynamic_array_shutdown (&instance->samplers);
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

    return (kan_rpl_compiler_context_t) instance;
}

kan_bool_t kan_rpl_compiler_context_use_module (kan_rpl_compiler_context_t compiler_context,
                                                struct kan_rpl_intermediate_t *intermediate_reference)
{
    struct rpl_compiler_context_t *instance = (struct rpl_compiler_context_t *) compiler_context;
    for (uint64_t module_index = 0u; module_index < instance->modules.size; ++module_index)
    {
        if (((struct kan_rpl_intermediate_t **) instance->modules.data)[module_index] == intermediate_reference)
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Caught attempt to use module \"%s\" twice.",
                     instance->log_name, intermediate_reference->log_name)
            return KAN_TRUE;
        }
    }

    for (uint64_t new_option_index = 0u; new_option_index < intermediate_reference->options.size; ++new_option_index)
    {
        struct kan_rpl_option_t *new_option =
            &((struct kan_rpl_option_t *) intermediate_reference->options.data)[new_option_index];

        for (uint64_t old_option_index = 0u; old_option_index < instance->option_values.size; ++old_option_index)
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

    struct kan_rpl_intermediate_t **spot = kan_dynamic_array_add_last (&instance->modules);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (&instance->modules, KAN_MAX (1u, instance->modules.size * 2u));
        spot = kan_dynamic_array_add_last (&instance->modules);
        KAN_ASSERT (spot)
    }

    *spot = intermediate_reference;
    kan_dynamic_array_set_capacity (&instance->option_values,
                                    instance->option_values.size + intermediate_reference->options.size);

    for (uint64_t new_option_index = 0u; new_option_index < intermediate_reference->options.size; ++new_option_index)
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

kan_bool_t kan_rpl_compiler_context_set_option_flag (kan_rpl_compiler_context_t compiler_context,
                                                     kan_interned_string_t name,
                                                     kan_bool_t value)
{
    struct rpl_compiler_context_t *instance = (struct rpl_compiler_context_t *) compiler_context;
    for (uint64_t index = 0u; index < instance->option_values.size; ++index)
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

            option->flag_value = value;
            return KAN_TRUE;
        }
    }

    KAN_LOG (rpl_compiler_context, KAN_LOG_WARNING, "[%s] Unable to find flag option \"%s\".", instance->log_name, name)
    return KAN_FALSE;
}

kan_bool_t kan_rpl_compiler_context_set_option_count (kan_rpl_compiler_context_t compiler_context,
                                                      kan_interned_string_t name,
                                                      uint64_t value)
{
    struct rpl_compiler_context_t *instance = (struct rpl_compiler_context_t *) compiler_context;
    for (uint64_t index = 0u; index < instance->option_values.size; ++index)
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
    struct rpl_compiler_instance_t *instance = (struct rpl_compiler_instance_t *) compiler_instance;
    kan_stack_group_allocator_shutdown (&instance->resolve_allocator);
    kan_free_general (STATICS.rpl_compiler_instance_allocation_group, instance,
                      sizeof (struct rpl_compiler_instance_t));
}

void kan_rpl_compiler_context_destroy (kan_rpl_compiler_context_t compiler_context)
{
    struct rpl_compiler_context_t *instance = (struct rpl_compiler_context_t *) compiler_context;
    kan_dynamic_array_shutdown (&instance->option_values);
    kan_dynamic_array_shutdown (&instance->modules);
    kan_stack_group_allocator_shutdown (&instance->resolve_allocator);
    kan_free_general (STATICS.rpl_compiler_context_allocation_group, instance, sizeof (struct rpl_compiler_context_t));
}
