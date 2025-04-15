#include <string.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/container/hash_storage.h>
#include <kan/container/list.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/context/all_system_names.h>
#include <kan/context/render_backend_system.h>
#include <kan/context/vulkan_memory_allocator.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/memory/allocation.h>
#include <kan/platform/application.h>
#include <kan/precise_time/precise_time.h>
#include <kan/threading/atomic.h>
#include <kan/threading/conditional_variable.h>
#include <kan/threading/mutex.h>
#include <kan/threading/thread.h>

KAN_C_HEADER_BEGIN

/// \brief Vulkan API uses 32 bit integers for almost any unsigned integer data.
typedef uint32_t vulkan_size_t;

/// \brief Vulkan API uses 32 bit integers for almost any signed integer data.
typedef int32_t vulkan_offset_t;

#if VK_USE_64_BIT_PTR_DEFINES == 1
/// \brief Helper for correctly converting handles to uint64_t for debug data submission.
#    define CONVERT_HANDLE_FOR_DEBUG (uint64_t)
#else
/// \brief Helper for correctly converting handles to uint64_t for debug data submission.
#    define CONVERT_HANDLE_FOR_DEBUG
#endif

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
    kan_allocation_group_t gpu_buffer_read_back_storage_group;
    kan_allocation_group_t gpu_image_group;

    VkAllocationCallbacks vulkan_allocation_callbacks;
    VmaDeviceMemoryCallbacks vma_device_memory_callbacks;
};
#endif

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
    kan_instance_size_t images_count;
    VkImage *images;
    VkImageView *image_views;

    VkSemaphore image_available_semaphores[KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT];
    vulkan_size_t acquired_image_frame;
    vulkan_size_t acquired_image_index;
    kan_bool_t needs_recreation;

    VkImageLayout current_frame_layout;
    struct surface_frame_buffer_attachment_t *first_frame_buffer_attachment;

    vulkan_size_t swap_chain_creation_window_width;
    vulkan_size_t swap_chain_creation_window_height;

    kan_application_system_window_t window_handle;
    kan_application_system_window_resource_id_t resource_id;

    enum kan_render_surface_present_mode_t present_modes_queue[KAN_RENDER_SURFACE_PRESENT_MODE_COUNT];
};

/// \details We have fully separate command state with pools and buffers for every frame in flight index,
///          because it allows us to reset full pools instead of separate buffer which should be better from performance
///          point of view and is advised on docs.vulkan.org.
struct render_backend_command_state_t
{
    VkCommandPool command_pool;
    VkCommandBuffer primary_command_buffer;

    /// \details Currently, all secondary command buffers use the same pool,
    ///          therefore we need global lock for command submission.
    struct kan_atomic_int_t command_operation_lock;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct mutator_t)
    struct kan_dynamic_array_t secondary_command_buffers;

    kan_instance_size_t secondary_command_buffers_used;
};

struct render_backend_descriptor_set_allocation_t
{
    VkDescriptorSet descriptor_set;
    struct render_backend_descriptor_set_pool_t *source_pool;
};

struct scheduled_buffer_flush_transfer_t
{
    struct scheduled_buffer_flush_transfer_t *next;
    struct render_backend_buffer_t *source_buffer;
    struct render_backend_buffer_t *target_buffer;
    vulkan_size_t source_offset;
    vulkan_size_t target_offset;
    vulkan_size_t size;
};

struct scheduled_buffer_flush_t
{
    struct scheduled_buffer_flush_t *next;
    struct render_backend_buffer_t *buffer;
    vulkan_size_t offset;
    vulkan_size_t size;
};

struct scheduled_image_upload_t
{
    struct scheduled_image_upload_t *next;
    struct render_backend_image_t *image;
    uint8_t layer;
    uint8_t mip;
    struct render_backend_buffer_t *staging_buffer;
    vulkan_size_t staging_buffer_offset;
    vulkan_size_t staging_buffer_size;
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
    uint8_t layer;
    uint8_t first;
    uint8_t last;
};

struct scheduled_image_copy_data_t
{
    struct scheduled_image_copy_data_t *next;
    struct render_backend_image_t *from_image;
    struct render_backend_image_t *to_image;
    uint8_t from_layer;
    uint8_t from_mip;
    uint8_t to_layer;
    uint8_t to_mip;
};

struct scheduled_surface_read_back_t
{
    struct scheduled_surface_read_back_t *next;
    struct render_backend_surface_t *surface;
    struct render_backend_buffer_t *read_back_buffer;
    vulkan_size_t read_back_offset;
    struct render_backend_read_back_status_t *status;
};

struct scheduled_buffer_read_back_t
{
    struct scheduled_buffer_read_back_t *next;
    struct render_backend_buffer_t *buffer;
    vulkan_size_t offset;
    vulkan_size_t slice;

    struct render_backend_buffer_t *read_back_buffer;
    vulkan_size_t read_back_offset;

    struct render_backend_read_back_status_t *status;
};

struct scheduled_image_read_back_t
{
    struct scheduled_image_read_back_t *next;
    struct render_backend_image_t *image;
    uint8_t layer;
    uint8_t mip;

    struct render_backend_buffer_t *read_back_buffer;
    vulkan_size_t read_back_offset;

    struct render_backend_read_back_status_t *status;
};

struct scheduled_surface_blit_request_t
{
    struct scheduled_surface_blit_request_t *next;
    struct render_backend_surface_t *surface;
    struct render_backend_image_t *image;
    uint8_t image_layer;
    struct kan_render_integer_region_t image_region;
    struct kan_render_integer_region_t surface_region;
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

struct scheduled_pipeline_parameter_set_destroy_t
{
    struct scheduled_pipeline_parameter_set_destroy_t *next;
    struct render_backend_pipeline_parameter_set_t *set;
};

struct scheduled_graphics_pipeline_destroy_t
{
    struct scheduled_graphics_pipeline_destroy_t *next;
    struct render_backend_graphics_pipeline_t *pipeline;
};

struct scheduled_pipeline_parameter_set_layout_destroy_t
{
    struct scheduled_pipeline_parameter_set_layout_destroy_t *next;
    struct render_backend_pipeline_parameter_set_layout_t *layout;
};

struct scheduled_detached_descriptor_set_destroy_t
{
    struct scheduled_detached_descriptor_set_destroy_t *next;
    struct render_backend_descriptor_set_allocation_t allocation;
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
    kan_allocation_group_t gpu_allocation_group;
#endif
};

struct render_backend_read_back_status_t
{
    struct render_backend_read_back_status_t *next;
    struct render_backend_system_t *system;
    enum kan_render_read_back_state_t state;
    kan_bool_t referenced_in_schedule;
    kan_bool_t referenced_outside;
};

struct render_backend_schedule_state_t
{
    struct kan_stack_group_allocator_t item_allocator;

