#include <kan/context/all_system_names.h>
#include <kan/context/hot_reload_coordination_system.h>
#include <kan/memory/allocation.h>

struct hot_reload_coordination_system_t
{
    kan_context_t context;
    kan_allocation_group_t group;
    struct kan_hot_reload_automatic_config_t automatic_config;
    struct kan_hot_reload_on_request_config_t on_request_config;
};

kan_context_system_t hot_reload_coordination_system_create (kan_allocation_group_t group, void *user_config)
{
    struct hot_reload_coordination_system_t *system = kan_allocate_general (
        group, sizeof (struct hot_reload_coordination_system_t), _Alignof (struct hot_reload_coordination_system_t));
    system->group = group;

    struct kan_hot_reload_coordination_system_config_t default_config;
    struct kan_hot_reload_coordination_system_config_t *config = user_config;

    if (!config)
    {
        kan_hot_reload_coordination_system_config_init (&default_config);
        config = &default_config;
    }

    system->automatic_config = config->automatic_independent;
    system->on_request_config = config->on_request;
    return KAN_HANDLE_SET (kan_context_system_t, system);
}

void hot_reload_coordination_system_connect (kan_context_system_t handle, kan_context_t context)
{
    struct hot_reload_coordination_system_t *system = KAN_HANDLE_GET (handle);
    system->context = context;
}

void hot_reload_coordination_system_init (kan_context_system_t handle)
{
    // Do nothing, it is a stub implementation.
}

void hot_reload_coordination_system_shutdown (kan_context_system_t handle)
{
    // Do nothing, it is a stub implementation.
}

void hot_reload_coordination_system_disconnect (kan_context_system_t handle)
{
    // Do nothing, it is a stub implementation.
}

void hot_reload_coordination_system_destroy (kan_context_system_t handle)
{
    struct hot_reload_coordination_system_t *system = KAN_HANDLE_GET (handle);
    // Do nothing, it is a stub implementation.
    kan_free_general (system->group, system, sizeof (struct hot_reload_coordination_system_t));
}

CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (
    hot_reload_coordination_system_t) = {
    .name = KAN_CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_NAME,
    .create = hot_reload_coordination_system_create,
    .connect = hot_reload_coordination_system_connect,
    .connected_init = hot_reload_coordination_system_init,
    .connected_shutdown = hot_reload_coordination_system_shutdown,
    .disconnect = hot_reload_coordination_system_disconnect,
    .destroy = hot_reload_coordination_system_destroy,
};

void kan_hot_reload_coordination_system_config_init (struct kan_hot_reload_coordination_system_config_t *instance)
{
    instance->initial_mode = KAN_HOT_RELOAD_MODE_DISABLED;
    instance->automatic_independent.change_wait_time_ns = 100000000u;
    instance->automatic_independent.enable_hot_key = KAN_PLATFORM_SCAN_CODE_COMMA;
    instance->automatic_independent.enable_hot_key_modifiers = KAN_PLATFORM_MODIFIER_MASK_LEFT_CONTROL;
    instance->on_request.enable_hot_key = KAN_PLATFORM_SCAN_CODE_PERIOD;
    instance->on_request.enable_hot_key_modifiers = KAN_PLATFORM_MODIFIER_MASK_LEFT_CONTROL;
    instance->on_request.trigger_hot_key = KAN_PLATFORM_SCAN_CODE_SLASH;
    instance->on_request.trigger_hot_key_modifiers = KAN_PLATFORM_MODIFIER_MASK_LEFT_CONTROL;
}

enum kan_hot_reload_mode_t kan_hot_reload_coordination_system_get_current_mode (kan_context_system_t system)
{
    return KAN_HOT_RELOAD_MODE_DISABLED;
}

void kan_hot_reload_coordination_system_set_current_mode (kan_context_system_t system, enum kan_hot_reload_mode_t mode)
{
    // Do nothing, it is a stub implementation.
}

struct kan_hot_reload_automatic_config_t *kan_hot_reload_coordination_system_get_automatic_config (
    kan_context_system_t system)
{
    struct hot_reload_coordination_system_t *data = KAN_HANDLE_GET (system);
    return &data->automatic_config;
}

struct kan_hot_reload_on_request_config_t *kan_hot_reload_coordination_system_get_on_request_config (
    kan_context_system_t system)
{
    struct hot_reload_coordination_system_t *data = KAN_HANDLE_GET (system);
    return &data->on_request_config;
}

kan_bool_t kan_hot_reload_coordination_system_is_hot_swap (kan_context_system_t system) { return KAN_FALSE; }

void kan_hot_reload_coordination_system_request_hot_swap (kan_context_system_t system)
{
    // Do nothing, it is a stub implementation.
}
