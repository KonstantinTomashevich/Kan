#pragma once

#include <platform_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Provides basic API for platform-specific pixel formats.

KAN_C_HEADER_BEGIN

/// \brief Enumerates pixel formats that can be known to platform.
enum kan_platform_pixel_format_t
{
    KAN_PLATFORM_PIXEL_FORMAT_UNKNOWN = 0u,
    KAN_PLATFORM_PIXEL_FORMAT_RGBA32,
    KAN_PLATFORM_PIXEL_FORMAT_ABGR32,
    KAN_PLATFORM_PIXEL_FORMAT_ARGB32,
};

KAN_C_HEADER_END
