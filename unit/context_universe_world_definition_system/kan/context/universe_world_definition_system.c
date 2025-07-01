#include <kan/container/hash_storage.h>
#include <kan/context/all_system_names.h>
#include <kan/context/hot_reload_coordination_system.h>
#include <kan/context/reflection_system.h>
#include <kan/context/universe_world_definition_system.h>
#include <kan/context/update_system.h>
#include <kan/context/virtual_file_system.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/precise_time/precise_time.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>

KAN_LOG_DEFINE_CATEGORY (universe_world_definition_system);
KAN_USE_STATIC_CPU_SECTIONS

struct world_definition_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
    struct kan_universe_world_definition_t definition;
};

struct rescan_stack_node_t
{
    struct rescan_stack_node_t *next;
    kan_interned_string_t to_rescan;
    bool is_binary;
    kan_time_size_t after_ns;
};

struct universe_world_definition_system_t
{
    kan_context_t context;
    kan_allocation_group_t group;
    kan_allocation_group_t observation_group;

    kan_interned_string_t definitions_mount_path;
    kan_instance_size_t definitions_mount_path_length;

    kan_reflection_registry_t registry;
    kan_serialization_binary_script_storage_t binary_script_storage;
    struct kan_hash_storage_t stored_world_definitions;
    bool first_scan_done;

    kan_virtual_file_system_watcher_t file_system_watcher;
    kan_virtual_file_system_watcher_iterator_t file_system_watcher_iterator;
    struct rescan_stack_node_t *first_rescan_node;
};

kan_context_system_t universe_world_definition_system_create (kan_allocation_group_t group, void *user_config)
{
    kan_cpu_static_sections_ensure_initialized ();
    struct universe_world_definition_system_t *system = kan_allocate_general (
        group, sizeof (struct universe_world_definition_system_t), alignof (struct universe_world_definition_system_t));

    system->group = group;
    system->observation_group = kan_allocation_group_get_child (group, "observation");

    if (user_config)
    {
        struct kan_universe_world_definition_system_config_t *config =
            (struct kan_universe_world_definition_system_config_t *) user_config;

        system->definitions_mount_path = config->definitions_mount_path;
        system->definitions_mount_path_length = (kan_instance_size_t) strlen (system->definitions_mount_path);
    }
    else
    {
        system->definitions_mount_path = NULL;
        system->definitions_mount_path_length = 0u;
    }

    kan_hash_storage_init (&system->stored_world_definitions, group, KAN_UNIVERSE_WORLD_DEFINITION_SYSTEM_BUCKETS);
    system->first_scan_done = false;

    system->file_system_watcher = KAN_HANDLE_SET_INVALID (kan_virtual_file_system_watcher_t);
    system->first_rescan_node = NULL;
    return KAN_HANDLE_SET (kan_context_system_t, system);
}

static inline bool extract_info_from_path (const char *path,
                                           kan_instance_size_t path_length,
                                           kan_instance_size_t base_path_length,
                                           kan_interned_string_t *name_output,
                                           bool *is_binary_output)
{
    if (base_path_length + 1u >= path_length)
    {
        return false;
    }

    const char *name_begin = path + base_path_length + 1u;
    const char *name_end = path + path_length;
    const kan_instance_size_t initial_name_length = (kan_instance_size_t) (name_end - name_begin);

    if (initial_name_length > 4u && *(name_end - 4u) == '.' && *(name_end - 3u) == 'b' && *(name_end - 2u) == 'i' &&
        *(name_end - 1u) == 'n')
    {
        name_end -= 4u;
        *name_output = kan_char_sequence_intern (name_begin, name_end);
        *is_binary_output = true;
        return true;
    }
    else if (initial_name_length > 3u && *(name_end - 3u) == '.' && *(name_end - 2u) == 'r' && *(name_end - 1u) == 'd')
    {
        name_end -= 3u;
        *name_output = kan_char_sequence_intern (name_begin, name_end);
        *is_binary_output = false;
        return true;
    }
    return false;
}

static inline void free_definition_node (struct universe_world_definition_system_t *system,
                                         struct world_definition_node_t *node)
{
    kan_allocation_group_stack_push (system->group);
    kan_universe_world_definition_shutdown (&node->definition);
    kan_allocation_group_stack_pop ();
    kan_free_batched (system->group, node);
}

static void scan_file (struct universe_world_definition_system_t *system,
                       kan_virtual_file_system_volume_t volume,
                       struct kan_file_system_path_container_t *scan_path_container)
{
    kan_interned_string_t name;
    bool is_binary;

