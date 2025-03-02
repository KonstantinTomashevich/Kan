#pragma once

#include <context_render_backend_system_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/interned_string.h>
#include <kan/context/application_system.h>
#include <kan/context/context.h>

///// \file
/// \brief Contains full API of render backed context system with functional basic graphics interface.
///
/// \par Definition
/// \parblock
/// Render backend system goal is to provide full API for working with GPU and rendering. This API aims to both provide
/// enough details for optimization and be not as low level as modern APIs like Vulkan, because we need less verbose
/// and more concrete API to build render foundation on top of this system.
///
/// Currently, only the basic API needed to properly render game and build render graph is provided. This system
/// functionality will expand as needed in the future.
/// \endparblock
///
/// \par Why context system?
/// \parblock
/// Render backend needs to manage render surfaces and in order to do it, it must work with windows. It creates strong
/// coupling with context application system as we need to synchronize all potentially multithreaded operations on
/// windows through this system. Due to this strong coupling, render backend lifetime became dependent on context
/// application system. Therefore, it was decided to fully integrate render backend into context ecosystem as system.
/// \endparblock
///
/// \par Render context initialization
/// \parblock
/// Render context must be properly initialized in order to be used. It can't be done automatically as render backend
/// system needs code user to provide configuration data based on device support information which is generated during
/// system startup.
///
/// First of all, render context is never created if `kan_render_backend_system_config_t::disable_render` is true.
///
/// Then, to properly initialize context, user must select appropriate device from supported devices information.
/// To get supported devices, use `kan_render_backend_system_get_devices`. Device query is guaranteed to be done during
/// system startup on all implementations, therefore this function call is technically almost free. After that, program
/// should use provided information to select supported device and pass its handle to
/// `kan_render_backend_system_select_device`. If that call has returned KAN_TRUE, then render backend is initialized
/// successfully and cannot be reinitialized during this program execution.
/// \endparblock
///
/// \par Surfaces
/// \parblock
/// Surfaces are used to present render results to windows. Use `kan_render_backend_system_create_surface` to create
/// surface and `kan_render_backend_system_destroy_surface` to schedule surface destruction. Swap chain is created
/// automatically under the hood. Swap chain recreation on window size change is executed automatically too.
///
/// Due to synchronization with window manager, surface might not be ready until the next frame after its creation.
/// It can still be attached to frame buffer as valid handle, but that means that this frame buffer will also be
/// ready for rendering only after surface creation.
///
/// Keep in mind, that for window surfaces to be supported, window must be created with
/// `kan_render_get_required_window_flags` enabled.
/// \endparblock
///
/// \par Render passes
/// \parblock
/// Render pass is a concept that encloses routine of rendering objects into frame buffer that has specific set of
/// attachments. It also usually has high level meaning, but for render backend it is irrelevant. Render passes can
/// form dependencies one on another through `kan_render_pass_add_static_dependency`.
///
/// In order to submit commands to render pass, it must be instantiated for the current frame through
/// `kan_render_pass_instantiate` with appropriate frame buffer, viewport, scissor and clear values. If render pass
/// instance was successfully created, it can then receive commands through `kan_render_pass_instance_*` functions
/// and receive frame-lifetime dependencies through `kan_render_pass_instance_add_dynamic_dependency`.
///
/// It is allowed to create as many instances of render passes as needed: render pass functions as blueprint for the
/// instances. Also, instances lifetime is only one frame and they are automatically destroyed when frame ends.
/// \endparblock
///
/// \par Frame buffers
/// \parblock
/// Frame buffers serve as collections of render targets -- images and surfaces -- for the render pass. Therefore,
/// frame buffers are always bound to specific render passes. When surface is attached to frame buffer, swap chain
/// image management is done automatically and frame buffer automatically selects appropriate images from it.
/// \endparblock
///
/// \par Pipeline parameter set layouts
/// \parblock
/// Pipeline parameter set layout describes bindings in one particular parameter set along with this set index.
/// Set layouts are used for creation of pipelines and parameter sets.
/// \endparblock
///
/// \par Pipelines
/// \parblock
/// Pipeline describes state of the GPU pipeline that is used to execute GPU operations like draw commands.
///
/// Important subject is pipeline compilation. It takes considerable time and cannot be executed inside frame as it can
/// take much more that frame time. Therefore, pipeline compilation is done on separate thread and it is advised to
/// create all pipelines in the background and let the compile while applications works. Also, there are compilation
/// priorities that are used to define pipeline compilation order.
/// \endparblock
///
/// \par Pipeline parameter sets
/// \parblock
/// Pipeline parameter set describes data bindings for particular pipeline set using pipeline parameter set layout.
/// Sets are used to separate parameters that belong to different scopes: like pass data set, material data set,
/// object data set.
/// \endparblock
///
/// \par Buffers
/// \parblock
/// This implementation supports attribute, index 16 bit, index 32 bit, uniform and storage buffers. There is also a
/// special case: read back storage buffer that is optimized for usage as read back target and cannot be used for
/// anything else.
/// \endparblock
///
/// \par Frame lifetime allocators
/// \parblock
/// Frame lifetime allocators are used to allocate buffer space for data that is only relevant during current frame.
/// They reuse internal buffers and mark data with frame index, automatically clearing old data once it is safe.
/// \endparblock
///
/// \par Images
/// \parblock
/// This implementation supports 2d (with mips) and 3d images along with render target images. Cube maps and layered
/// images are not yet supported.
/// \endparblock
///
/// \par Render cycle
/// \parblock
/// Render cycle is built around `kan_render_backend_system_next_frame` function: it finalizes and submits old recorded
/// frame if any and starts new frame if possible. If function returned KAN_TRUE, then new frame was started and it is
/// allowed to submit new rendering commands. Otherwise, it is only allowed to create/destroy resources and upload data
/// to them. As this function execution could be relatively heavy, it is advised to execute some non-render work while
/// this function is executing.
/// \endparblock
///
/// \par Destruction routine
/// \parblock
/// Destroy functions do not destroy resources when they're called as these resources can be still in use on GPU.
/// Instead, resources are placed into destroy schedule and are destroyed as soon as it is safe. Therefore, destroy
/// call order doesn't matter: if destroy was called during the same frame (or between the same frames), it would
/// always be executed in the correct order once time is right.
/// \endparblock
///
/// \par Read back
/// \parblock
/// It is possible to read data back from surfaces, buffers and images using buffer with
/// KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE type through `kan_render_request_read_back_from_*` functions. Returned
/// `kan_render_read_back_status_t` can be used to track when read back is safe to access on CPU. It can take several
/// frames to ensure that. `kan_render_get_read_back_max_delay_in_frames` convenience function returns maximum count
/// of frames between read back request and its successful completion.
/// \endparblock
///
/// \par Synchronization
/// \parblock
/// One of the goals of render backend system is to be as thread safe as possible. It means that as long as user
/// guaranties that no other thread accesses objects that might be changed during function execution, this function
/// call is thread. Exclusions will be writen in function details.
///
/// But there is two major functions that are capable to change entire render backend state:
/// `kan_render_backend_system_next_frame` and `kan_application_system_sync_in_main_thread` (due to surfaces). These
/// functions should never be executed simultaneously with any render backend system function.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Size type that is used everywhere inside render backend.
typedef kan_platform_visual_size_t kan_render_size_t;

