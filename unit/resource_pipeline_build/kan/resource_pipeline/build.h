#pragma once

#include <resource_pipeline_build_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/log/logging.h>
#include <kan/reflection/markup.h>
#include <kan/resource_pipeline/meta.h>
#include <kan/resource_pipeline/project.h>
#include <kan/resource_pipeline/reflected_data.h>

/// \file
/// \brief Provides API for resource build tool implementation.
///
/// \par Overview
/// \parblock
/// Resource build tool is used to build resources for selected targets of given project. It takes versions and
/// timestamps into account in order to build only changed and only referenced resources. It is designed to be usable
/// both as a primary function in command line tool and as drop-in blocking function that can be inserted into other
/// tool. `kan_resource_build_setup_t` is used to configure tool execution and `kan_resource_build` is an actual
/// function that executes resource build routine.
///
/// Optionally, deployed resources can be packed into read only pack for virtual file system when
/// `kan_resource_build_pack_mode_t` is provided. When packing is done, resource index is automatically generated with
/// accompanying interned string registry if pack mode requires it.
/// \endparblock

KAN_C_HEADER_BEGIN

RESOURCE_PIPELINE_BUILD_API kan_allocation_group_t kan_resource_build_get_allocation_group (void);

/// \brief Enumerates pack modes for resource build execution.
enum kan_resource_build_pack_mode_t
{
    /// \brief Resource packing is not executed.
    KAN_RESOURCE_BUILD_PACK_MODE_NONE = 0u,

    /// \brief Deployed files are packed without changes and resource index is created.
    KAN_RESOURCE_BUILD_PACK_MODE_REGULAR,

    /// \brief Interned string sharing is executed during file packing and string registry is added as well.
    KAN_RESOURCE_BUILD_PACK_MODE_INTERNED,
};

/// \brief Contains full setup for resource build tool execution.
struct kan_resource_build_setup_t
{
    /// \brief Pointer to loaded resource project.
    const struct kan_resource_project_t *project;

    /// \brief Pointer to built reflected data.
    const struct kan_resource_reflected_data_storage_t *reflected_data;

    /// \brief Pack mode selection.
    enum kan_resource_build_pack_mode_t pack_mode;

    /// \brief Log verbosity that is applied to resource build tool logs.
    /// \details There are lots of KAN_LOG_DEBUG logs for debugging.
    ///          Also, there is a bunch of KAN_LOG_INFO logs that provide general information about build process.
    ///          Set this value to KAN_LOG_ERROR of you want only errors to be printed.
    enum kan_log_verbosity_t log_verbosity;

    /// \brief List of targets to build. Targets that are transitively visible from them will also be built.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t targets;
};

RESOURCE_PIPELINE_BUILD_API void kan_resource_build_setup_init (struct kan_resource_build_setup_t *instance);

RESOURCE_PIPELINE_BUILD_API void kan_resource_build_setup_shutdown (struct kan_resource_build_setup_t *instance);

/// \brief Enumerates build tool execution results.
enum kan_resource_build_result_t
{
    /// \brief Execution finished successfully.
    KAN_RESOURCE_BUILD_RESULT_SUCCESS = 0u,

    /// \brief Encountered duplicate targets in project.
    KAN_RESOURCE_BUILD_RESULT_ERROR_PROJECT_DUPLICATE_TARGETS,

    /// \brief Encountered unknown target among the list of targets requested by the user.
    KAN_RESOURCE_BUILD_RESULT_ERROR_PROJECT_UNKNOWN_TARGET,
    
    /// \brief Unable to find target that is specified as visible by other target.
    KAN_RESOURCE_BUILD_RESULT_ERROR_PROJECT_VISIBLE_TARGET_NOT_FOUND,

    /// \brief Platform configuration setup file is not found.
    KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_NOT_FOUND,

    /// \brief Encountered IO error while reading platform configuration.
    KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_IO_ERROR,

    /// \brief One layer is specified twice in platform configuration setup.
    KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_DUPLICATE_LAYER,

    /// \brief Encountered a file that is neither platform configuration setup nor platform configuration entry.
    KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_UNKNOWN_ENTRY_FILE,

    /// \brief Encountered unknown layer name in platform configuration entry.
    KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_UNKNOWN_LAYER,

    /// \brief There are several tag-enabled platform configuration entries of the same type on the same layer.
    KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_DUPLICATE_TYPE,

    /// \brief Failed to cleanup build workspace.
    KAN_RESOURCE_BUILD_RESULT_ERROR_WORKSPACE_CLEANUP_FAILED,

    /// \brief Failed to create build workspace directory.
    KAN_RESOURCE_BUILD_RESULT_ERROR_WORKSPACE_CANNOT_MAKE_DIRECTORY,

    /// \brief Failed to open resource build log.
    KAN_RESOURCE_BUILD_RESULT_ERROR_LOG_CANNOT_BE_OPENED,

    /// \brief Failed to deserialize resource build log.
    KAN_RESOURCE_BUILD_RESULT_ERROR_LOG_IO_ERROR,

    /// \brief Encountered failure while scanning for raw resources and changes in them.
    KAN_RESOURCE_BUILD_RESULT_ERROR_RAW_RESOURCE_SCAN_FAILED,

    /// \brief Encountered failure while building resources.
    KAN_RESOURCE_BUILD_RESULT_ERROR_BUILD_FAILED,

    /// \brief Encountered failure while packing resource targets.
    KAN_RESOURCE_BUILD_RESULT_ERROR_PACK_FAILED,
};

/// \brief Executes resource build routine, returns only after execution is done.
RESOURCE_PIPELINE_BUILD_API enum kan_resource_build_result_t kan_resource_build (
    struct kan_resource_build_setup_t *setup);

/// \brief Attempts to deserialize data from given path into given resource project instance.
/// \details Resource project dictates which plugins to load and therefore must be loaded prior to context creation.
///          However, it needs to have its own local reflection in order to be deserialized. Therefore its loading is
///          a little bit trickier than usual and handled in this helper function.
RESOURCE_PIPELINE_BUILD_API bool kan_resource_project_load (struct kan_resource_project_t *project,
                                                            const char *from_path);

/// \brief Helper that appends path to deployed entry to container with workspace path.
static inline void kan_resource_build_append_deploy_path_in_workspace (
    struct kan_file_system_path_container_t *container, const char *target, const char *type, const char *name)
{
    kan_file_system_path_container_append (container, KAN_RESOURCE_PROJECT_WORKSPACE_DEPLOY_DIRECTORY);
    kan_file_system_path_container_append (container, target);
    kan_file_system_path_container_append (container, type);
    kan_file_system_path_container_append (container, name);
    kan_file_system_path_container_add_suffix (container, ".bin");
}

/// \brief Helper that appends path to cached entry to container with workspace path.
static inline void kan_resource_build_append_cache_path_in_workspace (
    struct kan_file_system_path_container_t *container, const char *target, const char *type, const char *name)
{
    kan_file_system_path_container_append (container, KAN_RESOURCE_PROJECT_WORKSPACE_CACHE_DIRECTORY);
    kan_file_system_path_container_append (container, target);
    kan_file_system_path_container_append (container, type);
    kan_file_system_path_container_append (container, name);
    kan_file_system_path_container_add_suffix (container, ".bin");
}

/// \brief Helper that appends path to built pack to container with workspace path.
static inline void kan_resource_build_append_pack_path_in_workspace (struct kan_file_system_path_container_t *container,
                                                                     const char *target)
{
    kan_file_system_path_container_append (container, target);
    kan_file_system_path_container_add_suffix (container, ".pack");
}

KAN_C_HEADER_END
