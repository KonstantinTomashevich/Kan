#define KAN_RPL_COMPILER_IMPLEMENTATION
#include <kan/render_pipeline_language/compiler_internal.h>

KAN_USE_STATIC_INTERNED_IDS

#define SETTING_REQUIRE_IN_BLOCK                                                                                       \
    if (setting->block == KAN_RPL_SETTING_BLOCK_NONE)                                                                  \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Setting \"%s\" should have block index.",        \
                 instance->context_log_name, setting->module_name, setting->source_name, (long) setting->source_line,  \
                 setting->name)                                                                                        \
        valid = false;                                                                                                 \
    }                                                                                                                  \
    else

#define SETTING_REQUIRE_NOT_IN_BLOCK                                                                                   \
    if (setting->block != KAN_RPL_SETTING_BLOCK_NONE)                                                                  \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Setting \"%s\" should not have block index.",    \
                 instance->context_log_name, setting->module_name, setting->source_name, (long) setting->source_line,  \
                 setting->name)                                                                                        \
        valid = false;                                                                                                 \
    }                                                                                                                  \
    else

#define SETTING_REQUIRE_TYPE(TYPE, TYPE_NAME)                                                                          \
    if (setting->value.type != TYPE)                                                                                   \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Setting \"%s\" should have " TYPE_NAME " type.", \
                 instance->context_log_name, setting->module_name, setting->source_name, (long) setting->source_line,  \
                 setting->name)                                                                                        \
        valid = false;                                                                                                 \
    }                                                                                                                  \
    else

#define SETTING_STRING_VALUE(INTERNED_VALUE, REAL_VALUE, OUTPUT)                                                       \
    if (setting->value.string_value == INTERNED_VALUE)                                                                 \
    {                                                                                                                  \
        OUTPUT = REAL_VALUE;                                                                                           \
    }                                                                                                                  \
    else

#define SETTING_STRING_NO_MORE_VALUES                                                                                  \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Setting \"%s\" has unknown value \"%s\".",       \
                 instance->context_log_name, setting->module_name, setting->source_name, (long) setting->source_line,  \
                 setting->name, setting->value.string_value)                                                           \
        valid = false;                                                                                                 \
    }

static inline bool emit_meta_check_common_setting (struct rpl_compiler_instance_t *instance,
                                                   struct kan_rpl_meta_t *meta,
                                                   struct compiler_instance_setting_node_t *setting)
{
    bool valid = true;
    if (setting->name == KAN_STATIC_INTERNED_ID_GET (color_blend_constant_r))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_FLOAT, "floating")
        SETTING_REQUIRE_NOT_IN_BLOCK { meta->color_blend_constants.r = setting->value.float_value; }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (color_blend_constant_g))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_FLOAT, "floating")
        SETTING_REQUIRE_NOT_IN_BLOCK { meta->color_blend_constants.g = setting->value.float_value; }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (color_blend_constant_b))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_FLOAT, "floating")
        SETTING_REQUIRE_NOT_IN_BLOCK { meta->color_blend_constants.b = setting->value.float_value; }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (color_blend_constant_a))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_FLOAT, "floating")
        SETTING_REQUIRE_NOT_IN_BLOCK { meta->color_blend_constants.a = setting->value.float_value; }
    }
    else
    {
        valid = false;
    }

    return valid;
}

static inline bool emit_meta_check_graphics_classic_setting (struct rpl_compiler_instance_t *instance,
                                                             struct kan_rpl_meta_t *meta,
                                                             struct compiler_instance_setting_node_t *setting)
{
#define SETTING_COMPARE_OPERATION(OUTPUT)                                                                              \
    SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_STRING, "string")                                         \
    SETTING_REQUIRE_NOT_IN_BLOCK                                                                                       \
    {                                                                                                                  \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (always), KAN_RPL_COMPARE_OPERATION_ALWAYS, OUTPUT)           \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (never), KAN_RPL_COMPARE_OPERATION_NEVER, OUTPUT)             \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (equal), KAN_RPL_COMPARE_OPERATION_EQUAL, OUTPUT)             \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (not_equal), KAN_RPL_COMPARE_OPERATION_NOT_EQUAL, OUTPUT)     \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (less), KAN_RPL_COMPARE_OPERATION_LESS, OUTPUT)               \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (less_or_equal), KAN_RPL_COMPARE_OPERATION_LESS_OR_EQUAL,     \
                              OUTPUT)                                                                                  \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (greater), KAN_RPL_COMPARE_OPERATION_GREATER, OUTPUT)         \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (greater_or_equal),                                           \
                              KAN_RPL_COMPARE_OPERATION_GREATER_OR_EQUAL, OUTPUT)                                      \
        SETTING_STRING_NO_MORE_VALUES;                                                                                 \
    }

