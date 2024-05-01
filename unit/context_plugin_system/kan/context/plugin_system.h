#pragma once

#include <context_plugin_system_api.h>

#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>

/// \file
/// \brief Contains API for context plugin system -- system for loading reflection-driven plugins.
///
/// \par Definition
/// \parblock
/// Context plugin system uses data provided through `kan_plugin_system_config_t` to select and load plugin dynamic
/// libraries. It searches for static reflection registrars inside these plugins and uses them to populate reflection
/// system registry when requested.
/// \endparblock
///
/// \par Hot reload
/// \parblock
/// TODO: Hot reload will be implemented later as part of hot reload feature.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// Context plugin system has no public API.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief System name for requirements and queries.
#define KAN_CONTEXT_PLUGIN_SYSTEM_NAME "plugin_system_t"

/// \brief Contains plugin system configuration data.
struct kan_plugin_system_config_t
{
    char *plugin_directory_path;

    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t plugins;
};

CONTEXT_PLUGIN_SYSTEM_API void kan_plugin_system_config_init (struct kan_plugin_system_config_t *config);

/// \brief Returns allocation group that should be used to allocate `kan_plugin_system_config_t::plugin_directory_path`.
CONTEXT_PLUGIN_SYSTEM_API kan_allocation_group_t kan_plugin_system_config_get_allocation_group (void);

CONTEXT_PLUGIN_SYSTEM_API void kan_plugin_system_config_shutdown (struct kan_plugin_system_config_t *config);

KAN_C_HEADER_END
