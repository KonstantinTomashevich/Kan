#include <kan/context/render_backend_implementation_interface.h>

struct render_backend_pass_t *render_backend_system_create_pass (struct render_backend_system_t *system,
                                                                 struct kan_render_pass_description_t *description)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_pass_internal);

    VkAttachmentDescription *attachment_descriptions = kan_allocate_general (
        system->utility_allocation_group, sizeof (VkAttachmentDescription) * description->attachments_count,
        _Alignof (VkAttachmentDescription));

    VkAttachmentReference *color_attachments = kan_allocate_general (
        system->utility_allocation_group, sizeof (VkAttachmentReference) * description->attachments_count,
        _Alignof (VkAttachmentReference));

    VkAttachmentReference *next_color_attachment = color_attachments;
    kan_instance_size_t color_attachments_count = 0u;
    kan_bool_t has_depth_attachment = KAN_FALSE;
    VkAttachmentReference depth_attachment;

    for (kan_loop_size_t index = 0u; index < description->attachments_count; ++index)
    {
        struct kan_render_pass_attachment_t *attachment = &description->attachments[index];
        struct VkAttachmentDescription *vulkan_attachment = &attachment_descriptions[index];
        vulkan_attachment->flags = 0u;
        vulkan_attachment->format = image_format_to_vulkan (attachment->format);

        switch (attachment->type)
        {
        case KAN_RENDER_PASS_ATTACHMENT_COLOR:
            ++color_attachments_count;

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
            next_color_attachment->attachment = (vulkan_size_t) index;
            next_color_attachment->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            ++next_color_attachment;
            break;

        case KAN_RENDER_PASS_ATTACHMENT_DEPTH:
        case KAN_RENDER_PASS_ATTACHMENT_STENCIL:
        case KAN_RENDER_PASS_ATTACHMENT_DEPTH_STENCIL:
            // There should be not more than 1 depth attachment per pass.
            KAN_ASSERT (!has_depth_attachment)
            has_depth_attachment = KAN_TRUE;

            KAN_ASSERT (attachment->samples == 1u)
            vulkan_attachment->samples = VK_SAMPLE_COUNT_1_BIT;
            vulkan_attachment->finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            depth_attachment.attachment = (vulkan_size_t) index;
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

        // We'll do the transition ourselves in order to make sure that transition works as expected.
        // It might not be the best from performance point of view, might need investigation later.
        vulkan_attachment->initialLayout = vulkan_attachment->finalLayout;
    }

    VkSubpassDescription sub_pass_description = {
        .flags = 0u,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0u,
        .pInputAttachments = NULL,
        .colorAttachmentCount = (vulkan_size_t) color_attachments_count,
        .pColorAttachments = color_attachments,
        .pResolveAttachments = NULL,
        .pDepthStencilAttachment = has_depth_attachment ? &depth_attachment : NULL,
        .preserveAttachmentCount = 0u,
        .pPreserveAttachments = NULL,
    };

    vulkan_size_t destination_access_mask = 0u;
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
        .attachmentCount = (vulkan_size_t) description->attachments_count,
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
        kan_cpu_section_execution_shutdown (&execution);
        return NULL;
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
    snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "RenderPass::%s",
              description->tracking_name);

    struct VkDebugUtilsObjectNameInfoEXT object_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = NULL,
        .objectType = VK_OBJECT_TYPE_RENDER_PASS,
        .objectHandle = CONVERT_HANDLE_FOR_DEBUG render_pass,
        .pObjectName = debug_name,
    };

    vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
#endif

    struct render_backend_pass_t *pass =
        kan_allocate_batched (system->pass_wrapper_allocation_group, sizeof (struct render_backend_pass_t));

    kan_atomic_int_lock (&system->resource_registration_lock);
    kan_bd_list_add (&system->passes, NULL, &pass->list_node);
    kan_atomic_int_unlock (&system->resource_registration_lock);

    pass->pass = render_pass;
    pass->system = system;
    pass->first_dependant_pass = NULL;
    pass->first_instance = NULL;
    pass->tracking_name = description->tracking_name;

    kan_cpu_section_execution_shutdown (&execution);
    return pass;
}

