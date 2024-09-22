#include <string.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/context/render_backend_system.h>
#include <kan/context/vulkan_memory_allocator.h>
#include <kan/memory/allocation.h>
#include <kan/platform/application.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (render_backend_system_vulkan);

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

    kan_bool_t received_any_output;

    // TODO: Attached frame buffers.

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

struct render_backend_system_t
{
    kan_context_handle_t context;

    VkInstance instance;
    VkDevice device;
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

    VkFormat device_depth_image_format;
    kan_bool_t device_depth_image_has_stencil;

    struct render_backend_surface_t *first_surface;

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
    kan_bool_t has_validation_layer;
    VkDebugUtilsMessengerEXT debug_messenger;
#endif

    VmaVulkanFunctions gpu_memory_allocator_functions;
    kan_allocation_group_t main_allocation_group;
    kan_allocation_group_t utility_allocation_group;
    kan_allocation_group_t surface_allocation_group;

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
    const uint64_t original_size = *(uint64_t *) original_allocated_data;

    void *new_data = profiled_allocate (user_data, size, alignment, scope);
    memcpy (new_data, original, KAN_MIN ((size_t) original_size, size));
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
#else
#    define VULKAN_ALLOCATION_CALLBACKS(SYSTEM) NULL
#endif

kan_context_system_handle_t render_backend_system_create (kan_allocation_group_t group, void *user_config)
{
    struct render_backend_system_t *system = kan_allocate_general (group, sizeof (struct render_backend_system_t),
                                                                   _Alignof (struct render_backend_system_t));

    system->instance = VK_NULL_HANDLE;
    system->device = VK_NULL_HANDLE;
    system->physical_device = VK_NULL_HANDLE;

    system->main_allocation_group = group;
    system->utility_allocation_group = kan_allocation_group_get_child (group, "utility");
    system->surface_allocation_group = kan_allocation_group_get_child (group, "surface");

    system->frame_started = KAN_FALSE;
    system->current_frame_in_flight_index = 0u;

    system->first_surface = NULL;
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

    if (system->device != VK_NULL_HANDLE)
    {
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

    system->device_depth_image_has_stencil = system->device_depth_image_format == VK_FORMAT_D32_SFLOAT ||
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
        .flags = 0u,
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

    // TODO: Fill buffer with accumulated transfer commands.

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
        surface->received_any_output = KAN_FALSE;
        surface = surface->next;
    }

    // TODO: Fill buffer with accumulated graphics commands.

    surface = system->first_surface;
    while (surface)
    {
        if (!surface->received_any_output)
        {
            // Special catch for cases when there is no render commands: we still need to transition surface images.
            VkImageMemoryBarrier barrier_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = 0u,
                .dstAccessMask = 0u,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
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
                                  VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &barrier_info);
        }

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

    kan_free_general (surface->system->surface_allocation_group, surface->images,
                      sizeof (VkImage) * surface->images_count);
    kan_free_general (surface->system->surface_allocation_group, surface->image_views,
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

    surface->images = kan_allocate_general (surface->system->surface_allocation_group,
                                            sizeof (VkImage) * surface->images_count, _Alignof (VkImage));

    if (vkGetSwapchainImagesKHR (surface->system->device, surface->swap_chain, &surface->images_count,
                                 surface->images) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain image views for surface \"%s\": failed to query images.",
                 surface->tracking_name)

        kan_free_general (surface->system->surface_allocation_group, surface->images,
                          sizeof (VkImage) * surface->images_count);
        return KAN_FALSE;
    }

    surface->image_views = kan_allocate_general (surface->system->surface_allocation_group,
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
        if (formats[index].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
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

    surface->swap_chain_creation_window_width = window_info->width_for_render;
    surface->swap_chain_creation_window_height = window_info->height_for_render;

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

    // TODO: Init dependant frame buffers.
}

static void render_backend_surface_destroy_swap_chain (struct render_backend_surface_t *surface)
{
    if (surface->swap_chain == VK_NULL_HANDLE)
    {
        return;
    }

    // TODO: Destroy dependant frame buffers.

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

    // TODO: Clear leftovers from previous frame that executed on this index.
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

    kan_free_batched (surface->system->surface_allocation_group, surface);
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

    struct render_backend_surface_t *new_surface =
        kan_allocate_batched (system->surface_allocation_group, sizeof (struct render_backend_surface_t));

    new_surface->system = system;
    new_surface->tracking_name = tracking_name;
    new_surface->surface = VK_NULL_HANDLE;
    new_surface->swap_chain = VK_NULL_HANDLE;
    new_surface->window_handle = window;

    new_surface->previous = NULL;
    new_surface->next = system->first_surface;

    if (system->first_surface)
    {
        system->first_surface->previous = new_surface;
    }

    system->first_surface = new_surface;
    new_surface->resource_id =
        kan_application_system_window_add_resource (application_system, window,
                                                    (struct kan_application_system_window_resource_binding_t) {
                                                        .user_data = new_surface,
                                                        .init = render_backend_surface_init_with_window,
                                                        .shutdown = render_backend_surface_shutdown_with_window,
                                                    });

    return (kan_render_surface_t) new_surface;
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
