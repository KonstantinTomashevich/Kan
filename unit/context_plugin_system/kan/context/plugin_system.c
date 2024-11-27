#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <kan/container/interned_string.h>
#include <kan/context/plugin_system.h>
#include <kan/context/reflection_system.h>
#include <kan/context/update_system.h>
#include <kan/error/critical.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/path_container.h>
#include <kan/file_system/stream.h>
#include <kan/file_system_watcher/watcher.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/platform/dynamic_library.h>
#include <kan/platform/precise_time.h>

KAN_LOG_DEFINE_CATEGORY (plugin_system);

struct plugin_data_t
{
    kan_interned_string_t name;
    kan_platform_dynamic_library_t dynamic_library;
    kan_time_size_t last_loaded_file_time_stamp_ns;
};

struct plugin_system_t
{
    kan_context_t context;
    kan_allocation_group_t group;

    kan_interned_string_t plugins_directory_path;
    kan_bool_t enable_hot_reload;
    kan_time_offset_t hot_reload_update_delay_ns;
    kan_instance_size_t hot_reload_directory_id;

    /// \meta reflection_dynamic_array_type = "struct plugin_data_t"
    struct kan_dynamic_array_t plugins;
    kan_time_size_t newest_loaded_plugin_last_modification_file_time_ns;

    kan_time_size_t hot_reload_after_ns;
    kan_file_system_watcher_t watcher;
    kan_file_system_watcher_iterator_t watcher_iterator;
};

static kan_bool_t statics_initialized = KAN_FALSE;
static kan_allocation_group_t config_allocation_group;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        config_allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "plugin_system_config");
        statics_initialized = KAN_TRUE;
    }
}

kan_context_system_t plugin_system_create (kan_allocation_group_t group, void *user_config)
{
    struct plugin_system_t *system =
        kan_allocate_general (group, sizeof (struct plugin_system_t), _Alignof (struct plugin_system_t));
    system->group = group;

    if (user_config)
    {
        struct kan_plugin_system_config_t *config = user_config;
        KAN_ASSERT (config->plugin_directory_path)
        system->plugins_directory_path = config->plugin_directory_path;

        system->enable_hot_reload = config->enable_hot_reload;
        system->hot_reload_update_delay_ns = config->hot_reload_update_delay_ns;
        kan_dynamic_array_init (&system->plugins, config->plugins.size, sizeof (struct plugin_data_t),
                                _Alignof (struct plugin_data_t), group);

        for (kan_loop_size_t index = 0u; index < config->plugins.size; ++index)
        {
            kan_interned_string_t plugin_name = ((kan_interned_string_t *) config->plugins.data)[index];
            struct plugin_data_t *data = kan_dynamic_array_add_last (&system->plugins);
            KAN_ASSERT (data)
            data->name = plugin_name;
            data->dynamic_library = KAN_HANDLE_SET_INVALID (kan_platform_dynamic_library_t);
            data->last_loaded_file_time_stamp_ns = 0u;
        }
    }
    else
    {
        system->plugins_directory_path = NULL;
        system->enable_hot_reload = KAN_FALSE;
        system->hot_reload_update_delay_ns = 0u;
        kan_dynamic_array_init (&system->plugins, 0u, sizeof (struct plugin_data_t), _Alignof (struct plugin_data_t),
                                group);
    }

    system->newest_loaded_plugin_last_modification_file_time_ns = 0u;
    system->hot_reload_directory_id = 0u;
    system->hot_reload_after_ns = KAN_INT_MAX (kan_time_size_t);
    system->watcher = KAN_HANDLE_SET_INVALID (kan_file_system_watcher_t);
    return KAN_HANDLE_SET (kan_context_system_t, system);
}

