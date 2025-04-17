#include <kan/context/render_backend_implementation_interface.h>

KAN_LOG_DEFINE_CATEGORY (render_backend_system_vulkan);

void kan_render_backend_system_config_init (struct kan_render_backend_system_config_t *instance)
{
    instance->application_info_name = NULL;
    instance->version_major = 1u;
    instance->version_minor = 0u;
    instance->version_patch = 0u;
    instance->uses_custom_gamma_correction = KAN_FALSE;
}

kan_context_system_t render_backend_system_create (kan_allocation_group_t group, void *user_config)
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
    system->parameter_set_layout_wrapper_allocation_group =
        kan_allocation_group_get_child (group, "parameter_set_layout_wrapper");
    system->code_module_wrapper_allocation_group = kan_allocation_group_get_child (group, "code_module_wrapper");
    system->pipeline_layout_wrapper_allocation_group =
        kan_allocation_group_get_child (group, "pipeline_layout_wrapper");
    system->pipeline_wrapper_allocation_group = kan_allocation_group_get_child (group, "pipeline_wrapper");
    system->pipeline_parameter_set_wrapper_allocation_group =
        kan_allocation_group_get_child (group, "pipeline_parameter_set_wrapper");
    system->buffer_wrapper_allocation_group = kan_allocation_group_get_child (group, "buffer_wrapper");
    system->frame_lifetime_wrapper_allocation_group =
        kan_allocation_group_get_child (group, "frame_lifetime_allocator_wrapper");
    system->image_wrapper_allocation_group = kan_allocation_group_get_child (group, "descriptor_set_wrapper");
    system->descriptor_set_wrapper_allocation_group = kan_allocation_group_get_child (group, "descriptor_set_wrapper");
    system->read_back_status_allocation_group = kan_allocation_group_get_child (group, "read_back_status");
    system->cached_samplers_allocation_group = kan_allocation_group_get_child (group, "cached_samplers");

    system->section_create_surface = kan_cpu_section_get ("render_backend_create_surface");
    system->section_create_frame_buffer = kan_cpu_section_get ("render_backend_create_frame_buffer");
    system->section_create_frame_buffer_internal = kan_cpu_section_get ("render_backend_create_frame_buffer_internal");
    system->section_create_pass = kan_cpu_section_get ("render_backend_create_pass");
    system->section_create_pass_internal = kan_cpu_section_get ("render_backend_create_pass_internal");
    system->section_create_pass_instance = kan_cpu_section_get ("render_backend_create_pass_instance");
    system->section_register_pipeline_parameter_set_layout =
        kan_cpu_section_get ("render_backend_register_pipeline_parameter_set_layout");
    system->section_create_code_module = kan_cpu_section_get ("render_backend_create_code_module");
    system->section_create_code_module_internal = kan_cpu_section_get ("render_backend_create_code_module_internal");
    system->section_register_pipeline_layout = kan_cpu_section_get ("render_backend_register_pipeline_layout");
    system->section_create_graphics_pipeline = kan_cpu_section_get ("render_backend_create_graphics_pipeline");
    system->section_create_graphics_pipeline_internal =
        kan_cpu_section_get ("render_backend_create_graphics_pipeline_internal");
    system->section_create_pipeline_parameter_set =
        kan_cpu_section_get ("render_backend_create_pipeline_parameter_set");
    system->section_create_pipeline_parameter_set_internal =
        kan_cpu_section_get ("render_backend_create_pipeline_parameter_set_internal");
    system->section_create_buffer = kan_cpu_section_get ("render_backend_create_buffer");
    system->section_create_buffer_internal = kan_cpu_section_get ("render_backend_create_buffer_internal");
    system->section_create_frame_lifetime_allocator =
        kan_cpu_section_get ("render_backend_create_frame_lifetime_allocator");
    system->section_create_frame_lifetime_allocator_internal =
        kan_cpu_section_get ("render_backend_create_frame_lifetime_allocator_internal");
    system->section_create_image = kan_cpu_section_get ("render_backend_create_image");
    system->section_create_image_internal = kan_cpu_section_get ("render_backend_create_image_internal");

    system->section_surface_init_with_window = kan_cpu_section_get ("render_backend_surface_init_with_window");
    system->section_surface_shutdown_with_window = kan_cpu_section_get ("render_backend_surface_shutdown_with_window");
    system->section_surface_create_swap_chain = kan_cpu_section_get ("render_backend_surface_create_swap_chain");
    system->section_surface_destroy_swap_chain = kan_cpu_section_get ("render_backend_surface_destroy_swap_chain");

    system->section_pipeline_compiler_request = kan_cpu_section_get ("render_backend_pipeline_compiler_request");

    system->section_pipeline_compilation = kan_cpu_section_get ("render_backend_pipeline_compilation");
    system->section_wait_for_pipeline_compilation =
        kan_cpu_section_get ("render_backend_wait_for_pipeline_compilation");

    system->section_descriptor_set_allocator_allocate =
        kan_cpu_section_get ("render_backend_descriptor_set_allocator_allocate");
    system->section_descriptor_set_allocator_free =
        kan_cpu_section_get ("render_backend_descriptor_set_allocator_free");

    system->section_apply_descriptor_set_mutation =
        kan_cpu_section_get ("render_backend_apply_descriptor_set_mutation");
    system->section_pipeline_parameter_set_update =
        kan_cpu_section_get ("render_backend_pipeline_parameter_set_update");

    system->section_frame_lifetime_allocator_allocate =
        kan_cpu_section_get ("render_backend_frame_lifetime_allocator_allocate");
    system->section_frame_lifetime_allocator_retire_old_allocations =
        kan_cpu_section_get ("render_backend_frame_lifetime_allocator_retire_old_allocations");
    system->section_frame_lifetime_allocator_clean_empty_pages =
        kan_cpu_section_get ("render_backend_frame_lifetime_allocator_clean_empty_pages");
    system->section_allocate_for_staging = kan_cpu_section_get ("render_backend_allocate_for_staging");

    system->section_image_create_on_device = kan_cpu_section_get ("render_backend_image_create_on_device");
    system->section_image_upload = kan_cpu_section_get ("render_backend_image_upload");
    system->section_image_resize_render_target = kan_cpu_section_get ("render_backend_image_resize_render_target");

    system->section_next_frame = kan_cpu_section_get ("render_backend_next_frame");
    system->section_next_frame_synchronization = kan_cpu_section_get ("render_backend_next_frame_synchronization");
    system->section_next_frame_acquire_images = kan_cpu_section_get ("render_backend_next_frame_acquire_images");
    system->section_next_frame_command_pool_reset =
        kan_cpu_section_get ("render_backend_next_frame_command_pool_reset");
    system->section_next_frame_destruction_schedule =
        kan_cpu_section_get ("render_backend_next_frame_destruction_schedule");
    system->section_next_frame_destruction_schedule_waiting_pipeline_compilation =
        kan_cpu_section_get ("render_backend_next_frame_destruction_schedule_waiting_pipeline_compilation");

    system->section_submit_previous_frame = kan_cpu_section_get ("render_backend_submit_previous_frame");
    system->section_submit_transfer = kan_cpu_section_get ("render_backend_submit_transfer");
    system->section_submit_graphics = kan_cpu_section_get ("render_backend_submit_graphics");
    system->section_submit_mip_generation = kan_cpu_section_get ("render_backend_submit_mip_generation");
    system->section_submit_blit_requests = kan_cpu_section_get ("render_backend_submit_blit_requests");
    system->section_submit_pass_instance = kan_cpu_section_get ("render_backend_submit_pass_instance");
    system->section_pass_instance_resolve_checkpoints =
        kan_cpu_section_get ("render_backend_pass_instance_resolve_checkpoints");
    system->section_pass_instance_sort_and_submission =
        kan_cpu_section_get ("render_backend_pass_instance_sort_and_submission");
    system->section_submit_read_back = kan_cpu_section_get ("render_backend_submit_read_back");
    system->section_present = kan_cpu_section_get ("render_backend_present");

    system->frame_started = KAN_FALSE;
    system->current_frame_in_flight_index = 0u;

    system->resource_registration_lock = kan_atomic_int_init (0);
    system->pipeline_parameter_set_layout_registration_lock = kan_atomic_int_init (0);
    system->pipeline_layout_registration_lock = kan_atomic_int_init (0);
    system->pass_instance_state_management_lock = kan_atomic_int_init (0);

    kan_bd_list_init (&system->surfaces);
    kan_bd_list_init (&system->frame_buffers);
    kan_bd_list_init (&system->passes);
    kan_bd_list_init (&system->pass_instances);
    kan_bd_list_init (&system->pass_instances_available);

    kan_hash_storage_init (&system->pipeline_parameter_set_layouts,
                           system->parameter_set_layout_wrapper_allocation_group,
                           KAN_CONTEXT_RENDER_BACKEND_VULKAN_SET_LAYOUT_BUCKETS);

    kan_hash_storage_init (&system->pipeline_layouts, system->pipeline_layout_wrapper_allocation_group,
                           KAN_CONTEXT_RENDER_BACKEND_VULKAN_PL_BUCKETS);

    kan_bd_list_init (&system->code_modules);
    kan_bd_list_init (&system->graphics_pipelines);
    kan_bd_list_init (&system->pipeline_parameter_sets);
    kan_bd_list_init (&system->buffers);
    kan_bd_list_init (&system->frame_lifetime_allocators);
    kan_bd_list_init (&system->images);

    system->staging_frame_lifetime_allocator = NULL;
    system->supported_devices = NULL;
    system->selected_device_info = NULL;

    system->sampler_cache_lock = kan_atomic_int_init (0);
    system->first_cached_sampler = NULL;

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
        struct kan_render_backend_system_config_t *config = user_config;
        system->application_info_name = config->application_info_name;
        system->version_major = config->version_major;
        system->version_minor = config->version_minor;
        system->version_patch = config->version_patch;
        system->uses_custom_gamma_correction = config->uses_custom_gamma_correction;
    }
    else
    {
        system->application_info_name = kan_string_intern ("unnamed_application");
        system->version_major = 1u;
        system->version_minor = 0u;
        system->version_patch = 0u;
        system->uses_custom_gamma_correction = KAN_FALSE;
    }

    system->interned_temporary_staging_buffer = kan_string_intern ("temporary_staging_buffer");
    return KAN_HANDLE_SET (kan_context_system_t, system);
}

void render_backend_system_connect (kan_context_system_t handle, kan_context_t context)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (handle);
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

    for (kan_loop_size_t memory_type_index = 0u;
         memory_type_index < (kan_loop_size_t) memory_properties.memoryTypeCount; ++memory_type_index)
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

    for (kan_loop_size_t heap_index = 0u; heap_index < (kan_loop_size_t) memory_properties.memoryHeapCount;
         ++heap_index)
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
    kan_instance_size_t physical_device_count;
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
    system->supported_devices->supported_device_count = (kan_loop_size_t) physical_device_count;

    for (kan_loop_size_t device_index = 0u; device_index < physical_device_count; ++device_index)
    {
        struct kan_render_supported_device_info_t *device_info = &system->supported_devices->devices[device_index];
        _Static_assert (sizeof (kan_render_device_t) >= sizeof (VkPhysicalDevice),
                        "Can store Vulkan handle in Kan id.");
        device_info->id = KAN_HANDLE_SET (kan_render_device_t, physical_devices[device_index]);
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
        VkPhysicalDeviceFeatures device_features;
        vkGetPhysicalDeviceFeatures (physical_devices[device_index], &device_features);
        device_info->anisotropy_supported = device_features.samplerAnisotropy;
        device_info->anisotropy_max = device_properties.limits.maxSamplerAnisotropy;

        for (kan_loop_size_t format = 0u; format < KAN_RENDER_IMAGE_FORMAT_COUNT; ++format)
        {
            device_info->image_format_support[format] = 0u;
            VkFormatProperties format_properties;
            vkGetPhysicalDeviceFormatProperties (physical_devices[device_index], image_format_to_vulkan (format),
                                                 &format_properties);

            const vulkan_size_t transfer_mask = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
            if ((format_properties.optimalTilingFeatures & transfer_mask) == transfer_mask)
            {
                device_info->image_format_support[format] |= KAN_RENDER_IMAGE_FORMAT_SUPPORT_FLAG_TRANSFER;
            }

            const vulkan_size_t sampled_mask = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
            if ((format_properties.optimalTilingFeatures & sampled_mask) == sampled_mask)
            {
                device_info->image_format_support[format] |= KAN_RENDER_IMAGE_FORMAT_SUPPORT_FLAG_SAMPLED;
            }

            const vulkan_size_t color_render_mask = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                                                    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
                                                    VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;

            if ((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ||
                (format_properties.optimalTilingFeatures & color_render_mask) == color_render_mask)
            {
                device_info->image_format_support[format] |= KAN_RENDER_IMAGE_FORMAT_SUPPORT_FLAG_RENDER;
            }
        }
    }

    kan_free_general (system->utility_allocation_group, physical_devices,
                      sizeof (VkPhysicalDevice) * physical_device_count);
}

