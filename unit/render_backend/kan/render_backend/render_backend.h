#pragma once

#include <render_backend_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/container/interned_string.h>
#include <kan/platform/application.h>

KAN_C_HEADER_BEGIN

typedef uint64_t kan_render_context_t;

typedef uint64_t kan_render_device_id_t;

typedef uint64_t kan_render_surface_t;

#define KAN_INVALID_RENDER_SURFACE 0u

typedef uint64_t kan_render_frame_buffer_t;

#define KAN_INVALID_FRAME_BUFFER 0u

typedef uint64_t kan_render_pass_t;

#define KAN_INVALID_RENDER_PASS 0u

typedef uint64_t kan_render_pass_instance_t;

#define KAN_INVALID_RENDER_PASS_INSTANCE 0u

typedef uint64_t kan_render_classic_graphics_pipeline_family_t;

#define KAN_INVALID_RENDER_CLASSIC_GRAPHICS_PIPELINE_FAMILY 0u

typedef uint64_t kan_render_classic_graphics_pipeline_t;

#define KAN_INVALID_RENDER_CLASSIC_GRAPHICS_PIPELINE 0u

typedef uint64_t kan_render_classic_graphics_pipeline_instance_t;

#define KAN_INVALID_RENDER_CLASSIC_GRAPHICS_PIPELINE_INSTANCE 0u

typedef uint64_t kan_render_buffer_t;

#define KAN_INVALID_RENDER_BUFFER 0u

typedef uint64_t kan_render_image_t;

#define KAN_INVALID_RENDER_IMAGE 0u

RENDER_BACKEND_API kan_render_context_t kan_render_context_create (void);

enum kan_render_device_type_t
{
    KAN_RENDER_DEVICE_TYPE_DESCRITE_GPU = 0u,
    KAN_RENDER_DEVICE_TYPE_INTEGRATED_GPU,
    KAN_RENDER_DEVICE_TYPE_VIRTUAL_GPU,
    KAN_RENDER_DEVICE_TYPE_CPU,
    KAN_RENDER_DEVICE_TYPE_UNKNOWN,
};

struct kan_render_supported_device_info_t
{
    kan_render_device_id_t id;
    const char *name;
    enum kan_render_device_type_t device_type;
    kan_bool_t has_unified_memory;
};

struct kan_render_supported_devices_t
{
    uint64_t supported_device_count;
    struct kan_render_supported_device_info_t devices[];
};

RENDER_BACKEND_API struct kan_render_supported_devices_t *kan_render_context_query_devices (
    kan_render_context_t context);

RENDER_BACKEND_API struct kan_render_supported_devices_t *kan_render_context_free_device_query_result (
    kan_render_context_t context, struct kan_render_supported_devices_t *query_result);

RENDER_BACKEND_API void kan_render_context_select_device (kan_render_context_t context, kan_render_device_id_t device);

/// \details Submits recorded commands and presentation from previous frame, prepares data for the new frame.
RENDER_BACKEND_API void kan_render_context_next_frame (kan_render_context_t context);

RENDER_BACKEND_API void kan_render_context_destroy (kan_render_context_t context);

RENDER_BACKEND_API enum kan_platform_window_flag_t kan_render_get_required_window_flags (void);

RENDER_BACKEND_API kan_render_surface_t kan_render_surface_create (kan_render_context_t context,
                                                                   kan_platform_window_id_t window);

RENDER_BACKEND_API void kan_render_surface_destroy (kan_render_surface_t surface);

enum kan_render_pass_output_attachment_type_t
{
    KAN_FRAME_BUFFER_ATTACHMENT_IMAGE = 0u,
    KAN_FRAME_BUFFER_ATTACHMENT_SURFACE,
};

struct kan_render_frame_buffer_attachment_description_t
{
    enum kan_render_pass_output_attachment_type_t type;
    union
    {
        kan_render_image_t image;
        kan_render_surface_t surface;
    };
};

struct kan_render_frame_buffer_description_t
{
    kan_render_pass_t associated_pass;
    uint64_t attachment_count;
    struct kan_render_frame_buffer_attachment_description_t *attachments;
};

RENDER_BACKEND_API kan_render_frame_buffer_t kan_render_frame_buffer_create (
    kan_render_context_t context, struct kan_render_frame_buffer_description_t *description);

RENDER_BACKEND_API void kan_render_frame_buffer_destroy (kan_render_frame_buffer_t buffer);

