#pragma once

#include <file_system_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>

/// \file
/// \brief Contains API for working with file system entries.

KAN_C_HEADER_BEGIN

/// \brief Describes supported types of file system entries.
/// \details Minimalistic in order to be supported by all platforms.
enum kan_file_system_entry_type_t
{
    KAN_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN,
    KAN_FILE_SYSTEM_ENTRY_TYPE_FILE,
    KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY,
};

KAN_HANDLE_DEFINE (kan_file_system_directory_iterator_t);

/// \brief Describes file system entry status.
/// \details Minimalistic in order to be supported by all platforms.
struct kan_file_system_entry_status_t
{
    enum kan_file_system_entry_type_t type;

    /// \warning Not used for KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY.
    kan_file_size_t size;

    /// \brief In nanoseconds from Unix epoch in UTC zone.
    /// \warning Not used for KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY.
    kan_time_size_t last_modification_time_ns;

    /// \warning Not used for KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY.
    kan_bool_t read_only;
};

/// \brief Creates new directory content iterator for directory at given path.
FILE_SYSTEM_API kan_file_system_directory_iterator_t kan_file_system_directory_iterator_create (const char *path);

/// \brief Advances directory content iterator and returns next entry name or NULL if there are no more entries.
FILE_SYSTEM_API const char *kan_file_system_directory_iterator_advance (kan_file_system_directory_iterator_t iterator);

/// \brief Frees given directory content iterator.
FILE_SYSTEM_API void kan_file_system_directory_iterator_destroy (kan_file_system_directory_iterator_t iterator);

/// \brief Queries status of entry at given path. Returns KAN_TRUE on success. Returns KAN_FALSE if file does not exist.
FILE_SYSTEM_API kan_bool_t kan_file_system_query_entry (const char *path,
                                                        struct kan_file_system_entry_status_t *status);

/// \brief Queries whether entry at given path exists.
FILE_SYSTEM_API kan_bool_t kan_file_system_check_existence (const char *path);

/// \brief Attempts to move file entry from one path to another. Returns KAN_TRUE on success.
FILE_SYSTEM_API kan_bool_t kan_file_system_move_file (const char *from, const char *to);

/// \brief Attempts to remove file entry at given path. Returns KAN_TRUE on success.
FILE_SYSTEM_API kan_bool_t kan_file_system_remove_file (const char *path);

/// \brief Attempts to create directory entry at given path. Returns KAN_TRUE on success.
/// \warning We treat failure due to the existence of directory as success, as it is much more convenient for tools.
FILE_SYSTEM_API kan_bool_t kan_file_system_make_directory (const char *path);

/// \brief Attempts to remove directory entry and all its children at given path. Returns KAN_TRUE on success.
FILE_SYSTEM_API kan_bool_t kan_file_system_remove_directory_with_content (const char *path);

/// \brief Attempts to remove empty directory entry at given path. Returns KAN_TRUE on success.
FILE_SYSTEM_API kan_bool_t kan_file_system_remove_empty_directory (const char *path);

/// \brief Flags for lock file management functions.
enum kan_file_system_lock_file_flags_t
{
    /// \brief Syntax sugar for no flags value.
    KAN_FILE_SYSTEM_LOCK_FILE_NONE = 0u,

    /// \brief Given lock path is a path to lock file, not directory to lock.
    KAN_FILE_SYSTEM_LOCK_FILE_FILE_PATH = 1u << 0u,

    /// \brief Blocks execution until successful lock, retries every once in a while.
    KAN_FILE_SYSTEM_LOCK_FILE_BLOCKING = 1u << 1u,

    /// \brief Disables logging in lock file management functions.
    KAN_FILE_SYSTEM_LOCK_FILE_QUIET = 1u << 2u,
};

/// \brief Creates lock file in one system call, should be used as a cross-process locking mechanism.
/// \details
FILE_SYSTEM_API kan_bool_t kan_file_system_lock_file_create (const char *path,
                                                             enum kan_file_system_lock_file_flags_t flags);

/// \brief Removes .lock file created by kan_file_system_lock_file_create.
FILE_SYSTEM_API void kan_file_system_lock_file_destroy (const char *path, enum kan_file_system_lock_file_flags_t flags);

KAN_C_HEADER_END
