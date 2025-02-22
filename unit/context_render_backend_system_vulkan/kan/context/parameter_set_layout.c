#include <kan/context/render_backend_implementation_interface.h>

struct render_backend_pipeline_parameter_set_layout_t *render_backend_system_create_pipeline_parameter_set_layout (
    struct render_backend_system_t *system, struct kan_render_pipeline_parameter_set_layout_description_t *description)
{
    VkDescriptorSetLayoutBinding bindings_static[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_DESCS];
    VkDescriptorSetLayoutBinding *bindings = bindings_static;

    if (description->bindings_count > KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_DESCS)
    {
        bindings = kan_allocate_general (system->utility_allocation_group,
                                         sizeof (VkDescriptorSetLayoutBinding) * description->bindings_count,
                                         _Alignof (VkDescriptorSetLayoutBinding));
    }

    vulkan_size_t used_binding_index_count = 0u;
    for (kan_loop_size_t binding_index = 0u; binding_index < description->bindings_count; ++binding_index)
    {
        struct kan_render_parameter_binding_description_t *binding_description = &description->bindings[binding_index];
        VkDescriptorSetLayoutBinding *vulkan_binding = &bindings[binding_index];

        vulkan_binding->binding = (vulkan_size_t) binding_description->binding;
        vulkan_binding->descriptorCount = 1u;
        vulkan_binding->stageFlags = 0u;
        vulkan_binding->pImmutableSamplers = NULL;

        switch (binding_description->type)
        {
        case KAN_RENDER_PARAMETER_BINDING_TYPE_UNIFORM_BUFFER:
            vulkan_binding->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            break;

        case KAN_RENDER_PARAMETER_BINDING_TYPE_STORAGE_BUFFER:
            vulkan_binding->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            break;

        case KAN_RENDER_PARAMETER_BINDING_TYPE_COMBINED_IMAGE_SAMPLER:
            vulkan_binding->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            break;
        }

        if (binding_description->used_stage_mask & (1u << KAN_RENDER_STAGE_GRAPHICS_VERTEX))
        {
            vulkan_binding->stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
        }

        if (binding_description->used_stage_mask & (1u << KAN_RENDER_STAGE_GRAPHICS_FRAGMENT))
        {
            vulkan_binding->stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        used_binding_index_count = KAN_MAX (used_binding_index_count, binding_description->binding + 1u);
    }

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .bindingCount = (vulkan_size_t) description->bindings_count,
        .pBindings = bindings,
    };

    VkDescriptorSetLayout vulkan_layout = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorSetLayout (system->device, &layout_info, VULKAN_ALLOCATION_CALLBACKS (system),
                                                   &vulkan_layout);

    if (bindings != bindings_static)
    {
        kan_free_general (system->utility_allocation_group, bindings,
                          sizeof (VkDescriptorSetLayoutBinding) * description->bindings_count);
    }

    if (result != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to create descriptor set for layout \"%s\".",
                 description->tracking_name)
        return NULL;
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    {
        char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
        snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "DescriptorSetLayout::%s",
                  description->tracking_name);

        struct VkDebugUtilsObjectNameInfoEXT object_name = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = NULL,
            .objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
            .objectHandle = CONVERT_HANDLE_FOR_DEBUG vulkan_layout,
            .pObjectName = debug_name,
        };

        vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
    }
#endif

    struct render_backend_pipeline_parameter_set_layout_t *layout =
        kan_allocate_general (system->parameter_set_layout_wrapper_allocation_group,
                              sizeof (struct render_backend_pipeline_parameter_set_layout_t) +
                                  sizeof (struct render_backend_layout_binding_t) * used_binding_index_count,
                              _Alignof (struct render_backend_pipeline_parameter_set_layout_t));

    kan_atomic_int_lock (&system->resource_registration_lock);
    kan_bd_list_add (&system->pipeline_parameter_set_layouts, NULL, &layout->list_node);
    kan_atomic_int_unlock (&system->resource_registration_lock);
    layout->system = system;

    layout->layout = vulkan_layout;
    layout->set = description->set;

    layout->stable_binding = description->stable_binding;
    layout->bindings_count = used_binding_index_count;
    layout->uniform_buffers_count = 0u;
    layout->storage_buffers_count = 0u;
    layout->combined_image_samplers_count = 0u;
    layout->tracking_name = description->tracking_name;

    for (vulkan_size_t binding = 0u; binding < used_binding_index_count; ++binding)
    {
        layout->bindings[binding].type = KAN_RENDER_PARAMETER_BINDING_TYPE_UNIFORM_BUFFER;
        // Bindings with zero used stage mask are treated as non-existent.
        layout->bindings[binding].used_stage_mask = 0u;
    }

    for (kan_loop_size_t binding_index = 0u; binding_index < description->bindings_count; ++binding_index)
    {
        struct kan_render_parameter_binding_description_t *binding_description = &description->bindings[binding_index];

        layout->bindings[binding_description->binding].type = binding_description->type;
        layout->bindings[binding_description->binding].used_stage_mask = binding_description->used_stage_mask;

        switch (binding_description->type)
        {
        case KAN_RENDER_PARAMETER_BINDING_TYPE_UNIFORM_BUFFER:
            KAN_ASSERT (layout->uniform_buffers_count < UINT8_MAX)
            ++layout->uniform_buffers_count;
            break;

        case KAN_RENDER_PARAMETER_BINDING_TYPE_STORAGE_BUFFER:
            KAN_ASSERT (layout->storage_buffers_count < UINT8_MAX)
            ++layout->storage_buffers_count;
            break;

        case KAN_RENDER_PARAMETER_BINDING_TYPE_COMBINED_IMAGE_SAMPLER:
        {
            KAN_ASSERT (layout->combined_image_samplers_count < UINT8_MAX)
            ++layout->combined_image_samplers_count;
            break;
        }
        }
    }

    return layout;
}

void render_backend_system_destroy_pipeline_parameter_set_layout (
    struct render_backend_system_t *system, struct render_backend_pipeline_parameter_set_layout_t *layout)
{
    if (layout->layout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout (system->device, layout->layout, VULKAN_ALLOCATION_CALLBACKS (system));
    }

    kan_free_general (system->parameter_set_layout_wrapper_allocation_group, layout,
                      sizeof (struct render_backend_pipeline_parameter_set_layout_t) +
                          sizeof (struct render_backend_layout_binding_t) * layout->bindings_count);
}

kan_render_pipeline_parameter_set_layout_t kan_render_pipeline_parameter_set_layout_create (
    kan_render_context_t context, struct kan_render_pipeline_parameter_set_layout_description_t *description)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (context);
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_pipeline_parameter_set_layout);
    struct render_backend_pipeline_parameter_set_layout_t *layout =
        render_backend_system_create_pipeline_parameter_set_layout (system, description);
    kan_cpu_section_execution_shutdown (&execution);
    return layout ? KAN_HANDLE_SET (kan_render_pipeline_parameter_set_layout_t, layout) :
                    KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);
}

void kan_render_pipeline_parameter_set_layout_destroy (kan_render_pipeline_parameter_set_layout_t layout)
{
    struct render_backend_pipeline_parameter_set_layout_t *data = KAN_HANDLE_GET (layout);
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_pipeline_parameter_set_layout_destroy_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &schedule->item_allocator, struct scheduled_pipeline_parameter_set_layout_destroy_t);

    item->next = schedule->first_scheduled_pipeline_parameter_set_layout_destroy;
    schedule->first_scheduled_pipeline_parameter_set_layout_destroy = item;
    item->layout = data;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}
