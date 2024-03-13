#include <kan/container/event_queue.h>
#include <kan/container/interned_string.h>
#include <kan/cpu_dispatch/task.h>
#include <kan/error/critical.h>
#include <kan/file_system/entry.h>
#include <kan/file_system_watcher/watcher.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>

KAN_LOG_DEFINE_CATEGORY (file_system_watcher);

// Interned strings are used for files and directories for following reasons:
// - In asset tree there are usually lots of common patterns, therefore we could have lots of directories with the
//   same name in different parts of tree and save memory by interning.
// - File names are frequently used as resource IDs, therefore they're almost always used by other systems as interned
//   strings already. It means that we can save memory by separating name into interned string.
// - Extensions are usually really-really common, therefore it is logical to intern them separately from names.

struct file_node_t
{
    struct file_node_t *next;
    kan_interned_string_t name;
    kan_interned_string_t extension;
    uint64_t last_modification_time_ns;
    kan_bool_t mark_found;
};

struct directory_node_t
{
    struct directory_node_t *next_on_level_directory;
    struct directory_node_t *first_child_directory;
    struct file_node_t *first_file;
    kan_interned_string_t name;
    kan_bool_t mark_found;
};

struct event_queue_node_t
{
    struct kan_event_queue_node_t node;
    struct kan_file_system_watcher_event_t event;
};

struct watcher_t
{
    struct directory_node_t *root_directory;
    struct kan_atomic_int_t event_queue_lock;
    struct kan_event_queue_t event_queue;
    struct kan_atomic_int_t marked_for_destroy;

    /// \details Stores initial path by default. Used by recursive algorithms to store current recurrent path.
    struct kan_file_system_path_container_t path_container;
};

static kan_bool_t statics_initialized = KAN_FALSE;
static struct kan_atomic_int_t statics_initialization_lock = {.value = 0};

static kan_interned_string_t cpu_task_name;
static kan_allocation_group_t watcher_allocation_group;
static kan_allocation_group_t hierarchy_allocation_group;
static kan_allocation_group_t event_allocation_group;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        kan_atomic_int_lock (&statics_initialization_lock);
        if (!statics_initialized)
        {
            cpu_task_name = kan_string_intern ("file_system_poll");
            watcher_allocation_group =
                kan_allocation_group_get_child (kan_allocation_group_root (), "file_system_watcher");
            hierarchy_allocation_group = kan_allocation_group_get_child (watcher_allocation_group, "hierarchy");
            event_allocation_group = kan_allocation_group_get_child (watcher_allocation_group, "event");
            statics_initialized = KAN_TRUE;
        }

        kan_atomic_int_unlock (&statics_initialization_lock);
    }
}

static struct event_queue_node_t *allocate_event_queue_node (void)
{
    return (struct event_queue_node_t *) kan_allocate_general (
        event_allocation_group, sizeof (struct event_queue_node_t), _Alignof (struct event_queue_node_t));
}

static void free_event_queue_node (struct event_queue_node_t *node)
{
    kan_free_general (event_allocation_group, node, sizeof (struct event_queue_node_t));
}

static inline struct file_node_t *file_node_allocate (void)
{
    return (struct file_node_t *) kan_allocate_batched (hierarchy_allocation_group, sizeof (struct file_node_t));
}

static inline void file_node_free (struct file_node_t *node)
{
    kan_free_batched (hierarchy_allocation_group, node);
}

static struct directory_node_t *directory_node_create (kan_interned_string_t name)
{
    struct directory_node_t *node =
        (struct directory_node_t *) kan_allocate_batched (hierarchy_allocation_group, sizeof (struct directory_node_t));
    node->name = name;
    node->first_child_directory = NULL;
    node->next_on_level_directory = NULL;
    node->first_file = NULL;
    node->mark_found = KAN_TRUE;
    return node;
}

static void directory_node_destroy (struct directory_node_t *node)
{
    while (node->first_file)
    {
        struct file_node_t *next = node->first_file->next;
        file_node_free (node->first_file);
        node->first_file = next;
    }

    while (node->first_child_directory)
    {
        struct directory_node_t *next = node->first_child_directory->next_on_level_directory;
        directory_node_destroy (node->first_child_directory);
        node->first_child_directory = next;
    }

    kan_free_batched (hierarchy_allocation_group, node);
}

