#include <kan/context/render_backend_implementation_interface.h>

uint64_t kan_render_get_read_back_max_delay_in_frames (void)
{
    return KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT + 1u;
}

static inline struct render_backend_read_back_status_t *create_empty_status (struct render_backend_system_t *system)
{
    struct render_backend_read_back_status_t *status = kan_allocate_batched (
        system->read_back_status_allocation_group, sizeof (struct render_backend_read_back_status_t));

    status->system = system;
    status->state = KAN_RENDER_READ_BACK_STATE_REQUESTED;
    status->referenced_in_schedule = KAN_TRUE;
    status->referenced_outside = KAN_TRUE;
    return status;
}

kan_render_read_back_status_t kan_render_read_back_request_from_surface (kan_render_surface_t surface,
                                                                         kan_render_buffer_t read_back_buffer,
                                                                         uint32_t read_back_offset)
{
    struct render_backend_surface_t *surface_data = (struct render_backend_surface_t *) surface;
    KAN_ASSERT (surface_data->system->frame_started)

    struct render_backend_buffer_t *read_back_buffer_data = (struct render_backend_buffer_t *) read_back_buffer;
    KAN_ASSERT (read_back_buffer_data->type == KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE)

    struct render_backend_read_back_status_t *status = create_empty_status (surface_data->system);
    struct render_backend_schedule_state_t *schedule =
        render_backend_system_get_schedule_for_memory (surface_data->system);

    kan_atomic_int_lock (&schedule->schedule_lock);
    status->next = schedule->first_read_back_status;
    schedule->first_read_back_status = status;

    struct scheduled_surface_read_back_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_surface_read_back_t);

    item->next = schedule->first_scheduled_surface_read_back;
    schedule->first_scheduled_surface_read_back = item;
    item->surface = surface_data;
    item->read_back_buffer = read_back_buffer_data;
    item->read_back_offset = read_back_offset;
    item->status = status;

    kan_atomic_int_unlock (&schedule->schedule_lock);
    return (kan_render_read_back_status_t) status;
}

kan_render_read_back_status_t kan_render_read_back_request_from_buffer (kan_render_buffer_t buffer,
                                                                        uint32_t offset,
                                                                        uint32_t slice,
                                                                        kan_render_buffer_t read_back_buffer,
                                                                        uint32_t read_back_offset)
{
    struct render_backend_buffer_t *buffer_data = (struct render_backend_buffer_t *) buffer;
    KAN_ASSERT (buffer_data->system->frame_started)

    struct render_backend_buffer_t *read_back_buffer_data = (struct render_backend_buffer_t *) read_back_buffer;
    KAN_ASSERT (read_back_buffer_data->type == KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE)

    struct render_backend_read_back_status_t *status = create_empty_status (buffer_data->system);
    struct render_backend_schedule_state_t *schedule =
        render_backend_system_get_schedule_for_memory (buffer_data->system);

    kan_atomic_int_lock (&schedule->schedule_lock);
    status->next = schedule->first_read_back_status;
    schedule->first_read_back_status = status;

    struct scheduled_buffer_read_back_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_buffer_read_back_t);

    item->next = schedule->first_scheduled_buffer_read_back;
    schedule->first_scheduled_buffer_read_back = item;
    item->buffer = buffer_data;
    item->offset = offset;
    item->slice = slice;
    item->read_back_buffer = read_back_buffer_data;
    item->read_back_offset = read_back_offset;
    item->status = status;

    kan_atomic_int_unlock (&schedule->schedule_lock);
    return (kan_render_read_back_status_t) status;
}

kan_render_read_back_status_t kan_render_read_back_request_from_image (kan_render_image_t image,
                                                                       uint8_t mip,
                                                                       kan_render_buffer_t read_back_buffer,
                                                                       uint32_t read_back_offset)
{
    struct render_backend_image_t *image_data = (struct render_backend_image_t *) image;
    KAN_ASSERT (image_data->system->frame_started)

    struct render_backend_buffer_t *read_back_buffer_data = (struct render_backend_buffer_t *) read_back_buffer;
    KAN_ASSERT (read_back_buffer_data->type == KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE)

    struct render_backend_read_back_status_t *status = create_empty_status (image_data->system);
    struct render_backend_schedule_state_t *schedule =
        render_backend_system_get_schedule_for_memory (image_data->system);

    kan_atomic_int_lock (&schedule->schedule_lock);
    status->next = schedule->first_read_back_status;
    schedule->first_read_back_status = status;

    struct scheduled_image_read_back_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_image_read_back_t);

    item->next = schedule->first_scheduled_image_read_back;
    schedule->first_scheduled_image_read_back = item;
    item->image = image_data;
    item->mip = mip;
    item->read_back_buffer = read_back_buffer_data;
    item->read_back_offset = read_back_offset;
    item->status = status;

    kan_atomic_int_unlock (&schedule->schedule_lock);
    return (kan_render_read_back_status_t) status;
}

enum kan_render_read_back_state_t kan_read_read_back_status_get (kan_render_read_back_status_t status)
{
    return ((struct render_backend_read_back_status_t *) status)->state;
}

void kan_render_read_back_status_destroy (kan_render_read_back_status_t status)
{
    struct render_backend_read_back_status_t *data = (struct render_backend_read_back_status_t *) status;
    data->referenced_outside = KAN_FALSE;

    if (!data->referenced_in_schedule)
    {
        kan_free_batched (data->system->read_back_status_allocation_group, data);
    }
}