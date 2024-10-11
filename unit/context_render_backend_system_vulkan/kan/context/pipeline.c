#include <kan/context/render_backend_implementation_interface.h>

static inline VkBlendFactor to_vulkan_blend_factor (enum kan_render_blend_factor_t factor)
{
    switch (factor)
    {
    case KAN_RENDER_BLEND_FACTOR_ZERO:
        return VK_BLEND_FACTOR_ZERO;

    case KAN_RENDER_BLEND_FACTOR_ONE:
        return VK_BLEND_FACTOR_ONE;

    case KAN_RENDER_BLEND_FACTOR_SOURCE_COLOR:
        return VK_BLEND_FACTOR_SRC_COLOR;

    case KAN_RENDER_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;

    case KAN_RENDER_BLEND_FACTOR_DESTINATION_COLOR:
        return VK_BLEND_FACTOR_DST_COLOR;

    case KAN_RENDER_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;

    case KAN_RENDER_BLEND_FACTOR_SOURCE_ALPHA:
        return VK_BLEND_FACTOR_SRC_ALPHA;

    case KAN_RENDER_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    case KAN_RENDER_BLEND_FACTOR_DESTINATION_ALPHA:
        return VK_BLEND_FACTOR_DST_ALPHA;

    case KAN_RENDER_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;

    case KAN_RENDER_BLEND_FACTOR_CONSTANT_COLOR:
        return VK_BLEND_FACTOR_CONSTANT_COLOR;

    case KAN_RENDER_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;

    case KAN_RENDER_BLEND_FACTOR_CONSTANT_ALPHA:
        return VK_BLEND_FACTOR_CONSTANT_ALPHA;

    case KAN_RENDER_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;

    case KAN_RENDER_BLEND_FACTOR_SOURCE_ALPHA_SATURATE:
        return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    }

    KAN_ASSERT (KAN_FALSE)
    return VK_BLEND_FACTOR_MAX_ENUM;
}

static inline VkBlendOp to_vulkan_blend_operation (enum kan_render_blend_operation_t operation)
{
    switch (operation)
    {
    case KAN_RENDER_BLEND_OPERATION_ADD:
        return VK_BLEND_OP_ADD;

    case KAN_RENDER_BLEND_OPERATION_SUBTRACT:
        return VK_BLEND_OP_SUBTRACT;

    case KAN_RENDER_BLEND_OPERATION_REVERSE_SUBTRACT:
        return VK_BLEND_OP_REVERSE_SUBTRACT;

    case KAN_RENDER_BLEND_OPERATION_MIN:
        return VK_BLEND_OP_MIN;

    case KAN_RENDER_BLEND_OPERATION_MAX:
        return VK_BLEND_OP_MAX;
    }

    KAN_ASSERT (KAN_FALSE)
    return VK_BLEND_OP_MAX_ENUM;
}

