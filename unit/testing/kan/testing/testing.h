#pragma once

#include <testing_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/error/critical.h>

/// \file
/// \brief Contains utilities for setting up tests for Kan modules.
///
/// \par Setting up test cases
/// \parblock
/// To setup test case, you need to declare it using `KAN_TEST_CASE` macro like that:
/// ```c
/// KAN_TEST_CASE (test_case_name_as_valid_c_identifier)
/// {
///     // Your test code...
/// }
/// ```
/// These test case declaration can then be detected by CMake function `kan_setup_tests` and appropriate test runners
/// will be generated for them. Every test case is executed as separate process in its own worker directory, therefore
/// execution context is as clean as possible.
/// \endparblock
///
/// \par Reporting errors
/// \parblock
/// - To report non-critical errors tests should use `kan_test_check_failed` function.
/// - Checks that result in non-critical error if failed should use `KAN_TEST_CHECK` macro.
/// - Checks that result in critical error if failed should use `KAN_TEST_ASSERT` macro.
/// \endparblock

KAN_C_HEADER_BEGIN

// clang-format off
#if defined(_WIN32)
#    define KAN_TEST_CASE(NAME) __declspec(dllexport) void execute_test_case_##NAME ()
#else
#    define KAN_TEST_CASE(NAME) void execute_test_case_##NAME (void)
#endif
// clang-format on

#define KAN_TEST_ASSERT(...)                                                                                           \
    if (!(__VA_ARGS__))                                                                                                \
    {                                                                                                                  \
        kan_error_critical (#__VA_ARGS__, __FILE__, __LINE__);                                                         \
    }

TESTING_API void kan_test_check_failed (const char *message, const char *file, int line);

TESTING_API kan_bool_t kan_test_are_checks_passed (void);

#define KAN_TEST_CHECK(...)                                                                                            \
    if (!(__VA_ARGS__))                                                                                                \
    {                                                                                                                  \
        kan_test_check_failed (#__VA_ARGS__, __FILE__, __LINE__);                                                      \
    }

KAN_C_HEADER_END