static void poll_task_function (uint64_t user_data);

static void schedule_poll (struct watcher_t *watcher)
{
    struct kan_cpu_task_t task = {
        .name = cpu_task_name,
        .function = poll_task_function,
        .user_data = (kan_cpu_task_user_data_t) watcher,
    };

    const kan_cpu_task_handle_t handle = kan_cpu_task_dispatch (task, KAN_CPU_DISPATCH_QUEUE_BACKGROUND);
    if (handle == KAN_INVALID_CPU_TASK_HANDLE)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, file_system_watcher, KAN_LOG_ERROR,
                             "Failed to schedule poll for watcher at \"%s\".", watcher->path_container.path);
    }
    else
    {
        kan_cpu_task_detach (handle);
    }
}

static inline void split_entry_name (const char *entry_name,
                                     kan_interned_string_t *name_output,
                                     kan_interned_string_t *extension_output)
{
    const char *last_dot_position = strrchr (entry_name, '.');
    if (last_dot_position)
    {
        *name_output = kan_char_sequence_intern (entry_name, last_dot_position);
        *extension_output = kan_string_intern (last_dot_position + 1u);
    }
    else
    {
        *name_output = kan_string_intern (entry_name);
        *extension_output = NULL;
    }
}

static void directory_node_append_file (struct directory_node_t *directory,
                                        const char *entry_name,
                                        const struct kan_file_system_entry_status_t *status)
{
    struct file_node_t *file = file_node_allocate ();
    split_entry_name (entry_name, &file->name, &file->extension);

    file->last_modification_time_ns = status->last_modification_time_ns;
    file->mark_found = KAN_TRUE;

    file->next = directory->first_file;
    directory->first_file = file;
}

static struct directory_node_t *directory_node_append_empty_child_directory (struct directory_node_t *directory,
                                                                             kan_interned_string_t interned_entry_name)
{
    struct directory_node_t *child = directory_node_create (interned_entry_name);
    child->next_on_level_directory = directory->first_child_directory;
    directory->first_child_directory = child;
    return child;
}

static void initial_poll_to_directory_recursive (struct watcher_t *watcher, struct directory_node_t *directory)
{
    // We assume that path container holds path to given directory.
    kan_file_system_directory_iterator_t iterator =
        kan_file_system_directory_iterator_create (watcher->path_container.path);

    if (iterator != KAN_INVALID_FILE_SYSTEM_DIRECTORY_ITERATOR)
    {
        const char *entry_name;
        while ((entry_name = kan_file_system_directory_iterator_advance (iterator)))
        {
            if ((entry_name[0u] == '.' && entry_name[1u] == '\0') ||
                (entry_name[0u] == '.' && entry_name[1u] == '.' && entry_name[2u] == '\0'))
            {
                // Skip current and parent entries.
                continue;
            }

            const uint64_t length_backup = watcher->path_container.length;
            kan_file_system_path_container_append (&watcher->path_container, entry_name);

            struct kan_file_system_entry_status_t status;
            if (kan_file_system_query_entry (watcher->path_container.path, &status))
            {
                switch (status.type)
                {
                case KAN_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN:
                    KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, file_system_watcher, KAN_LOG_WARNING,
                             "Entry at \"%s\" has unknown type and will be ignored in snapshot.",
                             watcher->path_container.path)
                    break;

                case KAN_FILE_SYSTEM_ENTRY_TYPE_FILE:
                    directory_node_append_file (directory, entry_name, &status);
                    break;

                case KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY:
                    initial_poll_to_directory_recursive (watcher, directory_node_append_empty_child_directory (
                                                                      directory, kan_string_intern (entry_name)));
                    break;
                }
            }
            else
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, file_system_watcher, KAN_LOG_ERROR,
                         "Unable to query status of \"%s\", file system snapshot will be incomplete.",
                         watcher->path_container.path)
            }

            kan_file_system_path_container_reset_length (&watcher->path_container, length_backup);
        }

        kan_file_system_directory_iterator_destroy (iterator);
    }
    else
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, file_system_watcher, KAN_LOG_ERROR,
                 "Unable to iterate directory \"%s\", file system snapshot will be incomplete.",
                 watcher->path_container.path)
    }
}