// TODO: Overview of planned things.
//       Render graph is separated into 2 parts: definition and instances.
//       Graph definition creates render passes and stores their properties.
//       Graph instance is created once for every unique render path.
//       One path with multiple viewports that could point to multiple surfaces is still one render path.
//       Multiple pass instances can be created depending on what is being rendered.
//       For example, I think that every shadow map should be rendered in its pass instance.
//       Every viewport should also be rendered into its own pass instance.
//       That means that dependencies are being set up between pass instances rather that passes.
//       We can even receive the case when two pass instances from the same pass depend one on another.
//       For example, monitor inside 3d scene.
//       Also, it is worth mentioning that there are 2 types of viewports -- surface viewports and transient viewports.
//       Transient viewports (like monitors in game) should only be rendered if they're visible from viewport visibility
//       chain that goes from any surface viewport.

enum kan_render_pass_type_t
{
    KAN_RENDER_PASS_GRAPHICS = 0u,
};

enum kan_render_load_operation_t
{
    KAN_RENDER_LOAD_OPERATION_ANY = 0u,
    KAN_RENDER_LOAD_OPERATION_LOAD,
    KAN_RENDER_LOAD_OPERATION_CLEAR,
};

enum kan_render_store_operation_t
{
    KAN_RENDER_STORE_OPERATION_ANY = 0u,
    KAN_RENDER_STORE_OPERATION_STORE,
    KAN_RENDER_STORE_OPERATION_NONE,
};

struct kan_render_pass_attachment_t
{
    enum kan_render_pass_output_attachment_type_t type;
    uint64_t samples;
    enum kan_render_load_operation_t load_operation;
    enum kan_render_store_operation_t store_operation;
};

struct kan_render_pass_description_t
{
    enum kan_render_pass_type_t type;
    uint64_t attachments_count;
    struct kan_render_pass_attachment_t *attachments;
};

struct kan_render_viewport_bounds_t
{
    float x;
    float y;
    float width;
    float height;
    float depth_min;
    float depth_max;
};

struct kan_render_scissor_t
{
    int64_t offset_x;
    int64_t offset_y;
    uint64_t width;
    uint64_t height;
};

RENDER_BACKEND_API kan_render_pass_t kan_render_pass_create (kan_render_context_t context,
                                                             struct kan_render_pass_description_t *description);

RENDER_BACKEND_API kan_bool_t kan_render_pass_add_static_dependency (kan_render_pass_t pass,
                                                                     kan_render_pass_t dependency);

RENDER_BACKEND_API kan_render_pass_instance_t
kan_render_pass_instantiate (kan_render_pass_t pass,
                             kan_render_frame_buffer_t frame_buffer,
                             struct kan_render_viewport_bounds_t *viewport_bounds,
                             struct kan_render_scissor_t *scissor);

RENDER_BACKEND_API void kan_render_pass_instance_add_dynamic_dependency (kan_render_pass_instance_t pass_instance,
                                                                         kan_render_pass_instance_t dependency);

RENDER_BACKEND_API void kan_render_pass_instance_classic_graphics_pipeline (
    kan_render_pass_instance_t pass_instance, kan_render_classic_graphics_pipeline_instance_t pipeline_instance);

RENDER_BACKEND_API void kan_render_pass_instance_attributes (kan_render_pass_instance_t pass_instance,
                                                             uint64_t start_at_binding,
                                                             uint64_t buffers_count,
                                                             kan_render_buffer_t *buffers);

RENDER_BACKEND_API void kan_render_pass_instance_indices (kan_render_pass_instance_t pass_instance,
                                                          kan_render_buffer_t buffer);

RENDER_BACKEND_API void kan_render_pass_instance_draw (kan_render_pass_instance_t pass_instance,
                                                       uint64_t vertex_offset,
                                                       uint64_t vertex_count);

RENDER_BACKEND_API void kan_render_pass_instance_instanced_draw (kan_render_pass_instance_t pass_instance,
                                                                 uint64_t vertex_offset,
                                                                 uint64_t vertex_count,
                                                                 uint64_t instance_offset,
                                                                 uint64_t instance_count);

RENDER_BACKEND_API void kan_render_pass_destroy (kan_render_pass_t pass);

enum kan_render_stage_t
{
    KAN_RENDER_STAGE_CLASSIC_GRAPHICS_VERTEX = 0u,
    KAN_RENDER_STAGE_CLASSIC_GRAPHICS_FRAGMENT,
};

enum kan_render_attribute_rate_t
{
    KAN_RENDER_ATTRIBUTE_RATE_PER_VERTEX = 0u,
    KAN_RENDER_ATTRIBUTE_RATE_PER_INSTANCE,
};

