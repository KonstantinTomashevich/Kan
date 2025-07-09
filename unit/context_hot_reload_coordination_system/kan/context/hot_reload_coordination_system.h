#pragma once

#include <context_hot_reload_coordination_system_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/context/context.h>
#include <kan/platform/keyboard.h>

/// \file
/// \brief Contains API for context hot reload coordination system.
///
/// \par Definition
/// \parblock
/// Hot reload coordination is needed for centralized control over hot reload behavior in runtime and to centralize
/// hot reload settings instead of duplicating them everywhere.
/// \endparblock
///
/// \par Hot reload routine
/// \parblock
/// As hot reload means rebuilding shared libraries and rebuilding resources of the fly, it must be controlled with
/// accuracy to ensure that it will not fail irrecoverably due to a conflict or race condition with currently running
/// code, especially  when this conflict might break resource compilation due to simultaneous access from client for
/// loading which could make full resource rebuild needed.
///
/// For securing this, hot reload routine was introduced:
///
/// - Hot reload rebuild implementation should work in a separate thread. It should call
///   kan_hot_reload_coordination_system_schedule when it decides that new build process should be executed.
///   Then it must wait until kan_hot_reload_coordination_system_is_executing is true, which would usually take
///   several frames, so thread sleep is advised. It should also check kan_hot_reload_coordination_system_is_scheduled
///   as this function will return false for this thread if scheduled hot reload was declined for some reason. After
///   wait for kan_hot_reload_coordination_system_is_executing is done and it is true, hot reload build process
///   should be executed and kan_hot_reload_coordination_system_finish should be called to report hot reload build
///   finish, even if it was unsuccessful.
///
/// - User logic can check whether hot reload request was scheduled using
///   kan_hot_reload_coordination_system_is_scheduled and delay it if needed due to resource access by one frame using
///   kan_hot_reload_coordination_system_delay. The next frame would also be able to use
///   kan_hot_reload_coordination_system_delay up until the moment hot reload is allowed by the user logic.
///   kan_hot_reload_coordination_system_is_scheduled return value is guaranteed to change once per frame during
///   hot reload coordination system update and not in any other place, so race condition is impossible here.
///
/// - User logic may also call kan_hot_reload_coordination_system_is_executing to block things from happening
///   while hot reload build is in progress. It has the same race condition prevention guarantee as
///   kan_hot_reload_coordination_system_is_scheduled.
///
/// - User logic can check kan_hot_reload_coordination_system_is_possible for whether hot reload is possible at all
///   in current execution context for introducing optimizations that break hot reload. Return value is guaranteed
///   to never change during process execution.
///
/// - When reloading resources that were already loaded, kan_hot_reload_coordination_system_is_reload_allowed should
///   be checked first. User is allowed to pause hot reload at will using hotkeys and in that case hot reload should
///   never happen even if it is technically possible, and that function is used to check this condition.
/// \endparblock
///
/// \par Hot reload pause
/// \parblock
/// Use can toggle whether hot reload is active or paused using hotkeys specified in
/// kan_hot_reload_coordination_system_config_t. When hot reload is paused, scheduled hot reloads will be denied and
/// hot reload if existing assets will be forbidden also.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Contains hot reload coordination system configuration data.
struct kan_hot_reload_coordination_system_config_t
{
    kan_time_offset_t change_wait_time_ns;
    enum kan_platform_scan_code_t toggle_hot_key;
    enum kan_platform_modifier_mask_t toggle_hot_key_modifiers;
};

CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API void kan_hot_reload_coordination_system_config_init (
    struct kan_hot_reload_coordination_system_config_t *instance);

/// \brief Advised safe delay between file change detection and when it should be safe to recognize and process it.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API kan_time_offset_t
kan_hot_reload_coordination_system_get_change_wait_time_ns (kan_context_system_t system);

/// \brief Returns whether hot reload is possible at all in current process context.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API bool kan_hot_reload_coordination_system_is_possible (void);

/// \brief Returns whether reloading already loaded resources or code is currently allowed by user choices.
/// \invariant Should never be called outside context-derived execution routine, for example context update or
///            universe update (which is a part of context update). Otherwise it would cause race condition.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API bool kan_hot_reload_coordination_system_is_reload_allowed (
    kan_context_system_t system);

/// \brief Returns whether hot reload build process execution is requested and scheduled.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API bool kan_hot_reload_coordination_system_is_scheduled (
    kan_context_system_t system);

/// \brief Returns whether hot reload build process is expected to be executing right now.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API bool kan_hot_reload_coordination_system_is_executing (
    kan_context_system_t system);

/// \brief Requests ability to execute hot reload build process.
/// \details User implementation should be centralized in one place, this API is not designed for several request
///          sources at once.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API void kan_hot_reload_coordination_system_schedule (
    kan_context_system_t system);

/// \brief Allows other systems (and mutators in universe) to delay scheduled hot reload build by one frame.
/// \invariant Should never be called outside context-derived execution routine, for example context update or
///            universe update (which is a part of context update). Otherwise it would cause race condition.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API void kan_hot_reload_coordination_system_delay (kan_context_system_t system);

/// \brief Informs that hot reload build process execution has finished.
/// \details User implementation should be centralized in one place, this API is not designed for several execution
///          sources at once.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API void kan_hot_reload_coordination_system_finish (kan_context_system_t system);

KAN_C_HEADER_END