#define SETTING_STENCIL_OPERATION(OUTPUT)                                                                              \
    SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_STRING, "string")                                         \
    SETTING_REQUIRE_NOT_IN_BLOCK                                                                                       \
    {                                                                                                                  \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (keep), KAN_RPL_STENCIL_OPERATION_KEEP, OUTPUT)               \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (zero), KAN_RPL_STENCIL_OPERATION_ZERO, OUTPUT)               \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (replace), KAN_RPL_STENCIL_OPERATION_REPLACE, OUTPUT)         \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (increment_and_clamp),                                        \
                              KAN_RPL_STENCIL_OPERATION_INCREMENT_AND_CLAMP, OUTPUT)                                   \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (decrement_and_clamp),                                        \
                              KAN_RPL_STENCIL_OPERATION_DECREMENT_AND_CLAMP, OUTPUT)                                   \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (invert), KAN_RPL_STENCIL_OPERATION_INVERT, OUTPUT)           \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (increment_and_wrap),                                         \
                              KAN_RPL_STENCIL_OPERATION_INCREMENT_AND_WRAP, OUTPUT)                                    \
        SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (decrement_and_wrap),                                         \
                              KAN_RPL_STENCIL_OPERATION_DECREMENT_AND_WRAP, OUTPUT)                                    \
        SETTING_STRING_NO_MORE_VALUES;                                                                                 \
    }

    bool valid = true;
    if (setting->name == KAN_STATIC_INTERNED_ID_GET (polygon_mode))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_STRING, "string")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (fill), KAN_RPL_POLYGON_MODE_FILL,
                                  meta->graphics_classic_settings.polygon_mode)
            SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (wireframe), KAN_RPL_POLYGON_MODE_WIREFRAME,
                                  meta->graphics_classic_settings.polygon_mode)
            SETTING_STRING_NO_MORE_VALUES
        }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (cull_mode))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_STRING, "string")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (none), KAN_RPL_CULL_MODE_NONE,
                                  meta->graphics_classic_settings.cull_mode)
            SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (back), KAN_RPL_CULL_MODE_BACK,
                                  meta->graphics_classic_settings.cull_mode)
            SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (front), KAN_RPL_CULL_MODE_FRONT,
                                  meta->graphics_classic_settings.cull_mode)
            SETTING_STRING_NO_MORE_VALUES
        }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (depth_test))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_BOOLEAN, "flag")
        SETTING_REQUIRE_NOT_IN_BLOCK { meta->graphics_classic_settings.depth_test = setting->value.boolean_value; }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (depth_write))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_BOOLEAN, "flag")
        SETTING_REQUIRE_NOT_IN_BLOCK { meta->graphics_classic_settings.depth_write = setting->value.boolean_value; }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (depth_bounds_test))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_BOOLEAN, "flag")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.depth_bounds_test = setting->value.boolean_value;
        }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (depth_compare_operation))
    {
        SETTING_COMPARE_OPERATION (meta->graphics_classic_settings.depth_compare_operation)
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (depth_min))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_FLOAT, "floating")
        SETTING_REQUIRE_NOT_IN_BLOCK { meta->graphics_classic_settings.depth_min = setting->value.float_value; }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (depth_max))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_FLOAT, "floating")
        SETTING_REQUIRE_NOT_IN_BLOCK { meta->graphics_classic_settings.depth_max = setting->value.float_value; }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (stencil_test))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_BOOLEAN, "flag")
        SETTING_REQUIRE_NOT_IN_BLOCK { meta->graphics_classic_settings.stencil_test = setting->value.boolean_value; }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (stencil_front_on_fail))
    {
        SETTING_STENCIL_OPERATION (meta->graphics_classic_settings.stencil_front_on_fail)
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (stencil_front_on_depth_fail))
    {
        SETTING_STENCIL_OPERATION (meta->graphics_classic_settings.stencil_front_on_depth_fail)
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (stencil_front_on_pass))
    {
        SETTING_STENCIL_OPERATION (meta->graphics_classic_settings.stencil_front_on_pass)
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (stencil_front_compare))
    {
        SETTING_COMPARE_OPERATION (meta->graphics_classic_settings.stencil_front_compare)
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (stencil_front_compare_mask))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_UINT, "unsigned integer")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.stencil_front_compare_mask = (uint8_t) setting->value.uint_value;
        }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (stencil_front_write_mask))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_UINT, "unsigned integer")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.stencil_front_write_mask = (uint8_t) setting->value.uint_value;
        }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (stencil_front_reference))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_UINT, "unsigned integer")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.stencil_front_reference = (uint8_t) setting->value.uint_value;
        }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (stencil_back_on_fail))
    {
        SETTING_STENCIL_OPERATION (meta->graphics_classic_settings.stencil_back_on_fail)
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (stencil_back_on_depth_fail))
    {
        SETTING_STENCIL_OPERATION (meta->graphics_classic_settings.stencil_back_on_depth_fail)
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (stencil_back_on_pass))
    {
        SETTING_STENCIL_OPERATION (meta->graphics_classic_settings.stencil_back_on_pass)
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (stencil_back_compare))
    {
        SETTING_COMPARE_OPERATION (meta->graphics_classic_settings.stencil_back_compare)
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (stencil_back_compare_mask))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_UINT, "unsigned integer")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.stencil_back_compare_mask = (uint8_t) setting->value.uint_value;
        }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (stencil_back_write_mask))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_UINT, "unsigned integer")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.stencil_back_write_mask = (uint8_t) setting->value.uint_value;
        }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (stencil_back_reference))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_UINT, "unsigned integer")
        SETTING_REQUIRE_NOT_IN_BLOCK
        {
            meta->graphics_classic_settings.stencil_back_reference = (uint8_t) setting->value.uint_value;
        }
    }
    else
    {
        valid = false;
    }