    /// \brief Common lock for submitting scheduled operations.
    struct kan_atomic_int_t schedule_lock;

    struct scheduled_buffer_flush_transfer_t *first_scheduled_buffer_flush_transfer;
    struct scheduled_buffer_flush_t *first_scheduled_buffer_flush;
    struct scheduled_image_upload_t *first_scheduled_image_upload;
    struct scheduled_frame_buffer_create_t *first_scheduled_frame_buffer_create;
    struct scheduled_image_mip_generation_t *first_scheduled_image_mip_generation;
    struct scheduled_image_copy_data_t *first_scheduled_image_copy_data;
    struct scheduled_surface_read_back_t *first_scheduled_surface_read_back;
    struct scheduled_buffer_read_back_t *first_scheduled_buffer_read_back;
    struct scheduled_surface_blit_request_t *first_scheduled_frame_end_surface_blit;
    struct scheduled_image_read_back_t *first_scheduled_image_read_back;

    struct scheduled_frame_buffer_destroy_t *first_scheduled_frame_buffer_destroy;
    struct scheduled_detached_frame_buffer_destroy_t *first_scheduled_detached_frame_buffer_destroy;
    struct scheduled_pass_destroy_t *first_scheduled_pass_destroy;
    struct scheduled_pipeline_parameter_set_destroy_t *first_scheduled_pipeline_parameter_set_destroy;
    struct scheduled_detached_descriptor_set_destroy_t *first_scheduled_detached_descriptor_set_destroy;
    struct scheduled_graphics_pipeline_destroy_t *first_scheduled_graphics_pipeline_destroy;
    struct scheduled_pipeline_parameter_set_layout_destroy_t *first_scheduled_pipeline_parameter_set_layout_destroy;
    struct scheduled_buffer_destroy_t *first_scheduled_buffer_destroy;
    struct scheduled_frame_lifetime_allocator_destroy_t *first_scheduled_frame_lifetime_allocator_destroy;
    struct scheduled_detached_image_view_destroy_t *first_scheduled_detached_image_view_destroy;
    struct scheduled_image_destroy_t *first_scheduled_image_destroy;
    struct scheduled_detached_image_destroy_t *first_scheduled_detached_image_destroy;

    /// \details Read back statuses are allocated through batched allocator instead of stack group allocator,
    ///          because they can clog item allocator during continuous read back by preventing item allocator reset.
    struct render_backend_read_back_status_t *first_read_back_status;
};

struct render_backend_frame_buffer_image_attachment_t
{
    struct render_backend_image_t *data;
    uint8_t layer;
};

struct render_backend_frame_buffer_attachment_t
{
    enum kan_render_frame_buffer_attachment_type_t type;
    union
    {
        struct render_backend_frame_buffer_image_attachment_t image;
        struct render_backend_surface_t *surface;
    };
};

struct render_backend_frame_buffer_t
{
    struct kan_bd_list_node_t list_node;
    struct render_backend_system_t *system;

    VkFramebuffer instance;
    kan_instance_size_t instance_array_size;

    /// \details If there is a surface attachment, we need to create a separate frame buffer for each surface image.
    ///          Therefore, in that case frame_buffer_array is used instead of frame_buffer field.
    VkFramebuffer *instance_array;

    /// \brief If this frame buffer is attached to surface,
    ///        this variable contains index if frame buffer instance to use.
    vulkan_size_t instance_index;

    /// \brief Image views for attached images. VK_NULL_HANDLE is inserted in place of surface attachment if any.
    VkImageView *image_views;

    struct render_backend_pass_t *pass;
    kan_instance_size_t attachments_count;
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
    kan_interned_string_t tracking_name;
};

struct render_backend_pass_t *render_backend_system_create_pass (struct render_backend_system_t *system,
                                                                 struct kan_render_pass_description_t *description);

void render_backend_system_destroy_pass (struct render_backend_system_t *system, struct render_backend_pass_t *pass);

struct render_backend_pass_instance_instance_dependency_t
{
    struct render_backend_pass_instance_instance_dependency_t *next;
    struct render_backend_pass_instance_t *dependant_pass_instance;
};

struct render_backend_pass_instance_checkpoint_t
{
    struct render_backend_system_t *system;
    struct render_backend_pass_instance_instance_dependency_t *first_dependant_instance;
    struct render_backend_pass_instance_checkpoint_dependency_t *first_dependant_checkpoint;
};

struct render_backend_pass_instance_checkpoint_dependency_t
{
    struct render_backend_pass_instance_checkpoint_dependency_t *next;
    struct render_backend_pass_instance_checkpoint_t *dependant_checkpoint;
};

struct render_backend_pass_instance_t
{
    struct render_backend_system_t *system;
    struct render_backend_pass_t *pass;

    VkCommandBuffer command_buffer;
    struct render_backend_frame_buffer_t *frame_buffer;
    struct render_backend_graphics_pipeline_t *current_pipeline;

    kan_instance_size_t dependencies_left;
    struct render_backend_pass_instance_instance_dependency_t *first_dependant_instance;
    struct render_backend_pass_instance_checkpoint_dependency_t *first_dependant_checkpoint;

    struct scheduled_surface_blit_request_t *pass_end_surface_blit_requests;

    struct kan_bd_list_node_t node_in_available;
    struct kan_bd_list_node_t node_in_all;

    VkRenderPassBeginInfo render_pass_begin_info;
    VkClearValue clear_values[];
};

void render_backend_pass_instance_add_dependency_internal (struct render_backend_pass_instance_t *dependant,
                                                           struct render_backend_pass_instance_t *dependency);

struct render_backend_layout_binding_t
{
    enum kan_render_parameter_binding_type_t type;
    kan_render_size_t descriptor_count;
    vulkan_size_t used_stage_mask;
};

struct render_backend_pipeline_parameter_set_layout_t
{
    struct kan_hash_storage_node_t node;
    struct render_backend_system_t *system;