void render_backend_system_init (kan_context_system_t handle)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (handle);
    if (!kan_platform_application_register_vulkan_library_usage ())
    {
        kan_error_critical ("Failed to register vulkan library usage, unable to continue properly.", __FILE__,
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
    for (kan_loop_size_t index = 0u; index < extensions.size; ++index)
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
        .enabledExtensionCount = (vulkan_size_t) extensions.size,
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

    kan_instance_size_t layer_properties_count;
    vkEnumerateInstanceLayerProperties (&layer_properties_count, NULL);

    VkLayerProperties *layer_properties =
        kan_allocate_general (system->utility_allocation_group, sizeof (VkLayerProperties) * layer_properties_count,
                              _Alignof (VkLayerProperties));
    vkEnumerateInstanceLayerProperties (&layer_properties_count, layer_properties);
    system->has_validation_layer = KAN_FALSE;

    for (kan_loop_size_t index = 0u; index < layer_properties_count; ++index)
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
        kan_error_critical ("Failed to create Vulkan instance.", __FILE__, __LINE__);
    }

    volkLoadInstance (system->instance);
    for (kan_loop_size_t index = 0u; index < extensions.size; ++index)
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
    for (kan_loop_size_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        struct render_backend_schedule_state_t *state = &system->schedule_states[index];
        kan_stack_group_allocator_shutdown (&state->item_allocator);
    }
}

static void render_backend_system_destroy_command_states (struct render_backend_system_t *system)
{
    for (kan_loop_size_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
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
                                      (vulkan_size_t) state->secondary_command_buffers.size,
                                      (VkCommandBuffer *) state->secondary_command_buffers.data);
            }

            vkDestroyCommandPool (system->device, state->command_pool, VULKAN_ALLOCATION_CALLBACKS (system));
        }

        kan_dynamic_array_shutdown (&state->secondary_command_buffers);
    }
}

static void render_backend_system_destroy_synchronization_objects (struct render_backend_system_t *system)
{
    for (kan_loop_size_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
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

void render_backend_system_shutdown (kan_context_system_t handle)
{
}

void render_backend_system_disconnect (kan_context_system_t handle)
{
}

void render_backend_system_destroy (kan_context_system_t handle)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (handle);
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
    for (kan_loop_size_t schedule_index = 0u; schedule_index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT;
         ++schedule_index)
    {
        struct render_backend_schedule_state_t *schedule = &system->schedule_states[schedule_index];
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

        struct render_backend_read_back_status_t *status = schedule->first_read_back_status;
        while (status)
        {
            struct render_backend_read_back_status_t *next = status->next;
            kan_free_batched (system->read_back_status_allocation_group, status);
            status = next;
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

    struct render_backend_pipeline_layout_t *pipeline_layout =
        (struct render_backend_pipeline_layout_t *) system->pipeline_layouts.items.first;

    while (pipeline_layout)
    {
        struct render_backend_pipeline_layout_t *next =
            (struct render_backend_pipeline_layout_t *) pipeline_layout->node.list_node.next;
        render_backend_system_destroy_pipeline_layout (system, pipeline_layout);
        pipeline_layout = next;
    }

    kan_hash_storage_shutdown (&system->pipeline_layouts);
    struct render_backend_code_module_t *code_module =
        (struct render_backend_code_module_t *) system->code_modules.first;

    while (code_module)
    {
        struct render_backend_code_module_t *next = (struct render_backend_code_module_t *) code_module->list_node.next;
        render_backend_system_destroy_code_module (system, code_module);
        code_module = next;
    }

    struct render_backend_pipeline_parameter_set_layout_t *parameter_set_layout =
        (struct render_backend_pipeline_parameter_set_layout_t *) system->pipeline_parameter_set_layouts.items.first;

    while (parameter_set_layout)
    {
        struct render_backend_pipeline_parameter_set_layout_t *next =
            (struct render_backend_pipeline_parameter_set_layout_t *) parameter_set_layout->node.list_node.next;
        render_backend_system_destroy_pipeline_parameter_set_layout (system, parameter_set_layout);
        parameter_set_layout = next;
    }

    kan_hash_storage_shutdown (&system->pipeline_parameter_set_layouts);
    vkDestroyDescriptorSetLayout (system->device, system->empty_descriptor_set_layout,
                                  VULKAN_ALLOCATION_CALLBACKS (system));

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

    struct render_backend_cached_sampler_t *cached_sampler = system->first_cached_sampler;
    while (cached_sampler)
    {
        struct render_backend_cached_sampler_t *next = cached_sampler->next;
        vkDestroySampler (system->device, cached_sampler->sampler, VULKAN_ALLOCATION_CALLBACKS (system));
        kan_free_batched (system->cached_samplers_allocation_group, cached_sampler);
        cached_sampler = next;
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
    kan_free_general (system->main_allocation_group, system, sizeof (struct render_backend_system_t));
}

CONTEXT_RENDER_BACKEND_SYSTEM_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (
    render_backend_system_t) = {
    .name = KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME,
    .create = render_backend_system_create,
    .connect = render_backend_system_connect,
    .connected_init = render_backend_system_init,
    .connected_shutdown = render_backend_system_shutdown,
    .disconnect = render_backend_system_disconnect,
    .destroy = render_backend_system_destroy,
};

struct kan_render_supported_devices_t *kan_render_backend_system_get_devices (
    kan_context_system_t render_backend_system)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (render_backend_system);
    return system->supported_devices;
}

kan_bool_t kan_render_backend_system_select_device (kan_context_system_t render_backend_system,
                                                    kan_render_device_t device)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (render_backend_system);
    VkPhysicalDevice physical_device = (VkPhysicalDevice) KAN_HANDLE_GET (device);

    if (system->device)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Caught attempt to select device after device was already sucessfully selected!")
        return KAN_FALSE;
    }

    struct kan_render_supported_device_info_t *device_info = NULL;
    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) system->supported_devices->supported_device_count;
         ++index)
    {
        if (KAN_HANDLE_IS_EQUAL (system->supported_devices->devices[index].id, device))
        {
            device_info = &system->supported_devices->devices[index];
            break;
        }
    }

    if (!device_info)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Caught attempt to select device which is not listed in supported devices list!")
        return KAN_FALSE;
    }

    kan_instance_size_t properties_count;
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
    for (vulkan_size_t index = 0u; index < properties_count; ++index)
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

    kan_instance_size_t queues_count;
    vkGetPhysicalDeviceQueueFamilyProperties (physical_device, &queues_count, NULL);
    VkQueueFamilyProperties *queues =
        kan_allocate_general (system->utility_allocation_group, sizeof (VkQueueFamilyProperties) * queues_count,
                              _Alignof (VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties (physical_device, &queues_count, queues);

    system->device_queue_family_index = UINT32_MAX;
    for (vulkan_size_t index = 0u; index < queues_count; ++index)
    {
        const vulkan_size_t mask = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT;
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

    float queues_priorities = 0.0f;
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

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    {
        struct VkDebugUtilsObjectNameInfoEXT object_name = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = NULL,
            .objectType = VK_OBJECT_TYPE_INSTANCE,
            .objectHandle = CONVERT_HANDLE_FOR_DEBUG (kan_memory_size_t) system->instance,
            .pObjectName = "RenderContextInstance",
        };

        vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
    }
    {
        struct VkDebugUtilsObjectNameInfoEXT object_name = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = NULL,
            .objectType = VK_OBJECT_TYPE_DEVICE,
            .objectHandle = CONVERT_HANDLE_FOR_DEBUG (kan_memory_size_t) system->device,
            .pObjectName = "LogicalDevice",
        };

        vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
    }
#endif

    system->device_memory_type = query_device_memory_type (physical_device);
    system->physical_device = physical_device;

    volkLoadDevice (system->device);
    vkGetDeviceQueue (system->device, system->device_queue_family_index, 0u, &system->device_queue);

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    {
        struct VkDebugUtilsObjectNameInfoEXT object_name = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = NULL,
            .objectType = VK_OBJECT_TYPE_QUEUE,
            .objectHandle = CONVERT_HANDLE_FOR_DEBUG (kan_memory_size_t) system->device_queue,
            .pObjectName = "Queue::merged_queue",
        };

        vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
    }
#endif

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

    for (kan_loop_size_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        system->render_finished_semaphores[index] = VK_NULL_HANDLE;
        system->in_flight_fences[index] = VK_NULL_HANDLE;
        system->present_skipped_flags[index] = KAN_FALSE;
    }

    kan_bool_t synchronization_objects_created = KAN_TRUE;
    for (kan_loop_size_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
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
            snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME,
                      "Semaphore::render_finished::frame%lu", (unsigned long) index);

            struct VkDebugUtilsObjectNameInfoEXT object_name = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext = NULL,
                .objectType = VK_OBJECT_TYPE_SEMAPHORE,
                .objectHandle = CONVERT_HANDLE_FOR_DEBUG system->render_finished_semaphores[index],
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
            snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "Fence::in_flight::frame%lu",
                      (unsigned long) index);

            struct VkDebugUtilsObjectNameInfoEXT object_name = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext = NULL,
                .objectType = VK_OBJECT_TYPE_FENCE,
                .objectHandle = CONVERT_HANDLE_FOR_DEBUG system->in_flight_fences[index],
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

    for (kan_loop_size_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        system->command_states[index].command_pool = VK_NULL_HANDLE;
    }

    kan_bool_t command_states_created = KAN_TRUE;
    for (kan_loop_size_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
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
            snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "CommandPool::primary::frame%lu",
                      (unsigned long) index);

            struct VkDebugUtilsObjectNameInfoEXT object_name = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext = NULL,
                .objectType = VK_OBJECT_TYPE_COMMAND_POOL,
                .objectHandle = CONVERT_HANDLE_FOR_DEBUG system->command_states[index].command_pool,
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

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .bindingCount = 0u,
        .pBindings = NULL,
    };

    if (vkCreateDescriptorSetLayout (system->device, &layout_info, VULKAN_ALLOCATION_CALLBACKS (system),
                                     &system->empty_descriptor_set_layout) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to select device: failed to create empty descriptor set layout.")

        render_backend_system_destroy_command_states (system);
        render_backend_system_destroy_synchronization_objects (system);
        vmaDestroyAllocator (system->gpu_memory_allocator);
        vkDestroyDevice (system->device, VULKAN_ALLOCATION_CALLBACKS (system));
        system->device = VK_NULL_HANDLE;
        return KAN_FALSE;
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    struct VkDebugUtilsObjectNameInfoEXT object_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = NULL,
        .objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
        .objectHandle = CONVERT_HANDLE_FOR_DEBUG system->empty_descriptor_set_layout,
        .pObjectName = "DescriptorSetLayout::empty",
    };

    vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
#endif

    for (kan_loop_size_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        struct render_backend_schedule_state_t *state = &system->schedule_states[index];
        kan_stack_group_allocator_init (&state->item_allocator, system->schedule_allocation_group,
                                        KAN_CONTEXT_RENDER_BACKEND_VULKAN_SCHEDULE_STACK_SIZE);
        state->schedule_lock = kan_atomic_int_init (0);

        state->first_scheduled_buffer_flush_transfer = NULL;
        state->first_scheduled_buffer_flush = NULL;
        state->first_scheduled_image_upload = NULL;
        state->first_scheduled_image_mip_generation = NULL;
        state->first_scheduled_image_copy_data = NULL;
        state->first_scheduled_frame_end_buffer_read_back = NULL;
        state->first_scheduled_frame_end_image_read_back = NULL;
        state->first_scheduled_frame_end_surface_blit = NULL;

        state->first_scheduled_frame_buffer_destroy = NULL;
        state->first_scheduled_pass_destroy = NULL;
        state->first_scheduled_pipeline_parameter_set_destroy = NULL;
        state->first_scheduled_detached_descriptor_set_destroy = NULL;
        state->first_scheduled_graphics_pipeline_destroy = NULL;
        state->first_scheduled_pipeline_parameter_set_layout_destroy = NULL;
        state->first_scheduled_buffer_destroy = NULL;
        state->first_scheduled_frame_lifetime_allocator_destroy = NULL;
        state->first_scheduled_detached_image_view_destroy = NULL;
        state->first_scheduled_image_destroy = NULL;

        state->first_read_back_status = NULL;
    }

    system->staging_frame_lifetime_allocator = render_backend_system_create_frame_lifetime_allocator (
        system, RENDER_BACKEND_BUFFER_FAMILY_STAGING,
        // Buffer type does not matter anything for staging.
        KAN_RENDER_BUFFER_TYPE_STORAGE, KAN_CONTEXT_RENDER_BACKEND_VULKAN_STAGING_PAGE_SIZE,
        kan_string_intern ("default_staging_buffer"));

    system->selected_device_info = device_info;
    vkGetPhysicalDeviceMemoryProperties (physical_device, &system->selected_device_memory_properties);
    return KAN_TRUE;
}

