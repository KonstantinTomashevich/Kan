#pragma once

#include <application_framework_resource_tool_api.h>

#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>

/// \file
/// \brief Contains project format declaration for application framework tools.

KAN_C_HEADER_BEGIN

/// \brief Allocation group used for project data allocation.
APPLICATION_FRAMEWORK_RESOURCE_TOOL_API kan_allocation_group_t
kan_application_resource_project_allocation_group_get (void);

/// \brief Defines one resource building target.
/// \details As a result of resource building, each target is packed into separate read-only package.
struct kan_application_resource_target_t
{
    /// \brief Name of this target.
    kan_interned_string_t name;

    /// \brief Directories to look for resources of this target. Relative to project file.
    /// \meta reflection_dynamic_array_type = "char *"
    struct kan_dynamic_array_t directories;

    /// \brief List of target names which resources are visible for resources of this target.
    /// \details Visibility is used to make sure that dependencies between resources of different targets are handle
    ///          properly. It is purely verification mechanism.
    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t visible_targets;
};

APPLICATION_FRAMEWORK_RESOURCE_TOOL_API void kan_application_resource_target_init (
    struct kan_application_resource_target_t *instance);

APPLICATION_FRAMEWORK_RESOURCE_TOOL_API void kan_application_resource_target_shutdown (
    struct kan_application_resource_target_t *instance);

/// \brief Defines project format for application framework tools.
struct kan_application_resource_project_t
{
    /// \brief Relative path to directory with plugin to be loaded.
    char *plugin_relative_directory;

    /// \brief List of plugin names to be loaded.
    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t plugins;

    /// \brief List of resource building targets.
    /// \meta reflection_dynamic_array_type = "struct kan_application_resource_target_t"
    struct kan_dynamic_array_t targets;

    /// \brief Path to directory that is used as shared reference cache (might be shared with editors).
    ///        Relative to project file.
    char *reference_cache_directory;

    /// \brief Path to directory to save resource build output. Relative to project file.
    char *output_directory;

    /// \brief Whether to enable string interning pass for data compression.
    kan_bool_t use_string_interning;

    /// \brief Location of the directory where this application is defined. Relative to project file.
    char *application_source_directory;

    /// \brief Location of the directory where current CMake project is defined. Relative to project file.
    char *project_source_directory;

    /// \brief Location of CMake generation source directory. Relative to project file.
    char *source_directory;

    /// \brief Location of platform configuration file for resource building. Relative to project file.
    char *platform_configuration;
};

APPLICATION_FRAMEWORK_RESOURCE_TOOL_API void kan_application_resource_project_init (
    struct kan_application_resource_project_t *instance);

APPLICATION_FRAMEWORK_RESOURCE_TOOL_API void kan_application_resource_project_shutdown (
    struct kan_application_resource_project_t *instance);

/// \brief Reads project from given real path using temporary registry for deserialization.
APPLICATION_FRAMEWORK_RESOURCE_TOOL_API kan_bool_t
kan_application_resource_project_read (const char *path, struct kan_application_resource_project_t *project);

KAN_C_HEADER_END