#undef SETTING_COMPARE_OPERATION
#undef SETTING_STENCIL_OPERATION

    return valid;
}

static inline bool emit_meta_check_color_output_setting (struct rpl_compiler_instance_t *instance,
                                                         struct kan_rpl_meta_t *meta,
                                                         struct compiler_instance_setting_node_t *setting)
{
    bool valid = true;
#define SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK                                                                       \
    if (setting->block >= meta->color_outputs.size)                                                                    \
    {                                                                                                                  \
        KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR,                                                                  \
                 "[%s:%s:%s:%ld] Setting \"%s\" has block index %lu while there is only %lu color outputs.",           \
                 instance->context_log_name, setting->module_name, setting->source_name, (long) setting->source_line,  \
                 setting->name, (unsigned long) setting->block, (unsigned long) meta->color_outputs.size)              \
        valid = false;                                                                                                 \
    }                                                                                                                  \
    else

#define COLOR_OUTPUT_BLOCK (((struct kan_rpl_meta_color_output_t *) meta->color_outputs.data)[setting->block])

#define BLEND_FACTOR_VALUES(FIELD)                                                                                     \
    SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (zero), KAN_RPL_BLEND_FACTOR_ZERO, COLOR_OUTPUT_BLOCK.FIELD)      \
    SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (one), KAN_RPL_BLEND_FACTOR_ONE, COLOR_OUTPUT_BLOCK.FIELD)        \
    SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (source_color), KAN_RPL_BLEND_FACTOR_SOURCE_COLOR,                \
                          COLOR_OUTPUT_BLOCK.FIELD)                                                                    \
    SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (one_minus_source_color),                                         \
                          KAN_RPL_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR, COLOR_OUTPUT_BLOCK.FIELD)                       \
    SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (destination_color), KAN_RPL_BLEND_FACTOR_DESTINATION_COLOR,      \
                          COLOR_OUTPUT_BLOCK.FIELD)                                                                    \
    SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (one_minus_destination_color),                                    \
                          KAN_RPL_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR, COLOR_OUTPUT_BLOCK.FIELD)                  \
    SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (source_alpha), KAN_RPL_BLEND_FACTOR_SOURCE_ALPHA,                \
                          COLOR_OUTPUT_BLOCK.FIELD)                                                                    \
    SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (one_minus_source_alpha),                                         \
                          KAN_RPL_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA, COLOR_OUTPUT_BLOCK.FIELD)                       \
    SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (destination_alpha), KAN_RPL_BLEND_FACTOR_DESTINATION_ALPHA,      \
                          COLOR_OUTPUT_BLOCK.FIELD)                                                                    \
    SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (one_minus_destination_alpha),                                    \
                          KAN_RPL_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA, COLOR_OUTPUT_BLOCK.FIELD)                  \
    SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (constant_color), KAN_RPL_BLEND_FACTOR_CONSTANT_COLOR,            \
                          COLOR_OUTPUT_BLOCK.FIELD)                                                                    \
    SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (one_minus_constant_color),                                       \
                          KAN_RPL_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR, COLOR_OUTPUT_BLOCK.FIELD)                     \
    SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (constant_alpha), KAN_RPL_BLEND_FACTOR_CONSTANT_ALPHA,            \
                          COLOR_OUTPUT_BLOCK.FIELD)                                                                    \
    SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (one_minus_constant_alpha),                                       \
                          KAN_RPL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA, COLOR_OUTPUT_BLOCK.FIELD)                     \
    SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (source_alpha_saturate),                                          \
                          KAN_RPL_BLEND_FACTOR_SOURCE_ALPHA_SATURATE, COLOR_OUTPUT_BLOCK.FIELD)                        \
    SETTING_STRING_NO_MORE_VALUES

    if (setting->name == KAN_STATIC_INTERNED_ID_GET (color_output_use_blend))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_BOOLEAN, "flag")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK { COLOR_OUTPUT_BLOCK.use_blend = setting->value.boolean_value; }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (color_output_write_r))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_BOOLEAN, "flag")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK { COLOR_OUTPUT_BLOCK.write_r = setting->value.boolean_value; }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (color_output_write_g))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_BOOLEAN, "flag")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK { COLOR_OUTPUT_BLOCK.write_g = setting->value.boolean_value; }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (color_output_write_b))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_BOOLEAN, "flag")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK { COLOR_OUTPUT_BLOCK.write_b = setting->value.boolean_value; }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (color_output_write_a))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_BOOLEAN, "flag")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK { COLOR_OUTPUT_BLOCK.write_a = setting->value.boolean_value; }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (color_output_source_color_blend_factor))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_STRING, "string")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK { BLEND_FACTOR_VALUES (source_color_blend_factor) }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (color_output_destination_color_blend_factor))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_STRING, "string")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK { BLEND_FACTOR_VALUES (destination_color_blend_factor) }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (color_output_color_blend_operation))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_STRING, "string")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK
        {
            SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (add), KAN_RPL_BLEND_OPERATION_ADD,
                                  COLOR_OUTPUT_BLOCK.color_blend_operation)
            SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (subtract), KAN_RPL_BLEND_OPERATION_SUBTRACT,
                                  COLOR_OUTPUT_BLOCK.color_blend_operation)
            SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (reverse_subtract),
                                  KAN_RPL_BLEND_OPERATION_REVERSE_SUBTRACT, COLOR_OUTPUT_BLOCK.color_blend_operation)
            SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (min), KAN_RPL_BLEND_OPERATION_MIN,
                                  COLOR_OUTPUT_BLOCK.color_blend_operation)
            SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (max), KAN_RPL_BLEND_OPERATION_MAX,
                                  COLOR_OUTPUT_BLOCK.color_blend_operation)
            SETTING_STRING_NO_MORE_VALUES
        }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (color_output_source_alpha_blend_factor))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_STRING, "string")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK { BLEND_FACTOR_VALUES (source_alpha_blend_factor) }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (color_output_destination_alpha_blend_factor))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_STRING, "string")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK { BLEND_FACTOR_VALUES (destination_alpha_blend_factor) }
    }
    else if (setting->name == KAN_STATIC_INTERNED_ID_GET (color_output_alpha_blend_operation))
    {
        SETTING_REQUIRE_TYPE (COMPILE_TIME_EVALUATION_VALUE_TYPE_STRING, "string")
        SETTING_REQUIRE_IN_BLOCK
        SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK
        {
            SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (add), KAN_RPL_BLEND_OPERATION_ADD,
                                  COLOR_OUTPUT_BLOCK.alpha_blend_operation)
            SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (subtract), KAN_RPL_BLEND_OPERATION_SUBTRACT,
                                  COLOR_OUTPUT_BLOCK.alpha_blend_operation)
            SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (reverse_subtract),
                                  KAN_RPL_BLEND_OPERATION_REVERSE_SUBTRACT, COLOR_OUTPUT_BLOCK.alpha_blend_operation)
            SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (min), KAN_RPL_BLEND_OPERATION_MIN,
                                  COLOR_OUTPUT_BLOCK.alpha_blend_operation)
            SETTING_STRING_VALUE (KAN_STATIC_INTERNED_ID_GET (max), KAN_RPL_BLEND_OPERATION_MAX,
                                  COLOR_OUTPUT_BLOCK.alpha_blend_operation)
            SETTING_STRING_NO_MORE_VALUES
        }
    }
    else
    {
        valid = false;
    }

