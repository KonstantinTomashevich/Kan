#include <string.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/memory/allocation.h>
#include <kan/platform/application.h>
#include <kan/render_backend/render_backend.h>
#include <kan/render_backend/vulkan_memory_allocator.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (render_backend_vulkan);

#if defined(KAN_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
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

struct render_context_t
{
    VkInstance vulkan_instance;
    VkDevice vulkan_device;
    VkQueue graphics_queue;
    VkQueue transfer_queue;

    VmaAllocator vulkan_memory_allocator;

    uint32_t device_graphics_queue_family_index;
    uint32_t device_transfer_queue_family_index;

    VkFormat device_depth_image_format;
    kan_bool_t device_depth_image_has_stencil;

#if defined(KAN_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
    kan_bool_t has_validation_layer;
    VkDebugUtilsMessengerEXT vulkan_debug_messenger;
#endif

    VmaVulkanFunctions vulkan_memory_allocator_functions;

    kan_interned_string_t tracking_name;
    kan_allocation_group_t main_allocation_group;

#if defined(KAN_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    struct memory_profiling_t memory_profiling;
#endif
};

static struct kan_atomic_int_t statics_initialization_lock = {.value = 0};
static kan_bool_t statics_initialized = KAN_FALSE;
static kan_allocation_group_t render_backend_allocation_group;
static kan_allocation_group_t render_backend_utility_allocation_group;
static kan_allocation_group_t render_backend_context_allocation_group;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        kan_atomic_int_lock (&statics_initialization_lock);
        if (!statics_initialized)
        {
            volkInitializeCustom (
                (PFN_vkGetInstanceProcAddr) kan_platform_application_request_vulkan_resolve_function ());

            render_backend_allocation_group =
                kan_allocation_group_get_child (kan_allocation_group_root (), "render_backend");
            render_backend_utility_allocation_group =
                kan_allocation_group_get_child (render_backend_allocation_group, "utility");
            render_backend_context_allocation_group =
                kan_allocation_group_get_child (render_backend_allocation_group, "context");

            statics_initialized = KAN_TRUE;
        }

        kan_atomic_int_unlock (&statics_initialization_lock);
    }
}

#if defined(KAN_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
#    define VULKAN_ALLOCATION_CALLBACKS(CONTEXT) (&(CONTEXT)->memory_profiling.vulkan_allocation_callbacks)

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

static void memory_profiling_init (struct render_context_t *context)
{
    context->memory_profiling.driver_cpu_group =
        kan_allocation_group_get_child (context->main_allocation_group, "driver_cpu");
    context->memory_profiling.driver_cpu_generic_group =
        kan_allocation_group_get_child (context->memory_profiling.driver_cpu_group, "generic");
    context->memory_profiling.driver_cpu_internal_group =
        kan_allocation_group_get_child (context->memory_profiling.driver_cpu_group, "internal");

    context->memory_profiling.gpu_group = kan_allocation_group_get_child (context->main_allocation_group, "gpu");
    context->memory_profiling.gpu_unmarked_group =
        kan_allocation_group_get_child (context->memory_profiling.gpu_group, "unmarked");
    context->memory_profiling.gpu_buffer_group =
        kan_allocation_group_get_child (context->memory_profiling.gpu_group, "buffer");
    context->memory_profiling.gpu_buffer_attribute_group =
        kan_allocation_group_get_child (context->memory_profiling.gpu_buffer_group, "attribute");
    context->memory_profiling.gpu_buffer_index_group =
        kan_allocation_group_get_child (context->memory_profiling.gpu_buffer_group, "index");
    context->memory_profiling.gpu_buffer_uniform_group =
        kan_allocation_group_get_child (context->memory_profiling.gpu_buffer_group, "uniform");
    context->memory_profiling.gpu_buffer_storage_group =
        kan_allocation_group_get_child (context->memory_profiling.gpu_buffer_group, "storage");
    context->memory_profiling.gpu_image_group =
        kan_allocation_group_get_child (context->memory_profiling.gpu_group, "image");

    context->memory_profiling.vulkan_allocation_callbacks = (VkAllocationCallbacks) {
        .pUserData = &context->memory_profiling,
        .pfnAllocation = profiled_allocate,
        .pfnReallocation = profiled_reallocate,
        .pfnFree = profiled_free,
        .pfnInternalAllocation = notify_internal_cpu_allocation,
        .pfnInternalFree = notify_internal_cpu_free,
    };

    context->memory_profiling.vma_device_memory_callbacks = (VmaDeviceMemoryCallbacks) {
        .pfnAllocate = notify_device_allocation,
        .pfnFree = notify_device_free,
        .pUserData = &context->memory_profiling,
    };
}
#else
#    define VULKAN_ALLOCATION_CALLBACKS(CONTEXT) NULL
#endif

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

    KAN_LOG (render_backend_vulkan, verbosity, "Received message from Vulkan: %s", callback_data->pMessage)
    return VK_FALSE;
}

