#include <kan/context/render_backend_implementation_interface.h>

struct render_backend_frame_buffer_t *render_backend_system_create_frame_buffer (
    struct render_backend_system_t *system, struct kan_render_frame_buffer_description_t *description)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_frame_buffer_internal);

    struct render_backend_frame_buffer_t *buffer = kan_allocate_batched (system->frame_buffer_wrapper_allocation_group,
                                                                         sizeof (struct render_backend_frame_buffer_t));

    kan_atomic_int_lock (&system->resource_registration_lock);
    kan_bd_list_add (&system->frame_buffers, NULL, &buffer->list_node);
    kan_atomic_int_unlock (&system->resource_registration_lock);

    buffer->system = system;
    buffer->instance = VK_NULL_HANDLE;
    buffer->image_views = NULL;

    buffer->pass = KAN_HANDLE_GET (description->associated_pass);
    buffer->tracking_name = description->tracking_name;

    buffer->attachments_count = description->attachment_count;
    buffer->attachments =
        kan_allocate_general (system->frame_buffer_wrapper_allocation_group,
                              sizeof (struct render_backend_frame_buffer_attachment_t) * buffer->attachments_count,
                              _Alignof (struct render_backend_frame_buffer_attachment_t));

    for (kan_loop_size_t index = 0u; index < buffer->attachments_count; ++index)
    {
        struct kan_render_frame_buffer_attachment_description_t *source = &description->attachments[index];
        struct render_backend_frame_buffer_attachment_t *target = &buffer->attachments[index];
        target->image = KAN_HANDLE_GET (source->image);
        target->layer = source->layer;

        struct image_frame_buffer_attachment_t *attachment = kan_allocate_batched (
            system->image_wrapper_allocation_group, sizeof (struct image_frame_buffer_attachment_t));

        attachment->next = target->image->first_frame_buffer_attachment;
        target->image->first_frame_buffer_attachment = attachment;
        attachment->frame_buffer = buffer;
    }

    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_memory (system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_frame_buffer_create_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_frame_buffer_create_t);

    // We do not need to preserve order as frame buffers cannot depend one on another.
    item->next = schedule->first_scheduled_frame_buffer_create;
    schedule->first_scheduled_frame_buffer_create = item;
    item->frame_buffer = buffer;
    kan_atomic_int_unlock (&schedule->schedule_lock);

    kan_cpu_section_execution_shutdown (&execution);
    return buffer;
}

void render_backend_frame_buffer_destroy_resources (struct render_backend_system_t *system,
                                                    struct render_backend_frame_buffer_t *frame_buffer)
{
    if (frame_buffer->instance != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer (system->device, frame_buffer->instance, VULKAN_ALLOCATION_CALLBACKS (system));
        frame_buffer->instance = VK_NULL_HANDLE;
    }

    if (frame_buffer->image_views)
    {
        for (kan_loop_size_t index = 0u; index < frame_buffer->attachments_count; ++index)
        {
            if (frame_buffer->image_views[index] != VK_NULL_HANDLE)
            {
                vkDestroyImageView (system->device, frame_buffer->image_views[index],
                                    VULKAN_ALLOCATION_CALLBACKS (system));
            }
        }

        kan_free_general (system->frame_buffer_wrapper_allocation_group, frame_buffer->image_views,
                          sizeof (VkImageView) * frame_buffer->attachments_count);
        frame_buffer->image_views = NULL;
    }
}

void render_backend_frame_buffer_schedule_resource_destroy (struct render_backend_system_t *system,
                                                            struct render_backend_frame_buffer_t *frame_buffer,
                                                            struct render_backend_schedule_state_t *schedule)
{
    if (frame_buffer->instance != VK_NULL_HANDLE)
    {
        struct scheduled_detached_frame_buffer_destroy_t *frame_buffer_destroy =
            KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator,
                                                      struct scheduled_detached_frame_buffer_destroy_t);
        frame_buffer_destroy->next = schedule->first_scheduled_detached_frame_buffer_destroy;
        schedule->first_scheduled_detached_frame_buffer_destroy = frame_buffer_destroy;
        frame_buffer_destroy->detached_frame_buffer = frame_buffer->instance;
        frame_buffer->instance = VK_NULL_HANDLE;
    }

    if (frame_buffer->image_views)
    {
        for (kan_loop_size_t index = 0u; index < frame_buffer->attachments_count; ++index)
        {
            if (frame_buffer->image_views[index] != VK_NULL_HANDLE)
            {
                struct scheduled_detached_image_view_destroy_t *image_view_destroy =
                    KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator,
                                                              struct scheduled_detached_image_view_destroy_t);
                image_view_destroy->next = schedule->first_scheduled_detached_image_view_destroy;
                schedule->first_scheduled_detached_image_view_destroy = image_view_destroy;
                image_view_destroy->detached_image_view = frame_buffer->image_views[index];
            }
        }

        kan_free_general (system->frame_buffer_wrapper_allocation_group, frame_buffer->image_views,
                          sizeof (VkImageView) * frame_buffer->attachments_count);
        frame_buffer->image_views = NULL;
    }
}

void render_backend_system_destroy_frame_buffer (struct render_backend_system_t *system,
                                                 struct render_backend_frame_buffer_t *frame_buffer)
{
    // Detach from surface and images if attachment is created.
    for (kan_loop_size_t index = 0u; index < frame_buffer->attachments_count; ++index)
    {
        struct render_backend_frame_buffer_attachment_t *attachment = &frame_buffer->attachments[index];
        struct image_frame_buffer_attachment_t *previous = NULL;
        struct image_frame_buffer_attachment_t *current = attachment->image->first_frame_buffer_attachment;

        while (current)
        {
            if (current->frame_buffer == frame_buffer)
            {
                if (previous)
                {
                    previous->next = current->next;
                }
                else
                {
                    KAN_ASSERT (attachment->image->first_frame_buffer_attachment == current)
                    attachment->image->first_frame_buffer_attachment = current->next;
                }

                kan_free_batched (system->image_wrapper_allocation_group, current);
                break;
            }

            previous = current;
            current = current->next;
        }
    }

    render_backend_frame_buffer_destroy_resources (system, frame_buffer);
    kan_free_general (system->frame_buffer_wrapper_allocation_group, frame_buffer->attachments,
                      sizeof (struct render_backend_frame_buffer_attachment_t) * frame_buffer->attachments_count);
    kan_free_batched (system->frame_buffer_wrapper_allocation_group, frame_buffer);
}

kan_render_frame_buffer_t kan_render_frame_buffer_create (kan_render_context_t context,
                                                          struct kan_render_frame_buffer_description_t *description)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (context);
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_frame_buffer);

    struct render_backend_frame_buffer_t *frame_buffer =
        render_backend_system_create_frame_buffer (system, description);
    kan_cpu_section_execution_shutdown (&execution);
    return frame_buffer ? KAN_HANDLE_SET (kan_render_frame_buffer_t, frame_buffer) :
                          KAN_HANDLE_SET_INVALID (kan_render_frame_buffer_t);
}

void kan_render_frame_buffer_destroy (kan_render_frame_buffer_t buffer)
{
    struct render_backend_frame_buffer_t *data = KAN_HANDLE_GET (buffer);
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_frame_buffer_destroy_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_frame_buffer_destroy_t);

    // We do not need to preserve order as frame buffers cannot depend one on another.
    item->next = schedule->first_scheduled_frame_buffer_destroy;
    schedule->first_scheduled_frame_buffer_destroy = item;
    item->frame_buffer = data;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}