#undef SETTING_REQUIRE_VALID_COLOR_OUTPUT_BLOCK
#undef COLOR_OUTPUT_BLOCK
#undef BLEND_FACTOR_VALUES

    return valid;
}

static bool emit_meta_settings (struct rpl_compiler_instance_t *instance, struct kan_rpl_meta_t *meta)
{
    bool valid = true;
    meta->graphics_classic_settings = kan_rpl_graphics_classic_pipeline_settings_default ();
    struct compiler_instance_setting_node_t *setting = instance->first_setting;

    while (setting)
    {
        bool setting_accepted = emit_meta_check_common_setting (instance, meta, setting);

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
            valid = false;
        }

        setting = setting->next;
    }

    return valid;
}

#undef SETTING_REQUIRE_TYPE
#undef SETTING_STRING_VALUE
#undef SETTING_STRING_NO_MORE_VALUES

static inline bool emit_meta_variable_type_to_meta_type (struct compiler_instance_variable_t *variable,
                                                         enum kan_rpl_meta_variable_type_t *output,
                                                         kan_interned_string_t context_log_name,
                                                         kan_interned_string_t module_name,
                                                         kan_interned_string_t source_name,
                                                         kan_rpl_size_t source_line)
{
    switch (variable->type.class)
    {
    case COMPILER_INSTANCE_TYPE_CLASS_VOID:
    case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
    case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
    case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
    case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
    case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
        // Should not be parameter types. Resolve should fail.
        KAN_ASSERT (false)
        break;

    case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
        *output = variable->type.vector_data->meta_type;
        return true;

    case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
        *output = variable->type.matrix_data->meta_type;
        return true;
    }

    KAN_LOG (rpl_compiler_context, KAN_LOG_ERROR, "[%s:%s:%s:%ld] Unable to find meta type for type \"%s\".",
             context_log_name, module_name, source_name, (long) source_line,
             get_type_name_for_logging (&variable->type))
    return false;
}

