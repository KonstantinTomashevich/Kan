#include <kan/context/render_backend_implementation_interface.h>

struct render_backend_pipeline_layout_t *render_backend_system_register_pipeline_layout (
    struct render_backend_system_t *system,
    kan_instance_size_t parameter_set_layouts_count,
    kan_render_pipeline_parameter_set_layout_t *parameter_set_layouts,
    kan_interned_string_t tracking_name)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_register_pipeline_layout);
    vulkan_size_t used_set_index_count = 0u;
    kan_hash_t layout_hash = 0u;

    // Technically, it would be better to sort layouts by sets in case that user might pass them in different order,
    // but I don't think that this is really required right now. There is not a lot of places where pipelines are
    // created (most likely, only in material loading and in future for some compute passes), so it is not that easy
    // to introduce random order of layouts in description.
    //
    // Anyway, having several duplicated pipeline layouts is not an issue, in contrast to pipeline set layouts,
    // because pipeline set layouts are crucial for pipeline set sharing, but pipeline layouts are only used inside
    // pipelines and are not visible to the outside world.
    //
    // Therefore, allocating additional array and sorting it by set indices is kind of an overkill here.
    for (kan_loop_size_t index = 0u; index < parameter_set_layouts_count; ++index)
    {
        struct render_backend_pipeline_parameter_set_layout_t *layout = KAN_HANDLE_GET (parameter_set_layouts[index]);
        used_set_index_count = KAN_MAX (used_set_index_count, layout->set + 1u);
        kan_hash_t set_hash = KAN_HASH_OBJECT_POINTER (layout);

        if (index == 0u)
        {
            layout_hash = set_hash;
        }
        else
        {
            layout_hash = kan_hash_combine (layout_hash, set_hash);
        }
    }

    kan_atomic_int_lock (&system->pipeline_layout_registration_lock);
    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (&system->pipeline_layouts, layout_hash);

    struct render_backend_pipeline_layout_t *node = (struct render_backend_pipeline_layout_t *) bucket->first;
    const struct render_backend_pipeline_layout_t *node_end =
        (struct render_backend_pipeline_layout_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->node.hash == layout_hash && node->set_layouts_count == parameter_set_layouts_count)
        {
            kan_bool_t equal = KAN_TRUE;
            for (kan_loop_size_t index = 0u; index < parameter_set_layouts_count; ++index)
            {
                if (node->set_layouts[index] != KAN_HANDLE_GET (parameter_set_layouts[index]))
                {
                    equal = KAN_FALSE;
                    break;
                }
            }

            if (equal)
            {
                ++node->usage_count;
                kan_atomic_int_unlock (&system->pipeline_layout_registration_lock);
                return node;
            }
        }

        node = (struct render_backend_pipeline_layout_t *) node->node.list_node.next;
    }

    VkPipelineLayout vulkan_layout;
    VkDescriptorSetLayout layouts_for_pipeline_static[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_DESCS];
    VkDescriptorSetLayout *layouts_for_pipeline = layouts_for_pipeline_static;

    if (used_set_index_count > KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_DESCS)
    {
        layouts_for_pipeline = kan_allocate_general (system->utility_allocation_group,
                                                     sizeof (VkDescriptorSetLayout) * used_set_index_count,
                                                     _Alignof (VkDescriptorSetLayout));
    }

    for (kan_loop_size_t layout_index = 0u; layout_index < used_set_index_count; ++layout_index)
    {
        layouts_for_pipeline[layout_index] = system->empty_descriptor_set_layout;
    }

    for (kan_loop_size_t index = 0u; index < parameter_set_layouts_count; ++index)
    {
        struct render_backend_pipeline_parameter_set_layout_t *layout = KAN_HANDLE_GET (parameter_set_layouts[index]);

        if (layouts_for_pipeline[layout->set] != system->empty_descriptor_set_layout)
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Failed to add parameter set layout \"%s\" for set %lu for to pipeline layout \"%s\" as this "
                     "hardware set is already used.",
                     layout->tracking_name, (unsigned long) layout->set, tracking_name)

            if (layouts_for_pipeline != layouts_for_pipeline_static)
            {
                kan_free_general (system->utility_allocation_group, layouts_for_pipeline,
                                  sizeof (VkDescriptorSetLayout) * used_set_index_count);
            }

            kan_atomic_int_unlock (&system->pipeline_layout_registration_lock);
            return NULL;
        }
        else
        {
            layouts_for_pipeline[layout->set] = layout->layout;
        }
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .setLayoutCount = (vulkan_size_t) used_set_index_count,
        .pSetLayouts = layouts_for_pipeline,
        .pushConstantRangeCount = 0u,
        .pPushConstantRanges = NULL,
    };

    VkResult result = vkCreatePipelineLayout (system->device, &pipeline_layout_info,
                                              VULKAN_ALLOCATION_CALLBACKS (system), &vulkan_layout);

    if (layouts_for_pipeline != layouts_for_pipeline_static)
    {
        kan_free_general (system->utility_allocation_group, layouts_for_pipeline,
                          sizeof (VkDescriptorSetLayout) * used_set_index_count);
    }

    if (result != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to create pipeline layout \"%s\".",
                 tracking_name)
        kan_cpu_section_execution_shutdown (&execution);
        return NULL;
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
    snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "PipelineLayout::%s",
              tracking_name);

    struct VkDebugUtilsObjectNameInfoEXT object_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = NULL,
        .objectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT,
        .objectHandle = CONVERT_HANDLE_FOR_DEBUG vulkan_layout,
        .pObjectName = debug_name,
    };

    vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
#endif

    struct render_backend_pipeline_layout_t *pipeline_layout = kan_allocate_general (
        system->pipeline_layout_wrapper_allocation_group,
        sizeof (struct render_backend_pipeline_layout_t) +
            sizeof (struct render_backend_pipeline_parameter_set_layout_t *) * parameter_set_layouts_count,
        _Alignof (struct render_backend_pipeline_layout_t));

    pipeline_layout->node.hash = layout_hash;
    kan_hash_storage_add (&system->pipeline_layouts, &pipeline_layout->node);
    kan_hash_storage_update_bucket_count_default (&system->pipeline_layouts,
                                                  KAN_CONTEXT_RENDER_BACKEND_VULKAN_SET_LAYOUT_BUCKETS);

    pipeline_layout->layout = vulkan_layout;
    pipeline_layout->usage_count = 1u;
    pipeline_layout->set_layouts_count = parameter_set_layouts_count;

    for (kan_loop_size_t index = 0u; index < parameter_set_layouts_count; ++index)
    {
        pipeline_layout->set_layouts[index] = KAN_HANDLE_GET (parameter_set_layouts[index]);
    }

    kan_atomic_int_unlock (&system->pipeline_layout_registration_lock);
    return pipeline_layout;
}

void render_backend_system_destroy_pipeline_layout (struct render_backend_system_t *system,
                                                    struct render_backend_pipeline_layout_t *layout)
{
    vkDestroyPipelineLayout (system->device, layout->layout, VULKAN_ALLOCATION_CALLBACKS (system));
    kan_free_general (system->pipeline_layout_wrapper_allocation_group, layout,
                      sizeof (struct render_backend_pipeline_layout_t) +
                          sizeof (struct render_backend_pipeline_parameter_set_layout_t *) * layout->set_layouts_count);
}