void render_backend_system_destroy_pass (struct render_backend_system_t *system, struct render_backend_pass_t *pass)
{
    vkDestroyRenderPass (system->device, pass->pass, VULKAN_ALLOCATION_CALLBACKS (system));
    struct render_backend_pass_dependency_t *dependency = pass->first_dependant_pass;

    while (dependency)
    {
        struct render_backend_pass_dependency_t *next = dependency->next;
        kan_free_batched (system->pass_wrapper_allocation_group, dependency);
        dependency = next;
    }

    kan_free_batched (system->pass_wrapper_allocation_group, pass);
}

void render_backend_pass_instance_add_dependency_internal (struct render_backend_pass_instance_t *dependant,
                                                           struct render_backend_pass_instance_t *dependency)
{
    struct render_backend_pass_instance_dependency_t *dependency_info = dependency->first_dependant;
    while (dependency_info)
    {
        if (dependency_info->dependant_pass_instance == dependant)
        {
            return;
        }

        dependency_info = dependency_info->next;
    }

    dependency_info = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&dependant->system->pass_instance_allocator,
                                                                struct render_backend_pass_instance_dependency_t);

    dependency_info->next = dependency->first_dependant;
    dependency->first_dependant = dependency_info;
    dependency_info->dependant_pass_instance = dependant;

    if (dependant->dependencies_left == 0u)
    {
        kan_bd_list_remove (&dependant->system->pass_instances_available, &dependant->node_in_available);
    }

    ++dependant->dependencies_left;
}

kan_render_pass_t kan_render_pass_create (kan_render_context_t context,
                                          struct kan_render_pass_description_t *description)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (context);
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_pass);
    struct render_backend_pass_t *pass = render_backend_system_create_pass (system, description);
    kan_cpu_section_execution_shutdown (&execution);
    return pass ? KAN_HANDLE_SET (kan_render_pass_t, pass) : KAN_HANDLE_SET_INVALID (kan_render_pass_t);
}

void kan_render_pass_add_static_dependency (kan_render_pass_t pass, kan_render_pass_t dependency)
{
    struct render_backend_pass_t *dependant_pass = KAN_HANDLE_GET (pass);
    struct render_backend_pass_t *dependency_pass = KAN_HANDLE_GET (dependency);

    kan_atomic_int_lock (&dependant_pass->system->pass_static_dependency_lock);
    struct render_backend_pass_dependency_t *dependency_info = dependency_pass->first_dependant_pass;

    // Check if dependency already exists.
    while (dependency_info)
    {
        if (dependency_info->dependant_pass == dependant_pass)
        {
            break;
        }

        dependency_info = dependency_info->next;
    }

    if (!dependency_info)
    {
        // Dependency is new, create it.
        dependency_info = kan_allocate_batched (dependant_pass->system->pass_wrapper_allocation_group,
                                                sizeof (struct render_backend_pass_dependency_t));
        dependency_info->next = dependency_pass->first_dependant_pass;
        dependency_pass->first_dependant_pass = dependency_info;
        dependency_info->dependant_pass = dependant_pass;
    }

    kan_atomic_int_unlock (&dependant_pass->system->pass_static_dependency_lock);
}