    if (!extract_info_from_path (scan_path_container->path, scan_path_container->length,
                                 system->definitions_mount_path_length, &name, &is_binary))
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_world_definition_system, KAN_LOG_ERROR,
                             "Unable to extract info about \"%s\".", scan_path_container->path)
        return;
    }

    struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (volume, scan_path_container->path);
    if (!stream)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_world_definition_system, KAN_LOG_ERROR,
                             "Failed to open read stream for \"%s\".", scan_path_container->path)
        return;
    }

    stream = kan_random_access_stream_buffer_open_for_read (stream, KAN_UNIVERSE_WORLD_DEFINITION_SYSTEM_IO_BUFFER);
    struct world_definition_node_t *node =
        kan_allocate_batched (system->group, sizeof (struct world_definition_node_t));
    node->node.hash = KAN_HASH_OBJECT_POINTER (name);
    node->name = name;

    kan_allocation_group_stack_push (system->group);
    kan_universe_world_definition_init (&node->definition);
    kan_allocation_group_stack_pop ();
    bool deserialized = true;

    if (is_binary)
    {
        kan_interned_string_t type_name;
        const kan_interned_string_t expected_type_name = kan_string_intern ("kan_universe_world_definition_t");

        if (!kan_serialization_binary_read_type_header (
                stream, &type_name, KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t)))
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_world_definition_system, KAN_LOG_ERROR,
                                 "Failed to read binary type header \"%s\".", scan_path_container->path)
            deserialized = false;
        }
        else if (type_name != expected_type_name)
        {
            KAN_LOG_WITH_BUFFER (
                KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_world_definition_system, KAN_LOG_ERROR,
                "\"%s\" is not a world definition, only world definitions are expected in world definition sub path.",
                scan_path_container->path)
            deserialized = false;
        }
        else
        {
            kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
                stream, &node->definition, expected_type_name, system->binary_script_storage,
                KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t), system->group);

            enum kan_serialization_state_t state;
            while ((state = kan_serialization_binary_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
            {
            }

            kan_serialization_binary_reader_destroy (reader);
            if (state == KAN_SERIALIZATION_FAILED)
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_world_definition_system,
                                     KAN_LOG_ERROR, "Failed to deserialize binary world definition \"%s\".",
                                     scan_path_container->path)
                deserialized = false;
            }

            KAN_ASSERT (state == KAN_SERIALIZATION_FINISHED)
        }
    }
    else
    {
        kan_serialization_rd_reader_t reader = kan_serialization_rd_reader_create (
            stream, &node->definition, kan_string_intern ("kan_universe_world_definition_t"), system->registry,
            system->group);

        enum kan_serialization_state_t state;
        while ((state = kan_serialization_rd_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
        {
        }

        kan_serialization_rd_reader_destroy (reader);
        if (state == KAN_SERIALIZATION_FAILED)
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_world_definition_system, KAN_LOG_ERROR,
                                 "Failed to deserialize readable data world definition \"%s\".",
                                 scan_path_container->path)
            deserialized = false;
        }
        else
        {
            KAN_ASSERT (state == KAN_SERIALIZATION_FINISHED)
        }
    }

    stream->operations->close (stream);
    if (!deserialized)
    {
        free_definition_node (system, node);
        return;
    }

    kan_hash_storage_update_bucket_count_default (&system->stored_world_definitions,
                                                  KAN_UNIVERSE_WORLD_DEFINITION_SYSTEM_BUCKETS);
    kan_hash_storage_add (&system->stored_world_definitions, &node->node);
}

static void scan_directory (struct universe_world_definition_system_t *system,
                            kan_virtual_file_system_volume_t volume,
                            struct kan_file_system_path_container_t *scan_path_container)
{
    struct kan_virtual_file_system_directory_iterator_t iterator =
        kan_virtual_file_system_directory_iterator_create (volume, scan_path_container->path);
    const char *entry_name;

