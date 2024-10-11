#include <kan/context/render_backend_implementation_interface.h>

static inline void free_descriptor_set_layouts (struct render_backend_system_t *system,
                                                uint64_t descriptor_set_layouts_count,
                                                struct render_backend_descriptor_set_layout_t **descriptor_set_layouts)
{
    for (uint64_t index = 0u; index < descriptor_set_layouts_count; ++index)
    {
        if (descriptor_set_layouts[index] != NULL)
        {
            if (descriptor_set_layouts[index]->layout != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout (system->device, descriptor_set_layouts[index]->layout,
                                              VULKAN_ALLOCATION_CALLBACKS (system));
            }

            kan_free_general (
                system->pipeline_family_wrapper_allocation_group, descriptor_set_layouts[index],
                sizeof (struct render_backend_descriptor_set_layout_t) +
                    sizeof (struct render_backend_layout_binding_t) * descriptor_set_layouts[index]->bindings_count);
        }
    }

    kan_free_general (system->pipeline_family_wrapper_allocation_group, descriptor_set_layouts,
                      sizeof (struct render_backend_descriptor_set_layout_t *) * descriptor_set_layouts_count);
}

struct render_backend_classic_graphics_pipeline_family_t *
render_backend_system_create_classic_graphics_pipeline_family (
    struct render_backend_system_t *system,
    struct kan_render_classic_graphics_pipeline_family_description_t *description)
{
    kan_bool_t descriptor_sets_are_zero_sequential = KAN_TRUE;
    uint64_t sets_count = 0u;

