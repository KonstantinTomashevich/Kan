#pragma once

#include <universe_render_foundation_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/context/render_backend_system.h>
#include <kan/inline_math/inline_math.h>
#include <kan/threading/atomic.h>
#include <kan/universe/universe.h>
#include <kan/universe_object/universe_object.h>

KAN_C_HEADER_BEGIN

#define KAN_RENDER_FOUNDATION_ROOT_ROUTINE_MUTATOR_GROUP "render_foundation_root_routine"

#define KAN_RENDER_FOUNDATION_PASS_MANAGEMENT_BEGIN_CHECKPOINT "render_foundation_pass_management_begin"

#define KAN_RENDER_FOUNDATION_PASS_MANAGEMENT_END_CHECKPOINT "render_foundation_pass_management_end"

#define KAN_RENDER_FOUNDATION_FRAME_BEGIN "render_foundation_frame_begin"

#define KAN_RENDER_FOUNDATION_FRAME_END "render_foundation_frame_end"

struct kan_render_graph_pass_resource_t
{
    enum kan_render_pass_type_t type;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_pass_attachment_t)
    struct kan_dynamic_array_t attachments;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_graph_pass_resource_init (
    struct kan_render_graph_pass_resource_t *instance);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_graph_pass_resource_shutdown (
    struct kan_render_graph_pass_resource_t *instance);

struct kan_render_graph_pass_attachment_t
{
    enum kan_render_pass_attachment_type_t type;
    enum kan_render_image_format_t format;
};

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

/// \details Should only be opened with write access when whole render context is modified (for example when
///          `kan_render_backend_system_next_frame` is called). For other cases like buffer instantiation use
///          read access,
struct kan_render_context_singleton_t
{
    kan_render_context_t render_context;

    kan_bool_t frame_scheduled;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_render_context_singleton_init (struct kan_render_context_singleton_t *instance);

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

struct kan_render_graph_pass_instance_request_attachment_info_t
{
    kan_render_surface_t use_surface;
    kan_bool_t used_by_dependant_instances;
};

struct kan_render_graph_pass_instance_request_t
{
    kan_render_context_t context;
    struct kan_render_graph_pass_t *pass;

    kan_render_size_t frame_buffer_width;
    kan_render_size_t frame_buffer_height;
    struct kan_render_graph_pass_instance_request_attachment_info_t *attachment_info;

    kan_instance_size_t dependant_count;
    kan_render_pass_instance_t *dependant;

    struct kan_render_viewport_bounds_t *viewport_bounds;
    struct kan_render_integer_region_t *scissor;
    struct kan_render_clear_value_t *attachment_clear_values;
};

struct kan_render_graph_pass_instance_allocation_t
{
    kan_render_pass_instance_t pass_instance;
    kan_instance_size_t attachments_count;
    struct kan_render_frame_buffer_attachment_description_t *attachments;
};

UNIVERSE_RENDER_FOUNDATION_API kan_bool_t kan_render_graph_resource_management_singleton_request_pass (
    const struct kan_render_graph_resource_management_singleton_t *instance,
    const struct kan_render_graph_pass_instance_request_t *request,
    struct kan_render_graph_pass_instance_allocation_t *output);

UNIVERSE_RENDER_FOUNDATION_API void kan_render_graph_resource_management_singleton_shutdown (
    struct kan_render_graph_resource_management_singleton_t *instance);

KAN_C_HEADER_END
