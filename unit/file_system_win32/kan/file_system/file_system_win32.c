#include <kan/api_common/mute_warnings.h>

KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#include <Windows.h>
#include <time.h>
KAN_MUTE_THIRD_PARTY_WARNINGS_END

#include <kan/error/critical.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/path_container.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/precise_time/precise_time.h>

KAN_LOG_DEFINE_CATEGORY (file_system_win32);

enum directory_iterator_state_t
{
    DIRECTORY_ITERATOR_STATE_FIRST_FILE = 0u,
    DIRECTORY_ITERATOR_STATE_IN_THE_MIDDLE,
    DIRECTORY_ITERATOR_STATE_FINISHED,
};

struct directory_iterator_t
{
    enum directory_iterator_state_t state;
    HANDLE *find_handle;
    WIN32_FIND_DATA find_data;
};

static kan_bool_t statics_initialized = KAN_FALSE;
static kan_allocation_group_t allocation_group;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "file_system_win32");
        statics_initialized = KAN_TRUE;
    }
}

kan_file_system_directory_iterator_t kan_file_system_directory_iterator_create (const char *path)
{
    ensure_statics_initialized ();
    struct kan_file_system_path_container_t query_path_container;
    kan_file_system_path_container_copy_string (&query_path_container, path);
    kan_file_system_path_container_append (&query_path_container, "*");

    struct directory_iterator_t *iterator = kan_allocate_general (
        allocation_group, sizeof (struct directory_iterator_t), _Alignof (struct directory_iterator_t));
    iterator->find_handle = FindFirstFile (query_path_container.path, &iterator->find_data);

    if (iterator->find_handle == INVALID_HANDLE_VALUE)
    {
        KAN_LOG (file_system_win32, KAN_LOG_ERROR, "Failed to create directory iterator \"%s\": error code %lu.", path,
                 (unsigned long) GetLastError ())

        kan_free_general (allocation_group, iterator, sizeof (struct directory_iterator_t));
        return KAN_HANDLE_SET_INVALID (kan_file_system_directory_iterator_t);
    }

    iterator->state = DIRECTORY_ITERATOR_STATE_FIRST_FILE;
    return KAN_HANDLE_SET (kan_file_system_directory_iterator_t, iterator);
}

const char *kan_file_system_directory_iterator_advance (kan_file_system_directory_iterator_t iterator)
{
    struct directory_iterator_t *iterator_data = KAN_HANDLE_GET (iterator);
    switch (iterator_data->state)
    {
    case DIRECTORY_ITERATOR_STATE_FIRST_FILE:
        iterator_data->state = DIRECTORY_ITERATOR_STATE_IN_THE_MIDDLE;
        return iterator_data->find_data.cFileName;

    case DIRECTORY_ITERATOR_STATE_IN_THE_MIDDLE:
        if (FindNextFile (iterator_data->find_handle, &iterator_data->find_data))
        {
            return iterator_data->find_data.cFileName;
        }

        iterator_data->state = DIRECTORY_ITERATOR_STATE_FINISHED;
        return NULL;

    case DIRECTORY_ITERATOR_STATE_FINISHED:
        return NULL;
    }

    KAN_ASSERT (KAN_FALSE)
    return NULL;
}

void kan_file_system_directory_iterator_destroy (kan_file_system_directory_iterator_t iterator)
{
    struct directory_iterator_t *iterator_data = KAN_HANDLE_GET (iterator);
    FindClose (iterator_data->find_handle);
    kan_free_general (allocation_group, iterator_data, sizeof (struct directory_iterator_t));
}