static void send_directory_added_event (struct watcher_t *watcher)
{
    // We assume that path container holds path to added directory.

    kan_atomic_int_lock (&watcher->event_queue_lock);
    struct event_queue_node_t *event_node =
        (struct event_queue_node_t *) kan_event_queue_submit_begin (&watcher->event_queue);

    if (event_node)
    {
        event_node->event.event_type = KAN_FILE_SYSTEM_EVENT_TYPE_ADDED;
        event_node->event.entry_type = KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY;
        kan_file_system_path_container_copy (&event_node->event.path_container, &watcher->path_container);
        kan_event_queue_submit_end (&watcher->event_queue, &allocate_event_queue_node ()->node);
    }

    kan_atomic_int_unlock (&watcher->event_queue_lock);
}

static void send_directory_removed_event (struct watcher_t *watcher, struct directory_node_t *directory_node)
{
    // We assume that path container holds path to owner directory.

    kan_atomic_int_lock (&watcher->event_queue_lock);
    struct event_queue_node_t *event_node =
        (struct event_queue_node_t *) kan_event_queue_submit_begin (&watcher->event_queue);

    if (event_node)
    {
        event_node->event.event_type = KAN_FILE_SYSTEM_EVENT_TYPE_REMOVED;
        event_node->event.entry_type = KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY;
        kan_file_system_path_container_copy (&event_node->event.path_container, &watcher->path_container);
        kan_file_system_path_container_append (&event_node->event.path_container, directory_node->name);
        kan_event_queue_submit_end (&watcher->event_queue, &allocate_event_queue_node ()->node);
    }

    kan_atomic_int_unlock (&watcher->event_queue_lock);
}

static void send_file_added_event (struct watcher_t *watcher)
{
    // We assume that path container holds path to added file.

    kan_atomic_int_lock (&watcher->event_queue_lock);
    struct event_queue_node_t *event_node =
        (struct event_queue_node_t *) kan_event_queue_submit_begin (&watcher->event_queue);

    if (event_node)
    {
        event_node->event.event_type = KAN_FILE_SYSTEM_EVENT_TYPE_ADDED;
        event_node->event.entry_type = KAN_FILE_SYSTEM_ENTRY_TYPE_FILE;
        kan_file_system_path_container_copy (&event_node->event.path_container, &watcher->path_container);
        kan_event_queue_submit_end (&watcher->event_queue, &allocate_event_queue_node ()->node);
    }

    kan_atomic_int_unlock (&watcher->event_queue_lock);
}

static void send_file_modified_event (struct watcher_t *watcher)
{
    // We assume that path container holds path to added file.

    kan_atomic_int_lock (&watcher->event_queue_lock);
    struct event_queue_node_t *event_node =
        (struct event_queue_node_t *) kan_event_queue_submit_begin (&watcher->event_queue);

    if (event_node)
    {
        event_node->event.event_type = KAN_FILE_SYSTEM_EVENT_TYPE_MODIFIED;
        event_node->event.entry_type = KAN_FILE_SYSTEM_ENTRY_TYPE_FILE;
        kan_file_system_path_container_copy (&event_node->event.path_container, &watcher->path_container);
        kan_event_queue_submit_end (&watcher->event_queue, &allocate_event_queue_node ()->node);
    }

    kan_atomic_int_unlock (&watcher->event_queue_lock);
}

