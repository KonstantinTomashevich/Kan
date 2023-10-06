#pragma once

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Defines boolean literal to be used across `Kan` project.

KAN_C_HEADER_BEGIN

typedef uint8_t kan_bool_t;

#define KAN_FALSE 0u
#define KAN_TRUE 1u

KAN_C_HEADER_END
