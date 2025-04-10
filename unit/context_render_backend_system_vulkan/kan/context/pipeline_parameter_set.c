#include <kan/context/render_backend_implementation_interface.h>

void render_backend_descriptor_set_allocator_init (struct render_backend_descriptor_set_allocator_t *allocator)
{
    kan_bd_list_init (&allocator->pools);
    allocator->multithreaded_access_lock = kan_atomic_int_init (0);
    allocator->total_set_allocations = 0u;
    allocator->uniform_buffer_binding_allocations = 0u;
    allocator->storage_buffer_binding_allocations = 0u;
    allocator->sampler_binding_allocations = 0u;
    allocator->image_binding_allocations = 0u;
}

struct render_backend_descriptor_set_allocation_t render_backend_descriptor_set_allocator_allocate (
    struct render_backend_system_t *system,
    struct render_backend_descriptor_set_allocator_t *allocator,
    struct render_backend_pipeline_parameter_set_layout_t *layout)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_descriptor_set_allocator_allocate);

    struct render_backend_descriptor_set_allocation_t allocation = {
        .descriptor_set = VK_NULL_HANDLE,
        .source_pool = NULL,
    };

    kan_atomic_int_lock (&allocator->multithreaded_access_lock);
    ++allocator->total_set_allocations;
    allocator->uniform_buffer_binding_allocations += layout->uniform_buffers_count;
    allocator->storage_buffer_binding_allocations += layout->storage_buffers_count;
    allocator->sampler_binding_allocations += layout->samplers_count;
    allocator->image_binding_allocations += layout->images_count;

    struct render_backend_descriptor_set_pool_t *pool =
        (struct render_backend_descriptor_set_pool_t *) allocator->pools.first;

    while (pool)
    {
        VkDescriptorSetAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = NULL,
            .descriptorPool = pool->pool,
            1u,
            &layout->layout,
        };

        if (vkAllocateDescriptorSets (system->device, &allocate_info, &allocation.descriptor_set) == VK_SUCCESS)
        {
            allocation.source_pool = pool;
            ++pool->active_allocations;
            kan_atomic_int_unlock (&allocator->multithreaded_access_lock);

            kan_cpu_section_execution_shutdown (&execution);
            return allocation;
        }

        pool = (struct render_backend_descriptor_set_pool_t *) pool->list_node.next;
    }

    allocation.descriptor_set = VK_NULL_HANDLE;
    kan_instance_size_t uniform_buffer_bindings;
    kan_instance_size_t storage_buffer_bindings;
    kan_instance_size_t sampler_bindings;
    kan_instance_size_t image_bindings;

    if (allocator->pools.first)
    {
        // Can calculate better count of sets from history.
        const float total_allocations_float = (float) allocator->total_set_allocations;
        uniform_buffer_bindings =
            (kan_instance_size_t) (((float) allocator->uniform_buffer_binding_allocations) / total_allocations_float);
        storage_buffer_bindings =
            (kan_instance_size_t) (((float) allocator->storage_buffer_binding_allocations) / total_allocations_float);
        sampler_bindings =
            (kan_instance_size_t) (((float) allocator->sampler_binding_allocations) / total_allocations_float);
        image_bindings =
            (kan_instance_size_t) (((float) allocator->image_binding_allocations) / total_allocations_float);
    }
    else
    {
        // No history, we need to rely on predefined values for this pool.
        uniform_buffer_bindings = KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_UNIFORM;
        storage_buffer_bindings = KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_STORAGE;
        sampler_bindings = KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_SAMPLER;
        image_bindings = KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_IMAGE;
    }

    VkDescriptorPoolSize pool_sizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = (vulkan_size_t) uniform_buffer_bindings,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = (vulkan_size_t) storage_buffer_bindings,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = (vulkan_size_t) sampler_bindings,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = (vulkan_size_t) image_bindings,
        },
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSP_SETS,
        .poolSizeCount = sizeof (pool_sizes) / sizeof (pool_sizes[0u]),
        .pPoolSizes = pool_sizes,
    };

    VkDescriptorPool new_pool;
    if (vkCreateDescriptorPool (system->device, &pool_info, VULKAN_ALLOCATION_CALLBACKS (system), &new_pool) !=
        VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to allocate new descriptor set pool.")
        kan_atomic_int_unlock (&allocator->multithreaded_access_lock);
        kan_cpu_section_execution_shutdown (&execution);
        return allocation;
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
    snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "DescriptorPool::global_page_%llu",
              (unsigned long long) new_pool);

    struct VkDebugUtilsObjectNameInfoEXT object_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = NULL,
        .objectType = VK_OBJECT_TYPE_DESCRIPTOR_POOL,
        .objectHandle = CONVERT_HANDLE_FOR_DEBUG new_pool,
        .pObjectName = debug_name,
    };

    vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
