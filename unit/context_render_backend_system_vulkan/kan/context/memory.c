#include <kan/context/render_backend_implementation_interface.h>

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
#    define VULKAN_ALLOCATION_CALLBACKS(SYSTEM) (&(SYSTEM)->memory_profiling.vulkan_allocation_callbacks)

static void *profiled_allocate (void *user_data, size_t size, size_t alignment, VkSystemAllocationScope scope)
{
    struct memory_profiling_t *profiling = (struct memory_profiling_t *) user_data;
    uint32_t real_alignment = KAN_MAX (alignment, _Alignof (uint32_t));
    uint32_t real_size = kan_apply_alignment (real_alignment + size, real_alignment);

    void *allocated_data = kan_allocate_general (profiling->driver_cpu_generic_group, real_size, real_alignment);

    void *accessible_data = ((uint8_t *) allocated_data) + real_alignment;
    uint32_t *meta_output = allocated_data;
    *meta_output = real_size;
    ++meta_output;

    while ((uint8_t *) meta_output < (uint8_t *) accessible_data)
    {
        *meta_output = 0u;
        ++meta_output;
    }

    return accessible_data;
}

static inline void *walk_from_accessible_to_allocated (void *accessible)
{
    uint32_t *meta = accessible;
    do
    {
        meta--;
    } while (*meta == 0u);

    return meta;
}

static void *profiled_reallocate (
    void *user_data, void *original, size_t size, size_t alignment, VkSystemAllocationScope scope)
{
    struct memory_profiling_t *profiling = (struct memory_profiling_t *) user_data;
    void *original_user_accessible_data = original;
    void *original_allocated_data = walk_from_accessible_to_allocated (original_user_accessible_data);

    const uint32_t original_real_size = *(uint32_t *) original_allocated_data;
    const uint32_t original_data_size =
        original_real_size - (((uint8_t *) original_user_accessible_data) - ((uint8_t *) original_allocated_data));

    void *new_data = profiled_allocate (user_data, size, alignment, scope);
    memcpy (new_data, original, KAN_MIN ((size_t) original_data_size, size));
    kan_free_general (profiling->driver_cpu_generic_group, original_allocated_data,
                      *(uint32_t *) original_allocated_data);
    return new_data;
}

static void profiled_free (void *user_data, void *pointer)
{
    if (pointer)
    {
        struct memory_profiling_t *profiling = (struct memory_profiling_t *) user_data;
        void *accessible = pointer;
        void *allocated = walk_from_accessible_to_allocated (accessible);
        kan_free_general (profiling->driver_cpu_generic_group, allocated, *(uint32_t *) allocated);
    }
}

static void notify_internal_cpu_allocation (void *user_data,
                                            size_t size,
                                            VkInternalAllocationType type,
                                            VkSystemAllocationScope scope)
{
    struct memory_profiling_t *profiling = (struct memory_profiling_t *) user_data;
    kan_allocation_group_allocate (profiling->driver_cpu_internal_group, size);
}

static void notify_internal_cpu_free (void *user_data,
                                      size_t size,
                                      VkInternalAllocationType type,
                                      VkSystemAllocationScope scope)
{
    struct memory_profiling_t *profiling = (struct memory_profiling_t *) user_data;
    kan_allocation_group_free (profiling->driver_cpu_internal_group, size);
}

static void notify_device_allocation (
    VmaAllocator allocator, uint32_t memory_type, VkDeviceMemory memory, VkDeviceSize size, void *user_data)
{
    struct memory_profiling_t *profiling = (struct memory_profiling_t *) user_data;
    kan_allocation_group_allocate (profiling->gpu_unmarked_group, size);
}

static void notify_device_free (
    VmaAllocator allocator, uint32_t memory_type, VkDeviceMemory memory, VkDeviceSize size, void *user_data)
{
    struct memory_profiling_t *profiling = (struct memory_profiling_t *) user_data;
    kan_allocation_group_free (profiling->gpu_unmarked_group, size);
}

void render_backend_memory_profiling_init (struct render_backend_system_t *system)
{
    system->memory_profiling.driver_cpu_group =
        kan_allocation_group_get_child (system->main_allocation_group, "driver_cpu");
    system->memory_profiling.driver_cpu_generic_group =
        kan_allocation_group_get_child (system->memory_profiling.driver_cpu_group, "generic");
    system->memory_profiling.driver_cpu_internal_group =
        kan_allocation_group_get_child (system->memory_profiling.driver_cpu_group, "internal");

    system->memory_profiling.gpu_group = kan_allocation_group_get_child (system->main_allocation_group, "gpu");
    system->memory_profiling.gpu_unmarked_group =
        kan_allocation_group_get_child (system->memory_profiling.gpu_group, "unmarked");
    system->memory_profiling.gpu_buffer_group =
        kan_allocation_group_get_child (system->memory_profiling.gpu_group, "buffer");
    system->memory_profiling.gpu_buffer_attribute_group =
        kan_allocation_group_get_child (system->memory_profiling.gpu_buffer_group, "attribute");
    system->memory_profiling.gpu_buffer_index_group =
        kan_allocation_group_get_child (system->memory_profiling.gpu_buffer_group, "index");
    system->memory_profiling.gpu_buffer_uniform_group =
        kan_allocation_group_get_child (system->memory_profiling.gpu_buffer_group, "uniform");
    system->memory_profiling.gpu_buffer_storage_group =
        kan_allocation_group_get_child (system->memory_profiling.gpu_buffer_group, "storage");
    system->memory_profiling.gpu_image_group =
        kan_allocation_group_get_child (system->memory_profiling.gpu_group, "image");

    system->memory_profiling.vulkan_allocation_callbacks = (VkAllocationCallbacks) {
        .pUserData = &system->memory_profiling,
        .pfnAllocation = profiled_allocate,
        .pfnReallocation = profiled_reallocate,
        .pfnFree = profiled_free,
        .pfnInternalAllocation = notify_internal_cpu_allocation,
        .pfnInternalFree = notify_internal_cpu_free,
    };

    system->memory_profiling.vma_device_memory_callbacks = (VmaDeviceMemoryCallbacks) {
        .pfnAllocate = notify_device_allocation,
        .pfnFree = notify_device_free,
        .pUserData = &system->memory_profiling,
    };
}

#else
#    define VULKAN_ALLOCATION_CALLBACKS(SYSTEM) NULL
#endif
