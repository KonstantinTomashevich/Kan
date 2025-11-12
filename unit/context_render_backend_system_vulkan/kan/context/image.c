#include <math.h>

#include <kan/context/render_backend_implementation_interface.h>

KAN_USE_STATIC_CPU_SECTIONS

static bool create_vulkan_image (struct render_backend_system_t *system,
                                 struct kan_render_image_description_t *description,
                                 VkImage *output_image,
                                 VmaAllocation *output_allocation)
{
    KAN_CPU_SCOPED_STATIC_SECTION (render_backend_image_create_on_device)
    KAN_ASSERT (description->width > 0u)
    KAN_ASSERT (description->height > 0u)
    KAN_ASSERT (description->depth > 0u)
    KAN_ASSERT (description->layers > 0u)

    if ((description->layers > 1u || description->always_treat_as_layered) && description->depth > 1u)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create image \"%s\": having both depth > 1 and layers > 1 is not supported.",
                 description->tracking_name)
        return false;
    }

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

    VkImageCreateFlagBits flags = 0u;
    if (description->layers == 6u)
    {
        flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = flags,
        .imageType = image_type,
        .format = image_format_to_vulkan (description->format),
        .extent =
            {
                .width = (vulkan_size_t) description->width,
                .height = (vulkan_size_t) description->height,
                .depth = (vulkan_size_t) description->depth,
            },
        .mipLevels = (vulkan_size_t) description->mips,
        .arrayLayers = (vulkan_size_t) description->layers,
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
        return false;
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
    snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "Image::%s", description->tracking_name);

    struct VkDebugUtilsObjectNameInfoEXT object_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = NULL,
        .objectType = VK_OBJECT_TYPE_IMAGE,
        .objectHandle = CONVERT_HANDLE_FOR_DEBUG * output_image,
        .pObjectName = debug_name,
    };

    vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
#endif

    return true;
}

struct render_backend_image_t *render_backend_system_create_image (struct render_backend_system_t *system,
                                                                   struct kan_render_image_description_t *description)
{
    KAN_CPU_SCOPED_STATIC_SECTION (render_backend_create_image_internal)
    VkImage vulkan_image;
    VmaAllocation vulkan_allocation;

