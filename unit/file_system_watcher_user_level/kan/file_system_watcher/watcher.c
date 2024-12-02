#include <kan/container/event_queue.h>
#include <kan/container/interned_string.h>
#include <kan/error/critical.h>
#include <kan/file_system/entry.h>
#include <kan/file_system_watcher/watcher.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/precise_time/precise_time.h>
#include <kan/threading/thread.h>

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
    kan_time_size_t last_modification_time_ns;
    kan_file_size_t size;
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
    struct watcher_t *next_watcher;
    struct directory_node_t *root_directory;
    struct kan_atomic_int_t event_queue_lock;
    struct kan_event_queue_t event_queue;
    struct kan_atomic_int_t marked_for_destroy;

    /// \details Stores initial path by default. Used by recursive algorithms to store current recurrent path.
    struct kan_file_system_path_container_t path_container;
};

struct wait_up_to_date_queue_item_t
{
    struct wait_up_to_date_queue_item_t *next;
    struct kan_atomic_int_t is_everything_up_to_date;
};

static kan_bool_t statics_initialized = KAN_FALSE;
static struct kan_atomic_int_t statics_initialization_lock = {.value = 0};

static kan_allocation_group_t watcher_allocation_group;
static kan_allocation_group_t hierarchy_allocation_group;
static kan_allocation_group_t event_allocation_group;