struct kan_render_supported_device_info_t *kan_render_backend_system_get_selected_device_info (
    kan_context_system_t render_backend_system)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (render_backend_system);
    return system->selected_device_info;
}

kan_render_context_t kan_render_backend_system_get_render_context (kan_context_system_t render_backend_system)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (render_backend_system);
    return KAN_HANDLE_SET (kan_render_context_t, system);
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
        kan_error_critical ("Failed to start recording primary buffer.", __FILE__, __LINE__);
    }
}

static void render_backend_system_submit_transfer (struct render_backend_system_t *system)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_submit_transfer);

    struct render_backend_command_state_t *state = &system->command_states[system->current_frame_in_flight_index];
    DEBUG_LABEL_SCOPE_BEGIN (state->primary_command_buffer, "buffer_transfer", DEBUG_LABEL_COLOR_PASS)

    struct render_backend_schedule_state_t *schedule = &system->schedule_states[system->current_frame_in_flight_index];
    struct scheduled_buffer_flush_transfer_t *buffer_flush_transfer = schedule->first_scheduled_buffer_flush_transfer;
    schedule->first_scheduled_buffer_flush_transfer = NULL;

    while (buffer_flush_transfer)
    {
        if (buffer_flush_transfer->source_buffer->needs_flush)
        {
            if (vmaFlushAllocation (system->gpu_memory_allocator, buffer_flush_transfer->source_buffer->allocation,
                                    buffer_flush_transfer->source_offset, buffer_flush_transfer->size) != VK_SUCCESS)
            {
                kan_error_critical ("Unexpected failure while flushing buffer data, unable to continue properly.",
                                    __FILE__, __LINE__);
            }
        }

        struct VkBufferCopy region = {
            .srcOffset = buffer_flush_transfer->source_offset,
            .dstOffset = buffer_flush_transfer->target_offset,
            .size = buffer_flush_transfer->size,
        };

        vkCmdCopyBuffer (state->primary_command_buffer, buffer_flush_transfer->source_buffer->buffer,
                         buffer_flush_transfer->target_buffer->buffer, 1u, &region);

        VkAccessFlags destination_access_flags = 0u;
        VkPipelineStageFlags destination_stage = 0u;

        switch (buffer_flush_transfer->target_buffer->type)
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

        case KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE:
            // Read back buffer cannot be target of transfer to the GPU.
            KAN_ASSERT (KAN_FALSE)
            destination_access_flags = 0u;
            destination_stage = 0u;
            break;
        }

        VkBufferMemoryBarrier memory_barrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = destination_access_flags,
            .srcQueueFamilyIndex = system->device_queue_family_index,
            .dstQueueFamilyIndex = system->device_queue_family_index,
            .buffer = buffer_flush_transfer->target_buffer->buffer,
            .offset = region.dstOffset,
            .size = region.size,
        };

        vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, destination_stage, 0u, 0u,
                              NULL, 1u, &memory_barrier, 0u, NULL);
        buffer_flush_transfer = buffer_flush_transfer->next;
    }

    DEBUG_LABEL_SCOPE_END (state->primary_command_buffer)
    DEBUG_LABEL_SCOPE_BEGIN (state->primary_command_buffer, "buffer_flush", DEBUG_LABEL_COLOR_PASS)

    struct scheduled_buffer_flush_t *buffer_flush = schedule->first_scheduled_buffer_flush;
    schedule->first_scheduled_buffer_flush = NULL;

    while (buffer_flush)
    {
        if (buffer_flush->buffer->needs_flush)
        {
            if (vmaFlushAllocation (system->gpu_memory_allocator, buffer_flush->buffer->allocation,
                                    buffer_flush->offset, buffer_flush->size) != VK_SUCCESS)
            {
                kan_error_critical ("Unexpected failure while flushing buffer data, unable to continue properly.",
                                    __FILE__, __LINE__);
            }
        }

        buffer_flush = buffer_flush->next;
    }

    DEBUG_LABEL_SCOPE_END (state->primary_command_buffer)
    DEBUG_LABEL_SCOPE_BEGIN (state->primary_command_buffer, "image_upload", DEBUG_LABEL_COLOR_PASS)

    struct scheduled_image_upload_t *image_upload = schedule->first_scheduled_image_upload;
    schedule->first_scheduled_image_upload = NULL;

    while (image_upload)
    {
        VkImageAspectFlags image_aspect = get_image_aspects (&image_upload->image->description);

        vulkan_size_t width;
        vulkan_size_t height;
        vulkan_size_t depth;
        kan_render_image_description_calculate_size_at_mip (&image_upload->image->description, image_upload->mip,
                                                            &width, &height, &depth);

        VkImageMemoryBarrier prepare_transfer_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = 0u,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = system->device_queue_family_index,
            .dstQueueFamilyIndex = system->device_queue_family_index,
            .image = image_upload->image->image,
            .subresourceRange =
                {
                    .aspectMask = image_aspect,
                    .baseMipLevel = (vulkan_size_t) image_upload->mip,
                    .levelCount = 1u,
                    .baseArrayLayer = image_upload->layer,
                    .layerCount = 1u,
                },
        };

        vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &prepare_transfer_barrier);

        VkBufferImageCopy copy_region = {
            .bufferOffset = (vulkan_size_t) image_upload->staging_buffer_offset,
            .bufferRowLength = 0u,
            .bufferImageHeight = 0u,
            .imageSubresource =
                {
                    .aspectMask = image_aspect,
                    .mipLevel = (vulkan_size_t) image_upload->mip,
                    .baseArrayLayer = image_upload->layer,
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
                    .width = (vulkan_size_t) width,
                    .height = (vulkan_size_t) height,
                    .depth = (vulkan_size_t) depth,
                },
        };

        vkCmdCopyBufferToImage (state->primary_command_buffer, image_upload->staging_buffer->buffer,
                                image_upload->image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copy_region);

        VkImageMemoryBarrier finish_transfer_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = system->device_queue_family_index,
            .dstQueueFamilyIndex = system->device_queue_family_index,
            .image = image_upload->image->image,
            .subresourceRange =
                {
                    .aspectMask = image_aspect,
                    .baseMipLevel = (vulkan_size_t) image_upload->mip,
                    .levelCount = 1u,
                    .baseArrayLayer = image_upload->layer,
                    .layerCount = 1u,
                },
        };

        vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u,
                              NULL, 1u, &finish_transfer_barrier);

        set_image_layout_info (image_upload->image, image_upload->layer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        image_upload = image_upload->next;
    }

    struct scheduled_image_copy_data_t *image_copy = schedule->first_scheduled_image_copy_data;
    schedule->first_scheduled_image_copy_data = NULL;

    while (image_copy)
    {
        VkImageAspectFlags image_aspect = get_image_aspects (&image_copy->from_image->description);

        vulkan_size_t width;
        vulkan_size_t height;
        vulkan_size_t depth;
        kan_render_image_description_calculate_size_at_mip (&image_copy->from_image->description, image_copy->from_mip,
                                                            &width, &height, &depth);

        VkImageMemoryBarrier prepare_transfer_barriers[] = {
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = 0u,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .oldLayout = get_image_layout_info (image_copy->from_image, image_copy->from_layer),
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcQueueFamilyIndex = system->device_queue_family_index,
                .dstQueueFamilyIndex = system->device_queue_family_index,
                .image = image_copy->from_image->image,
                .subresourceRange =
                    {
                        .aspectMask = image_aspect,
                        .baseMipLevel = (vulkan_size_t) image_copy->from_mip,
                        .levelCount = 1u,
                        .baseArrayLayer = (vulkan_size_t) image_copy->from_layer,
                        .layerCount = 1u,
                    },
            },
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = 0u,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = system->device_queue_family_index,
                .dstQueueFamilyIndex = system->device_queue_family_index,
                .image = image_copy->to_image->image,
                .subresourceRange =
                    {
                        .aspectMask = image_aspect,
                        .baseMipLevel = (vulkan_size_t) image_copy->to_mip,
                        .levelCount = 1u,
                        .baseArrayLayer = (vulkan_size_t) image_copy->to_layer,
                        .layerCount = 1u,
                    },
            },
        };

        vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u, NULL,
                              sizeof (prepare_transfer_barriers) / sizeof (prepare_transfer_barriers[0u]),
                              prepare_transfer_barriers);

        VkImageCopy copy_region = {
            .srcSubresource =
                {
                    .aspectMask = image_aspect,
                    .mipLevel = (vulkan_size_t) image_copy->from_mip,
                    .baseArrayLayer = (vulkan_size_t) image_copy->from_layer,
                    .layerCount = 1u,
                },
            .srcOffset =
                {
                    .x = 0u,
                    .y = 0u,
                    .z = 0u,
                },
            .dstSubresource =
                {
                    .aspectMask = image_aspect,
                    .mipLevel = (vulkan_size_t) image_copy->to_mip,
                    .baseArrayLayer = (vulkan_size_t) image_copy->to_layer,
                    .layerCount = 1u,
                },
            .dstOffset =
                {
                    .x = 0u,
                    .y = 0u,
                    .z = 0u,
                },
            .extent =
                {
                    .width = (vulkan_size_t) width,
                    .height = (vulkan_size_t) height,
                    .depth = (vulkan_size_t) depth,
                },
        };

        vkCmdCopyImage (state->primary_command_buffer, image_copy->from_image->image,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image_copy->to_image->image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copy_region);

        VkImageMemoryBarrier finish_transfer_barriers[] = {
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamilyIndex = system->device_queue_family_index,
                .dstQueueFamilyIndex = system->device_queue_family_index,
                .image = image_copy->from_image->image,
                .subresourceRange =
                    {
                        .aspectMask = image_aspect,
                        .baseMipLevel = (vulkan_size_t) image_copy->from_mip,
                        .levelCount = 1u,
                        .baseArrayLayer = (vulkan_size_t) image_copy->from_layer,
                        .layerCount = 1u,
                    },
            },
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamilyIndex = system->device_queue_family_index,
                .dstQueueFamilyIndex = system->device_queue_family_index,
                .image = image_copy->to_image->image,
                .subresourceRange =
                    {
                        .aspectMask = image_aspect,
                        .baseMipLevel = (vulkan_size_t) image_copy->to_mip,
                        .levelCount = 1u,
                        .baseArrayLayer = (vulkan_size_t) image_copy->to_layer,
                        .layerCount = 1u,
                    },
            },
        };

        vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u,
                              NULL, sizeof (finish_transfer_barriers) / sizeof (finish_transfer_barriers[0u]),
                              finish_transfer_barriers);

        set_image_layout_info (image_copy->from_image, image_copy->from_layer,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        set_image_layout_info (image_copy->to_image, image_copy->to_layer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        image_copy = image_copy->next;
    }

    DEBUG_LABEL_SCOPE_END (state->primary_command_buffer)
    kan_cpu_section_execution_shutdown (&execution);
}

