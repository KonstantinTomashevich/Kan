#pragma once

#include <platform_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>

/// \file
/// \brief Contains utilities for querying information about hardware.

KAN_C_HEADER_BEGIN

/// \brief Returns count of available cpu cores, including logical cores.
PLATFORM_API kan_instance_size_t kan_platform_get_cpu_logical_core_count (void);

/// \brief Returns amount of installed random access memory in megabytes.
PLATFORM_API kan_memory_size_t kan_platform_get_random_access_memory (void);

KAN_C_HEADER_END