/// \brief Offset type that is used everywhere inside render backend.
typedef kan_platform_visual_offset_t kan_render_offset_t;

/// \brief Mask type that is used everywhere inside render backend.
typedef kan_platform_visual_size_t kan_render_mask_t;

KAN_HANDLE_DEFINE (kan_render_context_t);
KAN_HANDLE_DEFINE (kan_render_device_t);
KAN_HANDLE_DEFINE (kan_render_surface_t);
KAN_HANDLE_DEFINE (kan_render_frame_buffer_t);
KAN_HANDLE_DEFINE (kan_render_pass_t);
KAN_HANDLE_DEFINE (kan_render_pass_instance_t);
KAN_HANDLE_DEFINE (kan_render_pipeline_parameter_set_layout_t);
KAN_HANDLE_DEFINE (kan_render_code_module_t);
KAN_HANDLE_DEFINE (kan_render_graphics_pipeline_t);
KAN_HANDLE_DEFINE (kan_render_pipeline_parameter_set_t);
KAN_HANDLE_DEFINE (kan_render_buffer_t);
KAN_HANDLE_DEFINE (kan_render_frame_lifetime_buffer_allocator_t);
KAN_HANDLE_DEFINE (kan_render_image_t);
KAN_HANDLE_DEFINE (kan_render_read_back_status_t);

/// \brief Contains render backend system configuration data.
struct kan_render_backend_system_config_t
{
    kan_interned_string_t application_info_name;
    kan_render_size_t version_major;
    kan_render_size_t version_minor;
    kan_render_size_t version_patch;
};

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_backend_system_config_init (
    struct kan_render_backend_system_config_t *instance);

/// \brief Enumerates types of GPU-like devices known to render backend.
enum kan_render_device_type_t
{
    KAN_RENDER_DEVICE_TYPE_DISCRETE_GPU = 0u,
    KAN_RENDER_DEVICE_TYPE_INTEGRATED_GPU,
    KAN_RENDER_DEVICE_TYPE_VIRTUAL_GPU,
    KAN_RENDER_DEVICE_TYPE_CPU,
    KAN_RENDER_DEVICE_TYPE_UNKNOWN,
};

/// \brief Enumerates GPU memory types known to render backend.
enum kan_render_device_memory_type_t
{
    /// \brief Fully separate memory module on GPU. Often found in standalone PCs.
    KAN_RENDER_DEVICE_MEMORY_TYPE_SEPARATE = 0u,

    /// \brief Most memory is both device local and host visible, but not host coherent. Rare.
    KAN_RENDER_DEVICE_MEMORY_TYPE_UNIFIED,

    /// \brief Most memory is both device local and host coherent. Often found in laptops and mobile devices.
    KAN_RENDER_DEVICE_MEMORY_TYPE_UNIFIED_COHERENT,
};

/// \brief Enumerates all images formats known to render backend.
enum kan_render_image_format_t
{
    KAN_RENDER_IMAGE_FORMAT_R8_SRGB = 0u,
    KAN_RENDER_IMAGE_FORMAT_RG16_SRGB,
    KAN_RENDER_IMAGE_FORMAT_RGB24_SRGB,
    KAN_RENDER_IMAGE_FORMAT_RGBA32_SRGB,
    KAN_RENDER_IMAGE_FORMAT_BGRA32_SRGB,

    KAN_RENDER_IMAGE_FORMAT_R32_SFLOAT,
    KAN_RENDER_IMAGE_FORMAT_RG64_SFLOAT,
    KAN_RENDER_IMAGE_FORMAT_RGB96_SFLOAT,
    KAN_RENDER_IMAGE_FORMAT_RGBA128_SFLOAT,

    KAN_RENDER_IMAGE_FORMAT_D16_UNORM,
    KAN_RENDER_IMAGE_FORMAT_D32_SFLOAT,

    KAN_RENDER_IMAGE_FORMAT_S8_UINT,

    KAN_RENDER_IMAGE_FORMAT_D16_UNORM_S8_UINT,
    KAN_RENDER_IMAGE_FORMAT_D24_UNORM_S8_UINT,
    KAN_RENDER_IMAGE_FORMAT_D32_SFLOAT_S8_UINT,

