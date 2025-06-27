#include <kan/context/render_backend_implementation_interface.h>

static inline kan_instance_size_t calculate_aligned_layout_size (kan_instance_size_t used_binding_index_count)
{
    return (kan_instance_size_t) kan_apply_alignment (
        sizeof (struct render_backend_pipeline_parameter_set_layout_t) +
            sizeof (struct render_backend_layout_binding_t) * used_binding_index_count,
        _Alignof (struct render_backend_pipeline_parameter_set_layout_t));
}

struct render_backend_pipeline_parameter_set_layout_t *render_backend_system_register_pipeline_parameter_set_layout (
    struct render_backend_system_t *system, struct kan_render_pipeline_parameter_set_layout_description_t *description)
{
    // We make an assumption that if layouts have the same count of bindings for each type, then they are most likely
    // to be compatible, because tools use autogenerate bindings with per-type ordering (render pipeline language does
    // that too).

    uint8_t uniform_buffer_binding_count = 0u;
    uint8_t storage_buffer_binding_count = 0u;
    uint8_t sampler_binding_count = 0u;
    uint8_t image_binding_count = 0u;

    for (kan_loop_size_t binding_index = 0u; binding_index < description->bindings_count; ++binding_index)
    {
        struct kan_render_parameter_binding_description_t *binding_description = &description->bindings[binding_index];
        switch (binding_description->type)
        {
        case KAN_RENDER_PARAMETER_BINDING_TYPE_UNIFORM_BUFFER:
            KAN_ASSERT (uniform_buffer_binding_count < UINT8_MAX)
            ++uniform_buffer_binding_count;
            break;

        case KAN_RENDER_PARAMETER_BINDING_TYPE_STORAGE_BUFFER:
            KAN_ASSERT (storage_buffer_binding_count < UINT8_MAX)
            ++storage_buffer_binding_count;
            break;

        case KAN_RENDER_PARAMETER_BINDING_TYPE_SAMPLER:
        {
            KAN_ASSERT (sampler_binding_count < UINT8_MAX)
            ++sampler_binding_count;
            break;
        }

        case KAN_RENDER_PARAMETER_BINDING_TYPE_IMAGE:
        {
            KAN_ASSERT (sampler_binding_count < UINT8_MAX)
            ++image_binding_count;
            break;
        }
        }
    }

    _Static_assert (sizeof (kan_hash_t) >= 4u, "We can confidently pack all sizes into one unique hash.");
    const kan_hash_t layout_hash = (uniform_buffer_binding_count << 0u) | (storage_buffer_binding_count << 1u) |
                                   (sampler_binding_count << 2u) | (image_binding_count << 3u);