struct kan_render_attribute_source_description_t
{
    uint64_t binding;
    uint64_t stride;
    enum kan_render_attribute_rate_t rate;
};

enum kan_render_attribute_format_t
{
    KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_1,
    KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_2,
    KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_3,
    KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_4,
    KAN_RENDER_ATTRIBUTE_FORMAT_MATRIX_FLOAT_3_3,
    KAN_RENDER_ATTRIBUTE_FORMAT_MATRIX_FLOAT_4_4,
};

struct kan_render_attribute_description_t
{
    uint64_t binding;
    uint64_t location;
    uint64_t offset;
    enum kan_render_attribute_format_t format;
};

enum kan_render_layout_binding_type_t
{
    KAN_RENDER_LAYOUT_BINDING_TYPE_UNIFORM_BUFFER,
    KAN_RENDER_LAYOUT_BINDING_TYPE_STORAGE_BUFFER,
    KAN_RENDER_LAYOUT_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
};

struct kan_render_layout_binding_description_t
{
    uint64_t binding;
    enum kan_render_layout_binding_type_t type;
    uint64_t used_stage_mask;
};

struct kan_render_layout_description_t
{
    uint64_t set;
    uint64_t bindings_count;
    struct kan_render_layout_binding_description_t *bindings;
    kan_bool_t stable_binding;
};

enum kan_render_classic_graphics_topology_t
{
    KAN_RENDER_CLASSIC_GRAPHICS_TOPOLOGY_TRIANGLE_LIST = 0u,
};

struct kan_render_classic_graphics_pipeline_family_definition_t
{
    enum kan_render_classic_graphics_topology_t topology;

    uint64_t attribute_sources_count;
    struct kan_render_attribute_source_description_t *attribute_sources;

    uint64_t attributes_count;
    struct kan_render_attribute_description_t *attributes;

    uint64_t layouts_count;
    struct kan_render_layout_description_t *layouts;
};

RENDER_BACKEND_API kan_render_classic_graphics_pipeline_family_t kan_render_classic_graphics_pipeline_family_create (
    kan_render_context_t context, struct kan_render_classic_graphics_pipeline_family_definition_t *definition);

RENDER_BACKEND_API void kan_render_classic_graphics_pipeline_family_destroy (
    kan_render_classic_graphics_pipeline_family_t family);

enum kan_render_polygon_mode_t
{
    KAN_RENDER_POLYGON_MODE_FILL,
    KAN_RENDER_POLYGON_MODE_WIREFRAME,
};

enum kan_render_cull_mode_t
{
    KAN_RENDER_CULL_MODE_BACK = 0u,
};

enum kan_render_blend_factor_t
{
    KAN_RENDER_BLEND_FACTOR_ZERO = 0u,
    KAN_RENDER_BLEND_FACTOR_ONE,
    KAN_RENDER_BLEND_FACTOR_SOURCE_COLOR,
    KAN_RENDER_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR,
    KAN_RENDER_BLEND_FACTOR_DESTINATION_COLOR,
    KAN_RENDER_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR,
    KAN_RENDER_BLEND_FACTOR_SOURCE_ALPHA,
    KAN_RENDER_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA,
    KAN_RENDER_BLEND_FACTOR_DESTINATION_ALPHA,
    KAN_RENDER_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA,
    KAN_RENDER_BLEND_FACTOR_CONSTANT_COLOR,
    KAN_RENDER_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
    KAN_RENDER_BLEND_FACTOR_CONSTANT_ALPHA,
    KAN_RENDER_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
    KAN_RENDER_BLEND_FACTOR_SOURCE_ALPHA_SATURATE,
};

enum kan_render_blend_operation_t
{
    KAN_RENDER_BLEND_OPERATION_ADD = 0u,
    KAN_RENDER_BLEND_OPERATION_SUBTRACT,
    KAN_RENDER_BLEND_OPERATION_REVERSE_SUBTRACT,
    KAN_RENDER_BLEND_OPERATION_MIN,
    KAN_RENDER_BLEND_OPERATION_MAX,
};

struct kan_render_color_output_setup_description_t
{
    kan_bool_t use_blend;
    kan_bool_t write_r;
    kan_bool_t write_g;
    kan_bool_t write_b;
    kan_bool_t write_a;
    enum kan_render_blend_factor_t source_color_blend_factor;
    enum kan_render_blend_factor_t destination_color_blend_factor;
    enum kan_render_blend_operation_t color_blend_operation;
    enum kan_render_blend_factor_t source_alpha_blend_factor;
    enum kan_render_blend_factor_t destination_alpha_blend_factor;
    enum kan_render_blend_operation_t alpha_blend_operation;
};

