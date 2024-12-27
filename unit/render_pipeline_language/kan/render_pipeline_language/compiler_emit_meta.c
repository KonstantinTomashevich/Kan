#define KAN_RPL_COMPILER_IMPLEMENTATION
#include <kan/render_pipeline_language/compiler_internal.h>

#define SETTING_REQUIRE_IN_BLOCK                                                                                       \
    if (setting->block == KAN_RPL_SETTING_BLOCK_NONE)                                                                  \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Setting \"%s\" should have block index.",        \
                 instance->context_log_name, setting->module_name, setting->source_name, (long) setting->source_line,  \
                 setting->name)                                                                                        \
        valid = KAN_FALSE;                                                                                             \
    }                                                                                                                  \
    else

#define SETTING_REQUIRE_NOT_IN_BLOCK                                                                                   \
    if (setting->block != KAN_RPL_SETTING_BLOCK_NONE)                                                                  \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Setting \"%s\" should not have block index.",    \
                 instance->context_log_name, setting->module_name, setting->source_name, (long) setting->source_line,  \
                 setting->name)                                                                                        \
        valid = KAN_FALSE;                                                                                             \
    }                                                                                                                  \
    else

#define SETTING_REQUIRE_TYPE(TYPE, TYPE_NAME)                                                                          \
    if (setting->type != TYPE)                                                                                         \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Setting \"%s\" should have " TYPE_NAME " type.", \
                 instance->context_log_name, setting->module_name, setting->source_name, (long) setting->source_line,  \
                 setting->name)                                                                                        \
        valid = KAN_FALSE;                                                                                             \
    }                                                                                                                  \
    else

#define SETTING_REQUIRE_POSITIVE_INTEGER                                                                               \
    SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_INTEGER, "integer")                                                     \
    if (setting->integer < 0)                                                                                          \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Setting \"%s\" should be positive integer.",     \
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

static inline kan_bool_t emit_meta_check_common_setting (struct rpl_compiler_instance_t *instance,
                                                         struct kan_rpl_meta_t *meta,
                                                         struct compiler_instance_setting_node_t *setting)
{
    kan_bool_t valid = KAN_TRUE;
    if (setting->name == STATICS.interned_color_blend_constant_r)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLOATING, "floating")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->color_blend_constant_r = setting->floating;
        }
    }
    else if (setting->name == STATICS.interned_color_blend_constant_g)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLOATING, "floating")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->color_blend_constant_g = setting->floating;
        }
    }
    else if (setting->name == STATICS.interned_color_blend_constant_b)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLOATING, "floating")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->color_blend_constant_b = setting->floating;
        }
    }
    else if (setting->name == STATICS.interned_color_blend_constant_a)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLOATING, "floating")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->color_blend_constant_a = setting->floating;
        }
    }
    else
    {
        valid = KAN_FALSE;
    }

    return valid;
}

