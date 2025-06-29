#include <kan/context/render_backend_implementation_interface.h>

struct render_backend_buffer_t *render_backend_system_create_buffer (struct render_backend_system_t *system,
                                                                     enum render_backend_buffer_family_t family,
                                                                     enum kan_render_buffer_type_t buffer_type,
                                                                     vulkan_size_t full_size,
                                                                     kan_interned_string_t tracking_name)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_buffer_internal);

    VkBufferUsageFlags usage_flags = 0u;
    VmaAllocationCreateFlagBits allocation_flags = 0u;

    switch (system->device_memory_type)
    {
    case KAN_RENDER_DEVICE_MEMORY_TYPE_SEPARATE:
        break;

    case KAN_RENDER_DEVICE_MEMORY_TYPE_UNIFIED:
    case KAN_RENDER_DEVICE_MEMORY_TYPE_UNIFIED_COHERENT:
        // All memory is unified and host visible anyway.
        allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        break;
    }

    switch (family)
    {
    case RENDER_BACKEND_BUFFER_FAMILY_RESOURCE:
    case RENDER_BACKEND_BUFFER_FAMILY_DEVICE_FRAME_LIFETIME_ALLOCATOR:
        // Transfer source might be used if read back is requested.
        usage_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        break;

    case RENDER_BACKEND_BUFFER_FAMILY_STAGING:
        usage_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        break;

    case RENDER_BACKEND_BUFFER_FAMILY_HOST_FRAME_LIFETIME_ALLOCATOR:
        allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        break;
    }

    if (family != RENDER_BACKEND_BUFFER_FAMILY_STAGING)
    {
        switch (buffer_type)
        {
        case KAN_RENDER_BUFFER_TYPE_ATTRIBUTE:
            usage_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            break;

        case KAN_RENDER_BUFFER_TYPE_INDEX_16:
        case KAN_RENDER_BUFFER_TYPE_INDEX_32:
            usage_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            break;

        case KAN_RENDER_BUFFER_TYPE_UNIFORM:
            usage_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            break;

        case KAN_RENDER_BUFFER_TYPE_STORAGE:
            usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            break;

        case KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE:
            KAN_ASSERT (family == RENDER_BACKEND_BUFFER_FAMILY_RESOURCE)
            usage_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        }
    }

    VkBufferCreateInfo buffer_create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .size = full_size,
        .usage = usage_flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1u,
        .pQueueFamilyIndices = &system->device_queue_family_index,
    };

    VmaAllocationCreateInfo allocation_create_info = {
        .flags = allocation_flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = 0u,
        .preferredFlags = 0u,
        .memoryTypeBits = UINT32_MAX,
        .pool = NULL,
        .pUserData = NULL,
        .priority = 0.0f,
    };

    VmaAllocationInfo allocation_info;
    VkBuffer buffer_handle;
    VmaAllocation allocation_handle;

    if (vmaCreateBuffer (system->gpu_memory_allocator, &buffer_create_info, &allocation_create_info, &buffer_handle,
                         &allocation_handle, &allocation_info) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to create buffer \"%s\" of size %llu.",
                 tracking_name, (unsigned long long) full_size)
        kan_cpu_section_execution_shutdown (&execution);
        return NULL;
    }

    if ((allocation_flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0u && !allocation_info.pMappedData)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Failed to map buffer \"%s\" to memory while its type required memory mapping.", tracking_name)
        kan_cpu_section_execution_shutdown (&execution);
        return NULL;
    }

    KAN_ASSERT (!allocation_info.pMappedData || (allocation_flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0u)
#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    const char *buffer_type_name = "UnknownBuffer";
    switch (buffer_type)
    {
    case KAN_RENDER_BUFFER_TYPE_ATTRIBUTE:
        buffer_type_name = "AttributeBuffer";
        break;

    case KAN_RENDER_BUFFER_TYPE_INDEX_16:
        buffer_type_name = "Index16Buffer";
        break;

    case KAN_RENDER_BUFFER_TYPE_INDEX_32:
        buffer_type_name = "Index32Buffer";
        break;

    case KAN_RENDER_BUFFER_TYPE_UNIFORM:
        buffer_type_name = "UniformBuffer";
        break;

    case KAN_RENDER_BUFFER_TYPE_STORAGE:
        buffer_type_name = "StorageBuffer";
        break;

    case KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE:
        buffer_type_name = "ReadBackStorageBuffer";
        break;
    }

    char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
    snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "%s::%s", buffer_type_name, tracking_name);

    struct VkDebugUtilsObjectNameInfoEXT object_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = NULL,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = CONVERT_HANDLE_FOR_DEBUG buffer_handle,
        .pObjectName = debug_name,
    };

    vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