static inline kan_bool_t add_classic_graphics_request_unsafe (
    struct render_backend_pipeline_compiler_state_t *state,
    struct classic_graphics_pipeline_compilation_request_t *request)
{
#define DO_FOR_PRIORITY(PRIORITY)                                                                                      \
    request->next = NULL;                                                                                              \
    request->previous = state->last_classic_graphics_##PRIORITY;                                                       \
                                                                                                                       \
    if (state->last_classic_graphics_##PRIORITY)                                                                       \
    {                                                                                                                  \
        state->last_classic_graphics_##PRIORITY->next = request;                                                       \
        state->last_classic_graphics_##PRIORITY = request;                                                             \
        return KAN_FALSE;                                                                                              \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        state->first_classic_graphics_##PRIORITY = request;                                                            \
        state->last_classic_graphics_##PRIORITY = request;                                                             \
        return KAN_TRUE;                                                                                               \
    }

    switch (request->pipeline->compilation_priority)
    {
    case KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CRITICAL:
        DO_FOR_PRIORITY (critical)

    case KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_ACTIVE:
        DO_FOR_PRIORITY (active)

    case KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CACHE:
        DO_FOR_PRIORITY (cache)
    }

#undef DO_FOR_PRIORITY
    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

kan_thread_result_t render_backend_pipeline_compiler_state_worker_function (kan_thread_user_data_t user_data)
{
    struct render_backend_pipeline_compiler_state_t *state =
        (struct render_backend_pipeline_compiler_state_t *) user_data;

    while (KAN_TRUE)
    {
        if (kan_atomic_int_get (&state->should_terminate))
        {
            return 0;
        }

        struct classic_graphics_pipeline_compilation_request_t *request = NULL;
        kan_mutex_lock (state->state_transition_mutex);

        while (KAN_TRUE)
        {
            if (kan_atomic_int_get (&state->should_terminate))
            {
                kan_mutex_unlock (state->state_transition_mutex);
                return 0;
            }

            if (!state->first_classic_graphics_critical && !state->first_classic_graphics_active &&
                !state->first_classic_graphics_cache)
            {
                kan_conditional_variable_wait (state->has_more_work, state->state_transition_mutex);
                continue;
            }

            KAN_ASSERT (state->first_classic_graphics_critical || state->first_classic_graphics_active ||
                        state->first_classic_graphics_cache)

            if (state->first_classic_graphics_critical)
            {
                request = state->first_classic_graphics_critical;
                state->first_classic_graphics_critical = request->next;

                if (request == state->last_classic_graphics_critical)
                {
                    state->last_classic_graphics_critical = NULL;
                }
            }
            else if (state->first_classic_graphics_active)
            {
                request = state->first_classic_graphics_active;
                state->first_classic_graphics_active = request->next;

                if (request == state->last_classic_graphics_active)
                {
                    state->last_classic_graphics_active = NULL;
                }
            }
            else if (state->first_classic_graphics_cache)
            {
                request = state->first_classic_graphics_cache;
                state->first_classic_graphics_cache = request->next;

                if (request == state->last_classic_graphics_cache)
                {
                    state->last_classic_graphics_cache = NULL;
                }
            }
            else
            {
                KAN_ASSERT (KAN_FALSE)
            }

            break;
        }

        request->pipeline->compilation_state = PIPELINE_COMPILATION_STATE_EXECUTION;
        kan_mutex_unlock (state->state_transition_mutex);

        struct render_backend_classic_graphics_pipeline_family_t *family = request->pipeline->family;
        VkPipelineVertexInputStateCreateInfo vertex_input_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .vertexBindingDescriptionCount = (uint32_t) family->input_bindings_count,
            .pVertexBindingDescriptions = family->input_bindings,
            .vertexAttributeDescriptionCount = (uint32_t) family->attributes_count,
            .pVertexAttributeDescriptions = family->attributes,
        };

        VkPrimitiveTopology topology;
        switch (family->topology)
        {
        case KAN_RENDER_CLASSIC_GRAPHICS_TOPOLOGY_TRIANGLE_LIST:
            topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            break;
        }

        VkPipelineInputAssemblyStateCreateInfo input_assembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .topology = topology,
            .primitiveRestartEnable = VK_FALSE,
        };

        // Viewport and scissor are arbitrary as they're overwritten by dynamic states.
        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = 1.0f,
            .height = 1.0f,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        VkRect2D scissor = {
            .offset = {0, 0},
            .extent = {1, 1},
        };

        VkPipelineViewportStateCreateInfo viewport_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .viewportCount = 1u,
            .pViewports = &viewport,
            .scissorCount = 1u,
            .pScissors = &scissor,
        };

        VkDynamicState dynamic_state_array[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .dynamicStateCount = 2u,
            .pDynamicStates = dynamic_state_array,
        };

        VkGraphicsPipelineCreateInfo pipeline_create_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .stageCount = (uint32_t) request->shader_stages_count,
            .pStages = request->shader_stages,
            .pVertexInputState = &vertex_input_info,
            .pInputAssemblyState = &input_assembly,
            .pTessellationState = NULL,
            .pViewportState = &viewport_state,
            .pRasterizationState = &request->rasterization,
            .pMultisampleState = &request->multisampling,
            .pDepthStencilState = &request->depth_stencil,
            .pColorBlendState = &request->color_blending,
            .pDynamicState = &dynamic_state,
            .layout = family->layout,
            .renderPass = request->pipeline->pass->pass,
            .subpass = 0u,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1,
        };

        VkPipeline pipeline;
        VkResult result =
            vkCreateGraphicsPipelines (request->pipeline->system->device, VK_NULL_HANDLE, 1u, &pipeline_create_info,
                                       VULKAN_ALLOCATION_CALLBACKS (request->pipeline->system), &pipeline);

        if (result != VK_SUCCESS)
        {
            pipeline = VK_NULL_HANDLE;
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Failed to compile pipeline \"%s\" from family \"%s\".", request->pipeline->tracking_name,
                     request->pipeline->family->tracking_name)
        }

        kan_mutex_lock (state->state_transition_mutex);
        request->pipeline->pipeline = pipeline;
        request->pipeline->compilation_state =
            result == VK_SUCCESS ? PIPELINE_COMPILATION_STATE_SUCCESS : PIPELINE_COMPILATION_STATE_FAILURE;
        request->pipeline->compilation_request = NULL;

        kan_mutex_unlock (state->state_transition_mutex);
        render_backend_compiler_state_destroy_classic_graphics_request (request);
    }
}

