#pragma once

#include <render_pipeline_language_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/reflection/markup.h>
#include <kan/render_pipeline_language/parser.h>

/// \file
/// \brief Contains declarations for resolve and emit steps of render pipeline language.

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_rpl_compiler_context_t);
KAN_HANDLE_DEFINE (kan_rpl_compiler_instance_t);

/// \brief Target scope for option setters.
/// \details Makes it possible for user to validate that option belongs to expected scope.
enum kan_rpl_option_target_scope_t
{
    KAN_RPL_OPTION_TARGET_SCOPE_ANY = 0u,
    KAN_RPL_OPTION_TARGET_SCOPE_GLOBAL,
    KAN_RPL_OPTION_TARGET_SCOPE_INSTANCE,
};

/// \brief Defines entry point using its stage and function name.
struct kan_rpl_entry_point_t
{
    enum kan_rpl_pipeline_stage_t stage;
    kan_interned_string_t function_name;
};

/// \brief Enumerates supported polygon rasterization modes.
enum kan_rpl_polygon_mode_t
{
    KAN_RPL_POLYGON_MODE_FILL = 0u,
    KAN_RPL_POLYGON_MODE_WIREFRAME,
};

/// \brief Enumerates supported polygon cull modes.
enum kan_rpl_cull_mode_t
{
    KAN_RPL_CULL_MODE_BACK = 0u,
};

/// \brief Enumerates supported compare operations for depth and stencil tests.
enum kan_rpl_compare_operation_t
{
    KAN_RPL_COMPARE_OPERATION_NEVER = 0u,
    KAN_RPL_COMPARE_OPERATION_ALWAYS,
    KAN_RPL_COMPARE_OPERATION_EQUAL,
    KAN_RPL_COMPARE_OPERATION_NOT_EQUAL,
    KAN_RPL_COMPARE_OPERATION_LESS,
    KAN_RPL_COMPARE_OPERATION_LESS_OR_EQUAL,
    KAN_RPL_COMPARE_OPERATION_GREATER,
    KAN_RPL_COMPARE_OPERATION_GREATER_OR_EQUAL,
};

/// \brief Enumerates supported stencil result operations.
enum kan_rpl_stencil_operation_t
{
    KAN_RPL_STENCIL_OPERATION_KEEP = 0u,
    KAN_RPL_STENCIL_OPERATION_ZERO,
    KAN_RPL_STENCIL_OPERATION_REPLACE,
    KAN_RPL_STENCIL_OPERATION_INCREMENT_AND_CLAMP,
    KAN_RPL_STENCIL_OPERATION_DECREMENT_AND_CLAMP,
    KAN_RPL_STENCIL_OPERATION_INVERT,
    KAN_RPL_STENCIL_OPERATION_INCREMENT_AND_WRAP,
    KAN_RPL_STENCIL_OPERATION_DECREMENT_AND_WRAP,
};

/// \brief Contains resolved settings for classic graphics pipeline.
struct kan_rpl_graphics_classic_pipeline_settings_t
{
    enum kan_rpl_polygon_mode_t polygon_mode;
    enum kan_rpl_cull_mode_t cull_mode;

    kan_bool_t depth_test;
    kan_bool_t depth_write;
    kan_bool_t depth_bounds_test;
    enum kan_rpl_compare_operation_t depth_compare_operation;
    float depth_min;
    float depth_max;

    kan_bool_t stencil_test;
    enum kan_rpl_stencil_operation_t stencil_front_on_fail;
    enum kan_rpl_stencil_operation_t stencil_front_on_depth_fail;
    enum kan_rpl_stencil_operation_t stencil_front_on_pass;
    enum kan_rpl_compare_operation_t stencil_front_compare;
    uint8_t stencil_front_compare_mask;
    uint8_t stencil_front_write_mask;
    uint8_t stencil_front_reference;

    enum kan_rpl_stencil_operation_t stencil_back_on_fail;
    enum kan_rpl_stencil_operation_t stencil_back_on_depth_fail;
    enum kan_rpl_stencil_operation_t stencil_back_on_pass;
    enum kan_rpl_compare_operation_t stencil_back_compare;
    uint8_t stencil_back_compare_mask;
    uint8_t stencil_back_write_mask;
    uint8_t stencil_back_reference;
};