    while ((entry_name = kan_virtual_file_system_directory_iterator_advance (&iterator)))
    {
        if ((entry_name[0u] == '.' && entry_name[1u] == '\0') ||
            (entry_name[0u] == '.' && entry_name[1u] == '.' && entry_name[2u] == '\0'))
        {
            continue;
        }

        const kan_instance_size_t path_length = scan_path_container->length;
        kan_file_system_path_container_append (scan_path_container, entry_name);
        struct kan_virtual_file_system_entry_status_t status;

        if (kan_virtual_file_system_query_entry (volume, scan_path_container->path, &status))
        {
            switch (status.type)
            {
            case KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN:
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_world_definition_system,
                                     KAN_LOG_ERROR, "Entry \"%s\" has unknown type.", scan_path_container->path)
                break;

            case KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE:
                scan_file (system, volume, scan_path_container);
                break;

            case KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY:
                scan_directory (system, volume, scan_path_container);
                break;
            }
        }
        else
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_world_definition_system, KAN_LOG_ERROR,
                                 "Failed to get status of \"%s\"", scan_path_container->path)
        }

        kan_file_system_path_container_reset_length (scan_path_container, path_length);
    }

    kan_virtual_file_system_directory_iterator_destroy (&iterator);
}

static void universe_world_definition_system_on_reflection_generated (kan_context_system_t handle,
                                                                      kan_reflection_registry_t registry,
                                                                      kan_reflection_migration_seed_t migration_seed,
                                                                      kan_reflection_struct_migrator_t migrator)
{
    struct universe_world_definition_system_t *system = KAN_HANDLE_GET (handle);
    system->registry = registry;

    if (system->first_scan_done)
    {
        kan_serialization_binary_script_storage_destroy (system->binary_script_storage);
        system->binary_script_storage = kan_serialization_binary_script_storage_create (registry);
        return;
    }

    KAN_CPU_SCOPED_STATIC_SECTION (context_universe_world_definition_system_first_scan)
    system->first_scan_done = true;
    system->binary_script_storage = kan_serialization_binary_script_storage_create (registry);
    kan_context_system_t virtual_file_system =
        kan_context_query (system->context, KAN_CONTEXT_VIRTUAL_FILE_SYSTEM_NAME);

    if (!KAN_HANDLE_IS_VALID (virtual_file_system) || !system->definitions_mount_path)
    {
        return;
    }

    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_get_context_volume_for_read (virtual_file_system);
    struct kan_file_system_path_container_t scan_path_container;
    kan_file_system_path_container_copy_string (&scan_path_container, system->definitions_mount_path);
    scan_directory (system, volume, &scan_path_container);
    kan_virtual_file_system_close_context_read_access (virtual_file_system);

    kan_context_system_t hot_reload_system =
        kan_context_query (system->context, KAN_CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (hot_reload_system) &&
        kan_hot_reload_coordination_system_get_current_mode (hot_reload_system) != KAN_HOT_RELOAD_MODE_DISABLED &&
        !KAN_HANDLE_IS_VALID (system->file_system_watcher))
    {
        volume = kan_virtual_file_system_get_context_volume_for_write (virtual_file_system);
        system->file_system_watcher = kan_virtual_file_system_watcher_create (volume, system->definitions_mount_path);
        system->file_system_watcher_iterator =
            kan_virtual_file_system_watcher_iterator_create (system->file_system_watcher);
        kan_virtual_file_system_close_context_write_access (virtual_file_system);
    }
}

static void universe_world_definition_system_on_reflection_pre_shutdown (kan_context_system_t handle)
{
    struct universe_world_definition_system_t *system = KAN_HANDLE_GET (handle);
    kan_serialization_binary_script_storage_destroy (system->binary_script_storage);

    struct world_definition_node_t *node =
        (struct world_definition_node_t *) system->stored_world_definitions.items.first;

    while (node)
    {
        struct world_definition_node_t *next = (struct world_definition_node_t *) node->node.list_node.next;
        free_definition_node (system, node);
        node = next;
    }

    kan_hash_storage_shutdown (&system->stored_world_definitions);
}

static void remove_definition_by_name (struct universe_world_definition_system_t *system,
                                       kan_interned_string_t definition_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&system->stored_world_definitions, KAN_HASH_OBJECT_POINTER (definition_name));
    struct world_definition_node_t *node = (struct world_definition_node_t *) bucket->first;
    const struct world_definition_node_t *node_end =
        (struct world_definition_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == definition_name)
        {
            kan_hash_storage_remove (&system->stored_world_definitions, &node->node);
            free_definition_node (system, node);
            return;
        }

        node = (struct world_definition_node_t *) node->node.list_node.next;
    }
}

