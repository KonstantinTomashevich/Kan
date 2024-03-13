#pragma once

#include <file_system_watcher_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/file_system/path_container.h>

/// \file
/// \brief Provides API for watching for file system entry changes in file system tree.
///
/// \par File system watcher
/// \parblock
/// File system watcher checks file system for changes in file system tree starting from given directory entry.
/// Events are stored in event queue like structure and then can be accessed through iterators.
/// Keep in mind that events might be delayed due to how underlying implementation works (even OS-specific routines
/// usually do not offer realtime delivery of file system events). Ordering is also not guaranteed.
/// \endparblock

KAN_C_HEADER_BEGIN

typedef uint64_t kan_file_system_watcher_t;

#define KAN_INVALID_FILE_SYSTEM_WATCHER 0u

typedef uint64_t kan_file_system_watcher_iterator_t;

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