static inline kan_bool_t emit_meta_check_graphics_classic_setting (struct rpl_compiler_instance_t *instance,
                                                                   struct kan_rpl_meta_t *meta,
                                                                   struct compiler_instance_setting_node_t *setting)
{
#define SETTING_COMPARE_OPERATION(OUTPUT)                                                                              \
    SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")                                                       \
    SETTING_REQUIRE_NOT_IN_BLOCK                                                                                       \
    {                                                                                                                  \
        SETTING_STRING_VALUE (STATICS.interned_always, KAN_RPL_COMPARE_OPERATION_ALWAYS, OUTPUT)                       \
        SETTING_STRING_VALUE (STATICS.interned_never, KAN_RPL_COMPARE_OPERATION_NEVER, OUTPUT)                         \
        SETTING_STRING_VALUE (STATICS.interned_equal, KAN_RPL_COMPARE_OPERATION_EQUAL, OUTPUT)                         \
        SETTING_STRING_VALUE (STATICS.interned_not_equal, KAN_RPL_COMPARE_OPERATION_NOT_EQUAL, OUTPUT)                 \
        SETTING_STRING_VALUE (STATICS.interned_less, KAN_RPL_COMPARE_OPERATION_LESS, OUTPUT)                           \
        SETTING_STRING_VALUE (STATICS.interned_less_or_equal, KAN_RPL_COMPARE_OPERATION_LESS_OR_EQUAL, OUTPUT)         \
        SETTING_STRING_VALUE (STATICS.interned_greater, KAN_RPL_COMPARE_OPERATION_GREATER, OUTPUT)                     \
        SETTING_STRING_VALUE (STATICS.interned_greater_or_equal, KAN_RPL_COMPARE_OPERATION_GREATER_OR_EQUAL, OUTPUT)   \
        SETTING_STRING_NO_MORE_VALUES;                                                                                 \
    }

#define SETTING_STENCIL_OPERATION(OUTPUT)                                                                              \
    SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")                                                       \
    SETTING_REQUIRE_NOT_IN_BLOCK                                                                                       \
    {                                                                                                                  \
        SETTING_STRING_VALUE (STATICS.interned_keep, KAN_RPL_STENCIL_OPERATION_KEEP, OUTPUT)                           \
        SETTING_STRING_VALUE (STATICS.interned_zero, KAN_RPL_STENCIL_OPERATION_ZERO, OUTPUT)                           \
        SETTING_STRING_VALUE (STATICS.interned_replace, KAN_RPL_STENCIL_OPERATION_REPLACE, OUTPUT)                     \
        SETTING_STRING_VALUE (STATICS.interned_increment_and_clamp, KAN_RPL_STENCIL_OPERATION_INCREMENT_AND_CLAMP,     \
                              OUTPUT)                                                                                  \
        SETTING_STRING_VALUE (STATICS.interned_decrement_and_clamp, KAN_RPL_STENCIL_OPERATION_DECREMENT_AND_CLAMP,     \
                              OUTPUT)                                                                                  \
        SETTING_STRING_VALUE (STATICS.interned_invert, KAN_RPL_STENCIL_OPERATION_INVERT, OUTPUT)                       \
        SETTING_STRING_VALUE (STATICS.interned_increment_and_wrap, KAN_RPL_STENCIL_OPERATION_INCREMENT_AND_WRAP,       \
                              OUTPUT)                                                                                  \
        SETTING_STRING_VALUE (STATICS.interned_decrement_and_wrap, KAN_RPL_STENCIL_OPERATION_DECREMENT_AND_WRAP,       \
                              OUTPUT)                                                                                  \
        SETTING_STRING_NO_MORE_VALUES;                                                                                 \
    }

