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

/// \brief Contains resolved settings for classic graphics pipeline.
struct kan_rpl_graphics_classic_pipeline_settings_t
{
    enum kan_rpl_polygon_mode_t polygon_mode;
    enum kan_rpl_cull_mode_t cull_mode;

    kan_bool_t depth_test;
    kan_bool_t depth_write;

    uint64_t fragment_output_count;
};

static inline struct kan_rpl_graphics_classic_pipeline_settings_t kan_rpl_graphics_classic_pipeline_settings_default (
    void)
{
    return (struct kan_rpl_graphics_classic_pipeline_settings_t) {
        .polygon_mode = KAN_RPL_POLYGON_MODE_FILL,
        .cull_mode = KAN_RPL_CULL_MODE_BACK,
        .depth_test = KAN_TRUE,
        .depth_write = KAN_TRUE,
        .fragment_output_count = 0u,
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

/// \brief Stores information about buffer exposed in metadata.
struct kan_rpl_meta_buffer_t
{
    kan_interned_string_t name;

    /// \brief Binding point index for buffer.
    /// \details Vertex buffer binding for vertex attribute buffers or buffer binding point for other buffers.
    uint64_t binding;

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
    KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_BORDER,
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
    uint64_t binding;
    enum kan_rpl_sampler_type_t type;
    struct kan_rpl_meta_sampler_settings_t settings;
};

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
