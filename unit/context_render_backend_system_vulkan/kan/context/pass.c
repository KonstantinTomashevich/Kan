#include <kan/context/render_backend_implementation_interface.h>

struct render_backend_pass_t *render_backend_system_create_pass (struct render_backend_system_t *system,
                                                                 struct kan_render_pass_description_t *description)
{
    VkAttachmentDescription *attachment_descriptions = kan_allocate_general (
        system->utility_allocation_group, sizeof (VkAttachmentDescription) * description->attachments_count,
        _Alignof (VkAttachmentDescription));

    VkAttachmentReference *color_attachments = kan_allocate_general (
        system->utility_allocation_group, sizeof (VkAttachmentReference) * description->attachments_count,
        _Alignof (VkAttachmentReference));

    VkAttachmentReference *next_color_attachment = color_attachments;
    uint64_t color_attachments_count = 0u;
    kan_bool_t has_depth_attachment = KAN_FALSE;
    VkAttachmentReference depth_attachment;

    for (uint64_t index = 0u; index < description->attachments_count; ++index)
    {
        struct kan_render_pass_attachment_t *attachment = &description->attachments[index];
        struct VkAttachmentDescription *vulkan_attachment = &attachment_descriptions[index];
        vulkan_attachment->flags = 0u;

        switch (attachment->type)
        {
        case KAN_RENDER_PASS_ATTACHMENT_COLOR:
            ++color_attachments_count;

            switch (attachment->color_format)
            {
            case KAN_RENDER_COLOR_FORMAT_R8_SRGB:
                vulkan_attachment->format = VK_FORMAT_R8_SRGB;
                break;

            case KAN_RENDER_COLOR_FORMAT_RG16_SRGB:
                vulkan_attachment->format = VK_FORMAT_R8G8_SRGB;
                break;

            case KAN_RENDER_COLOR_FORMAT_RGB24_SRGB:
                vulkan_attachment->format = VK_FORMAT_R8G8B8_SRGB;
                break;

            case KAN_RENDER_COLOR_FORMAT_RGBA32_SRGB:
                vulkan_attachment->format = VK_FORMAT_R8G8B8A8_SRGB;
                break;

            case KAN_RENDER_COLOR_FORMAT_R32_SFLOAT:
                vulkan_attachment->format = VK_FORMAT_R32_SFLOAT;
                break;

            case KAN_RENDER_COLOR_FORMAT_RG64_SFLOAT:
                vulkan_attachment->format = VK_FORMAT_R32G32_SFLOAT;
                break;

            case KAN_RENDER_COLOR_FORMAT_RGB96_SFLOAT:
                vulkan_attachment->format = VK_FORMAT_R32G32B32_SFLOAT;
                break;

            case KAN_RENDER_COLOR_FORMAT_RGBA128_SFLOAT:
                vulkan_attachment->format = VK_FORMAT_R32G32B32A32_SFLOAT;
                break;

            case KAN_RENDER_COLOR_FORMAT_SURFACE:
                vulkan_attachment->format = SURFACE_COLOR_FORMAT;
                break;
            }

            switch (attachment->samples)
            {
            case 1u:
                vulkan_attachment->samples = VK_SAMPLE_COUNT_1_BIT;
                break;

            case 2u:
                vulkan_attachment->samples = VK_SAMPLE_COUNT_2_BIT;
                break;

            case 4u:
                vulkan_attachment->samples = VK_SAMPLE_COUNT_4_BIT;
                break;

            case 8u:
                vulkan_attachment->samples = VK_SAMPLE_COUNT_8_BIT;
                break;

            case 16u:
                vulkan_attachment->samples = VK_SAMPLE_COUNT_16_BIT;
                break;

            case 32u:
                vulkan_attachment->samples = VK_SAMPLE_COUNT_32_BIT;
                break;

            case 64u:
                vulkan_attachment->samples = VK_SAMPLE_COUNT_64_BIT;
                break;

            default:
                // Count of samples is not power of two.
                KAN_ASSERT (KAN_FALSE)
                break;
            }

            vulkan_attachment->finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            next_color_attachment->attachment = (uint32_t) index;
            next_color_attachment->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            ++next_color_attachment;
            break;

        case KAN_RENDER_PASS_ATTACHMENT_DEPTH:
        case KAN_RENDER_PASS_ATTACHMENT_STENCIL:
        case KAN_RENDER_PASS_ATTACHMENT_DEPTH_STENCIL:
            // There should be not more than 1 depth attachment per pass.
            KAN_ASSERT (!has_depth_attachment)
            has_depth_attachment = KAN_TRUE;

            if (attachment->type == KAN_RENDER_PASS_ATTACHMENT_DEPTH)
            {
                vulkan_attachment->format = DEPTH_FORMAT;
            }
            else if (attachment->type == KAN_RENDER_PASS_ATTACHMENT_STENCIL)
            {
                vulkan_attachment->format = STENCIL_FORMAT;
            }
            else if (attachment->type == KAN_RENDER_PASS_ATTACHMENT_DEPTH_STENCIL)
            {
                vulkan_attachment->format = DEPTH_STENCIL_FORMAT;
            }

            KAN_ASSERT (attachment->samples == 1u)
            vulkan_attachment->samples = VK_SAMPLE_COUNT_1_BIT;
            vulkan_attachment->finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            depth_attachment.attachment = (uint32_t) index;
            depth_attachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;
        }

        switch (attachment->load_operation)
        {
        case KAN_RENDER_LOAD_OPERATION_ANY:
            vulkan_attachment->loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            break;

        case KAN_RENDER_LOAD_OPERATION_LOAD:
            vulkan_attachment->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            break;

        case KAN_RENDER_LOAD_OPERATION_CLEAR:
            vulkan_attachment->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            break;
        }

        switch (attachment->store_operation)
        {
        case KAN_RENDER_STORE_OPERATION_ANY:
            vulkan_attachment->storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            break;

        case KAN_RENDER_STORE_OPERATION_STORE:
            vulkan_attachment->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            break;

        case KAN_RENDER_STORE_OPERATION_NONE:
            vulkan_attachment->storeOp = VK_ATTACHMENT_STORE_OP_NONE;
            break;
        }

        vulkan_attachment->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        vulkan_attachment->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        vulkan_attachment->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VkSubpassDescription sub_pass_description = {
        .flags = 0u,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0u,
        .pInputAttachments = NULL,
        .colorAttachmentCount = (uint32_t) color_attachments_count,
        .pColorAttachments = color_attachments,
        .pResolveAttachments = NULL,
        .pDepthStencilAttachment = has_depth_attachment ? &depth_attachment : NULL,
        .preserveAttachmentCount = 0u,
        .pPreserveAttachments = NULL,
    };

    uint32_t destination_access_mask = 0u;
    if (color_attachments_count > 0u)
    {
        destination_access_mask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    if (has_depth_attachment)
    {
        destination_access_mask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    VkSubpassDependency sub_pass_dependencies[] = {
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0u,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0u,
            .dstAccessMask = destination_access_mask,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
    };

    VkRenderPassCreateInfo render_pass_description = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .attachmentCount = (uint32_t) description->attachments_count,
        .pAttachments = attachment_descriptions,
        .subpassCount = 1u,
        .pSubpasses = &sub_pass_description,
        .dependencyCount = 1u,
        .pDependencies = sub_pass_dependencies,
    };

    VkRenderPass render_pass;
    VkResult result = vkCreateRenderPass (system->device, &render_pass_description,
                                          VULKAN_ALLOCATION_CALLBACKS (system), &render_pass);

    kan_free_general (system->utility_allocation_group, attachment_descriptions,
                      sizeof (VkAttachmentDescription) * description->attachments_count);
    kan_free_general (system->utility_allocation_group, color_attachments,
                      sizeof (VkAttachmentReference) * description->attachments_count);

    if (result != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to create render pass \"%s\".",
                 description->tracking_name)
        return NULL;
    }

    struct render_backend_pass_t *pass =
        kan_allocate_batched (system->pass_wrapper_allocation_group, sizeof (struct render_backend_pass_t));
    kan_bd_list_add (&system->passes, NULL, &pass->list_node);
    pass->pass = render_pass;
    return pass;
}

void render_backend_system_destroy_pass (struct render_backend_system_t *system, struct render_backend_pass_t *pass)
{
    vkDestroyRenderPass (system->device, pass->pass, VULKAN_ALLOCATION_CALLBACKS (system));
    kan_free_batched (system->pass_wrapper_allocation_group, pass);
}

kan_render_pass_t kan_render_pass_create (kan_render_context_t context,
                                          struct kan_render_pass_description_t *description)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) context;
    kan_atomic_int_lock (&system->resource_management_lock);
    struct render_backend_pass_t *pass = render_backend_system_create_pass (system, description);
    kan_atomic_int_unlock (&system->resource_management_lock);
    return pass ? (kan_render_pass_t) pass : KAN_INVALID_RENDER_PASS;
}

