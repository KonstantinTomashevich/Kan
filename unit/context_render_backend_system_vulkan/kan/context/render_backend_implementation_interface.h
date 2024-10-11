#include <string.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/container/list.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/context/render_backend_system.h>
#include <kan/context/vulkan_memory_allocator.h>
#include <kan/memory/allocation.h>
#include <kan/platform/application.h>
#include <kan/platform/precise_time.h>
#include <kan/threading/atomic.h>
#include <kan/threading/conditional_variable.h>
#include <kan/threading/mutex.h>
#include <kan/threading/thread.h>

KAN_C_HEADER_BEGIN

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
    struct kan_bd_list_node_t list_node;
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

struct scheduled_classic_graphics_pipeline_destroy_t
{
    struct scheduled_classic_graphics_pipeline_destroy_t *next;
    struct render_backend_classic_graphics_pipeline_t *pipeline;
};

struct scheduled_classic_graphics_pipeline_family_destroy_t
{
    struct scheduled_classic_graphics_pipeline_family_destroy_t *next;
    struct render_backend_classic_graphics_pipeline_family_t *family;
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
    struct scheduled_classic_graphics_pipeline_destroy_t *first_scheduled_classic_graphics_pipeline_destroy;
    struct scheduled_classic_graphics_pipeline_family_destroy_t
        *first_scheduled_classic_graphics_pipeline_family_destroy;
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
    struct kan_bd_list_node_t list_node;
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

struct render_backend_frame_buffer_t *render_backend_system_create_frame_buffer (
    struct render_backend_system_t *system, struct kan_render_frame_buffer_description_t *description);

void render_backend_frame_buffer_destroy_resources (struct render_backend_system_t *system,
                                                    struct render_backend_frame_buffer_t *frame_buffer);

void render_backend_frame_buffer_schedule_resource_destroy (struct render_backend_system_t *system,
                                                            struct render_backend_frame_buffer_t *frame_buffer,
                                                            struct render_backend_schedule_state_t *schedule);

void render_backend_system_destroy_frame_buffer (struct render_backend_system_t *system,
                                                 struct render_backend_frame_buffer_t *frame_buffer);

struct render_backend_pass_t
{
    struct kan_bd_list_node_t list_node;
    struct render_backend_system_t *system;
    VkRenderPass pass;
};

struct render_backend_pass_t *render_backend_system_create_pass (struct render_backend_system_t *system,
                                                                 struct kan_render_pass_description_t *description);

void render_backend_system_destroy_pass (struct render_backend_system_t *system, struct render_backend_pass_t *pass);

struct render_backend_layout_binding_t
{
    enum kan_render_layout_binding_type_t type;
    uint64_t used_stage_mask;
};

struct render_backend_descriptor_set_layout_t
{
    VkDescriptorSetLayout layout;
    kan_bool_t stable_binding;
    uint64_t bindings_count;
    struct render_backend_layout_binding_t bindings[];
};

struct render_backend_classic_graphics_pipeline_family_t
{
    struct kan_bd_list_node_t list_node;
    struct render_backend_system_t *system;

    VkPipelineLayout layout;
    uint64_t descriptor_set_layouts_count;
    struct render_backend_descriptor_set_layout_t **descriptor_set_layouts;
    kan_bool_t descriptor_sets_are_zero_sequential;

    enum kan_render_classic_graphics_topology_t topology;
    kan_interned_string_t tracking_name;

    uint64_t input_bindings_count;
    VkVertexInputBindingDescription *input_bindings;

    uint64_t attributes_count;
    VkVertexInputAttributeDescription *attributes;
};

struct render_backend_classic_graphics_pipeline_family_t *
render_backend_system_create_classic_graphics_pipeline_family (
    struct render_backend_system_t *system,
    struct kan_render_classic_graphics_pipeline_family_description_t *description);

void render_backend_system_destroy_classic_graphics_pipeline_family (
    struct render_backend_system_t *system, struct render_backend_classic_graphics_pipeline_family_t *family);

enum pipeline_compilation_state_t
{
    PIPELINE_COMPILATION_STATE_PENDING = 0u,
    PIPELINE_COMPILATION_STATE_EXECUTION,
    PIPELINE_COMPILATION_STATE_SUCCESS,
    PIPELINE_COMPILATION_STATE_FAILURE,
};

struct render_backend_classic_graphics_pipeline_t
{
    struct kan_bd_list_node_t list_node;
    struct render_backend_system_t *system;