    VkDescriptorSetLayout layout;
    struct kan_atomic_int_t reference_count;

    uint8_t uniform_buffers_count;
    uint8_t storage_buffers_count;
    uint8_t samplers_count;
    uint8_t images_count;

    kan_interned_string_t tracking_name;
    kan_instance_size_t bindings_count;
    struct render_backend_layout_binding_t bindings[];
};

struct render_backend_pipeline_parameter_set_layout_t *render_backend_system_register_pipeline_parameter_set_layout (
    struct render_backend_system_t *system, struct kan_render_pipeline_parameter_set_layout_description_t *description);

void render_backend_system_destroy_pipeline_parameter_set_layout (
    struct render_backend_system_t *system, struct render_backend_pipeline_parameter_set_layout_t *layout);

struct render_backend_code_module_t
{
    struct kan_bd_list_node_t list_node;
    struct render_backend_system_t *system;
    VkShaderModule module;
    struct kan_atomic_int_t links;
    kan_interned_string_t tracking_name;
};

struct render_backend_code_module_t *render_backend_system_create_code_module (struct render_backend_system_t *system,
                                                                               vulkan_size_t code_length,
                                                                               void *code,
                                                                               kan_interned_string_t tracking_name);

void render_backend_system_unlink_code_module (struct render_backend_code_module_t *code_module);

void render_backend_system_destroy_code_module (struct render_backend_system_t *system,
                                                struct render_backend_code_module_t *code_module);

struct render_backend_pipeline_layout_t
{
    struct kan_hash_storage_node_t node;
    VkPipelineLayout layout;
    kan_instance_size_t usage_count;

    kan_instance_size_t push_constant_size;
    kan_instance_size_t set_layouts_count;
    struct render_backend_pipeline_parameter_set_layout_t *set_layouts[];
};

struct render_backend_pipeline_layout_t *render_backend_system_register_pipeline_layout (
    struct render_backend_system_t *system,
    kan_instance_size_t push_constant_size,
    kan_instance_size_t parameter_set_layouts_count,
    kan_render_pipeline_parameter_set_layout_t *parameter_set_layouts,
    kan_interned_string_t tracking_name);

void render_backend_system_destroy_pipeline_layout (struct render_backend_system_t *system,
                                                    struct render_backend_pipeline_layout_t *layout);

enum pipeline_compilation_state_t
{
    PIPELINE_COMPILATION_STATE_PENDING = 0u,
    PIPELINE_COMPILATION_STATE_EXECUTION,
    PIPELINE_COMPILATION_STATE_SUCCESS,
    PIPELINE_COMPILATION_STATE_FAILURE,
};

struct render_backend_graphics_pipeline_t
{
    struct kan_bd_list_node_t list_node;
    struct render_backend_system_t *system;

    VkPipeline pipeline;
    struct render_backend_pipeline_layout_t *layout;

    struct render_backend_pass_t *pass;
    float min_depth;
    float max_depth;

    enum kan_render_pipeline_compilation_priority_t compilation_priority;
    enum pipeline_compilation_state_t compilation_state;
    struct graphics_pipeline_compilation_request_t *compilation_request;

    kan_interned_string_t tracking_name;
};

struct render_backend_graphics_pipeline_t *render_backend_system_create_graphics_pipeline (
    struct render_backend_system_t *system,
    struct kan_render_graphics_pipeline_description_t *description,
    enum kan_render_pipeline_compilation_priority_t compilation_priority);

void render_backend_system_destroy_graphics_pipeline (struct render_backend_system_t *system,
                                                      struct render_backend_graphics_pipeline_t *pipeline);

enum render_backend_buffer_family_t
{
    RENDER_BACKEND_BUFFER_FAMILY_RESOURCE = 0u,
    RENDER_BACKEND_BUFFER_FAMILY_STAGING,
    RENDER_BACKEND_BUFFER_FAMILY_HOST_FRAME_LIFETIME_ALLOCATOR,
    RENDER_BACKEND_BUFFER_FAMILY_DEVICE_FRAME_LIFETIME_ALLOCATOR,
};

struct render_backend_stable_parameter_set_data_t
{
    struct render_backend_descriptor_set_allocation_t allocation;

    /// \details Stable parameter sets are expected to be close-to-immutable, but still can be rarely updated. When it
    ///          happens, we need to know whether set was ever submitted to command buffers. It is needed to avoid
    ///          excessive allocations when update was called twice during the same frame on one parameter set before
    ///          even submitting it.
    kan_bool_t has_been_submitted;
};

struct render_backend_unstable_parameter_set_data_t
{
    struct render_backend_descriptor_set_allocation_t *allocations;

    /// \details When last accessed index is not equal to current frame index, it means that allocations wasn't yet
    ///          accessed in current frame context and set data must be copied from last accessed allocations index.
    ///          It can be detected both when updating parameter set and when submitting it to command buffer.
    vulkan_size_t last_accessed_allocation_index;
};

struct render_backend_parameter_set_render_target_attachment_t
{
    struct render_backend_parameter_set_render_target_attachment_t *next;
    vulkan_size_t binding;
    struct render_backend_image_t *image;
};

struct render_backend_pipeline_parameter_set_t
{
    struct kan_bd_list_node_t list_node;
    struct render_backend_system_t *system;

    struct render_backend_pipeline_parameter_set_layout_t *layout;
    kan_bool_t stable_binding;

    union
    {
        struct render_backend_stable_parameter_set_data_t stable;
        struct render_backend_unstable_parameter_set_data_t unstable;
    };

    VkImageView *bound_image_views;
    struct render_backend_parameter_set_render_target_attachment_t *first_render_target_attachment;
    kan_interned_string_t tracking_name;
};

struct render_backend_pipeline_parameter_set_t *render_backend_system_create_pipeline_parameter_set (
    struct render_backend_system_t *system, struct kan_render_pipeline_parameter_set_description_t *description);

void render_backend_system_destroy_pipeline_parameter_set (struct render_backend_system_t *system,
                                                           struct render_backend_pipeline_parameter_set_t *set);

struct render_backend_buffer_t
{
    struct kan_bd_list_node_t list_node;
    struct render_backend_system_t *system;