void render_backend_compiler_state_request_classic_graphics (
    struct render_backend_pipeline_compiler_state_t *state,
    struct render_backend_classic_graphics_pipeline_t *pipeline,
    struct kan_render_classic_graphics_pipeline_description_t *description)
{
    struct classic_graphics_pipeline_compilation_request_t *request =
        kan_allocate_batched (pipeline->system->pipeline_wrapper_allocation_group,
                              sizeof (struct classic_graphics_pipeline_compilation_request_t));

    request->pipeline = pipeline;
    pipeline->compilation_request = request;
    pipeline->compilation_state = PIPELINE_COMPILATION_STATE_PENDING;

    request->shader_stages_count = 0u;
    for (uint64_t module_index = 0u; module_index < description->code_modules_count; ++module_index)
    {
        request->shader_stages_count += description->code_modules[module_index].entry_points_count;
    }

    request->shader_stages =
        kan_allocate_general (pipeline->system->pipeline_wrapper_allocation_group,
                              sizeof (VkPipelineShaderStageCreateInfo) * request->shader_stages_count,
                              _Alignof (VkPipelineShaderStageCreateInfo));

    VkPipelineShaderStageCreateInfo *output_stage = request->shader_stages;
    KAN_ASSERT (pipeline->shader_modules_count == description->code_modules_count)

    for (uint64_t module_index = 0u; module_index < description->code_modules_count; ++module_index)
    {
        VkShaderModule module = pipeline->shader_modules[module_index];
        for (uint64_t entry_point_index = 0u;
             entry_point_index < description->code_modules[module_index].entry_points_count; ++entry_point_index)
        {
            struct kan_render_pipeline_code_entry_point_t *entry_point =
                &description->code_modules[module_index].entry_points[entry_point_index];

            output_stage->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            output_stage->pNext = NULL;
            output_stage->flags = 0u;
            output_stage->module = module;
            output_stage->pName = entry_point->function_name;
            output_stage->pSpecializationInfo = NULL;

            switch (entry_point->stage)
            {
            case KAN_RENDER_STAGE_CLASSIC_GRAPHICS_VERTEX:
                output_stage->stage = VK_SHADER_STAGE_VERTEX_BIT;
                break;

            case KAN_RENDER_STAGE_CLASSIC_GRAPHICS_FRAGMENT:
                output_stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
            }

            ++output_stage;
        }
    }

    VkPolygonMode polygon_mode;
    switch (description->polygon_mode)
    {
    case KAN_RENDER_POLYGON_MODE_FILL:
        polygon_mode = VK_POLYGON_MODE_FILL;
        break;

    case KAN_RENDER_POLYGON_MODE_WIREFRAME:
        polygon_mode = VK_POLYGON_MODE_LINE;
        break;
    }

    VkCullModeFlags cull_mode;
    switch (description->cull_mode)
    {
    case KAN_RENDER_CULL_MODE_BACK:
        cull_mode = VK_CULL_MODE_BACK_BIT;
        break;
    }

    request->rasterization = (VkPipelineRasterizationStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .depthClampEnable = description->use_depth_clamp,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = polygon_mode,
        .cullMode = cull_mode,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f,
    };

