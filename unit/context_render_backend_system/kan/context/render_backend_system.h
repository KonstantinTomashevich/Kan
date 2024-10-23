#pragma once

#include <context_render_backend_system_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/container/interned_string.h>
#include <kan/context/application_system.h>
#include <kan/context/context.h>

KAN_C_HEADER_BEGIN

typedef uint64_t kan_render_context_t;

#define KAN_INVALID_RENDER_CONTEXT 0u

typedef uint64_t kan_render_device_id_t;

typedef uint64_t kan_render_surface_t;

#define KAN_INVALID_RENDER_SURFACE 0u

typedef uint64_t kan_render_frame_buffer_t;

#define KAN_INVALID_FRAME_BUFFER 0u

typedef uint64_t kan_render_pass_t;

#define KAN_INVALID_RENDER_PASS 0u

typedef uint64_t kan_render_pass_instance_t;

#define KAN_INVALID_RENDER_PASS_INSTANCE 0u

typedef uint64_t kan_render_graphics_pipeline_family_t;

#define KAN_INVALID_RENDER_GRAPHICS_PIPELINE_FAMILY 0u

typedef uint64_t kan_render_graphics_pipeline_t;

#define KAN_INVALID_RENDER_GRAPHICS_PIPELINE 0u

typedef uint64_t kan_render_pipeline_parameter_set_t;

#define KAN_INVALID_RENDER_PIPELINE_PARAMETER_SET 0u

typedef uint64_t kan_render_buffer_t;

#define KAN_INVALID_RENDER_BUFFER 0u

typedef uint64_t kan_render_image_t;

#define KAN_INVALID_RENDER_IMAGE 0u

/// \brief System name for requirements and queries.
#define KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME "render_backend_system_t"

/// \brief Contains render backend system configuration data.
struct kan_render_backend_system_config_t
{
    kan_bool_t disable_render;
    kan_bool_t prefer_vsync;

    kan_interned_string_t application_info_name;
    uint64_t version_major;
    uint64_t version_minor;
    uint64_t version_patch;
};

enum kan_render_device_type_t
{
    KAN_RENDER_DEVICE_TYPE_DISCRETE_GPU = 0u,
    KAN_RENDER_DEVICE_TYPE_INTEGRATED_GPU,
    KAN_RENDER_DEVICE_TYPE_VIRTUAL_GPU,
    KAN_RENDER_DEVICE_TYPE_CPU,
    KAN_RENDER_DEVICE_TYPE_UNKNOWN,
};

enum kan_render_device_memory_type_t
{
    KAN_RENDER_DEVICE_MEMORY_TYPE_SEPARATE = 0u,
    KAN_RENDER_DEVICE_MEMORY_TYPE_UNIFIED,
    KAN_RENDER_DEVICE_MEMORY_TYPE_UNIFIED_COHERENT,
};

struct kan_render_supported_device_info_t
{
    kan_render_device_id_t id;
    const char *name;
    enum kan_render_device_type_t device_type;
    enum kan_render_device_memory_type_t memory_type;
};

struct kan_render_supported_devices_t
{
    uint64_t supported_device_count;
    struct kan_render_supported_device_info_t devices[];
};

CONTEXT_RENDER_BACKEND_SYSTEM_API struct kan_render_supported_devices_t *kan_render_backend_system_get_devices (
    kan_context_system_handle_t render_backend_system);

CONTEXT_RENDER_BACKEND_SYSTEM_API kan_bool_t kan_render_backend_system_select_device (
    kan_context_system_handle_t render_backend_system, kan_render_device_id_t device);

CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_context_t
kan_render_backend_system_get_render_context (kan_context_system_handle_t render_backend_system);

// TODO: Integrate with cpu profiler sections, especially kan_render_backend_system_next_frame.

/// \details Submits recorded commands and presentation from previous frame, prepares data for the new frame.
/// \return True if next frame submit should be started, false otherwise. For example, we might not be able to submit
///         new frame while using frames in flights when GPU is not fast enough to process all the frames.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_bool_t
kan_render_backend_system_next_frame (kan_context_system_handle_t render_backend_system);

struct kan_render_integer_region_t
{
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
};

// TODO: Add API for making screenshots. Possibly something for recording screen (useful for crashes and bug reports)?

CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_surface_t
kan_render_backend_system_create_surface (kan_context_system_handle_t render_backend_system,
                                          kan_application_system_window_handle_t window,
                                          kan_interned_string_t tracking_name);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_backend_system_present_image_on_surface (
    kan_render_surface_t surface,
    kan_render_image_t image,
    struct kan_render_integer_region_t image_region,
    struct kan_render_integer_region_t surface_region);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_backend_system_destroy_surface (
    kan_context_system_handle_t render_backend_system, kan_render_surface_t surface);

CONTEXT_RENDER_BACKEND_SYSTEM_API enum kan_platform_window_flag_t kan_render_get_required_window_flags (void);

enum kan_render_frame_buffer_attachment_type_t
{
    KAN_FRAME_BUFFER_ATTACHMENT_IMAGE = 0u,
    KAN_FRAME_BUFFER_ATTACHMENT_SURFACE,
};

struct kan_render_frame_buffer_attachment_description_t
{
    enum kan_render_frame_buffer_attachment_type_t type;
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
    kan_interned_string_t tracking_name;
};

CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_frame_buffer_t kan_render_frame_buffer_create (
    kan_render_context_t context, struct kan_render_frame_buffer_description_t *description);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_frame_buffer_destroy (kan_render_frame_buffer_t buffer);

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

enum kan_render_pass_attachment_type_t
{
    KAN_RENDER_PASS_ATTACHMENT_COLOR = 0u,
    KAN_RENDER_PASS_ATTACHMENT_DEPTH_STENCIL,
};

enum kan_render_color_format_t
{
    KAN_RENDER_COLOR_FORMAT_RGBA32_SRGB = 0u,
    KAN_RENDER_COLOR_FORMAT_RGBA128_SFLOAT,

    // TODO: 1-2-3 component formats, needed for things like deferred rendering.

    /// \brief Special value that indicates that color format which is currently used for surfaces should be used here.
    KAN_RENDER_COLOR_FORMAT_SURFACE,
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
    enum kan_render_pass_attachment_type_t type;
    enum kan_render_color_format_t color_format;
    uint64_t samples;
    enum kan_render_load_operation_t load_operation;
    enum kan_render_store_operation_t store_operation;
};

struct kan_render_pass_description_t
{
    enum kan_render_pass_type_t type;
    uint64_t attachments_count;
    struct kan_render_pass_attachment_t *attachments;
    kan_interned_string_t tracking_name;
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

CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_pass_t
kan_render_pass_create (kan_render_context_t context, struct kan_render_pass_description_t *description);

CONTEXT_RENDER_BACKEND_SYSTEM_API kan_bool_t kan_render_pass_add_static_dependency (kan_render_pass_t pass,
                                                                                    kan_render_pass_t dependency);

CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_pass_instance_t
kan_render_pass_instantiate (kan_render_pass_t pass,
                             kan_render_frame_buffer_t frame_buffer,
                             struct kan_render_viewport_bounds_t *viewport_bounds,
                             struct kan_render_integer_region_t *scissor);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pass_instance_add_dynamic_dependency (
    kan_render_pass_instance_t pass_instance, kan_render_pass_instance_t dependency);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pass_instance_graphics_pipeline (
    kan_render_pass_instance_t pass_instance, kan_render_pipeline_parameter_set_t *parameter_sets);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pass_instance_attributes (kan_render_pass_instance_t pass_instance,
                                                                            uint64_t start_at_binding,
                                                                            uint64_t buffers_count,
                                                                            kan_render_buffer_t *buffers);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pass_instance_indices (kan_render_pass_instance_t pass_instance,
                                                                         kan_render_buffer_t buffer);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pass_instance_draw (kan_render_pass_instance_t pass_instance,
                                                                      uint64_t vertex_offset,
                                                                      uint64_t vertex_count);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pass_instance_instanced_draw (
    kan_render_pass_instance_t pass_instance,
    uint64_t vertex_offset,
    uint64_t vertex_count,
    uint64_t instance_offset,
    uint64_t instance_count);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pass_destroy (kan_render_pass_t pass);

enum kan_render_pipeline_type_t
{
    KAN_RENDER_PIPELINE_TYPE_GRAPHICS,
};

enum kan_render_stage_t
{
    KAN_RENDER_STAGE_GRAPHICS_VERTEX = 0u,
    KAN_RENDER_STAGE_GRAPHICS_FRAGMENT,
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
    KAN_RENDER_LAYOUT_BINDING_TYPE_UNIFORM_BUFFER = 0u,
    KAN_RENDER_LAYOUT_BINDING_TYPE_STORAGE_BUFFER,
    KAN_RENDER_LAYOUT_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
};

struct kan_render_layout_binding_description_t
{
    uint64_t binding;
    enum kan_render_layout_binding_type_t type;
    uint64_t used_stage_mask;
};

struct kan_render_parameter_set_description_t
{
    uint64_t set;
    uint64_t bindings_count;
    struct kan_render_layout_binding_description_t *bindings;
    kan_bool_t stable_binding;
};

enum kan_render_graphics_topology_t
{
    KAN_RENDER_GRAPHICS_TOPOLOGY_TRIANGLE_LIST = 0u,
};

struct kan_render_graphics_pipeline_family_description_t
{
    enum kan_render_graphics_topology_t topology;