static struct kan_atomic_int_t has_context = {.value = 0};

kan_render_context_t kan_render_context_create (struct kan_render_context_description_t *description)
{
    if (!kan_platform_application_register_vulkan_library_usage ())
    {
        kan_critical_error ("Failed to register vulkan library usage, unable to continue properly.", __FILE__,
                            __LINE__);
    }

    ensure_statics_initialized ();
    if (!kan_atomic_int_compare_and_set (&has_context, 0, 1))
    {
        // Lack of multi-context support is mostly related to the fact that volk (loader) also doesn't support it.
        // It should not be very difficult to rewrite, but we don't need multiple render contexts right now anyway.
        kan_critical_error (
            "Current Vulkan render backend implementation does not work well with multiple contexts, unfortunately.",
            __FILE__, __LINE__);
    }

    struct kan_dynamic_array_t extensions;
    kan_platform_application_request_vulkan_extensions (&extensions, render_backend_utility_allocation_group);

#if defined(KAN_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
    {
        kan_dynamic_array_set_capacity (&extensions, extensions.size + 1u);
        char **output = kan_dynamic_array_add_last (&extensions);
        *output = kan_allocate_general (render_backend_utility_allocation_group,
                                        sizeof (VK_EXT_DEBUG_UTILS_EXTENSION_NAME), _Alignof (char));
        memcpy (*output, VK_EXT_DEBUG_UTILS_EXTENSION_NAME, sizeof (VK_EXT_DEBUG_UTILS_EXTENSION_NAME));
    }
#endif

    KAN_LOG (render_backend_vulkan, KAN_LOG_INFO, "Preparing to create Vulkan instance. Used extensions:")
    for (uint64_t index = 0u; index < extensions.size; ++index)
    {
        KAN_LOG (render_backend_vulkan, KAN_LOG_INFO, "    - %s", ((char **) extensions.data)[index])
    }

    kan_allocation_group_t context_allocation_group =
        kan_allocation_group_get_child (render_backend_context_allocation_group, description->tracking_name);
    struct render_context_t *context = kan_allocate_general (context_allocation_group, sizeof (struct render_context_t),
                                                             _Alignof (struct render_context_t));

    context->vulkan_device = VK_NULL_HANDLE;
    context->tracking_name = description->tracking_name;
    context->main_allocation_group = context_allocation_group;

#if defined(KAN_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
    context->vulkan_debug_messenger = VK_NULL_HANDLE;
#endif

#if defined(KAN_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    memory_profiling_init (context);
#endif

    VkApplicationInfo application_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = description->application_info_name,
        .applicationVersion =
            VK_MAKE_VERSION (description->version_major, description->version_minor, description->version_patch),
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

#if defined(KAN_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
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
        kan_allocate_general (render_backend_utility_allocation_group,
                              sizeof (VkLayerProperties) * layer_properties_count, _Alignof (VkLayerProperties));
    vkEnumerateInstanceLayerProperties (&layer_properties_count, layer_properties);
    context->has_validation_layer = KAN_FALSE;

    for (uint64_t index = 0u; index < layer_properties_count; ++index)
    {
        if (strcmp (layer_properties[index].layerName, "VK_LAYER_KHRONOS_validation") == 0)
        {
            context->has_validation_layer = KAN_TRUE;
            break;
        }
    }

    const char *enabled_layers[1u];
    if (context->has_validation_layer)
    {
        instance_create_info.enabledLayerCount = 1u;
        enabled_layers[0u] = "VK_LAYER_KHRONOS_validation";
        instance_create_info.ppEnabledLayerNames = enabled_layers;
        instance_create_info.pNext = &debug_messenger_create_info;
    }
    else
    {
        KAN_LOG (render_backend_vulkan, KAN_LOG_ERROR, "Unable to initialize validation layer as it is not supported.")
    }

    kan_free_general (render_backend_utility_allocation_group, layer_properties,
                      sizeof (VkLayerProperties) * layer_properties_count);
#endif

    if (vkCreateInstance (&instance_create_info, VULKAN_ALLOCATION_CALLBACKS (context), &context->vulkan_instance) !=
        VK_SUCCESS)
    {
        kan_critical_error ("Failed to create Vulkan instance.", __FILE__, __LINE__);
    }

    volkLoadInstance (context->vulkan_instance);
    for (uint64_t index = 0u; index < extensions.size; ++index)
    {
        kan_free_general (render_backend_utility_allocation_group, ((char **) extensions.data)[index],
                          strlen (((char **) extensions.data)[index]) + 1u);
    }

    kan_dynamic_array_shutdown (&extensions);

#if defined(KAN_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
    if (context->has_validation_layer)
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

        if (vkCreateDebugUtilsMessengerEXT (context->vulkan_instance, &debug_messenger_create_info,
                                            VULKAN_ALLOCATION_CALLBACKS (context),
                                            &context->vulkan_debug_messenger) != VK_SUCCESS)
        {
            KAN_LOG (render_backend_vulkan, KAN_LOG_ERROR, "Failed to create debug messenger.")
        }
    }
