#include <kan/context/render_backend_implementation_interface.h>

static kan_bool_t create_vulkan_image (struct render_backend_system_t *system,
                                       struct kan_render_image_description_t *description,
                                       VkImage *output_image,
                                       VmaAllocation *output_allocation)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_image_create_on_device);

    VkImageType image_type = description->depth > 1u ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    // Always support at least transfer source as read back might be requested.
    VkImageUsageFlags image_usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    if (description->render_target)
    {
        switch (get_image_format_class (description->format))
        {
        case IMAGE_FORMAT_CLASS_COLOR:
            image_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            break;

        case IMAGE_FORMAT_CLASS_DEPTH:
        case IMAGE_FORMAT_CLASS_STENCIL:
        case IMAGE_FORMAT_CLASS_DEPTH_STENCIL:
            image_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            break;
        }
    }
    else
    {
        image_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (description->supports_sampling)
    {
        image_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .imageType = image_type,
        .format = image_format_to_vulkan (description->format),
        .extent =
            {
                .width = (uint32_t) description->width,
                .height = (uint32_t) description->height,
                .depth = (uint32_t) description->depth,
            },
        .mipLevels = (uint32_t) description->mips,
        .arrayLayers = 1u,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = image_usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1u,
        .pQueueFamilyIndices = &system->device_queue_family_index,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocation_create_info = {
        .flags = 0u,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = 0u,
        .preferredFlags = 0u,
        .memoryTypeBits = UINT32_MAX,
        .pool = NULL,
        .pUserData = NULL,
        .priority = 0.0f,
    };

    VmaAllocationInfo allocation_info;
    if (vmaCreateImage (system->gpu_memory_allocator, &image_create_info, &allocation_create_info, output_image,
                        output_allocation, &allocation_info) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to create image \"%s\".",
                 description->tracking_name)
        kan_cpu_section_execution_shutdown (&execution);
        return KAN_FALSE;
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
    snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "Image::%s", description->tracking_name);

    struct VkDebugUtilsObjectNameInfoEXT object_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = NULL,
        .objectType = VK_OBJECT_TYPE_IMAGE,
        .objectHandle = (uint64_t) *output_image,
        .pObjectName = debug_name,
    };

    vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
#endif

    kan_cpu_section_execution_shutdown (&execution);
    return KAN_TRUE;
}

struct render_backend_image_t *render_backend_system_create_image (struct render_backend_system_t *system,
                                                                   struct kan_render_image_description_t *description)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_image_internal);

    VkImage vulkan_image;
    VmaAllocation vulkan_allocation;

    if (!create_vulkan_image (system, description, &vulkan_image, &vulkan_allocation))
    {
        kan_cpu_section_execution_shutdown (&execution);
        return NULL;
    }

    struct render_backend_image_t *image =
        kan_allocate_batched (system->image_wrapper_allocation_group, sizeof (struct render_backend_image_t));

    kan_atomic_int_lock (&system->resource_registration_lock);
    kan_bd_list_add (&system->images, NULL, &image->list_node);
    kan_atomic_int_unlock (&system->resource_registration_lock);
    image->system = system;

    image->image = vulkan_image;
    image->allocation = vulkan_allocation;
    image->description = *description;
    image->last_command_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->first_frame_buffer_attachment = NULL;
    image->first_parameter_set_attachment = NULL;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    image->device_allocation_group =
        kan_allocation_group_get_child (system->memory_profiling.gpu_image_group, description->tracking_name);

    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements (system->device, image->image, &requirements);

    transfer_memory_between_groups (requirements.size, system->memory_profiling.gpu_unmarked_group,
                                    image->device_allocation_group);
#endif

    kan_cpu_section_execution_shutdown (&execution);
    return image;
}

void render_backend_system_destroy_image (struct render_backend_system_t *system, struct render_backend_image_t *image)
{
    struct image_frame_buffer_attachment_t *frame_buffer_attachment = image->first_frame_buffer_attachment;
    while (frame_buffer_attachment)
    {
        struct image_frame_buffer_attachment_t *next = frame_buffer_attachment->next;
        kan_free_batched (system->image_wrapper_allocation_group, frame_buffer_attachment);
        frame_buffer_attachment = next;
    }

    struct image_parameter_set_attachment_t *parameter_set_attachment = image->first_parameter_set_attachment;
    while (parameter_set_attachment)
    {
        struct image_parameter_set_attachment_t *next = parameter_set_attachment->next;
        kan_free_batched (system->image_wrapper_allocation_group, parameter_set_attachment);
        parameter_set_attachment = next;
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements (system->device, image->image, &requirements);

    transfer_memory_between_groups (requirements.size,
                                    image->device_allocation_group, system->memory_profiling.gpu_unmarked_group);
#endif

    vmaDestroyImage (system->gpu_memory_allocator, image->image, image->allocation);
    kan_free_batched (system->image_wrapper_allocation_group, image);
}

kan_render_image_t kan_render_image_create (kan_render_context_t context,
                                            struct kan_render_image_description_t *description)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) context;
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_image);
    struct render_backend_image_t *image = render_backend_system_create_image (system, description);
    kan_cpu_section_execution_shutdown (&execution);
    return image ? (kan_render_image_t) image : KAN_INVALID_RENDER_IMAGE;
}

