#include <string.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/context/render_backend_system.h>
#include <kan/context/vulkan_memory_allocator.h>
#include <kan/memory/allocation.h>
#include <kan/platform/application.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (render_backend_system_vulkan);

#define SURFACE_COLOR_FORMAT VK_FORMAT_B8G8R8A8_SRGB
#define SURFACE_COLOR_SPACE VK_COLOR_SPACE_SRGB_NONLINEAR_KHR

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
struct memory_profiling_t
{
    kan_allocation_group_t driver_cpu_group;
    kan_allocation_group_t driver_cpu_generic_group;
    kan_allocation_group_t driver_cpu_internal_group;

    kan_allocation_group_t gpu_group;
    kan_allocation_group_t gpu_unmarked_group;
    kan_allocation_group_t gpu_buffer_group;
    kan_allocation_group_t gpu_buffer_attribute_group;
    kan_allocation_group_t gpu_buffer_index_group;
    kan_allocation_group_t gpu_buffer_uniform_group;
    kan_allocation_group_t gpu_buffer_storage_group;
    kan_allocation_group_t gpu_image_group;

    VkAllocationCallbacks vulkan_allocation_callbacks;
    VmaDeviceMemoryCallbacks vma_device_memory_callbacks;
};
#endif

enum surface_render_state_t
{
    SURFACE_RENDER_STATE_RECEIVED_NO_OUTPUT = 0u,
    SURFACE_RENDER_STATE_RECEIVED_DATA_FROM_FRAME_BUFFER,
    SURFACE_RENDER_STATE_RECEIVED_DATA_FROM_BLIT,
};

struct surface_blit_request_t
{
    struct surface_blit_request_t *next;
    struct render_backend_image_t *image;
    struct kan_render_integer_region_t image_region;
    struct kan_render_integer_region_t surface_region;
};

struct surface_frame_buffer_attachment_t
{
    struct surface_frame_buffer_attachment_t *next;
    struct render_backend_frame_buffer_t *frame_buffer;
};

struct render_backend_surface_t
{
    struct render_backend_surface_t *previous;
    struct render_backend_surface_t *next;
    struct render_backend_system_t *system;

    kan_interned_string_t tracking_name;
    VkSurfaceKHR surface;

    VkSwapchainKHR swap_chain;
    uint32_t images_count;
    VkImage *images;
    VkImageView *image_views;

    VkSemaphore image_available_semaphores[KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT];
    uint32_t acquired_image_frame;
    uint32_t acquired_image_index;
    kan_bool_t needs_recreation;

    enum surface_render_state_t render_state;
    struct kan_atomic_int_t blit_request_lock;
    struct surface_blit_request_t *first_blit_request;
    struct surface_frame_buffer_attachment_t *first_frame_buffer_attachment;

    uint32_t swap_chain_creation_window_width;
    uint32_t swap_chain_creation_window_height;

    kan_application_system_window_handle_t window_handle;
    kan_application_system_window_resource_id_t resource_id;
};

/// \details We have fully separate command state with pools and buffers for every frame in flight index,
///          because it allows us to reset full pools instead of separate buffer which should be better from performance
///          point of view and is advised on docs.vulkan.org.
struct render_backend_command_state_t
{
    VkCommandPool graphics_command_pool;
    VkCommandBuffer primary_graphics_command_buffer;

    // TODO: Secondary graphic buffers.

    VkCommandPool transfer_command_pool;
    VkCommandBuffer primary_transfer_command_buffer;
};

struct scheduled_buffer_unmap_flush_transfer_t
{
    struct scheduled_buffer_unmap_flush_transfer_t *next;
    struct render_backend_buffer_t *source_buffer;
    struct render_backend_buffer_t *target_buffer;
    uint64_t source_offset;
    uint64_t target_offset;
    uint64_t size;
};

struct scheduled_buffer_unmap_flush_t
{
    struct scheduled_buffer_unmap_flush_t *next;
    struct render_backend_buffer_t *buffer;
    uint64_t offset;
    uint64_t size;
};

struct scheduled_image_upload_t
{
    struct scheduled_image_upload_t *next;
    struct render_backend_image_t *image;
    uint64_t mip;
    struct render_backend_buffer_t *staging_buffer;
    uint64_t staging_buffer_offset;
};

struct scheduled_frame_buffer_create_t
{
    struct scheduled_frame_buffer_create_t *next;
    struct render_backend_frame_buffer_t *frame_buffer;
};

struct scheduled_image_mip_generation_t
{
    struct scheduled_image_mip_generation_t *next;
    struct render_backend_image_t *image;
    uint64_t first;
    uint64_t last;
};

struct scheduled_frame_buffer_destroy_t
{
    struct scheduled_frame_buffer_destroy_t *next;
    struct render_backend_frame_buffer_t *frame_buffer;
};

struct scheduled_detached_frame_buffer_destroy_t
{
    struct scheduled_detached_frame_buffer_destroy_t *next;
    VkFramebuffer detached_frame_buffer;
};

struct scheduled_pass_destroy_t
{
    struct scheduled_pass_destroy_t *next;
    struct render_backend_pass_t *pass;
};

struct scheduled_buffer_destroy_t
{
    struct scheduled_buffer_destroy_t *next;
    struct render_backend_buffer_t *buffer;
};

struct scheduled_frame_lifetime_allocator_destroy_t
{
    struct scheduled_frame_lifetime_allocator_destroy_t *next;
    struct render_backend_frame_lifetime_allocator_t *frame_lifetime_allocator;
};

struct scheduled_detached_image_view_destroy_t
{
    struct scheduled_detached_image_view_destroy_t *next;
    VkImageView detached_image_view;
};

struct scheduled_image_destroy_t
{
    struct scheduled_image_destroy_t *next;
    struct render_backend_image_t *image;
};

struct scheduled_detached_image_destroy_t
{
    struct scheduled_detached_image_destroy_t *next;
    VkImage detached_image;
    VmaAllocation detached_allocation;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    uint64_t gpu_size;
    kan_allocation_group_t gpu_allocation_group;
#endif
};

struct render_backend_schedule_state_t
{
    struct kan_stack_group_allocator_t item_allocator;

    /// \brief Common lock for submitting scheduled operations.
    struct kan_atomic_int_t schedule_lock;

    struct scheduled_buffer_unmap_flush_transfer_t *first_scheduled_buffer_unmap_flush_transfer;
    struct scheduled_buffer_unmap_flush_t *first_scheduled_buffer_unmap_flush;
    struct scheduled_image_upload_t *first_scheduled_image_upload;
    struct scheduled_frame_buffer_create_t *first_scheduled_frame_buffer_create;
    struct scheduled_image_mip_generation_t *first_scheduled_image_mip_generation;

    struct scheduled_frame_buffer_destroy_t *first_scheduled_frame_buffer_destroy;
    struct scheduled_detached_frame_buffer_destroy_t *first_scheduled_detached_frame_buffer_destroy;
    struct scheduled_pass_destroy_t *first_scheduled_pass_destroy;
    struct scheduled_buffer_destroy_t *first_scheduled_buffer_destroy;
    struct scheduled_frame_lifetime_allocator_destroy_t *first_scheduled_frame_lifetime_allocator_destroy;
    struct scheduled_detached_image_view_destroy_t *first_scheduled_detached_image_view_destroy;
    struct scheduled_image_destroy_t *first_scheduled_image_destroy;
    struct scheduled_detached_image_destroy_t *first_scheduled_detached_image_destroy;
};

struct render_backend_frame_buffer_attachment_t
{
    enum kan_render_frame_buffer_attachment_type_t type;
    union
    {
        struct render_backend_image_t *image;
        struct render_backend_surface_t *surface;
    };
};

struct render_backend_frame_buffer_t
{
    struct render_backend_frame_buffer_t *next;
    struct render_backend_frame_buffer_t *previous;
    struct render_backend_system_t *system;

    VkFramebuffer instance;
    uint64_t instance_array_size;

    /// \details If there is a surface attachment, we need to create a separate frame buffer for each surface image.
    ///          Therefore, in that case frame_buffer_array is used instead of frame_buffer field.
    VkFramebuffer *instance_array;

    /// \brief Image views for attached images. VK_NULL_HANDLE is inserted in place of surface attachment if any.
    VkImageView *image_views;

    struct render_backend_pass_t *pass;
    uint64_t attachments_count;
    struct render_backend_frame_buffer_attachment_t *attachments;
    kan_interned_string_t tracking_name;
};

struct render_backend_pass_t
{
    struct render_backend_pass_t *next;
    struct render_backend_pass_t *previous;
    struct render_backend_system_t *system;
    VkRenderPass pass;
};

enum render_backend_buffer_family_t
{
    RENDER_BACKEND_BUFFER_FAMILY_RESOURCE = 0u,
    RENDER_BACKEND_BUFFER_FAMILY_STAGING,
    RENDER_BACKEND_BUFFER_FAMILY_FRAME_LIFETIME_ALLOCATOR,
};

struct render_backend_buffer_t
{
    struct render_backend_buffer_t *next;
    struct render_backend_buffer_t *previous;
    struct render_backend_system_t *system;

    VkBuffer buffer;
    VmaAllocation allocation;
    enum render_backend_buffer_family_t family;
    enum kan_render_buffer_type_t type;
    uint64_t full_size;
    kan_interned_string_t tracking_name;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    kan_allocation_group_t device_allocation_group;
#endif
};

#define CHUNK_FREE_MARKER UINT64_MAX

struct render_backend_frame_lifetime_allocator_chunk_t
{
    struct render_backend_frame_lifetime_allocator_chunk_t *next;
    struct render_backend_frame_lifetime_allocator_chunk_t *previous;
    struct render_backend_frame_lifetime_allocator_chunk_t *next_free;

    uint64_t offset;
    uint64_t size;
    uint64_t occupied_by_frame;
};

struct render_backend_frame_lifetime_allocator_page_t
{
    struct render_backend_frame_lifetime_allocator_page_t *next;
    struct render_backend_buffer_t *buffer;

    struct render_backend_frame_lifetime_allocator_chunk_t *first_chunk;
    struct render_backend_frame_lifetime_allocator_chunk_t *first_free_chunk;
};

struct render_backend_frame_lifetime_allocator_t
{
    struct render_backend_frame_lifetime_allocator_t *next;
    struct render_backend_frame_lifetime_allocator_t *previous;
    struct render_backend_system_t *system;

    struct render_backend_frame_lifetime_allocator_page_t *first_page;
    struct render_backend_frame_lifetime_allocator_page_t *last_page;

    /// \brief Common lock for allocation operations.
    struct kan_atomic_int_t allocation_lock;

    enum render_backend_buffer_family_t buffer_family;
    enum kan_render_buffer_type_t buffer_type;
    uint64_t page_size;
    kan_interned_string_t tracking_name;
};

struct render_backend_frame_lifetime_allocator_allocation_t
{
    struct render_backend_buffer_t *buffer;
    uint64_t offset;
};

#define STAGING_BUFFER_ALLOCATION_ALIGNMENT _Alignof (float)

struct image_frame_buffer_attachment_t
{
    struct image_frame_buffer_attachment_t *next;
    struct render_backend_frame_buffer_t *frame_buffer;
};

struct render_backend_image_t
{
    struct render_backend_image_t *next;
    struct render_backend_image_t *previous;
    struct render_backend_system_t *system;

    VkImage image;
    VmaAllocation allocation;

    struct kan_render_image_description_t description;

    /// \brief Flag used by blit-to-present routine in order to avoid
    ///        incorrectly changing layout of the same image twice.
    kan_bool_t switched_to_transfer_source;

    struct image_frame_buffer_attachment_t *first_frame_buffer_attachment;

    // TODO: Attached pipeline instances (for render target resize).

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    kan_allocation_group_t device_allocation_group;
#endif
};

struct render_backend_system_t
{
    kan_context_handle_t context;

    VkInstance instance;
    VkDevice device;
    enum kan_render_device_memory_type_t device_memory_type;
    VkPhysicalDevice physical_device;

    VkQueue graphics_queue;
    VkQueue transfer_queue;

    VmaAllocator gpu_memory_allocator;

    uint32_t device_graphics_queue_family_index;
    uint32_t device_transfer_queue_family_index;

    kan_bool_t frame_started;
    uint32_t current_frame_in_flight_index;

    VkSemaphore transfer_finished_semaphores[KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT];
    VkSemaphore render_finished_semaphores[KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT];
    VkFence in_flight_fences[KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT];

    struct render_backend_command_state_t command_states[KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT];
    struct render_backend_schedule_state_t schedule_states[KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT];

    VkFormat device_depth_image_format;
    kan_bool_t device_depth_image_has_stencil;

    /// \brief Lock for safe resource management (resource creation, memory management) in multithreaded environment.
    /// \details Surfaces are the exemption from this rule as they're always managed from application system thread.
    ///          Everything that is done from kan_render_backend_system_next_frame is an exemption from this rule as
    ///          strict frame ordering must prevent any calls that are simultaneous with next frame call.
    struct kan_atomic_int_t resource_management_lock;

    struct render_backend_surface_t *first_surface;
    struct render_backend_frame_buffer_t *first_frame_buffer;
    struct render_backend_pass_t *first_pass;
    struct render_backend_buffer_t *first_buffer;
    struct render_backend_frame_lifetime_allocator_t *first_frame_lifetime_allocator;
    struct render_backend_image_t *first_image;

    /// \details Still listed in frame lifetime allocator list above, but referenced here too for usability.
    struct render_backend_frame_lifetime_allocator_t *staging_frame_lifetime_allocator;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
    kan_bool_t has_validation_layer;
    VkDebugUtilsMessengerEXT debug_messenger;
#endif

    VmaVulkanFunctions gpu_memory_allocator_functions;
    kan_allocation_group_t main_allocation_group;
    kan_allocation_group_t utility_allocation_group;
    kan_allocation_group_t surface_wrapper_allocation_group;
    kan_allocation_group_t schedule_allocation_group;
    kan_allocation_group_t frame_buffer_wrapper_allocation_group;
    kan_allocation_group_t pass_wrapper_allocation_group;
    kan_allocation_group_t buffer_wrapper_allocation_group;
    kan_allocation_group_t frame_lifetime_wrapper_allocation_group;
    kan_allocation_group_t image_wrapper_allocation_group;

    struct kan_render_supported_devices_t *supported_devices;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    struct memory_profiling_t memory_profiling;
#endif

    kan_bool_t render_enabled;
    kan_bool_t prefer_vsync;

    kan_interned_string_t application_info_name;
    uint64_t version_major;
    uint64_t version_minor;
    uint64_t version_patch;

    kan_interned_string_t interned_temporary_staging_buffer;
};

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
#    define VULKAN_ALLOCATION_CALLBACKS(SYSTEM) (&(SYSTEM)->memory_profiling.vulkan_allocation_callbacks)

static void *profiled_allocate (void *user_data, size_t size, size_t alignment, VkSystemAllocationScope scope)
{
    struct memory_profiling_t *profiling = (struct memory_profiling_t *) user_data;
    uint64_t real_alignment = KAN_MAX (alignment, _Alignof (uint64_t));
    uint64_t real_size = kan_apply_alignment (real_alignment + size, real_alignment);

    void *allocated_data = kan_allocate_general (profiling->driver_cpu_generic_group, real_size, real_alignment);

    void *accessible_data = ((uint8_t *) allocated_data) + real_alignment;
    uint64_t *meta_output = allocated_data;
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
    uint64_t *meta = accessible;
    do
    {
        meta--;
    } while (*meta == 0u);

    return meta;
}

static inline void profiled_free_internal (struct memory_profiling_t *profiling, void *allocated, void *accessible)
{
    uint64_t size_to_free = (*(uint64_t *) allocated) + (((uint8_t *) accessible) - ((uint8_t *) allocated));
    kan_free_general (profiling->driver_cpu_generic_group, allocated, size_to_free);
}

static void *profiled_reallocate (
    void *user_data, void *original, size_t size, size_t alignment, VkSystemAllocationScope scope)
{
    struct memory_profiling_t *profiling = (struct memory_profiling_t *) user_data;
    void *original_user_accessible_data = original;
    void *original_allocated_data = walk_from_accessible_to_allocated (original_user_accessible_data);

    const uint64_t original_real_size = *(uint64_t *) original_allocated_data;
    const uint64_t original_data_size =
        original_real_size - (((uint8_t *) original_user_accessible_data) - ((uint8_t *) original_allocated_data));

    void *new_data = profiled_allocate (user_data, size, alignment, scope);
    memcpy (new_data, original, KAN_MIN ((size_t) original_data_size, size));
    profiled_free_internal (profiling, original_allocated_data, original_user_accessible_data);
    return new_data;
}

static void profiled_free (void *user_data, void *pointer)
{
    if (pointer)
    {
        struct memory_profiling_t *profiling = (struct memory_profiling_t *) user_data;
        void *accessible = pointer;
        void *allocated = walk_from_accessible_to_allocated (accessible);
        profiled_free_internal (profiling, allocated, accessible);
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

static void memory_profiling_init (struct render_backend_system_t *system)
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

static inline void transfer_memory_between_groups (uint64_t amount,
                                                   kan_allocation_group_t from,
                                                   kan_allocation_group_t to)
{
    kan_allocation_group_free (from, amount);
    kan_allocation_group_allocate (to, amount);
}
#else
#    define VULKAN_ALLOCATION_CALLBACKS(SYSTEM) NULL
#endif

static inline struct render_backend_schedule_state_t *render_backend_system_get_schedule_for_memory (
    struct render_backend_system_t *system)
{
    // Always schedule into the current frame, even if it is not yet started.
    return &system->schedule_states[system->current_frame_in_flight_index];
}

static inline struct render_backend_schedule_state_t *render_backend_system_get_schedule_for_destroy (
    struct render_backend_system_t *system)
{
    uint64_t schedule_index = system->current_frame_in_flight_index;
    if (!system->frame_started)
    {
        // If frame is not started, then we can't schedule destroy for current frame as destroy is done when frame
        // starts and therefore we have a risk of destroying used resources. Technically, if frame is noy yet started,
        // then resource to be destroyed will not be used in it, therefore the most safe and logical solution is to
        // add destroy into previous frame schedule.
        schedule_index =
            schedule_index == 0u ? KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT - 1u : schedule_index - 1u;
    }

    return &system->schedule_states[schedule_index];
}

static inline struct render_backend_frame_buffer_t *render_backend_system_create_frame_buffer (
    struct render_backend_system_t *system, struct kan_render_frame_buffer_description_t *description)
{
    struct render_backend_frame_buffer_t *buffer = kan_allocate_batched (system->frame_buffer_wrapper_allocation_group,
                                                                         sizeof (struct render_backend_frame_buffer_t));

    buffer->next = system->first_frame_buffer;
    buffer->previous = NULL;
    buffer->system = system;

    if (system->first_frame_buffer)
    {
        system->first_frame_buffer->previous = buffer;
    }

    system->first_frame_buffer = buffer;
    buffer->instance = VK_NULL_HANDLE;
    buffer->instance_array_size = 0u;
    buffer->instance_array = NULL;
    buffer->image_views = NULL;

    buffer->pass = (struct render_backend_pass_t *) description->associated_pass;
    buffer->tracking_name = description->tracking_name;

    buffer->attachments_count = description->attachment_count;
    buffer->attachments =
        kan_allocate_general (system->frame_buffer_wrapper_allocation_group,
                              sizeof (struct render_backend_frame_buffer_attachment_t) * buffer->attachments_count,
                              _Alignof (struct render_backend_frame_buffer_attachment_t));

    for (uint64_t index = 0u; index < buffer->attachments_count; ++index)
    {
        struct kan_render_frame_buffer_attachment_description_t *source = &description->attachments[index];
        struct render_backend_frame_buffer_attachment_t *target = &buffer->attachments[index];
        target->type = source->type;

        switch (source->type)
        {
        case KAN_FRAME_BUFFER_ATTACHMENT_IMAGE:
        {
            target->image = (struct render_backend_image_t *) source->image;
            struct image_frame_buffer_attachment_t *attachment = kan_allocate_batched (
                system->image_wrapper_allocation_group, sizeof (struct image_frame_buffer_attachment_t));

            attachment->next = target->image->first_frame_buffer_attachment;
            target->image->first_frame_buffer_attachment = attachment;
            attachment->frame_buffer = buffer;
            break;
        }

        case KAN_FRAME_BUFFER_ATTACHMENT_SURFACE:
        {
            target->surface = (struct render_backend_surface_t *) source->surface;
            struct surface_frame_buffer_attachment_t *attachment = kan_allocate_batched (
                system->surface_wrapper_allocation_group, sizeof (struct surface_frame_buffer_attachment_t));

            attachment->next = target->surface->first_frame_buffer_attachment;
            target->surface->first_frame_buffer_attachment = attachment;
            attachment->frame_buffer = buffer;
            break;
        }
        }
    }

    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_memory (system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_frame_buffer_create_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_frame_buffer_create_t);

    // We do not need to preserve order as frame buffers cannot depend one on another.
    item->next = schedule->first_scheduled_frame_buffer_create;
    schedule->first_scheduled_frame_buffer_create = item;
    item->frame_buffer = buffer;
    kan_atomic_int_unlock (&schedule->schedule_lock);
    return buffer;
}

static inline void render_backend_frame_buffer_destroy_resources (struct render_backend_system_t *system,
                                                                  struct render_backend_frame_buffer_t *frame_buffer)
{
    if (frame_buffer->instance != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer (system->device, frame_buffer->instance, VULKAN_ALLOCATION_CALLBACKS (system));
        frame_buffer->instance = VK_NULL_HANDLE;
    }

    if (frame_buffer->instance_array)
    {
        for (uint64_t index = 0u; index < frame_buffer->instance_array_size; ++index)
        {
            vkDestroyFramebuffer (system->device, frame_buffer->instance_array[index],
                                  VULKAN_ALLOCATION_CALLBACKS (system));
        }

        kan_free_general (system->frame_buffer_wrapper_allocation_group, frame_buffer->instance_array,
                          sizeof (VkFramebuffer) * frame_buffer->instance_array_size);
        frame_buffer->instance_array_size = 0u;
        frame_buffer->instance_array = NULL;
    }

    if (frame_buffer->image_views)
    {
        for (uint64_t index = 0u; index < frame_buffer->attachments_count; ++index)
        {
            if (frame_buffer->image_views[index] != VK_NULL_HANDLE)
            {
                vkDestroyImageView (system->device, frame_buffer->image_views[index],
                                    VULKAN_ALLOCATION_CALLBACKS (system));
            }
        }

        kan_free_general (system->frame_buffer_wrapper_allocation_group, frame_buffer->image_views,
                          sizeof (VkImageView) * frame_buffer->attachments_count);
        frame_buffer->image_views = NULL;
    }
}

static inline void render_backend_frame_buffer_schedule_resource_destroy (
    struct render_backend_system_t *system,
    struct render_backend_frame_buffer_t *frame_buffer,
    struct render_backend_schedule_state_t *schedule)
{
    if (frame_buffer->instance != VK_NULL_HANDLE)
    {
        struct scheduled_detached_frame_buffer_destroy_t *frame_buffer_destroy =
            KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator,
                                                      struct scheduled_detached_frame_buffer_destroy_t);
        frame_buffer_destroy->next = schedule->first_scheduled_detached_frame_buffer_destroy;
        schedule->first_scheduled_detached_frame_buffer_destroy = frame_buffer_destroy;
        frame_buffer_destroy->detached_frame_buffer = frame_buffer->instance;
        frame_buffer->instance = VK_NULL_HANDLE;
    }

    if (frame_buffer->instance_array)
    {
        for (uint64_t index = 0u; index < frame_buffer->instance_array_size; ++index)
        {
            struct scheduled_detached_frame_buffer_destroy_t *frame_buffer_destroy =
                KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator,
                                                          struct scheduled_detached_frame_buffer_destroy_t);
            frame_buffer_destroy->next = schedule->first_scheduled_detached_frame_buffer_destroy;
            schedule->first_scheduled_detached_frame_buffer_destroy = frame_buffer_destroy;
            frame_buffer_destroy->detached_frame_buffer = frame_buffer->instance_array[index];
        }

        kan_free_general (system->frame_buffer_wrapper_allocation_group, frame_buffer->instance_array,
                          sizeof (VkFramebuffer) * frame_buffer->instance_array_size);
        frame_buffer->instance_array_size = 0u;
        frame_buffer->instance_array = NULL;
    }

    if (frame_buffer->image_views)
    {
        for (uint64_t index = 0u; index < frame_buffer->attachments_count; ++index)
        {
            if (frame_buffer->image_views[index] != VK_NULL_HANDLE)
            {
                struct scheduled_detached_image_view_destroy_t *image_view_destroy =
                    KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator,
                                                              struct scheduled_detached_image_view_destroy_t);
                image_view_destroy->next = schedule->first_scheduled_detached_image_view_destroy;
                schedule->first_scheduled_detached_image_view_destroy = image_view_destroy;
                image_view_destroy->detached_image_view = frame_buffer->image_views[index];
            }
        }

        kan_free_general (system->frame_buffer_wrapper_allocation_group, frame_buffer->image_views,
                          sizeof (VkImageView) * frame_buffer->attachments_count);
        frame_buffer->image_views = NULL;
    }
}

