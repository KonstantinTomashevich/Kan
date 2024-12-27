#pragma once

#include <error_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>

/// \file
/// \brief Contains common error handling context functions.

KAN_C_HEADER_BEGIN

/// \brief Initializes error context with default settings. Setups crash handling.
ERROR_API void kan_error_initialize_context (void);

KAN_C_HEADER_END
