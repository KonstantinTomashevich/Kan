#include <kan/context/all_system_names.h>
#include <kan/context/application_system.h>
#include <kan/context/hot_reload_coordination_system.h>
#include <kan/context/update_system.h>
#include <kan/error/critical.h>
#include <kan/file_system_watcher/watcher.h>
#include <kan/memory/allocation.h>

struct hot_reload_coordination_system_t
{
    kan_context_t context;
    kan_allocation_group_t group;

    enum kan_hot_reload_mode_t current_mode;
    kan_bool_t hot_swap_requested;
    kan_bool_t hot_swap;

    struct kan_hot_reload_automatic_config_t automatic_config;
    struct kan_hot_reload_on_request_config_t on_request_config;

    kan_application_system_event_iterator_t event_iterator;
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

    system->current_mode = config->initial_mode;
    system->automatic_config = config->automatic_independent;
    system->on_request_config = config->on_request;

    system->hot_swap_requested = KAN_FALSE;
    system->hot_swap = KAN_FALSE;

    system->event_iterator = KAN_HANDLE_SET_INVALID (kan_application_system_event_iterator_t);
    return KAN_HANDLE_SET (kan_context_system_t, system);
}

static void hot_reload_coordination_system_update (kan_context_system_t handle);

void hot_reload_coordination_system_connect (kan_context_system_t handle, kan_context_t context)
{
    struct hot_reload_coordination_system_t *system = KAN_HANDLE_GET (handle);
    system->context = context;

    if (system->current_mode == KAN_HOT_RELOAD_MODE_DISABLED)
    {
        return;
    }

    kan_context_system_t application_system = kan_context_query (system->context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);
    if (KAN_HANDLE_IS_VALID (application_system))
    {
        system->event_iterator = kan_application_system_event_iterator_create (application_system);
    }

    kan_context_system_t update_system = kan_context_query (system->context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
    if (KAN_HANDLE_IS_VALID (update_system))
    {
        kan_update_system_connect_on_run (update_system, handle, hot_reload_coordination_system_update, 0u, NULL, 0u,
                                          NULL);
    }
}

void hot_reload_coordination_system_init (kan_context_system_t handle)
{
    // Nothing to do here.
}

static void hot_reload_coordination_system_update (kan_context_system_t handle)
{
    struct hot_reload_coordination_system_t *system = KAN_HANDLE_GET (handle);
    system->hot_swap = KAN_FALSE;
    KAN_ASSERT (system->current_mode != KAN_HOT_RELOAD_MODE_DISABLED)

    kan_context_system_t application_system = kan_context_query (system->context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);
    if (KAN_HANDLE_IS_VALID (application_system))
    {
        const struct kan_platform_application_event_t *event;
        while ((event = kan_application_system_event_iterator_get (application_system, system->event_iterator)))
        {
            if (event->type == KAN_PLATFORM_APPLICATION_EVENT_TYPE_KEY_UP)
            {
                if (event->keyboard.scan_code == system->automatic_config.enable_hot_key &&
                    event->keyboard.modifiers == system->automatic_config.enable_hot_key_modifiers)
                {
                    system->current_mode = KAN_HOT_RELOAD_MODE_AUTOMATIC_INDEPENDENT;
                }
                else if (event->keyboard.scan_code == system->on_request_config.enable_hot_key &&
                         event->keyboard.modifiers == system->on_request_config.enable_hot_key_modifiers)
                {
                    system->current_mode = KAN_HOT_RELOAD_MODE_ON_REQUEST;
                }
                else if (system->current_mode == KAN_HOT_RELOAD_MODE_ON_REQUEST &&
                         event->keyboard.scan_code == system->on_request_config.trigger_hot_key &&
                         event->keyboard.modifiers == system->on_request_config.trigger_hot_key_modifiers)
                {
                    system->hot_swap_requested = KAN_TRUE;
                }
            }

            system->event_iterator = kan_application_system_event_iterator_advance (system->event_iterator);
        }
    }

    switch (system->current_mode)
    {
    case KAN_HOT_RELOAD_MODE_DISABLED:
        KAN_ASSERT (KAN_FALSE)
        break;

    case KAN_HOT_RELOAD_MODE_AUTOMATIC_INDEPENDENT:
        // Automatic mode does nothing, it just skips requests if they were made.
        system->hot_swap_requested = KAN_FALSE;
        break;

    case KAN_HOT_RELOAD_MODE_ON_REQUEST:
        if (system->hot_swap_requested)
        {
            // On request received -- declare hot reload frame and wait for file system watchers to check everything.
            system->hot_swap_requested = KAN_FALSE;
            system->hot_swap = KAN_TRUE;
            kan_file_system_watcher_ensure_all_watchers_are_up_to_date ();
        }

        break;
    }
}

void hot_reload_coordination_system_shutdown (kan_context_system_t handle)
{
    // Nothing to do here.
}

void hot_reload_coordination_system_disconnect (kan_context_system_t handle)
{
    struct hot_reload_coordination_system_t *system = KAN_HANDLE_GET (handle);
    kan_context_system_t application_system = kan_context_query (system->context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (application_system) && KAN_HANDLE_IS_VALID (system->event_iterator))
    {
        kan_application_system_event_iterator_destroy (application_system, system->event_iterator);
    }

    kan_context_system_t update_system = kan_context_query (system->context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
    if (KAN_HANDLE_IS_VALID (update_system))
    {
        kan_update_system_disconnect_on_run (update_system, handle);
    }
}

void hot_reload_coordination_system_destroy (kan_context_system_t handle)
{
    struct hot_reload_coordination_system_t *system = KAN_HANDLE_GET (handle);
    // Nothing to clear here.
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
    struct hot_reload_coordination_system_t *data = KAN_HANDLE_GET (system);
    return data->current_mode;
}

void kan_hot_reload_coordination_system_set_current_mode (kan_context_system_t system, enum kan_hot_reload_mode_t mode)
{
    struct hot_reload_coordination_system_t *data = KAN_HANDLE_GET (system);
    // Cannot switch from and into disabled mode.
    KAN_ASSERT (data->current_mode != KAN_HOT_RELOAD_MODE_DISABLED && mode != KAN_HOT_RELOAD_MODE_DISABLED)
    data->current_mode = mode;
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

kan_bool_t kan_hot_reload_coordination_system_is_hot_swap (kan_context_system_t system)
{
    struct hot_reload_coordination_system_t *data = KAN_HANDLE_GET (system);
    return data->hot_swap;
}

void kan_hot_reload_coordination_system_request_hot_swap (kan_context_system_t system)
{
    struct hot_reload_coordination_system_t *data = KAN_HANDLE_GET (system);
    data->hot_swap_requested = KAN_TRUE;
}
