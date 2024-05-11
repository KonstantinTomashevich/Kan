#pragma once

#include <context_universe_world_definition_system_api.h>

#include <kan/api_common/c_header.h>
#include <kan/context/context.h>
#include <kan/universe/universe.h>

/// \file
/// \brief Contains API for context universe world definition system.
///
/// \par Definition
/// \parblock
/// Universe world definition system automatically loads universe world definitions from virtual file system
/// and provides access to them. If it is requested, this system is also able to observe changes and reload
/// world definitions when needed.
/// \endparblock
///
/// \par Definition naming
/// \parblock
/// Definition names are based on path to them in virtual file system. Path is split into several parts:
/// - Base mount path (and following slash).
/// - Definition name.
/// - Format extension.
///
/// For example, lets set "universe_world" as base mount path, then definition from path
/// "universe_world/editor/scene.rd" will have name "editor/scene".
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief System name for requirements and queries.
#define KAN_CONTEXT_UNIVERSE_WORLD_DEFINITION_SYSTEM_NAME "universe_world_definition_system_t"

/// \brief Describes configuration for universe world definition system.
struct kan_universe_world_definition_system_config_t
{
    /// \brief Path to virtual directory under which world definitions are stored.
    /// \invariant All files under this path must be world definitions, other types are not permitted.
    char *definitions_mount_path;

    /// \brief Whether system should observe definitions directory for changes and reload definitions.
    kan_bool_t observe_definitions;

    /// \brief Time between last change to definition file and definition reload.
    /// \details Needed to correctly reload in case of subsequent changes to file.
    uint64_t observation_rescan_delay_ns;
};

/// \brief Queries world definition by its unique name.
CONTEXT_UNIVERSE_WORLD_DEFINITION_SYSTEM_API const struct kan_universe_world_definition_t *
kan_universe_world_definition_system_query (kan_context_system_handle_t universe_world_definition_system,
                                            kan_interned_string_t definition_name);

KAN_C_HEADER_END
