#pragma once

#include <precise_time_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>

/// \file
/// \brief Contains utility functions for precise timing.

KAN_C_HEADER_BEGIN

/// \brief Returns count of nanoseconds since precise time initialization (initialization may happen during this call).
PRECISE_TIME_API kan_time_size_t kan_precise_time_get_elapsed_nanoseconds (void);

/// \brief Returns count of nanoseconds since Unix epoch in nanoseconds in UTC zone.
PRECISE_TIME_API kan_time_size_t kan_precise_time_get_epoch_nanoseconds_utc (void);

/// \brief Transfers current thread to sleeping state for given amount of nanoseconds.
PRECISE_TIME_API void kan_precise_time_sleep (kan_time_offset_t nanoseconds);

KAN_C_HEADER_END
