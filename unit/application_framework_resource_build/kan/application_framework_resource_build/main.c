#include <stdio.h>
#include <string.h>

#include <kan/context/all_system_names.h>
#include <kan/context/plugin_system.h>
#include <kan/context/reflection_system.h>
#include <kan/error/context.h>
#include <kan/error/critical.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/path_container.h>
#include <kan/resource_pipeline/build.h>

KAN_LOG_DEFINE_CATEGORY (application_framework_resource_build);

enum argument_mode_t
{
    ARGUMENT_MODE_NONE = 0u,
    ARGUMENT_MODE_PROJECT,
    ARGUMENT_MODE_LOG,
    ARGUMENT_MODE_PACK,
    ARGUMENT_MODE_TARGETS,
};

static const char help_message[] =
    "Resource pipeline build tool\n"
    "Tool for building resources for projects on Kan engine.\n"
    "Every project should have its own project-specific executable.\n"
    "\n"
    "To specify what arguments mean, argument switches are used:\n"
    "\n"
    "    --project        Argument after this one is treated as path to resource project.\n"
    "\n"
    "    --log            Specifies logging mode for resource build tool. Supported:\n"
    "                         debug      All messages are printed for better debugging.\n"
    "                         regular    Only error and generic high level informational messages are printed.\n"
    "                         quiet      Only error messages are printed.\n"
    "\n"
    "    --pack           Specifies pack mode for resource build tool. Supported:\n"
    "                         none        Packing is not done in this execution.\n"
    "                         regular     Uses regular packing strategy for resources.\n"
    "                         interned    Strings are interned in resource binary data in target scope.\n"
    "\n"
    "    --targets        Specifies target names to build. Supports several arguments.\n"
    "\n"
    "For proper execution, resource project and at least one target must be specified.\n";

enum error_code_t
{
    ERROR_CODE_INVALID_ARGUMENTS = -1,
    ERROR_CODE_SETUP_FAILED = -2,
    ERROR_CODE_BUILD_FAILED = -3,
};