    KAN_RENDER_IMAGE_FORMAT_BC1_RGB_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_BC1_RGB_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_BC1_RGBA_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_BC1_RGBA_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_BC2_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_BC2_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_BC3_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_BC3_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_BC4_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_BC4_SNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_BC5_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_BC5_SNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_BC6H_UFLOAT_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_BC6H_SFLOAT_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_BC7_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_BC7_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ETC2_RGB24_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ETC2_RGB24_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ETC2_RGBA25_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ETC2_RGBA25_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ETC2_RGBA32_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ETC2_RGBA32_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_4x4_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_4x4_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_5x4_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_5x4_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_5x5_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_5x5_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_6x5_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_6x5_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_6x6_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_6x6_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_8x5_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_8x5_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_8x6_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_8x6_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_8x8_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_8x8_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_10x5_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_10x5_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_10x6_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_10x6_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_10x8_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_10x8_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_10x10_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_10x10_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_12x10_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_12x10_SRGB_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_12x12_UNORM_BLOCK,
    KAN_RENDER_IMAGE_FORMAT_ASTC_12x12_SRGB_BLOCK,

    KAN_RENDER_IMAGE_FORMAT_COUNT,

    /// \brief As of now, we always use BGRA for surfaces.
    KAN_RENDER_IMAGE_FORMAT_SURFACE = KAN_RENDER_IMAGE_FORMAT_BGRA32_SRGB,
};

/// \brief For kan_render_supported_device_info_t::image_format_support. Format transfer is supported.
#define KAN_RENDER_IMAGE_FORMAT_SUPPORT_FLAG_TRANSFER (1u << 0u)

/// \brief For kan_render_supported_device_info_t::image_format_support. Format is supported as sampled image.
#define KAN_RENDER_IMAGE_FORMAT_SUPPORT_FLAG_SAMPLED (1u << 1u)

/// \brief For kan_render_supported_device_info_t::image_format_support. Format is supported as render target.
#define KAN_RENDER_IMAGE_FORMAT_SUPPORT_FLAG_RENDER (1u << 2u)

/// \brief Describes information about found supported device.
struct kan_render_supported_device_info_t
{
    kan_render_device_t id;
    const char *name;
    enum kan_render_device_type_t device_type;
    enum kan_render_device_memory_type_t memory_type;
    uint8_t image_format_support[KAN_RENDER_IMAGE_FORMAT_COUNT];
};

/// \brief Describes information about all found supported devices.
struct kan_render_supported_devices_t
{
    kan_instance_size_t supported_device_count;
    KAN_REFLECTION_IGNORE
    struct kan_render_supported_device_info_t devices[];
};

/// \brief Returns cached information about all found supported devices.
CONTEXT_RENDER_BACKEND_SYSTEM_API struct kan_render_supported_devices_t *kan_render_backend_system_get_devices (
    kan_context_system_t render_backend_system);

/// \brief Initializes render backend system with given physical device.
///        If successfully initialized, cannot be initialized again.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_bool_t
kan_render_backend_system_select_device (kan_context_system_t render_backend_system, kan_render_device_t device);

/// \brief Returns selected device info if any device was selected.
CONTEXT_RENDER_BACKEND_SYSTEM_API struct kan_render_supported_device_info_t *
kan_render_backend_system_get_selected_device_info (kan_context_system_t render_backend_system);

/// \brief Returns render context that is used with most other render backend functions.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_context_t
kan_render_backend_system_get_render_context (kan_context_system_t render_backend_system);

/// \details Submits recorded commands and presentation from previous frame, prepares data for the new frame.
/// \return True if next frame submit should be started, false otherwise. For example, we might not be able to submit
///         new frame while using frames in flights when GPU is not fast enough to process all the frames.
/// \details User should never call other render backend functions while this function is executing. Also, user must
///          ensure that `kan_application_system_sync_in_main_thread` is not executed simultaneously with that function.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_bool_t
kan_render_backend_system_next_frame (kan_context_system_t render_backend_system);

/// \brief Enumerates known present formats.
enum kan_render_surface_present_mode_t
{
    /// \brief Special value that indicates that there is no more present modes in requested present queue.
    KAN_RENDER_SURFACE_PRESENT_MODE_INVALID = 0u,

    KAN_RENDER_SURFACE_PRESENT_MODE_IMMEDIATE,
    KAN_RENDER_SURFACE_PRESENT_MODE_MAILBOX,
    KAN_RENDER_SURFACE_PRESENT_MODE_FIFO,
    KAN_RENDER_SURFACE_PRESENT_MODE_FIFO_RELAXED,

    /// \brief Special value used to get count of known present modes.
    KAN_RENDER_SURFACE_PRESENT_MODE_COUNT,
};

/// \brief Utility structure for integer regions.
struct kan_render_integer_region_t
{
    kan_render_offset_t x;
    kan_render_offset_t y;
    kan_render_size_t width;
    kan_render_size_t height;
};

/// \brief Requests new render surface to be created. Surface will be created and initialized when
///        given application window becomes available.
/// \details Present mode queue is an array of kan_render_surface_present_mode_t of size
///          KAN_RENDER_SURFACE_PRESENT_MODE_COUNT. We check present modes in queue from first to last for their
///          availability and select the first available. If array iterator reaches
///          KAN_RENDER_SURFACE_PRESENT_MODE_INVALID value, then surface creation fails.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_surface_t
kan_render_backend_system_create_surface (kan_context_system_t render_backend_system,
                                          kan_application_system_window_t window,
                                          enum kan_render_surface_present_mode_t *present_mode_queue,
                                          kan_interned_string_t tracking_name);