#endif

    struct render_backend_descriptor_set_pool_t *new_pool_node = kan_allocate_batched (
        system->descriptor_set_wrapper_allocation_group, sizeof (struct render_backend_descriptor_set_pool_t));

    new_pool_node->pool = new_pool;
    new_pool_node->active_allocations = 0u;
    kan_bd_list_add (&allocator->pools, NULL, &new_pool_node->list_node);

    VkDescriptorSetAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = new_pool,
        1u,
        &layout->layout,
    };

    if (vkAllocateDescriptorSets (system->device, &allocate_info, &allocation.descriptor_set) == VK_SUCCESS)
    {
        allocation.source_pool = new_pool_node;
        ++new_pool_node->active_allocations;
    }
    else
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to allocate descriptor set from fresh pool.")
        allocation.descriptor_set = VK_NULL_HANDLE;
    }

    kan_atomic_int_unlock (&allocator->multithreaded_access_lock);
    kan_cpu_section_execution_shutdown (&execution);
    return allocation;
}

void render_backend_descriptor_set_allocator_free (struct render_backend_system_t *system,
                                                   struct render_backend_descriptor_set_allocator_t *allocator,
                                                   struct render_backend_descriptor_set_allocation_t *allocation)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_descriptor_set_allocator_free);

    kan_atomic_int_lock (&allocator->multithreaded_access_lock);
    vkFreeDescriptorSets (system->device, allocation->source_pool->pool, 1u, &allocation->descriptor_set);
    --allocation->source_pool->active_allocations;

    if (allocation->source_pool->active_allocations == 0u)
    {
        // Free empty pool.
        vkDestroyDescriptorPool (system->device, allocation->source_pool->pool, VULKAN_ALLOCATION_CALLBACKS (system));
        kan_bd_list_remove (&allocator->pools, &allocation->source_pool->list_node);
        kan_free_batched (system->descriptor_set_wrapper_allocation_group, allocation->source_pool);
    }

    kan_atomic_int_unlock (&allocator->multithreaded_access_lock);
    kan_cpu_section_execution_shutdown (&execution);
}

void render_backend_descriptor_set_allocator_shutdown (struct render_backend_system_t *system,
                                                       struct render_backend_descriptor_set_allocator_t *allocator)
{
    struct render_backend_descriptor_set_pool_t *pool =
        (struct render_backend_descriptor_set_pool_t *) allocator->pools.first;

    while (pool)
    {
        struct render_backend_descriptor_set_pool_t *next =
            (struct render_backend_descriptor_set_pool_t *) pool->list_node.next;
        vkDestroyDescriptorPool (system->device, pool->pool, VULKAN_ALLOCATION_CALLBACKS (system));
        kan_free_batched (system->descriptor_set_wrapper_allocation_group, pool);
        pool = next;
    }
}

struct render_backend_pipeline_parameter_set_t *render_backend_system_create_pipeline_parameter_set (
    struct render_backend_system_t *system, struct kan_render_pipeline_parameter_set_description_t *description)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_pipeline_parameter_set_internal);

    struct render_backend_pipeline_parameter_set_layout_t *layout = KAN_HANDLE_GET (description->layout);
    struct render_backend_descriptor_set_allocation_t stable_allocation = {VK_NULL_HANDLE, NULL};
    struct render_backend_descriptor_set_allocation_t *unstable_allocations = NULL;

    if (description->stable_binding)
    {
        stable_allocation =
            render_backend_descriptor_set_allocator_allocate (system, &system->descriptor_set_allocator, layout);

        if (stable_allocation.descriptor_set == VK_NULL_HANDLE)
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Failed to create parameter set \"%s\": failed to allocate descriptor set.",
                     description->tracking_name)
            kan_cpu_section_execution_shutdown (&execution);
            return NULL;
        }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
        char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
        snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "PipelineParameterSet::%s::stable",
                  description->tracking_name);

        struct VkDebugUtilsObjectNameInfoEXT object_name = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = NULL,
            .objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET,
            .objectHandle = CONVERT_HANDLE_FOR_DEBUG stable_allocation.descriptor_set,
            .pObjectName = debug_name,
        };

        vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
