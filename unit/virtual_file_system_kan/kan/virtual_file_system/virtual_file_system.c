#include <stdlib.h>

#include <kan/api_common/min_max.h>
#include <kan/api_common/type_punning.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/event_queue.h>
#include <kan/container/hash_storage.h>
#include <kan/container/interned_string.h>
#include <kan/error/critical.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/stream.h>
#include <kan/file_system_watcher/watcher.h>
#include <kan/hash/hash.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/reflection/generated_reflection.h>
#include <kan/serialization/binary.h>
#include <kan/threading/atomic.h>
#include <kan/virtual_file_system/virtual_file_system.h>

// \c_interface_scanner_disable
KAN_LOG_DEFINE_CATEGORY (virtual_file_system);
// \c_interface_scanner_enable

KAN_REFLECTION_EXPECT_UNIT_REGISTRAR_LOCAL (virtual_file_system_kan);

struct mount_point_real_t
{
    kan_interned_string_t name;
    char *real_directory_path;
    struct mount_point_real_t *next;
    struct mount_point_real_t *previous;
    struct virtual_directory_t *owner_directory;
};

struct read_only_pack_file_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
    kan_interned_string_t extension;
    uint64_t offset;
    uint64_t size;
};

struct read_only_pack_directory_t
{
    kan_interned_string_t name;
    struct read_only_pack_directory_t *next_on_level;
    struct read_only_pack_directory_t *first_child;
    struct kan_hash_storage_t files;
};

struct mount_point_read_only_pack_t
{
    struct read_only_pack_directory_t root_directory;
    char *real_file_path;
    struct mount_point_read_only_pack_t *next;
    struct mount_point_read_only_pack_t *previous;
};

struct virtual_directory_t
{
    kan_interned_string_t name;
    struct virtual_directory_t *parent;
    struct virtual_directory_t *next_on_level;
    struct virtual_directory_t *first_child;
    struct mount_point_real_t *first_mount_point_real;
    struct mount_point_read_only_pack_t *first_mount_point_read_only_pack;
};

struct volume_t
{
    struct virtual_directory_t root_directory;
    struct file_system_watcher_t *first_watcher;
};

enum path_extraction_result_t
{
    PATH_EXTRACTION_RESULT_HAS_MORE_COMPONENTS_AFTER = 0u,
    PATH_EXTRACTION_RESULT_LAST_COMPONENT,
    PATH_EXTRACTION_RESULT_FAILED,
};

enum follow_path_result_t
{
    FOLLOW_PATH_RESULT_REACHED_END = 0u,
    FOLLOW_PATH_RESULT_STOPPED,
    FOLLOW_PATH_RESULT_FAILED,
};

struct read_only_pack_registry_item_t
{
    char *path;
    uint64_t offset;
    uint64_t size;
};

struct read_only_pack_registry_t
{
    /// \meta reflection_dynamic_array_type = "struct read_only_pack_registry_item_t"
    struct kan_dynamic_array_t items;
};

enum directory_iterator_type_t
{
    DIRECTORY_ITERATOR_TYPE_INVALID,
    DIRECTORY_ITERATOR_TYPE_VIRTUAL_DIRECTORY,
    DIRECTORY_ITERATOR_TYPE_REAL_DIRECTORY,
    DIRECTORY_ITERATOR_TYPE_READ_ONLY_PACK_DIRECTORY,
};

enum virtual_directory_iterator_stage_t
{
    VIRTUAL_DIRECTORY_ITERATOR_STAGE_CHILDREN,
    VIRTUAL_DIRECTORY_ITERATOR_STAGE_MOUNT_POINTS_REAL,
    VIRTUAL_DIRECTORY_ITERATOR_STAGE_MOUNT_POINTS_READ_ONLY_PACK,
};

struct virtual_directory_iterator_suffix_t
{
    struct virtual_directory_t *directory;
    enum virtual_directory_iterator_stage_t stage;

    union
    {
        struct virtual_directory_t *next_virtual_directory;
        struct mount_point_real_t *next_mount_point_real;
        struct mount_point_read_only_pack_t *next_mount_point_read_only_pack;
    };
};

enum read_only_pack_directory_iterator_stage_t
{
    READ_ONLY_PACK_DIRECTORY_ITERATOR_STAGE_CHILDREN,
    READ_ONLY_PACK_DIRECTORY_ITERATOR_STAGE_FILES,
};

struct read_only_pack_directory_iterator_suffix_t
{
    struct read_only_pack_directory_t *directory;
    char *file_name_buffer;
    enum read_only_pack_directory_iterator_stage_t stage;

    union
    {
        struct read_only_pack_directory_t *next_directory;
        struct read_only_pack_file_node_t *next_file;
    };
};

struct directory_iterator_t
{
    enum directory_iterator_type_t type;
    union
    {
        struct virtual_directory_iterator_suffix_t virtual_directory_suffix;
        struct read_only_pack_directory_iterator_suffix_t read_only_pack_suffix;
        kan_file_system_directory_iterator_t real_file_system_iterator;
    };
};

_Static_assert (sizeof (struct directory_iterator_t) <= sizeof (struct kan_virtual_file_system_directory_iterator_t),
                "Directory iterator size matches.");
_Static_assert (_Alignof (struct directory_iterator_t) ==
                    _Alignof (struct kan_virtual_file_system_directory_iterator_t),
                "Directory iterator alignment matches.");

struct read_only_pack_file_read_stream_t
{
    struct kan_stream_t stream;
    struct kan_stream_t *base_stream;
    uint64_t offset;
    uint64_t size;
    uint64_t position;
};

struct read_only_pack_builder_t
{
    struct kan_stream_t *output_stream;
    uint64_t beginning_offset_in_stream;
    struct read_only_pack_registry_t registry;
};

struct file_system_watcher_event_node_t
{
    struct kan_event_queue_node_t node;
    struct kan_virtual_file_system_watcher_event_t event;
};

struct real_file_system_watcher_attachment_t
{
    kan_file_system_watcher_t real_watcher;
    kan_file_system_watcher_iterator_t iterator;
    struct mount_point_real_t *mount_point;
};

struct file_system_watcher_t
{
    struct volume_t *volume;

    struct file_system_watcher_t *next;

    struct file_system_watcher_t *previous;

    struct virtual_directory_t *attached_to_virtual_directory;

    /// \meta reflection_dynamic_array_type = "struct real_file_system_watcher_attachment_t"
    struct kan_dynamic_array_t real_file_system_attachments;

    struct kan_atomic_int_t event_queue_lock;
    struct kan_event_queue_t event_queue;
};

static kan_bool_t statics_initialized = KAN_FALSE;
static struct kan_atomic_int_t statics_initialization_lock = {.value = 0};

static kan_reflection_registry_t serialization_registry;
static kan_interned_string_t type_name_read_only_pack_registry_t;
static kan_serialization_binary_script_storage_t serialization_script_storage;

static kan_allocation_group_t root_allocation_group;
static kan_allocation_group_t hierarchy_allocation_group;
static kan_allocation_group_t read_only_pack_files_allocation_group;
static kan_allocation_group_t read_only_pack_operation_allocation_group;
static kan_allocation_group_t read_only_pack_directory_iterator_allocation_group;
static kan_allocation_group_t file_system_watcher_allocation_group;
static kan_allocation_group_t file_system_watcher_events_allocation_group;

static void shutdown_statics (void)
{
    kan_serialization_binary_script_storage_destroy (serialization_script_storage);
    kan_reflection_registry_destroy (serialization_registry);
}

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        kan_atomic_int_lock (&statics_initialization_lock);
        if (!statics_initialized)
        {
            serialization_registry = kan_reflection_registry_create ();
            KAN_REFLECTION_UNIT_REGISTRAR_NAME (virtual_file_system_kan) (serialization_registry);
            type_name_read_only_pack_registry_t = kan_string_intern ("read_only_pack_registry_t");
            serialization_script_storage = kan_serialization_binary_script_storage_create (serialization_registry);

            root_allocation_group =
                kan_allocation_group_get_child (kan_allocation_group_root (), "virtual_file_system");
            hierarchy_allocation_group = kan_allocation_group_get_child (root_allocation_group, "hierarchy");
            read_only_pack_files_allocation_group =
                kan_allocation_group_get_child (hierarchy_allocation_group, "read_only_pack_files");
            read_only_pack_operation_allocation_group =
                kan_allocation_group_get_child (root_allocation_group, "read_only_pack_operation");
            read_only_pack_directory_iterator_allocation_group =
                kan_allocation_group_get_child (root_allocation_group, "read_only_pack_directory_iterator");
            file_system_watcher_allocation_group =
                kan_allocation_group_get_child (root_allocation_group, "file_system_watcher");
            file_system_watcher_events_allocation_group =
                kan_allocation_group_get_child (file_system_watcher_allocation_group, "events");

            atexit (shutdown_statics);
            statics_initialized = KAN_TRUE;
        }

        kan_atomic_int_unlock (&statics_initialization_lock);
    }
}

static const char *read_only_pack_file_find_name_separator (const char *name_begin, const char *name_end)
{
    if (name_begin == name_end)
    {
        return NULL;
    }

    --name_end;
    while (name_end > name_begin)
    {
        if (*name_end == '.')
        {
            return name_end;
        }

        --name_end;
    }

    return *name_begin == '.' ? name_begin : NULL;
}

static void read_only_pack_directory_init (struct read_only_pack_directory_t *directory)
{
    directory->name = NULL;
    directory->next_on_level = NULL;
    directory->first_child = NULL;
    kan_hash_storage_init (&directory->files, read_only_pack_files_allocation_group,
                           KAN_VIRTUAL_FILE_SYSTEM_ROPACK_DIRECTORY_INITIAL_ITEMS);
}

static void read_only_pack_directory_shutdown (struct read_only_pack_directory_t *directory)
{
    struct read_only_pack_directory_t *child = directory->first_child;
    while (child)
    {
        struct read_only_pack_directory_t *next = child->next_on_level;
        read_only_pack_directory_shutdown (child);
        kan_free_batched (hierarchy_allocation_group, child);
        child = next;
    }

    struct read_only_pack_file_node_t *file_node = (struct read_only_pack_file_node_t *) directory->files.items.first;
    while (file_node)
    {
        struct read_only_pack_file_node_t *next = (struct read_only_pack_file_node_t *) file_node->node.list_node.next;
        kan_free_batched (read_only_pack_files_allocation_group, file_node);
        file_node = next;
    }

    kan_hash_storage_shutdown (&directory->files);
}

static void mount_point_read_only_pack_shutdown (struct mount_point_read_only_pack_t *mount_point)
{
    if (mount_point->real_file_path)
    {
        kan_free_general (hierarchy_allocation_group, mount_point->real_file_path,
                          strlen (mount_point->real_file_path) + 1u);
    }

    read_only_pack_directory_shutdown (&mount_point->root_directory);
}

static void mount_point_real_shutdown (struct mount_point_real_t *mount_point)
{
    if (mount_point->real_directory_path)
    {
        kan_free_general (hierarchy_allocation_group, mount_point->real_directory_path,
                          strlen (mount_point->real_directory_path) + 1u);
    }
}

static void virtual_directory_remove_child (struct virtual_directory_t *parent, struct virtual_directory_t *child)
{
    // Search is ineffective, but it is okay for virtual directories as they usually don't have lots of children.
    if (parent->first_child == child)
    {
        parent->first_child = child->next_on_level;
        return;
    }

    struct virtual_directory_t *current = parent->first_child;
    while (current)
    {
        if (current->next_on_level == child)
        {
            current->next_on_level = child->next_on_level;
            return;
        }

        current = current->next_on_level;
    }

    // Not a child.
    KAN_ASSERT (KAN_FALSE)
}

static void virtual_directory_shutdown (struct virtual_directory_t *directory)
{
    struct virtual_directory_t *child = directory->first_child;
    while (child)
    {
        struct virtual_directory_t *next = child->next_on_level;
        virtual_directory_shutdown (child);
        kan_free_batched (hierarchy_allocation_group, child);
        child = next;
    }

    struct mount_point_real_t *mount_point_real = directory->first_mount_point_real;
    while (mount_point_real)
    {
        struct mount_point_real_t *next = mount_point_real->next;
        mount_point_real_shutdown (mount_point_real);
        kan_free_batched (hierarchy_allocation_group, mount_point_real);
        mount_point_real = next;
    }

    struct mount_point_read_only_pack_t *mount_point_pack = directory->first_mount_point_read_only_pack;
    while (mount_point_pack)
    {
        struct mount_point_read_only_pack_t *next = mount_point_pack->next;
        mount_point_read_only_pack_shutdown (mount_point_pack);
        kan_free_batched (hierarchy_allocation_group, mount_point_pack);
        mount_point_pack = next;
    }
}

static inline enum path_extraction_result_t path_extract_next_part (const char **path_iterator,
                                                                    const char **part_begin_output,
                                                                    const char **part_end_output)
{
    if (!*path_iterator || !**path_iterator)
    {
        return PATH_EXTRACTION_RESULT_FAILED;
    }

    // Skip initial slashes.
    while (**path_iterator == '/')
    {
        ++*path_iterator;
    }

    *part_begin_output = *path_iterator;
    while (**path_iterator != '\0')
    {
        if (**path_iterator == '/')
        {
            *part_end_output = *path_iterator;
            ++*path_iterator;

            // Skip trailing slashes.
            while (**path_iterator == '/')
            {
                ++*path_iterator;
            }

            return **path_iterator != '\0' ? PATH_EXTRACTION_RESULT_HAS_MORE_COMPONENTS_AFTER :
                                             PATH_EXTRACTION_RESULT_LAST_COMPONENT;
        }

        ++*path_iterator;
    }

    *part_end_output = *path_iterator;
    return PATH_EXTRACTION_RESULT_LAST_COMPONENT;
}