#endif

    return (kan_render_context_t) context;
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

struct kan_render_supported_devices_t *kan_render_context_query_devices (kan_render_context_t context)
{
    struct render_context_t *render_context = (struct render_context_t *) context;
    uint32_t physical_device_count;

    if (vkEnumeratePhysicalDevices (render_context->vulkan_instance, &physical_device_count, NULL) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_vulkan, KAN_LOG_ERROR, "Failed to query physical devices.")
        return NULL;
    }

    VkPhysicalDevice *physical_devices =
        kan_allocate_general (render_backend_utility_allocation_group,
                              sizeof (VkPhysicalDevice) * physical_device_count, _Alignof (VkPhysicalDevice));

    if (vkEnumeratePhysicalDevices (render_context->vulkan_instance, &physical_device_count, physical_devices) !=
        VK_SUCCESS)
    {
        kan_free_general (render_backend_utility_allocation_group, physical_devices,
                          sizeof (VkPhysicalDevice) * physical_device_count);

        KAN_LOG (render_backend_vulkan, KAN_LOG_ERROR, "Failed to query physical devices.")
        return NULL;
    }

    struct kan_render_supported_devices_t *supported_devices =
        kan_allocate_general (render_backend_utility_allocation_group,
                              sizeof (struct kan_render_supported_devices_t) +
                                  sizeof (struct kan_render_supported_device_info_t) * physical_device_count,
                              _Alignof (struct kan_render_supported_devices_t));
    supported_devices->supported_device_count = (uint64_t) physical_device_count;

    for (uint64_t device_index = 0u; device_index < physical_device_count; ++device_index)
    {
        struct kan_render_supported_device_info_t *device_info = &supported_devices->devices[device_index];
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

    kan_free_general (render_backend_utility_allocation_group, physical_devices,
                      sizeof (VkPhysicalDevice) * physical_device_count);
    return supported_devices;
}

void kan_render_context_free_device_query_result (kan_render_context_t context,
                                                  struct kan_render_supported_devices_t *query_result)
{
    kan_free_general (render_backend_utility_allocation_group, query_result,
                      sizeof (struct kan_render_supported_devices_t) +
                          sizeof (struct kan_render_supported_device_info_t) * query_result->supported_device_count);
}

