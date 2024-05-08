#include <string.h>

#include <kan/api_common/min_max.h>
#include <kan/application_framework_tool/application_framework_tool.h>
#include <kan/context/plugin_system.h>
#include <kan/context/reflection_system.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>

KAN_LOG_DEFINE_CATEGORY (application_framework_tool);

kan_context_handle_t kan_application_framework_tool_create_context (void)
{
    const kan_allocation_group_t context_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "tool_context");
    kan_context_handle_t context = kan_context_create (context_group);

    const kan_bool_t has_plugins =
        kan_application_framework_tool_plugins_directory && kan_application_framework_tool_plugins_count > 0u;
    struct kan_plugin_system_config_t plugin_system_config;

    if (has_plugins)
    {
        kan_plugin_system_config_init (&plugin_system_config);
        const uint64_t path_length = strlen (kan_application_framework_tool_plugins_directory);
        plugin_system_config.plugin_directory_path =
            kan_allocate_general (kan_plugin_system_config_get_allocation_group (), path_length + 1u, _Alignof (char));
        memcpy (plugin_system_config.plugin_directory_path, kan_application_framework_tool_plugins_directory,
                path_length + 1u);

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