static void read_only_pack_registry_init (struct read_only_pack_registry_t *registry)
{
    kan_dynamic_array_init (
        &registry->items, KAN_VIRTUAL_FILE_SYSTEM_ROPACKH_INITIAL_ITEMS, sizeof (struct read_only_pack_registry_item_t),
        _Alignof (struct read_only_pack_registry_item_t), read_only_pack_operation_allocation_group);
}

static void read_only_pack_registry_reset (struct read_only_pack_registry_t *registry)
{
    for (uint64_t index = 0u; index < registry->items.size; ++index)
    {
        struct read_only_pack_registry_item_t *item =
            &((struct read_only_pack_registry_item_t *) registry->items.data)[index];
        kan_free_general (read_only_pack_operation_allocation_group, item->path, strlen (item->path) + 1u);
    }

    registry->items.size = 0u;
}

static void read_only_pack_registry_shutdown (struct read_only_pack_registry_t *registry)
{
    for (uint64_t index = 0u; index < registry->items.size; ++index)
    {
        struct read_only_pack_registry_item_t *item =
            &((struct read_only_pack_registry_item_t *) registry->items.data)[index];
        kan_free_general (read_only_pack_operation_allocation_group, item->path, strlen (item->path) + 1u);
    }

    kan_dynamic_array_shutdown (&registry->items);
}

static inline struct virtual_directory_t *virtual_directory_find_child_by_raw_name (
    struct virtual_directory_t *directory, const char *name_begin, const char *name_end)
{
    const uint64_t length = name_end - name_begin;
    struct virtual_directory_t *child = directory->first_child;

    while (child)
    {
        if (strncmp (child->name, name_begin, length) == 0 && child->name[length] == '\0')
        {
            return child;
        }

        child = child->next_on_level;
    }

    return NULL;
}

static inline struct mount_point_real_t *virtual_directory_find_mount_point_real_by_raw_name (
    struct virtual_directory_t *directory, const char *name_begin, const char *name_end)
{
    const uint64_t length = name_end - name_begin;
    struct mount_point_real_t *mount_point = directory->first_mount_point_real;

    while (mount_point)
    {
        if (strncmp (mount_point->name, name_begin, length) == 0 && mount_point->name[length] == '\0')
        {
            return mount_point;
        }

        mount_point = mount_point->next;
    }

    return NULL;
}

static inline struct mount_point_read_only_pack_t *virtual_directory_find_mount_point_read_only_pack_by_raw_name (
    struct virtual_directory_t *directory, const char *name_begin, const char *name_end)
{
    const uint64_t length = name_end - name_begin;
    struct mount_point_read_only_pack_t *mount_point = directory->first_mount_point_read_only_pack;

    while (mount_point)
    {
        if (strncmp (mount_point->root_directory.name, name_begin, length) == 0 &&
            mount_point->root_directory.name[length] == '\0')
        {
            return mount_point;
        }

        mount_point = mount_point->next;
    }

    return NULL;
}

static inline enum follow_path_result_t follow_virtual_directory_path (
    struct virtual_directory_t **current_virtual_directory,
    const char **current_path_iterator,
    const char **current_part_begin,
    const char **current_part_end)
{
    while (KAN_TRUE)
    {
        switch (path_extract_next_part (current_path_iterator, current_part_begin, current_part_end))
        {
        case PATH_EXTRACTION_RESULT_HAS_MORE_COMPONENTS_AFTER:
        {
            struct virtual_directory_t *next_virtual_directory = virtual_directory_find_child_by_raw_name (
                *current_virtual_directory, *current_part_begin, *current_part_end);

            if (!next_virtual_directory)
            {
                return FOLLOW_PATH_RESULT_STOPPED;
            }

            *current_virtual_directory = next_virtual_directory;
            break;
        }

        case PATH_EXTRACTION_RESULT_LAST_COMPONENT:
        {
            struct virtual_directory_t *next_virtual_directory = virtual_directory_find_child_by_raw_name (
                *current_virtual_directory, *current_part_begin, *current_part_end);

            if (!next_virtual_directory)
            {
                return FOLLOW_PATH_RESULT_STOPPED;
            }

            *current_virtual_directory = next_virtual_directory;
            return FOLLOW_PATH_RESULT_REACHED_END;
        }

        case PATH_EXTRACTION_RESULT_FAILED:
            return FOLLOW_PATH_RESULT_FAILED;
        }
    }

    KAN_ASSERT (KAN_FALSE)
    return FOLLOW_PATH_RESULT_FAILED;
}

static inline void virtual_directory_form_path (struct virtual_directory_t *directory,
                                                struct kan_file_system_path_container_t *container)
{
    if (!directory->parent)
    {
        kan_file_system_path_container_reset_length (container, 0u);
        return;
    }
    else if (directory->parent->parent)
    {
        virtual_directory_form_path (directory->parent, container);
        kan_file_system_path_container_append (container, directory->name);
    }
    else
    {
        kan_file_system_path_container_copy_string (container, directory->name);
    }
}

static inline kan_bool_t file_system_watcher_is_observing_virtual_directory (
    struct file_system_watcher_t *file_system_watcher, struct virtual_directory_t *directory)
{
    if (file_system_watcher->attached_to_virtual_directory)
    {
        while (directory)
        {
            if (directory == file_system_watcher->attached_to_virtual_directory)
            {
                return KAN_TRUE;
            }

            directory = directory->parent;
        }
    }

    return KAN_FALSE;
}

static uint64_t read_only_pack_file_read (struct kan_stream_t *stream, uint64_t amount, void *output_buffer)
{
    struct read_only_pack_file_read_stream_t *stream_data = (struct read_only_pack_file_read_stream_t *) stream;
    const uint64_t can_read = stream_data->size - stream_data->position;

    if (can_read == 0u)
    {
        return 0u;
    }

    const uint64_t will_read = KAN_MIN (can_read, amount);
    const uint64_t read =
        stream_data->base_stream->operations->read (stream_data->base_stream, will_read, output_buffer);

    stream_data->position += read;
    return read;
}

static uint64_t read_only_pack_file_tell (struct kan_stream_t *stream)
{
    return ((struct read_only_pack_file_read_stream_t *) stream)->position;
}

static kan_bool_t read_only_pack_file_seek (struct kan_stream_t *stream,
                                            enum kan_stream_seek_pivot pivot,
                                            int64_t offset)
{
    struct read_only_pack_file_read_stream_t *stream_data = (struct read_only_pack_file_read_stream_t *) stream;
    int64_t new_position = 0;

    switch (pivot)
    {
    case KAN_STREAM_SEEK_START:
        new_position = offset;
        break;

    case KAN_STREAM_SEEK_CURRENT:
        new_position = ((int64_t) stream_data->position) + offset;
        break;

    case KAN_STREAM_SEEK_END:
        new_position = ((int64_t) stream_data->size) + offset;
        break;
    }

    if (new_position < 0 || new_position > (int64_t) stream_data->size)
    {
        return KAN_FALSE;
    }

    stream_data->position = (uint64_t) new_position;
    return stream_data->base_stream->operations->seek (stream_data->base_stream, KAN_STREAM_SEEK_START,
                                                       stream_data->offset + stream_data->position);
}

static void read_only_pack_file_close (struct kan_stream_t *stream)
{
    struct read_only_pack_file_read_stream_t *stream_data = (struct read_only_pack_file_read_stream_t *) stream;
    stream_data->base_stream->operations->close (stream_data->base_stream);
    kan_free_batched (read_only_pack_operation_allocation_group, stream_data);
}

static struct kan_stream_operations_t read_only_pack_file_read_operations = {
    .read = read_only_pack_file_read,
    .write = NULL,
    .flush = NULL,
    .tell = read_only_pack_file_tell,
    .seek = read_only_pack_file_seek,
    .close = read_only_pack_file_close,
};

static inline struct file_system_watcher_event_node_t *file_system_watcher_event_node_allocate (void)
{
    return (struct file_system_watcher_event_node_t *) kan_allocate_general (
        file_system_watcher_events_allocation_group, sizeof (struct file_system_watcher_event_node_t),
        _Alignof (struct file_system_watcher_event_node_t));
}

static inline void file_system_watcher_event_node_free (struct file_system_watcher_event_node_t *node)
{
    kan_free_general (file_system_watcher_events_allocation_group, node,
                      sizeof (struct file_system_watcher_event_node_t));
}

static void real_file_system_watcher_attachment_shutdown (struct real_file_system_watcher_attachment_t *attachment)
{
    kan_file_system_watcher_iterator_destroy (attachment->real_watcher, attachment->iterator);
    kan_file_system_watcher_destroy (attachment->real_watcher);
}

static kan_bool_t file_system_watcher_add_real_watcher (struct file_system_watcher_t *watcher,
                                                        struct mount_point_real_t *mount_point,
                                                        const char *real_path);

static void add_real_watchers_in_hierarchy (struct file_system_watcher_t *watcher,
                                            struct virtual_directory_t *directory)
{
    struct mount_point_real_t *mount_point_real = directory->first_mount_point_real;
    while (mount_point_real)
    {
        file_system_watcher_add_real_watcher (watcher, mount_point_real, mount_point_real->real_directory_path);
        mount_point_real = mount_point_real->next;
    }

    struct virtual_directory_t *child_directory = directory->first_child;
    while (child_directory)
    {
        add_real_watchers_in_hierarchy (watcher, child_directory);
        child_directory = child_directory->next_on_level;
    }
}

static struct file_system_watcher_t *file_system_watcher_create (
    struct volume_t *volume, struct virtual_directory_t *attached_to_virtual_directory)
{
    struct file_system_watcher_t *watcher = (struct file_system_watcher_t *) kan_allocate_batched (
        file_system_watcher_allocation_group, sizeof (struct file_system_watcher_t));

    watcher->volume = volume;
    watcher->next = volume->first_watcher;
    watcher->previous = NULL;
    volume->first_watcher = watcher;
    watcher->attached_to_virtual_directory = attached_to_virtual_directory;

    kan_dynamic_array_init (
        &watcher->real_file_system_attachments, 0u, sizeof (struct real_file_system_watcher_attachment_t),
        _Alignof (struct real_file_system_watcher_attachment_t), file_system_watcher_allocation_group);

    watcher->event_queue_lock = kan_atomic_int_init (0);
    kan_event_queue_init (&watcher->event_queue, &file_system_watcher_event_node_allocate ()->node);

    if (attached_to_virtual_directory)
    {
        add_real_watchers_in_hierarchy (watcher, attached_to_virtual_directory);
    }

    return watcher;
}

static kan_bool_t file_system_watcher_add_real_watcher (struct file_system_watcher_t *watcher,
                                                        struct mount_point_real_t *mount_point,
                                                        const char *real_path)
{
    kan_file_system_watcher_t real_watcher = kan_file_system_watcher_create (real_path);
    if (real_watcher == KAN_INVALID_FILE_SYSTEM_WATCHER)
    {
        return KAN_FALSE;
    }

    void *spot = kan_dynamic_array_add_last (&watcher->real_file_system_attachments);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (&watcher->real_file_system_attachments,
                                        KAN_MAX (1u, watcher->real_file_system_attachments.capacity * 2u));
        spot = kan_dynamic_array_add_last (&watcher->real_file_system_attachments);
        KAN_ASSERT (spot)
    }

    struct real_file_system_watcher_attachment_t *attachment = (struct real_file_system_watcher_attachment_t *) spot;

    attachment->mount_point = mount_point;
    attachment->real_watcher = real_watcher;
    attachment->iterator = kan_file_system_watcher_iterator_create (attachment->real_watcher);
    return KAN_TRUE;
}

static void file_system_watcher_destroy (struct file_system_watcher_t *watcher)
{
    KAN_ASSERT (watcher->volume)
    if (watcher->next)
    {
        watcher->next->previous = watcher->previous;
    }

    if (watcher->previous)
    {
        watcher->previous->next = watcher->next;
    }
    else
    {
        watcher->volume->first_watcher = watcher->next;
    }

    for (uint64_t index = 0u; index < watcher->real_file_system_attachments.size; ++index)
    {
        struct real_file_system_watcher_attachment_t *attachment =
            &((struct real_file_system_watcher_attachment_t *) watcher->real_file_system_attachments.data)[index];
        real_file_system_watcher_attachment_shutdown (attachment);
    }

    kan_dynamic_array_shutdown (&watcher->real_file_system_attachments);
    struct file_system_watcher_event_node_t *queue_node =
        (struct file_system_watcher_event_node_t *) watcher->event_queue.oldest;

    while (queue_node)
    {
        struct file_system_watcher_event_node_t *next =
            (struct file_system_watcher_event_node_t *) queue_node->node.next;
        file_system_watcher_event_node_free (queue_node);
        queue_node = next;
    }

    kan_free_batched (file_system_watcher_allocation_group, watcher);
}

static void inform_real_directory_added (struct file_system_watcher_t *watcher,
                                         struct kan_file_system_path_container_t *recursive_virtual_path,
                                         struct kan_file_system_path_container_t *recursive_real_path)
{
    kan_atomic_int_lock (&watcher->event_queue_lock);
    struct file_system_watcher_event_node_t *directory_event =
        (struct file_system_watcher_event_node_t *) kan_event_queue_submit_begin (&watcher->event_queue);

    if (directory_event)
    {
        directory_event->event.event_type = KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_ADDED;
        directory_event->event.entry_type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY;
        kan_file_system_path_container_copy (&directory_event->event.path_container, recursive_virtual_path);
        kan_event_queue_submit_end (&watcher->event_queue, &file_system_watcher_event_node_allocate ()->node);
    }

    kan_atomic_int_unlock (&watcher->event_queue_lock);

    kan_file_system_directory_iterator_t directory_iterator =
        kan_file_system_directory_iterator_create (recursive_real_path->path);