    if (!create_vulkan_image (system, description, &vulkan_image, &vulkan_allocation))
    {
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

    if (image->description.layers == 1u)
    {
        image->last_command_layout_single_layer = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    else
    {
        image->last_command_layouts_per_layer =
            kan_allocate_general (system->image_wrapper_allocation_group,
                                  sizeof (VkImageLayout) * image->description.layers, alignof (VkImageLayout));

        for (kan_loop_size_t index = 0u; index < image->description.layers; ++index)
        {
            image->last_command_layouts_per_layer[index] = VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    image->device_allocation_group =
        kan_allocation_group_get_child (system->memory_profiling.gpu_image_group, description->tracking_name);

    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements (system->device, image->image, &requirements);

    transfer_memory_between_groups ((vulkan_size_t) requirements.size, system->memory_profiling.gpu_unmarked_group,
                                    image->device_allocation_group);
#endif

    return image;
}

void render_backend_system_destroy_image (struct render_backend_system_t *system, struct render_backend_image_t *image)
{
    if (image->description.layers > 1u)
    {
        kan_free_general (system->image_wrapper_allocation_group, image->last_command_layouts_per_layer,
                          sizeof (VkImageLayout) * image->description.layers);
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements (system->device, image->image, &requirements);

    transfer_memory_between_groups ((vulkan_size_t) requirements.size, image->device_allocation_group,
                                    system->memory_profiling.gpu_unmarked_group);
#endif

    vmaDestroyImage (system->gpu_memory_allocator, image->image, image->allocation);
    kan_free_batched (system->image_wrapper_allocation_group, image);
}

kan_render_image_t kan_render_image_create (kan_render_context_t context,
                                            struct kan_render_image_description_t *description)
{
    kan_cpu_static_sections_ensure_initialized ();
    struct render_backend_system_t *system = KAN_HANDLE_GET (context);
    KAN_CPU_SCOPED_STATIC_SECTION (render_backend_create_image)
    struct render_backend_image_t *image = render_backend_system_create_image (system, description);
    return image ? KAN_HANDLE_SET (kan_render_image_t, image) : KAN_HANDLE_SET_INVALID (kan_render_image_t);
}

static inline struct scheduled_image_upload_t *find_or_add_image_upload_to_schedule (
    struct render_backend_image_t *image_data,
    struct render_backend_schedule_state_t *schedule,
    kan_instance_size_t layer,
    uint8_t mip)
{
    // We merge image uploads for better pipeline barriers. Plain search should be okay as
    // we're not uploading hundreds of images per frame.
    struct scheduled_image_upload_t *item = schedule->first_scheduled_image_upload;

    while (item)
    {
        if (item->image == image_data && item->layer == layer && item->mip == mip)
        {
            break;
        }

        item = item->next;
    }

    if (!item)
    {
        item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_image_upload_t);
        // We do not need to preserve order as images cannot depend one on another.
        item->next = schedule->first_scheduled_image_upload;
        schedule->first_scheduled_image_upload = item;

        item->image = image_data;
        item->layer = layer;
        item->mip = mip;

        item->clear = false;
        item->ranges_first = NULL;
        item->ranges_last = NULL;
    }

    return item;
}

void kan_render_image_clear_color (kan_render_image_t image,
                                   kan_instance_size_t layer,
                                   uint8_t mip,
                                   const struct kan_render_clear_color_t *clear_color)
{
    struct render_backend_image_t *image_data = KAN_HANDLE_GET (image);
    KAN_CPU_SCOPED_STATIC_SECTION (render_backend_image_upload)

    KAN_ASSERT (!image_data->description.render_target)
    KAN_ASSERT (mip < image_data->description.mips)

    struct render_backend_schedule_state_t *schedule =
        render_backend_system_get_schedule_for_memory (image_data->system);
    KAN_ATOMIC_INT_SCOPED_LOCK (&schedule->schedule_lock)

    struct scheduled_image_upload_t *item = find_or_add_image_upload_to_schedule (image_data, schedule, layer, mip);
    item->clear = true;

    switch (image_data->description.format)
    {
    case KAN_RENDER_IMAGE_FORMAT_S8_UINT:
        item->clear_color.uint32[0u] = (uint32_t) roundf (clear_color->r * 255.0f);
        item->clear_color.uint32[1u] = (uint32_t) roundf (clear_color->g * 255.0f);
        item->clear_color.uint32[2u] = (uint32_t) roundf (clear_color->b * 255.0f);
        item->clear_color.uint32[3u] = (uint32_t) roundf (clear_color->a * 255.0f);
        break;

    default:
        item->clear_color.float32[0u] = clear_color->r;
        item->clear_color.float32[1u] = clear_color->g;
        item->clear_color.float32[2u] = clear_color->b;
        item->clear_color.float32[3u] = clear_color->a;
        break;
    }
}

void kan_render_image_upload_data (
    kan_render_image_t image, kan_instance_size_t layer, uint8_t mip, vulkan_size_t data_size, void *data)
{
    struct render_backend_image_t *image_data = KAN_HANDLE_GET (image);
    vulkan_size_t width;
    vulkan_size_t height;
    vulkan_size_t depth;

    kan_render_image_description_calculate_size_at_mip (&image_data->description, mip, &width, &height, &depth);
    struct kan_render_integer_region_3d_t region = {
        .x = 0,
        .y = 0,
        .z = 0,
        .width = width,
        .height = height,
        .depth = depth,
    };

    kan_render_image_upload_data_region (image, layer, mip, region, data_size, data);
}

void kan_render_image_upload_data_region (kan_render_image_t image,
                                          kan_instance_size_t layer,
                                          uint8_t mip,
                                          struct kan_render_integer_region_3d_t region,
                                          kan_instance_size_t data_size,
                                          void *data)
{
    struct render_backend_image_t *image_data = KAN_HANDLE_GET (image);
    KAN_CPU_SCOPED_STATIC_SECTION (render_backend_image_upload)

    KAN_ASSERT (!image_data->description.render_target)
    KAN_ASSERT (mip < image_data->description.mips)

    const vulkan_size_t allocation_size = data_size;
    struct render_backend_frame_lifetime_allocator_allocation_t staging_allocation =
        render_backend_system_allocate_for_staging (image_data->system, allocation_size);

    if (!staging_allocation.buffer)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Failed to upload image \"%s\" mip %lu: out of staging memory.", image_data->description.tracking_name,
                 (unsigned long) mip)
        return;
    }

    KAN_ASSERT (staging_allocation.buffer->mapped_memory)
    memcpy (((uint8_t *) staging_allocation.buffer->mapped_memory) + staging_allocation.offset, data, allocation_size);

    if (staging_allocation.buffer->needs_flush)
    {
        if (vmaFlushAllocation (image_data->system->gpu_memory_allocator, staging_allocation.buffer->allocation,
                                staging_allocation.offset, allocation_size) != VK_SUCCESS)
        {
            kan_error_critical ("Unexpected failure while flushing buffer data, unable to continue properly.", __FILE__,
                                __LINE__);
        }
    }

    struct render_backend_schedule_state_t *schedule =
        render_backend_system_get_schedule_for_memory (image_data->system);

    KAN_ATOMIC_INT_SCOPED_LOCK (&schedule->schedule_lock)
    struct scheduled_image_upload_t *item = find_or_add_image_upload_to_schedule (image_data, schedule, layer, mip);

    struct scheduled_image_upload_range_t *range =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_image_upload_range_t);

    range->next = NULL;
    range->staging_buffer = staging_allocation.buffer;
    range->staging_buffer_offset = staging_allocation.offset;

    range->region_offset.x = region.x;
    range->region_offset.y = region.y;
    range->region_offset.z = region.z;
    range->region_extent.width = region.width;
    range->region_extent.height = region.height;
    range->region_extent.depth = region.depth;

    if (item->ranges_last)
    {
        item->ranges_last->next = range;
    }
    else
    {
        item->ranges_first = range;
    }

    item->ranges_last = range;
}

void kan_render_image_request_mip_generation (kan_render_image_t image,
                                              kan_instance_size_t layer,
                                              uint8_t first,
                                              uint8_t last)
{
    struct render_backend_image_t *data = KAN_HANDLE_GET (image);
    KAN_ASSERT (!data->description.render_target)

    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_memory (data->system);
    KAN_ATOMIC_INT_SCOPED_LOCK (&schedule->schedule_lock)

    struct scheduled_image_mip_generation_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_image_mip_generation_t);