static inline void submit_mip_generation (struct render_backend_system_t *system,
                                          struct render_backend_schedule_state_t *schedule,
                                          struct render_backend_command_state_t *state)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_submit_mip_generation);
    struct scheduled_image_mip_generation_t *image_mip_generation = schedule->first_scheduled_image_mip_generation;

    while (image_mip_generation)
    {
        VkImageAspectFlags image_aspect = get_image_aspects (&image_mip_generation->image->description);

        for (uint8_t output_mip = image_mip_generation->first + 1u; output_mip <= image_mip_generation->last;
             ++output_mip)
        {
            const uint8_t input_mip = output_mip - 1u;
            vulkan_size_t input_width;
            vulkan_size_t input_height;
            vulkan_size_t input_depth;
            kan_render_image_description_calculate_size_at_mip (&image_mip_generation->image->description, input_mip,
                                                                &input_width, &input_height, &input_depth);

            vulkan_size_t output_width;
            vulkan_size_t output_height;
            vulkan_size_t output_depth;
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
                        .baseMipLevel = (vulkan_size_t) input_mip,
                        .levelCount = 1u,
                        .baseArrayLayer = (vulkan_size_t) image_mip_generation->layer,
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
                        .baseMipLevel = (vulkan_size_t) output_mip,
                        .levelCount = 1u,
                        .baseArrayLayer = (vulkan_size_t) image_mip_generation->layer,
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
                        .mipLevel = (vulkan_size_t) input_mip,
                        .baseArrayLayer = (vulkan_size_t) image_mip_generation->layer,
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
                            .x = (vulkan_offset_t) input_width,
                            .y = (vulkan_offset_t) input_height,
                            .z = (vulkan_offset_t) input_depth,
                        },
                    },
                .dstSubresource =
                    {
                        .aspectMask = image_aspect,
                        .mipLevel = (vulkan_size_t) output_mip,
                        .baseArrayLayer = (vulkan_size_t) image_mip_generation->layer,
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
                            .x = (vulkan_offset_t) output_width,
                            .y = (vulkan_offset_t) output_height,
                            .z = (vulkan_offset_t) output_depth,
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
                        .baseMipLevel = (vulkan_size_t) input_mip,
                        .levelCount = 1u,
                        .baseArrayLayer = (vulkan_size_t) image_mip_generation->layer,
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
                    .baseMipLevel = (vulkan_size_t) image_mip_generation->last,
                    .levelCount = 1u,
                    .baseArrayLayer = (vulkan_size_t) image_mip_generation->layer,
                    .layerCount = 1u,
                },
        };

        vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &last_to_read_only_barrier);

        set_image_layout_info (image_mip_generation->image, image_mip_generation->layer,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        image_mip_generation = image_mip_generation->next;
    }

    schedule->first_scheduled_image_mip_generation = NULL;
    kan_cpu_section_execution_shutdown (&execution);
}

static VkAccessFlags calculate_surface_access_mask (struct render_backend_surface_t *surface)
{
    switch (surface->current_frame_layout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        // No access this frame.
        return 0u;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_ACCESS_TRANSFER_WRITE_BIT;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_ACCESS_TRANSFER_READ_BIT;

    default:
        // We do not expect other layouts on surfaces.
        KAN_ASSERT (KAN_FALSE)
        return 0u;
    }

    KAN_ASSERT (KAN_FALSE)
    return 0u;
}

static VkAccessFlags calculate_surface_source_stage (struct render_backend_surface_t *surface)
{
    switch (surface->current_frame_layout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;

    default:
        // We do not expect other layouts on surfaces.
        KAN_ASSERT (KAN_FALSE)
        return 0u;
    }

    KAN_ASSERT (KAN_FALSE)
    return 0u;
}

static void process_surface_blit_requests (struct render_backend_system_t *system,
                                           struct render_backend_command_state_t *state,
                                           struct scheduled_surface_blit_request_t *first_request)
{
    if (!first_request)
    {
        return;
    }

    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_submit_blit_requests);

    VkImageMemoryBarrier image_barriers_static[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_BARRIERS];
    VkImageMemoryBarrier *image_barriers = image_barriers_static;

    struct scheduled_surface_blit_request_t *request = first_request;
    kan_loop_size_t requests_count = 0u;

    while (request)
    {
        ++requests_count;
        request = request->next;
    }

    if (requests_count * 2u > KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_BARRIERS)
    {
        // We over-allocate barriers to be sure that we won't need to reallocate and copy them.
        image_barriers =
            kan_allocate_general (system->utility_allocation_group, sizeof (VkImageMemoryBarrier) * requests_count * 2u,
                                  _Alignof (VkImageMemoryBarrier));
    }

    VkPipelineStageFlags source_stage_flags =
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    vulkan_size_t added_barriers = 0u;
    request = first_request;

    while (request)
    {
        VkImageLayout image_old_layout = get_image_layout_info (request->image, request->image_layer);
        if (image_old_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            image_barriers[added_barriers] = (VkImageMemoryBarrier) {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .oldLayout = image_old_layout,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcQueueFamilyIndex = system->device_queue_family_index,
                .dstQueueFamilyIndex = system->device_queue_family_index,
                .image = request->image->image,
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0u,
                        .levelCount = 1u,
                        .baseArrayLayer = request->image_layer,
                        .layerCount = 1u,
                    },
            };

            ++added_barriers;
            set_image_layout_info (request->image, request->image_layer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        }

        // Prepare destination surface image.
        if (request->surface->current_frame_layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            image_barriers[added_barriers] = (VkImageMemoryBarrier) {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = calculate_surface_access_mask (request->surface),
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout = request->surface->current_frame_layout,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = system->device_queue_family_index,
                .dstQueueFamilyIndex = system->device_queue_family_index,
                .image = request->surface->images[request->surface->acquired_image_index],
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0u,
                        .levelCount = 1u,
                        .baseArrayLayer = 0u,
                        .layerCount = 1u,
                    },
            };

            source_stage_flags |= calculate_surface_source_stage (request->surface);
            ++added_barriers;
            request->surface->current_frame_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }

        request = request->next;
    }

    vkCmdPipelineBarrier (state->primary_command_buffer, source_stage_flags, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                          NULL, 0u, NULL, added_barriers, image_barriers);
    request = first_request;

    while (request)
    {
        VkImageBlit image_blit = {
            .srcSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0u,
                    .baseArrayLayer = request->image_layer,
                    .layerCount = 1u,
                },
            .srcOffsets =
                {
                    {
                        .x = (vulkan_offset_t) request->image_region.x,
                        .y = (vulkan_offset_t) request->image_region.y,
                        .z = 0,
                    },
                    {
                        .x = (vulkan_offset_t) request->image_region.x + (vulkan_offset_t) request->image_region.width,
                        .y = (vulkan_offset_t) request->image_region.y + (vulkan_offset_t) request->image_region.height,
                        .z = 1,
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
                        .x = (vulkan_offset_t) request->surface_region.x,
                        .y = (vulkan_offset_t) request->surface_region.y,
                        .z = 0,
                    },
                    {
                        .x = (vulkan_offset_t) request->surface_region.x +
                             (vulkan_offset_t) request->surface_region.width,
                        .y = (vulkan_offset_t) request->surface_region.y +
                             (vulkan_offset_t) request->surface_region.height,
                        .z = 1,
                    },
                },
        };

        // We silently skip blits with incorrect regions. Regions should be properly validated during blit request,
        // but resize may happen between blit request submission and command generation, which would make the blit
        // invalid for one frame. Therefore, we should skip blits in such corner cases.
        if (image_blit.dstOffsets[0u].x >= 0 && image_blit.dstOffsets[0u].y >= 0 &&
            image_blit.dstOffsets[1u].x <= (kan_render_offset_t) request->surface->swap_chain_creation_window_width &&
            image_blit.dstOffsets[1u].y <= (kan_render_offset_t) request->surface->swap_chain_creation_window_height)
        {
            vkCmdBlitImage (state->primary_command_buffer, request->image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            request->surface->images[request->surface->acquired_image_index],
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &image_blit, VK_FILTER_LINEAR);
        }

        request = request->next;
    }

    request = first_request;
    added_barriers = 0u;

    // Return images to shader readable format if needed.
    while (request)
    {
        VkImageLayout image_old_layout = get_image_layout_info (request->image, request->image_layer);
        if (request->image->description.supports_sampling && image_old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            image_barriers[added_barriers] = (VkImageMemoryBarrier) {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .dstAccessMask = 0u,
                .oldLayout = image_old_layout,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamilyIndex = system->device_queue_family_index,
                .dstQueueFamilyIndex = system->device_queue_family_index,
                .image = request->image->image,
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0u,
                        .levelCount = 1u,
                        .baseArrayLayer = request->image_layer,
                        .layerCount = 1u,
                    },
            };

            ++added_barriers;
            set_image_layout_info (request->image, request->image_layer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        request = request->next;
    }

    vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, NULL, 0u, NULL, added_barriers,
                          image_barriers);

    if (image_barriers != image_barriers_static)
    {
        kan_free_general (system->utility_allocation_group, image_barriers,
                          sizeof (VkImageMemoryBarrier) * requests_count * 2u);
    }

    kan_cpu_section_execution_shutdown (&execution);
}