    if (directory_iterator != KAN_INVALID_FILE_SYSTEM_DIRECTORY_ITERATOR)
    {
        const char *entry_name;
        while ((entry_name = kan_file_system_directory_iterator_advance (directory_iterator)))
        {
            if ((entry_name[0u] == '.' && entry_name[1u] == '\0') ||
                (entry_name[0u] == '.' && entry_name[1u] == '.' && entry_name[2u] == '\0'))
            {
                continue;
            }

            const uint64_t length_backup_virtual = recursive_virtual_path->length;
            kan_file_system_path_container_append (recursive_virtual_path, entry_name);

            const uint64_t length_backup_real = recursive_real_path->length;
            kan_file_system_path_container_append (recursive_real_path, entry_name);

            struct kan_file_system_entry_status_t entry_status;
            if (kan_file_system_query_entry (recursive_real_path->path, &entry_status))
            {
                switch (entry_status.type)
                {
                case KAN_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN:
                case KAN_FILE_SYSTEM_ENTRY_TYPE_FILE:
                {
                    kan_atomic_int_lock (&watcher->event_queue_lock);
                    struct file_system_watcher_event_node_t *event =
                        (struct file_system_watcher_event_node_t *) kan_event_queue_submit_begin (
                            &watcher->event_queue);

                    if (event)
                    {
                        event->event.event_type = KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_ADDED;
                        event->event.entry_type = entry_status.type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE ?
                                                      KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE :
                                                      KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN;

                        kan_file_system_path_container_copy (&event->event.path_container, recursive_virtual_path);
                        kan_event_queue_submit_end (&watcher->event_queue,
                                                    &file_system_watcher_event_node_allocate ()->node);
                    }

                    kan_atomic_int_unlock (&watcher->event_queue_lock);
                    break;
                }

                case KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY:
                    inform_real_directory_added (watcher, recursive_virtual_path, recursive_real_path);
                    break;
                }
            }
            else
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 3u, virtual_file_system, KAN_LOG_ERROR,
                                     "Failed to query entry at \"%s\" to simulate addition of virtual path \"%s\".",
                                     recursive_real_path->path, recursive_virtual_path->path)
            }

            kan_file_system_path_container_reset_length (recursive_virtual_path, length_backup_virtual);
            kan_file_system_path_container_reset_length (recursive_real_path, length_backup_real);
        }

        kan_file_system_directory_iterator_destroy (directory_iterator);
    }
    else
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 3u, virtual_file_system, KAN_LOG_ERROR,
                             "Failed to open directory iterator at \"%s\" to addition addition of virtual path \"%s\".",
                             recursive_real_path->path, recursive_virtual_path->path)
    }
}

static void inform_mount_point_real_added (struct volume_t *volume,
                                           struct virtual_directory_t *owner_directory,
                                           struct mount_point_real_t *mount_point)
{
    struct file_system_watcher_t *file_system_watcher = volume->first_watcher;
    while (file_system_watcher)
    {
        if (file_system_watcher_is_observing_virtual_directory (file_system_watcher, owner_directory))
        {
            struct kan_file_system_path_container_t recursive_virtual_path;
            virtual_directory_form_path (owner_directory, &recursive_virtual_path);
            kan_file_system_path_container_append (&recursive_virtual_path, mount_point->name);

            struct kan_file_system_path_container_t recursive_real_path;
            kan_file_system_path_container_copy_string (&recursive_real_path, mount_point->real_directory_path);
            inform_real_directory_added (file_system_watcher, &recursive_virtual_path, &recursive_real_path);

            if (!file_system_watcher_add_real_watcher (file_system_watcher, mount_point,
                                                       mount_point->real_directory_path))
            {
                KAN_LOG_WITH_BUFFER (
                    KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, virtual_file_system, KAN_LOG_ERROR,
                    "Failed to create watcher for real path \"%s\" in order to watch virtual path \"%s\".",
                    mount_point->real_directory_path, recursive_virtual_path.path)
            }
        }

        file_system_watcher = file_system_watcher->next;
    }
}

static void inform_real_directory_removed (struct file_system_watcher_t *watcher,
                                           struct kan_file_system_path_container_t *recursive_virtual_path,
                                           struct kan_file_system_path_container_t *recursive_real_path)
{
    kan_file_system_directory_iterator_t directory_iterator =
        kan_file_system_directory_iterator_create (recursive_real_path->path);

    if (directory_iterator != KAN_INVALID_FILE_SYSTEM_DIRECTORY_ITERATOR)
    {
        const char *entry_name;
        while ((entry_name = kan_file_system_directory_iterator_advance (directory_iterator)))
        {
            if ((entry_name[0u] == '.' && entry_name[1u] == '\0') ||
                (entry_name[0u] == '.' && entry_name[1u] == '.' && entry_name[2u] == '\0'))
            {
                continue;
            }

            const uint64_t length_backup_virtual = recursive_virtual_path->length;
            kan_file_system_path_container_append (recursive_virtual_path, entry_name);

            const uint64_t length_backup_real = recursive_real_path->length;
            kan_file_system_path_container_append (recursive_real_path, entry_name);

            struct kan_file_system_entry_status_t entry_status;
            if (kan_file_system_query_entry (recursive_real_path->path, &entry_status))
            {
                switch (entry_status.type)
                {
                case KAN_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN:
                case KAN_FILE_SYSTEM_ENTRY_TYPE_FILE:
                {
                    kan_atomic_int_lock (&watcher->event_queue_lock);
                    struct file_system_watcher_event_node_t *event =
                        (struct file_system_watcher_event_node_t *) kan_event_queue_submit_begin (
                            &watcher->event_queue);

                    if (event)
                    {
                        event->event.event_type = KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_REMOVED;
                        event->event.entry_type = entry_status.type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE ?
                                                      KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE :
                                                      KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN;

                        kan_file_system_path_container_copy (&event->event.path_container, recursive_virtual_path);
                        kan_event_queue_submit_end (&watcher->event_queue,
                                                    &file_system_watcher_event_node_allocate ()->node);
                    }

                    kan_atomic_int_unlock (&watcher->event_queue_lock);
                    break;
                }

                case KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY:
                    inform_real_directory_removed (watcher, recursive_virtual_path, recursive_real_path);
                    break;
                }
            }
            else
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 3u, virtual_file_system, KAN_LOG_ERROR,
                                     "Failed to query entry at \"%s\" to simulate deletion of virtual path \"%s\".",
                                     recursive_real_path->path, recursive_virtual_path->path)
            }

            kan_file_system_path_container_reset_length (recursive_virtual_path, length_backup_virtual);
            kan_file_system_path_container_reset_length (recursive_real_path, length_backup_real);
        }

        kan_file_system_directory_iterator_destroy (directory_iterator);
    }
    else
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 3u, virtual_file_system, KAN_LOG_ERROR,
                             "Failed to open directory iterator at \"%s\" to simulate deletion of virtual path \"%s\".",
                             recursive_real_path->path, recursive_virtual_path->path)
    }

    kan_atomic_int_lock (&watcher->event_queue_lock);
    struct file_system_watcher_event_node_t *event =
        (struct file_system_watcher_event_node_t *) kan_event_queue_submit_begin (&watcher->event_queue);

    if (event)
    {
        event->event.event_type = KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_REMOVED;
        event->event.entry_type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY;
        kan_file_system_path_container_copy (&event->event.path_container, recursive_virtual_path);
        kan_event_queue_submit_end (&watcher->event_queue, &file_system_watcher_event_node_allocate ()->node);
    }

    kan_atomic_int_unlock (&watcher->event_queue_lock);
}

static void inform_mount_point_real_removed (struct volume_t *volume,
                                             struct virtual_directory_t *owner_directory,
                                             struct mount_point_real_t *mount_point)
{
    struct file_system_watcher_t *file_system_watcher = volume->first_watcher;
    while (file_system_watcher)
    {
        if (file_system_watcher_is_observing_virtual_directory (file_system_watcher, owner_directory))
        {
            struct kan_file_system_path_container_t recursive_virtual_path;
            virtual_directory_form_path (owner_directory, &recursive_virtual_path);
            kan_file_system_path_container_append (&recursive_virtual_path, mount_point->name);

            struct kan_file_system_path_container_t recursive_real_path;
            kan_file_system_path_container_copy_string (&recursive_real_path, mount_point->real_directory_path);
            inform_real_directory_removed (file_system_watcher, &recursive_virtual_path, &recursive_real_path);
        }

        for (uint64_t index = 0u; index < file_system_watcher->real_file_system_attachments.size;)
        {
            struct real_file_system_watcher_attachment_t *attachment =
                &((struct real_file_system_watcher_attachment_t *)
                      file_system_watcher->real_file_system_attachments.data)[index];

            if (attachment->mount_point == mount_point)
            {
                real_file_system_watcher_attachment_shutdown (attachment);
                kan_dynamic_array_remove_swap_at (&file_system_watcher->real_file_system_attachments, index);
            }
            else
            {
                ++index;
            }
        }

        file_system_watcher = file_system_watcher->next;
    }
}

static inline void append_read_only_pack_file_name (struct kan_file_system_path_container_t *container,
                                                    struct read_only_pack_file_node_t *file_node)
{
    if (file_node->name)
    {
        kan_file_system_path_container_append (container, file_node->name);
        if (file_node->extension)
        {
            kan_file_system_path_container_add_suffix (container, ".");
            kan_file_system_path_container_add_suffix (container, file_node->extension);
        }
    }
    else
    {
        KAN_ASSERT (file_node->extension)
        kan_file_system_path_container_append (container, ".");
        kan_file_system_path_container_add_suffix (container, file_node->extension);
    }
}

static void inform_read_only_pack_directory_added (struct file_system_watcher_t *watcher,
                                                   struct read_only_pack_directory_t *directory,
                                                   struct kan_file_system_path_container_t *recursive_path)
{
    const uint64_t length_backup = recursive_path->length;
    kan_file_system_path_container_append (recursive_path, directory->name);

    kan_atomic_int_lock (&watcher->event_queue_lock);
    struct file_system_watcher_event_node_t *directory_event =
        (struct file_system_watcher_event_node_t *) kan_event_queue_submit_begin (&watcher->event_queue);

    if (directory_event)
    {
        directory_event->event.event_type = KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_ADDED;
        directory_event->event.entry_type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY;
        kan_file_system_path_container_copy (&directory_event->event.path_container, recursive_path);
        kan_event_queue_submit_end (&watcher->event_queue, &file_system_watcher_event_node_allocate ()->node);
    }

    kan_atomic_int_unlock (&watcher->event_queue_lock);
    struct read_only_pack_file_node_t *file_node = (struct read_only_pack_file_node_t *) directory->files.items.first;

    while (file_node)
    {
        kan_atomic_int_lock (&watcher->event_queue_lock);
        struct file_system_watcher_event_node_t *event =
            (struct file_system_watcher_event_node_t *) kan_event_queue_submit_begin (&watcher->event_queue);

        if (event)
        {
            event->event.event_type = KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_ADDED;
            event->event.entry_type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE;
            kan_file_system_path_container_copy (&event->event.path_container, recursive_path);
            append_read_only_pack_file_name (&event->event.path_container, file_node);
            kan_event_queue_submit_end (&watcher->event_queue, &file_system_watcher_event_node_allocate ()->node);
        }

        kan_atomic_int_unlock (&watcher->event_queue_lock);
        file_node = (struct read_only_pack_file_node_t *) file_node->node.list_node.next;
    }

    struct read_only_pack_directory_t *child_directory = directory->first_child;
    while (child_directory)
    {
        inform_read_only_pack_directory_added (watcher, child_directory, recursive_path);
        child_directory = child_directory->next_on_level;
    }

    kan_file_system_path_container_reset_length (recursive_path, length_backup);
}

static void inform_mount_point_read_only_pack_added (struct volume_t *volume,
                                                     struct virtual_directory_t *owner_directory,
                                                     struct mount_point_read_only_pack_t *mount_point)
{
    struct file_system_watcher_t *file_system_watcher = volume->first_watcher;
    while (file_system_watcher)
    {
        if (file_system_watcher_is_observing_virtual_directory (file_system_watcher, owner_directory))
        {
            struct kan_file_system_path_container_t recursive_path;
            virtual_directory_form_path (owner_directory, &recursive_path);
            inform_read_only_pack_directory_added (file_system_watcher, &mount_point->root_directory, &recursive_path);
        }

        file_system_watcher = file_system_watcher->next;
    }
}

static void inform_read_only_pack_directory_removed (struct file_system_watcher_t *watcher,
                                                     struct read_only_pack_directory_t *directory,
                                                     struct kan_file_system_path_container_t *recursive_path)
{
    const uint64_t length_backup = recursive_path->length;
    kan_file_system_path_container_append (recursive_path, directory->name);
    struct read_only_pack_file_node_t *file_node = (struct read_only_pack_file_node_t *) directory->files.items.first;

    while (file_node)
    {
        kan_atomic_int_lock (&watcher->event_queue_lock);
        struct file_system_watcher_event_node_t *event =
            (struct file_system_watcher_event_node_t *) kan_event_queue_submit_begin (&watcher->event_queue);

        if (event)
        {
            event->event.event_type = KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_REMOVED;
            event->event.entry_type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE;
            kan_file_system_path_container_copy (&event->event.path_container, recursive_path);
            append_read_only_pack_file_name (&event->event.path_container, file_node);
            kan_event_queue_submit_end (&watcher->event_queue, &file_system_watcher_event_node_allocate ()->node);
        }

        kan_atomic_int_unlock (&watcher->event_queue_lock);
        file_node = (struct read_only_pack_file_node_t *) file_node->node.list_node.next;
    }

    struct read_only_pack_directory_t *child_directory = directory->first_child;
    while (child_directory)
    {
        inform_read_only_pack_directory_removed (watcher, child_directory, recursive_path);
        child_directory = child_directory->next_on_level;
    }

    kan_atomic_int_lock (&watcher->event_queue_lock);
    struct file_system_watcher_event_node_t *event =
        (struct file_system_watcher_event_node_t *) kan_event_queue_submit_begin (&watcher->event_queue);