/// \brief Blits given image onto given surface at the end of the frame.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_backend_system_present_image_on_surface (
    kan_render_surface_t surface,
    kan_render_image_t image,
    struct kan_render_integer_region_t surface_region,
    struct kan_render_integer_region_t image_region);

/// \brief Recreates surface using new present mode queue (the same format as for creation).
/// \details Surface will be recreated and initialized during next application system sync.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_backend_system_change_surface_present_mode (
    kan_render_surface_t surface, enum kan_render_surface_present_mode_t *present_mode_queue);

/// \brief Requests given surface to be destroyed when possible.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_backend_system_destroy_surface (
    kan_context_system_t render_backend_system, kan_render_surface_t surface);

/// \brief Returns flags that are required for application windows in order to create surfaces.
CONTEXT_RENDER_BACKEND_SYSTEM_API enum kan_platform_window_flag_t kan_render_get_required_window_flags (void);

/// \brief Enumerates known code formats that can be passed to render backends.
enum kan_render_code_format_t
{
    KAN_RENDER_CODE_FORMAT_SPIRV = 0u,
};

/// \brief Returns bitmask of code formats supported by current render backend.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_memory_size_t kan_render_get_supported_code_format_flags (void);

/// \brief Frame buffer supported attachment types.
enum kan_render_frame_buffer_attachment_type_t
{
    KAN_FRAME_BUFFER_ATTACHMENT_IMAGE = 0u,
    KAN_FRAME_BUFFER_ATTACHMENT_SURFACE,
};

/// \brief Describes one frame buffer attachment.
struct kan_render_frame_buffer_attachment_description_t
{
    enum kan_render_frame_buffer_attachment_type_t type;
    union
    {
        kan_render_image_t image;
        kan_render_surface_t surface;
    };
};

/// \brief Contains information needed for frame buffer creation.
struct kan_render_frame_buffer_description_t
{
    kan_render_pass_t associated_pass;
    kan_instance_size_t attachment_count;
    struct kan_render_frame_buffer_attachment_description_t *attachments;
    kan_interned_string_t tracking_name;
};

/// \brief Requests new frame buffer to be created. Actual frame buffers will be created when all attachments are ready.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_frame_buffer_t kan_render_frame_buffer_create (
    kan_render_context_t context, struct kan_render_frame_buffer_description_t *description);

/// \brief Requests given frame buffer to be destroyed.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_frame_buffer_destroy (kan_render_frame_buffer_t buffer);

/// \brief Enumerates all supported render pass types.
enum kan_render_pass_type_t
{
    KAN_RENDER_PASS_GRAPHICS = 0u,
};

/// \brief Enumerates supported render pass attachment types.
enum kan_render_pass_attachment_type_t
{
    KAN_RENDER_PASS_ATTACHMENT_COLOR = 0u,
    KAN_RENDER_PASS_ATTACHMENT_DEPTH,
    KAN_RENDER_PASS_ATTACHMENT_STENCIL,
    KAN_RENDER_PASS_ATTACHMENT_DEPTH_STENCIL,
};

/// \brief Enumerates supported attachment load operations.
enum kan_render_load_operation_t
{
    KAN_RENDER_LOAD_OPERATION_ANY = 0u,
    KAN_RENDER_LOAD_OPERATION_LOAD,
    KAN_RENDER_LOAD_OPERATION_CLEAR,
};

/// \brief Enumerates supported attachment store operations.
enum kan_render_store_operation_t
{
    KAN_RENDER_STORE_OPERATION_ANY = 0u,
    KAN_RENDER_STORE_OPERATION_STORE,
    KAN_RENDER_STORE_OPERATION_NONE,
};

/// \brief Describes one render pass attachment configuration.
struct kan_render_pass_attachment_t
{
    enum kan_render_pass_attachment_type_t type;
    enum kan_render_image_format_t format;
    kan_render_size_t samples;
    enum kan_render_load_operation_t load_operation;
    enum kan_render_store_operation_t store_operation;
};

/// \brief Contains full information needed to create render pass.
struct kan_render_pass_description_t
{
    enum kan_render_pass_type_t type;
    kan_instance_size_t attachments_count;
    struct kan_render_pass_attachment_t *attachments;
    kan_interned_string_t tracking_name;
};

/// \brief Describes viewport bounds.
struct kan_render_viewport_bounds_t
{
    float x;
    float y;
    float width;
    float height;
    float depth_min;
    float depth_max;
};

/// \brief Describes color for clearing.
struct kan_render_clear_color_t
{
    float r;
    float g;
    float b;
    float a;
};

/// \brief Describes depth and stencil values for clearing.
struct kan_render_clear_depth_stencil_t
{
    float depth;
    uint8_t stencil;
};

/// \brief Describes clear value for one attachment.
struct kan_render_clear_value_t
{
    union
    {
        struct kan_render_clear_color_t color;
        struct kan_render_clear_depth_stencil_t depth_stencil;
    };
};

/// \brief Creates new render pass from given description.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_pass_t
kan_render_pass_create (kan_render_context_t context, struct kan_render_pass_description_t *description);

/// \brief Creates dependency between render passes that will be inherited by all instances.
/// \details Fully thread safe for both passes.
///          Static dependency creation binds passes together and requires both passes to be destroyed during the
///          same frame. It shouldn't be an issue for the architecture, because passes are part of render graph and
///          graph should only be destroyed as a whole, not partially.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pass_add_static_dependency (kan_render_pass_t pass,
                                                                              kan_render_pass_t dependency);

/// \brief Instantiates render pass for given frame buffer with given configuration.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_pass_instance_t
kan_render_pass_instantiate (kan_render_pass_t pass,
                             kan_render_frame_buffer_t frame_buffer,
                             struct kan_render_viewport_bounds_t *viewport_bounds,
                             struct kan_render_integer_region_t *scissor,
                             struct kan_render_clear_value_t *attachment_clear_values);

