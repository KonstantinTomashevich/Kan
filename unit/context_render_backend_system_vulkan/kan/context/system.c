#include <kan/context/render_backend_implementation_interface.h>

KAN_LOG_DEFINE_CATEGORY (render_backend_system_vulkan);

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
    system->pass_instance_allocation_group = kan_allocation_group_get_child (group, "pass_instance");
    system->pipeline_family_wrapper_allocation_group =
        kan_allocation_group_get_child (group, "pipeline_family_wrapper");
    system->code_module_wrapper_allocation_group = kan_allocation_group_get_child (group, "code_module_wrapper");
    system->pipeline_wrapper_allocation_group = kan_allocation_group_get_child (group, "pipeline_wrapper");
    system->pipeline_parameter_set_wrapper_allocation_group =
        kan_allocation_group_get_child (group, "pipeline_parameter_set_wrapper");
    system->buffer_wrapper_allocation_group = kan_allocation_group_get_child (group, "buffer_wrapper");
    system->frame_lifetime_wrapper_allocation_group =
        kan_allocation_group_get_child (group, "frame_lifetime_allocator_wrapper");
    system->image_wrapper_allocation_group = kan_allocation_group_get_child (group, "descriptor_set_wrapper");
    system->descriptor_set_wrapper_allocation_group = kan_allocation_group_get_child (group, "descriptor_set_wrapper");

    system->frame_started = KAN_FALSE;
    system->current_frame_in_flight_index = 0u;

    system->resource_management_lock = kan_atomic_int_init (0);
    system->pass_static_dependency_lock = kan_atomic_int_init (0);
    system->pass_instance_state_management_lock = kan_atomic_int_init (0);

    kan_bd_list_init (&system->surfaces);
    kan_bd_list_init (&system->frame_buffers);
    kan_bd_list_init (&system->passes);
    kan_bd_list_init (&system->pass_instances);
    kan_bd_list_init (&system->pass_instances_available);
    kan_bd_list_init (&system->graphics_pipeline_families);
    kan_bd_list_init (&system->code_modules);
    kan_bd_list_init (&system->graphics_pipelines);
    kan_bd_list_init (&system->pipeline_parameter_sets);
    kan_bd_list_init (&system->buffers);
    kan_bd_list_init (&system->frame_lifetime_allocators);
    kan_bd_list_init (&system->images);

    system->staging_frame_lifetime_allocator = NULL;
    system->supported_devices = NULL;

    system->compiler_state.state_transition_mutex = kan_mutex_create ();
    system->compiler_state.has_more_work = kan_conditional_variable_create ();
    system->compiler_state.should_terminate = kan_atomic_int_init (0);

    kan_bd_list_init (&system->compiler_state.graphics_critical);
    kan_bd_list_init (&system->compiler_state.graphics_active);
    kan_bd_list_init (&system->compiler_state.graphics_cache);

    system->compiler_state.thread = kan_thread_create ("context_render_backend_system_vulkan_pipeline_compiler",
                                                       render_backend_pipeline_compiler_state_worker_function,
                                                       (kan_thread_user_data_t) &system->compiler_state);
    render_backend_descriptor_set_allocator_init (&system->descriptor_set_allocator);
    kan_stack_group_allocator_init (&system->pass_instance_allocator, system->pass_instance_allocation_group,
                                    KAN_CONTEXT_RENDER_BACKEND_VULKAN_PASS_STACK_SIZE);

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

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
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
#endif

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

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
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

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    system->debug_messenger = VK_NULL_HANDLE;
#endif

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    render_backend_memory_profiling_init (system);
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

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
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

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
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
        state->primary_command_buffer = VK_NULL_HANDLE;
        if (state->command_pool != VK_NULL_HANDLE)
        {
            vkResetCommandPool (system->device, state->command_pool, 0u);
            if (state->secondary_command_buffers.size > 0u)
            {
                vkFreeCommandBuffers (system->device, state->command_pool,
                                      (uint32_t) state->secondary_command_buffers.size,
                                      (VkCommandBuffer *) state->secondary_command_buffers.data);
            }

            vkDestroyCommandPool (system->device, state->command_pool, VULKAN_ALLOCATION_CALLBACKS (system));
        }

        kan_dynamic_array_shutdown (&state->secondary_command_buffers);
    }
}

