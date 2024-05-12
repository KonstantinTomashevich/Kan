#include <string.h>

#include <kan/api_common/min_max.h>
#include <kan/application_framework_tool/application_framework_tool.h>
#include <kan/context/plugin_system.h>
#include <kan/context/reflection_system.h>
#include <kan/error/critical.h>
#include <kan/file_system/path_container.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>

KAN_LOG_DEFINE_CATEGORY (application_framework_tool);

kan_context_handle_t kan_application_framework_tool_create_context (uint64_t arguments_count, char **arguments)
{
    const kan_allocation_group_t context_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "tool_context");
    kan_context_handle_t context = kan_context_create (context_group);

    const kan_bool_t has_plugins =
        kan_application_framework_tool_plugins_directory && kan_application_framework_tool_plugins_count > 0u;
    struct kan_plugin_system_config_t plugin_system_config;

    if (has_plugins)
    {
        // Calculate correct plugins directory from path to application.
        // Tool applications should be runnable with any working directory, therefore we need this logic.

        KAN_ASSERT (arguments_count > 0u)
        const char *path_to_application = arguments[0u];
        KAN_ASSERT (path_to_application[0u] != '\0')

        struct kan_file_system_path_container_t path_container;
        kan_file_system_path_container_copy_string (&path_container, path_to_application);
        uint64_t check_index = path_container.length - 1u;

        while (check_index > 0u)
        {
            if (path_container.path[check_index] == '/' || path_container.path[check_index] == '\\')
            {
                break;
            }

            --check_index;
        }

        kan_file_system_path_container_reset_length (&path_container, check_index);
        kan_file_system_path_container_append (&path_container, kan_application_framework_tool_plugins_directory);

        kan_plugin_system_config_init (&plugin_system_config);
        plugin_system_config.plugin_directory_path = kan_allocate_general (
            kan_plugin_system_config_get_allocation_group (), path_container.length + 1u, _Alignof (char));
        memcpy (plugin_system_config.plugin_directory_path, path_container.path, path_container.length + 1u);

        for (uint64_t index = 0u; index < kan_application_framework_tool_plugins_count; ++index)
        {
            const kan_interned_string_t plugin_name = kan_string_intern (kan_application_framework_tool_plugins[index]);
            void *spot = kan_dynamic_array_add_last (&plugin_system_config.plugins);

            if (!spot)
            {
                kan_dynamic_array_set_capacity (&plugin_system_config.plugins,
                                                KAN_MAX (1u, plugin_system_config.plugins.capacity * 2u));
                spot = kan_dynamic_array_add_last (&plugin_system_config.plugins);
                KAN_ASSERT (spot)
            }

            *(kan_interned_string_t *) spot = plugin_name;
        }

        if (!kan_context_request_system (context, KAN_CONTEXT_PLUGIN_SYSTEM_NAME, &plugin_system_config))
        {
            KAN_LOG (application_framework_tool, KAN_LOG_ERROR, "Failed to request plugin system.")
        }
    }

    if (!kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    {
        KAN_LOG (application_framework_tool, KAN_LOG_ERROR, "Failed to request reflection system.")
    }

    kan_context_assembly (context);
    if (has_plugins)
    {
        kan_plugin_system_config_shutdown (&plugin_system_config);
    }

    return context;
}