    kan_bool_t valid = KAN_TRUE;
    if (setting->name == STATICS.interned_polygon_mode)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
        SETTING_REQUIRE_NOT_IN_BLOCK
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
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            SETTING_STRING_VALUE (STATICS.interned_back, KAN_RPL_CULL_MODE_BACK,
                                  meta->graphics_classic_settings.cull_mode)
            SETTING_STRING_NO_MORE_VALUES
        }
    }
    else if (setting->name == STATICS.interned_depth_test)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLAG, "flag")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.depth_test = setting->flag;
        }
    }
    else if (setting->name == STATICS.interned_depth_write)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLAG, "flag")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.depth_write = setting->flag;
        }
    }
    else if (setting->name == STATICS.interned_depth_bounds_test)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLAG, "flag")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.depth_bounds_test = setting->flag;
        }
    }
    else if (setting->name == STATICS.interned_depth_compare_operation)
    {
        SETTING_COMPARE_OPERATION (meta->graphics_classic_settings.depth_compare_operation)
    }
    else if (setting->name == STATICS.interned_depth_min)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLOATING, "floating")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.depth_min = setting->floating;
        }
    }
    else if (setting->name == STATICS.interned_depth_max)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLOATING, "floating")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.depth_max = setting->floating;
        }
    }
    else if (setting->name == STATICS.interned_stencil_test)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLAG, "flag")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.stencil_test = setting->flag;
        }
    }
    else if (setting->name == STATICS.interned_stencil_front_on_fail)
    {
        SETTING_STENCIL_OPERATION (meta->graphics_classic_settings.stencil_front_on_fail)
    }
    else if (setting->name == STATICS.interned_stencil_front_on_depth_fail)
    {
        SETTING_STENCIL_OPERATION (meta->graphics_classic_settings.stencil_front_on_depth_fail)
    }
    else if (setting->name == STATICS.interned_stencil_front_on_pass)
    {
        SETTING_STENCIL_OPERATION (meta->graphics_classic_settings.stencil_front_on_pass)
    }
    else if (setting->name == STATICS.interned_stencil_front_compare)
    {
        SETTING_COMPARE_OPERATION (meta->graphics_classic_settings.stencil_front_compare)
    }
    else if (setting->name == STATICS.interned_stencil_front_compare_mask)
    {
        SETTING_REQUIRE_POSITIVE_INTEGER
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.stencil_front_compare_mask = (uint8_t) setting->integer;
        }
    }
    else if (setting->name == STATICS.interned_stencil_front_write_mask)
    {
        SETTING_REQUIRE_POSITIVE_INTEGER
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.stencil_front_write_mask = (uint8_t) setting->integer;
        }
    }
    else if (setting->name == STATICS.interned_stencil_front_reference)
    {
        SETTING_REQUIRE_POSITIVE_INTEGER
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.stencil_front_reference = (uint8_t) setting->integer;
        }
    }
    else if (setting->name == STATICS.interned_stencil_back_on_fail)
    {
        SETTING_STENCIL_OPERATION (meta->graphics_classic_settings.stencil_back_on_fail)
    }
    else if (setting->name == STATICS.interned_stencil_back_on_depth_fail)
    {
        SETTING_STENCIL_OPERATION (meta->graphics_classic_settings.stencil_back_on_depth_fail)
    }
    else if (setting->name == STATICS.interned_stencil_back_on_pass)
    {
        SETTING_STENCIL_OPERATION (meta->graphics_classic_settings.stencil_back_on_pass)
    }
    else if (setting->name == STATICS.interned_stencil_back_compare)
    {
        SETTING_COMPARE_OPERATION (meta->graphics_classic_settings.stencil_back_compare)
    }
    else if (setting->name == STATICS.interned_stencil_back_compare_mask)
    {
        SETTING_REQUIRE_POSITIVE_INTEGER
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.stencil_back_compare_mask = (uint8_t) setting->integer;
        }
    }
    else if (setting->name == STATICS.interned_stencil_back_write_mask)
    {
        SETTING_REQUIRE_POSITIVE_INTEGER
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.stencil_back_write_mask = (uint8_t) setting->integer;
        }
    }
    else if (setting->name == STATICS.interned_stencil_back_reference)
    {
        SETTING_REQUIRE_POSITIVE_INTEGER
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.stencil_back_reference = (uint8_t) setting->integer;
        }
    }
    else
    {
        valid = KAN_FALSE;
    }

#undef SETTING_COMPARE_OPERATION
#undef SETTING_STENCIL_OPERATION

    return valid;
}

static inline kan_bool_t emit_meta_check_color_output_setting (struct rpl_compiler_instance_t *instance,
                                                               struct kan_rpl_meta_t *meta,
                                                               struct compiler_instance_setting_node_t *setting)
{
    kan_bool_t valid = KAN_TRUE;
#define SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK                                                                       \
    if (setting->block >= meta->color_outputs.size)                                                                    \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Setting \"%s\" has block index %lu while there is only %lu color outputs.",           \
                 instance->context_log_name, setting->module_name, setting->source_name, (long) setting->source_line,  \
                 setting->name, (unsigned long) setting->block, (unsigned long) meta->color_outputs.size)              \
        valid = KAN_FALSE;                                                                                             \
    }                                                                                                                  \
    else

#define COLOR_OUTPUT_BLOCK (((struct kan_rpl_meta_color_output_t *) meta->color_outputs.data)[setting->block])