    // We do not need to preserve order as images cannot depend one on another.
    item->next = schedule->first_scheduled_image_mip_generation;
    schedule->first_scheduled_image_mip_generation = item;
    item->image = data;
    item->layer = layer;
    item->first = first;
    item->last = last;
}

void kan_render_image_copy_data (kan_render_image_t from_image,
                                 kan_instance_size_t from_layer,
                                 uint8_t from_mip,
                                 kan_render_image_t to_image,
                                 kan_instance_size_t to_layer,
                                 uint8_t to_mip)
{
    struct render_backend_image_t *source_data = KAN_HANDLE_GET (from_image);
    KAN_ASSERT (!source_data->description.render_target)

    struct render_backend_image_t *target_data = KAN_HANDLE_GET (to_image);
    KAN_ASSERT (!target_data->description.render_target)

    struct render_backend_schedule_state_t *schedule =
        render_backend_system_get_schedule_for_memory (source_data->system);
    KAN_ATOMIC_INT_SCOPED_LOCK (&schedule->schedule_lock)

    struct scheduled_image_copy_data_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_image_copy_data_t);

    // Image copies actually might depend one on another, but there should not be too many of them.
    item->next = NULL;

    if (schedule->last_scheduled_image_copy_data)
    {
        schedule->last_scheduled_image_copy_data->next = item;
    }
    else
    {
        schedule->first_scheduled_image_copy_data = item;
    }

    schedule->last_scheduled_image_copy_data = item;
    item->from_image = source_data;
    item->to_image = target_data;
    item->from_layer = from_layer;
    item->from_mip = from_mip;
    item->to_layer = to_layer;
    item->to_mip = to_mip;
}

void kan_render_image_get_sizes (kan_render_image_t image,
                                 kan_instance_size_t *width,
                                 kan_instance_size_t *height,
                                 kan_instance_size_t *depth,
                                 kan_instance_size_t *layers)
{
    struct render_backend_image_t *data = KAN_HANDLE_GET (image);
    if (width)
    {
        *width = data->description.width;
    }

    if (height)
    {
        *height = data->description.height;
    }

    if (depth)
    {
        *depth = data->description.depth;
    }

    if (layers)
    {
        *layers = data->description.layers;
    }
}

void kan_render_image_destroy (kan_render_image_t image)
{
    struct render_backend_image_t *data = KAN_HANDLE_GET (image);
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    KAN_ATOMIC_INT_SCOPED_LOCK (&schedule->schedule_lock)

    struct scheduled_image_destroy_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_image_destroy_t);

    // We do not need to preserve order as images cannot depend one on another.
    item->next = schedule->first_scheduled_image_destroy;
    schedule->first_scheduled_image_destroy = item;
    item->image = data;
}