    if (event)
    {
        event->event.event_type = KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_REMOVED;
        event->event.entry_type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY;
        kan_file_system_path_container_copy (&event->event.path_container, recursive_path);
        kan_event_queue_submit_end (&watcher->event_queue, &file_system_watcher_event_node_allocate ()->node);
    }

    kan_atomic_int_unlock (&watcher->event_queue_lock);
    kan_file_system_path_container_reset_length (recursive_path, length_backup);
}

static void inform_mount_point_read_only_pack_removed (struct volume_t *volume,
                                                       struct virtual_directory_t *owner_directory,
                                                       struct mount_point_read_only_pack_t *mount_point)
{
    struct file_system_watcher_t *file_system_watcher = volume->first_watcher;
    while (file_system_watcher)
    {
        if (file_system_watcher_is_observing_virtual_directory (file_system_watcher, owner_directory))
        {
            struct kan_file_system_path_container_t recursive_path;
            virtual_directory_form_path (owner_directory, &recursive_path);
            inform_read_only_pack_directory_removed (file_system_watcher, &mount_point->root_directory,
                                                     &recursive_path);
        }

        file_system_watcher = file_system_watcher->next;
    }
}

static void inform_virtual_directory_added (struct volume_t *volume, struct virtual_directory_t *directory)
{
    struct file_system_watcher_t *file_system_watcher = volume->first_watcher;
    while (file_system_watcher)
    {
        if (file_system_watcher_is_observing_virtual_directory (file_system_watcher, directory))
        {
            kan_atomic_int_lock (&file_system_watcher->event_queue_lock);
            struct file_system_watcher_event_node_t *event =
                (struct file_system_watcher_event_node_t *) kan_event_queue_submit_begin (
                    &file_system_watcher->event_queue);

            if (event)
            {
                event->event.event_type = KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_ADDED;
                event->event.entry_type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY;
                virtual_directory_form_path (directory, &event->event.path_container);
                kan_event_queue_submit_end (&file_system_watcher->event_queue,
                                            &file_system_watcher_event_node_allocate ()->node);
            }

            kan_atomic_int_unlock (&file_system_watcher->event_queue_lock);
        }

        file_system_watcher = file_system_watcher->next;
    }

    struct virtual_directory_t *child_directory = directory->first_child;
    while (child_directory)
    {
        inform_virtual_directory_added (volume, child_directory);
        child_directory = child_directory->next_on_level;
    }

    struct mount_point_real_t *mount_point_real = directory->first_mount_point_real;
    while (mount_point_real)
    {
        inform_mount_point_real_added (volume, directory, mount_point_real);
        mount_point_real = mount_point_real->next;
    }

    struct mount_point_read_only_pack_t *mount_point_read_only_pack = directory->first_mount_point_read_only_pack;
    while (mount_point_read_only_pack)
    {
        inform_mount_point_read_only_pack_added (volume, directory, mount_point_read_only_pack);
        mount_point_read_only_pack = mount_point_read_only_pack->next;
    }
}

static void inform_virtual_directory_removed (struct volume_t *volume, struct virtual_directory_t *directory)
{
    struct mount_point_real_t *mount_point_real = directory->first_mount_point_real;
    while (mount_point_real)
    {
        inform_mount_point_real_removed (volume, directory, mount_point_real);
        mount_point_real = mount_point_real->next;
    }

    struct mount_point_read_only_pack_t *mount_point_read_only_pack = directory->first_mount_point_read_only_pack;
    while (mount_point_read_only_pack)
    {
        inform_mount_point_read_only_pack_removed (volume, directory, mount_point_read_only_pack);
        mount_point_read_only_pack = mount_point_read_only_pack->next;
    }

    struct virtual_directory_t *child_directory = directory->first_child;
    while (child_directory)
    {
        inform_virtual_directory_removed (volume, child_directory);
        child_directory = child_directory->next_on_level;
    }

    struct file_system_watcher_t *file_system_watcher = volume->first_watcher;
    while (file_system_watcher)
    {
        if (file_system_watcher_is_observing_virtual_directory (file_system_watcher, directory))
        {
            kan_atomic_int_lock (&file_system_watcher->event_queue_lock);
            struct file_system_watcher_event_node_t *event =
                (struct file_system_watcher_event_node_t *) kan_event_queue_submit_begin (
                    &file_system_watcher->event_queue);

            if (event)
            {
                event->event.event_type = KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_REMOVED;
                event->event.entry_type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY;
                virtual_directory_form_path (directory, &event->event.path_container);
                kan_event_queue_submit_end (&file_system_watcher->event_queue,
                                            &file_system_watcher_event_node_allocate ()->node);
            }

            kan_atomic_int_unlock (&file_system_watcher->event_queue_lock);
            if (file_system_watcher->attached_to_virtual_directory == directory)
            {
                file_system_watcher->attached_to_virtual_directory = NULL;
            }
        }

        file_system_watcher = file_system_watcher->next;
    }
}

static inline void mount_point_real_fill_path (struct mount_point_real_t *mount_point_real,
                                               const char *path_iterator,
                                               struct kan_file_system_path_container_t *path_container)
{
    kan_file_system_path_container_copy_string (path_container, mount_point_real->real_directory_path);
    if (*path_iterator != '\0')
    {
        kan_file_system_path_container_append (path_container, path_iterator);
    }
}

static struct read_only_pack_directory_t *read_only_pack_directory_find_child (
    struct read_only_pack_directory_t *directory, const char *name_begin, const char *name_end)
{
    const uint64_t length = name_end - name_begin;
    struct read_only_pack_directory_t *child = directory->first_child;

    while (child)
    {
        if (strncmp (child->name, name_begin, length) == 0 && child->name[length] == '\0')
        {
            return child;
        }

        child = child->next_on_level;
    }

    return NULL;
}

static inline enum follow_path_result_t follow_read_only_pack_directory_path (
    struct read_only_pack_directory_t **current_read_only_pack_directory,
    const char **current_path_iterator,
    const char **current_part_begin,
    const char **current_part_end)
{
    while (KAN_TRUE)
    {
        switch (path_extract_next_part (current_path_iterator, current_part_begin, current_part_end))
        {
        case PATH_EXTRACTION_RESULT_HAS_MORE_COMPONENTS_AFTER:
        {
            struct read_only_pack_directory_t *next_read_only_pack_directory = read_only_pack_directory_find_child (
                *current_read_only_pack_directory, *current_part_begin, *current_part_end);

            if (!next_read_only_pack_directory)
            {
                return FOLLOW_PATH_RESULT_STOPPED;
            }

            *current_read_only_pack_directory = next_read_only_pack_directory;
            break;
        }

        case PATH_EXTRACTION_RESULT_LAST_COMPONENT:
        {
            struct read_only_pack_directory_t *next_read_only_pack_directory = read_only_pack_directory_find_child (
                *current_read_only_pack_directory, *current_part_begin, *current_part_end);

            if (!next_read_only_pack_directory)
            {
                return FOLLOW_PATH_RESULT_STOPPED;
            }

            *current_read_only_pack_directory = next_read_only_pack_directory;
            return FOLLOW_PATH_RESULT_REACHED_END;
        }

        case PATH_EXTRACTION_RESULT_FAILED:
            return FOLLOW_PATH_RESULT_FAILED;
        }
    }

    KAN_ASSERT (KAN_FALSE)
    return FOLLOW_PATH_RESULT_FAILED;
}

static struct read_only_pack_file_node_t *read_only_pack_directory_find_file (
    struct read_only_pack_directory_t *directory, const char *name_begin, const char *name_end)
{
    const char *separator = read_only_pack_file_find_name_separator (name_begin, name_end);
    const char *file_name_begin = name_begin;
    const char *file_name_end = separator ? separator : name_end;
    const char *extension_begin = separator ? separator + 1u : name_end;
    const char *extension_end = name_end;

    const uint64_t file_name_length = file_name_end - file_name_begin;
    const uint64_t extension_length = extension_end - extension_begin;
    uint64_t hash = kan_char_sequence_hash (name_begin, name_end);

    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (&directory->files, hash);
    struct read_only_pack_file_node_t *node = (struct read_only_pack_file_node_t *) bucket->first;
    const struct read_only_pack_file_node_t *node_end =
        (struct read_only_pack_file_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->node.hash == hash &&
            ((file_name_length == 0u && node->name == NULL) ||
             (file_name_length > 0u && node->name != NULL &&
              strncmp (file_name_begin, node->name, file_name_length) == 0 && node->name[file_name_length] == '\0')) &&
            ((extension_length == 0u && node->extension == NULL) ||
             (extension_length > 0u && node->extension != NULL &&
              strncmp (extension_begin, node->extension, extension_length) == 0 &&
              node->extension[extension_length] == '\0')))
        {
            return node;
        }

        node = (struct read_only_pack_file_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static struct read_only_pack_directory_t *read_only_pack_directory_find_or_create_child (
    struct read_only_pack_directory_t *directory, const char *name_begin, const char *name_end)
{
    struct read_only_pack_directory_t *child = read_only_pack_directory_find_child (directory, name_begin, name_end);
    if (child)
    {
        return child;
    }

    if (read_only_pack_directory_find_file (directory, name_begin, name_end))
    {
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                 "Failed to create directory for read only pack path: file with the same name already exists.")
        return NULL;
    }

    struct read_only_pack_directory_t *new_directory =
        kan_allocate_batched (hierarchy_allocation_group, sizeof (struct read_only_pack_directory_t));
    read_only_pack_directory_init (new_directory);
    new_directory->name = kan_char_sequence_intern (name_begin, name_end);
    new_directory->next_on_level = directory->first_child;
    directory->first_child = new_directory;
    return new_directory;
}

static kan_bool_t mount_point_read_only_pack_load_item (struct mount_point_read_only_pack_t *mount_point,
                                                        struct read_only_pack_registry_item_t *item)
{
    const char *path_iterator = item->path;
    struct read_only_pack_directory_t *current_directory = &mount_point->root_directory;

    while (KAN_TRUE)
    {
        const char *parse_start = path_iterator;
        const char *part_begin;
        const char *part_end;

        switch (path_extract_next_part (&path_iterator, &part_begin, &part_end))
        {
        case PATH_EXTRACTION_RESULT_HAS_MORE_COMPONENTS_AFTER:
            current_directory = read_only_pack_directory_find_or_create_child (current_directory, part_begin, part_end);

            if (!current_directory)
            {
                KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                         "Failed to continue follow path (inside read only pack) \"%s\" at \"%s\", child virtual "
                         "directory cannot be created.",
                         item->path, part_begin)
                return KAN_FALSE;
            }

            break;

        case PATH_EXTRACTION_RESULT_LAST_COMPONENT:
        {
            if (read_only_pack_directory_find_child (current_directory, part_begin, part_end) ||
                read_only_pack_directory_find_file (current_directory, part_begin, part_end))
            {
                KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                         "Failed to add file inside read only pack \"%s\", file with this name already exists!",
                         item->path)
                return KAN_FALSE;
            }

            const char *separator = read_only_pack_file_find_name_separator (part_begin, part_end);
            const char *name_begin = part_begin;
            const char *name_end = separator ? separator : part_end;
            const char *extension_begin = separator ? separator + 1u : part_end;
            const char *extension_end = part_end;

            struct read_only_pack_file_node_t *file_node = (struct read_only_pack_file_node_t *) kan_allocate_batched (
                read_only_pack_files_allocation_group, sizeof (struct read_only_pack_file_node_t));

            file_node->node.hash = kan_char_sequence_hash (part_begin, part_end);
            file_node->name = name_begin != name_end ? kan_char_sequence_intern (name_begin, name_end) : NULL;
            file_node->extension =
                extension_begin != extension_end ? kan_char_sequence_intern (extension_begin, extension_end) : NULL;

            file_node->offset = item->offset;
            file_node->size = item->size;

            if (current_directory->files.items.size >=
                KAN_VIRTUAL_FILE_SYSTEM_ROPACK_DIRECTORY_LOAD_FACTOR * current_directory->files.bucket_count)
            {
                kan_hash_storage_set_bucket_count (&current_directory->files,
                                                   current_directory->files.bucket_count * 2u);
            }

            kan_hash_storage_add (&current_directory->files, &file_node->node);
            return KAN_TRUE;
        }