    uint64_t attribute_sources_count;
    struct kan_render_attribute_source_description_t *attribute_sources;

    uint64_t attributes_count;
    struct kan_render_attribute_description_t *attributes;

    uint64_t parameter_sets_count;
    struct kan_render_parameter_set_description_t *parameter_sets;

    kan_interned_string_t tracking_name;
};

CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_graphics_pipeline_family_t kan_render_graphics_pipeline_family_create (
    kan_render_context_t context, struct kan_render_graphics_pipeline_family_description_t *description);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_graphics_pipeline_family_destroy (
    kan_render_graphics_pipeline_family_t family);

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
};

struct kan_render_sampler_description_t
{
    uint64_t layout_set;
    uint64_t layout_binding;
    enum kan_render_filter_mode_t mag_filter;
    enum kan_render_filter_mode_t min_filter;
    enum kan_render_mip_map_mode_t mip_map_mode;
    enum kan_render_address_mode_t address_mode_u;
    enum kan_render_address_mode_t address_mode_v;
    enum kan_render_address_mode_t address_mode_w;
};

struct kan_render_graphics_pipeline_description_t
{
    kan_render_pass_t pass;
    kan_render_graphics_pipeline_family_t family;

    enum kan_render_polygon_mode_t polygon_mode;
    enum kan_render_cull_mode_t cull_mode;
    kan_bool_t use_depth_clamp;

    uint64_t output_setups_count;
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

    // TODO: Stencil test, it is useful.

    uint64_t code_modules_count;
    struct kan_render_pipeline_code_module_t *code_modules;

    uint64_t samplers_count;
    struct kan_render_sampler_description_t *samplers;

    kan_interned_string_t tracking_name;
};

/// \details Some backends need to compile pipelines for optimization. It might take considerable time (10+ms),
///          therefore it should be done in non-blocking manner on separate thread. Priority manages which pipelines
///          are compiled first. Pipeline is allowed to be used in any method calls even when it is not compiled, but
///          objects that are using non-compiled pipeline will not be rendered until pipeline is compiled.
enum kan_render_pipeline_compilation_priority_t
{
    /// \brief Priority for pipelines that should be ready right away even if it causes hitches in gameplay.
    /// \details If critical pipeline is not ready by the time it is submitted, it will be compiled during submit,
    ///          which can result in noticeable frame time hitch.
    KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CRITICAL = 0u,

    /// \brief Priority for pipelines that should be compiled as soon as possible because they are used in game world.
    KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_ACTIVE,

    /// \brief Priority for pipelines that are compiled ahead of time and are not yet used by anything in the game.
    KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CACHE,
};

CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_graphics_pipeline_t
kan_render_graphics_pipeline_create (kan_render_context_t context,
                                     struct kan_render_graphics_pipeline_description_t *description,
                                     enum kan_render_pipeline_compilation_priority_t compilation_priority);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_graphics_pipeline_change_compilation_priority (
    kan_render_graphics_pipeline_t pipeline, enum kan_render_pipeline_compilation_priority_t compilation_priority);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_graphics_pipeline_destroy (kan_render_graphics_pipeline_t pipeline);

struct kan_render_pipeline_parameter_set_description_t
{
    enum kan_render_pipeline_type_t pipeline_type;
    union
    {
        kan_render_graphics_pipeline_family_t graphics_pipeline;
    };

    uint64_t set;
    kan_interned_string_t tracking_name;