    VkBuffer buffer;
    VmaAllocation allocation;
    enum render_backend_buffer_family_t family;
    enum kan_render_buffer_type_t type;
    void *mapped_memory;
    vulkan_size_t full_size;
    kan_interned_string_t tracking_name;
    kan_bool_t needs_flush;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    kan_allocation_group_t device_allocation_group;
#endif
};

struct render_backend_buffer_t *render_backend_system_create_buffer (struct render_backend_system_t *system,
                                                                     enum render_backend_buffer_family_t family,
                                                                     enum kan_render_buffer_type_t buffer_type,
                                                                     vulkan_size_t full_size,
                                                                     kan_interned_string_t tracking_name);

void render_backend_system_destroy_buffer (struct render_backend_system_t *system,
                                           struct render_backend_buffer_t *buffer);

#define CHUNK_FREE_MARKER UINT32_MAX

struct render_backend_frame_lifetime_allocator_chunk_t
{
    struct render_backend_frame_lifetime_allocator_chunk_t *next;
    struct render_backend_frame_lifetime_allocator_chunk_t *previous;
    struct render_backend_frame_lifetime_allocator_chunk_t *next_free;

    vulkan_size_t offset;
    vulkan_size_t size;
    vulkan_size_t occupied_by_frame;
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
    vulkan_size_t page_size;
    kan_interned_string_t tracking_name;
    kan_interned_string_t buffer_tracking_name;
};

struct render_backend_frame_lifetime_allocator_allocation_t
{
    struct render_backend_buffer_t *buffer;
    vulkan_size_t offset;
};

#define STAGING_BUFFER_ALLOCATION_ALIGNMENT _Alignof (float)

struct render_backend_frame_lifetime_allocator_t *render_backend_system_create_frame_lifetime_allocator (
    struct render_backend_system_t *system,
    enum render_backend_buffer_family_t buffer_family,
    enum kan_render_buffer_type_t buffer_type,
    vulkan_size_t page_size,
    kan_interned_string_t tracking_name);

struct render_backend_frame_lifetime_allocator_allocation_t render_backend_frame_lifetime_allocator_allocate_on_page (
    struct render_backend_frame_lifetime_allocator_t *allocator,
    struct render_backend_frame_lifetime_allocator_page_t *page,
    vulkan_size_t size,
    vulkan_size_t alignment);

struct render_backend_frame_lifetime_allocator_allocation_t render_backend_frame_lifetime_allocator_allocate (
    struct render_backend_frame_lifetime_allocator_t *allocator, vulkan_size_t size, vulkan_size_t alignment);

struct render_backend_frame_lifetime_allocator_allocation_t render_backend_system_allocate_for_staging (
    struct render_backend_system_t *system, vulkan_size_t size);

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

struct image_parameter_set_attachment_t
{
    struct image_parameter_set_attachment_t *next;
    struct render_backend_pipeline_parameter_set_t *set;
    vulkan_size_t binding;
};

struct render_backend_image_t
{
    struct kan_bd_list_node_t list_node;
    struct render_backend_system_t *system;

    VkImage image;
    VmaAllocation allocation;

    struct kan_render_image_description_t description;
    union
    {
        VkImageLayout last_command_layout_single_layer;
        VkImageLayout *last_command_layouts_per_layer;
    };

    struct image_frame_buffer_attachment_t *first_frame_buffer_attachment;
    struct image_parameter_set_attachment_t *first_parameter_set_attachment;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    kan_allocation_group_t device_allocation_group;
#endif
};

struct render_backend_image_t *render_backend_system_create_image (struct render_backend_system_t *system,
                                                                   struct kan_render_image_description_t *description);

void render_backend_system_destroy_image (struct render_backend_system_t *system, struct render_backend_image_t *image);

struct graphics_pipeline_compilation_request_t
{
    struct kan_bd_list_node_t list_node;
    struct render_backend_graphics_pipeline_t *pipeline;

    kan_instance_size_t shader_stages_count;
    VkPipelineShaderStageCreateInfo *shader_stages;

    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    kan_instance_size_t input_bindings_count;
    VkVertexInputBindingDescription *input_bindings;

    kan_instance_size_t attributes_count;
    VkVertexInputAttributeDescription *attributes;

    VkPipelineRasterizationStateCreateInfo rasterization;
    VkPipelineMultisampleStateCreateInfo multisampling;
    VkPipelineDepthStencilStateCreateInfo depth_stencil;
    VkPipelineColorBlendStateCreateInfo color_blending;

    kan_instance_size_t color_blending_attachments_count;
    VkPipelineColorBlendAttachmentState *color_blending_attachments;

    kan_instance_size_t linked_code_modules_count;
    struct render_backend_code_module_t **linked_code_modules;
};

/// \details We use one separate thread for compiling pipelines instead of several threads, because some drivers are
///          observed to have bugs while compiling several pipelines at once (despite the fact that it must be ok
///          by Vulkan specifications). For example:
///          https://community.amd.com/t5/opengl-vulkan/parallel-vkcreategraphicspipelines-calls-lead-to-corrupted/td-p/571884
///          If this changes, we might rethink our approach.
struct render_backend_pipeline_compiler_state_t
{
    kan_thread_t thread;
    kan_mutex_t state_transition_mutex;
    kan_conditional_variable_t has_more_work;
    struct kan_atomic_int_t should_terminate;

    struct kan_bd_list_t graphics_critical;
    struct kan_bd_list_t graphics_active;
    struct kan_bd_list_t graphics_cache;
};

kan_thread_result_t render_backend_pipeline_compiler_state_worker_function (kan_thread_user_data_t user_data);

void render_backend_compiler_state_request_graphics (struct render_backend_pipeline_compiler_state_t *state,
                                                     struct render_backend_graphics_pipeline_t *pipeline,
                                                     struct kan_render_graphics_pipeline_description_t *description);

/// \invariant Request must be already detached from queue.
void render_backend_compiler_state_destroy_graphics_request (struct graphics_pipeline_compilation_request_t *request);

struct render_backend_descriptor_set_pool_t
{
    struct kan_bd_list_node_t list_node;
    VkDescriptorPool pool;
    vulkan_size_t active_allocations;
};

struct render_backend_descriptor_set_allocator_t
{
    struct kan_bd_list_t pools;
    struct kan_atomic_int_t multithreaded_access_lock;