static inline kan_bool_t find_source_plugin_path (const char *source_path,
                                                  const char *plugin_name,
                                                  char *buffer,
                                                  const char **output_extension)
{
    snprintf (buffer, KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, "%s/%s.dll", source_path, plugin_name);
    if (kan_file_system_check_existence (buffer))
    {
        *output_extension = ".dll";
        return KAN_TRUE;
    }

    snprintf (buffer, KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, "%s/%s.so", source_path, plugin_name);
    if (kan_file_system_check_existence (buffer))
    {
        *output_extension = ".so";
        return KAN_TRUE;
    }

    snprintf (buffer, KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, "%s/lib%s.so", source_path, plugin_name);
    if (kan_file_system_check_existence (buffer))
    {
        *output_extension = ".so";
        return KAN_TRUE;
    }

    snprintf (buffer, KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, "%s/lib%s.so.0", source_path, plugin_name);
    if (kan_file_system_check_existence (buffer))
    {
        *output_extension = ".so.0";
        return KAN_TRUE;
    }

    return KAN_FALSE;
}

static inline void load_plugins (const char *path,
                                 struct kan_dynamic_array_t *array,
                                 kan_time_size_t *newest_loaded_file_time_ns_output)
{
    *newest_loaded_file_time_ns_output = 0u;
    for (kan_loop_size_t index = 0u; index < array->size; ++index)
    {
        struct plugin_data_t *data = &((struct plugin_data_t *) array->data)[index];
        KAN_ASSERT (!KAN_HANDLE_IS_VALID (data->dynamic_library))
        char library_path_buffer[KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u];
        const char *extension;

        if (find_source_plugin_path (path, data->name, library_path_buffer, &extension))
        {
            data->dynamic_library = kan_platform_dynamic_library_load (library_path_buffer);
            if (!KAN_HANDLE_IS_VALID (data->dynamic_library))
            {
                data->last_loaded_file_time_stamp_ns = 0u;
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 3u, plugin_system, KAN_LOG_ERROR,
                                     "Failed to load dynamic library from \"%s\".", library_path_buffer)
            }
            else
            {
                struct kan_file_system_entry_status_t status;
                if (kan_file_system_query_entry (library_path_buffer, &status))
                {
                    data->last_loaded_file_time_stamp_ns = status.last_modification_time_ns;
                    if (data->last_loaded_file_time_stamp_ns > *newest_loaded_file_time_ns_output)
                    {
                        *newest_loaded_file_time_ns_output = data->last_loaded_file_time_stamp_ns;
                    }
                }
                else
                {
                    KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 3u, plugin_system, KAN_LOG_ERROR,
                                         "Failed to query entry status of \"%s\".", library_path_buffer)
                    data->last_loaded_file_time_stamp_ns = 0u;
                }
            }
        }
        else
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, plugin_system, KAN_LOG_ERROR,
                                 "Unable to find dynamic library \"%s\" at directory \"%s\".", data->name, path)
        }
    }
}

static inline void unload_plugins (struct kan_dynamic_array_t *array)
{
    for (kan_loop_size_t index = 0u; index < array->size; ++index)
    {
        struct plugin_data_t *data = &((struct plugin_data_t *) array->data)[index];
        if (KAN_HANDLE_IS_VALID (data->dynamic_library))
        {
            kan_platform_dynamic_library_unload (data->dynamic_library);
        }
    }
}

static void on_reflection_populate (kan_context_system_t other_system, kan_reflection_registry_t registry)
{
    struct plugin_system_t *system = KAN_HANDLE_GET (other_system);
    for (kan_loop_size_t index = 0u; index < system->plugins.size; ++index)
    {
        struct plugin_data_t *data = &((struct plugin_data_t *) system->plugins.data)[index];
        if (KAN_HANDLE_IS_VALID (data->dynamic_library))
        {
            typedef void (*registrar_function_t) (kan_reflection_registry_t registry);
            registrar_function_t function = (registrar_function_t) kan_platform_dynamic_library_find_function (
                data->dynamic_library, KAN_CONTEXT_REFLECTION_SYSTEM_REGISTRAR_FUNCTION_NAME);

            if (function)
            {
                function (registry);
            }
            else
            {
                KAN_LOG (plugin_system, KAN_LOG_ERROR, "Unable to find registrar function \"%s\" in plugin \"%s\".",
                         KAN_CONTEXT_REFLECTION_SYSTEM_REGISTRAR_FUNCTION_NAME, data->name)
            }
        }
    }
}

