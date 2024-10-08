#include <kan/context/render_backend_implementation_interface.h>

static kan_bool_t create_vulkan_image (struct render_backend_system_t *system,
                                       struct kan_render_image_description_t *description,
                                       VkImage *output_image,
                                       VmaAllocation *output_allocation)
{
    VkImageType image_type = VK_IMAGE_TYPE_2D;
    VkImageUsageFlags image_usage = 0u;

    VkSharingMode sharing_mode;
    uint32_t sharing_index_count;
    uint32_t shared_indices[2u] = {system->device_graphics_queue_family_index,
                                   system->device_transfer_queue_family_index};

    switch (description->type)
    {
    case KAN_RENDER_IMAGE_TYPE_COLOR_2D:
        KAN_ASSERT (description->depth == 1u)
        image_type = VK_IMAGE_TYPE_2D;
        image_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        break;

    case KAN_RENDER_IMAGE_TYPE_COLOR_3D:
        image_type = VK_IMAGE_TYPE_3D;
        image_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        break;

    case KAN_RENDER_IMAGE_TYPE_DEPTH_STENCIL:
        KAN_ASSERT (description->render_target)
        image_type = VK_IMAGE_TYPE_2D;
        image_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        break;
    }

    if (description->render_target)
    {
        KAN_ASSERT (description->mips == 1u)
        switch (description->type)
        {
        case KAN_RENDER_IMAGE_TYPE_COLOR_2D:
        case KAN_RENDER_IMAGE_TYPE_COLOR_3D:
            image_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            break;

        case KAN_RENDER_IMAGE_TYPE_DEPTH_STENCIL:
            image_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            break;
        }
    }
    else
    {
        // We need source transfer in order to generate mip maps, unless they are expected to be loaded from textures.
        image_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (description->supports_sampling)
    {
        image_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    if (description->render_target &&
        system->device_graphics_queue_family_index != system->device_transfer_queue_family_index)
    {
        sharing_mode = VK_SHARING_MODE_CONCURRENT;
        sharing_index_count = 2u;
    }
    else
    {
        sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        sharing_index_count = 1u;
    }

    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .imageType = image_type,
        .format = kan_render_image_description_calculate_format (system, description),
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
        .sharingMode = sharing_mode,
        .queueFamilyIndexCount = sharing_index_count,
        .pQueueFamilyIndices = shared_indices,
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
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

struct render_backend_image_t *render_backend_system_create_image (struct render_backend_system_t *system,
                                                                   struct kan_render_image_description_t *description)
{
    VkImage vulkan_image;
    VmaAllocation vulkan_allocation;

    if (!create_vulkan_image (system, description, &vulkan_image, &vulkan_allocation))
    {
        return NULL;
    }

    struct render_backend_image_t *image =
        kan_allocate_batched (system->image_wrapper_allocation_group, sizeof (struct render_backend_image_t));
    image->next = system->first_image;
    image->previous = NULL;

    if (system->first_image)
    {
        system->first_image->previous = image;
    }

    system->first_image = image;
    image->system = system;

    image->image = vulkan_image;
    image->allocation = vulkan_allocation;
    image->description = *description;
    image->switched_to_transfer_source = KAN_FALSE;
    image->first_frame_buffer_attachment = NULL;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    image->device_allocation_group =
        kan_allocation_group_get_child (system->memory_profiling.gpu_image_group, description->tracking_name);
    transfer_memory_between_groups (render_backend_image_calculate_gpu_size (system, image),
                                    system->memory_profiling.gpu_unmarked_group, image->device_allocation_group);
#endif

    return image;
}

void render_backend_system_destroy_image (struct render_backend_system_t *system,
                                          struct render_backend_image_t *image,
                                          kan_bool_t remove_from_list)
{
    if (remove_from_list)
    {
        if (image->next)
        {
            image->next->previous = image->previous;
        }

        if (image->previous)
        {
            image->previous->next = image->next;
        }
        else
        {
            KAN_ASSERT (system->first_image == image)
            system->first_image = image->next;
        }
    }

    struct image_frame_buffer_attachment_t *attachment = image->first_frame_buffer_attachment;
    while (attachment)
    {
        struct image_frame_buffer_attachment_t *next = attachment->next;
        kan_free_batched (system->image_wrapper_allocation_group, attachment);
        attachment = next;
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    transfer_memory_between_groups (render_backend_image_calculate_gpu_size (system, image),
                                    image->device_allocation_group, system->memory_profiling.gpu_unmarked_group);
#endif

    vmaDestroyImage (system->gpu_memory_allocator, image->image, image->allocation);
    kan_free_batched (system->image_wrapper_allocation_group, image);
}

kan_render_image_t kan_render_image_create (kan_render_context_t context,
                                            struct kan_render_image_description_t *description)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) context;
    kan_atomic_int_lock (&system->resource_management_lock);
    struct render_backend_image_t *image = render_backend_system_create_image (system, description);
    kan_atomic_int_unlock (&system->resource_management_lock);
    return image ? (kan_render_image_t) image : KAN_INVALID_RENDER_IMAGE;
}

void kan_render_image_upload_data (kan_render_image_t image, uint64_t mip, void *data)
{
    struct render_backend_image_t *image_data = (struct render_backend_image_t *) image;
    KAN_ASSERT (!image_data->description.render_target)
    KAN_ASSERT (mip < image_data->description.mips)

    uint64_t texel_size =
        kan_render_image_description_calculate_texel_size (image_data->system, &image_data->description);

    uint64_t width;
    uint64_t height;
    uint64_t depth;
    kan_render_image_description_calculate_size_at_mip (&image_data->description, mip, &width, &height, &depth);

    const uint64_t allocation_size = texel_size * width * height * depth;
    struct render_backend_frame_lifetime_allocator_allocation_t staging_allocation =
        render_backend_system_allocate_for_staging (image_data->system, allocation_size);

    if (!staging_allocation.buffer)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Failed to upload image \"%s\" mip %lu: out of staging memory.", image_data->description.tracking_name,
                 (unsigned long) mip)
        return;
    }

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
}

