#include <kan/context/all_system_names.h>
#include <kan/context/hot_reload_coordination_system.h>
#include <kan/context/reflection_system.h>
#include <kan/context/resource_pipeline_system.h>
#include <kan/context/update_system.h>
#include <kan/error/critical.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/path_container.h>
#include <kan/file_system/stream.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/precise_time/precise_time.h>
#include <kan/serialization/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (resource_pipeline_system);

struct platform_configuration_t
{
    const struct kan_reflection_struct_t *type;
    void *data;
};

struct platform_configuration_change_listener_t
{
    struct kan_bd_list_node_t node;
    kan_bool_t has_unconsumed_change;
};

struct resource_pipeline_system_t
{
    kan_context_t context;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct platform_configuration_t)
    struct kan_dynamic_array_t platform_configurations;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (char *)
    struct kan_dynamic_array_t platform_configuration_paths;

    kan_time_size_t latest_platform_configuration_modification_time_ns;
    kan_time_size_t reload_platform_configuration_after_ns;
    kan_time_size_t last_platform_configuration_reload_time_ns;

    KAN_REFLECTION_IGNORE
    struct kan_atomic_int_t platform_configuration_change_listeners_lock;

    KAN_REFLECTION_IGNORE
    struct kan_bd_list_t platform_configuration_change_listeners;

    kan_reflection_registry_t last_reflection_registry;

    kan_interned_string_t platform_configuration_path;
    kan_bool_t enable_runtime_compilation;
    kan_bool_t build_reference_type_info_storage;

    struct kan_resource_reference_type_info_storage_t reference_type_info_storage;

    kan_allocation_group_t group;
    kan_allocation_group_t platform_configuration_group;
    kan_allocation_group_t reference_type_info_storage_group;
    kan_allocation_group_t listeners_group;

    kan_interned_string_t interned_kan_resource_platform_configuration_t;
};

kan_context_system_t resource_pipeline_system_create (kan_allocation_group_t group, void *user_config)
{
    struct resource_pipeline_system_t *system = kan_allocate_general (group, sizeof (struct resource_pipeline_system_t),
                                                                      _Alignof (struct resource_pipeline_system_t));
    system->group = group;
    system->platform_configuration_group = kan_allocation_group_get_child (group, "platform_configuration");
    system->reference_type_info_storage_group = kan_allocation_group_get_child (group, "reference_type_info_storage");
    system->listeners_group = kan_allocation_group_get_child (group, "listeners");
    system->interned_kan_resource_platform_configuration_t =
        kan_string_intern ("kan_resource_platform_configuration_t");

    struct kan_resource_pipeline_system_config_t *config = user_config;
    static struct kan_resource_pipeline_system_config_t default_config;

    if (!config)
    {
        kan_resource_pipeline_system_config_init (&default_config);
        config = &default_config;
    }

    system->platform_configuration_path = config->platform_configuration_path;
    system->enable_runtime_compilation = config->enable_runtime_compilation;
    system->build_reference_type_info_storage = config->build_reference_type_info_storage;

    kan_dynamic_array_init (&system->platform_configurations, KAN_RESOURCE_PIPELINE_SYSTEM_PC_INITIAL_SIZE,
                            sizeof (struct platform_configuration_t), _Alignof (struct platform_configuration_t),
                            system->platform_configuration_group);

    kan_dynamic_array_init (&system->platform_configuration_paths, 0u, sizeof (char *), _Alignof (char *),
                            system->platform_configuration_group);

    system->latest_platform_configuration_modification_time_ns = 0u;
    system->reload_platform_configuration_after_ns = KAN_INT_MAX (kan_time_size_t);

    system->platform_configuration_change_listeners_lock = kan_atomic_int_init (0);
    kan_bd_list_init (&system->platform_configuration_change_listeners);
    return KAN_HANDLE_SET (kan_context_system_t, system);
}

static void resource_pipeline_system_on_reflection_generated (kan_context_system_t other_system,
                                                              kan_reflection_registry_t registry,
                                                              kan_reflection_migration_seed_t migration_seed,
                                                              kan_reflection_struct_migrator_t migrator);

static void resource_pipeline_system_on_reflection_pre_shutdown (kan_context_system_t other_system);

static void resource_pipeline_system_on_reflection_update (kan_context_system_t other_system);