enum kan_render_depth_compare_operation_t
{
    KAN_RENDER_DEPTH_COMPARE_OPERATION_NEVER = 0u,
    KAN_RENDER_DEPTH_COMPARE_OPERATION_ALWAYS,
    KAN_RENDER_DEPTH_COMPARE_OPERATION_EQUAL,
    KAN_RENDER_DEPTH_COMPARE_OPERATION_NOT_EQUAL,
    KAN_RENDER_DEPTH_COMPARE_OPERATION_LESS,
    KAN_RENDER_DEPTH_COMPARE_OPERATION_LESS_OR_EQUAL,
    KAN_RENDER_DEPTH_COMPARE_OPERATION_GREATER,
    KAN_RENDER_DEPTH_COMPARE_OPERATION_GREATER_OR_EQUAL,
};

struct kan_render_pipeline_code_entry_point_t
{
    enum kan_render_stage_t stage;
    kan_interned_string_t function_name;
};

struct kan_render_pipeline_code_module_t
{
    uint64_t code_length;
    void *code;

    uint64_t entry_points_count;
    struct kan_render_pipeline_code_entry_point_t *entry_points;
};

enum kan_render_filter_mode_t
{
    KAN_RENDER_FILTER_MODE_NEAREST = 0u,
    KAN_RENDER_FILTER_MODE_LINEAR,
};

enum kan_render_mip_map_mode_t
{
    KAN_RENDER_MIP_MAP_MODE_NEAREST = 0u,
    KAN_RENDER_MIP_MAP_MODE_LINEAR,
};

enum kan_render_address_mode_t
{
    KAN_RENDER_ADDRESS_MODE_REPEAT = 0u,
    KAN_RENDER_ADDRESS_MODE_MIRRORED_REPEAT,
    KAN_RENDER_ADDRESS_MODE_CLAMP_TO_EDGE,
    KAN_RENDER_ADDRESS_MODE_MIRRORED_CLAMP_TO_EDGE,
    KAN_RENDER_ADDRESS_MODE_CLAMP_TO_BORDER,
    KAN_RENDER_ADDRESS_MODE_MIRRORED_CLAMP_TO_BORDER,
};

struct kan_render_sampler_definition_t
{
    uint64_t layout_set;
    uint64_t layout_binding;
    enum kan_render_filter_mode_t mag_filter;
    enum kan_render_filter_mode_t min_filter;
    enum kan_render_mip_map_mode_t mip_map_mode;
    enum kan_render_address_mode_t address_mode_u;
    enum kan_render_address_mode_t address_mode_v;
    enum kan_render_address_mode_t address_mode_w;
    float border_r;
    float border_g;
    float border_b;
    float border_a;
};

struct kan_render_classic_graphics_pipeline_definition_t
{
    kan_render_pass_t pass;
    kan_render_classic_graphics_pipeline_family_t family;

    enum kan_render_polygon_mode_t polygon_mode;
    enum kan_render_cull_mode_t cull_mode;
    kan_bool_t use_depth_clamp;

    uint64_t *output_setups_count;
    struct kan_render_color_output_setup_description_t *output_setups;
    float blend_constant_r;
    float blend_constant_g;
    float blend_constant_b;
    float blend_constant_a;

    kan_bool_t depth_test_enabled;
    kan_bool_t depth_write_enabled;
    kan_bool_t depth_bounds_test_enabled;
    enum kan_render_depth_compare_operation_t depth_compare_operation;
    float min_depth;
    float max_depth;

    uint64_t code_modules_count;
    struct kan_render_pipeline_code_module_t *code_modules;

    uint64_t samplers_count;
    struct kan_render_sampler_definition_t *samplers;
};

RENDER_BACKEND_API kan_render_classic_graphics_pipeline_t kan_render_classic_graphics_pipeline_create (
    kan_render_context_t context, struct kan_render_classic_graphics_pipeline_definition_t *definition);

RENDER_BACKEND_API void kan_render_classic_graphics_pipeline_destroy (kan_render_classic_graphics_pipeline_t pipeline);

struct kan_render_layout_update_description_t
{
    uint64_t set;
    uint64_t binding;

    kan_render_frame_buffer_t bind_buffer;
    uint64_t buffer_offset;
    uint64_t buffer_range;

    kan_render_image_t bind_image;
};

