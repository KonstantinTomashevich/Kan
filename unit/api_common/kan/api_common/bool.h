#pragma once

#include <kan/api_common/c_header.h>

/// \file
/// \brief Defines boolean literal to be used across `Kan` project.

KAN_C_HEADER_BEGIN

/// \brief Defines boolean literal as simple enumeration.
enum kan_bool_t : uint8_t
{
    KAN_FALSE = 0u,
    KAN_TRUE = 1u,
};

typedef enum kan_bool_t kan_bool_t;

KAN_C_HEADER_END
