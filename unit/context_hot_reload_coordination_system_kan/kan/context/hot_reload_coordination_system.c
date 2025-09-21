#include <kan/context/all_system_names.h>
#include <kan/context/application_system.h>
#include <kan/context/hot_reload_coordination_system.h>
#include <kan/context/update_system.h>
#include <kan/context/virtual_file_system.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/precise_time/precise_time.h>
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

struct hot_reload_real_file_system_watcher_t
{
    struct hot_reload_real_file_system_watcher_t *previous;
    struct hot_reload_real_file_system_watcher_t *next;
    struct hot_reload_coordination_system_t *system;

    kan_file_system_watcher_t watcher;
    kan_file_system_watcher_iterator_t iterator;
};

struct hot_reload_virtual_file_system_watcher_t
{
    struct hot_reload_virtual_file_system_watcher_t *previous;
    struct hot_reload_virtual_file_system_watcher_t *next;
    struct hot_reload_coordination_system_t *system;

    kan_virtual_file_system_watcher_t watcher;
    kan_virtual_file_system_watcher_iterator_t iterator;
};

enum hot_reload_file_watcher_state_t
{
    HOT_RELOAD_FILE_WATCHER_STATE_AVAILABLE = 0u,

    /// \brief Block by ongoing build execution.
    HOT_RELOAD_FILE_WATCHER_STATE_BLOCKED,

    /// \brief Waiting for some time after hot reload build routine to make sure that
    ///        file system has acknowledged all the changes.
    HOT_RELOAD_FILE_WATCHER_STATE_SAFE_WAIT,

    /// \brief Waiting for all the watchers to be up to date.
    HOT_RELOAD_FILE_WATCHER_STATE_UPDATING,

    /// \brief Additional availability window after update so everything could start loading prior to any possible
    ///        new hot reload build request,
    HOT_RELOAD_FILE_WATCHER_STATE_RECEIVE_WINDOW,
};

struct hot_reload_coordination_system_t
{
    kan_context_t context;
    kan_allocation_group_t group;

    struct kan_atomic_int_t state;
    bool paused;

    enum hot_reload_file_watcher_state_t watcher_state;
    kan_time_size_t watcher_state_transition_after_ns;

    struct kan_atomic_int_t watcher_list_lock;
    struct hot_reload_real_file_system_watcher_t *first_real_watcher;
    struct hot_reload_virtual_file_system_watcher_t *first_virtual_watcher;
    kan_context_system_t context_virtual_file_system;

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

    system->watcher_state = HOT_RELOAD_FILE_WATCHER_STATE_AVAILABLE;
    system->watcher_state_transition_after_ns = 0u;

    system->watcher_list_lock = kan_atomic_int_init (0);
    system->first_real_watcher = NULL;
    system->first_virtual_watcher = NULL;

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
    struct hot_reload_coordination_system_t *system = KAN_HANDLE_GET (handle);
    system->context_virtual_file_system = kan_context_query (system->context, KAN_CONTEXT_VIRTUAL_FILE_SYSTEM_NAME);
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

