#pragma once

#include <stddef.h>
#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/type_punning.h>

/// \file
/// \brief Contains definitions and macros for basic types commonly used across Kan.
///
/// \par Boolean
/// \parblock
/// Classic one-byte boolean is implemented as `kan_bool_t`.
/// \endparblock
///
/// \par Numeric types
/// \parblock
/// This file provides several numeric types with most of them defined by platform numeric preset:
///
/// - `kan_file_size_t` and `kan_file_offset_t` are for file coordinates,
///   they are 64 bit on every preset due to large files.
///
/// - `kan_instance_size_t` used to describe object counts and object sizes. Used in cases where it is unexpected to
///   have really large values. Therefore, 32 bit on most presets.
///
/// - `kan_memory_size_t` and `kan_memory_offset_t` correspond to unsigned and signed integers native to the preset,
///   that are able to reference the whole available memory on this platform.
///
/// - `kan_time_size_t` is type that safely holds time in nanoseconds on every platform and is unlikely to overflow.
///   Might be bigger that reasonable for performance on low end platforms.
///
/// - `kan_time_offset_t` is type for storing differences between `kan_time_size_t`. Can have lower precision than
///   `kan_time_size_t` as it does not need to store such big integers.
///
/// - `kan_packed_timer_t` is type for effectively storing time on platform, but may overflow after uptime of 20+ days
///   on low end platforms. Makes it possible to effectively pass time to different data structures inside Kan on
///   low end platforms without making data structure interface more difficult. It is advised to use it along with its
///   macros: `KAN_PACKED_TIMER_NEVER` stores value is considered to be unachievable, `KAN_PACKED_TIMER_SET` converts
///   `kan_time_size_t` to timer value and `KAN_PACKED_TIMER_IS_SAFE_TO_SET` checks whether it is safe to convert
///   `kan_time_size_t` into timer.
///
/// - `kan_loop_size_t` is advised type for loops and iterations. Using `kan_instance_size_t` can be fine on most
///   platforms, but `kan_loop_size_t` has hold platform-specific type if `kan_instance_size_t` is expected to be slower
///   that platform specific type.
///
/// - `kan_functor_user_data_t` stores type which is used for passing user data for different functor types.
///
/// - `kan_access_counter_t` is a specialized type for counting read-write accesses.
///
/// - `kan_floating_t` is a floating point type that is advised to be used on selected platform preset.
///
/// - `kan_serialized_size_t`, `kan_serialized_offset_t` and `kan_serialized_floating_t` are common types for data
///   that can be serialized to binary and should always be 32 bit.
/// \endparblock
///
/// \par Handles
/// \parblock
/// Handles are opaque pointers -- they point to something that belongs to private implementation and user can only
/// use them as arguments to public implementation functions.
///
/// Handles must be type-safe and code that works with them must be explicit in order to catch more errors on compile
/// time. Therefore, for each handle type new structure type with pointer inside is created and special set of macros
/// is advised to be used when working with handles.
/// \endparblock
///
/// \par Typed 32 bit ids
/// \parblock
/// Typed 32 bit ids are 32 bit unsigned integers that are packed into unique structure like handles. This provides
/// handle-like type security and makes it possible to detect type-related mistakes during compilation. For example,
/// it can be used to separate display ids from window ids and make sure they would never mix up.
/// \endparblock

KAN_C_HEADER_BEGIN

typedef uint8_t kan_bool_t;

#define KAN_FALSE 0u
#define KAN_TRUE 1u

/// \brief File coordinates are always 64 bit due to large file sizes.
typedef uint64_t kan_file_size_t;

/// \brief File coordinates are always 64 bit due to large file sizes.
typedef int64_t kan_file_offset_t;

typedef uint32_t kan_serialized_size_t;
typedef int32_t kan_serialized_offset_t;
typedef float kan_serialized_floating_t;

#if defined(KAN_CORE_TYPES_PRESET_X64)
typedef uint32_t kan_instance_size_t;

typedef uint64_t kan_memory_size_t;
typedef int64_t kan_memory_offset_t;

typedef uint64_t kan_time_size_t;
typedef uint64_t kan_time_offset_t;

typedef uint64_t kan_packed_timer_t;
#    define KAN_PACKED_TIMER_NEVER UINT64_MAX
#    define KAN_PACKED_TIMER_MAX (UINT64_MAX - 1u)
#    define KAN_PACKED_TIMER_SET(TIME_SIZE) ((TIME_SIZE))
#    define KAN_PACKED_TIMER_IS_SAFE_TO_SET(TIME_SIZE) ((TIME_SIZE) != UINT64_MAX)

typedef uint_fast32_t kan_loop_size_t;

typedef uint64_t kan_functor_user_data_t;

typedef int64_t kan_access_counter_t;

typedef double kan_floating_t;

#elif defined(KAN_CORE_TYPES_PRESET_X32)
typedef uint32_t kan_instance_size_t;

typedef uint32_t kan_memory_size_t;
typedef int32_t kan_memory_offset_t;

typedef uint64_t kan_time_size_t;
typedef uint32_t kan_time_offset_t;

typedef uint32_t kan_packed_timer_t;
#    define KAN_PACKED_TIMER_NEVER 0xFFFFFFFF
#    define KAN_PACKED_TIMER_MAX (0xFFFFFFFF - 1u)
#    define KAN_PACKED_TIMER_SET(TIME_SIZE) ((kan_packed_timer_t) ((TIME_SIZE) & 0x0007FFFFFFF00000) >> 20u)
#    define KAN_PACKED_TIMER_IS_SAFE_TO_SET(TIME_SIZE) (((TIME_SIZE) & 0xFFF8000000000000) == 0u)

