#include <kan/api_common/min_max.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/log/logging.h>
#include <kan/universe_time/universe_time.h>
#include <kan/universe_transform/universe_transform.h>

// \c_interface_scanner_disable
KAN_LOG_DEFINE_CATEGORY (universe_transform);
// \c_interface_scanner_enable

// \meta reflection_struct_meta = "kan_transform_2_component_t"
UNIVERSE_TRANSFORM_API struct kan_repository_meta_automatic_cascade_deletion_t
    kan_transform_2_component_hierarchy_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
        .child_type_name = "kan_transform_2_component_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"parent_object_id"}},
};

// \meta reflection_struct_meta = "kan_transform_3_component_t"
UNIVERSE_TRANSFORM_API struct kan_repository_meta_automatic_cascade_deletion_t
    kan_transform_3_component_hierarchy_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
        .child_type_name = "kan_transform_3_component_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"parent_object_id"}},
};

// \meta reflection_function_meta = "kan_universe_mutator_execute_visual_transform_sync_2_invalidate"
// \meta reflection_function_meta = "kan_universe_mutator_execute_visual_transform_sync_2_calculate"
UNIVERSE_TRANSFORM_API struct kan_universe_mutator_group_meta_t visual_transform_sync_2_calculate_mutator_group = {
    .group_name = KAN_TRANSFORM_VISUAL_SYNC_2_MUTATOR_GROUP,
};

// \meta reflection_function_meta = "kan_universe_mutator_execute_visual_transform_sync_3_invalidate"
// \meta reflection_function_meta = "kan_universe_mutator_execute_visual_transform_sync_3_calculate"
UNIVERSE_TRANSFORM_API struct kan_universe_mutator_group_meta_t visual_transform_sync_3_calculate_mutator_group = {
    .group_name = KAN_TRANSFORM_VISUAL_SYNC_3_MUTATOR_GROUP,
};

void kan_transform_2_component_init (struct kan_transform_2_component_t *instance)
{
    instance->object_id = KAN_INVALID_UNIVERSE_OBJECT_ID;
    instance->parent_object_id = KAN_INVALID_UNIVERSE_OBJECT_ID;

    instance->logical_local = kan_transform_2_get_identity ();
    instance->logical_local_time_ns = 0u;
    // For components that were instanced and filled through reflection automatically.
    instance->visual_sync_needed = KAN_TRUE;
    instance->visual_synced_at_least_once = KAN_FALSE;
    instance->visual_local = kan_transform_2_get_identity ();

    instance->logical_global_lock = kan_atomic_int_init (0);
    instance->logical_global_dirty = KAN_TRUE;
    instance->logical_global = kan_transform_2_get_identity ();

    instance->visual_global_lock = kan_atomic_int_init (0);
    instance->visual_global_dirty = KAN_TRUE;
    instance->visual_global = kan_transform_2_get_identity ();
}

void kan_transform_3_component_init (struct kan_transform_3_component_t *instance)
{
    instance->object_id = KAN_INVALID_UNIVERSE_OBJECT_ID;
    instance->parent_object_id = KAN_INVALID_UNIVERSE_OBJECT_ID;

    instance->logical_local = kan_transform_3_get_identity ();
    instance->logical_local_time_ns = 0u;
    // For components that were instanced and filled through reflection automatically.
    instance->visual_sync_needed = KAN_TRUE;
    instance->visual_synced_at_least_once = KAN_FALSE;
    instance->visual_local = kan_transform_3_get_identity ();

    instance->logical_global_lock = kan_atomic_int_init (0);
    instance->logical_global_dirty = KAN_TRUE;
    instance->logical_global = kan_transform_3_get_identity ();

    instance->visual_global_lock = kan_atomic_int_init (0);
    instance->visual_global_dirty = KAN_TRUE;
    instance->visual_global = kan_transform_3_get_identity ();
}

