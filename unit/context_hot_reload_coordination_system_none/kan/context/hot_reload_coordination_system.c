#include <kan/context/all_system_names.h>
#include <kan/context/hot_reload_coordination_system.h>
#include <kan/memory/allocation.h>

struct hot_reload_coordination_system_t
{
    kan_context_t context;
    kan_allocation_group_t group;
};

kan_context_system_t hot_reload_coordination_system_create (kan_allocation_group_t group, void *user_config)
{
    struct hot_reload_coordination_system_t *system = kan_allocate_general (
        group, sizeof (struct hot_reload_coordination_system_t), alignof (struct hot_reload_coordination_system_t));
    system->group = group;
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
    instance->change_wait_time_ns = 100000000u;
    instance->toggle_hot_key = KAN_PLATFORM_SCAN_CODE_COMMA;
    instance->toggle_hot_key_modifiers = KAN_PLATFORM_MODIFIER_MASK_LEFT_CONTROL;
}

kan_time_offset_t kan_hot_reload_coordination_system_get_change_wait_time_ns (kan_context_system_t system)
{
    return 100000000u;
}

bool kan_hot_reload_coordination_system_is_possible (void) { return false; }

bool kan_hot_reload_coordination_system_is_reload_allowed (kan_context_system_t system) { return false; }

bool kan_hot_reload_coordination_system_is_scheduled (kan_context_system_t system) { return false; }

bool kan_hot_reload_coordination_system_is_executing (kan_context_system_t system) { return false; }

void kan_hot_reload_coordination_system_schedule (kan_context_system_t system) {}

void kan_hot_reload_coordination_system_delay (kan_context_system_t system) {}

void kan_hot_reload_coordination_system_finish (kan_context_system_t system) {}

kan_hot_reload_file_event_provider_t kan_hot_reload_file_event_provider_create (kan_context_system_t system,
                                                                                const char *path)
{
    return KAN_HANDLE_SET_INVALID (kan_hot_reload_file_event_provider_t);
}

const struct kan_file_system_watcher_event_t *kan_hot_reload_file_event_provider_get (
    kan_hot_reload_file_event_provider_t provider)
{
    return NULL;
}

void kan_hot_reload_file_event_provider_advance (kan_hot_reload_file_event_provider_t provider) {}

void kan_hot_reload_file_event_provider_destroy (kan_hot_reload_file_event_provider_t provider) {}

kan_hot_reload_virtual_file_event_provider_t kan_hot_reload_virtual_file_event_provider_create (
    kan_context_system_t system, const char *path)
{
    return KAN_HANDLE_SET_INVALID (kan_hot_reload_virtual_file_event_provider_t);
}

const struct kan_virtual_file_system_watcher_event_t *kan_hot_reload_virtual_file_event_provider_get (
    kan_hot_reload_virtual_file_event_provider_t provider)
{
    return NULL;
}

void kan_hot_reload_virtual_file_event_provider_advance (kan_hot_reload_virtual_file_event_provider_t provider) {}

void kan_hot_reload_virtual_file_event_provider_destroy (kan_hot_reload_virtual_file_event_provider_t provider) {}