typedef uint32_t kan_loop_size_t;

typedef uint32_t kan_functor_user_data_t;

typedef int32_t kan_access_counter_t;

typedef float kan_floating_t;

#else
#    error "Core types preset not selected."
#endif

#define KAN_INT_MIN(TYPE)                                                                                              \
    _Generic (*(TYPE *) NULL,                                                                                          \
        int8_t: INT8_MIN,                                                                                              \
        int16_t: INT16_MIN,                                                                                            \
        int32_t: INT32_MIN,                                                                                            \
        int64_t: INT64_MIN,                                                                                            \
        uint8_t: 0u,                                                                                                   \
        uint16_t: 0u,                                                                                                  \
        uint32_t: 0u,                                                                                                  \
        uint64_t: 0u,                                                                                                  \
        default: 0u)

#define KAN_INT_MAX(TYPE)                                                                                              \
    _Generic (*(TYPE *) NULL,                                                                                          \
        int8_t: INT8_MAX,                                                                                              \
        int16_t: INT16_MAX,                                                                                            \
        int32_t: INT32_MAX,                                                                                            \
        int64_t: INT64_MAX,                                                                                            \
        uint8_t: UINT8_MAX,                                                                                            \
        uint16_t: UINT16_MAX,                                                                                          \
        uint32_t: UINT32_MAX,                                                                                          \
        uint64_t: UINT64_MAX,                                                                                          \
        default: UINT32_MAX)

/// \brief Defines new handle type with given name.
#define KAN_HANDLE_DEFINE(NAME)                                                                                        \
    struct handle_struct_for_##NAME                                                                                    \
    {                                                                                                                  \
        void *handle_internals;                                                                                        \
    };                                                                                                                 \
    typedef struct handle_struct_for_##NAME NAME

/// \brief Checks whether given handle points to valid data.
#define KAN_HANDLE_IS_VALID(HANDLE) ((HANDLE).handle_internals != NULL)

/// \brief Checks whether two given handles are equal.
#define KAN_HANDLE_IS_EQUAL(FIRST, SECOND) ((FIRST).handle_internals == (SECOND).handle_internals)

/// \brief Acquires pointer stored inside handle.
#define KAN_HANDLE_GET(HANDLE) ((HANDLE).handle_internals)

/// \brief Converts given handle to the given handle type.
#define KAN_HANDLE_TRANSIT(TO_TYPE, FROM_HANDLE)                                                                       \
    ((TO_TYPE) {                                                                                                       \
        .handle_internals = (FROM_HANDLE).handle_internals,                                                            \
    })

/// \brief Creates handle with given pointer value.
#define KAN_HANDLE_SET(HANDLE_TYPE, POINTER)                                                                           \
    ((HANDLE_TYPE) {                                                                                                   \
        .handle_internals = (POINTER),                                                                                 \
    })

/// \brief Creates handle with invalid pointer value.
#define KAN_HANDLE_SET_INVALID(HANDLE_TYPE)                                                                            \
    ((HANDLE_TYPE) {                                                                                                   \
        .handle_internals = NULL,                                                                                      \
    })

/// \brief Used for initialization, compatible with static variables. Sets handle value to invalid state.
#define KAN_HANDLE_INITIALIZE_INVALID                                                                                  \
    {                                                                                                                  \
        NULL,                                                                                                          \
    }

/// \brief Convenience type for using along with typed id 32's without pasting uint32_t everywhere.
typedef uint32_t kan_id_32_t;

/// \brief Defines new id 32 type with given name.
#define KAN_TYPED_ID_32_DEFINE(NAME)                                                                                   \
    struct typed_id_32_struct_for_##NAME                                                                               \
    {                                                                                                                  \
        uint32_t typed_id_32_internals;                                                                                \
    };                                                                                                                 \
    typedef struct typed_id_32_struct_for_##NAME NAME

/// \brief Literal value that is used to mark invalid typed id 32 instances.
#define KAN_TYPED_ID_32_INVALID_LITERAL 0u

/// \brief Checks whether given id is valid. Zero ids are considered invalid.
#define KAN_TYPED_ID_32_IS_VALID(ID) ((ID).typed_id_32_internals != KAN_TYPED_ID_32_INVALID_LITERAL)

/// \brief Checks whether two given ids are equal.
#define KAN_TYPED_ID_32_IS_EQUAL(FIRST, SECOND) ((FIRST).typed_id_32_internals == (SECOND).typed_id_32_internals)

/// \brief Gets value of given typed id.
#define KAN_TYPED_ID_32_GET(ID) ((ID).typed_id_32_internals)

/// \brief Creates typed id with given value.
#define KAN_TYPED_ID_32_SET(TYPE, DATA)                                                                                \
    ((TYPE) {                                                                                                          \
        .typed_id_32_internals = (DATA),                                                                               \
    })

/// \brief Creates typed id with invalid value.
#define KAN_TYPED_ID_32_SET_INVALID(TYPE)                                                                              \
    ((TYPE) {                                                                                                          \
        .typed_id_32_internals = KAN_TYPED_ID_32_INVALID_LITERAL,                                                                                   \
    })

/// \brief Used for initialization, compatible with static variables. Sets typed id value to invalid state.
#define KAN_TYPED_ID_32_INITIALIZE_INVALID                                                                             \
    {                                                                                                                  \
        KAN_TYPED_ID_32_INVALID_LITERAL,                                                                                                            \
    }

KAN_C_HEADER_END
