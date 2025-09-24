#include <kan/universe/macro.h>
#include <kan/universe_transform/universe_transform.h>

#define TRANSFORM_COMPONENT_META(DIMENSIONS, DIMENSIONS_STRING)                                                        \
    KAN_REFLECTION_STRUCT_META (kan_transform_##DIMENSIONS##_component_t)                                              \
    UNIVERSE_TRANSFORM_API struct kan_repository_meta_automatic_cascade_deletion_t                                     \
        kan_transform_##DIMENSIONS##_component_hierarchy_cascade_deletion = {                                          \
            .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},      \
            .child_type_name = "kan_transform_" DIMENSIONS_STRING "_component_t",                                      \
            .child_key_path = {.reflection_path_length = 1u,                                                           \
                               .reflection_path = (const char *[]) {"parent_object_id"}},                              \
    }

TRANSFORM_COMPONENT_META (2, "2");
TRANSFORM_COMPONENT_META (3, "3");
#undef TRANSFORM_COMPONENT_META

#define TRANSFORM_COMPONENT_INIT(DIMENSIONS)                                                                           \
    void kan_transform_##DIMENSIONS##_component_init (struct kan_transform_##DIMENSIONS##_component_t *instance)       \
    {                                                                                                                  \
        instance->object_id = KAN_TYPED_ID_32_SET_INVALID (kan_universe_object_id_t);                                  \
        instance->parent_object_id = KAN_TYPED_ID_32_SET_INVALID (kan_universe_object_id_t);                           \
                                                                                                                       \
        instance->global_lock = kan_atomic_int_init (0);                                                               \
        instance->global = kan_transform_##DIMENSIONS##_get_identity ();                                               \
        instance->local = kan_transform_##DIMENSIONS##_get_identity ();                                                \
        instance->global_dirty = true;                                                                                 \
    }

TRANSFORM_COMPONENT_INIT (2)
TRANSFORM_COMPONENT_INIT (3)
#undef TRANSFORM_COMPONENT_INIT