        case PATH_EXTRACTION_RESULT_FAILED:
            KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                     "Failed to continue parsing path (inside read only pack) \"%s\" at \"%s\".", item->path,
                     parse_start)
            return KAN_FALSE;
        }
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static kan_bool_t mount_read_only_pack (struct volume_t *volume,
                                        struct virtual_directory_t *owner_directory,
                                        kan_interned_string_t pack_name,
                                        const char *pack_real_path)
{
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_read (pack_real_path, KAN_TRUE);
    if (!stream)
    {
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Unable to open read only pack at \"%s\".", pack_real_path)
        return KAN_FALSE;
    }

    uint64_t registry_offset;
    if (stream->operations->read (stream, sizeof (uint64_t), &registry_offset) != sizeof (uint64_t))
    {
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to read registry offset of read only pack at \"%s\".",
                 pack_real_path)
        return KAN_FALSE;
    }

    if (!stream->operations->seek (stream, KAN_STREAM_SEEK_CURRENT, ((int64_t) registry_offset) - sizeof (uint64_t)))
    {
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to seek to registry of read only pack at \"%s\".",
                 pack_real_path)
        return KAN_FALSE;
    }

    struct read_only_pack_registry_t registry;
    read_only_pack_registry_init (&registry);

    kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
        stream, &registry, type_name_read_only_pack_registry_t, serialization_script_storage,
        KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY, read_only_pack_operation_allocation_group);

    enum kan_serialization_state_t state = KAN_SERIALIZATION_IN_PROGRESS;
    while (state != KAN_SERIALIZATION_FINISHED)
    {
        switch ((state = kan_serialization_binary_reader_step (reader)))
        {
        case KAN_SERIALIZATION_IN_PROGRESS:
        case KAN_SERIALIZATION_FINISHED:
            break;

        case KAN_SERIALIZATION_FAILED:
            kan_serialization_binary_reader_destroy (reader);
            read_only_pack_registry_shutdown (&registry);
            stream->operations->close (stream);
            KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to read registry of read only pack at \"%s\".",
                     pack_real_path)
            return KAN_FALSE;
        }
    }

    kan_serialization_binary_reader_destroy (reader);
    stream->operations->close (stream);

    struct mount_point_read_only_pack_t *mount_point =
        kan_allocate_batched (hierarchy_allocation_group, sizeof (struct mount_point_read_only_pack_t));
    mount_point->next = owner_directory->first_mount_point_read_only_pack;
    mount_point->previous = NULL;
    owner_directory->first_mount_point_read_only_pack = mount_point;

    const uint64_t real_path_length = strlen (pack_real_path);
    mount_point->real_file_path =
        kan_allocate_general (hierarchy_allocation_group, real_path_length + 1u, _Alignof (char));
    memcpy (mount_point->real_file_path, pack_real_path, real_path_length + 1u);
    read_only_pack_directory_init (&mount_point->root_directory);
    mount_point->root_directory.name = pack_name;
    kan_bool_t result = KAN_TRUE;

    for (uint64_t index = 0u; index < registry.items.size; ++index)
    {
        struct read_only_pack_registry_item_t *item =
            &((struct read_only_pack_registry_item_t *) registry.items.data)[index];
        result &= mount_point_read_only_pack_load_item (mount_point, item);
    }

    read_only_pack_registry_shutdown (&registry);
    inform_mount_point_read_only_pack_added (volume, owner_directory, mount_point);
    return result;
}

kan_virtual_file_system_volume_t kan_virtual_file_system_volume_create (void)
{
    ensure_statics_initialized ();
    struct volume_t *volume = kan_allocate_batched (root_allocation_group, sizeof (struct volume_t));
    volume->root_directory.name = NULL;
    volume->root_directory.parent = NULL;
    volume->root_directory.next_on_level = NULL;
    volume->root_directory.first_child = NULL;
    volume->root_directory.first_mount_point_real = NULL;
    volume->root_directory.first_mount_point_read_only_pack = NULL;
    volume->first_watcher = NULL;
    return (kan_virtual_file_system_volume_t) volume;
}

kan_bool_t kan_virtual_file_system_volume_mount_real (kan_virtual_file_system_volume_t volume,
                                                      const char *mount_path,
                                                      const char *real_file_system_path)
{
    struct volume_t *volume_data = (struct volume_t *) volume;
    const char *path_iterator = mount_path;
    struct virtual_directory_t *current_directory = &volume_data->root_directory;
    const char *part_begin;
    const char *part_end;

    switch (follow_virtual_directory_path (&current_directory, &path_iterator, &part_begin, &part_end))
    {
    case FOLLOW_PATH_RESULT_REACHED_END:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to mount at \"%s\", \"%s\" already exists!", mount_path,
                 part_begin)
        return KAN_FALSE;

    case FOLLOW_PATH_RESULT_STOPPED:
    {
        const kan_bool_t at_last_component = *path_iterator == '\0';
        if (at_last_component)
        {
            if (virtual_directory_find_mount_point_real_by_raw_name (current_directory, part_begin, part_end) ||
                virtual_directory_find_mount_point_read_only_pack_by_raw_name (current_directory, part_begin, part_end))
            {
                KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to mount at \"%s\", \"%s\" already exists!",
                         mount_path, part_begin)
                return KAN_FALSE;
            }

            const uint64_t real_file_system_path_length = strlen (real_file_system_path);
            if (real_file_system_path_length == 0u)
            {
                KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                         "Failed to mount at \"%s\", given real file system path is empty!", mount_path)
                return KAN_FALSE;
            }

            struct mount_point_real_t *mount_point =
                kan_allocate_batched (hierarchy_allocation_group, sizeof (struct mount_point_real_t));
            mount_point->name = kan_char_sequence_intern (part_begin, part_end);

            mount_point->real_directory_path =
                kan_allocate_general (hierarchy_allocation_group, real_file_system_path_length + 1u, _Alignof (char));
            memcpy (mount_point->real_directory_path, real_file_system_path, real_file_system_path_length + 1u);

            mount_point->next = current_directory->first_mount_point_real;
            mount_point->previous = NULL;
            current_directory->first_mount_point_real = mount_point;
            mount_point->owner_directory = current_directory;
            inform_mount_point_real_added (volume_data, current_directory, mount_point);
            return KAN_TRUE;
        }

        KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                 "Failed to follow virtual path \"%s\" at \"%s\", directory does not exists.", mount_path, part_begin)
        return KAN_FALSE;
    }

    case FOLLOW_PATH_RESULT_FAILED:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".", mount_path,
                 part_begin)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

kan_bool_t kan_virtual_file_system_volume_mount_read_only_pack (kan_virtual_file_system_volume_t volume,
                                                                const char *mount_path,
                                                                const char *pack_real_path)
{
    struct volume_t *volume_data = (struct volume_t *) volume;
    const char *path_iterator = mount_path;
    struct virtual_directory_t *current_directory = &volume_data->root_directory;
    const char *part_begin;
    const char *part_end;

    switch (follow_virtual_directory_path (&current_directory, &path_iterator, &part_begin, &part_end))
    {
    case FOLLOW_PATH_RESULT_REACHED_END:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to mount at \"%s\", \"%s\" already exists!", mount_path,
                 part_begin)
        return KAN_FALSE;

    case FOLLOW_PATH_RESULT_STOPPED:
    {
        const kan_bool_t at_last_component = *path_iterator == '\0';
        if (at_last_component)
        {
            if (virtual_directory_find_mount_point_real_by_raw_name (current_directory, part_begin, part_end) ||
                virtual_directory_find_mount_point_read_only_pack_by_raw_name (current_directory, part_begin, part_end))
            {
                KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to mount at \"%s\", \"%s\" already exists!",
                         mount_path, part_begin)
                return KAN_FALSE;
            }

            return mount_read_only_pack (volume_data, current_directory,
                                         kan_char_sequence_intern (part_begin, part_end), pack_real_path);
        }

        KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                 "Failed to follow virtual path \"%s\" at \"%s\", directory does not exists.", mount_path, part_begin)
        return KAN_FALSE;
    }

    case FOLLOW_PATH_RESULT_FAILED:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".", mount_path,
                 part_begin)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

kan_bool_t kan_virtual_file_system_volume_unmount (kan_virtual_file_system_volume_t volume, const char *mount_path)
{
    struct volume_t *volume_data = (struct volume_t *) volume;
    const char *path_iterator = mount_path;
    struct virtual_directory_t *current_directory = &volume_data->root_directory;
    const char *part_begin;
    const char *part_end;

    switch (follow_virtual_directory_path (&current_directory, &path_iterator, &part_begin, &part_end))
    {
    case FOLLOW_PATH_RESULT_REACHED_END:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to mount at \"%s\", \"%s\" already exists!", mount_path,
                 part_begin)
        return KAN_FALSE;

    case FOLLOW_PATH_RESULT_STOPPED:
    {
        const kan_bool_t at_last_component = *path_iterator == '\0';
        if (at_last_component)
        {
            struct mount_point_real_t *mount_point_real =
                virtual_directory_find_mount_point_real_by_raw_name (current_directory, part_begin, part_end);

            if (mount_point_real)
            {
                if (mount_point_real->previous)
                {
                    mount_point_real->previous->next = mount_point_real->next;
                }
                else
                {
                    current_directory->first_mount_point_real = mount_point_real->next;
                }

                if (mount_point_real->next)
                {
                    mount_point_real->next->previous = mount_point_real->previous;
                }

                inform_mount_point_real_removed (volume_data, current_directory, mount_point_real);
                mount_point_real_shutdown (mount_point_real);
                kan_free_batched (hierarchy_allocation_group, mount_point_real);
                return KAN_TRUE;
            }

            struct mount_point_read_only_pack_t *mount_point_read_only_pack =
                virtual_directory_find_mount_point_read_only_pack_by_raw_name (current_directory, part_begin, part_end);

            if (mount_point_read_only_pack)
            {
                if (mount_point_read_only_pack->previous)
                {
                    mount_point_read_only_pack->previous->next = mount_point_read_only_pack->next;
                }
                else
                {
                    current_directory->first_mount_point_read_only_pack = mount_point_read_only_pack->next;
                }

                if (mount_point_read_only_pack->next)
                {
                    mount_point_read_only_pack->next->previous = mount_point_read_only_pack->previous;
                }

                inform_mount_point_read_only_pack_removed (volume_data, current_directory, mount_point_read_only_pack);
                mount_point_read_only_pack_shutdown (mount_point_read_only_pack);
                kan_free_batched (hierarchy_allocation_group, mount_point_read_only_pack);
                return KAN_TRUE;
            }
        }

        KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                 "Failed to follow virtual path \"%s\" at \"%s\", directory does not exists.", mount_path, part_begin)
        return KAN_FALSE;
    }

    case FOLLOW_PATH_RESULT_FAILED:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".", mount_path,
                 part_begin)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

void kan_virtual_file_system_volume_destroy (kan_virtual_file_system_volume_t volume)
{
    struct volume_t *volume_data = (struct volume_t *) volume;
    virtual_directory_shutdown (&volume_data->root_directory);

    while (volume_data->first_watcher)
    {
        struct file_system_watcher_t *watcher = volume_data->first_watcher;
        file_system_watcher_destroy (watcher);
    }

    kan_free_batched (root_allocation_group, volume_data);
}

struct kan_virtual_file_system_directory_iterator_t kan_virtual_file_system_directory_iterator_create (
    kan_virtual_file_system_volume_t volume, const char *directory_path)
{
    struct directory_iterator_t iterator = {
        .type = DIRECTORY_ITERATOR_TYPE_REAL_DIRECTORY,
        .real_file_system_iterator = KAN_INVALID_FILE_SYSTEM_DIRECTORY_ITERATOR,
    };

    struct volume_t *volume_data = (struct volume_t *) volume;
    const char *path_iterator = directory_path;
    struct virtual_directory_t *current_directory = &volume_data->root_directory;
    const char *part_begin;
    const char *part_end;

    switch (follow_virtual_directory_path (&current_directory, &path_iterator, &part_begin, &part_end))
    {
    case FOLLOW_PATH_RESULT_REACHED_END:
        iterator.type = DIRECTORY_ITERATOR_TYPE_VIRTUAL_DIRECTORY;
        iterator.virtual_directory_suffix = (struct virtual_directory_iterator_suffix_t) {
            .directory = current_directory,
            .stage = VIRTUAL_DIRECTORY_ITERATOR_STAGE_CHILDREN,
            .next_virtual_directory = current_directory->first_child,
        };
        break;

    case FOLLOW_PATH_RESULT_STOPPED:
    {
        struct mount_point_real_t *mount_point_real =
            virtual_directory_find_mount_point_real_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_real)
        {
            iterator.type = DIRECTORY_ITERATOR_TYPE_REAL_DIRECTORY;
            struct kan_file_system_path_container_t path_container;
            mount_point_real_fill_path (mount_point_real, path_iterator, &path_container);

            iterator.real_file_system_iterator = kan_file_system_directory_iterator_create (path_container.path);
            if (iterator.real_file_system_iterator == KAN_INVALID_FILE_SYSTEM_DIRECTORY_ITERATOR)
            {
                KAN_LOG_WITH_BUFFER (
                    KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, virtual_file_system, KAN_LOG_ERROR,
                    "Failed to create virtual file system iterator for virtual path \"%s\" (real path \"%s\").",
                    directory_path, path_container.path)
                iterator.type = DIRECTORY_ITERATOR_TYPE_INVALID;
            }

            break;
        }

        struct mount_point_read_only_pack_t *mount_point_read_only_pack =
            virtual_directory_find_mount_point_read_only_pack_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_read_only_pack)
        {
            struct read_only_pack_directory_t *read_only_pack_directory = &mount_point_read_only_pack->root_directory;
            switch (follow_read_only_pack_directory_path (&read_only_pack_directory, &path_iterator, &part_begin,
                                                          &part_end))
            {
            case FOLLOW_PATH_RESULT_REACHED_END:
                iterator.type = DIRECTORY_ITERATOR_TYPE_READ_ONLY_PACK_DIRECTORY;
                iterator.read_only_pack_suffix = (struct read_only_pack_directory_iterator_suffix_t) {
                    .directory = read_only_pack_directory,
                    .file_name_buffer =
                        kan_allocate_general (read_only_pack_directory_iterator_allocation_group,
                                              KAN_VIRTUAL_FILE_SYSTEM_ROPACK_MAX_FILE_NAME_LENGTH, _Alignof (char)),
                    .stage = READ_ONLY_PACK_DIRECTORY_ITERATOR_STAGE_CHILDREN,
                    .next_directory = read_only_pack_directory->first_child,
                };
                break;

            case FOLLOW_PATH_RESULT_STOPPED:
                KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                         "Failed to create virtual file system iterator for virtual path \"%s\": unable to follow path "
                         "since \"%s\".",
                         directory_path, part_begin)
                iterator.type = DIRECTORY_ITERATOR_TYPE_INVALID;

            case FOLLOW_PATH_RESULT_FAILED:
                KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".",
                         directory_path, part_begin)
                iterator.type = DIRECTORY_ITERATOR_TYPE_INVALID;
                break;
            }

            break;
        }

        KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                 "Failed to create virtual file system iterator for virtual path \"%s\": unable to follow path since "
                 "\"%s\".",
                 directory_path, part_begin)
        iterator.type = DIRECTORY_ITERATOR_TYPE_INVALID;
        break;
    }

    case FOLLOW_PATH_RESULT_FAILED:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".",
                 directory_path, part_begin)
        iterator.type = DIRECTORY_ITERATOR_TYPE_INVALID;
        break;
    }

    return KAN_PUN_TYPE (struct directory_iterator_t, struct kan_virtual_file_system_directory_iterator_t, iterator);
}

