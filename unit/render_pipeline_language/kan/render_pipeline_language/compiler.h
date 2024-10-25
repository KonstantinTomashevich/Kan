#pragma once

#include <render_pipeline_language_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/render_pipeline_language/parser.h>

/// \file
/// \brief Contains declarations for resolve and emit steps of render pipeline language.

KAN_C_HEADER_BEGIN

typedef uint64_t kan_rpl_compiler_context_t;

typedef uint64_t kan_rpl_compiler_instance_t;

#define KAN_INVALID_RPL_COMPILER_INSTANCE 0u

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
    double depth_min;
    double depth_max;

    kan_bool_t stencil_test;
    enum kan_rpl_stencil_operation_t stencil_front_on_fail;
    enum kan_rpl_stencil_operation_t stencil_front_on_depth_fail;
    enum kan_rpl_stencil_operation_t stencil_front_on_pass;
    enum kan_rpl_compare_operation_t stencil_front_compare;
    uint32_t stencil_front_compare_mask;
    uint32_t stencil_front_write_mask;
    uint32_t stencil_front_reference;
    
    enum kan_rpl_stencil_operation_t stencil_back_on_fail;
    enum kan_rpl_stencil_operation_t stencil_back_on_depth_fail;
    enum kan_rpl_stencil_operation_t stencil_back_on_pass;
    enum kan_rpl_compare_operation_t stencil_back_compare;
    uint32_t stencil_back_compare_mask;
    uint32_t stencil_back_write_mask;
    uint32_t stencil_back_reference;
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

/// \brief Stores information about exposed buffer attribute.
struct kan_rpl_meta_attribute_t
{
    uint64_t location;
    uint64_t offset;
    enum kan_rpl_meta_variable_type_t type;
};

/// \brief Stores information about exposed buffer parameter.
struct kan_rpl_meta_parameter_t
{
    kan_interned_string_t name;
    enum kan_rpl_meta_variable_type_t type;
    uint64_t offset;

