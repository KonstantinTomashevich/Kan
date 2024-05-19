#pragma once

#include <universe_time_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Provides common singleton that is used to provide information about time.
///
/// \par Definition
/// \parblock
/// Time singleton goal is to provide time information to user logic without depending on scheduler selection.
/// For example, logic and visual mutators might be split into different pipelines, but might be in one pipeline.
/// Mutators should be able to use time from this singleton and work correctly without adding dependency on scheduler
/// and pipeline selection.
/// \endparblock
///
/// \par Logical and visual time
/// \parblock
/// There are two types of reported time:
/// - Logical time should be used for world simulation and game logic, for example for physics simulation and character
///   abilities feature. Depending on scheduler selection, this time might be incremented using fixed sub-stepping.
/// - Visual time should be used for visual effects and everything connected to them, from rendering to UI effects.
/// \endparblock
///
/// \par Time scaling
/// \parblock
/// Time singleton also allows user to provide time scale value, which is used in time-related calculations. This value
/// affects how deltas are calculated and how time is incremented. For special cases when time scale should be ignored,
/// visual_unscaled_delta_ns value is provided.
/// \endparblock
///
/// \par Usage in worlds
/// \parblock
/// It is advised to avoid using time singleton in logic that belongs to root worlds or some other worlds that have
/// lots of children. By using time singleton only in leaf worlds you could easily have separate context-related time,
/// like in-game time, for example.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Contains data about time.
struct kan_time_singleton_t
{
    uint64_t logical_time_ns;
    uint64_t logical_delta_ns;

    uint64_t visual_time_ns;
    uint64_t visual_delta_ns;
    uint64_t visual_unscaled_delta_ns;

    float scale;
};

UNIVERSE_TIME_API void kan_time_singleton_init (struct kan_time_singleton_t *instance);

KAN_C_HEADER_END