    kan_instance_size_t total_set_allocations;
    kan_instance_size_t uniform_buffer_binding_allocations;
    kan_instance_size_t storage_buffer_binding_allocations;
    kan_instance_size_t sampler_binding_allocations;
    kan_instance_size_t image_binding_allocations;
};

void render_backend_descriptor_set_allocator_init (struct render_backend_descriptor_set_allocator_t *allocator);

struct render_backend_descriptor_set_allocation_t render_backend_descriptor_set_allocator_allocate (
    struct render_backend_system_t *system,
    struct render_backend_descriptor_set_allocator_t *allocator,
    struct render_backend_pipeline_parameter_set_layout_t *layout);

void render_backend_descriptor_set_allocator_free (struct render_backend_system_t *system,
                                                   struct render_backend_descriptor_set_allocator_t *allocator,
                                                   struct render_backend_descriptor_set_allocation_t *allocation);

void render_backend_descriptor_set_allocator_shutdown (struct render_backend_system_t *system,
                                                       struct render_backend_descriptor_set_allocator_t *allocator);

/// \details Mutation is a term for transfer-and-or-update operation for descriptors. There are multiple places in code
///          where we need to either transfer data from one set to another, update set or do both at the same time.
///          If we need to both transfer and update, it is much more efficient to merge this operation into one.
///          Therefore, it is merge into one mutation operation.
void render_backend_apply_descriptor_set_mutation (struct render_backend_pipeline_parameter_set_t *set_context,
                                                   VkDescriptorSet source_set,
                                                   VkDescriptorSet target_set,
                                                   kan_instance_size_t bindings_count,
                                                   struct kan_render_parameter_update_description_t *bindings);

struct render_backend_cached_sampler_t
{
    struct render_backend_cached_sampler_t *next;
    VkSampler sampler;

    /// \brief Packed description values for faster comparison.
    kan_loop_size_t packed_description_values;

    struct kan_render_sampler_t description;
};

VkSampler render_backend_resolve_cached_sampler (struct render_backend_system_t *system,
                                                 struct kan_render_sampler_t *sampler);

struct render_backend_system_t
{
    kan_context_t context;

    VkInstance instance;
    VkDevice device;
    enum kan_render_device_memory_type_t device_memory_type;
    VkPhysicalDevice physical_device;

    VmaAllocator gpu_memory_allocator;
    vulkan_size_t device_queue_family_index;
    VkQueue device_queue;

    kan_bool_t frame_started;
    vulkan_size_t current_frame_in_flight_index;

    VkSemaphore render_finished_semaphores[KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT];
    VkFence in_flight_fences[KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT];

    /// \brief True if present was skipped due to absence of swap chains for this frames.
    /// \details Needed to properly handle synchronization between frames in such cases.
    kan_bool_t present_skipped_flags[KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT];

    struct render_backend_command_state_t command_states[KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT];
    struct render_backend_schedule_state_t schedule_states[KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT];

    /// \brief Lock for safe resource creation registration in multithreaded environment.
    /// \details Reused for various lists as we only insert to these lists under this lock.
    ///          Therefore, reusing it would not cause too much wait time.
    struct kan_atomic_int_t resource_registration_lock;

    /// \brief Separate lock for pipeline parameter set layout registration: it is more difficult due to the need to
    ///        keep cache consistent, therefore it would waste too much time if it was under common registration lock.
    struct kan_atomic_int_t pipeline_parameter_set_layout_registration_lock;

    /// \brief Separate lock for pipeline layout registration: it is more difficult due to the need to
    ///        keep cache consistent, therefore it would waste too much time if it was under common registration lock.
    struct kan_atomic_int_t pipeline_layout_registration_lock;

    /// \details Lock used for operations than change pass instance state by moving it in different lists.
    struct kan_atomic_int_t pass_instance_state_management_lock;

    struct kan_bd_list_t surfaces;
    struct kan_bd_list_t frame_buffers;
    struct kan_bd_list_t passes;
    struct kan_bd_list_t pass_instances;
    struct kan_bd_list_t pass_instances_available;

    struct kan_hash_storage_t pipeline_parameter_set_layouts;
    struct kan_hash_storage_t pipeline_layouts;

    struct kan_bd_list_t code_modules;
    struct kan_bd_list_t graphics_pipelines;
    struct kan_bd_list_t pipeline_parameter_sets;
    struct kan_bd_list_t buffers;
    struct kan_bd_list_t frame_lifetime_allocators;
    struct kan_bd_list_t images;

    /// \details Still listed in frame lifetime allocator list above, but referenced here too for usability.
    struct render_backend_frame_lifetime_allocator_t *staging_frame_lifetime_allocator;

    struct kan_atomic_int_t sampler_cache_lock;
    struct render_backend_cached_sampler_t *first_cached_sampler;

    struct render_backend_pipeline_compiler_state_t compiler_state;
    struct render_backend_descriptor_set_allocator_t descriptor_set_allocator;

    struct kan_stack_group_allocator_t pass_instance_allocator;

    VkDescriptorSetLayout empty_descriptor_set_layout;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
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
    kan_allocation_group_t pass_instance_allocation_group;
    kan_allocation_group_t parameter_set_layout_wrapper_allocation_group;
    kan_allocation_group_t code_module_wrapper_allocation_group;
    kan_allocation_group_t pipeline_layout_wrapper_allocation_group;
    kan_allocation_group_t pipeline_wrapper_allocation_group;
    kan_allocation_group_t pipeline_parameter_set_wrapper_allocation_group;
    kan_allocation_group_t buffer_wrapper_allocation_group;
    kan_allocation_group_t frame_lifetime_wrapper_allocation_group;
    kan_allocation_group_t image_wrapper_allocation_group;
    kan_allocation_group_t descriptor_set_wrapper_allocation_group;
    kan_allocation_group_t read_back_status_allocation_group;
    kan_allocation_group_t cached_samplers_allocation_group;