static void process_read_back_requests (struct render_backend_system_t *system,
                                        struct render_backend_command_state_t *state,
                                        struct scheduled_buffer_read_back_t *first_buffer_read_back,
                                        struct scheduled_image_read_back_t *first_image_read_back)
{
    if (!first_buffer_read_back && !first_image_read_back)
    {
        return;
    }

    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_submit_read_back);

    DEBUG_LABEL_SCOPE_BEGIN (state->primary_command_buffer, "read_back", DEBUG_LABEL_COLOR_PASS)
    kan_instance_size_t image_barriers_needed = 0u;
    kan_instance_size_t buffer_barriers_needed = 0u;

    struct scheduled_buffer_read_back_t *buffer_read_back = first_buffer_read_back;
    while (buffer_read_back)
    {
        if (buffer_read_back->buffer->buffer != VK_NULL_HANDLE)
        {
            buffer_read_back->status->state = KAN_RENDER_READ_BACK_STATE_SCHEDULED;
            ++buffer_barriers_needed;
        }
        else
        {
            buffer_read_back->status->state = KAN_RENDER_READ_BACK_STATE_FAILED;
        }

        buffer_read_back = buffer_read_back->next;
    }

    struct scheduled_image_read_back_t *image_read_back = first_image_read_back;
    while (image_read_back)
    {
        if (image_read_back->image->image != VK_NULL_HANDLE &&
            image_read_back->mip < image_read_back->image->description.mips)
        {
            image_read_back->status->state = KAN_RENDER_READ_BACK_STATE_SCHEDULED;
            ++image_barriers_needed;
        }
        else
        {
            image_read_back->status->state = KAN_RENDER_READ_BACK_STATE_FAILED;
        }

        image_read_back = image_read_back->next;
    }

    // Collect barriers before read back.

    static VkBufferMemoryBarrier static_buffer_barriers[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_BARRIERS];
    static VkImageMemoryBarrier static_image_barriers[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_BARRIERS];

    VkBufferMemoryBarrier *buffer_barriers = static_buffer_barriers;
    if (buffer_barriers_needed > KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_BARRIERS)
    {
        buffer_barriers = kan_allocate_general (system->utility_allocation_group,
                                                sizeof (VkBufferMemoryBarrier) * buffer_barriers_needed,
                                                _Alignof (VkBufferMemoryBarrier));
    }

    VkImageMemoryBarrier *image_barriers = static_image_barriers;
    if (image_barriers_needed > KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_BARRIERS)
    {
        image_barriers = kan_allocate_general (system->utility_allocation_group,
                                               sizeof (VkImageMemoryBarrier) * buffer_barriers_needed,
                                               _Alignof (VkImageMemoryBarrier));
    }

    struct VkBufferMemoryBarrier *buffer_barrier_output = buffer_barriers;
    struct VkImageMemoryBarrier *image_barrier_output = image_barriers;

    buffer_read_back = first_buffer_read_back;
    while (buffer_read_back)
    {
        if (buffer_read_back->status->state == KAN_RENDER_READ_BACK_STATE_SCHEDULED)
        {
            *buffer_barrier_output = (VkBufferMemoryBarrier) {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = 0u,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .srcQueueFamilyIndex = system->device_queue_family_index,
                .dstQueueFamilyIndex = system->device_queue_family_index,
                .buffer = buffer_read_back->buffer->buffer,
                .offset = buffer_read_back->offset,
                .size = buffer_read_back->slice,
            };

            ++buffer_barrier_output;
        }

        buffer_read_back = buffer_read_back->next;
    }

    image_read_back = first_image_read_back;
    while (image_read_back)
    {
        if (image_read_back->status->state == KAN_RENDER_READ_BACK_STATE_SCHEDULED)
        {
            VkImageLayout image_old_layout = get_image_layout_info (image_read_back->image, image_read_back->layer);
            if (image_old_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
            {
                *image_barrier_output = (VkImageMemoryBarrier) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .pNext = NULL,
                    .srcAccessMask = 0u,
                    .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                    .oldLayout = image_old_layout,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .srcQueueFamilyIndex = system->device_queue_family_index,
                    .dstQueueFamilyIndex = system->device_queue_family_index,
                    .image = image_read_back->image->image,
                    .subresourceRange =
                        {
                            .aspectMask = get_image_aspects (&image_read_back->image->description),
                            .baseMipLevel = image_read_back->mip,
                            .levelCount = 1u,
                            .baseArrayLayer = 0u,
                            .layerCount = 1u,
                        },
                };

                set_image_layout_info (image_read_back->image, image_read_back->layer,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                ++image_barrier_output;
            }
        }

        image_read_back = image_read_back->next;
    }

    vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          0u, 0u, NULL, (vulkan_size_t) (buffer_barrier_output - buffer_barriers), buffer_barriers,
                          (vulkan_size_t) (image_barrier_output - image_barriers), image_barriers);

    // Execute read back.

    buffer_read_back = first_buffer_read_back;
    while (buffer_read_back)
    {
        if (buffer_read_back->status->state == KAN_RENDER_READ_BACK_STATE_SCHEDULED)
        {
            struct VkBufferCopy region = {
                .srcOffset = buffer_read_back->offset,
                .dstOffset = buffer_read_back->read_back_offset,
                .size = buffer_read_back->slice,
            };

            vkCmdCopyBuffer (state->primary_command_buffer, buffer_read_back->buffer->buffer,
                             buffer_read_back->read_back_buffer->buffer, 1u, &region);
        }

        buffer_read_back = buffer_read_back->next;
    }

    image_read_back = first_image_read_back;
    while (image_read_back)
    {
        if (image_read_back->status->state == KAN_RENDER_READ_BACK_STATE_SCHEDULED)
        {
            vulkan_size_t width;
            vulkan_size_t height;
            vulkan_size_t depth;
            kan_render_image_description_calculate_size_at_mip (&image_read_back->image->description,
                                                                image_read_back->mip, &width, &height, &depth);

            VkBufferImageCopy region = {
                .bufferOffset = image_read_back->read_back_offset,
                .bufferRowLength = 0u,
                .bufferImageHeight = 0u,
                .imageSubresource =
                    {
                        .aspectMask = get_image_aspects (&image_read_back->image->description),
                        .mipLevel = image_read_back->mip,
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
                        .width = width,
                        .height = height,
                        .depth = depth,
                    },
            };

            vkCmdCopyImageToBuffer (state->primary_command_buffer, image_read_back->image->image,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image_read_back->read_back_buffer->buffer, 1u,
                                    &region);
        }

        image_read_back = image_read_back->next;
    }

    // Change back layout of images that support sampling.

    image_barrier_output = image_barriers;
    image_read_back = first_image_read_back;

    while (image_read_back)
    {
        if (image_read_back->status->state == KAN_RENDER_READ_BACK_STATE_SCHEDULED &&
            image_read_back->image->description.supports_sampling)
        {
            *image_barrier_output = (VkImageMemoryBarrier) {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .dstAccessMask = 0u,
                .oldLayout = get_image_layout_info (image_read_back->image, image_read_back->layer),
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamilyIndex = system->device_queue_family_index,
                .dstQueueFamilyIndex = system->device_queue_family_index,
                .image = image_read_back->image->image,
                .subresourceRange =
                    {
                        .aspectMask = get_image_aspects (&image_read_back->image->description),
                        .baseMipLevel = image_read_back->mip,
                        .levelCount = 1u,
                        .baseArrayLayer = 0u,
                        .layerCount = 1u,
                    },
            };

            set_image_layout_info (image_read_back->image, image_read_back->layer,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            ++image_barrier_output;
        }

        image_read_back = image_read_back->next;
    }

    if (image_barrier_output != image_barriers)
    {
        vkCmdPipelineBarrier (state->primary_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, NULL, 0u, NULL,
                              (vulkan_size_t) (image_barrier_output - image_barriers), image_barriers);
    }

    if (buffer_barriers != static_buffer_barriers)
    {
        kan_free_general (system->utility_allocation_group, buffer_barriers,
                          sizeof (VkBufferMemoryBarrier) * buffer_barriers_needed);
    }

    if (image_barriers != static_image_barriers)
    {
        kan_free_general (system->utility_allocation_group, image_barriers,
                          sizeof (VkImageMemoryBarrier) * buffer_barriers_needed);
    }

    DEBUG_LABEL_SCOPE_END (state->primary_command_buffer)
    kan_cpu_section_execution_shutdown (&execution);
}

static void propagate_pass_instance_dependency_through_checkpoints (
    struct render_backend_pass_instance_t *pass_instance, struct render_backend_pass_instance_checkpoint_t *checkpoint)
{
    struct render_backend_pass_instance_instance_dependency_t *dependant_instance =
        checkpoint->first_dependant_instance;

    while (dependant_instance)
    {
        render_backend_pass_instance_add_dependency_internal (dependant_instance->dependant_pass_instance,
                                                              pass_instance);
        dependant_instance = dependant_instance->next;
    }

    struct render_backend_pass_instance_checkpoint_dependency_t *dependant_checkpoint =
        checkpoint->first_dependant_checkpoint;

    while (dependant_checkpoint)
    {
        propagate_pass_instance_dependency_through_checkpoints (pass_instance,
                                                                dependant_checkpoint->dependant_checkpoint);
        dependant_checkpoint = dependant_checkpoint->next;
    }
}

static inline void execute_pass_instance_submission (struct render_backend_system_t *system,
                                                     struct render_backend_command_state_t *state,
                                                     struct render_backend_pass_instance_t *pass_instance)
{
    // We put lots of barrier here in order to be sure that everything works properly.
    // It might not be the best from performance point of view, might need investigation later.

    VkImageMemoryBarrier image_barriers_static[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_BARRIERS];
    VkImageMemoryBarrier *image_barriers = image_barriers_static;
    VkPipelineStageFlags source_stage_flags =
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    vulkan_size_t added_barriers = 0u;

    if (pass_instance->frame_buffer->attachments_count > KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_BARRIERS)
    {
        image_barriers =
            kan_allocate_general (system->utility_allocation_group,
                                  sizeof (VkImageMemoryBarrier) * pass_instance->frame_buffer->attachments_count,
                                  _Alignof (VkImageMemoryBarrier));
    }

    for (kan_loop_size_t attachment_index = 0u; attachment_index < pass_instance->frame_buffer->attachments_count;
         ++attachment_index)
    {
        struct render_backend_frame_buffer_attachment_t *attachment =
            &pass_instance->frame_buffer->attachments[attachment_index];

        VkImageLayout target_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        // Currently we include everything possible into possible access flags, which is not optimal.
        VkAccessFlags possible_access_flags = 0u;
        VkAccessFlags target_access_flags = 0u;

        switch (get_image_format_class (attachment->image->description.format))
        {
        case IMAGE_FORMAT_CLASS_COLOR:
            target_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            possible_access_flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            target_access_flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case IMAGE_FORMAT_CLASS_DEPTH:
        case IMAGE_FORMAT_CLASS_STENCIL:
        case IMAGE_FORMAT_CLASS_DEPTH_STENCIL:
            target_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            possible_access_flags |=
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            target_access_flags |=
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        }

        VkImageLayout image_old_layout = get_image_layout_info (attachment->image, attachment->layer);
        // We're trying to be safe and add barrier to be sure that any previous work on image has finished, for example
        // previous pass instances. Even if layout is the same, previous pass might be still writing something into
        // this attachment, so we need to wait properly.

        if (attachment->image->description.supports_sampling)
        {
            possible_access_flags |= VK_ACCESS_SHADER_READ_BIT;
        }

        image_barriers[added_barriers] = (VkImageMemoryBarrier) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = possible_access_flags,
            .dstAccessMask = target_access_flags,
            .oldLayout = image_old_layout,
            .newLayout = target_layout,
            .srcQueueFamilyIndex = system->device_queue_family_index,
            .dstQueueFamilyIndex = system->device_queue_family_index,
            .image = attachment->image->image,
            .subresourceRange =
                {
                    .aspectMask = get_image_aspects (&attachment->image->description),
                    .baseMipLevel = 0u,
                    .levelCount = 1u,
                    .baseArrayLayer = attachment->layer,
                    .layerCount = 1u,
                },
        };

        set_image_layout_info (attachment->image, attachment->layer, target_layout);
        ++added_barriers;
    }

    DEBUG_LABEL_SCOPE_BEGIN (state->primary_command_buffer, pass_instance->pass->tracking_name, DEBUG_LABEL_COLOR_PASS)

    vkCmdPipelineBarrier (state->primary_command_buffer, source_stage_flags,
                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          0u, 0u, NULL, 0u, NULL, added_barriers, image_barriers);

    vkCmdBeginRenderPass (state->primary_command_buffer, &pass_instance->render_pass_begin_info,
                          VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    vkCmdExecuteCommands (state->primary_command_buffer, 1u, &pass_instance->command_buffer);
    vkCmdEndRenderPass (state->primary_command_buffer);

    process_surface_blit_requests (system, state, pass_instance->pass_end_surface_blit_requests);
    pass_instance->pass_end_surface_blit_requests = NULL;

    process_read_back_requests (system, state, pass_instance->pass_end_buffer_read_back_requests,
                                pass_instance->pass_end_image_read_back_requests);
    pass_instance->pass_end_buffer_read_back_requests = NULL;
    pass_instance->pass_end_image_read_back_requests = NULL;

    // Transition readable render targets so they can be used in shaders for sampling.
    added_barriers = 0u;

    for (kan_loop_size_t attachment_index = 0u; attachment_index < pass_instance->frame_buffer->attachments_count;
         ++attachment_index)
    {
        struct render_backend_frame_buffer_attachment_t *attachment =
            &pass_instance->frame_buffer->attachments[attachment_index];

        if (attachment->image->description.supports_sampling)
        {
            // TODO: This can cause additional unnecessary transitions of an image if it is used both for sampling and
            //       as attachment, but several passes use it as attachment before other pass samples it.

            VkAccessFlags possible_access_flags = 0u;
            VkImageLayout new_layout = VK_IMAGE_LAYOUT_UNDEFINED;

            switch (get_image_format_class (attachment->image->description.format))
            {
            case IMAGE_FORMAT_CLASS_COLOR:
                possible_access_flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                break;

            case IMAGE_FORMAT_CLASS_DEPTH:
            case IMAGE_FORMAT_CLASS_STENCIL:
            case IMAGE_FORMAT_CLASS_DEPTH_STENCIL:
                possible_access_flags |=
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                break;
            }

            VkImageLayout old_layout = get_image_layout_info (attachment->image, attachment->layer);

            // Layout could be already transitioned due to blit requests.
            if (old_layout != new_layout)
            {
                image_barriers[added_barriers] = (VkImageMemoryBarrier) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .pNext = NULL,
                    .srcAccessMask = possible_access_flags,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                    .oldLayout = get_image_layout_info (attachment->image, attachment->layer),
                    .newLayout = new_layout,
                    .srcQueueFamilyIndex = system->device_queue_family_index,
                    .dstQueueFamilyIndex = system->device_queue_family_index,
                    .image = attachment->image->image,
                    .subresourceRange =
                        {
                            .aspectMask = get_image_aspects (&attachment->image->description),
                            .baseMipLevel = 0u,
                            .levelCount = 1u,
                            .baseArrayLayer = attachment->layer,
                            .layerCount = 1u,
                        },
                };

                set_image_layout_info (attachment->image, attachment->layer, new_layout);
                ++added_barriers;
            }
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
}

static void render_backend_system_submit_pass_instance (struct render_backend_system_t *system,
                                                        struct render_backend_command_state_t *state,
                                                        struct render_backend_pass_instance_t *pass_instance)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_submit_pass_instance);
    vkEndCommandBuffer (pass_instance->command_buffer);

    KAN_ASSERT (pass_instance->render_pass_begin_info.framebuffer != VK_NULL_HANDLE)
    execute_pass_instance_submission (system, state, pass_instance);
    struct render_backend_pass_instance_instance_dependency_t *dependant = pass_instance->first_dependant_instance;

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
    kan_cpu_section_execution_shutdown (&execution);
}

