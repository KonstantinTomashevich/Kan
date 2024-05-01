#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <kan/container/interned_string.h>
#include <kan/context/plugin_system.h>
#include <kan/context/reflection_system.h>
#include <kan/error/critical.h>
#include <kan/file_system/entry.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/platform/dynamic_library.h>

KAN_LOG_DEFINE_CATEGORY (plugin_system);

struct plugin_data_t
{
    kan_interned_string_t name;
    kan_platform_dynamic_library_t dynamic_library;
};

struct plugin_system_t
{
    kan_context_handle_t context;
    kan_allocation_group_t group;

    char *plugins_directory_path;

    /// \meta reflection_dynamic_array_type = "struct plugin_data_t"
    struct kan_dynamic_array_t plugins;
};

kan_context_system_handle_t plugin_system_create (kan_allocation_group_t group, void *user_config)
{
    struct plugin_system_t *system =
        kan_allocate_general (group, sizeof (struct plugin_system_t), _Alignof (struct plugin_system_t));
    system->group = group;

    if (user_config)
    {
        struct kan_plugin_system_config_t *config = user_config;
        KAN_ASSERT (config->plugin_directory_path)
        const uint64_t path_length = strlen (config->plugin_directory_path);
        system->plugins_directory_path = kan_allocate_general (group, path_length + 1u, _Alignof (char));
        memcpy (system->plugins_directory_path, config->plugin_directory_path, path_length + 1u);

        kan_dynamic_array_init (&system->plugins, config->plugins.size, sizeof (struct plugin_data_t),
                                _Alignof (struct plugin_data_t), group);

        for (uint64_t index = 0u; index < config->plugins.size; ++index)
        {
            kan_interned_string_t plugin_name = ((kan_interned_string_t *) config->plugins.data)[index];
            struct plugin_data_t *data = kan_dynamic_array_add_last (&system->plugins);
            KAN_ASSERT (data)
            data->name = plugin_name;
            data->dynamic_library = KAN_INVALID_PLATFORM_DYNAMIC_LIBRARY;
        }
    }
    else
    {
        system->plugins_directory_path = NULL;
        kan_dynamic_array_init (&system->plugins, 0u, sizeof (struct plugin_data_t), _Alignof (struct plugin_data_t),
                                group);
    }

    return (kan_context_system_handle_t) system;
}

static inline void load_plugins (const char *path, struct kan_dynamic_array_t *array)
{
    for (uint64_t index = 0u; index < array->size; ++index)
    {
        struct plugin_data_t *data = &((struct plugin_data_t *) array->data)[index];
        KAN_ASSERT (data->dynamic_library == KAN_INVALID_PLATFORM_DYNAMIC_LIBRARY)

        char library_path_buffer[KAN_FILE_SYSTEM_MAX_PATH_LENGTH];
        snprintf (library_path_buffer, KAN_FILE_SYSTEM_MAX_PATH_LENGTH, "%s/%s.dll", path, data->name);

        if (kan_file_system_check_existence (library_path_buffer))
        {
            data->dynamic_library = kan_platform_dynamic_library_load (library_path_buffer);
            if (data->dynamic_library == KAN_INVALID_PLATFORM_DYNAMIC_LIBRARY)
            {
                KAN_LOG (plugin_system, KAN_LOG_ERROR, "Failed to load dynamic library from \"%s\".",
                         library_path_buffer)
            }

            continue;
        }

        snprintf (library_path_buffer, KAN_FILE_SYSTEM_MAX_PATH_LENGTH, "%s/%s.so", path, data->name);
        if (kan_file_system_check_existence (library_path_buffer))
        {
            data->dynamic_library = kan_platform_dynamic_library_load (library_path_buffer);
            if (data->dynamic_library == KAN_INVALID_PLATFORM_DYNAMIC_LIBRARY)
            {
                KAN_LOG (plugin_system, KAN_LOG_ERROR, "Failed to load dynamic library from \"%s\".",
                         library_path_buffer)
            }

            continue;
        }

        snprintf (library_path_buffer, KAN_FILE_SYSTEM_MAX_PATH_LENGTH, "%s/lib%s.so", path, data->name);
        if (kan_file_system_check_existence (library_path_buffer))
        {
            data->dynamic_library = kan_platform_dynamic_library_load (library_path_buffer);
            if (data->dynamic_library == KAN_INVALID_PLATFORM_DYNAMIC_LIBRARY)
            {
                KAN_LOG (plugin_system, KAN_LOG_ERROR, "Failed to load dynamic library from \"%s\".",
                         library_path_buffer)
            }

            continue;
        }

        snprintf (library_path_buffer, KAN_FILE_SYSTEM_MAX_PATH_LENGTH, "%s/lib%s.so.0", path, data->name);
        if (kan_file_system_check_existence (library_path_buffer))
        {
            data->dynamic_library = kan_platform_dynamic_library_load (library_path_buffer);
            if (data->dynamic_library == KAN_INVALID_PLATFORM_DYNAMIC_LIBRARY)
            {
                KAN_LOG (plugin_system, KAN_LOG_ERROR, "Failed to load dynamic library from \"%s\".",
                         library_path_buffer)
            }

            continue;
        }

        KAN_LOG (plugin_system, KAN_LOG_ERROR, "Unable to find dynamic library \"%s\" at directory \"%s\".", data->name,
                 path)
    }
}

