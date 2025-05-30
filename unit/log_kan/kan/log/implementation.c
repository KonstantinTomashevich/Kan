#define _CRT_SECURE_NO_WARNINGS

#define __STDC_WANT_LIB_EXT1__ 1
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/event_queue.h>
#include <kan/container/hash_storage.h>
#include <kan/container/interned_string.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/log/observation.h>
#include <kan/memory/allocation.h>
#include <kan/threading/atomic.h>

struct category_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
    enum kan_log_verbosity_t verbosity;
};

static kan_bool_t category_context_initialized = KAN_FALSE;
static struct kan_atomic_int_t category_context_lock = {0u};
static kan_allocation_group_t category_storage_group;
static struct kan_hash_storage_t category_storage;

kan_log_category_t kan_log_category_get (const char *name)
{
    kan_interned_string_t interned_name = kan_string_intern (name);
    kan_atomic_int_lock (&category_context_lock);

    if (!category_context_initialized)
    {
        category_storage_group = kan_allocation_group_get_child (kan_allocation_group_root (), "log_category");
        kan_hash_storage_init (&category_storage, category_storage_group, KAN_LOG_CATEGORIES_INITIAL_BUCKETS);
        category_context_initialized = KAN_TRUE;
    }

    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&category_storage, KAN_HASH_OBJECT_POINTER (interned_name));
    struct category_node_t *node = (struct category_node_t *) bucket->first;
    const struct category_node_t *end = (struct category_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->name == interned_name)
        {
            // Category already exists.
            kan_atomic_int_unlock (&category_context_lock);
            return KAN_HANDLE_SET (kan_log_category_t, node);
        }

        node = (struct category_node_t *) node->node.list_node.next;
    }

    // Category not found: we will create it with default settings.
    node = (struct category_node_t *) kan_allocate_batched (category_storage_group, sizeof (struct category_node_t));
    node->node.hash = KAN_HASH_OBJECT_POINTER (interned_name);
    node->name = interned_name;
    node->verbosity = KAN_LOG_DEFAULT;

    kan_hash_storage_update_bucket_count_default (&category_storage, KAN_LOG_CATEGORIES_INITIAL_BUCKETS);
    kan_hash_storage_add (&category_storage, &node->node);
    kan_atomic_int_unlock (&category_context_lock);
    return KAN_HANDLE_SET (kan_log_category_t, node);
}

void kan_log_category_set_verbosity (kan_log_category_t category, enum kan_log_verbosity_t verbosity)
{
    struct category_node_t *node = KAN_HANDLE_GET (category);
    node->verbosity = verbosity;
}

enum kan_log_verbosity_t kan_log_category_get_verbosity (kan_log_category_t category)
{
    struct category_node_t *node = KAN_HANDLE_GET (category);
    return node->verbosity;
}

kan_interned_string_t kan_log_category_get_name (kan_log_category_t category)
{
    struct category_node_t *node = KAN_HANDLE_GET (category);
    return node->name;
}

struct callback_t
{
    kan_log_callback_t callback;
    kan_functor_user_data_t user_data;
};

struct event_node_t
{
    struct kan_event_queue_node_t node;
    struct kan_log_event_t event;
};

struct logging_context_t
{
    kan_allocation_group_t main_allocation_group;
    kan_allocation_group_t events_allocation_group;
    struct kan_dynamic_array_t callback_array;
    struct kan_event_queue_t event_queue;
};

static kan_bool_t logging_context_initialized = KAN_FALSE;
static struct kan_atomic_int_t logging_context_lock = {0u};
static struct logging_context_t logging_context;

static struct kan_atomic_int_t formatting_large_buffer_access_lock = {0};
static char formatting_large_buffer[KAN_LOG_MAX_FORMATTING_BUFFER_SIZE];

static struct event_node_t *allocate_event_node (void)
{
    return (struct event_node_t *) kan_allocate_batched (logging_context.events_allocation_group,
                                                         sizeof (struct event_node_t));
}

static void ensure_logging_context_initialized (void)
{
    if (!logging_context_initialized)
    {
        logging_context.main_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "log_context");

        logging_context.events_allocation_group =
            kan_allocation_group_get_child (logging_context.main_allocation_group, "events");

        kan_dynamic_array_init (&logging_context.callback_array, KAN_LOG_CATEGORIES_CALLBACK_ARRAY_INITIAL_SIZE,
                                sizeof (struct callback_t), _Alignof (struct callback_t),
                                logging_context.main_allocation_group);

        struct callback_t *default_callback =
            (struct callback_t *) kan_dynamic_array_add_last (&logging_context.callback_array);
        default_callback->callback = kan_log_default_callback;
        default_callback->user_data = 0u;

        kan_event_queue_init (&logging_context.event_queue, &allocate_event_node ()->node);
        logging_context_initialized = KAN_TRUE;
    }
}

