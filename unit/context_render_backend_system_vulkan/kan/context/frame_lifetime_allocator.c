#include <kan/context/render_backend_implementation_interface.h>

struct render_backend_frame_lifetime_allocator_t *render_backend_system_create_frame_lifetime_allocator (
    struct render_backend_system_t *system,
    enum render_backend_buffer_family_t buffer_family,
    enum kan_render_buffer_type_t buffer_type,
    vulkan_size_t page_size,
    kan_interned_string_t tracking_name)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_frame_lifetime_allocator_internal);

    struct render_backend_frame_lifetime_allocator_t *allocator = kan_allocate_batched (
        system->frame_lifetime_wrapper_allocation_group, sizeof (struct render_backend_frame_lifetime_allocator_t));

    kan_atomic_int_lock (&system->resource_registration_lock);
    kan_bd_list_add (&system->frame_lifetime_allocators, NULL, &allocator->list_node);
    kan_atomic_int_unlock (&system->resource_registration_lock);

    allocator->system = system;
    allocator->first_page = NULL;
    allocator->last_page = NULL;
    allocator->allocation_lock = kan_atomic_int_init (0);

    KAN_ASSERT (buffer_family == RENDER_BACKEND_BUFFER_FAMILY_STAGING ||
                buffer_family == RENDER_BACKEND_BUFFER_FAMILY_HOST_FRAME_LIFETIME_ALLOCATOR ||
                buffer_family == RENDER_BACKEND_BUFFER_FAMILY_DEVICE_FRAME_LIFETIME_ALLOCATOR)
    allocator->buffer_family = buffer_family;
    allocator->buffer_type = buffer_type;
    allocator->page_size = page_size;
    allocator->tracking_name = tracking_name;

    char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
    snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "ForFLA::%s", allocator->tracking_name);
    allocator->buffer_tracking_name = kan_string_intern (debug_name);

    kan_cpu_section_execution_shutdown (&execution);
    return allocator;
}

struct render_backend_frame_lifetime_allocator_allocation_t render_backend_frame_lifetime_allocator_allocate_on_page (
    struct render_backend_frame_lifetime_allocator_t *allocator,
    struct render_backend_frame_lifetime_allocator_page_t *page,
    vulkan_size_t size,
    vulkan_size_t alignment)
{
    struct render_backend_frame_lifetime_allocator_allocation_t result = {
        .buffer = NULL,
        .offset = 0u,
    };

    struct render_backend_frame_lifetime_allocator_chunk_t *previous_chunk = NULL;
    struct render_backend_frame_lifetime_allocator_chunk_t *chunk = page->first_free_chunk;

    while (chunk)
    {
        KAN_ASSERT (chunk->occupied_by_frame == CHUNK_FREE_MARKER)
        const vulkan_size_t memory_offset = chunk->offset;
        const vulkan_size_t allocation_offset = (vulkan_size_t) kan_apply_alignment (memory_offset, alignment);
        const vulkan_size_t allocation_size = size + allocation_offset - memory_offset;

        if (chunk->size >= allocation_size)
        {
            // Can allocate from here.
            result.buffer = page->buffer;
            result.offset = allocation_offset;

            chunk->offset += allocation_size;
            chunk->size -= allocation_size;

            if (chunk->previous &&
                chunk->previous->occupied_by_frame == allocator->system->current_frame_in_flight_index)
            {
                // Previous chunk is associated with current frame, increase it by allocation.
                chunk->previous->size += allocation_size;

                // If current chunk is fully consumed by this operation, delete it.
                if (chunk->size == 0u)
                {
                    chunk->previous->next = chunk->next;
                    if (chunk->next)
                    {
                        chunk->next->previous = chunk->previous;
                    }

                    if (previous_chunk)
                    {
                        previous_chunk->next_free = chunk->next_free;
                    }
                    else
                    {
                        KAN_ASSERT (page->first_free_chunk)
                        page->first_free_chunk = chunk->next_free;
                    }

                    kan_free_batched (allocator->system->frame_lifetime_wrapper_allocation_group, chunk);
                }
            }
            else if (chunk->size == 0u)
            {
                // Chunk was fully used by this allocation. Undo it and mark chunk as occupied by current frame.
                chunk->offset = memory_offset;
                chunk->size = allocation_size;
                chunk->occupied_by_frame = allocator->system->current_frame_in_flight_index;

                // Remove from free list.
                if (previous_chunk)
                {
                    previous_chunk->next_free = chunk->next_free;
                }
                else
                {
                    KAN_ASSERT (page->first_free_chunk)
                    page->first_free_chunk = chunk->next_free;
                }
            }
            else
            {
                // We need to allocate new chunk in order to represent this allocation.
                struct render_backend_frame_lifetime_allocator_chunk_t *new_chunk =
                    kan_allocate_batched (allocator->system->frame_lifetime_wrapper_allocation_group,
                                          sizeof (struct render_backend_frame_lifetime_allocator_chunk_t));

                new_chunk->offset = memory_offset;
                new_chunk->size = allocation_size;
                new_chunk->occupied_by_frame = allocator->system->current_frame_in_flight_index;

                new_chunk->next_free = NULL;
                new_chunk->next = chunk;
                new_chunk->previous = chunk->previous;
                chunk->previous = new_chunk;

                if (new_chunk->previous)
                {
                    new_chunk->previous->next = new_chunk;
                }
                else
                {
                    KAN_ASSERT (page->first_chunk == chunk)
                    page->first_chunk = new_chunk;
                }
            }

            return result;
        }

        previous_chunk = chunk;
        chunk = chunk->next_free;
    }

    return result;
}