int main (int argc, char **argv)
{
    kan_error_initialize ();
    kan_log_ensure_initialized ();

    if (argc == 1 || (argc == 2 && (strcmp (argv[1u], "--help") == 0 || strcmp (argv[1u], "-help") == 0 ||
                                    strcmp (argv[1u], "/?") == 0)))
    {
        fprintf (stdout, "%s\n", help_message);
        return 0;
    }

    enum argument_mode_t argument_mode = ARGUMENT_MODE_NONE;
    const char *project_path = NULL;
    bool log_level_selected = false;
    bool pack_selected = false;

    struct kan_resource_build_setup_t setup;
    kan_resource_build_setup_init (&setup);
    CUSHION_DEFER { kan_resource_build_setup_shutdown (&setup); }

    for (unsigned int index = 1u; index < (unsigned int) argc; ++index)
    {
        char *argument = argv[index];
        if (strcmp (argument, "--project") == 0)
        {
            argument_mode = ARGUMENT_MODE_PROJECT;
            continue;
        }
        else if (strcmp (argument, "--log") == 0)
        {
            argument_mode = ARGUMENT_MODE_LOG;
            continue;
        }
        else if (strcmp (argument, "--pack") == 0)
        {
            argument_mode = ARGUMENT_MODE_PACK;
            continue;
        }
        else if (strcmp (argument, "--targets") == 0)
        {
            argument_mode = ARGUMENT_MODE_TARGETS;
            continue;
        }

        switch (argument_mode)
        {
        case ARGUMENT_MODE_NONE:
            KAN_LOG (application_framework_resource_build, KAN_LOG_ERROR,
                     "Encountered argument without any argument switch before that.")
            return ERROR_CODE_INVALID_ARGUMENTS;

        case ARGUMENT_MODE_PROJECT:
            if (project_path)
            {
                KAN_LOG (application_framework_resource_build, KAN_LOG_ERROR,
                         "Encountered project path argument when project path is already provided.")
                return ERROR_CODE_INVALID_ARGUMENTS;
            }

            project_path = argument;
            break;

        case ARGUMENT_MODE_LOG:
            if (log_level_selected)
            {
                KAN_LOG (application_framework_resource_build, KAN_LOG_ERROR,
                         "Encountered log level argument when log level is already provided.")
                return ERROR_CODE_INVALID_ARGUMENTS;
            }

            log_level_selected = true;
            if (strcmp (argument, "debug") == 0)
            {
                setup.log_verbosity = KAN_LOG_DEBUG;
            }
            else if (strcmp (argument, "regular") == 0)
            {
                setup.log_verbosity = KAN_LOG_INFO;
            }
            else if (strcmp (argument, "quiet") == 0)
            {
                setup.log_verbosity = KAN_LOG_ERROR;
            }
            else
            {
                KAN_LOG (application_framework_resource_build, KAN_LOG_ERROR,
                         "Encountered unknown log level argument: \"%s\".", argument)
                return ERROR_CODE_INVALID_ARGUMENTS;
            }

            break;

        case ARGUMENT_MODE_PACK:
            if (pack_selected)
            {
                KAN_LOG (application_framework_resource_build, KAN_LOG_ERROR,
                         "Encountered pack argument when pack is already provided.")
                return ERROR_CODE_INVALID_ARGUMENTS;
            }

            pack_selected = true;
            if (strcmp (argument, "none") == 0)
            {
                setup.pack_mode = KAN_RESOURCE_BUILD_PACK_MODE_NONE;
            }
            else if (strcmp (argument, "regular") == 0)
            {
                setup.pack_mode = KAN_RESOURCE_BUILD_PACK_MODE_REGULAR;
            }
            else if (strcmp (argument, "interned") == 0)
            {
                setup.pack_mode = KAN_RESOURCE_BUILD_PACK_MODE_INTERNED;
            }
            else
            {
                KAN_LOG (application_framework_resource_build, KAN_LOG_ERROR,
                         "Encountered unknown pack argument: \"%s\".", argument)
                return ERROR_CODE_INVALID_ARGUMENTS;
            }

            break;

        case ARGUMENT_MODE_TARGETS:
        {
            kan_interned_string_t *spot = kan_dynamic_array_add_last (&setup.targets);
            if (!spot)
            {
                kan_dynamic_array_set_capacity (&setup.targets, KAN_MAX (1u, setup.targets.size * 2u));
                spot = kan_dynamic_array_add_last (&setup.targets);
            }

            *spot = kan_string_intern (argument);
            break;
        }
        }
    }

    if (!project_path)
    {
        KAN_LOG (application_framework_resource_build, KAN_LOG_ERROR,
                 "Project path is not specified, aborting execution.");
        return ERROR_CODE_INVALID_ARGUMENTS;
    }

    if (setup.targets.size == 0u)
    {
        KAN_LOG (application_framework_resource_build, KAN_LOG_ERROR, "No targets specified, aborting execution.")
        return ERROR_CODE_INVALID_ARGUMENTS;
    }

    struct kan_resource_project_t project;
    setup.project = &project;
    kan_resource_project_init (&project);
    CUSHION_DEFER { kan_resource_project_shutdown (&project); }

    if (!kan_resource_project_load (&project, project_path))
    {
        KAN_LOG (application_framework_resource_build, KAN_LOG_ERROR, "Failed to load resource project from \"%s\".",
                 project_path)
        return ERROR_CODE_SETUP_FAILED;
    }

    struct kan_file_system_path_container_t lock_file_path;
    kan_file_system_path_container_copy_string (&lock_file_path, project.workspace_directory);

    while (lock_file_path.length > 0u && (lock_file_path.path[lock_file_path.length - 1u] == '/' ||
                                          lock_file_path.path[lock_file_path.length - 1u] == '\\'))
    {
        --lock_file_path.length;
    }

    kan_file_system_path_container_add_suffix (&lock_file_path, ".build_lock");
    enum kan_file_system_lock_file_flags_t lock_flags =
        KAN_FILE_SYSTEM_LOCK_FILE_FILE_PATH | KAN_FILE_SYSTEM_LOCK_FILE_BLOCKING;

    if (setup.log_verbosity == KAN_LOG_ERROR)
    {
        lock_flags |= KAN_FILE_SYSTEM_LOCK_FILE_QUIET;
    }

    if (!kan_file_system_lock_file_create (lock_file_path.path, lock_flags))
    {
        KAN_LOG (application_framework_resource_build, KAN_LOG_ERROR,
                 "Failed to lock workspace using lock file \"%s\".", lock_file_path.path)
        return ERROR_CODE_SETUP_FAILED;
    }

    CUSHION_DEFER { kan_file_system_lock_file_destroy (lock_file_path.path, lock_flags); }
    const kan_allocation_group_t context_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "tool_context");

    kan_context_t context = kan_context_create (context_allocation_group);
    CUSHION_DEFER { kan_context_destroy (context); }

    struct kan_plugin_system_config_t plugin_system_config;
    kan_plugin_system_config_init (&plugin_system_config);
    CUSHION_DEFER { kan_plugin_system_config_shutdown (&plugin_system_config); }

    struct kan_file_system_path_container_t plugin_directory_path;
    kan_file_system_path_container_copy_string (&plugin_directory_path, argv[0u]);

    // Clear out executable name from path.
    while (plugin_directory_path.length > 0u && plugin_directory_path.path[plugin_directory_path.length - 1u] != '/' &&
           plugin_directory_path.path[plugin_directory_path.length - 1u] != '\\')
    {
        --plugin_directory_path.length;
    }

    kan_file_system_path_container_append (&plugin_directory_path, project.plugin_directory_name);
    plugin_system_config.plugin_directory_path = kan_string_intern (plugin_directory_path.path);

    kan_dynamic_array_set_capacity (&plugin_system_config.plugins, project.plugins.size);
    plugin_system_config.plugins.size = project.plugins.size;
    memcpy (plugin_system_config.plugins.data, project.plugins.data,
            sizeof (kan_interned_string_t) * project.plugins.size);
    bool context_systems_requested = true;

    if (!kan_context_request_system (context, KAN_CONTEXT_PLUGIN_SYSTEM_NAME, &plugin_system_config))
    {
        KAN_LOG (application_framework_resource_build, KAN_LOG_ERROR, "Failed to request plugin system.")
        context_systems_requested = false;
    }

    if (!kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    {
        KAN_LOG (application_framework_resource_build, KAN_LOG_ERROR, "Failed to request reflection system.")
        context_systems_requested = false;
    }

    if (!context_systems_requested)
    {
        KAN_LOG (application_framework_resource_build, KAN_LOG_ERROR,
                 "Failed to request required systems from context.")
        return ERROR_CODE_SETUP_FAILED;
    }

    kan_context_assembly (context);
    kan_context_system_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_ASSERT (KAN_HANDLE_IS_VALID (reflection_system))

    struct kan_resource_reflected_data_storage_t reflected_data;
    setup.reflected_data = &reflected_data;
    kan_resource_reflected_data_storage_build (&reflected_data, kan_reflection_system_get_registry (reflection_system));
    CUSHION_DEFER { kan_resource_reflected_data_storage_shutdown (&reflected_data); }

    const enum kan_resource_build_result_t result = kan_resource_build (&setup);
    if (result != KAN_RESOURCE_BUILD_RESULT_SUCCESS)
    {
        KAN_LOG (application_framework_resource_build, KAN_LOG_ERROR, "Resource build failed with code %u.",
                 (unsigned int) result)
    }

    return result == KAN_RESOURCE_BUILD_RESULT_SUCCESS ? 0 : ERROR_CODE_BUILD_FAILED;
}