static void render_backend_system_destroy_synchronization_objects (struct render_backend_system_t *system)
{
    for (uint64_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
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
    KAN_ASSERT (!system->surfaces.first)

    // Shutdown pipeline compiler thread.
    system->compiler_state.should_terminate = kan_atomic_int_init (1);
    kan_conditional_variable_signal_one (system->compiler_state.has_more_work);
    kan_thread_wait (system->compiler_state.thread);

    kan_mutex_destroy (system->compiler_state.state_transition_mutex);
    kan_conditional_variable_destroy (system->compiler_state.has_more_work);

#define DESTROY_GRAPHICS_PIPELINE_REQUESTS                                                                             \
    while (pipeline_request)                                                                                           \
    {                                                                                                                  \
        struct graphics_pipeline_compilation_request_t *next =                                                         \
            (struct graphics_pipeline_compilation_request_t *) pipeline_request->list_node.next;                       \
        pipeline_request->pipeline->compilation_request = NULL;                                                        \
        render_backend_compiler_state_destroy_graphics_request (pipeline_request);                                     \
        pipeline_request = next;                                                                                       \
    }

    struct graphics_pipeline_compilation_request_t *pipeline_request =
        (struct graphics_pipeline_compilation_request_t *) system->compiler_state.graphics_critical.first;
    DESTROY_GRAPHICS_PIPELINE_REQUESTS

    pipeline_request = (struct graphics_pipeline_compilation_request_t *) system->compiler_state.graphics_active.first;
    DESTROY_GRAPHICS_PIPELINE_REQUESTS

    pipeline_request = (struct graphics_pipeline_compilation_request_t *) system->compiler_state.graphics_cache.first;
    DESTROY_GRAPHICS_PIPELINE_REQUESTS
#undef DESTROY_GRAPHICS_PIPELINE_REQUESTS

    struct render_backend_frame_buffer_t *frame_buffer =
        (struct render_backend_frame_buffer_t *) system->frame_buffers.first;

    while (frame_buffer)
    {
        struct render_backend_frame_buffer_t *next =
            (struct render_backend_frame_buffer_t *) frame_buffer->list_node.next;
        render_backend_system_destroy_frame_buffer (system, frame_buffer);
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

        // Remark. We actually do not care about detached descriptor sets here:
        // they'll be destroyed anyway when descriptor set pools are destroyed.

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

        // We need to unmap scheduled uploads.
        struct scheduled_buffer_unmap_flush_transfer_t *buffer_unmap_flush_transfer =
            schedule->first_scheduled_buffer_unmap_flush_transfer;
        schedule->first_scheduled_buffer_unmap_flush_transfer = NULL;

        while (buffer_unmap_flush_transfer)
        {
            vmaUnmapMemory (system->gpu_memory_allocator, buffer_unmap_flush_transfer->source_buffer->allocation);
            buffer_unmap_flush_transfer = buffer_unmap_flush_transfer->next;
        }

        struct scheduled_buffer_unmap_flush_t *buffer_unmap_flush = schedule->first_scheduled_buffer_unmap_flush;
        schedule->first_scheduled_buffer_unmap_flush = NULL;

        while (buffer_unmap_flush)
        {
            vmaUnmapMemory (system->gpu_memory_allocator, buffer_unmap_flush->buffer->allocation);
            buffer_unmap_flush = buffer_unmap_flush->next;
        }
    }

    struct render_backend_pipeline_parameter_set_t *parameter_set =
        (struct render_backend_pipeline_parameter_set_t *) system->pipeline_parameter_sets.first;

    while (parameter_set)
    {
        struct render_backend_pipeline_parameter_set_t *next =
            (struct render_backend_pipeline_parameter_set_t *) parameter_set->list_node.next;
        render_backend_system_destroy_pipeline_parameter_set (system, parameter_set);
        parameter_set = next;
    }

    render_backend_descriptor_set_allocator_shutdown (system, &system->descriptor_set_allocator);
    struct render_backend_graphics_pipeline_t *pipeline =
        (struct render_backend_graphics_pipeline_t *) system->graphics_pipelines.first;

    while (pipeline)
    {
        struct render_backend_graphics_pipeline_t *next =
            (struct render_backend_graphics_pipeline_t *) pipeline->list_node.next;
        render_backend_system_destroy_graphics_pipeline (system, pipeline);
        pipeline = next;
    }

    struct render_backend_code_module_t *code_module =
        (struct render_backend_code_module_t *) system->code_modules.first;

    while (code_module)
    {
        struct render_backend_code_module_t *next = (struct render_backend_code_module_t *) code_module->list_node.next;
        render_backend_system_destroy_code_module (system, code_module);
        code_module = next;
    }

    struct render_backend_graphics_pipeline_family_t *family =
        (struct render_backend_graphics_pipeline_family_t *) system->graphics_pipeline_families.first;

    while (family)
    {
        struct render_backend_graphics_pipeline_family_t *next =
            (struct render_backend_graphics_pipeline_family_t *) family->list_node.next;
        render_backend_system_destroy_graphics_pipeline_family (system, family);
        family = next;
    }

    // Pass instances should always be allocated on special stack allocator,
    // therefore we do not care about them at all here.

    struct render_backend_pass_t *pass = (struct render_backend_pass_t *) system->passes.first;
    while (pass)
    {
        struct render_backend_pass_t *next = (struct render_backend_pass_t *) pass->list_node.next;
        render_backend_system_destroy_pass (system, pass);
        pass = next;
    }

    struct render_backend_buffer_t *buffer = (struct render_backend_buffer_t *) system->buffers.first;
    while (buffer)
    {
        struct render_backend_buffer_t *next = (struct render_backend_buffer_t *) buffer->list_node.next;
        render_backend_system_destroy_buffer (system, buffer);
        buffer = next;
    }

    struct render_backend_frame_lifetime_allocator_t *frame_lifetime_allocator =
        (struct render_backend_frame_lifetime_allocator_t *) system->frame_lifetime_allocators.first;

    while (frame_lifetime_allocator)
    {
        struct render_backend_frame_lifetime_allocator_t *next =
            (struct render_backend_frame_lifetime_allocator_t *) frame_lifetime_allocator->list_node.next;
        render_backend_system_destroy_frame_lifetime_allocator (system, frame_lifetime_allocator, KAN_FALSE);
        frame_lifetime_allocator = next;
    }

    struct render_backend_image_t *image = (struct render_backend_image_t *) system->images.first;
    while (image)
    {
        struct render_backend_image_t *next = (struct render_backend_image_t *) image->list_node.next;
        render_backend_system_destroy_image (system, image);
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

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    if (system->debug_messenger != VK_NULL_HANDLE)
    {
        vkDestroyDebugUtilsMessengerEXT (system->instance, system->debug_messenger,
                                         VULKAN_ALLOCATION_CALLBACKS (system));
    }
#endif

    vkDestroyInstance (system->instance, VULKAN_ALLOCATION_CALLBACKS (system));
    kan_stack_group_allocator_shutdown (&system->pass_instance_allocator);
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
    .name = KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME,
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

    system->device_queue_family_index = UINT32_MAX;
    for (uint32_t index = 0u; index < queues_count; ++index)
    {
        const uint32_t mask = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT;
        if (mask == (queues[index].queueFlags & mask))
        {
            system->device_queue_family_index = index;
            break;
        }
    }

    kan_free_general (system->utility_allocation_group, queues, sizeof (VkQueueFamilyProperties) * queues_count);
    if (system->device_queue_family_index == UINT32_MAX)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to select device: requested device has no combined graphics and transfer queue family.")
        return KAN_FALSE;
    }

    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties (physical_device, DEPTH_FORMAT, &format_properties);

    if ((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0u)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to select device: preferred depth format is not optimal.")
        return KAN_FALSE;
    }

    vkGetPhysicalDeviceFormatProperties (physical_device, STENCIL_FORMAT, &format_properties);
    if ((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0u)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to select device: preferred stencil format is not optimal.")
        return KAN_FALSE;
    }

    vkGetPhysicalDeviceFormatProperties (physical_device, DEPTH_STENCIL_FORMAT, &format_properties);
    if ((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0u)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to select device: preferred depth stencil format is not optimal.")
        return KAN_FALSE;
    }

    float queues_priorities = 0u;
    VkDeviceQueueCreateInfo queues_create_info[] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = NULL,
            .queueFamilyIndex = system->device_queue_family_index,
            .queueCount = 1u,
            .pQueuePriorities = &queues_priorities,
        },
    };

    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures (physical_device, &device_features);
    const char *enabled_extensions_names[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .queueCreateInfoCount = 1u,
        .pQueueCreateInfos = queues_create_info,
        .enabledLayerCount = 0u,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = sizeof (enabled_extensions_names) / sizeof (enabled_extensions_names[0u]),
        .ppEnabledExtensionNames = enabled_extensions_names,
        .pEnabledFeatures = &device_features,
    };

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
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
    vkGetDeviceQueue (system->device, system->device_queue_family_index, 0u, &system->device_queue);

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
        system->render_finished_semaphores[index] = VK_NULL_HANDLE;
        system->in_flight_fences[index] = VK_NULL_HANDLE;
    }

    kan_bool_t synchronization_objects_created = KAN_TRUE;
    for (uint64_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        if (vkCreateSemaphore (system->device, &semaphore_creation_info, VULKAN_ALLOCATION_CALLBACKS (system),
                               &system->render_finished_semaphores[index]) != VK_SUCCESS)
        {
            system->render_finished_semaphores[index] = VK_NULL_HANDLE;
            synchronization_objects_created = KAN_FALSE;
            break;
        }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
        {
            char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
            snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "render_finished_semaphore_%lu",
                      (unsigned long) index);

            struct VkDebugUtilsObjectNameInfoEXT object_name = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext = NULL,
                .objectType = VK_OBJECT_TYPE_SEMAPHORE,
                .objectHandle = (uint64_t) system->render_finished_semaphores[index],
                .pObjectName = debug_name,
            };

            vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
        }
#endif

        if (vkCreateFence (system->device, &fence_creation_info, VULKAN_ALLOCATION_CALLBACKS (system),
                           &system->in_flight_fences[index]) != VK_SUCCESS)
        {
            system->in_flight_fences[index] = VK_NULL_HANDLE;
            synchronization_objects_created = KAN_FALSE;
            break;
        }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
        {
            char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
            snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "in_flight_%lu",
                      (unsigned long) index);

            struct VkDebugUtilsObjectNameInfoEXT object_name = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext = NULL,
                .objectType = VK_OBJECT_TYPE_FENCE,
                .objectHandle = (uint64_t) system->in_flight_fences[index],
                .pObjectName = debug_name,
            };

            vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
        }
#endif
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
        system->command_states[index].command_pool = VK_NULL_HANDLE;
    }

    kan_bool_t command_states_created = KAN_TRUE;
    for (uint64_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        VkCommandPoolCreateInfo graphics_command_pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .flags = 0u,
            .queueFamilyIndex = system->device_queue_family_index,
        };

        if (vkCreateCommandPool (system->device, &graphics_command_pool_info, VULKAN_ALLOCATION_CALLBACKS (system),
                                 &system->command_states[index].command_pool))
        {
            system->command_states[index].command_pool = VK_NULL_HANDLE;
            command_states_created = KAN_FALSE;
            break;
        }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
        {
            char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
            snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "graphics_command_pool_%lu",
                      (unsigned long) index);

            struct VkDebugUtilsObjectNameInfoEXT object_name = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext = NULL,
                .objectType = VK_OBJECT_TYPE_COMMAND_POOL,
                .objectHandle = (uint64_t) system->command_states[index].command_pool,
                .pObjectName = debug_name,
            };

            vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
        }