    uint64_t initial_bindings_count;
    struct kan_render_parameter_update_description_t *initial_bindings;
};

struct kan_render_parameter_update_description_buffer_t
{
    kan_render_frame_buffer_t buffer;
    uint64_t offset;
    uint64_t range;
};

struct kan_render_parameter_update_description_image_t
{
    kan_render_image_t image;
};

struct kan_render_parameter_update_description_t
{
    uint64_t binding;
    union
    {
        struct kan_render_parameter_update_description_buffer_t buffer_binding;
        struct kan_render_parameter_update_description_image_t image_binding;
    };
};

CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_pipeline_parameter_set_t kan_render_pipeline_parameter_set_create (
    kan_render_context_t context, struct kan_render_pipeline_parameter_set_description_t *description);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pipeline_parameter_set_update (
    kan_render_pipeline_parameter_set_t set,
    uint64_t bindings_count,
    struct kan_render_parameter_update_description_t *bindings);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pipeline_parameter_set_destroy (
    kan_render_pipeline_parameter_set_t set);

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
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_buffer_t kan_render_buffer_create (kan_render_context_t context,
                                                                                enum kan_render_buffer_type_t type,
                                                                                uint64_t full_size,
                                                                                void *optional_initial_data,
                                                                                kan_interned_string_t tracking_name);

CONTEXT_RENDER_BACKEND_SYSTEM_API void *kan_render_buffer_patch (kan_render_buffer_t buffer,
                                                                 uint64_t slice_offset,
                                                                 uint64_t slice_size);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_buffer_destroy (kan_render_buffer_t buffer);

struct kan_render_allocated_slice_t
{
    kan_render_buffer_t buffer;
    uint64_t slice_offset;
};

typedef uint64_t kan_render_frame_lifetime_buffer_allocator_t;

CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_frame_lifetime_buffer_allocator_t
kan_render_frame_lifetime_buffer_allocator_create (kan_render_context_t context,
                                                   enum kan_render_buffer_type_t buffer_type,
                                                   uint64_t page_size,
                                                   kan_interned_string_t tracking_name);

CONTEXT_RENDER_BACKEND_SYSTEM_API struct kan_render_allocated_slice_t
kan_render_frame_lifetime_buffer_allocator_allocate (kan_render_frame_lifetime_buffer_allocator_t allocator,
                                                     uint64_t size,
                                                     uint64_t alignment);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_frame_lifetime_buffer_allocator_destroy (
    kan_render_frame_lifetime_buffer_allocator_t allocator);

// TODO: For future iterations: cube maps and layered images (aka image arrays).

enum kan_render_image_type_t
{
    KAN_RENDER_IMAGE_TYPE_COLOR_2D,
    KAN_RENDER_IMAGE_TYPE_COLOR_3D,
    // TODO: Separate depth-only format.
    // TODO: Separate stencil-only format.
    KAN_RENDER_IMAGE_TYPE_DEPTH_STENCIL,
};

struct kan_render_image_description_t
{
    enum kan_render_image_type_t type;
    enum kan_render_color_format_t color_format;

    uint64_t width;
    uint64_t height;
    uint64_t depth;
    uint64_t mips;

    kan_bool_t render_target;
    kan_bool_t supports_sampling;

    kan_interned_string_t tracking_name;
};

CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_image_t
kan_render_image_create (kan_render_context_t context, struct kan_render_image_description_t *description);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_image_upload_data (kan_render_image_t image,
                                                                     uint64_t mip,
                                                                     void *data);

/// \brief Requests image mip generation to be executed from the first mip to the last (including it).
/// \invariant First mip is already filled with image data using `kan_render_image_upload_data`.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_image_request_mip_generation (kan_render_image_t image,
                                                                                uint64_t first,
                                                                                uint64_t last);

/// \brief Requests render target to be resized.
/// \details In most cases this call results in creation of the new image under the hood.
///          In this case, all frame buffers are updated automatically.
///          Therefore, main goal of this function is to provide user-friendly way for recreating render targets with
///          another size and updating attached frame buffers automatically under the hood.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_image_resize_render_target (kan_render_image_t image,
                                                                              uint64_t new_width,
                                                                              uint64_t new_height,
                                                                              uint64_t new_depth);

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_image_destroy (kan_render_image_t image);

KAN_C_HEADER_END