    const kan_time_size_t current_time_ns = kan_precise_time_get_elapsed_nanoseconds ();
    switch (system->watcher_state)
    {
    case HOT_RELOAD_FILE_WATCHER_STATE_AVAILABLE:
        break;

    case HOT_RELOAD_FILE_WATCHER_STATE_BLOCKED:
        if (kan_atomic_int_get (&system->state) != HOT_RELOAD_COORDINATION_STATE_EXECUTING)
        {
            system->watcher_state = HOT_RELOAD_FILE_WATCHER_STATE_SAFE_WAIT;
            system->watcher_state_transition_after_ns = current_time_ns + system->config.change_wait_time_ns;
        }

        break;

    case HOT_RELOAD_FILE_WATCHER_STATE_SAFE_WAIT:
        if (current_time_ns > system->watcher_state_transition_after_ns)
        {
            struct hot_reload_real_file_system_watcher_t *real_watcher = system->first_real_watcher;
            while (real_watcher)
            {
                kan_file_system_watcher_mark_for_update (real_watcher->watcher);
                real_watcher = real_watcher->next;
            }

            struct hot_reload_virtual_file_system_watcher_t *virtual_watcher = system->first_virtual_watcher;
            while (virtual_watcher)
            {
                kan_virtual_file_system_watcher_mark_for_update (virtual_watcher->watcher);
                virtual_watcher = virtual_watcher->next;
            }

            system->watcher_state = HOT_RELOAD_FILE_WATCHER_STATE_UPDATING;
        }

        break;

    case HOT_RELOAD_FILE_WATCHER_STATE_UPDATING:
    {
        bool up_to_date = true;
        struct hot_reload_real_file_system_watcher_t *real_watcher = system->first_real_watcher;

        while (real_watcher)
        {
            if (!(up_to_date &= kan_file_system_watcher_is_up_to_date (real_watcher->watcher)))
            {
                break;
            }

            real_watcher = real_watcher->next;
        }

        struct hot_reload_virtual_file_system_watcher_t *virtual_watcher = system->first_virtual_watcher;
        while (virtual_watcher)
        {
            if (!(up_to_date &= kan_virtual_file_system_watcher_is_up_to_date (virtual_watcher->watcher)))
            {
                break;
            }

            virtual_watcher = virtual_watcher->next;
        }

        if (up_to_date)
        {
            system->watcher_state = HOT_RELOAD_FILE_WATCHER_STATE_RECEIVE_WINDOW;
            system->watcher_state_transition_after_ns = current_time_ns + system->config.receive_window_time_ns;
        }

        break;
    }

    case HOT_RELOAD_FILE_WATCHER_STATE_RECEIVE_WINDOW:
        if (current_time_ns > system->watcher_state_transition_after_ns)
        {
            system->watcher_state = HOT_RELOAD_FILE_WATCHER_STATE_AVAILABLE;
        }

        break;
    }

    const enum hot_reload_file_watcher_state_t old_watcher_state = system->watcher_state;
    KAN_ATOMIC_INT_COMPARE_AND_SET (&system->state)
    {
        system->watcher_state = old_watcher_state;
        switch ((enum hot_reload_coordination_state_t) old_value)
        {
        case HOT_RELOAD_COORDINATION_STATE_DORMANT:
        case HOT_RELOAD_COORDINATION_STATE_EXECUTING:
            new_value = old_value;
            break;

        case HOT_RELOAD_COORDINATION_STATE_REQUESTED:
        case HOT_RELOAD_COORDINATION_STATE_DELAYED:
            if (system->paused || system->watcher_state != HOT_RELOAD_FILE_WATCHER_STATE_AVAILABLE)
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
                system->watcher_state = HOT_RELOAD_FILE_WATCHER_STATE_BLOCKED;
                new_value = HOT_RELOAD_COORDINATION_STATE_EXECUTING;
            }

            break;
        }
    }
}

static void hot_reload_real_file_system_watcher_destroy (struct hot_reload_real_file_system_watcher_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->watcher))
    {
        kan_file_system_watcher_iterator_destroy (instance->watcher, instance->iterator);
        kan_file_system_watcher_destroy (instance->watcher);
    }

    kan_free_general (instance->system->group, instance, sizeof (struct hot_reload_real_file_system_watcher_t));
}

static void hot_reload_virtual_file_system_watcher_destroy (struct hot_reload_virtual_file_system_watcher_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->watcher))
    {
        kan_virtual_file_system_watcher_iterator_destroy (instance->watcher, instance->iterator);
        kan_virtual_file_system_watcher_destroy (instance->watcher);
    }

    kan_free_general (instance->system->group, instance, sizeof (struct hot_reload_virtual_file_system_watcher_t));
}