#endif

        VkCommandBufferAllocateInfo graphics_primary_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = system->command_states[index].command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1u,
        };

        if (vkAllocateCommandBuffers (system->device, &graphics_primary_buffer_info,
                                      &system->command_states[index].primary_command_buffer))
        {
            command_states_created = KAN_FALSE;
            break;
        }

        system->command_states[index].command_operation_lock = kan_atomic_int_init (0);
        kan_dynamic_array_init (&system->command_states[index].secondary_command_buffers,
                                KAN_CONTEXT_RENDER_BACKEND_VULKAN_GCB_ARRAY_SIZE, sizeof (VkCommandBuffer),
                                _Alignof (VkCommandBuffer), system->pass_instance_allocation_group);
        system->command_states[index].secondary_command_buffers_used = 0u;
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
                                        KAN_CONTEXT_RENDER_BACKEND_VULKAN_SCHEDULE_STACK_SIZE);
        state->schedule_lock = kan_atomic_int_init (0);

        state->first_scheduled_buffer_unmap_flush_transfer = NULL;
        state->first_scheduled_buffer_unmap_flush = NULL;
        state->first_scheduled_image_upload = NULL;
        state->first_scheduled_frame_buffer_create = NULL;
        state->first_scheduled_image_mip_generation = NULL;

        state->first_scheduled_frame_buffer_destroy = NULL;
        state->first_scheduled_detached_frame_buffer_destroy = NULL;
        state->first_scheduled_pass_destroy = NULL;
        state->first_scheduled_pipeline_parameter_set_destroy = NULL;
        state->first_scheduled_detached_descriptor_set_destroy = NULL;
        state->first_scheduled_code_module_destroy = NULL;
        state->first_scheduled_graphics_pipeline_destroy = NULL;
        state->first_scheduled_graphics_pipeline_family_destroy = NULL;
        state->first_scheduled_buffer_destroy = NULL;
        state->first_scheduled_frame_lifetime_allocator_destroy = NULL;
        state->first_scheduled_detached_image_view_destroy = NULL;
        state->first_scheduled_image_destroy = NULL;
        state->first_scheduled_detached_image_destroy = NULL;
    }

    system->staging_frame_lifetime_allocator = render_backend_system_create_frame_lifetime_allocator (
        system, RENDER_BACKEND_BUFFER_FAMILY_STAGING,
        // Buffer type does not matter anything for staging.
        KAN_RENDER_BUFFER_TYPE_STORAGE, KAN_CONTEXT_RENDER_BACKEND_VULKAN_STAGING_PAGE_SIZE,
        kan_string_intern ("default_staging_buffer"));
    return KAN_TRUE;
}

kan_render_context_t kan_render_backend_system_get_render_context (kan_context_system_handle_t render_backend_system)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) render_backend_system;
    return system->render_enabled ? (kan_render_context_t) system : KAN_INVALID_RENDER_CONTEXT;
}

static void render_backend_system_begin_command_submission (struct render_backend_system_t *system)
{
    VkCommandBufferBeginInfo buffer_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = 0u,
        .pInheritanceInfo = NULL,
    };

    if (vkBeginCommandBuffer (system->command_states[system->current_frame_in_flight_index].primary_command_buffer,
                              &buffer_begin_info) != VK_SUCCESS)
    {
        kan_critical_error ("Failed to start recording primary buffer.", __FILE__, __LINE__);
    }
}

static void render_backend_system_submit_transfer (struct render_backend_system_t *system)
{
    struct render_backend_command_state_t *state = &system->command_states[system->current_frame_in_flight_index];
    DEBUG_LABEL_SCOPE_BEGIN (state->primary_command_buffer, "buffer_transfer", DEBUG_LABEL_COLOR_PASS)

    struct render_backend_schedule_state_t *schedule = &system->schedule_states[system->current_frame_in_flight_index];
    struct scheduled_buffer_unmap_flush_transfer_t *buffer_unmap_flush_transfer =
        schedule->first_scheduled_buffer_unmap_flush_transfer;
    schedule->first_scheduled_buffer_unmap_flush_transfer = NULL;

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

        vkCmdCopyBuffer (state->primary_command_buffer, buffer_unmap_flush_transfer->source_buffer->buffer,
                         buffer_unmap_flush_transfer->target_buffer->buffer, 1u, &region);

        VkAccessFlags destination_access_flags = 0u;
        VkPipelineStageFlags destination_stage;

        switch (buffer_unmap_flush_transfer->target_buffer->type)
        {
        case KAN_RENDER_BUFFER_TYPE_ATTRIBUTE:
            destination_access_flags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            destination_stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            break;

        case KAN_RENDER_BUFFER_TYPE_INDEX_16:
        case KAN_RENDER_BUFFER_TYPE_INDEX_32:
            destination_access_flags |= VK_ACCESS_INDEX_READ_BIT;
            destination_stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            break;

        case KAN_RENDER_BUFFER_TYPE_UNIFORM:
            destination_access_flags |= VK_ACCESS_UNIFORM_READ_BIT;
            destination_stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;

        case KAN_RENDER_BUFFER_TYPE_STORAGE:
            destination_access_flags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            destination_stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        }

        VkBufferMemoryBarrier memory_barrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = destination_access_flags,
            .srcQueueFamilyIndex = system->device_queue_family_index,
            .dstQueueFamilyIndex = system->device_queue_family_index,
            .buffer = buffer_unmap_flush_transfer->target_buffer->buffer,
            .offset = region.dstOffset,
            .size = region.size,
        };

        vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, destination_stage, 0u, 0u,
                              NULL, 1u, &memory_barrier, 0u, NULL);
        buffer_unmap_flush_transfer = buffer_unmap_flush_transfer->next;
    }

    DEBUG_LABEL_SCOPE_END (state->primary_command_buffer)
    DEBUG_LABEL_SCOPE_BEGIN (state->primary_command_buffer, "buffer_flush", DEBUG_LABEL_COLOR_PASS)

    struct scheduled_buffer_unmap_flush_t *buffer_unmap_flush = schedule->first_scheduled_buffer_unmap_flush;
    schedule->first_scheduled_buffer_unmap_flush = NULL;

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

    DEBUG_LABEL_SCOPE_END (state->primary_command_buffer)
    DEBUG_LABEL_SCOPE_BEGIN (state->primary_command_buffer, "image_upload", DEBUG_LABEL_COLOR_PASS)

    struct scheduled_image_upload_t *image_upload = schedule->first_scheduled_image_upload;
    schedule->first_scheduled_image_upload = NULL;

    while (image_upload)
    {
        VkImageAspectFlags image_aspect =
            kan_render_image_description_calculate_aspects (&image_upload->image->description);

        uint32_t width;
        uint32_t height;
        uint32_t depth;
        kan_render_image_description_calculate_size_at_mip (&image_upload->image->description, image_upload->mip,
                                                            &width, &height, &depth);

        VkImageMemoryBarrier prepare_transfer_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = 0u,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = image_upload->image->last_command_layout,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = system->device_queue_family_index,
            .dstQueueFamilyIndex = system->device_queue_family_index,
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

        vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &prepare_transfer_barrier);
        image_upload->image->last_command_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

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

        vkCmdCopyBufferToImage (state->primary_command_buffer, image_upload->staging_buffer->buffer,
                                image_upload->image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copy_region);

        VkImageMemoryBarrier finish_transfer_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = image_upload->image->last_command_layout,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = system->device_queue_family_index,
            .dstQueueFamilyIndex = system->device_queue_family_index,
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

        vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u,
                              NULL, 1u, &finish_transfer_barrier);

        image_upload->image->last_command_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_upload = image_upload->next;
    }

    DEBUG_LABEL_SCOPE_END (state->primary_command_buffer)
}