const char *kan_virtual_file_system_directory_iterator_advance (
    struct kan_virtual_file_system_directory_iterator_t *iterator)
{
    struct directory_iterator_t *iterator_data = (struct directory_iterator_t *) iterator;
    switch (iterator_data->type)
    {
    case DIRECTORY_ITERATOR_TYPE_INVALID:
        return NULL;

    case DIRECTORY_ITERATOR_TYPE_VIRTUAL_DIRECTORY:
        while (KAN_TRUE)
        {
            switch (iterator_data->virtual_directory_suffix.stage)
            {
            case VIRTUAL_DIRECTORY_ITERATOR_STAGE_CHILDREN:
                if (iterator_data->virtual_directory_suffix.next_virtual_directory)
                {
                    const char *name = iterator_data->virtual_directory_suffix.next_virtual_directory->name;
                    iterator_data->virtual_directory_suffix.next_virtual_directory =
                        iterator_data->virtual_directory_suffix.next_virtual_directory->next_on_level;
                    return name;
                }

                iterator_data->virtual_directory_suffix.stage = VIRTUAL_DIRECTORY_ITERATOR_STAGE_MOUNT_POINTS_REAL;
                iterator_data->virtual_directory_suffix.next_mount_point_real =
                    iterator_data->virtual_directory_suffix.directory->first_mount_point_real;
                break;

            case VIRTUAL_DIRECTORY_ITERATOR_STAGE_MOUNT_POINTS_REAL:
                if (iterator_data->virtual_directory_suffix.next_mount_point_real)
                {
                    const char *name = iterator_data->virtual_directory_suffix.next_mount_point_real->name;
                    iterator_data->virtual_directory_suffix.next_mount_point_real =
                        iterator_data->virtual_directory_suffix.next_mount_point_real->next;
                    return name;
                }

                iterator_data->virtual_directory_suffix.stage =
                    VIRTUAL_DIRECTORY_ITERATOR_STAGE_MOUNT_POINTS_READ_ONLY_PACK;
                iterator_data->virtual_directory_suffix.next_mount_point_read_only_pack =
                    iterator_data->virtual_directory_suffix.directory->first_mount_point_read_only_pack;
                break;

            case VIRTUAL_DIRECTORY_ITERATOR_STAGE_MOUNT_POINTS_READ_ONLY_PACK:
                if (iterator_data->virtual_directory_suffix.next_mount_point_read_only_pack)
                {
                    const char *name =
                        iterator_data->virtual_directory_suffix.next_mount_point_read_only_pack->root_directory.name;
                    iterator_data->virtual_directory_suffix.next_mount_point_read_only_pack =
                        iterator_data->virtual_directory_suffix.next_mount_point_read_only_pack->next;
                    return name;
                }

                return NULL;
            }
        }

        break;
        break;
    case DIRECTORY_ITERATOR_TYPE_REAL_DIRECTORY:
        return kan_file_system_directory_iterator_advance (iterator_data->real_file_system_iterator);

    case DIRECTORY_ITERATOR_TYPE_READ_ONLY_PACK_DIRECTORY:
        while (KAN_TRUE)
        {
            switch (iterator_data->read_only_pack_suffix.stage)
            {
            case READ_ONLY_PACK_DIRECTORY_ITERATOR_STAGE_CHILDREN:
                if (iterator_data->read_only_pack_suffix.next_directory)
                {
                    const char *name = iterator_data->read_only_pack_suffix.next_directory->name;
                    iterator_data->read_only_pack_suffix.next_directory =
                        iterator_data->read_only_pack_suffix.next_directory->next_on_level;
                    return name;
                }

                iterator_data->read_only_pack_suffix.stage = READ_ONLY_PACK_DIRECTORY_ITERATOR_STAGE_FILES;
                iterator_data->read_only_pack_suffix.next_file =
                    (struct read_only_pack_file_node_t *)
                        iterator_data->read_only_pack_suffix.directory->files.items.first;
                break;

            case READ_ONLY_PACK_DIRECTORY_ITERATOR_STAGE_FILES:
                if (iterator_data->read_only_pack_suffix.next_file)
                {
                    const uint64_t name_length = iterator_data->read_only_pack_suffix.next_file->name ?
                                                     strlen (iterator_data->read_only_pack_suffix.next_file->name) :
                                                     0u;

                    const uint64_t extension_length =
                        iterator_data->read_only_pack_suffix.next_file->extension ?
                            strlen (iterator_data->read_only_pack_suffix.next_file->extension) :
                            0u;

                    if (name_length > 0u)
                    {
                        memcpy (iterator_data->read_only_pack_suffix.file_name_buffer,
                                iterator_data->read_only_pack_suffix.next_file->name,
                                KAN_MIN (name_length, KAN_VIRTUAL_FILE_SYSTEM_ROPACK_MAX_FILE_NAME_LENGTH - 1u));
                    }

                    if (name_length < KAN_VIRTUAL_FILE_SYSTEM_ROPACK_MAX_FILE_NAME_LENGTH && extension_length > 0u)
                    {
                        iterator_data->read_only_pack_suffix.file_name_buffer[name_length] = '.';
                        if (name_length + 1u < KAN_VIRTUAL_FILE_SYSTEM_ROPACK_MAX_FILE_NAME_LENGTH)
                        {
                            memcpy (iterator_data->read_only_pack_suffix.file_name_buffer + name_length + 1u,
                                    iterator_data->read_only_pack_suffix.next_file->extension,
                                    KAN_MIN (extension_length,
                                             KAN_VIRTUAL_FILE_SYSTEM_ROPACK_MAX_FILE_NAME_LENGTH - name_length - 1u));
                        }
                    }

                    iterator_data->read_only_pack_suffix
                        .file_name_buffer[KAN_MIN (name_length + 1u + extension_length,
                                                   KAN_VIRTUAL_FILE_SYSTEM_ROPACK_MAX_FILE_NAME_LENGTH - 1u)] = '\0';

                    iterator_data->read_only_pack_suffix.next_file =
                        (struct read_only_pack_file_node_t *)
                            iterator_data->read_only_pack_suffix.next_file->node.list_node.next;

                    return iterator_data->read_only_pack_suffix.file_name_buffer;
                }

                return NULL;
            }
        }

        break;
    }

    KAN_ASSERT (KAN_FALSE)
    return NULL;
}

void kan_virtual_file_system_directory_iterator_destroy (struct kan_virtual_file_system_directory_iterator_t *iterator)
{
    struct directory_iterator_t *iterator_data = (struct directory_iterator_t *) iterator;
    switch (iterator_data->type)
    {
    case DIRECTORY_ITERATOR_TYPE_INVALID:
    case DIRECTORY_ITERATOR_TYPE_VIRTUAL_DIRECTORY:
        break;

    case DIRECTORY_ITERATOR_TYPE_REAL_DIRECTORY:
        kan_file_system_directory_iterator_destroy (iterator_data->real_file_system_iterator);
        break;

    case DIRECTORY_ITERATOR_TYPE_READ_ONLY_PACK_DIRECTORY:
        kan_free_general (read_only_pack_directory_iterator_allocation_group,
                          iterator_data->read_only_pack_suffix.file_name_buffer,
                          KAN_VIRTUAL_FILE_SYSTEM_ROPACK_MAX_FILE_NAME_LENGTH);
        break;
    }
}

kan_bool_t kan_virtual_file_system_query_entry (kan_virtual_file_system_volume_t volume,
                                                const char *path,
                                                struct kan_virtual_file_system_entry_status_t *status)
{
    struct volume_t *volume_data = (struct volume_t *) volume;
    const char *path_iterator = path;
    struct virtual_directory_t *current_directory = &volume_data->root_directory;
    const char *part_begin;
    const char *part_end;

    switch (follow_virtual_directory_path (&current_directory, &path_iterator, &part_begin, &part_end))
    {
    case FOLLOW_PATH_RESULT_REACHED_END:
        status->type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY;
        return KAN_TRUE;

    case FOLLOW_PATH_RESULT_STOPPED:
    {
        struct mount_point_real_t *mount_point_real =
            virtual_directory_find_mount_point_real_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_real)
        {
            struct kan_file_system_path_container_t path_container;
            mount_point_real_fill_path (mount_point_real, path_iterator, &path_container);
            struct kan_file_system_entry_status_t real_status;
            const kan_bool_t result = kan_file_system_query_entry (path_container.path, &real_status);

            if (result)
            {
                switch (real_status.type)
                {
                case KAN_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN:
                    status->type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN;
                    break;

                case KAN_FILE_SYSTEM_ENTRY_TYPE_FILE:
                    status->type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE;
                    status->size = real_status.size;
                    status->last_modification_time_ns = real_status.last_modification_time_ns;
                    status->read_only = real_status.read_only;
                    break;

                case KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY:
                    status->type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY;
                    break;
                }
            }

            return result;
        }

        struct mount_point_read_only_pack_t *mount_point_read_only_pack =
            virtual_directory_find_mount_point_read_only_pack_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_read_only_pack)
        {
            struct read_only_pack_directory_t *read_only_pack_directory = &mount_point_read_only_pack->root_directory;
            switch (follow_read_only_pack_directory_path (&read_only_pack_directory, &path_iterator, &part_begin,
                                                          &part_end))
            {
            case FOLLOW_PATH_RESULT_REACHED_END:
                status->type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY;
                return KAN_TRUE;

            case FOLLOW_PATH_RESULT_STOPPED:
                if (*path_iterator == '\0')
                {
                    struct read_only_pack_file_node_t *file_node =
                        read_only_pack_directory_find_file (read_only_pack_directory, part_begin, part_end);

                    if (file_node)
                    {
                        status->type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE;
                        status->size = file_node->size;
                        // We treat read only pack files as never modified.
                        status->last_modification_time_ns = 0u;
                        status->read_only = KAN_TRUE;
                        return 0u;
                    }
                }

                KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                         "Failed to query status for virtual path \"%s\": unable to follow path "
                         "since \"%s\".",
                         path, part_begin)
                return KAN_FALSE;

            case FOLLOW_PATH_RESULT_FAILED:
                KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".", path,
                         part_begin)
                return KAN_FALSE;
            }

            break;
        }

        KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                 "Failed to query status for virtual path \"%s\": unable to follow path "
                 "since \"%s\".",
                 path, part_begin)
        return KAN_FALSE;
    }

    case FOLLOW_PATH_RESULT_FAILED:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".", path,
                 part_begin)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

kan_bool_t kan_virtual_file_system_check_existence (kan_virtual_file_system_volume_t volume, const char *path)
{
    struct volume_t *volume_data = (struct volume_t *) volume;
    const char *path_iterator = path;
    struct virtual_directory_t *current_directory = &volume_data->root_directory;
    const char *part_begin;
    const char *part_end;

    switch (follow_virtual_directory_path (&current_directory, &path_iterator, &part_begin, &part_end))
    {
    case FOLLOW_PATH_RESULT_REACHED_END:
        return KAN_TRUE;

    case FOLLOW_PATH_RESULT_STOPPED:
    {
        struct mount_point_real_t *mount_point_real =
            virtual_directory_find_mount_point_real_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_real)
        {
            struct kan_file_system_path_container_t path_container;
            mount_point_real_fill_path (mount_point_real, path_iterator, &path_container);
            return kan_file_system_check_existence (path_container.path);
        }

        struct mount_point_read_only_pack_t *mount_point_read_only_pack =
            virtual_directory_find_mount_point_read_only_pack_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_read_only_pack)
        {
            struct read_only_pack_directory_t *read_only_pack_directory = &mount_point_read_only_pack->root_directory;
            switch (follow_read_only_pack_directory_path (&read_only_pack_directory, &path_iterator, &part_begin,
                                                          &part_end))
            {
            case FOLLOW_PATH_RESULT_REACHED_END:
                return KAN_TRUE;

            case FOLLOW_PATH_RESULT_STOPPED:
                if (*path_iterator == '\0')
                {
                    struct read_only_pack_file_node_t *file_node =
                        read_only_pack_directory_find_file (read_only_pack_directory, part_begin, part_end);

                    if (file_node)
                    {
                        return KAN_TRUE;
                    }
                }

                return KAN_FALSE;

            case FOLLOW_PATH_RESULT_FAILED:
                KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".", path,
                         part_begin)
                return KAN_FALSE;
            }

            break;
        }

        return KAN_FALSE;
    }

    case FOLLOW_PATH_RESULT_FAILED:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".", path,
                 part_begin)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

kan_bool_t kan_virtual_file_system_remove_file (kan_virtual_file_system_volume_t volume, const char *path)
{
    struct volume_t *volume_data = (struct volume_t *) volume;
    const char *path_iterator = path;
    struct virtual_directory_t *current_directory = &volume_data->root_directory;
    const char *part_begin;
    const char *part_end;

    switch (follow_virtual_directory_path (&current_directory, &path_iterator, &part_begin, &part_end))
    {
    case FOLLOW_PATH_RESULT_REACHED_END:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to remove file \"%s\": it is a directory.", path)
        return KAN_FALSE;

    case FOLLOW_PATH_RESULT_STOPPED:
    {
        struct mount_point_real_t *mount_point_real =
            virtual_directory_find_mount_point_real_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_real)
        {
            struct kan_file_system_path_container_t path_container;
            mount_point_real_fill_path (mount_point_real, path_iterator, &path_container);
            return kan_file_system_remove_file (path_container.path);
        }

        struct mount_point_read_only_pack_t *mount_point_read_only_pack =
            virtual_directory_find_mount_point_read_only_pack_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_read_only_pack)
        {
            KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                     "Failed to remove file \"%s\": it belongs to read only pack and cannot be modified or removed.",
                     path)
            return KAN_FALSE;
        }

        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to remove file \"%s\": does not exists.", path)
        return KAN_FALSE;
    }

    case FOLLOW_PATH_RESULT_FAILED:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".", path,
                 part_begin)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