void kan_submit_log (kan_log_category_t category,
                     enum kan_log_verbosity_t verbosity,
                     kan_instance_size_t buffer_size,
                     const char *format,
                     ...)
{
    va_list variadic_arguments;
    va_start (variadic_arguments, format);
    char buffer_static[KAN_LOG_DEFAULT_BUFFER_SIZE];
    char *buffer = buffer_static;

    if (buffer_size > KAN_LOG_DEFAULT_BUFFER_SIZE)
    {
        KAN_ASSERT (buffer_size < KAN_LOG_MAX_FORMATTING_BUFFER_SIZE)
        kan_atomic_int_lock (&formatting_large_buffer_access_lock);
        buffer = formatting_large_buffer;
    }

    vsnprintf (buffer, buffer_size, format, variadic_arguments);
    va_end (variadic_arguments);

    kan_atomic_int_lock (&logging_context_lock);
    ensure_logging_context_initialized ();

    struct timespec time;
    timespec_get (&time, TIME_UTC);

    struct callback_t *callbacks = (struct callback_t *) logging_context.callback_array.data;
    for (kan_loop_size_t callback_index = 0u; callback_index < logging_context.callback_array.size; ++callback_index)
    {
        callbacks[callback_index].callback (category, verbosity, time, buffer, callbacks[callback_index].user_data);
    }

    struct event_node_t *event = (struct event_node_t *) kan_event_queue_submit_begin (&logging_context.event_queue);
    if (event)
    {
        event->event.category = category;
        event->event.verbosity = verbosity;

        event->event.message =
            kan_allocate_general (logging_context.events_allocation_group, strlen (buffer) + 1u, _Alignof (char));
        strcpy (event->event.message, buffer);

        kan_event_queue_submit_end (&logging_context.event_queue, &allocate_event_node ()->node);
    }

    kan_atomic_int_unlock (&logging_context_lock);

    if (buffer != buffer_static)
    {
        kan_atomic_int_unlock (&formatting_large_buffer_access_lock);
    }
}

void kan_log_ensure_initialized (void)
{
    kan_atomic_int_lock (&logging_context_lock);
    ensure_logging_context_initialized ();
    kan_atomic_int_unlock (&logging_context_lock);
}

void kan_log_callback_add (kan_log_callback_t callback, kan_functor_user_data_t user_data)
{
    kan_atomic_int_lock (&logging_context_lock);
    ensure_logging_context_initialized ();

    struct callback_t *callback_item =
        (struct callback_t *) kan_dynamic_array_add_last (&logging_context.callback_array);

    if (!callback_item)
    {
        kan_dynamic_array_set_capacity (&logging_context.callback_array, logging_context.callback_array.capacity * 2u);
        callback_item = (struct callback_t *) kan_dynamic_array_add_last (&logging_context.callback_array);
    }

    callback_item->callback = callback;
    callback_item->user_data = user_data;
    kan_atomic_int_unlock (&logging_context_lock);
}

void kan_log_callback_remove (kan_log_callback_t callback, kan_functor_user_data_t user_data)
{
    kan_atomic_int_lock (&logging_context_lock);
    ensure_logging_context_initialized ();

    struct callback_t *callbacks = (struct callback_t *) logging_context.callback_array.data;
    for (kan_loop_size_t callback_index = 0u; callback_index < logging_context.callback_array.size; ++callback_index)
    {
        if (callbacks[callback_index].callback == callback && callbacks[callback_index].user_data == user_data)
        {
            kan_dynamic_array_remove_swap_at (&logging_context.callback_array, callback_index);
            break;
        }
    }

    kan_atomic_int_unlock (&logging_context_lock);
}