static inline void submit_mip_generation (struct render_backend_system_t *system,
                                          struct render_backend_schedule_state_t *schedule,
                                          struct render_backend_command_state_t *state)
{
    struct scheduled_image_mip_generation_t *image_mip_generation = schedule->first_scheduled_image_mip_generation;
    while (image_mip_generation)
    {
        VkImageAspectFlags image_aspect =
            kan_render_image_description_calculate_aspects (&image_mip_generation->image->description);

        for (uint8_t output_mip = image_mip_generation->first + 1u; output_mip <= image_mip_generation->last;
             ++output_mip)
        {
            const uint8_t input_mip = output_mip - 1u;
            uint32_t input_width;
            uint32_t input_height;
            uint32_t input_depth;
            kan_render_image_description_calculate_size_at_mip (&image_mip_generation->image->description, input_mip,
                                                                &input_width, &input_height, &input_depth);

            uint32_t output_width;
            uint32_t output_height;
            uint32_t output_depth;
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
                .srcQueueFamilyIndex = system->device_queue_family_index,
                .dstQueueFamilyIndex = system->device_queue_family_index,
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

            vkCmdPipelineBarrier (
                state->primary_command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &input_to_transfer_source_barrier);

            VkImageMemoryBarrier output_to_transfer_destination_barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = 0u,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = system->device_queue_family_index,
                .dstQueueFamilyIndex = system->device_queue_family_index,
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

            vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
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

            vkCmdBlitImage (state->primary_command_buffer, image_mip_generation->image->image,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image_mip_generation->image->image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &image_blit, VK_FILTER_LINEAR);

            VkImageMemoryBarrier input_to_read_only_barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .dstAccessMask = 0u,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamilyIndex = system->device_queue_family_index,
                .dstQueueFamilyIndex = system->device_queue_family_index,
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

            vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u, NULL, 1u,
                                  &input_to_read_only_barrier);
        }

        VkImageMemoryBarrier last_to_read_only_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = 0u,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = system->device_queue_family_index,
            .dstQueueFamilyIndex = system->device_queue_family_index,
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

        vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &last_to_read_only_barrier);

        image_mip_generation->image->last_command_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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
                if (!frame_buffer->attachments[attachment_index].surface)
                {
                    can_be_created = KAN_FALSE;
                    KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                             "Unable to create frame buffer \"%s\" as its surface is already gone.",
                             frame_buffer->tracking_name)
                }

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
                VkImageViewCreateInfo create_info = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .pNext = NULL,
                    .flags = 0u,
                    .image = frame_buffer->attachments[attachment_index].image->image,
                    .viewType = kan_render_image_description_calculate_view_type (&image->description),
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
                            .aspectMask = kan_render_image_description_calculate_aspects (&image->description),
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

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
                if (frame_buffer->image_views[attachment_index] != VK_NULL_HANDLE)
                {
                    char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
                    snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "%s_image_view_%lu",
                              frame_buffer->tracking_name, (unsigned long) attachment_index);

                    struct VkDebugUtilsObjectNameInfoEXT object_name = {
                        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                        .pNext = NULL,
                        .objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
                        .objectHandle = (uint64_t) frame_buffer->image_views[attachment_index],
                        .pObjectName = debug_name,
                    };

                    vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
                }
#endif

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

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
            if (*output != VK_NULL_HANDLE)
            {
                struct VkDebugUtilsObjectNameInfoEXT object_name = {
                    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                    .pNext = NULL,
                    .objectType = VK_OBJECT_TYPE_FRAMEBUFFER,
                    .objectHandle = (uint64_t) *output,
                    .pObjectName = frame_buffer->tracking_name,
                };

                vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
            }