static inline void render_backend_system_destroy_frame_buffer (struct render_backend_system_t *system,
                                                               struct render_backend_frame_buffer_t *frame_buffer,
                                                               kan_bool_t remove_from_list)
{
    if (remove_from_list)
    {
        if (frame_buffer->next)
        {
            frame_buffer->next->previous = frame_buffer->previous;
        }

        if (frame_buffer->previous)
        {
            frame_buffer->previous->next = frame_buffer->next;
        }
        else
        {
            KAN_ASSERT (system->first_frame_buffer = frame_buffer)
            system->first_frame_buffer = frame_buffer->next;
        }
    }

    // Detach from surface and images if attachment is created.
    for (uint64_t index = 0u; index < frame_buffer->attachments_count; ++index)
    {
        struct render_backend_frame_buffer_attachment_t *attachment = &frame_buffer->attachments[index];
        switch (attachment->type)
        {
        case KAN_FRAME_BUFFER_ATTACHMENT_IMAGE:
        {
            struct image_frame_buffer_attachment_t *previous = NULL;
            struct image_frame_buffer_attachment_t *current = attachment->image->first_frame_buffer_attachment;

            while (current)
            {
                if (current->frame_buffer == frame_buffer)
                {
                    if (previous)
                    {
                        previous->next = current->next;
                    }
                    else
                    {
                        KAN_ASSERT (attachment->image->first_frame_buffer_attachment == current)
                        attachment->image->first_frame_buffer_attachment = current->next;
                    }

                    kan_free_batched (system->image_wrapper_allocation_group, current);
                    break;
                }

                previous = current;
                current = current->next;
            }

            break;
        }

        case KAN_FRAME_BUFFER_ATTACHMENT_SURFACE:
        {
            struct surface_frame_buffer_attachment_t *previous = NULL;
            struct surface_frame_buffer_attachment_t *current = attachment->surface->first_frame_buffer_attachment;

            while (current)
            {
                if (current->frame_buffer == frame_buffer)
                {
                    if (previous)
                    {
                        previous->next = current->next;
                    }
                    else
                    {
                        KAN_ASSERT (attachment->surface->first_frame_buffer_attachment == current)
                        attachment->surface->first_frame_buffer_attachment = current->next;
                    }

                    kan_free_batched (system->surface_wrapper_allocation_group, current);
                    break;
                }

                previous = current;
                current = current->next;
            }

            break;
        }
        }
    }

    render_backend_frame_buffer_destroy_resources (system, frame_buffer);
    kan_free_general (system->frame_buffer_wrapper_allocation_group, frame_buffer->attachments,
                      sizeof (struct render_backend_frame_buffer_attachment_t) * frame_buffer->attachments_count);
    kan_free_batched (system->frame_buffer_wrapper_allocation_group, frame_buffer);
}

static inline struct render_backend_pass_t *render_backend_system_create_pass (
    struct render_backend_system_t *system, struct kan_render_pass_description_t *description)
{
    VkAttachmentDescription *attachment_descriptions = kan_allocate_general (
        system->utility_allocation_group, sizeof (VkAttachmentDescription) * description->attachments_count,
        _Alignof (VkAttachmentDescription));

    VkAttachmentReference *color_attachments = kan_allocate_general (
        system->utility_allocation_group, sizeof (VkAttachmentReference) * description->attachments_count,
        _Alignof (VkAttachmentReference));

    VkAttachmentReference *next_color_attachment = color_attachments;
    uint64_t color_attachments_count = 0u;
    kan_bool_t has_depth_attachment = KAN_FALSE;
    VkAttachmentReference depth_attachment;

    for (uint64_t index = 0u; index < description->attachments_count; ++index)
    {
        struct kan_render_pass_attachment_t *attachment = &description->attachments[index];
        struct VkAttachmentDescription *vulkan_attachment = &attachment_descriptions[index];
        vulkan_attachment->flags = 0u;

        switch (attachment->type)
        {
        case KAN_RENDER_PASS_ATTACHMENT_COLOR:
            ++color_attachments_count;

            switch (attachment->color_format)
            {
            case KAN_RENDER_COLOR_FORMAT_RGBA32_SRGB:
                vulkan_attachment->format = VK_FORMAT_R8G8B8A8_SRGB;
                break;

            case KAN_RENDER_COLOR_FORMAT_RGBA128_SFLOAT:
                vulkan_attachment->format = VK_FORMAT_R32G32B32A32_SFLOAT;
                break;

            case KAN_RENDER_COLOR_FORMAT_SURFACE:
                vulkan_attachment->format = SURFACE_COLOR_FORMAT;
                break;
            }

            switch (attachment->samples)
            {
            case 1u:
                vulkan_attachment->samples = VK_SAMPLE_COUNT_1_BIT;
                break;

            case 2u:
                vulkan_attachment->samples = VK_SAMPLE_COUNT_2_BIT;
                break;

            case 4u:
                vulkan_attachment->samples = VK_SAMPLE_COUNT_4_BIT;
                break;

            case 8u:
                vulkan_attachment->samples = VK_SAMPLE_COUNT_8_BIT;
                break;

            case 16u:
                vulkan_attachment->samples = VK_SAMPLE_COUNT_16_BIT;
                break;

            case 32u:
                vulkan_attachment->samples = VK_SAMPLE_COUNT_32_BIT;
                break;

            case 64u:
                vulkan_attachment->samples = VK_SAMPLE_COUNT_64_BIT;
                break;

            default:
                // Count of samples is not power of two.
                KAN_ASSERT (KAN_FALSE)
                break;
            }

            vulkan_attachment->finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            next_color_attachment->attachment = (uint32_t) index;
            next_color_attachment->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            ++next_color_attachment;
            break;

        case KAN_RENDER_PASS_ATTACHMENT_DEPTH_STENCIL:
            // There should be not more than 1 depth attachment per pass.
            KAN_ASSERT (!has_depth_attachment)

            has_depth_attachment = KAN_TRUE;
            vulkan_attachment->format = system->device_depth_image_format;
            KAN_ASSERT (attachment->samples == 1u)
            vulkan_attachment->samples = VK_SAMPLE_COUNT_1_BIT;
            vulkan_attachment->finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            depth_attachment.attachment = (uint32_t) index;
            depth_attachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;
        }

        switch (attachment->load_operation)
        {
        case KAN_RENDER_LOAD_OPERATION_ANY:
            vulkan_attachment->loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            break;

        case KAN_RENDER_LOAD_OPERATION_LOAD:
            vulkan_attachment->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            break;

        case KAN_RENDER_LOAD_OPERATION_CLEAR:
            vulkan_attachment->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            break;
        }

        switch (attachment->store_operation)
        {
        case KAN_RENDER_STORE_OPERATION_ANY:
            vulkan_attachment->storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            break;

        case KAN_RENDER_STORE_OPERATION_STORE:
            vulkan_attachment->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            break;

        case KAN_RENDER_STORE_OPERATION_NONE:
            vulkan_attachment->storeOp = VK_ATTACHMENT_STORE_OP_NONE;
            break;
        }

        vulkan_attachment->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        vulkan_attachment->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        vulkan_attachment->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VkSubpassDescription sub_pass_description = {
        .flags = 0u,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0u,
        .pInputAttachments = NULL,
        .colorAttachmentCount = (uint32_t) color_attachments_count,
        .pColorAttachments = color_attachments,
        .pResolveAttachments = NULL,
        .pDepthStencilAttachment = has_depth_attachment ? &depth_attachment : NULL,
        .preserveAttachmentCount = 0u,
        .pPreserveAttachments = NULL,
    };

    uint32_t destination_access_mask = 0u;
    if (color_attachments_count > 0u)
    {
        destination_access_mask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    if (has_depth_attachment)
    {
        destination_access_mask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    VkSubpassDependency sub_pass_dependencies[] = {
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0u,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0u,
            .dstAccessMask = destination_access_mask,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
    };

    VkRenderPassCreateInfo render_pass_description = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .attachmentCount = (uint32_t) description->attachments_count,
        .pAttachments = attachment_descriptions,
        .subpassCount = 1u,
        .pSubpasses = &sub_pass_description,
        .dependencyCount = 1u,
        .pDependencies = sub_pass_dependencies,
    };

    VkRenderPass render_pass;
    VkResult result = vkCreateRenderPass (system->device, &render_pass_description,
                                          VULKAN_ALLOCATION_CALLBACKS (system), &render_pass);

    kan_free_general (system->utility_allocation_group, attachment_descriptions,
                      sizeof (VkAttachmentDescription) * description->attachments_count);
    kan_free_general (system->utility_allocation_group, color_attachments,
                      sizeof (VkAttachmentReference) * description->attachments_count);

    if (result != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to create render pass \"%s\".",
                 description->tracking_name)
        return NULL;
    }

    struct render_backend_pass_t *pass =
        kan_allocate_batched (system->pass_wrapper_allocation_group, sizeof (struct render_backend_pass_t));

    pass->next = system->first_pass;
    pass->previous = NULL;
    pass->system = system;

    if (system->first_pass)
    {
        system->first_pass->previous = pass;
    }

    system->first_pass = pass;
    pass->pass = render_pass;
    return pass;
}

static inline void render_backend_system_destroy_pass (struct render_backend_system_t *system,
                                                       struct render_backend_pass_t *pass,
                                                       kan_bool_t remove_from_list)
{
    if (remove_from_list)
    {
        if (pass->next)
        {
            pass->next->previous = pass->previous;
        }

        if (pass->previous)
        {
            pass->previous->next = pass->next;
        }
        else
        {
            KAN_ASSERT (system->first_pass = pass)
            system->first_pass = pass->next;
        }
    }

    vkDestroyRenderPass (system->device, pass->pass, VULKAN_ALLOCATION_CALLBACKS (system));
    kan_free_batched (system->pass_wrapper_allocation_group, pass);
}

static inline struct render_backend_buffer_t *render_backend_system_create_buffer (
    struct render_backend_system_t *system,
    enum render_backend_buffer_family_t family,
    enum kan_render_buffer_type_t buffer_type,
    uint64_t full_size,
    kan_interned_string_t tracking_name)
{
    VkBufferUsageFlags usage_flags = 0u;
    VmaAllocationCreateFlagBits allocation_flags = 0u;

    switch (system->device_memory_type)
    {
    case KAN_RENDER_DEVICE_MEMORY_TYPE_SEPARATE:
        break;

    case KAN_RENDER_DEVICE_MEMORY_TYPE_UNIFIED:
    case KAN_RENDER_DEVICE_MEMORY_TYPE_UNIFIED_COHERENT:
        // All memory is unified and host visible anyway.
        allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        break;
    }

    switch (family)
    {
    case RENDER_BACKEND_BUFFER_FAMILY_RESOURCE:
        usage_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        break;

    case RENDER_BACKEND_BUFFER_FAMILY_STAGING:
        usage_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        break;

    case RENDER_BACKEND_BUFFER_FAMILY_FRAME_LIFETIME_ALLOCATOR:
        allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        break;
    }

    if (family != RENDER_BACKEND_BUFFER_FAMILY_STAGING)
    {
        switch (buffer_type)
        {
        case KAN_RENDER_BUFFER_TYPE_ATTRIBUTE:
            usage_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            break;

        case KAN_RENDER_BUFFER_TYPE_INDEX_16:
        case KAN_RENDER_BUFFER_TYPE_INDEX_32:
            usage_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            break;

        case KAN_RENDER_BUFFER_TYPE_UNIFORM:
            usage_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            break;

        case KAN_RENDER_BUFFER_TYPE_STORAGE:
            usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            break;
        }
    }

    uint32_t used_queues_count;
    uint32_t used_queues_indices[2u];

    switch (family)
    {
    case RENDER_BACKEND_BUFFER_FAMILY_RESOURCE:
        if (system->device_graphics_queue_family_index == system->device_transfer_queue_family_index)
        {
            used_queues_count = 1u;
            used_queues_indices[0u] = system->device_graphics_queue_family_index;
        }
        else
        {
            used_queues_count = 2u;
            used_queues_indices[0u] = system->device_graphics_queue_family_index;
            used_queues_indices[0u] = system->device_transfer_queue_family_index;
        }

        break;

    case RENDER_BACKEND_BUFFER_FAMILY_STAGING:
        used_queues_count = 1u;
        used_queues_indices[0u] = system->device_transfer_queue_family_index;
        break;

    case RENDER_BACKEND_BUFFER_FAMILY_FRAME_LIFETIME_ALLOCATOR:
        used_queues_count = 1u;
        used_queues_indices[0u] = system->device_graphics_queue_family_index;
        break;
    }

    VkBufferCreateInfo buffer_create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .size = full_size,
        .usage = usage_flags,
        .sharingMode = used_queues_count <= 1u ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = used_queues_count,
        .pQueueFamilyIndices = used_queues_indices,
    };

    VmaAllocationCreateInfo allocation_create_info = {
        .flags = allocation_flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = 0u,
        .preferredFlags = 0u,
        .memoryTypeBits = UINT32_MAX,
        .pool = NULL,
        .pUserData = NULL,
        .priority = 0.0f,
    };

    VmaAllocationInfo allocation_info;
    VkBuffer buffer_handle;
    VmaAllocation allocation_handle;

    if (vmaCreateBuffer (system->gpu_memory_allocator, &buffer_create_info, &allocation_create_info, &buffer_handle,
                         &allocation_handle, &allocation_info) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to create buffer \"%s\" of size %llu.",
                 tracking_name, (unsigned long long) full_size)
        return NULL;
    }

    struct render_backend_buffer_t *buffer =
        kan_allocate_batched (system->buffer_wrapper_allocation_group, sizeof (struct render_backend_buffer_t));

    buffer->next = system->first_buffer;
    buffer->previous = NULL;

    if (system->first_buffer)
    {
        system->first_buffer->previous = buffer;
    }

    system->first_buffer = buffer;
    buffer->system = system;
    buffer->buffer = buffer_handle;
    buffer->allocation = allocation_handle;
    buffer->family = family;
    buffer->type = buffer_type;
    buffer->full_size = full_size;
    buffer->tracking_name = tracking_name;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    kan_allocation_group_t type_allocation_group = system->memory_profiling.gpu_buffer_group;
    switch (buffer_type)
    {
    case KAN_RENDER_BUFFER_TYPE_ATTRIBUTE:
        type_allocation_group = system->memory_profiling.gpu_buffer_attribute_group;
        break;

    case KAN_RENDER_BUFFER_TYPE_INDEX_16:
    case KAN_RENDER_BUFFER_TYPE_INDEX_32:
        type_allocation_group = system->memory_profiling.gpu_buffer_index_group;
        break;

    case KAN_RENDER_BUFFER_TYPE_UNIFORM:
        type_allocation_group = system->memory_profiling.gpu_buffer_uniform_group;
        break;

    case KAN_RENDER_BUFFER_TYPE_STORAGE:
        type_allocation_group = system->memory_profiling.gpu_buffer_storage_group;
        break;
    }

    buffer->device_allocation_group = kan_allocation_group_get_child (type_allocation_group, buffer->tracking_name);
    transfer_memory_between_groups (full_size, system->memory_profiling.gpu_unmarked_group,
                                    buffer->device_allocation_group);
#endif

    return buffer;
}

