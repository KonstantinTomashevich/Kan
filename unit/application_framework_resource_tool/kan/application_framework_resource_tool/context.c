#include <kan/application_framework_resource_tool/context.h>
#include <kan/context/all_system_names.h>
#include <kan/context/plugin_system.h>
#include <kan/context/reflection_system.h>
#include <kan/error/critical.h>
#include <kan/file_system/path_container.h>
#include <kan/log/logging.h>

KAN_LOG_DEFINE_CATEGORY (application_framework_resource_tool_context);

kan_context_t kan_application_create_resource_tool_context (const struct kan_application_resource_project_t *project,
                                                            const char *executable_path)
{
    const kan_allocation_group_t context_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "tool_context");
    kan_context_t context = kan_context_create (context_group);

    struct kan_plugin_system_config_t plugin_system_config;
    kan_plugin_system_config_init (&plugin_system_config);

    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_string (&path_container, executable_path);
    kan_instance_size_t check_index = path_container.length - 1u;

    while (check_index > 0u)
    {
        if (path_container.path[check_index] == '/' || path_container.path[check_index] == '\\')
        {
            break;
        }

        --check_index;
    }

    kan_file_system_path_container_reset_length (&path_container, check_index);
    kan_file_system_path_container_append (&path_container, project->plugin_relative_directory);
    plugin_system_config.plugin_directory_path = kan_string_intern (path_container.path);

    for (kan_loop_size_t index = 0u; index < project->plugins.size; ++index)
    {
        const kan_interned_string_t plugin_name = ((kan_interned_string_t *) project->plugins.data)[index];
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
        KAN_LOG (application_framework_resource_tool_context, KAN_LOG_ERROR, "Failed to request plugin system.")
    }

    if (!kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    {
        KAN_LOG (application_framework_resource_tool_context, KAN_LOG_ERROR, "Failed to request reflection system.")
    }

    kan_context_assembly (context);
    kan_plugin_system_config_shutdown (&plugin_system_config);
    return context;
}