void hot_reload_coordination_system_shutdown (kan_context_system_t handle)
{
    struct hot_reload_coordination_system_t *system = KAN_HANDLE_GET (handle);
    struct hot_reload_real_file_system_watcher_t *real_watcher = system->first_real_watcher;

    while (real_watcher)
    {
        struct hot_reload_real_file_system_watcher_t *next = real_watcher->next;
        hot_reload_real_file_system_watcher_destroy (real_watcher);
        real_watcher = next;
    }

    struct hot_reload_virtual_file_system_watcher_t *virtual_watcher = system->first_virtual_watcher;
    while (virtual_watcher)
    {
        struct hot_reload_virtual_file_system_watcher_t *next = virtual_watcher->next;
        hot_reload_virtual_file_system_watcher_destroy (virtual_watcher);
        virtual_watcher = next;
    }
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
    instance->receive_window_time_ns = 50000000u;
    instance->toggle_hot_key = KAN_PLATFORM_SCAN_CODE_COMMA;
    instance->toggle_hot_key_modifiers = KAN_PLATFORM_MODIFIER_MASK_LEFT_CONTROL;
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

kan_hot_reload_file_event_provider_t kan_hot_reload_file_event_provider_create (kan_context_system_t system,
                                                                                const char *path)
{
    struct hot_reload_coordination_system_t *data = KAN_HANDLE_GET (system);
    KAN_ATOMIC_INT_SCOPED_LOCK (&data->watcher_list_lock)

    struct hot_reload_real_file_system_watcher_t *real_watcher =
        kan_allocate_general (data->group, sizeof (struct hot_reload_real_file_system_watcher_t),
                              alignof (struct hot_reload_real_file_system_watcher_t));

    real_watcher->previous = NULL;
    real_watcher->system = data;

    real_watcher->watcher = kan_file_system_watcher_create (path);
    real_watcher->iterator = kan_file_system_watcher_iterator_create (real_watcher->watcher);

    real_watcher->next = data->first_real_watcher;
    data->first_real_watcher = real_watcher;
    return KAN_HANDLE_SET (kan_hot_reload_file_event_provider_t, real_watcher);
}

const struct kan_file_system_watcher_event_t *kan_hot_reload_file_event_provider_get (
    kan_hot_reload_file_event_provider_t provider)
{
    struct hot_reload_real_file_system_watcher_t *data = KAN_HANDLE_GET (provider);
    switch (data->system->watcher_state)
    {
    case HOT_RELOAD_FILE_WATCHER_STATE_AVAILABLE:
    case HOT_RELOAD_FILE_WATCHER_STATE_RECEIVE_WINDOW:
        return kan_file_system_watcher_iterator_get (data->watcher, data->iterator);

    case HOT_RELOAD_FILE_WATCHER_STATE_BLOCKED:
    case HOT_RELOAD_FILE_WATCHER_STATE_SAFE_WAIT:
    case HOT_RELOAD_FILE_WATCHER_STATE_UPDATING:
        return NULL;
    }

    return NULL;
}

void kan_hot_reload_file_event_provider_advance (kan_hot_reload_file_event_provider_t provider)
{
    struct hot_reload_real_file_system_watcher_t *data = KAN_HANDLE_GET (provider);
    switch (data->system->watcher_state)
    {
    case HOT_RELOAD_FILE_WATCHER_STATE_AVAILABLE:
    case HOT_RELOAD_FILE_WATCHER_STATE_RECEIVE_WINDOW:
        data->iterator = kan_file_system_watcher_iterator_advance (data->watcher, data->iterator);
        break;

    case HOT_RELOAD_FILE_WATCHER_STATE_BLOCKED:
    case HOT_RELOAD_FILE_WATCHER_STATE_SAFE_WAIT:
    case HOT_RELOAD_FILE_WATCHER_STATE_UPDATING:
        break;
    }
}

void kan_hot_reload_file_event_provider_destroy (kan_hot_reload_file_event_provider_t provider)
{
    struct hot_reload_real_file_system_watcher_t *data = KAN_HANDLE_GET (provider);
    struct hot_reload_coordination_system_t *system = data->system;
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->watcher_list_lock)

    if (data->previous)
    {
        data->previous = data->next;
    }
    else
    {
        data->system->first_real_watcher = data->next;
    }

    if (data->next)
    {
        data->next->previous = data->previous;
    }

    hot_reload_real_file_system_watcher_destroy (data);
}