#define BLEND_FACTOR_VALUES(FIELD)                                                                                     \
    SETTING_STRING_VALUE (STATICS.interned_zero, KAN_RPL_BLEND_FACTOR_ZERO, COLOR_OUTPUT_BLOCK.FIELD)                  \
    SETTING_STRING_VALUE (STATICS.interned_one, KAN_RPL_BLEND_FACTOR_ONE, COLOR_OUTPUT_BLOCK.FIELD)                    \
    SETTING_STRING_VALUE (STATICS.interned_source_color, KAN_RPL_BLEND_FACTOR_SOURCE_COLOR, COLOR_OUTPUT_BLOCK.FIELD)  \
    SETTING_STRING_VALUE (STATICS.interned_one_minus_source_color, KAN_RPL_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR,        \
                          COLOR_OUTPUT_BLOCK.FIELD)                                                                    \
    SETTING_STRING_VALUE (STATICS.interned_destination_color, KAN_RPL_BLEND_FACTOR_DESTINATION_COLOR,                  \
                          COLOR_OUTPUT_BLOCK.FIELD)                                                                    \
    SETTING_STRING_VALUE (STATICS.interned_one_minus_destination_color,                                                \
                          KAN_RPL_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR, COLOR_OUTPUT_BLOCK.FIELD)                  \
    SETTING_STRING_VALUE (STATICS.interned_source_alpha, KAN_RPL_BLEND_FACTOR_SOURCE_ALPHA, COLOR_OUTPUT_BLOCK.FIELD)  \
    SETTING_STRING_VALUE (STATICS.interned_one_minus_source_alpha, KAN_RPL_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA,        \
                          COLOR_OUTPUT_BLOCK.FIELD)                                                                    \
    SETTING_STRING_VALUE (STATICS.interned_destination_alpha, KAN_RPL_BLEND_FACTOR_DESTINATION_ALPHA,                  \
                          COLOR_OUTPUT_BLOCK.FIELD)                                                                    \
    SETTING_STRING_VALUE (STATICS.interned_one_minus_destination_alpha,                                                \
                          KAN_RPL_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA, COLOR_OUTPUT_BLOCK.FIELD)                  \
    SETTING_STRING_VALUE (STATICS.interned_constant_color, KAN_RPL_BLEND_FACTOR_CONSTANT_COLOR,                        \
                          COLOR_OUTPUT_BLOCK.FIELD)                                                                    \
    SETTING_STRING_VALUE (STATICS.interned_one_minus_constant_color, KAN_RPL_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,    \
                          COLOR_OUTPUT_BLOCK.FIELD)                                                                    \
    SETTING_STRING_VALUE (STATICS.interned_constant_alpha, KAN_RPL_BLEND_FACTOR_CONSTANT_ALPHA,                        \
                          COLOR_OUTPUT_BLOCK.FIELD)                                                                    \
    SETTING_STRING_VALUE (STATICS.interned_one_minus_constant_alpha, KAN_RPL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,    \
                          COLOR_OUTPUT_BLOCK.FIELD)                                                                    \
    SETTING_STRING_VALUE (STATICS.interned_source_alpha_saturate, KAN_RPL_BLEND_FACTOR_SOURCE_ALPHA_SATURATE,          \
                          COLOR_OUTPUT_BLOCK.FIELD)                                                                    \
    SETTING_STRING_NO_MORE_VALUES

    if (setting->name == STATICS.interned_color_output_use_blend)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLAG, "flag")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK
        {
            COLOR_OUTPUT_BLOCK.use_blend = setting->flag;
        }
    }
    else if (setting->name == STATICS.interned_color_output_write_r)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLAG, "flag")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK
        {
            COLOR_OUTPUT_BLOCK.write_r = setting->flag;
        }
    }
    else if (setting->name == STATICS.interned_color_output_write_g)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLAG, "flag")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK
        {
            COLOR_OUTPUT_BLOCK.write_g = setting->flag;
        }
    }
    else if (setting->name == STATICS.interned_color_output_write_b)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLAG, "flag")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK
        {
            COLOR_OUTPUT_BLOCK.write_b = setting->flag;
        }
    }
    else if (setting->name == STATICS.interned_color_output_write_a)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_FLAG, "flag")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK
        {
            COLOR_OUTPUT_BLOCK.write_a = setting->flag;
        }
    }
    else if (setting->name == STATICS.interned_color_output_source_color_blend_factor)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK
        {
            BLEND_FACTOR_VALUES (source_color_blend_factor)
        }
    }
    else if (setting->name == STATICS.interned_color_output_destination_color_blend_factor)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK
        {
            BLEND_FACTOR_VALUES (destination_color_blend_factor)
        }
    }
    else if (setting->name == STATICS.interned_color_output_color_blend_operation)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK
        {
            SETTING_STRING_VALUE (STATICS.interned_add, KAN_RPL_BLEND_OPERATION_ADD,
                                  COLOR_OUTPUT_BLOCK.color_blend_operation)
            SETTING_STRING_VALUE (STATICS.interned_subtract, KAN_RPL_BLEND_OPERATION_SUBTRACT,
                                  COLOR_OUTPUT_BLOCK.color_blend_operation)
            SETTING_STRING_VALUE (STATICS.interned_reverse_subtract, KAN_RPL_BLEND_OPERATION_REVERSE_SUBTRACT,
                                  COLOR_OUTPUT_BLOCK.color_blend_operation)
            SETTING_STRING_VALUE (STATICS.interned_min, KAN_RPL_BLEND_OPERATION_MIN,
                                  COLOR_OUTPUT_BLOCK.color_blend_operation)
            SETTING_STRING_VALUE (STATICS.interned_max, KAN_RPL_BLEND_OPERATION_MAX,
                                  COLOR_OUTPUT_BLOCK.color_blend_operation)
            SETTING_STRING_NO_MORE_VALUES
        }
    }
    else if (setting->name == STATICS.interned_color_output_source_alpha_blend_factor)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK
        {
            BLEND_FACTOR_VALUES (source_alpha_blend_factor)
        }
    }
    else if (setting->name == STATICS.interned_color_output_destination_alpha_blend_factor)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK
        {
            BLEND_FACTOR_VALUES (destination_alpha_blend_factor)
        }
    }
    else if (setting->name == STATICS.interned_color_output_alpha_blend_operation)
    {
        SETTING_REQUIRE_TYPE (KAN_RPL_SETTING_TYPE_STRING, "string")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK
        {
            SETTING_STRING_VALUE (STATICS.interned_add, KAN_RPL_BLEND_OPERATION_ADD,
                                  COLOR_OUTPUT_BLOCK.alpha_blend_operation)
            SETTING_STRING_VALUE (STATICS.interned_subtract, KAN_RPL_BLEND_OPERATION_SUBTRACT,
                                  COLOR_OUTPUT_BLOCK.alpha_blend_operation)
            SETTING_STRING_VALUE (STATICS.interned_reverse_subtract, KAN_RPL_BLEND_OPERATION_REVERSE_SUBTRACT,
                                  COLOR_OUTPUT_BLOCK.alpha_blend_operation)
            SETTING_STRING_VALUE (STATICS.interned_min, KAN_RPL_BLEND_OPERATION_MIN,
                                  COLOR_OUTPUT_BLOCK.alpha_blend_operation)
            SETTING_STRING_VALUE (STATICS.interned_max, KAN_RPL_BLEND_OPERATION_MAX,
                                  COLOR_OUTPUT_BLOCK.alpha_blend_operation)
            SETTING_STRING_NO_MORE_VALUES
        }
    }
    else
    {
        valid = KAN_FALSE;
    }

