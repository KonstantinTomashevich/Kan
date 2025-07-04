#include <kan/context/all_system_names.h>
#include <kan/context/application_system.h>
#include <kan/context/hot_reload_coordination_system.h>
#include <kan/context/update_system.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (hot_reload_coordination_system);

enum hot_reload_coordination_state_t
{
    HOT_RELOAD_COORDINATION_STATE_DORMANT = 0,
    HOT_RELOAD_COORDINATION_STATE_REQUESTED,
    HOT_RELOAD_COORDINATION_STATE_SCHEDULED,
    HOT_RELOAD_COORDINATION_STATE_DELAYED,
    HOT_RELOAD_COORDINATION_STATE_EXECUTING,
};

struct hot_reload_coordination_system_t
{
    kan_context_t context;
    kan_allocation_group_t group;

    struct kan_atomic_int_t state;
    bool paused;

    struct kan_hot_reload_coordination_system_config_t config;
    kan_application_system_event_iterator_t event_iterator;
};

kan_context_system_t hot_reload_coordination_system_create (kan_allocation_group_t group, void *user_config)
{
    struct hot_reload_coordination_system_t *system = kan_allocate_general (
        group, sizeof (struct hot_reload_coordination_system_t), alignof (struct hot_reload_coordination_system_t));

    system->group = group;
    system->state = kan_atomic_int_init (HOT_RELOAD_COORDINATION_STATE_DORMANT);
    system->paused = false;

    if (user_config)
    {
        system->config = *(struct kan_hot_reload_coordination_system_config_t *) user_config;
    }
    else
    {
        kan_hot_reload_coordination_system_config_init (&system->config);
    }

    system->event_iterator = KAN_HANDLE_SET_INVALID (kan_application_system_event_iterator_t);
    return KAN_HANDLE_SET (kan_context_system_t, system);
}

static void hot_reload_coordination_system_update (kan_context_system_t handle);

void hot_reload_coordination_system_connect (kan_context_system_t handle, kan_context_t context)
{
    struct hot_reload_coordination_system_t *system = KAN_HANDLE_GET (handle);
    system->context = context;

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
    kan_context_system_t application_system = kan_context_query (system->context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (application_system))
    {
        const struct kan_platform_application_event_t *event;
        while ((event = kan_application_system_event_iterator_get (application_system, system->event_iterator)))
        {
            if (event->type == KAN_PLATFORM_APPLICATION_EVENT_TYPE_KEY_UP)
            {
                if (event->keyboard.scan_code == system->config.toggle_hot_key &&
                    event->keyboard.modifiers == system->config.toggle_hot_key_modifiers)
                {
                    system->paused = !system->paused;
                    KAN_LOG (hot_reload_coordination_system, KAN_LOG_INFO, "Hot reload mode: %s.",
                             system->paused ? "paused by user" : "automatic")
                }
            }

            system->event_iterator = kan_application_system_event_iterator_advance (system->event_iterator);
        }
    }
    KAN_ATOMIC_INT_COMPARE_AND_SET (&system->state)
    {
        switch ((enum hot_reload_coordination_state_t) old_value)
        {
        case HOT_RELOAD_COORDINATION_STATE_DORMANT:
        case HOT_RELOAD_COORDINATION_STATE_EXECUTING:
            new_value = old_value;
            break;

        case HOT_RELOAD_COORDINATION_STATE_REQUESTED:
        case HOT_RELOAD_COORDINATION_STATE_DELAYED:
            if (system->paused)
            {
                new_value = HOT_RELOAD_COORDINATION_STATE_DORMANT;
            }
            else
            {
                new_value = HOT_RELOAD_COORDINATION_STATE_SCHEDULED;
            }

            break;

        case HOT_RELOAD_COORDINATION_STATE_SCHEDULED:
            if (system->paused)
            {
                new_value = HOT_RELOAD_COORDINATION_STATE_DORMANT;
            }
            else
            {
                new_value = HOT_RELOAD_COORDINATION_STATE_EXECUTING;
            }

            break;
        }
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
    instance->change_wait_time_ns = 100000000u;
    instance->toggle_hot_key = KAN_PLATFORM_SCAN_CODE_COMMA;
    instance->toggle_hot_key_modifiers = KAN_PLATFORM_MODIFIER_MASK_LEFT_CONTROL;
}

kan_time_offset_t kan_hot_reload_coordination_system_get_change_wait_time_ns (kan_context_system_t system)
{
    struct hot_reload_coordination_system_t *data = KAN_HANDLE_GET (system);
    return data->config.change_wait_time_ns;
}

bool kan_hot_reload_coordination_system_is_possible (void) { return true; }

bool kan_hot_reload_coordination_system_is_reload_allowed (kan_context_system_t system)
{
    struct hot_reload_coordination_system_t *data = KAN_HANDLE_GET (system);
    return !data->paused && kan_atomic_int_get (&data->state) == HOT_RELOAD_COORDINATION_STATE_DORMANT;
}

bool kan_hot_reload_coordination_system_is_scheduled (kan_context_system_t system)
{
    struct hot_reload_coordination_system_t *data = KAN_HANDLE_GET (system);
    switch ((enum hot_reload_coordination_state_t) kan_atomic_int_get (&data->state))
    {
    case HOT_RELOAD_COORDINATION_STATE_DORMANT:
    case HOT_RELOAD_COORDINATION_STATE_EXECUTING:
        // If we're in executing state, we're no longer technically in scheduled state.
        // User logic should be different for that cases.
        return false;

    case HOT_RELOAD_COORDINATION_STATE_REQUESTED:
    case HOT_RELOAD_COORDINATION_STATE_SCHEDULED:
    case HOT_RELOAD_COORDINATION_STATE_DELAYED:
        return true;
    }

    return false;
}

bool kan_hot_reload_coordination_system_is_executing (kan_context_system_t system)
{
    struct hot_reload_coordination_system_t *data = KAN_HANDLE_GET (system);
    return kan_atomic_int_get (&data->state) == HOT_RELOAD_COORDINATION_STATE_EXECUTING;
}

void kan_hot_reload_coordination_system_schedule (kan_context_system_t system)
{
    struct hot_reload_coordination_system_t *data = KAN_HANDLE_GET (system);
    // If failed, that scheduling routine is broken by the user.
    KAN_ASSERT (kan_atomic_int_get (&data->state) == HOT_RELOAD_COORDINATION_STATE_DORMANT)
    kan_atomic_int_set (&data->state, HOT_RELOAD_COORDINATION_STATE_REQUESTED);
}

void kan_hot_reload_coordination_system_delay (kan_context_system_t system)
{
    // If failed, either race condition due to attempt to delay outside of context update routine
    // or broken scheduling routine due to user mistake.
    KAN_ASSERT (kan_hot_reload_coordination_system_is_scheduled (system))
    struct hot_reload_coordination_system_t *data = KAN_HANDLE_GET (system);
    kan_atomic_int_set (&data->state, HOT_RELOAD_COORDINATION_STATE_DELAYED);
}

void kan_hot_reload_coordination_system_finish (kan_context_system_t system)
{
    struct hot_reload_coordination_system_t *data = KAN_HANDLE_GET (system);
    // If failed, that scheduling routine is broken by the user.
    KAN_ASSERT (kan_atomic_int_get (&data->state) == HOT_RELOAD_COORDINATION_STATE_EXECUTING)
    kan_atomic_int_set (&data->state, HOT_RELOAD_COORDINATION_STATE_DORMANT);
}
