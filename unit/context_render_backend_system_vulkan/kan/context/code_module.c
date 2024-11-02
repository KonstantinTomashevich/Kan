#include <kan/context/render_backend_implementation_interface.h>

struct render_backend_code_module_t *render_backend_system_create_code_module (struct render_backend_system_t *system,
                                                                               uint32_t code_length,
                                                                               void *code,
                                                                               kan_interned_string_t tracking_name)
{
    VkShaderModuleCreateInfo module_create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0u,
        .codeSize = code_length,
        .pCode = code,
    };

    VkShaderModule shader_module;
    if (vkCreateShaderModule (system->device, &module_create_info, VULKAN_ALLOCATION_CALLBACKS (system),
                              &shader_module) != VK_SUCCESS)
    {
        KAN_LOG (render_backend_system_vulkan, KAN_LOG_ERROR, "Failed to create shader module \"%s\".", tracking_name)
        return NULL;
    }

#if defined(KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    struct VkDebugUtilsObjectNameInfoEXT object_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = NULL,
        .objectType = VK_OBJECT_TYPE_SHADER_MODULE,
        .objectHandle = (uint64_t) shader_module,
        .pObjectName = tracking_name,
    };

    vkSetDebugUtilsObjectNameEXT (system->device, &object_name);
#endif

    struct render_backend_code_module_t *module = kan_allocate_batched (system->code_module_wrapper_allocation_group,
                                                                        sizeof (struct render_backend_code_module_t));

    kan_bd_list_add (&system->code_modules, NULL, &module->list_node);
    module->system = system;
    module->module = shader_module;
    module->tracking_name = tracking_name;
    return module;
}

void render_backend_system_destroy_code_module (struct render_backend_system_t *system,
                                                struct render_backend_code_module_t *code_module)
{
    vkDestroyShaderModule (system->device, code_module->module, VULKAN_ALLOCATION_CALLBACKS (system));
    kan_free_batched (system->code_module_wrapper_allocation_group, code_module);
}

kan_render_code_module_t kan_render_code_module_create (kan_render_context_t context,
                                                        uint32_t code_length,
                                                        void *code,
                                                        kan_interned_string_t tracking_name)
{
    struct render_backend_system_t *system = (struct render_backend_system_t *) context;
    kan_atomic_int_lock (&system->resource_management_lock);
    struct render_backend_code_module_t *module =
        render_backend_system_create_code_module (system, code_length, code, tracking_name);
    kan_atomic_int_unlock (&system->resource_management_lock);
    return module ? (kan_render_code_module_t) module : KAN_INVALID_RENDER_CODE_MODULE;
}

void kan_render_code_module_destroy (kan_render_code_module_t code_module)
{
    struct render_backend_code_module_t *data = (struct render_backend_code_module_t *) code_module;
    struct render_backend_schedule_state_t *schedule = render_backend_system_get_schedule_for_destroy (data->system);
    kan_atomic_int_lock (&schedule->schedule_lock);

    struct scheduled_code_module_destroy_t *item =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&schedule->item_allocator, struct scheduled_code_module_destroy_t);

    item->next = schedule->first_scheduled_code_module_destroy;
    schedule->first_scheduled_code_module_destroy = item;
    item->module = data;
    kan_atomic_int_unlock (&schedule->schedule_lock);
}
