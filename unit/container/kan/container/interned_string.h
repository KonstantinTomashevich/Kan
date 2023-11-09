#pragma once

#include <container_api.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Contains function for string interning.
///
/// \par String interning
/// \parblock
/// Some strings, for example identifiers, are repeatedly used in lots of places and it would be costly to store
/// string instance in every place it is used. This is one of the primary reasons to use string interning: interned
/// string is string literal that is never deallocated and can be copied everywhere as safe null terminated string
/// pointer. There is always only one copy of interned string literal, therefore this pointer can be used as unique
/// value for hashing and comparing, making string interning very effective for hash lookups. It is advised to use
/// interned strings for all textual identifiers.
/// \endparblock

KAN_C_HEADER_BEGIN

typedef const char *kan_interned_string_t;

/// \brief Interns given null terminated string and returns pointer to interned version of it.
/// \details No allocations happen if string or its value is already interned.
CONTAINER_API kan_interned_string_t kan_string_intern (const char *null_terminated_string);

/// \brief Interns character sequence in the same way as kan_string_intern.
CONTAINER_API kan_interned_string_t kan_char_sequence_intern (const char *begin, const char *end);

KAN_C_HEADER_END