static bool emit_meta_gather_parameters_process_field (struct rpl_compiler_instance_t *instance,
                                                       kan_instance_size_t base_offset,
                                                       struct compiler_instance_declaration_node_t *first_declaration,
                                                       struct kan_rpl_meta_buffer_t *meta_output,
                                                       struct kan_trivial_string_buffer_t *name_generation_buffer,
                                                       kan_instance_size_t name_skip_offset,
                                                       bool tail);

static bool emit_meta_gather_parameters_process_field_list (
    struct rpl_compiler_instance_t *instance,
    kan_instance_size_t base_offset,
    struct compiler_instance_declaration_node_t *first_declaration,
    struct kan_rpl_meta_buffer_t *meta_output,
    struct kan_trivial_string_buffer_t *name_generation_buffer,
    kan_instance_size_t name_skip_offset,
    bool tail)
{
    bool valid = true;
    struct compiler_instance_declaration_node_t *field = first_declaration;

    while (field)
    {
        const kan_instance_size_t length = name_generation_buffer->size;
        if (name_generation_buffer->size > name_skip_offset)
        {
            kan_trivial_string_buffer_append_string (name_generation_buffer, ".");
        }

        kan_trivial_string_buffer_append_string (name_generation_buffer, field->variable.name);
        if (!emit_meta_gather_parameters_process_field (instance, base_offset, field, meta_output,
                                                        name_generation_buffer, name_skip_offset, tail))
        {
            valid = false;
        }

        kan_trivial_string_buffer_reset (name_generation_buffer, length);
        field = field->next;
    }

    return valid;
}