kan_bool_t kan_file_system_query_entry (const char *path, struct kan_file_system_entry_status_t *status)
{
    WIN32_FILE_ATTRIBUTE_DATA win32_status;
    if (GetFileAttributesEx (path, GetFileExInfoStandard, &win32_status))
    {
        if (win32_status.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            status->type = KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY;
        }
        else
        {
            status->type = KAN_FILE_SYSTEM_ENTRY_TYPE_FILE;

            LARGE_INTEGER size;
            size.LowPart = win32_status.nFileSizeLow;
            size.HighPart = (LONG) win32_status.nFileSizeHigh;
            status->size = (kan_file_size_t) size.QuadPart;

            // Unfortunately, there is no better way to convert Windows file time to
            // Unix-like time than to do it manually.
#define WINDOWS_TICKS_IN_SECOND 10000000LL
#define SEC_TO_UNIX_EPOCH 11644473600LL
            long long windows_ticks = (((long long) win32_status.ftLastWriteTime.dwHighDateTime) << 32u) |
                                      win32_status.ftLastWriteTime.dwLowDateTime;
            long long unix_like_time_ns =
                (windows_ticks - SEC_TO_UNIX_EPOCH * WINDOWS_TICKS_IN_SECOND) * (1000000000u / WINDOWS_TICKS_IN_SECOND);
            status->last_modification_time_ns = (kan_time_size_t) unix_like_time_ns;
        }

        status->read_only = (win32_status.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? KAN_TRUE : KAN_FALSE;
        return KAN_TRUE;
    }

    // False as a result of query on non-existent file is not treated like an error.
    if (GetLastError () != ERROR_PATH_NOT_FOUND)
    {
        KAN_LOG (file_system_win32, KAN_LOG_ERROR, "Failed to query info about \"%s\": error code %lu.", path,
                 (unsigned long) GetLastError ())
    }

    return KAN_FALSE;
}

kan_bool_t kan_file_system_check_existence (const char *path)
{
    return GetFileAttributes (path) != INVALID_FILE_ATTRIBUTES;
}

kan_bool_t kan_file_system_move_file (const char *from, const char *to)
{
    if (MoveFile (from, to))
    {
        return KAN_TRUE;
    }

    KAN_LOG (file_system_win32, KAN_LOG_ERROR, "Failed to move file \"%s\" tp \"%s\": error code %lu.", from, to,
             (unsigned long) GetLastError ())
    return KAN_FALSE;
}

kan_bool_t kan_file_system_remove_file (const char *path)
{
    if (DeleteFile (path))
    {
        return KAN_TRUE;
    }

    KAN_LOG (file_system_win32, KAN_LOG_ERROR, "Failed to remove file \"%s\": error code %lu.", path,
             (unsigned long) GetLastError ())
    return KAN_FALSE;
}

kan_bool_t kan_file_system_make_directory (const char *path)
{
    if (CreateDirectory (path, NULL) ||
        // Special case; we're okay with directory already existing.
        GetLastError () == ERROR_ALREADY_EXISTS)
    {
        return KAN_TRUE;
    }

    KAN_LOG (file_system_win32, KAN_LOG_ERROR, "Failed to create directory \"%s\": error code %lu.", path,
             (unsigned long) GetLastError ())
    return KAN_FALSE;
}

kan_bool_t kan_file_system_remove_directory_with_content (const char *path)
{
    HANDLE *find_handle;
    WIN32_FIND_DATA find_data;

    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_string (&path_container, path);

    kan_instance_size_t old_length = path_container.length;
    kan_file_system_path_container_append (&path_container, "*");
    find_handle = FindFirstFile (path_container.path, &find_data);
    kan_file_system_path_container_reset_length (&path_container, old_length);

    if (find_handle != INVALID_HANDLE_VALUE)
    {
        do
        {
            if ((find_data.cFileName[0u] == '.' && find_data.cFileName[1u] == '\0') ||
                (find_data.cFileName[0u] == '.' && find_data.cFileName[1u] == '.' && find_data.cFileName[2u] == '\0'))
            {
                // Skip current and parent entries.
                continue;
            }

            old_length = path_container.length;
            kan_file_system_path_container_append (&path_container, find_data.cFileName);

            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (!kan_file_system_remove_directory_with_content (path_container.path))
                {
                    break;
                }
            }
            else
            {
                if (!kan_file_system_remove_file (path_container.path))
                {
                    break;
                }
            }

            kan_file_system_path_container_reset_length (&path_container, old_length);

        } while (FindNextFile (find_handle, &find_data));

        FindClose (find_handle);
    }

    return kan_file_system_remove_empty_directory (path);
}

kan_bool_t kan_file_system_remove_empty_directory (const char *path)
{
    if (RemoveDirectory (path))
    {
        return KAN_TRUE;
    }

    KAN_LOG (file_system_win32, KAN_LOG_ERROR, "Failed to remove directory \"%s\": error code %lu.", path,
             (unsigned long) GetLastError ())
    return KAN_FALSE;
}

kan_bool_t kan_file_system_lock_file_create (const char *directory_path, kan_bool_t blocking)
{
    struct kan_file_system_path_container_t container;
    kan_file_system_path_container_copy_string (&container, directory_path);
    kan_file_system_path_container_append (&container, ".lock");

    while (KAN_TRUE)
    {
        HANDLE file_handle =
            CreateFile (container.path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

        if (file_handle != INVALID_HANDLE_VALUE)
        {
            KAN_LOG (file_system_win32, KAN_LOG_INFO, "Locked directory \"%s\" using lock file.", directory_path)
            CloseHandle (file_handle);
            return KAN_TRUE;
        }

        if (!blocking)
        {
            KAN_LOG (file_system_win32, KAN_LOG_INFO, "Failed to lock directory \"%s\" using lock file.",
                     directory_path)
            break;
        }

        KAN_LOG (file_system_win32, KAN_LOG_INFO,
                 "Failed to lock directory \"%s\" using lock file, waiting for another chance...", directory_path)
        kan_precise_time_sleep (KAN_FILE_SYSTEM_WIN32_LOCK_FILE_WAIT_NS);
    }

    return KAN_FALSE;
}

FILE_SYSTEM_API void kan_file_system_lock_file_destroy (const char *directory_path)
{
    struct kan_file_system_path_container_t container;
    kan_file_system_path_container_copy_string (&container, directory_path);
    kan_file_system_path_container_append (&container, ".lock");
    DeleteFile (container.path);
    KAN_LOG (file_system_win32, KAN_LOG_INFO, "Unlocked directory \"%s\" using lock file.", directory_path)
}
