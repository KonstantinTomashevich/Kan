#pragma once

/// \file
/// \brief Defines begin-end macro for headers for compatibility with C++.
/// \details It also adds highlight Cushion-specific macros so Cushion would not break highlight.

#if defined(__cplusplus)
#    define KAN_C_HEADER_BEGIN                                                                                         \
        extern "C"                                                                                                     \
        {
#    define KAN_C_HEADER_END }
#else
#    define KAN_C_HEADER_BEGIN
#    define KAN_C_HEADER_END
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
// __CUSHION_PRESERVE__ must be added as compile definition, otherwise preserved macro in scan only headers
// would be incorrectly unwrapped during regular preprocessing.

#    define CUSHION_DEFER
#    define __CUSHION_WRAPPED__
#    define CUSHION_STATEMENT_ACCUMULATOR(...)
#    define CUSHION_STATEMENT_ACCUMULATOR_PUSH(...)
#    define CUSHION_STATEMENT_ACCUMULATOR_REF(...)
#    define CUSHION_STATEMENT_ACCUMULATOR_UNREF(...)
#endif