#undef SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK
#undef COLOR_OUTPUT_BLOCK
#undef BLEND_FACTOR_VALUES

    return valid;
}

static kan_bool_t emit_meta_settings (struct rpl_compiler_instance_t *instance, struct kan_rpl_meta_t *meta)
{
    kan_bool_t valid = KAN_TRUE;
    meta->graphics_classic_settings = kan_rpl_graphics_classic_pipeline_settings_default ();
    struct compiler_instance_setting_node_t *setting = instance->first_setting;

    while (setting)
    {
        kan_bool_t setting_accepted = emit_meta_check_common_setting (instance, meta, setting);

        if (!setting_accepted)
        {
            switch (instance->pipeline_type)
            {
            case KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC:
                setting_accepted = emit_meta_check_graphics_classic_setting (instance, meta, setting);
                break;
            }
        }

        if (!setting_accepted)
        {
            setting_accepted = emit_meta_check_color_output_setting (instance, meta, setting);
        }

        if (!setting_accepted)
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

#undef SETTING_REQUIRE_TYPE
#undef SETTING_STRING_VALUE
#undef SETTING_STRING_NO_MORE_VALUES

static inline kan_bool_t emit_meta_variable_type_to_meta_type (struct compiler_instance_variable_t *variable,
                                                               enum kan_rpl_meta_variable_type_t *output,
                                                               kan_interned_string_t context_log_name,
                                                               kan_interned_string_t module_name,
                                                               kan_interned_string_t source_name,
                                                               kan_rpl_size_t source_line)
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
    kan_instance_size_t base_offset,
    struct compiler_instance_declaration_node_t *first_declaration,
    struct kan_rpl_meta_buffer_t *meta_output,
    struct kan_trivial_string_buffer_t *name_generation_buffer,
    kan_bool_t tail);

static kan_bool_t emit_meta_gather_parameters_process_field_list (
    struct rpl_compiler_instance_t *instance,
    kan_instance_size_t base_offset,
    struct compiler_instance_declaration_node_t *first_declaration,
    struct kan_rpl_meta_buffer_t *meta_output,
    struct kan_trivial_string_buffer_t *name_generation_buffer,
    kan_bool_t tail)
{
    kan_bool_t valid = KAN_TRUE;
    struct compiler_instance_declaration_node_t *field = first_declaration;

    while (field)
    {
        const kan_instance_size_t length = name_generation_buffer->size;
        if (name_generation_buffer->size > 0u)
        {
            kan_trivial_string_buffer_append_string (name_generation_buffer, ".");
        }

        kan_trivial_string_buffer_append_string (name_generation_buffer, field->variable.name);
        if (!emit_meta_gather_parameters_process_field (instance, base_offset, field, meta_output,
                                                        name_generation_buffer, tail))
        {
            valid = KAN_FALSE;
        }

        kan_trivial_string_buffer_reset (name_generation_buffer, length);
        field = field->next;
    }

    return valid;
}

static kan_bool_t emit_meta_gather_parameters_process_field (struct rpl_compiler_instance_t *instance,
                                                             kan_instance_size_t base_offset,
                                                             struct compiler_instance_declaration_node_t *field,
                                                             struct kan_rpl_meta_buffer_t *meta_output,
                                                             struct kan_trivial_string_buffer_t *name_generation_buffer,
                                                             kan_bool_t tail)
{
    if (field->variable.type.if_vector || field->variable.type.if_matrix)
    {
        if (field->variable.type.array_size_runtime)
        {
            // We do not export parameters from non-structured tails.
            return KAN_TRUE;
        }

        kan_bool_t valid = KAN_TRUE;
        struct kan_dynamic_array_t *parameters =
            tail ? &meta_output->tail_item_parameters : &meta_output->main_parameters;

        struct kan_rpl_meta_parameter_t *parameter = kan_dynamic_array_add_last (parameters);

        if (!parameter)
        {
            kan_dynamic_array_set_capacity (parameters, KAN_MAX (1u, parameters->size * 2u));
            parameter = kan_dynamic_array_add_last (parameters);
            KAN_ASSERT (parameter)
        }

        kan_rpl_meta_parameter_init (parameter);
        parameter->name = kan_char_sequence_intern (name_generation_buffer->buffer,
                                                    name_generation_buffer->buffer + name_generation_buffer->size);
        parameter->offset = base_offset + field->offset;

        if (!emit_meta_variable_type_to_meta_type (&field->variable, &parameter->type, instance->context_log_name,
                                                   field->module_name, field->source_name, field->source_line))
        {
            valid = KAN_FALSE;
        }

        parameter->total_item_count = 1u;
        for (kan_loop_size_t index = 0u; index < field->variable.type.array_dimensions_count; ++index)
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
        if (field->variable.type.array_size_runtime)
        {
            // Should be guaranteed by resolve stage.
            KAN_ASSERT (!tail)
            meta_output->tail_name = field->variable.name;

            return emit_meta_gather_parameters_process_field_list (instance, 0u,
                                                                   field->variable.type.if_struct->first_field,
                                                                   meta_output, name_generation_buffer, KAN_TRUE);
        }
        // Currently we only generate parameters for non-array structs as parameters from arrays of structs sound
        // like a strange and not entirely useful idea.
        else if (field->variable.type.array_dimensions_count == 0u)
        {
            return emit_meta_gather_parameters_process_field_list (instance, base_offset + field->offset,
                                                                   field->variable.type.if_struct->first_field,
                                                                   meta_output, name_generation_buffer, tail);
        }
    }

    return KAN_TRUE;
}

kan_bool_t kan_rpl_compiler_instance_emit_meta (kan_rpl_compiler_instance_t compiler_instance,
                                                struct kan_rpl_meta_t *meta)
{
    struct rpl_compiler_instance_t *instance = KAN_HANDLE_GET (compiler_instance);
    meta->pipeline_type = instance->pipeline_type;
    kan_bool_t valid = KAN_TRUE;

    struct kan_trivial_string_buffer_t name_generation_buffer;
    kan_trivial_string_buffer_init (&name_generation_buffer, STATICS.rpl_meta_allocation_group,
                                    KAN_RPL_COMPILER_INSTANCE_MAX_FLAT_NAME_LENGTH);

    kan_loop_size_t attribute_buffer_count = 0u;
    kan_loop_size_t pass_buffer_count = 0u;
    kan_loop_size_t material_buffer_count = 0u;
    kan_loop_size_t object_buffer_count = 0u;
    kan_loop_size_t unstable_buffer_count = 0u;
    kan_loop_size_t color_outputs = 0u;
    struct compiler_instance_buffer_node_t *buffer = instance->first_buffer;

    while (buffer)
    {
        switch (buffer->type)
        {
        case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
            ++attribute_buffer_count;
            break;

        case KAN_RPL_BUFFER_TYPE_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
            switch (buffer->set)
            {
            case KAN_RPL_SET_PASS:
                ++pass_buffer_count;
                break;

            case KAN_RPL_SET_MATERIAL:
                ++material_buffer_count;
                break;

            case KAN_RPL_SET_OBJECT:
                ++object_buffer_count;
                break;

            case KAN_RPL_SET_UNSTABLE:
                ++unstable_buffer_count;
                break;
            }

            break;

        case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
            break;

        case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
            // Not exposed, only affects pipeline settings.
            if (instance->pipeline_type == KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC)
            {
                struct compiler_instance_buffer_flattened_declaration_t *declaration =
                    buffer->first_flattened_declaration;

                while (declaration)
                {
                    ++color_outputs;
                    declaration = declaration->next;
                }
            }

            break;
        }

        buffer = buffer->next;
    }

    kan_dynamic_array_set_capacity (&meta->attribute_buffers, attribute_buffer_count);
    kan_dynamic_array_set_capacity (&meta->set_pass.buffers, pass_buffer_count);
    kan_dynamic_array_set_capacity (&meta->set_material.buffers, material_buffer_count);
    kan_dynamic_array_set_capacity (&meta->set_object.buffers, object_buffer_count);
    kan_dynamic_array_set_capacity (&meta->set_unstable.buffers, unstable_buffer_count);
    kan_dynamic_array_set_capacity (&meta->color_outputs, color_outputs);

    for (kan_loop_size_t output_index = 0u; output_index < color_outputs; ++output_index)
    {
        *(struct kan_rpl_meta_color_output_t *) kan_dynamic_array_add_last (&meta->color_outputs) =
            kan_rpl_meta_color_output_default ();
    }

    kan_loop_size_t color_output_index = 0u;
    buffer = instance->first_buffer;

    while (buffer)
    {
        struct kan_dynamic_array_t *buffer_array = NULL;
        switch (buffer->type)
        {
        case KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE:
        case KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE:
            buffer_array = &meta->attribute_buffers;
            break;

        case KAN_RPL_BUFFER_TYPE_UNIFORM:
        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
            switch (buffer->set)
            {
            case KAN_RPL_SET_PASS:
                buffer_array = &meta->set_pass.buffers;
                break;

            case KAN_RPL_SET_MATERIAL:
                buffer_array = &meta->set_material.buffers;
                break;

            case KAN_RPL_SET_OBJECT:
                buffer_array = &meta->set_object.buffers;
                break;

            case KAN_RPL_SET_UNSTABLE:
                buffer_array = &meta->set_unstable.buffers;
                break;
            }

            break;

        case KAN_RPL_BUFFER_TYPE_VERTEX_STAGE_OUTPUT:
            break;

        case KAN_RPL_BUFFER_TYPE_FRAGMENT_STAGE_OUTPUT:
            // Not exposed, only affects pipeline settings.
            if (instance->pipeline_type == KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC)
            {
                struct compiler_instance_buffer_flattened_declaration_t *declaration =
                    buffer->first_flattened_declaration;

                while (declaration)
                {
                    KAN_ASSERT (declaration->source_declaration->variable.type.if_vector)
                    ((struct kan_rpl_meta_color_output_t *) meta->color_outputs.data)[color_output_index]
                        .components_count =
                        (uint8_t) declaration->source_declaration->variable.type.if_vector->items_count;

                    ++color_output_index;
                    declaration = declaration->next;
                }
            }

            break;
        }

        if (!buffer_array)
        {
            buffer = buffer->next;
            continue;
        }

        struct kan_rpl_meta_buffer_t *meta_buffer = kan_dynamic_array_add_last (buffer_array);
        KAN_ASSERT (meta_buffer)
        kan_rpl_meta_buffer_init (meta_buffer);

        meta_buffer->name = buffer->name;
        meta_buffer->binding = buffer->binding;
        meta_buffer->type = buffer->type;
        meta_buffer->main_size = buffer->main_size;
        meta_buffer->tail_item_size = buffer->tail_item_size;

        if (buffer->type == KAN_RPL_BUFFER_TYPE_VERTEX_ATTRIBUTE ||
            buffer->type == KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE)
        {
            kan_loop_size_t count = 0u;
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
            buffer->type == KAN_RPL_BUFFER_TYPE_INSTANCED_ATTRIBUTE)
        {
            if (!emit_meta_gather_parameters_process_field_list (instance, 0u, buffer->first_field, meta_buffer,
                                                                 &name_generation_buffer, KAN_FALSE))
            {
                valid = KAN_FALSE;
            }

            kan_dynamic_array_set_capacity (&meta_buffer->main_parameters, meta_buffer->main_parameters.size);
        }

        buffer = buffer->next;
    }

    kan_loop_size_t pass_sampler_count = 0u;
    kan_loop_size_t material_sampler_count = 0u;
    kan_loop_size_t object_sampler_count = 0u;
    kan_loop_size_t unstable_sampler_count = 0u;
    struct compiler_instance_sampler_node_t *sampler = instance->first_sampler;

    while (sampler)
    {
        switch (sampler->set)
        {
        case KAN_RPL_SET_PASS:
            ++pass_sampler_count;
            break;

        case KAN_RPL_SET_MATERIAL:
            ++material_sampler_count;
            break;

        case KAN_RPL_SET_OBJECT:
            ++object_sampler_count;
            break;

        case KAN_RPL_SET_UNSTABLE:
            ++unstable_sampler_count;
            break;
        }

        sampler = sampler->next;
    }

    kan_dynamic_array_set_capacity (&meta->set_pass.samplers, pass_sampler_count);
    kan_dynamic_array_set_capacity (&meta->set_material.samplers, material_sampler_count);
    kan_dynamic_array_set_capacity (&meta->set_object.samplers, object_sampler_count);
    kan_dynamic_array_set_capacity (&meta->set_unstable.samplers, unstable_sampler_count);
    sampler = instance->first_sampler;

    while (sampler)
    {
        struct kan_dynamic_array_t *sampler_array = NULL;
        switch (sampler->set)
        {
        case KAN_RPL_SET_PASS:
            sampler_array = &meta->set_pass.samplers;
            break;

        case KAN_RPL_SET_MATERIAL:
            sampler_array = &meta->set_material.samplers;
            break;

        case KAN_RPL_SET_OBJECT:
            sampler_array = &meta->set_object.samplers;
            break;

        case KAN_RPL_SET_UNSTABLE:
            sampler_array = &meta->set_unstable.samplers;
            break;
        }

        struct kan_rpl_meta_sampler_t *meta_sampler = kan_dynamic_array_add_last (sampler_array);
        KAN_ASSERT (meta_sampler)

        meta_sampler->name = sampler->name;
        meta_sampler->binding = sampler->binding;
        meta_sampler->type = sampler->type;
        sampler = sampler->next;
    }

    if (!emit_meta_settings (instance, meta))
    {
        valid = KAN_FALSE;
    }

    kan_trivial_string_buffer_shutdown (&name_generation_buffer);
    return valid;
}
