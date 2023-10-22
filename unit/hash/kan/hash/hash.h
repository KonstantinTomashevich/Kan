#pragma once

#include <hash_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Contains functions for hashing different kinds of data.

KAN_C_HEADER_BEGIN

/// \brief Hashes given null-terminated string.
HASH_API uint64_t kan_string_hash (const char *string);

/// \brief Appends given null-terminated string to given hash value.
HASH_API uint64_t kan_string_hash_append (uint64_t hash_value, const char *string);

/// \brief Hashes given character sequence.
HASH_API uint64_t kan_char_sequence_hash (const char *begin, const char *end);

/// \brief Appends given character sequence to given hash value.
HASH_API uint64_t kan_char_sequence_hash_append (uint64_t hash_value, const char *begin, const char *end);

/// \brief Combines two hashes into one hash value.
HASH_API uint64_t kan_hash_combine (uint64_t first, uint64_t second);

KAN_C_HEADER_END