static inline struct kan_rpl_graphics_classic_pipeline_settings_t kan_rpl_graphics_classic_pipeline_settings_default (
    void)
{
    return (struct kan_rpl_graphics_classic_pipeline_settings_t) {
        .polygon_mode = KAN_RPL_POLYGON_MODE_FILL,
        .cull_mode = KAN_RPL_CULL_MODE_BACK,
        .depth_test = KAN_TRUE,
        .depth_write = KAN_TRUE,
        .depth_bounds_test = KAN_FALSE,
        .depth_compare_operation = KAN_RPL_COMPARE_OPERATION_LESS,
        .depth_min = 0.0,
        .depth_max = 1.0,
        .stencil_test = KAN_FALSE,
        .stencil_front_on_fail = KAN_RPL_STENCIL_OPERATION_KEEP,
        .stencil_front_on_depth_fail = KAN_RPL_STENCIL_OPERATION_KEEP,
        .stencil_front_on_pass = KAN_RPL_STENCIL_OPERATION_KEEP,
        .stencil_front_compare = KAN_RPL_COMPARE_OPERATION_NEVER,
        .stencil_front_compare_mask = 0u,
        .stencil_front_write_mask = 0u,
        .stencil_front_reference = 0u,
        .stencil_back_on_fail = KAN_RPL_STENCIL_OPERATION_KEEP,
        .stencil_back_on_depth_fail = KAN_RPL_STENCIL_OPERATION_KEEP,
        .stencil_back_on_pass = KAN_RPL_STENCIL_OPERATION_KEEP,
        .stencil_back_compare = KAN_RPL_COMPARE_OPERATION_NEVER,
        .stencil_back_compare_mask = 0u,
        .stencil_back_write_mask = 0u,
        .stencil_back_reference = 0u,
    };
}

/// \brief Enumerates exposed variables types for metadata.
enum kan_rpl_meta_variable_type_t
{
    KAN_RPL_META_VARIABLE_TYPE_F1 = 0u,
    KAN_RPL_META_VARIABLE_TYPE_F2,
    KAN_RPL_META_VARIABLE_TYPE_F3,
    KAN_RPL_META_VARIABLE_TYPE_F4,
    KAN_RPL_META_VARIABLE_TYPE_I1,
    KAN_RPL_META_VARIABLE_TYPE_I2,
    KAN_RPL_META_VARIABLE_TYPE_I3,
    KAN_RPL_META_VARIABLE_TYPE_I4,
    KAN_RPL_META_VARIABLE_TYPE_F3X3,
    KAN_RPL_META_VARIABLE_TYPE_F4X4,
};

/// \brief Helper for representing `kan_rpl_meta_variable_type_t` as strings in logs.
RENDER_PIPELINE_LANGUAGE_API const char *kan_rpl_meta_variable_type_to_string (enum kan_rpl_meta_variable_type_t type);

/// \brief Stores information about exposed buffer attribute.
struct kan_rpl_meta_attribute_t
{
    kan_rpl_size_t location;
    kan_rpl_size_t offset;
    enum kan_rpl_meta_variable_type_t type;
};

/// \brief Stores information about exposed buffer parameter.
struct kan_rpl_meta_parameter_t
{
    kan_interned_string_t name;
    enum kan_rpl_meta_variable_type_t type;
    kan_rpl_size_t offset;