static struct kan_atomic_int_t server_thread_access_lock;
static kan_bool_t server_thread_running = KAN_FALSE;
struct watcher_t *serve_queue = NULL;
struct wait_up_to_date_queue_item_t *wait_up_to_date_queue = NULL;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        kan_atomic_int_lock (&statics_initialization_lock);
        if (!statics_initialized)
        {
            watcher_allocation_group =
                kan_allocation_group_get_child (kan_allocation_group_root (), "file_system_watcher");
            hierarchy_allocation_group = kan_allocation_group_get_child (watcher_allocation_group, "hierarchy");
            event_allocation_group = kan_allocation_group_get_child (watcher_allocation_group, "event");
            statics_initialized = KAN_TRUE;
            server_thread_access_lock = kan_atomic_int_init (0);
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
    file->size = status->size;
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

    if (KAN_HANDLE_IS_VALID (iterator))
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

            const kan_instance_size_t length_backup = watcher->path_container.length;
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

static void send_removal_events_to_directory_content (struct watcher_t *watcher, struct directory_node_t *directory)
{
    // We assume that path container holds path to given directory.

    struct directory_node_t *child_directory = directory->first_child_directory;
    while (child_directory)
    {
        const kan_instance_size_t length_backup = watcher->path_container.length;
        kan_file_system_path_container_append (&watcher->path_container, child_directory->name);
        send_removal_events_to_directory_content (watcher, child_directory);
        kan_file_system_path_container_reset_length (&watcher->path_container, length_backup);
        child_directory = child_directory->next_on_level_directory;
    }

    struct file_node_t *child_file = directory->first_file;
    while (child_file)
    {
        send_file_removed_event (watcher, child_file);
        child_file = child_file->next;
    }
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

    if (KAN_HANDLE_IS_VALID (iterator))
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

            const kan_instance_size_t length_backup = watcher->path_container.length;
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
                        if (file_node->last_modification_time_ns != status.last_modification_time_ns ||
                            file_node->size != status.size)
                        {
                            send_file_modified_event (watcher);
                            file_node->last_modification_time_ns = status.last_modification_time_ns;
                            file_node->size = status.size;
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
            const kan_instance_size_t length_backup = watcher->path_container.length;
            kan_file_system_path_container_append (&watcher->path_container, child_directory->name);
            send_removal_events_to_directory_content (watcher, child_directory);
            kan_file_system_path_container_reset_length (&watcher->path_container, length_backup);
            send_directory_removed_event (watcher, child_directory);

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

static int server_thread (void *user_data)
{
    while (KAN_TRUE)
    {
        // Check watchers and execute scheduled deletion. Exit if no watchers.
        kan_atomic_int_lock (&server_thread_access_lock);
        struct watcher_t *previous = NULL;
        struct watcher_t *watcher = serve_queue;

        while (watcher)
        {
            struct watcher_t *next = watcher->next_watcher;
            if (kan_atomic_int_get (&watcher->marked_for_destroy))
            {
                if (watcher->root_directory)
                {
                    directory_node_destroy (watcher->root_directory);
                }

                struct event_queue_node_t *queue_node = (struct event_queue_node_t *) watcher->event_queue.oldest;
                while (queue_node)
                {
                    struct event_queue_node_t *next_event = (struct event_queue_node_t *) queue_node->node.next;
                    kan_free_general (event_allocation_group, queue_node, sizeof (struct event_queue_node_t));
                    queue_node = next_event;
                }

                kan_free_general (watcher_allocation_group, watcher, sizeof (struct watcher_t));
                if (previous)
                {
                    previous->next_watcher = next;
                }
                else
                {
                    serve_queue = next;
                }
            }
            else
            {
                previous = watcher;
            }

            watcher = next;
        }

        if (!serve_queue)
        {
            // If there is no one to serve -- exit thread.
            server_thread_running = KAN_FALSE;
            kan_atomic_int_unlock (&server_thread_access_lock);
            return 0;
        }

        watcher = serve_queue;
        kan_atomic_int_unlock (&server_thread_access_lock);

        // We've captured serve queue value in watcher and there is no one who changes next pointers.
        // Therefore, we can safely iterate and serve watchers.

        const kan_time_size_t serve_start = kan_precise_time_get_elapsed_nanoseconds ();
        while (watcher)
        {
            KAN_ASSERT (watcher->root_directory)
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, file_system_watcher, KAN_LOG_ERROR,
                                 "Running file system watcher at \"%s\".",
                                 watcher->path_container.path)
            verification_poll_at_directory_recursive (watcher, watcher->root_directory);
            watcher = watcher->next_watcher;
        }

        kan_atomic_int_lock (&server_thread_access_lock);
        struct wait_up_to_date_queue_item_t *wait_item = wait_up_to_date_queue;
        wait_up_to_date_queue = NULL;

        while (wait_item)
        {
            kan_atomic_int_set (&wait_item->is_everything_up_to_date, 1);
            wait_item = wait_item->next;
        }

        kan_atomic_int_unlock (&server_thread_access_lock);
        const kan_time_size_t serve_end = kan_precise_time_get_elapsed_nanoseconds ();
        const kan_time_offset_t serve_time = (kan_time_offset_t) (serve_end - serve_start);

        if (serve_time < KAN_FILE_SYSTEM_WATCHER_UL_MIN_FRAME_NS)
        {
            kan_precise_time_sleep (KAN_FILE_SYSTEM_WATCHER_UL_MIN_FRAME_NS - serve_time);
        }
    }
}

static void register_new_watcher (struct watcher_t *watcher)
{
    kan_atomic_int_lock (&server_thread_access_lock);
    watcher->next_watcher = serve_queue;
    serve_queue = watcher;

    if (!server_thread_running)
    {
        kan_thread_t thread = kan_thread_create ("file_system_watcher_server", server_thread, NULL);
        kan_thread_detach (thread);
        server_thread_running = KAN_TRUE;
    }

    kan_atomic_int_unlock (&server_thread_access_lock);
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

    // Doing initial poll as background task on some worker thread seems like a good idea at first,
    // but it has one issue: there is a potential window between watcher creation and initial poll
    // where user can create new files and they will be considered already existing, therefore
    // producing hard to track bugs.
    watcher_data->root_directory = directory_node_create (NULL);
    initial_poll_to_directory_recursive (watcher_data, watcher_data->root_directory);

    register_new_watcher (watcher_data);
    return KAN_HANDLE_SET (kan_file_system_watcher_t, watcher_data);
}

void kan_file_system_watcher_destroy (kan_file_system_watcher_t watcher)
{
    struct watcher_t *data = KAN_HANDLE_GET (watcher);
    kan_atomic_int_set (&data->marked_for_destroy, 1);
}

kan_file_system_watcher_iterator_t kan_file_system_watcher_iterator_create (kan_file_system_watcher_t watcher)
{
    struct watcher_t *watcher_data = KAN_HANDLE_GET (watcher);
    kan_atomic_int_lock (&watcher_data->event_queue_lock);
    kan_event_queue_iterator_t iterator = kan_event_queue_iterator_create (&watcher_data->event_queue);
    kan_atomic_int_unlock (&watcher_data->event_queue_lock);
    return KAN_HANDLE_TRANSIT (kan_file_system_watcher_iterator_t, iterator);
}

const struct kan_file_system_watcher_event_t *kan_file_system_watcher_iterator_get (
    kan_file_system_watcher_t watcher, kan_file_system_watcher_iterator_t iterator)
{
    struct watcher_t *watcher_data = KAN_HANDLE_GET (watcher);
    kan_atomic_int_lock (&watcher_data->event_queue_lock);

    const struct event_queue_node_t *node = (const struct event_queue_node_t *) kan_event_queue_iterator_get (
        &watcher_data->event_queue, KAN_HANDLE_TRANSIT (kan_event_queue_iterator_t, iterator));

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
    struct watcher_t *watcher_data = KAN_HANDLE_GET (watcher);
    kan_atomic_int_lock (&watcher_data->event_queue_lock);
    iterator = KAN_HANDLE_TRANSIT (
        kan_file_system_watcher_iterator_t,
        kan_event_queue_iterator_advance (KAN_HANDLE_TRANSIT (kan_event_queue_iterator_t, iterator)));

    watcher_cleanup_events (watcher_data);
    kan_atomic_int_unlock (&watcher_data->event_queue_lock);
    return iterator;
}

void kan_file_system_watcher_iterator_destroy (kan_file_system_watcher_t watcher,
                                               kan_file_system_watcher_iterator_t iterator)
{
    struct watcher_t *watcher_data = KAN_HANDLE_GET (watcher);
    kan_atomic_int_lock (&watcher_data->event_queue_lock);
    kan_event_queue_iterator_destroy (&watcher_data->event_queue,
                                      KAN_HANDLE_TRANSIT (kan_event_queue_iterator_t, iterator));

    watcher_cleanup_events (watcher_data);
    kan_atomic_int_unlock (&watcher_data->event_queue_lock);
}

void kan_file_system_watcher_ensure_all_watchers_are_up_to_date (void)
{
    struct wait_up_to_date_queue_item_t item = {
        .next = NULL,
        .is_everything_up_to_date = kan_atomic_int_init (0),
    };

    kan_atomic_int_lock (&server_thread_access_lock);
    item.next = wait_up_to_date_queue;
    wait_up_to_date_queue = &item;
    kan_atomic_int_unlock (&server_thread_access_lock);

    while (kan_atomic_int_get (&item.is_everything_up_to_date) == 0)
    {
        kan_precise_time_sleep (KAN_FILE_SYSTEM_WATCHER_UL_WAKE_UP_DELTA_NS);
    }
}