#endif
    }
    else
    {
        unstable_allocations = kan_allocate_general (system->pipeline_parameter_set_wrapper_allocation_group,
                                                     sizeof (struct render_backend_descriptor_set_allocation_t) *
                                                         KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT,
                                                     _Alignof (struct render_backend_descriptor_set_allocation_t));

        kan_bool_t allocated_successfully = KAN_TRUE;
        for (kan_loop_size_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
        {
            unstable_allocations[index].descriptor_set = VK_NULL_HANDLE;
        }

        for (kan_loop_size_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
        {
            unstable_allocations[index] =
                render_backend_descriptor_set_allocator_allocate (system, &system->descriptor_set_allocator, layout);

            if (unstable_allocations[index].descriptor_set == VK_NULL_HANDLE)
            {
                KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                         "Failed to create parameter set \"%s\": failed to allocate descriptor set.",
                         description->tracking_name)

                allocated_successfully = KAN_FALSE;
                break;
            }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
            char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
            snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME,
                      "PipelineParameterSet::%s::unstable%lu", description->tracking_name, (unsigned long) index);

            struct VkDebugUtilsObjectNameInfoEXT object_name = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext = NULL,
                .objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET,
                .objectHandle = CONVERT_HANDLE_FOR_DEBUG unstable_allocations[index].descriptor_set,
                .pObjectName = debug_name,
            };

            vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
#endif
        }

        if (!allocated_successfully)
        {
            for (kan_loop_size_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
            {
                if (unstable_allocations[index].descriptor_set != VK_NULL_HANDLE)
                {
                    render_backend_descriptor_set_allocator_free (system, &system->descriptor_set_allocator,
                                                                  &unstable_allocations[index]);
                }
            }

            kan_free_general (system->pipeline_parameter_set_wrapper_allocation_group, unstable_allocations,
                              sizeof (struct render_backend_descriptor_set_allocation_t) *
                                  KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT);

            kan_cpu_section_execution_shutdown (&execution);
            return NULL;
        }
    }

    struct render_backend_pipeline_parameter_set_t *set =
        kan_allocate_batched (system->pipeline_parameter_set_wrapper_allocation_group,
                              sizeof (struct render_backend_pipeline_parameter_set_t));

    kan_atomic_int_lock (&system->resource_registration_lock);
    kan_bd_list_add (&system->pipeline_parameter_sets, NULL, &set->list_node);
    kan_atomic_int_unlock (&system->resource_registration_lock);

    set->system = system;
    set->layout = layout;
    set->stable_binding = description->stable_binding;

    if (description->stable_binding)
    {
        set->stable.allocation = stable_allocation;
        set->stable.has_been_submitted = KAN_FALSE;
    }
    else
    {
        set->unstable.allocations = unstable_allocations;
        // Never accessed, therefore index is UINT32_MAX.
        set->unstable.last_accessed_allocation_index = UINT32_MAX;
    }

    set->bound_image_views = NULL;
    if (layout->images_count > 0u)
    {
        set->bound_image_views =
            kan_allocate_general (system->pipeline_parameter_set_wrapper_allocation_group,
                                  sizeof (VkImageView) * layout->bindings_count, _Alignof (VkImageView));

        for (kan_loop_size_t index = 0u; index < layout->bindings_count; ++index)
        {
            set->bound_image_views[index] = VK_NULL_HANDLE;
        }
    }

    set->first_render_target_attachment = NULL;
    set->tracking_name = description->tracking_name;
    kan_cpu_section_execution_shutdown (&execution);
    return set;
}

static inline void detach_parameter_set_from_render_target (
    struct render_backend_pipeline_parameter_set_t *set,
    struct render_backend_parameter_set_render_target_attachment_t *attachment)
{
    struct render_backend_image_t *image = attachment->image;
    struct image_parameter_set_attachment_t *previous = NULL;
    struct image_parameter_set_attachment_t *image_attachment = image->first_parameter_set_attachment;