    VkPipeline pipeline;
    struct render_backend_pass_t *pass;
    struct render_backend_classic_graphics_pipeline_family_t *family;

    float min_depth;
    float max_depth;

    uint64_t shader_modules_count;
    VkShaderModule *shader_modules;

    enum kan_render_pipeline_compilation_priority_t compilation_priority;
    enum pipeline_compilation_state_t compilation_state;
    struct classic_graphics_pipeline_compilation_request_t *compilation_request;

    kan_interned_string_t tracking_name;
};

struct render_backend_classic_graphics_pipeline_t *render_backend_system_create_classic_graphics_pipeline (
    struct render_backend_system_t *system,
    struct kan_render_classic_graphics_pipeline_description_t *description,
    enum kan_render_pipeline_compilation_priority_t compilation_priority);

void render_backend_system_destroy_classic_graphics_pipeline (
    struct render_backend_system_t *system, struct render_backend_classic_graphics_pipeline_t *family);

enum render_backend_buffer_family_t
{
    RENDER_BACKEND_BUFFER_FAMILY_RESOURCE = 0u,
    RENDER_BACKEND_BUFFER_FAMILY_STAGING,
    RENDER_BACKEND_BUFFER_FAMILY_FRAME_LIFETIME_ALLOCATOR,
};

struct render_backend_buffer_t
{
    struct kan_bd_list_node_t list_node;
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

struct render_backend_buffer_t *render_backend_system_create_buffer (struct render_backend_system_t *system,
                                                                     enum render_backend_buffer_family_t family,
                                                                     enum kan_render_buffer_type_t buffer_type,
                                                                     uint64_t full_size,
                                                                     kan_interned_string_t tracking_name);

void render_backend_system_destroy_buffer (struct render_backend_system_t *system,
                                           struct render_backend_buffer_t *buffer);

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
    struct kan_bd_list_node_t list_node;
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

struct render_backend_frame_lifetime_allocator_t *render_backend_system_create_frame_lifetime_allocator (
    struct render_backend_system_t *system,
    enum render_backend_buffer_family_t buffer_family,
    enum kan_render_buffer_type_t buffer_type,
    uint64_t page_size,
    kan_interned_string_t tracking_name);

struct render_backend_frame_lifetime_allocator_allocation_t render_backend_frame_lifetime_allocator_allocate_on_page (
    struct render_backend_frame_lifetime_allocator_t *allocator,
    struct render_backend_frame_lifetime_allocator_page_t *page,
    uint64_t size,
    uint64_t alignment);

struct render_backend_frame_lifetime_allocator_allocation_t render_backend_frame_lifetime_allocator_allocate (
    struct render_backend_frame_lifetime_allocator_t *allocator, uint64_t size, uint64_t alignment);

struct render_backend_frame_lifetime_allocator_allocation_t render_backend_system_allocate_for_staging (
    struct render_backend_system_t *system, uint64_t size);

void render_backend_frame_lifetime_allocator_retire_old_allocations (
    struct render_backend_frame_lifetime_allocator_t *allocator);

void render_backend_frame_lifetime_allocator_clean_empty_pages (
    struct render_backend_frame_lifetime_allocator_t *allocator);

void render_backend_system_destroy_frame_lifetime_allocator (
    struct render_backend_system_t *system,
    struct render_backend_frame_lifetime_allocator_t *frame_lifetime_allocator,
    kan_bool_t destroy_buffers);

struct image_frame_buffer_attachment_t
{
    struct image_frame_buffer_attachment_t *next;
    struct render_backend_frame_buffer_t *frame_buffer;
};

struct render_backend_image_t
{
    struct kan_bd_list_node_t list_node;
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

struct render_backend_image_t *render_backend_system_create_image (struct render_backend_system_t *system,
                                                                   struct kan_render_image_description_t *description);

void render_backend_system_destroy_image (struct render_backend_system_t *system, struct render_backend_image_t *image);

struct classic_graphics_pipeline_compilation_request_t
{
    struct kan_bd_list_node_t list_node;
    struct render_backend_classic_graphics_pipeline_t *pipeline;