static void render_backend_system_submit_graphics (struct render_backend_system_t *system)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_submit_graphics);

    struct render_backend_command_state_t *state = &system->command_states[system->current_frame_in_flight_index];
    struct render_backend_surface_t *surface = (struct render_backend_surface_t *) system->surfaces.first;

    while (surface)
    {
        surface->current_frame_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        surface = (struct render_backend_surface_t *) surface->list_node.next;
    }

    struct render_backend_schedule_state_t *schedule = &system->schedule_states[system->current_frame_in_flight_index];
    submit_mip_generation (system, schedule, state);

    struct kan_cpu_section_execution_t checkpoint_execution;
    kan_cpu_section_execution_init (&checkpoint_execution, system->section_pass_instance_resolve_checkpoints);

#define PASS_INSTANCE_GET_FROM_NODE(SOURCE, NODE)                                                                      \
    SOURCE ? (struct render_backend_pass_instance_t *) (((uint8_t *) SOURCE) -                                         \
                                                        offsetof (struct render_backend_pass_instance_t, NODE)) :      \
             NULL

    struct render_backend_pass_instance_t *pass_instance =
        PASS_INSTANCE_GET_FROM_NODE (system->pass_instances.first, node_in_all);

    while (pass_instance)
    {
        struct render_backend_pass_instance_checkpoint_dependency_t *dependant =
            pass_instance->first_dependant_checkpoint;

        while (dependant)
        {
            propagate_pass_instance_dependency_through_checkpoints (pass_instance, dependant->dependant_checkpoint);
            dependant = dependant->next;
        }

        pass_instance = PASS_INSTANCE_GET_FROM_NODE (pass_instance->node_in_all.next, node_in_all);
    }

    kan_cpu_section_execution_shutdown (&checkpoint_execution);
    struct kan_cpu_section_execution_t pass_instance_execution;
    kan_cpu_section_execution_init (&pass_instance_execution, system->section_pass_instance_sort_and_submission);

    while (system->pass_instances.size > 0u)
    {
        while (system->pass_instances_available.size > 0u)
        {
            render_backend_system_submit_pass_instance (
                system, state, PASS_INSTANCE_GET_FROM_NODE (system->pass_instances_available.first, node_in_available));
        }

        if (system->pass_instances.size > 0u)
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                     "Failed to topologically sort pass instances. Submitting pass instance with lowest amount of "
                     "dependencies to try to work around the issue.")

            struct render_backend_pass_instance_t *make_available_anyway = NULL;
            pass_instance = PASS_INSTANCE_GET_FROM_NODE (system->pass_instances.first, node_in_all);

            while (pass_instance)
            {
                if (make_available_anyway == NULL ||
                    make_available_anyway->dependencies_left > pass_instance->dependencies_left)
                {
                    make_available_anyway = pass_instance;
                }

                pass_instance = PASS_INSTANCE_GET_FROM_NODE (pass_instance->node_in_all.next, node_in_all);
            }

            make_available_anyway->dependencies_left = 0u;
            kan_bd_list_add (&system->pass_instances_available, NULL, &make_available_anyway->node_in_available);
        }
    }

#undef PASS_INSTANCE_GET_FROM_NODE
    kan_cpu_section_execution_shutdown (&pass_instance_execution);
    kan_stack_group_allocator_shrink (&system->pass_instance_allocator);
    kan_stack_group_allocator_reset (&system->pass_instance_allocator);

    process_surface_blit_requests (system, state, schedule->first_scheduled_frame_end_surface_blit);
    schedule->first_scheduled_frame_end_surface_blit = NULL;
    kan_cpu_section_execution_shutdown (&execution);
}

static void render_backend_system_finish_command_submission (struct render_backend_system_t *system)
{
    struct render_backend_command_state_t *state = &system->command_states[system->current_frame_in_flight_index];
    struct render_backend_surface_t *surface = (struct render_backend_surface_t *) system->surfaces.first;

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
            .oldLayout = surface->current_frame_layout,
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

    if (vkEndCommandBuffer (state->primary_command_buffer) != VK_SUCCESS)
    {
        kan_error_critical ("Failed to end recording primary buffer.", __FILE__, __LINE__);
    }

    vulkan_size_t semaphores_to_wait = 0u;
    static VkSemaphore static_wait_semaphores[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES];
    static VkPipelineStageFlags static_semaphore_stages[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES];

    VkSemaphore *wait_semaphores = static_wait_semaphores;
    VkPipelineStageFlags *semaphore_stages = static_semaphore_stages;

    if (system->present_skipped_flags[system->current_frame_in_flight_index])
    {
        wait_semaphores[semaphores_to_wait] = system->render_finished_semaphores[system->current_frame_in_flight_index];
        semaphore_stages[semaphores_to_wait] = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        ++semaphores_to_wait;
    }

    surface = (struct render_backend_surface_t *) system->surfaces.first;
    while (surface)
    {
        if (surface->surface != VK_NULL_HANDLE && surface->acquired_image_frame != UINT32_MAX)
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
        if (system->present_skipped_flags[system->current_frame_in_flight_index])
        {
            wait_semaphores[semaphores_to_wait] =
                system->render_finished_semaphores[system->current_frame_in_flight_index];
            semaphore_stages[semaphores_to_wait] = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            ++semaphores_to_wait;
        }

        surface = (struct render_backend_surface_t *) system->surfaces.first;
        while (surface)
        {
            if (surface->surface != VK_NULL_HANDLE && surface->acquired_image_frame != UINT32_MAX)
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
        kan_error_critical ("Failed to submit work to merged queue.", __FILE__, __LINE__);
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
    static vulkan_size_t static_image_indices[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES];

    vulkan_size_t swap_chains_count = 0u;
    VkSwapchainKHR *swap_chains = static_swap_chains;
    vulkan_size_t *image_indices = static_image_indices;
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
        image_indices = kan_allocate_general (system->utility_allocation_group,
                                              sizeof (vulkan_size_t) * swap_chains_count, _Alignof (vulkan_size_t));
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

    if (swap_chains_count == 0u)
    {
        // Nowhere to present.
        system->present_skipped_flags[system->current_frame_in_flight_index] = KAN_TRUE;
        return;
    }

    system->present_skipped_flags[system->current_frame_in_flight_index] = KAN_FALSE;
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

    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_present);
    const VkResult present_result = vkQueuePresentKHR (system->device_queue, &present_info);
    kan_cpu_section_execution_shutdown (&execution);

    if (present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR &&
        present_result != VK_ERROR_OUT_OF_DATE_KHR)
    {
        kan_error_critical ("Failed to request present operations.", __FILE__, __LINE__);
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

    if (!schedule->first_scheduled_buffer_flush_transfer && !schedule->first_scheduled_buffer_flush &&
        !schedule->first_scheduled_image_upload && !schedule->first_scheduled_image_mip_generation &&
        !schedule->first_scheduled_image_copy_data && !schedule->first_scheduled_frame_end_buffer_read_back &&
        !schedule->first_scheduled_frame_end_image_read_back && !schedule->first_scheduled_frame_end_surface_blit &&
        !schedule->first_scheduled_frame_buffer_destroy && !schedule->first_scheduled_pass_destroy &&
        !schedule->first_scheduled_pipeline_parameter_set_destroy &&
        !schedule->first_scheduled_detached_descriptor_set_destroy &&
        !schedule->first_scheduled_graphics_pipeline_destroy &&
        !schedule->first_scheduled_pipeline_parameter_set_layout_destroy && !schedule->first_scheduled_buffer_destroy &&
        !schedule->first_scheduled_frame_lifetime_allocator_destroy &&
        !schedule->first_scheduled_detached_image_view_destroy && !schedule->first_scheduled_image_destroy)
    {
        // No active scheduled operations, safe to reset allocator completely.
        kan_stack_group_allocator_reset (&schedule->item_allocator);
    }
}

static void render_backend_system_process_read_back (struct render_backend_system_t *system)
{
    struct render_backend_command_state_t *state = &system->command_states[system->current_frame_in_flight_index];
    struct render_backend_schedule_state_t *schedule = &system->schedule_states[system->current_frame_in_flight_index];

    process_read_back_requests (system, state, schedule->first_scheduled_frame_end_buffer_read_back, schedule->first_scheduled_frame_end_image_read_back);
    schedule->first_scheduled_frame_end_buffer_read_back = NULL;
    schedule->first_scheduled_frame_end_image_read_back = NULL;

    // Cleanup statuses that weren't scheduled during this frame.
    struct render_backend_read_back_status_t *previous = NULL;
    struct render_backend_read_back_status_t *status = schedule->first_read_back_status;

    while (status)
    {
        struct render_backend_read_back_status_t *next = status->next;
        if (status->state != KAN_RENDER_READ_BACK_STATE_SCHEDULED)
        {
            status->state = KAN_RENDER_READ_BACK_STATE_FAILED;
            status->referenced_in_schedule = KAN_FALSE;

            if (previous)
            {
                previous->next = previous;
            }
            else
            {
                KAN_ASSERT (status == schedule->first_read_back_status)
                schedule->first_read_back_status = next;
            }

            if (!status->referenced_outside)
            {
                kan_free_batched (system->read_back_status_allocation_group, status);
            }
        }
        else
        {
            previous = status;
        }

        status = next;
    }
}

static void render_backend_system_submit_previous_frame (struct render_backend_system_t *system)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_submit_previous_frame);

    render_backend_system_begin_command_submission (system);
    render_backend_system_submit_transfer (system);
    render_backend_system_submit_graphics (system);
    render_backend_system_process_read_back (system);
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
    const kan_instance_size_t excess_command_buffers =
        command_state->secondary_command_buffers.size - command_state->secondary_command_buffers_used;

    if (excess_command_buffers > 0u)
    {
        VkCommandBuffer *first_excess_buffer =
            &((VkCommandBuffer *)
                  command_state->secondary_command_buffers.data)[command_state->secondary_command_buffers_used];

        vkFreeCommandBuffers (system->device, command_state->command_pool, (vulkan_size_t) excess_command_buffers,
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
    kan_cpu_section_execution_shutdown (&execution);
}

static void render_backend_surface_destroy_swap_chain_image_views (struct render_backend_surface_t *surface)
{
    for (vulkan_size_t view_index = 0u; view_index < surface->images_count; ++view_index)
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

    for (vulkan_size_t view_index = 0u; view_index < surface->images_count; ++view_index)
    {
        surface->image_views[view_index] = VK_NULL_HANDLE;
    }

    kan_bool_t views_created_successfully = KAN_TRUE;
    for (vulkan_size_t view_index = 0u; view_index < surface->images_count; ++view_index)
    {
#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
        {
            char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
            snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME,
                      "Image::ForSurface::%s::instance%lu", surface->tracking_name, (unsigned long) view_index);

            struct VkDebugUtilsObjectNameInfoEXT object_name = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext = NULL,
                .objectType = VK_OBJECT_TYPE_IMAGE,
                .objectHandle = CONVERT_HANDLE_FOR_DEBUG surface->images[view_index],
                .pObjectName = debug_name,
            };

            vkSetDebugUtilsObjectNameEXT (surface->system->device, &object_name);
        }
#endif

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
        char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
        snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "ImageVew::ForSurface:::%s::image%lu",
                  surface->tracking_name, (unsigned long) view_index);

        struct VkDebugUtilsObjectNameInfoEXT object_name = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = NULL,
            .objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
            .objectHandle = CONVERT_HANDLE_FOR_DEBUG surface->image_views[view_index],
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
    for (kan_loop_size_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
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
    for (kan_loop_size_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
    {
        surface->image_available_semaphores[index] = VK_NULL_HANDLE;
    }

    kan_bool_t created_successfully = KAN_TRUE;
    VkSemaphoreCreateInfo semaphore_creation_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
    };

    for (kan_loop_size_t index = 0u; index < KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT; ++index)
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
        snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME,
                  "Semaphore::ForSurface::%s::image_available%lu", surface->tracking_name, (unsigned long) index);

        struct VkDebugUtilsObjectNameInfoEXT object_name = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext = NULL,
            .objectType = VK_OBJECT_TYPE_SEMAPHORE,
            .objectHandle = CONVERT_HANDLE_FOR_DEBUG surface->image_available_semaphores[index],
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
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, surface->system->section_surface_create_swap_chain);

    VkBool32 present_supported;
    if (vkGetPhysicalDeviceSurfaceSupportKHR (surface->system->physical_device,
                                              surface->system->device_queue_family_index, surface->surface,
                                              &present_supported) != VK_SUCCESS)
    {
        KAN_LOG (
            render_backend_system_vulkan, KAN_LOG_ERROR,
            "Unable to create swap chain for surface \"%s\": failed to query whether present to surface is supported.",
            surface->tracking_name)

        kan_cpu_section_execution_shutdown (&execution);
        return;
    }

    if (!present_supported)
    {
        KAN_LOG (
            render_backend_system_vulkan, KAN_LOG_ERROR,
            "Unable to create swap chain for surface \"%s\": picked device is unable to present to created surface.",
            surface->tracking_name)

        kan_cpu_section_execution_shutdown (&execution);
        return;
    }

    kan_instance_size_t formats_count;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR (surface->system->physical_device, surface->surface, &formats_count,
                                              NULL) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": failed to query surface formats.",
                 surface->tracking_name)

        kan_cpu_section_execution_shutdown (&execution);
        return;
    }

    if (formats_count == 0u)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": there is no supported surface formats.",
                 surface->tracking_name)

        kan_cpu_section_execution_shutdown (&execution);
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
        kan_cpu_section_execution_shutdown (&execution);
        return;
    }

    static struct VkSurfaceFormatKHR supported_formats_hardware_gamma[] = {
        {
            .format = VK_FORMAT_B8G8R8A8_SRGB,
            .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        },
        {
            .format = VK_FORMAT_R8G8B8A8_SRGB,
            .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        },
    };

    static struct VkSurfaceFormatKHR supported_formats_custom_gamma[] = {
        {
            .format = VK_FORMAT_B8G8R8A8_UNORM,
            .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        },
        {
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        },
    };

    kan_instance_size_t supported_formats_count = 0u;
    VkSurfaceFormatKHR *supported_formats = NULL;

    if (surface->system->uses_custom_gamma_correction)
    {
        supported_formats_count = sizeof (supported_formats_custom_gamma) / sizeof (supported_formats_custom_gamma[0u]);
        supported_formats = supported_formats_custom_gamma;
    }
    else
    {
        supported_formats_count =
            sizeof (supported_formats_hardware_gamma) / sizeof (supported_formats_hardware_gamma[0u]);
        supported_formats = supported_formats_hardware_gamma;
    }

    VkSurfaceFormatKHR selected_format = {
        .format = VK_FORMAT_UNDEFINED,
        .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
    };

    for (vulkan_size_t index = 0u; index < formats_count; ++index)
    {
        for (vulkan_size_t supported_index = 0u; supported_index < supported_formats_count; ++supported_index)
        {
            if (formats[index].format == supported_formats[supported_index].format &&
                formats[index].colorSpace == supported_formats[supported_index].colorSpace)
            {
                selected_format = formats[index];
                break;
            }
        }

        if (selected_format.format != VK_FORMAT_UNDEFINED)
        {
            break;
        }
    }

    kan_free_general (surface->system->utility_allocation_group, formats, sizeof (VkSurfaceFormatKHR) * formats_count);
    if (selected_format.format == VK_FORMAT_UNDEFINED)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": failed to found supported surface format.",
                 surface->tracking_name)
        kan_cpu_section_execution_shutdown (&execution);
        return;
    }

    kan_instance_size_t present_modes_count;
    if (vkGetPhysicalDeviceSurfacePresentModesKHR (surface->system->physical_device, surface->surface,
                                                   &present_modes_count, NULL) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": failed to query surface present modes.",
                 surface->tracking_name)
        kan_cpu_section_execution_shutdown (&execution);
        return;
    }

    if (present_modes_count == 0u)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": there is no supported surface present modes.",
                 surface->tracking_name)
        kan_cpu_section_execution_shutdown (&execution);
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
        kan_cpu_section_execution_shutdown (&execution);
        return;
    }

    kan_bool_t present_mode_found = KAN_FALSE;
    VkPresentModeKHR surface_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

    for (kan_loop_size_t queue_index = 0u; queue_index < (kan_loop_size_t) KAN_RENDER_SURFACE_PRESENT_MODE_COUNT;
         ++queue_index)
    {
        VkPresentModeKHR requested_mode = VK_PRESENT_MODE_MAX_ENUM_KHR;
        switch (surface->present_modes_queue[queue_index])
        {
        case KAN_RENDER_SURFACE_PRESENT_MODE_INVALID:
            break;

        case KAN_RENDER_SURFACE_PRESENT_MODE_IMMEDIATE:
            requested_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            break;

        case KAN_RENDER_SURFACE_PRESENT_MODE_MAILBOX:
            requested_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;

        case KAN_RENDER_SURFACE_PRESENT_MODE_FIFO:
            requested_mode = VK_PRESENT_MODE_FIFO_KHR;
            break;

        case KAN_RENDER_SURFACE_PRESENT_MODE_FIFO_RELAXED:
            requested_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            break;

        case KAN_RENDER_SURFACE_PRESENT_MODE_COUNT:
            KAN_ASSERT (KAN_FALSE)
            break;
        }

        if (requested_mode == VK_PRESENT_MODE_MAX_ENUM_KHR)
        {
            break;
        }

        for (kan_loop_size_t supported_index = 0u; supported_index < (kan_loop_size_t) present_modes_count;
             ++supported_index)
        {
            if (present_modes[supported_index] == requested_mode)
            {
                present_mode_found = KAN_TRUE;
                surface_present_mode = requested_mode;
                break;
            }
        }

        if (present_mode_found)
        {
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
        kan_cpu_section_execution_shutdown (&execution);
        return;
    }

    VkSurfaceCapabilitiesKHR surface_capabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR (surface->system->physical_device, surface->surface,
                                                   &surface_capabilities) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": failed to query surface capabilities.",
                 surface->tracking_name)
        kan_cpu_section_execution_shutdown (&execution);
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
        .imageFormat = selected_format.format,
        .imageColorSpace = selected_format.colorSpace,
        .imageExtent = surface_extent,
        .imageArrayLayers = 1u,
        .imageUsage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1u,
        .pQueueFamilyIndices = &surface->system->device_queue_family_index,
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
        kan_cpu_section_execution_shutdown (&execution);
        return;
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    char debug_name[KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME];
    snprintf (debug_name, KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME, "Surface::%s", surface->tracking_name);

    struct VkDebugUtilsObjectNameInfoEXT object_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = NULL,
        .objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR,
        .objectHandle = CONVERT_HANDLE_FOR_DEBUG surface->swap_chain,
        .pObjectName = debug_name,
    };

    vkSetDebugUtilsObjectNameEXT (surface->system->device, &object_name);
