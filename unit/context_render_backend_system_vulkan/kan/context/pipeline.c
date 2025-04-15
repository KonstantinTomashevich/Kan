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

static VkStencilOp to_vulkan_stencil_operation (enum kan_render_stencil_operation_t operation)
{
    switch (operation)
    {
    case KAN_RENDER_STENCIL_OPERATION_KEEP:
        return VK_STENCIL_OP_KEEP;

    case KAN_RENDER_STENCIL_OPERATION_ZERO:
        return VK_STENCIL_OP_ZERO;

    case KAN_RENDER_STENCIL_OPERATION_REPLACE:
        return VK_STENCIL_OP_REPLACE;

    case KAN_RENDER_STENCIL_OPERATION_INCREMENT_AND_CLAMP:
        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;

    case KAN_RENDER_STENCIL_OPERATION_DECREMENT_AND_CLAMP:
        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;

    case KAN_RENDER_STENCIL_OPERATION_INVERT:
        return VK_STENCIL_OP_INVERT;

    case KAN_RENDER_STENCIL_OPERATION_INCREMENT_AND_WRAP:
        return VK_STENCIL_OP_INCREMENT_AND_WRAP;

    case KAN_RENDER_STENCIL_OPERATION_DECREMENT_AND_WRAP:
        return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }

    KAN_ASSERT (KAN_FALSE)
    return VK_STENCIL_OP_KEEP;
}

static inline kan_bool_t add_graphics_request_unsafe (struct render_backend_pipeline_compiler_state_t *state,
                                                      struct graphics_pipeline_compilation_request_t *request)
{
    switch (request->pipeline->compilation_priority)
    {
    case KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CRITICAL:
        kan_bd_list_add (&state->graphics_critical, NULL, &request->list_node);
        return state->graphics_critical.size == 1u;

    case KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_ACTIVE:
        kan_bd_list_add (&state->graphics_active, NULL, &request->list_node);
        return state->graphics_active.size == 1u;

    case KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CACHE:
        kan_bd_list_add (&state->graphics_cache, NULL, &request->list_node);
        return state->graphics_cache.size == 1u;
    }

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

        struct graphics_pipeline_compilation_request_t *request = NULL;
        kan_mutex_lock (state->state_transition_mutex);

        while (KAN_TRUE)
        {
            if (kan_atomic_int_get (&state->should_terminate))
            {
                kan_mutex_unlock (state->state_transition_mutex);
                return 0;
            }

            if (!state->graphics_critical.first && !state->graphics_active.first && !state->graphics_cache.first)
            {
                kan_conditional_variable_wait (state->has_more_work, state->state_transition_mutex);
                continue;
            }

            if (state->graphics_critical.first)
            {
                request = (struct graphics_pipeline_compilation_request_t *) state->graphics_critical.first;
                kan_bd_list_remove (&state->graphics_critical, &request->list_node);
            }
            else if (state->graphics_active.first)
            {
                request = (struct graphics_pipeline_compilation_request_t *) state->graphics_active.first;
                kan_bd_list_remove (&state->graphics_active, &request->list_node);
            }
            else if (state->graphics_cache.first)
            {
                request = (struct graphics_pipeline_compilation_request_t *) state->graphics_cache.first;
                kan_bd_list_remove (&state->graphics_cache, &request->list_node);
            }
            else
            {
                KAN_ASSERT (KAN_FALSE)
            }

            break;
        }

        request->pipeline->compilation_state = PIPELINE_COMPILATION_STATE_EXECUTION;
        kan_mutex_unlock (state->state_transition_mutex);

        VkPipelineVertexInputStateCreateInfo vertex_input_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .vertexBindingDescriptionCount = (vulkan_size_t) request->input_bindings_count,
            .pVertexBindingDescriptions = request->input_bindings,
            .vertexAttributeDescriptionCount = (vulkan_size_t) request->attributes_count,
            .pVertexAttributeDescriptions = request->attributes,
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