    kan_atomic_int_lock (&system->pipeline_parameter_set_layout_registration_lock);
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&system->pipeline_parameter_set_layouts, layout_hash);

    struct render_backend_pipeline_parameter_set_layout_t *node =
        (struct render_backend_pipeline_parameter_set_layout_t *) bucket->first;
    const struct render_backend_pipeline_parameter_set_layout_t *node_end =
        (struct render_backend_pipeline_parameter_set_layout_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->node.hash == layout_hash)
        {
            // Now lets check that layout is truly compatible.
            // As we pack binding counts and stability to hash, this check is quite easy.
            bool compatible = true;

            for (kan_loop_size_t binding_index = 0u; binding_index < description->bindings_count; ++binding_index)
            {
                struct kan_render_parameter_binding_description_t *binding_description =
                    &description->bindings[binding_index];

                if (binding_description->binding > node->bindings_count ||
                    binding_description->type != node->bindings[binding_description->binding].type ||
                    binding_description->descriptor_count !=
                        node->bindings[binding_description->binding].descriptor_count ||
                    binding_description->used_stage_mask !=
                        node->bindings[binding_description->binding].used_stage_mask)
                {
                    compatible = false;
                    break;
                }
            }

            if (compatible)
            {
                // Layout is compatible, we can just use it, no need for the new one.
                kan_atomic_int_unlock (&system->pipeline_parameter_set_layout_registration_lock);
                kan_atomic_int_add (&node->reference_count, 1u);
                return node;
            }
        }

        node = (struct render_backend_pipeline_parameter_set_layout_t *) node->node.list_node.next;
    }

    // No compatible layout, we need to create new one. But we cannot unlock the lock, as it would make it possible
    // to get into the race condition and create two similar layouts. We expect new layout creation to be quite rare,
    // therefore it should be okay to do that.

    VkDescriptorSetLayoutBinding bindings_static[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_DESCS];
    VkDescriptorBindingFlagsEXT bindings_flags_static[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_DESCS];

    VkDescriptorSetLayoutBinding *bindings = bindings_static;
    VkDescriptorBindingFlagsEXT *bindings_flags = bindings_flags_static;

    if (description->bindings_count > KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_DESCS)
    {
        bindings = kan_allocate_general (system->utility_allocation_group,
                                         sizeof (VkDescriptorSetLayoutBinding) * description->bindings_count,
                                         _Alignof (VkDescriptorSetLayoutBinding));
        bindings_flags = kan_allocate_general (system->utility_allocation_group,
                                               sizeof (VkDescriptorBindingFlagsEXT) * description->bindings_count,
                                               _Alignof (VkDescriptorBindingFlagsEXT));
    }

    vulkan_size_t used_binding_index_count = 0u;
    for (kan_loop_size_t binding_index = 0u; binding_index < description->bindings_count; ++binding_index)
    {
        struct kan_render_parameter_binding_description_t *binding_description = &description->bindings[binding_index];
        VkDescriptorSetLayoutBinding *vulkan_binding = &bindings[binding_index];
        bindings_flags[binding_index] = 0u;

        vulkan_binding->binding = (vulkan_size_t) binding_description->binding;
        KAN_ASSERT (binding_description->descriptor_count > 0u &&
                    (binding_description->type == KAN_RENDER_PARAMETER_BINDING_TYPE_IMAGE ||
                     binding_description->descriptor_count == 1u))

        if (binding_description->descriptor_count > 1u)
        {
            // If there are several descriptors, we assume that there
            bindings_flags[binding_index] |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT;
        }

        vulkan_binding->descriptorCount = binding_description->descriptor_count;
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

        case KAN_RENDER_PARAMETER_BINDING_TYPE_SAMPLER:
            vulkan_binding->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            break;

        case KAN_RENDER_PARAMETER_BINDING_TYPE_IMAGE:
            vulkan_binding->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
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

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT layout_bindings_flags = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
        .pNext = NULL,
        .bindingCount = (vulkan_size_t) description->bindings_count,
        .pBindingFlags = bindings_flags,
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &layout_bindings_flags,
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
        kan_free_general (system->utility_allocation_group, bindings_flags,
                          sizeof (VkDescriptorBindingFlagsEXT) * description->bindings_count);
    }

    if (result != VK_SUCCESS)
    {
        kan_atomic_int_unlock (&system->pipeline_parameter_set_layout_registration_lock);
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

    struct render_backend_pipeline_parameter_set_layout_t *layout = kan_allocate_general (
        system->parameter_set_layout_wrapper_allocation_group, calculate_aligned_layout_size (used_binding_index_count),
        _Alignof (struct render_backend_pipeline_parameter_set_layout_t));

    layout->node.hash = layout_hash;
    layout->system = system;

    kan_hash_storage_add (&system->pipeline_parameter_set_layouts, &layout->node);
    kan_hash_storage_update_bucket_count_default (&system->pipeline_parameter_set_layouts,
                                                  KAN_CONTEXT_RENDER_BACKEND_VULKAN_SET_LAYOUT_BUCKETS);

    layout->reference_count = kan_atomic_int_init (1);
    layout->layout = vulkan_layout;

    layout->bindings_count = used_binding_index_count;
    layout->uniform_buffers_count = uniform_buffer_binding_count;
    layout->storage_buffers_count = storage_buffer_binding_count;
    layout->samplers_count = sampler_binding_count;
    layout->images_count = image_binding_count;
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
    }

    // Only now, when new layout is finally filled with all the info, we can lift the lock.
    // If we did it earlier, we would risk having incoherent layout cache due to race condition.
    kan_atomic_int_unlock (&system->pipeline_parameter_set_layout_registration_lock);
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
                      calculate_aligned_layout_size (layout->bindings_count));
}

kan_render_pipeline_parameter_set_layout_t kan_render_pipeline_parameter_set_layout_create (
    kan_render_context_t context, struct kan_render_pipeline_parameter_set_layout_description_t *description)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (context);
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_register_pipeline_parameter_set_layout);
    struct render_backend_pipeline_parameter_set_layout_t *layout =
        render_backend_system_register_pipeline_parameter_set_layout (system, description);
    kan_cpu_section_execution_shutdown (&execution);
    return layout ? KAN_HANDLE_SET (kan_render_pipeline_parameter_set_layout_t, layout) :
                    KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);
}

void kan_render_pipeline_parameter_set_layout_destroy (kan_render_pipeline_parameter_set_layout_t layout)
{
    struct render_backend_pipeline_parameter_set_layout_t *data = KAN_HANDLE_GET (layout);
    if (kan_atomic_int_add (&data->reference_count, -1) == 1)
    {
        // We only disturb the schedule if we think that layout is not used anymore at all.
        struct render_backend_schedule_state_t *schedule =
            render_backend_system_get_schedule_for_destroy (data->system);
        kan_atomic_int_lock (&schedule->schedule_lock);

        struct scheduled_pipeline_parameter_set_layout_destroy_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &schedule->item_allocator, struct scheduled_pipeline_parameter_set_layout_destroy_t);

        item->next = schedule->first_scheduled_pipeline_parameter_set_layout_destroy;
        schedule->first_scheduled_pipeline_parameter_set_layout_destroy = item;
        item->layout = data;
        kan_atomic_int_unlock (&schedule->schedule_lock);
    }
}
