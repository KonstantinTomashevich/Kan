#include <math.h>

#include <kan/context/render_backend_implementation_interface.h>

static inline VkFilter to_vulkan_filter (enum kan_render_filter_mode_t filter)
{
    switch (filter)
    {
    case KAN_RENDER_FILTER_MODE_NEAREST:
        return VK_FILTER_NEAREST;

    case KAN_RENDER_FILTER_MODE_LINEAR:
        return VK_FILTER_LINEAR;

    case KAN_RENDER_FILTER_MODE_COUNT:
        KAN_ASSERT (false)
        return VK_FILTER_NEAREST;
    }

    KAN_ASSERT (false)
    return VK_FILTER_NEAREST;
}

static inline VkSamplerMipmapMode to_vulkan_sampler_mip_map_mode (enum kan_render_mip_map_mode_t mode)
{
    switch (mode)
    {
    case KAN_RENDER_MIP_MAP_MODE_NEAREST:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case KAN_RENDER_MIP_MAP_MODE_LINEAR:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;

    case KAN_RENDER_MIP_MAP_MODE_COUNT:
        KAN_ASSERT (false)
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }

    KAN_ASSERT (false)
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

static inline VkSamplerAddressMode to_vulkan_sampler_address_mode (enum kan_render_address_mode_t mode)
{
    switch (mode)
    {
    case KAN_RENDER_ADDRESS_MODE_REPEAT:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;

    case KAN_RENDER_ADDRESS_MODE_MIRRORED_REPEAT:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

    case KAN_RENDER_ADDRESS_MODE_CLAMP_TO_EDGE:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    case KAN_RENDER_ADDRESS_MODE_MIRRORED_CLAMP_TO_EDGE:
        return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;

    case KAN_RENDER_ADDRESS_MODE_CLAMP_TO_BORDER:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

    case KAN_RENDER_ADDRESS_MODE_COUNT:
        KAN_ASSERT (false)
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }

    KAN_ASSERT (false)
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkSampler render_backend_resolve_cached_sampler (struct render_backend_system_t *system,
                                                 struct kan_render_sampler_t *sampler)
{
    kan_atomic_int_lock (&system->sampler_cache_lock);
    struct render_backend_cached_sampler_t *last = NULL;
    struct render_backend_cached_sampler_t *cached = system->first_cached_sampler;

    _Static_assert (KAN_RENDER_FILTER_MODE_COUNT <= 2u, "Filter mode takes one bit of hash.");
    _Static_assert (KAN_RENDER_MIP_MAP_MODE_COUNT <= 2u, "Mip map mode takes one bit of hash.");
    _Static_assert (KAN_RENDER_ADDRESS_MODE_COUNT <= 8u, "Address mode takes 3 bits of hash.");
    _Static_assert (KAN_RENDER_COMPARE_OPERATION_COUNT <= 8u, "Depth compare operation takes 3 bits of hash.");
    _Static_assert (sizeof (kan_loop_size_t) * 8u >= 32u, "Loop size can contain 32 or more bits.");

    const kan_loop_size_t packed_description = sampler->mag_filter | (sampler->min_filter << 1u) |
                                               (sampler->mip_map_mode << 2u) | (sampler->address_mode_u << 3u) |
                                               (sampler->address_mode_v << 6u) | (sampler->address_mode_w << 9u) |
                                               (sampler->depth_compare_enabled << 12u) |
                                               (sampler->anisotropy_enabled << 13u) | (sampler->depth_compare << 14u);
    // We do not pack max anisotropy here.

    while (cached)
    {
        if (cached->packed_description_values == packed_description &&
            // Anisotropy values should be integer almost always, but we still use abs with high threshold just in case.
            fabs (cached->description.anisotropy_max - sampler->anisotropy_max) < 0.01f)
        {
            break;
        }

        last = cached;
        cached = cached->next;
    }

    if (!cached)
    {
        VkSamplerCreateInfo sampler_create_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .magFilter = to_vulkan_filter (sampler->mag_filter),
            .minFilter = to_vulkan_filter (sampler->min_filter),
            .mipmapMode = to_vulkan_sampler_mip_map_mode (sampler->mip_map_mode),
            .addressModeU = to_vulkan_sampler_address_mode (sampler->address_mode_u),
            .addressModeV = to_vulkan_sampler_address_mode (sampler->address_mode_v),
            .addressModeW = to_vulkan_sampler_address_mode (sampler->address_mode_w),
            .mipLodBias = 0.0f,
            .anisotropyEnable =
                system->selected_device_info->anisotropy_supported ? sampler->anisotropy_enabled : false,
            .maxAnisotropy = KAN_CLAMP (sampler->anisotropy_max, 1.0f, system->selected_device_info->anisotropy_max),
            .compareEnable = sampler->depth_compare_enabled,
            .compareOp = to_vulkan_compare_operation (sampler->depth_compare),
            .minLod = 0.0f,
            .maxLod = VK_LOD_CLAMP_NONE,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            .unnormalizedCoordinates = VK_FALSE,
        };

        VkSampler new_sampler = VK_NULL_HANDLE;
        if (vkCreateSampler (system->device, &sampler_create_info, VULKAN_ALLOCATION_CALLBACKS (system),
                             &new_sampler) != VK_SUCCESS)
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Unable to create cached sampler.")
            kan_atomic_int_unlock (&system->sampler_cache_lock);
            return VK_NULL_HANDLE;
        }

        cached = kan_allocate_batched (system->cached_samplers_allocation_group,
                                       sizeof (struct render_backend_cached_sampler_t));
        cached->next = NULL;
        cached->sampler = new_sampler;
        cached->packed_description_values = packed_description;
        cached->description = *sampler;

        if (last)
        {
            last->next = cached;
        }
        else
        {
            system->first_cached_sampler = cached;
        }
    }

    kan_atomic_int_unlock (&system->sampler_cache_lock);
    return cached->sampler;
}