/// \brief Creates dependency between two render pass instances.
/// \details Fully thread safe for both pass instances.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pass_instance_add_dynamic_dependency (
    kan_render_pass_instance_t pass_instance, kan_render_pass_instance_t dependency);

/// \brief Submits graphics pipeline binding to the render pass.
/// \return Whether pipeline was successfully bound. Binding will fail if pipeline is not compiled yet and priority is
///         not KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CRITICAL. If priority is
///         KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CRITICAL, function will not return until pipeline is compiled.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_bool_t kan_render_pass_instance_graphics_pipeline (
    kan_render_pass_instance_t pass_instance, kan_render_graphics_pipeline_t graphics_pipeline);

/// \brief Submits parameter set bindings to the render pass.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pass_instance_pipeline_parameter_sets (
    kan_render_pass_instance_t pass_instance,
    kan_instance_size_t parameter_sets_count,
    kan_render_pipeline_parameter_set_t *parameter_sets);

/// \brief Submits attributes to the render pass.
/// \details `buffer_offsets` is an optional array of offsets for passed buffers.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pass_instance_attributes (kan_render_pass_instance_t pass_instance,
                                                                            kan_render_size_t start_at_binding,
                                                                            kan_instance_size_t buffers_count,
                                                                            kan_render_buffer_t *buffers,
                                                                            kan_instance_size_t *buffer_offsets);

/// \brief Submits indices to the render pass.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pass_instance_indices (kan_render_pass_instance_t pass_instance,
                                                                         kan_render_buffer_t buffer);

/// \brief Submits one instance draw call to the render pass.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pass_instance_draw (kan_render_pass_instance_t pass_instance,
                                                                      kan_render_size_t index_offset,
                                                                      kan_render_size_t index_count,
                                                                      kan_render_size_t vertex_offset);

/// \brief Submits multiple instances draw call to the render pass.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pass_instance_instanced_draw (
    kan_render_pass_instance_t pass_instance,
    kan_render_size_t index_offset,
    kan_render_size_t index_count,
    kan_render_size_t vertex_offset,
    kan_render_size_t instance_offset,
    kan_render_size_t instance_count);

/// \brief Requests given render pass to be destroyed.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pass_destroy (kan_render_pass_t pass);

/// \brief Enumerates supported types of parameter bindings.
enum kan_render_parameter_binding_type_t
{
    KAN_RENDER_PARAMETER_BINDING_TYPE_UNIFORM_BUFFER = 0u,
    KAN_RENDER_PARAMETER_BINDING_TYPE_STORAGE_BUFFER,
    KAN_RENDER_PARAMETER_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
};

/// \brief Describes parameter that can be bound to the pipeline.
struct kan_render_parameter_binding_description_t
{
    kan_render_size_t binding;
    enum kan_render_parameter_binding_type_t type;
    kan_render_mask_t used_stage_mask;
};

/// \brief Describes set of parameters that can be bound at particular set index.
struct kan_render_pipeline_parameter_set_layout_description_t
{
    kan_render_size_t set;
    kan_instance_size_t bindings_count;
    struct kan_render_parameter_binding_description_t *bindings;

    /// \brief True if bindings are rarely changed. False otherwise. Used for optimization.
    kan_bool_t stable_binding;

    kan_interned_string_t tracking_name;
};

/// \brief Creates new pipeline parameter set from given parameters.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_pipeline_parameter_set_layout_t
kan_render_pipeline_parameter_set_layout_create (
    kan_render_context_t context, struct kan_render_pipeline_parameter_set_layout_description_t *description);

/// \brief Requests given pipeline parameter set to be destroyed.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pipeline_parameter_set_layout_destroy (
    kan_render_pipeline_parameter_set_layout_t layout);

/// \brief Creates new pipeline code module from given implementation-specific code.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_code_module_t kan_render_code_module_create (
    kan_render_context_t context, kan_instance_size_t code_length, void *code, kan_interned_string_t tracking_name);

/// \brief Requests given code module to be destroyed.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_code_module_destroy (kan_render_code_module_t code_module);

/// \brief Enumerates supported topologies.
enum kan_render_graphics_topology_t
{
    KAN_RENDER_GRAPHICS_TOPOLOGY_TRIANGLE_LIST = 0u,
};

/// \brief Enumerates supported polygon modes,
enum kan_render_polygon_mode_t
{
    KAN_RENDER_POLYGON_MODE_FILL,
    KAN_RENDER_POLYGON_MODE_WIREFRAME,
};

/// \brief Enumerates supported cull modes.
enum kan_render_cull_mode_t
{
    KAN_RENDER_CULL_MODE_BACK = 0u,
};

/// \brief Enumerates supported render stages.
enum kan_render_stage_t
{
    KAN_RENDER_STAGE_GRAPHICS_VERTEX = 0u,
    KAN_RENDER_STAGE_GRAPHICS_FRAGMENT,
};

/// \brief Enumerates supported attribute rates.
enum kan_render_attribute_rate_t
{
    KAN_RENDER_ATTRIBUTE_RATE_PER_VERTEX = 0u,
    KAN_RENDER_ATTRIBUTE_RATE_PER_INSTANCE,
};

/// \brief Provides information about attribute source buffer.
struct kan_render_attribute_source_description_t
{
    kan_render_size_t binding;
    kan_render_size_t stride;
    enum kan_render_attribute_rate_t rate;
};

/// \brief Enumerates supported attribute formats.
enum kan_render_attribute_format_t
{
    KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_1,
    KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_2,
    KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_3,
    KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_4,
    KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_SIGNED_INT_1,
    KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_SIGNED_INT_2,
    KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_SIGNED_INT_3,
    KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_SIGNED_INT_4,
    KAN_RENDER_ATTRIBUTE_FORMAT_MATRIX_FLOAT_3_3,
    KAN_RENDER_ATTRIBUTE_FORMAT_MATRIX_FLOAT_4_4,
};