    request->multisampling = (VkPipelineMultisampleStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = NULL,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    VkCompareOp depth_compare;
    switch (description->depth_compare_operation)
    {
    case KAN_RENDER_DEPTH_COMPARE_OPERATION_NEVER:
        depth_compare = VK_COMPARE_OP_NEVER;
        break;

    case KAN_RENDER_DEPTH_COMPARE_OPERATION_ALWAYS:
        depth_compare = VK_COMPARE_OP_ALWAYS;
        break;

    case KAN_RENDER_DEPTH_COMPARE_OPERATION_EQUAL:
        depth_compare = VK_COMPARE_OP_EQUAL;
        break;

    case KAN_RENDER_DEPTH_COMPARE_OPERATION_NOT_EQUAL:
        depth_compare = VK_COMPARE_OP_NOT_EQUAL;
        break;

    case KAN_RENDER_DEPTH_COMPARE_OPERATION_LESS:
        depth_compare = VK_COMPARE_OP_LESS;
        break;

    case KAN_RENDER_DEPTH_COMPARE_OPERATION_LESS_OR_EQUAL:
        depth_compare = VK_COMPARE_OP_LESS_OR_EQUAL;
        break;

    case KAN_RENDER_DEPTH_COMPARE_OPERATION_GREATER:
        depth_compare = VK_COMPARE_OP_GREATER;
        break;

    case KAN_RENDER_DEPTH_COMPARE_OPERATION_GREATER_OR_EQUAL:
        depth_compare = VK_COMPARE_OP_GREATER_OR_EQUAL;
        break;
    }

    request->depth_stencil = (VkPipelineDepthStencilStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .depthTestEnable = description->depth_test_enabled,
        .depthWriteEnable = description->depth_write_enabled,
        .depthCompareOp = depth_compare,
        .depthBoundsTestEnable = description->depth_bounds_test_enabled,
        .stencilTestEnable = VK_FALSE,
        // Stencil test disabled, therefore we don't care about front and back at all.
        .front =
            {
                .failOp = VK_STENCIL_OP_MAX_ENUM,
                .passOp = VK_STENCIL_OP_MAX_ENUM,
                .depthFailOp = VK_STENCIL_OP_MAX_ENUM,
                .compareOp = VK_COMPARE_OP_MAX_ENUM,
                .compareMask = 0u,
                .writeMask = 0u,
                .reference = 0u,
            },
        .back =
            {
                .failOp = VK_STENCIL_OP_MAX_ENUM,
                .passOp = VK_STENCIL_OP_MAX_ENUM,
                .depthFailOp = VK_STENCIL_OP_MAX_ENUM,
                .compareOp = VK_COMPARE_OP_MAX_ENUM,
                .compareMask = 0u,
                .writeMask = 0u,
                .reference = 0u,
            },
        .minDepthBounds = description->min_depth,
        .maxDepthBounds = description->max_depth,
    };

    request->color_blending_attachments_count = description->output_setups_count;
    request->color_blending_attachments =
        kan_allocate_general (pipeline->system->pipeline_wrapper_allocation_group,
                              sizeof (VkPipelineColorBlendAttachmentState) * request->color_blending_attachments_count,
                              _Alignof (VkPipelineColorBlendAttachmentState));

    for (uint64_t attachment_index = 0u; attachment_index < request->color_blending_attachments_count;
         ++attachment_index)
    {
        struct kan_render_color_output_setup_description_t *source = &description->output_setups[attachment_index];
        VkPipelineColorBlendAttachmentState *target = &request->color_blending_attachments[attachment_index];

        target->blendEnable = source->use_blend;
        target->colorWriteMask = 0u;

        if (source->write_r)
        {
            target->colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
        }

        if (source->write_g)
        {
            target->colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
        }

        if (source->write_b)
        {
            target->colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
        }

        if (source->write_a)
        {
            target->colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
        }

        target->srcColorBlendFactor = to_vulkan_blend_factor (source->source_color_blend_factor);
        target->dstColorBlendFactor = to_vulkan_blend_factor (source->destination_color_blend_factor);
        target->colorBlendOp = to_vulkan_blend_operation (source->color_blend_operation);
        target->srcAlphaBlendFactor = to_vulkan_blend_factor (source->source_alpha_blend_factor);
        target->dstAlphaBlendFactor = to_vulkan_blend_factor (source->destination_alpha_blend_factor);
        target->alphaBlendOp = to_vulkan_blend_operation (source->alpha_blend_operation);
    }

    request->color_blending = (VkPipelineColorBlendStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = (uint32_t) request->color_blending_attachments_count,
        .pAttachments = request->color_blending_attachments,
        .blendConstants =
            {
                description->blend_constant_r,
                description->blend_constant_g,
                description->blend_constant_b,
                description->blend_constant_a,
            },
    };

    kan_mutex_lock (state->state_transition_mutex);
    kan_bool_t first_in_the_queue = add_classic_graphics_request_unsafe (state, request);
    kan_mutex_unlock (state->state_transition_mutex);

    if (first_in_the_queue)
    {
        kan_conditional_variable_signal_one (state->has_more_work);
    }
}

void render_backend_compiler_state_destroy_classic_graphics_request (
    struct classic_graphics_pipeline_compilation_request_t *request)
{
    kan_allocation_group_t allocation_group = request->pipeline->system->pipeline_wrapper_allocation_group;
    kan_free_general (allocation_group, request->shader_stages,
                      sizeof (VkPipelineShaderStageCreateInfo) * request->shader_stages_count);
    kan_free_general (allocation_group, request->color_blending_attachments,
                      sizeof (VkPipelineColorBlendAttachmentState) * request->color_blending_attachments_count);
    kan_free_batched (allocation_group, request);
}

struct render_backend_classic_graphics_pipeline_t *render_backend_system_create_classic_graphics_pipeline (
    struct render_backend_system_t *system,
    struct kan_render_classic_graphics_pipeline_description_t *description,
    enum kan_render_pipeline_compilation_priority_t compilation_priority)
{
    kan_bool_t shader_modules_created = KAN_TRUE;
    VkShaderModule *shader_modules =
        kan_allocate_general (system->pipeline_wrapper_allocation_group,
                              sizeof (VkShaderModule) * description->code_modules_count, _Alignof (VkShaderModule));