void resource_pipeline_system_connect (kan_context_system_t handle, kan_context_t context)
{
    struct resource_pipeline_system_t *system = KAN_HANDLE_GET (handle);
    system->context = context;
    kan_context_system_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (reflection_system))
    {
        kan_reflection_system_connect_on_generated (reflection_system, handle,
                                                    resource_pipeline_system_on_reflection_generated);
        kan_reflection_system_connect_on_pre_shutdown (reflection_system, handle,
                                                       resource_pipeline_system_on_reflection_pre_shutdown);
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

            kan_update_system_connect_on_run (update_system, handle, resource_pipeline_system_on_reflection_update, 1u,
                                              &hot_reload_system, 1u, &universe_system);
        }
    }
}

void resource_pipeline_system_init (kan_context_system_t handle)
{
}

static void resource_pipeline_system_reset_platform_configuration (struct resource_pipeline_system_t *system)
{
    for (kan_loop_size_t index = 0u; index < system->platform_configurations.size; ++index)
    {
        struct platform_configuration_t *configuration =
            &((struct platform_configuration_t *) system->platform_configurations.data)[index];

        if (configuration->type->shutdown)
        {
            configuration->type->shutdown (configuration->type->functor_user_data, configuration->data);
        }

        kan_free_general (system->platform_configuration_group, configuration->data, configuration->type->size);
    }

    for (kan_loop_size_t index = 0u; index < system->platform_configuration_paths.size; ++index)
    {
        char *path = ((char **) system->platform_configuration_paths.data)[index];
        kan_free_general (system->platform_configuration_group, path, strlen (path) + 1u);
    }

    system->platform_configurations.size = 0u;
    system->platform_configuration_paths.size = 0u;
}

static void resource_pipeline_system_load_platform_configuration_recursive (
    struct resource_pipeline_system_t *system, struct kan_file_system_path_container_t *path)
{
    const struct kan_reflection_struct_t *file_type = kan_reflection_registry_query_struct (
        system->last_reflection_registry, system->interned_kan_resource_platform_configuration_t);
    KAN_ASSERT (file_type)
    struct kan_resource_platform_configuration_t *loaded_configuration = NULL;