static void send_file_removed_event (struct watcher_t *watcher, struct file_node_t *file_node)
{
    // We assume that path container holds path to owner directory.

    kan_atomic_int_lock (&watcher->event_queue_lock);
    struct event_queue_node_t *event_node =
        (struct event_queue_node_t *) kan_event_queue_submit_begin (&watcher->event_queue);

    if (event_node)
    {
        event_node->event.event_type = KAN_FILE_SYSTEM_EVENT_TYPE_REMOVED;
        event_node->event.entry_type = KAN_FILE_SYSTEM_ENTRY_TYPE_FILE;
        kan_file_system_path_container_copy (&event_node->event.path_container, &watcher->path_container);

        if (file_node->name)
        {
            kan_file_system_path_container_append (&event_node->event.path_container, file_node->name);
            if (file_node->extension)
            {
                kan_file_system_path_container_add_suffix (&event_node->event.path_container, ".");
                kan_file_system_path_container_add_suffix (&event_node->event.path_container, file_node->extension);
            }
        }
        else if (file_node->extension)
        {
            kan_file_system_path_container_append (&event_node->event.path_container, ".");
            kan_file_system_path_container_add_suffix (&event_node->event.path_container, file_node->extension);
        }
        else
        {
            // File with no name and no extension. Shouldn't happen.
            KAN_ASSERT (KAN_FALSE)
        }

        kan_event_queue_submit_end (&watcher->event_queue, &allocate_event_queue_node ()->node);
    }

    kan_atomic_int_unlock (&watcher->event_queue_lock);
}

static void send_events_recursively_on_directory_removal (struct watcher_t *watcher, struct directory_node_t *directory)
{
    // We assume that path container holds path to given directory.

    struct directory_node_t *child_directory = directory->first_child_directory;
    while (child_directory)
    {
        const uint64_t length_backup = watcher->path_container.length;
        kan_file_system_path_container_append (&watcher->path_container, child_directory->name);
        send_events_recursively_on_directory_removal (watcher, child_directory);
        kan_file_system_path_container_reset_length (&watcher->path_container, length_backup);
        child_directory = child_directory->next_on_level_directory;
    }

    struct file_node_t *child_file = directory->first_file;
    while (child_file)
    {
        send_file_removed_event (watcher, child_file);
        child_file = child_file->next;
    }

    send_directory_removed_event (watcher, directory);
}

static inline struct directory_node_t *directory_find_child_directory_node (
    struct directory_node_t *directory, kan_interned_string_t interned_directory_name)
{
    // We don't use hash storages to accelerate search because in general file systems aren't optimized to handle tons
    // of files and directories in one directory either.
    struct directory_node_t *child_directory = directory->first_child_directory;

    while (child_directory)
    {
        if (child_directory->name == interned_directory_name)
        {
            return child_directory;
        }
        child_directory = child_directory->next_on_level_directory;
    }

    return NULL;
}

static inline struct file_node_t *directory_find_child_file_node (struct directory_node_t *directory,
                                                                  kan_interned_string_t name_part,
                                                                  kan_interned_string_t extension_part)
{
    // We don't use hash storages to accelerate search because in general file systems aren't optimized to handle tons
    // of files and directories in one directory either.
    struct file_node_t *child_file = directory->first_file;

    while (child_file)
    {
        if (child_file->name == name_part && child_file->extension == extension_part)
        {
            return child_file;
        }

        child_file = child_file->next;
    }

    return NULL;
}

