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

/// \brief Defines new id 32 type with given name.
#define KAN_TYPED_ID_32_DEFINE(NAME)                                                                                   \
    struct typed_id_32_struct_for_##NAME                                                                               \
    {                                                                                                                  \
        uint32_t typed_id_32_internals;                                                                                \
    };                                                                                                                 \
    typedef struct typed_id_32_struct_for_##NAME NAME

/// \brief Checks whether given id is valid. Zero ids are considered invalid.
#define KAN_TYPED_ID_32_IS_VALID(ID) ((ID).typed_id_32_internals != 0u)

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
        .typed_id_32_internals = 0u,                                                                                   \
    })

/// \brief Used for initialization, compatible with static variables. Sets typed id value to invalid state.
#define KAN_TYPED_ID_32_INITIALIZE_INVALID                                                                             \
    {                                                                                                                  \
        0u,                                                                                                            \
    }

KAN_C_HEADER_END