#endif

    surface->swap_chain_creation_window_width = (vulkan_size_t) surface_extent.width;
    surface->swap_chain_creation_window_height = (vulkan_size_t) surface_extent.height;

    if (!render_backend_surface_create_swap_chain_image_views (surface, selected_format))
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create swap chain for surface \"%s\": failed to construct image views.",
                 surface->tracking_name)

        vkDestroySwapchainKHR (surface->system->device, surface->swap_chain,
                               VULKAN_ALLOCATION_CALLBACKS (surface->system));
        surface->swap_chain = VK_NULL_HANDLE;
        kan_cpu_section_execution_shutdown (&execution);
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
        kan_cpu_section_execution_shutdown (&execution);
        return;
    }

    surface->acquired_image_frame = UINT32_MAX;
    surface->needs_recreation = KAN_FALSE;
    kan_cpu_section_execution_shutdown (&execution);
}

static void render_backend_surface_destroy_swap_chain (struct render_backend_surface_t *surface)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, surface->system->section_surface_destroy_swap_chain);

    if (surface->swap_chain == VK_NULL_HANDLE)
    {
        kan_cpu_section_execution_shutdown (&execution);
        return;
    }

    render_backend_surface_destroy_semaphores (surface);
    render_backend_surface_destroy_swap_chain_image_views (surface);
    vkDestroySwapchainKHR (surface->system->device, surface->swap_chain, VULKAN_ALLOCATION_CALLBACKS (surface->system));
    kan_cpu_section_execution_shutdown (&execution);
}

static kan_bool_t render_backend_system_acquire_images (struct render_backend_system_t *system)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_next_frame_acquire_images);
    kan_context_system_t application_system = kan_context_query (system->context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);

    kan_bool_t acquired_all_images = KAN_TRUE;
    kan_bool_t any_swap_chain_outdated = KAN_FALSE;
    struct render_backend_surface_t *surface = (struct render_backend_surface_t *) system->surfaces.first;

    while (surface)
    {
        const struct kan_application_system_window_info_t *window_info =
            kan_application_system_get_window_info_from_handle (application_system, surface->window_handle);

        if (surface->needs_recreation)
        {
            acquired_all_images = KAN_FALSE;
            any_swap_chain_outdated = KAN_TRUE;
        }
        else if (surface->swap_chain == VK_NULL_HANDLE)
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

    kan_cpu_section_execution_shutdown (&execution);
    return acquired_all_images && !any_swap_chain_outdated;
}

