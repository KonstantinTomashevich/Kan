#pragma once

/// \file
/// \brief Provides simple min-max macros for code readability.

#define KAN_MIN(first, second) ((first) < (second) ? (first) : (second))
#define KAN_MAX(first, second) ((first) > (second) ? (first) : (second))
#define KAN_CLAMP(value, min, max) (KAN_MAX (min, KAN_MIN (max, value)))