    struct kan_stream_t *input_stream = kan_direct_file_stream_open_for_read (path->path, KAN_TRUE);
    if (!input_stream)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_system, KAN_LOG_ERROR,
                             "Failed to open platform configuration at path \"%s\".", path->path)
        return;
    }

    input_stream = kan_random_access_stream_buffer_open_for_read (input_stream, KAN_RESOURCE_PIPELINE_SYSTEM_IO_BUFFER);
    kan_interned_string_t type_name;

    if (!kan_serialization_rd_read_type_header (input_stream, &type_name))
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_system, KAN_LOG_ERROR,
                             "Failed to read type header of \"%s\".", path->path)
        input_stream->operations->close (input_stream);
        return;
    }
    else if (type_name != file_type->name)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_system, KAN_LOG_ERROR,
                             "Expected type \"%s\" at \"%s\", but got \"%s\".", file_type->name, path->path, type_name)
        input_stream->operations->close (input_stream);
        return;
    }
    else
    {
        loaded_configuration = kan_allocate_general (system->platform_configuration_group,
                                                     sizeof (struct kan_resource_platform_configuration_t),
                                                     _Alignof (struct kan_resource_platform_configuration_t));
        kan_resource_platform_configuration_init (loaded_configuration);

        kan_serialization_rd_reader_t reader =
            kan_serialization_rd_reader_create (input_stream, loaded_configuration, file_type->name,
                                                system->last_reflection_registry, system->platform_configuration_group);

        enum kan_serialization_state_t serialization_state;
        while ((serialization_state = kan_serialization_rd_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
        {
        }

        kan_serialization_rd_reader_destroy (reader);
        input_stream->operations->close (input_stream);

        if (serialization_state == KAN_SERIALIZATION_FAILED)
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_system, KAN_LOG_ERROR,
                                 "Failed to deserialize platform configuration from \"%s\".", path->path)
            kan_resource_platform_configuration_shutdown (loaded_configuration);
            kan_free_general (system->platform_configuration_group, loaded_configuration,
                              sizeof (struct kan_resource_platform_configuration_t));
            return;
        }
        else
        {
            KAN_ASSERT (serialization_state == KAN_SERIALIZATION_FINISHED)
        }
    }

    kan_context_system_t hot_reload_system =
        kan_context_query (system->context, KAN_CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (hot_reload_system) &&
        kan_hot_reload_coordination_system_get_current_mode (hot_reload_system) != KAN_HOT_RELOAD_MODE_DISABLED)
    {
        char **spot = kan_dynamic_array_add_last (&system->platform_configuration_paths);
        if (!spot)
        {
            kan_dynamic_array_set_capacity (&system->platform_configuration_paths,
                                            KAN_MAX (1u, system->platform_configuration_paths.size * 2u));
            spot = kan_dynamic_array_add_last (&system->platform_configuration_paths);
        }

        *spot = kan_allocate_general (system->platform_configuration_group, path->length + 1u, _Alignof (char));
        memcpy (*spot, path->path, path->length + 1u);
    }

    struct kan_file_system_entry_status_t status;
    if (kan_file_system_query_entry (path->path, &status))
    {
        system->latest_platform_configuration_modification_time_ns =
            KAN_MAX (system->latest_platform_configuration_modification_time_ns, status.last_modification_time_ns);
    }
    else
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_system, KAN_LOG_ERROR,
                             "Failed to query status of platform configuration file \"%s\".", path->path)
    }

    if (loaded_configuration->parent)
    {
        // Step back to the directory level.
        const char *last_separator = strrchr (path->path, '/');

        if (last_separator)
        {
            kan_file_system_path_container_reset_length (path, (kan_instance_size_t) (last_separator - path->path));
        }
        else
        {
            kan_file_system_path_container_reset_length (path, 0u);
        }

        kan_file_system_path_container_append (path, loaded_configuration->parent);
        resource_pipeline_system_load_platform_configuration_recursive (system, path);
    }

    for (kan_loop_size_t patch_index = 0u; patch_index < loaded_configuration->configuration.size; ++patch_index)
    {
        kan_reflection_patch_t patch =
            ((kan_reflection_patch_t *) loaded_configuration->configuration.data)[patch_index];
        const struct kan_reflection_struct_t *patch_type = kan_reflection_patch_get_type (patch);

        if (!patch_type)
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_system, KAN_LOG_ERROR,
                                 "Platform configuration at \"%s\" contains patches with unknown or broken types.",
                                 path->path)
            break;
        }

        struct platform_configuration_t *configuration_instance = NULL;
        for (kan_loop_size_t configuration_index = 0u; configuration_index < system->platform_configurations.size;
             ++configuration_index)
        {
            struct platform_configuration_t *existent_configuration =
                &((struct platform_configuration_t *) system->platform_configurations.data)[patch_index];

            if (existent_configuration->type == patch_type)
            {
                configuration_instance = existent_configuration;
                break;
            }
        }

        if (!configuration_instance)
        {
            configuration_instance = kan_dynamic_array_add_last (&system->platform_configurations);
            if (!configuration_instance)
            {
                kan_dynamic_array_set_capacity (&system->platform_configurations,
                                                system->platform_configurations.size * 2u);
                configuration_instance = kan_dynamic_array_add_last (&system->platform_configurations);
            }

            configuration_instance->type = patch_type;
            configuration_instance->data =
                kan_allocate_general (system->platform_configuration_group, patch_type->size, patch_type->alignment);

            if (patch_type->init)
            {
                patch_type->init (patch_type->functor_user_data, configuration_instance->data);
            }
        }

        kan_reflection_patch_apply (patch, configuration_instance->data);
    }

    kan_resource_platform_configuration_shutdown (loaded_configuration);
    kan_free_general (system->platform_configuration_group, loaded_configuration,
                      sizeof (struct kan_resource_platform_configuration_t));
}

static void resource_pipeline_system_load_platform_configuration (struct resource_pipeline_system_t *system)
{
    if (!system->platform_configuration_path)
    {
        KAN_LOG (resource_pipeline_system, KAN_LOG_INFO,
                 "Unable to load platform configuration as no path is provided.")
        return;
    }

    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_string (&path_container, system->platform_configuration_path);
    resource_pipeline_system_load_platform_configuration_recursive (system, &path_container);

    kan_atomic_int_lock (&system->platform_configuration_change_listeners_lock);
    struct platform_configuration_change_listener_t *listener =
        (struct platform_configuration_change_listener_t *) system->platform_configuration_change_listeners.first;

    while (listener)
    {
        listener->has_unconsumed_change = KAN_TRUE;
        listener = (struct platform_configuration_change_listener_t *) listener->node.next;
    }

    kan_atomic_int_unlock (&system->platform_configuration_change_listeners_lock);
}

