#include <kan/context/render_backend_implementation_interface.h>

KAN_USE_STATIC_CPU_SECTIONS

static inline void destroy_frame_buffer_image_views (struct render_backend_system_t *system,
                                                     kan_instance_size_t attachments_count,
                                                     VkImageView *image_views)
{
    for (kan_loop_size_t index = 0u; index < attachments_count; ++index)
    {
        if (image_views[index] != VK_NULL_HANDLE)
        {
            vkDestroyImageView (system->device, image_views[index], VULKAN_ALLOCATION_CALLBACKS (system));
        }
    }

    kan_free_general (system->frame_buffer_wrapper_allocation_group, image_views,
                      sizeof (VkImageView) * attachments_count);
}

struct render_backend_frame_buffer_t *render_backend_system_create_frame_buffer (
    struct render_backend_system_t *system, struct kan_render_frame_buffer_description_t *description)
{
    KAN_CPU_SCOPED_STATIC_SECTION (render_backend_create_frame_buffer_internal)
    // As frame buffers only depend on images and images are always created right away,
    // we can create frame buffers right away too.

    bool can_be_created = true;
    kan_render_size_t width = 0u;
    kan_render_size_t height = 0u;

    for (kan_loop_size_t attachment_index = 0u; attachment_index < description->attachments_count; ++attachment_index)
    {
        struct render_backend_image_t *image = KAN_HANDLE_GET (description->attachments[attachment_index].image);
        vulkan_size_t attachment_width = (vulkan_size_t) image->description.width;
        vulkan_size_t attachment_height = (vulkan_size_t) image->description.height;

        if (attachment_index == 0u)
        {
            width = attachment_width;
            height = attachment_height;
        }
        else if (attachment_width != width || attachment_height != height)
        {
            can_be_created = false;
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Unable to create frame buffer \"%s\" as its attachment %lu has size %lux%lu while previous "
                     "attachments have size %lux%lu.",
                     description->tracking_name, (unsigned long) attachment_index, (unsigned long) attachment_width,
                     (unsigned long) attachment_height, (unsigned long) width, (unsigned long) height)
        }
    }

    if (!can_be_created)
    {
        return NULL;
    }

    VkImageView *image_views =
        kan_allocate_general (system->frame_buffer_wrapper_allocation_group,
                              sizeof (VkImageView) * description->attachments_count, alignof (VkImageView));

    for (kan_loop_size_t attachment_index = 0u; attachment_index < description->attachments_count; ++attachment_index)
    {
        struct render_backend_image_t *image = KAN_HANDLE_GET (description->attachments[attachment_index].image);
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .image = image->image,
            .viewType = get_image_view_type_for_attachment (&image->description),
            .format = image_format_to_vulkan (image->description.format),
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange =
                {
                    .aspectMask = get_image_aspects (&image->description),
                    .baseMipLevel = 0u,
                    .levelCount = 1u,
                    .baseArrayLayer = description->attachments[attachment_index].layer,
                    .layerCount = 1u,
                },
        };

        if (vkCreateImageView (system->device, &create_info, VULKAN_ALLOCATION_CALLBACKS (system),
                               &image_views[attachment_index]) != VK_SUCCESS)
        {
            can_be_created = false;
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Unable to create frame buffer \"%s\" due to failure when creating image view for "
                     "attachment %lu.",
                     description->tracking_name, (unsigned long) attachment_index)
        }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
        if (image_views[attachment_index] != VK_NULL_HANDLE)
        {
            char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
            snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME,
                      "ImageView::ForFrameBuffer::%s::attachment%lu", description->tracking_name,
                      (unsigned long) attachment_index);

            struct VkDebugUtilsObjectNameInfoEXT object_name = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext = NULL,
                .objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
                .objectHandle = CONVERT_HANDLE_FOR_DEBUG image_views[attachment_index],
                .pObjectName = debug_name,
            };

            vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
        }
