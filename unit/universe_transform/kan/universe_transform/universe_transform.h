#pragma once

#include <universe_transform_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/inline_math/inline_math.h>
#include <kan/threading/atomic.h>
#include <kan/universe/universe.h>
#include <kan/universe_object/universe_object.h>

/// \file
/// \brief Provides components with 2d and 3d transform hierarchies for universe objects
///        and functions to work with them.
///
/// \par Definition
/// \parblock
/// Transform components utilize their own query storage for requesting other transform components from their utility
/// functions, which is crucial to properly move through the hierarchies. Also, it utilizes custom lock for properly
/// updating global transform cache without requiring update/write access to do so.
/// \endparblock

KAN_C_HEADER_BEGIN

#define KAN_TRANSFORM_INTERFACE(DIMENSIONS)                                                                            \
    struct kan_transform_##DIMENSIONS##_component_t                                                                    \
    {                                                                                                                  \
        kan_universe_object_id_t object_id;                                                                            \
        kan_universe_object_id_t parent_object_id;                                                                     \
                                                                                                                       \
        struct kan_atomic_int_t global_lock;                                                                           \
        struct kan_transform_##DIMENSIONS##_t global;                                                                  \
        struct kan_transform_##DIMENSIONS##_t local;                                                                   \
        bool global_dirty;                                                                                             \
    };                                                                                                                 \
                                                                                                                       \
    UNIVERSE_TRANSFORM_API void kan_transform_##DIMENSIONS##_component_init (                                          \
        struct kan_transform_##DIMENSIONS##_component_t *instance);                                                    \
                                                                                                                       \
    /** \brief Mutator queries needed for helper functions for working with transform. */                              \
    struct kan_transform_##DIMENSIONS##_queries_t                                                                      \
    {                                                                                                                  \
        struct kan_repository_indexed_value_read_query_t                                                               \
            read_value__kan_transform_##DIMENSIONS##_component_t__object_id;                                           \
        struct kan_repository_indexed_value_read_query_t                                                               \
            read_value__kan_transform_##DIMENSIONS##_component_t__parent_object_id;                                    \
    };                                                                                                                 \
                                                                                                                       \
    /** \brief Updates parent object id and invalidates global transforms across hierarchy. */                         \
    UNIVERSE_TRANSFORM_API void kan_transform_##DIMENSIONS##_component_set_parent_object_id (                          \
        struct kan_transform_##DIMENSIONS##_queries_t *queries,                                                        \
        struct kan_transform_##DIMENSIONS##_component_t *component, kan_universe_object_id_t parent_object_id);        \
                                                                                                                       \
    /** Just a helper to illustrate that local transform can be directly accessed without locks. */                    \
    static inline struct kan_transform_##DIMENSIONS##_t kan_transform_##DIMENSIONS##_component_get_local (             \
        const struct kan_transform_##DIMENSIONS##_component_t *component)                                              \
    {                                                                                                                  \
        return component->local;                                                                                       \
    }                                                                                                                  \
                                                                                                                       \
    /** \brief Queries component global transform updating it if it is dirty. */                                       \
    UNIVERSE_TRANSFORM_API struct kan_transform_##DIMENSIONS##_t kan_transform_##DIMENSIONS##_component_get_global (   \
        struct kan_transform_##DIMENSIONS##_queries_t *queries,                                                        \
        const struct kan_transform_##DIMENSIONS##_component_t *component);                                             \
                                                                                                                       \
    /** \brief Sets component local transform invalidating children global transforms. */                              \
    UNIVERSE_TRANSFORM_API void kan_transform_##DIMENSIONS##_component_set_local (                                     \
        struct kan_transform_##DIMENSIONS##_queries_t *queries,                                                        \
        struct kan_transform_##DIMENSIONS##_component_t *component,                                                    \
        const struct kan_transform_##DIMENSIONS##_t *new_transform);                                                   \
                                                                                                                       \
    /** \brief Sets component global transform directly invalidating children global transforms. */                    \
    UNIVERSE_TRANSFORM_API void kan_transform_##DIMENSIONS##_component_set_global (                                    \
        struct kan_transform_##DIMENSIONS##_queries_t *queries,                                                        \
        struct kan_transform_##DIMENSIONS##_component_t *component,                                                    \
        const struct kan_transform_##DIMENSIONS##_t *new_transform)

KAN_TRANSFORM_INTERFACE (2);
KAN_TRANSFORM_INTERFACE (3);
#undef KAN_TRANSFORM_INTERFACE

KAN_C_HEADER_END
