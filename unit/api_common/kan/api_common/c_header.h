#pragma once

/// \file
/// \brief Defines begin-end macro for headers for compatibility with C++.

#if defined(__cplusplus)
#    define KAN_C_HEADER_BEGIN                                                                                         \
        extern "C"                                                                                                     \
        {
#    define KAN_C_HEADER_END }
#else
#    define KAN_C_HEADER_BEGIN
#    define KAN_C_HEADER_END
#endif