static inline void render_backend_system_destroy_buffer (struct render_backend_system_t *system,
                                                         struct render_backend_buffer_t *buffer,
                                                         kan_bool_t remove_from_list)
{
    if (remove_from_list)
    {
        if (buffer->next)
        {
            buffer->next->previous = buffer->previous;
        }

        if (buffer->previous)
        {
            buffer->previous->next = buffer->next;
        }
        else
        {
            KAN_ASSERT (system->first_buffer = buffer)
            system->first_buffer = buffer->next;
        }
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    transfer_memory_between_groups (buffer->full_size, buffer->device_allocation_group,
                                    system->memory_profiling.gpu_unmarked_group);
#endif

    vmaDestroyBuffer (system->gpu_memory_allocator, buffer->buffer, buffer->allocation);
    kan_free_batched (system->buffer_wrapper_allocation_group, buffer);
}

static struct render_backend_frame_lifetime_allocator_t *render_backend_system_create_frame_lifetime_allocator (
    struct render_backend_system_t *system,
    enum render_backend_buffer_family_t buffer_family,
    enum kan_render_buffer_type_t buffer_type,
    uint64_t page_size,
    kan_interned_string_t tracking_name)
{
    struct render_backend_frame_lifetime_allocator_t *allocator = kan_allocate_batched (
        system->frame_lifetime_wrapper_allocation_group, sizeof (struct render_backend_frame_lifetime_allocator_t));

    allocator->next = system->first_frame_lifetime_allocator;
    allocator->previous = NULL;

    if (system->first_frame_lifetime_allocator)
    {
        system->first_frame_lifetime_allocator->previous = allocator;
    }

    system->first_frame_lifetime_allocator = allocator;
    allocator->system = system;

    allocator->first_page = NULL;
    allocator->last_page = NULL;
    allocator->allocation_lock = kan_atomic_int_init (0);

    KAN_ASSERT (buffer_family == RENDER_BACKEND_BUFFER_FAMILY_STAGING ||
                buffer_family == RENDER_BACKEND_BUFFER_FAMILY_FRAME_LIFETIME_ALLOCATOR)
    allocator->buffer_family = buffer_family;
    allocator->buffer_type = buffer_type;
    allocator->page_size = page_size;
    allocator->tracking_name = tracking_name;
    return allocator;
}

static struct render_backend_frame_lifetime_allocator_allocation_t
render_backend_frame_lifetime_allocator_allocate_on_page (struct render_backend_frame_lifetime_allocator_t *allocator,
                                                          struct render_backend_frame_lifetime_allocator_page_t *page,
                                                          uint64_t size,
                                                          uint64_t alignment)
{
    struct render_backend_frame_lifetime_allocator_allocation_t result = {
        .buffer = NULL,
        .offset = 0u,
    };

    struct render_backend_frame_lifetime_allocator_chunk_t *previous_chunk = NULL;
    struct render_backend_frame_lifetime_allocator_chunk_t *chunk = page->first_free_chunk;

    while (chunk)
    {
        KAN_ASSERT (chunk->occupied_by_frame == CHUNK_FREE_MARKER)
        const uint64_t memory_offset = chunk->offset;
        const uint64_t allocation_offset = kan_apply_alignment (memory_offset, alignment);
        const uint64_t allocation_size = size + allocation_offset - memory_offset;

        if (chunk->size >= allocation_size)
        {
            // Can allocate from here.
            result.buffer = page->buffer;
            result.offset = allocation_offset;

            chunk->offset += allocation_size;
            chunk->size -= allocation_size;

            if (chunk->previous &&
                chunk->previous->occupied_by_frame == allocator->system->current_frame_in_flight_index)
            {
                // Previous chunk is associated with current frame, increase it by allocation.
                chunk->previous->size += allocation_size;

                // If current chunk is fully consumed by this operation, delete it.
                if (chunk->size == 0u)
                {
                    chunk->previous->next = chunk->next;
                    if (chunk->next)
                    {
                        chunk->next->previous = chunk->previous;
                    }

                    if (previous_chunk)
                    {
                        previous_chunk->next_free = chunk->next_free;
                    }
                    else
                    {
                        KAN_ASSERT (page->first_free_chunk)
                        page->first_free_chunk = chunk->next_free;
                    }

                    kan_free_batched (allocator->system->frame_lifetime_wrapper_allocation_group, chunk);
                }
            }
            else if (chunk->size == 0u)
            {
                // Chunk was fully used by this allocation. Undo it and mark chunk as occupied by current frame.
                chunk->offset = memory_offset;
                chunk->size = allocation_size;
                chunk->occupied_by_frame = allocator->system->current_frame_in_flight_index;

                // Remove from free list.
                if (previous_chunk)
                {
                    previous_chunk->next_free = chunk->next_free;
                }
                else
                {
                    KAN_ASSERT (page->first_free_chunk)
                    page->first_free_chunk = chunk->next_free;
                }
            }
            else
            {
                // We need to allocate new chunk in order to represent this allocation.
                struct render_backend_frame_lifetime_allocator_chunk_t *new_chunk =
                    kan_allocate_batched (allocator->system->frame_lifetime_wrapper_allocation_group,
                                          sizeof (struct render_backend_frame_lifetime_allocator_chunk_t));

                new_chunk->offset = memory_offset;
                new_chunk->size = allocation_size;
                new_chunk->occupied_by_frame = allocator->system->current_frame_in_flight_index;

                new_chunk->next_free = NULL;
                new_chunk->next = chunk;
                new_chunk->previous = chunk->previous;
                chunk->previous = new_chunk;

                if (new_chunk->previous)
                {
                    new_chunk->previous->next = new_chunk;
                }
                else
                {
                    KAN_ASSERT (page->first_chunk == chunk)
                    page->first_chunk = new_chunk;
                }
            }

            return result;
        }

        previous_chunk = chunk;
        chunk = chunk->next_free;
    }

    return result;
}

static struct render_backend_frame_lifetime_allocator_allocation_t render_backend_frame_lifetime_allocator_allocate (
    struct render_backend_frame_lifetime_allocator_t *allocator, uint64_t size, uint64_t alignment)
{
    struct render_backend_frame_lifetime_allocator_allocation_t result = {
        .buffer = NULL,
        .offset = 0u,
    };

    if (size > allocator->page_size)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Frame-lifetime allocator \"%s\": Caught attempt to allocate data of size %llu which is greater than "
                 "page size %llu.",
                 allocator->tracking_name, (unsigned long long) size, (unsigned long long) allocator->page_size)
        return result;
    }

    kan_atomic_int_lock (&allocator->allocation_lock);
    struct render_backend_frame_lifetime_allocator_page_t *page = allocator->first_page;

    while (page)
    {
        result = render_backend_frame_lifetime_allocator_allocate_on_page (allocator, page, size, alignment);
        if (result.buffer)
        {
            kan_atomic_int_unlock (&allocator->allocation_lock);
            return result;
        }

        page = page->next;
    }

    // No space on existing pages, need to create new one.
    struct render_backend_buffer_t *new_page_buffer =
        render_backend_system_create_buffer (allocator->system, allocator->buffer_family, allocator->buffer_type,
                                             allocator->page_size, allocator->tracking_name);

    if (!new_page_buffer)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Frame-lifetime allocator \"%s\": Failed to allocate %llu bytes as all pages are full and new page "
                 "allocation has failed.",
                 allocator->tracking_name, (unsigned long long) size)

        kan_atomic_int_unlock (&allocator->allocation_lock);
        return result;
    }

    struct render_backend_frame_lifetime_allocator_page_t *new_page =
        kan_allocate_batched (allocator->system->frame_lifetime_wrapper_allocation_group,
                              sizeof (struct render_backend_frame_lifetime_allocator_page_t));

    new_page->next = NULL;
    if (allocator->last_page)
    {
        allocator->last_page->next = new_page;
    }
    else
    {
        allocator->first_page = new_page;
    }

    allocator->last_page = new_page;
    new_page->buffer = new_page_buffer;

    new_page->first_chunk = kan_allocate_batched (allocator->system->frame_lifetime_wrapper_allocation_group,
                                                  sizeof (struct render_backend_frame_lifetime_allocator_chunk_t));
    new_page->first_free_chunk = new_page->first_chunk;

    new_page->first_chunk->next = NULL;
    new_page->first_chunk->previous = NULL;
    new_page->first_chunk->next_free = NULL;
    new_page->first_chunk->offset = 0u;
    new_page->first_chunk->size = allocator->page_size;
    new_page->first_chunk->occupied_by_frame = CHUNK_FREE_MARKER;

    result = render_backend_frame_lifetime_allocator_allocate_on_page (allocator, new_page, size, alignment);
    kan_atomic_int_unlock (&allocator->allocation_lock);
    return result;
}

static struct render_backend_frame_lifetime_allocator_allocation_t render_backend_system_allocate_for_staging (
    struct render_backend_system_t *system, uint64_t size)
{
    if (size <= system->staging_frame_lifetime_allocator->page_size)
    {
        // Size is not overwhelmingly big, so we can use staging frame lifetime allocator.
        return render_backend_frame_lifetime_allocator_allocate (system->staging_frame_lifetime_allocator, size,
                                                                 STAGING_BUFFER_ALLOCATION_ALIGNMENT);
    }

    // Size is super big, we need separate buffer.
    kan_atomic_int_lock (&system->resource_management_lock);
    struct render_backend_buffer_t *buffer = render_backend_system_create_buffer (
        system, RENDER_BACKEND_BUFFER_FAMILY_STAGING, KAN_RENDER_BUFFER_TYPE_STORAGE, size,
        system->interned_temporary_staging_buffer);
    kan_atomic_int_unlock (&system->resource_management_lock);

    if (!buffer)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Failed to stage %llu bytes: allocation is too big for staging allocator and there is no memory for "
                 "separate staging buffer.",
                 (unsigned long long) size)

        return (struct render_backend_frame_lifetime_allocator_allocation_t) {
            .buffer = NULL,
            .offset = 0u,
        };
    }

    // Schedule deletion of temporary buffer.
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_buffer_destroy_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_buffer_destroy_t);

    // We do not need to preserve order as buffers cannot depend one on another.
    item->next = schedule->first_scheduled_buffer_destroy;
    schedule->first_scheduled_buffer_destroy = item;
    item->buffer = buffer;
    kan_atomic_int_unlock (&schedule->schedule_lock);

    return (struct render_backend_frame_lifetime_allocator_allocation_t) {
        .buffer = buffer,
        .offset = 0u,
    };
}

static void render_backend_frame_lifetime_allocator_retire_old_allocations (
    struct render_backend_frame_lifetime_allocator_t *allocator)
{
    struct render_backend_frame_lifetime_allocator_page_t *page = allocator->first_page;
    while (page)
    {
        struct render_backend_frame_lifetime_allocator_chunk_t *chunk = page->first_chunk;
        while (chunk)
        {
            struct render_backend_frame_lifetime_allocator_chunk_t *next_chunk = chunk->next;
            if (chunk->occupied_by_frame == allocator->system->current_frame_in_flight_index)
            {
                // Chunk should retire as its data lifetime has ended.
                chunk->occupied_by_frame = CHUNK_FREE_MARKER;
            }

            if (chunk->occupied_by_frame == CHUNK_FREE_MARKER && chunk->previous &&
                chunk->previous->occupied_by_frame == CHUNK_FREE_MARKER)
            {
                // We can merge this chunk into previous free chunk.
                chunk->previous->size += chunk->size;
                chunk->previous->next = chunk->next;

                if (chunk->next)
                {
                    chunk->next->previous = chunk->previous;
                }

                kan_free_batched (allocator->system->frame_lifetime_wrapper_allocation_group, chunk);
            }

            chunk = next_chunk;
        }

        page->first_free_chunk = NULL;
        chunk = page->first_chunk;

        while (chunk)
        {
            if (chunk->occupied_by_frame == CHUNK_FREE_MARKER)
            {
                chunk->next_free = page->first_free_chunk;
                page->first_free_chunk = chunk;
            }

            chunk = chunk->next;
        }

        page = page->next;
    }
}

static void render_backend_frame_lifetime_allocator_destroy_page (
    struct render_backend_system_t *system,
    struct render_backend_frame_lifetime_allocator_page_t *page,
    kan_bool_t destroy_buffers)
{
    if (destroy_buffers)
    {
        render_backend_system_destroy_buffer (system, page->buffer, KAN_TRUE);
    }

    struct render_backend_frame_lifetime_allocator_chunk_t *chunk = page->first_chunk;
    while (chunk)
    {
        struct render_backend_frame_lifetime_allocator_chunk_t *next_chunk = chunk->next;
        kan_free_batched (system->frame_lifetime_wrapper_allocation_group, chunk);
        chunk = next_chunk;
    }

    kan_free_batched (system->frame_lifetime_wrapper_allocation_group, page);
}

static void render_backend_frame_lifetime_allocator_clean_empty_pages (
    struct render_backend_frame_lifetime_allocator_t *allocator)
{
    struct render_backend_frame_lifetime_allocator_page_t *previous_page = NULL;
    struct render_backend_frame_lifetime_allocator_page_t *page = allocator->first_page;

    while (page)
    {
        struct render_backend_frame_lifetime_allocator_page_t *next_page = page->next;
        if (page->first_free_chunk && page->first_free_chunk->offset == 0u &&
            page->first_free_chunk->size == allocator->page_size)
        {
            KAN_ASSERT (!page->first_free_chunk->next_free)
            // Looks like page is empty. Lets get rid of it.

            if (previous_page)
            {
                previous_page->next = next_page;
            }
            else
            {
                KAN_ASSERT (page == allocator->first_page)
                allocator->first_page = page->next;
            }

            if (!page->next)
            {
                allocator->last_page = previous_page;
            }

            render_backend_frame_lifetime_allocator_destroy_page (allocator->system, page, KAN_TRUE);
        }
        else
        {
            // Page is not empty, we should keep it.
            previous_page = page;
        }

        page = next_page;
    }
}

static void render_backend_system_destroy_frame_lifetime_allocator (
    struct render_backend_system_t *system,
    struct render_backend_frame_lifetime_allocator_t *frame_lifetime_allocator,
    kan_bool_t destroy_buffers,
    kan_bool_t remove_from_list)
{
    if (remove_from_list)
    {
        if (frame_lifetime_allocator->next)
        {
            frame_lifetime_allocator->next->previous = frame_lifetime_allocator->previous;
        }

        if (frame_lifetime_allocator->previous)
        {
            frame_lifetime_allocator->previous->next = frame_lifetime_allocator->next;
        }
        else
        {
            KAN_ASSERT (system->first_frame_lifetime_allocator = frame_lifetime_allocator)
            system->first_frame_lifetime_allocator = frame_lifetime_allocator->next;
        }
    }

    struct render_backend_frame_lifetime_allocator_page_t *page = frame_lifetime_allocator->first_page;
    while (page)
    {
        struct render_backend_frame_lifetime_allocator_page_t *next = page->next;
        render_backend_frame_lifetime_allocator_destroy_page (system, page, destroy_buffers);
        page = next;
    }

    kan_free_batched (system->frame_lifetime_wrapper_allocation_group, frame_lifetime_allocator);
}

static inline VkFormat kan_render_image_description_calculate_format (
    struct render_backend_system_t *system, struct kan_render_image_description_t *description)
{
    VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;
    switch (description->type)
    {
    case KAN_RENDER_IMAGE_TYPE_COLOR_2D:
    case KAN_RENDER_IMAGE_TYPE_COLOR_3D:
        switch (description->color_format)
        {
        case KAN_RENDER_COLOR_FORMAT_RGBA32_SRGB:
            image_format = VK_FORMAT_R8G8B8A8_SRGB;
            break;

        case KAN_RENDER_COLOR_FORMAT_RGBA128_SFLOAT:
            image_format = VK_FORMAT_R32G32B32A32_SFLOAT;
            break;

        case KAN_RENDER_COLOR_FORMAT_SURFACE:
            image_format = SURFACE_COLOR_FORMAT;
            break;
        }

        break;

    case KAN_RENDER_IMAGE_TYPE_DEPTH_STENCIL:
        image_format = system->device_depth_image_format;
        break;
    }

    return image_format;
}