static inline void unload_plugins (struct kan_dynamic_array_t *array)
{
    for (uint64_t index = 0u; index < array->size; ++index)
    {
        struct plugin_data_t *data = &((struct plugin_data_t *) array->data)[index];
        if (data->dynamic_library != KAN_INVALID_PLATFORM_DYNAMIC_LIBRARY)
        {
            kan_platform_dynamic_library_unload (data->dynamic_library);
        }
    }
}

static void on_reflection_populate (kan_context_system_handle_t other_system, kan_reflection_registry_t registry)
{
    struct plugin_system_t *system = (struct plugin_system_t *) other_system;
    for (uint64_t index = 0u; index < system->plugins.size; ++index)
    {
        struct plugin_data_t *data = &((struct plugin_data_t *) system->plugins.data)[index];
        if (data->dynamic_library != KAN_INVALID_PLATFORM_DYNAMIC_LIBRARY)
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

void plugin_system_connect (kan_context_system_handle_t handle, kan_context_handle_t context)
{
    struct plugin_system_t *system = (struct plugin_system_t *) handle;
    system->context = context;

    kan_context_system_handle_t reflection_system =
        kan_context_query (system->context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);

    if (reflection_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        kan_reflection_system_connect_on_populate (reflection_system, handle, on_reflection_populate);
    }
}

void plugin_system_init (kan_context_system_handle_t handle)
{
    struct plugin_system_t *system = (struct plugin_system_t *) handle;
    if (system->plugins_directory_path)
    {
        load_plugins (system->plugins_directory_path, &system->plugins);
    }
}

void plugin_system_shutdown (kan_context_system_handle_t handle)
{
    struct plugin_system_t *system = (struct plugin_system_t *) handle;
    unload_plugins (&system->plugins);
}

void plugin_system_disconnect (kan_context_system_handle_t handle)
{
    struct plugin_system_t *system = (struct plugin_system_t *) handle;
    kan_context_system_handle_t reflection_system =
        kan_context_query (system->context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);

    if (reflection_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        kan_reflection_system_disconnect_on_populate (reflection_system, handle);
    }
}

void plugin_system_destroy (kan_context_system_handle_t handle)
{
    struct plugin_system_t *system = (struct plugin_system_t *) handle;
    if (system->plugins_directory_path)
    {
        kan_free_general (system->group, system->plugins_directory_path, strlen (system->plugins_directory_path) + 1u);
    }

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

void kan_plugin_system_config_init (struct kan_plugin_system_config_t *config)
{
    ensure_statics_initialized ();
    config->plugin_directory_path = NULL;
    kan_dynamic_array_init (&config->plugins, KAN_PLUGIN_SYSTEM_PLUGINS_INITIAL_SIZE, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), config_allocation_group);
}

kan_allocation_group_t kan_plugin_system_config_get_allocation_group (void)
{
    ensure_statics_initialized ();
    return config_allocation_group;
}

void kan_plugin_system_config_shutdown (struct kan_plugin_system_config_t *config)
{
    if (config->plugin_directory_path)
    {
        kan_free_general (config_allocation_group, config->plugin_directory_path,
                          strlen (config->plugin_directory_path) + 1u);
    }

    kan_dynamic_array_shutdown (&config->plugins);
}
