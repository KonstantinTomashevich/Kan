#include <kan/container/stack_group_allocator.h>
#include <kan/log/logging.h>
#include <kan/universe/macro.h>
#include <kan/universe_time/universe_time.h>
#include <kan/universe_transform/universe_transform.h>

// We're not using universe preprocessor here as we're utilizing macros to write the same logic for 2d and 3d.
// However, the might be changed in the future.

KAN_LOG_DEFINE_CATEGORY (universe_transform);

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
        instance->logical_local = kan_transform_##DIMENSIONS##_get_identity ();                                        \
        instance->logical_local_time_ns = 0u;                                                                          \
        /* For components that were instanced and filled through reflection automatically. */                          \
        instance->visual_sync_needed = true;                                                                           \
        instance->visual_synced_at_least_once = false;                                                                 \
        instance->visual_local = kan_transform_##DIMENSIONS##_get_identity ();                                         \
                                                                                                                       \
        instance->logical_global_lock = kan_atomic_int_init (0);                                                       \
        instance->logical_global_dirty = true;                                                                         \
        instance->logical_global = kan_transform_##DIMENSIONS##_get_identity ();                                       \
                                                                                                                       \
        instance->visual_global_lock = kan_atomic_int_init (0);                                                        \
        instance->visual_global_dirty = true;                                                                          \
        instance->visual_global = kan_transform_##DIMENSIONS##_get_identity ();                                        \
    }

TRANSFORM_COMPONENT_INIT (2)
TRANSFORM_COMPONENT_INIT (3)
#undef TRANSFORM_COMPONENT_INIT

KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP (visual_transform_sync_2_invalidate)
KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP (visual_transform_sync_2_calculate)
UNIVERSE_TRANSFORM_API KAN_UM_MUTATOR_GROUP_META (visual_transform_sync_2_calculate,
                                                  KAN_TRANSFORM_VISUAL_SYNC_2_MUTATOR_GROUP);

KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP (visual_transform_sync_3_invalidate)
KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP (visual_transform_sync_3_calculate)
UNIVERSE_TRANSFORM_API KAN_UM_MUTATOR_GROUP_META (visual_transform_sync_3_calculate,
                                                  KAN_TRANSFORM_VISUAL_SYNC_3_MUTATOR_GROUP);