kan_bool_t kan_render_backend_system_next_frame (kan_context_system_t render_backend_system)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (render_backend_system);
    struct kan_cpu_section_execution_t next_frame_execution;
    kan_cpu_section_execution_init (&next_frame_execution, system->section_next_frame);

    if (system->frame_started)
    {
        if (!render_backend_system_acquire_images (system))
        {
            KAN_LOG (render_backend_system_vulkan, KAN_LOG_INFO, "Skipping frame as swap chain images are not ready.")
            kan_cpu_section_execution_shutdown (&next_frame_execution);
            return KAN_FALSE;
        }

        render_backend_system_submit_previous_frame (system);
    }

    struct kan_cpu_section_execution_t synchronization_execution;
    kan_cpu_section_execution_init (&synchronization_execution, system->section_next_frame_synchronization);

    VkResult fence_wait_result =
        vkWaitForFences (system->device, 1u, &system->in_flight_fences[system->current_frame_in_flight_index], VK_TRUE,
                         KAN_CONTEXT_RENDER_BACKEND_VULKAN_FENCE_WAIT_NS);

    if (fence_wait_result == VK_TIMEOUT)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_INFO, "Skipping frame due to in flight fence wait timeout.")
        kan_cpu_section_execution_shutdown (&synchronization_execution);
        kan_cpu_section_execution_shutdown (&next_frame_execution);
        return KAN_FALSE;
    }
    else if (fence_wait_result != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed waiting for in flight fence.")
        kan_cpu_section_execution_shutdown (&synchronization_execution);
        kan_cpu_section_execution_shutdown (&next_frame_execution);
        return KAN_FALSE;
    }

    vkResetFences (system->device, 1u, &system->in_flight_fences[system->current_frame_in_flight_index]);
    system->frame_started = KAN_TRUE;
    kan_cpu_section_execution_shutdown (&synchronization_execution);

    struct kan_cpu_section_execution_t command_pool_reset_execution;
    kan_cpu_section_execution_init (&command_pool_reset_execution, system->section_next_frame_command_pool_reset);

    if (vkResetCommandPool (system->device, system->command_states[system->current_frame_in_flight_index].command_pool,
                            0u) != VK_SUCCESS)
    {
        kan_error_critical ("Unexpected failure when resetting graphics command pool.", __FILE__, __LINE__);
    }

    kan_cpu_section_execution_shutdown (&command_pool_reset_execution);
    struct kan_cpu_section_execution_t destruction_schedule_execution;
    kan_cpu_section_execution_init (&destruction_schedule_execution, system->section_next_frame_destruction_schedule);

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

    struct scheduled_graphics_pipeline_destroy_t *graphics_pipeline_destroy =
        schedule->first_scheduled_graphics_pipeline_destroy;
    schedule->first_scheduled_graphics_pipeline_destroy = NULL;
    kan_bool_t any_pipeline_layout_destroyed = KAN_FALSE;

    while (graphics_pipeline_destroy)
    {
        // If we still have lingering compilation request, we must deal with it.
        while (graphics_pipeline_destroy->pipeline->compilation_request)
        {
            struct kan_cpu_section_execution_t waiting_compilation_execution;
            kan_cpu_section_execution_init (
                &waiting_compilation_execution,
                system->section_next_frame_destruction_schedule_waiting_pipeline_compilation);

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
                kan_precise_time_sleep (KAN_CONTEXT_RENDER_BACKEND_VULKAN_COMPILATION_WAIT_NS);
                break;

            case PIPELINE_COMPILATION_STATE_SUCCESS:
            case PIPELINE_COMPILATION_STATE_FAILURE:
                // Already got completed when lock is acquired, we can exit.
                KAN_ASSERT (!graphics_pipeline_destroy->pipeline->compilation_request)
                kan_mutex_unlock (system->compiler_state.state_transition_mutex);
                break;
            }

            kan_cpu_section_execution_shutdown (&waiting_compilation_execution);
        }

        struct render_backend_pipeline_layout_t *pipeline_layout = graphics_pipeline_destroy->pipeline->layout;
        kan_bd_list_remove (&system->graphics_pipelines, &graphics_pipeline_destroy->pipeline->list_node);
        render_backend_system_destroy_graphics_pipeline (system, graphics_pipeline_destroy->pipeline);
        --pipeline_layout->usage_count;

        if (pipeline_layout->usage_count == 0u)
        {
            kan_hash_storage_remove (&system->pipeline_layouts, &pipeline_layout->node);
            render_backend_system_destroy_pipeline_layout (system, pipeline_layout);
            any_pipeline_layout_destroyed = KAN_TRUE;
        }

        graphics_pipeline_destroy = graphics_pipeline_destroy->next;
    }

    if (any_pipeline_layout_destroyed)
    {
        kan_hash_storage_update_bucket_count_default (&system->pipeline_layouts,
                                                      KAN_CONTEXT_RENDER_BACKEND_VULKAN_PL_BUCKETS);
    }

    struct scheduled_pipeline_parameter_set_layout_destroy_t *pipeline_parameter_set_layout_destroy =
        schedule->first_scheduled_pipeline_parameter_set_layout_destroy;
    schedule->first_scheduled_pipeline_parameter_set_layout_destroy = NULL;
    kan_bool_t any_pipeline_parameter_set_layout_destroyed = KAN_FALSE;

    while (pipeline_parameter_set_layout_destroy)
    {
        // References could've been updated even after layout destruction: for example, new layout with the same
        // content could've been created right away -- in that case, there is no need to delete old one.
        if (kan_atomic_int_get (&pipeline_parameter_set_layout_destroy->layout->reference_count) == 0)
        {
            kan_hash_storage_remove (&system->pipeline_parameter_set_layouts,
                                     &pipeline_parameter_set_layout_destroy->layout->node);
            render_backend_system_destroy_pipeline_parameter_set_layout (system,
                                                                         pipeline_parameter_set_layout_destroy->layout);
            any_pipeline_parameter_set_layout_destroyed = KAN_TRUE;
        }

        pipeline_parameter_set_layout_destroy = pipeline_parameter_set_layout_destroy->next;
    }

    if (any_pipeline_parameter_set_layout_destroyed)
    {
        kan_hash_storage_update_bucket_count_default (&system->pipeline_parameter_set_layouts,
                                                      KAN_CONTEXT_RENDER_BACKEND_VULKAN_SET_LAYOUT_BUCKETS);
    }

    struct scheduled_frame_buffer_destroy_t *frame_buffer_destroy = schedule->first_scheduled_frame_buffer_destroy;
    schedule->first_scheduled_frame_buffer_destroy = NULL;

    while (frame_buffer_destroy)
    {
        kan_bd_list_remove (&system->frame_buffers, &frame_buffer_destroy->frame_buffer->list_node);
        render_backend_system_destroy_frame_buffer (system, frame_buffer_destroy->frame_buffer);
        frame_buffer_destroy = frame_buffer_destroy->next;
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

    kan_cpu_section_execution_shutdown (&destruction_schedule_execution);
    render_backend_system_clean_current_schedule_if_safe (system);

    struct render_backend_read_back_status_t *status = schedule->first_read_back_status;
    schedule->first_read_back_status = NULL;

    while (status)
    {
        struct render_backend_read_back_status_t *next = status->next;
        KAN_ASSERT (status->state == KAN_RENDER_READ_BACK_STATE_SCHEDULED)
        status->state = KAN_RENDER_READ_BACK_STATE_FINISHED;
        status->referenced_in_schedule = KAN_FALSE;

        if (!status->referenced_outside)
        {
            kan_free_batched (system->read_back_status_allocation_group, status);
        }

        status = next;
    }

    struct render_backend_frame_lifetime_allocator_t *frame_lifetime_allocator =
        (struct render_backend_frame_lifetime_allocator_t *) system->frame_lifetime_allocators.first;

    while (frame_lifetime_allocator)
    {
        render_backend_frame_lifetime_allocator_retire_old_allocations (frame_lifetime_allocator);
        frame_lifetime_allocator =
            (struct render_backend_frame_lifetime_allocator_t *) frame_lifetime_allocator->list_node.next;
    }

    struct render_backend_command_state_t *command_state =
        &system->command_states[system->current_frame_in_flight_index];
    command_state->secondary_command_buffers_used = 0u;

    kan_cpu_section_execution_shutdown (&next_frame_execution);
    return KAN_TRUE;
}

static void render_backend_surface_init_with_window (void *user_data,
                                                     const struct kan_application_system_window_info_t *window_info)
{
    struct render_backend_surface_t *surface = user_data;
    KAN_ASSERT (surface->surface == VK_NULL_HANDLE)

    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, surface->system->section_surface_init_with_window);

    surface->surface = (VkSurfaceKHR) kan_platform_application_window_create_vulkan_surface (
        window_info->id, (kan_memory_size_t) surface->system->instance, VULKAN_ALLOCATION_CALLBACKS (surface->system));
    render_backend_surface_create_swap_chain (surface, window_info);
    kan_cpu_section_execution_shutdown (&execution);
}

static void render_backend_surface_shutdown_with_window (void *user_data,
                                                         const struct kan_application_system_window_info_t *window_info)
{
    struct render_backend_surface_t *surface = user_data;
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, surface->system->section_surface_shutdown_with_window);

    vkDeviceWaitIdle (surface->system->device);

    if (surface->surface != VK_NULL_HANDLE)
    {
        render_backend_surface_destroy_swap_chain (surface);
        kan_platform_application_window_destroy_vulkan_surface (
            window_info->id, (kan_memory_size_t) surface->system->instance, (uint64_t) surface->surface,
            VULKAN_ALLOCATION_CALLBACKS (surface->system));
        surface->surface = VK_NULL_HANDLE;
    }

    kan_bd_list_remove (&surface->system->surfaces, &surface->list_node);
    kan_free_batched (surface->system->surface_wrapper_allocation_group, surface);
    kan_cpu_section_execution_shutdown (&execution);
}

kan_render_surface_t kan_render_backend_system_create_surface (
    kan_context_system_t render_backend_system,
    kan_application_system_window_t window,
    enum kan_render_surface_present_mode_t *present_mode_queue,
    kan_interned_string_t tracking_name)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (render_backend_system);
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, system->section_create_surface);

    kan_context_system_t application_system = kan_context_query (system->context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);

    if (!KAN_HANDLE_IS_VALID (application_system))
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Unable to create surfaces due to absence of application system in context.")
        kan_cpu_section_execution_shutdown (&execution);
        return KAN_HANDLE_SET_INVALID (kan_render_surface_t);
    }

    struct render_backend_surface_t *new_surface =
        kan_allocate_batched (system->surface_wrapper_allocation_group, sizeof (struct render_backend_surface_t));

    new_surface->system = system;
    new_surface->tracking_name = tracking_name;
    new_surface->surface = VK_NULL_HANDLE;
    new_surface->swap_chain = VK_NULL_HANDLE;
    new_surface->window_handle = window;

    new_surface->current_frame_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    kan_bool_t encountered_invalid_present_mode = KAN_FALSE;

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) KAN_RENDER_SURFACE_PRESENT_MODE_COUNT; ++index)
    {
        if (encountered_invalid_present_mode)
        {
            new_surface->present_modes_queue[index] = KAN_RENDER_SURFACE_PRESENT_MODE_INVALID;
        }
        else
        {
            new_surface->present_modes_queue[index] = present_mode_queue[index];
            if (present_mode_queue[index] == KAN_RENDER_SURFACE_PRESENT_MODE_INVALID)
            {
                encountered_invalid_present_mode = KAN_TRUE;
            }
        }
    }

    kan_atomic_int_lock (&system->resource_registration_lock);
    kan_bd_list_add (&system->surfaces, NULL, &new_surface->list_node);
    kan_atomic_int_unlock (&system->resource_registration_lock);

    new_surface->resource_id =
        kan_application_system_window_add_resource (application_system, window,
                                                    (struct kan_application_system_window_resource_binding_t) {
                                                        .user_data = new_surface,
                                                        .init = render_backend_surface_init_with_window,
                                                        .shutdown = render_backend_surface_shutdown_with_window,
                                                    });

    kan_cpu_section_execution_shutdown (&execution);
    return KAN_HANDLE_SET (kan_render_surface_t, new_surface);
}

void kan_render_backend_system_present_image_on_surface (kan_render_surface_t surface,
                                                         kan_render_image_t image,
                                                         uint8_t image_layer,
                                                         struct kan_render_integer_region_t surface_region,
                                                         struct kan_render_integer_region_t image_region,
                                                         kan_render_pass_instance_t present_result_of_pass_instance)
{
    struct render_backend_surface_t *surface_data = KAN_HANDLE_GET (surface);
    struct render_backend_image_t *image_data = KAN_HANDLE_GET (image);

    if (surface_region.x < 0 || surface_region.y < 0 ||
        surface_region.x + (kan_render_offset_t) surface_region.width >
            (kan_render_offset_t) surface_data->swap_chain_creation_window_width ||
        surface_region.y + (kan_render_offset_t) surface_region.height >
            (kan_render_offset_t) surface_data->swap_chain_creation_window_height)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Failed to present image \"%s\" on surface \"%s\": surface region is out of surface bounds.",
                 image_data->description.tracking_name, surface_data->tracking_name)
        return;
    }

    if (image_region.x < 0 || image_region.y < 0 ||
        image_region.x + (kan_render_offset_t) image_region.width >
            (kan_render_offset_t) image_data->description.width ||
        image_region.y + (kan_render_offset_t) image_region.height >
            (kan_render_offset_t) image_data->description.height)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Failed to present image \"%s\" on surface \"%s\": image region is out of image bounds.",
                 image_data->description.tracking_name, image_data->description.tracking_name)
        return;
    }

    if (get_image_format_class (image_data->description.format) != IMAGE_FORMAT_CLASS_COLOR)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR,
                 "Failed to present image \"%s\" on surface \"%s\" as image is not a color image.",
                 image_data->description.tracking_name, surface_data->tracking_name)
        return;
    }

    struct render_backend_schedule_state_t *schedule =
        render_backend_system_get_schedule_for_memory (surface_data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_surface_blit_request_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_surface_blit_request_t);

    if (KAN_HANDLE_IS_VALID (present_result_of_pass_instance))
    {
        struct render_backend_pass_instance_t *pass_instance = KAN_HANDLE_GET (present_result_of_pass_instance);
        item->next = pass_instance->pass_end_surface_blit_requests;
        pass_instance->pass_end_surface_blit_requests = item;
    }
    else
    {
        item->next = schedule->first_scheduled_frame_end_surface_blit;
        schedule->first_scheduled_frame_end_surface_blit = item;
    }

    item->surface = surface_data;
    item->image = image_data;
    item->image_layer = image_layer;
    item->surface_region = surface_region;
    item->image_region = image_region;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}

void kan_render_backend_system_change_surface_present_mode (kan_render_surface_t surface,
                                                            enum kan_render_surface_present_mode_t *present_mode_queue)
{
    struct render_backend_surface_t *data = KAN_HANDLE_GET (surface);
    memcpy (data->present_modes_queue, present_mode_queue,
            sizeof (enum kan_render_surface_present_mode_t) * KAN_RENDER_SURFACE_PRESENT_MODE_COUNT);
    data->needs_recreation = KAN_TRUE;
}

void kan_render_backend_system_destroy_surface (kan_context_system_t render_backend_system,
                                                kan_render_surface_t surface)
{
    struct render_backend_system_t *system = KAN_HANDLE_GET (render_backend_system);
    struct render_backend_surface_t *surface_data = KAN_HANDLE_GET (surface);

    kan_context_system_t application_system = kan_context_query (system->context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);

    // Valid surface couldn't have been created without application system.
    KAN_ASSERT (KAN_HANDLE_IS_VALID (application_system))
    kan_application_system_window_remove_resource (application_system, surface_data->window_handle,
                                                   surface_data->resource_id);
}

enum kan_platform_window_flag_t kan_render_get_required_window_flags (void)
{
    return KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_VULKAN;
}

kan_memory_size_t kan_render_get_supported_code_format_flags (void)
{
    return (1u << KAN_RENDER_CODE_FORMAT_SPIRV);
}
