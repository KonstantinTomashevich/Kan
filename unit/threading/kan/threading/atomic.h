#pragma once

#include <threading_api.h>

#include <stdint.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>

/// \file
/// \brief Defines atomic types and operations on them.
///
/// \par Atomic integer
/// \parblock
/// Atomic integers can be used for two different purposes:
/// - Spin locking: mutual exclusion mechanism that is much more efficient
///   that mutex when conflicts are rare or operations are quick.
/// - Thread-safe counting, for example thread safe reference counting.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Atomic integer that is encapsulated inside structure to forbid arithmetical operations on it,
struct kan_atomic_int_t
{
    int value;
};

/// \brief Creates and initializes new atomic integer with given value.
THREADING_API struct kan_atomic_int_t kan_atomic_int_init (int value);

/// \brief Waits in spinlock until integer becomes zero and captures it by making it non-zero again.
THREADING_API void kan_atomic_int_lock (struct kan_atomic_int_t *atomic);

/// \brief Tries to capture spinlock by making it non-zero if it is zero. Returns whether lock is captured.
THREADING_API kan_bool_t kan_atomic_int_try_lock (struct kan_atomic_int_t *atomic);

/// \brief Unlocks atomic spinlock by setting it to zero value.
THREADING_API void kan_atomic_int_unlock (struct kan_atomic_int_t *atomic);

/// \brief Atomically adds given delta to given atomic integer.
THREADING_API int kan_atomic_int_add (struct kan_atomic_int_t *atomic, int delta);

/// \brief Atomically sets atomic integer value.
THREADING_API int kan_atomic_int_set (struct kan_atomic_int_t *atomic, int new_value);

/// \brief Atomically compares current value with old value and sets new value if old and current values are equal.
THREADING_API kan_bool_t kan_atomic_int_compare_and_set (struct kan_atomic_int_t *atomic, int old_value, int new_value);

/// \brief Atomically retrieves atomic integer values.
THREADING_API int kan_atomic_int_get (struct kan_atomic_int_t *atomic);

KAN_C_HEADER_END