    uint64_t shader_stages_count;
    VkPipelineShaderStageCreateInfo *shader_stages;

    VkPipelineRasterizationStateCreateInfo rasterization;
    VkPipelineMultisampleStateCreateInfo multisampling;
    VkPipelineDepthStencilStateCreateInfo depth_stencil;
    VkPipelineColorBlendStateCreateInfo color_blending;

    uint64_t color_blending_attachments_count;
    VkPipelineColorBlendAttachmentState *color_blending_attachments;
};

/// \details We use one separate thread for compiling pipelines instead of several threads, because some drivers are
///          observed to have bugs while compiling several pipelines at once (despite the fact that it must be ok
///          by Vulkan specifications). For example:
///          https://community.amd.com/t5/opengl-vulkan/parallel-vkcreategraphicspipelines-calls-lead-to-corrupted/td-p/571884
///          If this changes, we might rethink our approach.
struct render_backend_pipeline_compiler_state_t
{
    kan_thread_handle_t thread;
    kan_mutex_handle_t state_transition_mutex;
    kan_conditional_variable_handle_t has_more_work;
    struct kan_atomic_int_t should_terminate;

    struct kan_bd_list_t classic_graphics_critical;
    struct kan_bd_list_t classic_graphics_active;
    struct kan_bd_list_t classic_graphics_cache;
};

kan_thread_result_t render_backend_pipeline_compiler_state_worker_function (kan_thread_user_data_t user_data);

void render_backend_compiler_state_request_classic_graphics (
    struct render_backend_pipeline_compiler_state_t *state,
    struct render_backend_classic_graphics_pipeline_t *pipeline,
    struct kan_render_classic_graphics_pipeline_description_t *description);

/// \invariant Request must be already detached from queue.
void render_backend_compiler_state_destroy_classic_graphics_request (
    struct classic_graphics_pipeline_compilation_request_t *request);

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

    struct kan_bd_list_t surfaces;
    struct kan_bd_list_t frame_buffers;
    struct kan_bd_list_t passes;
    struct kan_bd_list_t classic_graphics_pipeline_families;
    struct kan_bd_list_t classic_graphics_pipelines;
    struct kan_bd_list_t buffers;
    struct kan_bd_list_t frame_lifetime_allocators;
    struct kan_bd_list_t images;

    /// \details Still listed in frame lifetime allocator list above, but referenced here too for usability.
    struct render_backend_frame_lifetime_allocator_t *staging_frame_lifetime_allocator;

    struct render_backend_pipeline_compiler_state_t compiler_state;

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
    kan_allocation_group_t pipeline_family_wrapper_allocation_group;
    kan_allocation_group_t pipeline_wrapper_allocation_group;
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

void render_backend_memory_profiling_init (struct render_backend_system_t *system);

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

static inline void render_backend_pipeline_compiler_state_remove_classic_graphics_request_unsafe (
    struct render_backend_pipeline_compiler_state_t *state,
    struct classic_graphics_pipeline_compilation_request_t *request)
{
    switch (request->pipeline->compilation_priority)
    {
    case KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CRITICAL:
        kan_bd_list_remove (&state->classic_graphics_critical, &request->list_node);
        break;

    case KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_ACTIVE:
        kan_bd_list_remove (&state->classic_graphics_active, &request->list_node);
        break;

    case KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CACHE:
        kan_bd_list_remove (&state->classic_graphics_cache, &request->list_node);
        break;
    }

    request->list_node.next = NULL;
    request->list_node.previous = NULL;
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
static inline uint64_t render_backend_image_calculate_gpu_size (struct render_backend_system_t *system,
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

KAN_C_HEADER_END