static void resource_pipeline_system_on_reflection_generated (kan_context_system_t other_system,
                                                              kan_reflection_registry_t registry,
                                                              kan_reflection_migration_seed_t migration_seed,
                                                              kan_reflection_struct_migrator_t migrator)
{
    struct resource_pipeline_system_t *system = KAN_HANDLE_GET (other_system);
    system->last_reflection_registry = registry;
    const kan_bool_t first_generation = !KAN_HANDLE_IS_VALID (migrator);

    if (!first_generation)
    {
        resource_pipeline_system_reset_platform_configuration (system);
        if (system->build_reference_type_info_storage)
        {
            kan_resource_reference_type_info_storage_shutdown (&system->reference_type_info_storage);
        }
    }

    resource_pipeline_system_load_platform_configuration (system);
    if (system->build_reference_type_info_storage)
    {
        kan_resource_reference_type_info_storage_build (&system->reference_type_info_storage, registry,
                                                        system->reference_type_info_storage_group);
    }
}

static void resource_pipeline_system_on_reflection_pre_shutdown (kan_context_system_t other_system)
{
    struct resource_pipeline_system_t *system = KAN_HANDLE_GET (other_system);
    resource_pipeline_system_reset_platform_configuration (system);
    kan_dynamic_array_shutdown (&system->platform_configurations);
    kan_dynamic_array_shutdown (&system->platform_configuration_paths);

    if (system->build_reference_type_info_storage)
    {
        kan_resource_reference_type_info_storage_shutdown (&system->reference_type_info_storage);
    }
}

static void resource_pipeline_system_on_reflection_update (kan_context_system_t other_system)
{
    struct resource_pipeline_system_t *system = KAN_HANDLE_GET (other_system);
    kan_context_system_t hot_reload_system =
        kan_context_query (system->context, KAN_CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_NAME);
    KAN_ASSERT (KAN_HANDLE_IS_VALID (hot_reload_system))

    struct kan_hot_reload_automatic_config_t *automatic_config =
        kan_hot_reload_coordination_system_get_automatic_config (hot_reload_system);

    for (kan_loop_size_t index = 0u; index < system->platform_configuration_paths.size; ++index)
    {
        char *path = ((char **) system->platform_configuration_paths.data)[index];
        struct kan_file_system_entry_status_t status;

        if (kan_file_system_query_entry (path, &status))
        {
            if (status.last_modification_time_ns > system->latest_platform_configuration_modification_time_ns)
            {
                system->reload_platform_configuration_after_ns =
                    kan_precise_time_get_elapsed_nanoseconds () + automatic_config->change_wait_time_ns;
                system->latest_platform_configuration_modification_time_ns = status.last_modification_time_ns;
            }
        }
        else
        {
            KAN_LOG (resource_pipeline_system, KAN_LOG_ERROR,
                     "Failed to query status of platform configuration file \"%s\".", path)
        }
    }

    kan_bool_t do_hot_reload = KAN_FALSE;
    switch (kan_hot_reload_coordination_system_get_current_mode (hot_reload_system))
    {
    case KAN_HOT_RELOAD_MODE_DISABLED:
        KAN_ASSERT (KAN_FALSE)
        break;

    case KAN_HOT_RELOAD_MODE_AUTOMATIC_INDEPENDENT:
        do_hot_reload = kan_precise_time_get_elapsed_nanoseconds () >= system->reload_platform_configuration_after_ns;
        break;

    case KAN_HOT_RELOAD_MODE_ON_REQUEST:
        do_hot_reload =
            kan_hot_reload_coordination_system_is_hot_swap (hot_reload_system) &&
            system->last_platform_configuration_reload_time_ns < system->reload_platform_configuration_after_ns;
        break;
    }

    if (do_hot_reload)
    {
        system->last_platform_configuration_reload_time_ns = system->reload_platform_configuration_after_ns;
        resource_pipeline_system_reset_platform_configuration (system);
        resource_pipeline_system_load_platform_configuration (system);
    }
}

void resource_pipeline_system_shutdown (kan_context_system_t handle)
{
}