/// \brief Describes one attribute.
struct kan_render_attribute_description_t
{
    kan_render_size_t binding;
    kan_render_size_t location;
    kan_render_size_t offset;
    enum kan_render_attribute_format_t format;
};

/// \brief Enumerates supported blend factors.
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

/// \brief Enumerates supported blend operations.
enum kan_render_blend_operation_t
{
    KAN_RENDER_BLEND_OPERATION_ADD = 0u,
    KAN_RENDER_BLEND_OPERATION_SUBTRACT,
    KAN_RENDER_BLEND_OPERATION_REVERSE_SUBTRACT,
    KAN_RENDER_BLEND_OPERATION_MIN,
    KAN_RENDER_BLEND_OPERATION_MAX,
};

/// \brief Describes color output setup for one color attachment.
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

/// \brief Enumerates supported compare operations.
enum kan_render_compare_operation_t
{
    KAN_RENDER_COMPARE_OPERATION_NEVER = 0u,
    KAN_RENDER_COMPARE_OPERATION_ALWAYS,
    KAN_RENDER_COMPARE_OPERATION_EQUAL,
    KAN_RENDER_COMPARE_OPERATION_NOT_EQUAL,
    KAN_RENDER_COMPARE_OPERATION_LESS,
    KAN_RENDER_COMPARE_OPERATION_LESS_OR_EQUAL,
    KAN_RENDER_COMPARE_OPERATION_GREATER,
    KAN_RENDER_COMPARE_OPERATION_GREATER_OR_EQUAL,
};

/// \brief Enumerates supported stencil operations.
enum kan_render_stencil_operation_t
{
    KAN_RENDER_STENCIL_OPERATION_KEEP = 0u,
    KAN_RENDER_STENCIL_OPERATION_ZERO,
    KAN_RENDER_STENCIL_OPERATION_REPLACE,
    KAN_RENDER_STENCIL_OPERATION_INCREMENT_AND_CLAMP,
    KAN_RENDER_STENCIL_OPERATION_DECREMENT_AND_CLAMP,
    KAN_RENDER_STENCIL_OPERATION_INVERT,
    KAN_RENDER_STENCIL_OPERATION_INCREMENT_AND_WRAP,
    KAN_RENDER_STENCIL_OPERATION_DECREMENT_AND_WRAP,
};

/// \brief Describes stencil test.
struct kan_render_stencil_test_t
{
    enum kan_render_stencil_operation_t on_fail;
    enum kan_render_stencil_operation_t on_depth_fail;
    enum kan_render_stencil_operation_t on_pass;
    enum kan_render_compare_operation_t compare;
    kan_render_mask_t compare_mask;
    kan_render_mask_t write_mask;
    kan_render_size_t reference;
};

/// \brief Describes one code entry point with its stage,
struct kan_render_pipeline_code_entry_point_t
{
    enum kan_render_stage_t stage;
    kan_interned_string_t function_name;
};

/// \brief Describes code module usage with its entry points.
struct kan_render_pipeline_code_module_usage_t
{
    kan_render_code_module_t code_module;
    kan_instance_size_t entry_points_count;
    struct kan_render_pipeline_code_entry_point_t *entry_points;
};

/// \brief Contains required information for graphics pipeline creation.
struct kan_render_graphics_pipeline_description_t
{
    kan_render_pass_t pass;
    enum kan_render_graphics_topology_t topology;
    enum kan_render_polygon_mode_t polygon_mode;
    enum kan_render_cull_mode_t cull_mode;
    kan_bool_t use_depth_clamp;

    kan_instance_size_t attribute_sources_count;
    struct kan_render_attribute_source_description_t *attribute_sources;

    kan_instance_size_t attributes_count;
    struct kan_render_attribute_description_t *attributes;

    kan_instance_size_t parameter_set_layouts_count;
    kan_render_pipeline_parameter_set_layout_t *parameter_set_layouts;

    kan_instance_size_t output_setups_count;
    struct kan_render_color_output_setup_description_t *output_setups;
    float blend_constant_r;
    float blend_constant_g;
    float blend_constant_b;
    float blend_constant_a;

    kan_bool_t depth_test_enabled;
    kan_bool_t depth_write_enabled;
    kan_bool_t depth_bounds_test_enabled;
    enum kan_render_compare_operation_t depth_compare_operation;
    float min_depth;
    float max_depth;

    kan_bool_t stencil_test_enabled;
    struct kan_render_stencil_test_t stencil_front;
    struct kan_render_stencil_test_t stencil_back;

    kan_instance_size_t code_modules_count;
    struct kan_render_pipeline_code_module_usage_t *code_modules;

    kan_interned_string_t tracking_name;
};

/// \brief Enumerates pipeline compilation priorities.
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

/// \brief Creates new graphics pipeline and adds it to compilation queue.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_graphics_pipeline_t
kan_render_graphics_pipeline_create (kan_render_context_t context,
                                     struct kan_render_graphics_pipeline_description_t *description,
                                     enum kan_render_pipeline_compilation_priority_t compilation_priority);

/// \brief Changes graphics pipeline compilation priority if it is still waiting for compilation.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_graphics_pipeline_change_compilation_priority (
    kan_render_graphics_pipeline_t pipeline, enum kan_render_pipeline_compilation_priority_t compilation_priority);

/// \brief Requests graphics pipeline to be destroyed.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_graphics_pipeline_destroy (kan_render_graphics_pipeline_t pipeline);

/// \brief Contains information for pipeline parameter set creation.
struct kan_render_pipeline_parameter_set_description_t
{
    kan_render_pipeline_parameter_set_layout_t layout;
    kan_interned_string_t tracking_name;