    kan_cpu_section_t section_create_surface;
    kan_cpu_section_t section_create_frame_buffer;
    kan_cpu_section_t section_create_frame_buffer_internal;
    kan_cpu_section_t section_create_pass;
    kan_cpu_section_t section_create_pass_internal;
    kan_cpu_section_t section_create_pass_instance;
    kan_cpu_section_t section_register_pipeline_parameter_set_layout;
    kan_cpu_section_t section_create_code_module;
    kan_cpu_section_t section_create_code_module_internal;
    kan_cpu_section_t section_register_pipeline_layout;
    kan_cpu_section_t section_create_graphics_pipeline;
    kan_cpu_section_t section_create_graphics_pipeline_internal;
    kan_cpu_section_t section_create_pipeline_parameter_set;
    kan_cpu_section_t section_create_pipeline_parameter_set_internal;
    kan_cpu_section_t section_create_buffer;
    kan_cpu_section_t section_create_buffer_internal;
    kan_cpu_section_t section_create_frame_lifetime_allocator;
    kan_cpu_section_t section_create_frame_lifetime_allocator_internal;
    kan_cpu_section_t section_create_image;
    kan_cpu_section_t section_create_image_internal;

    kan_cpu_section_t section_surface_init_with_window;
    kan_cpu_section_t section_surface_shutdown_with_window;
    kan_cpu_section_t section_surface_create_swap_chain;
    kan_cpu_section_t section_surface_destroy_swap_chain;

    kan_cpu_section_t section_pipeline_compiler_request;

    kan_cpu_section_t section_pipeline_compilation;
    kan_cpu_section_t section_wait_for_pipeline_compilation;

    kan_cpu_section_t section_descriptor_set_allocator_allocate;
    kan_cpu_section_t section_descriptor_set_allocator_free;

    kan_cpu_section_t section_apply_descriptor_set_mutation;
    kan_cpu_section_t section_pipeline_parameter_set_update;

    kan_cpu_section_t section_frame_lifetime_allocator_allocate;
    kan_cpu_section_t section_frame_lifetime_allocator_retire_old_allocations;
    kan_cpu_section_t section_frame_lifetime_allocator_clean_empty_pages;
    kan_cpu_section_t section_allocate_for_staging;

    kan_cpu_section_t section_image_create_on_device;
    kan_cpu_section_t section_image_upload;
    kan_cpu_section_t section_image_resize_render_target;

    kan_cpu_section_t section_next_frame;
    kan_cpu_section_t section_next_frame_synchronization;
    kan_cpu_section_t section_next_frame_acquire_images;
    kan_cpu_section_t section_next_frame_command_pool_reset;
    kan_cpu_section_t section_next_frame_destruction_schedule;
    kan_cpu_section_t section_next_frame_destruction_schedule_waiting_pipeline_compilation;

    kan_cpu_section_t section_submit_previous_frame;
    kan_cpu_section_t section_submit_transfer;
    kan_cpu_section_t section_submit_graphics;
    kan_cpu_section_t section_submit_mip_generation;
    kan_cpu_section_t section_execute_frame_buffer_creation;
    kan_cpu_section_t section_submit_blit_requests;
    kan_cpu_section_t section_submit_pass_instance;
    kan_cpu_section_t section_pass_instance_resolve_checkpoints;
    kan_cpu_section_t section_pass_instance_sort_and_submission;
    kan_cpu_section_t section_submit_read_back;
    kan_cpu_section_t section_present;

    struct kan_render_supported_devices_t *supported_devices;
    struct kan_render_supported_device_info_t *selected_device_info;

    VkPhysicalDeviceMemoryProperties selected_device_memory_properties;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    struct memory_profiling_t memory_profiling;
#endif

    kan_interned_string_t application_info_name;
    vulkan_size_t version_major;
    vulkan_size_t version_minor;
    vulkan_size_t version_patch;

