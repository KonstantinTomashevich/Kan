#pragma once

#include <universe_object_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/reflection/markup.h>
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

KAN_TYPED_ID_32_DEFINE (kan_universe_object_id_t);

/// \brief Singleton that stores id generation data.
struct kan_object_id_generator_singleton_t
{
    struct kan_atomic_int_t counter;

    kan_instance_size_t stub;
};

UNIVERSE_OBJECT_API void kan_object_id_generator_singleton_init (struct kan_object_id_generator_singleton_t *instance);

/// \brief Generates new object id using singleton data.
static inline kan_universe_object_id_t kan_universe_object_id_generate (
    const struct kan_object_id_generator_singleton_t *singleton)
{
    return KAN_TYPED_ID_32_SET (
        kan_universe_object_id_t,
        (kan_id_32_t) kan_atomic_int_add (&((struct kan_object_id_generator_singleton_t *) singleton)->counter, 1));
}

KAN_C_HEADER_END