    for (uint64_t index = 0u; index < description->layouts_count; ++index)
    {
        if (description->layouts[index].set != index)
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Pipeline family \"%s\" layout set indices do not form zero-based sequence with step equal to "
                     "one. It makes bind less efficient.",
                     description->tracking_name)
            descriptor_sets_are_zero_sequential = KAN_FALSE;
            break;
        }

        sets_count = KAN_MAX (sets_count, description->layouts[index].set + 1u);
    }

    if (sets_count >= KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_SET_INDEX)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Pipeline family \"%s\" last layout set index %lu is higher than maximum allowed %lu.",
                 description->tracking_name, (unsigned long) (sets_count - 1u),
                 (unsigned long) KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_SET_INDEX)
        return NULL;
    }

    VkPipelineLayout pipeline_layout;
    struct render_backend_descriptor_set_layout_t **descriptor_set_layouts =
        kan_allocate_general (system->pipeline_family_wrapper_allocation_group,
                              sizeof (struct render_backend_descriptor_set_layout_t *) * sets_count,
                              _Alignof (struct render_backend_descriptor_set_layout_t *));

    for (uint64_t index = 0u; index < sets_count; ++index)
    {
        descriptor_set_layouts[index] = NULL;
    }

    kan_bool_t descriptor_set_layouts_created = KAN_TRUE;
    uint64_t bindings_size = 0u;
    VkDescriptorSetLayoutBinding *bindings = NULL;

    VkDescriptorSetLayout *descriptor_set_layouts_for_pipeline = kan_allocate_general (
        system->utility_allocation_group, sizeof (VkDescriptorSetLayout) * description->layouts_count,
        _Alignof (VkDescriptorSetLayout));

    for (uint64_t layout_index = 0u; layout_index < description->layouts_count; ++layout_index)
    {
        struct kan_render_layout_description_t *layout_description = &description->layouts[layout_index];
        if (descriptor_set_layouts[layout_description->set])
        {
            KAN_LOG (
                render_backend_system_vulkan, KAN_LOG_ERROR,
                "Failed to create descriptor set for layout %lu for pipeline family \"%s\" as set %lu is already used.",
                (unsigned long) layout_index, description->tracking_name, (unsigned long) layout_description->set)
            descriptor_set_layouts_created = KAN_FALSE;
            break;
        }

        if (bindings_size < layout_description->bindings_count)
        {
            if (bindings)
            {
                kan_free_general (system->utility_allocation_group, bindings,
                                  sizeof (VkDescriptorSetLayoutBinding) * bindings_size);
            }

            bindings_size = layout_description->bindings_count;
            bindings = kan_allocate_general (system->utility_allocation_group,
                                             sizeof (VkDescriptorSetLayoutBinding) * bindings_size,
                                             _Alignof (VkDescriptorSetLayoutBinding));
        }

        uint64_t bindings_count = 0u;
        for (uint64_t binding_index = 0u; binding_index < layout_description->bindings_count; ++binding_index)
        {
            struct kan_render_layout_binding_description_t *binding_description =
                &layout_description->bindings[binding_index];
            VkDescriptorSetLayoutBinding *vulkan_binding = &bindings[binding_index];

            vulkan_binding->binding = (uint32_t) binding_description->binding;
            vulkan_binding->descriptorCount = 1u;
            vulkan_binding->stageFlags = 0u;
            vulkan_binding->pImmutableSamplers = NULL;

            switch (binding_description->type)
            {
            case KAN_RENDER_LAYOUT_BINDING_TYPE_UNIFORM_BUFFER:
                vulkan_binding->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                break;

            case KAN_RENDER_LAYOUT_BINDING_TYPE_STORAGE_BUFFER:
                vulkan_binding->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                break;

            case KAN_RENDER_LAYOUT_BINDING_TYPE_COMBINED_IMAGE_SAMPLER:
                vulkan_binding->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                break;
            }

            if (binding_description->used_stage_mask & (1u << KAN_RENDER_STAGE_CLASSIC_GRAPHICS_VERTEX))
            {
                vulkan_binding->stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
            }

            if (binding_description->used_stage_mask & (1u << KAN_RENDER_STAGE_CLASSIC_GRAPHICS_FRAGMENT))
            {
                vulkan_binding->stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            bindings_count = KAN_MAX (bindings_count, binding_description->binding + 1u);
        }

        if (bindings_count >= KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_BINDING_INDEX)
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Pipeline family \"%s\" descriptor set layout %lu last binding index %lu is higher than maximum "
                     "allowed %lu.",
                     description->tracking_name, (unsigned long) layout_index, (unsigned long) (sets_count - 1u),
                     (unsigned long) KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_BINDING_INDEX)
            descriptor_set_layouts_created = KAN_FALSE;
            break;
        }

        VkDescriptorSetLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .bindingCount = (uint32_t) layout_description->bindings_count,
            .pBindings = bindings,
        };

        if (vkCreateDescriptorSetLayout (system->device, &layout_info, VULKAN_ALLOCATION_CALLBACKS (system),
                                         &descriptor_set_layouts_for_pipeline[layout_index]) != VK_SUCCESS)
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Failed to create descriptor set for layout %lu for pipeline family \"%s\".",
                     (unsigned long) layout_index, description->tracking_name)
            descriptor_set_layouts_created = KAN_FALSE;
            break;
        }

        struct render_backend_descriptor_set_layout_t *layout =
            kan_allocate_general (system->pipeline_family_wrapper_allocation_group,
                                  sizeof (struct render_backend_descriptor_set_layout_t) +
                                      sizeof (struct render_backend_layout_binding_t) * bindings_count,
                                  _Alignof (struct render_backend_descriptor_set_layout_t));

        layout->layout = descriptor_set_layouts_for_pipeline[layout_index];
        layout->stable_binding = layout_description->stable_binding;
        layout->bindings_count = bindings_count;

        for (uint64_t binding = 0u; binding < bindings_count; ++binding)
        {
            layout->bindings[binding].type = KAN_RENDER_LAYOUT_BINDING_TYPE_UNIFORM_BUFFER;
            // Bindings with zero used stage mask are treated as non-existent.
            layout->bindings[binding].used_stage_mask = 0u;
        }

        for (uint64_t binding_index = 0u; binding_index < layout_description->bindings_count; ++binding_index)
        {
            struct kan_render_layout_binding_description_t *binding_description =
                &layout_description->bindings[binding_index];
            layout->bindings[binding_description->binding].type = binding_description->type;
            layout->bindings[binding_description->binding].used_stage_mask = binding_description->used_stage_mask;
        }
    }

    if (bindings)
    {
        kan_free_general (system->utility_allocation_group, bindings,
                          sizeof (VkDescriptorSetLayoutBinding) * bindings_size);
    }

    if (!descriptor_set_layouts_created)
    {
        free_descriptor_set_layouts (system, sets_count, descriptor_set_layouts);
        kan_free_general (system->utility_allocation_group, descriptor_set_layouts_for_pipeline,
                          sizeof (VkDescriptorSetLayout) * description->layouts_count);
        return NULL;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .setLayoutCount = (uint32_t) description->layouts_count,
        .pSetLayouts = descriptor_set_layouts_for_pipeline,
        .pushConstantRangeCount = 0u,
        .pPushConstantRanges = NULL,
    };

    VkResult result = vkCreatePipelineLayout (system->device, &pipeline_layout_info,
                                              VULKAN_ALLOCATION_CALLBACKS (system), &pipeline_layout);
    kan_free_general (system->utility_allocation_group, descriptor_set_layouts_for_pipeline,
                      sizeof (VkDescriptorSetLayout) * description->layouts_count);

    if (result != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to pipeline layout for pipeline family \"%s\".",
                 description->tracking_name)
        free_descriptor_set_layouts (system, sets_count, descriptor_set_layouts);
        return NULL;
    }

    struct render_backend_classic_graphics_pipeline_family_t *family =
        kan_allocate_batched (system->pipeline_family_wrapper_allocation_group,
                              sizeof (struct render_backend_classic_graphics_pipeline_family_t));

    kan_bd_list_add (&system->classic_graphics_pipeline_families, NULL, &family->list_node);
    family->system = system;

    family->layout = pipeline_layout;
    family->descriptor_set_layouts_count = sets_count;
    family->descriptor_set_layouts = descriptor_set_layouts;
    family->descriptor_sets_are_zero_sequential = descriptor_sets_are_zero_sequential;

    family->topology = description->topology;
    family->tracking_name = description->tracking_name;

    family->input_bindings_count = description->attribute_sources_count;
    family->input_bindings =
        kan_allocate_general (system->pipeline_family_wrapper_allocation_group,
                              sizeof (VkVertexInputBindingDescription) * family->input_bindings_count,
                              _Alignof (VkVertexInputBindingDescription));

    for (uint64_t index = 0u; index < description->attribute_sources_count; ++index)
    {
        struct kan_render_attribute_source_description_t *input = &description->attribute_sources[index];
        VkVertexInputBindingDescription *output = &family->input_bindings[index];

        output->binding = (uint32_t) input->binding;
        output->stride = (uint32_t) input->stride;

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

    family->attributes_count = 0u;
    for (uint64_t index = 0u; index < description->attributes_count; ++index)
    {
        struct kan_render_attribute_description_t *input = &description->attributes[index];
        switch (input->format)
        {
        case KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_1:
        case KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_2:
        case KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_3:
        case KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_4:
            ++family->attributes_count;
            break;

        case KAN_RENDER_ATTRIBUTE_FORMAT_MATRIX_FLOAT_3_3:
            family->attributes_count += 3u;
            break;

        case KAN_RENDER_ATTRIBUTE_FORMAT_MATRIX_FLOAT_4_4:
            family->attributes_count += 4u;
            break;
        }
    }

    family->attributes = kan_allocate_general (system->utility_allocation_group,
                                               sizeof (VkVertexInputAttributeDescription) * family->attributes_count,
                                               _Alignof (VkVertexInputAttributeDescription));
    VkVertexInputAttributeDescription *attribute_output = family->attributes;

    for (uint64_t index = 0u; index < description->attributes_count; ++index)
    {
        struct kan_render_attribute_description_t *input = &description->attributes[index];
        VkFormat input_format;
        uint32_t attribute_count;
        uint32_t item_offset;

        switch (input->format)
        {
        case KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_1:
            input_format = VK_FORMAT_R32_SFLOAT;
            attribute_count = 1u;
            item_offset = sizeof (float);
            break;

        case KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_2:
            input_format = VK_FORMAT_R32G32_SFLOAT;
            attribute_count = 1u;
            item_offset = sizeof (float) * 2u;
            break;

        case KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_3:
            input_format = VK_FORMAT_R32G32B32_SFLOAT;
            attribute_count = 1u;
            item_offset = sizeof (float) * 3u;
            break;

        case KAN_RENDER_ATTRIBUTE_FORMAT_VECTOR_FLOAT_4:
            input_format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attribute_count = 1u;
            item_offset = sizeof (float) * 4u;
            break;

        case KAN_RENDER_ATTRIBUTE_FORMAT_MATRIX_FLOAT_3_3:
            input_format = VK_FORMAT_R32G32B32_SFLOAT;
            attribute_count = 3u;
            item_offset = sizeof (float) * 3u;
            break;

        case KAN_RENDER_ATTRIBUTE_FORMAT_MATRIX_FLOAT_4_4:
            input_format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attribute_count = 4u;
            item_offset = sizeof (float) * 4u;
            break;
        }

        for (uint32_t attribute_index = 0u; attribute_index < attribute_count; ++attribute_index)
        {
            attribute_output->binding = (uint32_t) input->binding;
            attribute_output->location = (uint32_t) input->location;
            attribute_output->offset = (uint32_t) input->offset + item_offset * attribute_index;
            attribute_output->format = input_format;
            ++attribute_output;
        }
    }

    return family;
}

