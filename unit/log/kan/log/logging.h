#pragma once

#include <log_api.h>

#include <stdint.h>
#include <stdio.h>

#include <kan/api_common/c_header.h>
#include <kan/container/interned_string.h>

/// \file
/// \brief Provides API for logging messages and setting up log categories.
///
/// \par Categories
/// \parblock
/// Explicitly categorizing logs makes it easier to to process and filter them automatically. It is useful both for
/// logging API capabilities as it provides opportunity to select different verbosity for every category and for end
/// user as it makes programmatic filtering straightforward (for example, filter animation-related logs in animation
/// editor).
///
/// For the reasons above, log categories are expected to be statically defined using `KAN_LOG_DEFINE_CATEGORY`.
/// The main reason to declare categories statically is to speedup category access during logging.
/// \endparblock
///
/// \par Logging
/// \parblock
/// Logging is done through `KAN_LOG` and `KAN_LOG_WITH_BUFFER` macros. We embed quite huge amount of logic under the
/// macro, because we need to reduce performance impact of logs, therefore category caching logic is needed here. Then
/// category verbosity must be checked before formatting and formatting must be done on stack.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// Logging API is fully thread safe and operates under logging atomic lock.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Lists supported log verbosity values.
enum kan_log_verbosity_t
{
    KAN_LOG_VERBOSE = 0u,
    KAN_LOG_DEBUG,
    KAN_LOG_INFO,
    KAN_LOG_WARNING,
    KAN_LOG_ERROR,
    KAN_LOG_CRITICAL_ERROR,
    KAN_LOG_DEFAULT = KAN_LOG_INFO
};

typedef uint64_t kan_log_category_t;

/// \brief Indicates that `kan_log_category_t` does not point to correct category.
#define KAN_LOG_CATEGORY_INVALID 0u

#if defined(_WIN32)
#    define KAN_LOG_EXPORT __declspec (dllexport)
#else
#    define KAN_LOG_EXPORT
#endif

/// \brief Statically defines category to be used along with `KAN_LOG`.
#define KAN_LOG_DEFINE_CATEGORY(NAME)                                                                                  \
    KAN_LOG_EXPORT kan_log_category_t kan_log_category_##NAME##_reference = KAN_LOG_CATEGORY_INVALID

/// \brief Requests log category with given name. If category does not exist,
///        it is automatically created with `KAN_LOG_DEFAULT` verbosity.
LOG_API kan_log_category_t kan_log_category_get (const char *name);

/// \brief Sets minimum verbosity for log category.
LOG_API void kan_log_category_set_verbosity (kan_log_category_t category, enum kan_log_verbosity_t verbosity);

/// \brief Returns minimum verbosity for log category.
LOG_API enum kan_log_verbosity_t kan_log_category_get_verbosity (kan_log_category_t category);

/// \brief Returns log category name.
LOG_API kan_interned_string_t kan_log_category_get_name (kan_log_category_t category);

/// \brief Logs given formatted message inside given category using given verbosity.
/// \details Example usage:
///          `KAN_LOG (gameplay_movement, KAN_LOG_INFO, "Character is flying for %d steps.", in_air_steps)`.
#define KAN_LOG(CATEGORY, VERBOSITY, ...)                                                                              \
    KAN_LOG_WITH_BUFFER (KAN_LOG_DEFAULT_BUFFER_SIZE, CATEGORY, VERBOSITY, __VA_ARGS__)

/// \brief The same as `KAN_LOG`, but specifies custom formatting buffer size as first argument.
#define KAN_LOG_WITH_BUFFER(BUFFER_SIZE, CATEGORY, VERBOSITY, ...)                                                     \
    {                                                                                                                  \
        extern kan_log_category_t kan_log_category_##CATEGORY##_reference;                                             \
        if (kan_log_category_##CATEGORY##_reference == KAN_LOG_CATEGORY_INVALID)                                       \
        {                                                                                                              \
            kan_log_category_##CATEGORY##_reference = kan_log_category_get (#CATEGORY);                                \
        }                                                                                                              \
                                                                                                                       \
        enum kan_log_verbosity_t verbosity = kan_log_category_get_verbosity (kan_log_category_##CATEGORY##_reference); \
        if (VERBOSITY >= verbosity)                                                                                    \
        {                                                                                                              \
            char buffer[BUFFER_SIZE];                                                                                  \
            snprintf (buffer, BUFFER_SIZE, __VA_ARGS__);                                                               \
            kan_submit_log (kan_log_category_##CATEGORY##_reference, VERBOSITY, buffer);                               \
        }                                                                                                              \
    }

/// \brief Internal function for `KAN_LOG`, should never be called directly.
LOG_API void kan_submit_log (kan_log_category_t category, enum kan_log_verbosity_t verbosity, const char *message);

KAN_C_HEADER_END
