#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <kan/file_system/entry.h>
#include <kan/file_system/path_container.h>
#include <kan/log/logging.h>
#include <kan/precise_time/precise_time.h>

KAN_LOG_DEFINE_CATEGORY (file_system_linux);

kan_file_system_directory_iterator_t kan_file_system_directory_iterator_create (const char *path)
{
    DIR *directory = opendir (path);
    if (!directory)
    {
        KAN_LOG (file_system_linux, KAN_LOG_ERROR, "Failed to create directory iterator \"%s\": %s.", path,
                 strerror (errno))
        return KAN_HANDLE_SET_INVALID (kan_file_system_directory_iterator_t);
    }

    return KAN_HANDLE_SET (kan_file_system_directory_iterator_t, directory);
}

const char *kan_file_system_directory_iterator_advance (kan_file_system_directory_iterator_t iterator)
{
    struct dirent *unix_entry = readdir (KAN_HANDLE_GET (iterator));
    return unix_entry ? unix_entry->d_name : NULL;
}

void kan_file_system_directory_iterator_destroy (kan_file_system_directory_iterator_t iterator)
{
    closedir (KAN_HANDLE_GET (iterator));
}

bool kan_file_system_query_entry (const char *path, struct kan_file_system_entry_status_t *status)
{
    struct stat unix_status;
    if (stat (path, &unix_status) != 0)
    {
        // False as a result of query on non-existent file is not treated like an error.
        if (errno != ENOENT)
        {
            KAN_LOG (file_system_linux, KAN_LOG_ERROR, "Failed to get status of \"%s\": %s.", path, strerror (errno))
        }

        return false;
    }

    if (S_ISREG (unix_status.st_mode))
    {
        status->type = KAN_FILE_SYSTEM_ENTRY_TYPE_FILE;
    }
    else if (S_ISDIR (unix_status.st_mode))
    {
        status->type = KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY;
    }
    else
    {
        status->type = KAN_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN;
    }

    status->size = unix_status.st_size;
    status->last_modification_time_ns =
        ((kan_time_size_t) unix_status.st_mtim.tv_sec) * 1000000000u + unix_status.st_mtim.tv_nsec;

    status->read_only = unix_status.st_mode & S_IRUSR ? false : true;
    return true;
}

bool kan_file_system_check_existence (const char *path) { return access (path, F_OK) == 0; }

bool kan_file_system_move_file (const char *from, const char *to)
{
    // Linux default rename may overwrite target file. Therefore, we're using link-unlink pair instead.

    if (link (from, to) != 0)
    {
        KAN_LOG (file_system_linux, KAN_LOG_ERROR, "Failed to rename file \"%s\" to \"%s\": link failed.", from, to)
        return false;
    }

    if (unlink (from) != 0)
    {
        KAN_LOG (file_system_linux, KAN_LOG_ERROR, "Failed to rename file \"%s\" to \"%s\": unlink failed.", from, to)
        return false;
    }

    return true;
}

bool kan_file_system_remove_file (const char *path)
{
    if (unlink (path) != 0)
    {
        KAN_LOG (file_system_linux, KAN_LOG_ERROR, "Failed to remove file \"%s\": %s.", path, strerror (errno))
        return false;
    }

    return true;
}

bool kan_file_system_make_directory (const char *path)
{
    if (mkdir (path, S_IRWXU | S_IRWXG | S_IRWXO) != 0)
    {
        if (errno == EEXIST)
        {
            // Special case; we're okay with directory already existing.
            return true;
        }

        KAN_LOG (file_system_linux, KAN_LOG_ERROR, "Failed to create directory \"%s\": %s.", path, strerror (errno))
        return false;
    }

    return true;
}