        kan_instance_size_t dynamic_state_count = 2u;
        VkDynamicState dynamic_state_array[3u] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        if (request->depth_stencil.depthBoundsTestEnable)
        {
            dynamic_state_array[dynamic_state_count] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;
            ++dynamic_state_count;
        }

        VkPipelineDynamicStateCreateInfo dynamic_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .dynamicStateCount = dynamic_state_count,
            .pDynamicStates = dynamic_state_array,
        };

        VkGraphicsPipelineCreateInfo pipeline_create_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .stageCount = (vulkan_size_t) request->shader_stages_count,
            .pStages = request->shader_stages,
            .pVertexInputState = &vertex_input_info,
            .pInputAssemblyState = &request->input_assembly,
            .pTessellationState = NULL,
            .pViewportState = &viewport_state,
            .pRasterizationState = &request->rasterization,
            .pMultisampleState = &request->multisampling,
            .pDepthStencilState = &request->depth_stencil,
            .pColorBlendState = &request->color_blending,
            .pDynamicState = &dynamic_state,
            .layout = request->pipeline->layout->layout,
            .renderPass = request->pipeline->pass->pass,
            .subpass = 0u,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1,
        };

        struct kan_cpu_section_execution_t execution;
        kan_cpu_section_execution_init (&execution, request->pipeline->system->section_pipeline_compilation);

        VkPipeline pipeline;
        VkResult result =
            vkCreateGraphicsPipelines (request->pipeline->system->device, VK_NULL_HANDLE, 1u, &pipeline_create_info,
                                       VULKAN_ALLOCATION_CALLBACKS (request->pipeline->system), &pipeline);
        kan_cpu_section_execution_shutdown (&execution);

        if (result != VK_SUCCESS)
        {
            pipeline = VK_NULL_HANDLE;
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to compile pipeline \"%s\".",
                     request->pipeline->tracking_name)
        }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
        if (pipeline != VK_NULL_HANDLE)
        {
            char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
            snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "Pipeline::%s",
                      request->pipeline->tracking_name);

            struct VkDebugUtilsObjectNameInfoEXT object_name = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext = NULL,
                .objectType = VK_OBJECT_TYPE_PIPELINE,
                .objectHandle = CONVERT_HANDLE_FOR_DEBUG pipeline,
                .pObjectName = debug_name,
            };

            vkSetDebugUtilsObjectNameEXT (request->pipeline->system->device, &object_name);
        }
#endif

        kan_mutex_lock (state->state_transition_mutex);
        request->pipeline->pipeline = pipeline;
        request->pipeline->compilation_state =
            result == VK_SUCCESS ? PIPELINE_COMPILATION_STATE_SUCCESS : PIPELINE_COMPILATION_STATE_FAILURE;
        request->pipeline->compilation_request = NULL;

        kan_mutex_unlock (state->state_transition_mutex);
        render_backend_compiler_state_destroy_graphics_request (request);
    }
}