    for (uint64_t module_index = 0u; module_index < description->code_modules_count; ++module_index)
    {
        shader_modules[module_index] = VK_NULL_HANDLE;
    }

    for (uint64_t module_index = 0u; module_index < description->code_modules_count; ++module_index)
    {
        VkShaderModuleCreateInfo module_create_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .codeSize = description->code_modules[module_index].code_length,
            .pCode = (const uint32_t *) description->code_modules[module_index].code,
        };

        if (vkCreateShaderModule (system->device, &module_create_info, VULKAN_ALLOCATION_CALLBACKS (system),
                                  &shader_modules[module_index]))
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Failed to compile shader module %lu of pipeline \"%s\" from family \"%s\".",
                     (unsigned long) module_index, description->tracking_name,
                     ((struct render_backend_classic_graphics_pipeline_family_t *) description->family)->tracking_name)
            shader_modules_created = KAN_FALSE;
            break;
        }
    }

    if (!shader_modules_created)
    {
        for (uint64_t module_index = 0u; module_index < description->code_modules_count; ++module_index)
        {
            if (shader_modules[module_index] != VK_NULL_HANDLE)
            {
                vkDestroyShaderModule (system->device, shader_modules[module_index],
                                       VULKAN_ALLOCATION_CALLBACKS (system));
            }
        }

        kan_free_general (system->pipeline_wrapper_allocation_group, shader_modules,
                          sizeof (VkShaderModule) * description->code_modules_count);
        return NULL;
    }

    struct render_backend_classic_graphics_pipeline_t *pipeline = kan_allocate_batched (
        system->pipeline_wrapper_allocation_group, sizeof (struct render_backend_classic_graphics_pipeline_t));

    pipeline->next = system->first_classic_graphics_pipeline;
    pipeline->previous = NULL;
    pipeline->system = system;

    if (system->first_classic_graphics_pipeline)
    {
        system->first_classic_graphics_pipeline->previous = pipeline;
    }

    system->first_classic_graphics_pipeline = pipeline;
    pipeline->pipeline = VK_NULL_HANDLE;
    pipeline->pass = (struct render_backend_pass_t *) description->pass;
    pipeline->family = (struct render_backend_classic_graphics_pipeline_family_t *) description->family;

    pipeline->min_depth = description->min_depth;
    pipeline->max_depth = description->max_depth;

    pipeline->shader_modules_count = description->code_modules_count;
    pipeline->shader_modules = shader_modules;

    // Because creation is done under resource management lock and compilation request does not require it,
    // we schedule compilation separately. Therefore, initially pipeline is in compilation failed state.
    pipeline->compilation_priority = compilation_priority;
    pipeline->compilation_state = PIPELINE_COMPILATION_STATE_FAILURE;
    pipeline->compilation_request = NULL;

    pipeline->tracking_name = description->tracking_name;
    return pipeline;
}