struct render_backend_frame_lifetime_allocator_allocation_t render_backend_frame_lifetime_allocator_allocate (
    struct render_backend_frame_lifetime_allocator_t *allocator, vulkan_size_t size, vulkan_size_t alignment)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, allocator->system->section_frame_lifetime_allocator_allocate);

    struct render_backend_frame_lifetime_allocator_allocation_t result = {
        .buffer = NULL,
        .offset = 0u,
    };

    if (size > allocator->page_size)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Frame-lifetime allocator \"%s\": Caught attempt to allocate data of size %llu which is greater than "
                 "page size %llu.",
                 allocator->tracking_name, (unsigned long long) size, (unsigned long long) allocator->page_size)

        kan_cpu_section_execution_shutdown (&execution);
        return result;
    }

    KAN_ATOMIC_INT_SCOPED_LOCK (&allocator->allocation_lock)
    struct render_backend_frame_lifetime_allocator_page_t *page = allocator->first_page;

    while (page)
    {
        result = render_backend_frame_lifetime_allocator_allocate_on_page (allocator, page, size, alignment);
        if (result.buffer)
        {
            kan_cpu_section_execution_shutdown (&execution);
            return result;
        }

        page = page->next;
    }

    // No space on existing pages, need to create new one.
    struct render_backend_buffer_t *new_page_buffer =
        render_backend_system_create_buffer (allocator->system, allocator->buffer_family, allocator->buffer_type,
                                             allocator->page_size, allocator->buffer_tracking_name);

    if (!new_page_buffer)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Frame-lifetime allocator \"%s\": Failed to allocate %llu bytes as all pages are full and new page "
                 "allocation has failed.",
                 allocator->tracking_name, (unsigned long long) size)

        kan_cpu_section_execution_shutdown (&execution);
        return result;
    }

    struct render_backend_frame_lifetime_allocator_page_t *new_page =
        kan_allocate_batched (allocator->system->frame_lifetime_wrapper_allocation_group,
                              sizeof (struct render_backend_frame_lifetime_allocator_page_t));

    new_page->next = NULL;
    if (allocator->last_page)
    {
        allocator->last_page->next = new_page;
    }
    else
    {
        allocator->first_page = new_page;
    }

    allocator->last_page = new_page;
    new_page->buffer = new_page_buffer;

    new_page->first_chunk = kan_allocate_batched (allocator->system->frame_lifetime_wrapper_allocation_group,
                                                  sizeof (struct render_backend_frame_lifetime_allocator_chunk_t));
    new_page->first_free_chunk = new_page->first_chunk;

    new_page->first_chunk->next = NULL;
    new_page->first_chunk->previous = NULL;
    new_page->first_chunk->next_free = NULL;
    new_page->first_chunk->offset = 0u;
    new_page->first_chunk->size = allocator->page_size;
    new_page->first_chunk->occupied_by_frame = CHUNK_FREE_MARKER;

    result = render_backend_frame_lifetime_allocator_allocate_on_page (allocator, new_page, size, alignment);
    kan_cpu_section_execution_shutdown (&execution);
    return result;
}