    kan_instance_size_t initial_bindings_count;
    struct kan_render_parameter_update_description_t *initial_bindings;
};

/// \brief Contains information for buffer binding update.
struct kan_render_parameter_update_description_buffer_t
{
    kan_render_buffer_t buffer;
    kan_render_size_t offset;
    kan_render_size_t range;
};

/// \brief Enumerates supported filter modes.
enum kan_render_filter_mode_t
{
    KAN_RENDER_FILTER_MODE_NEAREST = 0u,
    KAN_RENDER_FILTER_MODE_LINEAR,
};

/// \brief Enumerates supported mip map modes.
enum kan_render_mip_map_mode_t
{
    KAN_RENDER_MIP_MAP_MODE_NEAREST = 0u,
    KAN_RENDER_MIP_MAP_MODE_LINEAR,
};

/// \brief Enumerates supported address modes.
enum kan_render_address_mode_t
{
    KAN_RENDER_ADDRESS_MODE_REPEAT = 0u,
    KAN_RENDER_ADDRESS_MODE_MIRRORED_REPEAT,
    KAN_RENDER_ADDRESS_MODE_CLAMP_TO_EDGE,
    KAN_RENDER_ADDRESS_MODE_MIRRORED_CLAMP_TO_EDGE,
    KAN_RENDER_ADDRESS_MODE_CLAMP_TO_BORDER,
};

/// \brief Contains image sampling parameters.
struct kan_render_sampler_t
{
    enum kan_render_filter_mode_t mag_filter;
    enum kan_render_filter_mode_t min_filter;
    enum kan_render_mip_map_mode_t mip_map_mode;
    enum kan_render_address_mode_t address_mode_u;
    enum kan_render_address_mode_t address_mode_v;
    enum kan_render_address_mode_t address_mode_w;
};

/// \brief Contains information for image binding update.
struct kan_render_parameter_update_description_image_t
{
    kan_render_image_t image;
    struct kan_render_sampler_t sampler;
};

/// \brief Contains information on how to update one parameter binding.
struct kan_render_parameter_update_description_t
{
    kan_render_size_t binding;
    union
    {
        struct kan_render_parameter_update_description_buffer_t buffer_binding;
        struct kan_render_parameter_update_description_image_t image_binding;
    };
};

/// \brief Creates new pipeline parameter set from given description.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_pipeline_parameter_set_t kan_render_pipeline_parameter_set_create (
    kan_render_context_t context, struct kan_render_pipeline_parameter_set_description_t *description);

/// \brief Updates given parameter set with new bindings.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pipeline_parameter_set_update (
    kan_render_pipeline_parameter_set_t set,
    kan_instance_size_t bindings_count,
    struct kan_render_parameter_update_description_t *bindings);

/// \brief Requests parameter set to be destroyed.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pipeline_parameter_set_destroy (
    kan_render_pipeline_parameter_set_t set);

/// \brief Enumerates supported buffer types.
enum kan_render_buffer_type_t
{
    /// \brief Buffer for storing attribute data.
    KAN_RENDER_BUFFER_TYPE_ATTRIBUTE = 0u,

    /// \brief Buffer for storing 16 bit indices.
    KAN_RENDER_BUFFER_TYPE_INDEX_16,

    /// \brief Buffer for storing 32 bit indices.
    KAN_RENDER_BUFFER_TYPE_INDEX_32,

    /// \brief Uniform buffer.
    KAN_RENDER_BUFFER_TYPE_UNIFORM,

    /// \brief Arbitrary storage buffer.
    KAN_RENDER_BUFFER_TYPE_STORAGE,

    /// \brief Storage buffer that functions as read back target.
    KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE,
};

/// \brief Maximum guaranteed size of uniform buffer.
#define KAN_UNIFORM_BUFFER_MAXIMUM_GUARANTEED_SIZE (16u * 1024u)

/// \brief Maximum guaranteed size of arbitrary storage buffer.
#define KAN_STORAGE_BUFFER_MAXIMUM_GUARANTEED_SIZE (128u * 1024u * 1024u)

/// \brief Creates new buffer of given type and size.
/// \details Optional initial data allows to directly upload initial data to buffer without transfer operation on
///          devices with unified memory. In other cases direct upload with this devices might not be possible
///          due to frames in flights feature.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_buffer_t kan_render_buffer_create (kan_render_context_t context,
                                                                                enum kan_render_buffer_type_t type,
                                                                                kan_render_size_t full_size,
                                                                                void *optional_initial_data,
                                                                                kan_interned_string_t tracking_name);

/// \brief Declares intent to patch buffer slice with given offset and size.
/// \returns If it is possible to patch buffer, returns write-only pointer for updating buffer data.
/// \invariant Buffer type is not KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE.
CONTEXT_RENDER_BACKEND_SYSTEM_API void *kan_render_buffer_patch (kan_render_buffer_t buffer,
                                                                 kan_render_size_t slice_offset,
                                                                 kan_render_size_t slice_size);

/// \brief Returns full size of a buffer specified during creation.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_size_t kan_render_buffer_get_full_size (kan_render_buffer_t buffer);

/// \brief Requests read access to read back buffer.
/// \return Pointer to read back buffer data on success.
/// \invariant Buffer type is KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE.
CONTEXT_RENDER_BACKEND_SYSTEM_API void *kan_render_buffer_begin_access (kan_render_buffer_t buffer);

/// \brief Closes read access requested previously by `kan_render_buffer_begin_access`.
/// \invariant Buffer type is KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_buffer_end_access (kan_render_buffer_t buffer);

/// \brief Requests given buffer to be destroyed.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_buffer_destroy (kan_render_buffer_t buffer);