kan_hot_reload_virtual_file_event_provider_t kan_hot_reload_virtual_file_event_provider_create (
    kan_context_system_t system, const char *path)
{
    struct hot_reload_coordination_system_t *data = KAN_HANDLE_GET (system);

    if (!KAN_HANDLE_IS_VALID (data->context_virtual_file_system))
    {
        return KAN_HANDLE_SET_INVALID (kan_hot_reload_virtual_file_event_provider_t);
    }

    kan_virtual_file_system_watcher_t watcher = KAN_HANDLE_INITIALIZE_INVALID;
    kan_virtual_file_system_watcher_iterator_t iterator = KAN_HANDLE_INITIALIZE_INVALID;

    {
        kan_virtual_file_system_volume_t volume =
            kan_virtual_file_system_get_context_volume_for_write (data->context_virtual_file_system);
        CUSHION_DEFER { kan_virtual_file_system_close_context_write_access (data->context_virtual_file_system); }

        watcher = kan_virtual_file_system_watcher_create (volume, path);
        if (!KAN_HANDLE_IS_VALID (watcher))
        {
            KAN_LOG (hot_reload_coordination_system, KAN_LOG_ERROR,
                     "Failed to create virtual file system watcher at path \"%s\".", path)
            return KAN_HANDLE_SET_INVALID (kan_hot_reload_virtual_file_event_provider_t);
        }

        iterator = kan_virtual_file_system_watcher_iterator_create (watcher);
    }

    KAN_ATOMIC_INT_SCOPED_LOCK (&data->watcher_list_lock)
    struct hot_reload_virtual_file_system_watcher_t *virtual_watcher =
        kan_allocate_general (data->group, sizeof (struct hot_reload_virtual_file_system_watcher_t),
                              alignof (struct hot_reload_virtual_file_system_watcher_t));

    virtual_watcher->previous = NULL;
    virtual_watcher->system = data;

    virtual_watcher->watcher = watcher;
    virtual_watcher->iterator = iterator;

    virtual_watcher->next = data->first_virtual_watcher;
    data->first_virtual_watcher = virtual_watcher;
    return KAN_HANDLE_SET (kan_hot_reload_virtual_file_event_provider_t, virtual_watcher);
}

const struct kan_virtual_file_system_watcher_event_t *kan_hot_reload_virtual_file_event_provider_get (
    kan_hot_reload_virtual_file_event_provider_t provider)
{
    struct hot_reload_virtual_file_system_watcher_t *data = KAN_HANDLE_GET (provider);
    switch (data->system->watcher_state)
    {
    case HOT_RELOAD_FILE_WATCHER_STATE_AVAILABLE:
    case HOT_RELOAD_FILE_WATCHER_STATE_RECEIVE_WINDOW:
        return kan_virtual_file_system_watcher_iterator_get (data->watcher, data->iterator);

    case HOT_RELOAD_FILE_WATCHER_STATE_BLOCKED:
    case HOT_RELOAD_FILE_WATCHER_STATE_SAFE_WAIT:
    case HOT_RELOAD_FILE_WATCHER_STATE_UPDATING:
        return NULL;
    }

    return NULL;
}

void kan_hot_reload_virtual_file_event_provider_advance (kan_hot_reload_virtual_file_event_provider_t provider)
{
    struct hot_reload_virtual_file_system_watcher_t *data = KAN_HANDLE_GET (provider);
    switch (data->system->watcher_state)
    {
    case HOT_RELOAD_FILE_WATCHER_STATE_AVAILABLE:
    case HOT_RELOAD_FILE_WATCHER_STATE_RECEIVE_WINDOW:
        data->iterator = kan_virtual_file_system_watcher_iterator_advance (data->watcher, data->iterator);
        break;

    case HOT_RELOAD_FILE_WATCHER_STATE_BLOCKED:
    case HOT_RELOAD_FILE_WATCHER_STATE_SAFE_WAIT:
    case HOT_RELOAD_FILE_WATCHER_STATE_UPDATING:
        break;
    }
}

void kan_hot_reload_virtual_file_event_provider_destroy (kan_hot_reload_virtual_file_event_provider_t provider)
{
    struct hot_reload_virtual_file_system_watcher_t *data = KAN_HANDLE_GET (provider);
    struct hot_reload_coordination_system_t *system = data->system;
    KAN_ATOMIC_INT_SCOPED_LOCK (&system->watcher_list_lock)

    if (data->previous)
    {
        data->previous = data->next;
    }
    else
    {
        data->system->first_virtual_watcher = data->next;
    }

    if (data->next)
    {
        data->next->previous = data->previous;
    }

    hot_reload_virtual_file_system_watcher_destroy (data);
}
