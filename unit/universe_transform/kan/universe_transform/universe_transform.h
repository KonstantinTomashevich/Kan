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
///        and utilities to work with them.
///
/// \par Definition
/// \parblock
/// Provided components have double-transform system: logical transform and visual transform. It makes it possible
/// to keep render transformations smooth when logical update rate is much lower than visual update rate by utilizing
/// linear interpolation in transform visual sync mutators. Transform visual sync mutators automatically keep visual
/// transform in sync with logical transform changes.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Name of the mutator group that adds transform visual sync to 2d transform components.
/// \details These mutators provide automatic sync between logical and visual transforms along with visual transform
///          linear interpolation.
#define KAN_TRANSFORM_VISUAL_SYNC_2_MUTATOR_GROUP "transform_visual_sync_2"

/// \brief Name of the mutator group that adds transform visual sync to 3d transform components.
/// \details These mutators provide automatic sync between logical and visual transforms along with visual transform
///          linear interpolation.
#define KAN_TRANSFORM_VISUAL_SYNC_3_MUTATOR_GROUP "transform_visual_sync_3"

/// \brief Transform visual sync is started after this checkpoint.
#define KAN_TRANSFORM_VISUAL_SYNC_BEGIN_CHECKPOINT "transform_visual_sync_begin"

/// \brief Transform visual sync is finished before this checkpoint.
#define KAN_TRANSFORM_VISUAL_SYNC_END_CHECKPOINT "transform_visual_sync_end"

#define KAN_TRANSFORM_INTERFACE(DIMENSIONS)                                                                            \
    struct kan_transform_##DIMENSIONS##_component_t                                                                    \
    {                                                                                                                  \
        kan_universe_object_id_t object_id;                                                                            \
        kan_universe_object_id_t parent_object_id;                                                                     \
                                                                                                                       \
        struct kan_transform_##DIMENSIONS##_t logical_local;                                                           \
        kan_time_size_t logical_local_time_ns;                                                                         \
        kan_bool_t visual_sync_needed;                                                                                 \
        kan_bool_t visual_synced_at_least_once;                                                                        \
        struct kan_transform_##DIMENSIONS##_t visual_local;                                                            \
                                                                                                                       \
        KAN_REFLECTION_IGNORE                                                                                          \
        struct kan_atomic_int_t logical_global_lock;                                                                   \
                                                                                                                       \
        kan_bool_t logical_global_dirty;                                                                               \
        struct kan_transform_##DIMENSIONS##_t logical_global;                                                          \
                                                                                                                       \
        KAN_REFLECTION_IGNORE                                                                                          \
        struct kan_atomic_int_t visual_global_lock;                                                                    \
                                                                                                                       \
        kan_bool_t visual_global_dirty;                                                                                \
        struct kan_transform_##DIMENSIONS##_t visual_global;                                                           \
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
    UNIVERSE_TRANSFORM_API void kan_transform_##DIMENSIONS##_set_parent_object_id (                                    \
        struct kan_transform_##DIMENSIONS##_queries_t *queries,                                                        \
        struct kan_transform_##DIMENSIONS##_component_t *component, kan_universe_object_id_t parent_object_id);        \
                                                                                                                       \
    /** \brief Queries component logical global transform updating it if it is dirty. */                               \
    UNIVERSE_TRANSFORM_API void kan_transform_##DIMENSIONS##_get_logical_global (                                      \
        struct kan_transform_##DIMENSIONS##_queries_t *queries,                                                        \
        const struct kan_transform_##DIMENSIONS##_component_t *component,                                              \
        struct kan_transform_##DIMENSIONS##_t *output);                                                                \
                                                                                                                       \
    /** \brief Queries component visual global transform updating it if it is dirty. */                                \
    UNIVERSE_TRANSFORM_API void kan_transform_##DIMENSIONS##_get_visual_global (                                       \
        struct kan_transform_##DIMENSIONS##_queries_t *queries,                                                        \
        const struct kan_transform_##DIMENSIONS##_component_t *component,                                              \
        struct kan_transform_##DIMENSIONS##_t *output);                                                                \
                                                                                                                       \
    /**                                                                                                                \
     * \brief Sets component logical local transform invalidating logical global transforms across hierarchy and       \
     *        requesting visual transform sync.                                                                        \
     */                                                                                                                \
    UNIVERSE_TRANSFORM_API void kan_transform_##DIMENSIONS##_set_logical_local (                                       \
        struct kan_transform_##DIMENSIONS##_queries_t *queries,                                                        \
        struct kan_transform_##DIMENSIONS##_component_t *component,                                                    \
        const struct kan_transform_##DIMENSIONS##_t *new_transform, kan_time_size_t transform_logical_time_ns);        \
                                                                                                                       \
    /** \brief Sets component visual local transform invalidating visual global transforms across hierarchy. */        \
    UNIVERSE_TRANSFORM_API void kan_transform_##DIMENSIONS##_set_visual_local (                                        \
        struct kan_transform_##DIMENSIONS##_queries_t *queries,                                                        \
        struct kan_transform_##DIMENSIONS##_component_t *component,                                                    \
        const struct kan_transform_##DIMENSIONS##_t *new_transform);                                                   \
                                                                                                                       \
    /**                                                                                                                \
     * \brief Sets component logical global transform invalidating logical global transforms across hierarchy and      \
     *        requesting visual transform sync.                                                                        \
     */                                                                                                                \
    UNIVERSE_TRANSFORM_API void kan_transform_##DIMENSIONS##_set_logical_global (                                      \
        struct kan_transform_##DIMENSIONS##_queries_t *queries,                                                        \
        struct kan_transform_##DIMENSIONS##_component_t *component,                                                    \
        const struct kan_transform_##DIMENSIONS##_t *new_transform, kan_time_size_t transform_logical_time_ns);        \
                                                                                                                       \
    /** \brief Sets component visual global transform invalidating visual global transforms across hierarchy. */       \
    UNIVERSE_TRANSFORM_API void kan_transform_##DIMENSIONS##_set_visual_global (                                       \
        struct kan_transform_##DIMENSIONS##_queries_t *queries,                                                        \
        struct kan_transform_##DIMENSIONS##_component_t *component,                                                    \
        const struct kan_transform_##DIMENSIONS##_t *new_transform)

KAN_TRANSFORM_INTERFACE (2);
KAN_TRANSFORM_INTERFACE (3);
#undef KAN_TRANSFORM_INTERFACE

KAN_C_HEADER_END
