#pragma once

#include <threading_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>

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
THREADING_API bool kan_atomic_int_try_lock (struct kan_atomic_int_t *atomic);

/// \brief Unlocks atomic spinlock by setting it to zero value.
THREADING_API void kan_atomic_int_unlock (struct kan_atomic_int_t *atomic);

/// \brief Atomically adds given delta to given atomic integer.
THREADING_API int kan_atomic_int_add (struct kan_atomic_int_t *atomic, int delta);

/// \brief Atomically sets atomic integer value.
THREADING_API int kan_atomic_int_set (struct kan_atomic_int_t *atomic, int new_value);

/// \brief Atomically compares current value with old value and sets new value if old and current values are equal.
THREADING_API bool kan_atomic_int_compare_and_set (struct kan_atomic_int_t *atomic, int old_value, int new_value);

/// \brief Atomically retrieves atomic integer values.
THREADING_API int kan_atomic_int_get (struct kan_atomic_int_t *atomic);

// TODO: Atomic int scoped lock.

/// \def KAN_ATOMIC_INT_COMPARE_AND_SET
/// \brief Helper macro for looped compare and set attempts.

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_ATOMIC_INT_COMPARE_AND_SET(PATH)                                                                       \
        const int old_value = kan_atomic_int_get (PATH);                                                               \
        int new_value = 0;                                                                                             \
        for (int fake_cas_iterator = 0; fake_cas_iterator < 1; ++fake_cas_iterator)
#else
#    define KAN_ATOMIC_INT_COMPARE_AND_SET(PATH)                                                                       \
        while (true)                                                                                                   \
        {                                                                                                              \
            const int old_value = kan_atomic_int_get (PATH);                                                           \
            int new_value = 0;                                                                                         \
                                                                                                                       \
            __CUSHION_WRAPPED__                                                                                        \
                                                                                                                       \
            if (kan_atomic_int_compare_and_set (PATH, old_value, new_value))                                           \
            {                                                                                                          \
                break;                                                                                                 \
            }                                                                                                          \
        }
#endif

KAN_C_HEADER_END
