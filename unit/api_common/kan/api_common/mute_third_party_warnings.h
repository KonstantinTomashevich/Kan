#pragma once

/// \file
/// \brief Contains macros for muting warnings in third party headers.

#if defined(_MSC_VER) && !defined(__clang__)
#    define KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN _Pragma ("warning (push, 0)")
#    define KAN_MUTE_THIRD_PARTY_WARNINGS_END _Pragma ("warning (pop)")
#else
// clang-format off
#    define KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN                                                                        \
        _Pragma ("GCC diagnostic push")                                                                                \
        _Pragma ("GCC diagnostic ignored \"-Wpedantic\"")
// clang-format on

#    define KAN_MUTE_THIRD_PARTY_WARNINGS_END _Pragma ("GCC diagnostic pop")
#endif