static void add_to_rescan_stack (struct universe_world_definition_system_t *system,
                                 struct kan_hot_reload_automatic_config_t *automatic_config,
                                 kan_interned_string_t definition_name,
                                 bool is_binary)
{
    struct rescan_stack_node_t *node = system->first_rescan_node;
    while (node)
    {
        if (node->to_rescan == definition_name)
        {
            KAN_ASSERT (node->is_binary == is_binary)
            break;
        }

        node = node->next;
    }

    if (!node)
    {
        node = kan_allocate_batched (system->observation_group, sizeof (struct rescan_stack_node_t));
        node->next = system->first_rescan_node;
        system->first_rescan_node = node;
        node->to_rescan = definition_name;
        node->is_binary = is_binary;
    }

    node->after_ns =
        kan_precise_time_get_elapsed_nanoseconds () + (kan_time_size_t) automatic_config->change_wait_time_ns;
}

static void universe_world_definition_system_update (kan_context_system_t handle)
{
    struct universe_world_definition_system_t *system = KAN_HANDLE_GET (handle);
    kan_context_system_t hot_reload_system =
        kan_context_query (system->context, KAN_CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_NAME);
    KAN_ASSERT (KAN_HANDLE_IS_VALID (hot_reload_system))

    struct kan_hot_reload_automatic_config_t *automatic_config =
        kan_hot_reload_coordination_system_get_automatic_config (hot_reload_system);
    const struct kan_virtual_file_system_watcher_event_t *event;

    while ((event = kan_virtual_file_system_watcher_iterator_get (system->file_system_watcher,
                                                                  system->file_system_watcher_iterator)))
    {
        if (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
        {
            kan_interned_string_t name;
            bool is_binary;

            if (extract_info_from_path (event->path_container.path, event->path_container.length,
                                        system->definitions_mount_path_length, &name, &is_binary))
            {
                switch (event->event_type)
                {
                case KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_ADDED:
                case KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_MODIFIED:
                    add_to_rescan_stack (system, automatic_config, name, is_binary);
                    break;

                case KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_REMOVED:
                    remove_definition_by_name (system, name);
                    break;
                }
            }
            else
            {
                KAN_LOG_WITH_BUFFER (
                    KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_world_definition_system, KAN_LOG_ERROR,
                    "Received event with file \"%s\", but unable to extract info about it.", event->path_container.path)
            }
        }

        system->file_system_watcher_iterator = kan_virtual_file_system_watcher_iterator_advance (
            system->file_system_watcher, system->file_system_watcher_iterator);
    }

    struct rescan_stack_node_t *previous_node = NULL;
    struct rescan_stack_node_t *current_node = system->first_rescan_node;

    const kan_time_size_t elapsed_ns = kan_precise_time_get_elapsed_nanoseconds ();
    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_string (&path_container, system->definitions_mount_path);
    kan_context_system_t virtual_file_system =
        kan_context_query (system->context, KAN_CONTEXT_VIRTUAL_FILE_SYSTEM_NAME);

    while (current_node)
    {
        struct rescan_stack_node_t *next_node = current_node->next;
        bool do_hot_reload = false;
        switch (kan_hot_reload_coordination_system_get_current_mode (hot_reload_system))
        {
        case KAN_HOT_RELOAD_MODE_DISABLED:
            KAN_ASSERT (false)
            break;

        case KAN_HOT_RELOAD_MODE_AUTOMATIC_INDEPENDENT:
            do_hot_reload = elapsed_ns >= current_node->after_ns;
            break;

        case KAN_HOT_RELOAD_MODE_ON_REQUEST:
            do_hot_reload = kan_hot_reload_coordination_system_is_hot_swap (hot_reload_system);
            break;
        }

        if (do_hot_reload)
        {
            remove_definition_by_name (system, current_node->to_rescan);
            kan_file_system_path_container_append (&path_container, current_node->to_rescan);

            if (current_node->is_binary)
            {
                kan_file_system_path_container_add_suffix (&path_container, ".bin");
            }
            else
            {
                kan_file_system_path_container_add_suffix (&path_container, ".rd");
            }

            kan_virtual_file_system_volume_t volume =
                kan_virtual_file_system_get_context_volume_for_read (virtual_file_system);
            scan_file (system, volume, &path_container);

            kan_virtual_file_system_close_context_read_access (virtual_file_system);
            kan_file_system_path_container_reset_length (&path_container, system->definitions_mount_path_length);

            if (previous_node)
            {
                previous_node->next = next_node;
            }
            else
            {
                system->first_rescan_node = next_node;
            }

            kan_free_batched (system->observation_group, current_node);
            current_node = next_node;
        }
        else
        {
            previous_node = current_node;
            current_node = next_node;
        }
    }
}

void universe_world_definition_system_connect (kan_context_system_t handle, kan_context_t context)
{
    struct universe_world_definition_system_t *system = KAN_HANDLE_GET (handle);
    system->context = context;

    kan_context_system_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    if (KAN_HANDLE_IS_VALID (reflection_system))
    {
        kan_reflection_system_connect_on_generated (reflection_system, handle,
                                                    universe_world_definition_system_on_reflection_generated);
        kan_reflection_system_connect_on_pre_shutdown (reflection_system, handle,
                                                       universe_world_definition_system_on_reflection_pre_shutdown);
    }

    kan_context_system_t hot_reload_system =
        kan_context_query_no_connect (system->context, KAN_CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (hot_reload_system) &&
        kan_hot_reload_coordination_system_get_current_mode (hot_reload_system) != KAN_HOT_RELOAD_MODE_DISABLED)
    {
        kan_context_system_t update_system = kan_context_query (system->context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
        if (KAN_HANDLE_IS_VALID (update_system))
        {
            kan_context_system_t universe_system =
                kan_context_query_no_connect (system->context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME);

            kan_update_system_connect_on_run (update_system, handle, universe_world_definition_system_update, 1u,
                                              &hot_reload_system, 1u, &universe_system);
        }
    }
}

void universe_world_definition_system_init (kan_context_system_t handle) {}

void universe_world_definition_system_shutdown (kan_context_system_t handle)
{
    struct universe_world_definition_system_t *system = KAN_HANDLE_GET (handle);
    if (KAN_HANDLE_IS_VALID (system->file_system_watcher))
    {
        kan_virtual_file_system_watcher_iterator_destroy (system->file_system_watcher,
                                                          system->file_system_watcher_iterator);
        kan_virtual_file_system_watcher_destroy (system->file_system_watcher);
    }
}

void universe_world_definition_system_disconnect (kan_context_system_t handle)
{
    struct universe_world_definition_system_t *system = KAN_HANDLE_GET (handle);
    kan_context_system_t reflection_system = kan_context_query (system->context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (reflection_system))
    {
        kan_reflection_system_disconnect_on_generated (reflection_system, handle);
        kan_reflection_system_disconnect_on_pre_shutdown (reflection_system, handle);
    }

    kan_context_system_t hot_reload_system =
        kan_context_query (system->context, KAN_CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (hot_reload_system) &&
        kan_hot_reload_coordination_system_get_current_mode (hot_reload_system) != KAN_HOT_RELOAD_MODE_DISABLED)
    {
        kan_context_system_t update_system = kan_context_query (system->context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
        if (KAN_HANDLE_IS_VALID (update_system))
        {
            kan_update_system_disconnect_on_run (update_system, handle);
        }
    }
}

void universe_world_definition_system_destroy (kan_context_system_t handle)
{
    struct universe_world_definition_system_t *system = KAN_HANDLE_GET (handle);
    while (system->first_rescan_node)
    {
        struct rescan_stack_node_t *next = system->first_rescan_node->next;
        kan_free_batched (system->observation_group, system->first_rescan_node);
        system->first_rescan_node = next;
    }

    kan_free_general (system->group, system, sizeof (struct universe_world_definition_system_t));
}

CONTEXT_UNIVERSE_WORLD_DEFINITION_SYSTEM_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (
    universe_world_definition_system_t) = {
    .name = "universe_world_definition_system_t",
    .create = universe_world_definition_system_create,
    .connect = universe_world_definition_system_connect,
    .connected_init = universe_world_definition_system_init,
    .connected_shutdown = universe_world_definition_system_shutdown,
    .disconnect = universe_world_definition_system_disconnect,
    .destroy = universe_world_definition_system_destroy,
};

void kan_universe_world_definition_system_config_init (struct kan_universe_world_definition_system_config_t *instance)
{
    instance->definitions_mount_path = NULL;
}

const struct kan_universe_world_definition_t *kan_universe_world_definition_system_query (
    kan_context_system_t universe_world_definition_system, kan_interned_string_t definition_name)
{
    struct universe_world_definition_system_t *system = KAN_HANDLE_GET (universe_world_definition_system);
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&system->stored_world_definitions, KAN_HASH_OBJECT_POINTER (definition_name));
    struct world_definition_node_t *node = (struct world_definition_node_t *) bucket->first;
    const struct world_definition_node_t *node_end =
        (struct world_definition_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == definition_name)
        {
            return &node->definition;
        }

        node = (struct world_definition_node_t *) node->node.list_node.next;
    }

    return NULL;
}