kan_bool_t kan_render_pass_add_static_dependency (kan_render_pass_t pass, kan_render_pass_t dependency)
{
    // TODO: Implement.
    return KAN_FALSE;
}

kan_render_pass_instance_t kan_render_pass_instantiate (kan_render_pass_t pass,
                                                        kan_render_frame_buffer_t frame_buffer,
                                                        struct kan_render_viewport_bounds_t *viewport_bounds,
                                                        struct kan_render_integer_region_t *scissor)
{
    // TODO: Implement.
    return KAN_INVALID_RENDER_PASS_INSTANCE;
}

void kan_render_pass_instance_add_dynamic_dependency (kan_render_pass_instance_t pass_instance,
                                                      kan_render_pass_instance_t dependency)
{
    // TODO: Implement.
}

void kan_render_pass_instance_graphics_pipeline (kan_render_pass_instance_t pass_instance,
                                                 kan_render_pipeline_parameter_set_t *parameter_sets)
{
    // TODO: Implement.
}

void kan_render_pass_instance_attributes (kan_render_pass_instance_t pass_instance,
                                          uint64_t start_at_binding,
                                          uint64_t buffers_count,
                                          kan_render_buffer_t *buffers)
{
    // TODO: Implement.
}

void kan_render_pass_instance_indices (kan_render_pass_instance_t pass_instance, kan_render_buffer_t buffer)
{
    // TODO: Implement.
}

void kan_render_pass_instance_draw (kan_render_pass_instance_t pass_instance,
                                    uint64_t vertex_offset,
                                    uint64_t vertex_count)
{
    // TODO: Implement.
}

void kan_render_pass_instance_instanced_draw (kan_render_pass_instance_t pass_instance,
                                              uint64_t vertex_offset,
                                              uint64_t vertex_count,
                                              uint64_t instance_offset,
                                              uint64_t instance_count)
{
    // TODO: Implement.
}

void kan_render_pass_destroy (kan_render_pass_t pass)
{
    struct render_backend_pass_t *data = (struct render_backend_pass_t *) pass;
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_pass_destroy_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_pass_destroy_t);

    // We do not need to preserve order as passes cannot depend one on another.
    item->next = schedule->first_scheduled_pass_destroy;
    schedule->first_scheduled_pass_destroy = item;
    item->pass = data;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}