static inline void build_hot_reload_directory_path (struct kan_file_system_path_container_t *path_container,
                                                    const char *base_path,
                                                    kan_instance_size_t hot_reload_id)
{
    kan_file_system_path_container_copy_string (path_container, base_path);
    char suffix_buffer[KAN_PLUGIN_SYSTEM_HOT_RELOAD_SUFFIX_BUFFER];
    snprintf (suffix_buffer, KAN_PLUGIN_SYSTEM_HOT_RELOAD_SUFFIX_BUFFER, "_%llu", (unsigned long long) hot_reload_id);
    kan_file_system_path_container_add_suffix (path_container, suffix_buffer);
}

static inline void update_hot_reload_id (struct plugin_system_t *system)
{
    while (KAN_TRUE)
    {
        struct kan_file_system_path_container_t path_container;
        build_hot_reload_directory_path (&path_container, system->plugins_directory_path,
                                         system->hot_reload_directory_id);

        if (!kan_file_system_check_existence (path_container.path) &&
            kan_file_system_make_directory (path_container.path))
        {
            break;
        }

        ++system->hot_reload_directory_id;
    }
}

static inline void init_hot_reload_directory (struct plugin_system_t *system)
{
    for (kan_loop_size_t index = 0u; index < system->plugins.size; ++index)
    {
        struct plugin_data_t *data = &((struct plugin_data_t *) system->plugins.data)[index];
        char library_path_buffer[KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u];
        const char *extension;

        if (find_source_plugin_path (system->plugins_directory_path, data->name, library_path_buffer, &extension))
        {
            struct kan_stream_t *input_stream = kan_direct_file_stream_open_for_read (library_path_buffer, KAN_TRUE);
            if (!input_stream)
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 3u, plugin_system, KAN_LOG_ERROR,
                                     "Failed to open for read dynamic library \"%s\" for copying.", library_path_buffer)
                continue;
            }

            struct kan_file_system_path_container_t output_path_container;
            build_hot_reload_directory_path (&output_path_container, system->plugins_directory_path,
                                             system->hot_reload_directory_id);

            kan_file_system_path_container_append (&output_path_container, data->name);
            kan_file_system_path_container_add_suffix (&output_path_container, extension);
            struct kan_stream_t *output_stream =
                kan_direct_file_stream_open_for_write (output_path_container.path, KAN_TRUE);

            if (!output_stream)
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, plugin_system, KAN_LOG_ERROR,
                                     "Failed to open for write dynamic library \"%s\" for copying.",
                                     output_path_container.path)
                continue;
            }

            char copy_buffer[KAN_PLUGIN_SYSTEM_HOT_RELOAD_IO_BUFFER];
            while (KAN_TRUE)
            {
                const kan_memory_size_t read =
                    input_stream->operations->read (input_stream, KAN_PLUGIN_SYSTEM_HOT_RELOAD_IO_BUFFER, copy_buffer);

                if (read == 0u)
                {
                    break;
                }

                const kan_memory_size_t written = output_stream->operations->write (output_stream, read, copy_buffer);
                if (written != read)
                {
                    KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 4u, plugin_system, KAN_LOG_ERROR,
                                         "Failed to copy dynamic library \"%s\" to \"%s\".", library_path_buffer,
                                         output_path_container.path)
                    break;
                }
            }

            input_stream->operations->close (input_stream);
            output_stream->operations->close (output_stream);
        }
        else
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, plugin_system, KAN_LOG_ERROR,
                                 "Unable to find dynamic library \"%s\" at directory \"%s\".", data->name,
                                 system->plugins_directory_path)
        }
    }
}

