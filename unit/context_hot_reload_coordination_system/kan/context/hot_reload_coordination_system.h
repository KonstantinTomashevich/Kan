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
/// \par Modes
/// \parblock
/// Hot reload can operation in several modes:
/// - Disabled mode disables anything related to hot reload. Used in rare cases where we still want to have hot reload
///   coordination system, but do not need hot reload. For release, it is advised to select "none" implementation in
///   order to delete hot reload coordination entirely from code base.
/// - Automatic independent mode allows logical components to hot reload independently whenever they want. It is the
///   default behavior for most cases as it is usually very convenient.
/// - On request mode trigger hot swap on hotkey or on request by code. Hot swap is a special type of hot reload that
///   is done by every logical component one after another in one frame, which guarantees absence of incorrect
///   intermediate states.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Enumerates supported hot reload modes.
enum kan_hot_reload_mode_t
{
    /// \brief Hot reload is disabled and can never be enabled during program execution.
    KAN_HOT_RELOAD_MODE_DISABLED = 0u,

    /// \brief Every logical component is allowed to automatically hot reload itself as it sees fit
    ///        independently of other components.
    /// \details This is the recommended mode for development when small changes are made one by one and there is no
    ///          logical dependencies between changes as changes will be loaded independently in random order.
    KAN_HOT_RELOAD_MODE_AUTOMATIC_INDEPENDENT,

    /// \brief Hot reload is done on by request basis (for example, through hotkey) and is always done in one frame.
    /// \details This mode is useful when big interconnected changes are made, for example render passes, pipelines
    ///          and material are being changed and changes depend on each other. In that case, it is useful to switch
    ///          to on request mode and request hot swap when the changes are ready to be loaded.
    KAN_HOT_RELOAD_MODE_ON_REQUEST,
};

/// \brief Configuration for automatic independent hot reload mode.
/// \details Enable hotkey is used to switch into this mode.
struct kan_hot_reload_automatic_config_t
{
    /// \brief Safety delay between receiving info about file changes and executing hot reload.
    kan_time_offset_t change_wait_time_ns;

    enum kan_platform_scan_code_t enable_hot_key;
    enum kan_platform_modifier_mask_t enable_hot_key_modifiers;
};

/// \brief Configuration for on request hot reload mode.
/// \details Enable hotkey is used to switch into this mode.
///          Trigger hotkey is used to trigger hot swap.
struct kan_hot_reload_on_request_config_t
{
    enum kan_platform_scan_code_t enable_hot_key;
    enum kan_platform_modifier_mask_t enable_hot_key_modifiers;

    enum kan_platform_scan_code_t trigger_hot_key;
    enum kan_platform_modifier_mask_t trigger_hot_key_modifiers;
};

/// \brief Contains hot reload coordination system configuration data.
struct kan_hot_reload_coordination_system_config_t
{
    enum kan_hot_reload_mode_t initial_mode;
    struct kan_hot_reload_automatic_config_t automatic_independent;
    struct kan_hot_reload_on_request_config_t on_request;
};

CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API void kan_hot_reload_coordination_system_config_init (
    struct kan_hot_reload_coordination_system_config_t *instance);

/// \brief Returns current hot reload mode.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API enum kan_hot_reload_mode_t
kan_hot_reload_coordination_system_get_current_mode (kan_context_system_t system);

/// \brief Changes hot reload mode programmatically.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API void kan_hot_reload_coordination_system_set_current_mode (
    kan_context_system_t system, enum kan_hot_reload_mode_t mode);

/// \brief Returns config for automatic independent hot reload mode.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API struct kan_hot_reload_automatic_config_t *
kan_hot_reload_coordination_system_get_automatic_config (kan_context_system_t system);

/// \brief Returns config for on request hot reload mode.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API struct kan_hot_reload_on_request_config_t *
kan_hot_reload_coordination_system_get_on_request_config (kan_context_system_t system);

/// \brief Returns whether this frame is a hot swap frame. Always false when mode is not on request.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API bool kan_hot_reload_coordination_system_is_hot_swap (
    kan_context_system_t system);

/// \brief Requests hot swap frame. Ignored when mode is not on request.
CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_API void kan_hot_reload_coordination_system_request_hot_swap (
    kan_context_system_t system);

KAN_C_HEADER_END