void resource_pipeline_system_disconnect (kan_context_system_t handle)
{
    struct resource_pipeline_system_t *system = KAN_HANDLE_GET (handle);
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

void resource_pipeline_system_destroy (kan_context_system_t handle)
{
    struct resource_pipeline_system_t *system = KAN_HANDLE_GET (handle);
    kan_atomic_int_lock (&system->platform_configuration_change_listeners_lock);

    struct platform_configuration_change_listener_t *listener =
        (struct platform_configuration_change_listener_t *) system->platform_configuration_change_listeners.first;

    while (listener)
    {
        struct platform_configuration_change_listener_t *next =
            (struct platform_configuration_change_listener_t *) listener->node.next;

        kan_free_batched (system->listeners_group, listener);
        listener = next;
    }

    kan_atomic_int_unlock (&system->platform_configuration_change_listeners_lock);
    kan_free_general (system->group, system, sizeof (struct resource_pipeline_system_t));
}

CONTEXT_RESOURCE_PIPELINE_SYSTEM_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (
    resource_pipeline_system_t) = {
    .name = KAN_CONTEXT_RESOURCE_PIPELINE_SYSTEM_NAME,
    .create = resource_pipeline_system_create,
    .connect = resource_pipeline_system_connect,
    .connected_init = resource_pipeline_system_init,
    .connected_shutdown = resource_pipeline_system_shutdown,
    .disconnect = resource_pipeline_system_disconnect,
    .destroy = resource_pipeline_system_destroy,
};

void kan_resource_pipeline_system_config_init (struct kan_resource_pipeline_system_config_t *instance)
{
    instance->platform_configuration_path = NULL;
    instance->enable_runtime_compilation = KAN_FALSE;
    instance->build_reference_type_info_storage = KAN_FALSE;
}

kan_time_size_t kan_resource_pipeline_system_get_platform_configuration_file_time_ns (kan_context_system_t system)
{
    struct resource_pipeline_system_t *data = KAN_HANDLE_GET (system);
    return data->latest_platform_configuration_modification_time_ns;
}

const void *kan_resource_pipeline_system_query_platform_configuration (kan_context_system_t system,
                                                                       kan_interned_string_t configuration_type_name)
{
    struct resource_pipeline_system_t *data = KAN_HANDLE_GET (system);
    for (kan_loop_size_t index = 0u; index < data->platform_configurations.size; ++index)
    {
        struct platform_configuration_t *configuration =
            &((struct platform_configuration_t *) data->platform_configurations.data)[index];

        if (configuration->type->name == configuration_type_name)
        {
            return configuration->data;
        }
    }

    return NULL;
}

kan_resource_pipeline_system_platform_configuration_listener
kan_resource_pipeline_system_add_platform_configuration_change_listener (kan_context_system_t system)
{
    struct resource_pipeline_system_t *data = KAN_HANDLE_GET (system);
    struct platform_configuration_change_listener_t *listener =
        kan_allocate_batched (data->listeners_group, sizeof (struct platform_configuration_change_listener_t));

    listener->has_unconsumed_change = KAN_FALSE;
    kan_atomic_int_lock (&data->platform_configuration_change_listeners_lock);
    kan_bd_list_add (&data->platform_configuration_change_listeners, NULL, &listener->node);
    kan_atomic_int_unlock (&data->platform_configuration_change_listeners_lock);
    return KAN_HANDLE_SET (kan_resource_pipeline_system_platform_configuration_listener, listener);
}

kan_bool_t kan_resource_pipeline_system_platform_configuration_listener_consume (
    kan_resource_pipeline_system_platform_configuration_listener listener)
{
    struct platform_configuration_change_listener_t *data = KAN_HANDLE_GET (listener);
    if (data->has_unconsumed_change)
    {
        data->has_unconsumed_change = KAN_FALSE;
        return KAN_TRUE;
    }

    return KAN_FALSE;
}

void kan_resource_pipeline_system_remove_platform_configuration_change_listener (
    kan_context_system_t system, kan_resource_pipeline_system_platform_configuration_listener listener)
{
    struct resource_pipeline_system_t *data = KAN_HANDLE_GET (system);
    struct platform_configuration_change_listener_t *listener_data = KAN_HANDLE_GET (listener);
    kan_atomic_int_lock (&data->platform_configuration_change_listeners_lock);
    kan_bd_list_remove (&data->platform_configuration_change_listeners, &listener_data->node);
    kan_atomic_int_unlock (&data->platform_configuration_change_listeners_lock);
    kan_free_batched (data->listeners_group, listener_data);
}

kan_bool_t kan_resource_pipeline_system_is_runtime_compilation_enabled (kan_context_system_t system)
{
    struct resource_pipeline_system_t *data = KAN_HANDLE_GET (system);
    return data->enable_runtime_compilation;
}

struct kan_resource_reference_type_info_storage_t *kan_resource_pipeline_system_get_reference_type_info_storage (
    kan_context_system_t system)
{
    struct resource_pipeline_system_t *data = KAN_HANDLE_GET (system);
    if (data->build_reference_type_info_storage)
    {
        return &data->reference_type_info_storage;
    }
    else
    {
        return NULL;
    }
}
