#pragma once

#include <platform_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Contains utility functions for precise timing.

KAN_C_HEADER_BEGIN

/// \brief Returns count of nanoseconds since precise time initialization (initialization may happen during this call).
PLATFORM_API uint64_t kan_platform_get_elapsed_nanoseconds (void);

/// \brief Transfers current thread to sleeping state for given amount of nanoseconds.
PLATFORM_API void kan_platform_sleep (uint64_t nanoseconds);

KAN_C_HEADER_END
