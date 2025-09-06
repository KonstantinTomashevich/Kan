#pragma once

#include <file_system_watcher_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/path_container.h>

/// \file
/// \brief Provides API for watching for file system entry changes in file system tree.
///
/// \par File system watcher
/// \parblock
/// File system watcher checks file system for changes in file system tree starting from given directory entry.
/// Change detection is always done on dedicated thread and does not block caller execution, however change detection
/// is only executed when watcher is marked for update using `kan_file_system_watcher_mark_for_update` and update mark
/// is cleaned after successful change detection execution, making this operation essentially on-demand.
///
/// On-demand strategy was selected because in most cases we don't need to watch for changes continuously: we need to
/// detect all changes after important operations like code rebuild or resource rebuild during hot reload. And we'd like
/// to receive only diff changes between these operations, not changes that were detected in-between while operation is
/// being executed. It is crucial to provide proper guarantees for high level code as otherwise it would be very
/// difficult and tedious to process every little case that we shouldn't worry about as we're watching only results
/// of huge operations, not "everything and everywhere".
///
/// As watcher update is asynchronous, it is advised to check `kan_file_system_watcher_is_up_to_date` before reading
/// events as usually it is better to process full bunch of events after big operation at once. However, not doing so
/// will not result in crash or error on watcher side, but user is responsible for processing partial bunches of events.
///
/// Events are stored in event queue like structure and then can be accessed through iterators. Ordering is only
/// guaranteed between several updates, but not inside one update as we have no way to tell the order of changes
/// between operations as it is not usually guaranteed even by OS-level watchers.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// All operations on file system watcher iterators are thread safe. The only requirement is that file system
/// watcher should not be destroyed before all its iterators.
/// \endparblock

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_file_system_watcher_t);
KAN_HANDLE_DEFINE (kan_file_system_watcher_iterator_t);

/// \brief Lists supported file system event types.
/// \details List of events is very minimalistic in order to be supported by all platforms.
enum kan_file_system_watcher_event_type_t
{
    /// \brief File or directory added to tree.
    KAN_FILE_SYSTEM_EVENT_TYPE_ADDED = 0u,

    /// \brief File last modification date changed.
    KAN_FILE_SYSTEM_EVENT_TYPE_MODIFIED,

    /// \brief File or directory removed from tree.
    KAN_FILE_SYSTEM_EVENT_TYPE_REMOVED,
};

/// \brief Describes particular file system event.
struct kan_file_system_watcher_event_t
{
    enum kan_file_system_watcher_event_type_t event_type;
    enum kan_file_system_entry_type_t entry_type;
    struct kan_file_system_path_container_t path_container;
};

/// \brief Creates file system watcher instance for directory at given path.
FILE_SYSTEM_WATCHER_API kan_file_system_watcher_t kan_file_system_watcher_create (const char *directory_path);

/// \brief Requests this watcher to be updated in background thread as soon as possible.
FILE_SYSTEM_WATCHER_API void kan_file_system_watcher_mark_for_update (kan_file_system_watcher_t watcher);

/// \brief Whether this watcher event list is updated. Returns false when still waiting for the update.
FILE_SYSTEM_WATCHER_API bool kan_file_system_watcher_is_up_to_date (kan_file_system_watcher_t watcher);

/// \brief Destroys given file system watcher instance.
FILE_SYSTEM_WATCHER_API void kan_file_system_watcher_destroy (kan_file_system_watcher_t watcher);

/// \brief Creates iterator for observing file system events from given watcher.
FILE_SYSTEM_WATCHER_API kan_file_system_watcher_iterator_t
kan_file_system_watcher_iterator_create (kan_file_system_watcher_t watcher);

/// \brief Extracts current event or NULL if there is no more events using given iterator for given watcher.
FILE_SYSTEM_WATCHER_API const struct kan_file_system_watcher_event_t *kan_file_system_watcher_iterator_get (
    kan_file_system_watcher_t watcher, kan_file_system_watcher_iterator_t iterator);

/// \brief Advances given iterator for given watcher and returns new iterator value.
FILE_SYSTEM_WATCHER_API kan_file_system_watcher_iterator_t kan_file_system_watcher_iterator_advance (
    kan_file_system_watcher_t watcher, kan_file_system_watcher_iterator_t iterator);

/// \brief Destroys given iterator for given watcher.
FILE_SYSTEM_WATCHER_API void kan_file_system_watcher_iterator_destroy (kan_file_system_watcher_t watcher,
                                                                       kan_file_system_watcher_iterator_t iterator);

KAN_C_HEADER_END