bool kan_file_system_remove_directory_with_content (const char *path)
{
    // nftw is not supported everywhere, therefore we use our iterator for compatibility.
    kan_file_system_directory_iterator_t iterator = kan_file_system_directory_iterator_create (path);
    if (!KAN_HANDLE_IS_VALID (iterator))
    {
        return false;
    }

    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_string (&path_container, path);
    const char *entry_name;

    while ((entry_name = kan_file_system_directory_iterator_advance (iterator)))
    {
        if ((entry_name[0u] == '.' && entry_name[1u] == '\0') ||
            (entry_name[0u] == '.' && entry_name[1u] == '.' && entry_name[2u] == '\0'))
        {
            // Skip current and parent entries.
            continue;
        }

        struct stat unix_status;
        if (fstatat (dirfd (KAN_HANDLE_GET (iterator)), entry_name, &unix_status, 0) != 0)
        {
            KAN_LOG (file_system_linux, KAN_LOG_ERROR, "Failed to get status of \"%s\": %s.", entry_name,
                     strerror (errno))
            break;
        }

        if (S_ISREG (unix_status.st_mode))
        {
            if (unlinkat (dirfd (KAN_HANDLE_GET (iterator)), entry_name, 0) != 0)
            {
                KAN_LOG (file_system_linux, KAN_LOG_ERROR, "Failed to remove file \"%s\": %s.", path, strerror (errno))
                break;
            }
        }
        else if (S_ISDIR (unix_status.st_mode))
        {
            const kan_instance_size_t old_length = path_container.length;
            kan_file_system_path_container_append (&path_container, entry_name);

            if (!kan_file_system_remove_directory_with_content (path_container.path))
            {
                break;
            }

            kan_file_system_path_container_reset_length (&path_container, old_length);
        }
    }

    kan_file_system_directory_iterator_destroy (iterator);
    return kan_file_system_remove_empty_directory (path);
}

bool kan_file_system_remove_empty_directory (const char *path)
{
    if (rmdir (path) != 0)
    {
        KAN_LOG (file_system_linux, KAN_LOG_ERROR, "Failed to remove directory \"%s\": %s.", path, strerror (errno))
        return false;
    }

    return true;
}

bool kan_file_system_lock_file_create (const char *path, enum kan_file_system_lock_file_flags_t flags)
{
    struct kan_file_system_path_container_t container;
    kan_file_system_path_container_copy_string (&container, path);

    if ((flags & KAN_FILE_SYSTEM_LOCK_FILE_FILE_PATH) == 0u)
    {
        kan_file_system_path_container_append (&container, ".lock");
    }

    while (true)
    {
        int file_descriptor = creat (container.path, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (file_descriptor != -1)
        {
            if ((flags & KAN_FILE_SYSTEM_LOCK_FILE_QUIET) == 0u)
            {
                KAN_LOG (file_system_linux, KAN_LOG_INFO, "Locked path \"%s\" using lock file.", path)
            }

            close (file_descriptor);
            return true;
        }

        if ((flags & KAN_FILE_SYSTEM_LOCK_FILE_BLOCKING) == 0u)
        {
            if ((flags & KAN_FILE_SYSTEM_LOCK_FILE_QUIET) == 0u)
            {
                KAN_LOG (file_system_linux, KAN_LOG_INFO, "Failed to lock path \"%s\" using lock file.", path)
            }

            break;
        }

        if ((flags & KAN_FILE_SYSTEM_LOCK_FILE_QUIET) == 0u)
        {
            KAN_LOG (file_system_linux, KAN_LOG_INFO,
                     "Failed to lock path \"%s\" using lock file, waiting for another chance...", path)
        }

        kan_precise_time_sleep (KAN_FILE_SYSTEM_LINUX_LOCK_FILE_WAIT_NS);
    }

    return false;
}

FILE_SYSTEM_API void kan_file_system_lock_file_destroy (const char *path, enum kan_file_system_lock_file_flags_t flags)
{
    struct kan_file_system_path_container_t container;
    kan_file_system_path_container_copy_string (&container, path);

    if ((flags & KAN_FILE_SYSTEM_LOCK_FILE_FILE_PATH) == 0u)
    {
        kan_file_system_path_container_append (&container, ".lock");
    }

    unlink (container.path);
    if ((flags & KAN_FILE_SYSTEM_LOCK_FILE_QUIET) == 0u)
    {
        KAN_LOG (file_system_linux, KAN_LOG_INFO, "Unlocked path \"%s\" using lock file.", path)
    }
}