static inline kan_bool_t create_vulkan_image (struct render_backend_system_t *system,
                                              struct kan_render_image_description_t *description,
                                              VkImage *output_image,
                                              VmaAllocation *output_allocation)
{
    VkImageType image_type = VK_IMAGE_TYPE_2D;
    VkImageUsageFlags image_usage = 0u;

    VkSharingMode sharing_mode;
    uint32_t sharing_index_count;
    uint32_t shared_indices[2u] = {system->device_graphics_queue_family_index,
                                   system->device_transfer_queue_family_index};

    switch (description->type)
    {
    case KAN_RENDER_IMAGE_TYPE_COLOR_2D:
        KAN_ASSERT (description->depth == 1u)
        image_type = VK_IMAGE_TYPE_2D;
        image_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        break;

    case KAN_RENDER_IMAGE_TYPE_COLOR_3D:
        image_type = VK_IMAGE_TYPE_3D;
        image_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        break;

    case KAN_RENDER_IMAGE_TYPE_DEPTH_STENCIL:
        KAN_ASSERT (description->render_target)
        image_type = VK_IMAGE_TYPE_2D;
        image_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        break;
    }

    if (description->render_target)
    {
        KAN_ASSERT (description->mips == 1u)
        switch (description->type)
        {
        case KAN_RENDER_IMAGE_TYPE_COLOR_2D:
        case KAN_RENDER_IMAGE_TYPE_COLOR_3D:
            image_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            break;

        case KAN_RENDER_IMAGE_TYPE_DEPTH_STENCIL:
            image_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            break;
        }
    }
    else
    {
        // We need source transfer in order to generate mip maps, unless they are expected to be loaded from textures.
        image_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (description->supports_sampling)
    {
        image_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    if (description->render_target &&
        system->device_graphics_queue_family_index != system->device_transfer_queue_family_index)
    {
        sharing_mode = VK_SHARING_MODE_CONCURRENT;
        sharing_index_count = 2u;
    }
    else
    {
        sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        sharing_index_count = 1u;
    }

    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .imageType = image_type,
        .format = kan_render_image_description_calculate_format (system, description),
        .extent =
            {
                .width = (uint32_t) description->width,
                .height = (uint32_t) description->height,
                .depth = (uint32_t) description->depth,
            },
        .mipLevels = (uint32_t) description->mips,
        .arrayLayers = 1u,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = image_usage,
        .sharingMode = sharing_mode,
        .queueFamilyIndexCount = sharing_index_count,
        .pQueueFamilyIndices = shared_indices,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocation_create_info = {
        .flags = 0u,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = 0u,
        .preferredFlags = 0u,
        .memoryTypeBits = UINT32_MAX,
        .pool = NULL,
        .pUserData = NULL,
        .priority = 0.0f,
    };

    VmaAllocationInfo allocation_info;
    if (vmaCreateImage (system->gpu_memory_allocator, &image_create_info, &allocation_create_info, output_image,
                        output_allocation, &allocation_info) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to create image \"%s\".",
                 description->tracking_name)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline VkImageAspectFlags kan_render_image_description_calculate_aspects (
    struct render_backend_system_t *system, struct kan_render_image_description_t *description)
{
    VkImageAspectFlags aspects = 0u;
    switch (description->type)
    {
    case KAN_RENDER_IMAGE_TYPE_COLOR_2D:
    case KAN_RENDER_IMAGE_TYPE_COLOR_3D:
        aspects |= VK_IMAGE_ASPECT_COLOR_BIT;
        break;

    case KAN_RENDER_IMAGE_TYPE_DEPTH_STENCIL:
        aspects |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if (system->device_depth_image_has_stencil)
        {
            aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        break;
    }

    return aspects;
}

static inline void kan_render_image_description_calculate_size_at_mip (
    struct kan_render_image_description_t *description,
    uint64_t mip,
    uint64_t *output_width,
    uint64_t *output_height,
    uint64_t *output_depth)
{
    KAN_ASSERT (mip < description->mips)
    *output_width = KAN_MAX (1u, description->width >> mip);
    *output_height = KAN_MAX (1u, description->height >> mip);
    *output_depth = KAN_MAX (1u, description->depth >> mip);
}

static inline uint64_t kan_render_image_description_calculate_texel_size (
    struct render_backend_system_t *system, struct kan_render_image_description_t *description)
{
    uint64_t texel_size = 0u;
    switch (description->type)
    {
    case KAN_RENDER_IMAGE_TYPE_COLOR_2D:
    case KAN_RENDER_IMAGE_TYPE_COLOR_3D:
        switch (description->color_format)
        {
        case KAN_RENDER_COLOR_FORMAT_RGBA32_SRGB:
        case KAN_RENDER_COLOR_FORMAT_SURFACE:
            texel_size = 4u;
            break;

        case KAN_RENDER_COLOR_FORMAT_RGBA128_SFLOAT:
            texel_size = 16u;
            break;
        }

        break;

    case KAN_RENDER_IMAGE_TYPE_DEPTH_STENCIL:
        switch (system->device_depth_image_format)
        {
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
            texel_size = 4u;
            break;

        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            texel_size = 5u;
            break;

        default:
            KAN_ASSERT (KAN_FALSE)
            texel_size = 0u;
            break;
        }

        break;
    }

    return texel_size;
}

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
static uint64_t render_backend_image_calculate_gpu_size (struct render_backend_system_t *system,
                                                         struct render_backend_image_t *image)
{
    uint64_t texel_size = kan_render_image_description_calculate_texel_size (system, &image->description);
    uint64_t size = 0u;

    for (uint64_t mip = 0u; mip < image->description.mips; ++mip)
    {
        uint64_t width;
        uint64_t height;
        uint64_t depth;
        kan_render_image_description_calculate_size_at_mip (&image->description, mip, &width, &height, &depth);
        size += texel_size * width * height * depth;
    }

    return size;
}
#endif

static struct render_backend_image_t *render_backend_system_create_image (
    struct render_backend_system_t *system, struct kan_render_image_description_t *description)
{
    VkImage vulkan_image;
    VmaAllocation vulkan_allocation;

    if (!create_vulkan_image (system, description, &vulkan_image, &vulkan_allocation))
    {
        return NULL;
    }

    struct render_backend_image_t *image =
        kan_allocate_batched (system->image_wrapper_allocation_group, sizeof (struct render_backend_image_t));
    image->next = system->first_image;
    image->previous = NULL;

    if (system->first_image)
    {
        system->first_image->previous = image;
    }

    system->first_image = image;
    image->system = system;

    image->image = vulkan_image;
    image->allocation = vulkan_allocation;
    image->description = *description;
    image->switched_to_transfer_source = KAN_FALSE;
    image->first_frame_buffer_attachment = NULL;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    image->device_allocation_group =
        kan_allocation_group_get_child (system->memory_profiling.gpu_image_group, description->tracking_name);
    transfer_memory_between_groups (render_backend_image_calculate_gpu_size (system, image),
                                    system->memory_profiling.gpu_unmarked_group, image->device_allocation_group);
#endif

    return image;
}

static void render_backend_system_destroy_image (struct render_backend_system_t *system,
                                                 struct render_backend_image_t *image,
                                                 kan_bool_t remove_from_list)
{
    if (remove_from_list)
    {
        if (image->next)
        {
            image->next->previous = image->previous;
        }

        if (image->previous)
        {
            image->previous->next = image->next;
        }
        else
        {
            KAN_ASSERT (system->first_image == image)
            system->first_image = image->next;
        }
    }

    struct image_frame_buffer_attachment_t *attachment = image->first_frame_buffer_attachment;
    while (attachment)
    {
        struct image_frame_buffer_attachment_t *next = attachment->next;
        kan_free_batched (system->image_wrapper_allocation_group, attachment);
        attachment = next;
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    transfer_memory_between_groups (render_backend_image_calculate_gpu_size (system, image),
                                    image->device_allocation_group, system->memory_profiling.gpu_unmarked_group);
#endif

    vmaDestroyImage (system->gpu_memory_allocator, image->image, image->allocation);
    kan_free_batched (system->image_wrapper_allocation_group, image);
}

kan_context_system_handle_t render_backend_system_create (kan_allocation_group_t group, void *user_config)
{
    struct render_backend_system_t *system = kan_allocate_general (group, sizeof (struct render_backend_system_t),
                                                                   _Alignof (struct render_backend_system_t));

    system->instance = VK_NULL_HANDLE;
    system->device = VK_NULL_HANDLE;
    system->physical_device = VK_NULL_HANDLE;

    system->main_allocation_group = group;
    system->utility_allocation_group = kan_allocation_group_get_child (group, "utility");
    system->surface_wrapper_allocation_group = kan_allocation_group_get_child (group, "surface_wrapper");
    system->schedule_allocation_group = kan_allocation_group_get_child (group, "schedule");
    system->frame_buffer_wrapper_allocation_group = kan_allocation_group_get_child (group, "frame_buffer_wrapper");
    system->pass_wrapper_allocation_group = kan_allocation_group_get_child (group, "pass_wrapper");
    system->buffer_wrapper_allocation_group = kan_allocation_group_get_child (group, "buffer_wrapper");
    system->frame_lifetime_wrapper_allocation_group =
        kan_allocation_group_get_child (group, "frame_lifetime_allocator_wrapper");

    system->frame_started = KAN_FALSE;
    system->current_frame_in_flight_index = 0u;

    system->resource_management_lock = kan_atomic_int_init (0);

    system->first_surface = NULL;
    system->first_frame_buffer = NULL;
    system->first_pass = NULL;
    system->first_buffer = NULL;
    system->first_frame_lifetime_allocator = NULL;
    system->first_image = NULL;
    system->staging_frame_lifetime_allocator = NULL;
    system->supported_devices = NULL;

    if (user_config)
    {
        struct kan_render_backend_system_config_t *config = (struct kan_render_backend_system_config_t *) user_config;
        system->render_enabled = !config->disable_render;
        system->prefer_vsync = config->prefer_vsync;
        system->application_info_name = config->application_info_name;
        system->version_major = config->version_major;
        system->version_minor = config->version_minor;
        system->version_patch = config->version_patch;
    }
    else
    {
        system->render_enabled = KAN_TRUE;
        system->prefer_vsync = KAN_FALSE;
        system->application_info_name = kan_string_intern ("unnamed_application");
        system->version_major = 1u;
        system->version_minor = 0u;
        system->version_patch = 0u;
    }

    system->interned_temporary_staging_buffer = kan_string_intern ("temporary_staging_buffer");
    return (kan_context_system_handle_t) system;
}

void render_backend_system_connect (kan_context_system_handle_t handle, kan_context_handle_t context)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) handle;
    system->context = context;

    // We request application system here in order to ensure that shutdown is called for render backend system
    // after the application system shutdown.
    kan_context_query (system->context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
vulkan_message_callback (VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                         VkDebugUtilsMessageTypeFlagsEXT type,
                         const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                         void *user_data)
{
    enum kan_log_verbosity_t verbosity;
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        verbosity = KAN_LOG_ERROR;
    }
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        verbosity = KAN_LOG_WARNING;
    }
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        verbosity = KAN_LOG_INFO;
    }
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        verbosity = KAN_LOG_DEBUG;
    }
    else
    {
        verbosity = KAN_LOG_VERBOSE;
    }

    KAN_LOG (render_backend_system_vulkan, verbosity, "Received message from Vulkan: %s", callback_data->pMessage)
    return VK_FALSE;
}

static enum kan_render_device_memory_type_t query_device_memory_type (VkPhysicalDevice device)
{
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties (device, &memory_properties);

    kan_bool_t is_host_visible[VK_MAX_MEMORY_HEAPS];
    kan_bool_t is_host_coherent[VK_MAX_MEMORY_HEAPS];

    for (uint64_t memory_type_index = 0u; memory_type_index < (uint64_t) memory_properties.memoryTypeCount;
         ++memory_type_index)
    {
        is_host_visible[memory_properties.memoryTypes[memory_type_index].heapIndex] |=
            (memory_properties.memoryTypes[memory_type_index].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ?
                KAN_TRUE :
                KAN_FALSE;

        is_host_coherent[memory_properties.memoryTypes[memory_type_index].heapIndex] |=
            (memory_properties.memoryTypes[memory_type_index].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ?
                KAN_TRUE :
                KAN_FALSE;
    }

    kan_bool_t any_local_non_visible = KAN_FALSE;
    kan_bool_t any_local_non_coherent = KAN_FALSE;

    for (uint64_t heap_index = 0u; heap_index < (uint64_t) memory_properties.memoryHeapCount; ++heap_index)
    {
        if (memory_properties.memoryHeaps[heap_index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        {
            any_local_non_visible |= !is_host_visible[heap_index];
            any_local_non_coherent |= !is_host_coherent[heap_index];
        }
    }

    if (any_local_non_visible)
    {
        return KAN_RENDER_DEVICE_MEMORY_TYPE_SEPARATE;
    }
    else if (any_local_non_coherent)
    {
        return KAN_RENDER_DEVICE_MEMORY_TYPE_UNIFIED;
    }
    else
    {
        return KAN_RENDER_DEVICE_MEMORY_TYPE_UNIFIED_COHERENT;
    }
}

static void render_backend_system_query_devices (struct render_backend_system_t *system)
{
    uint32_t physical_device_count;
    if (vkEnumeratePhysicalDevices (system->instance, &physical_device_count, NULL) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to query physical devices.")
        return;
    }

    VkPhysicalDevice *physical_devices =
        kan_allocate_general (system->utility_allocation_group, sizeof (VkPhysicalDevice) * physical_device_count,
                              _Alignof (VkPhysicalDevice));

    if (vkEnumeratePhysicalDevices (system->instance, &physical_device_count, physical_devices) != VK_SUCCESS)
    {
        kan_free_general (system->utility_allocation_group, physical_devices,
                          sizeof (VkPhysicalDevice) * physical_device_count);

        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to query physical devices.")
        return;
    }

    system->supported_devices =
        kan_allocate_general (system->utility_allocation_group,
                              sizeof (struct kan_render_supported_devices_t) +
                                  sizeof (struct kan_render_supported_device_info_t) * physical_device_count,
                              _Alignof (struct kan_render_supported_devices_t));
    system->supported_devices->supported_device_count = (uint64_t) physical_device_count;

    for (uint64_t device_index = 0u; device_index < physical_device_count; ++device_index)
    {
        struct kan_render_supported_device_info_t *device_info = &system->supported_devices->devices[device_index];
        _Static_assert (sizeof (kan_render_device_id_t) >= sizeof (VkPhysicalDevice),
                        "Can store Vulkan handle in Kan id.");
        device_info->id = (kan_render_device_id_t) physical_devices[device_index];
        device_info->name = NULL;
        device_info->device_type = KAN_RENDER_DEVICE_TYPE_UNKNOWN;
        device_info->memory_type = KAN_RENDER_DEVICE_MEMORY_TYPE_SEPARATE;

        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties (physical_devices[device_index], &device_properties);
        device_info->name = kan_string_intern (device_properties.deviceName);

        switch (device_properties.deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            device_info->device_type = KAN_RENDER_DEVICE_TYPE_INTEGRATED_GPU;
            break;

        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            device_info->device_type = KAN_RENDER_DEVICE_TYPE_DISCRETE_GPU;
            break;

        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            device_info->device_type = KAN_RENDER_DEVICE_TYPE_VIRTUAL_GPU;
            break;

        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            device_info->device_type = KAN_RENDER_DEVICE_TYPE_CPU;
            break;

        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        case VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM:
            device_info->device_type = KAN_RENDER_DEVICE_TYPE_UNKNOWN;
            break;
        }

        device_info->memory_type = query_device_memory_type (physical_devices[device_index]);
    }

    kan_free_general (system->utility_allocation_group, physical_devices,
                      sizeof (VkPhysicalDevice) * physical_device_count);
}

void render_backend_system_init (kan_context_system_handle_t handle)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) handle;
    if (!system->render_enabled)
    {
        return;
    }

    if (!kan_platform_application_register_vulkan_library_usage ())
    {
        kan_critical_error ("Failed to register vulkan library usage, unable to continue properly.", __FILE__,
                            __LINE__);
    }

    volkInitializeCustom ((PFN_vkGetInstanceProcAddr) kan_platform_application_request_vulkan_resolve_function ());
    struct kan_dynamic_array_t extensions;
    kan_platform_application_request_vulkan_extensions (&extensions, system->utility_allocation_group);

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
    {
        kan_dynamic_array_set_capacity (&extensions, extensions.size + 1u);
        char **output = kan_dynamic_array_add_last (&extensions);
        *output = kan_allocate_general (system->utility_allocation_group, sizeof (VK_EXT_DEBUG_UTILS_EXTENSION_NAME),
                                        _Alignof (char));
        memcpy (*output, VK_EXT_DEBUG_UTILS_EXTENSION_NAME, sizeof (VK_EXT_DEBUG_UTILS_EXTENSION_NAME));
    }
#endif

    KAN_LOG (render_backend_system_vulkan, KAN_LOG_INFO, "Preparing to create Vulkan instance. Used extensions:")
    for (uint64_t index = 0u; index < extensions.size; ++index)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_INFO, "    - %s", ((char **) extensions.data)[index])
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
    system->debug_messenger = VK_NULL_HANDLE;
#endif

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    memory_profiling_init (system);
#endif

    VkApplicationInfo application_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = system->application_info_name,
        .applicationVersion = VK_MAKE_VERSION (system->version_major, system->version_minor, system->version_patch),
        .pEngineName = "Kan",
        .engineVersion = VK_MAKE_VERSION (1, 0, 0),
        .apiVersion = VK_API_VERSION_1_1,
    };

    VkInstanceCreateInfo instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .pApplicationInfo = &application_info,
        .enabledExtensionCount = (uint32_t) extensions.size,
        .ppEnabledExtensionNames = (const char *const *) extensions.data,
        .enabledLayerCount = 0u,
        .ppEnabledLayerNames = NULL,
    };

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = vulkan_message_callback,
        .pUserData = NULL,
    };

    uint32_t layer_properties_count;
    vkEnumerateInstanceLayerProperties (&layer_properties_count, NULL);

    VkLayerProperties *layer_properties =
        kan_allocate_general (system->utility_allocation_group, sizeof (VkLayerProperties) * layer_properties_count,
                              _Alignof (VkLayerProperties));
    vkEnumerateInstanceLayerProperties (&layer_properties_count, layer_properties);
    system->has_validation_layer = KAN_FALSE;

    for (uint64_t index = 0u; index < layer_properties_count; ++index)
    {
        if (strcmp (layer_properties[index].layerName, "VK_LAYER_KHRONOS_validation") == 0)
        {
            system->has_validation_layer = KAN_TRUE;
            break;
        }
    }

    const char *enabled_layers[1u];
    if (system->has_validation_layer)
    {
        instance_create_info.enabledLayerCount = 1u;
        enabled_layers[0u] = "VK_LAYER_KHRONOS_validation";
        instance_create_info.ppEnabledLayerNames = enabled_layers;
        instance_create_info.pNext = &debug_messenger_create_info;
    }
    else
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to initialize validation layer as it is not supported.")
    }

    kan_free_general (system->utility_allocation_group, layer_properties,
                      sizeof (VkLayerProperties) * layer_properties_count);
#endif

    if (vkCreateInstance (&instance_create_info, VULKAN_ALLOCATION_CALLBACKS (system), &system->instance) != VK_SUCCESS)
    {
        kan_critical_error ("Failed to create Vulkan instance.", __FILE__, __LINE__);
    }

    volkLoadInstance (system->instance);
    for (uint64_t index = 0u; index < extensions.size; ++index)
    {
        kan_free_general (system->utility_allocation_group, ((char **) extensions.data)[index],
                          strlen (((char **) extensions.data)[index]) + 1u);
    }

    kan_dynamic_array_shutdown (&extensions);

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
    if (system->has_validation_layer)
    {
        debug_messenger_create_info = (VkDebugUtilsMessengerCreateInfoEXT) {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = vulkan_message_callback,
            .pUserData = NULL,
        };

        if (vkCreateDebugUtilsMessengerEXT (system->instance, &debug_messenger_create_info,
                                            VULKAN_ALLOCATION_CALLBACKS (system),
                                            &system->debug_messenger) != VK_SUCCESS)
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to create debug messenger.")
        }
    }
#endif

    render_backend_system_query_devices (system);
}

static void render_backend_system_destroy_schedule_states (struct render_backend_system_t *system)
{
    for (uint64_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        struct render_backend_schedule_state_t *state = &system->schedule_states[index];
        kan_stack_group_allocator_shutdown (&state->item_allocator);
    }
}

static void render_backend_system_destroy_command_states (struct render_backend_system_t *system)
{
    for (uint64_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        struct render_backend_command_state_t *state = &system->command_states[index];

        // There is no need to free command buffers as we're destroying pools already.
        state->primary_graphics_command_buffer = VK_NULL_HANDLE;
        state->primary_transfer_command_buffer = VK_NULL_HANDLE;

        if (state->graphics_command_pool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool (system->device, state->graphics_command_pool, VULKAN_ALLOCATION_CALLBACKS (system));
        }

        if (state->transfer_command_pool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool (system->device, state->transfer_command_pool, VULKAN_ALLOCATION_CALLBACKS (system));
        }
    }
}

static void render_backend_system_destroy_synchronization_objects (struct render_backend_system_t *system)
{
    for (uint64_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        if (system->transfer_finished_semaphores[index] != VK_NULL_HANDLE)
        {
            vkDestroySemaphore (system->device, system->transfer_finished_semaphores[index],
                                VULKAN_ALLOCATION_CALLBACKS (system));
        }

        if (system->render_finished_semaphores[index] != VK_NULL_HANDLE)
        {
            vkDestroySemaphore (system->device, system->render_finished_semaphores[index],
                                VULKAN_ALLOCATION_CALLBACKS (system));
        }

        if (system->in_flight_fences[index] != VK_NULL_HANDLE)
        {
            vkDestroyFence (system->device, system->in_flight_fences[index], VULKAN_ALLOCATION_CALLBACKS (system));
        }
    }
}

void render_backend_system_shutdown (kan_context_system_handle_t handle)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) handle;
    if (!system->render_enabled)
    {
        return;
    }

    vkDeviceWaitIdle (system->device);
    // All surfaces should've been automatically destroyed during application system shutdown.
    KAN_ASSERT (!system->first_surface)

    struct render_backend_frame_buffer_t *frame_buffer = system->first_frame_buffer;
    while (frame_buffer)
    {
        struct render_backend_frame_buffer_t *next = frame_buffer->next;
        render_backend_system_destroy_frame_buffer (system, frame_buffer, KAN_FALSE);
        frame_buffer = next;
    }

    // Destroy all detached data so we won't leak memory.
    for (uint64_t schedule_index = 0u; schedule_index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT;
         ++schedule_index)
    {
        struct render_backend_schedule_state_t *schedule = &system->schedule_states[schedule_index];
        struct scheduled_detached_frame_buffer_destroy_t *detached_frame_buffer_destroy =
            schedule->first_scheduled_detached_frame_buffer_destroy;

        while (detached_frame_buffer_destroy)
        {
            vkDestroyFramebuffer (system->device, detached_frame_buffer_destroy->detached_frame_buffer,
                                  VULKAN_ALLOCATION_CALLBACKS (system));
            detached_frame_buffer_destroy = detached_frame_buffer_destroy->next;
        }

        struct scheduled_detached_image_view_destroy_t *detached_image_view_destroy =
            schedule->first_scheduled_detached_image_view_destroy;

        while (detached_image_view_destroy)
        {
            vkDestroyImageView (system->device, detached_image_view_destroy->detached_image_view,
                                VULKAN_ALLOCATION_CALLBACKS (system));
            detached_image_view_destroy = detached_image_view_destroy->next;
        }

        struct scheduled_detached_image_destroy_t *detached_image_destroy =
            schedule->first_scheduled_detached_image_destroy;

        while (detached_image_destroy)
        {
#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
            transfer_memory_between_groups (detached_image_destroy->gpu_size,
                                            detached_image_destroy->gpu_allocation_group,
                                            system->memory_profiling.gpu_unmarked_group);
#endif

            vmaDestroyImage (system->gpu_memory_allocator, detached_image_destroy->detached_image,
                             detached_image_destroy->detached_allocation);
            detached_image_destroy = detached_image_destroy->next;
        }
    }

    struct render_backend_pass_t *pass = system->first_pass;
    while (pass)
    {
        struct render_backend_pass_t *next = pass->next;
        render_backend_system_destroy_pass (system, pass, KAN_FALSE);
        pass = next;
    }

    struct render_backend_buffer_t *buffer = system->first_buffer;
    while (buffer)
    {
        struct render_backend_buffer_t *next = buffer->next;
        render_backend_system_destroy_buffer (system, buffer, KAN_FALSE);
        buffer = next;
    }

    struct render_backend_frame_lifetime_allocator_t *frame_lifetime_allocator = system->first_frame_lifetime_allocator;
    while (frame_lifetime_allocator)
    {
        struct render_backend_frame_lifetime_allocator_t *next = frame_lifetime_allocator->next;
        render_backend_system_destroy_frame_lifetime_allocator (system, frame_lifetime_allocator, KAN_FALSE, KAN_FALSE);
        frame_lifetime_allocator = next;
    }

    struct render_backend_image_t *image = system->first_image;
    while (image)
    {
        struct render_backend_image_t *next = image->next;
        render_backend_system_destroy_image (system, image, KAN_FALSE);
        image = next;
    }

    if (system->device != VK_NULL_HANDLE)
    {
        render_backend_system_destroy_schedule_states (system);
        render_backend_system_destroy_command_states (system);
        render_backend_system_destroy_synchronization_objects (system);
        vmaDestroyAllocator (system->gpu_memory_allocator);
        vkDestroyDevice (system->device, VULKAN_ALLOCATION_CALLBACKS (system));
    }

    if (system->supported_devices)
    {
        kan_free_general (
            system->utility_allocation_group, system->supported_devices,
            sizeof (struct kan_render_supported_devices_t) +
                sizeof (struct kan_render_supported_device_info_t) * system->supported_devices->supported_device_count);
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
    if (system->debug_messenger != VK_NULL_HANDLE)
    {
        vkDestroyDebugUtilsMessengerEXT (system->instance, system->debug_messenger,
                                         VULKAN_ALLOCATION_CALLBACKS (system));
    }
#endif

    vkDestroyInstance (system->instance, VULKAN_ALLOCATION_CALLBACKS (system));
    kan_platform_application_unregister_vulkan_library_usage ();
}

void render_backend_system_disconnect (kan_context_system_handle_t handle)
{
}

void render_backend_system_destroy (kan_context_system_handle_t handle)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) handle;
    kan_free_general (system->main_allocation_group, system, sizeof (struct render_backend_system_t));
}

struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (render_backend_system_t) = {
    .name = "render_backend_system_t",
    .create = render_backend_system_create,
    .connect = render_backend_system_connect,
    .connected_init = render_backend_system_init,
    .connected_shutdown = render_backend_system_shutdown,
    .disconnect = render_backend_system_disconnect,
    .destroy = render_backend_system_destroy,
};

struct kan_render_supported_devices_t *kan_render_backend_system_get_devices (
    kan_context_system_handle_t render_backend_system)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) render_backend_system;
    return system->supported_devices;
}

kan_bool_t kan_render_backend_system_select_device (kan_context_system_handle_t render_backend_system,
                                                    kan_render_device_id_t device)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) render_backend_system;
    VkPhysicalDevice physical_device = (VkPhysicalDevice) device;

    if (system->device)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Caught attempt to select device after device was already sucessfully selected!")
        return KAN_FALSE;
    }

    uint32_t properties_count;
    if (vkEnumerateDeviceExtensionProperties (physical_device, NULL, &properties_count, NULL) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Unable to read physical device properties.")
        return KAN_FALSE;
    }

    VkExtensionProperties *properties =
        kan_allocate_general (system->utility_allocation_group, sizeof (VkExtensionProperties) * properties_count,
                              _Alignof (VkExtensionProperties));

    if (vkEnumerateDeviceExtensionProperties (physical_device, NULL, &properties_count, properties) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Unable to read physical device properties.")
        return KAN_FALSE;
    }

    kan_bool_t swap_chain_found = KAN_FALSE;
    for (uint32_t index = 0u; index < properties_count; ++index)
    {
        if (strcmp (properties[index].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
        {
            swap_chain_found = KAN_TRUE;
            break;
        }
    }

    kan_free_general (system->utility_allocation_group, properties, sizeof (VkExtensionProperties) * properties_count);
    if (!swap_chain_found)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to select device: requested device has no swap chain.")
        return KAN_FALSE;
    }

    uint32_t queues_count;
    vkGetPhysicalDeviceQueueFamilyProperties (physical_device, &queues_count, NULL);
    VkQueueFamilyProperties *queues =
        kan_allocate_general (system->utility_allocation_group, sizeof (VkQueueFamilyProperties) * queues_count,
                              _Alignof (VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties (physical_device, &queues_count, queues);

    system->device_graphics_queue_family_index = UINT32_MAX;
    system->device_transfer_queue_family_index = UINT32_MAX;

    for (uint32_t index = 0u; index < queues_count; ++index)
    {
        if (system->device_graphics_queue_family_index == UINT32_MAX &&
            (queues[index].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            system->device_graphics_queue_family_index = index;
        }

        if (system->device_transfer_queue_family_index == UINT32_MAX &&
            (queues[index].queueFlags & VK_QUEUE_TRANSFER_BIT))
        {
            system->device_transfer_queue_family_index = index;
        }
    }

    kan_free_general (system->utility_allocation_group, queues, sizeof (VkQueueFamilyProperties) * queues_count);
    if (system->device_graphics_queue_family_index == UINT32_MAX)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to select device: requested device has no graphics family.")
        return KAN_FALSE;
    }

    if (system->device_transfer_queue_family_index == UINT32_MAX)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to select device: requested device has no transfer family.")
        return KAN_FALSE;
    }

    static VkFormat depth_formats[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    system->device_depth_image_format = VK_FORMAT_UNDEFINED;

    for (uint64_t index = 0u; index < sizeof (depth_formats) / sizeof (depth_formats[0u]); ++index)
    {
        VkFormatProperties format_properties;
        vkGetPhysicalDeviceFormatProperties (physical_device, depth_formats[index], &format_properties);

        if (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            system->device_depth_image_format = depth_formats[index];
            break;
        }
    }

    system->device_depth_image_has_stencil = system->device_depth_image_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                                             system->device_depth_image_format == VK_FORMAT_D24_UNORM_S8_UINT;

    if (system->device_depth_image_format == VK_FORMAT_UNDEFINED)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to select device: unable to select depth format for device.")
        return KAN_FALSE;
    }

    float queues_priorities = 0u;
    VkDeviceQueueCreateInfo queues_create_info[] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = NULL,
            .queueFamilyIndex = system->device_graphics_queue_family_index,
            .queueCount = 1u,
            .pQueuePriorities = &queues_priorities,
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = NULL,
            .queueFamilyIndex = system->device_transfer_queue_family_index,
            .queueCount = 1u,
            .pQueuePriorities = &queues_priorities,
        },
    };

    uint32_t queues_to_create_count =
        system->device_graphics_queue_family_index == system->device_transfer_queue_family_index ? 1u : 2u;

    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures (physical_device, &device_features);
    const char *enabled_extensions_names[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .queueCreateInfoCount = queues_to_create_count,
        .pQueueCreateInfos = queues_create_info,
        .enabledLayerCount = 0u,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = sizeof (enabled_extensions_names) / sizeof (enabled_extensions_names[0u]),
        .ppEnabledExtensionNames = enabled_extensions_names,
        .pEnabledFeatures = &device_features,
    };

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
    const char *enabled_layers[] = {"VK_LAYER_KHRONOS_validation"};
    if (system->has_validation_layer)
    {
        device_create_info.enabledLayerCount = sizeof (enabled_layers) / sizeof (enabled_layers[0u]);
        device_create_info.ppEnabledLayerNames = enabled_layers;
    }
