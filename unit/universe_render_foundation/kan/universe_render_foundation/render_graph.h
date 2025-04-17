#pragma once

#include <universe_render_foundation_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/hash_storage.h>
#include <kan/context/render_backend_system.h>
#include <kan/inline_math/inline_math.h>
#include <kan/resource_material/resource_render_pass.h>
#include <kan/threading/atomic.h>
#include <kan/universe/universe.h>
#include <kan/universe_object/universe_object.h>

/// \file
/// \brief Provides API for render foundation render graph implementation.
///
/// \par Definition
/// \parblock
/// This file provides API for working with render foundation root routine implementation that consists of render
/// passes, render graph and frame scheduling. Mutators from root routine should only be added to the root world as
/// they manage render state as a whole -- having independent routines in several worlds at once is not supported.
/// \endparblock
///
/// \par Render passes
/// \parblock
/// All found render pass resources are loaded and prepared for usage automatically, because in most cases all render
/// passes are required for the application, therefore there is no sense to load them on demand. Passes that are needed
/// for tools, but are not needed for the game (for example, editor-only passes), should be excluded through tag
/// requirement routine, described in `kan_resource_render_pass_t` documentation.
/// \endparblock
///
/// \par Render graph
/// \parblock
/// Render graph goal is to reduce GPU resource usage by pass instances in complex cases like split-screen rendering or
/// rendering different worlds (which is a common case for the editor). It is done by tracking resource usage graph and
/// inserting additional dependencies to checkpoints to get rid of unneeded parallelism. For example, we don't need to
/// draw two scenes in parallel as GPU would rarely benefit from it. And if we get rid of this parallelism, we might be
/// able to reuse depth texture.
///
/// Resources for render passes are requested in bundles using `kan_render_graph_resource_request_t`: it enumerates
/// required images, frame buffers that use these images and other successfully allocated bundles that depend on
/// passes in this bundle and might use images from this bundle as outputs. `kan_render_graph_resource_response_t`
/// contains created images, frame buffers and pass instance checkpoints. Response pointer is always valid during
/// this frame and invalidated at frame end: it has the same lifetime as pass instances.
///
/// Bundles were used instead of actual pass instances, because it allows user to separate pass instance creation and
/// do it manually. The reason is that rendering one view properly usually requires several pass instances with
/// custom attachment resource sharing between them. Adding this details into resource allocation would make it too
/// complex. Therefore, it was decided that user would request resources in bundles and then create passes with proper
/// resource usage manually.
///
/// Keep in mind that render graph caches render target images and frame buffers between frames, automatically
/// destroying them if they're no longer useful. It makes it possible to avoid costly resource creation every frame,
/// but may result in keeping some resources in memory for a little bit more time than they're actually used.
/// \endparblock
///
/// \par Frame scheduling
/// \parblock
/// Render foundation root routine also handles frame execution by calling `kan_render_backend_system_next_frame` and
/// updating frame-dependant render graph data. Also, when render backend device wasn't previously selected, it
/// automatically selects available device with priority to discrete devices.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Group that is used to add all render foundation root routine mutators.
#define KAN_RENDER_FOUNDATION_ROOT_ROUTINE_MUTATOR_GROUP "render_foundation_root_routine"

/// \brief Checkpoint, after which render foundation pass management mutators are executed.
#define KAN_RENDER_FOUNDATION_PASS_MANAGEMENT_BEGIN_CHECKPOINT "render_foundation_pass_management_begin"

/// \brief Checkpoint, that is hit after all render foundation pass management mutators finished execution.
#define KAN_RENDER_FOUNDATION_PASS_MANAGEMENT_END_CHECKPOINT "render_foundation_pass_management_end"

/// \brief Checkpoint, after which render foundation frame scheduling mutators are executed.
#define KAN_RENDER_FOUNDATION_FRAME_BEGIN "render_foundation_frame_begin"

/// \brief Checkpoint, that is hit after all render foundation frame scheduling mutators finished execution.
#define KAN_RENDER_FOUNDATION_FRAME_END "render_foundation_frame_end"

/// \brief Contains layout and binding information about single variant for pipelines inside render pass.
struct kan_render_graph_pass_variant_t
{
    /// \details Can be invalid handle when pipelines has empty parameter set layout.
    kan_render_pipeline_parameter_set_layout_t pass_parameter_set_layout;