#endif
        }

        if (surface_index != UINT64_MAX)
        {
            frame_buffer->image_views[surface_index] = VK_NULL_HANDLE;
        }

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
    struct render_backend_surface_t *surface = (struct render_backend_surface_t *) system->surfaces.first;
    while (surface)
    {
        struct surface_blit_request_t *request = surface->first_blit_request;
        while (request)
        {
            if (request->image->last_command_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
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
                        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                        .oldLayout = request->image->last_command_layout,
                        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        .srcQueueFamilyIndex = system->device_queue_family_index,
                        .dstQueueFamilyIndex = system->device_queue_family_index,
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

                    vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &barrier_info);
                    request->image->last_command_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                }
            }

            // Prepare destination surface image.
            if (surface->render_state != SURFACE_RENDER_STATE_RECEIVED_DATA_FROM_BLIT)
            {
                VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
                VkAccessFlags source_access_mask = 0u;
                VkPipelineStageFlags source_stage = 0u;

                switch (surface->render_state)
                {
                case SURFACE_RENDER_STATE_RECEIVED_NO_OUTPUT:
                    old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
                    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                    break;

                case SURFACE_RENDER_STATE_RECEIVED_DATA_FROM_FRAME_BUFFER:
                    old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    source_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    break;

                case SURFACE_RENDER_STATE_RECEIVED_DATA_FROM_BLIT:
                    KAN_ASSERT (KAN_FALSE)
                    break;
                }

                VkImageMemoryBarrier barrier_info = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .pNext = NULL,
                    .srcAccessMask = source_access_mask,
                    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .oldLayout = old_layout,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .srcQueueFamilyIndex = system->device_queue_family_index,
                    .dstQueueFamilyIndex = system->device_queue_family_index,
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

                vkCmdPipelineBarrier (state->primary_command_buffer, source_stage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                                      0u, NULL, 0u, NULL, 1u, &barrier_info);
                surface->render_state = SURFACE_RENDER_STATE_RECEIVED_DATA_FROM_BLIT;
            }

            request = request->next;
        }

        surface = (struct render_backend_surface_t *) surface->list_node.next;
    }

    surface = (struct render_backend_surface_t *) system->surfaces.first;
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

            vkCmdBlitImage (state->primary_command_buffer, request->image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            surface->images[surface->acquired_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u,
                            &image_blit, VK_FILTER_LINEAR);
            request = request->next;
        }

        surface = (struct render_backend_surface_t *) surface->list_node.next;
    }

    surface = (struct render_backend_surface_t *) system->surfaces.first;
    while (surface)
    {
        struct surface_blit_request_t *request = surface->first_blit_request;
        while (request)
        {
            struct surface_blit_request_t *next = request->next;
            if (request->image->last_command_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
            {
                // If image supports sampling, return it back to normal layout.
                if (request->image->description.supports_sampling)
                {
                    VkImageMemoryBarrier barrier_info = {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        .pNext = NULL,
                        .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                        .dstAccessMask = 0u,
                        .oldLayout = request->image->last_command_layout,
                        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        .srcQueueFamilyIndex = system->device_queue_family_index,
                        .dstQueueFamilyIndex = system->device_queue_family_index,
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

                    vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &barrier_info);
                    request->image->last_command_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
            }

            kan_free_batched (system->surface_wrapper_allocation_group, request);
            request = next;
        }

        surface->first_blit_request = NULL;
        surface = (struct render_backend_surface_t *) surface->list_node.next;
    }
}

static void render_backend_system_submit_pass_instance (struct render_backend_system_t *system,
                                                        struct render_backend_command_state_t *state,
                                                        struct render_backend_pass_instance_t *pass_instance)
{
    vkEndCommandBuffer (pass_instance->command_buffer);

    // We put lots of barrier here in order to be sure that everything works properly.
    // It might not be the best from performance point of view, might need investigation later.

    VkImageMemoryBarrier image_barriers_static[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_BARRIERS];
    VkImageMemoryBarrier *image_barriers = image_barriers_static;
    uint32_t added_barriers = 0u;

    if (pass_instance->frame_buffer->attachments_count > KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_BARRIERS)
    {
        image_barriers =
            kan_allocate_general (system->utility_allocation_group,
                                  sizeof (VkImageMemoryBarrier) * pass_instance->frame_buffer->attachments_count,
                                  _Alignof (VkImageMemoryBarrier));
    }

    for (uint64_t attachment_index = 0u; attachment_index < pass_instance->frame_buffer->attachments_count;
         ++attachment_index)
    {
        struct render_backend_frame_buffer_attachment_t *attachment =
            &pass_instance->frame_buffer->attachments[attachment_index];

        switch (attachment->type)
        {
        case KAN_FRAME_BUFFER_ATTACHMENT_IMAGE:
        {
            VkImageLayout target_layout;
            // Currently we include everything possible into possible access flags, which is not optimal.
            VkAccessFlags possible_access_flags = 0u;
            VkAccessFlags target_access_flags = 0u;

            switch (attachment->image->description.type)
            {
            case KAN_RENDER_IMAGE_TYPE_COLOR_2D:
            case KAN_RENDER_IMAGE_TYPE_COLOR_3D:
                target_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                possible_access_flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                target_access_flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case KAN_RENDER_IMAGE_TYPE_DEPTH:
            case KAN_RENDER_IMAGE_TYPE_STENCIL:
            case KAN_RENDER_IMAGE_TYPE_DEPTH_STENCIL:
                target_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                possible_access_flags |=
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                target_access_flags |=
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;
            }

            if (attachment->image->last_command_layout != target_layout ||
                // There is always at least one attachment (the current one) and if there are several attachments,
                // then we're trying to be safe and add barrier to be sure that previous attachment has finished its
                // work. Previous attachment might not exist in this frame, but it is better to be safe than sorry
                // right now.
                attachment->image->first_frame_buffer_attachment->next)
            {
                if (attachment->image->description.supports_sampling)
                {
                    possible_access_flags |= VK_ACCESS_SHADER_READ_BIT;
                }

                image_barriers[added_barriers] = (VkImageMemoryBarrier) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .pNext = NULL,
                    .srcAccessMask = possible_access_flags,
                    .dstAccessMask = target_access_flags,
                    .oldLayout = attachment->image->last_command_layout,
                    .newLayout = target_layout,
                    .srcQueueFamilyIndex = system->device_queue_family_index,
                    .dstQueueFamilyIndex = system->device_queue_family_index,
                    .image = attachment->image->image,
                    .subresourceRange =
                        {
                            .aspectMask =
                                kan_render_image_description_calculate_aspects (&attachment->image->description),
                            .baseMipLevel = 0u,
                            .levelCount = 1u,
                            .baseArrayLayer = 0u,
                            .layerCount = 1u,
                        },
                };

                attachment->image->last_command_layout = target_layout;
                ++added_barriers;
            }

            break;
        }

        case KAN_FRAME_BUFFER_ATTACHMENT_SURFACE:
            switch (attachment->surface->render_state)
            {
            case SURFACE_RENDER_STATE_RECEIVED_NO_OUTPUT:
                image_barriers[added_barriers] = (VkImageMemoryBarrier) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .pNext = NULL,
                    .srcAccessMask = 0u,
                    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .srcQueueFamilyIndex = system->device_queue_family_index,
                    .dstQueueFamilyIndex = system->device_queue_family_index,
                    .image = attachment->surface->images[attachment->surface->acquired_image_index],
                    .subresourceRange =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .baseMipLevel = 0u,
                            .levelCount = 1u,
                            .baseArrayLayer = 0u,
                            .layerCount = 1u,
                        },
                };

                attachment->surface->render_state = SURFACE_RENDER_STATE_RECEIVED_DATA_FROM_FRAME_BUFFER;
                ++added_barriers;
                break;

            case SURFACE_RENDER_STATE_RECEIVED_DATA_FROM_FRAME_BUFFER:
                // Already transitioned to attachment state.
                break;

            case SURFACE_RENDER_STATE_RECEIVED_DATA_FROM_BLIT:
                // Blits should only be done after passes, how did we end up here?
                KAN_ASSERT (KAN_FALSE)
                break;
            }

            break;
        }
    }

    DEBUG_LABEL_SCOPE_BEGIN (state->primary_command_buffer, pass_instance->pass->tracking_name, DEBUG_LABEL_COLOR_PASS)

    vkCmdPipelineBarrier (state->primary_command_buffer,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                              VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          0u, 0u, NULL, 0u, NULL, added_barriers, image_barriers);

    vkCmdBeginRenderPass (state->primary_command_buffer, &pass_instance->render_pass_begin_info,
                          VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    vkCmdExecuteCommands (state->primary_command_buffer, 1u, &pass_instance->command_buffer);
    vkCmdEndRenderPass (state->primary_command_buffer);

    // Transition readable render targets so they can be used in shaders for sampling.
    added_barriers = 0u;

    for (uint64_t attachment_index = 0u; attachment_index < pass_instance->frame_buffer->attachments_count;
         ++attachment_index)
    {
        struct render_backend_frame_buffer_attachment_t *attachment =
            &pass_instance->frame_buffer->attachments[attachment_index];

        if (attachment->type == KAN_FRAME_BUFFER_ATTACHMENT_IMAGE && attachment->image->description.supports_sampling)
        {
            VkAccessFlags possible_access_flags = 0u;
            switch (attachment->image->description.type)
            {
            case KAN_RENDER_IMAGE_TYPE_COLOR_2D:
            case KAN_RENDER_IMAGE_TYPE_COLOR_3D:
                possible_access_flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case KAN_RENDER_IMAGE_TYPE_DEPTH:
            case KAN_RENDER_IMAGE_TYPE_STENCIL:
            case KAN_RENDER_IMAGE_TYPE_DEPTH_STENCIL:
                possible_access_flags |=
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;
            }

            image_barriers[added_barriers] = (VkImageMemoryBarrier) {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = possible_access_flags,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .oldLayout = attachment->image->last_command_layout,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamilyIndex = system->device_queue_family_index,
                .dstQueueFamilyIndex = system->device_queue_family_index,
                .image = attachment->image->image,
                .subresourceRange =
                    {
                        .aspectMask = kan_render_image_description_calculate_aspects (&attachment->image->description),
                        .baseMipLevel = 0u,
                        .levelCount = 1u,
                        .baseArrayLayer = 0u,
                        .layerCount = 1u,
                    },
            };

            attachment->image->last_command_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            ++added_barriers;
        }
    }

    if (added_barriers > 0u)
    {
        vkCmdPipelineBarrier (state->primary_command_buffer,
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, NULL, 0u, NULL, added_barriers,
                              image_barriers);
    }

    DEBUG_LABEL_SCOPE_END (state->primary_command_buffer)
    if (image_barriers != image_barriers_static)
    {
        kan_free_general (system->utility_allocation_group, image_barriers,
                          sizeof (VkImageMemoryBarrier) * pass_instance->frame_buffer->attachments_count);
    }

    struct render_backend_pass_instance_dependency_t *dependant = pass_instance->first_dependant;
    while (dependant)
    {
        // Dependencies left can be already zero if we were trying to get out of dead lock situation.
        if (dependant->dependant_pass_instance->dependencies_left != 0u)
        {
            --dependant->dependant_pass_instance->dependencies_left;
            if (dependant->dependant_pass_instance->dependencies_left == 0u)
            {
                kan_bd_list_add (&system->pass_instances_available, NULL,
                                 &dependant->dependant_pass_instance->node_in_available);
            }
        }

        dependant = dependant->next;
    }

    kan_bd_list_remove (&system->pass_instances, &pass_instance->node_in_all);
    kan_bd_list_remove (&system->pass_instances_available, &pass_instance->node_in_available);
}