void render_backend_system_destroy_classic_graphics_pipeline (
    struct render_backend_system_t *system,
    struct render_backend_classic_graphics_pipeline_t *pipeline,
    kan_bool_t remove_from_list)
{
    if (remove_from_list)
    {
        if (pipeline->next)
        {
            pipeline->next->previous = pipeline->previous;
        }

        if (pipeline->previous)
        {
            pipeline->previous->next = pipeline->next;
        }
        else
        {
            KAN_ASSERT (system->first_classic_graphics_pipeline = pipeline)
            system->first_classic_graphics_pipeline = pipeline->next;
        }
    }

    // Compilation request should be dealt with previously.
    KAN_ASSERT (!pipeline->compilation_request)

    if (pipeline->pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline (system->device, pipeline->pipeline, VULKAN_ALLOCATION_CALLBACKS (system));
    }

    for (uint64_t module_index = 0u; module_index < pipeline->shader_modules_count; ++module_index)
    {
        vkDestroyShaderModule (system->device, pipeline->shader_modules[module_index],
                               VULKAN_ALLOCATION_CALLBACKS (system));
    }

    kan_free_general (system->pipeline_wrapper_allocation_group, pipeline->shader_modules,
                      sizeof (VkShaderModule) * pipeline->shader_modules_count);
}

kan_render_classic_graphics_pipeline_t kan_render_classic_graphics_pipeline_create (
    kan_render_context_t context,
    struct kan_render_classic_graphics_pipeline_description_t *description,
    enum kan_render_pipeline_compilation_priority_t compilation_priority)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) context;
    kan_atomic_int_lock (&system->resource_management_lock);
    struct render_backend_classic_graphics_pipeline_t *pipeline =
        render_backend_system_create_classic_graphics_pipeline (system, description, compilation_priority);
    kan_atomic_int_unlock (&system->resource_management_lock);

    render_backend_compiler_state_request_classic_graphics (&system->compiler_state, pipeline, description);
    return pipeline ? (kan_render_classic_graphics_pipeline_t) pipeline : KAN_INVALID_RENDER_CLASSIC_GRAPHICS_PIPELINE;
}

void kan_render_classic_graphics_pipeline_change_compilation_priority (
    kan_render_classic_graphics_pipeline_t pipeline,
    enum kan_render_pipeline_compilation_priority_t compilation_priority)
{
    struct render_backend_classic_graphics_pipeline_t *data =
        (struct render_backend_classic_graphics_pipeline_t *) pipeline;

    // Shortcut to avoid excessive locking: check if it is not already too late without even locking.
    if (data->compilation_state != PIPELINE_COMPILATION_STATE_PENDING)
    {
        return;
    }

    kan_mutex_lock (data->system->compiler_state.state_transition_mutex);
    if (data->compilation_state != PIPELINE_COMPILATION_STATE_PENDING)
    {
        // While we were locking, request was already moved from pending to other state.
        kan_mutex_unlock (data->system->compiler_state.state_transition_mutex);
        return;
    }

    render_backend_pipeline_compiler_state_remove_classic_graphics_request_unsafe (&data->system->compiler_state,
                                                                                   data->compilation_request);
    data->compilation_priority = compilation_priority;
    add_classic_graphics_request_unsafe (&data->system->compiler_state, data->compilation_request);
    kan_mutex_unlock (data->system->compiler_state.state_transition_mutex);
}

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_classic_graphics_pipeline_destroy (
    kan_render_classic_graphics_pipeline_t pipeline)
{
    struct render_backend_classic_graphics_pipeline_t *data =
        (struct render_backend_classic_graphics_pipeline_t *) pipeline;
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_classic_graphics_pipeline_destroy_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &schedule->item_allocator, struct scheduled_classic_graphics_pipeline_destroy_t);

    item->next = schedule->first_scheduled_classic_graphics_pipeline_destroy;
    schedule->first_scheduled_classic_graphics_pipeline_destroy = item;
    item->pipeline = data;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}
