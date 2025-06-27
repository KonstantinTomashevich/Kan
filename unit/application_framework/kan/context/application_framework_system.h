#pragma once

#include <application_framework_api.h>

#include <kan/api_common/c_header.h>
#include <kan/context/context.h>

/// \file
/// \brief Contains API for application framework context system.
///
/// \par Definition
/// \parblock
/// Application framework context system provides API for communicating with application framework implementation
/// from user code.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// As of now, application framework system is not thread safe, because we expect all application framework
/// related requests to be handled in a centralized way. For example, we don't expect several threads to request
/// application exit simultaneously by themselves.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Contains configuration for application framework system.
struct kan_application_framework_system_config_t
{
    /// \brief Number of arguments passed to application framework.
    kan_instance_size_t arguments_count;

    /// \brief Arguments passed to application framework. Managed by application framework itself.
    char **arguments;

    /// \brief Command for automatic plugin code build and hot reload.
    /// \details It should be left NULL when hot reload is disabled.
    char *auto_build_command;

    /// \brief Path to file used as a lock file to prevent concurrent builds from several auto build triggers.
    char *auto_build_lock_file;

    /// \brief Delay in nanoseconds between auto build triggers.
    kan_time_size_t auto_build_delay_ns;
};

/// \brief Returns count of arguments passed to application framework.
APPLICATION_FRAMEWORK_API kan_instance_size_t
kan_application_framework_system_get_arguments_count (kan_context_system_t application_framework_system);

/// \brief Returns arguments passed to application framework.
APPLICATION_FRAMEWORK_API char **kan_application_framework_system_get_arguments (
    kan_context_system_t application_framework_system);

/// \brief Returns minimum frame time in nanoseconds.
/// \details Minimum frame time limits maximum FPS to save hardware resources.
APPLICATION_FRAMEWORK_API kan_time_offset_t
kan_application_framework_get_min_frame_time_ns (kan_context_system_t application_framework_system);

/// \brief Sets minimum frame time in nanoseconds.
APPLICATION_FRAMEWORK_API void kan_application_framework_set_min_frame_time_ns (
    kan_context_system_t application_framework_system, kan_time_offset_t min_frame_time_ns);

/// \brief Request application exit with given exit code.
APPLICATION_FRAMEWORK_API void kan_application_framework_system_request_exit (
    kan_context_system_t application_framework_system, int exit_code);

/// \brief Checks if application exit has been requested.
APPLICATION_FRAMEWORK_API bool kan_application_framework_system_is_exit_requested (
    kan_context_system_t application_framework_system, int *exit_code_output);

KAN_C_HEADER_END