    kan_interned_string_t interned_temporary_staging_buffer;
};

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
#    define VULKAN_ALLOCATION_CALLBACKS(SYSTEM) (&(SYSTEM)->memory_profiling.vulkan_allocation_callbacks)

void render_backend_memory_profiling_init (struct render_backend_system_t *system);

static inline void transfer_memory_between_groups (vulkan_size_t amount,
                                                   kan_allocation_group_t from,
                                                   kan_allocation_group_t to)
{
    kan_allocation_group_free (from, amount);
    kan_allocation_group_allocate (to, amount);
}
#else
#    define VULKAN_ALLOCATION_CALLBACKS(SYSTEM) NULL
#endif

static inline VkCompareOp to_vulkan_compare_operation (enum kan_render_compare_operation_t operation)
{
    switch (operation)
    {
    case KAN_RENDER_COMPARE_OPERATION_NEVER:
        return VK_COMPARE_OP_NEVER;
        break;

    case KAN_RENDER_COMPARE_OPERATION_ALWAYS:
        return VK_COMPARE_OP_ALWAYS;

    case KAN_RENDER_COMPARE_OPERATION_EQUAL:
        return VK_COMPARE_OP_EQUAL;

    case KAN_RENDER_COMPARE_OPERATION_NOT_EQUAL:
        return VK_COMPARE_OP_NOT_EQUAL;

    case KAN_RENDER_COMPARE_OPERATION_LESS:
        return VK_COMPARE_OP_LESS;

    case KAN_RENDER_COMPARE_OPERATION_LESS_OR_EQUAL:
        return VK_COMPARE_OP_LESS_OR_EQUAL;

    case KAN_RENDER_COMPARE_OPERATION_GREATER:
        return VK_COMPARE_OP_GREATER;

    case KAN_RENDER_COMPARE_OPERATION_GREATER_OR_EQUAL:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;

    case KAN_RENDER_COMPARE_OPERATION_COUNT:
        KAN_ASSERT (KAN_FALSE)
        return VK_COMPARE_OP_NEVER;
    }

    KAN_ASSERT (KAN_FALSE)
    return VK_COMPARE_OP_NEVER;
}

static inline VkImageLayout get_image_layout_info (const struct render_backend_image_t *image, uint8_t layer)
{
    if (image->description.layers == 1u)
    {
        return image->last_command_layout_single_layer;
    }
    else
    {
        return image->last_command_layouts_per_layer[layer];
    }
}

static inline void set_image_layout_info (struct render_backend_image_t *image, uint8_t layer, VkImageLayout layout)
{
    if (image->description.layers == 1u)
    {
        image->last_command_layout_single_layer = layout;
    }
    else
    {
        image->last_command_layouts_per_layer[layer] = layout;
    }
}

static inline VkFormat image_format_to_vulkan (enum kan_render_image_format_t format)
{
    switch (format)
    {
    case KAN_RENDER_IMAGE_FORMAT_R8_SRGB:
        return VK_FORMAT_R8_SRGB;

    case KAN_RENDER_IMAGE_FORMAT_RG16_SRGB:
        return VK_FORMAT_R8G8_SRGB;

    case KAN_RENDER_IMAGE_FORMAT_RGB24_SRGB:
        return VK_FORMAT_R8G8B8_SRGB;

    case KAN_RENDER_IMAGE_FORMAT_RGBA32_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;

    case KAN_RENDER_IMAGE_FORMAT_BGRA32_SRGB:
        return VK_FORMAT_B8G8R8A8_SRGB;

    case KAN_RENDER_IMAGE_FORMAT_R32_SFLOAT:
        return VK_FORMAT_R32_SFLOAT;

    case KAN_RENDER_IMAGE_FORMAT_RG64_SFLOAT:
        return VK_FORMAT_R32G32_SFLOAT;

    case KAN_RENDER_IMAGE_FORMAT_RGB96_SFLOAT:
        return VK_FORMAT_R32G32B32_SFLOAT;

    case KAN_RENDER_IMAGE_FORMAT_RGBA128_SFLOAT:
        return VK_FORMAT_R32G32B32A32_SFLOAT;

    case KAN_RENDER_IMAGE_FORMAT_D16_UNORM:
        return VK_FORMAT_D16_UNORM;

    case KAN_RENDER_IMAGE_FORMAT_D32_SFLOAT:
        return VK_FORMAT_D32_SFLOAT;

    case KAN_RENDER_IMAGE_FORMAT_S8_UINT:
        return VK_FORMAT_S8_UINT;

    case KAN_RENDER_IMAGE_FORMAT_D16_UNORM_S8_UINT:
        return VK_FORMAT_D16_UNORM_S8_UINT;

    case KAN_RENDER_IMAGE_FORMAT_D24_UNORM_S8_UINT:
        return VK_FORMAT_D24_UNORM_S8_UINT;

    case KAN_RENDER_IMAGE_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_FORMAT_D32_SFLOAT_S8_UINT;

    case KAN_RENDER_IMAGE_FORMAT_BC1_RGB_UNORM_BLOCK:
        return VK_FORMAT_BC1_RGB_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_BC1_RGB_SRGB_BLOCK:
        return VK_FORMAT_BC1_RGB_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_BC1_RGBA_UNORM_BLOCK:
        return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_BC1_RGBA_SRGB_BLOCK:
        return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_BC2_UNORM_BLOCK:
        return VK_FORMAT_BC2_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_BC2_SRGB_BLOCK:
        return VK_FORMAT_BC2_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_BC3_UNORM_BLOCK:
        return VK_FORMAT_BC3_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_BC3_SRGB_BLOCK:
        return VK_FORMAT_BC3_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_BC4_UNORM_BLOCK:
        return VK_FORMAT_BC4_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_BC4_SNORM_BLOCK:
        return VK_FORMAT_BC4_SNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_BC5_UNORM_BLOCK:
        return VK_FORMAT_BC5_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_BC5_SNORM_BLOCK:
        return VK_FORMAT_BC5_SNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_BC6H_UFLOAT_BLOCK:
        return VK_FORMAT_BC6H_UFLOAT_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_BC6H_SFLOAT_BLOCK:
        return VK_FORMAT_BC6H_SFLOAT_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_BC7_UNORM_BLOCK:
        return VK_FORMAT_BC7_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_BC7_SRGB_BLOCK:
        return VK_FORMAT_BC7_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ETC2_RGB24_UNORM_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ETC2_RGB24_SRGB_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ETC2_RGBA25_UNORM_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ETC2_RGBA25_SRGB_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ETC2_RGBA32_UNORM_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ETC2_RGBA32_SRGB_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_4x4_UNORM_BLOCK:
        return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_4x4_SRGB_BLOCK:
        return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_5x4_UNORM_BLOCK:
        return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_5x4_SRGB_BLOCK:
        return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_5x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_5x5_SRGB_BLOCK:
        return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_6x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_6x5_SRGB_BLOCK:
        return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_6x6_UNORM_BLOCK:
        return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_6x6_SRGB_BLOCK:
        return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_8x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_8x5_SRGB_BLOCK:
        return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_8x6_UNORM_BLOCK:
        return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_8x6_SRGB_BLOCK:
        return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_8x8_UNORM_BLOCK:
        return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_8x8_SRGB_BLOCK:
        return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x5_SRGB_BLOCK:
        return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x6_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x6_SRGB_BLOCK:
        return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x8_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x8_SRGB_BLOCK:
        return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x10_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x10_SRGB_BLOCK:
        return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_12x10_UNORM_BLOCK:
        return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_12x10_SRGB_BLOCK:
        return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_12x12_UNORM_BLOCK:
        return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_ASTC_12x12_SRGB_BLOCK:
        return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;

    case KAN_RENDER_IMAGE_FORMAT_COUNT:
        KAN_ASSERT (KAN_FALSE)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

enum image_format_class_t
{
    IMAGE_FORMAT_CLASS_COLOR = 0u,
    IMAGE_FORMAT_CLASS_DEPTH,
    IMAGE_FORMAT_CLASS_STENCIL,
    IMAGE_FORMAT_CLASS_DEPTH_STENCIL,
};

static inline enum image_format_class_t get_image_format_class (enum kan_render_image_format_t format)
{
    switch (format)
    {
    case KAN_RENDER_IMAGE_FORMAT_R8_SRGB:
    case KAN_RENDER_IMAGE_FORMAT_RG16_SRGB:
    case KAN_RENDER_IMAGE_FORMAT_RGB24_SRGB:
    case KAN_RENDER_IMAGE_FORMAT_RGBA32_SRGB:
    case KAN_RENDER_IMAGE_FORMAT_BGRA32_SRGB:
    case KAN_RENDER_IMAGE_FORMAT_R32_SFLOAT:
    case KAN_RENDER_IMAGE_FORMAT_RG64_SFLOAT:
    case KAN_RENDER_IMAGE_FORMAT_RGB96_SFLOAT:
    case KAN_RENDER_IMAGE_FORMAT_RGBA128_SFLOAT:
    case KAN_RENDER_IMAGE_FORMAT_BC1_RGB_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_BC1_RGB_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_BC2_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_BC2_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_BC3_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_BC3_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_BC4_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_BC4_SNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_BC5_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_BC5_SNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_BC6H_UFLOAT_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_BC6H_SFLOAT_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_BC7_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_BC7_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ETC2_RGB24_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ETC2_RGB24_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ETC2_RGBA25_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ETC2_RGBA25_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ETC2_RGBA32_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ETC2_RGBA32_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_8x8_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case KAN_RENDER_IMAGE_FORMAT_ASTC_12x12_SRGB_BLOCK:
        return IMAGE_FORMAT_CLASS_COLOR;

    case KAN_RENDER_IMAGE_FORMAT_D16_UNORM:
    case KAN_RENDER_IMAGE_FORMAT_D32_SFLOAT:
        return IMAGE_FORMAT_CLASS_DEPTH;

    case KAN_RENDER_IMAGE_FORMAT_S8_UINT:
        return IMAGE_FORMAT_CLASS_STENCIL;

    case KAN_RENDER_IMAGE_FORMAT_D16_UNORM_S8_UINT:
    case KAN_RENDER_IMAGE_FORMAT_D24_UNORM_S8_UINT:
    case KAN_RENDER_IMAGE_FORMAT_D32_SFLOAT_S8_UINT:
        return IMAGE_FORMAT_CLASS_DEPTH_STENCIL;

    case KAN_RENDER_IMAGE_FORMAT_COUNT:
        KAN_ASSERT (KAN_FALSE)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline struct render_backend_schedule_state_t *render_backend_system_get_schedule_for_memory (
    struct render_backend_system_t *system)
{
    // Always schedule into the current frame, even if it is not yet started.
    return &system->schedule_states[system->current_frame_in_flight_index];
}

static inline struct render_backend_schedule_state_t *render_backend_system_get_schedule_for_destroy (
    struct render_backend_system_t *system)
{
    vulkan_size_t schedule_index = system->current_frame_in_flight_index;
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

static inline void render_backend_pipeline_compiler_state_remove_graphics_request_unsafe (
    struct render_backend_pipeline_compiler_state_t *state, struct graphics_pipeline_compilation_request_t *request)
{
    switch (request->pipeline->compilation_priority)
    {
    case KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CRITICAL:
        kan_bd_list_remove (&state->graphics_critical, &request->list_node);
        break;

    case KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_ACTIVE:
        kan_bd_list_remove (&state->graphics_active, &request->list_node);
        break;

    case KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CACHE:
        kan_bd_list_remove (&state->graphics_cache, &request->list_node);
        break;
    }

    request->list_node.next = NULL;
    request->list_node.previous = NULL;
}
static inline VkImageViewType get_image_view_type (struct kan_render_image_description_t *description)
{
    if (description->depth > 1u)
    {
        return VK_IMAGE_VIEW_TYPE_3D;
    }
    else
    {
        return VK_IMAGE_VIEW_TYPE_2D;
    }
}

static inline VkImageAspectFlags get_image_aspects (struct kan_render_image_description_t *description)
{
    VkImageAspectFlags aspects = 0u;
    switch (get_image_format_class (description->format))
    {
    case IMAGE_FORMAT_CLASS_COLOR:
        aspects |= VK_IMAGE_ASPECT_COLOR_BIT;
        break;

    case IMAGE_FORMAT_CLASS_DEPTH:
        aspects |= VK_IMAGE_ASPECT_DEPTH_BIT;
        break;

    case IMAGE_FORMAT_CLASS_STENCIL:
        aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
        break;

    case IMAGE_FORMAT_CLASS_DEPTH_STENCIL:
        aspects |= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        break;
    }

    return aspects;
}

static inline void kan_render_image_description_calculate_size_at_mip (
    struct kan_render_image_description_t *description,
    uint8_t mip,
    vulkan_size_t *output_width,
    vulkan_size_t *output_height,
    vulkan_size_t *output_depth)
{
    KAN_ASSERT (mip < description->mips)
    *output_width = KAN_MAX (1u, description->width >> mip);
    *output_height = KAN_MAX (1u, description->height >> mip);
    *output_depth = KAN_MAX (1u, description->depth >> mip);
}

#define DEBUG_LABEL_COLOR_PASS 1.0f, 0.796f, 0.0f, 1.0f

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
#    define DEBUG_LABEL_SCOPE_BEGIN(BUFFER, LABEL, ...)                                                                \
        {                                                                                                              \
            struct VkDebugUtilsLabelEXT label = {                                                                      \
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,                                                      \
                .pNext = NULL,                                                                                         \
                .pLabelName = LABEL,                                                                                   \
                .color = {__VA_ARGS__},                                                                                \
            };                                                                                                         \
                                                                                                                       \
            vkCmdBeginDebugUtilsLabelEXT (BUFFER, &label);                                                             \
        }

#    define DEBUG_LABEL_SCOPE_END(BUFFER) vkCmdEndDebugUtilsLabelEXT (BUFFER);

#    define DEBUG_LABEL_INSERT(BUFFER, LABEL, ...)                                                                     \
        {                                                                                                              \
            struct VkDebugUtilsLabelEXT label = {                                                                      \
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,                                                      \
                .pNext = NULL,                                                                                         \
                .pLabelName = LABEL,                                                                                   \
                .color = {__VA_ARGS__},                                                                                \
            };                                                                                                         \
                                                                                                                       \
            vkCmdInsertDebugUtilsLabelEXT (BUFFER, &label);                                                            \
        }
#else
#    define DEBUG_LABEL_SCOPE_BEGIN(BUFFER, LABEL, ...) /* Disabled. */
#    define DEBUG_LABEL_SCOPE_END(BUFFER)               /* Disabled. */
#    define DEBUG_LABEL_INSERT(BUFFER, LABEL, ...)      /* Disabled. */
#endif

KAN_C_HEADER_END