void kan_render_image_upload_data (kan_render_image_t image, uint8_t mip, uint32_t data_size, void *data)
{
    struct render_backend_image_t *image_data = (struct render_backend_image_t *) image;
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, image_data->system->section_image_upload);

    KAN_ASSERT (!image_data->description.render_target)
    KAN_ASSERT (mip < image_data->description.mips)

    const uint32_t allocation_size = data_size;
    struct render_backend_frame_lifetime_allocator_allocation_t staging_allocation =
        render_backend_system_allocate_for_staging (image_data->system, allocation_size);

    if (!staging_allocation.buffer)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Failed to upload image \"%s\" mip %lu: out of staging memory.", image_data->description.tracking_name,
                 (unsigned long) mip)
        kan_cpu_section_execution_shutdown (&execution);
        return;
    }

    void *output = kan_render_buffer_patch ((kan_render_buffer_t) staging_allocation.buffer, staging_allocation.offset,
                                            allocation_size);

    if (!output)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Failed to upload image \"%s\" mip %lu: unable to patch acquired staging memory.",
                 image_data->description.tracking_name, (unsigned long) mip)
        kan_cpu_section_execution_shutdown (&execution);
        return;
    }

    memcpy (output, data, allocation_size);
    struct render_backend_schedule_state_t *schedule =
        render_backend_system_get_schedule_for_memory (image_data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_image_upload_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_image_upload_t);

    // We do not need to preserve order as images cannot depend one on another.
    item->next = schedule->first_scheduled_image_upload;
    schedule->first_scheduled_image_upload = item;
    item->image = image_data;
    item->mip = mip;
    item->staging_buffer = staging_allocation.buffer;
    item->staging_buffer_offset = staging_allocation.offset;
    kan_atomic_int_unlock (&schedule->schedule_lock);

    kan_cpu_section_execution_shutdown (&execution);
}

void kan_render_image_request_mip_generation (kan_render_image_t image, uint8_t first, uint8_t last)
{
    struct render_backend_image_t *data = (struct render_backend_image_t *) image;
    KAN_ASSERT (!data->description.render_target)

    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_memory (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_image_mip_generation_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_image_mip_generation_t);

    // We do not need to preserve order as images cannot depend one on another.
    item->next = schedule->first_scheduled_image_mip_generation;
    schedule->first_scheduled_image_mip_generation = item;
    item->image = data;
    item->first = first;
    item->last = last;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}

void kan_render_image_resize_render_target (kan_render_image_t image,
                                            uint32_t new_width,
                                            uint32_t new_height,
                                            uint32_t new_depth)
{
    struct render_backend_image_t *data = (struct render_backend_image_t *) image;
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, data->system->section_image_resize_render_target);
    KAN_ASSERT (data->description.render_target)

    // If it is null handle, then we've tried to resize earlier and failed. No need for additional cleanup.
    if (data->image != VK_NULL_HANDLE)
    {
        struct render_backend_schedule_state_t *schedule =
            render_backend_system_get_schedule_for_destroy (data->system);
        kan_atomic_int_lock (&schedule->schedule_lock);

        struct image_frame_buffer_attachment_t *frame_buffer_attachment = data->first_frame_buffer_attachment;
        while (frame_buffer_attachment)
        {
            render_backend_frame_buffer_schedule_resource_destroy (data->system, frame_buffer_attachment->frame_buffer,
                                                                   schedule);
            frame_buffer_attachment = frame_buffer_attachment->next;
        }

        struct scheduled_detached_image_destroy_t *image_destroy = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &schedule->item_allocator, struct scheduled_detached_image_destroy_t);

        image_destroy->next = schedule->first_scheduled_detached_image_destroy;
        schedule->first_scheduled_detached_image_destroy = image_destroy;
        image_destroy->detached_image = data->image;
        image_destroy->detached_allocation = data->allocation;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
        image_destroy->gpu_allocation_group = data->device_allocation_group;
#endif

        kan_atomic_int_unlock (&schedule->schedule_lock);
    }

    data->last_command_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    data->description.width = new_width;
    data->description.height = new_height;
    data->description.depth = new_depth;

    if (!create_vulkan_image (data->system, &data->description, &data->image, &data->allocation))
    {
        data->image = VK_NULL_HANDLE;
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Failed to resize image \"%s\": new image creation failed.", data->description.tracking_name)

        kan_cpu_section_execution_shutdown (&execution);
        return;
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements (data->system->device, data->image, &requirements);

    transfer_memory_between_groups (requirements.size,
                                    data->system->memory_profiling.gpu_unmarked_group, data->device_allocation_group);
#endif

    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_memory (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);
    struct image_frame_buffer_attachment_t *frame_buffer_attachment = data->first_frame_buffer_attachment;

    while (frame_buffer_attachment)
    {
        struct scheduled_frame_buffer_create_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &schedule->item_allocator, struct scheduled_frame_buffer_create_t);

        // We do not need to preserve order as frame buffers cannot depend one on another.
        item->next = schedule->first_scheduled_frame_buffer_create;
        schedule->first_scheduled_frame_buffer_create = item;
        item->frame_buffer = frame_buffer_attachment->frame_buffer;
        frame_buffer_attachment = frame_buffer_attachment->next;
    }

    struct image_parameter_set_attachment_t *parameter_set_attachment = data->first_parameter_set_attachment;
    while (parameter_set_attachment)
    {
        struct kan_render_parameter_update_description_t update = {
            .binding = parameter_set_attachment->binding,
            .image_binding =
                {
                    .image = image,
                },
        };

        kan_render_pipeline_parameter_set_update ((kan_render_pipeline_parameter_set_t) parameter_set_attachment->set,
                                                  1u, &update);
        parameter_set_attachment = parameter_set_attachment->next;
    }

    kan_atomic_int_unlock (&schedule->schedule_lock);
    kan_cpu_section_execution_shutdown (&execution);
}

void kan_render_image_destroy (kan_render_image_t image)
{
    struct render_backend_image_t *data = (struct render_backend_image_t *) image;
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_image_destroy_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_image_destroy_t);

    // We do not need to preserve order as images cannot depend one on another.
    item->next = schedule->first_scheduled_image_destroy;
    schedule->first_scheduled_image_destroy = item;
    item->image = data;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}
