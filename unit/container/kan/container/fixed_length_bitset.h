#pragma once

#include <container_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/memory_profiler/allocation_group.h>

/// \file
/// \brief Contains utility functions for working with runtime fixed length bitsets.
///
/// \par Definition
/// \parblock
/// Fixed length bitset is basically an array of 64-bit integers that are used as a set of bits with specialized
/// operations.
/// \endparblock
///
/// \par Allocation policy
/// \parblock
/// Bitset should be allocated by the user as a continuous memory block of size returned by
/// `kan_fixed_length_bitset_calculate_allocation_size`.
/// \endparblock
///
/// \par Usage
/// \parblock
/// After you've calculated how many bits you need, you can allocate fixed length bitset using
/// `kan_fixed_length_bitset_calculate_allocation_size` to get allocation size. Then `kan_fixed_length_bitset_init`
/// should be called to init bitset and fill it with zeroes. After that, any implemented function can be called in
/// order to modify or query bitset data. There is no shutdown function due to the fact that bitset does not
/// allocate anything itself.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// Fixed length bitset follows classic access guidelines: operations that do not modify state are thread safe unless
/// some other thread modifies state. All other operations are not thread safe.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Represents fixed length bitset structure in memory.
struct kan_fixed_length_bitset_t
{
    uint64_t items;
    uint64_t data[];
};

/// \brief Calculates allocation size for the bitset with given length (count of bits).
static inline uint64_t kan_fixed_length_bitset_calculate_allocation_size (uint64_t length)
{
    return sizeof (struct kan_fixed_length_bitset_t) + (length / 64u) * sizeof (uint64_t) +
           (length % 64u == 0u ? 0u : sizeof (uint64_t));
}

/// \brief Initializes allocated bitset with given length (count of bits) with zeroes.
CONTAINER_API void kan_fixed_length_bitset_init (struct kan_fixed_length_bitset_t *bitset, uint64_t length);

/// \brief Sets value of bit at given bit index.
CONTAINER_API void kan_fixed_length_bitset_set (struct kan_fixed_length_bitset_t *bitset,
                                                uint64_t index,
                                                kan_bool_t value);

/// \brief Queries value of bit at given bit index.
CONTAINER_API kan_bool_t kan_fixed_length_bitset_get (const struct kan_fixed_length_bitset_t *bitset, uint64_t index);

/// \brief Performs logical or operation on every bit from two bitsets and assigns result to the first bitset.
CONTAINER_API void kan_fixed_length_bitset_or_assign (struct kan_fixed_length_bitset_t *bitset,
                                                      const struct kan_fixed_length_bitset_t *source_bitset);

/// \brief Checks if there is any bit that is set in both bitsets.
CONTAINER_API kan_bool_t kan_fixed_length_bitset_check_intersection (
    const struct kan_fixed_length_bitset_t *bitset, const struct kan_fixed_length_bitset_t *other_bitset);

KAN_C_HEADER_END
