#pragma once

#include <error_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>

/// \file
/// \brief Declares critical error raising and handling mechanism.
///
/// \par Critical error
/// \parblock
/// Critical error is an error that should never happen to shipped product in the right environment,
/// therefore classic response for this errors is aborting program right away.
/// \endparblock
///
/// \par Assert
/// \parblock
/// Asserts are a useful way to add development-only checks that will be erased in shipping builds.
/// Failed assert in development build triggers critical error.
/// \endparblock
///
/// \par Interactive mode
/// \parblock
/// In development builds it might be useful to skip some critical errors that are too paranoid or do not really crash
/// application. For this case interactive critical error handling mechanism was introduced: when critical error
/// happens, message box pops up that allows us to skip this error, skip all errors from the same file and line,
/// stop and debug or just abort application.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Selects whether interactive critical error reporting is enabled.
ERROR_API void kan_error_set_critical_interactive (bool is_interactive);

/// \brief Reports that critical error has happened.
ERROR_API void kan_error_critical (const char *message, const char *file, int line);

#if defined(KAN_WITH_ASSERT)
#    define KAN_ASSERT(...)                                                                                            \
        if (!(__VA_ARGS__))                                                                                            \
        {                                                                                                              \
            kan_error_critical (#__VA_ARGS__, __FILE__, __LINE__);                                                     \
        }
#else
#    define KAN_ASSERT(...)
#endif

KAN_C_HEADER_END