static void verification_poll_at_directory_recursive (struct watcher_t *watcher, struct directory_node_t *directory)
{
    // We assume that path container holds path to given directory.

    // Start by marking everything as not found.

    struct directory_node_t *child_directory = directory->first_child_directory;
    while (child_directory)
    {
        child_directory->mark_found = KAN_FALSE;
        child_directory = child_directory->next_on_level_directory;
    }

    struct file_node_t *child_file = directory->first_file;
    while (child_file)
    {
        child_file->mark_found = KAN_FALSE;
        child_file = child_file->next;
    }

    // Poll file system to discover existing entries, update timestamps and discover new entries.

    kan_file_system_directory_iterator_t iterator =
        kan_file_system_directory_iterator_create (watcher->path_container.path);

    if (iterator != KAN_INVALID_FILE_SYSTEM_DIRECTORY_ITERATOR)
    {
        const char *entry_name;
        while ((entry_name = kan_file_system_directory_iterator_advance (iterator)))
        {
            if ((entry_name[0u] == '.' && entry_name[1u] == '\0') ||
                (entry_name[0u] == '.' && entry_name[1u] == '.' && entry_name[2u] == '\0'))
            {
                // Skip current and parent entries.
                continue;
            }

            const uint64_t length_backup = watcher->path_container.length;
            kan_file_system_path_container_append (&watcher->path_container, entry_name);

            struct kan_file_system_entry_status_t status;
            if (kan_file_system_query_entry (watcher->path_container.path, &status))
            {
                switch (status.type)
                {
                case KAN_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN:
                    KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, file_system_watcher, KAN_LOG_WARNING,
                             "Entry at \"%s\" has unknown type and will be ignored in snapshot.",
                             watcher->path_container.path)
                    break;

                case KAN_FILE_SYSTEM_ENTRY_TYPE_FILE:
                {
                    kan_interned_string_t name_part;
                    kan_interned_string_t extension_part;
                    split_entry_name (entry_name, &name_part, &extension_part);

                    struct file_node_t *file_node =
                        directory_find_child_file_node (directory, name_part, extension_part);

                    if (file_node)
                    {
                        if (file_node->last_modification_time_ns != status.last_modification_time_ns)
                        {
                            send_file_modified_event (watcher);
                            file_node->last_modification_time_ns = status.last_modification_time_ns;
                        }

                        file_node->mark_found = KAN_TRUE;
                    }
                    else
                    {
                        directory_node_append_file (directory, entry_name, &status);
                        send_file_added_event (watcher);
                    }

                    break;
                }

                case KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY:
                {
                    kan_interned_string_t name = kan_string_intern (entry_name);
                    struct directory_node_t *child_directory_node =
                        directory_find_child_directory_node (directory, name);

                    if (child_directory_node)
                    {
                        child_directory_node->mark_found = KAN_TRUE;
                        verification_poll_at_directory_recursive (watcher, child_directory_node);
                    }
                    else
                    {
                        child_directory_node = directory_node_append_empty_child_directory (directory, name);
                        send_directory_added_event (watcher);
                        verification_poll_at_directory_recursive (watcher, child_directory_node);
                    }

                    break;
                }
                }
            }
            else
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, file_system_watcher, KAN_LOG_ERROR,
                         "Unable to query status of \"%s\", file system snapshot will be incomplete.",
                         watcher->path_container.path)
            }

            kan_file_system_path_container_reset_length (&watcher->path_container, length_backup);
        }

        kan_file_system_directory_iterator_destroy (iterator);
    }
    else
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, file_system_watcher, KAN_LOG_ERROR,
                 "Unable to iterate directory \"%s\", file system snapshot will be incomplete.",
                 watcher->path_container.path)
    }

    // Delete undiscovered entries.

    child_directory = directory->first_child_directory;
    struct directory_node_t *previous_child_directory = NULL;

    while (child_directory)
    {
        struct directory_node_t *next = child_directory->next_on_level_directory;
        if (child_directory->mark_found)
        {
            previous_child_directory = child_directory;
        }
        else
        {
            send_events_recursively_on_directory_removal (watcher, child_directory);
            if (previous_child_directory)
            {
                previous_child_directory->next_on_level_directory = next;
            }
            else
            {
                directory->first_child_directory = next;
            }

            directory_node_destroy (child_directory);
        }

        child_directory = next;
    }

    child_file = directory->first_file;
    struct file_node_t *previous_child_file = NULL;

    while (child_file)
    {
        struct file_node_t *next = child_file->next;
        if (child_file->mark_found)
        {
            previous_child_file = child_file;
        }
        else
        {
            send_file_removed_event (watcher, child_file);
            if (previous_child_file)
            {
                previous_child_file->next = next;
            }
            else
            {
                directory->first_file = next;
            }

            file_node_free (child_file);
        }

        child_file = next;
    }
}