    /// \brief Total item count -- 1 for single parameter, multiplication of every dimension for arrays.
    kan_rpl_size_t total_item_count;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t meta;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_parameter_init (struct kan_rpl_meta_parameter_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_parameter_init_copy (struct kan_rpl_meta_parameter_t *instance,
                                                                    const struct kan_rpl_meta_parameter_t *copy_from);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_parameter_shutdown (struct kan_rpl_meta_parameter_t *instance);

/// \brief Stores information about buffer exposed in metadata.
struct kan_rpl_meta_buffer_t
{
    kan_interned_string_t name;

    /// \brief Binding point index for buffer.
    /// \details Vertex buffer binding for vertex attribute buffers or buffer binding point for other buffers.
    kan_rpl_size_t binding;

    /// \brief Buffer type.
    /// \details Stage outputs are not listed in meta buffers.
    enum kan_rpl_buffer_type_t type;

    /// \brief Buffer main part size (without runtime sized array tail).
    kan_rpl_size_t main_size;

    /// \brief Size of a tail item of runtime sized array if any (if none, then zero).
    kan_rpl_size_t tail_item_size;

    /// \brief Attributes provided by this buffer, needed for pipeline setup.
    /// \details Only provided for attribute buffers.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_meta_attribute_t)
    struct kan_dynamic_array_t attributes;

    /// \brief Parameters provided by this buffer main part, useful for things like materials.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_meta_parameter_t)
    struct kan_dynamic_array_t main_parameters;

    /// \brief Name of the buffer tail if it has any.
    kan_interned_string_t tail_name;

    /// \brief Parameters for every tail item in coordinates local to tail item, useful for things like materials.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_meta_parameter_t)
    struct kan_dynamic_array_t tail_item_parameters;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_buffer_init (struct kan_rpl_meta_buffer_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_buffer_init_copy (struct kan_rpl_meta_buffer_t *instance,
                                                                 const struct kan_rpl_meta_buffer_t *copy_from);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_buffer_shutdown (struct kan_rpl_meta_buffer_t *instance);

/// \brief Stores information about sampler exposed to metadata.
struct kan_rpl_meta_sampler_t
{
    kan_interned_string_t name;
    kan_rpl_size_t binding;
    enum kan_rpl_sampler_type_t type;
};

/// \brief Stores information about buffer and sampler bindings for concrete descriptor set.
struct kan_rpl_meta_set_bindings_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_meta_buffer_t)
    struct kan_dynamic_array_t buffers;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_meta_sampler_t)
    struct kan_dynamic_array_t samplers;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_set_bindings_init (struct kan_rpl_meta_set_bindings_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_set_bindings_init_copy (
    struct kan_rpl_meta_set_bindings_t *instance, const struct kan_rpl_meta_set_bindings_t *copy_from);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_set_bindings_shutdown (struct kan_rpl_meta_set_bindings_t *instance);

/// \brief Enumerates supported blend factor values.
enum kan_rpl_blend_factor_t
{
    KAN_RPL_BLEND_FACTOR_ZERO = 0u,
    KAN_RPL_BLEND_FACTOR_ONE,
    KAN_RPL_BLEND_FACTOR_SOURCE_COLOR,
    KAN_RPL_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR,
    KAN_RPL_BLEND_FACTOR_DESTINATION_COLOR,
    KAN_RPL_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR,
    KAN_RPL_BLEND_FACTOR_SOURCE_ALPHA,
    KAN_RPL_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA,
    KAN_RPL_BLEND_FACTOR_DESTINATION_ALPHA,
    KAN_RPL_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA,
    KAN_RPL_BLEND_FACTOR_CONSTANT_COLOR,
    KAN_RPL_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
    KAN_RPL_BLEND_FACTOR_CONSTANT_ALPHA,
    KAN_RPL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
    KAN_RPL_BLEND_FACTOR_SOURCE_ALPHA_SATURATE,
};

/// \brief Enumerates supported blend operations.
enum kan_rpl_blend_operation_t
{
    KAN_RPL_BLEND_OPERATION_ADD = 0u,
    KAN_RPL_BLEND_OPERATION_SUBTRACT,
    KAN_RPL_BLEND_OPERATION_REVERSE_SUBTRACT,
    KAN_RPL_BLEND_OPERATION_MIN,
    KAN_RPL_BLEND_OPERATION_MAX,
};

/// \brief Contains configuration for single color output of the pipeline.
struct kan_rpl_meta_color_output_t
{
    uint8_t components_count;
    kan_bool_t use_blend;
    kan_bool_t write_r;
    kan_bool_t write_g;
    kan_bool_t write_b;
    kan_bool_t write_a;
    enum kan_rpl_blend_factor_t source_color_blend_factor;
    enum kan_rpl_blend_factor_t destination_color_blend_factor;
    enum kan_rpl_blend_operation_t color_blend_operation;
    enum kan_rpl_blend_factor_t source_alpha_blend_factor;
    enum kan_rpl_blend_factor_t destination_alpha_blend_factor;
    enum kan_rpl_blend_operation_t alpha_blend_operation;
};

static inline struct kan_rpl_meta_color_output_t kan_rpl_meta_color_output_default (void)
{
    return (struct kan_rpl_meta_color_output_t) {
        .components_count = 4u,
        .use_blend = KAN_FALSE,
        .write_r = KAN_TRUE,
        .write_g = KAN_TRUE,
        .write_b = KAN_TRUE,
        .write_a = KAN_TRUE,
        .source_color_blend_factor = KAN_RPL_BLEND_FACTOR_ONE,
        .destination_color_blend_factor = KAN_RPL_BLEND_FACTOR_ZERO,
        .color_blend_operation = KAN_RPL_BLEND_OPERATION_ADD,
        .source_alpha_blend_factor = KAN_RPL_BLEND_FACTOR_ONE,
        .destination_alpha_blend_factor = KAN_RPL_BLEND_FACTOR_ZERO,
        .alpha_blend_operation = KAN_RPL_BLEND_OPERATION_ADD,
    };
}

/// \brief Contains constants for color blending operations.
struct kan_rpl_color_blend_constants_t
{
    float r;
    float g;
    float b;
    float a;
};

/// \brief Provides full metadata about resolved pipeline.
struct kan_rpl_meta_t
{
    enum kan_rpl_pipeline_type_t pipeline_type;

    union
    {
        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (pipeline_type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC)
        struct kan_rpl_graphics_classic_pipeline_settings_t graphics_classic_settings;
    };

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_meta_buffer_t)
    struct kan_dynamic_array_t attribute_buffers;

    struct kan_rpl_meta_set_bindings_t set_pass;
    struct kan_rpl_meta_set_bindings_t set_material;
    struct kan_rpl_meta_set_bindings_t set_object;
    struct kan_rpl_meta_set_bindings_t set_unstable;

    /// \brief Contains information about pipeline color outputs if any.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_meta_color_output_t)
    struct kan_dynamic_array_t color_outputs;

    struct kan_rpl_color_blend_constants_t color_blend_constants;
};

/// \brief Meta emission flags that make it possible to skip generation of some parts of meta.
KAN_REFLECTION_FLAGS
enum kan_rpl_meta_emission_flags_t
{
    /// \brief Constant that indicates that nothing is skipped.
    KAN_RPL_META_EMISSION_FULL = 0u,

    /// \brief Flag that tells compiler to skip generation of attribute buffers meta.
    KAN_RPL_META_EMISSION_SKIP_ATTRIBUTE_BUFFERS = 1u << 0u,

    /// \brief Flags that tells compiler to skip generation of parameter sets meta.
    KAN_RPL_META_EMISSION_SKIP_SETS = 1u << 1u,
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_init (struct kan_rpl_meta_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_init_copy (struct kan_rpl_meta_t *instance,
                                                          const struct kan_rpl_meta_t *copy_from);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_shutdown (struct kan_rpl_meta_t *instance);

/// \brief Creates compiler context for gathering options and modules to be resolved.
RENDER_PIPELINE_LANGUAGE_API kan_rpl_compiler_context_t
kan_rpl_compiler_context_create (enum kan_rpl_pipeline_type_t pipeline_type, kan_interned_string_t log_name);

/// \brief Adds given module to resolution scope. Does not copy and does not transfer ownership.
RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_compiler_context_use_module (
    kan_rpl_compiler_context_t compiler_context, const struct kan_rpl_intermediate_t *intermediate_reference);

/// \brief Attempts to set flag option value.
/// \details `only_instance_scope` additionally restricts operation to only instanced options, which can be required in
///          some contexts in order to process option application with due validation.
RENDER_PIPELINE_LANGUAGE_API kan_bool_t
kan_rpl_compiler_context_set_option_flag (kan_rpl_compiler_context_t compiler_context,
                                          enum kan_rpl_option_target_scope_t target_scope,
                                          kan_interned_string_t name,
                                          kan_bool_t value);

/// \brief Attempts to set count option value.
/// \details `only_instance_scope` additionally restricts operation to only instanced options, which can be required in
///          some contexts in order to process option application with due validation.
RENDER_PIPELINE_LANGUAGE_API kan_bool_t
kan_rpl_compiler_context_set_option_count (kan_rpl_compiler_context_t compiler_context,
                                           enum kan_rpl_option_target_scope_t target_scope,
                                           kan_interned_string_t name,
                                           kan_rpl_unsigned_int_literal_t value);

/// \brief Resolves context with given entry points to provide data for emit step.
/// \details One context can be used for multiple resolves as resolves do not modify the context.
///          Also, resolve with zero entry points is supported for cases when only metadata is needed.
RENDER_PIPELINE_LANGUAGE_API kan_rpl_compiler_instance_t
kan_rpl_compiler_context_resolve (kan_rpl_compiler_context_t compiler_context,
                                  kan_instance_size_t entry_point_count,
                                  struct kan_rpl_entry_point_t *entry_points);

/// \brief Emits meta using resolved instance data.
RENDER_PIPELINE_LANGUAGE_API kan_bool_t
kan_rpl_compiler_instance_emit_meta (kan_rpl_compiler_instance_t compiler_instance,
                                     struct kan_rpl_meta_t *meta,
                                     enum kan_rpl_meta_emission_flags_t flags);

/// \brief Emits SPIRV 1.3 bytecode using resolved instance data.
/// \invariant Given output dynamic array must not be initialized.
///            It will be initialized by emit logic with proper item size and alignment.
RENDER_PIPELINE_LANGUAGE_API kan_bool_t
kan_rpl_compiler_instance_emit_spirv (kan_rpl_compiler_instance_t compiler_instance,
                                      struct kan_dynamic_array_t *output,
                                      kan_allocation_group_t output_allocation_group);

/// \brief Destroys resolved instance data.
RENDER_PIPELINE_LANGUAGE_API void kan_rpl_compiler_instance_destroy (kan_rpl_compiler_instance_t compiler_instance);

/// \brief Destroys resolution context.
RENDER_PIPELINE_LANGUAGE_API void kan_rpl_compiler_context_destroy (kan_rpl_compiler_context_t compiler_context);

KAN_C_HEADER_END
