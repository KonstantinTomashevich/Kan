#include <kan/context/render_backend_implementation_interface.h>

static inline VkFilter to_vulkan_filter (enum kan_render_filter_mode_t filter)
{
    switch (filter)
    {
    case KAN_RENDER_FILTER_MODE_NEAREST:
        return VK_FILTER_NEAREST;

    case KAN_RENDER_FILTER_MODE_LINEAR:
        return VK_FILTER_LINEAR;
    }

    KAN_ASSERT (KAN_FALSE)
    return VK_FILTER_LINEAR;
}

static inline VkSamplerMipmapMode to_vulkan_sampler_mip_map_mode (enum kan_render_mip_map_mode_t mode)
{
    switch (mode)
    {
    case KAN_RENDER_MIP_MAP_MODE_NEAREST:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case KAN_RENDER_MIP_MAP_MODE_LINEAR:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }

    KAN_ASSERT (KAN_FALSE)
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
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
    }

    KAN_ASSERT (KAN_FALSE)
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkSampler render_backend_resolve_cached_sampler (struct render_backend_system_t *system,
                                                 struct kan_render_sampler_t *sampler)
{
    kan_atomic_int_lock (&system->sampler_cache_lock);
    struct render_backend_cached_sampler_t *last = NULL;
    struct render_backend_cached_sampler_t *cached = system->first_cached_sampler;

    while (cached)
    {
        if (cached->description.mag_filter == sampler->mag_filter &&
            cached->description.min_filter == sampler->min_filter &&
            cached->description.mip_map_mode == sampler->mip_map_mode &&
            cached->description.address_mode_u == sampler->address_mode_u &&
            cached->description.address_mode_v == sampler->address_mode_v &&
            cached->description.address_mode_w == sampler->address_mode_w)
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
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 1.0f,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_NEVER,
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
