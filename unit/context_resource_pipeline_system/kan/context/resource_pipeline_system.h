#pragma once

#include <context_resource_pipeline_system_api.h>

#include <kan/api_common/c_header.h>
#include <kan/container/interned_string.h>
#include <kan/context/context.h>
#include <kan/resource_pipeline/resource_pipeline.h>

// TODO: Scheduled for removal, must be a part of resource build tool and nothing else.

/// \file
/// \brief Contains API for context resource pipeline system which is used for common resource pipeline related tasks.
///
/// \par Definition
/// \parblock
/// Resource pipeline system can be used to:
///
/// - Load platform configuration, including hot reload support.
/// - Store whether runtime compilation is enabled.
/// - Build and manage shared reference type info storage.
///
/// These tasks are usually required only for development iterations, therefore it is advised to disable this system
/// in packaged builds unless it is fully necessary.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// All API except for listeners is fully thread safe as it does not modify anything.
/// Listener addition and removal is thread safe.
/// Listeners consume is thread safe on per-listener basis.
/// \endparblock

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_resource_pipeline_system_platform_configuration_listener);

/// \brief Contains resource pipeline system configuration data.
struct kan_resource_pipeline_system_config_t
{
    /// \brief Real path from which platform configuration loading should be started.
    kan_interned_string_t platform_configuration_path;

    /// \brief Whether runtime compilation should be enabled in this execution.
    bool enable_runtime_compilation;

    /// \brief Whether reference type info storage should be built for this execution.
    bool build_reference_type_info_storage;
};

CONTEXT_RESOURCE_PIPELINE_SYSTEM_API void kan_resource_pipeline_system_config_init (
    struct kan_resource_pipeline_system_config_t *instance);

/// \brief Returns latest file modification time among platform configuration files hierarchy.
CONTEXT_RESOURCE_PIPELINE_SYSTEM_API kan_time_size_t
kan_resource_pipeline_system_get_platform_configuration_file_time_ns (kan_context_system_t system);

/// \brief Searches for platform configuration with given type name.
CONTEXT_RESOURCE_PIPELINE_SYSTEM_API const void *kan_resource_pipeline_system_query_platform_configuration (
    kan_context_system_t system, kan_interned_string_t configuration_type_name);

/// \brief Creates new flag-listener instance that is triggered when platform configuration is reloaded.
CONTEXT_RESOURCE_PIPELINE_SYSTEM_API kan_resource_pipeline_system_platform_configuration_listener
kan_resource_pipeline_system_add_platform_configuration_change_listener (kan_context_system_t system);

/// \brief Returns true if listener is triggered. Consumes trigger status in the process.
CONTEXT_RESOURCE_PIPELINE_SYSTEM_API bool kan_resource_pipeline_system_platform_configuration_listener_consume (
    kan_resource_pipeline_system_platform_configuration_listener listener);

/// \brief Removes flag-listener instance for listening to platform configuration reloads.
CONTEXT_RESOURCE_PIPELINE_SYSTEM_API void kan_resource_pipeline_system_remove_platform_configuration_change_listener (
    kan_context_system_t system, kan_resource_pipeline_system_platform_configuration_listener listener);

/// \brief Returns whether runtime compilation is enabled for this execution.
CONTEXT_RESOURCE_PIPELINE_SYSTEM_API bool kan_resource_pipeline_system_is_runtime_compilation_enabled (
    kan_context_system_t system);

/// \brief Returns reference type info storage which is built for this execution or NULL if not enabled.
CONTEXT_RESOURCE_PIPELINE_SYSTEM_API struct kan_resource_reference_type_info_storage_t *
kan_resource_pipeline_system_get_reference_type_info_storage (kan_context_system_t system);

KAN_C_HEADER_END