#define TRANSFORM_INVALIDATOR_FUNCTION(TRANSFORM_TYPE, TRANSFORM_DIMENSION)                                            \
    static void kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_##TRANSFORM_TYPE##_global (                  \
        struct kan_transform_##TRANSFORM_DIMENSION##_queries_t *queries,                                               \
        const struct kan_transform_##TRANSFORM_DIMENSION##_component_t *component)                                     \
    {                                                                                                                  \
        struct kan_repository_indexed_value_read_cursor_t cursor = kan_repository_indexed_value_read_query_execute (   \
            &queries->read_value__kan_transform_##TRANSFORM_DIMENSION##_component__parent_object_id,                   \
            &component->object_id);                                                                                    \
                                                                                                                       \
        while (KAN_TRUE)                                                                                               \
        {                                                                                                              \
            struct kan_repository_indexed_value_read_access_t access =                                                 \
                kan_repository_indexed_value_read_cursor_next (&cursor);                                               \
            const struct kan_transform_##TRANSFORM_DIMENSION##_component_t *child_component =                          \
                kan_repository_indexed_value_read_access_resolve (&access);                                            \
                                                                                                                       \
            if (child_component)                                                                                       \
            {                                                                                                          \
                struct kan_transform_##TRANSFORM_DIMENSION##_component_t *mutable_child_component =                    \
                    (struct kan_transform_##TRANSFORM_DIMENSION##_component_t *) child_component;                      \
                                                                                                                       \
                kan_atomic_int_lock (&mutable_child_component->TRANSFORM_TYPE##_global_lock);                          \
                mutable_child_component->TRANSFORM_TYPE##_global_dirty = KAN_TRUE;                                     \
                                                                                                                       \
                kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_##TRANSFORM_TYPE##_global (                  \
                    queries, mutable_child_component);                                                                 \
                kan_atomic_int_unlock (&mutable_child_component->TRANSFORM_TYPE##_global_lock);                        \
                kan_repository_indexed_value_read_access_close (&access);                                              \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                kan_repository_indexed_value_read_cursor_close (&cursor);                                              \
                break;                                                                                                 \
            }                                                                                                          \
        }                                                                                                              \
    }

TRANSFORM_INVALIDATOR_FUNCTION (logical, 2)
TRANSFORM_INVALIDATOR_FUNCTION (visual, 2)
TRANSFORM_INVALIDATOR_FUNCTION (logical, 3)
TRANSFORM_INVALIDATOR_FUNCTION (visual, 3)
#undef TRANSFORM_INVALIDATOR_FUNCTION

void kan_transform_2_set_parent_object_id (struct kan_transform_2_queries_t *queries,
                                           struct kan_transform_2_component_t *component,
                                           kan_universe_object_id_t parent_object_id)
{
    component->parent_object_id = parent_object_id;
    kan_atomic_int_lock (&component->logical_global_lock);
    component->logical_global_dirty = KAN_TRUE;
    kan_transform_2_invalidate_children_logical_global (queries, component);
    kan_atomic_int_unlock (&component->logical_global_lock);

    kan_atomic_int_lock (&component->visual_global_lock);
    component->visual_global_dirty = KAN_TRUE;
    kan_transform_2_invalidate_children_visual_global (queries, component);
    kan_atomic_int_unlock (&component->visual_global_lock);
}

void kan_transform_2_get_logical_global (struct kan_transform_2_queries_t *queries,
                                         const struct kan_transform_2_component_t *component,
                                         struct kan_transform_2_t *output)
{
#define TRANSFORM_GET_GLOBAL(TRANSFORM_TYPE, TRANSFORM_DIMENSION, MATRIX_DIMENSION, MULTIPLIER)                        \
    if (component->parent_object_id == KAN_INVALID_UNIVERSE_OBJECT_ID)                                                 \
    {                                                                                                                  \
        *output = component->TRANSFORM_TYPE##_local;                                                                   \
        return;                                                                                                        \
    }                                                                                                                  \
                                                                                                                       \
    struct kan_transform_##TRANSFORM_DIMENSION##_component_t *mutable_component =                                      \
        (struct kan_transform_##TRANSFORM_DIMENSION##_component_t *) component;                                        \
    kan_atomic_int_lock (&mutable_component->TRANSFORM_TYPE##_global_lock);                                            \
                                                                                                                       \
    if (!mutable_component->TRANSFORM_TYPE##_global_dirty)                                                             \
    {                                                                                                                  \
        *output = mutable_component->TRANSFORM_TYPE##_global;                                                          \
        kan_atomic_int_unlock (&mutable_component->TRANSFORM_TYPE##_global_lock);                                      \
        return;                                                                                                        \
    }                                                                                                                  \
                                                                                                                       \
    struct kan_transform_##TRANSFORM_DIMENSION##_t parent_transform;                                                   \
    struct kan_repository_indexed_value_read_cursor_t cursor = kan_repository_indexed_value_read_query_execute (       \
        &queries->read_value__kan_transform_##TRANSFORM_DIMENSION##_component__object_id,                              \
        &component->parent_object_id);                                                                                 \
                                                                                                                       \
    struct kan_repository_indexed_value_read_access_t parent_access =                                                  \
        kan_repository_indexed_value_read_cursor_next (&cursor);                                                       \
                                                                                                                       \
    const struct kan_transform_##TRANSFORM_DIMENSION##_component_t *parent_component =                                 \
        kan_repository_indexed_value_read_access_resolve (&parent_access);                                             \
                                                                                                                       \
    if (parent_component)                                                                                              \
    {                                                                                                                  \
        kan_transform_##TRANSFORM_DIMENSION##_get_##TRANSFORM_TYPE##_global (queries, parent_component,                \
                                                                             &parent_transform);                       \
        struct kan_float_matrix_##MATRIX_DIMENSION##_t parent_matrix;                                                  \
        kan_transform_##TRANSFORM_DIMENSION##_to_float_matrix_##MATRIX_DIMENSION (&parent_transform, &parent_matrix);  \
                                                                                                                       \
        struct kan_float_matrix_##MATRIX_DIMENSION##_t local_matrix;                                                   \
        kan_transform_##TRANSFORM_DIMENSION##_to_float_matrix_##MATRIX_DIMENSION (&component->TRANSFORM_TYPE##_local,  \
                                                                                  &local_matrix);                      \
                                                                                                                       \
        struct kan_float_matrix_##MATRIX_DIMENSION##_t result_matrix;                                                  \
        MULTIPLIER (&parent_matrix, &local_matrix, &result_matrix);                                                    \
                                                                                                                       \
        kan_float_matrix_##MATRIX_DIMENSION##_to_transform_##TRANSFORM_DIMENSION (                                     \
            &result_matrix, &mutable_component->TRANSFORM_TYPE##_global);                                              \
        *output = mutable_component->TRANSFORM_TYPE##_global;                                                          \
        mutable_component->TRANSFORM_TYPE##_global_dirty = KAN_FALSE;                                                  \
        kan_repository_indexed_value_read_access_close (&parent_access);                                               \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        KAN_LOG (universe_transform, KAN_LOG_ERROR, "Unable to find parent %llu of transform %llu.",                   \
                 (unsigned long long) component->parent_object_id, (unsigned long long) component->object_id)          \
        *output = component->TRANSFORM_TYPE##_local;                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    kan_repository_indexed_value_read_cursor_close (&cursor);                                                          \
    kan_atomic_int_unlock (&mutable_component->TRANSFORM_TYPE##_global_lock)

    TRANSFORM_GET_GLOBAL (logical, 2, 3x3, kan_float_matrix_3x3_multiply);
}

void kan_transform_2_set_logical_local (struct kan_transform_2_queries_t *queries,
                                        struct kan_transform_2_component_t *component,
                                        const struct kan_transform_2_t *new_transform,
                                        uint64_t transform_logical_time_ns)
{
    component->logical_local = *new_transform;
    component->logical_local_time_ns = transform_logical_time_ns;
    component->visual_sync_needed = KAN_TRUE;

    kan_atomic_int_lock (&component->logical_global_lock);
    component->logical_global_dirty = KAN_TRUE;
    kan_transform_2_invalidate_children_logical_global (queries, component);
    kan_atomic_int_unlock (&component->logical_global_lock);
}

void kan_transform_2_set_logical_global (struct kan_transform_2_queries_t *queries,
                                         struct kan_transform_2_component_t *component,
                                         const struct kan_transform_2_t *new_transform,
                                         uint64_t transform_logical_time_ns)
{
#define TRANSFORM_SET_GLOBAL(TRANSFORM_TYPE, TRANSFORM_DIMENSION, MATRIX_DIMENSION, MULTIPLIER, ADDITIONAL_SETTER,     \
                             ...)                                                                                      \
    if (component->parent_object_id == KAN_INVALID_UNIVERSE_OBJECT_ID)                                                 \
    {                                                                                                                  \
        kan_transform_##TRANSFORM_DIMENSION##_set_##TRANSFORM_TYPE##_local (queries, component,                        \
                                                                            new_transform __VA_ARGS__);                \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        kan_atomic_int_lock (&component->TRANSFORM_TYPE##_global_lock);                                                \
        struct kan_repository_indexed_value_read_cursor_t cursor = kan_repository_indexed_value_read_query_execute (   \
            &queries->read_value__kan_transform_##TRANSFORM_DIMENSION##_component__object_id,                          \
            &component->parent_object_id);                                                                             \
        struct kan_repository_indexed_value_read_access_t access =                                                     \
            kan_repository_indexed_value_read_cursor_next (&cursor);                                                   \
        kan_repository_indexed_value_read_cursor_close (&cursor);                                                      \
                                                                                                                       \
        const struct kan_transform_##TRANSFORM_DIMENSION##_component_t *parent_component =                             \
            kan_repository_indexed_value_read_access_resolve (&access);                                                \
                                                                                                                       \
        if (parent_component)                                                                                          \
        {                                                                                                              \
            struct kan_transform_##TRANSFORM_DIMENSION##_t parent_transform;                                           \
            kan_transform_##TRANSFORM_DIMENSION##_get_##TRANSFORM_TYPE##_global (queries, parent_component,            \
                                                                                 &parent_transform);                   \
                                                                                                                       \
            struct kan_float_matrix_##MATRIX_DIMENSION##_t parent_matrix;                                              \
            kan_transform_##TRANSFORM_DIMENSION##_to_float_matrix_##MATRIX_DIMENSION (&parent_transform,               \
                                                                                      &parent_matrix);                 \
                                                                                                                       \
            struct kan_float_matrix_##MATRIX_DIMENSION##_t parent_matrix_inverse;                                      \
            kan_float_matrix_##MATRIX_DIMENSION##_inverse (&parent_matrix, &parent_matrix_inverse);                    \
                                                                                                                       \
            struct kan_float_matrix_##MATRIX_DIMENSION##_t global_matrix;                                              \
            kan_transform_##TRANSFORM_DIMENSION##_to_float_matrix_##MATRIX_DIMENSION (new_transform, &global_matrix);  \
                                                                                                                       \
            struct kan_float_matrix_##MATRIX_DIMENSION##_t result_local_matrix;                                        \
            MULTIPLIER (&parent_matrix_inverse, &global_matrix, &result_local_matrix);                                 \
                                                                                                                       \
            kan_float_matrix_##MATRIX_DIMENSION##_to_transform_##TRANSFORM_DIMENSION (                                 \
                &result_local_matrix, &component->TRANSFORM_TYPE##_local);                                             \
            ADDITIONAL_SETTER                                                                                          \
                                                                                                                       \
            component->TRANSFORM_TYPE##_global_dirty = KAN_FALSE;                                                      \
            component->TRANSFORM_TYPE##_global = *new_transform;                                                       \
            kan_transform_##TRANSFORM_DIMENSION##_invalidate_children_##TRANSFORM_TYPE##_global (queries, component);  \
            kan_atomic_int_unlock (&component->TRANSFORM_TYPE##_global_lock);                                          \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            KAN_LOG (universe_transform, KAN_LOG_ERROR, "Unable to find parent %llu of transform %llu.",               \
                     (unsigned long long) component->parent_object_id, (unsigned long long) component->object_id)      \
            kan_atomic_int_unlock (&component->TRANSFORM_TYPE##_global_lock);                                          \
            kan_transform_##TRANSFORM_DIMENSION##_set_##TRANSFORM_TYPE##_local (queries, component,                    \
                                                                                new_transform __VA_ARGS__);            \
        }                                                                                                              \
    }

    TRANSFORM_SET_GLOBAL (logical, 2, 3x3, kan_float_matrix_3x3_multiply,
                          component->logical_local_time_ns = transform_logical_time_ns;
                          component->visual_sync_needed = KAN_TRUE;, , transform_logical_time_ns)
}

void kan_transform_2_get_visual_global (struct kan_transform_2_queries_t *queries,
                                        const struct kan_transform_2_component_t *component,
                                        struct kan_transform_2_t *output)
{
    TRANSFORM_GET_GLOBAL (visual, 2, 3x3, kan_float_matrix_3x3_multiply);
}

void kan_transform_2_set_visual_local (struct kan_transform_2_queries_t *queries,
                                       struct kan_transform_2_component_t *component,
                                       const struct kan_transform_2_t *new_transform)
{
    component->visual_local = *new_transform;
    kan_atomic_int_lock (&component->visual_global_lock);
    component->visual_global_dirty = KAN_TRUE;
    kan_transform_2_invalidate_children_visual_global (queries, component);
    kan_atomic_int_unlock (&component->visual_global_lock);
}

void kan_transform_2_set_visual_global (struct kan_transform_2_queries_t *queries,
                                        struct kan_transform_2_component_t *component,
                                        const struct kan_transform_2_t *new_transform)
{
    TRANSFORM_SET_GLOBAL (visual, 2, 3x3, kan_float_matrix_3x3_multiply, , )
}

void kan_transform_3_set_parent_object_id (struct kan_transform_3_queries_t *queries,
                                           struct kan_transform_3_component_t *component,
                                           kan_universe_object_id_t parent_object_id)
{
    component->parent_object_id = parent_object_id;
    kan_atomic_int_lock (&component->logical_global_lock);
    component->logical_global_dirty = KAN_TRUE;
    kan_transform_3_invalidate_children_logical_global (queries, component);
    kan_atomic_int_unlock (&component->logical_global_lock);

    kan_atomic_int_lock (&component->visual_global_lock);
    component->visual_global_dirty = KAN_TRUE;
    kan_transform_3_invalidate_children_visual_global (queries, component);
    kan_atomic_int_unlock (&component->visual_global_lock);
}

void kan_transform_3_get_logical_global (struct kan_transform_3_queries_t *queries,
                                         const struct kan_transform_3_component_t *component,
                                         struct kan_transform_3_t *output)
{
    TRANSFORM_GET_GLOBAL (logical, 3, 4x4, kan_float_matrix_4x4_multiply_for_transform);
}

void kan_transform_3_set_logical_local (struct kan_transform_3_queries_t *queries,
                                        struct kan_transform_3_component_t *component,
                                        const struct kan_transform_3_t *new_transform,
                                        uint64_t transform_logical_time_ns)
{
    component->logical_local = *new_transform;
    component->logical_local_time_ns = transform_logical_time_ns;
    component->visual_sync_needed = KAN_TRUE;

    kan_atomic_int_lock (&component->logical_global_lock);
    component->logical_global_dirty = KAN_TRUE;
    kan_transform_3_invalidate_children_logical_global (queries, component);
    kan_atomic_int_unlock (&component->logical_global_lock);
}

void kan_transform_3_set_logical_global (struct kan_transform_3_queries_t *queries,
                                         struct kan_transform_3_component_t *component,
                                         const struct kan_transform_3_t *new_transform,
                                         uint64_t transform_logical_time_ns)
{
    TRANSFORM_SET_GLOBAL (logical, 3, 4x4, kan_float_matrix_4x4_multiply_for_transform,
                          component->logical_local_time_ns = transform_logical_time_ns;
                          component->visual_sync_needed = KAN_TRUE;, , transform_logical_time_ns)
}

void kan_transform_3_get_visual_global (struct kan_transform_3_queries_t *queries,
                                        const struct kan_transform_3_component_t *component,
                                        struct kan_transform_3_t *output)
{
    TRANSFORM_GET_GLOBAL (visual, 3, 4x4, kan_float_matrix_4x4_multiply_for_transform);
#undef TRANSFORM_GET_GLOBAL
}

void kan_transform_3_set_visual_local (struct kan_transform_3_queries_t *queries,
                                       struct kan_transform_3_component_t *component,
                                       const struct kan_transform_3_t *new_transform)
{
    component->visual_local = *new_transform;
    kan_atomic_int_lock (&component->visual_global_lock);
    component->visual_global_dirty = KAN_TRUE;
    kan_transform_3_invalidate_children_visual_global (queries, component);
    kan_atomic_int_unlock (&component->visual_global_lock);
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
        kan_float_vector_2_lerp (&component->visual_local.location, &component->logical_local.location, alpha);

    component->visual_local.rotation =
        kan_float_lerp (component->visual_local.rotation, component->logical_local.rotation, alpha);

    component->visual_local.scale =
        kan_float_vector_2_lerp (&component->visual_local.scale, &component->logical_local.scale, alpha);
}

static inline void kan_transform_3_interpolate_visual (struct kan_transform_3_component_t *component, float alpha)
{
    component->visual_local.location =
        kan_float_vector_3_lerp (&component->visual_local.location, &component->logical_local.location, alpha);

    component->visual_local.rotation =
        kan_float_vector_4_slerp (&component->visual_local.rotation, &component->logical_local.rotation, alpha);

    component->visual_local.scale =
        kan_float_vector_3_lerp (&component->visual_local.scale, &component->logical_local.scale, alpha);
}

struct visual_transform_sync_2_invalidate_state_t
{
    struct kan_repository_indexed_signal_read_query_t read_signal__kan_transform_2_component__visual_sync_needed__1;
    struct kan_transform_2_queries_t transform_queries;

    /// \meta reflection_ignore_struct_field
    struct kan_stack_group_allocator_t temporary_allocator;

    kan_allocation_group_t my_allocation_group;
    kan_interned_string_t task_name;
};

struct visual_transform_sync_2_invalidate_task_user_data_t
{
    struct visual_transform_sync_2_invalidate_state_t *source_state;
    struct kan_repository_indexed_signal_read_access_t transform_read_access;
};

UNIVERSE_TRANSFORM_API void visual_transform_sync_2_invalidate_state_init (
    struct visual_transform_sync_2_invalidate_state_t *instance)
{
    instance->my_allocation_group = kan_allocation_group_stack_get ();
    instance->task_name = kan_string_intern ("visual_transform_sync_invalidate_2");
}

UNIVERSE_TRANSFORM_API void kan_universe_mutator_deploy_visual_transform_sync_2_invalidate (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct visual_transform_sync_2_invalidate_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_TRANSFORM_VISUAL_SYNC_BEGIN_CHECKPOINT);
    kan_stack_group_allocator_init (&state->temporary_allocator, state->my_allocation_group,
                                    KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_INV_TASK_STACK);
}

static void visual_transform_sync_2_invalidate_execute (uint64_t user_data)
{
    struct visual_transform_sync_2_invalidate_task_user_data_t *data =
        (struct visual_transform_sync_2_invalidate_task_user_data_t *) user_data;
    const struct kan_transform_2_component_t *component =
        kan_repository_indexed_signal_read_access_resolve (&data->transform_read_access);
    kan_transform_2_invalidate_children_visual_global (&data->source_state->transform_queries, component);
    kan_repository_indexed_signal_read_access_close (&data->transform_read_access);
}

UNIVERSE_TRANSFORM_API void kan_universe_mutator_execute_visual_transform_sync_2_invalidate (
    kan_cpu_job_t job, struct visual_transform_sync_2_invalidate_state_t *state)
{
#define TRANSFORM_INVALIDATE_MUTATOR(TRANSFORM_DIMENSIONS)                                                             \
    kan_stack_group_allocator_reset (&state->temporary_allocator);                                                     \
    struct kan_cpu_task_list_node_t *task_node = NULL;                                                                 \
    struct kan_repository_indexed_signal_read_cursor_t read_cursor =                                                   \
        kan_repository_indexed_signal_read_query_execute (                                                             \
            &state->read_signal__kan_transform_##TRANSFORM_DIMENSIONS##_component__visual_sync_needed__1);             \
                                                                                                                       \
    while (KAN_TRUE)                                                                                                   \
    {                                                                                                                  \
        struct kan_repository_indexed_signal_read_access_t access =                                                    \
            kan_repository_indexed_signal_read_cursor_next (&read_cursor);                                             \
                                                                                                                       \
        if (kan_repository_indexed_signal_read_access_resolve (&access))                                               \
        {                                                                                                              \
            KAN_CPU_TASK_LIST_USER_STRUCT (                                                                            \
                &task_node, &state->temporary_allocator, state->task_name,                                             \
                visual_transform_sync_##TRANSFORM_DIMENSIONS##_invalidate_execute, FOREGROUND,                         \
                struct visual_transform_sync_##TRANSFORM_DIMENSIONS##_invalidate_task_user_data_t,                     \
                {                                                                                                      \
                    .source_state = state,                                                                             \
                    .transform_read_access = access,                                                                   \
                });                                                                                                    \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            kan_repository_indexed_signal_read_cursor_close (&read_cursor);                                            \
            break;                                                                                                     \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    kan_cpu_job_dispatch_and_detach_task_list (job, task_node);                                                        \
    kan_cpu_job_release (job)

    TRANSFORM_INVALIDATE_MUTATOR (2);
}

UNIVERSE_TRANSFORM_API void kan_universe_mutator_undeploy_visual_transform_sync_2_invalidate (
    struct visual_transform_sync_2_invalidate_state_t *state)
{
    kan_stack_group_allocator_shutdown (&state->temporary_allocator);
}

struct visual_transform_sync_2_calculate_state_t
{
    struct kan_repository_singleton_read_query_t read__kan_time_singleton;
    struct kan_repository_indexed_signal_update_query_t update_signal__kan_transform_2_component__visual_sync_needed__1;

    struct kan_transform_2_queries_t transform_queries;

    /// \meta reflection_ignore_struct_field
    struct kan_stack_group_allocator_t temporary_allocator;

    kan_allocation_group_t my_allocation_group;
    kan_interned_string_t task_name;
};

struct visual_transform_sync_2_calculate_task_user_data_t
{
    struct visual_transform_sync_2_calculate_state_t *source_state;
    struct kan_repository_indexed_signal_update_access_t transform_update_access;
};

UNIVERSE_TRANSFORM_API void visual_transform_sync_2_calculate_state_init (
    struct visual_transform_sync_2_calculate_state_t *instance)
{
    instance->my_allocation_group = kan_allocation_group_stack_get ();
    instance->task_name = kan_string_intern ("visual_transform_sync_calculate_2");
}

UNIVERSE_TRANSFORM_API void kan_universe_mutator_deploy_visual_transform_sync_2_calculate (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct visual_transform_sync_2_calculate_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, "visual_transform_sync_2_invalidate");
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_TRANSFORM_VISUAL_SYNC_END_CHECKPOINT);
    kan_stack_group_allocator_init (&state->temporary_allocator, state->my_allocation_group,
                                    KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_CALC_TASK_STACK);
}

static void visual_transform_sync_2_calculate_execute (uint64_t user_data)
{
#define TRANSFORM_SYNC_EXECUTE(TRANSFORM_DIMENSIONS)                                                                   \
    struct visual_transform_sync_##TRANSFORM_DIMENSIONS##_calculate_task_user_data_t *data =                           \
        (struct visual_transform_sync_##TRANSFORM_DIMENSIONS##_calculate_task_user_data_t *) user_data;                \
                                                                                                                       \
    kan_repository_singleton_read_access_t time_access =                                                               \
        kan_repository_singleton_read_query_execute (&data->source_state->read__kan_time_singleton);                   \
    const struct kan_time_singleton_t *time = kan_repository_singleton_read_access_resolve (time_access);              \
                                                                                                                       \
    struct kan_transform_##TRANSFORM_DIMENSIONS##_component_t *component =                                             \
        kan_repository_indexed_signal_update_access_resolve (&data->transform_update_access);                          \
                                                                                                                       \
    const uint64_t source_time_ns = time->visual_time_ns - time->visual_delta_ns;                                      \
    const uint64_t target_time_ns = component->logical_local_time_ns;                                                  \
                                                                                                                       \
    if (component->visual_synced_at_least_once && source_time_ns < target_time_ns)                                     \
    {                                                                                                                  \
        const uint64_t max_delta_ns = target_time_ns - source_time_ns;                                                 \
        const uint64_t delta_ns = KAN_MIN (time->visual_delta_ns, max_delta_ns);                                       \
        const float alpha = ((float) delta_ns) / ((float) max_delta_ns);                                               \
        kan_transform_##TRANSFORM_DIMENSIONS##_interpolate_visual (component, alpha);                                  \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        component->visual_synced_at_least_once = KAN_TRUE;                                                             \
        component->visual_local = component->logical_local;                                                            \
    }                                                                                                                  \
                                                                                                                       \
    component->visual_sync_needed = time->visual_time_ns < component->logical_local_time_ns;                           \
    kan_atomic_int_lock (&component->visual_global_lock);                                                              \
    component->visual_global_dirty = KAN_TRUE;                                                                         \
    kan_atomic_int_unlock (&component->visual_global_lock);                                                            \
                                                                                                                       \
    kan_repository_indexed_signal_update_access_close (&data->transform_update_access);                                \
    kan_repository_singleton_read_access_close (time_access)

    TRANSFORM_SYNC_EXECUTE (2);
}

UNIVERSE_TRANSFORM_API void kan_universe_mutator_execute_visual_transform_sync_2_calculate (
    kan_cpu_job_t job, struct visual_transform_sync_2_calculate_state_t *state)
{
#define TRANSFORM_SYNC_MUTATOR(TRANSFORM_DIMENSIONS)                                                                   \
    kan_stack_group_allocator_reset (&state->temporary_allocator);                                                     \
    struct kan_cpu_task_list_node_t *task_node = NULL;                                                                 \
    struct kan_repository_indexed_signal_update_cursor_t update_cursor =                                               \
        kan_repository_indexed_signal_update_query_execute (                                                           \
            &state->update_signal__kan_transform_##TRANSFORM_DIMENSIONS##_component__visual_sync_needed__1);           \
                                                                                                                       \
    while (KAN_TRUE)                                                                                                   \
    {                                                                                                                  \
        struct kan_repository_indexed_signal_update_access_t access =                                                  \
            kan_repository_indexed_signal_update_cursor_next (&update_cursor);                                         \
                                                                                                                       \
        struct kan_transform_##TRANSFORM_DIMENSIONS##_component_t *component =                                         \
            kan_repository_indexed_signal_update_access_resolve (&access);                                             \
                                                                                                                       \
        if (component)                                                                                                 \
        {                                                                                                              \
            KAN_CPU_TASK_LIST_USER_STRUCT (                                                                            \
                &task_node, &state->temporary_allocator, state->task_name,                                             \
                visual_transform_sync_##TRANSFORM_DIMENSIONS##_calculate_execute, FOREGROUND,                          \
                struct visual_transform_sync_##TRANSFORM_DIMENSIONS##_calculate_task_user_data_t,                      \
                {                                                                                                      \
                    .source_state = state,                                                                             \
                    .transform_update_access = access,                                                                 \
                });                                                                                                    \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            kan_repository_indexed_signal_update_cursor_close (&update_cursor);                                        \
            break;                                                                                                     \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    kan_cpu_job_dispatch_and_detach_task_list (job, task_node);                                                        \
    kan_cpu_job_release (job)

    TRANSFORM_SYNC_MUTATOR (2);
}

UNIVERSE_TRANSFORM_API void kan_universe_mutator_undeploy_visual_transform_sync_2_calculate (
    struct visual_transform_sync_2_calculate_state_t *state)
{
    kan_stack_group_allocator_shutdown (&state->temporary_allocator);
}

struct visual_transform_sync_3_invalidate_state_t
{
    struct kan_repository_indexed_signal_read_query_t read_signal__kan_transform_3_component__visual_sync_needed__1;
    struct kan_transform_3_queries_t transform_queries;

    /// \meta reflection_ignore_struct_field
    struct kan_stack_group_allocator_t temporary_allocator;

    kan_allocation_group_t my_allocation_group;
    kan_interned_string_t task_name;
};

struct visual_transform_sync_3_invalidate_task_user_data_t
{
    struct visual_transform_sync_3_invalidate_state_t *source_state;
    struct kan_repository_indexed_signal_read_access_t transform_read_access;
};

UNIVERSE_TRANSFORM_API void visual_transform_sync_3_invalidate_state_init (
    struct visual_transform_sync_3_invalidate_state_t *instance)
{
    instance->my_allocation_group = kan_allocation_group_stack_get ();
    instance->task_name = kan_string_intern ("visual_transform_sync_invalidate_3");
}

UNIVERSE_TRANSFORM_API void kan_universe_mutator_deploy_visual_transform_sync_3_invalidate (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct visual_transform_sync_3_invalidate_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_TRANSFORM_VISUAL_SYNC_BEGIN_CHECKPOINT);
    kan_stack_group_allocator_init (&state->temporary_allocator, state->my_allocation_group,
                                    KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_INV_TASK_STACK);
}

static void visual_transform_sync_3_invalidate_execute (uint64_t user_data)
{
    struct visual_transform_sync_3_invalidate_task_user_data_t *data =
        (struct visual_transform_sync_3_invalidate_task_user_data_t *) user_data;
    const struct kan_transform_3_component_t *component =
        kan_repository_indexed_signal_read_access_resolve (&data->transform_read_access);
    kan_transform_3_invalidate_children_visual_global (&data->source_state->transform_queries, component);
    kan_repository_indexed_signal_read_access_close (&data->transform_read_access);
}

UNIVERSE_TRANSFORM_API void kan_universe_mutator_execute_visual_transform_sync_3_invalidate (
    kan_cpu_job_t job, struct visual_transform_sync_3_invalidate_state_t *state)
{
    TRANSFORM_INVALIDATE_MUTATOR (3);
#undef TRANSFORM_INVALIDATE_MUTATOR
}

UNIVERSE_TRANSFORM_API void kan_universe_mutator_undeploy_visual_transform_sync_3_invalidate (
    struct visual_transform_sync_3_invalidate_state_t *state)
{
    kan_stack_group_allocator_shutdown (&state->temporary_allocator);
}

struct visual_transform_sync_3_calculate_state_t
{
    struct kan_repository_singleton_read_query_t read__kan_time_singleton;
    struct kan_repository_indexed_signal_update_query_t update_signal__kan_transform_3_component__visual_sync_needed__1;

    struct kan_transform_3_queries_t transform_queries;

    /// \meta reflection_ignore_struct_field
    struct kan_stack_group_allocator_t temporary_allocator;

    kan_allocation_group_t my_allocation_group;
    kan_interned_string_t task_name;
};

struct visual_transform_sync_3_calculate_task_user_data_t
{
    struct visual_transform_sync_3_calculate_state_t *source_state;
    struct kan_repository_indexed_signal_update_access_t transform_update_access;
};

UNIVERSE_TRANSFORM_API void visual_transform_sync_3_calculate_state_init (
    struct visual_transform_sync_3_calculate_state_t *instance)
{
    instance->my_allocation_group = kan_allocation_group_stack_get ();
    instance->task_name = kan_string_intern ("visual_transform_sync_3_calculate");
}

UNIVERSE_TRANSFORM_API void kan_universe_mutator_deploy_visual_transform_sync_3_calculate (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct visual_transform_sync_3_calculate_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, "visual_transform_sync_3_invalidate");
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_TRANSFORM_VISUAL_SYNC_END_CHECKPOINT);
    kan_stack_group_allocator_init (&state->temporary_allocator, state->my_allocation_group,
                                    KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_CALC_TASK_STACK);
}

static void visual_transform_sync_3_calculate_execute (uint64_t user_data)
{
    TRANSFORM_SYNC_EXECUTE (3);
#undef TRANSFORM_SYNC_EXECUTE
}

UNIVERSE_TRANSFORM_API void kan_universe_mutator_execute_visual_transform_sync_3_calculate (
    kan_cpu_job_t job, struct visual_transform_sync_3_calculate_state_t *state)
{
    TRANSFORM_SYNC_MUTATOR (3);
#undef TRANSFORM_SYNC_MUTATOR
}

UNIVERSE_TRANSFORM_API void kan_universe_mutator_undeploy_visual_transform_sync_3_calculate (
    struct visual_transform_sync_3_calculate_state_t *state)
{
    kan_stack_group_allocator_shutdown (&state->temporary_allocator);
}