kan_render_pass_instance_t kan_render_pass_instantiate (kan_render_pass_t pass,
                                                        kan_render_frame_buffer_t frame_buffer,
                                                        struct kan_render_viewport_bounds_t *viewport_bounds,
                                                        struct kan_render_integer_region_t *scissor,
                                                        struct kan_render_clear_value_t *attachment_clear_values)
{
    struct render_backend_pass_t *pass_data = KAN_HANDLE_GET (pass);
    struct render_backend_frame_buffer_t *frame_buffer_data = KAN_HANDLE_GET (frame_buffer);

    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, pass_data->system->section_create_pass_instance);

    VkFramebuffer selected_frame_buffer = frame_buffer_data->instance_array ?
                                              frame_buffer_data->instance_array[frame_buffer_data->instance_index] :
                                              frame_buffer_data->instance;

    // We intentionally skip frame buffer check as frame buffer might be created during frame end before submission.
    if (!pass_data->system->frame_started)
    {
        kan_cpu_section_execution_shutdown (&execution);
        return KAN_HANDLE_SET_INVALID (kan_render_pass_instance_t);
    }

    struct render_backend_command_state_t *command_state =
        &pass_data->system->command_states[pass_data->system->current_frame_in_flight_index];
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    kan_atomic_int_lock (&command_state->command_operation_lock);
    const kan_instance_size_t command_buffer_index = command_state->secondary_command_buffers_used;
    ++command_state->secondary_command_buffers_used;

    if (command_buffer_index >= command_state->secondary_command_buffers.size)
    {
        VkCommandBuffer *slot = kan_dynamic_array_add_last (&command_state->secondary_command_buffers);
        if (!slot)
        {
            kan_dynamic_array_set_capacity (&command_state->secondary_command_buffers,
                                            command_state->secondary_command_buffers.capacity * 2u);
            slot = kan_dynamic_array_add_last (&command_state->secondary_command_buffers);
            KAN_ASSERT (slot)
        }

        VkCommandBufferAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = command_state->command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
            .commandBufferCount = 1u,
        };

        if (vkAllocateCommandBuffers (pass_data->system->device, &allocate_info, slot) != VK_SUCCESS)
        {
            *slot = VK_NULL_HANDLE;
            --command_state->secondary_command_buffers_used;
        }

        command_buffer = *slot;
    }
    else
    {
        command_buffer = ((VkCommandBuffer *) command_state->secondary_command_buffers.data)[command_buffer_index];
    }

    if (command_buffer != VK_NULL_HANDLE)
    {
        struct VkCommandBufferInheritanceInfo inheritance_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
            .pNext = NULL,
            .renderPass = pass_data->pass,
            .subpass = 0u,
            .framebuffer = selected_frame_buffer,
            .occlusionQueryEnable = VK_FALSE,
            .queryFlags = 0u,
            .pipelineStatistics = 0u,
        };

        struct VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
            .pInheritanceInfo = &inheritance_info,
        };

        if (vkBeginCommandBuffer (command_buffer, &begin_info) != VK_SUCCESS)
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Failed to begin command buffer for new pass \"%s\" instance.", pass_data->tracking_name)
            command_buffer = VK_NULL_HANDLE;
        }
    }

    kan_atomic_int_unlock (&command_state->command_operation_lock);
    if (command_buffer == VK_NULL_HANDLE)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Failed to retrieve command buffer for new pass \"%s\" instance.", pass_data->tracking_name)
        kan_cpu_section_execution_shutdown (&execution);
        return KAN_HANDLE_SET_INVALID (kan_render_pass_instance_t);
    }

    kan_atomic_int_lock (&pass_data->system->pass_instance_state_management_lock);
    struct render_backend_pass_instance_t *instance = kan_stack_group_allocator_allocate (
        &pass_data->system->pass_instance_allocator,
        sizeof (struct render_backend_pass_instance_t) + sizeof (VkClearValue) * frame_buffer_data->attachments_count,
        _Alignof (struct render_backend_pass_instance_t));

    instance->system = pass_data->system;
    instance->pass = pass_data;

    instance->command_buffer = command_buffer;
    instance->frame_buffer = KAN_HANDLE_GET (frame_buffer);
    instance->current_pipeline_layout = VK_NULL_HANDLE;
    instance->dependencies_left = 0u;
    instance->first_dependant = NULL;

    instance->next_in_pass = pass_data->first_instance;
    pass_data->first_instance = instance;
    kan_bd_list_add (&pass_data->system->pass_instances, NULL, &instance->node_in_all);
    kan_bd_list_add (&pass_data->system->pass_instances_available, NULL, &instance->node_in_available);
    kan_atomic_int_unlock (&pass_data->system->pass_instance_state_management_lock);

    // Initial commands:
    // - Prepare render pass info for primary buffer. Render pass cannot be started in secondary buffers.
    // - Bind viewport and scissor.

    for (kan_loop_size_t index = 0u; index < frame_buffer_data->attachments_count; ++index)
    {
        switch (frame_buffer_data->attachments[index].type)
        {
        case KAN_FRAME_BUFFER_ATTACHMENT_IMAGE:
            switch (get_image_format_class (frame_buffer_data->attachments[index].image->description.format))
            {
            case IMAGE_FORMAT_CLASS_COLOR:
                instance->clear_values[index].color.float32[0u] = attachment_clear_values[index].color.r;
                instance->clear_values[index].color.float32[1u] = attachment_clear_values[index].color.g;
                instance->clear_values[index].color.float32[2u] = attachment_clear_values[index].color.b;
                instance->clear_values[index].color.float32[3u] = attachment_clear_values[index].color.a;
                break;

            case IMAGE_FORMAT_CLASS_DEPTH:
            case IMAGE_FORMAT_CLASS_STENCIL:
            case IMAGE_FORMAT_CLASS_DEPTH_STENCIL:
                instance->clear_values[index].depthStencil.depth = attachment_clear_values[index].depth_stencil.depth;
                instance->clear_values[index].depthStencil.stencil =
                    attachment_clear_values[index].depth_stencil.stencil;
                break;
            }

            break;

        case KAN_FRAME_BUFFER_ATTACHMENT_SURFACE:
            instance->clear_values[index].color.float32[0u] = attachment_clear_values[index].color.r;
            instance->clear_values[index].color.float32[1u] = attachment_clear_values[index].color.g;
            instance->clear_values[index].color.float32[2u] = attachment_clear_values[index].color.b;
            instance->clear_values[index].color.float32[3u] = attachment_clear_values[index].color.a;
            break;
        }
    }

    instance->render_pass_begin_info = (VkRenderPassBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .renderPass = pass_data->pass,
        .framebuffer = selected_frame_buffer,
        .renderArea =
            {
                .offset =
                    {
                        .x = (vulkan_offset_t) viewport_bounds->x,
                        .y = (vulkan_offset_t) viewport_bounds->y,
                    },
                .extent =
                    {
                        .width = (vulkan_size_t) viewport_bounds->width,
                        .height = (vulkan_size_t) viewport_bounds->height,
                    },
            },
        .clearValueCount = (vulkan_size_t) frame_buffer_data->attachments_count,
        .pClearValues = instance->clear_values,
    };

    VkViewport pass_viewport = {
        .x = viewport_bounds->x,
        .y = viewport_bounds->y + viewport_bounds->height,
        .width = viewport_bounds->width,
        .height = -viewport_bounds->height,
        .minDepth = viewport_bounds->depth_min,
        .maxDepth = viewport_bounds->depth_max,
    };

    VkRect2D pass_scissor = {
        .offset =
            {
                .x = (vulkan_offset_t) scissor->x,
                .y = (vulkan_offset_t) scissor->y,
            },
        .extent =
            {
                .width = (vulkan_size_t) scissor->width,
                .height = (vulkan_size_t) scissor->height,
            },
    };

    kan_atomic_int_lock (&command_state->command_operation_lock);
    vkCmdSetViewport (instance->command_buffer, 0u, 1u, &pass_viewport);
    vkCmdSetScissor (instance->command_buffer, 0u, 1u, &pass_scissor);
    kan_atomic_int_unlock (&command_state->command_operation_lock);

    kan_cpu_section_execution_shutdown (&execution);
    return KAN_HANDLE_SET (kan_render_pass_instance_t, instance);
}