struct render_backend_frame_lifetime_allocator_allocation_t render_backend_system_allocate_for_staging (
    struct render_backend_system_t *system, vulkan_size_t size)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_allocate_for_staging);

    if (size <= system->staging_frame_lifetime_allocator->page_size)
    {
        // Size is not overwhelmingly big, so we can use staging frame lifetime allocator.
        struct render_backend_frame_lifetime_allocator_allocation_t allocation =
            render_backend_frame_lifetime_allocator_allocate (system->staging_frame_lifetime_allocator, size,
                                                              STAGING_BUFFER_ALLOCATION_ALIGNMENT);
        kan_cpu_section_execution_shutdown (&execution);
        return allocation;
    }

    // Size is super big, we need separate buffer.
    struct render_backend_buffer_t *buffer = render_backend_system_create_buffer (
        system, RENDER_BACKEND_BUFFER_FAMILY_STAGING, KAN_RENDER_BUFFER_TYPE_STORAGE, size,
        // Very rare, might as well call kan_string_intern directly.
        kan_string_intern ("temporary_staging_buffer"));

    if (!buffer)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Failed to stage %llu bytes: allocation is too big for staging allocator and there is no memory for "
                 "separate staging buffer.",
                 (unsigned long long) size)

        kan_cpu_section_execution_shutdown (&execution);
        return (struct render_backend_frame_lifetime_allocator_allocation_t) {
            .buffer = NULL,
            .offset = 0u,
        };
    }

    // Schedule deletion of temporary buffer.
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (system);
    KAN_ATOMIC_INT_SCOPED_LOCK (&schedule->schedule_lock)

    struct scheduled_buffer_destroy_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_buffer_destroy_t);

    // We do not need to preserve order as buffers cannot depend one on another.
    item->next = schedule->first_scheduled_buffer_destroy;
    schedule->first_scheduled_buffer_destroy = item;
    item->buffer = buffer;

    kan_cpu_section_execution_shutdown (&execution);
    return (struct render_backend_frame_lifetime_allocator_allocation_t) {
        .buffer = buffer,
        .offset = 0u,
    };
}

void render_backend_frame_lifetime_allocator_retire_old_allocations (
    struct render_backend_frame_lifetime_allocator_t *allocator)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution,
                                    allocator->system->section_frame_lifetime_allocator_retire_old_allocations);
    struct render_backend_frame_lifetime_allocator_page_t *page = allocator->first_page;

    while (page)
    {
        struct render_backend_frame_lifetime_allocator_chunk_t *chunk = page->first_chunk;
        while (chunk)
        {
            struct render_backend_frame_lifetime_allocator_chunk_t *next_chunk = chunk->next;
            if (chunk->occupied_by_frame == allocator->system->current_frame_in_flight_index)
            {
                // Chunk should retire as its data lifetime has ended.
                chunk->occupied_by_frame = CHUNK_FREE_MARKER;
            }

            if (chunk->occupied_by_frame == CHUNK_FREE_MARKER && chunk->previous &&
                chunk->previous->occupied_by_frame == CHUNK_FREE_MARKER)
            {
                // We can merge this chunk into previous free chunk.
                chunk->previous->size += chunk->size;
                chunk->previous->next = chunk->next;

                if (chunk->next)
                {
                    chunk->next->previous = chunk->previous;
                }

                kan_free_batched (allocator->system->frame_lifetime_wrapper_allocation_group, chunk);
            }

            chunk = next_chunk;
        }

        page->first_free_chunk = NULL;
        chunk = page->first_chunk;

        while (chunk)
        {
            if (chunk->occupied_by_frame == CHUNK_FREE_MARKER)
            {
                chunk->next_free = page->first_free_chunk;
                page->first_free_chunk = chunk;
            }

            chunk = chunk->next;
        }

        page = page->next;
    }

    kan_cpu_section_execution_shutdown (&execution);
}

static inline void render_backend_frame_lifetime_allocator_destroy_page (
    struct render_backend_system_t *system,
    struct render_backend_frame_lifetime_allocator_page_t *page,
    bool destroy_buffers)
{
    if (destroy_buffers)
    {
        kan_bd_list_remove (&system->buffers, &page->buffer->list_node);
        render_backend_system_destroy_buffer (system, page->buffer);
    }

    struct render_backend_frame_lifetime_allocator_chunk_t *chunk = page->first_chunk;
    while (chunk)
    {
        struct render_backend_frame_lifetime_allocator_chunk_t *next_chunk = chunk->next;
        kan_free_batched (system->frame_lifetime_wrapper_allocation_group, chunk);
        chunk = next_chunk;
    }

    kan_free_batched (system->frame_lifetime_wrapper_allocation_group, page);
}