    /// \brief Bindings meta for pass pipeline parameter set.
    struct kan_rpl_meta_set_bindings_t pass_parameter_set_bindings;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_graph_pass_variant_init (
    struct kan_render_graph_pass_variant_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_graph_pass_variant_shutdown (
    struct kan_render_graph_pass_variant_t *instance);

/// \brief Stores information about pass attachment that could be useful for outer users.
struct kan_render_graph_pass_attachment_t
{
    enum kan_render_pass_attachment_type_t type;
    enum kan_render_image_format_t format;
};

/// \brief Represents loaded and successfully created render pass.
struct kan_render_graph_pass_t
{
    kan_interned_string_t name;
    enum kan_render_pass_type_t type;
    kan_render_pass_t pass;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_graph_pass_attachment_t)
    struct kan_dynamic_array_t attachments;

    /// \brief Information about layout and binding for pass pipeline variants in the same order as in resource.
    /// \details Will be empty for passes that do not have any pass customization (no special set layout).
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_graph_pass_variant_t)
    struct kan_dynamic_array_t variants;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_graph_pass_init (struct kan_render_graph_pass_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_graph_pass_shutdown (struct kan_render_graph_pass_t *instance);

/// \brief Helper function for construction parameter set layout using meta.
/// \details Used across different routine in render foundation.
UNIVERSE_RENDER_FOUNDATION_API kan_render_pipeline_parameter_set_layout_t
kan_render_construct_parameter_set_layout_from_meta (kan_render_context_t render_context,
                                                     const struct kan_rpl_meta_set_bindings_t *meta,
                                                     kan_interned_string_t tracking_name,
                                                     kan_allocation_group_t temporary_allocation_group);

/// \brief Event that is being sent when `kan_render_graph_pass_t` is inserted or updated.
struct kan_render_graph_pass_updated_event_t
{
    kan_interned_string_t name;
};

/// \brief Event that is being sent when `kan_render_graph_pass_t` is deleted.
struct kan_render_graph_pass_deleted_event_t
{
    kan_interned_string_t name;
};

/// \brief Singleton that contains render context and used to manage access to it.
/// \details Should only be opened with write access when whole render context is modified (for example when
///          `kan_render_backend_system_next_frame` is called). For other cases like buffer instantiation
///          read access should be used.
struct kan_render_context_singleton_t
{
    kan_render_context_t render_context;

    /// \brief Whether last call to `kan_render_backend_system_next_frame` started new render frame.
    /// \details Render passes should not be created when frame is not scheduled.
    kan_bool_t frame_scheduled;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_context_singleton_init (struct kan_render_context_singleton_t *instance);

/// \brief Singleton that is used to store data for render graph resource management logic.
/// \details Providing singleton with functions seems much more convenient for the high level usage than providing
///          mutators and events that would need to be executed in every leaf world. Also, it makes render graph
///          implementation easier as graph node creation order is stable and guaranteed.
///          Users should only access these singleton with read access and should only use it to call
///          `kan_render_graph_resource_management_singleton_request`: concurrency is handled under the hood.
struct kan_render_graph_resource_management_singleton_t
{
    KAN_REFLECTION_IGNORE
    struct kan_atomic_int_t request_lock;

    KAN_REFLECTION_IGNORE
    struct kan_hash_storage_t image_cache;

    KAN_REFLECTION_IGNORE
    struct kan_hash_storage_t frame_buffer_cache;

    KAN_REFLECTION_IGNORE
    struct kan_stack_group_allocator_t temporary_allocator;

    KAN_REFLECTION_IGNORE
    kan_allocation_group_t allocation_group;

    KAN_REFLECTION_IGNORE
    kan_allocation_group_t cache_group;

    kan_interned_string_t cached_image_name;
    kan_interned_string_t cached_frame_buffer_name;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_graph_resource_management_singleton_init (
    struct kan_render_graph_resource_management_singleton_t *instance);

/// \brief Response for render graph resource request, valid during frame lifetime.
/// \details Is used not only to return resources to user, but also can be made dependant on other requests.
struct kan_render_graph_resource_response_t
{
    kan_render_pass_instance_checkpoint_t usage_begin_checkpoint;
    kan_render_pass_instance_checkpoint_t usage_end_checkpoint;

    kan_instance_size_t images_count;
    kan_render_image_t *images;

    kan_instance_size_t frame_buffers_count;
    kan_render_frame_buffer_t *frame_buffers;
};

/// \brief Describes one image request for render graph resource management.
/// \details Image must be a render target and must have only 1 mip. Allocating non-render-target image or image with
///          mips has no sense in this context.
struct kan_render_graph_resource_image_request_t
{
    struct kan_render_image_description_t description;

    /// \brief If true, then we don't expect this image to be accessed from responses that
    ///        depend on response to this request.
    /// \details Making image response-internal makes its lifetime shorter and therefore
    ///          makes it easier to reuse this image.
    kan_bool_t internal;
};

/// \brief Describes one attachment for frame buffer request.
struct kan_render_graph_resource_frame_buffer_request_attachment_t
{
    /// \brief Index of the image in image requests.
    /// \details Frame buffers can only use images that were allocate from the same resource request.
    kan_instance_size_t image_index;

    /// \brief Image layer.
    kan_instance_size_t image_layer;
};

/// \brief Describes one frame buffer request for render graph resource management.
struct kan_render_graph_resource_frame_buffer_request_t
{
    kan_render_pass_t pass;

    kan_instance_size_t attachments_count;
    struct kan_render_graph_resource_frame_buffer_request_attachment_t *attachments;
};

/// \brief Describes resource bundle request from render graph resource management.
struct kan_render_graph_resource_request_t
{
    kan_render_context_t context;

    kan_instance_size_t dependant_count;
    const struct kan_render_graph_resource_response_t **dependant;

    kan_instance_size_t images_count;
    struct kan_render_graph_resource_image_request_t *images;

    kan_instance_size_t frame_buffers_count;
    struct kan_render_graph_resource_frame_buffer_request_t *frame_buffers;
};

/// \brief Executes resource bundle request from render graph management. Returns valid response or `NULL` on failure.
UNIVERSE_RENDER_FOUNDATION_API const struct kan_render_graph_resource_response_t *
kan_render_graph_resource_management_singleton_request (
    const struct kan_render_graph_resource_management_singleton_t *instance,
    const struct kan_render_graph_resource_request_t *request);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_graph_resource_management_singleton_shutdown (
    struct kan_render_graph_resource_management_singleton_t *instance);

KAN_C_HEADER_END