static void poll_task_function (uint64_t user_data)
{
    struct watcher_t *watcher = (struct watcher_t *) user_data;
    if (kan_atomic_int_get (&watcher->marked_for_destroy))
    {
        if (watcher->root_directory)
        {
            directory_node_destroy (watcher->root_directory);
        }

        struct event_queue_node_t *queue_node = (struct event_queue_node_t *) watcher->event_queue.oldest;
        while (queue_node)
        {
            struct event_queue_node_t *next = (struct event_queue_node_t *) queue_node->node.next;
            kan_free_general (event_allocation_group, queue_node, sizeof (struct event_queue_node_t));
            queue_node = next;
        }

        kan_free_general (watcher_allocation_group, watcher, sizeof (struct watcher_t));
        return;
    }

    if (watcher->root_directory)
    {
        verification_poll_at_directory_recursive (watcher, watcher->root_directory);
    }
    else
    {
        watcher->root_directory = directory_node_create (NULL);
        initial_poll_to_directory_recursive (watcher, watcher->root_directory);
    }

    // Schedule next poll.
    schedule_poll (watcher);
}

kan_file_system_watcher_t kan_file_system_watcher_create (const char *directory_path)
{
    ensure_statics_initialized ();
    struct watcher_t *watcher_data =
        kan_allocate_general (watcher_allocation_group, sizeof (struct watcher_t), _Alignof (struct watcher_t));

    watcher_data->root_directory = NULL;
    watcher_data->event_queue_lock = kan_atomic_int_init (0);
    kan_event_queue_init (&watcher_data->event_queue, &allocate_event_queue_node ()->node);
    watcher_data->marked_for_destroy = kan_atomic_int_init (0);
    kan_file_system_path_container_copy_string (&watcher_data->path_container, directory_path);

    schedule_poll (watcher_data);
    return (kan_file_system_watcher_t) watcher_data;
}

void kan_file_system_watcher_destroy (kan_file_system_watcher_t watcher)
{
    kan_atomic_int_set (&((struct watcher_t *) watcher)->marked_for_destroy, 1);
}

kan_file_system_watcher_iterator_t kan_file_system_watcher_iterator_create (kan_file_system_watcher_t watcher)
{
    struct watcher_t *watcher_data = (struct watcher_t *) watcher;
    kan_atomic_int_lock (&watcher_data->event_queue_lock);
    kan_event_queue_iterator_t iterator = kan_event_queue_iterator_create (&watcher_data->event_queue);
    kan_atomic_int_unlock (&watcher_data->event_queue_lock);
    return iterator;
}

const struct kan_file_system_watcher_event_t *kan_file_system_watcher_iterator_get (
    kan_file_system_watcher_t watcher, kan_file_system_watcher_iterator_t iterator)
{
    struct watcher_t *watcher_data = (struct watcher_t *) watcher;
    kan_atomic_int_lock (&watcher_data->event_queue_lock);

    const struct event_queue_node_t *node =
        (const struct event_queue_node_t *) kan_event_queue_iterator_get (&watcher_data->event_queue, iterator);

    kan_atomic_int_unlock (&watcher_data->event_queue_lock);
    return node ? &node->event : NULL;
}

static inline void watcher_cleanup_events (struct watcher_t *watcher)
{
    struct event_queue_node_t *node;
    while ((node = (struct event_queue_node_t *) kan_event_queue_clean_oldest (&watcher->event_queue)))
    {
        free_event_queue_node (node);
    }
}

kan_file_system_watcher_iterator_t kan_file_system_watcher_iterator_advance (
    kan_file_system_watcher_t watcher, kan_file_system_watcher_iterator_t iterator)
{
    struct watcher_t *watcher_data = (struct watcher_t *) watcher;
    kan_atomic_int_lock (&watcher_data->event_queue_lock);
    iterator = kan_event_queue_iterator_advance (iterator);

    watcher_cleanup_events (watcher_data);
    kan_atomic_int_unlock (&watcher_data->event_queue_lock);
    return iterator;
}

void kan_file_system_watcher_iterator_destroy (kan_file_system_watcher_t watcher,
                                               kan_file_system_watcher_iterator_t iterator)
{
    struct watcher_t *watcher_data = (struct watcher_t *) watcher;
    kan_atomic_int_lock (&watcher_data->event_queue_lock);
    kan_event_queue_iterator_destroy (&watcher_data->event_queue, iterator);

    watcher_cleanup_events (watcher_data);
    kan_atomic_int_unlock (&watcher_data->event_queue_lock);
}
