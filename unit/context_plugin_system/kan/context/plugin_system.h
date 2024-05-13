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
/// If requested, plugin system supports hot reload technique. When hot reload is enabled:
/// - Plugin dynamic libraries are copied to separate directory in order to let original dynamic libraries be editable.
/// - Original plugins directory is observed for changes.
/// - When changes are detected, hot reload is triggered.
/// - New directory is created for new version of plugins and they're copied there.
/// - Reflection invalidation triggered and reflection is repopulated from new plugins.
/// - Old plugins are unloaded and their temporary directory is deleted.
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

    /// \brief Whether plugin hot reload is enabled.
    kan_bool_t enable_hot_reload;

    /// \brief If ::enable_hot_reload, plugin hot reload is triggered with this delay after changes.
    uint64_t hot_reload_update_delay_ns;
};

CONTEXT_PLUGIN_SYSTEM_API void kan_plugin_system_config_init (struct kan_plugin_system_config_t *config);

/// \brief Returns allocation group that should be used to allocate `kan_plugin_system_config_t::plugin_directory_path`.
CONTEXT_PLUGIN_SYSTEM_API kan_allocation_group_t kan_plugin_system_config_get_allocation_group (void);

CONTEXT_PLUGIN_SYSTEM_API void kan_plugin_system_config_shutdown (struct kan_plugin_system_config_t *config);

KAN_C_HEADER_END