static void render_backend_system_submit_graphics (struct render_backend_system_t *system)
{
    struct render_backend_command_state_t *state = &system->command_states[system->current_frame_in_flight_index];
    struct render_backend_surface_t *surface = (struct render_backend_surface_t *) system->surfaces.first;

    while (surface)
    {
        surface->render_state = SURFACE_RENDER_STATE_RECEIVED_NO_OUTPUT;
        surface = (struct render_backend_surface_t *) surface->list_node.next;
    }

    struct render_backend_schedule_state_t *schedule = &system->schedule_states[system->current_frame_in_flight_index];
    submit_mip_generation (system, schedule, state);
    process_frame_buffer_create_requests (system, schedule, state);

    struct render_backend_pass_t *pass = (struct render_backend_pass_t *) system->passes.first;
    while (pass)
    {
        struct render_backend_pass_dependency_t *dependant_pass = pass->first_dependant_pass;
        while (dependant_pass)
        {
            struct render_backend_pass_instance_t *dependant_instance = dependant_pass->dependant_pass->first_instance;
            while (dependant_instance)
            {
                struct render_backend_pass_instance_t *dependency_instance = pass->first_instance;
                while (dependency_instance)
                {
                    render_backend_pass_instance_add_dependency_internal (dependant_instance, dependency_instance);
                    dependency_instance = dependency_instance->next_in_pass;
                }

                dependant_instance = dependant_instance->next_in_pass;
            }

            dependant_pass = dependant_pass->next;
        }

        pass = (struct render_backend_pass_t *) pass->list_node.next;
    }

    while (system->pass_instances.size > 0u)
    {
        while (system->pass_instances_available.size > 0u)
        {
            render_backend_system_submit_pass_instance (
                system, state,
                (struct render_backend_pass_instance_t *) (((uint8_t *) system->pass_instances_available.first) -
                                                           offsetof (struct render_backend_pass_instance_t,
                                                                     node_in_available)));
        }

        if (system->pass_instances.size > 0u)
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Failed to topologically sort pass instances. Submitting pass instance with lowest amount of "
                     "dependencies to try to work around the issue.")

            struct render_backend_pass_instance_t *make_available_anyway = NULL;
            struct render_backend_pass_instance_t *pass_instance =
                (struct render_backend_pass_instance_t *) (((uint8_t *) system->pass_instances.first) -
                                                           offsetof (struct render_backend_pass_instance_t,
                                                                     node_in_all));

            while (pass_instance)
            {
                if (make_available_anyway == NULL ||
                    make_available_anyway->dependencies_left > pass_instance->dependencies_left ||
                    // We prefer instances that do not write to surfaces to instance that do write.
                    // If instance frame buffer has several instances under the hood, then it definitely
                    // writes to the surface.
                    (make_available_anyway->frame_buffer->instance_array_size > 0u &&
                     pass_instance->frame_buffer->instance_array_size == 0u))
                {
                    make_available_anyway = pass_instance;
                }

                pass_instance =
                    (struct render_backend_pass_instance_t *) (((uint8_t *) pass_instance->node_in_all.next) -
                                                               offsetof (struct render_backend_pass_instance_t,
                                                                         node_in_all));
            }

            make_available_anyway->dependencies_left = 0u;
            kan_bd_list_add (&system->pass_instances_available, NULL, &make_available_anyway->node_in_available);
        }
    }

    pass = (struct render_backend_pass_t *) system->passes.first;
    while (pass)
    {
        pass->first_instance = NULL;
        pass = (struct render_backend_pass_t *) pass->list_node.next;
    }

    kan_stack_group_allocator_shrink (&system->pass_instance_allocator);
    kan_stack_group_allocator_reset (&system->pass_instance_allocator);

    process_surface_blit_requests (system, state);
    surface = (struct render_backend_surface_t *) system->surfaces.first;

    while (surface)
    {
        // We need to transition swap chain images into present layout.
        // If surface image didn't receive any output, we don't care about old layout.
        // Otherwise, it must be properly transitioned.

        VkImageLayout old_layout;
        switch (surface->render_state)
        {
        case SURFACE_RENDER_STATE_RECEIVED_NO_OUTPUT:
            old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            break;

        case SURFACE_RENDER_STATE_RECEIVED_DATA_FROM_FRAME_BUFFER:
            old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;

        case SURFACE_RENDER_STATE_RECEIVED_DATA_FROM_BLIT:
            old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            break;
        }

        VkImageMemoryBarrier barrier_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = 0u,
            .dstAccessMask = 0u,
            .oldLayout = old_layout,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = system->device_queue_family_index,
            .dstQueueFamilyIndex = system->device_queue_family_index,
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

        vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &barrier_info);
        surface = (struct render_backend_surface_t *) surface->list_node.next;
    }
}

static void render_backend_system_finish_command_submission (struct render_backend_system_t *system)
{
    struct render_backend_command_state_t *state = &system->command_states[system->current_frame_in_flight_index];
    if (vkEndCommandBuffer (state->primary_command_buffer) != VK_SUCCESS)
    {
        kan_critical_error ("Failed to end recording primary buffer.", __FILE__, __LINE__);
    }

    uint32_t semaphores_to_wait = 0u;
    static VkSemaphore static_wait_semaphores[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES];
    static VkPipelineStageFlags static_semaphore_stages[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES];

    VkSemaphore *wait_semaphores = static_wait_semaphores;
    VkPipelineStageFlags *semaphore_stages = static_semaphore_stages;
    struct render_backend_surface_t *surface = (struct render_backend_surface_t *) system->surfaces.first;

    while (surface)
    {
        if (surface->surface != VK_NULL_HANDLE)
        {
            if (semaphores_to_wait < KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES)
            {
                wait_semaphores[semaphores_to_wait] =
                    surface->image_available_semaphores[system->current_frame_in_flight_index];
                semaphore_stages[semaphores_to_wait] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            }

            ++semaphores_to_wait;
        }

        surface = (struct render_backend_surface_t *) surface->list_node.next;
    }

    if (semaphores_to_wait > KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES)
    {
        // Too many semaphores to capture everything to static array, allocate new one.
        wait_semaphores = kan_allocate_general (system->utility_allocation_group,
                                                sizeof (VkSemaphore) * semaphores_to_wait, _Alignof (VkSemaphore));
        semaphore_stages =
            kan_allocate_general (system->utility_allocation_group, sizeof (VkPipelineStageFlags) * semaphores_to_wait,
                                  _Alignof (VkPipelineStageFlags));

        semaphores_to_wait = 0u;
        surface = (struct render_backend_surface_t *) system->surfaces.first;

        while (surface)
        {
            if (surface->surface != VK_NULL_HANDLE)
            {
                wait_semaphores[semaphores_to_wait] =
                    surface->image_available_semaphores[system->current_frame_in_flight_index];
                semaphore_stages[semaphores_to_wait] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                ++semaphores_to_wait;
            }

            surface = (struct render_backend_surface_t *) surface->list_node.next;
        }
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    struct VkDebugUtilsLabelEXT queue_label = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pNext = NULL,
        .pLabelName = "Merged Queue",
        .color = {0.082f, 0.639f, 0.114f, 1.0f},
    };

    vkQueueBeginDebugUtilsLabelEXT (system->device_queue, &queue_label);
#endif

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = semaphores_to_wait,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = semaphore_stages,
        .commandBufferCount = 1u,
        .pCommandBuffers = &state->primary_command_buffer,
        .signalSemaphoreCount = 1u,
        .pSignalSemaphores = &system->render_finished_semaphores[system->current_frame_in_flight_index],
    };

    if (vkQueueSubmit (system->device_queue, 1u, &submit_info,
                       system->in_flight_fences[system->current_frame_in_flight_index]) != VK_SUCCESS)
    {
        kan_critical_error ("Failed to submit work to merged queue.", __FILE__, __LINE__);
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    vkQueueEndDebugUtilsLabelEXT (system->device_queue);
#endif

    if (wait_semaphores != static_wait_semaphores)
    {
        kan_free_general (system->utility_allocation_group, wait_semaphores, sizeof (VkSemaphore) * semaphores_to_wait);
        kan_free_general (system->utility_allocation_group, semaphore_stages,
                          sizeof (VkPipelineStageFlags) * semaphores_to_wait);
    }
}

