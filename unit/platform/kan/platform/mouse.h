#pragma once

#include <platform_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Provides basic API for platform-specific mouse data.

KAN_C_HEADER_BEGIN

/// \brief Enumerates mouse buttons that can be known to platform.
enum kan_platform_mouse_button_t
{
    KAN_PLATFORM_MOUSE_BUTTON_LEFT = 0u,
    KAN_PLATFORM_MOUSE_BUTTON_MIDDLE,
    KAN_PLATFORM_MOUSE_BUTTON_RIGHT,
    KAN_PLATFORM_MOUSE_BUTTON_X1,
    KAN_PLATFORM_MOUSE_BUTTON_X2,
};

KAN_C_HEADER_END