#define TRANSFORM_INVALIDATOR_FUNCTION(TRANSFORM_DIMENSION)                                                            \
    static void kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_global (                                     \
        struct kan_transform_##TRANSFORM_DIMENSION##_queries_t *queries,                                               \
        const struct kan_transform_##TRANSFORM_DIMENSION##_component_t *component)                                     \
    {                                                                                                                  \
        KAN_UM_BIND_STATE_FIELDLESS (kan_transform_##TRANSFORM_DIMENSION##_queries_t, queries)                         \
        KAN_UML_VALUE_READ (child_component, kan_transform_##TRANSFORM_DIMENSION##_component_t, parent_object_id,      \
                            &component->object_id)                                                                     \
        {                                                                                                              \
            struct kan_transform_##TRANSFORM_DIMENSION##_component_t *mutable_child_component =                        \
                (struct kan_transform_##TRANSFORM_DIMENSION##_component_t *) child_component;                          \
                                                                                                                       \
            KAN_ATOMIC_INT_SCOPED_LOCK (&mutable_child_component->global_lock)                                         \
            mutable_child_component->global_dirty = true;                                                              \
            kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_global (queries, mutable_child_component);       \
        }                                                                                                              \
    }

TRANSFORM_INVALIDATOR_FUNCTION (2)
TRANSFORM_INVALIDATOR_FUNCTION (3)
#undef TRANSFORM_INVALIDATOR_FUNCTION

#define TRANSFORM_SET_PARENT_OBJECT_ID(TRANSFORM_DIMENSION)                                                            \
    void kan_transform_##TRANSFORM_DIMENSION##_component_set_parent_object_id (                                        \
        struct kan_transform_##TRANSFORM_DIMENSION##_queries_t *queries,                                               \
        struct kan_transform_##TRANSFORM_DIMENSION##_component_t *component,                                           \
        kan_universe_object_id_t parent_object_id)                                                                     \
    {                                                                                                                  \
        component->parent_object_id = parent_object_id;                                                                \
        kan_atomic_int_lock (&component->global_lock);                                                                 \
        component->global_dirty = true;                                                                                \
        kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_global (queries, component);                         \
        kan_atomic_int_unlock (&component->global_lock);                                                               \
    }

TRANSFORM_SET_PARENT_OBJECT_ID (2)
TRANSFORM_SET_PARENT_OBJECT_ID (3)
#undef TRANSFORM_SET_PARENT_OBJECT_ID

#define TRANSFORM_GET_GLOBAL(TRANSFORM_DIMENSION, MATRIX_DIMENSION, MULTIPLIER)                                        \
    struct kan_transform_##TRANSFORM_DIMENSION##_t kan_transform_##TRANSFORM_DIMENSION##_component_get_global (        \
        struct kan_transform_##TRANSFORM_DIMENSION##_queries_t *queries,                                               \
        const struct kan_transform_##TRANSFORM_DIMENSION##_component_t *component)                                     \
    {                                                                                                                  \
        KAN_UM_BIND_STATE_FIELDLESS (kan_transform_##TRANSFORM_DIMENSION##_queries_t, queries)                         \
        if (!KAN_TYPED_ID_32_IS_VALID (component->parent_object_id))                                                   \
        {                                                                                                              \
            return component->local;                                                                                   \
        }                                                                                                              \
                                                                                                                       \
        struct kan_transform_##TRANSFORM_DIMENSION##_component_t *mutable_component =                                  \
            (struct kan_transform_##TRANSFORM_DIMENSION##_component_t *) component;                                    \
        KAN_ATOMIC_INT_SCOPED_LOCK (&mutable_component->global_lock)                                                   \
                                                                                                                       \
        if (!mutable_component->global_dirty)                                                                          \
        {                                                                                                              \
            return mutable_component->global;                                                                          \
        }                                                                                                              \
                                                                                                                       \
        KAN_UMI_VALUE_READ_REQUIRED (parent_component, kan_transform_##TRANSFORM_DIMENSION##_component_t, object_id,   \
                                     &component->parent_object_id)                                                     \
                                                                                                                       \
        struct kan_transform_##TRANSFORM_DIMENSION##_t parent_transform =                                              \
            kan_transform_##TRANSFORM_DIMENSION##_component_get_global (queries, parent_component);                    \
                                                                                                                       \
        struct kan_float_matrix_##MATRIX_DIMENSION##_t parent_matrix =                                                 \
            kan_transform_##TRANSFORM_DIMENSION##_to_float_matrix_##MATRIX_DIMENSION (&parent_transform);              \
                                                                                                                       \
        struct kan_float_matrix_##MATRIX_DIMENSION##_t local_matrix =                                                  \
            kan_transform_##TRANSFORM_DIMENSION##_to_float_matrix_##MATRIX_DIMENSION (&component->local);              \
        struct kan_float_matrix_##MATRIX_DIMENSION##_t result_matrix = MULTIPLIER (&parent_matrix, &local_matrix);     \
                                                                                                                       \
        mutable_component->global =                                                                                    \
            kan_float_matrix_##MATRIX_DIMENSION##_to_transform_##TRANSFORM_DIMENSION (&result_matrix);                 \
        mutable_component->global_dirty = false;                                                                       \
        return mutable_component->global;                                                                              \
    }

TRANSFORM_GET_GLOBAL (2, 3x3, kan_float_matrix_3x3_multiply)
TRANSFORM_GET_GLOBAL (3, 4x4, kan_float_matrix_4x4_multiply_for_transform)
#undef TRANSFORM_GET_GLOBAL

#define TRANSFORM_SET_LOCAL(TRANSFORM_DIMENSION)                                                                       \
    void kan_transform_##TRANSFORM_DIMENSION##_component_set_local (                                                   \
        struct kan_transform_##TRANSFORM_DIMENSION##_queries_t *queries,                                               \
        struct kan_transform_##TRANSFORM_DIMENSION##_component_t *component,                                           \
        const struct kan_transform_##TRANSFORM_DIMENSION##_t *new_transform)                                           \
    {                                                                                                                  \
        component->local = *new_transform;                                                                             \
        kan_atomic_int_lock (&component->global_lock);                                                                 \
        component->global_dirty = true;                                                                                \
        kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_global (queries, component);                         \
        kan_atomic_int_unlock (&component->global_lock);                                                               \
    }

TRANSFORM_SET_LOCAL (2)
TRANSFORM_SET_LOCAL (3)
#undef TRANSFORM_SET_LOCAL

#define TRANSFORM_SET_GLOBAL(TRANSFORM_DIMENSION, MATRIX_DIMENSION, MULTIPLIER)                                        \
    void kan_transform_##TRANSFORM_DIMENSION##_component_set_global (                                                  \
        struct kan_transform_##TRANSFORM_DIMENSION##_queries_t *queries,                                               \
        struct kan_transform_##TRANSFORM_DIMENSION##_component_t *component,                                           \
        const struct kan_transform_##TRANSFORM_DIMENSION##_t *new_transform)                                           \
    {                                                                                                                  \
        KAN_UM_BIND_STATE_FIELDLESS (kan_transform_##TRANSFORM_DIMENSION##_queries_t, queries)                         \
        if (!KAN_TYPED_ID_32_IS_VALID (component->parent_object_id))                                                   \
        {                                                                                                              \
            kan_transform_##TRANSFORM_DIMENSION##_component_set_local (queries, component, new_transform);             \
            return;                                                                                                    \
        }                                                                                                              \
                                                                                                                       \
        KAN_ATOMIC_INT_SCOPED_LOCK (&component->global_lock)                                                           \
        KAN_UMI_VALUE_READ_REQUIRED (parent_component, kan_transform_##TRANSFORM_DIMENSION##_component_t, object_id,   \
                                     &component->parent_object_id)                                                     \
                                                                                                                       \
        struct kan_transform_##TRANSFORM_DIMENSION##_t parent_transform =                                              \
            kan_transform_##TRANSFORM_DIMENSION##_component_get_global (queries, parent_component);                    \
                                                                                                                       \
        struct kan_float_matrix_##MATRIX_DIMENSION##_t parent_matrix =                                                 \
            kan_transform_##TRANSFORM_DIMENSION##_to_float_matrix_##MATRIX_DIMENSION (&parent_transform);              \
                                                                                                                       \
        struct kan_float_matrix_##MATRIX_DIMENSION##_t parent_matrix_inverse =                                         \
            kan_float_matrix_##MATRIX_DIMENSION##_inverse (&parent_matrix);                                            \
                                                                                                                       \
        struct kan_float_matrix_##MATRIX_DIMENSION##_t global_matrix =                                                 \
            kan_transform_##TRANSFORM_DIMENSION##_to_float_matrix_##MATRIX_DIMENSION (new_transform);                  \
                                                                                                                       \
        struct kan_float_matrix_##MATRIX_DIMENSION##_t result_local_matrix =                                           \
            MULTIPLIER (&parent_matrix_inverse, &global_matrix);                                                       \
                                                                                                                       \
        component->local =                                                                                             \
            kan_float_matrix_##MATRIX_DIMENSION##_to_transform_##TRANSFORM_DIMENSION (&result_local_matrix);           \
                                                                                                                       \
        component->global_dirty = false;                                                                               \
        component->global = *new_transform;                                                                            \
        kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_global (queries, component);                         \
    }

TRANSFORM_SET_GLOBAL (2, 3x3, kan_float_matrix_3x3_multiply)
TRANSFORM_SET_GLOBAL (3, 4x4, kan_float_matrix_4x4_multiply_for_transform)

#undef VISUAL_TRANSFORM_SYNC_CALCULATE_MUTATOR