static void render_backend_system_submit_present (struct render_backend_system_t *system)
{
    static VkSwapchainKHR static_swap_chains[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES];
    static uint32_t static_image_indices[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES];

    uint32_t swap_chains_count = 0u;
    VkSwapchainKHR *swap_chains = static_swap_chains;
    uint32_t *image_indices = static_image_indices;
    struct render_backend_surface_t *surface = (struct render_backend_surface_t *) system->surfaces.first;

    while (surface)
    {
        if (surface->surface != VK_NULL_HANDLE)
        {
            if (swap_chains_count < KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES)
            {
                swap_chains[swap_chains_count] = surface->swap_chain;
                image_indices[swap_chains_count] = surface->acquired_image_index;
            }

            ++swap_chains_count;
        }

        surface = (struct render_backend_surface_t *) surface->list_node.next;
    }

    if (swap_chains_count > KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES)
    {
        // Too many surfaces to capture everything to static array, allocate new one.
        swap_chains = kan_allocate_general (system->utility_allocation_group, sizeof (VkSemaphore) * swap_chains_count,
                                            _Alignof (VkSwapchainKHR));
        image_indices = kan_allocate_general (system->utility_allocation_group, sizeof (uint32_t) * swap_chains_count,
                                              _Alignof (uint32_t));
        swap_chains_count = 0u;
        surface = (struct render_backend_surface_t *) system->surfaces.first;

        while (surface)
        {
            if (surface->surface != VK_NULL_HANDLE)
            {
                swap_chains[swap_chains_count] = surface->swap_chain;
                image_indices[swap_chains_count] = surface->acquired_image_index;
                ++swap_chains_count;
            }

            surface = (struct render_backend_surface_t *) surface->list_node.next;
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

    const VkResult present_result = vkQueuePresentKHR (system->device_queue, &present_info);
    if (present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR &&
        present_result != VK_ERROR_OUT_OF_DATE_KHR)
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
        !schedule->first_scheduled_pipeline_parameter_set_destroy &&
        !schedule->first_scheduled_detached_descriptor_set_destroy && !schedule->first_scheduled_code_module_destroy &&
        !schedule->first_scheduled_graphics_pipeline_destroy &&
        !schedule->first_scheduled_graphics_pipeline_family_destroy && !schedule->first_scheduled_buffer_destroy &&
        !schedule->first_scheduled_frame_lifetime_allocator_destroy &&
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

    render_backend_system_begin_command_submission (system);
    render_backend_system_submit_transfer (system);
    render_backend_system_submit_graphics (system);
    render_backend_system_finish_command_submission (system);
    render_backend_system_submit_present (system);

    struct render_backend_frame_lifetime_allocator_t *frame_lifetime_allocator =
        (struct render_backend_frame_lifetime_allocator_t *) system->frame_lifetime_allocators.first;

    while (frame_lifetime_allocator)
    {
        render_backend_frame_lifetime_allocator_clean_empty_pages (frame_lifetime_allocator);
        frame_lifetime_allocator =
            (struct render_backend_frame_lifetime_allocator_t *) frame_lifetime_allocator->list_node.next;
    }

    render_backend_system_clean_current_schedule_if_safe (system);
    struct render_backend_command_state_t *command_state =
        &system->command_states[system->current_frame_in_flight_index];

    KAN_ASSERT (command_state->secondary_command_buffers_used <= command_state->secondary_command_buffers.size)
    const uint64_t excess_command_buffers =
        command_state->secondary_command_buffers.size - command_state->secondary_command_buffers_used;

    if (excess_command_buffers > 0u)
    {
        VkCommandBuffer *first_excess_buffer =
            &((VkCommandBuffer *)
                  command_state->secondary_command_buffers.data)[command_state->secondary_command_buffers_used];

        vkFreeCommandBuffers (system->device, command_state->command_pool, (uint32_t) excess_command_buffers,
                              first_excess_buffer);

        command_state->secondary_command_buffers.size = command_state->secondary_command_buffers_used;
        if (command_state->secondary_command_buffers.size * 2u < command_state->secondary_command_buffers.capacity &&
            command_state->secondary_command_buffers.size > KAN_CONTEXT_RENDER_BACKEND_VULKAN_GCB_ARRAY_SIZE)
        {
            kan_dynamic_array_set_capacity (&command_state->secondary_command_buffers,
                                            command_state->secondary_command_buffers.capacity / 2u);
        }
    }

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

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
        // TODO: Add type prefixes to all names everywhere -- things like RenderDoc show names without types.
        char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
        snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "%s_image_view_%lu",
                  surface->tracking_name, (unsigned long) index);

        struct VkDebugUtilsObjectNameInfoEXT object_name = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = NULL,
            .objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
            .objectHandle = (uint64_t) surface->image_views[view_index],
            .pObjectName = debug_name,
        };

        vkSetDebugUtilsObjectNameEXT (surface->system->device, &object_name);
#endif
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

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
        char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
        snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "%s_image_available_%lu",
                  surface->tracking_name, (unsigned long) index);

        struct VkDebugUtilsObjectNameInfoEXT object_name = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = NULL,
            .objectType = VK_OBJECT_TYPE_SEMAPHORE,
            .objectHandle = (uint64_t) surface->image_available_semaphores[index],
            .pObjectName = debug_name,
        };

        vkSetDebugUtilsObjectNameEXT (surface->system->device, &object_name);
#endif
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
                                              surface->system->device_queue_family_index, surface->surface,
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
                      sizeof (VkPresentModeKHR) * present_modes_count);

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
        .imageUsage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
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

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    struct VkDebugUtilsObjectNameInfoEXT object_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = NULL,
        .objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR,
        .objectHandle = (uint64_t) surface->swap_chain,
        .pObjectName = surface->tracking_name,
    };

    vkSetDebugUtilsObjectNameEXT (surface->system->device, &object_name);
