#include <kan/context/virtual_file_system.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/threading/conditional_variable.h>
#include <kan/threading/mutex.h>

KAN_LOG_DEFINE_CATEGORY (context_virtual_file_system);

struct virtual_file_system_t
{
    kan_context_handle_t context;
    kan_allocation_group_t group;

    kan_virtual_file_system_volume_t volume;
    kan_mutex_handle_t access_management_mutex;
    kan_conditional_variable_handle_t access_management_neutral_condition;
    int64_t access_management_counter;
};

static inline kan_bool_t ensure_mount_path_exists (kan_virtual_file_system_volume_t volume, const char *path)
{
    struct kan_file_system_path_container_t path_container;
    const char *last_separator = strrchr (path, '/');

    if (!last_separator || last_separator == path)
    {
        // No separator, therefore at the root and always exists.
        return KAN_TRUE;
    }

    kan_file_system_path_container_copy_char_sequence (&path_container, path, last_separator);
    struct kan_virtual_file_system_entry_status_t entry_status;

    if (kan_virtual_file_system_query_entry (volume, path_container.path, &entry_status))
    {
        return entry_status.type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY;
    }

    return kan_virtual_file_system_make_directory (volume, path_container.path);
}

kan_context_system_handle_t virtual_file_system_create (kan_allocation_group_t group, void *user_config)
{
    struct virtual_file_system_t *system =
        kan_allocate_general (group, sizeof (struct virtual_file_system_t), _Alignof (struct virtual_file_system_t));

    system->group = group;
    system->volume = kan_virtual_file_system_volume_create ();
    system->access_management_mutex = kan_mutex_create ();
    system->access_management_neutral_condition = kan_conditional_variable_create ();
    system->access_management_counter = 0;

    if (user_config)
    {
        struct kan_virtual_file_system_config_t *config = (struct kan_virtual_file_system_config_t *) user_config;
        struct kan_virtual_file_system_config_mount_real_t *mount_point_real = config->first_mount_real;

        while (mount_point_real)
        {
            if (ensure_mount_path_exists (system->volume, mount_point_real->mount_path))
            {
                if (!kan_virtual_file_system_volume_mount_real (system->volume, mount_point_real->mount_path,
                                                                mount_point_real->real_path))
                {
                    KAN_LOG (context_virtual_file_system, KAN_LOG_ERROR, "Failed to mount \"%s\" at \"%s\".",
                             mount_point_real->real_path, mount_point_real->mount_path)
                }
            }
            else
            {
                KAN_LOG (context_virtual_file_system, KAN_LOG_ERROR,
                         "Failed to ensure existence of parent for mount path \"%s\".", mount_point_real->mount_path)
            }

            mount_point_real = mount_point_real->next;
        }

        struct kan_virtual_file_system_config_mount_read_only_pack_t *mount_point_read_only_pack =
            config->first_mount_read_only_pack;

        while (mount_point_read_only_pack)
        {
            if (ensure_mount_path_exists (system->volume, mount_point_read_only_pack->mount_path))
            {
                if (!kan_virtual_file_system_volume_mount_read_only_pack (system->volume,
                                                                          mount_point_read_only_pack->mount_path,
                                                                          mount_point_read_only_pack->pack_real_path))
                {
                    KAN_LOG (context_virtual_file_system, KAN_LOG_ERROR, "Failed to mount \"%s\" at \"%s\".",
                             mount_point_read_only_pack->pack_real_path, mount_point_read_only_pack->mount_path)
                }
            }
            else
            {
                KAN_LOG (context_virtual_file_system, KAN_LOG_ERROR,
                         "Failed to ensure existence of parent for mount path \"%s\".",
                         mount_point_read_only_pack->mount_path)
            }

            mount_point_read_only_pack = mount_point_read_only_pack->next;
        }
    }

    return (kan_context_system_handle_t) system;
}

void virtual_file_system_connect (kan_context_system_handle_t handle, kan_context_handle_t context)
{
    struct virtual_file_system_t *system = (struct virtual_file_system_t *) handle;
    system->context = context;
}

void virtual_file_system_init (kan_context_system_handle_t handle)
{
}

void virtual_file_system_shutdown (kan_context_system_handle_t handle)
{
}

void virtual_file_system_disconnect (kan_context_system_handle_t handle)
{
}

void virtual_file_system_destroy (kan_context_system_handle_t handle)
{
    struct virtual_file_system_t *system = (struct virtual_file_system_t *) handle;
    kan_conditional_variable_destroy (system->access_management_neutral_condition);
    kan_mutex_destroy (system->access_management_mutex);
    kan_virtual_file_system_volume_destroy (system->volume);
    kan_free_general (system->group, system, sizeof (struct virtual_file_system_t));
}

struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (virtual_file_system_t) = {
    .name = "virtual_file_system_t",
    .create = virtual_file_system_create,
    .connect = virtual_file_system_connect,
    .connected_init = virtual_file_system_init,
    .connected_shutdown = virtual_file_system_shutdown,
    .disconnect = virtual_file_system_disconnect,
    .destroy = virtual_file_system_destroy,
};

kan_virtual_file_system_volume_t kan_virtual_file_system_get_context_volume_for_read (
    kan_context_system_handle_t virtual_file_system)
{
    struct virtual_file_system_t *system = (struct virtual_file_system_t *) virtual_file_system;
    kan_mutex_lock (system->access_management_mutex);

    // Wait until there is no writers.
    while (system->access_management_counter < 0)
    {
        kan_conditional_variable_wait (system->access_management_neutral_condition, system->access_management_mutex);
    }

    ++system->access_management_counter;
    kan_mutex_unlock (system->access_management_mutex);
    return system->volume;
}

void kan_virtual_file_system_close_context_read_access (kan_context_system_handle_t virtual_file_system)
{
    struct virtual_file_system_t *system = (struct virtual_file_system_t *) virtual_file_system;
    kan_mutex_lock (system->access_management_mutex);
    KAN_ASSERT (system->access_management_counter > 0)
    const int64_t previous_value = --system->access_management_counter;
    kan_mutex_unlock (system->access_management_mutex);

    if (previous_value == 1)
    {
        kan_conditional_variable_signal_all (system->access_management_neutral_condition);
    }
}

kan_virtual_file_system_volume_t kan_virtual_file_system_get_context_volume_for_write (
    kan_context_system_handle_t virtual_file_system)
{
    struct virtual_file_system_t *system = (struct virtual_file_system_t *) virtual_file_system;
    kan_mutex_lock (system->access_management_mutex);

    // Wait until neutral situation -- no readers and no writers.
    while (system->access_management_counter != 0)
    {
        kan_conditional_variable_wait (system->access_management_neutral_condition, system->access_management_mutex);
    }

    --system->access_management_counter;
    kan_mutex_unlock (system->access_management_mutex);
    return system->volume;
}

void kan_virtual_file_system_close_context_write_access (kan_context_system_handle_t virtual_file_system)
{
    struct virtual_file_system_t *system = (struct virtual_file_system_t *) virtual_file_system;
    kan_mutex_lock (system->access_management_mutex);
    KAN_ASSERT (system->access_management_counter == -1)
    ++system->access_management_counter;
    kan_mutex_unlock (system->access_management_mutex);
    kan_conditional_variable_signal_all (system->access_management_neutral_condition);
}