    while (image_attachment)
    {
        if (image_attachment->set == set)
        {
            if (previous)
            {
                previous->next = image_attachment->next;
            }
            else
            {
                KAN_ASSERT (image_attachment == image->first_parameter_set_attachment)
                image->first_parameter_set_attachment = image_attachment->next;
            }

            kan_free_batched (set->system->image_wrapper_allocation_group, image_attachment);
            break;
        }

        previous = image_attachment;
        image_attachment = image_attachment->next;
    }
}

void render_backend_system_destroy_pipeline_parameter_set (struct render_backend_system_t *system,
                                                           struct render_backend_pipeline_parameter_set_t *set)
{
    if (set->stable_binding)
    {
        render_backend_descriptor_set_allocator_free (system, &system->descriptor_set_allocator,
                                                      &set->stable.allocation);
    }
    else
    {
        for (kan_loop_size_t allocation_index = 0u;
             allocation_index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++allocation_index)
        {
            render_backend_descriptor_set_allocator_free (system, &system->descriptor_set_allocator,
                                                          &set->unstable.allocations[allocation_index]);
        }

        kan_free_general (system->pipeline_parameter_set_wrapper_allocation_group, set->unstable.allocations,
                          sizeof (struct render_backend_descriptor_set_allocation_t) *
                              KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT);
    }

    struct render_backend_parameter_set_render_target_attachment_t *attachment = set->first_render_target_attachment;
    while (attachment)
    {
        struct render_backend_parameter_set_render_target_attachment_t *next = attachment->next;
        detach_parameter_set_from_render_target (set, attachment);
        kan_free_batched (system->pipeline_parameter_set_wrapper_allocation_group, attachment);
        attachment = next;
    }

    if (set->bound_image_views)
    {
        for (kan_loop_size_t index = 0u; index < set->layout->bindings_count; ++index)
        {
            // When we're destroying parameter set, it is surely already safe to destroy its image views too.
            if (set->bound_image_views[index] != VK_NULL_HANDLE)
            {
                vkDestroyImageView (system->device, set->bound_image_views[index],
                                    VULKAN_ALLOCATION_CALLBACKS (system));
            }
        }

        kan_free_general (system->pipeline_parameter_set_wrapper_allocation_group, set->bound_image_views,
                          sizeof (VkImageView) * set->layout->bindings_count);
    }

    kan_free_batched (system->pipeline_parameter_set_wrapper_allocation_group, set);
}