    /// \brief Total item count -- 1 for single parameter, multiplication of every dimension for arrays.
    uint64_t total_item_count;

    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t meta;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_parameter_init (struct kan_rpl_meta_parameter_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_parameter_shutdown (struct kan_rpl_meta_parameter_t *instance);

// TODO: We really need the ability to specify buffers sets on language level as well as whether binding is stable or
//       no. The main reason is descriptor set allocation optimization. One of the proposed solutions is to add ability
//       to specify settings in buffers. We might as well use named buffer groups instead of direct set indices in
//       settings. As there are 5 predicted buffer groups (material-global parameters that can be very rarely overridden
//       by objects, pass-global parameters that are unique for every pass but cannot be overridden by objects,
//       object-scope parameters that are always unique for every object like model space, object-group-scope parameters
//       that are shared between multiple objects like skeleton joints can be shared between several meshes, instanced
//       parameters that are always pushed from CPU to GPU through instancing buffers). Theoretically, we can even
//       remove stable binding flag from buffer as all groups except instanced parameters should be stable in almost
//       every case.
// TODO: 5 is too much, only 4 is guaranteed to be supported on every hardware. Think about another grouping?

/// \brief Stores information about buffer exposed in metadata.
struct kan_rpl_meta_buffer_t
{
    kan_interned_string_t name;

    /// \brief Descriptor set in which buffer binding should be passed.
    /// \details Currently we are only using sets to separate stable and unstable bindings.
    uint64_t set;

    /// \brief Binding point index for buffer.
    /// \details Vertex buffer binding for vertex attribute buffers or buffer binding point for other buffers.
    uint64_t binding;

    /// \brief If true, binding changes very rarely and therefore can be cached.
    ///        Otherwise, binding is prone to be changed every frame.
    /// \details Currently there is no language feature to specify stable and unstable bindings.
    ///          But we assume that instancing buffers are unstable and all other buffers are stable.
    ///          Nevertheless, it can change in the future, therefore this property was introduced to avoid hardcode.
    kan_bool_t stable_binding;

    /// \brief Buffer type.
    /// \details Stage outputs are not listed in meta buffers.
    enum kan_rpl_buffer_type_t type;

    /// \brief Buffer size.
    /// \details Item size for instanced and vertex attribute buffers and full size for other buffers.
    uint64_t size;

    /// \brief Attributes provided by this buffer, needed for pipeline setup.
    /// \details Only provided for attribute buffers.
    /// \meta reflection_dynamic_array_type = "struct kan_rpl_meta_attribute_t"
    struct kan_dynamic_array_t attributes;

    /// \brief Parameters provided by this buffer, useful for things like materials.
    /// \meta reflection_dynamic_array_type = "struct kan_rpl_meta_parameter_t"
    struct kan_dynamic_array_t parameters;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_buffer_init (struct kan_rpl_meta_buffer_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_buffer_shutdown (struct kan_rpl_meta_buffer_t *instance);

/// \brief Enumerates supported sampler filter modes.
enum kan_rpl_meta_sampler_filter_t
{
    KAN_RPL_META_SAMPLER_FILTER_NEAREST = 0u,
    KAN_RPL_META_SAMPLER_FILTER_LINEAR,
};

/// \brief Enumerates supported sampler mip map modes.
enum kan_rpl_meta_sampler_mip_map_mode_t
{
    KAN_RPL_META_SAMPLER_MIP_MAP_MODE_NEAREST = 0u,
    KAN_RPL_META_SAMPLER_MIP_MAP_MODE_LINEAR,
};

/// \brief Enumerates supported sampler address modes.
enum kan_rpl_meta_sampler_address_mode_t
{
    KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT = 0u,
    KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
};

/// \brief Stores information about sampler settings.
struct kan_rpl_meta_sampler_settings_t
{
    enum kan_rpl_meta_sampler_filter_t mag_filter;
    enum kan_rpl_meta_sampler_filter_t min_filter;
    enum kan_rpl_meta_sampler_mip_map_mode_t mip_map_mode;
    enum kan_rpl_meta_sampler_address_mode_t address_mode_u;
    enum kan_rpl_meta_sampler_address_mode_t address_mode_v;
    enum kan_rpl_meta_sampler_address_mode_t address_mode_w;
};

static inline struct kan_rpl_meta_sampler_settings_t kan_rpl_meta_sampler_settings_default (void)
{
    return (struct kan_rpl_meta_sampler_settings_t) {
        .mag_filter = KAN_RPL_META_SAMPLER_FILTER_NEAREST,
        .min_filter = KAN_RPL_META_SAMPLER_FILTER_NEAREST,
        .mip_map_mode = KAN_RPL_META_SAMPLER_MIP_MAP_MODE_NEAREST,
        .address_mode_u = KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_v = KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_w = KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT,
    };
}

/// \brief Stores information about sampler exposed to metadata.
struct kan_rpl_meta_sampler_t
{
    kan_interned_string_t name;
    uint64_t set;
    uint64_t binding;
    enum kan_rpl_sampler_type_t type;
    struct kan_rpl_meta_sampler_settings_t settings;
};

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

/// \brief Provides full metadata about resolved pipeline.
struct kan_rpl_meta_t
{
    enum kan_rpl_pipeline_type_t pipeline_type;

    union
    {
        /// \meta reflection_visibility_condition_field = "pipeline_type"
        /// \meta reflection_visibility_condition_values = "(int64_t) KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC"
        struct kan_rpl_graphics_classic_pipeline_settings_t graphics_classic_settings;
    };

    /// \meta reflection_dynamic_array_type = "struct kan_rpl_meta_buffer_t"
    struct kan_dynamic_array_t buffers;

    /// \meta reflection_dynamic_array_type = "struct kan_rpl_meta_sampler_t"
    struct kan_dynamic_array_t samplers;

    /// \brief Contains information about pipeline color outputs if any.
    /// \meta reflection_dynamic_array_type = "struct kan_rpl_meta_color_output_t"
    struct kan_dynamic_array_t color_outputs;

    double color_blend_constant_r;
    double color_blend_constant_g;
    double color_blend_constant_b;
    double color_blend_constant_a;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_init (struct kan_rpl_meta_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_shutdown (struct kan_rpl_meta_t *instance);

/// \brief Creates compiler context for gathering options and modules to be resolved.
RENDER_PIPELINE_LANGUAGE_API kan_rpl_compiler_context_t
kan_rpl_compiler_context_create (enum kan_rpl_pipeline_type_t pipeline_type, kan_interned_string_t log_name);

/// \brief Adds given module to resolution scope. Does not copy and does not transfer ownership.
RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_compiler_context_use_module (
    kan_rpl_compiler_context_t compiler_context, struct kan_rpl_intermediate_t *intermediate_reference);

/// \brief Attempts to set flag option value.
RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_compiler_context_set_option_flag (
    kan_rpl_compiler_context_t compiler_context, kan_interned_string_t name, kan_bool_t value);

/// \brief Attempts to set count option value.
RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_compiler_context_set_option_count (
    kan_rpl_compiler_context_t compiler_context, kan_interned_string_t name, uint64_t value);

/// \brief Resolves context with given entry points to provide data for emit step.
/// \details One context can be used for multiple resolves as resolves do not modify the context.
///          Also, resolve with zero entry points is supported for cases when only metadata is needed.
RENDER_PIPELINE_LANGUAGE_API kan_rpl_compiler_instance_t
kan_rpl_compiler_context_resolve (kan_rpl_compiler_context_t compiler_context,
                                  uint64_t entry_point_count,
                                  struct kan_rpl_entry_point_t *entry_points);

/// \brief Emits meta using resolved instance data.
RENDER_PIPELINE_LANGUAGE_API kan_bool_t
kan_rpl_compiler_instance_emit_meta (kan_rpl_compiler_instance_t compiler_instance, struct kan_rpl_meta_t *meta);

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