void kan_log_default_callback (kan_log_category_t category,
                               enum kan_log_verbosity_t verbosity,
                               struct timespec time,
                               const char *message,
                               kan_functor_user_data_t user_data)
{
    char date_time_string[18u];
    struct tm local_time;
#if defined(_MSC_VER)
    localtime_s (&local_time, &time.tv_sec);
#else
    localtime_r (&time.tv_sec, &local_time);
#endif
    strftime (date_time_string, 18u, "%D %T", &local_time);

    FILE *output = (FILE *) user_data;
    if (!output)
    {
        switch (verbosity)
        {
        case KAN_LOG_VERBOSE:
        case KAN_LOG_DEBUG:
        case KAN_LOG_INFO:
        case KAN_LOG_WARNING:
            output = stdout;
            break;

        case KAN_LOG_ERROR:
        case KAN_LOG_CRITICAL_ERROR:
            output = stderr;
            break;
        }
    }

    switch (verbosity)
    {
    case KAN_LOG_VERBOSE:
        fprintf (output, "[%s.%09ld] [%s] [verbose] %s\n", date_time_string, time.tv_nsec,
                 kan_log_category_get_name (category), message);
        break;

    case KAN_LOG_DEBUG:
        fprintf (output, "[%s.%09ld] [%s] [debug] %s\n", date_time_string, time.tv_nsec,
                 kan_log_category_get_name (category), message);
        break;

    case KAN_LOG_INFO:
        fprintf (output, "[%s.%09ld] [%s] [info] %s\n", date_time_string, time.tv_nsec,
                 kan_log_category_get_name (category), message);
        break;

    case KAN_LOG_WARNING:
        fprintf (output, "[%s.%09ld] [%s] [warning] %s\n", date_time_string, time.tv_nsec,
                 kan_log_category_get_name (category), message);
        break;

    case KAN_LOG_ERROR:
        fprintf (output, "[%s.%09ld] [%s] [error] %s\n", date_time_string, time.tv_nsec,
                 kan_log_category_get_name (category), message);
        break;

    case KAN_LOG_CRITICAL_ERROR:
        fprintf (output, "[%s.%09ld] [%s] [critical_error] %s\n", date_time_string, time.tv_nsec,
                 kan_log_category_get_name (category), message);
        break;
    }

    if (user_data != 0u)
    {
        switch (verbosity)
        {
        case KAN_LOG_VERBOSE:
        case KAN_LOG_DEBUG:
        case KAN_LOG_INFO:
        case KAN_LOG_WARNING:
            break;

        case KAN_LOG_ERROR:
        case KAN_LOG_CRITICAL_ERROR:
            fflush (output);
            break;
        }
    }
}

kan_log_event_iterator_t kan_log_event_iterator_create (void)
{
    kan_atomic_int_lock (&logging_context_lock);
    ensure_logging_context_initialized ();

    kan_log_event_iterator_t iterator = KAN_HANDLE_TRANSIT (
        kan_log_event_iterator_t, KAN_HANDLE_TRANSIT (kan_event_queue_iterator_t,
                                                      kan_event_queue_iterator_create (&logging_context.event_queue)));

    kan_atomic_int_unlock (&logging_context_lock);
    return iterator;
}

const struct kan_log_event_t *kan_log_event_iterator_get (kan_log_event_iterator_t iterator)
{
    KAN_ASSERT (logging_context_initialized)
    kan_atomic_int_lock (&logging_context_lock);

    struct event_node_t *node = (struct event_node_t *) kan_event_queue_iterator_get (
        &logging_context.event_queue, KAN_HANDLE_TRANSIT (kan_event_queue_iterator_t, iterator));

    kan_atomic_int_unlock (&logging_context_lock);
    return node ? &node->event : NULL;
}

static void cleanup_event_queue (void)
{
    struct event_node_t *node;
    while ((node = (struct event_node_t *) kan_event_queue_clean_oldest (&logging_context.event_queue)))
    {
        kan_free_general (logging_context.events_allocation_group, node->event.message,
                          strlen (node->event.message) + 1u);
        kan_free_batched (logging_context.events_allocation_group, node);
    }
}

kan_log_event_iterator_t kan_log_event_iterator_advance (kan_log_event_iterator_t iterator)
{
    KAN_ASSERT (logging_context_initialized)
    kan_atomic_int_lock (&logging_context_lock);

    iterator = KAN_HANDLE_TRANSIT (kan_log_event_iterator_t, kan_event_queue_iterator_advance (KAN_HANDLE_TRANSIT (
                                                                 kan_event_queue_iterator_t, iterator)));
    cleanup_event_queue ();

    kan_atomic_int_unlock (&logging_context_lock);
    return iterator;
}

void kan_log_event_iterator_destroy (kan_log_event_iterator_t iterator)
{
    KAN_ASSERT (logging_context_initialized)
    kan_atomic_int_lock (&logging_context_lock);

    kan_event_queue_iterator_destroy (&logging_context.event_queue,
                                      KAN_HANDLE_TRANSIT (kan_event_queue_iterator_t, iterator));
    cleanup_event_queue ();

    kan_atomic_int_unlock (&logging_context_lock);
}