void render_backend_apply_descriptor_set_mutation (struct render_backend_pipeline_parameter_set_t *set_context,
                                                   VkDescriptorSet source_set,
                                                   VkDescriptorSet target_set,
                                                   kan_instance_size_t update_bindings_count,
                                                   struct kan_render_parameter_update_description_t *update_bindings)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, set_context->system->section_apply_descriptor_set_mutation);

    if (source_set == VK_NULL_HANDLE || target_set == VK_NULL_HANDLE)
    {
        kan_cpu_section_execution_shutdown (&execution);
        return;
    }

    const kan_bool_t transfer_needed = source_set != target_set;
    const kan_bool_t update_needed = update_bindings_count > 0u;

    kan_instance_size_t transfer_count = 0u;
    VkCopyDescriptorSet *transfer = NULL;

    if (transfer_needed)
    {
        transfer = kan_allocate_general (set_context->system->utility_allocation_group,
                                         sizeof (VkCopyDescriptorSet) * set_context->layout->bindings_count,
                                         _Alignof (VkCopyDescriptorSet));

        for (vulkan_size_t binding = 0u; binding < set_context->layout->bindings_count; ++binding)
        {
            kan_bool_t should_transfer = KAN_TRUE;
            if (update_needed)
            {
                for (kan_loop_size_t index = 0u; index < update_bindings_count; ++index)
                {
                    if (update_bindings[index].binding == binding)
                    {
                        should_transfer = KAN_FALSE;
                        break;
                    }
                }
            }

            if (should_transfer)
            {
                transfer[transfer_count] = (VkCopyDescriptorSet) {
                    .sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,
                    .pNext = NULL,
                    .srcSet = source_set,
                    .srcBinding = (vulkan_size_t) binding,
                    .srcArrayElement = 0u,
                    .dstSet = target_set,
                    .dstBinding = (vulkan_size_t) binding,
                    .dstArrayElement = 0u,
                    .descriptorCount = 1u,
                };

                ++transfer_count;
            }
        }
    }

    kan_instance_size_t update_count = 0u;
    VkWriteDescriptorSet *update = NULL;

    kan_instance_size_t buffer_info_count = 0u;
    VkDescriptorBufferInfo buffer_info_static[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_DESCS];
    VkDescriptorBufferInfo *buffer_info = buffer_info_static;

    kan_instance_size_t image_info_count = 0u;
    VkDescriptorImageInfo image_info_static[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_DESCS];
    VkDescriptorImageInfo *image_info = image_info_static;

    if (update_needed)
    {
        for (kan_loop_size_t index = 0u; index < update_bindings_count; ++index)
        {
            switch (set_context->layout->bindings[update_bindings[index].binding].type)
            {
            case KAN_RENDER_PARAMETER_BINDING_TYPE_UNIFORM_BUFFER:
            case KAN_RENDER_PARAMETER_BINDING_TYPE_STORAGE_BUFFER:
                ++buffer_info_count;
                break;

            case KAN_RENDER_PARAMETER_BINDING_TYPE_SAMPLER:
            case KAN_RENDER_PARAMETER_BINDING_TYPE_IMAGE:
                ++image_info_count;
                break;
            }
        }

        update = kan_allocate_general (set_context->system->utility_allocation_group,
                                       sizeof (VkWriteDescriptorSet) * update_bindings_count,
                                       _Alignof (VkWriteDescriptorSet));

        if (buffer_info_count >= KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_DESCS)
        {
            buffer_info = kan_allocate_general (set_context->system->utility_allocation_group,
                                                sizeof (VkDescriptorBufferInfo) * buffer_info_count,
                                                _Alignof (VkDescriptorBufferInfo));
        }

        if (image_info_count >= KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_DESCS)
        {
            image_info = kan_allocate_general (set_context->system->utility_allocation_group,
                                               sizeof (VkDescriptorImageInfo) * image_info_count,
                                               _Alignof (VkDescriptorImageInfo));
        }

        VkDescriptorBufferInfo *next_buffer_info = buffer_info;
        VkDescriptorImageInfo *next_image_info = image_info;

        for (kan_loop_size_t index = 0u; index < update_bindings_count; ++index)
        {
            VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
            VkDescriptorBufferInfo *this_buffer_info = NULL;
            VkDescriptorImageInfo *this_image_info = NULL;
            vulkan_size_t array_index = 0u;

            switch (set_context->layout->bindings[update_bindings[index].binding].type)
            {
            case KAN_RENDER_PARAMETER_BINDING_TYPE_UNIFORM_BUFFER:
                descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                break;

            case KAN_RENDER_PARAMETER_BINDING_TYPE_STORAGE_BUFFER:
                descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                break;

            case KAN_RENDER_PARAMETER_BINDING_TYPE_SAMPLER:
                descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLER;
                break;

            case KAN_RENDER_PARAMETER_BINDING_TYPE_IMAGE:
                descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                break;
            }

            switch (set_context->layout->bindings[update_bindings[index].binding].type)
            {
            case KAN_RENDER_PARAMETER_BINDING_TYPE_UNIFORM_BUFFER:
            case KAN_RENDER_PARAMETER_BINDING_TYPE_STORAGE_BUFFER:
                this_buffer_info = next_buffer_info;
                ++next_buffer_info;

                struct render_backend_buffer_t *buffer = KAN_HANDLE_GET (update_bindings[index].buffer_binding.buffer);
                *this_buffer_info = (VkDescriptorBufferInfo) {
                    .buffer = buffer->buffer,
                    .offset = (vulkan_size_t) update_bindings[index].buffer_binding.offset,
                    .range = (vulkan_size_t) update_bindings[index].buffer_binding.range,
                };

                break;

            case KAN_RENDER_PARAMETER_BINDING_TYPE_SAMPLER:
            {
                this_image_info = next_image_info;
                ++next_image_info;

                this_image_info->sampler = render_backend_resolve_cached_sampler (
                    set_context->system, &update_bindings[index].sampler_binding.sampler);

                this_image_info->imageView = VK_NULL_HANDLE;
                this_image_info->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                break;
            }

            case KAN_RENDER_PARAMETER_BINDING_TYPE_IMAGE:
            {
                this_image_info = next_image_info;
                ++next_image_info;

                this_image_info->sampler = VK_NULL_HANDLE;
                this_image_info->imageView = VK_NULL_HANDLE;
                this_image_info->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                array_index = update_bindings[index].image_binding.array_index;

                if (set_context->bound_image_views[update_bindings[index].binding] != VK_NULL_HANDLE)
                {
                    struct render_backend_schedule_state_t *schedule =
                        render_backend_system_get_schedule_for_destroy (set_context->system);
                    kan_atomic_int_lock (&schedule->schedule_lock);

                    struct scheduled_detached_image_view_destroy_t *image_view_destroy =
                        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator,
                                                                  struct scheduled_detached_image_view_destroy_t);

                    image_view_destroy->next = schedule->first_scheduled_detached_image_view_destroy;
                    schedule->first_scheduled_detached_image_view_destroy = image_view_destroy;
                    image_view_destroy->detached_image_view =
                        set_context->bound_image_views[update_bindings[index].binding];
                    kan_atomic_int_unlock (&schedule->schedule_lock);
                }

                if (KAN_HANDLE_IS_VALID (update_bindings[index].image_binding.image))
                {
                    struct render_backend_image_t *image = KAN_HANDLE_GET (update_bindings[index].image_binding.image);
                    switch (get_image_format_class (image->description.format))
                    {
                    case IMAGE_FORMAT_CLASS_COLOR:
                        this_image_info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        break;

                    case IMAGE_FORMAT_CLASS_DEPTH:
                    case IMAGE_FORMAT_CLASS_STENCIL:
                    case IMAGE_FORMAT_CLASS_DEPTH_STENCIL:
                        this_image_info->imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                        break;
                    }

                    VkImageViewCreateInfo create_info = {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                        .pNext = NULL,
                        .flags = 0u,
                        .image = image->image,
                        .viewType = get_image_view_type (&image->description),
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
                                // Aspects getter returns both depth and stencil flags, which is not supported by view.
                                // We should never need to sample stencil buffer, therefore we just disable it.
                                .aspectMask = get_image_aspects (&image->description) & ~VK_IMAGE_ASPECT_STENCIL_BIT,
                                .baseMipLevel = 0u,
                                .levelCount = (vulkan_size_t) image->description.mips,
                                .baseArrayLayer = (vulkan_size_t) update_bindings[index].image_binding.layer_offset,
                                .layerCount = (vulkan_size_t) update_bindings[index].image_binding.layer_count,
                            },
                    };

                    if (vkCreateImageView (set_context->system->device, &create_info,
                                           VULKAN_ALLOCATION_CALLBACKS (set_context->system),
                                           &this_image_info->imageView) != VK_SUCCESS)
                    {
                        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                                 "Failed to create image view in order to bind it at %lu of set \"%s\".",
                                 (unsigned long) update_bindings[index].binding, set_context->tracking_name)
                        this_image_info->imageView = VK_NULL_HANDLE;
                    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
                    if (this_image_info->imageView != VK_NULL_HANDLE)
                    {
                        char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
                        snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME,
                                  "ImageView::ForPipelineParameterSet::%s::binding%lu", set_context->tracking_name,
                                  (unsigned long) update_bindings[index].binding);

                        struct VkDebugUtilsObjectNameInfoEXT object_name = {
                            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                            .pNext = NULL,
                            .objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
                            .objectHandle = CONVERT_HANDLE_FOR_DEBUG this_image_info->imageView,
                            .pObjectName = debug_name,
                        };

                        vkSetDebugUtilsObjectNameEXT (set_context->system->device, &object_name);
                    }
#endif

                    set_context->bound_image_views[update_bindings[index].binding] = this_image_info->imageView;
                }

                break;
            }
            }

            update[update_count] = (VkWriteDescriptorSet) {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = NULL,
                .dstSet = target_set,
                .dstBinding = (vulkan_size_t) update_bindings[index].binding,
                .dstArrayElement = array_index,
                .descriptorCount = 1u,
                .descriptorType = descriptor_type,
                .pBufferInfo = this_buffer_info,
                .pImageInfo = this_image_info,
                .pTexelBufferView = NULL,
            };

            ++update_count;
        }
    }

    vkUpdateDescriptorSets (set_context->system->device, (vulkan_size_t) update_count, update,
                            (vulkan_size_t) transfer_count, transfer);

    if (transfer)
    {
        kan_free_general (set_context->system->utility_allocation_group, transfer,
                          sizeof (VkCopyDescriptorSet) * set_context->layout->bindings_count);
    }

    if (update)
    {
        kan_free_general (set_context->system->utility_allocation_group, update,
                          sizeof (VkWriteDescriptorSet) * update_bindings_count);
    }

    if (buffer_info != buffer_info_static)
    {
        kan_free_general (set_context->system->utility_allocation_group, buffer_info,
                          sizeof (VkDescriptorBufferInfo) * buffer_info_count);
    }

    if (image_info != image_info_static)
    {
        kan_free_general (set_context->system->utility_allocation_group, image_info,
                          sizeof (VkDescriptorImageInfo) * image_info_count);
    }

    kan_cpu_section_execution_shutdown (&execution);
}