void kan_render_image_request_mip_generation (kan_render_image_t image, uint64_t first, uint64_t last)
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
                                            uint64_t new_width,
                                            uint64_t new_height,
                                            uint64_t new_depth)
{
    struct render_backend_image_t *data = (struct render_backend_image_t *) image;
    KAN_ASSERT (data->description.render_target)

    // If it is null handle, then we've tried to resize earlier and failed. No need for additional cleanup.
    if (data->image != VK_NULL_HANDLE)
    {
        struct render_backend_schedule_state_t *schedule =
            render_backend_system_get_schedule_for_destroy (data->system);
        kan_atomic_int_lock (&schedule->schedule_lock);

        struct image_frame_buffer_attachment_t *attachment = data->first_frame_buffer_attachment;
        while (attachment)
        {
            render_backend_frame_buffer_schedule_resource_destroy (data->system, attachment->frame_buffer, schedule);
            attachment = attachment->next;
        }

        struct scheduled_detached_image_destroy_t *image_destroy = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &schedule->item_allocator, struct scheduled_detached_image_destroy_t);

        image_destroy->next = schedule->first_scheduled_detached_image_destroy;
        schedule->first_scheduled_detached_image_destroy = image_destroy;
        image_destroy->detached_image = data->image;
        image_destroy->detached_allocation = data->allocation;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
        image_destroy->gpu_size = render_backend_image_calculate_gpu_size (data->system, data);
        image_destroy->gpu_allocation_group = data->device_allocation_group;
#endif

        kan_atomic_int_unlock (&schedule->schedule_lock);
    }

    data->description.width = new_width;
    data->description.height = new_height;
    data->description.depth = new_depth;

    if (!create_vulkan_image (data->system, &data->description, &data->image, &data->allocation))
    {
        data->image = VK_NULL_HANDLE;
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Failed to resize image \"%s\": new image creation failed.", data->description.tracking_name)
        return;
    }

    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_memory (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);
    struct image_frame_buffer_attachment_t *attachment = data->first_frame_buffer_attachment;

    while (attachment)
    {
        struct scheduled_frame_buffer_create_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &schedule->item_allocator, struct scheduled_frame_buffer_create_t);

        // We do not need to preserve order as frame buffers cannot depend one on another.
        item->next = schedule->first_scheduled_frame_buffer_create;
        schedule->first_scheduled_frame_buffer_create = item;
        item->frame_buffer = attachment->frame_buffer;
        attachment = attachment->next;
    }

    kan_atomic_int_unlock (&schedule->schedule_lock);
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