#endif

    if (vkCreateDevice (physical_device, &device_create_info, VULKAN_ALLOCATION_CALLBACKS (system), &system->device) !=
        VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to select device: failed to create logical device.")
        return KAN_FALSE;
    }

    system->device_memory_type = query_device_memory_type (physical_device);
    system->physical_device = physical_device;

    volkLoadDevice (system->device);
    vkGetDeviceQueue (system->device, system->device_graphics_queue_family_index, 0u, &system->graphics_queue);
    vkGetDeviceQueue (system->device, system->device_transfer_queue_family_index, 0u, &system->transfer_queue);

    system->gpu_memory_allocator_functions = (VmaVulkanFunctions) {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory = vkAllocateMemory,
        .vkFreeMemory = vkFreeMemory,
        .vkMapMemory = vkMapMemory,
        .vkUnmapMemory = vkUnmapMemory,
        .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
        .vkBindBufferMemory = vkBindBufferMemory,
        .vkBindImageMemory = vkBindImageMemory,
        .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
        .vkCreateBuffer = vkCreateBuffer,
        .vkDestroyBuffer = vkDestroyBuffer,
        .vkCreateImage = vkCreateImage,
        .vkDestroyImage = vkDestroyImage,
        .vkCmdCopyBuffer = vkCmdCopyBuffer,
    };

    VmaAllocatorCreateInfo allocator_create_info = {
        .flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT,
        .physicalDevice = physical_device,
        .device = system->device,
        .preferredLargeHeapBlockSize = 0u, // Use default.
#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
        .pAllocationCallbacks = &system->memory_profiling.vulkan_allocation_callbacks,
        .pDeviceMemoryCallbacks = &system->memory_profiling.vma_device_memory_callbacks,
#else
        .pAllocationCallbacks = NULL,
        .pDeviceMemoryCallbacks = NULL,
#endif
        .pHeapSizeLimit = NULL,
        .pVulkanFunctions = &system->gpu_memory_allocator_functions,
        .instance = system->instance,
        .pTypeExternalMemoryHandleTypes = NULL,
        .vulkanApiVersion = VK_MAKE_VERSION (1u, 1u, 0u),
    };

    if (vmaCreateAllocator (&allocator_create_info, &system->gpu_memory_allocator) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to select device: failed to create vulkan memory allocator.")

        vkDestroyDevice (system->device, VULKAN_ALLOCATION_CALLBACKS (system));
        system->device = VK_NULL_HANDLE;
        return KAN_FALSE;
    }

    VkSemaphoreCreateInfo semaphore_creation_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
    };

    VkFenceCreateInfo fence_creation_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (uint64_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        system->transfer_finished_semaphores[index] = VK_NULL_HANDLE;
        system->render_finished_semaphores[index] = VK_NULL_HANDLE;
        system->in_flight_fences[index] = VK_NULL_HANDLE;
    }

    kan_bool_t synchronization_objects_created = KAN_TRUE;
    for (uint64_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        if (vkCreateSemaphore (system->device, &semaphore_creation_info, VULKAN_ALLOCATION_CALLBACKS (system),
                               &system->transfer_finished_semaphores[index]) != VK_SUCCESS)
        {
            system->transfer_finished_semaphores[index] = VK_NULL_HANDLE;
            synchronization_objects_created = KAN_FALSE;
            break;
        }

        if (vkCreateSemaphore (system->device, &semaphore_creation_info, VULKAN_ALLOCATION_CALLBACKS (system),
                               &system->render_finished_semaphores[index]) != VK_SUCCESS)
        {
            system->render_finished_semaphores[index] = VK_NULL_HANDLE;
            synchronization_objects_created = KAN_FALSE;
            break;
        }

        if (vkCreateFence (system->device, &fence_creation_info, VULKAN_ALLOCATION_CALLBACKS (system),
                           &system->in_flight_fences[index]) != VK_SUCCESS)
        {
            system->in_flight_fences[index] = VK_NULL_HANDLE;
            synchronization_objects_created = KAN_FALSE;
            break;
        }
    }

    if (!synchronization_objects_created)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to select device: failed to create synchronization objects.")

        render_backend_system_destroy_synchronization_objects (system);
        vmaDestroyAllocator (system->gpu_memory_allocator);
        vkDestroyDevice (system->device, VULKAN_ALLOCATION_CALLBACKS (system));
        system->device = VK_NULL_HANDLE;
        return KAN_FALSE;
    }

    for (uint64_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        system->command_states[index].graphics_command_pool = VK_NULL_HANDLE;
        system->command_states[index].transfer_command_pool = VK_NULL_HANDLE;
    }

    kan_bool_t command_states_created = KAN_TRUE;
    for (uint64_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        VkCommandPoolCreateInfo graphics_command_pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .queueFamilyIndex = system->device_graphics_queue_family_index,
        };

        if (vkCreateCommandPool (system->device, &graphics_command_pool_info, VULKAN_ALLOCATION_CALLBACKS (system),
                                 &system->command_states[index].graphics_command_pool))
        {
            system->command_states[index].graphics_command_pool = VK_NULL_HANDLE;
            command_states_created = KAN_FALSE;
            break;
        }

        VkCommandBufferAllocateInfo graphics_primary_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = system->command_states[index].graphics_command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1u,
        };

        if (vkAllocateCommandBuffers (system->device, &graphics_primary_buffer_info,
                                      &system->command_states[index].primary_graphics_command_buffer))
        {
            command_states_created = KAN_FALSE;
            break;
        }

        VkCommandPoolCreateInfo transfer_command_pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .queueFamilyIndex = system->device_transfer_queue_family_index,
        };

        if (vkCreateCommandPool (system->device, &transfer_command_pool_info, VULKAN_ALLOCATION_CALLBACKS (system),
                                 &system->command_states[index].transfer_command_pool))
        {
            system->command_states[index].transfer_command_pool = VK_NULL_HANDLE;
            command_states_created = KAN_FALSE;
            break;
        }

        VkCommandBufferAllocateInfo transfer_primary_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = system->command_states[index].transfer_command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1u,
        };

        if (vkAllocateCommandBuffers (system->device, &transfer_primary_buffer_info,
                                      &system->command_states[index].primary_transfer_command_buffer))
        {
            command_states_created = KAN_FALSE;
            break;
        }
    }

    if (!command_states_created)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to select device: failed to create command states.")

        render_backend_system_destroy_command_states (system);
        render_backend_system_destroy_synchronization_objects (system);
        vmaDestroyAllocator (system->gpu_memory_allocator);
        vkDestroyDevice (system->device, VULKAN_ALLOCATION_CALLBACKS (system));
        system->device = VK_NULL_HANDLE;
        return KAN_FALSE;
    }

    for (uint64_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        struct render_backend_schedule_state_t *state = &system->schedule_states[index];
        kan_stack_group_allocator_init (&state->item_allocator, system->schedule_allocation_group,
                                        KAN_CONTEXT_RENDER_BACKEND_SCHEDULE_STACK_SIZE);
        state->schedule_lock = kan_atomic_int_init (0);

        state->first_scheduled_buffer_unmap_flush_transfer = NULL;
        state->first_scheduled_buffer_unmap_flush = NULL;
        state->first_scheduled_image_upload = NULL;
        state->first_scheduled_frame_buffer_create = NULL;
        state->first_scheduled_image_mip_generation = NULL;

        state->first_scheduled_frame_buffer_destroy = NULL;
        state->first_scheduled_detached_frame_buffer_destroy = NULL;
        state->first_scheduled_pass_destroy = NULL;
        state->first_scheduled_buffer_destroy = NULL;
        state->first_scheduled_frame_lifetime_allocator_destroy = NULL;
        state->first_scheduled_detached_image_view_destroy = NULL;
        state->first_scheduled_image_destroy = NULL;
        state->first_scheduled_detached_image_destroy = NULL;
    }

    system->staging_frame_lifetime_allocator = render_backend_system_create_frame_lifetime_allocator (
        system, RENDER_BACKEND_BUFFER_FAMILY_STAGING,
        // Buffer type does not matter anything for staging.
        KAN_RENDER_BUFFER_TYPE_STORAGE, KAN_CONTEXT_RENDER_BACKEND_STAGING_PAGE_SIZE,
        kan_string_intern ("default_staging_buffer"));
    return KAN_TRUE;
}

kan_render_context_t kan_render_backend_system_get_render_context (kan_context_system_handle_t render_backend_system)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) render_backend_system;
    return system->render_enabled ? (kan_render_context_t) system : KAN_INVALID_RENDER_CONTEXT;
}

static void render_backend_system_submit_transfer (struct render_backend_system_t *system)
{
    VkCommandBufferBeginInfo buffer_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = 0u,
        .pInheritanceInfo = NULL,
    };

    struct render_backend_command_state_t *state = &system->command_states[system->current_frame_in_flight_index];
    if (vkBeginCommandBuffer (state->primary_transfer_command_buffer, &buffer_begin_info) != VK_SUCCESS)
    {
        kan_critical_error ("Failed to start recording primary transfer buffer.", __FILE__, __LINE__);
    }

    struct render_backend_schedule_state_t *schedule = &system->schedule_states[system->current_frame_in_flight_index];
    struct scheduled_buffer_unmap_flush_transfer_t *buffer_unmap_flush_transfer =
        schedule->first_scheduled_buffer_unmap_flush_transfer;

    while (buffer_unmap_flush_transfer)
    {
        vmaUnmapMemory (system->gpu_memory_allocator, buffer_unmap_flush_transfer->source_buffer->allocation);
        if (vmaFlushAllocation (system->gpu_memory_allocator, buffer_unmap_flush_transfer->source_buffer->allocation,
                                buffer_unmap_flush_transfer->source_offset,
                                buffer_unmap_flush_transfer->size) != VK_SUCCESS)
        {
            kan_critical_error ("Unexpected failure while flushing buffer data, unable to continue properly.", __FILE__,
                                __LINE__);
        }

        struct VkBufferCopy region = {
            .srcOffset = buffer_unmap_flush_transfer->source_offset,
            .dstOffset = buffer_unmap_flush_transfer->target_offset,
            .size = buffer_unmap_flush_transfer->size,
        };

        vkCmdCopyBuffer (state->primary_transfer_command_buffer, buffer_unmap_flush_transfer->source_buffer->buffer,
                         buffer_unmap_flush_transfer->target_buffer->buffer, 1u, &region);
        buffer_unmap_flush_transfer = buffer_unmap_flush_transfer->next;
    }

    schedule->first_scheduled_buffer_unmap_flush_transfer = NULL;
    struct scheduled_buffer_unmap_flush_t *buffer_unmap_flush = schedule->first_scheduled_buffer_unmap_flush;

    while (buffer_unmap_flush)
    {
        vmaUnmapMemory (system->gpu_memory_allocator, buffer_unmap_flush->buffer->allocation);
        if (vmaFlushAllocation (system->gpu_memory_allocator, buffer_unmap_flush->buffer->allocation,
                                buffer_unmap_flush->offset, buffer_unmap_flush->size) != VK_SUCCESS)
        {
            kan_critical_error ("Unexpected failure while flushing buffer data, unable to continue properly.", __FILE__,
                                __LINE__);
        }

        buffer_unmap_flush = buffer_unmap_flush->next;
    }

    schedule->first_scheduled_buffer_unmap_flush = NULL;
    struct scheduled_image_upload_t *image_upload = schedule->first_scheduled_image_upload;

    while (image_upload)
    {
        VkImageAspectFlags image_aspect =
            kan_render_image_description_calculate_aspects (system, &image_upload->image->description);

        uint64_t width;
        uint64_t height;
        uint64_t depth;
        kan_render_image_description_calculate_size_at_mip (&image_upload->image->description, image_upload->mip,
                                                            &width, &height, &depth);

        VkImageMemoryBarrier prepare_transfer_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = 0u,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image_upload->image->image,
            .subresourceRange =
                {
                    .aspectMask = image_aspect,
                    .baseMipLevel = (uint32_t) image_upload->mip,
                    .levelCount = 1u,
                    .baseArrayLayer = 0u,
                    .layerCount = 1u,
                },
        };

        vkCmdPipelineBarrier (state->primary_transfer_command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &prepare_transfer_barrier);

        VkBufferImageCopy copy_region = {
            .bufferOffset = (uint32_t) image_upload->staging_buffer_offset,
            .bufferRowLength = 0u,
            .bufferImageHeight = 0u,
            .imageSubresource =
                {
                    .aspectMask = image_aspect,
                    .mipLevel = (uint32_t) image_upload->mip,
                    .baseArrayLayer = 0u,
                    .layerCount = 1u,
                },
            .imageOffset =
                {
                    .x = 0u,
                    .y = 0u,
                    .z = 0u,
                },
            .imageExtent =
                {
                    .width = (uint32_t) width,
                    .height = (uint32_t) height,
                    .depth = (uint32_t) depth,
                },
        };

        vkCmdCopyBufferToImage (state->primary_transfer_command_buffer, image_upload->staging_buffer->buffer,
                                image_upload->image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copy_region);

        VkImageMemoryBarrier finish_transfer_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = 0u,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image_upload->image->image,
            .subresourceRange =
                {
                    .aspectMask = image_aspect,
                    .baseMipLevel = (uint32_t) image_upload->mip,
                    .levelCount = 1u,
                    .baseArrayLayer = 0u,
                    .layerCount = 1u,
                },
        };

        vkCmdPipelineBarrier (state->primary_transfer_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, NULL, 0u, NULL, 1u,
                              &finish_transfer_barrier);

        image_upload = image_upload->next;
    }

    schedule->first_scheduled_image_upload = NULL;
    if (vkEndCommandBuffer (state->primary_transfer_command_buffer) != VK_SUCCESS)
    {
        kan_critical_error ("Failed to end recording primary transfer buffer.", __FILE__, __LINE__);
    }

    uint32_t semaphores_to_wait = 0u;
    static VkSemaphore static_wait_semaphores[KAN_CONTEXT_RENDER_BACKEND_MAX_INLINE_HANDLES];
    static VkPipelineStageFlags static_semaphore_stages[KAN_CONTEXT_RENDER_BACKEND_MAX_INLINE_HANDLES];

    VkSemaphore *wait_semaphores = static_wait_semaphores;
    VkPipelineStageFlags *semaphore_stages = static_semaphore_stages;
    struct render_backend_surface_t *surface = system->first_surface;

    while (surface)
    {
        if (surface->surface != VK_NULL_HANDLE)
        {
            if (semaphores_to_wait < KAN_CONTEXT_RENDER_BACKEND_MAX_INLINE_HANDLES)
            {
                wait_semaphores[semaphores_to_wait] =
                    surface->image_available_semaphores[system->current_frame_in_flight_index];
                semaphore_stages[semaphores_to_wait] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            }

            ++semaphores_to_wait;
        }

        surface = surface->next;
    }

    if (semaphores_to_wait > KAN_CONTEXT_RENDER_BACKEND_MAX_INLINE_HANDLES)
    {
        // Too many semaphores to capture everything to static array, allocate new one.
        wait_semaphores = kan_allocate_general (system->utility_allocation_group,
                                                sizeof (VkSemaphore) * semaphores_to_wait, _Alignof (VkSemaphore));
        semaphore_stages =
            kan_allocate_general (system->utility_allocation_group, sizeof (VkPipelineStageFlags) * semaphores_to_wait,
                                  _Alignof (VkPipelineStageFlags));
        semaphores_to_wait = 0u;

        surface = system->first_surface;

        while (surface)
        {
            if (surface->surface != VK_NULL_HANDLE)
            {
                wait_semaphores[semaphores_to_wait] =
                    surface->image_available_semaphores[system->current_frame_in_flight_index];
                semaphore_stages[semaphores_to_wait] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                ++semaphores_to_wait;
            }

            surface = surface->next;
        }
    }

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = semaphores_to_wait,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = semaphore_stages,
        .commandBufferCount = 1u,
        .pCommandBuffers = &state->primary_transfer_command_buffer,
        .signalSemaphoreCount = 1u,
        .pSignalSemaphores = &system->transfer_finished_semaphores[system->current_frame_in_flight_index],
    };

    if (vkQueueSubmit (system->transfer_queue, 1u, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
    {
        kan_critical_error ("Failed to submit work to transfer queue.", __FILE__, __LINE__);
    }

    if (wait_semaphores != static_wait_semaphores)
    {
        kan_free_general (system->utility_allocation_group, wait_semaphores, sizeof (VkSemaphore) * semaphores_to_wait);
        kan_free_general (system->utility_allocation_group, semaphore_stages,
                          sizeof (VkPipelineStageFlags) * semaphores_to_wait);
    }
}

static VkImageLayout get_surface_layout_from_render_state (enum surface_render_state_t state)
{
    switch (state)
    {
    case SURFACE_RENDER_STATE_RECEIVED_NO_OUTPUT:
        return VK_IMAGE_LAYOUT_UNDEFINED;

    case SURFACE_RENDER_STATE_RECEIVED_DATA_FROM_FRAME_BUFFER:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    case SURFACE_RENDER_STATE_RECEIVED_DATA_FROM_BLIT:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }

    KAN_ASSERT (KAN_FALSE)
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

static inline void submit_mip_generation (struct render_backend_system_t *system,
                                          struct render_backend_schedule_state_t *schedule,
                                          struct render_backend_command_state_t *state)
{
    struct scheduled_image_mip_generation_t *image_mip_generation = schedule->first_scheduled_image_mip_generation;

    while (image_mip_generation)
    {
        VkImageAspectFlags image_aspect =
            kan_render_image_description_calculate_aspects (system, &image_mip_generation->image->description);

        for (uint64_t output_mip = image_mip_generation->first + 1u; output_mip <= image_mip_generation->last;
             ++output_mip)
        {
            const uint64_t input_mip = output_mip - 1u;
            uint64_t input_width;
            uint64_t input_height;
            uint64_t input_depth;
            kan_render_image_description_calculate_size_at_mip (&image_mip_generation->image->description, input_mip,
                                                                &input_width, &input_height, &input_depth);

            uint64_t output_width;
            uint64_t output_height;
            uint64_t output_depth;
            kan_render_image_description_calculate_size_at_mip (&image_mip_generation->image->description, output_mip,
                                                                &output_width, &output_height, &output_depth);

            VkImageMemoryBarrier input_to_transfer_source_barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = input_mip == image_mip_generation->first ? 0u : VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .oldLayout = input_mip == image_mip_generation->first ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL :
                                                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image_mip_generation->image->image,
                .subresourceRange =
                    {
                        .aspectMask = image_aspect,
                        .baseMipLevel = (uint32_t) input_mip,
                        .levelCount = 1u,
                        .baseArrayLayer = 0u,
                        .layerCount = 1u,
                    },
            };

            vkCmdPipelineBarrier (state->primary_graphics_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u, NULL, 1u,
                                  &input_to_transfer_source_barrier);

            VkImageMemoryBarrier output_to_transfer_destination_barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = 0u,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image_mip_generation->image->image,
                .subresourceRange =
                    {
                        .aspectMask = image_aspect,
                        .baseMipLevel = (uint32_t) output_mip,
                        .levelCount = 1u,
                        .baseArrayLayer = 0u,
                        .layerCount = 1u,
                    },
            };

            vkCmdPipelineBarrier (state->primary_graphics_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u, NULL, 1u,
                                  &output_to_transfer_destination_barrier);

            VkImageBlit image_blit = {
                .srcSubresource =
                    {
                        .aspectMask = image_aspect,
                        .mipLevel = (uint32_t) input_mip,
                        .baseArrayLayer = 0u,
                        .layerCount = 1u,
                    },
                .srcOffsets =
                    {
                        {
                            .x = 0,
                            .y = 0,
                            .z = 0,
                        },
                        {
                            .x = (int32_t) input_width,
                            .y = (int32_t) input_height,
                            .z = (int32_t) input_depth,
                        },
                    },
                .dstSubresource =
                    {
                        .aspectMask = image_aspect,
                        .mipLevel = (uint32_t) output_mip,
                        .baseArrayLayer = 0u,
                        .layerCount = 1u,
                    },
                .dstOffsets =
                    {
                        {
                            .x = 0,
                            .y = 0,
                            .z = 0,
                        },
                        {
                            .x = (int32_t) output_width,
                            .y = (int32_t) output_height,
                            .z = (int32_t) output_depth,
                        },
                    },
            };

            vkCmdBlitImage (state->primary_graphics_command_buffer, image_mip_generation->image->image,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image_mip_generation->image->image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &image_blit, VK_FILTER_LINEAR);

            VkImageMemoryBarrier input_to_read_only_barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .dstAccessMask = 0u,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image_mip_generation->image->image,
                .subresourceRange =
                    {
                        .aspectMask = image_aspect,
                        .baseMipLevel = (uint32_t) input_mip,
                        .levelCount = 1u,
                        .baseArrayLayer = 0u,
                        .layerCount = 1u,
                    },
            };

            vkCmdPipelineBarrier (state->primary_graphics_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, NULL, 0u, NULL, 1u,
                                  &input_to_read_only_barrier);
        }

        VkImageMemoryBarrier last_to_read_only_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = 0u,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image_mip_generation->image->image,
            .subresourceRange =
                {
                    .aspectMask = image_aspect,
                    .baseMipLevel = (uint32_t) image_mip_generation->last,
                    .levelCount = 1u,
                    .baseArrayLayer = 0u,
                    .layerCount = 1u,
                },
        };

        vkCmdPipelineBarrier (state->primary_graphics_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, NULL, 0u, NULL, 1u,
                              &last_to_read_only_barrier);

        image_mip_generation = image_mip_generation->next;
    }

    schedule->first_scheduled_image_mip_generation = NULL;
}