kan_render_pipeline_parameter_set_t kan_render_pipeline_parameter_set_create (
    kan_render_context_t context, struct kan_render_pipeline_parameter_set_description_t *description)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (context);
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_pipeline_parameter_set);

    struct render_backend_pipeline_parameter_set_t *set =
        render_backend_system_create_pipeline_parameter_set (system, description);

    if (!set)
    {
        kan_cpu_section_execution_shutdown (&execution);
        return KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_t);
    }

    kan_render_pipeline_parameter_set_t handle = KAN_HANDLE_SET (kan_render_pipeline_parameter_set_t, set);
    kan_render_pipeline_parameter_set_update (handle, description->initial_bindings_count,
                                              description->initial_bindings);

    kan_cpu_section_execution_shutdown (&execution);
    return handle;
}

void kan_render_pipeline_parameter_set_update (kan_render_pipeline_parameter_set_t set,
                                               kan_instance_size_t bindings_count,
                                               struct kan_render_parameter_update_description_t *bindings)
{
    struct render_backend_pipeline_parameter_set_t *data = KAN_HANDLE_GET (set);
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, data->system->section_pipeline_parameter_set_update);

    // Remove attachments to render targets if they've changed.
    struct render_backend_parameter_set_render_target_attachment_t *previous = NULL;
    struct render_backend_parameter_set_render_target_attachment_t *render_target_attachment =
        data->first_render_target_attachment;

    while (render_target_attachment)
    {
        kan_bool_t broken = KAN_FALSE;
        struct render_backend_parameter_set_render_target_attachment_t *next = render_target_attachment->next;

        for (kan_loop_size_t binding_index = 0u; binding_index < bindings_count; ++binding_index)
        {
            struct kan_render_parameter_update_description_t *update = &bindings[binding_index];
            if (data->layout->bindings[update->binding].type == KAN_RENDER_PARAMETER_BINDING_TYPE_IMAGE &&
                KAN_HANDLE_GET (update->image_binding.image) != render_target_attachment->image)
            {
                // Render target attachment changed, destroy it.
                broken = KAN_TRUE;
                break;
            }
        }

        if (broken)
        {
            if (previous)
            {
                previous->next = next;
            }
            else
            {
                KAN_ASSERT (data->first_render_target_attachment == render_target_attachment)
                data->first_render_target_attachment = next;
            }

            detach_parameter_set_from_render_target (data, render_target_attachment);
            kan_free_batched (data->system->pipeline_parameter_set_wrapper_allocation_group, render_target_attachment);
        }
        else
        {
            previous = render_target_attachment;
        }

        render_target_attachment = next;
    }

    if (data->stable_binding)
    {
        VkDescriptorSet source_set = data->stable.allocation.descriptor_set;
        if (data->stable.has_been_submitted || data->stable.allocation.descriptor_set == VK_NULL_HANDLE)
        {
            struct render_backend_schedule_state_t *schedule =
                render_backend_system_get_schedule_for_destroy (data->system);
            kan_atomic_int_lock (&schedule->schedule_lock);

            struct scheduled_detached_descriptor_set_destroy_t *descriptor_set_destroy =
                KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator,
                                                          struct scheduled_detached_descriptor_set_destroy_t);

            descriptor_set_destroy->next = schedule->first_scheduled_detached_descriptor_set_destroy;
            schedule->first_scheduled_detached_descriptor_set_destroy = descriptor_set_destroy;
            descriptor_set_destroy->allocation = data->stable.allocation;
            kan_atomic_int_unlock (&schedule->schedule_lock);

            data->stable.allocation = render_backend_descriptor_set_allocator_allocate (
                data->system, &data->system->descriptor_set_allocator, data->layout);
            data->stable.has_been_submitted = KAN_FALSE;

            if (data->stable.allocation.descriptor_set == VK_NULL_HANDLE)
            {
                KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                         "Failed to update stable parameter set \"%s\": failed to allocate new descriptor set.",
                         data->tracking_name)
            }
        }

        VkDescriptorSet target_set = data->stable.allocation.descriptor_set;
        render_backend_apply_descriptor_set_mutation (data, source_set, target_set, bindings_count, bindings);
    }
    else
    {
        VkDescriptorSet source_set =
            data->unstable.last_accessed_allocation_index == UINT32_MAX ?
                data->unstable.allocations[data->system->current_frame_in_flight_index].descriptor_set :
                data->unstable.allocations[data->unstable.last_accessed_allocation_index].descriptor_set;

        VkDescriptorSet target_set =
            data->unstable.allocations[data->system->current_frame_in_flight_index].descriptor_set;

        render_backend_apply_descriptor_set_mutation (data, source_set, target_set, bindings_count, bindings);
        data->unstable.last_accessed_allocation_index = data->system->current_frame_in_flight_index;
    }

    for (kan_loop_size_t binding_index = 0u; binding_index < bindings_count; ++binding_index)
    {
        struct kan_render_parameter_update_description_t *update = &bindings[binding_index];
        if (data->layout->bindings[update->binding].type == KAN_RENDER_PARAMETER_BINDING_TYPE_IMAGE)
        {
            struct render_backend_image_t *image = KAN_HANDLE_GET (update->image_binding.image);
            if (image && image->description.render_target)
            {
                kan_bool_t already_attached_to_set = KAN_FALSE;
                render_target_attachment = data->first_render_target_attachment;

                while (render_target_attachment)
                {
                    if (render_target_attachment->binding == update->binding)
                    {
                        KAN_ASSERT (render_target_attachment->image == image)
                        already_attached_to_set = KAN_TRUE;
                        break;
                    }

                    render_target_attachment = render_target_attachment->next;
                }

                if (!already_attached_to_set)
                {
                    render_target_attachment =
                        kan_allocate_batched (data->system->pipeline_parameter_set_wrapper_allocation_group,
                                              sizeof (struct render_backend_parameter_set_render_target_attachment_t));

                    render_target_attachment->next = data->first_render_target_attachment;
                    data->first_render_target_attachment = render_target_attachment;
                    render_target_attachment->binding = update->binding;
                    render_target_attachment->image = image;
                }

                kan_bool_t already_attached_to_image = KAN_FALSE;
                struct image_parameter_set_attachment_t *image_attachment = image->first_parameter_set_attachment;

                while (image_attachment)
                {
                    if (image_attachment->binding == update->binding && image_attachment->set == data)
                    {
                        already_attached_to_image = KAN_TRUE;
                        break;
                    }

                    image_attachment = image_attachment->next;
                }

                if (!already_attached_to_image)
                {
                    image_attachment = kan_allocate_batched (data->system->image_wrapper_allocation_group,
                                                             sizeof (struct image_parameter_set_attachment_t));

                    image_attachment->next = image->first_parameter_set_attachment;
                    image->first_parameter_set_attachment = image_attachment;
                    image_attachment->binding = update->binding;
                    image_attachment->set = data;
                }
            }

            break;
        }
    }

    kan_cpu_section_execution_shutdown (&execution);
}

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_pipeline_parameter_set_destroy (
    kan_render_pipeline_parameter_set_t set)
{
    struct render_backend_pipeline_parameter_set_t *data = KAN_HANDLE_GET (set);
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_pipeline_parameter_set_destroy_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &schedule->item_allocator, struct scheduled_pipeline_parameter_set_destroy_t);

    item->next = schedule->first_scheduled_pipeline_parameter_set_destroy;
    schedule->first_scheduled_pipeline_parameter_set_destroy = item;
    item->set = data;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}
