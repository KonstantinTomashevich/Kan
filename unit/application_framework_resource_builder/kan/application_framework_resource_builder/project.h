#pragma once

#include <application_framework_resource_builder_api.h>

#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>

/// \file
/// \brief Contains project format declaration for resource builder.

KAN_C_HEADER_BEGIN

/// \brief Allocation group used for project data allocation.
APPLICATION_FRAMEWORK_RESOURCE_BUILDER_API kan_allocation_group_t
kan_application_resource_project_allocation_group_get (void);

/// \brief Defines one resource building target.
/// \details As a result of resource building, each target is packed into separate read-only package.
struct kan_application_resource_target_t
{
    /// \brief Name of this target.
    kan_interned_string_t name;

    /// \brief Directories to look for resources of this target.
    /// \meta reflection_dynamic_array_type = "char *"
    struct kan_dynamic_array_t directories;

    /// \brief List of target names which resources are visible for resources of this target.
    /// \details Visibility is used to make sure that dependencies between resources of different targets are handle
    ///          properly. It is purely verification mechanism.
    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t visible_targets;
};

APPLICATION_FRAMEWORK_RESOURCE_BUILDER_API void kan_application_resource_target_init (
    struct kan_application_resource_target_t *instance);

APPLICATION_FRAMEWORK_RESOURCE_BUILDER_API void kan_application_resource_target_shutdown (
    struct kan_application_resource_target_t *instance);

/// \brief Defines project format for resource builder.
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

    /// \brief Absolute path to directory that is used as shared reference cache (might be shared with editors).
    char *reference_cache_absolute_directory;

    /// \brief Absolute path to directory to save output.
    char *output_absolute_directory;

    /// \brief Whether to enable string interning pass for data compression.
    kan_bool_t use_string_interning;
};

APPLICATION_FRAMEWORK_RESOURCE_BUILDER_API void kan_application_resource_project_init (
    struct kan_application_resource_project_t *instance);

APPLICATION_FRAMEWORK_RESOURCE_BUILDER_API void kan_application_resource_project_shutdown (
    struct kan_application_resource_project_t *instance);

KAN_C_HEADER_END