static inline void process_frame_buffer_create_requests (struct render_backend_system_t *system,
                                                         struct render_backend_schedule_state_t *schedule,
                                                         struct render_backend_command_state_t *state)
{
    struct scheduled_frame_buffer_create_t *frame_buffer_create = schedule->first_scheduled_frame_buffer_create;
    while (frame_buffer_create)
    {
        struct render_backend_frame_buffer_t *frame_buffer = frame_buffer_create->frame_buffer;
        const kan_bool_t already_created =
            frame_buffer->instance != VK_NULL_HANDLE || frame_buffer->instance_array != NULL;

        if (already_created)
        {
            frame_buffer_create = frame_buffer_create->next;
            continue;
        }

        kan_bool_t can_be_created = KAN_TRUE;
        uint64_t instance_count = 1u;
        uint32_t width = 0u;
        uint32_t height = 0u;
        uint64_t surface_index = UINT64_MAX;

        for (uint64_t attachment_index = 0u; attachment_index < frame_buffer->attachments_count; ++attachment_index)
        {
            if (frame_buffer->attachments[attachment_index].type == KAN_FRAME_BUFFER_ATTACHMENT_SURFACE)
            {
                if (surface_index == UINT64_MAX)
                {
                    instance_count = frame_buffer->attachments[attachment_index].surface->images_count;
                    surface_index = attachment_index;

                    if (frame_buffer->attachments[attachment_index].surface->surface == VK_NULL_HANDLE)
                    {
                        // Surface is not ready yet, creation will be automatically requested when surface is ready.
                        can_be_created = KAN_FALSE;
                        break;
                    }
                }
                else
                {
                    can_be_created = KAN_FALSE;
                    KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                             "Unable to create frame buffer \"%s\" as it has more than one surface attachment which is "
                             "not supported.",
                             frame_buffer->tracking_name)
                }
            }

            uint32_t attachment_width;
            uint32_t attachment_height;

            switch (frame_buffer->attachments[attachment_index].type)
            {
            case KAN_FRAME_BUFFER_ATTACHMENT_IMAGE:
                attachment_width = (uint32_t) frame_buffer->attachments[attachment_index].image->description.width;
                attachment_height = (uint32_t) frame_buffer->attachments[attachment_index].image->description.height;
                break;

            case KAN_FRAME_BUFFER_ATTACHMENT_SURFACE:
                attachment_width =
                    (uint32_t) frame_buffer->attachments[attachment_index].surface->swap_chain_creation_window_width;
                attachment_height =
                    (uint32_t) frame_buffer->attachments[attachment_index].surface->swap_chain_creation_window_height;
                break;
            }

            if (attachment_index == 0u)
            {
                width = attachment_width;
                height = attachment_height;
            }
            else if (attachment_width != width || attachment_height != height)
            {
                can_be_created = KAN_FALSE;
                KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                         "Unable to create frame buffer \"%s\" as its attachment %lu has size %lux%lu while previous "
                         "attachments have size %lux%lu.",
                         frame_buffer->tracking_name, (unsigned long) attachment_index,
                         (unsigned long) attachment_width, (unsigned long) attachment_height, (unsigned long) width,
                         (unsigned long) height)
            }
        }

        if (!can_be_created)
        {
            frame_buffer_create = frame_buffer_create->next;
            continue;
        }

        if (instance_count > 1u)
        {
            frame_buffer->instance_array_size = instance_count;
            frame_buffer->instance_array = kan_allocate_general (
                system->frame_buffer_wrapper_allocation_group,
                sizeof (VkFramebuffer) * frame_buffer->instance_array_size, _Alignof (VkFramebuffer));
        }

        frame_buffer->image_views =
            kan_allocate_general (system->frame_buffer_wrapper_allocation_group,
                                  sizeof (VkImageView) * frame_buffer->attachments_count, _Alignof (VkImageView));

        for (uint64_t attachment_index = 0u; attachment_index < frame_buffer->attachments_count; ++attachment_index)
        {
            switch (frame_buffer->attachments[attachment_index].type)
            {
            case KAN_FRAME_BUFFER_ATTACHMENT_IMAGE:
            {
                struct render_backend_image_t *image = frame_buffer->attachments[attachment_index].image;
                VkImageViewType image_view_type = VK_IMAGE_VIEW_TYPE_2D;

                switch (image->description.type)
                {
                case KAN_RENDER_IMAGE_TYPE_COLOR_2D:
                    image_view_type = VK_IMAGE_VIEW_TYPE_2D;
                    break;

                case KAN_RENDER_IMAGE_TYPE_COLOR_3D:
                    image_view_type = VK_IMAGE_VIEW_TYPE_3D;
                    break;

                case KAN_RENDER_IMAGE_TYPE_DEPTH_STENCIL:
                    image_view_type = VK_IMAGE_VIEW_TYPE_2D;
                    break;
                }

                VkImageViewCreateInfo create_info = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .pNext = NULL,
                    .flags = 0u,
                    .image = frame_buffer->attachments[attachment_index].image->image,
                    .viewType = image_view_type,
                    .format = kan_render_image_description_calculate_format (system, &image->description),
                    .components =
                        {
                            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                        },
                    .subresourceRange =
                        {
                            .aspectMask = kan_render_image_description_calculate_aspects (system, &image->description),
                            .baseMipLevel = 0u,
                            .levelCount = 1u,
                            .baseArrayLayer = 0u,
                            .layerCount = 1u,
                        },
                };

                if (vkCreateImageView (system->device, &create_info, VULKAN_ALLOCATION_CALLBACKS (system),
                                       &frame_buffer->image_views[attachment_index]) != VK_SUCCESS)
                {
                    can_be_created = KAN_FALSE;
                    KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                             "Unable to create frame buffer \"%s\" due to failure when creating image view for "
                             "attachment %lu.",
                             frame_buffer->tracking_name, (unsigned long) attachment_index)
                }

                break;
            }

            case KAN_FRAME_BUFFER_ATTACHMENT_SURFACE:
                frame_buffer->image_views[attachment_index] = VK_NULL_HANDLE;
                break;
            }
        }

        if (!can_be_created)
        {
            render_backend_frame_buffer_destroy_resources (system, frame_buffer);
            frame_buffer_create = frame_buffer_create->next;
            continue;
        }

        kan_bool_t created = KAN_TRUE;
        for (uint64_t instance_index = 0u; instance_index < instance_count; ++instance_index)
        {
            if (surface_index != UINT64_MAX)
            {
                frame_buffer->image_views[surface_index] =
                    frame_buffer->attachments[surface_index].surface->image_views[instance_index];
            }

            VkFramebufferCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext = NULL,
                .flags = 0u,
                .renderPass = frame_buffer->pass->pass,
                .attachmentCount = (uint32_t) frame_buffer->attachments_count,
                .pAttachments = frame_buffer->image_views,
                .width = width,
                .height = height,
                .layers = 1u,
            };

            VkFramebuffer *output =
                instance_count == 1u ? &frame_buffer->instance : &frame_buffer->instance_array[instance_index];

            if (vkCreateFramebuffer (system->device, &create_info, VULKAN_ALLOCATION_CALLBACKS (system), output) !=
                VK_SUCCESS)
            {
                created = KAN_FALSE;
                *output = VK_NULL_HANDLE;

                KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                         "Unable to create frame buffer \"%s\" due to failure when creating instance %lu.",
                         frame_buffer->tracking_name, (unsigned long) instance_index)
            }
        }

        frame_buffer->image_views[surface_index] = VK_NULL_HANDLE;
        if (!created)
        {
            render_backend_frame_buffer_destroy_resources (system, frame_buffer);
        }

        frame_buffer_create = frame_buffer_create->next;
    }

    schedule->first_scheduled_frame_buffer_create = NULL;
}

static inline void process_surface_blit_requests (struct render_backend_system_t *system,
                                                  struct render_backend_command_state_t *state)
{
    struct render_backend_surface_t *surface = system->first_surface;
    while (surface)
    {
        struct surface_blit_request_t *request = surface->first_blit_request;
        while (request)
        {
            if (!request->image->switched_to_transfer_source)
            {
                if (request->image->description.type != KAN_RENDER_IMAGE_TYPE_COLOR_2D)
                {
                    KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                             "Caught attempt to blit image to present surface where image is not color 2d image!")
                }
                else
                {
                    VkImageMemoryBarrier barrier_info = {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        .pNext = NULL,
                        .srcAccessMask = 0u,
                        .dstAccessMask = 0u,
                        .oldLayout = request->image->description.render_target ?
                                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image = request->image->image,
                        .subresourceRange =
                            {
                                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .baseMipLevel = 0u,
                                .levelCount = 1u,
                                .baseArrayLayer = 0u,
                                .layerCount = 1u,
                            },
                    };

                    vkCmdPipelineBarrier (state->primary_graphics_command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, NULL, 0u, NULL, 1u,
                                          &barrier_info);
                    request->image->switched_to_transfer_source = KAN_TRUE;
                }
            }

            // Prepare destination surface image.
            if (surface->render_state != SURFACE_RENDER_STATE_RECEIVED_DATA_FROM_BLIT)
            {
                VkImageMemoryBarrier barrier_info = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .pNext = NULL,
                    .srcAccessMask = 0u,
                    .dstAccessMask = 0u,
                    .oldLayout = get_surface_layout_from_render_state (surface->render_state),
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = surface->images[surface->acquired_image_index],
                    .subresourceRange =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .baseMipLevel = 0u,
                            .levelCount = 1u,
                            .baseArrayLayer = 0u,
                            .layerCount = 1u,
                        },
                };

                vkCmdPipelineBarrier (state->primary_graphics_command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &barrier_info);
                surface->render_state = SURFACE_RENDER_STATE_RECEIVED_DATA_FROM_BLIT;
            }

            request = request->next;
        }

        surface = surface->next;
    }

    surface = system->first_surface;
    while (surface)
    {
        struct surface_blit_request_t *request = surface->first_blit_request;
        while (request)
        {
            VkImageBlit image_blit = {
                .srcSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = 0u,
                        .baseArrayLayer = 0u,
                        .layerCount = 1u,
                    },
                .srcOffsets =
                    {
                        {
                            .x = (int32_t) request->image_region.x,
                            .y = (int32_t) request->image_region.y,
                            .z = 0,
                        },
                        {
                            .x = (int32_t) request->image_region.width,
                            .y = (int32_t) request->image_region.height,
                            .z = 1u,
                        },
                    },
                .dstSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = 0u,
                        .baseArrayLayer = 0u,
                        .layerCount = 1u,
                    },
                .dstOffsets =
                    {
                        {
                            .x = (int32_t) request->surface_region.x,
                            .y = (int32_t) request->surface_region.y,
                            .z = 0,
                        },
                        {
                            .x = (int32_t) request->surface_region.width,
                            .y = (int32_t) request->surface_region.height,
                            .z = 1u,
                        },
                    },
            };

            vkCmdBlitImage (state->primary_graphics_command_buffer, request->image->image,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, surface->images[surface->acquired_image_index],
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &image_blit, VK_FILTER_LINEAR);
            request = request->next;
        }

        surface = surface->next;
    }

    surface = system->first_surface;
    while (surface)
    {
        struct surface_blit_request_t *request = surface->first_blit_request;
        while (request)
        {
            struct surface_blit_request_t *next = request->next;
            if (request->image->switched_to_transfer_source)
            {
                // Render targets do not care as they'll be overwritten next frame anyway.
                if (!request->image->description.render_target)
                {
                    VkImageMemoryBarrier barrier_info = {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        .pNext = NULL,
                        .srcAccessMask = 0u,
                        .dstAccessMask = 0u,
                        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image = request->image->image,
                        .subresourceRange =
                            {
                                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .baseMipLevel = 0u,
                                .levelCount = 1u,
                                .baseArrayLayer = 0u,
                                .layerCount = 1u,
                            },
                    };

                    vkCmdPipelineBarrier (state->primary_graphics_command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, NULL, 0u, NULL, 1u,
                                          &barrier_info);
                }

                request->image->switched_to_transfer_source = KAN_FALSE;
            }

            kan_free_batched (system->surface_wrapper_allocation_group, request);
            request = next;
        }

        surface->first_blit_request = NULL;
        surface = surface->next;
    }
}

static void render_backend_system_submit_graphics (struct render_backend_system_t *system)
{
    VkCommandBufferBeginInfo buffer_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = 0u,
        .pInheritanceInfo = NULL,
    };

    struct render_backend_command_state_t *state = &system->command_states[system->current_frame_in_flight_index];
    if (vkBeginCommandBuffer (state->primary_graphics_command_buffer, &buffer_begin_info) != VK_SUCCESS)
    {
        kan_critical_error ("Failed to start recording primary graphics buffer.", __FILE__, __LINE__);
    }

    struct render_backend_surface_t *surface = system->first_surface;
    while (surface)
    {
        surface->render_state = SURFACE_RENDER_STATE_RECEIVED_NO_OUTPUT;
        surface = surface->next;
    }

    struct render_backend_schedule_state_t *schedule = &system->schedule_states[system->current_frame_in_flight_index];
    // Mip generation must be done in graphics queue.
    submit_mip_generation (system, schedule, state);
    process_frame_buffer_create_requests (system, schedule, state);

    // TODO: Fill buffer with accumulated graphics commands.

    process_surface_blit_requests (system, state);
    surface = system->first_surface;

    while (surface)
    {
        // We need to transition swap chain images into present layout.
        // If surface image didn't receive any output, we don't care about old layout.
        // Otherwise, it must be properly transitioned.
        VkImageMemoryBarrier barrier_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = 0u,
            .dstAccessMask = 0u,
            .oldLayout = get_surface_layout_from_render_state (surface->render_state),
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = surface->images[surface->acquired_image_index],
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0u,
                    .levelCount = 1u,
                    .baseArrayLayer = 0u,
                    .layerCount = 1u,
                },
        };

        vkCmdPipelineBarrier (state->primary_graphics_command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &barrier_info);
        surface = surface->next;
    }

    if (vkEndCommandBuffer (state->primary_graphics_command_buffer) != VK_SUCCESS)
    {
        kan_critical_error ("Failed to end recording primary graphics buffer.", __FILE__, __LINE__);
    }

    VkSemaphore wait_semaphores[] = {system->transfer_finished_semaphores[system->current_frame_in_flight_index]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = sizeof (wait_semaphores) / sizeof (wait_semaphores[0u]),
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1u,
        .pCommandBuffers = &state->primary_graphics_command_buffer,
        .signalSemaphoreCount = 1u,
        .pSignalSemaphores = &system->render_finished_semaphores[system->current_frame_in_flight_index],
    };

    if (vkQueueSubmit (system->graphics_queue, 1u, &submit_info,
                       system->in_flight_fences[system->current_frame_in_flight_index]) != VK_SUCCESS)
    {
        kan_critical_error ("Failed to submit work to graphics queue.", __FILE__, __LINE__);
    }
}

static void render_backend_system_submit_present (struct render_backend_system_t *system)
{
    static VkSwapchainKHR static_swap_chains[KAN_CONTEXT_RENDER_BACKEND_MAX_INLINE_HANDLES];
    static uint32_t static_image_indices[KAN_CONTEXT_RENDER_BACKEND_MAX_INLINE_HANDLES];

    uint32_t swap_chains_count = 0u;
    VkSwapchainKHR *swap_chains = static_swap_chains;
    uint32_t *image_indices = static_image_indices;
    struct render_backend_surface_t *surface = system->first_surface;

    while (surface)
    {
        if (surface->surface != VK_NULL_HANDLE)
        {
            if (swap_chains_count < KAN_CONTEXT_RENDER_BACKEND_MAX_INLINE_HANDLES)
            {
                swap_chains[swap_chains_count] = surface->swap_chain;
                image_indices[swap_chains_count] = surface->acquired_image_index;
            }

            ++swap_chains_count;
        }

        surface = surface->next;
    }

    if (swap_chains_count > KAN_CONTEXT_RENDER_BACKEND_MAX_INLINE_HANDLES)
    {
        // Too many surfaces to capture everything to static array, allocate new one.
        swap_chains = kan_allocate_general (system->utility_allocation_group, sizeof (VkSemaphore) * swap_chains_count,
                                            _Alignof (VkSwapchainKHR));
        image_indices = kan_allocate_general (system->utility_allocation_group, sizeof (uint32_t) * swap_chains_count,
                                              _Alignof (uint32_t));
        swap_chains_count = 0u;
        surface = system->first_surface;

        while (surface)
        {
            if (surface->surface != VK_NULL_HANDLE)
            {
                swap_chains[swap_chains_count] = surface->swap_chain;
                image_indices[swap_chains_count] = surface->acquired_image_index;
                ++swap_chains_count;
            }

            surface = surface->next;
        }
    }

    VkSemaphore wait_semaphores[] = {system->render_finished_semaphores[system->current_frame_in_flight_index]};
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = sizeof (wait_semaphores) / sizeof (wait_semaphores[0u]),
        .pWaitSemaphores = wait_semaphores,
        .swapchainCount = swap_chains_count,
        .pSwapchains = swap_chains,
        .pImageIndices = image_indices,
        .pResults = NULL,
    };

    if (vkQueuePresentKHR (system->graphics_queue, &present_info) != VK_SUCCESS)
    {
        kan_critical_error ("Failed to request present operations.", __FILE__, __LINE__);
    }

    if (swap_chains != static_swap_chains)
    {
        kan_free_general (system->utility_allocation_group, swap_chains, sizeof (VkSwapchainKHR) * swap_chains_count);
        kan_free_general (system->utility_allocation_group, image_indices, sizeof (image_indices) * swap_chains_count);
    }
}

static void render_backend_system_clean_current_schedule_if_safe (struct render_backend_system_t *system)
{
    // Due to the fact that out-of-frame scheduling is allowed, for example frame was skipped but user wants to schedule
    // geometry upload, we do not have a point in schedules where it is guaranteed to be safe to reset schedule
    // allocator. Therefore, we have separate logic to check whether it is safe and reset only when it is safe.

    struct render_backend_schedule_state_t *schedule = &system->schedule_states[system->current_frame_in_flight_index];
    // Always shrink allocator while it is not empty. We need to reduce memory footprint in case previous executions
    // took too much space and we don't need it anymore.
    kan_stack_group_allocator_shrink (&schedule->item_allocator);

    if (!schedule->first_scheduled_buffer_unmap_flush_transfer && !schedule->first_scheduled_buffer_unmap_flush &&
        !schedule->first_scheduled_image_upload && !schedule->first_scheduled_frame_buffer_create &&
        !schedule->first_scheduled_image_mip_generation && !schedule->first_scheduled_frame_buffer_destroy &&
        !schedule->first_scheduled_detached_frame_buffer_destroy && !schedule->first_scheduled_pass_destroy &&
        !schedule->first_scheduled_buffer_destroy && !schedule->first_scheduled_frame_lifetime_allocator_destroy &&
        !schedule->first_scheduled_detached_image_view_destroy && !schedule->first_scheduled_image_destroy &&
        !schedule->first_scheduled_detached_image_destroy)
    {
        // No active scheduled operations, safe to reset allocator completely.
        kan_stack_group_allocator_reset (&schedule->item_allocator);
    }
}

static void render_backend_system_submit_previous_frame (struct render_backend_system_t *system)
{
    if (!system->frame_started)
    {
        return;
    }

    render_backend_system_submit_transfer (system);
    render_backend_system_submit_graphics (system);
    render_backend_system_submit_present (system);

    // TODO: Destroy unused resources (secondary command buffers, etc).
    render_backend_frame_lifetime_allocator_clean_empty_pages (system->staging_frame_lifetime_allocator);
    render_backend_system_clean_current_schedule_if_safe (system);

    system->current_frame_in_flight_index =
        (system->current_frame_in_flight_index + 1u) % KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT;
    system->frame_started = KAN_FALSE;
}

static void render_backend_surface_destroy_swap_chain_image_views (struct render_backend_surface_t *surface)
{
    for (uint32_t view_index = 0u; view_index < surface->images_count; ++view_index)
    {
        if (surface->image_views[view_index] != VK_NULL_HANDLE)
        {
            vkDestroyImageView (surface->system->device, surface->image_views[view_index],
                                VULKAN_ALLOCATION_CALLBACKS (surface->system));
        }
    }

    kan_free_general (surface->system->surface_wrapper_allocation_group, surface->images,
                      sizeof (VkImage) * surface->images_count);
    kan_free_general (surface->system->surface_wrapper_allocation_group, surface->image_views,
                      sizeof (VkImageView) * surface->images_count);

    surface->images = NULL;
    surface->image_views = NULL;
}

