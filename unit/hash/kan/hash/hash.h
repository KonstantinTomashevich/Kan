#pragma once

#include <hash_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>

/// \file
/// \brief Contains functions for hashing different kinds of data.

KAN_C_HEADER_BEGIN

/// \brief Integer type that is used for storing hashes across Kan.
typedef kan_memory_size_t kan_hash_t;

/// \brief Hashes given null-terminated string.
HASH_API kan_hash_t kan_string_hash (const char *string);

/// \brief Appends given null-terminated string to given hash value.
HASH_API kan_hash_t kan_string_hash_append (kan_hash_t hash_value, const char *string);

/// \brief Hashes given character sequence.
HASH_API kan_hash_t kan_char_sequence_hash (const char *begin, const char *end);

/// \brief Appends given character sequence to given hash value.
HASH_API kan_hash_t kan_char_sequence_hash_append (kan_hash_t hash_value, const char *begin, const char *end);

/// \brief Combines two hashes into one hash value.
HASH_API kan_hash_t kan_hash_combine (kan_hash_t first, kan_hash_t second);

KAN_C_HEADER_END
