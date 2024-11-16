#include <string.h>

#include <kan/application_framework_resource_project/project.h>
#include <kan/memory/allocation.h>

static kan_allocation_group_t allocation_group;
static kan_bool_t statics_initialized = KAN_FALSE;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (),
                                                           "application_framework_resource_builder_project");
        statics_initialized = KAN_TRUE;
    }
}

kan_allocation_group_t kan_application_resource_project_allocation_group_get (void)
{
    ensure_statics_initialized ();
    return allocation_group;
}

void kan_application_resource_target_init (struct kan_application_resource_target_t *instance)
{
    instance->name = NULL;
    kan_dynamic_array_init (&instance->directories, 0u, sizeof (char *), _Alignof (char *), allocation_group);
    kan_dynamic_array_init (&instance->visible_targets, 0u, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), allocation_group);
}

void kan_application_resource_target_shutdown (struct kan_application_resource_target_t *instance)
{
    for (kan_loop_size_t directory_index = 0u; directory_index < instance->directories.size; ++directory_index)
    {
        char *directory = ((char **) instance->directories.data)[directory_index];
        if (directory)
        {
            kan_free_general (allocation_group, directory, strlen (directory) + 1u);
        }
    }

    kan_dynamic_array_shutdown (&instance->directories);
    kan_dynamic_array_shutdown (&instance->visible_targets);
}

void kan_application_resource_project_init (struct kan_application_resource_project_t *instance)
{
    ensure_statics_initialized ();
    instance->plugin_relative_directory = NULL;
    kan_dynamic_array_init (&instance->plugins, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            allocation_group);
    kan_dynamic_array_init (&instance->targets, 0u, sizeof (struct kan_application_resource_target_t),
                            _Alignof (struct kan_application_resource_target_t), allocation_group);
    instance->reference_cache_absolute_directory = NULL;
    instance->output_absolute_directory = NULL;
    instance->use_string_interning = KAN_TRUE;
    instance->application_source_directory = NULL;
    instance->project_source_directory = NULL;
    instance->source_directory = NULL;
}

void kan_application_resource_project_shutdown (struct kan_application_resource_project_t *instance)
{
    if (instance->plugin_relative_directory)
    {
        kan_free_general (allocation_group, instance->plugin_relative_directory,
                          strlen (instance->plugin_relative_directory) + 1u);
    }

    for (kan_loop_size_t target_index = 0u; target_index < instance->targets.size; ++target_index)
    {
        struct kan_application_resource_target_t *target =
            &((struct kan_application_resource_target_t *) instance->targets.data)[target_index];
        kan_application_resource_target_shutdown (target);
    }

    kan_dynamic_array_shutdown (&instance->plugins);
    kan_dynamic_array_shutdown (&instance->targets);

    if (instance->reference_cache_absolute_directory)
    {
        kan_free_general (allocation_group, instance->reference_cache_absolute_directory,
                          strlen (instance->reference_cache_absolute_directory) + 1u);
    }

    if (instance->output_absolute_directory)
    {
        kan_free_general (allocation_group, instance->output_absolute_directory,
                          strlen (instance->output_absolute_directory) + 1u);
    }

    if (instance->application_source_directory)
    {
        kan_free_general (allocation_group, instance->application_source_directory,
                          strlen (instance->application_source_directory) + 1u);
    }

    if (instance->project_source_directory)
    {
        kan_free_general (allocation_group, instance->project_source_directory,
                          strlen (instance->project_source_directory) + 1u);
    }

    if (instance->source_directory)
    {
        kan_free_general (allocation_group, instance->source_directory, strlen (instance->source_directory) + 1u);
    }
}