static inline void delete_hot_reload_directory (struct plugin_system_t *system, kan_instance_size_t directory_id)
{
    struct kan_file_system_path_container_t path_container;
    build_hot_reload_directory_path (&path_container, system->plugins_directory_path, directory_id);

    if (!kan_file_system_remove_directory_with_content (path_container.path))
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, plugin_system, KAN_LOG_ERROR,
                             "Failed to remove hot reload directory \"%s\".", path_container.path)
    }
}

void plugin_system_on_update (kan_context_system_t handle)
{
    struct plugin_system_t *system = KAN_HANDLE_GET (handle);
    KAN_ASSERT (system->enable_hot_reload)
    const struct kan_file_system_watcher_event_t *event;

    while ((event = kan_file_system_watcher_iterator_get (system->watcher, system->watcher_iterator)))
    {
        if (event->entry_type == KAN_FILE_SYSTEM_ENTRY_TYPE_FILE)
        {
            system->hot_reload_after_ns =
                kan_platform_get_elapsed_nanoseconds () + (kan_time_size_t) system->hot_reload_update_delay_ns;
        }

        system->watcher_iterator = kan_file_system_watcher_iterator_advance (system->watcher, system->watcher_iterator);
    }

    if (kan_platform_get_elapsed_nanoseconds () >= system->hot_reload_after_ns)
    {
        const kan_instance_size_t old_directory_id = system->hot_reload_directory_id;
        update_hot_reload_id (system);
        init_hot_reload_directory (system);

        struct kan_dynamic_array_t plugins_copy;
        kan_dynamic_array_init (&plugins_copy, system->plugins.size, sizeof (struct plugin_data_t),
                                _Alignof (struct plugin_data_t), system->plugins.allocation_group);

        for (kan_loop_size_t index = 0u; index < system->plugins.size; ++index)
        {
            struct plugin_data_t *source_data = &((struct plugin_data_t *) system->plugins.data)[index];
            struct plugin_data_t *target_data = &((struct plugin_data_t *) plugins_copy.data)[index];
            target_data->dynamic_library = source_data->dynamic_library;
            source_data->dynamic_library = KAN_HANDLE_SET_INVALID (kan_platform_dynamic_library_t);
            source_data->last_loaded_file_time_stamp_ns = 0u;
        }

        struct kan_file_system_path_container_t path_container;
        build_hot_reload_directory_path (&path_container, system->plugins_directory_path,
                                         system->hot_reload_directory_id);
        load_plugins (path_container.path, &system->plugins,
                      &system->newest_loaded_plugin_last_modification_file_time_ns);

        kan_context_system_t reflection_system =
            kan_context_query (system->context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);

        if (KAN_HANDLE_IS_VALID (reflection_system))
        {
            kan_reflection_system_invalidate (reflection_system);
        }

        unload_plugins (&plugins_copy);
        kan_dynamic_array_shutdown (&plugins_copy);
        delete_hot_reload_directory (system, old_directory_id);
        system->hot_reload_after_ns = KAN_INT_MAX (kan_time_size_t);
    }
}

