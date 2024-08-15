#pragma once

#include <render_pipeline_language_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/render_pipeline_language/parser.h>

KAN_C_HEADER_BEGIN

enum kan_rpl_polygon_mode_t
{
    KAN_RPL_POLYGON_MODE_FILL = 0u,
    KAN_RPL_POLYGON_MODE_WIREFRAME,
};

enum kan_rpl_cull_mode_t
{
    KAN_RPL_CULL_MODE_BACK = 0u,
};

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

struct kan_rpl_meta_attribute_t
{
    uint64_t location;
    uint64_t offset;
    enum kan_rpl_meta_variable_type_t type;
};

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

struct kan_rpl_meta_buffer_t
{
    kan_interned_string_t name;

    /// \details Vertex buffer binding for vertex attribute buffers or buffer binding point for other buffers.
    uint64_t binding;

    /// \details Stage outputs are not listed in meta buffers.
    enum kan_rpl_buffer_type_t type;

    /// \details Item size for instanced and vertex attribute buffers and full size for other buffers.
    uint64_t size;

    /// \details Only provided for attribute buffers.
    /// \meta reflection_dynamic_array_type = "struct kan_rpl_meta_attribute_t"
    struct kan_dynamic_array_t attributes;

    /// \details Information about buffer parameters that should be provided by user (for example, through materials).
    /// \meta reflection_dynamic_array_type = "struct kan_rpl_meta_parameter_t"
    struct kan_dynamic_array_t parameters;
};

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_buffer_init (struct kan_rpl_meta_buffer_t *instance);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_meta_buffer_shutdown (struct kan_rpl_meta_buffer_t *instance);

enum kan_rpl_meta_sampler_filter_t
{
    KAN_RPL_META_SAMPLER_FILTER_NEAREST = 0u,
    KAN_RPL_META_SAMPLER_FILTER_LINEAR,
};

enum kan_rpl_meta_sampler_mip_map_mode_t
{
    KAN_RPL_META_SAMPLER_MIP_MAP_MODE_NEAREST = 0u,
    KAN_RPL_META_SAMPLER_MIP_MAP_MODE_LINEAR,
};

enum kan_rpl_meta_sampler_address_mode_t
{
    KAN_RPL_META_SAMPLER_ADDRESS_MODE_REPEAT = 0u,
    KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    KAN_RPL_META_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
    KAN_RPL_META_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_BORDER,
};

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

struct kan_rpl_meta_sampler_t
{
    kan_interned_string_t name;
    uint64_t binding;
    enum kan_rpl_sampler_type_t type;
    struct kan_rpl_meta_sampler_settings_t settings;
};

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

typedef uint64_t kan_rpl_emitter_t;

RENDER_PIPELINE_LANGUAGE_API kan_rpl_emitter_t kan_rpl_emitter_create (kan_interned_string_t log_name,
                                                                       enum kan_rpl_pipeline_type_t pipeline_type,
                                                                       struct kan_rpl_intermediate_t *intermediate);

RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_emitter_set_flag_option (kan_rpl_emitter_t emitter,
                                                                         kan_interned_string_t name,
                                                                         kan_bool_t value);

RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_emitter_set_count_option (kan_rpl_emitter_t emitter,
                                                                          kan_interned_string_t name,
                                                                          uint64_t value);

RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_emitter_validate (kan_rpl_emitter_t emitter);

RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_emitter_emit_meta (kan_rpl_emitter_t emitter,
                                                                   struct kan_rpl_meta_t *meta_output);

RENDER_PIPELINE_LANGUAGE_API kan_bool_t kan_rpl_emitter_emit_code_spirv (kan_rpl_emitter_t emitter,
                                                                         kan_interned_string_t entry_function_name,
                                                                         enum kan_rpl_pipeline_stage_t stage,
                                                                         struct kan_dynamic_array_t *code_output);

RENDER_PIPELINE_LANGUAGE_API void kan_rpl_emitter_destroy (kan_rpl_emitter_t emitter);

KAN_C_HEADER_END