#endif

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
    struct render_backend_surface_t *surface = (struct render_backend_surface_t *) system->surfaces.first;

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
                system->device, surface->swap_chain, KAN_CONTEXT_RENDER_BACKEND_VULKAN_IMAGE_WAIT_NS,
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

            struct surface_frame_buffer_attachment_t *attachment = surface->first_frame_buffer_attachment;
            while (attachment)
            {
                attachment->frame_buffer->instance_index = surface->acquired_image_index;
                attachment = attachment->next;
            }
        }

        surface = (struct render_backend_surface_t *) surface->list_node.next;
    }

    if (any_swap_chain_outdated)
    {
        vkDeviceWaitIdle (system->device);
        surface = (struct render_backend_surface_t *) system->surfaces.first;

        while (surface)
        {
            if (surface->needs_recreation)
            {
                render_backend_surface_destroy_swap_chain (surface);
                render_backend_surface_create_swap_chain (surface, kan_application_system_get_window_info_from_handle (
                                                                       application_system, surface->window_handle));
            }

            surface = (struct render_backend_surface_t *) surface->list_node.next;
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
                         KAN_CONTEXT_RENDER_BACKEND_VULKAN_FENCE_WAIT_NS);

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

    if (vkResetCommandPool (system->device, system->command_states[system->current_frame_in_flight_index].command_pool,
                            0u) != VK_SUCCESS)
    {
        kan_critical_error ("Unexpected failure when resetting graphics command pool.", __FILE__, __LINE__);
    }

    struct render_backend_schedule_state_t *schedule = &system->schedule_states[system->current_frame_in_flight_index];
    struct scheduled_pipeline_parameter_set_destroy_t *pipeline_parameter_set_destroy =
        schedule->first_scheduled_pipeline_parameter_set_destroy;
    schedule->first_scheduled_pipeline_parameter_set_destroy = NULL;

    while (pipeline_parameter_set_destroy)
    {
        kan_bd_list_remove (&system->pipeline_parameter_sets, &pipeline_parameter_set_destroy->set->list_node);
        render_backend_system_destroy_pipeline_parameter_set (system, pipeline_parameter_set_destroy->set);
        pipeline_parameter_set_destroy = pipeline_parameter_set_destroy->next;
    }

    struct scheduled_detached_descriptor_set_destroy_t *detached_descriptor_set_destroy =
        schedule->first_scheduled_detached_descriptor_set_destroy;
    schedule->first_scheduled_detached_descriptor_set_destroy = NULL;

    while (detached_descriptor_set_destroy)
    {
        render_backend_descriptor_set_allocator_free (system, &system->descriptor_set_allocator,
                                                      &detached_descriptor_set_destroy->allocation);
        detached_descriptor_set_destroy = detached_descriptor_set_destroy->next;
    }

    struct scheduled_code_module_destroy_t *code_module_destroy = schedule->first_scheduled_code_module_destroy;
    schedule->first_scheduled_code_module_destroy = NULL;

    while (code_module_destroy)
    {
        kan_bd_list_remove (&system->code_modules, &code_module_destroy->module->list_node);
        render_backend_system_destroy_code_module (system, code_module_destroy->module);
        code_module_destroy = code_module_destroy->next;
    }

    struct scheduled_graphics_pipeline_destroy_t *graphics_pipeline_destroy =
        schedule->first_scheduled_graphics_pipeline_destroy;
    schedule->first_scheduled_graphics_pipeline_destroy = NULL;

    while (graphics_pipeline_destroy)
    {
        // If we still have lingering compilation request, we must deal with it.
        while (graphics_pipeline_destroy->pipeline->compilation_request)
        {
            kan_mutex_lock (system->compiler_state.state_transition_mutex);
            switch (graphics_pipeline_destroy->pipeline->compilation_state)
            {
            case PIPELINE_COMPILATION_STATE_PENDING:
                // Request is pending, therefore it is possible to safely remove it.
                render_backend_pipeline_compiler_state_remove_graphics_request_unsafe (
                    &system->compiler_state, graphics_pipeline_destroy->pipeline->compilation_request);
                kan_mutex_unlock (system->compiler_state.state_transition_mutex);

                render_backend_compiler_state_destroy_graphics_request (
                    graphics_pipeline_destroy->pipeline->compilation_request);

                graphics_pipeline_destroy->pipeline->compilation_state = PIPELINE_COMPILATION_STATE_FAILURE;
                graphics_pipeline_destroy->pipeline->compilation_request = NULL;
                break;

            case PIPELINE_COMPILATION_STATE_EXECUTION:
                // Bad case, it is already executing and we cannot stop it.
                // The best solution is to delay destruction, but to do that we also need to delay family destruction.
                // It is a rare case, therefore we're using simplistic wait here instead of real delay.
                kan_mutex_unlock (system->compiler_state.state_transition_mutex);
                kan_platform_sleep (KAN_CONTEXT_RENDER_BACKEND_VULKAN_COMPILATION_WAIT_NS);
                break;

            case PIPELINE_COMPILATION_STATE_SUCCESS:
            case PIPELINE_COMPILATION_STATE_FAILURE:
                // Already got completed when lock is acquired, we can exit.
                KAN_ASSERT (!graphics_pipeline_destroy->pipeline->compilation_request)
                kan_mutex_unlock (system->compiler_state.state_transition_mutex);
                break;
            }
        }

        kan_bd_list_remove (&system->graphics_pipelines, &graphics_pipeline_destroy->pipeline->list_node);
        render_backend_system_destroy_graphics_pipeline (system, graphics_pipeline_destroy->pipeline);
        graphics_pipeline_destroy = graphics_pipeline_destroy->next;
    }

    struct scheduled_graphics_pipeline_family_destroy_t *graphics_pipeline_family_destroy =
        schedule->first_scheduled_graphics_pipeline_family_destroy;
    schedule->first_scheduled_graphics_pipeline_family_destroy = NULL;

    while (graphics_pipeline_family_destroy)
    {
        kan_bd_list_remove (&system->graphics_pipeline_families, &graphics_pipeline_family_destroy->family->list_node);
        render_backend_system_destroy_graphics_pipeline_family (system, graphics_pipeline_family_destroy->family);
        graphics_pipeline_family_destroy = graphics_pipeline_family_destroy->next;
    }

    struct scheduled_frame_buffer_destroy_t *frame_buffer_destroy = schedule->first_scheduled_frame_buffer_destroy;
    schedule->first_scheduled_frame_buffer_destroy = NULL;

    while (frame_buffer_destroy)
    {
        kan_bd_list_remove (&system->frame_buffers, &frame_buffer_destroy->frame_buffer->list_node);
        render_backend_system_destroy_frame_buffer (system, frame_buffer_destroy->frame_buffer);
        frame_buffer_destroy = frame_buffer_destroy->next;
    }

    struct scheduled_detached_frame_buffer_destroy_t *detached_frame_buffer_destroy =
        schedule->first_scheduled_detached_frame_buffer_destroy;
    schedule->first_scheduled_detached_frame_buffer_destroy = NULL;

    while (detached_frame_buffer_destroy)
    {
        vkDestroyFramebuffer (system->device, detached_frame_buffer_destroy->detached_frame_buffer,
                              VULKAN_ALLOCATION_CALLBACKS (system));
        detached_frame_buffer_destroy = detached_frame_buffer_destroy->next;
    }

    struct scheduled_pass_destroy_t *pass_destroy = schedule->first_scheduled_pass_destroy;
    schedule->first_scheduled_pass_destroy = NULL;

    while (pass_destroy)
    {
        kan_bd_list_remove (&system->passes, &pass_destroy->pass->list_node);
        render_backend_system_destroy_pass (system, pass_destroy->pass);
        pass_destroy = pass_destroy->next;
    }

    struct scheduled_buffer_destroy_t *buffer_destroy = schedule->first_scheduled_buffer_destroy;
    schedule->first_scheduled_buffer_destroy = NULL;

    while (buffer_destroy)
    {
        kan_bd_list_remove (&system->buffers, &buffer_destroy->buffer->list_node);
        render_backend_system_destroy_buffer (system, buffer_destroy->buffer);
        buffer_destroy = buffer_destroy->next;
    }

    struct scheduled_frame_lifetime_allocator_destroy_t *frame_lifetime_allocator_destroy =
        schedule->first_scheduled_frame_lifetime_allocator_destroy;
    schedule->first_scheduled_frame_lifetime_allocator_destroy = NULL;

    while (frame_lifetime_allocator_destroy)
    {
        kan_bd_list_remove (&system->frame_lifetime_allocators,
                            &frame_lifetime_allocator_destroy->frame_lifetime_allocator->list_node);
        render_backend_system_destroy_frame_lifetime_allocator (
            system, frame_lifetime_allocator_destroy->frame_lifetime_allocator, KAN_TRUE);
        frame_lifetime_allocator_destroy = frame_lifetime_allocator_destroy->next;
    }

    struct scheduled_detached_image_view_destroy_t *detached_image_view_destroy =
        schedule->first_scheduled_detached_image_view_destroy;
    schedule->first_scheduled_detached_image_view_destroy = NULL;

    while (detached_image_view_destroy)
    {
        vkDestroyImageView (system->device, detached_image_view_destroy->detached_image_view,
                            VULKAN_ALLOCATION_CALLBACKS (system));
        detached_image_view_destroy = detached_image_view_destroy->next;
    }

    struct scheduled_image_destroy_t *image_destroy = schedule->first_scheduled_image_destroy;
    schedule->first_scheduled_image_destroy = NULL;

    while (image_destroy)
    {
        kan_bd_list_remove (&system->images, &image_destroy->image->list_node);
        render_backend_system_destroy_image (system, image_destroy->image);
        image_destroy = image_destroy->next;
    }

    struct scheduled_detached_image_destroy_t *detached_image_destroy =
        schedule->first_scheduled_detached_image_destroy;
    schedule->first_scheduled_detached_image_destroy = NULL;

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

    render_backend_system_clean_current_schedule_if_safe (system);
    struct render_backend_frame_lifetime_allocator_t *frame_lifetime_allocator =
        (struct render_backend_frame_lifetime_allocator_t *) system->frame_lifetime_allocators.first;

    while (frame_lifetime_allocator)
    {
        render_backend_frame_lifetime_allocator_retire_old_allocations (frame_lifetime_allocator);
        frame_lifetime_allocator =
            (struct render_backend_frame_lifetime_allocator_t *) frame_lifetime_allocator->list_node.next;
    }

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
    surface->first_blit_request = NULL;

    while (request)
    {
        struct surface_blit_request_t *next = request->next;
        kan_free_batched (surface->system->surface_wrapper_allocation_group, request);
        request = next;
    }

    struct surface_frame_buffer_attachment_t *attachment = surface->first_frame_buffer_attachment;
    surface->first_frame_buffer_attachment = NULL;

    while (attachment)
    {
        struct surface_frame_buffer_attachment_t *next = attachment->next;
        for (uint64_t attachment_index = 0u; attachment_index < attachment->frame_buffer->attachments_count;
             ++attachment_index)
        {
            if (attachment->frame_buffer->attachments[attachment_index].type == KAN_FRAME_BUFFER_ATTACHMENT_SURFACE &&
                attachment->frame_buffer->attachments[attachment_index].surface == surface)
            {
                // Forcibly detach buffer from surface to avoid memory corruption.
                attachment->frame_buffer->attachments[attachment_index].surface = NULL;
            }
        }

        kan_free_batched (surface->system->surface_wrapper_allocation_group, attachment);
        attachment = next;
    }

    kan_bd_list_remove (&surface->system->surfaces, &surface->list_node);
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

    kan_bd_list_add (&system->surfaces, NULL, &new_surface->list_node);
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