/// \brief Describes frame lifetime allocator allocation: buffer and data offset in this buffer.
/// \details If buffer is invalid, then allocation has failed.
struct kan_render_allocated_slice_t
{
    kan_render_buffer_t buffer;
    kan_render_size_t slice_offset;
};

/// \brief Creates new frame lifetime allocator for buffers of given type and with given page size.
/// \details Usually, there is no need for frame lifetime allocation on device, as either way memory is transferred from
///          host to GPU. However, in some cases transferring data to GPU every frame is still faster than letting
///          GPU access host memory directly, although it is usually not noticeable, as GPU might access the same data
///          several times. Therefore, we've made it possible to use frame lifetime allocators with device memory.
///          However, using such frame allocators essentially doubles the memory usage as they need to use staging
///          buffer to execute transfer to device memory.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_frame_lifetime_buffer_allocator_t
kan_render_frame_lifetime_buffer_allocator_create (kan_render_context_t context,
                                                   enum kan_render_buffer_type_t buffer_type,
                                                   kan_render_size_t page_size,
                                                   kan_bool_t on_device,
                                                   kan_interned_string_t tracking_name);

/// \brief Requests given amount of memory with given alignment from frame lifetime allocator.
/// \details Allocated memory is automatically freed when we're sure that it is no longer used.
///          This function is explicitly thread safe, therefore it is allowed to allocate from multiple threads
///          simultaneously using the same allocator.
CONTEXT_RENDER_BACKEND_SYSTEM_API struct kan_render_allocated_slice_t
kan_render_frame_lifetime_buffer_allocator_allocate (kan_render_frame_lifetime_buffer_allocator_t allocator,
                                                     kan_render_size_t size,
                                                     kan_render_size_t alignment);

/// \brief Requests given frame lifetime allocator to be destroyed.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_frame_lifetime_buffer_allocator_destroy (
    kan_render_frame_lifetime_buffer_allocator_t allocator);

// TODO: For future iterations: cube maps and layered images (aka image arrays).

/// \brief Contains information for image creation.
struct kan_render_image_description_t
{
    enum kan_render_image_format_t format;
    kan_render_size_t width;
    kan_render_size_t height;
    kan_render_size_t depth;
    uint8_t mips;

    kan_bool_t render_target;
    kan_bool_t supports_sampling;

    kan_interned_string_t tracking_name;
};

/// \brief Creates image from given description.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_image_t
kan_render_image_create (kan_render_context_t context, struct kan_render_image_description_t *description);

/// \brief Schedules data upload to given image mip.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_image_upload_data (kan_render_image_t image,
                                                                     uint8_t mip,
                                                                     kan_render_size_t data_size,
                                                                     void *data);

/// \brief Requests image mip generation to be executed from the first mip to the last (including it).
/// \invariant First mip is already filled with image data using `kan_render_image_upload_data`.
///            It is allowed to call `kan_render_image_upload_data` and then call this function during the same frame.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_image_request_mip_generation (kan_render_image_t image,
                                                                                uint8_t first,
                                                                                uint8_t last);

/// \brief Schedules data copy from one mip of one image to another mip of another image.
/// \invariant User must guarantee that images are compatible and that sizes at given mips are equal.
/// \invariant For thread safety, both images should not be modified by other functions during this call.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_image_copy_data (kan_render_image_t from_image,
                                                                   uint8_t from_mip,
                                                                   kan_render_image_t to_image,
                                                                   uint8_t to_mip);

/// \brief Requests render target to be resized without breaking the attachments.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_image_resize_render_target (kan_render_image_t image,
                                                                              kan_render_size_t new_width,
                                                                              kan_render_size_t new_height,
                                                                              kan_render_size_t new_depth);

/// \brief Requests given image to be destroyed.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_image_destroy (kan_render_image_t image);

/// \brief Enumerates read back request states.
enum kan_render_read_back_state_t
{
    /// \brief Requests is created and not yet processed.
    KAN_RENDER_READ_BACK_STATE_REQUESTED = 0,

    /// \brief Request is sent to GPU and we're waiting until it is safe to read.
    KAN_RENDER_READ_BACK_STATE_SCHEDULED,

    /// \brief Request finished and it is safe to read.
    KAN_RENDER_READ_BACK_STATE_FINISHED,

    /// \brief Read back operation failed.
    KAN_RENDER_READ_BACK_STATE_FAILED,
};

/// \brief Returns maximum delay between read back request and completion allowed by implementation.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_instance_size_t kan_render_get_read_back_max_delay_in_frames (void);

/// \brief Requests to read data back from surface when this frame ends.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_read_back_status_t kan_render_request_read_back_from_surface (
    kan_render_surface_t surface, kan_render_buffer_t read_back_buffer, kan_render_size_t read_back_offset);

/// \brief Requests to read data back from buffer when this frame ends.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_read_back_status_t
kan_render_request_read_back_from_buffer (kan_render_buffer_t buffer,
                                          kan_render_size_t offset,
                                          kan_render_size_t slice,
                                          kan_render_buffer_t read_back_buffer,
                                          kan_render_size_t read_back_offset);

/// \brief Requests to read data back from image when this frame ends.
CONTEXT_RENDER_BACKEND_SYSTEM_API kan_render_read_back_status_t kan_render_request_read_back_from_image (
    kan_render_image_t image, uint8_t mip, kan_render_buffer_t read_back_buffer, kan_render_size_t read_back_offset);

/// \brief Queries current status of read back operation.
CONTEXT_RENDER_BACKEND_SYSTEM_API enum kan_render_read_back_state_t kan_read_read_back_status_get (
    kan_render_read_back_status_t status);

/// \brief Destroys read back status. Does not cancel read back.
CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_read_back_status_destroy (kan_render_read_back_status_t status);

KAN_C_HEADER_END
