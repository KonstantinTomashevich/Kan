#pragma once

#include <platform_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Contains utilities for querying information about hardware.

KAN_C_HEADER_BEGIN

/// \brief Returns count of available cpu cores, including logical cores.
PLATFORM_API uint64_t kan_platform_get_cpu_count (void);

/// \brief Returns amount of installed random access memory in megabytes.
PLATFORM_API uint64_t kan_platform_get_random_access_memory (void);

KAN_C_HEADER_END
