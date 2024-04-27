#pragma once

#include <platform_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Provides basic API for platform-specific pixel formats.

KAN_C_HEADER_BEGIN

typedef uint32_t kan_pixel_format_t;

#define KAN_INVALID_PIXEL_FORMAT 0u

/// \brief Contains values for well known pixel formats on this platform.
struct kan_platform_pixel_format_table_t
{
    kan_pixel_format_t rgba_32;
    kan_pixel_format_t abgr_32;
    kan_pixel_format_t argb_32;
};

/// \brief Returns pixel format value table.
PLATFORM_API const struct kan_platform_pixel_format_table_t *kan_platform_get_pixel_format_table (void);

KAN_C_HEADER_END