#define TRANSFORM_INVALIDATOR_FUNCTION(TRANSFORM_TYPE, TRANSFORM_DIMENSION)                                            \
    static void kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_##TRANSFORM_TYPE##_global (                  \
        struct kan_transform_##TRANSFORM_DIMENSION##_queries_t *queries,                                               \
        const struct kan_transform_##TRANSFORM_DIMENSION##_component_t *component)                                     \
    {                                                                                                                  \
        KAN_UM_BIND_STATE_FIELDLESS (kan_transform_##TRANSFORM_DIMENSION##_queries_t, queries)                         \
        KAN_UML_VALUE_READ (child_component, kan_transform_##TRANSFORM_DIMENSION##_component_t, parent_object_id,      \
                            &component->object_id)                                                                     \
        {                                                                                                              \
            struct kan_transform_##TRANSFORM_DIMENSION##_component_t *mutable_child_component =                        \
                (struct kan_transform_##TRANSFORM_DIMENSION##_component_t *) child_component;                          \
            kan_atomic_int_lock (&mutable_child_component->TRANSFORM_TYPE##_global_lock);                              \
            mutable_child_component->TRANSFORM_TYPE##_global_dirty = 1u;                                               \
            kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_##TRANSFORM_TYPE##_global (                      \
                queries, mutable_child_component);                                                                     \
            kan_atomic_int_unlock (&mutable_child_component->TRANSFORM_TYPE##_global_lock);                            \
        }                                                                                                              \
    }

TRANSFORM_INVALIDATOR_FUNCTION (logical, 2)
TRANSFORM_INVALIDATOR_FUNCTION (visual, 2)
TRANSFORM_INVALIDATOR_FUNCTION (logical, 3)
TRANSFORM_INVALIDATOR_FUNCTION (visual, 3)
#undef TRANSFORM_INVALIDATOR_FUNCTION

#define TRANSFORM_SET_PARENT_OBJECT_ID(TRANSFORM_DIMENSION)                                                            \
    void kan_transform_##TRANSFORM_DIMENSION##_set_parent_object_id (                                                  \
        struct kan_transform_##TRANSFORM_DIMENSION##_queries_t *queries,                                               \
        struct kan_transform_##TRANSFORM_DIMENSION##_component_t *component,                                           \
        kan_universe_object_id_t parent_object_id)                                                                     \
    {                                                                                                                  \
        component->parent_object_id = parent_object_id;                                                                \
        kan_atomic_int_lock (&component->logical_global_lock);                                                         \
        component->logical_global_dirty = true;                                                                        \
        kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_logical_global (queries, component);                 \
        kan_atomic_int_unlock (&component->logical_global_lock);                                                       \
                                                                                                                       \
        kan_atomic_int_lock (&component->visual_global_lock);                                                          \
        component->visual_global_dirty = true;                                                                         \
        kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_visual_global (queries, component);                  \
        kan_atomic_int_unlock (&component->visual_global_lock);                                                        \
    }

TRANSFORM_SET_PARENT_OBJECT_ID (2)
TRANSFORM_SET_PARENT_OBJECT_ID (3)
#undef TRANSFORM_SET_PARENT_OBJECT_ID

#define TRANSFORM_GET_GLOBAL(TRANSFORM_TYPE, TRANSFORM_DIMENSION, MATRIX_DIMENSION, MULTIPLIER)                        \
    void kan_transform_##TRANSFORM_DIMENSION##_get_##TRANSFORM_TYPE##_global (                                         \
        struct kan_transform_##TRANSFORM_DIMENSION##_queries_t *queries,                                               \
        const struct kan_transform_##TRANSFORM_DIMENSION##_component_t *component,                                     \
        struct kan_transform_##TRANSFORM_DIMENSION##_t *output)                                                        \
    {                                                                                                                  \
        KAN_UM_BIND_STATE_FIELDLESS (kan_transform_##TRANSFORM_DIMENSION##_queries_t, queries)                         \
        if (!KAN_TYPED_ID_32_IS_VALID (component->parent_object_id))                                                   \
        {                                                                                                              \
            *output = component->TRANSFORM_TYPE##_local;                                                               \
            return;                                                                                                    \
        }                                                                                                              \
                                                                                                                       \
        struct kan_transform_##TRANSFORM_DIMENSION##_component_t *mutable_component =                                  \
            (struct kan_transform_##TRANSFORM_DIMENSION##_component_t *) component;                                    \
        kan_atomic_int_lock (&mutable_component->TRANSFORM_TYPE##_global_lock);                                        \
                                                                                                                       \
        if (!mutable_component->TRANSFORM_TYPE##_global_dirty)                                                         \
        {                                                                                                              \
            *output = mutable_component->TRANSFORM_TYPE##_global;                                                      \
            kan_atomic_int_unlock (&mutable_component->TRANSFORM_TYPE##_global_lock);                                  \
            return;                                                                                                    \
        }                                                                                                              \
                                                                                                                       \
        KAN_UMI_VALUE_READ_REQUIRED (parent_component, kan_transform_##TRANSFORM_DIMENSION##_component_t, object_id,   \
                                     &component->parent_object_id)                                                     \
                                                                                                                       \
        struct kan_transform_##TRANSFORM_DIMENSION##_t parent_transform;                                               \
        kan_transform_##TRANSFORM_DIMENSION##_get_##TRANSFORM_TYPE##_global (queries, parent_component,                \
                                                                             &parent_transform);                       \
        struct kan_float_matrix_##MATRIX_DIMENSION##_t parent_matrix =                                                 \
            kan_transform_##TRANSFORM_DIMENSION##_to_float_matrix_##MATRIX_DIMENSION (&parent_transform);              \
                                                                                                                       \
        struct kan_float_matrix_##MATRIX_DIMENSION##_t local_matrix =                                                  \
            kan_transform_##TRANSFORM_DIMENSION##_to_float_matrix_##MATRIX_DIMENSION (                                 \
                &component->TRANSFORM_TYPE##_local);                                                                   \
        struct kan_float_matrix_##MATRIX_DIMENSION##_t result_matrix = MULTIPLIER (&parent_matrix, &local_matrix);     \
                                                                                                                       \
        mutable_component->TRANSFORM_TYPE##_global =                                                                   \
            kan_float_matrix_##MATRIX_DIMENSION##_to_transform_##TRANSFORM_DIMENSION (&result_matrix);                 \
        *output = mutable_component->TRANSFORM_TYPE##_global;                                                          \
        mutable_component->TRANSFORM_TYPE##_global_dirty = false;                                                      \
        kan_atomic_int_unlock (&mutable_component->TRANSFORM_TYPE##_global_lock);                                      \
    }

TRANSFORM_GET_GLOBAL (logical, 2, 3x3, kan_float_matrix_3x3_multiply)
TRANSFORM_GET_GLOBAL (visual, 2, 3x3, kan_float_matrix_3x3_multiply)
TRANSFORM_GET_GLOBAL (logical, 3, 4x4, kan_float_matrix_4x4_multiply_for_transform)
TRANSFORM_GET_GLOBAL (visual, 3, 4x4, kan_float_matrix_4x4_multiply_for_transform)
#undef TRANSFORM_GET_GLOBAL

#define TRANSFORM_SET_LOGICAL_LOCAL(TRANSFORM_DIMENSION)                                                               \
    void kan_transform_##TRANSFORM_DIMENSION##_set_logical_local (                                                     \
        struct kan_transform_##TRANSFORM_DIMENSION##_queries_t *queries,                                               \
        struct kan_transform_##TRANSFORM_DIMENSION##_component_t *component,                                           \
        const struct kan_transform_##TRANSFORM_DIMENSION##_t *new_transform,                                           \
        kan_time_size_t transform_logical_time_ns)                                                                     \
    {                                                                                                                  \
        component->logical_local = *new_transform;                                                                     \
        component->logical_local_time_ns = transform_logical_time_ns;                                                  \
        component->visual_sync_needed = true;                                                                          \
                                                                                                                       \
        kan_atomic_int_lock (&component->logical_global_lock);                                                         \
        component->logical_global_dirty = true;                                                                        \
        kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_logical_global (queries, component);                 \
        kan_atomic_int_unlock (&component->logical_global_lock);                                                       \
    }

TRANSFORM_SET_LOGICAL_LOCAL (2)
TRANSFORM_SET_LOGICAL_LOCAL (3)
#undef TRANSFORM_SET_LOGICAL_LOCAL

#define TRANSFORM_SET_VISUAL_LOCAL(TRANSFORM_DIMENSION)                                                                \
    void kan_transform_##TRANSFORM_DIMENSION##_set_visual_local (                                                      \
        struct kan_transform_##TRANSFORM_DIMENSION##_queries_t *queries,                                               \
        struct kan_transform_##TRANSFORM_DIMENSION##_component_t *component,                                           \
        const struct kan_transform_##TRANSFORM_DIMENSION##_t *new_transform)                                           \
    {                                                                                                                  \
        component->visual_local = *new_transform;                                                                      \
        kan_atomic_int_lock (&component->visual_global_lock);                                                          \
        component->visual_global_dirty = true;                                                                         \
        kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_visual_global (queries, component);                  \
        kan_atomic_int_unlock (&component->visual_global_lock);                                                        \
    }

TRANSFORM_SET_VISUAL_LOCAL (2)
TRANSFORM_SET_VISUAL_LOCAL (3)
#undef TRANSFORM_SET_VISUAL_LOCAL

void kan_transform_2_set_logical_global (struct kan_transform_2_queries_t *queries,
                                         struct kan_transform_2_component_t *component,
                                         const struct kan_transform_2_t *new_transform,
                                         kan_time_size_t transform_logical_time_ns)
{
#define TRANSFORM_SET_GLOBAL(TRANSFORM_TYPE, TRANSFORM_DIMENSION, MATRIX_DIMENSION, MULTIPLIER, ADDITIONAL_SETTER,     \
                             ...)                                                                                      \
    KAN_UM_BIND_STATE_FIELDLESS (kan_transform_##TRANSFORM_DIMENSION##_queries_t, queries)                             \
    if (!KAN_TYPED_ID_32_IS_VALID (component->parent_object_id))                                                       \
    {                                                                                                                  \
        kan_transform_##TRANSFORM_DIMENSION##_set_##TRANSFORM_TYPE##_local (queries, component,                        \
                                                                            new_transform __VA_ARGS__);                \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        kan_atomic_int_lock (&component->TRANSFORM_TYPE##_global_lock);                                                \
        KAN_UMI_VALUE_READ_REQUIRED (parent_component, kan_transform_##TRANSFORM_DIMENSION##_component_t, object_id,   \
                                     &component->parent_object_id)                                                     \
                                                                                                                       \
        struct kan_transform_##TRANSFORM_DIMENSION##_t parent_transform;                                               \
        kan_transform_##TRANSFORM_DIMENSION##_get_##TRANSFORM_TYPE##_global (queries, parent_component,                \
                                                                             &parent_transform);                       \
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
        component->TRANSFORM_TYPE##_local =                                                                            \
            kan_float_matrix_##MATRIX_DIMENSION##_to_transform_##TRANSFORM_DIMENSION (&result_local_matrix);           \
        ADDITIONAL_SETTER                                                                                              \
                                                                                                                       \
        component->TRANSFORM_TYPE##_global_dirty = false;                                                              \
        component->TRANSFORM_TYPE##_global = *new_transform;                                                           \
        kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_##TRANSFORM_TYPE##_global (queries, component);      \
        kan_atomic_int_unlock (&component->TRANSFORM_TYPE##_global_lock);                                              \
    }

    TRANSFORM_SET_GLOBAL (logical, 2, 3x3, kan_float_matrix_3x3_multiply,
                          component->logical_local_time_ns = transform_logical_time_ns;
                          component->visual_sync_needed = true;, , transform_logical_time_ns)
}

void kan_transform_2_set_visual_global (struct kan_transform_2_queries_t *queries,
                                        struct kan_transform_2_component_t *component,
                                        const struct kan_transform_2_t *new_transform)
{
    TRANSFORM_SET_GLOBAL (visual, 2, 3x3, kan_float_matrix_3x3_multiply, , )
}

void kan_transform_3_set_logical_global (struct kan_transform_3_queries_t *queries,
                                         struct kan_transform_3_component_t *component,
                                         const struct kan_transform_3_t *new_transform,
                                         kan_time_size_t transform_logical_time_ns)
{
    TRANSFORM_SET_GLOBAL (logical, 3, 4x4, kan_float_matrix_4x4_multiply_for_transform,
                          component->logical_local_time_ns = transform_logical_time_ns;
                          component->visual_sync_needed = true;, , transform_logical_time_ns)
}

void kan_transform_3_set_visual_global (struct kan_transform_3_queries_t *queries,
                                        struct kan_transform_3_component_t *component,
                                        const struct kan_transform_3_t *new_transform)
{
    TRANSFORM_SET_GLOBAL (visual, 3, 4x4, kan_float_matrix_4x4_multiply_for_transform, , )
#undef TRANSFORM_SET_GLOBAL
}

static inline void kan_transform_2_interpolate_visual (struct kan_transform_2_component_t *component, float alpha)
{
    component->visual_local.location =
        kan_float_vector_2_lerp (component->visual_local.location, component->logical_local.location, alpha);

    component->visual_local.rotation =
        kan_float_lerp (component->visual_local.rotation, component->logical_local.rotation, alpha);

    component->visual_local.scale =
        kan_float_vector_2_lerp (component->visual_local.scale, component->logical_local.scale, alpha);
}

static inline void kan_transform_3_interpolate_visual (struct kan_transform_3_component_t *component, float alpha)
{
    component->visual_local.location =
        kan_float_vector_3_lerp (component->visual_local.location, component->logical_local.location, alpha);

    component->visual_local.rotation =
        kan_float_vector_4_slerp (component->visual_local.rotation, component->logical_local.rotation, alpha);

    component->visual_local.scale =
        kan_float_vector_3_lerp (component->visual_local.scale, component->logical_local.scale, alpha);
}

#define VISUAL_TRANSFORM_SYNC_INVALIDATE_MUTATOR(TRANSFORM_DIMENSION, TRANSFORM_DIMENSION_STRING)                      \
    struct visual_transform_sync_##TRANSFORM_DIMENSION##_invalidate_state_t                                            \
    {                                                                                                                  \
        KAN_UM_GENERATE_STATE_QUERIES (visual_transform_sync_##TRANSFORM_DIMENSION##_invalidate)                       \
        KAN_UM_BIND_STATE (visual_transform_sync_##TRANSFORM_DIMENSION##_invalidate, state)                            \
                                                                                                                       \
        struct kan_transform_##TRANSFORM_DIMENSION##_queries_t transform_queries;                                      \
                                                                                                                       \
        KAN_REFLECTION_IGNORE                                                                                          \
        struct kan_stack_group_allocator_t temporary_allocator;                                                        \
                                                                                                                       \
        kan_allocation_group_t my_allocation_group;                                                                    \
        kan_cpu_section_t task_section;                                                                                \
    };                                                                                                                 \
                                                                                                                       \
    UNIVERSE_TRANSFORM_API void visual_transform_sync_##TRANSFORM_DIMENSION##_invalidate_state_init (                  \
        struct visual_transform_sync_##TRANSFORM_DIMENSION##_invalidate_state_t *instance)                             \
    {                                                                                                                  \
        instance->my_allocation_group = kan_allocation_group_stack_get ();                                             \
        instance->task_section = kan_cpu_section_get ("visual_transform_sync_invalidate" TRANSFORM_DIMENSION_STRING);  \
    }                                                                                                                  \
                                                                                                                       \
    UNIVERSE_TRANSFORM_API KAN_UM_MUTATOR_DEPLOY (visual_transform_sync_##TRANSFORM_DIMENSION##_invalidate)            \
    {                                                                                                                  \
        kan_workflow_graph_node_depend_on (workflow_node, KAN_TRANSFORM_VISUAL_SYNC_BEGIN_CHECKPOINT);                 \
        kan_stack_group_allocator_init (&state->temporary_allocator, state->my_allocation_group,                       \
                                        KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_INV_TASK_STACK);                            \
    }                                                                                                                  \
                                                                                                                       \
    KAN_CPU_TASK_BATCHED_HEADER (visual_transform_sync_##TRANSFORM_DIMENSION##_invalidate_execute)                     \
    {                                                                                                                  \
        struct visual_transform_sync_##TRANSFORM_DIMENSION##_invalidate_state_t *state;                                \
    };                                                                                                                 \
                                                                                                                       \
    KAN_CPU_TASK_BATCHED_BODY (visual_transform_sync_##TRANSFORM_DIMENSION##_invalidate_execute)                       \
    {                                                                                                                  \
        struct kan_repository_indexed_signal_read_access_t transform_read_access;                                      \
    };                                                                                                                 \
                                                                                                                       \
    KAN_CPU_TASK_BATCHED_DEFINE (visual_transform_sync_##TRANSFORM_DIMENSION##_invalidate_execute)                     \
    {                                                                                                                  \
        const struct kan_transform_##TRANSFORM_DIMENSION##_component_t *component =                                    \
            kan_repository_indexed_signal_read_access_resolve (&body->transform_read_access);                          \
        kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_visual_global (&header->state->transform_queries,    \
                                                                                 component);                           \
        kan_repository_indexed_signal_read_access_close (&body->transform_read_access);                                \
    }                                                                                                                  \
                                                                                                                       \
    UNIVERSE_TRANSFORM_API KAN_UM_MUTATOR_EXECUTE (visual_transform_sync_##TRANSFORM_DIMENSION##_invalidate)           \
    {                                                                                                                  \
        kan_stack_group_allocator_reset (&state->temporary_allocator);                                                 \
        struct kan_cpu_task_list_node_t *task_node = NULL;                                                             \
                                                                                                                       \
        KAN_CPU_TASK_LIST_BATCHED_PREPARE (visual_transform_sync_##TRANSFORM_DIMENSION##_invalidate_execute,           \
                                           KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_TASK_BATCH_MIN,                          \
                                           KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_TASK_BATCH_ATE,                          \
                                           {                                                                           \
                                               .state = state,                                                         \
                                           });                                                                         \
                                                                                                                       \
        KAN_UML_SIGNAL_READ (component, kan_transform_##TRANSFORM_DIMENSION##_component_t, visual_sync_needed, 1)      \
        {                                                                                                              \
            struct kan_repository_indexed_signal_read_access_t escaped_access;                                         \
            KAN_UM_ACCESS_ESCAPE (escaped_access, component);                                                          \
                                                                                                                       \
            KAN_CPU_TASK_LIST_BATCHED (&task_node, &state->temporary_allocator,                                        \
                                       visual_transform_sync_##TRANSFORM_DIMENSION##_invalidate_execute,               \
                                       state->task_section,                                                            \
                                       {                                                                               \
                                           .transform_read_access = escaped_access,                                    \
                                       });                                                                             \
        }                                                                                                              \
                                                                                                                       \
        kan_cpu_job_dispatch_and_detach_task_list (job, task_node);                                                    \
    }                                                                                                                  \
                                                                                                                       \
    UNIVERSE_TRANSFORM_API void                                                                                        \
        kan_universe_mutator_undeploy_visual_transform_sync_##TRANSFORM_DIMENSION##_invalidate (                       \
            struct visual_transform_sync_##TRANSFORM_DIMENSION##_invalidate_state_t *state)                            \
    {                                                                                                                  \
        kan_stack_group_allocator_shutdown (&state->temporary_allocator);                                              \
    }

VISUAL_TRANSFORM_SYNC_INVALIDATE_MUTATOR (2, "2")
VISUAL_TRANSFORM_SYNC_INVALIDATE_MUTATOR (3, "3")
#undef VISUAL_TRANSFORM_SYNC_INVALIDATE_MUTATOR

#define VISUAL_TRANSFORM_SYNC_CALCULATE_MUTATOR(TRANSFORM_DIMENSION, TRANSFORM_DIMENSION_STRING)                       \
    struct visual_transform_sync_##TRANSFORM_DIMENSION##_calculate_state_t                                             \
    {                                                                                                                  \
        KAN_UM_GENERATE_STATE_QUERIES (visual_transform_sync_##TRANSFORM_DIMENSION##_calculate)                        \
        KAN_UM_BIND_STATE (visual_transform_sync_##TRANSFORM_DIMENSION##_calculate, state)                             \
                                                                                                                       \
        struct kan_transform_##TRANSFORM_DIMENSION##_queries_t transform_queries;                                      \
                                                                                                                       \
        KAN_REFLECTION_IGNORE                                                                                          \
        struct kan_stack_group_allocator_t temporary_allocator;                                                        \
                                                                                                                       \
        kan_allocation_group_t my_allocation_group;                                                                    \
        kan_cpu_section_t task_section;                                                                                \
    };                                                                                                                 \
                                                                                                                       \
    UNIVERSE_TRANSFORM_API void visual_transform_sync_##TRANSFORM_DIMENSION##_calculate_state_init (                   \
        struct visual_transform_sync_##TRANSFORM_DIMENSION##_calculate_state_t *instance)                              \
    {                                                                                                                  \
        instance->my_allocation_group = kan_allocation_group_stack_get ();                                             \
        instance->task_section = kan_cpu_section_get ("visual_transform_sync_calculate" TRANSFORM_DIMENSION_STRING);   \
    }                                                                                                                  \
                                                                                                                       \
    UNIVERSE_TRANSFORM_API KAN_UM_MUTATOR_DEPLOY (visual_transform_sync_##TRANSFORM_DIMENSION##_calculate)             \
    {                                                                                                                  \
        kan_workflow_graph_node_depend_on (workflow_node,                                                              \
                                           "visual_transform_sync_" TRANSFORM_DIMENSION_STRING "_invalidate");         \
        kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_TRANSFORM_VISUAL_SYNC_END_CHECKPOINT);          \
        kan_stack_group_allocator_init (&state->temporary_allocator, state->my_allocation_group,                       \
                                        KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_CALC_TASK_STACK);                           \
    }                                                                                                                  \
                                                                                                                       \
    KAN_CPU_TASK_BATCHED_HEADER (visual_transform_sync_##TRANSFORM_DIMENSION##_calculate_execute)                      \
    {                                                                                                                  \
        struct visual_transform_sync_##TRANSFORM_DIMENSION##_calculate_state_t *state;                                 \
    };                                                                                                                 \
                                                                                                                       \
    KAN_CPU_TASK_BATCHED_BODY (visual_transform_sync_##TRANSFORM_DIMENSION##_calculate_execute)                        \
    {                                                                                                                  \
        struct kan_repository_indexed_signal_update_access_t transform_update_access;                                  \
    };                                                                                                                 \
                                                                                                                       \
    KAN_CPU_TASK_BATCHED_DEFINE (visual_transform_sync_##TRANSFORM_DIMENSION##_calculate_execute)                      \
    {                                                                                                                  \
        struct visual_transform_sync_##TRANSFORM_DIMENSION##_calculate_state_t *state = header->state;                 \
        KAN_UMI_SINGLETON_READ (time, kan_time_singleton_t)                                                            \
                                                                                                                       \
        struct kan_transform_##TRANSFORM_DIMENSION##_component_t *component =                                          \
            kan_repository_indexed_signal_update_access_resolve (&body->transform_update_access);                      \
        const kan_time_size_t source_time_ns = time->visual_time_ns - time->visual_delta_ns;                           \
        const kan_time_size_t target_time_ns = component->logical_local_time_ns;                                       \
                                                                                                                       \
        if (component->visual_synced_at_least_once && source_time_ns < target_time_ns)                                 \
        {                                                                                                              \
            const kan_time_offset_t max_delta_ns = (kan_time_offset_t) (target_time_ns - source_time_ns);              \
            const kan_time_offset_t delta_ns =                                                                         \
                ((time->visual_delta_ns) < (max_delta_ns) ? (time->visual_delta_ns) : (max_delta_ns));                 \
            const float alpha = ((float) delta_ns) / ((float) max_delta_ns);                                           \
            kan_transform_##TRANSFORM_DIMENSION##_interpolate_visual (component, alpha);                               \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            component->visual_synced_at_least_once = 1u;                                                               \
            component->visual_local = component->logical_local;                                                        \
        }                                                                                                              \
                                                                                                                       \
        component->visual_sync_needed = time->visual_time_ns < component->logical_local_time_ns;                       \
        kan_atomic_int_lock (&component->visual_global_lock);                                                          \
        component->visual_global_dirty = 1u;                                                                           \
        kan_atomic_int_unlock (&component->visual_global_lock);                                                        \
        kan_repository_indexed_signal_update_access_close (&body->transform_update_access);                            \
    }                                                                                                                  \
                                                                                                                       \
    UNIVERSE_TRANSFORM_API KAN_UM_MUTATOR_EXECUTE (visual_transform_sync_##TRANSFORM_DIMENSION##_calculate)            \
    {                                                                                                                  \
        kan_stack_group_allocator_reset (&state->temporary_allocator);                                                 \
        struct kan_cpu_task_list_node_t *task_node = NULL;                                                             \
                                                                                                                       \
        KAN_CPU_TASK_LIST_BATCHED_PREPARE (visual_transform_sync_##TRANSFORM_DIMENSION##_calculate_execute,            \
                                           KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_TASK_BATCH_MIN,                          \
                                           KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_TASK_BATCH_ATE,                          \
                                           {                                                                           \
                                               .state = state,                                                         \
                                           });                                                                         \
                                                                                                                       \
        KAN_UML_SIGNAL_UPDATE (component, kan_transform_##TRANSFORM_DIMENSION##_component_t, visual_sync_needed, 1)    \
        {                                                                                                              \
            struct kan_repository_indexed_signal_update_access_t escaped_access;                                       \
            KAN_UM_ACCESS_ESCAPE (escaped_access, component);                                                          \
                                                                                                                       \
            KAN_CPU_TASK_LIST_BATCHED (&task_node, &state->temporary_allocator,                                        \
                                       visual_transform_sync_##TRANSFORM_DIMENSION##_calculate_execute,                \
                                       state->task_section,                                                            \
                                       {                                                                               \
                                           .transform_update_access = escaped_access,                                  \
                                       });                                                                             \
        }                                                                                                              \
                                                                                                                       \
        kan_cpu_job_dispatch_and_detach_task_list (job, task_node);                                                    \
    }                                                                                                                  \
                                                                                                                       \
    UNIVERSE_TRANSFORM_API void                                                                                        \
        kan_universe_mutator_undeploy_visual_transform_sync_##TRANSFORM_DIMENSION##_calculate (                        \
            struct visual_transform_sync_##TRANSFORM_DIMENSION##_calculate_state_t *state)                             \
    {                                                                                                                  \
        kan_stack_group_allocator_shutdown (&state->temporary_allocator);                                              \
    }

VISUAL_TRANSFORM_SYNC_CALCULATE_MUTATOR (2, "2")
VISUAL_TRANSFORM_SYNC_CALCULATE_MUTATOR (3, "3")
#undef VISUAL_TRANSFORM_SYNC_CALCULATE_MUTATOR