void render_backend_compiler_state_request_graphics (struct render_backend_pipeline_compiler_state_t *state,
                                                     struct render_backend_graphics_pipeline_t *pipeline,
                                                     struct kan_render_graphics_pipeline_description_t *description)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, pipeline->system->section_pipeline_compiler_request);

    struct graphics_pipeline_compilation_request_t *request = kan_allocate_batched (
        pipeline->system->pipeline_wrapper_allocation_group, sizeof (struct graphics_pipeline_compilation_request_t));

    request->pipeline = pipeline;
    pipeline->compilation_request = request;
    pipeline->compilation_state = PIPELINE_COMPILATION_STATE_PENDING;

    request->shader_stages_count = 0u;
    for (kan_loop_size_t module_index = 0u; module_index < description->code_modules_count; ++module_index)
    {
        request->shader_stages_count += description->code_modules[module_index].entry_points_count;
    }

    request->shader_stages =
        kan_allocate_general (pipeline->system->pipeline_wrapper_allocation_group,
                              sizeof (VkPipelineShaderStageCreateInfo) * request->shader_stages_count,
                              _Alignof (VkPipelineShaderStageCreateInfo));

    request->linked_code_modules_count = description->code_modules_count;
    request->linked_code_modules =
        kan_allocate_general (pipeline->system->pipeline_wrapper_allocation_group,
                              sizeof (struct render_backend_code_module_t *) * description->code_modules_count,
                              _Alignof (struct render_backend_code_module_t *));

    VkPipelineShaderStageCreateInfo *output_stage = request->shader_stages;
    for (kan_loop_size_t module_index = 0u; module_index < description->code_modules_count; ++module_index)
    {
        struct render_backend_code_module_t *code_module =
            KAN_HANDLE_GET (description->code_modules[module_index].code_module);
        request->linked_code_modules[module_index] = code_module;
        kan_atomic_int_add (&code_module->links, 1);
        VkShaderModule module = code_module->module;

        for (kan_loop_size_t entry_point_index = 0u;
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
            case KAN_RENDER_STAGE_GRAPHICS_VERTEX:
                output_stage->stage = VK_SHADER_STAGE_VERTEX_BIT;
                break;

            case KAN_RENDER_STAGE_GRAPHICS_FRAGMENT:
                output_stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
            }

            ++output_stage;
        }
    }

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    switch (description->topology)
    {
    case KAN_RENDER_GRAPHICS_TOPOLOGY_TRIANGLE_LIST:
        topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        break;
    }

    request->input_assembly = (VkPipelineInputAssemblyStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .topology = topology,
        .primitiveRestartEnable = VK_FALSE,
    };

    request->input_bindings_count = description->attribute_sources_count;
    request->input_bindings =
        kan_allocate_general (pipeline->system->pipeline_wrapper_allocation_group,
                              sizeof (VkVertexInputBindingDescription) * request->input_bindings_count,
                              _Alignof (VkVertexInputBindingDescription));

    for (kan_loop_size_t index = 0u; index < description->attribute_sources_count; ++index)
    {
        struct kan_render_attribute_source_description_t *input = &description->attribute_sources[index];
        VkVertexInputBindingDescription *output = &request->input_bindings[index];

        output->binding = (vulkan_size_t) input->binding;
        output->stride = (vulkan_size_t) input->stride;

        switch (input->rate)
        {
        case KAN_RENDER_ATTRIBUTE_RATE_PER_VERTEX:
            output->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            break;

        case KAN_RENDER_ATTRIBUTE_RATE_PER_INSTANCE:
            output->inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
            break;
        }
    }

    request->attributes_count = 0u;
    for (kan_loop_size_t index = 0u; index < description->attributes_count; ++index)
    {
        struct kan_render_attribute_description_t *input = &description->attributes[index];
        switch (input->class)
        {
        case KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_1:
        case KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_2:
        case KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_3:
        case KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_4:
            ++request->attributes_count;
            break;

        case KAN_RENDER_ATTRIBUTE_CLASS_MATRIX_3_3:
            request->attributes_count += 3u;
            break;

        case KAN_RENDER_ATTRIBUTE_CLASS_MATRIX_4_4:
            request->attributes_count += 4u;
            break;
        }
    }

    request->attributes = kan_allocate_general (pipeline->system->pipeline_wrapper_allocation_group,
                                                sizeof (VkVertexInputAttributeDescription) * request->attributes_count,
                                                _Alignof (VkVertexInputAttributeDescription));
    VkVertexInputAttributeDescription *attribute_output = request->attributes;

    for (kan_loop_size_t index = 0u; index < description->attributes_count; ++index)
    {
        struct kan_render_attribute_description_t *input = &description->attributes[index];
        VkFormat input_format = VK_FORMAT_R32_SFLOAT;
        kan_instance_size_t attribute_count = 1u;
        kan_instance_size_t items_count_in_attribute = 1u;
        vulkan_size_t item_offset = sizeof (float);

        switch (input->class)
        {
        case KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_1:
            items_count_in_attribute = 1u;
            attribute_count = 1u;
            break;

        case KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_2:
            items_count_in_attribute = 2u;
            attribute_count = 1u;
            break;

        case KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_3:
            items_count_in_attribute = 3u;
            attribute_count = 1u;
            break;

        case KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_4:
            items_count_in_attribute = 4u;
            attribute_count = 1u;
            break;

        case KAN_RENDER_ATTRIBUTE_CLASS_MATRIX_3_3:
            items_count_in_attribute = 3u;
            attribute_count = 3u;
            break;

        case KAN_RENDER_ATTRIBUTE_CLASS_MATRIX_4_4:
            items_count_in_attribute = 4u;
            attribute_count = 4u;
            break;
        }

        switch (input->item_format)
        {
        case KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_16:
            item_offset = sizeof (uint16_t);

            switch (items_count_in_attribute)
            {
            case 1u:
                input_format = VK_FORMAT_R16_SFLOAT;
                break;

            case 2u:
                input_format = VK_FORMAT_R16G16_SFLOAT;
                break;

            case 3u:
                input_format = VK_FORMAT_R16G16B16_SFLOAT;
                break;

            case 4u:
                input_format = VK_FORMAT_R16G16B16A16_SFLOAT;
                break;
            }

            break;

        case KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_32:
            item_offset = sizeof (float);

            switch (items_count_in_attribute)
            {
            case 1u:
                input_format = VK_FORMAT_R32_SFLOAT;
                break;

            case 2u:
                input_format = VK_FORMAT_R32G32_SFLOAT;
                break;

            case 3u:
                input_format = VK_FORMAT_R32G32B32_SFLOAT;
                break;

            case 4u:
                input_format = VK_FORMAT_R32G32B32A32_SFLOAT;
                break;
            }

            break;

        case KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_UNORM_8:
            item_offset = sizeof (uint8_t);

            switch (items_count_in_attribute)
            {
            case 1u:
                input_format = VK_FORMAT_R8_UNORM;
                break;

            case 2u:
                input_format = VK_FORMAT_R8G8_UNORM;
                break;

            case 3u:
                input_format = VK_FORMAT_R8G8B8_UNORM;
                break;

            case 4u:
                input_format = VK_FORMAT_R8G8B8A8_UNORM;
                break;
            }

            break;

        case KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_UNORM_16:
            item_offset = sizeof (uint16_t);

            switch (items_count_in_attribute)
            {
            case 1u:
                input_format = VK_FORMAT_R16_UNORM;
                break;

            case 2u:
                input_format = VK_FORMAT_R16G16_UNORM;
                break;

            case 3u:
                input_format = VK_FORMAT_R16G16B16_UNORM;
                break;

            case 4u:
                input_format = VK_FORMAT_R16G16B16A16_UNORM;
                break;
            }

            break;

        case KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_SNORM_8:
            item_offset = sizeof (uint8_t);

            switch (items_count_in_attribute)
            {
            case 1u:
                input_format = VK_FORMAT_R8_SNORM;
                break;

            case 2u:
                input_format = VK_FORMAT_R8G8_SNORM;
                break;

            case 3u:
                input_format = VK_FORMAT_R8G8B8_SNORM;
                break;

            case 4u:
                input_format = VK_FORMAT_R8G8B8A8_SNORM;
                break;
            }

            break;

        case KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_SNORM_16:
            item_offset = sizeof (uint16_t);

            switch (items_count_in_attribute)
            {
            case 1u:
                input_format = VK_FORMAT_R16_SNORM;
                break;

            case 2u:
                input_format = VK_FORMAT_R16G16_SNORM;
                break;

            case 3u:
                input_format = VK_FORMAT_R16G16B16_SNORM;
                break;

            case 4u:
                input_format = VK_FORMAT_R16G16B16A16_SNORM;
                break;
            }

            break;

        case KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_UINT_8:
            item_offset = sizeof (uint8_t);

            switch (items_count_in_attribute)
            {
            case 1u:
                input_format = VK_FORMAT_R8_UINT;
                break;

            case 2u:
                input_format = VK_FORMAT_R8G8_UINT;
                break;

            case 3u:
                input_format = VK_FORMAT_R8G8B8_UINT;
                break;

            case 4u:
                input_format = VK_FORMAT_R8G8B8A8_UINT;
                break;
            }

            break;

        case KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_UINT_16:
            item_offset = sizeof (uint16_t);

            switch (items_count_in_attribute)
            {
            case 1u:
                input_format = VK_FORMAT_R16_UINT;
                break;

            case 2u:
                input_format = VK_FORMAT_R16G16_UINT;
                break;

            case 3u:
                input_format = VK_FORMAT_R16G16B16_UINT;
                break;

            case 4u:
                input_format = VK_FORMAT_R16G16B16A16_UINT;
                break;
            }

            break;

        case KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_UINT_32:
            item_offset = sizeof (uint32_t);

            switch (items_count_in_attribute)
            {
            case 1u:
                input_format = VK_FORMAT_R32_UINT;
                break;

            case 2u:
                input_format = VK_FORMAT_R32G32_UINT;
                break;

            case 3u:
                input_format = VK_FORMAT_R32G32B32_UINT;
                break;

            case 4u:
                input_format = VK_FORMAT_R32G32B32A32_UINT;
                break;
            }

            break;

        case KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_SINT_8:
            item_offset = sizeof (uint8_t);

            switch (items_count_in_attribute)
            {
            case 1u:
                input_format = VK_FORMAT_R8_SINT;
                break;

            case 2u:
                input_format = VK_FORMAT_R8G8_SINT;
                break;

            case 3u:
                input_format = VK_FORMAT_R8G8B8_SINT;
                break;

            case 4u:
                input_format = VK_FORMAT_R8G8B8A8_SINT;
                break;
            }

            break;

        case KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_SINT_16:
            item_offset = sizeof (uint16_t);

            switch (items_count_in_attribute)
            {
            case 1u:
                input_format = VK_FORMAT_R16_SINT;
                break;

            case 2u:
                input_format = VK_FORMAT_R16G16_SINT;
                break;

            case 3u:
                input_format = VK_FORMAT_R16G16B16_SINT;
                break;

            case 4u:
                input_format = VK_FORMAT_R16G16B16A16_SINT;
                break;
            }

            break;

        case KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_SINT_32:
            item_offset = sizeof (uint32_t);

            switch (items_count_in_attribute)
            {
            case 1u:
                input_format = VK_FORMAT_R32_SINT;
                break;

            case 2u:
                input_format = VK_FORMAT_R32G32_SINT;
                break;

            case 3u:
                input_format = VK_FORMAT_R32G32B32_SINT;
                break;

            case 4u:
                input_format = VK_FORMAT_R32G32B32A32_SINT;
                break;
            }

            break;
        }

        for (vulkan_size_t attribute_index = 0u; attribute_index < attribute_count; ++attribute_index)
        {
            attribute_output->binding = (vulkan_size_t) input->binding;
            attribute_output->location = (vulkan_size_t) input->location + attribute_index;
            attribute_output->offset =
                (vulkan_size_t) input->offset + item_offset * items_count_in_attribute * attribute_index;
            attribute_output->format = input_format;
            ++attribute_output;
        }
    }

    VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
    switch (description->polygon_mode)
    {
    case KAN_RENDER_POLYGON_MODE_FILL:
        polygon_mode = VK_POLYGON_MODE_FILL;
        break;

    case KAN_RENDER_POLYGON_MODE_WIREFRAME:
        polygon_mode = VK_POLYGON_MODE_LINE;
        break;
    }

    VkCullModeFlags cull_mode = VK_CULL_MODE_NONE;
    switch (description->cull_mode)
    {
    case KAN_RENDER_CULL_MODE_NONE:
        cull_mode = VK_CULL_MODE_NONE;
        break;

    case KAN_RENDER_CULL_MODE_BACK:
        cull_mode = VK_CULL_MODE_BACK_BIT;
        break;

    case KAN_RENDER_CULL_MODE_FRONT:
        cull_mode = VK_CULL_MODE_FRONT_BIT;
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

    request->depth_stencil = (VkPipelineDepthStencilStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .depthTestEnable = description->depth_test_enabled,
        .depthWriteEnable = description->depth_write_enabled,
        .depthCompareOp = to_vulkan_compare_operation (description->depth_compare_operation),
        .depthBoundsTestEnable = description->depth_bounds_test_enabled,
        .stencilTestEnable = description->stencil_test_enabled,
        // Stencil test disabled, therefore we don't care about front and back at all.
        .front =
            {
                .failOp = to_vulkan_stencil_operation (description->stencil_front.on_fail),
                .passOp = to_vulkan_stencil_operation (description->stencil_front.on_pass),
                .depthFailOp = to_vulkan_stencil_operation (description->stencil_front.on_depth_fail),
                .compareOp = to_vulkan_compare_operation (description->stencil_front.compare),
                .compareMask = description->stencil_front.compare_mask,
                .writeMask = description->stencil_front.write_mask,
                .reference = description->stencil_front.reference,
            },
        .back =
            {
                .failOp = to_vulkan_stencil_operation (description->stencil_back.on_fail),
                .passOp = to_vulkan_stencil_operation (description->stencil_back.on_pass),
                .depthFailOp = to_vulkan_stencil_operation (description->stencil_back.on_depth_fail),
                .compareOp = to_vulkan_compare_operation (description->stencil_back.compare),
                .compareMask = description->stencil_back.compare_mask,
                .writeMask = description->stencil_back.write_mask,
                .reference = description->stencil_back.reference,
            },
        .minDepthBounds = description->min_depth,
        .maxDepthBounds = description->max_depth,
    };

    request->color_blending_attachments_count = description->output_setups_count;
    request->color_blending_attachments =
        kan_allocate_general (pipeline->system->pipeline_wrapper_allocation_group,
                              sizeof (VkPipelineColorBlendAttachmentState) * request->color_blending_attachments_count,
                              _Alignof (VkPipelineColorBlendAttachmentState));

    for (kan_loop_size_t attachment_index = 0u; attachment_index < request->color_blending_attachments_count;
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
        .attachmentCount = (vulkan_size_t) request->color_blending_attachments_count,
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
    kan_bool_t first_in_the_queue = add_graphics_request_unsafe (state, request);
    kan_mutex_unlock (state->state_transition_mutex);

    if (first_in_the_queue)
    {
        kan_conditional_variable_signal_one (state->has_more_work);
    }

    kan_cpu_section_execution_shutdown (&execution);
}

void render_backend_compiler_state_destroy_graphics_request (struct graphics_pipeline_compilation_request_t *request)
{
    for (kan_loop_size_t module_index = 0u; module_index < request->linked_code_modules_count; ++module_index)
    {
        render_backend_system_unlink_code_module (request->linked_code_modules[module_index]);
    }

    kan_allocation_group_t allocation_group = request->pipeline->system->pipeline_wrapper_allocation_group;
    kan_free_general (allocation_group, request->shader_stages,
                      sizeof (VkPipelineShaderStageCreateInfo) * request->shader_stages_count);
    kan_free_general (allocation_group, request->input_bindings,
                      sizeof (VkVertexInputBindingDescription) * request->input_bindings_count);
    kan_free_general (allocation_group, request->attributes,
                      sizeof (VkVertexInputAttributeDescription) * request->attributes_count);
    kan_free_general (allocation_group, request->color_blending_attachments,
                      sizeof (VkPipelineColorBlendAttachmentState) * request->color_blending_attachments_count);
    kan_free_general (allocation_group, request->linked_code_modules,
                      sizeof (struct render_backend_code_module_t *) * request->linked_code_modules_count);
    kan_free_batched (allocation_group, request);
}

struct render_backend_graphics_pipeline_t *render_backend_system_create_graphics_pipeline (
    struct render_backend_system_t *system,
    struct kan_render_graphics_pipeline_description_t *description,
    enum kan_render_pipeline_compilation_priority_t compilation_priority)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_graphics_pipeline_internal);

    struct render_backend_graphics_pipeline_t *pipeline = kan_allocate_batched (
        system->pipeline_wrapper_allocation_group, sizeof (struct render_backend_graphics_pipeline_t));

    kan_atomic_int_lock (&system->resource_registration_lock);
    kan_bd_list_add (&system->graphics_pipelines, NULL, &pipeline->list_node);
    kan_atomic_int_unlock (&system->resource_registration_lock);
    pipeline->system = system;

    pipeline->pipeline = VK_NULL_HANDLE;
    pipeline->layout = render_backend_system_register_pipeline_layout (
        system, description->push_constant_size, description->parameter_set_layouts_count,
        description->parameter_set_layouts, description->tracking_name);
    pipeline->pass = KAN_HANDLE_GET (description->pass);

    pipeline->min_depth = description->min_depth;
    pipeline->max_depth = description->max_depth;

    // Because creation is done under resource management lock and compilation request does not require it,
    // we schedule compilation separately. Therefore, initially pipeline is in compilation failed state.
    pipeline->compilation_priority = compilation_priority;
    pipeline->compilation_state = PIPELINE_COMPILATION_STATE_FAILURE;
    pipeline->compilation_request = NULL;

    pipeline->tracking_name = description->tracking_name;
    kan_cpu_section_execution_shutdown (&execution);
    return pipeline;
}

