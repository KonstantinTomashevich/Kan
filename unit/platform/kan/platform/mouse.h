#pragma once

#include <platform_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Provides basic API for platform-specific mouse data.

KAN_C_HEADER_BEGIN

typedef uint8_t kan_mouse_button_t;

/// \brief Provides value for well known mouse buttons on this platform.
struct kan_mouse_button_table_t
{
    kan_mouse_button_t left;
    kan_mouse_button_t middle;
    kan_mouse_button_t right;
    kan_mouse_button_t x1;
    kan_mouse_button_t x2;
};

/// \brief Returns mouse button value table.
PLATFORM_API const struct kan_mouse_button_table_t *kan_platform_get_mouse_button_table (void);

/// \brief Converts mouse button value to mouse buttons flag for state masking.
PLATFORM_API uint8_t kan_platform_get_mouse_button_mask (kan_mouse_button_t button);

KAN_C_HEADER_END
