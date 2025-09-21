#pragma once

#include <context_hot_reload_coordination_system_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/context/context.h>
#include <kan/file_system_watcher/watcher.h>
#include <kan/platform/keyboard.h>
#include <kan/virtual_file_system/virtual_file_system.h>

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
///   `kan_hot_reload_coordination_system_schedule` when it decides that new build process should be executed.
///   Then it must wait until `kan_hot_reload_coordination_system_is_executing` is true, which would usually take
///   several frames, so thread sleep is advised. It should also check `kan_hot_reload_coordination_system_is_scheduled`
///   as this function will return false for this thread if scheduled hot reload was declined for some reason. After
///   wait for `kan_hot_reload_coordination_system_is_executing` is done and it is true, hot reload build process
///   should be executed and `kan_hot_reload_coordination_system_finish` should be called to report hot reload build
///   finish, even if it was unsuccessful.
///
/// - User logic can check whether hot reload request was scheduled using
///   `kan_hot_reload_coordination_system_is_scheduled` and delay it if needed due to resource access by one frame using
///   `kan_hot_reload_coordination_system_delay`. The next frame would also be able to use
///   `kan_hot_reload_coordination_system_delay` up until the moment hot reload is allowed by the user logic.
///   `kan_hot_reload_coordination_system_is_scheduled` return value is guaranteed to change once per frame during
///   hot reload coordination system update and not in any other place, so race condition is impossible here.
///
/// - User logic may also call `kan_hot_reload_coordination_system_is_executing` to block things from happening
///   while hot reload build is in progress. It has the same race condition prevention guarantee as
///   `kan_hot_reload_coordination_system_is_scheduled`.
///
/// - User logic can check `kan_hot_reload_coordination_system_is_possible` for whether hot reload is possible at all
///   in current execution context for introducing optimizations that break hot reload. Return value is guaranteed
///   to never change during process execution.
///
/// - When reloading resources that were already loaded, `kan_hot_reload_coordination_system_is_reload_allowed` should
///   be checked first. User is allowed to pause hot reload at will using hotkeys and in that case hot reload should
///   never happen even if it is technically possible, and that function is used to check this condition.
/// \endparblock
///
/// \par Hot reload pause
/// \parblock
/// Use can toggle whether hot reload is active or paused using hotkeys specified in
/// `kan_hot_reload_coordination_system_config_t`. When hot reload is paused, scheduled hot reloads will be denied and
/// hot reload if existing assets will be forbidden also.
/// \endparblock
///
/// \par File event providers
/// \parblock
/// Generally, watching for file changes due to hot reload is pretty easy as we only need to check for file changes
/// after scheduled hot reload build operation is complete. However, we would also like to block new hot reload build
/// for some time in order to properly scan for changes. It is much easier to encapsulate all this file watching logic
/// inside hot reload coordination system instead of duplicating it in every user logic unit.
///
/// Therefore, `kan_hot_reload_file_event_provider_t` and `kan_hot_reload_virtual_file_event_provider_t` are provided
/// as wrappers for regular and virtual file system watchers that follow several rules:
///
/// - When hot reload build operation is complete, timer with delay equal to
///   `kan_hot_reload_coordination_system_config_t::change_wait_time_ns` is created. That timer blocks new hot reload 
///   build operations and marks all file event providers for update when it runs out.
///
/// - While file event providers are waiting for updates, hot reload build operations are still denied.
///
/// - When all file event providers have received their updates, receive-window timer is started during which hot reload
///   build operations are still denied and user code has some time to process events.
///
/// - File event providers only provide non-NULL events when all file event providers are updated and there is no active
///   hot reload build operation. It means that all events will start being readable for all providers during the same
///   frame, which should make processing much easier for the user. Also, it guarantees that no events will be readable
///   during operations that are considered blocking for hot reloadable content processing.
///
/// Therefore, file event providers are top level abstraction that is designed to make hot reload more coordinated and
/// more predictable.
/// \endparblock

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_hot_reload_file_event_provider_t);
KAN_HANDLE_DEFINE (kan_hot_reload_virtual_file_event_provider_t);

/// \brief Contains hot reload coordination system configuration data.
struct kan_hot_reload_coordination_system_config_t
{
    kan_time_offset_t change_wait_time_ns;
    kan_time_offset_t receive_window_time_ns;
    enum kan_platform_scan_code_t toggle_hot_key;
    enum kan_platform_modifier_mask_t toggle_hot_key_modifiers;
};

CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API void kan_hot_reload_coordination_system_config_init (
    struct kan_hot_reload_coordination_system_config_t *instance);

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

/// \brief Creates new file event provider that watches real file system at given path.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API kan_hot_reload_file_event_provider_t
kan_hot_reload_file_event_provider_create (kan_context_system_t system, const char *path);

/// \brief Retrieves new change event if any. Returns NULL if no events or not allowed to return events right now.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API const struct kan_file_system_watcher_event_t *
kan_hot_reload_file_event_provider_get (kan_hot_reload_file_event_provider_t provider);

/// \brief Advances event iterator for underlying file system watcher.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API void kan_hot_reload_file_event_provider_advance (
    kan_hot_reload_file_event_provider_t provider);

/// \brief Destroys given file event provider.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API void kan_hot_reload_file_event_provider_destroy (
    kan_hot_reload_file_event_provider_t provider);

/// \brief Creates new file event provider that watches virtual file system at given path.
/// \details Executes proper virtual file system locking under the hood.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API kan_hot_reload_virtual_file_event_provider_t
kan_hot_reload_virtual_file_event_provider_create (kan_context_system_t system, const char *path);

/// \brief Retrieves new change event if any. Returns NULL if no events or not allowed to return events right now.
/// \invariant Must only be called under virtual file system read lock (see `kan_virtual_file_stream_open_for_read`).
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API const struct kan_virtual_file_system_watcher_event_t *
kan_hot_reload_virtual_file_event_provider_get (kan_hot_reload_virtual_file_event_provider_t provider);

/// \brief Advances event iterator for underlying file system watcher.
/// \invariant Must only be called under virtual file system read lock (see `kan_virtual_file_stream_open_for_read`).
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API void kan_hot_reload_virtual_file_event_provider_advance (
    kan_hot_reload_virtual_file_event_provider_t provider);

/// \brief Destroys given file event provider.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API void kan_hot_reload_virtual_file_event_provider_destroy (
    kan_hot_reload_virtual_file_event_provider_t provider);

KAN_C_HEADER_END