void kan_render_pass_instance_add_dynamic_dependency (kan_render_pass_instance_t pass_instance,
                                                      kan_render_pass_instance_t dependency)
{
    struct render_backend_pass_instance_t *dependant_instance = KAN_HANDLE_GET (pass_instance);
    struct render_backend_pass_instance_t *dependency_instance = KAN_HANDLE_GET (dependency);

    kan_atomic_int_lock (&dependant_instance->system->pass_instance_state_management_lock);
    render_backend_pass_instance_add_dependency_internal (dependant_instance, dependency_instance);
    kan_atomic_int_unlock (&dependant_instance->system->pass_instance_state_management_lock);
}

kan_bool_t kan_render_pass_instance_graphics_pipeline (kan_render_pass_instance_t pass_instance,
                                                       kan_render_graphics_pipeline_t graphics_pipeline)
{
    struct render_backend_pass_instance_t *instance = KAN_HANDLE_GET (pass_instance);
    struct render_backend_graphics_pipeline_t *pipeline = KAN_HANDLE_GET (graphics_pipeline);

    if (pipeline->pipeline == VK_NULL_HANDLE)
    {
        if (pipeline->compilation_priority != KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CRITICAL ||
            pipeline->compilation_state == PIPELINE_COMPILATION_STATE_FAILURE)
        {
            return KAN_FALSE;
        }

        while (pipeline->compilation_state != PIPELINE_COMPILATION_STATE_SUCCESS)
        {
            kan_mutex_lock (pipeline->system->compiler_state.state_transition_mutex);
            switch (pipeline->compilation_state)
            {
            case PIPELINE_COMPILATION_STATE_PENDING:
            case PIPELINE_COMPILATION_STATE_EXECUTION:
            {
                kan_mutex_unlock (pipeline->system->compiler_state.state_transition_mutex);
                struct kan_cpu_section_execution_t wait_execution;
                kan_cpu_section_execution_init (&wait_execution,
                                                pipeline->system->section_wait_for_pipeline_compilation);
                kan_precise_time_sleep (KAN_CONTEXT_RENDER_BACKEND_VULKAN_COMPILATION_WAIT_NS);
                kan_cpu_section_execution_shutdown (&wait_execution);
                break;
            }

            case PIPELINE_COMPILATION_STATE_SUCCESS:
                kan_mutex_unlock (pipeline->system->compiler_state.state_transition_mutex);
                break;

            case PIPELINE_COMPILATION_STATE_FAILURE:
                kan_mutex_unlock (pipeline->system->compiler_state.state_transition_mutex);
                return KAN_FALSE;
            }
        }
    }

    struct render_backend_command_state_t *command_state =
        &instance->system->command_states[instance->system->current_frame_in_flight_index];

    kan_atomic_int_lock (&command_state->command_operation_lock);
    DEBUG_LABEL_INSERT (instance->command_buffer, pipeline->tracking_name, 0.063f, 0.569f, 0.0f, 1.0f)
    vkCmdBindPipeline (instance->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    kan_atomic_int_unlock (&command_state->command_operation_lock);
    instance->current_pipeline_layout = pipeline->family->layout;

    return KAN_TRUE;
}

void kan_render_pass_instance_pipeline_parameter_sets (kan_render_pass_instance_t pass_instance,
                                                       kan_instance_size_t parameter_sets_count,
                                                       kan_render_pipeline_parameter_set_t *parameter_sets)
{
    struct render_backend_pass_instance_t *instance = KAN_HANDLE_GET (pass_instance);
    KAN_ASSERT (instance->current_pipeline_layout)
    struct render_backend_command_state_t *command_state =
        &instance->system->command_states[instance->system->current_frame_in_flight_index];

    // Mutate unstable parameter sets if needed.
    for (kan_loop_size_t index = 0u; index < parameter_sets_count; ++index)
    {
        struct render_backend_pipeline_parameter_set_t *set = KAN_HANDLE_GET (parameter_sets[index]);
        if (!set->layout->stable_binding && set->unstable.last_accessed_allocation_index != UINT32_MAX &&
            set->unstable.last_accessed_allocation_index != set->system->current_frame_in_flight_index)
        {
            VkDescriptorSet source_set =
                set->unstable.allocations[set->system->current_frame_in_flight_index].descriptor_set;
            VkDescriptorSet target_set =
                set->unstable.allocations[set->system->current_frame_in_flight_index].descriptor_set;
            render_backend_apply_descriptor_set_mutation (set, source_set, target_set, 0u, NULL);
        }
    }

    kan_atomic_int_lock (&command_state->command_operation_lock);
    for (kan_loop_size_t index = 0u; index < parameter_sets_count; ++index)
    {
        // We don't implement sequential set optimization as it looks like it is not worth it in most cases for us.
        struct render_backend_pipeline_parameter_set_t *set = KAN_HANDLE_GET (parameter_sets[index]);

        VkDescriptorSet descriptor_set;
        if (set->layout->stable_binding)
        {
            descriptor_set = set->stable.allocation.descriptor_set;
            set->stable.has_been_submitted = KAN_TRUE;
        }
        else
        {
            descriptor_set = set->unstable.allocations[set->system->current_frame_in_flight_index].descriptor_set;
        }

        DEBUG_LABEL_INSERT (instance->command_buffer, set->tracking_name, 0.918f, 0.98f, 0.0f, 1.0f)
        vkCmdBindDescriptorSets (instance->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 instance->current_pipeline_layout, (vulkan_size_t) set->set_index, 1u, &descriptor_set,
                                 0u, NULL);
    }

    kan_atomic_int_unlock (&command_state->command_operation_lock);
}

void kan_render_pass_instance_attributes (kan_render_pass_instance_t pass_instance,
                                          vulkan_size_t start_at_binding,
                                          kan_instance_size_t buffers_count,
                                          kan_render_buffer_t *buffers,
                                          kan_instance_size_t *buffer_offsets)
{
    struct render_backend_pass_instance_t *instance = KAN_HANDLE_GET (pass_instance);
    struct render_backend_command_state_t *command_state =
        &instance->system->command_states[instance->system->current_frame_in_flight_index];

    VkBuffer buffer_handles_static[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES];
    VkDeviceSize vulkan_buffer_offsets_static[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES];

    VkBuffer *buffer_handles = buffer_handles_static;
    VkDeviceSize *vulkan_buffer_offsets = vulkan_buffer_offsets_static;

    if (buffers_count > KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES)
    {
        buffer_handles = kan_allocate_general (instance->system->utility_allocation_group,
                                               sizeof (VkBuffer) * buffers_count, _Alignof (VkBuffer));
        vulkan_buffer_offsets = kan_allocate_general (instance->system->utility_allocation_group,
                                                      sizeof (VkDeviceSize) * buffers_count, _Alignof (VkDeviceSize));
    }

    for (kan_loop_size_t index = 0u; index < buffers_count; ++index)
    {
        struct render_backend_buffer_t *buffer = KAN_HANDLE_GET (buffers[index]);
        KAN_ASSERT (buffer->type == KAN_RENDER_BUFFER_TYPE_ATTRIBUTE)
        buffer_handles[index] = buffer->buffer;
        vulkan_buffer_offsets[index] = buffer_offsets ? buffer_offsets[index] : 0u;
    }

    kan_atomic_int_lock (&command_state->command_operation_lock);
    vkCmdBindVertexBuffers (instance->command_buffer, (vulkan_size_t) start_at_binding, (vulkan_size_t) buffers_count,
                            buffer_handles, vulkan_buffer_offsets);
    kan_atomic_int_unlock (&command_state->command_operation_lock);

    if (buffer_handles != buffer_handles_static)
    {
        kan_free_general (instance->system->utility_allocation_group, buffer_handles,
                          sizeof (VkBuffer) * buffers_count);
        kan_free_general (instance->system->utility_allocation_group, vulkan_buffer_offsets,
                          sizeof (VkDeviceSize) * buffers_count);
    }
}

void kan_render_pass_instance_indices (kan_render_pass_instance_t pass_instance, kan_render_buffer_t buffer)
{
    struct render_backend_pass_instance_t *instance = KAN_HANDLE_GET (pass_instance);
    struct render_backend_buffer_t *index_buffer = KAN_HANDLE_GET (buffer);

    KAN_ASSERT (index_buffer->type == KAN_RENDER_BUFFER_TYPE_INDEX_16 ||
                index_buffer->type == KAN_RENDER_BUFFER_TYPE_INDEX_32)
    const VkIndexType index_type =
        index_buffer->type == KAN_RENDER_BUFFER_TYPE_INDEX_16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

    struct render_backend_command_state_t *command_state =
        &instance->system->command_states[instance->system->current_frame_in_flight_index];

    kan_atomic_int_lock (&command_state->command_operation_lock);
    vkCmdBindIndexBuffer (instance->command_buffer, index_buffer->buffer, 0u, index_type);
    kan_atomic_int_unlock (&command_state->command_operation_lock);
}

void kan_render_pass_instance_draw (kan_render_pass_instance_t pass_instance,
                                    vulkan_size_t index_offset,
                                    kan_instance_size_t index_count,
                                    vulkan_size_t vertex_offset)
{
    struct render_backend_pass_instance_t *instance = KAN_HANDLE_GET (pass_instance);
    struct render_backend_command_state_t *command_state =
        &instance->system->command_states[instance->system->current_frame_in_flight_index];

    kan_atomic_int_lock (&command_state->command_operation_lock);
    vkCmdDrawIndexed (instance->command_buffer, index_count, 1u, index_offset, (vulkan_offset_t) vertex_offset, 0u);
    kan_atomic_int_unlock (&command_state->command_operation_lock);
}

void kan_render_pass_instance_instanced_draw (kan_render_pass_instance_t pass_instance,
                                              vulkan_size_t index_offset,
                                              kan_instance_size_t index_count,
                                              vulkan_size_t vertex_offset,
                                              vulkan_size_t instance_offset,
                                              kan_instance_size_t instance_count)
{
    struct render_backend_pass_instance_t *instance = KAN_HANDLE_GET (pass_instance);
    struct render_backend_command_state_t *command_state =
        &instance->system->command_states[instance->system->current_frame_in_flight_index];

    kan_atomic_int_lock (&command_state->command_operation_lock);
    vkCmdDrawIndexed (instance->command_buffer, index_count, instance_count, index_offset,
                      (vulkan_offset_t) vertex_offset, instance_offset);
    kan_atomic_int_unlock (&command_state->command_operation_lock);
}

void kan_render_pass_destroy (kan_render_pass_t pass)
{
    struct render_backend_pass_t *data = KAN_HANDLE_GET (pass);
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