kan_bool_t kan_render_context_select_device (kan_render_context_t context, kan_render_device_id_t device)
{
    struct render_context_t *render_context = (struct render_context_t *) context;
    VkPhysicalDevice physical_device = (VkPhysicalDevice) device;

    if (render_context->vulkan_device)
    {
        KAN_LOG (render_backend_vulkan, KAN_LOG_ERROR,
                 "Caught attempt to set device for context \"%s\" which already has selected device.",
                 render_context->tracking_name)
        return KAN_FALSE;
    }

    uint32_t properties_count;
    if (vkEnumerateDeviceExtensionProperties (physical_device, NULL, &properties_count, NULL) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_vulkan, KAN_LOG_ERROR, "Unable to read physical device properties for context \"%s\".",
                 render_context->tracking_name)
        return KAN_FALSE;
    }

    VkExtensionProperties *properties =
        kan_allocate_general (render_backend_utility_allocation_group,
                              sizeof (VkExtensionProperties) * properties_count, _Alignof (VkExtensionProperties));

    if (vkEnumerateDeviceExtensionProperties (physical_device, NULL, &properties_count, properties) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_vulkan, KAN_LOG_ERROR, "Unable to read physical device properties for context \"%s\".",
                 render_context->tracking_name)
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

    kan_free_general (render_backend_utility_allocation_group, properties,
                      sizeof (VkExtensionProperties) * properties_count);

    if (!swap_chain_found)
    {
        KAN_LOG (render_backend_vulkan, KAN_LOG_ERROR,
                 "Unable to select device for context \"%s\": requested device has no swap chain.",
                 render_context->tracking_name)
        return KAN_FALSE;
    }

    uint32_t queues_count;
    vkGetPhysicalDeviceQueueFamilyProperties (physical_device, &queues_count, NULL);
    VkQueueFamilyProperties *queues =
        kan_allocate_general (render_backend_utility_allocation_group, sizeof (VkQueueFamilyProperties) * queues_count,
                              _Alignof (VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties (physical_device, &queues_count, queues);

    render_context->device_graphics_queue_family_index = UINT32_MAX;
    render_context->device_transfer_queue_family_index = UINT32_MAX;

    for (uint32_t index = 0u; index < queues_count; ++index)
    {
        if (render_context->device_graphics_queue_family_index == UINT32_MAX &&
            (queues[index].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            render_context->device_graphics_queue_family_index = index;
        }

        if (render_context->device_transfer_queue_family_index == UINT32_MAX &&
            (queues[index].queueFlags & VK_QUEUE_TRANSFER_BIT))
        {
            render_context->device_transfer_queue_family_index = index;
        }
    }

    kan_free_general (render_backend_utility_allocation_group, queues, sizeof (VkQueueFamilyProperties) * queues_count);
    if (render_context->device_graphics_queue_family_index == UINT32_MAX)
    {
        KAN_LOG (render_backend_vulkan, KAN_LOG_ERROR,
                 "Unable to select device for context \"%s\": requested device has no graphics family.",
                 render_context->tracking_name)
        return KAN_FALSE;
    }

    if (render_context->device_transfer_queue_family_index == UINT32_MAX)
    {
        KAN_LOG (render_backend_vulkan, KAN_LOG_ERROR,
                 "Unable to select device for context \"%s\": requested device has no transfer family.",
                 render_context->tracking_name)
        return KAN_FALSE;
    }

    static VkFormat depth_formats[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    render_context->device_depth_image_format = VK_FORMAT_UNDEFINED;

    for (uint64_t index = 0u; index < sizeof (depth_formats) / sizeof (depth_formats[0u]); ++index)
    {
        VkFormatProperties format_properties;
        vkGetPhysicalDeviceFormatProperties (physical_device, depth_formats[index], &format_properties);

        if (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            render_context->device_depth_image_format = depth_formats[index];
            break;
        }
    }

    render_context->device_depth_image_has_stencil =
        render_context->device_depth_image_format == VK_FORMAT_D32_SFLOAT ||
        render_context->device_depth_image_format == VK_FORMAT_D24_UNORM_S8_UINT;

    if (render_context->device_depth_image_format == VK_FORMAT_UNDEFINED)
    {
        KAN_LOG (render_backend_vulkan, KAN_LOG_ERROR,
                 "Unable to select device for context \"%s\": unable to select depth format for device.",
                 render_context->tracking_name)
        return KAN_FALSE;
    }

    float queues_priorities = 0u;
    VkDeviceQueueCreateInfo queues_create_info[] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = NULL,
            .queueFamilyIndex = render_context->device_graphics_queue_family_index,
            .queueCount = 1u,
            .pQueuePriorities = &queues_priorities,
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = NULL,
            .queueFamilyIndex = render_context->device_transfer_queue_family_index,
            .queueCount = 1u,
            .pQueuePriorities = &queues_priorities,
        },
    };

    uint32_t queues_to_create_count =
        render_context->device_graphics_queue_family_index == render_context->device_transfer_queue_family_index ? 1u :
                                                                                                                   2u;

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

#if defined(KAN_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
    const char *enabled_layers[] = {"VK_LAYER_KHRONOS_validation"};
    if (render_context->has_validation_layer)
    {
        device_create_info.enabledLayerCount = sizeof (enabled_layers) / sizeof (enabled_layers[0u]);
        device_create_info.ppEnabledLayerNames = enabled_layers;
    }
#endif

    if (vkCreateDevice (physical_device, &device_create_info, VULKAN_ALLOCATION_CALLBACKS (render_context),
                        &render_context->vulkan_device) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_vulkan, KAN_LOG_ERROR,
                 "Unable to select device for context \"%s\": failed to create logical device.",
                 render_context->tracking_name)
        return KAN_FALSE;
    }

    volkLoadDevice (render_context->vulkan_device);
    vkGetDeviceQueue (render_context->vulkan_device, render_context->device_graphics_queue_family_index, 0u,
                      &render_context->graphics_queue);
    vkGetDeviceQueue (render_context->vulkan_device, render_context->device_transfer_queue_family_index, 0u,
                      &render_context->transfer_queue);

    render_context->vulkan_memory_allocator_functions = (VmaVulkanFunctions) {
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
        .device = render_context->vulkan_device,
        .preferredLargeHeapBlockSize = 0u, // Use default.
#if defined(KAN_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
        .pAllocationCallbacks = &render_context->memory_profiling.vulkan_allocation_callbacks,
        .pDeviceMemoryCallbacks = &render_context->memory_profiling.vma_device_memory_callbacks,
#else
        .pAllocationCallbacks = NULL,
        .pDeviceMemoryCallbacks = NULL,
#endif
        .pHeapSizeLimit = NULL,
        .pVulkanFunctions = &render_context->vulkan_memory_allocator_functions,
        .instance = render_context->vulkan_instance,
        .pTypeExternalMemoryHandleTypes = NULL,
        .vulkanApiVersion = VK_MAKE_VERSION (1u, 1u, 0u),
    };

    if (vmaCreateAllocator (&allocator_create_info, &render_context->vulkan_memory_allocator) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_vulkan, KAN_LOG_ERROR,
                 "Unable to select device for context \"%s\": failed to create vulkan memory allocator.",
                 render_context->tracking_name)

        vkDestroyDevice (render_context->vulkan_device, VULKAN_ALLOCATION_CALLBACKS (render_context));
        render_context->vulkan_device = VK_NULL_HANDLE;
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

kan_bool_t kan_render_context_next_frame (kan_render_context_t context)
{
    // TODO: Implement.
    return KAN_TRUE;
}

void kan_render_context_destroy (kan_render_context_t context)
{
    struct render_context_t *render_context = (struct render_context_t *) context;
    if (render_context->vulkan_device != VK_NULL_HANDLE)
    {
        vmaDestroyAllocator (render_context->vulkan_memory_allocator);
        vkDestroyDevice (render_context->vulkan_device, VULKAN_ALLOCATION_CALLBACKS (render_context));
    }

#if defined(KAN_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
    if (render_context->vulkan_debug_messenger != VK_NULL_HANDLE)
    {
        vkDestroyDebugUtilsMessengerEXT (render_context->vulkan_instance, render_context->vulkan_debug_messenger,
                                         VULKAN_ALLOCATION_CALLBACKS (render_context));
    }
#endif

    vkDestroyInstance (render_context->vulkan_instance, VULKAN_ALLOCATION_CALLBACKS (render_context));
    kan_atomic_int_compare_and_set (&has_context, 1, 0);
    kan_platform_application_unregister_vulkan_library_usage ();
}

enum kan_platform_window_flag_t kan_render_get_required_window_flags (void)
{
    return KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_VULKAN;
}
