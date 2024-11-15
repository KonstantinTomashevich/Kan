#pragma once

#include <universe_api.h>

#include <kan/api_common/c_header.h>
#include <kan/context/context.h>
#include <kan/universe/universe.h>

/// \file
/// \brief Contains API for context universe system -- integration of universe into context.
///
/// \par Definition
/// \parblock
/// Universe system integrates universe instance with reflection and update systems,
/// therefore providing automatic migration and update for the stored universe.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief System name for requirements and queries.
#define KAN_CONTEXT_UNIVERSE_SYSTEM_NAME "universe_system_t"

/// \brief Configuration for the universe system.
struct kan_universe_system_config_t
{
    /// \brief List of environment tags that will be passed to the universe.
    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t environment_tags;
};

UNIVERSE_API void kan_universe_system_config_init (struct kan_universe_system_config_t *instance);

UNIVERSE_API void kan_universe_system_config_shutdown (struct kan_universe_system_config_t *instance);

/// \brief Returns stored universe instance if it was possible to create it.
UNIVERSE_API kan_universe_t kan_universe_system_get_universe (kan_context_system_t universe_system);

KAN_C_HEADER_END
