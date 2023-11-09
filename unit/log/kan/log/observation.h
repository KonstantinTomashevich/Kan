#pragma once

#include <log_api.h>

#include <time.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Provides API for observing logging activity.
///
/// \par Callbacks
/// \parblock
/// Logging callbacks are called under the logging lock right when log message is submitted. They function like log
/// sinks and their primary goal is to deliver log message to the right receiver. For example, to print log message
/// on console or to write it to file. It is advised to use log callbacks only when you need to react immediately,
/// for example to flush error log to file, and avoid using callbacks when immediate response is not needed.
/// \endparblock
///
/// \par Events
/// \parblock
/// Log events are designed for the cases when immediate response is not required, for example when we're redirecting
/// log message to user interface of some editor application. Log events follow the event queue pattern: user creates
/// iterator and then uses it to iterate over unread events.
/// \endparblock

KAN_C_HEADER_BEGIN

typedef void (*kan_log_callback_t) (kan_log_category_t category,
                                    enum kan_log_verbosity_t verbosity,
                                    struct timespec time,
                                    const char *message,
                                    uint64_t user_data);

/// \brief Adds new callback to callback sequence.
LOG_API void kan_log_callback_add (kan_log_callback_t callback, uint64_t user_data);

/// \brief Removes callback with given user data from callback sequence.
LOG_API void kan_log_callback_remove (kan_log_callback_t callback, uint64_t user_data);

/// \brief Default callback that should be always added automatically, but can be removed manually.
LOG_API void kan_log_default_callback (kan_log_category_t category,
                                       enum kan_log_verbosity_t verbosity,
                                       struct timespec time,
                                       const char *message,
                                       uint64_t user_data);

/// \brief Describes single event from log event queue.
struct kan_log_event_t
{
    kan_log_category_t category;
    enum kan_log_verbosity_t verbosity;
    struct timespec time;
    char *message;
};

typedef uint64_t kan_log_event_iterator_t;

/// \brief Creates new iterator for log events.
LOG_API kan_log_event_iterator_t kan_log_event_iterator_create (void);

/// \brief Returns next log event from iterator or `NULL` if there are new events.
LOG_API const struct kan_log_event_t *kan_log_event_iterator_get (kan_log_event_iterator_t iterator);

/// \brief Advances given iterator to next event and returns new iterator handle.
LOG_API kan_log_event_iterator_t kan_log_event_iterator_advance (kan_log_event_iterator_t iterator);

/// \brief Destroys log event iterator and frees used resources.
LOG_API void kan_log_event_iterator_destroy (kan_log_event_iterator_t iterator);

KAN_C_HEADER_END