RENDER_BACKEND_API kan_render_classic_graphics_pipeline_instance_t
kan_render_classic_graphics_pipeline_instance_create (kan_render_context_t context,
                                                      kan_render_classic_graphics_pipeline_t pipeline,
                                                      uint64_t initial_bindings_count,
                                                      struct kan_render_layout_update_description_t *initial_bindings);

RENDER_BACKEND_API void kan_render_classic_graphics_pipeline_instance_update_layout (
    kan_render_classic_graphics_pipeline_instance_t instance,
    uint64_t bindings_count,
    struct kan_render_layout_update_description_t *bindings);

RENDER_BACKEND_API void kan_render_classic_graphics_pipeline_instance_destroy (
    kan_render_classic_graphics_pipeline_instance_t instance);

enum kan_render_buffer_type_t
{
    KAN_RENDER_BUFFER_TYPE_ATTRIBUTE = 0u,
    KAN_RENDER_BUFFER_TYPE_INDEX_16,
    KAN_RENDER_BUFFER_TYPE_INDEX_32,
    KAN_RENDER_BUFFER_TYPE_UNIFORM,
    KAN_RENDER_BUFFER_TYPE_STORAGE,
};

#define KAN_UNIFORM_BUFFER_MAXIMUM_GUARANTEED_SIZE (16u * 1024u)

#define KAN_STORAGE_BUFFER_MAXIMUM_GUARANTEED_SIZE (128u * 1024u * 1024u)

/// \details Optional initial data allows to directly upload initial data to buffer without transfer operation on
///          devices with unified memory. In other cases direct upload with this devices might not be possible
///          due to frames in flights feature.
RENDER_BACKEND_API kan_render_buffer_t kan_render_buffer_create (kan_render_context_t context,
                                                                 enum kan_render_buffer_type_t type,
                                                                 uint64_t full_size,
                                                                 void *optional_initial_data);

RENDER_BACKEND_API void *kan_render_buffer_patch (kan_render_buffer_t buffer,
                                                  uint64_t slice_offset,
                                                  uint64_t slice_size);

RENDER_BACKEND_API void kan_render_buffer_destroy (kan_render_buffer_t buffer);

struct kan_render_allocated_slice_t
{
    kan_render_buffer_t buffer;
    uint64_t slice_offset;
};

typedef uint64_t kan_render_frame_lifetime_buffer_allocator_t;

RENDER_BACKEND_API kan_render_frame_lifetime_buffer_allocator_t kan_render_frame_lifetime_buffer_allocator_create (
    kan_render_context_t context, enum kan_render_buffer_type_t buffer_type, uint64_t page_size);

RENDER_BACKEND_API struct kan_uniform_allocated_slice_t kan_render_frame_lifetime_buffer_allocator_allocate (
    kan_render_frame_lifetime_buffer_allocator_t allocator, uint64_t size, uint64_t alignment);

RENDER_BACKEND_API void kan_render_frame_lifetime_buffer_allocator_destroy (
    kan_render_frame_lifetime_buffer_allocator_t allocator);

// TODO: For future iterations: cube maps and layered images (aka image arrays).

enum kan_render_image_type_t
{
    KAN_RENDER_IMAGE_TYPE_COLOR_2D,
    KAN_RENDER_IMAGE_TYPE_COLOR_3D,
    KAN_RENDER_IMAGE_TYPE_DEPTH_STENCIL,
};

enum kan_render_image_color_format_t
{
    KAN_RENDER_IMAGE_COLOR_FORMAT_RGBA32_SRGB = 0u,
};

struct kan_render_image_description_t
{
    enum kan_render_image_type_t type;
    enum kan_render_image_color_format_t color_format;

    uint64_t width;
    uint64_t height;
    uint64_t depth;
    uint64_t mips;

    kan_bool_t render_target;
};

RENDER_BACKEND_API kan_render_image_t kan_render_image_create (kan_render_context_t context,
                                                               struct kan_render_image_description_t *description);

RENDER_BACKEND_API enum kan_render_image_type_t kan_render_image_get_type (kan_render_image_t image);

RENDER_BACKEND_API void kan_render_image_upload_data (kan_render_image_t image, uint64_t mip, void *data);

RENDER_BACKEND_API void kan_render_image_request_mip_generation (kan_render_image_t image,
                                                                 uint64_t base,
                                                                 uint64_t start,
                                                                 uint64_t end);

RENDER_BACKEND_API void kan_render_image_destroy (kan_render_image_t image);

KAN_C_HEADER_END
