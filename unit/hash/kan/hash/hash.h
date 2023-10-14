#pragma once

#include <hash_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Contains functions for hashing different kinds of data.

KAN_C_HEADER_BEGIN

/// \brief Hashes given null-terminated string.
HASH_API uint64_t kan_string_hash (const char *string);

/// \brief Hashes given character sequence.
HASH_API uint64_t kan_char_sequence_hash (const char *begin, const char *end);

KAN_C_HEADER_END
