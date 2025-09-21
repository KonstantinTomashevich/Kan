#include <string.h>

#include <kan/memory/allocation.h>
#include <kan/resource_pipeline/project.h>

static kan_allocation_group_t allocation_group;
static bool statics_initialized = false;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "resource_pipeline_project");
        statics_initialized = true;
    }
}

kan_allocation_group_t kan_resource_project_get_allocation_group (void)
{
    ensure_statics_initialized ();
    return allocation_group;
}

void kan_resource_project_target_init (struct kan_resource_project_target_t *instance)
{
    instance->name = NULL;
    kan_dynamic_array_init (&instance->directories, 0u, sizeof (char *), alignof (char *), allocation_group);
    kan_dynamic_array_init (&instance->visible_targets, 0u, sizeof (kan_interned_string_t),
                            alignof (kan_interned_string_t), allocation_group);
}

void kan_resource_project_target_shutdown (struct kan_resource_project_target_t *instance)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS (instance->directories, char *)
    {
        if (*value)
        {
            kan_free_general (allocation_group, *value, strlen (*value) + 1u);
        }
    }

    kan_dynamic_array_shutdown (&instance->visible_targets);
}

void kan_resource_project_init (struct kan_resource_project_t *instance)
{
    ensure_statics_initialized ();
    kan_dynamic_array_init (&instance->targets, 0u, sizeof (struct kan_resource_project_target_t),
                            alignof (struct kan_resource_project_target_t), allocation_group);

    instance->workspace_directory = NULL;
    instance->platform_configuration_directory = NULL;

    kan_dynamic_array_init (&instance->platform_configuration_tags, 0u, sizeof (kan_interned_string_t),
                            alignof (kan_interned_string_t), allocation_group);

    instance->plugin_directory_name = NULL;
    kan_dynamic_array_init (&instance->plugins, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            allocation_group);
}

void kan_resource_project_shutdown (struct kan_resource_project_t *instance)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->targets, kan_resource_project_target)
    if (instance->workspace_directory)
    {
        kan_free_general (allocation_group, instance->workspace_directory, strlen (instance->workspace_directory) + 1u);
    }

    if (instance->platform_configuration_directory)
    {
        kan_free_general (allocation_group, instance->platform_configuration_directory,
                          strlen (instance->platform_configuration_directory) + 1u);
    }

    kan_dynamic_array_shutdown (&instance->platform_configuration_tags);
    if (instance->plugin_directory_name)
    {
        kan_free_general (allocation_group, instance->plugin_directory_name,
                          strlen (instance->plugin_directory_name) + 1u);
    }

    kan_dynamic_array_shutdown (&instance->plugins);
}