#endif

    struct render_backend_buffer_t *buffer =
        kan_allocate_batched (system->buffer_wrapper_allocation_group, sizeof (struct render_backend_buffer_t));

    kan_atomic_int_lock (&system->resource_registration_lock);
    kan_bd_list_add (&system->buffers, NULL, &buffer->list_node);
    kan_atomic_int_unlock (&system->resource_registration_lock);

    buffer->system = system;
    buffer->buffer = buffer_handle;
    buffer->allocation = allocation_handle;
    buffer->family = family;
    buffer->type = buffer_type;
    buffer->mapped_memory = allocation_info.pMappedData;
    buffer->full_size = full_size;
    buffer->tracking_name = tracking_name;

    buffer->needs_flush =
        (system->selected_device_memory_properties.memoryTypes[allocation_info.memoryType].propertyFlags &
         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0u;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    kan_allocation_group_t type_allocation_group = system->memory_profiling.gpu_buffer_group;
    switch (buffer_type)
    {
    case KAN_RENDER_BUFFER_TYPE_ATTRIBUTE:
        type_allocation_group = system->memory_profiling.gpu_buffer_attribute_group;
        break;

    case KAN_RENDER_BUFFER_TYPE_INDEX_16:
    case KAN_RENDER_BUFFER_TYPE_INDEX_32:
        type_allocation_group = system->memory_profiling.gpu_buffer_index_group;
        break;

    case KAN_RENDER_BUFFER_TYPE_UNIFORM:
        type_allocation_group = system->memory_profiling.gpu_buffer_uniform_group;
        break;

    case KAN_RENDER_BUFFER_TYPE_STORAGE:
        type_allocation_group = system->memory_profiling.gpu_buffer_storage_group;
        break;

    case KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE:
        type_allocation_group = system->memory_profiling.gpu_buffer_read_back_storage_group;
        break;
    }

    buffer->device_allocation_group = kan_allocation_group_get_child (type_allocation_group, buffer->tracking_name);
    transfer_memory_between_groups (full_size, system->memory_profiling.gpu_unmarked_group,
                                    buffer->device_allocation_group);
#endif

    kan_cpu_section_execution_shutdown (&execution);
    return buffer;
}

void render_backend_system_destroy_buffer (struct render_backend_system_t *system,
                                           struct render_backend_buffer_t *buffer)
{
#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    transfer_memory_between_groups (buffer->full_size, buffer->device_allocation_group,
                                    system->memory_profiling.gpu_unmarked_group);
#endif

    vmaDestroyBuffer (system->gpu_memory_allocator, buffer->buffer, buffer->allocation);
    kan_free_batched (system->buffer_wrapper_allocation_group, buffer);
}

kan_render_buffer_t kan_render_buffer_create (kan_render_context_t context,
                                              enum kan_render_buffer_type_t type,
                                              vulkan_size_t full_size,
                                              void *optional_initial_data,
                                              kan_interned_string_t tracking_name)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (context);
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_buffer);

    struct render_backend_buffer_t *buffer = render_backend_system_create_buffer (
        system, RENDER_BACKEND_BUFFER_FAMILY_RESOURCE, type, full_size, tracking_name);

    if (!buffer)
    {
        kan_cpu_section_execution_shutdown (&execution);
        return KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
    }

    kan_render_buffer_t handle = KAN_HANDLE_SET (kan_render_buffer_t, buffer);
    if (optional_initial_data)
    {
        if (system->device_memory_type == KAN_RENDER_DEVICE_MEMORY_TYPE_UNIFIED ||
            system->device_memory_type == KAN_RENDER_DEVICE_MEMORY_TYPE_UNIFIED_COHERENT)
        {
            // Due to unified memory, we should be able to directly initialize buffer.
            void *memory;

            // Memory shouldn't be automatically mapped as it is a stable resource buffer.
            KAN_ASSERT (!buffer->mapped_memory)

            if (vmaMapMemory (system->gpu_memory_allocator, buffer->allocation, &memory) != VK_SUCCESS)
            {
                kan_error_critical ("Unexpected failure while mapping buffer memory, unable to continue properly.",
                                    __FILE__, __LINE__);
            }

            memcpy (memory, optional_initial_data, full_size);
            vmaUnmapMemory (system->gpu_memory_allocator, buffer->allocation);
        }
        else
        {
            void *data = kan_render_buffer_patch (handle, 0u, full_size);
            // Patch can fail if there is no available staging memory,
            // but we have no special handling for that right now.
            KAN_ASSERT (data)
            memcpy (data, optional_initial_data, full_size);
        }
    }

    kan_cpu_section_execution_shutdown (&execution);
    return handle;
}

