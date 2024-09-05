#define KAN_RPL_COMPILER_IMPLEMENTATION
#include <kan/render_pipeline_language/compiler_internal.h>

#define SETTING_REQUIRE_TYPE(TYPE, TYPE_NAME)                                                                          \
    if (setting->type != TYPE)                                                                                         \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Setting \"%s\" should have " TYPE_NAME " type.", \
                 instance->context_log_name, setting->module_name, setting->source_name, (long) setting->source_line,  \
                 setting->name)                                                                                        \
        valid = KAN_FALSE;                                                                                             \
    }                                                                                                                  \
    else

#define SETTING_STRING_VALUE(INTERNED_VALUE, REAL_VALUE, OUTPUT)                                                       \
    if (setting->string == INTERNED_VALUE)                                                                             \
    {                                                                                                                  \
        OUTPUT = REAL_VALUE;                                                                                           \
    }                                                                                                                  \
    else

#define SETTING_STRING_NO_MORE_VALUES                                                                                  \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Setting \"%s\" has unknown value \"%s\".",       \
                 instance->context_log_name, setting->module_name, setting->source_name, (long) setting->source_line,  \
                 setting->name, setting->string)                                                                       \
        valid = KAN_FALSE;                                                                                             \
    }

static kan_bool_t emit_meta_graphics_classic_settings (struct rpl_compiler_instance_t *instance,
                                                       struct kan_rpl_meta_t *meta)
{
    kan_bool_t valid = KAN_TRUE;
    meta->graphics_classic_settings = kan_rpl_graphics_classic_pipeline_settings_default ();
    struct compiler_instance_setting_node_t *setting = instance->first_setting;

    while (setting)
    {
        if (setting->name == STATICS.interned_polygon_mode)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (STATICS.interned_fill, KAN_RPL_POLYGON_MODE_FILL,
                                      meta->graphics_classic_settings.polygon_mode)
                SETTING_STRING_VALUE (STATICS.interned_wireframe, KAN_RPL_POLYGON_MODE_WIREFRAME,
                                      meta->graphics_classic_settings.polygon_mode)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else if (setting->name == STATICS.interned_cull_mode)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (STATICS.interned_back, KAN_RPL_CULL_MODE_BACK,
                                      meta->graphics_classic_settings.cull_mode)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else if (setting->name == STATICS.interned_depth_test)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLAG, "flag")
            {
                meta->graphics_classic_settings.depth_test = setting->flag;
            }
        }
        else if (setting->name == STATICS.interned_depth_write)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLAG, "flag")
            {
                meta->graphics_classic_settings.depth_write = setting->flag;
            }
        }
        else
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Unknown global settings \"%s\".",
                     instance->context_log_name, setting->module_name, setting->source_name,
                     (long) setting->source_line, setting->name)
            valid = KAN_FALSE;
        }

        setting = setting->next;
    }

    return valid;
}

static kan_bool_t emit_meta_sampler_settings (struct rpl_compiler_instance_t *instance,
                                              struct compiler_instance_sampler_node_t *sampler,
                                              struct kan_rpl_meta_sampler_settings_t *settings_output)
{
    kan_bool_t valid = KAN_TRUE;
    *settings_output = kan_rpl_meta_sampler_settings_default ();
    struct compiler_instance_setting_node_t *setting = sampler->first_setting;

