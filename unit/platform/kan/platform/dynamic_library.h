#pragma once

#include <platform_api.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Contains functions for working with dynamic libraries.

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_platform_dynamic_library_t);

/// \brief Attempts to load dynamic library from given path.
PLATFORM_API kan_platform_dynamic_library_t kan_platform_dynamic_library_load (const char *path);

/// \brief Attempts to find function in given dynamic library.
PLATFORM_API void *kan_platform_dynamic_library_find_function (kan_platform_dynamic_library_t library,
                                                               const char *name);

/// \brief Unloads given dynamic library.
PLATFORM_API void kan_platform_dynamic_library_unload (kan_platform_dynamic_library_t library);

KAN_C_HEADER_END