void *kan_render_buffer_patch (kan_render_buffer_t buffer, vulkan_size_t slice_offset, vulkan_size_t slice_size)
{
    struct render_backend_buffer_t *data = KAN_HANDLE_GET (buffer);
    // Read back buffers should be accessed through kan_render_buffer_begin_access/kan_render_buffer_end_access.
    KAN_ASSERT (data->type != KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE)
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_memory (data->system);

    switch (data->family)
    {
    case RENDER_BACKEND_BUFFER_FAMILY_RESOURCE:
    case RENDER_BACKEND_BUFFER_FAMILY_DEVICE_FRAME_LIFETIME_ALLOCATOR:
    {
        struct render_backend_frame_lifetime_allocator_allocation_t staging_allocation =
            render_backend_system_allocate_for_staging (data->system, slice_size);

        if (!staging_allocation.buffer)
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Failed to patch buffer \"%s\": out of staging memory.", data->tracking_name)
            return NULL;
        }

        // Memory should always be mapped for staging buffers.
        KAN_ASSERT (staging_allocation.buffer->mapped_memory)

        KAN_ATOMIC_INT_SCOPED_LOCK (&schedule->schedule_lock)
        struct scheduled_buffer_flush_transfer_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &schedule->item_allocator, struct scheduled_buffer_flush_transfer_t);

        // We do not need to preserve order as buffers cannot depend one on another.
        item->next = schedule->first_scheduled_buffer_flush_transfer;
        schedule->first_scheduled_buffer_flush_transfer = item;
        item->source_buffer = staging_allocation.buffer;
        item->target_buffer = data;
        item->source_offset = staging_allocation.offset;
        item->target_offset = slice_offset;
        item->size = slice_size;

        return ((uint8_t *) staging_allocation.buffer->mapped_memory) + staging_allocation.offset;
    }

    case RENDER_BACKEND_BUFFER_FAMILY_STAGING:
        // Staging buffers are not exposed to user, we shouldn't be able to get here normally.
        KAN_ASSERT (false)
        break;

    case RENDER_BACKEND_BUFFER_FAMILY_HOST_FRAME_LIFETIME_ALLOCATOR:
    {
        // Frame lifetime allocations are always host visible, there is no need to stage them due to their lifetime.
        KAN_ASSERT (data->mapped_memory)

        if (data->needs_flush)
        {
            KAN_ATOMIC_INT_SCOPED_LOCK (&schedule->schedule_lock)
            struct scheduled_buffer_flush_t *item =
                KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_buffer_flush_t);

            // We do not need to preserve order as buffers cannot depend one on another.
            item->next = schedule->first_scheduled_buffer_flush;
            schedule->first_scheduled_buffer_flush = item;
            item->buffer = data;
            item->offset = slice_offset;
            item->size = slice_size;
        }

        return ((uint8_t *) data->mapped_memory) + slice_offset;
    }
    }

    KAN_ASSERT (false)
    return NULL;
}

kan_render_size_t kan_render_buffer_get_full_size (kan_render_buffer_t buffer)
{
    struct render_backend_buffer_t *data = KAN_HANDLE_GET (buffer);
    return (kan_render_size_t) data->full_size;
}

const void *kan_render_buffer_read (kan_render_buffer_t buffer)
{
    struct render_backend_buffer_t *data = KAN_HANDLE_GET (buffer);
    KAN_ASSERT (data->type == KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE)
    KAN_ASSERT (data->mapped_memory)
    return data->mapped_memory;
}

void kan_render_buffer_destroy (kan_render_buffer_t buffer)
{
    struct render_backend_buffer_t *data = KAN_HANDLE_GET (buffer);
    // Only resource family buffers can be destroyed externally through scheduling.
    KAN_ASSERT (data->family == RENDER_BACKEND_BUFFER_FAMILY_RESOURCE)

    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    KAN_ATOMIC_INT_SCOPED_LOCK (&schedule->schedule_lock)

    struct scheduled_buffer_destroy_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_buffer_destroy_t);

    // We do not need to preserve order as buffers cannot depend one on another.
    item->next = schedule->first_scheduled_buffer_destroy;
    schedule->first_scheduled_buffer_destroy = item;
    item->buffer = data;
}