    while (setting)
    {
        if (setting->name == STATICS.interned_mag_filter)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (STATICS.interned_nearest, KAN_RPL_META_SAMPLER_FILTER_NEAREST,
                                      settings_output->mag_filter)
                SETTING_STRING_VALUE (STATICS.interned_linear, KAN_RPL_META_SAMPLER_FILTER_LINEAR,
                                      settings_output->mag_filter)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else if (setting->name == STATICS.interned_min_filter)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (STATICS.interned_nearest, KAN_RPL_META_SAMPLER_FILTER_NEAREST,
                                      settings_output->min_filter)
                SETTING_STRING_VALUE (STATICS.interned_linear, KAN_RPL_META_SAMPLER_FILTER_LINEAR,
                                      settings_output->min_filter)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else if (setting->name == STATICS.interned_mip_map_mode)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (STATICS.interned_nearest, KAN_RPL_META_SAMPLER_MIP_MAP_MODE_NEAREST,
                                      settings_output->mip_map_mode)
                SETTING_STRING_VALUE (STATICS.interned_linear, KAN_RPL_META_SAMPLER_MIP_MAP_MODE_LINEAR,
                                      settings_output->mip_map_mode)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else if (setting->name == STATICS.interned_address_mode_u)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (STATICS.interned_repeat, KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT,
                                      settings_output->address_mode_u)
                SETTING_STRING_VALUE (STATICS.interned_mirrored_repeat,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
                                      settings_output->address_mode_u)
                SETTING_STRING_VALUE (STATICS.interned_clamp_to_edge, KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                      settings_output->address_mode_u)
                SETTING_STRING_VALUE (STATICS.interned_clamp_to_border,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                      settings_output->address_mode_u)
                SETTING_STRING_VALUE (STATICS.interned_mirror_clamp_to_edge,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
                                      settings_output->address_mode_u)
                SETTING_STRING_VALUE (STATICS.interned_mirror_clamp_to_border,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_BORDER,
                                      settings_output->address_mode_u)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else if (setting->name == STATICS.interned_address_mode_v)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (STATICS.interned_repeat, KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT,
                                      settings_output->address_mode_v)
                SETTING_STRING_VALUE (STATICS.interned_mirrored_repeat,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
                                      settings_output->address_mode_v)
                SETTING_STRING_VALUE (STATICS.interned_clamp_to_edge, KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                      settings_output->address_mode_v)
                SETTING_STRING_VALUE (STATICS.interned_clamp_to_border,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                      settings_output->address_mode_v)
                SETTING_STRING_VALUE (STATICS.interned_mirror_clamp_to_edge,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
                                      settings_output->address_mode_v)
                SETTING_STRING_VALUE (STATICS.interned_mirror_clamp_to_border,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_BORDER,
                                      settings_output->address_mode_v)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else if (setting->name == STATICS.interned_address_mode_w)
        {
            SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
            {
                SETTING_STRING_VALUE (STATICS.interned_repeat, KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT,
                                      settings_output->address_mode_w)
                SETTING_STRING_VALUE (STATICS.interned_mirrored_repeat,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
                                      settings_output->address_mode_w)
                SETTING_STRING_VALUE (STATICS.interned_clamp_to_edge, KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                      settings_output->address_mode_w)
                SETTING_STRING_VALUE (STATICS.interned_clamp_to_border,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                      settings_output->address_mode_w)
                SETTING_STRING_VALUE (STATICS.interned_mirror_clamp_to_edge,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
                                      settings_output->address_mode_w)
                SETTING_STRING_VALUE (STATICS.interned_mirror_clamp_to_border,
                                      KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_BORDER,
                                      settings_output->address_mode_w)
                SETTING_STRING_NO_MORE_VALUES
            }
        }
        else
        {
            KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Unknown sampler settings \"%s\".",
                     instance->context_log_name, setting->module_name, setting->source_name,
                     (long) setting->source_line, setting->name)
            valid = KAN_FALSE;
        }

        setting = setting->next;
    }

    return valid;
}

#undef SETTING_REQUIRE_TYPE
#undef SETTING_STRING_VALUE
#undef SETTING_STRING_NO_MORE_VALUES