kan_bool_t kan_virtual_file_system_make_directory (kan_virtual_file_system_volume_t volume, const char *path)
{
    struct volume_t *volume_data = (struct volume_t *) volume;
    const char *path_iterator = path;
    struct virtual_directory_t *current_directory = &volume_data->root_directory;
    const char *part_begin;
    const char *part_end;

    switch (follow_virtual_directory_path (&current_directory, &path_iterator, &part_begin, &part_end))
    {
    case FOLLOW_PATH_RESULT_REACHED_END:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to make directory \"%s\": already exists.", path)
        return KAN_FALSE;

    case FOLLOW_PATH_RESULT_STOPPED:
    {
        struct mount_point_real_t *mount_point_real =
            virtual_directory_find_mount_point_real_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_real)
        {
            struct kan_file_system_path_container_t path_container;
            kan_file_system_path_container_copy_string (&path_container, mount_point_real->real_directory_path);

            while (KAN_TRUE)
            {
                if (*path_iterator == '\0')
                {
                    return KAN_TRUE;
                }

                if (path_extract_next_part (&path_iterator, &part_begin, &part_end) == PATH_EXTRACTION_RESULT_FAILED)
                {
                    KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".",
                             path, part_begin)
                    return KAN_FALSE;
                }

                kan_file_system_path_container_append_char_sequence (&path_container, part_begin, part_end);
                if (!kan_file_system_check_existence (path_container.path) &&
                    !kan_file_system_make_directory (path_container.path))
                {
                    KAN_LOG_WITH_BUFFER (
                        KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, virtual_file_system, KAN_LOG_ERROR,
                        "Failed to create real directory \"%s\" in order to create virtual directory \"%s\".",
                        path_container.path, path)
                    return KAN_FALSE;
                }
            }

            KAN_ASSERT (KAN_FALSE)
            return KAN_FALSE;
        }

        struct mount_point_read_only_pack_t *mount_point_read_only_pack =
            virtual_directory_find_mount_point_read_only_pack_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_read_only_pack)
        {
            KAN_LOG (
                virtual_file_system, KAN_LOG_ERROR,
                "Failed to create directory \"%s\": it belongs to read only pack that cannot be modified or removed.",
                path)
            return KAN_FALSE;
        }

        while (KAN_TRUE)
        {
            struct virtual_directory_t *new_directory =
                kan_allocate_batched (hierarchy_allocation_group, sizeof (struct virtual_directory_t));

            new_directory->name = kan_char_sequence_intern (part_begin, part_end);
            new_directory->parent = current_directory;
            new_directory->next_on_level = current_directory->first_child;
            new_directory->first_child = NULL;
            new_directory->first_mount_point_real = NULL;
            new_directory->first_mount_point_read_only_pack = NULL;
            current_directory->first_child = new_directory;
            current_directory = new_directory;
            inform_virtual_directory_added (volume_data, new_directory);

            if (*path_iterator == '\0')
            {
                return KAN_TRUE;
            }

            if (path_extract_next_part (&path_iterator, &part_begin, &part_end) == PATH_EXTRACTION_RESULT_FAILED)
            {
                KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".", path,
                         part_begin)
                return KAN_FALSE;
            }
        }

        break;
    }

    case FOLLOW_PATH_RESULT_FAILED:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".", path,
                 part_begin)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

kan_bool_t kan_virtual_file_system_remove_directory_with_content (kan_virtual_file_system_volume_t volume,
                                                                  const char *path)
{
    struct volume_t *volume_data = (struct volume_t *) volume;
    const char *path_iterator = path;
    struct virtual_directory_t *current_directory = &volume_data->root_directory;
    const char *part_begin;
    const char *part_end;

    switch (follow_virtual_directory_path (&current_directory, &path_iterator, &part_begin, &part_end))
    {
    case FOLLOW_PATH_RESULT_REACHED_END:
        KAN_ASSERT (current_directory->parent)
        inform_virtual_directory_removed (volume_data, current_directory);
        virtual_directory_remove_child (current_directory->parent, current_directory);
        virtual_directory_shutdown (current_directory);
        kan_free_batched (hierarchy_allocation_group, current_directory);
        return KAN_TRUE;

    case FOLLOW_PATH_RESULT_STOPPED:
    {
        const kan_bool_t at_last_component = *path_iterator == '\0';
        struct mount_point_real_t *mount_point_real =
            virtual_directory_find_mount_point_real_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_real)
        {
            if (at_last_component)
            {
                KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                         "Failed to remove directory \"%s\": it is a mount point, use "
                         "kan_virtual_file_system_volume_unmount.",
                         path)
                return KAN_FALSE;
            }

            struct kan_file_system_path_container_t path_container;
            mount_point_real_fill_path (mount_point_real, path_iterator, &path_container);
            return kan_file_system_remove_directory_with_content (path_container.path);
        }

        struct mount_point_read_only_pack_t *mount_point_read_only_pack =
            virtual_directory_find_mount_point_read_only_pack_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_read_only_pack)
        {
            KAN_LOG (
                virtual_file_system, KAN_LOG_ERROR,
                "Failed to remove directory \"%s\": it belongs to read only pack and cannot be modified or removed.",
                path)
            return KAN_FALSE;
        }

        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to remove directory \"%s\": does not exists.", path)
        return KAN_FALSE;
    }

    case FOLLOW_PATH_RESULT_FAILED:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".", path,
                 part_begin)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

kan_bool_t kan_virtual_file_system_remove_empty_directory (kan_virtual_file_system_volume_t volume, const char *path)
{
    struct volume_t *volume_data = (struct volume_t *) volume;
    const char *path_iterator = path;
    struct virtual_directory_t *current_directory = &volume_data->root_directory;
    const char *part_begin;
    const char *part_end;

    switch (follow_virtual_directory_path (&current_directory, &path_iterator, &part_begin, &part_end))
    {
    case FOLLOW_PATH_RESULT_REACHED_END:
        if (current_directory->first_child || current_directory->first_mount_point_real ||
            current_directory->first_mount_point_read_only_pack)
        {
            KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to remove directory \"%s\": it is not empty.", path)
            return KAN_FALSE;
        }

        KAN_ASSERT (current_directory->parent)
        inform_virtual_directory_removed (volume_data, current_directory);
        virtual_directory_remove_child (current_directory->parent, current_directory);
        virtual_directory_shutdown (current_directory);
        kan_free_batched (hierarchy_allocation_group, current_directory);
        return KAN_TRUE;

    case FOLLOW_PATH_RESULT_STOPPED:
    {
        const kan_bool_t at_last_component = *path_iterator == '\0';
        struct mount_point_real_t *mount_point_real =
            virtual_directory_find_mount_point_real_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_real)
        {
            if (at_last_component)
            {
                KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                         "Failed to remove directory \"%s\": it is a mount point, use "
                         "kan_virtual_file_system_volume_unmount.",
                         path)
                return KAN_FALSE;
            }

            struct kan_file_system_path_container_t path_container;
            mount_point_real_fill_path (mount_point_real, path_iterator, &path_container);
            return kan_file_system_remove_empty_directory (path_container.path);
        }

        struct mount_point_read_only_pack_t *mount_point_read_only_pack =
            virtual_directory_find_mount_point_read_only_pack_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_read_only_pack)
        {
            KAN_LOG (
                virtual_file_system, KAN_LOG_ERROR,
                "Failed to remove directory \"%s\": it belongs to read only pack and cannot be modified or removed.",
                path)
            return KAN_FALSE;
        }

        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to remove directory \"%s\": does not exists.", path)
        return KAN_FALSE;
    }

    case FOLLOW_PATH_RESULT_FAILED:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".", path,
                 part_begin)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

struct kan_stream_t *kan_virtual_file_stream_open_for_read (kan_virtual_file_system_volume_t volume, const char *path)
{
    struct volume_t *volume_data = (struct volume_t *) volume;
    const char *path_iterator = path;
    struct virtual_directory_t *current_directory = &volume_data->root_directory;
    const char *part_begin;
    const char *part_end;

    switch (follow_virtual_directory_path (&current_directory, &path_iterator, &part_begin, &part_end))
    {
    case FOLLOW_PATH_RESULT_REACHED_END:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to open file for read \"%s\": it is a directory.", path)
        return NULL;

    case FOLLOW_PATH_RESULT_STOPPED:
    {
        struct mount_point_real_t *mount_point_real =
            virtual_directory_find_mount_point_real_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_real)
        {
            struct kan_file_system_path_container_t path_container;
            mount_point_real_fill_path (mount_point_real, path_iterator, &path_container);
            return kan_direct_file_stream_open_for_read (path_container.path, KAN_TRUE);
        }

        struct mount_point_read_only_pack_t *mount_point_read_only_pack =
            virtual_directory_find_mount_point_read_only_pack_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_read_only_pack)
        {
            struct read_only_pack_directory_t *read_only_pack_directory = &mount_point_read_only_pack->root_directory;
            switch (follow_read_only_pack_directory_path (&read_only_pack_directory, &path_iterator, &part_begin,
                                                          &part_end))
            {
            case FOLLOW_PATH_RESULT_REACHED_END:
                KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to open file for read \"%s\": it is a directory.",
                         path)
                return NULL;

            case FOLLOW_PATH_RESULT_STOPPED:
                if (*path_iterator == '\0')
                {
                    struct read_only_pack_file_node_t *file_node =
                        read_only_pack_directory_find_file (read_only_pack_directory, part_begin, part_end);

                    if (file_node)
                    {
                        struct kan_stream_t *real_stream =
                            kan_direct_file_stream_open_for_read (mount_point_read_only_pack->real_file_path, KAN_TRUE);

                        if (!real_stream)
                        {
                            KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                                     "Failed to open file for read \"%s\": failed to open read only pack \"%s\".", path,
                                     mount_point_read_only_pack->real_file_path)
                            return NULL;
                        }

                        struct read_only_pack_file_read_stream_t *stream =
                            (struct read_only_pack_file_read_stream_t *) kan_allocate_batched (
                                read_only_pack_operation_allocation_group,
                                sizeof (struct read_only_pack_file_read_stream_t));

                        stream->stream.operations = &read_only_pack_file_read_operations;
                        stream->base_stream = real_stream;
                        stream->offset = file_node->offset;
                        stream->size = file_node->size;
                        stream->position = 0u;
                        read_only_pack_file_seek (&stream->stream, KAN_STREAM_SEEK_START, 0);
                        return &stream->stream;
                    }
                }

                KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to open file for read \"%s\": does not exists.",
                         path)
                return NULL;

            case FOLLOW_PATH_RESULT_FAILED:
                KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".", path,
                         part_begin)
                return NULL;
            }

            break;
        }

        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to open file for read \"%s\": does not exists.", path)
        return NULL;
    }

    case FOLLOW_PATH_RESULT_FAILED:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".", path,
                 part_begin)
        return NULL;
    }

    KAN_ASSERT (KAN_FALSE)
    return NULL;
}

struct kan_stream_t *kan_virtual_file_stream_open_for_write (kan_virtual_file_system_volume_t volume, const char *path)
{
    struct volume_t *volume_data = (struct volume_t *) volume;
    const char *path_iterator = path;
    struct virtual_directory_t *current_directory = &volume_data->root_directory;
    const char *part_begin;
    const char *part_end;

    switch (follow_virtual_directory_path (&current_directory, &path_iterator, &part_begin, &part_end))
    {
    case FOLLOW_PATH_RESULT_REACHED_END:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to open file for write \"%s\": it is a directory.", path)
        return NULL;

    case FOLLOW_PATH_RESULT_STOPPED:
    {
        struct mount_point_real_t *mount_point_real =
            virtual_directory_find_mount_point_real_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_real)
        {
            struct kan_file_system_path_container_t path_container;
            mount_point_real_fill_path (mount_point_real, path_iterator, &path_container);
            return kan_direct_file_stream_open_for_write (path_container.path, KAN_TRUE);
        }

        struct mount_point_read_only_pack_t *mount_point_read_only_pack =
            virtual_directory_find_mount_point_read_only_pack_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_read_only_pack)
        {
            KAN_LOG (
                virtual_file_system, KAN_LOG_ERROR,
                "Failed to open file for write \"%s\": it belongs to read only pack and cannot be modified or removed.",
                path)
            return NULL;
        }

        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to open file for write \"%s\": does not exists.", path)
        return NULL;
    }

    case FOLLOW_PATH_RESULT_FAILED:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".", path,
                 part_begin)
        return NULL;
    }

    KAN_ASSERT (KAN_FALSE)
    return NULL;
}

kan_virtual_file_system_read_only_pack_builder_t kan_virtual_file_system_read_only_pack_builder_create (void)
{
    struct read_only_pack_builder_t *builder = (struct read_only_pack_builder_t *) kan_allocate_batched (
        read_only_pack_operation_allocation_group, sizeof (struct read_only_pack_builder_t));

    builder->output_stream = NULL;
    builder->beginning_offset_in_stream = 0u;
    read_only_pack_registry_init (&builder->registry);
    return (kan_virtual_file_system_read_only_pack_builder_t) builder;
}