void render_backend_system_destroy_graphics_pipeline (struct render_backend_system_t *system,
                                                      struct render_backend_graphics_pipeline_t *pipeline)
{
    // Compilation request should be dealt with properly before destroying pipeline.
    KAN_ASSERT (!pipeline->compilation_request)

    if (pipeline->pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline (system->device, pipeline->pipeline, VULKAN_ALLOCATION_CALLBACKS (system));
    }

    kan_free_batched (system->pipeline_wrapper_allocation_group, pipeline);
}

kan_render_graphics_pipeline_t kan_render_graphics_pipeline_create (
    kan_render_context_t context,
    struct kan_render_graphics_pipeline_description_t *description,
    enum kan_render_pipeline_compilation_priority_t compilation_priority)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (context);
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_graphics_pipeline);

    struct render_backend_graphics_pipeline_t *pipeline =
        render_backend_system_create_graphics_pipeline (system, description, compilation_priority);
    render_backend_compiler_state_request_graphics (&system->compiler_state, pipeline, description);
    kan_cpu_section_execution_shutdown (&execution);
    return pipeline ? KAN_HANDLE_SET (kan_render_graphics_pipeline_t, pipeline) :
                      KAN_HANDLE_SET_INVALID (kan_render_graphics_pipeline_t);
}

void kan_render_graphics_pipeline_change_compilation_priority (
    kan_render_graphics_pipeline_t pipeline, enum kan_render_pipeline_compilation_priority_t compilation_priority)
{
    struct render_backend_graphics_pipeline_t *data = KAN_HANDLE_GET (pipeline);

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

    render_backend_pipeline_compiler_state_remove_graphics_request_unsafe (&data->system->compiler_state,
                                                                           data->compilation_request);
    data->compilation_priority = compilation_priority;
    add_graphics_request_unsafe (&data->system->compiler_state, data->compilation_request);
    kan_mutex_unlock (data->system->compiler_state.state_transition_mutex);
}

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_graphics_pipeline_destroy (kan_render_graphics_pipeline_t pipeline)
{
    struct render_backend_graphics_pipeline_t *data = KAN_HANDLE_GET (pipeline);
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_graphics_pipeline_destroy_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &schedule->item_allocator, struct scheduled_graphics_pipeline_destroy_t);

    item->next = schedule->first_scheduled_graphics_pipeline_destroy;
    schedule->first_scheduled_graphics_pipeline_destroy = item;
    item->pipeline = data;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}
