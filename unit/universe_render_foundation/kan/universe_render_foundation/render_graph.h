#pragma once

#include <universe_render_foundation_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/hash_storage.h>
#include <kan/universe/universe.h>
#include <kan/universe_render_foundation/program.h>

/// \file
/// \brief Provides API for render foundation render graph implementation and frame scheduling.
///
/// \par Definition
/// \parblock
/// Frame scheduling mutator handles frame execution by calling `kan_render_backend_system_next_frame` and
/// updating of frame-dependant render graph data. Also, when render backend device wasn't previously selected, it
/// automatically selects available device with priority to discrete devices. All render foundation resource management
/// tasks are executed after frame scheduling, therefore all destroys on render backend side are guaranteed to be done
/// from the same schedule, which is crucial for proper hot reload support when user-level mutators construct their own
/// GPU objects like parameter sets and manage them manually.
///
/// Render graph goal is to reduce GPU resource usage by pass instances in complex cases like split-screen rendering or
/// rendering different worlds (which is a common case for the editor). It is done by tracking resource usage graph and
/// inserting additional dependencies to checkpoints to get rid of unneeded parallelism. For example, we don't need to
/// draw two scenes in parallel as GPU would rarely benefit from it. And if we get rid of this parallelism, we might be
/// able to reuse some textures.
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

KAN_C_HEADER_BEGIN

/// \brief Group that is used to add frame scheduling mutators, should be always added to root world.
#define KAN_RENDER_FOUNDATION_FRAME_MUTATOR_GROUP "render_foundation_frame"

/// \brief Checkpoint, after which render foundation frame scheduling mutators are executed.
#define KAN_RENDER_FOUNDATION_FRAME_BEGIN_CHECKPOINT "render_foundation_frame_begin"

/// \brief Checkpoint, that is hit after all render foundation frame scheduling mutators have finished execution.
#define KAN_RENDER_FOUNDATION_FRAME_END_CHECKPOINT "render_foundation_frame_end"

/// \brief Singleton that contains render context and used to manage access to it.
/// \details Should only be opened with write access when whole render context is modified (for example when
///          `kan_render_backend_system_next_frame` is called). For other cases like buffer instantiation
///          read access should be used.
struct kan_render_context_singleton_t
{
    kan_render_context_t render_context;

    /// \brief Whether last call to `kan_render_backend_system_next_frame` started new render frame.
    /// \details Render pass instances should not be created when frame is not scheduled.
    bool frame_scheduled;

    struct kan_render_supported_device_info_t *selected_device_info;
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

    kan_allocation_group_t allocation_group;
    kan_allocation_group_t cache_group;
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
    bool internal;
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