kan_bool_t kan_virtual_file_system_read_only_pack_builder_begin (
    kan_virtual_file_system_read_only_pack_builder_t builder, struct kan_stream_t *output_stream)
{
    struct read_only_pack_builder_t *builder_data = (struct read_only_pack_builder_t *) builder;
    KAN_ASSERT (!builder_data->output_stream)
    KAN_ASSERT (kan_stream_is_writeable (output_stream))
    KAN_ASSERT (kan_stream_is_random_access (output_stream))

    builder_data->output_stream = output_stream;
    builder_data->beginning_offset_in_stream = output_stream->operations->tell (output_stream);
    uint64_t placeholder = 0u;

    if (output_stream->operations->write (output_stream, sizeof (uint64_t), &placeholder) != sizeof (uint64_t))
    {
        builder_data->output_stream = NULL;
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                 "Failed to write registry offset placeholder for new read only pack.")
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

kan_bool_t kan_virtual_file_system_read_only_pack_builder_add (kan_virtual_file_system_read_only_pack_builder_t builder,
                                                               struct kan_stream_t *input_stream,
                                                               const char *path_in_pack)
{
    struct read_only_pack_builder_t *builder_data = (struct read_only_pack_builder_t *) builder;
    KAN_ASSERT (builder_data->output_stream)
    KAN_ASSERT (kan_stream_is_readable (input_stream))
    char buffer[KAN_VIRTUAL_FILE_SYSTEM_ROPACK_BUILDER_CHUNK_SIZE];

    struct read_only_pack_registry_item_t *item =
        (struct read_only_pack_registry_item_t *) kan_dynamic_array_add_last (&builder_data->registry.items);

    if (!item)
    {
        kan_dynamic_array_set_capacity (&builder_data->registry.items, builder_data->registry.items.capacity * 2u);
        item = (struct read_only_pack_registry_item_t *) kan_dynamic_array_add_last (&builder_data->registry.items);
        KAN_ASSERT (item)
    }

    const uint64_t path_length = strlen (path_in_pack);
    item->path = kan_allocate_general (read_only_pack_operation_allocation_group, path_length + 1u, _Alignof (char));
    memcpy (item->path, path_in_pack, path_length + 1u);

    item->size = 0u;
    item->offset = builder_data->output_stream->operations->tell (builder_data->output_stream) -
                   builder_data->beginning_offset_in_stream;

    while (KAN_TRUE)
    {
        uint64_t read =
            input_stream->operations->read (input_stream, KAN_VIRTUAL_FILE_SYSTEM_ROPACK_BUILDER_CHUNK_SIZE, buffer);

        if (read > 0u)
        {
            item->size += read;
            if (builder_data->output_stream->operations->write (builder_data->output_stream, read, buffer) != read)
            {
                builder_data->output_stream = NULL;
                read_only_pack_registry_reset (&builder_data->registry);
                KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to write registry item at path \"%s\".",
                         item->path)
                return KAN_FALSE;
            }

            if (read != KAN_VIRTUAL_FILE_SYSTEM_ROPACK_BUILDER_CHUNK_SIZE)
            {
                // We've read everything.
                break;
            }
        }
        else
        {
            // We've read everything.
            break;
        }
    }

    return KAN_TRUE;
}

kan_bool_t kan_virtual_file_system_read_only_pack_builder_finalize (
    kan_virtual_file_system_read_only_pack_builder_t builder)
{
    struct read_only_pack_builder_t *builder_data = (struct read_only_pack_builder_t *) builder;
    KAN_ASSERT (builder_data->output_stream)

    const uint64_t registry_position = builder_data->output_stream->operations->tell (builder_data->output_stream);
    KAN_ASSERT (registry_position > builder_data->beginning_offset_in_stream)

    if (!builder_data->output_stream->operations->seek (builder_data->output_stream, KAN_STREAM_SEEK_START,
                                                        builder_data->beginning_offset_in_stream))
    {
        builder_data->output_stream = NULL;
        read_only_pack_registry_reset (&builder_data->registry);
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                 "Failed to seek to registry offset position for new read only pack.")
        return KAN_FALSE;
    }

    const uint64_t registry_offset = registry_position - builder_data->beginning_offset_in_stream;
    if (builder_data->output_stream->operations->write (builder_data->output_stream, sizeof (uint64_t),
                                                        &registry_offset) != sizeof (uint64_t))
    {
        builder_data->output_stream = NULL;
        read_only_pack_registry_reset (&builder_data->registry);
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to write registry offset for new read only pack.")
        return KAN_FALSE;
    }

    if (!builder_data->output_stream->operations->seek (builder_data->output_stream, KAN_STREAM_SEEK_START,
                                                        registry_position))
    {
        builder_data->output_stream = NULL;
        read_only_pack_registry_reset (&builder_data->registry);
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to seek to registry position for new read only pack.")
        return KAN_FALSE;
    }

    kan_serialization_binary_writer_t writer = kan_serialization_binary_writer_create (
        builder_data->output_stream, &builder_data->registry, type_name_read_only_pack_registry_t,
        serialization_script_storage, KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY);

    enum kan_serialization_state_t state = KAN_SERIALIZATION_IN_PROGRESS;
    while (state != KAN_SERIALIZATION_FINISHED)
    {
        switch ((state = kan_serialization_binary_writer_step (writer)))
        {
        case KAN_SERIALIZATION_IN_PROGRESS:
        case KAN_SERIALIZATION_FINISHED:
            break;

        case KAN_SERIALIZATION_FAILED:
            builder_data->output_stream = NULL;
            read_only_pack_registry_reset (&builder_data->registry);
            KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to write registry for new read only pack.")
            return KAN_FALSE;
        }
    }

    kan_serialization_binary_writer_destroy (writer);
    builder_data->output_stream = NULL;
    read_only_pack_registry_reset (&builder_data->registry);
    return KAN_TRUE;
}

void kan_virtual_file_system_read_only_pack_builder_destroy (kan_virtual_file_system_read_only_pack_builder_t builder)
{
    struct read_only_pack_builder_t *builder_data = (struct read_only_pack_builder_t *) builder;
    KAN_ASSERT (!builder_data->output_stream)
    read_only_pack_registry_shutdown (&builder_data->registry);
    kan_free_batched (read_only_pack_operation_allocation_group, builder_data);
}

kan_virtual_file_system_watcher_t kan_virtual_file_system_watcher_create (kan_virtual_file_system_volume_t volume,
                                                                          const char *directory_path)
{
    struct volume_t *volume_data = (struct volume_t *) volume;
    const char *path_iterator = directory_path;
    struct virtual_directory_t *current_directory = &volume_data->root_directory;
    const char *part_begin;
    const char *part_end;

    switch (follow_virtual_directory_path (&current_directory, &path_iterator, &part_begin, &part_end))
    {
    case FOLLOW_PATH_RESULT_REACHED_END:
        return (kan_virtual_file_system_watcher_t) file_system_watcher_create (volume_data, current_directory);

    case FOLLOW_PATH_RESULT_STOPPED:
    {
        struct mount_point_real_t *mount_point_real =
            virtual_directory_find_mount_point_real_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_real)
        {
            struct kan_file_system_path_container_t path_container;
            mount_point_real_fill_path (mount_point_real, path_iterator, &path_container);
            struct file_system_watcher_t *watcher = file_system_watcher_create (volume_data, NULL);

            if (!file_system_watcher_add_real_watcher (watcher, mount_point_real, path_container.path))
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, virtual_file_system, KAN_LOG_ERROR,
                                     "Failed to create file system watcher \"%s\": unable to watch real path \"%s\".",
                                     directory_path, path_container.path)
                file_system_watcher_destroy (watcher);
                return KAN_INVALID_VIRTUAL_FILE_SYSTEM_WATCHER;
            }

            return (kan_virtual_file_system_watcher_t) watcher;
        }

        struct mount_point_read_only_pack_t *mount_point_read_only_pack =
            virtual_directory_find_mount_point_read_only_pack_by_raw_name (current_directory, part_begin, part_end);

        if (mount_point_read_only_pack)
        {
            KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                     "Failed to create file system watcher \"%s\": path belongs to read only pack and therefore cannot "
                     "be modified or removed.",
                     directory_path)
            return KAN_INVALID_VIRTUAL_FILE_SYSTEM_WATCHER;
        }

        KAN_LOG (virtual_file_system, KAN_LOG_ERROR,
                 "Failed to create file system watcher for directory \"%s\": it does not exists.", directory_path)
        return KAN_INVALID_VIRTUAL_FILE_SYSTEM_WATCHER;
    }

    case FOLLOW_PATH_RESULT_FAILED:
        KAN_LOG (virtual_file_system, KAN_LOG_ERROR, "Failed to continue parsing path \"%s\" at \"%s\".",
                 directory_path, part_begin)
        return KAN_INVALID_VIRTUAL_FILE_SYSTEM_WATCHER;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_INVALID_VIRTUAL_FILE_SYSTEM_WATCHER;
}

void kan_virtual_file_system_watcher_destroy (kan_virtual_file_system_watcher_t watcher)
{
    file_system_watcher_destroy ((struct file_system_watcher_t *) watcher);
}

kan_virtual_file_system_watcher_iterator_t kan_virtual_file_system_watcher_iterator_create (
    kan_virtual_file_system_watcher_t watcher)
{
    struct file_system_watcher_t *watcher_data = (struct file_system_watcher_t *) watcher;
    kan_atomic_int_lock (&watcher_data->event_queue_lock);
    kan_event_queue_iterator_t iterator = kan_event_queue_iterator_create (&watcher_data->event_queue);
    kan_atomic_int_unlock (&watcher_data->event_queue_lock);
    return iterator;
}

const struct kan_virtual_file_system_watcher_event_t *kan_virtual_file_system_watcher_iterator_get (
    kan_virtual_file_system_watcher_t watcher, kan_virtual_file_system_watcher_iterator_t iterator)
{
    struct file_system_watcher_t *watcher_data = (struct file_system_watcher_t *) watcher;
    kan_atomic_int_lock (&watcher_data->event_queue_lock);

    const struct file_system_watcher_event_node_t *node =
        (const struct file_system_watcher_event_node_t *) kan_event_queue_iterator_get (&watcher_data->event_queue,
                                                                                        iterator);

    if (!node)
    {
        for (uint64_t index = 0u; index < watcher_data->real_file_system_attachments.size; ++index)
        {
            struct real_file_system_watcher_attachment_t *attachment =
                &((struct real_file_system_watcher_attachment_t *)
                      watcher_data->real_file_system_attachments.data)[index];
            const struct kan_file_system_watcher_event_t *real_event;

            struct kan_file_system_path_container_t base_position;
            kan_bool_t base_position_ready = KAN_FALSE;

            while ((real_event = kan_file_system_watcher_iterator_get (attachment->real_watcher, attachment->iterator)))
            {
                struct file_system_watcher_event_node_t *virtual_event =
                    (struct file_system_watcher_event_node_t *) kan_event_queue_submit_begin (
                        &watcher_data->event_queue);

                // We have iterators, therefore submit should always be started.
                KAN_ASSERT (virtual_event)

                switch (real_event->event_type)
                {
                case KAN_FILE_SYSTEM_EVENT_TYPE_ADDED:
                    virtual_event->event.event_type = KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_ADDED;
                    break;

                case KAN_FILE_SYSTEM_EVENT_TYPE_MODIFIED:
                    virtual_event->event.event_type = KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_MODIFIED;
                    break;

                case KAN_FILE_SYSTEM_EVENT_TYPE_REMOVED:
                    virtual_event->event.event_type = KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_REMOVED;
                    break;
                }

                switch (real_event->entry_type)
                {
                case KAN_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN:
                    virtual_event->event.entry_type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN;
                    break;

                case KAN_FILE_SYSTEM_ENTRY_TYPE_FILE:
                    virtual_event->event.entry_type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE;
                    break;

                case KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY:
                    virtual_event->event.entry_type = KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY;
                    break;
                }

                const uint64_t real_path_length = strlen (attachment->mount_point->real_directory_path);
                KAN_ASSERT (strncmp (real_event->path_container.path, attachment->mount_point->real_directory_path,
                                     real_path_length) == 0)

                if (!base_position_ready)
                {
                    virtual_directory_form_path (attachment->mount_point->owner_directory, &base_position);
                    kan_file_system_path_container_append (&base_position, attachment->mount_point->name);
                    base_position_ready = KAN_TRUE;
                }

                kan_file_system_path_container_copy (&virtual_event->event.path_container, &base_position);
                kan_file_system_path_container_append (&virtual_event->event.path_container,
                                                       real_event->path_container.path + real_path_length + 1u);
                kan_event_queue_submit_end (&watcher_data->event_queue,
                                            &file_system_watcher_event_node_allocate ()->node);

                attachment->iterator =
                    kan_file_system_watcher_iterator_advance (attachment->real_watcher, attachment->iterator);
            }
        }

        node = (const struct file_system_watcher_event_node_t *) kan_event_queue_iterator_get (
            &watcher_data->event_queue, iterator);
    }

    kan_atomic_int_unlock (&watcher_data->event_queue_lock);
    return node ? &node->event : NULL;
}

static inline void watcher_cleanup_events (struct file_system_watcher_t *watcher)
{
    struct file_system_watcher_event_node_t *node;
    while ((node = (struct file_system_watcher_event_node_t *) kan_event_queue_clean_oldest (&watcher->event_queue)))
    {
        file_system_watcher_event_node_free (node);
    }
}

kan_virtual_file_system_watcher_iterator_t kan_virtual_file_system_watcher_iterator_advance (
    kan_virtual_file_system_watcher_t watcher, kan_virtual_file_system_watcher_iterator_t iterator)
{
    struct file_system_watcher_t *watcher_data = (struct file_system_watcher_t *) watcher;
    kan_atomic_int_lock (&watcher_data->event_queue_lock);
    iterator = kan_event_queue_iterator_advance (iterator);

    watcher_cleanup_events (watcher_data);
    kan_atomic_int_unlock (&watcher_data->event_queue_lock);
    return iterator;
}

void kan_virtual_file_system_watcher_iterator_destroy (kan_virtual_file_system_watcher_t watcher,
                                                       kan_virtual_file_system_watcher_iterator_t iterator)
{
    struct file_system_watcher_t *watcher_data = (struct file_system_watcher_t *) watcher;
    kan_atomic_int_lock (&watcher_data->event_queue_lock);
    kan_event_queue_iterator_destroy (&watcher_data->event_queue, iterator);

    watcher_cleanup_events (watcher_data);
    kan_atomic_int_unlock (&watcher_data->event_queue_lock);
}
