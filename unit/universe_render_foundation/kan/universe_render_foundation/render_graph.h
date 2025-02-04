#pragma once

#include <universe_render_foundation_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/hash_storage.h>
#include <kan/context/render_backend_system.h>
#include <kan/inline_math/inline_math.h>
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
/// for tools, but are not needed for the game (for example, editor-only passes), should be stored in appropriate
/// resource directories that are visible for the editor and invisible for the game. Pass hot reload is done
/// automatically when needed.
/// \endparblock
///
/// \par Render graph
/// \parblock
/// Render graph goal is to reduce GPU resource usage by pass instances in complex cases like split-screen rendering or
/// rendering different worlds (which is a common case for the editor). It is done by tracking resource usage graph and
/// inserting additional dependencies to passes to get rid of unneeded parallelism. For example, we don't need to draw
/// two scenes in parallel as GPU would rarely benefit from it. And if we get rid of this parallelism, we might be able
/// to reuse depth texture. Render graph algorithm detects situations like that and introduces dependencies in order
/// to make resource reuse possible.
///
/// Also, render graph caches render target images and frame buffers between frames, automatically destroying them if
/// they're no longer used.
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

/// \brief Represents data structure of resource that describes render pass.
struct kan_resource_render_graph_pass_t
{
    /// \brief Render pass type.
    enum kan_render_pass_type_t type;

    /// \brief List of render pass attachments and their descriptions.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_pass_attachment_t)
    struct kan_dynamic_array_t attachments;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_resource_render_graph_pass_init (
    struct kan_resource_render_graph_pass_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_resource_render_graph_pass_shutdown (
    struct kan_resource_render_graph_pass_t *instance);

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
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_graph_pass_init (struct kan_render_graph_pass_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_graph_pass_shutdown (struct kan_render_graph_pass_t *instance);

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

/// \brief Singleton that is used to store data for render graph resource management and pass instantiation logic.
/// \details Providing singleton with functions seems much more convenient for the high level usage than providing
///          mutators and events that would need to be executed in every leaf world. Also, it makes render graph
///          implementation easier as graph node creation order is stable and guaranteed.
///          Users should only access these singleton with read access and should only use it to call
///          `kan_render_graph_resource_management_singleton_request_pass`: concurrency is handled under the hood.
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

// TODO: We'll need layered image support later (when it is ready in the render backend and render pipeline language).
//       Usually, layered images are produced from the family of pass instances.
//       For example cubemap is produced from 6 shadow map passes and them consumed as one cubemap image.
//       Therefore, in the future, we would need to be able to accept multi-pass-instance requests which would
//       automatically mean that all the requests share layered images as outputs. For example,

/// \brief Contains information about one attachment for the render pass instantiation.
struct kan_render_graph_pass_instance_request_attachment_info_t
{
    /// \brief If not invalid, surface will be used instead of render target image.
    kan_render_surface_t use_surface;

    /// \brief If true, dependant pass instances will be registered as consumers and will be guaranteed to be able to
    ///        properly access generated render target texture.
    /// \invariant Incompatible with ::use_surface.
    kan_bool_t used_by_dependant_instances;
};

/// \brief Contains data that describes pass instantiation request.
struct kan_render_graph_pass_instance_request_t
{
    /// \brief Render context for pass instantiation.
    kan_render_context_t context;

    /// \brief Pointer to render pass instance with read access.
    const struct kan_render_graph_pass_t *pass;

    /// \brief Width for the full frame buffer.
    /// \details Will be different from viewport bounds if several viewports share an output.
    ///          For example, for split screen.
    kan_render_size_t frame_buffer_width;

    /// \brief Height for the full frame buffer.
    /// \details Will be different from viewport bounds if several viewports share an output.
    ///          For example, for split screen.
    kan_render_size_t frame_buffer_height;

    /// \brief Array of additional information for attachments.
    /// \details Must have the same size as `kan_render_graph_pass_t::attachments`.
    struct kan_render_graph_pass_instance_request_attachment_info_t *attachment_info;

    /// \brief Count of render pass instances that depend on outputs from requested pass.
    kan_instance_size_t dependant_count;

    /// \brief Array of render pass instances that depend on outputs from requested pass.
    kan_render_pass_instance_t *dependant;

    /// \brief Argument for `kan_render_pass_instantiate`.
    struct kan_render_viewport_bounds_t *viewport_bounds;

    /// \brief Argument for `kan_render_pass_instantiate`.
    struct kan_render_integer_region_t *scissor;

    /// \brief Argument for `kan_render_pass_instantiate`.
    struct kan_render_clear_value_t *attachment_clear_values;
};

/// \brief Contains results of a successful pass instance allocation.
struct kan_render_graph_pass_instance_allocation_t
{
    /// \brief Created pass instance.
    kan_render_pass_instance_t pass_instance;

    /// \brief Count of pass attachments.
    kan_instance_size_t attachments_count;

    /// \brief Concrete attachments that are passed to the instance through frame buffer.
    /// \details Stored in render graph and should not be freed manually. Pointer is guaranteed to be accessible only
    ///          during the allocation frame, therefore this raw pointer should not be exposed and should not be
    ///          stored somewhere.
    struct kan_render_frame_buffer_attachment_description_t *attachments;
};

/// \brief Attempts to create new pass instance and properly allocate resources for it.
UNIVERSE_RENDER_FOUNDATION_API kan_bool_t kan_render_graph_resource_management_singleton_request_pass (
    const struct kan_render_graph_resource_management_singleton_t *instance,
    const struct kan_render_graph_pass_instance_request_t *request,
    struct kan_render_graph_pass_instance_allocation_t *output);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_graph_resource_management_singleton_shutdown (
    struct kan_render_graph_resource_management_singleton_t *instance);

KAN_C_HEADER_END