void plugin_system_connect (kan_context_system_t handle, kan_context_t context)
{
    struct plugin_system_t *system = KAN_HANDLE_GET (handle);
    system->context = context;

    kan_context_system_t reflection_system = kan_context_query (system->context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (reflection_system))
    {
        kan_reflection_system_connect_on_populate (reflection_system, handle, on_reflection_populate);
    }

    if (system->enable_hot_reload)
    {
        kan_context_system_t update_system = kan_context_query (system->context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
        if (KAN_HANDLE_IS_VALID (update_system))
        {
            kan_update_system_connect_on_run (update_system, handle, plugin_system_on_update, 0u, NULL, 0u, NULL);
        }
    }
}

void plugin_system_init (kan_context_system_t handle)
{
    struct plugin_system_t *system = KAN_HANDLE_GET (handle);
    if (system->enable_hot_reload)
    {
        update_hot_reload_id (system);
        init_hot_reload_directory (system);

        system->watcher = kan_file_system_watcher_create (system->plugins_directory_path);
        system->watcher_iterator = kan_file_system_watcher_iterator_create (system->watcher);
    }

    if (system->plugins_directory_path)
    {
        if (system->enable_hot_reload)
        {
            struct kan_file_system_path_container_t path_container;
            build_hot_reload_directory_path (&path_container, system->plugins_directory_path,
                                             system->hot_reload_directory_id);
            load_plugins (path_container.path, &system->plugins,
                          &system->newest_loaded_plugin_last_modification_file_time_ns);
        }
        else
        {
            load_plugins (system->plugins_directory_path, &system->plugins,
                          &system->newest_loaded_plugin_last_modification_file_time_ns);
        }
    }
}

void plugin_system_shutdown (kan_context_system_t handle)
{
    struct plugin_system_t *system = KAN_HANDLE_GET (handle);
    unload_plugins (&system->plugins);

    if (KAN_HANDLE_IS_VALID (system->watcher))
    {
        kan_file_system_watcher_iterator_destroy (system->watcher, system->watcher_iterator);
        kan_file_system_watcher_destroy (system->watcher);
    }

    if (system->enable_hot_reload)
    {
        delete_hot_reload_directory (system, system->hot_reload_directory_id);
    }
}

void plugin_system_disconnect (kan_context_system_t handle)
{
    struct plugin_system_t *system = KAN_HANDLE_GET (handle);
    kan_context_system_t reflection_system = kan_context_query (system->context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (reflection_system))
    {
        kan_reflection_system_disconnect_on_populate (reflection_system, handle);
    }

    if (system->enable_hot_reload)
    {
        kan_context_system_t update_system = kan_context_query (system->context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
        if (KAN_HANDLE_IS_VALID (update_system))
        {
            kan_update_system_disconnect_on_run (update_system, handle);
        }
    }
}

void plugin_system_destroy (kan_context_system_t handle)
{
    struct plugin_system_t *system = KAN_HANDLE_GET (handle);
    kan_dynamic_array_shutdown (&system->plugins);
    kan_free_general (system->group, system, sizeof (struct plugin_system_t));
}

CONTEXT_PLUGIN_SYSTEM_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (plugin_system_t) = {
    .name = "plugin_system_t",
    .create = plugin_system_create,
    .connect = plugin_system_connect,
    .connected_init = plugin_system_init,
    .connected_shutdown = plugin_system_shutdown,
    .disconnect = plugin_system_disconnect,
    .destroy = plugin_system_destroy,
};

void kan_plugin_system_config_init (struct kan_plugin_system_config_t *config)
{
    ensure_statics_initialized ();
    config->plugin_directory_path = NULL;
    config->enable_hot_reload = KAN_FALSE;
    kan_dynamic_array_init (&config->plugins, KAN_PLUGIN_SYSTEM_PLUGINS_INITIAL_SIZE, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), config_allocation_group);
    config->hot_reload_update_delay_ns = 300000000u;
}

kan_allocation_group_t kan_plugin_system_config_get_allocation_group (void)
{
    ensure_statics_initialized ();
    return config_allocation_group;
}

void kan_plugin_system_config_shutdown (struct kan_plugin_system_config_t *config)
{
    kan_dynamic_array_shutdown (&config->plugins);
}

kan_time_size_t kan_plugin_system_get_newest_loaded_plugin_last_modification_file_time_ns (
    kan_context_system_t plugin_system)
{
    struct plugin_system_t *system = KAN_HANDLE_GET (plugin_system);
    return system->newest_loaded_plugin_last_modification_file_time_ns;
}