void render_backend_system_destroy_classic_graphics_pipeline_family (
    struct render_backend_system_t *system,
    struct render_backend_classic_graphics_pipeline_family_t *family)
{
    vkDestroyPipelineLayout (system->device, family->layout, VULKAN_ALLOCATION_CALLBACKS (system));
    free_descriptor_set_layouts (system, family->descriptor_set_layouts_count, family->descriptor_set_layouts);

    kan_free_general (system->pipeline_family_wrapper_allocation_group, family->input_bindings,
                      sizeof (VkVertexInputBindingDescription) * family->input_bindings_count);
    kan_free_general (system->pipeline_family_wrapper_allocation_group, family->attributes,
                      sizeof (VkVertexInputAttributeDescription) * family->attributes_count);
}

kan_render_classic_graphics_pipeline_family_t kan_render_classic_graphics_pipeline_family_create (
    kan_render_context_t context, struct kan_render_classic_graphics_pipeline_family_description_t *description)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) context;
    kan_atomic_int_lock (&system->resource_management_lock);
    struct render_backend_classic_graphics_pipeline_family_t *family =
        render_backend_system_create_classic_graphics_pipeline_family (system, description);
    kan_atomic_int_unlock (&system->resource_management_lock);
    return family ? (kan_render_classic_graphics_pipeline_family_t) family :
                    KAN_INVALID_RENDER_CLASSIC_GRAPHICS_PIPELINE_FAMILY;
}

void kan_render_classic_graphics_pipeline_family_destroy (kan_render_classic_graphics_pipeline_family_t family)
{
    struct render_backend_classic_graphics_pipeline_family_t *data =
        (struct render_backend_classic_graphics_pipeline_family_t *) family;
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_classic_graphics_pipeline_family_destroy_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &schedule->item_allocator, struct scheduled_classic_graphics_pipeline_family_destroy_t);

    item->next = schedule->first_scheduled_classic_graphics_pipeline_family_destroy;
    schedule->first_scheduled_classic_graphics_pipeline_family_destroy = item;
    item->family = data;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}
