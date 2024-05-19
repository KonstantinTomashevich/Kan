#pragma once

#include <universe_object_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/threading/atomic.h>

/// \file
/// \brief Defines object id type for universe and object id generator.
///
/// \par Definition
/// \parblock
/// Object ids are used in different contexts across different units, therefore this unit unites concept of object id
/// and its generation.
/// \endparblock

KAN_C_HEADER_BEGIN

typedef uint64_t kan_universe_object_id_t;

#define KAN_INVALID_UNIVERSE_OBJECT_ID 0u

/// \brief Singleton that stores id generation data.
struct kan_object_id_generator_singleton_t
{
    /// \meta reflection_ignore_struct_field
    struct kan_atomic_int_t counter;

    uint64_t stub;
};

UNIVERSE_OBJECT_API void kan_object_id_generator_singleton_init (struct kan_object_id_generator_singleton_t *instance);

/// \brief Generates new object id using singleton data.
static inline kan_universe_object_id_t kan_universe_object_id_generate (
    const struct kan_object_id_generator_singleton_t *singleton)
{
    return (kan_universe_object_id_t) kan_atomic_int_add (
        &((struct kan_object_id_generator_singleton_t *) singleton)->counter, 1);
}

KAN_C_HEADER_END
