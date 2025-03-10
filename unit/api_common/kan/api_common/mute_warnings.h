#pragma once

/// \file
/// \brief Contains macros for muting warnings in third party headers or for some rare cases in Kan.

#if defined(_MSC_VER) && !defined(__clang__)
#    define KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN _Pragma ("warning (push, 0)")
#    define KAN_MUTE_THIRD_PARTY_WARNINGS_END _Pragma ("warning (pop)")
#else
// clang-format off
#    define KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN                                                                        \
        _Pragma ("GCC diagnostic push")                                                                                \
        _Pragma ("GCC diagnostic ignored \"-Wpragmas\"")                                                               \
        _Pragma ("GCC diagnostic ignored \"-Wimplicit-function-declaration\"")                                         \
        _Pragma ("GCC diagnostic ignored \"-Wmissing-field-initializers\"")                                            \
        _Pragma ("GCC diagnostic ignored \"-Wnullability-completeness\"")                                              \
        _Pragma ("GCC diagnostic ignored \"-Wpedantic\"")                                                              \
        _Pragma ("GCC diagnostic ignored \"-Wself-assign\"")                                                           \
        _Pragma ("GCC diagnostic ignored \"-Wignored-attributes\"")                                                    \
        _Pragma ("GCC diagnostic ignored \"-Wunused-function\"")                                                       \
        _Pragma ("GCC diagnostic ignored \"-Wunused-private-field\"")                                                  \
        _Pragma ("GCC diagnostic ignored \"-Wunused-variable\"")
// clang-format on

#    define KAN_MUTE_THIRD_PARTY_WARNINGS_END _Pragma ("GCC diagnostic pop")
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#    define KAN_MUTE_UNINITIALIZED_WARNINGS_BEGIN _Pragma ("warning (push)") _Pragma ("warning (disable : 4701)")
#    define KAN_MUTE_UNINITIALIZED_WARNINGS_END _Pragma ("warning (pop)")
#else
// clang-format off
#    define KAN_MUTE_UNINITIALIZED_WARNINGS_BEGIN                                                                      \
        _Pragma ("GCC diagnostic push")                                                                                \
        _Pragma ("GCC diagnostic ignored \"-Wpragmas\"")                                                               \
        _Pragma ("GCC diagnostic ignored \"-Wunknown-warning-option\"")                                                \
        _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
// clang-format on

#    define KAN_MUTE_UNINITIALIZED_WARNINGS_END _Pragma ("GCC diagnostic pop")
#endif

#if defined(_MSC_VER) && !defined(__clang__)
// clang-format off
#    define KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN                                                                 \
        _Pragma ("warning (push)")                                                                                     \
        _Pragma ("warning (disable : 4024)")                                                                           \
        _Pragma ("warning (disable : 4047)")                                                                           \
        _Pragma ("warning (disable : 4090)")                                                                           \
        _Pragma ("warning (disable : 4133)")
// clang-format on
#    define KAN_MUTE_POINTER_CONVERSION_WARNINGS_END _Pragma ("warning (pop)")
#else
// clang-format off
#    define KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN                                                                 \
        _Pragma ("GCC diagnostic push")                                                                                \
        _Pragma ("GCC diagnostic ignored \"-Wpragmas\"")                                                               \
        _Pragma ("GCC diagnostic ignored \"-Wincompatible-pointer-types\"")
// clang-format on

#    define KAN_MUTE_POINTER_CONVERSION_WARNINGS_END _Pragma ("GCC diagnostic pop")
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#    define KAN_MUTE_UNUSED_WARNINGS_BEGIN _Pragma ("warning (push)") _Pragma ("warning (disable : 4189)")
#    define KAN_MUTE_UNUSED_WARNINGS_END _Pragma ("warning (pop)")
#else
// clang-format off
#    define KAN_MUTE_UNUSED_WARNINGS_BEGIN                                                                             \
        _Pragma ("GCC diagnostic push")                                                                                \
        _Pragma ("GCC diagnostic ignored \"-Wpragmas\"")                                                               \
        _Pragma ("GCC diagnostic ignored \"-Wunused-variable\"")                                                       \
        _Pragma ("GCC diagnostic ignored \"-Wunused-but-set-variable\"")
// clang-format on

#    define KAN_MUTE_UNUSED_WARNINGS_END _Pragma ("GCC diagnostic pop")
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#    define KAN_MUTE_STRUCTURE_PADDED_WARNINGS_BEGIN _Pragma ("warning (push)") _Pragma ("warning (disable : 4324)")
#    define KAN_MUTE_STRUCTURE_PADDED_WARNINGS_END _Pragma ("warning (pop)")
#else
// clang-format off
#    define KAN_MUTE_STRUCTURE_PADDED_WARNINGS_BEGIN                                                                   \
         _Pragma ("GCC diagnostic push")
// clang-format on

#    define KAN_MUTE_STRUCTURE_PADDED_WARNINGS_END _Pragma ("GCC diagnostic pop")
#endif