#endif
    }

    if (!can_be_created)
    {
        destroy_frame_buffer_image_views (system, description->attachments_count, image_views);
        return NULL;
    }

    struct render_backend_pass_t *pass = KAN_HANDLE_GET (description->associated_pass);
    VkFramebufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .renderPass = pass->pass,
        .attachmentCount = (vulkan_size_t) description->attachments_count,
        .pAttachments = image_views,
        .width = width,
        .height = height,
        .layers = 1u,
    };

    VkFramebuffer instance = VK_NULL_HANDLE;
    if (vkCreateFramebuffer (system->device, &create_info, VULKAN_ALLOCATION_CALLBACKS (system), &instance) !=
        VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Unable to create frame buffer \"%s\".",
                 description->tracking_name)

        destroy_frame_buffer_image_views (system, description->attachments_count, image_views);
        return NULL;
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    if (instance != VK_NULL_HANDLE)
    {
        char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
        snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "FrameBuffer::%s",
                  description->tracking_name);

        struct VkDebugUtilsObjectNameInfoEXT object_name = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = NULL,
            .objectType = VK_OBJECT_TYPE_FRAMEBUFFER,
            .objectHandle = CONVERT_HANDLE_FOR_DEBUG instance,
            .pObjectName = debug_name,
        };

        vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
    }
#endif

    struct render_backend_frame_buffer_t *buffer = kan_allocate_batched (system->frame_buffer_wrapper_allocation_group,
                                                                         sizeof (struct render_backend_frame_buffer_t));

    kan_atomic_int_lock (&system->resource_registration_lock);
    kan_bd_list_add (&system->frame_buffers, NULL, &buffer->list_node);
    kan_atomic_int_unlock (&system->resource_registration_lock);

    buffer->system = system;
    buffer->instance = instance;
    buffer->image_views = image_views;

    buffer->pass = pass;
    buffer->tracking_name = description->tracking_name;

    buffer->attachments_count = description->attachments_count;
    buffer->attachments =
        kan_allocate_general (system->frame_buffer_wrapper_allocation_group,
                              sizeof (struct render_backend_frame_buffer_attachment_t) * buffer->attachments_count,
                              alignof (struct render_backend_frame_buffer_attachment_t));

    for (kan_loop_size_t index = 0u; index < buffer->attachments_count; ++index)
    {
        struct kan_render_frame_buffer_attachment_description_t *source = &description->attachments[index];
        struct render_backend_frame_buffer_attachment_t *target = &buffer->attachments[index];
        target->image = KAN_HANDLE_GET (source->image);
        target->layer = source->layer;
    }

    return buffer;
}

void render_backend_system_destroy_frame_buffer (struct render_backend_system_t *system,
                                                 struct render_backend_frame_buffer_t *frame_buffer)
{
    if (frame_buffer->instance != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer (system->device, frame_buffer->instance, VULKAN_ALLOCATION_CALLBACKS (system));
        frame_buffer->instance = VK_NULL_HANDLE;
    }

    if (frame_buffer->image_views)
    {
        destroy_frame_buffer_image_views (system, frame_buffer->attachments_count, frame_buffer->image_views);
        frame_buffer->image_views = NULL;
    }

    kan_free_general (system->frame_buffer_wrapper_allocation_group, frame_buffer->attachments,
                      sizeof (struct render_backend_frame_buffer_attachment_t) * frame_buffer->attachments_count);
    kan_free_batched (system->frame_buffer_wrapper_allocation_group, frame_buffer);
}

kan_render_frame_buffer_t kan_render_frame_buffer_create (kan_render_context_t context,
                                                          struct kan_render_frame_buffer_description_t *description)
{
    kan_cpu_static_sections_ensure_initialized ();
    struct render_backend_system_t *system = KAN_HANDLE_GET (context);
    KAN_CPU_SCOPED_STATIC_SECTION (render_backend_create_frame_buffer)

    struct render_backend_frame_buffer_t *frame_buffer =
        render_backend_system_create_frame_buffer (system, description);

    return frame_buffer ? KAN_HANDLE_SET (kan_render_frame_buffer_t, frame_buffer) :
                          KAN_HANDLE_SET_INVALID (kan_render_frame_buffer_t);
}

void kan_render_frame_buffer_destroy (kan_render_frame_buffer_t buffer)
{
    struct render_backend_frame_buffer_t *data = KAN_HANDLE_GET (buffer);
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    KAN_ATOMIC_INT_SCOPED_LOCK (&schedule->schedule_lock)

    struct scheduled_frame_buffer_destroy_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_frame_buffer_destroy_t);

    // We do not need to preserve order as frame buffers cannot depend one on another.
    item->next = schedule->first_scheduled_frame_buffer_destroy;
    schedule->first_scheduled_frame_buffer_destroy = item;
    item->frame_buffer = data;
}