void render_backend_frame_lifetime_allocator_clean_empty_pages (
    struct render_backend_frame_lifetime_allocator_t *allocator)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, allocator->system->section_frame_lifetime_allocator_clean_empty_pages);
    struct render_backend_frame_lifetime_allocator_page_t *previous_page = NULL;
    struct render_backend_frame_lifetime_allocator_page_t *page = allocator->first_page;

    while (page)
    {
        struct render_backend_frame_lifetime_allocator_page_t *next_page = page->next;
        if (page->first_free_chunk && page->first_free_chunk->offset == 0u &&
            page->first_free_chunk->size == allocator->page_size)
        {
            KAN_ASSERT (!page->first_free_chunk->next_free)
            // Looks like page is empty. Lets get rid of it.

            if (previous_page)
            {
                previous_page->next = next_page;
            }
            else
            {
                KAN_ASSERT (page == allocator->first_page)
                allocator->first_page = page->next;
            }

            if (!page->next)
            {
                allocator->last_page = previous_page;
            }

            render_backend_frame_lifetime_allocator_destroy_page (allocator->system, page, true);
        }
        else
        {
            // Page is not empty, we should keep it.
            previous_page = page;
        }

        page = next_page;
    }

    kan_cpu_section_execution_shutdown (&execution);
}

void render_backend_system_destroy_frame_lifetime_allocator (
    struct render_backend_system_t *system,
    struct render_backend_frame_lifetime_allocator_t *frame_lifetime_allocator,
    bool destroy_buffers)
{
    struct render_backend_frame_lifetime_allocator_page_t *page = frame_lifetime_allocator->first_page;
    while (page)
    {
        struct render_backend_frame_lifetime_allocator_page_t *next = page->next;
        render_backend_frame_lifetime_allocator_destroy_page (system, page, destroy_buffers);
        page = next;
    }

    kan_free_batched (system->frame_lifetime_wrapper_allocation_group, frame_lifetime_allocator);
}

kan_render_frame_lifetime_buffer_allocator_t kan_render_frame_lifetime_buffer_allocator_create (
    kan_render_context_t context,
    enum kan_render_buffer_type_t buffer_type,
    vulkan_size_t page_size,
    bool on_device,
    kan_interned_string_t tracking_name)
{
    // Frame-lifetime read back just doesn't make sense as it would be destroyed right after collecting data.
    KAN_ASSERT (buffer_type != KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE)

    struct render_backend_system_t *system = KAN_HANDLE_GET (context);
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_frame_lifetime_allocator);

    struct render_backend_frame_lifetime_allocator_t *allocator =
        render_backend_system_create_frame_lifetime_allocator (
            system,
            on_device ? RENDER_BACKEND_BUFFER_FAMILY_DEVICE_FRAME_LIFETIME_ALLOCATOR :
                        RENDER_BACKEND_BUFFER_FAMILY_HOST_FRAME_LIFETIME_ALLOCATOR,
            buffer_type, page_size, tracking_name);

    kan_cpu_section_execution_shutdown (&execution);
    return KAN_HANDLE_SET (kan_render_frame_lifetime_buffer_allocator_t, allocator);
}

struct kan_render_allocated_slice_t kan_render_frame_lifetime_buffer_allocator_allocate (
    kan_render_frame_lifetime_buffer_allocator_t allocator, vulkan_size_t size, vulkan_size_t alignment)
{
    struct render_backend_frame_lifetime_allocator_t *data = KAN_HANDLE_GET (allocator);
    KAN_ASSERT (data->system->frame_started)

    struct render_backend_frame_lifetime_allocator_allocation_t allocation =
        render_backend_frame_lifetime_allocator_allocate (data, size, alignment);

    return (struct kan_render_allocated_slice_t) {
        .buffer = KAN_HANDLE_SET (kan_render_buffer_t, allocation.buffer),
        .slice_offset = allocation.offset,
    };
}

void kan_render_frame_lifetime_buffer_allocator_destroy (kan_render_frame_lifetime_buffer_allocator_t allocator)
{
    struct render_backend_frame_lifetime_allocator_t *data = KAN_HANDLE_GET (allocator);
    // Only resource family buffers can be destroyed externally through scheduling.
    KAN_ASSERT (data->buffer_family == RENDER_BACKEND_BUFFER_FAMILY_HOST_FRAME_LIFETIME_ALLOCATOR ||
                data->buffer_family == RENDER_BACKEND_BUFFER_FAMILY_DEVICE_FRAME_LIFETIME_ALLOCATOR)

    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    KAN_ATOMIC_INT_SCOPED_LOCK (&schedule->schedule_lock)

    struct scheduled_frame_lifetime_allocator_destroy_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &schedule->item_allocator, struct scheduled_frame_lifetime_allocator_destroy_t);

    // We do not need to preserve order as frame_lifetime_allocators cannot depend one on another.
    item->next = schedule->first_scheduled_frame_lifetime_allocator_destroy;
    schedule->first_scheduled_frame_lifetime_allocator_destroy = item;
    item->frame_lifetime_allocator = data;
}