static bool emit_meta_gather_parameters_process_field (struct rpl_compiler_instance_t *instance,
                                                       kan_instance_size_t base_offset,
                                                       struct compiler_instance_declaration_node_t *field,
                                                       struct kan_rpl_meta_buffer_t *meta_output,
                                                       struct kan_trivial_string_buffer_t *name_generation_buffer,
                                                       kan_instance_size_t name_skip_offset,
                                                       bool tail)
{
    switch (field->variable.type.class)
    {
    case COMPILER_INSTANCE_TYPE_CLASS_VOID:
    case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
    case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
    case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
    case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
        // Cannot be part of the properly resolved AST.
        KAN_ASSERT (false)
        return false;

    case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
    case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
    {
        if (field->variable.type.array_size_runtime)
        {
            // We do not export parameters from non-structured tails.
            return true;
        }

        bool valid = true;
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
        parameter->name = kan_char_sequence_intern (name_generation_buffer->buffer + name_skip_offset,
                                                    name_generation_buffer->buffer + name_generation_buffer->size);
        parameter->offset = base_offset + field->offset;

        if (!emit_meta_variable_type_to_meta_type (&field->variable, &parameter->type, instance->context_log_name,
                                                   field->module_name, field->source_name, field->source_line))
        {
            valid = false;
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

    case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
        if (field->variable.type.array_size_runtime)
        {
            // Should be guaranteed by resolve stage.
            KAN_ASSERT (!tail)
            meta_output->tail_name = field->variable.name;
            name_skip_offset = name_generation_buffer->size;

            return emit_meta_gather_parameters_process_field_list (
                instance, 0u, field->variable.type.struct_data->first_field, meta_output, name_generation_buffer,
                name_skip_offset, true);
        }
        // Currently we only generate parameters for non-array structs as parameters from arrays of structs sound
        // like a strange and not entirely useful idea.
        else if (field->variable.type.array_dimensions_count == 0u)
        {
            return emit_meta_gather_parameters_process_field_list (
                instance, base_offset + field->offset, field->variable.type.struct_data->first_field, meta_output,
                name_generation_buffer, name_skip_offset, tail);
        }

        return true;
    }

    KAN_ASSERT (false)
    return false;
}

bool kan_rpl_compiler_instance_emit_meta (kan_rpl_compiler_instance_t compiler_instance,
                                          struct kan_rpl_meta_t *meta,
                                          enum kan_rpl_meta_emission_flags_t flags)
{
    kan_static_interned_ids_ensure_initialized ();
    struct rpl_compiler_instance_t *instance = KAN_HANDLE_GET (compiler_instance);
    meta->pipeline_type = instance->pipeline_type;
    bool valid = true;

    struct kan_trivial_string_buffer_t name_generation_buffer;
    kan_trivial_string_buffer_init (&name_generation_buffer, STATICS.rpl_meta_allocation_group,
                                    KAN_RPL_COMPILER_INSTANCE_MAX_FLAT_NAME_LENGTH);

    kan_loop_size_t attribute_sources_count = 0u;
    kan_loop_size_t color_outputs = 0u;
    struct compiler_instance_container_node_t *container = instance->first_container;

    while (container)
    {
        switch (container->type)
        {
        case KAN_RPL_CONTAINER_TYPE_VERTEX_ATTRIBUTE:
        case KAN_RPL_CONTAINER_TYPE_INSTANCED_ATTRIBUTE:
            ++attribute_sources_count;
            break;

        case KAN_RPL_CONTAINER_TYPE_STATE:
            break;

        case KAN_RPL_CONTAINER_TYPE_COLOR_OUTPUT:
            // Not exposed, only affects pipeline settings.
            if (instance->pipeline_type == KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC)
            {
                struct compiler_instance_container_field_node_t *field = container->first_field;
                while (field)
                {
                    ++color_outputs;
                    field = field->next;
                }
            }

            break;
        }

        container = container->next;
    }

    if ((flags & KAN_RPL_META_EMISSION_SKIP_ATTRIBUTE_SOURCES) == 0u)
    {
        kan_dynamic_array_set_capacity (&meta->attribute_sources, attribute_sources_count);
    }

    kan_dynamic_array_set_capacity (&meta->color_outputs, color_outputs);
    for (kan_loop_size_t output_index = 0u; output_index < color_outputs; ++output_index)
    {
        *(struct kan_rpl_meta_color_output_t *) kan_dynamic_array_add_last (&meta->color_outputs) =
            kan_rpl_meta_color_output_default ();
    }

    kan_loop_size_t color_output_index = 0u;
    container = instance->first_container;

    while (container)
    {
        switch (container->type)
        {
        case KAN_RPL_CONTAINER_TYPE_VERTEX_ATTRIBUTE:
        case KAN_RPL_CONTAINER_TYPE_INSTANCED_ATTRIBUTE:
        {
            if ((flags & KAN_RPL_META_EMISSION_SKIP_ATTRIBUTE_SOURCES) == 0u)
            {
                struct kan_rpl_meta_attribute_source_t *attribute_source =
                    kan_dynamic_array_add_last (&meta->attribute_sources);
                KAN_ASSERT (attribute_source)

                kan_rpl_meta_attribute_source_init (attribute_source);
                attribute_source->name = container->name;
                attribute_source->rate = container->type == KAN_RPL_CONTAINER_TYPE_VERTEX_ATTRIBUTE ?
                                             KAN_RPL_META_ATTRIBUTE_SOURCE_RATE_VERTEX :
                                             KAN_RPL_META_ATTRIBUTE_SOURCE_RATE_INSTANCE;
                attribute_source->binding = container->binding_if_input;
                attribute_source->block_size = container->block_size_if_input;

                kan_instance_size_t attribute_count = 0u;
                struct compiler_instance_container_field_node_t *field = container->first_field;

                while (field)
                {
                    ++attribute_count;
                    field = field->next;
                }

                kan_dynamic_array_set_capacity (&attribute_source->attributes, attribute_count);
                field = container->first_field;

                while (field)
                {
                    struct kan_rpl_meta_attribute_t *attribute =
                        kan_dynamic_array_add_last (&attribute_source->attributes);
                    KAN_ASSERT (attribute)

                    kan_rpl_meta_attribute_init (attribute);
                    attribute->name = field->variable.name;
                    attribute->location = field->location;
                    attribute->offset = field->offset_if_input;
                    attribute->item_format = field->input_item_format;

                    switch (field->variable.type.class)
                    {
                    case COMPILER_INSTANCE_TYPE_CLASS_VOID:
                    case COMPILER_INSTANCE_TYPE_CLASS_STRUCT:
                    case COMPILER_INSTANCE_TYPE_CLASS_BOOLEAN:
                    case COMPILER_INSTANCE_TYPE_CLASS_BUFFER:
                    case COMPILER_INSTANCE_TYPE_CLASS_SAMPLER:
                    case COMPILER_INSTANCE_TYPE_CLASS_IMAGE:
                        KAN_ASSERT (false)
                        break;

                    case COMPILER_INSTANCE_TYPE_CLASS_VECTOR:
                        switch (field->variable.type.vector_data->items_count)
                        {
                        case 1u:
                            attribute->class = KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_1;
                            break;

                        case 2u:
                            attribute->class = KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_2;
                            break;

                        case 3u:
                            attribute->class = KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_3;
                            break;

                        case 4u:
                            attribute->class = KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_4;
                            break;

                        default:
                            KAN_ASSERT (false);
                            break;
                        }

                        break;

                    case COMPILER_INSTANCE_TYPE_CLASS_MATRIX:
                        if (field->variable.type.matrix_data->rows == 3u &&
                            field->variable.type.matrix_data->columns == 3u)
                        {
                            attribute->class = KAN_RPL_META_ATTRIBUTE_CLASS_MATRIX_3X3;
                        }
                        else if (field->variable.type.matrix_data->rows == 4u &&
                                 field->variable.type.matrix_data->columns == 4u)
                        {
                            attribute->class = KAN_RPL_META_ATTRIBUTE_CLASS_MATRIX_4X4;
                        }
                        else
                        {
                            KAN_ASSERT (false)
                        }

                        break;
                    }

                    kan_dynamic_array_set_capacity (&attribute->meta, field->meta_count);
                    attribute->meta.size = field->meta_count;

                    if (field->meta_count > 0u)
                    {
                        memcpy (attribute->meta.data, field->meta, sizeof (kan_interned_string_t) * field->meta_count);
                    }

                    field = field->next;
                }
            }

            break;
        }

        case KAN_RPL_CONTAINER_TYPE_STATE:
            break;

        case KAN_RPL_CONTAINER_TYPE_COLOR_OUTPUT:
            // Not exposed, only affects pipeline settings.
            if (instance->pipeline_type == KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC)
            {
                struct compiler_instance_container_field_node_t *field = container->first_field;
                while (field)
                {
                    KAN_ASSERT (field->variable.type.class == COMPILER_INSTANCE_TYPE_CLASS_VECTOR)

                    struct kan_rpl_meta_color_output_t *output =
                        &((struct kan_rpl_meta_color_output_t *) meta->color_outputs.data)[color_output_index];

                    output->components_count = (uint8_t) field->variable.type.vector_data->items_count;
                    ++color_output_index;
                    field = field->next;
                }
            }

            break;
        }

        container = container->next;
    }

    kan_loop_size_t pass_buffer_count = 0u;
    kan_loop_size_t material_buffer_count = 0u;
    kan_loop_size_t object_buffer_count = 0u;
    kan_loop_size_t shared_buffer_count = 0u;
    struct compiler_instance_buffer_node_t *buffer = instance->first_buffer;

    while (buffer)
    {
        switch (buffer->type)
        {
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

            case KAN_RPL_SET_SHARED:
                ++shared_buffer_count;
                break;
            }

            break;

        case KAN_RPL_BUFFER_TYPE_PUSH_CONSTANT:
            // Push constant buffer does not participate in sets, but it is a good place to fetch its size here.
            meta->push_constant_size = buffer->main_size;
            break;
        }

        buffer = buffer->next;
    }

    if ((flags & KAN_RPL_META_EMISSION_SKIP_SETS) == 0u)
    {
        kan_dynamic_array_set_capacity (&meta->set_pass.buffers, pass_buffer_count);
        kan_dynamic_array_set_capacity (&meta->set_material.buffers, material_buffer_count);
        kan_dynamic_array_set_capacity (&meta->set_object.buffers, object_buffer_count);
        kan_dynamic_array_set_capacity (&meta->set_shared.buffers, shared_buffer_count);
    }

    buffer = instance->first_buffer;
    while (buffer)
    {
        struct kan_dynamic_array_t *buffer_array = NULL;
        bool skip = false;

        switch (buffer->type)
        {
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

            case KAN_RPL_SET_SHARED:
                buffer_array = &meta->set_shared.buffers;
                break;
            }

            skip = (flags & KAN_RPL_META_EMISSION_SKIP_SETS) != 0u;
            break;

        case KAN_RPL_BUFFER_TYPE_PUSH_CONSTANT:
            // Push buffers do not participate in sets.
            skip = true;
            break;
        }

        if (skip || !buffer_array)
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

        if (buffer->type == KAN_RPL_BUFFER_TYPE_UNIFORM || buffer->type == KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE)
        {
            if (!emit_meta_gather_parameters_process_field_list (instance, 0u, buffer->first_field, meta_buffer,
                                                                 &name_generation_buffer, 0u, false))
            {
                valid = false;
            }

            kan_dynamic_array_set_capacity (&meta_buffer->main_parameters, meta_buffer->main_parameters.size);
        }

        buffer = buffer->next;
    }

    if ((flags & KAN_RPL_META_EMISSION_SKIP_SETS) == 0u)
    {
        kan_loop_size_t pass_sampler_count = 0u;
        kan_loop_size_t material_sampler_count = 0u;
        kan_loop_size_t object_sampler_count = 0u;
        kan_loop_size_t shared_sampler_count = 0u;
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

            case KAN_RPL_SET_SHARED:
                ++shared_sampler_count;
                break;
            }

            sampler = sampler->next;
        }

        kan_dynamic_array_set_capacity (&meta->set_pass.samplers, pass_sampler_count);
        kan_dynamic_array_set_capacity (&meta->set_material.samplers, material_sampler_count);
        kan_dynamic_array_set_capacity (&meta->set_object.samplers, object_sampler_count);
        kan_dynamic_array_set_capacity (&meta->set_shared.samplers, shared_sampler_count);

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

            case KAN_RPL_SET_SHARED:
                sampler_array = &meta->set_shared.samplers;
                break;
            }

            struct kan_rpl_meta_sampler_t *meta_sampler = kan_dynamic_array_add_last (sampler_array);
            KAN_ASSERT (meta_sampler)

            meta_sampler->name = sampler->name;
            meta_sampler->binding = sampler->binding;
            sampler = sampler->next;
        }
    }

    if ((flags & KAN_RPL_META_EMISSION_SKIP_SETS) == 0u)
    {
        kan_loop_size_t pass_image_count = 0u;
        kan_loop_size_t material_image_count = 0u;
        kan_loop_size_t object_image_count = 0u;
        kan_loop_size_t shared_image_count = 0u;
        struct compiler_instance_image_node_t *image = instance->first_image;

        while (image)
        {
            switch (image->set)
            {
            case KAN_RPL_SET_PASS:
                ++pass_image_count;
                break;

            case KAN_RPL_SET_MATERIAL:
                ++material_image_count;
                break;

            case KAN_RPL_SET_OBJECT:
                ++object_image_count;
                break;

            case KAN_RPL_SET_SHARED:
                ++shared_image_count;
                break;
            }

            image = image->next;
        }

        kan_dynamic_array_set_capacity (&meta->set_pass.images, pass_image_count);
        kan_dynamic_array_set_capacity (&meta->set_material.images, material_image_count);
        kan_dynamic_array_set_capacity (&meta->set_object.images, object_image_count);
        kan_dynamic_array_set_capacity (&meta->set_shared.images, shared_image_count);

        image = instance->first_image;
        while (image)
        {
            struct kan_dynamic_array_t *image_array = NULL;
            switch (image->set)
            {
            case KAN_RPL_SET_PASS:
                image_array = &meta->set_pass.images;
                break;

            case KAN_RPL_SET_MATERIAL:
                image_array = &meta->set_material.images;
                break;

            case KAN_RPL_SET_OBJECT:
                image_array = &meta->set_object.images;
                break;

            case KAN_RPL_SET_SHARED:
                image_array = &meta->set_shared.images;
                break;
            }

            struct kan_rpl_meta_image_t *meta_image = kan_dynamic_array_add_last (image_array);
            KAN_ASSERT (meta_image)

            meta_image->name = image->name;
            meta_image->binding = image->binding;
            meta_image->type = image->type;
            meta_image->image_array_size = image->array_size;
            image = image->next;
        }
    }

    if (!emit_meta_settings (instance, meta))
    {
        valid = false;
    }

    kan_trivial_string_buffer_shutdown (&name_generation_buffer);
    return valid;
}