static kan_bool_t render_backend_surface_create_swap_chain_image_views (struct render_backend_surface_t *surface,
                                                                        VkSurfaceFormatKHR surface_format)
{
    if (vkGetSwapchainImagesKHR (surface->system->device, surface->swap_chain, &surface->images_count, NULL) !=
        VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain image views for surface \"%s\": failed to query images.",
                 surface->tracking_name)
        return KAN_FALSE;
    }

    surface->images = kan_allocate_general (surface->system->surface_wrapper_allocation_group,
                                            sizeof (VkImage) * surface->images_count, _Alignof (VkImage));

    if (vkGetSwapchainImagesKHR (surface->system->device, surface->swap_chain, &surface->images_count,
                                 surface->images) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain image views for surface \"%s\": failed to query images.",
                 surface->tracking_name)

        kan_free_general (surface->system->surface_wrapper_allocation_group, surface->images,
                          sizeof (VkImage) * surface->images_count);
        return KAN_FALSE;
    }

    surface->image_views = kan_allocate_general (surface->system->surface_wrapper_allocation_group,
                                                 sizeof (VkImageView) * surface->images_count, _Alignof (VkImageView));

    for (uint32_t view_index = 0u; view_index < surface->images_count; ++view_index)
    {
        surface->image_views[view_index] = VK_NULL_HANDLE;
    }

    kan_bool_t views_created_successfully = KAN_TRUE;
    for (uint32_t view_index = 0u; view_index < surface->images_count; ++view_index)
    {
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .image = surface->images[view_index],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surface_format.format,
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0u,
                    .levelCount = 1u,
                    .baseArrayLayer = 0u,
                    .layerCount = 1u,
                },
        };

        if (vkCreateImageView (surface->system->device, &create_info, VULKAN_ALLOCATION_CALLBACKS (surface->system),
                               &surface->image_views[view_index]) != VK_SUCCESS)
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Unable to create swap chain image views for surface \"%s\": image view creation failed.",
                     surface->tracking_name)

            surface->image_views[view_index] = VK_NULL_HANDLE;
            views_created_successfully = KAN_FALSE;
            break;
        }
    }

    if (!views_created_successfully)
    {
        render_backend_surface_destroy_swap_chain_image_views (surface);
    }

    return views_created_successfully;
}

static void render_backend_surface_destroy_semaphores (struct render_backend_surface_t *surface)
{
    for (uint64_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        if (surface->image_available_semaphores[index] != VK_NULL_HANDLE)
        {
            vkDestroySemaphore (surface->system->device, surface->image_available_semaphores[index],
                                VULKAN_ALLOCATION_CALLBACKS (surface->system));
            surface->image_available_semaphores[index] = VK_NULL_HANDLE;
        }
    }
}

static kan_bool_t render_backend_surface_create_semaphores (struct render_backend_surface_t *surface)
{
    for (uint64_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        surface->image_available_semaphores[index] = VK_NULL_HANDLE;
    }

    kan_bool_t created_successfully = KAN_TRUE;
    VkSemaphoreCreateInfo semaphore_creation_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
    };

    for (uint64_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        if (vkCreateSemaphore (surface->system->device, &semaphore_creation_info,
                               VULKAN_ALLOCATION_CALLBACKS (surface->system),
                               &surface->image_available_semaphores[index]) != VK_SUCCESS)
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Unable to create swap chain semaphores for surface \"%s\": creation failed.",
                     surface->tracking_name)

            surface->image_available_semaphores[index] = VK_NULL_HANDLE;
            created_successfully = KAN_FALSE;
            break;
        }
    }

    if (!created_successfully)
    {
        render_backend_surface_destroy_semaphores (surface);
    }

    return created_successfully;
}

static void render_backend_surface_create_swap_chain (struct render_backend_surface_t *surface,
                                                      const struct kan_application_system_window_info_t *window_info)
{
    VkBool32 present_supported;
    if (vkGetPhysicalDeviceSurfaceSupportKHR (surface->system->physical_device,
                                              surface->system->device_graphics_queue_family_index, surface->surface,
                                              &present_supported) != VK_SUCCESS)
    {
        KAN_LOG (
            render_backend_system_vulkan, KAN_LOG_ERROR,
            "Unable to create swap chain for surface \"%s\": failed to query whether present to surface is supported.",
            surface->tracking_name)
        return;
    }

    if (!present_supported)
    {
        KAN_LOG (
            render_backend_system_vulkan, KAN_LOG_ERROR,
            "Unable to create swap chain for surface \"%s\": picked device is unable to present to created surface.",
            surface->tracking_name)
        return;
    }

    uint32_t formats_count;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR (surface->system->physical_device, surface->surface, &formats_count,
                                              NULL) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": failed to query surface formats.",
                 surface->tracking_name)
        return;
    }

    if (formats_count == 0u)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": there is no supported surface formats.",
                 surface->tracking_name)
        return;
    }

    VkSurfaceFormatKHR *formats =
        kan_allocate_general (surface->system->utility_allocation_group, sizeof (VkSurfaceFormatKHR) * formats_count,
                              _Alignof (VkSurfaceFormatKHR));

    if (vkGetPhysicalDeviceSurfaceFormatsKHR (surface->system->physical_device, surface->surface, &formats_count,
                                              formats) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": failed to query surface formats.",
                 surface->tracking_name)

        kan_free_general (surface->system->utility_allocation_group, formats,
                          sizeof (VkSurfaceFormatKHR) * formats_count);
        return;
    }

    kan_bool_t format_found = KAN_FALSE;
    VkSurfaceFormatKHR surface_format;

    for (uint32_t index = 0u; index < formats_count; ++index)
    {
        if (formats[index].format == SURFACE_COLOR_FORMAT && formats[index].colorSpace == SURFACE_COLOR_SPACE)
        {
            format_found = KAN_TRUE;
            surface_format = formats[index];
            break;
        }
    }

    kan_free_general (surface->system->utility_allocation_group, formats, sizeof (VkSurfaceFormatKHR) * formats_count);
    if (!format_found)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": failed to found supported surface format.",
                 surface->tracking_name)
        return;
    }

    uint32_t present_modes_count;
    if (vkGetPhysicalDeviceSurfacePresentModesKHR (surface->system->physical_device, surface->surface,
                                                   &present_modes_count, NULL) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": failed to query surface present modes.",
                 surface->tracking_name)
        return;
    }

    if (present_modes_count == 0u)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": there is no supported surface present modes.",
                 surface->tracking_name)
        return;
    }

    VkPresentModeKHR *present_modes =
        kan_allocate_general (surface->system->utility_allocation_group,
                              sizeof (VkPresentModeKHR) * present_modes_count, _Alignof (VkPresentModeKHR));

    if (vkGetPhysicalDeviceSurfacePresentModesKHR (surface->system->physical_device, surface->surface,
                                                   &present_modes_count, present_modes) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": failed to query surface present modes.",
                 surface->tracking_name)

        kan_free_general (surface->system->utility_allocation_group, present_modes,
                          sizeof (VkPresentModeKHR) * present_modes_count);
        return;
    }

    kan_bool_t present_mode_found = KAN_FALSE;
    VkPresentModeKHR surface_present_mode;

    for (uint32_t index = 0u; index < present_modes_count; ++index)
    {
        if ((surface->system->prefer_vsync && present_modes[index] == VK_PRESENT_MODE_FIFO_KHR) ||
            present_modes[index] == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            present_mode_found = KAN_TRUE;
            surface_present_mode = present_modes[index];
            break;
        }
    }

    kan_free_general (surface->system->utility_allocation_group, present_modes,
                      sizeof (VkSurfaceFormatKHR) * present_modes_count);

    if (!present_mode_found)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": failed to found supported surface present mode.",
                 surface->tracking_name)
        return;
    }

    VkSurfaceCapabilitiesKHR surface_capabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR (surface->system->physical_device, surface->surface,
                                                   &surface_capabilities) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": failed to query surface capabilities.",
                 surface->tracking_name)
        return;
    }

    VkExtent2D surface_extent = surface_capabilities.currentExtent;
    if (surface_extent.width == UINT32_MAX)
    {
        // No current extent, need initialization.
        surface_extent.width = window_info->width_for_render;
        surface_extent.height = window_info->height_for_render;

        if (surface_extent.width < surface_capabilities.minImageExtent.width)
        {
            surface_extent.width = surface_capabilities.minImageExtent.width;
        }

        if (surface_extent.height < surface_capabilities.minImageExtent.height)
        {
            surface_extent.height = surface_capabilities.minImageExtent.height;
        }

        if (surface_extent.width > surface_capabilities.maxImageExtent.width)
        {
            surface_extent.width = surface_capabilities.maxImageExtent.width;
        }

        if (surface_extent.height > surface_capabilities.maxImageExtent.height)
        {
            surface_extent.height = surface_capabilities.maxImageExtent.height;
        }
    }

    VkSwapchainCreateInfoKHR swap_chain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0u,
        .surface = surface->surface,
        .minImageCount =
            KAN_MAX (KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT, surface_capabilities.minImageCount) + 1u,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = surface_extent,
        .imageArrayLayers = 1u,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0u,
        .pQueueFamilyIndices = NULL,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = surface_present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    if (vkCreateSwapchainKHR (surface->system->device, &swap_chain_create_info,
                              VULKAN_ALLOCATION_CALLBACKS (surface->system), &surface->swap_chain) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": construction failed.", surface->tracking_name)
        surface->swap_chain = VK_NULL_HANDLE;
        return;
    }

    surface->swap_chain_creation_window_width = (uint32_t) surface_extent.width;
    surface->swap_chain_creation_window_height = (uint32_t) surface_extent.height;

    if (!render_backend_surface_create_swap_chain_image_views (surface, surface_format))
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": failed to construct image views.",
                 surface->tracking_name)

        vkDestroySwapchainKHR (surface->system->device, surface->swap_chain,
                               VULKAN_ALLOCATION_CALLBACKS (surface->system));
        surface->swap_chain = VK_NULL_HANDLE;
        return;
    }

    if (!render_backend_surface_create_semaphores (surface))
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": failed to construct semaphores.",
                 surface->tracking_name)

        render_backend_surface_destroy_swap_chain_image_views (surface);
        vkDestroySwapchainKHR (surface->system->device, surface->swap_chain,
                               VULKAN_ALLOCATION_CALLBACKS (surface->system));
        surface->swap_chain = VK_NULL_HANDLE;
        return;
    }

    surface->acquired_image_frame = UINT32_MAX;
    surface->needs_recreation = KAN_FALSE;

    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_memory (surface->system);
    struct surface_frame_buffer_attachment_t *attachment = surface->first_frame_buffer_attachment;

    while (attachment)
    {
        // We're submitting without lock as this function can only be called from application system thread or
        // from kan_render_backend_system_next_frame, therefore locks are not necessary.

        struct scheduled_frame_buffer_create_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &schedule->item_allocator, struct scheduled_frame_buffer_create_t);

        // We do not need to preserve order as frame buffers cannot depend one on another.
        item->next = schedule->first_scheduled_frame_buffer_create;
        schedule->first_scheduled_frame_buffer_create = item;
        item->frame_buffer = attachment->frame_buffer;

        attachment = attachment->next;
    }
}

static void render_backend_surface_destroy_swap_chain (struct render_backend_surface_t *surface)
{
    if (surface->swap_chain == VK_NULL_HANDLE)
    {
        return;
    }

    // As swap chain destruction always happens when device is idle, we can destroy frame buffers right away.
    struct surface_frame_buffer_attachment_t *attachment = surface->first_frame_buffer_attachment;

    while (attachment)
    {
        render_backend_frame_buffer_destroy_resources (surface->system, attachment->frame_buffer);
        attachment = attachment->next;
    }

    render_backend_surface_destroy_semaphores (surface);
    render_backend_surface_destroy_swap_chain_image_views (surface);
    vkDestroySwapchainKHR (surface->system->device, surface->swap_chain, VULKAN_ALLOCATION_CALLBACKS (surface->system));
}

static kan_bool_t render_backend_system_acquire_images (struct render_backend_system_t *system)
{
    kan_context_system_handle_t application_system =
        kan_context_query (system->context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);

    kan_bool_t acquired_all_images = KAN_TRUE;
    kan_bool_t any_swap_chain_outdated = KAN_FALSE;
    struct render_backend_surface_t *surface = system->first_surface;

    while (surface)
    {
        const struct kan_application_system_window_info_t *window_info =
            kan_application_system_get_window_info_from_handle (application_system, surface->window_handle);

        if (surface->swap_chain == VK_NULL_HANDLE)
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Failed to acquire image for surface \"%s\" as its swap chain is not yet created.",
                     surface->tracking_name)
            acquired_all_images = KAN_FALSE;
        }
        else if (surface->swap_chain_creation_window_width != window_info->width_for_render ||
                 surface->swap_chain_creation_window_height != window_info->height_for_render)
        {
            surface->needs_recreation = KAN_TRUE;
            acquired_all_images = KAN_FALSE;
            any_swap_chain_outdated = KAN_TRUE;
        }
        else if (surface->acquired_image_frame != system->current_frame_in_flight_index)
        {
            VkResult result = vkAcquireNextImageKHR (
                system->device, surface->swap_chain, KAN_CONTEXT_RENDER_BACKEND_IMAGE_WAIT_TIMEOUT_NS,
                surface->image_available_semaphores[system->current_frame_in_flight_index], VK_NULL_HANDLE,
                &surface->acquired_image_index);

            if (result == VK_SUCCESS)
            {
                surface->acquired_image_frame = system->current_frame_in_flight_index;
            }
            else
            {
                surface->acquired_image_frame = UINT32_MAX;
                acquired_all_images = KAN_FALSE;

                if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
                {
                    surface->needs_recreation = KAN_TRUE;
                    any_swap_chain_outdated = KAN_TRUE;
                }
            }
        }

        surface = surface->next;
    }

    if (any_swap_chain_outdated)
    {
        vkDeviceWaitIdle (system->device);
        surface = system->first_surface;

        while (surface)
        {
            if (surface->needs_recreation)
            {
                render_backend_surface_destroy_swap_chain (surface);
                render_backend_surface_create_swap_chain (surface, kan_application_system_get_window_info_from_handle (
                                                                       application_system, surface->window_handle));
            }

            surface = surface->next;
        }
    }

    return acquired_all_images && !any_swap_chain_outdated;
}

kan_bool_t kan_render_backend_system_next_frame (kan_context_system_handle_t render_backend_system)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) render_backend_system;
    render_backend_system_submit_previous_frame (system);

    VkResult fence_wait_result =
        vkWaitForFences (system->device, 1u, &system->in_flight_fences[system->current_frame_in_flight_index], VK_TRUE,
                         KAN_CONTEXT_RENDER_BACKEND_FENCE_WAIT_TIMEOUT_NS);

    if (fence_wait_result == VK_TIMEOUT)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_INFO, "Skipping frame due to in flight fence wait timeout.")
        return KAN_FALSE;
    }
    else if (fence_wait_result != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed waiting for in flight fence.")
        return KAN_FALSE;
    }

    if (!render_backend_system_acquire_images (system))
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_INFO, "Skipping frame as swap chain images are not ready.")
        return KAN_FALSE;
    }

    vkResetFences (system->device, 1u, &system->in_flight_fences[system->current_frame_in_flight_index]);
    system->frame_started = KAN_TRUE;

    if (vkResetCommandPool (system->device,
                            system->command_states[system->current_frame_in_flight_index].graphics_command_pool,
                            0u) != VK_SUCCESS)
    {
        kan_critical_error ("Unexpected failure when resetting graphics command pool.", __FILE__, __LINE__);
    }

    if (vkResetCommandPool (system->device,
                            system->command_states[system->current_frame_in_flight_index].transfer_command_pool,
                            0u) != VK_SUCCESS)
    {
        kan_critical_error ("Unexpected failure when resetting transfer command pool.", __FILE__, __LINE__);
    }

    struct render_backend_schedule_state_t *schedule = &system->schedule_states[system->current_frame_in_flight_index];
    struct scheduled_frame_buffer_destroy_t *frame_buffer_destroy = schedule->first_scheduled_frame_buffer_destroy;

    while (frame_buffer_destroy)
    {
        render_backend_system_destroy_frame_buffer (system, frame_buffer_destroy->frame_buffer, KAN_TRUE);
        frame_buffer_destroy = frame_buffer_destroy->next;
    }

    schedule->first_scheduled_frame_buffer_destroy = NULL;
    struct scheduled_detached_frame_buffer_destroy_t *detached_frame_buffer_destroy =
        schedule->first_scheduled_detached_frame_buffer_destroy;

    while (detached_frame_buffer_destroy)
    {
        vkDestroyFramebuffer (system->device, detached_frame_buffer_destroy->detached_frame_buffer,
                              VULKAN_ALLOCATION_CALLBACKS (system));
        detached_frame_buffer_destroy = detached_frame_buffer_destroy->next;
    }

    schedule->first_scheduled_detached_frame_buffer_destroy = NULL;
    struct scheduled_pass_destroy_t *pass_destroy = schedule->first_scheduled_pass_destroy;

    while (pass_destroy)
    {
        render_backend_system_destroy_pass (system, pass_destroy->pass, KAN_TRUE);
        pass_destroy = pass_destroy->next;
    }

    schedule->first_scheduled_pass_destroy = NULL;
    struct scheduled_buffer_destroy_t *buffer_destroy = schedule->first_scheduled_buffer_destroy;

    while (buffer_destroy)
    {
        render_backend_system_destroy_buffer (system, buffer_destroy->buffer, KAN_TRUE);
        buffer_destroy = buffer_destroy->next;
    }

    schedule->first_scheduled_buffer_destroy = NULL;
    struct scheduled_frame_lifetime_allocator_destroy_t *frame_lifetime_allocator_destroy =
        schedule->first_scheduled_frame_lifetime_allocator_destroy;

    while (frame_lifetime_allocator_destroy)
    {
        render_backend_system_destroy_frame_lifetime_allocator (
            system, frame_lifetime_allocator_destroy->frame_lifetime_allocator, KAN_TRUE, KAN_TRUE);
        frame_lifetime_allocator_destroy = frame_lifetime_allocator_destroy->next;
    }

    schedule->first_scheduled_frame_lifetime_allocator_destroy = NULL;
    struct scheduled_detached_image_view_destroy_t *detached_image_view_destroy =
        schedule->first_scheduled_detached_image_view_destroy;

    while (detached_image_view_destroy)
    {
        vkDestroyImageView (system->device, detached_image_view_destroy->detached_image_view,
                            VULKAN_ALLOCATION_CALLBACKS (system));
        detached_image_view_destroy = detached_image_view_destroy->next;
    }

    schedule->first_scheduled_detached_image_view_destroy = NULL;
    struct scheduled_image_destroy_t *image_destroy = schedule->first_scheduled_image_destroy;

    while (image_destroy)
    {
        render_backend_system_destroy_image (system, image_destroy->image, KAN_TRUE);
        image_destroy = image_destroy->next;
    }

    schedule->first_scheduled_image_destroy = NULL;
    struct scheduled_detached_image_destroy_t *detached_image_destroy =
        schedule->first_scheduled_detached_image_destroy;

    while (detached_image_destroy)
    {
#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
        transfer_memory_between_groups (detached_image_destroy->gpu_size, detached_image_destroy->gpu_allocation_group,
                                        system->memory_profiling.gpu_unmarked_group);
#endif

        vmaDestroyImage (system->gpu_memory_allocator, detached_image_destroy->detached_image,
                         detached_image_destroy->detached_allocation);
        detached_image_destroy = detached_image_destroy->next;
    }

    schedule->first_scheduled_detached_image_destroy = NULL;

    // TODO: Execute destroy schedule here.

    // TODO: Clear leftovers from previous frame that executed on this index.

    render_backend_system_clean_current_schedule_if_safe (system);
    render_backend_frame_lifetime_allocator_retire_old_allocations (system->staging_frame_lifetime_allocator);
    return KAN_TRUE;
}

static void render_backend_surface_init_with_window (void *user_data,
                                                     const struct kan_application_system_window_info_t *window_info)
{
    struct render_backend_surface_t *surface = user_data;
    KAN_ASSERT (surface->surface == VK_NULL_HANDLE)

    surface->surface = (VkSurfaceKHR) kan_platform_application_window_create_vulkan_surface (
        window_info->id, (uint64_t) surface->system->instance, VULKAN_ALLOCATION_CALLBACKS (surface->system));
    render_backend_surface_create_swap_chain (surface, window_info);
}

static void render_backend_surface_shutdown_with_window (void *user_data,
                                                         const struct kan_application_system_window_info_t *window_info)
{
    struct render_backend_surface_t *surface = user_data;
    vkDeviceWaitIdle (surface->system->device);

    if (surface->surface != VK_NULL_HANDLE)
    {
        render_backend_surface_destroy_swap_chain (surface);
        kan_platform_application_window_destroy_vulkan_surface (window_info->id, (uint64_t) surface->system->instance,
                                                                (uint64_t) surface->surface,
                                                                VULKAN_ALLOCATION_CALLBACKS (surface->system));
        surface->surface = VK_NULL_HANDLE;
    }

    struct surface_blit_request_t *request = surface->first_blit_request;
    while (request)
    {
        struct surface_blit_request_t *next = request->next;
        kan_free_batched (surface->system->surface_wrapper_allocation_group, request);
        request = next;
    }

    struct surface_frame_buffer_attachment_t *attachment = surface->first_frame_buffer_attachment;
    while (attachment)
    {
        struct surface_frame_buffer_attachment_t *next = attachment->next;
        kan_free_batched (surface->system->surface_wrapper_allocation_group, attachment);
        attachment = next;
    }

    if (surface->next)
    {
        surface->next->previous = surface->previous;
    }

    if (surface->previous)
    {
        surface->previous->next = surface->next;
    }
    else
    {
        KAN_ASSERT (surface->system->first_surface == surface)
        surface->system->first_surface = surface->next;
    }

    kan_free_batched (surface->system->surface_wrapper_allocation_group, surface);
}

kan_render_surface_t kan_render_backend_system_create_surface (kan_context_system_handle_t render_backend_system,
                                                               kan_application_system_window_handle_t window,
                                                               kan_interned_string_t tracking_name)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) render_backend_system;
    kan_context_system_handle_t application_system =
        kan_context_query (system->context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);

    if (application_system == KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create surfaces due to absence of application system in context.")
        return KAN_INVALID_RENDER_SURFACE;
    }

    kan_atomic_int_lock (&system->resource_management_lock);
    struct render_backend_surface_t *new_surface =
        kan_allocate_batched (system->surface_wrapper_allocation_group, sizeof (struct render_backend_surface_t));

    new_surface->system = system;
    new_surface->tracking_name = tracking_name;
    new_surface->surface = VK_NULL_HANDLE;
    new_surface->swap_chain = VK_NULL_HANDLE;
    new_surface->window_handle = window;

    new_surface->render_state = SURFACE_RENDER_STATE_RECEIVED_NO_OUTPUT;
    new_surface->blit_request_lock = kan_atomic_int_init (0);
    new_surface->first_blit_request = NULL;
    new_surface->first_frame_buffer_attachment = NULL;

    new_surface->previous = NULL;
    new_surface->next = system->first_surface;

    if (system->first_surface)
    {
        system->first_surface->previous = new_surface;
    }

    system->first_surface = new_surface;
    kan_atomic_int_unlock (&system->resource_management_lock);

    new_surface->resource_id =
        kan_application_system_window_add_resource (application_system, window,
                                                    (struct kan_application_system_window_resource_binding_t) {
                                                        .user_data = new_surface,
                                                        .init = render_backend_surface_init_with_window,
                                                        .shutdown = render_backend_surface_shutdown_with_window,
                                                    });

    return (kan_render_surface_t) new_surface;
}

