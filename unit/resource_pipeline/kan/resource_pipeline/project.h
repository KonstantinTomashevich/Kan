#pragma once

#include <resource_pipeline_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/reflection/markup.h>

/// \file
/// \brief Contains resource project format declaration for resource pipeline tools.

KAN_C_HEADER_BEGIN

/// \brief Returns allocation group that is used for allocating everything connected to resource project.
RESOURCE_PIPELINE_API kan_allocation_group_t kan_resource_project_get_allocation_group (void);

/// \brief Defines one resource building target.
/// \details As a result of resource building, each target is packed into separate read-only package for packaged
///          distribution. When in development mode, each target is deployed into its own directory.
struct kan_resource_project_target_t
{
    /// \brief Name of this target.
    kan_interned_string_t name;

    /// \brief Directories to look for raw resources of this target. Absolute.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (char *)
    struct kan_dynamic_array_t directories;

    /// \brief List of target names which resources are visible for resources of this target.
    /// \details Visibility is used to make sure that dependencies between resources of different targets are handled
    ///          properly and all references are properly resolved.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t visible_targets;
};

RESOURCE_PIPELINE_API void kan_resource_project_target_init (struct kan_resource_project_target_t *instance);

RESOURCE_PIPELINE_API void kan_resource_project_target_shutdown (struct kan_resource_project_target_t *instance);

/// \brief Deployed resources for every target must be stored in "<workspace>/deploy/<target_name>".
#define KAN_RESOURCE_PROJECT_WORKSPACE_DEPLOY_DIRECTORY "deploy"

/// \brief Cached resources for every target must be stored in "<workspace>/cache/<target_name>".
#define KAN_RESOURCE_PROJECT_WORKSPACE_CACHE_DIRECTORY "cache"

/// \brief Temporary data for every target must be stored in "<workspace>/temporary/<target_name>".
#define KAN_RESOURCE_PROJECT_WORKSPACE_TEMPORARY_DIRECTORY "temporary"

/// \brief Defines project format for application framework tools.
struct kan_resource_project_t
{
    /// \brief List of resource building targets.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_project_target_t)
    struct kan_dynamic_array_t targets;

    /// \brief Path to directory to be used as workspace.
    char *workspace_directory;

    /// \brief Path to directory with platform configuration for this resource project.
    char *platform_configuration_directory;

    /// \brief List of enabled tags for calculating platform configuration.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t platform_configuration_tags;

    /// \brief Name of the directory that should contain plugins to be loaded.
    /// \details Plugin directory is expected to be located in the same directory as resource tool executable.
    char *plugin_directory_name;

    /// \brief List of plugin names to be loaded.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t plugins;
};

RESOURCE_PIPELINE_API void kan_resource_project_init (struct kan_resource_project_t *instance);

RESOURCE_PIPELINE_API void kan_resource_project_shutdown (struct kan_resource_project_t *instance);

KAN_C_HEADER_END