static inline kan_bool_t emit_meta_variable_type_to_meta_type (struct compiler_instance_variable_t *variable,
                                                               enum kan_rpl_meta_variable_type_t *output,
                                                               kan_interned_string_t context_log_name,
                                                               kan_interned_string_t module_name,
                                                               kan_interned_string_t source_name,
                                                               uint64_t source_line)
{
    if (variable->type.if_vector == &STATICS.type_f1)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_F1;
    }
    else if (variable->type.if_vector == &STATICS.type_f2)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_F2;
    }
    else if (variable->type.if_vector == &STATICS.type_f3)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_F3;
    }
    else if (variable->type.if_vector == &STATICS.type_f4)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_F4;
    }
    else if (variable->type.if_vector == &STATICS.type_i1)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_I1;
    }
    else if (variable->type.if_vector == &STATICS.type_i2)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_I2;
    }
    else if (variable->type.if_vector == &STATICS.type_i3)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_I3;
    }
    else if (variable->type.if_vector == &STATICS.type_i4)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_I4;
    }
    else if (variable->type.if_matrix == &STATICS.type_f3x3)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_F3X3;
    }
    else if (variable->type.if_matrix == &STATICS.type_f4x4)
    {
        *output = KAN_RPL_META_VARIABLE_TYPE_F4X4;
    }
    else
    {
        KAN_LOG (
            rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Unable to find meta type for type \"%s\".",
            context_log_name, module_name, source_name, (long) source_line,
            get_type_name_for_logging (variable->type.if_vector, variable->type.if_matrix, variable->type.if_struct))
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static kan_bool_t emit_meta_gather_parameters_process_field (
    struct rpl_compiler_instance_t *instance,
    uint64_t base_offset,
    struct compiler_instance_declaration_node_t *first_declaration,
    struct kan_rpl_meta_buffer_t *meta_output,
    struct flattening_name_generation_buffer_t *name_generation_buffer);

static kan_bool_t emit_meta_gather_parameters_process_field_list (
    struct rpl_compiler_instance_t *instance,
    uint64_t base_offset,
    struct compiler_instance_declaration_node_t *first_declaration,
    struct kan_rpl_meta_buffer_t *meta_output,
    struct flattening_name_generation_buffer_t *name_generation_buffer)
{
    kan_bool_t valid = KAN_TRUE;
    struct compiler_instance_declaration_node_t *field = first_declaration;

    while (field)
    {
        const uint64_t length = name_generation_buffer->length;
        flattening_name_generation_buffer_append (name_generation_buffer, field->variable.name);

        if (!emit_meta_gather_parameters_process_field (instance, base_offset, field, meta_output,
                                                        name_generation_buffer))
        {
            valid = KAN_FALSE;
        }

        flattening_name_generation_buffer_reset (name_generation_buffer, length);
        field = field->next;
    }

    return valid;
}

static kan_bool_t emit_meta_gather_parameters_process_field (
    struct rpl_compiler_instance_t *instance,
    uint64_t base_offset,
    struct compiler_instance_declaration_node_t *field,
    struct kan_rpl_meta_buffer_t *meta_output,
    struct flattening_name_generation_buffer_t *name_generation_buffer)
{
    if (field->variable.type.if_vector || field->variable.type.if_matrix)
    {
        kan_bool_t valid = KAN_TRUE;
        struct kan_rpl_meta_parameter_t *parameter = kan_dynamic_array_add_last (&meta_output->parameters);

        if (!parameter)
        {
            kan_dynamic_array_set_capacity (&meta_output->parameters, KAN_MAX (1u, meta_output->parameters.size * 2u));
            parameter = kan_dynamic_array_add_last (&meta_output->parameters);
            KAN_ASSERT (parameter)
        }

        kan_rpl_meta_parameter_init (parameter);
        parameter->name = kan_string_intern (name_generation_buffer->buffer);
        parameter->offset = base_offset + field->offset;

        if (!emit_meta_variable_type_to_meta_type (&field->variable, &parameter->type, instance->context_log_name,
                                                   field->module_name, field->source_name, field->source_line))
        {
            valid = KAN_FALSE;
        }

        parameter->total_item_count = 1u;
        for (uint64_t index = 0u; index < field->variable.type.array_dimensions_count; ++index)
        {
            parameter->total_item_count *= field->variable.type.array_dimensions[index];
        }

        kan_dynamic_array_set_capacity (&parameter->meta, field->meta_count);
        parameter->meta.size = field->meta_count;

        if (field->meta_count > 0u)
        {
            memcpy (parameter->meta.data, field->meta, sizeof (kan_interned_string_t) * field->meta_count);
        }

        return valid;
    }
    else if (field->variable.type.if_struct)
    {
        return emit_meta_gather_parameters_process_field_list (instance, base_offset + field->offset,
                                                               field->variable.type.if_struct->first_field, meta_output,
                                                               name_generation_buffer);
    }

    return KAN_TRUE;
}

kan_bool_t kan_rpl_compiler_instance_emit_meta (kan_rpl_compiler_instance_t compiler_instance,
                                                struct kan_rpl_meta_t *meta)
{
    struct rpl_compiler_instance_t *instance = (struct rpl_compiler_instance_t *) compiler_instance;
    meta->pipeline_type = instance->pipeline_type;
    kan_bool_t valid = KAN_TRUE;

    switch (instance->pipeline_type)
    {
    case KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC:
        if (!emit_meta_graphics_classic_settings (instance, meta))
        {
            valid = KAN_FALSE;
        }

        break;
    }

    uint64_t buffer_count = 0u;
    struct compiler_instance_buffer_node_t *buffer = instance->first_buffer;

    while (buffer)
    {
        ++buffer_count;
        buffer = buffer->next;
    }

    kan_dynamic_array_set_capacity (&meta->buffers, buffer_count);
    buffer = instance->first_buffer;

    while (buffer)
    {
        if (buffer->type == KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT)
        {
            // Not exposed.
            buffer = buffer->next;
            continue;
        }

        if (buffer->type == KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT)
        {
            // Not exposed, only affects pipeline settings.
            if (instance->pipeline_type == KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC)
            {
                struct compiler_instance_buffer_flattened_declaration_t *declaration =
                    buffer->first_flattened_declaration;

                while (declaration)
                {
                    ++meta->graphics_classic_settings.fragment_output_count;
                    declaration = declaration->next;
                }
            }

            buffer = buffer->next;
            continue;
        }

        struct kan_rpl_meta_buffer_t *meta_buffer = kan_dynamic_array_add_last (&meta->buffers);
        KAN_ASSERT (meta_buffer)
        kan_rpl_meta_buffer_init (meta_buffer);

        meta_buffer->name = buffer->name;
        meta_buffer->set = buffer->set;
        meta_buffer->binding = buffer->binding;
        meta_buffer->stable_binding = buffer->stable_binding;
        meta_buffer->type = buffer->type;
        meta_buffer->size = buffer->size;

        if (buffer->type == KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE ||
            buffer->type == KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE)
        {
            uint64_t count = 0u;
            struct compiler_instance_buffer_flattened_declaration_t *flattened_declaration =
                buffer->first_flattened_declaration;

            while (flattened_declaration)
            {
                ++count;
                flattened_declaration = flattened_declaration->next;
            }

            kan_dynamic_array_set_capacity (&meta_buffer->attributes, count);
            flattened_declaration = buffer->first_flattened_declaration;

            while (flattened_declaration)
            {
                struct kan_rpl_meta_attribute_t *meta_attribute = kan_dynamic_array_add_last (&meta_buffer->attributes);
                meta_attribute->location = flattened_declaration->location;
                meta_attribute->offset = flattened_declaration->source_declaration->offset;

                if (!emit_meta_variable_type_to_meta_type (&flattened_declaration->source_declaration->variable,
                                                           &meta_attribute->type, instance->context_log_name,
                                                           flattened_declaration->source_declaration->module_name,
                                                           flattened_declaration->source_declaration->source_name,
                                                           flattened_declaration->source_declaration->source_line))
                {
                    valid = KAN_FALSE;
                }

                flattened_declaration = flattened_declaration->next;
            }
        }

        if (buffer->type == KAN_RPL_BUFFER_TYPE_UNIFORM || buffer->type == KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE ||
            buffer->type == KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE ||
            buffer->type == KAN_RPL_BUFFER_TYPE_INSTANCED_UNIFORM ||
            buffer->type == KAN_RPL_BUFFER_TYPE_INSTANCED_READ_ONLY_STORAGE)
        {
            struct flattening_name_generation_buffer_t name_generation_buffer;
            name_generation_buffer.length = 0u;
            name_generation_buffer.buffer[0u] = '\0';

            if (!emit_meta_gather_parameters_process_field_list (instance, 0u, buffer->first_field, meta_buffer,
                                                                 &name_generation_buffer))
            {
                valid = KAN_FALSE;
            }

            kan_dynamic_array_set_capacity (&meta_buffer->parameters, meta_buffer->parameters.size);
        }

        buffer = buffer->next;
    }

    uint64_t sampler_count = 0u;
    struct compiler_instance_sampler_node_t *sampler = instance->first_sampler;

    while (sampler)
    {
        ++sampler_count;
        sampler = sampler->next;
    }

    kan_dynamic_array_set_capacity (&meta->samplers, sampler_count);
    sampler = instance->first_sampler;

    while (sampler)
    {
        struct kan_rpl_meta_sampler_t *meta_sampler = kan_dynamic_array_add_last (&meta->samplers);
        KAN_ASSERT (meta_sampler)

        meta_sampler->name = sampler->name;
        meta_sampler->set = sampler->set;
        meta_sampler->binding = sampler->binding;
        meta_sampler->type = sampler->type;

        if (!emit_meta_sampler_settings (instance, sampler, &meta_sampler->settings))
        {
            valid = KAN_FALSE;
        }

        sampler = sampler->next;
    }

    return valid;
}