void kan_render_backend_system_present_image_on_surface (kan_render_surface_t surface,
                                                         kan_render_image_t image,
                                                         struct kan_render_integer_region_t image_region,
                                                         struct kan_render_integer_region_t surface_region)
{
    struct render_backend_surface_t *data = (struct render_backend_surface_t *) surface;
    struct surface_blit_request_t *request =
        kan_allocate_batched (data->system->surface_wrapper_allocation_group, sizeof (struct surface_blit_request_t));

    request->next = NULL;
    request->image = (struct render_backend_image_t *) image;
    request->image_region = image_region;
    request->surface_region = surface_region;

    kan_atomic_int_lock (&data->blit_request_lock);
    if (data->first_blit_request)
    {
        data->first_blit_request = request;
    }
    else
    {
        // Search for the last request. Count of requests should be very small, so it is okay thing to do.
        struct surface_blit_request_t *last = data->first_blit_request;
        while (last->next)
        {
            last = last->next;
        }

        last->next = request;
    }

    kan_atomic_int_unlock (&data->blit_request_lock);
}

void kan_render_backend_system_destroy_surface (kan_context_system_handle_t render_backend_system,
                                                kan_render_surface_t surface)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) render_backend_system;
    struct render_backend_surface_t *surface_data = (struct render_backend_surface_t *) surface;

    kan_context_system_handle_t application_system =
        kan_context_query (system->context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);

    // Valid surface couldn't have been created without application system.
    KAN_ASSERT (application_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    kan_application_system_window_remove_resource (application_system, surface_data->window_handle,
                                                   surface_data->resource_id);
}

enum kan_platform_window_flag_t kan_render_get_required_window_flags (void)
{
    return KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_VULKAN;
}

kan_render_frame_buffer_t kan_render_frame_buffer_create (kan_render_context_t context,
                                                          struct kan_render_frame_buffer_description_t *description)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) context;
    kan_atomic_int_lock (&system->resource_management_lock);
    struct render_backend_frame_buffer_t *frame_buffer =
        render_backend_system_create_frame_buffer (system, description);
    kan_atomic_int_unlock (&system->resource_management_lock);
    return (kan_render_frame_buffer_t) frame_buffer;
}

void kan_render_frame_buffer_destroy (kan_render_frame_buffer_t buffer)
{
    struct render_backend_frame_buffer_t *data = (struct render_backend_frame_buffer_t *) buffer;
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_frame_buffer_destroy_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_frame_buffer_destroy_t);

    // We do not need to preserve order as frame buffers cannot depend one on another.
    item->next = schedule->first_scheduled_frame_buffer_destroy;
    schedule->first_scheduled_frame_buffer_destroy = item;
    item->frame_buffer = data;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}

kan_render_pass_t kan_render_pass_create (kan_render_context_t context,
                                          struct kan_render_pass_description_t *description)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) context;
    kan_atomic_int_lock (&system->resource_management_lock);
    struct render_backend_pass_t *pass = render_backend_system_create_pass (system, description);
    kan_atomic_int_unlock (&system->resource_management_lock);
    return pass ? (kan_render_pass_t) pass : KAN_INVALID_RENDER_PASS;
}

kan_bool_t kan_render_pass_add_static_dependency (kan_render_pass_t pass, kan_render_pass_t dependency)
{
    // TODO: Implement.
    return KAN_FALSE;
}

kan_render_pass_instance_t kan_render_pass_instantiate (kan_render_pass_t pass,
                                                        kan_render_frame_buffer_t frame_buffer,
                                                        struct kan_render_viewport_bounds_t *viewport_bounds,
                                                        struct kan_render_integer_region_t *scissor)
{
    // TODO: Implement.
    return KAN_INVALID_RENDER_PASS_INSTANCE;
}

void kan_render_pass_instance_add_dynamic_dependency (kan_render_pass_instance_t pass_instance,
                                                      kan_render_pass_instance_t dependency)
{
    // TODO: Implement.
}

void kan_render_pass_instance_classic_graphics_pipeline (
    kan_render_pass_instance_t pass_instance, kan_render_classic_graphics_pipeline_instance_t pipeline_instance)
{
    // TODO: Implement.
}

void kan_render_pass_instance_attributes (kan_render_pass_instance_t pass_instance,
                                          uint64_t start_at_binding,
                                          uint64_t buffers_count,
                                          kan_render_buffer_t *buffers)
{
    // TODO: Implement.
}

void kan_render_pass_instance_indices (kan_render_pass_instance_t pass_instance, kan_render_buffer_t buffer)
{
    // TODO: Implement.
}

void kan_render_pass_instance_draw (kan_render_pass_instance_t pass_instance,
                                    uint64_t vertex_offset,
                                    uint64_t vertex_count)
{
    // TODO: Implement.
}

void kan_render_pass_instance_instanced_draw (kan_render_pass_instance_t pass_instance,
                                              uint64_t vertex_offset,
                                              uint64_t vertex_count,
                                              uint64_t instance_offset,
                                              uint64_t instance_count)
{
    // TODO: Implement.
}

void kan_render_pass_destroy (kan_render_pass_t pass)
{
    struct render_backend_pass_t *data = (struct render_backend_pass_t *) pass;
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_pass_destroy_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_pass_destroy_t);

    // We do not need to preserve order as passes cannot depend one on another.
    item->next = schedule->first_scheduled_pass_destroy;
    schedule->first_scheduled_pass_destroy = item;
    item->pass = data;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}

kan_render_classic_graphics_pipeline_family_t kan_render_classic_graphics_pipeline_family_create (
    kan_render_context_t context, struct kan_render_classic_graphics_pipeline_family_definition_t *definition)
{
    // TODO: Implement.
    return KAN_INVALID_RENDER_CLASSIC_GRAPHICS_PIPELINE_FAMILY;
}

void kan_render_classic_graphics_pipeline_family_destroy (kan_render_classic_graphics_pipeline_family_t family)
{
    // TODO: Implement.
}

kan_render_classic_graphics_pipeline_t kan_render_classic_graphics_pipeline_create (
    kan_render_context_t context,
    struct kan_render_classic_graphics_pipeline_definition_t *definition,
    enum kan_render_pipeline_compilation_priority_t compilation_priority)
{
    // TODO: Implement.
    return KAN_INVALID_RENDER_CLASSIC_GRAPHICS_PIPELINE;
}

void kan_render_classic_graphics_pipeline_change_compilation_priority (
    kan_render_classic_graphics_pipeline_t pipeline,
    enum kan_render_pipeline_compilation_priority_t compilation_priority)
{
    // TODO: Implement.
}

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_classic_graphics_pipeline_destroy (
    kan_render_classic_graphics_pipeline_t pipeline)
{
    // TODO: Implement.
}

kan_render_classic_graphics_pipeline_instance_t kan_render_classic_graphics_pipeline_instance_create (
    kan_render_context_t context,
    kan_render_classic_graphics_pipeline_t pipeline,
    uint64_t initial_bindings_count,
    struct kan_render_layout_update_description_t *initial_bindings,
    kan_interned_string_t tracking_name)
{
    // TODO: Implement.
    return KAN_INVALID_RENDER_CLASSIC_GRAPHICS_PIPELINE_INSTANCE;
}

void kan_render_classic_graphics_pipeline_instance_update_layout (
    kan_render_classic_graphics_pipeline_instance_t instance,
    uint64_t bindings_count,
    struct kan_render_layout_update_description_t *bindings)
{
    // TODO: Implement.
}

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_classic_graphics_pipeline_instance_destroy (
    kan_render_classic_graphics_pipeline_instance_t instance)
{
    // TODO: Implement.
}

kan_render_buffer_t kan_render_buffer_create (kan_render_context_t context,
                                              enum kan_render_buffer_type_t type,
                                              uint64_t full_size,
                                              void *optional_initial_data,
                                              kan_interned_string_t tracking_name)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) context;
    kan_atomic_int_lock (&system->resource_management_lock);
    struct render_backend_buffer_t *buffer = render_backend_system_create_buffer (
        system, RENDER_BACKEND_BUFFER_FAMILY_RESOURCE, type, full_size, tracking_name);
    kan_atomic_int_unlock (&system->resource_management_lock);

    if (!buffer)
    {
        return KAN_INVALID_RENDER_BUFFER;
    }

    if (optional_initial_data)
    {
        if (system->device_memory_type == KAN_RENDER_DEVICE_MEMORY_TYPE_UNIFIED ||
            system->device_memory_type == KAN_RENDER_DEVICE_MEMORY_TYPE_UNIFIED_COHERENT)
        {
            // Due to unified memory, we should be able to directly initialize buffer.
            void *memory;
            kan_atomic_int_lock (&system->resource_management_lock);

            if (vmaMapMemory (system->gpu_memory_allocator, buffer->allocation, &memory) != VK_SUCCESS)
            {
                kan_critical_error ("Unexpected failure while mapping buffer memory, unable to continue properly.",
                                    __FILE__, __LINE__);
            }

            kan_atomic_int_unlock (&system->resource_management_lock);
            memcpy (memory, optional_initial_data, full_size);
            kan_atomic_int_lock (&system->resource_management_lock);
            vmaUnmapMemory (system->gpu_memory_allocator, buffer->allocation);

            if (vmaFlushAllocation (system->gpu_memory_allocator, buffer->allocation, 0u, full_size) != VK_SUCCESS)
            {
                kan_critical_error (
                    "Unexpected failure while flushing buffer initial data, unable to continue properly.", __FILE__,
                    __LINE__);
            }

            kan_atomic_int_unlock (&system->resource_management_lock);
        }
        else
        {
            void *data = kan_render_buffer_patch ((kan_render_buffer_t) buffer, 0u, full_size);
            // Patch can fail if there is no available staging memory,
            // but we have no special handling for that right now.
            KAN_ASSERT (data)
            memcpy (data, optional_initial_data, full_size);
        }
    }

    return (kan_render_buffer_t) buffer;
}

void *kan_render_buffer_patch (kan_render_buffer_t buffer, uint64_t slice_offset, uint64_t slice_size)
{
    struct render_backend_buffer_t *data = (struct render_backend_buffer_t *) buffer;
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_memory (data->system);

    switch (data->family)
    {
    case RENDER_BACKEND_BUFFER_FAMILY_RESOURCE:
    {
        struct render_backend_frame_lifetime_allocator_allocation_t staging_allocation =
            render_backend_system_allocate_for_staging (data->system, slice_size);

        if (!staging_allocation.buffer)
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Failed to patch buffer \"%s\": out of staging memory.", data->tracking_name)
            return NULL;
        }

        kan_atomic_int_lock (&data->system->resource_management_lock);
        void *memory;

        if (vmaMapMemory (data->system->gpu_memory_allocator, staging_allocation.buffer->allocation, &memory) !=
            VK_SUCCESS)
        {
            kan_critical_error ("Unexpected failure while mapping staging buffer memory, unable to continue properly.",
                                __FILE__, __LINE__);
        }

        kan_atomic_int_unlock (&data->system->resource_management_lock);
        kan_atomic_int_lock (&schedule->schedule_lock);

        struct scheduled_buffer_unmap_flush_transfer_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &schedule->item_allocator, struct scheduled_buffer_unmap_flush_transfer_t);

        // We do not need to preserve order as buffers cannot depend one on another.
        item->next = schedule->first_scheduled_buffer_unmap_flush_transfer;
        schedule->first_scheduled_buffer_unmap_flush_transfer = item;
        item->source_buffer = staging_allocation.buffer;
        item->target_buffer = data;
        item->source_offset = staging_allocation.offset;
        item->target_offset = slice_offset;
        item->size = slice_size;
        kan_atomic_int_unlock (&schedule->schedule_lock);

        return ((uint8_t *) memory) + staging_allocation.offset;
    }

    case RENDER_BACKEND_BUFFER_FAMILY_STAGING:
        // Staging buffers are not exposed to user, we shouldn't be able to get here normally.
        KAN_ASSERT (KAN_FALSE)
        break;

    case RENDER_BACKEND_BUFFER_FAMILY_FRAME_LIFETIME_ALLOCATOR:
    {
        // Frame lifetime allocations are always host visible, there is no need to stage them due to their lifetime.
        void *memory;
        kan_atomic_int_lock (&data->system->resource_management_lock);

        if (vmaMapMemory (data->system->gpu_memory_allocator, data->allocation, &memory) != VK_SUCCESS)
        {
            kan_critical_error ("Unexpected failure while mapping buffer memory, unable to continue properly.",
                                __FILE__, __LINE__);
        }

        kan_atomic_int_unlock (&data->system->resource_management_lock);
        kan_atomic_int_lock (&schedule->schedule_lock);

        struct scheduled_buffer_unmap_flush_t *item =
            KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_buffer_unmap_flush_t);

        // We do not need to preserve order as buffers cannot depend one on another.
        item->next = schedule->first_scheduled_buffer_unmap_flush;
        schedule->first_scheduled_buffer_unmap_flush = item;
        item->buffer = data;
        item->offset = slice_offset;
        item->size = slice_size;
        kan_atomic_int_unlock (&schedule->schedule_lock);
        return ((uint8_t *) memory) + slice_offset;
    }
    }

    KAN_ASSERT (KAN_FALSE)
    return NULL;
}

void kan_render_buffer_destroy (kan_render_buffer_t buffer)
{
    struct render_backend_buffer_t *data = (struct render_backend_buffer_t *) buffer;
    // Only resource family buffers can be destroyed externally through scheduling.
    KAN_ASSERT (data->family == RENDER_BACKEND_BUFFER_FAMILY_RESOURCE)

    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_buffer_destroy_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_buffer_destroy_t);

    // We do not need to preserve order as buffers cannot depend one on another.
    item->next = schedule->first_scheduled_buffer_destroy;
    schedule->first_scheduled_buffer_destroy = item;
    item->buffer = data;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}

kan_render_frame_lifetime_buffer_allocator_t kan_render_frame_lifetime_buffer_allocator_create (
    kan_render_context_t context,
    enum kan_render_buffer_type_t buffer_type,
    uint64_t page_size,
    kan_interned_string_t tracking_name)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) context;
    kan_atomic_int_lock (&system->resource_management_lock);
    struct render_backend_frame_lifetime_allocator_t *allocator =
        render_backend_system_create_frame_lifetime_allocator (
            system, RENDER_BACKEND_BUFFER_FAMILY_FRAME_LIFETIME_ALLOCATOR, buffer_type, page_size, tracking_name);
    kan_atomic_int_unlock (&system->resource_management_lock);
    return (kan_render_frame_lifetime_buffer_allocator_t) allocator;
}

struct kan_render_allocated_slice_t kan_render_frame_lifetime_buffer_allocator_allocate (
    kan_render_frame_lifetime_buffer_allocator_t allocator, uint64_t size, uint64_t alignment)
{
    struct render_backend_frame_lifetime_allocator_t *data =
        (struct render_backend_frame_lifetime_allocator_t *) allocator;

    struct render_backend_frame_lifetime_allocator_allocation_t allocation =
        render_backend_frame_lifetime_allocator_allocate (data, size, alignment);

    return (struct kan_render_allocated_slice_t) {
        .buffer = (kan_render_buffer_t) allocation.buffer,
        .slice_offset = allocation.offset,
    };
}

void kan_render_frame_lifetime_buffer_allocator_destroy (kan_render_frame_lifetime_buffer_allocator_t allocator)
{
    struct render_backend_frame_lifetime_allocator_t *data =
        (struct render_backend_frame_lifetime_allocator_t *) allocator;
    // Only resource family buffers can be destroyed externally through scheduling.
    KAN_ASSERT (data->buffer_family == RENDER_BACKEND_BUFFER_FAMILY_FRAME_LIFETIME_ALLOCATOR)

    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_frame_lifetime_allocator_destroy_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &schedule->item_allocator, struct scheduled_frame_lifetime_allocator_destroy_t);

    // We do not need to preserve order as frame_lifetime_allocators cannot depend one on another.
    item->next = schedule->first_scheduled_frame_lifetime_allocator_destroy;
    schedule->first_scheduled_frame_lifetime_allocator_destroy = item;
    item->frame_lifetime_allocator = data;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}

kan_render_image_t kan_render_image_create (kan_render_context_t context,
                                            struct kan_render_image_description_t *description)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) context;
    kan_atomic_int_lock (&system->resource_management_lock);
    struct render_backend_image_t *image = render_backend_system_create_image (system, description);
    kan_atomic_int_unlock (&system->resource_management_lock);
    return image ? (kan_render_image_t) image : KAN_INVALID_RENDER_IMAGE;
}

void kan_render_image_upload_data (kan_render_image_t image, uint64_t mip, void *data)
{
    struct render_backend_image_t *image_data = (struct render_backend_image_t *) image;
    KAN_ASSERT (!image_data->description.render_target)
    KAN_ASSERT (mip < image_data->description.mips)

    uint64_t texel_size =
        kan_render_image_description_calculate_texel_size (image_data->system, &image_data->description);

    uint64_t width;
    uint64_t height;
    uint64_t depth;
    kan_render_image_description_calculate_size_at_mip (&image_data->description, mip, &width, &height, &depth);

    const uint64_t allocation_size = texel_size * width * height * depth;
    struct render_backend_frame_lifetime_allocator_allocation_t staging_allocation =
        render_backend_system_allocate_for_staging (image_data->system, allocation_size);

    if (!staging_allocation.buffer)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Failed to upload image \"%s\" mip %lu: out of staging memory.", image_data->description.tracking_name,
                 (unsigned long) mip)
        return;
    }

    struct render_backend_schedule_state_t *schedule =
        render_backend_system_get_schedule_for_memory (image_data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_image_upload_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_image_upload_t);

    // We do not need to preserve order as images cannot depend one on another.
    item->next = schedule->first_scheduled_image_upload;
    schedule->first_scheduled_image_upload = item;
    item->image = image_data;
    item->mip = mip;
    item->staging_buffer = staging_allocation.buffer;
    item->staging_buffer_offset = staging_allocation.offset;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}

void kan_render_image_request_mip_generation (kan_render_image_t image, uint64_t first, uint64_t last)
{
    struct render_backend_image_t *data = (struct render_backend_image_t *) image;
    KAN_ASSERT (!data->description.render_target)

    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_memory (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_image_mip_generation_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_image_mip_generation_t);

    // We do not need to preserve order as images cannot depend one on another.
    item->next = schedule->first_scheduled_image_mip_generation;
    schedule->first_scheduled_image_mip_generation = item;
    item->image = data;
    item->first = first;
    item->last = last;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}

void kan_render_image_resize_render_target (kan_render_image_t image,
                                            uint64_t new_width,
                                            uint64_t new_height,
                                            uint64_t new_depth)
{
    struct render_backend_image_t *data = (struct render_backend_image_t *) image;
    KAN_ASSERT (data->description.render_target)

    // If it is null handle, then we've tried to resize earlier and failed. No need for additional cleanup.
    if (data->image != VK_NULL_HANDLE)
    {
        struct render_backend_schedule_state_t *schedule =
            render_backend_system_get_schedule_for_destroy (data->system);
        kan_atomic_int_lock (&schedule->schedule_lock);

        struct image_frame_buffer_attachment_t *attachment = data->first_frame_buffer_attachment;
        while (attachment)
        {
            render_backend_frame_buffer_schedule_resource_destroy (data->system, attachment->frame_buffer, schedule);
            attachment = attachment->next;
        }

        struct scheduled_detached_image_destroy_t *image_destroy = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &schedule->item_allocator, struct scheduled_detached_image_destroy_t);

        image_destroy->next = schedule->first_scheduled_detached_image_destroy;
        schedule->first_scheduled_detached_image_destroy = image_destroy;
        image_destroy->detached_image = data->image;
        image_destroy->detached_allocation = data->allocation;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
        image_destroy->gpu_size = render_backend_image_calculate_gpu_size (data->system, data);
        image_destroy->gpu_allocation_group = data->device_allocation_group;
#endif

        kan_atomic_int_unlock (&schedule->schedule_lock);
    }

    data->description.width = new_width;
    data->description.height = new_height;
    data->description.depth = new_depth;

    if (!create_vulkan_image (data->system, &data->description, &data->image, &data->allocation))
    {
        data->image = VK_NULL_HANDLE;
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Failed to resize image \"%s\": new image creation failed.", data->description.tracking_name)
        return;
    }

    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_memory (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);
    struct image_frame_buffer_attachment_t *attachment = data->first_frame_buffer_attachment;

    while (attachment)
    {
        struct scheduled_frame_buffer_create_t *item = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
            &schedule->item_allocator, struct scheduled_frame_buffer_create_t);

        // We do not need to preserve order as frame buffers cannot depend one on another.
        item->next = schedule->first_scheduled_frame_buffer_create;
        schedule->first_scheduled_frame_buffer_create = item;
        item->frame_buffer = attachment->frame_buffer;
        attachment = attachment->next;
    }

    kan_atomic_int_unlock (&schedule->schedule_lock);
}

void kan_render_image_destroy (kan_render_image_t image)
{
    struct render_backend_image_t *data = (struct render_backend_image_t *) image;
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_image_destroy_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_image_destroy_t);

    // We do not need to preserve order as images cannot depend one on another.
    item->next = schedule->first_scheduled_image_destroy;
    schedule->first_scheduled_image_destroy = item;
    item->image = data;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}
