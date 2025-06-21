#include <test_universe_transform_api.h>

#include <stddef.h>

#include <kan/context/all_system_names.h>
#include <kan/context/reflection_system.h>
#include <kan/context/universe_system.h>
#include <kan/context/update_system.h>
#include <kan/testing/testing.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe_single_pipeline_scheduler/universe_single_pipeline_scheduler.h>
#include <kan/universe_time/universe_time.h>
#include <kan/universe_transform/universe_transform.h>

// We're not using universe preprocessor here as we're utilizing macros to write the same logic for 2d and 3d.
// However, the might be changed in the future.

struct test_utility_queries_2_t
{
    KAN_UP_GENERATE_STATE_QUERIES (test_utility_queries_2)
    struct kan_transform_2_queries_t inner_queries;
};

struct test_utility_queries_3_t
{
    KAN_UP_GENERATE_STATE_QUERIES (test_utility_queries_3)
    struct kan_transform_3_queries_t inner_queries;
};

#define TEST_UTILITY_CREATE_TRANSFORM(DIMENSIONS)                                                                      \
    static inline kan_universe_object_id_t create_transform_##DIMENSIONS (                                             \
        struct test_utility_queries_##DIMENSIONS##_t *queries, kan_universe_object_id_t parent_object_id,              \
        struct kan_transform_##DIMENSIONS##_t logical_transform,                                                       \
        struct kan_transform_##DIMENSIONS##_t visual_transform)                                                        \
    {                                                                                                                  \
        KAN_UP_BIND_STATE (test_utility_queries_##DIMENSIONS, queries)                                                 \
        KAN_UP_SINGLETON_READ (singleton, kan_object_id_generator_singleton_t)                                         \
                                                                                                                       \
        KAN_UP_INDEXED_INSERT (component, kan_transform_##DIMENSIONS##_component_t)                                    \
        {                                                                                                              \
            component->object_id = kan_universe_object_id_generate (singleton);                                        \
            component->parent_object_id = parent_object_id;                                                            \
            component->logical_local = logical_transform;                                                              \
            component->visual_local = visual_transform;                                                                \
            KAN_UP_QUERY_RETURN_VALUE (kan_universe_object_id_t, component->object_id);                                \
        }                                                                                                              \
                                                                                                                       \
        return KAN_TYPED_ID_32_SET_INVALID (kan_universe_object_id_t);                                                 \
    }

TEST_UTILITY_CREATE_TRANSFORM (2)
TEST_UTILITY_CREATE_TRANSFORM (3)
#undef TEST_UTILITY_CREATE_TRANSFORM

static inline kan_bool_t check_float (float value, float expected)
{
#define TOLERANCE 0.0001f
    return fabs (value - expected) <= TOLERANCE;
#undef TOLERANCE
}

static inline kan_bool_t check_transform_equality_2 (struct kan_transform_2_t value, struct kan_transform_2_t expected)
{
    return check_float (value.location.x, expected.location.x) && check_float (value.location.y, expected.location.y) &&
           check_float (value.rotation, expected.rotation) && check_float (value.scale.x, expected.scale.x) &&
           check_float (value.scale.y, expected.scale.y);
}

static inline kan_bool_t check_transform_equality_3 (struct kan_transform_3_t value, struct kan_transform_3_t expected)
{
    return check_float (value.location.x, expected.location.x) && check_float (value.location.y, expected.location.y) &&
           check_float (value.location.z, expected.location.z) && check_float (value.rotation.x, expected.rotation.x) &&
           check_float (value.rotation.y, expected.rotation.y) && check_float (value.rotation.z, expected.rotation.z) &&
           check_float (value.rotation.w, expected.rotation.w) && check_float (value.scale.x, expected.scale.x) &&
           check_float (value.scale.y, expected.scale.y) && check_float (value.scale.z, expected.scale.z);
}

#define TEST_UTILITY_CHECK_TRANSFORM_GLOBAL(DIMENSIONS)                                                                \
    static inline kan_bool_t check_transform_global_##DIMENSIONS (                                                     \
        struct test_utility_queries_##DIMENSIONS##_t *queries, kan_universe_object_id_t object_id, kan_bool_t logical, \
        struct kan_transform_##DIMENSIONS##_t expected)                                                                \
    {                                                                                                                  \
        KAN_UP_BIND_STATE (test_utility_queries_##DIMENSIONS, queries)                                                 \
        kan_bool_t equal = KAN_FALSE;                                                                                  \
                                                                                                                       \
        KAN_UP_VALUE_READ (component, kan_transform_##DIMENSIONS##_component_t, object_id, &object_id)                 \
        {                                                                                                              \
            struct kan_transform_##DIMENSIONS##_t value;                                                               \
            if (logical)                                                                                               \
            {                                                                                                          \
                kan_transform_##DIMENSIONS##_get_logical_global (&queries->inner_queries, component, &value);          \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                kan_transform_##DIMENSIONS##_get_visual_global (&queries->inner_queries, component, &value);           \
            }                                                                                                          \
                                                                                                                       \
            equal = check_transform_equality_##DIMENSIONS (value, expected);                                           \
        }                                                                                                              \
                                                                                                                       \
        return equal;                                                                                                  \
    }

TEST_UTILITY_CHECK_TRANSFORM_GLOBAL (2)
TEST_UTILITY_CHECK_TRANSFORM_GLOBAL (3)
#undef TEST_UTILITY_CHECK_TRANSFORM_GLOBAL

#define TEST_UTILITY_SET_TRANSFORM(SCOPE, DIMENSIONS)                                                                  \
    static inline void set_transform_##SCOPE##_##DIMENSIONS (struct test_utility_queries_##DIMENSIONS##_t *queries,    \
                                                             kan_universe_object_id_t object_id,                       \
                                                             struct kan_transform_##DIMENSIONS##_t logical_transform,  \
                                                             struct kan_transform_##DIMENSIONS##_t visual_transform)   \
    {                                                                                                                  \
        KAN_UP_BIND_STATE (test_utility_queries_##DIMENSIONS, queries)                                                 \
        KAN_UP_VALUE_UPDATE (component, kan_transform_##DIMENSIONS##_component_t, object_id, &object_id)               \
        {                                                                                                              \
            KAN_UP_SINGLETON_READ (time, kan_time_singleton_t)                                                         \
            kan_transform_##DIMENSIONS##_set_logical_##SCOPE (&queries->inner_queries, component, &logical_transform,  \
                                                              time->logical_time_ns);                                  \
            kan_transform_##DIMENSIONS##_set_visual_##SCOPE (&queries->inner_queries, component, &visual_transform);   \
        }                                                                                                              \
    }

TEST_UTILITY_SET_TRANSFORM (local, 2)
TEST_UTILITY_SET_TRANSFORM (local, 3)
TEST_UTILITY_SET_TRANSFORM (global, 2)
TEST_UTILITY_SET_TRANSFORM (global, 3)
#undef TEST_UTILITY_SET_TRANSFORM

#define TEST_UTILITY_SET_PARENT(DIMENSIONS)                                                                            \
    static inline void set_transform_parent_##DIMENSIONS (struct test_utility_queries_##DIMENSIONS##_t *queries,       \
                                                          kan_universe_object_id_t object_id,                          \
                                                          kan_universe_object_id_t new_parent_object_id)               \
    {                                                                                                                  \
        KAN_UP_BIND_STATE (test_utility_queries_##DIMENSIONS, queries)                                                 \
        KAN_UP_VALUE_UPDATE (component, kan_transform_##DIMENSIONS##_component_t, object_id, &object_id)               \
        {                                                                                                              \
            kan_transform_##DIMENSIONS##_set_parent_object_id (&queries->inner_queries, component,                     \
                                                               new_parent_object_id);                                  \
        }                                                                                                              \
    }

TEST_UTILITY_SET_PARENT (2)
TEST_UTILITY_SET_PARENT (3)
#undef TEST_UTILITY_SET_PARENT

static kan_context_t create_context (void)
{
    kan_context_t context =
        kan_context_create (kan_allocation_group_get_child (kan_allocation_group_root (), "context"));
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME, NULL))
    kan_context_assembly (context);
    return context;
}

struct test_global_2_state_t
{
    struct test_utility_queries_2_t utility;
};

TEST_UNIVERSE_TRANSFORM_API void kan_universe_mutator_execute_test_global_2 (kan_cpu_job_t job,
                                                                             struct test_global_2_state_t *state)
{
    const struct kan_transform_2_t initial_transform_0 = {
        .location = {2.0f, 4.0f},
        .rotation = 0.0f,
        .scale = {1.0f, 1.0f},
    };

    const struct kan_transform_2_t initial_transform_1 = {
        .location = {1.0f, 0.0f},
        .rotation = 0.0f,
        .scale = {2.0f, 2.0f},
    };

    const struct kan_transform_2_t initial_transform_2 = {
        .location = {3.0f, 11.0f},
        .rotation = 0.0f,
        .scale = {1.0f, 3.0f},
    };

    const struct kan_transform_2_t initial_transform_3 = {
        .location = {0.0f, -5.0f},
        .rotation = 0.0f,
        .scale = {1.0f, 1.0f},
    };

    const struct kan_transform_2_t initial_transform_4 = {
        .location = {12.0f, -5.0f},
        .rotation = KAN_PI * 0.5f,
        .scale = {3.0f, 3.0f},
    };

    const struct kan_transform_2_t initial_transform_5 = {
        .location = {1.0f, 10.0f},
        .rotation = 0.0f,
        .scale = {1.0f, 1.0f},
    };

    const struct kan_transform_2_t expected_global_transform_3 = {
        .location = {9.0f, -4.0f},
        .rotation = 0.0f,
        .scale = {2.0f, 6.0f},
    };

    const struct kan_transform_2_t transform_local_change_2 = {
        .location = {0.0f, 0.0f},
        .rotation = 0.0f,
        .scale = {1.0f, 1.0f},
    };

    const struct kan_transform_2_t expected_global_transform_changed_3 = {
        .location = {3.0f, -6.0f},
        .rotation = 0.0f,
        .scale = {2.0f, 2.0f},
    };

    const struct kan_transform_2_t expected_global_transform_other_parent_3 = {
        .location = {27.0f, -5.0f},
        .rotation = KAN_PI * 0.5f,
        .scale = {3.0f, 3.0f},
    };

    const struct kan_transform_2_t transform_global_change_4 = {
        .location = {0.0f, 10.0f},
        .rotation = 0.0f,
        .scale = {2.0f, 2.0f},
    };

    const struct kan_transform_2_t expected_global_transform_changed_again_3 = {
        .location = {0.0f, 0.0f},
        .rotation = 0.0f,
        .scale = {2.0f, 2.0f},
    };

    const struct kan_transform_2_t expected_global_transform_5 = {
        .location = {-18.0f, -2.0f},
        .rotation = KAN_PI * 0.5f,
        .scale = {3.0f, 3.0f},
    };

#define TRANSFORM_GLOBAL_TEST(DIMENSIONS)                                                                              \
    kan_universe_object_id_t transform_ids[6u];                                                                        \
    transform_ids[0u] =                                                                                                \
        create_transform_##DIMENSIONS (&state->utility, KAN_TYPED_ID_32_SET_INVALID (kan_universe_object_id_t),        \
                                       initial_transform_0, initial_transform_0);                                      \
                                                                                                                       \
    transform_ids[1u] =                                                                                                \
        create_transform_##DIMENSIONS (&state->utility, transform_ids[0u], initial_transform_1, initial_transform_1);  \
                                                                                                                       \
    transform_ids[2u] =                                                                                                \
        create_transform_##DIMENSIONS (&state->utility, transform_ids[1u], initial_transform_2, initial_transform_2);  \
                                                                                                                       \
    transform_ids[3u] =                                                                                                \
        create_transform_##DIMENSIONS (&state->utility, transform_ids[2u], initial_transform_3, initial_transform_3);  \
                                                                                                                       \
    transform_ids[4u] =                                                                                                \
        create_transform_##DIMENSIONS (&state->utility, KAN_TYPED_ID_32_SET_INVALID (kan_universe_object_id_t),        \
                                       initial_transform_4, initial_transform_4);                                      \
                                                                                                                       \
    transform_ids[5u] =                                                                                                \
        create_transform_##DIMENSIONS (&state->utility, transform_ids[4u], initial_transform_5, initial_transform_5);  \
                                                                                                                       \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_TRUE,                 \
                                                         expected_global_transform_3))                                 \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_TRUE,                 \
                                                         expected_global_transform_3))                                 \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[5u], KAN_TRUE,                 \
                                                         expected_global_transform_5))                                 \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[5u], KAN_TRUE,                 \
                                                         expected_global_transform_5))                                 \
                                                                                                                       \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_FALSE,                \
                                                         expected_global_transform_3))                                 \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_FALSE,                \
                                                         expected_global_transform_3))                                 \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[5u], KAN_FALSE,                \
                                                         expected_global_transform_5))                                 \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[5u], KAN_FALSE,                \
                                                         expected_global_transform_5))                                 \
                                                                                                                       \
    set_transform_local_##DIMENSIONS (&state->utility, transform_ids[2u], transform_local_change_2,                    \
                                      transform_local_change_2);                                                       \
                                                                                                                       \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_TRUE,                 \
                                                         expected_global_transform_changed_3))                         \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_TRUE,                 \
                                                         expected_global_transform_changed_3))                         \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_FALSE,                \
                                                         expected_global_transform_changed_3))                         \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_FALSE,                \
                                                         expected_global_transform_changed_3))                         \
                                                                                                                       \
    set_transform_parent_##DIMENSIONS (&state->utility, transform_ids[3u], transform_ids[4u]);                         \
                                                                                                                       \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_TRUE,                 \
                                                         expected_global_transform_other_parent_3))                    \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_TRUE,                 \
                                                         expected_global_transform_other_parent_3))                    \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_FALSE,                \
                                                         expected_global_transform_other_parent_3))                    \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_FALSE,                \
                                                         expected_global_transform_other_parent_3))                    \
                                                                                                                       \
    set_transform_global_##DIMENSIONS (&state->utility, transform_ids[4u], transform_global_change_4,                  \
                                       transform_global_change_4);                                                     \
                                                                                                                       \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_TRUE,                 \
                                                         expected_global_transform_changed_again_3))                   \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_TRUE,                 \
                                                         expected_global_transform_changed_again_3))                   \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_FALSE,                \
                                                         expected_global_transform_changed_again_3))                   \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], KAN_FALSE,                \
                                                         expected_global_transform_changed_again_3))

    TRANSFORM_GLOBAL_TEST (2)
    KAN_UP_MUTATOR_RETURN;
}

struct test_global_3_state_t
{
    struct test_utility_queries_3_t utility;
};

TEST_UNIVERSE_TRANSFORM_API void kan_universe_mutator_execute_test_global_3 (kan_cpu_job_t job,
                                                                             struct test_global_3_state_t *state)
{
    const struct kan_transform_3_t initial_transform_0 = {
        .location = {2.0f, 4.0f, 1.0f},
        .rotation = kan_make_float_vector_4_t (0.0f, 0.0f, 0.0f, 1.0f),
        .scale = {1.0f, 1.0f, 1.0f},
    };

    const struct kan_transform_3_t initial_transform_1 = {
        .location = {1.0f, 0.0f, -1.0f},
        .rotation = kan_make_float_vector_4_t (0.0f, 0.0f, 0.0f, 1.0f),
        .scale = {2.0f, 2.0f, 2.0f},
    };

    const struct kan_transform_3_t initial_transform_2 = {
        .location = {3.0f, 11.0f, 2.0f},
        .rotation = kan_make_float_vector_4_t (0.0f, 0.0f, 0.0f, 1.0f),
        .scale = {1.0f, 3.0f, 1.0f},
    };

    const struct kan_transform_3_t initial_transform_3 = {
        .location = {0.0f, -5.0f, -3.0f},
        .rotation = kan_make_float_vector_4_t (0.0f, 0.0f, 0.0f, 1.0f),
        .scale = {1.0f, 1.0f, 2.0f},
    };

    const struct kan_transform_3_t initial_transform_4 = {
        .location = {12.0f, -5.0f, 3.0f},
        .rotation = kan_make_quaternion_from_euler (KAN_PI * 0.5f, 0.0f, 0.0f),
        .scale = {3.0f, 3.0f, 3.0f},
    };

    const struct kan_transform_3_t initial_transform_5 = {
        .location = {1.0f, 10.0f, 11.0f},
        .rotation = kan_make_float_vector_4_t (0.0f, 0.0f, 0.0f, 1.0f),
        .scale = {1.0f, 1.0f, 1.0f},
    };

    const struct kan_transform_3_t expected_global_transform_3 = {
        .location = {9.0f, -4.0f, -2.0f},
        .rotation = kan_make_float_vector_4_t (0.0f, 0.0f, 0.0f, 1.0f),
        .scale = {2.0f, 6.0f, 4.0f},
    };

    const struct kan_transform_3_t transform_local_change_2 = {
        .location = {0.0f, 0.0f, 0.0f},
        .rotation = kan_make_float_vector_4_t (0.0f, 0.0f, 0.0f, 1.0f),
        .scale = {1.0f, 1.0f, 1.0f},
    };

    const struct kan_transform_3_t expected_global_transform_changed_3 = {
        .location = {3.0f, -6.0f, -6.0f},
        .rotation = kan_make_float_vector_4_t (0.0f, 0.0f, 0.0f, 1.0f),
        .scale = {2.0f, 2.0f, 4.0f},
    };

    const struct kan_transform_3_t expected_global_transform_other_parent_3 = {
        .location = {12.0f, 4.0f, -12.0f},
        .rotation = kan_make_quaternion_from_euler (KAN_PI * 0.5f, 0.0f, 0.0f),
        .scale = {3.0f, 3.0f, 6.0f},
    };

    const struct kan_transform_3_t transform_global_change_4 = {
        .location = {0.0f, 10.0f, 6.0f},
        .rotation = kan_make_float_vector_4_t (0.0f, 0.0f, 0.0f, 1.0f),
        .scale = {2.0f, 2.0f, 2.0f},
    };

    const struct kan_transform_3_t expected_global_transform_changed_again_3 = {
        .location = {0.0f, 0.0f, 0.0f},
        .rotation = kan_make_float_vector_4_t (0.0f, 0.0f, 0.0f, 1.0f),
        .scale = {2.0f, 2.0f, 4.0f},
    };

    const struct kan_transform_3_t expected_global_transform_5 = {
        .location = {15.0f, -38.0f, 33.0f},
        .rotation = kan_make_quaternion_from_euler (KAN_PI * 0.5f, 0.0f, 0.0f),
        .scale = {3.0f, 3.0f, 3.0f},
    };

    TRANSFORM_GLOBAL_TEST (3)
#undef TRANSFORM_GLOBAL_TEST
    KAN_UP_MUTATOR_RETURN;
}

static void test_global (kan_interned_string_t test_mutator)
{
    kan_context_t context = create_context ();
    kan_context_system_t universe_system_handle = kan_context_query (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe_system_handle))

    kan_universe_t universe = kan_universe_system_get_universe (universe_system_handle);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe))

    struct kan_universe_world_definition_t definition;
    kan_universe_world_definition_init (&definition);
    definition.world_name = kan_string_intern ("root_world");
    definition.scheduler_name = kan_string_intern (KAN_UNIVERSE_SINGLE_PIPELINE_NO_TIME_SCHEDULER_NAME);

    kan_dynamic_array_set_capacity (&definition.pipelines, 1u);
    struct kan_universe_world_pipeline_definition_t *update_pipeline =
        kan_dynamic_array_add_last (&definition.pipelines);

    kan_universe_world_pipeline_definition_init (update_pipeline);
    update_pipeline->name = kan_string_intern (KAN_UNIVERSE_SINGLE_PIPELINE_SCHEDULER_PIPELINE_NAME);

    kan_dynamic_array_set_capacity (&update_pipeline->mutators, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutators) = test_mutator;

    kan_universe_deploy_root (universe, &definition);
    kan_universe_world_definition_shutdown (&definition);

    kan_context_system_t update_system = kan_context_query (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (update_system))
    kan_update_system_run (update_system);
}

KAN_TEST_CASE (global_2)
{
    test_global (kan_string_intern ("test_global_2"));
}

KAN_TEST_CASE (global_3)
{
    test_global (kan_string_intern ("test_global_3"));
}

#define LOGICAL_PIPELINE_NAME "logical_update"
#define VISUAL_PIPELINE_NAME "visual_update"

struct test_sync_scheduler_state_t
{
    struct kan_repository_singleton_write_query_t write__kan_time_singleton;
};

static void fake_time (struct test_sync_scheduler_state_t *state,
                       kan_time_size_t logical_time_ns,
                       kan_time_offset_t logical_delta_ns,
                       kan_time_size_t visual_time_ns,
                       kan_time_offset_t visual_delta_ns)
{
    struct kan_repository_singleton_write_access_t time_access =
        kan_repository_singleton_write_query_execute (&state->write__kan_time_singleton);
    struct kan_time_singleton_t *time = kan_repository_singleton_write_access_resolve (&time_access);

    time->logical_time_ns = logical_time_ns;
    time->logical_delta_ns = logical_delta_ns;
    time->visual_time_ns = visual_time_ns;
    time->visual_delta_ns = visual_delta_ns;
    time->visual_unscaled_delta_ns = visual_delta_ns;
    time->scale = 1.0f;
    kan_repository_singleton_write_access_close (&time_access);
}

TEST_UNIVERSE_TRANSFORM_API void kan_universe_scheduler_execute_test_sync_scheduler (
    kan_universe_scheduler_interface_t interface, struct test_sync_scheduler_state_t *state)
{
    fake_time (state, 1000u, 0u, 1000u, 0u);
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern (LOGICAL_PIPELINE_NAME));
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern (VISUAL_PIPELINE_NAME));

    fake_time (state, 2000u, 1000u, 1000u, 0u);
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern (LOGICAL_PIPELINE_NAME));

    fake_time (state, 2000u, 1000u, 1100u, 100u);
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern (VISUAL_PIPELINE_NAME));

    fake_time (state, 2000u, 1000u, 1200u, 100u);
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern (VISUAL_PIPELINE_NAME));

    fake_time (state, 2000u, 1000u, 1500u, 300u);
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern (VISUAL_PIPELINE_NAME));

    fake_time (state, 2000u, 1000u, 1750u, 250u);
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern (VISUAL_PIPELINE_NAME));

    fake_time (state, 3000u, 1000u, 2100u, 350u);
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern (LOGICAL_PIPELINE_NAME));
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern (VISUAL_PIPELINE_NAME));
}

#define TRANSFORM_SYNC_BEGIN_X 10.0f
#define TRANSFORM_SYNC_END_X 20.0f

#define TRANSFORM_SYNC_PARENT_TRANSFORM_ID KAN_TYPED_ID_32_SET (kan_universe_object_id_t, 1u)
#define TRANSFORM_SYNC_CHILD_TRANSFORM_ID KAN_TYPED_ID_32_SET (kan_universe_object_id_t, 2u)

struct test_sync_logical_2_state_t
{
    struct test_utility_queries_2_t utility;
};

static inline struct kan_transform_2_t make_transform_2_x_only (float x)
{
    return (struct kan_transform_2_t) {
        .location = {x, 0.0f},
        .rotation = 0.0f,
        .scale = {1.0f, 1.0f},
    };
}

TEST_UNIVERSE_TRANSFORM_API void kan_universe_mutator_execute_test_sync_logical_2 (
    kan_cpu_job_t job, struct test_sync_logical_2_state_t *state)
{
    struct kan_transform_2_t logical_transform_begin = make_transform_2_x_only (TRANSFORM_SYNC_BEGIN_X);
    struct kan_transform_2_t logical_transform_end = make_transform_2_x_only (TRANSFORM_SYNC_END_X);

#define TRANSFORM_SYNC_LOGICAL(DIMENSIONS)                                                                             \
    KAN_UP_BIND_STATE (test_utility_queries_##DIMENSIONS, &state->utility)                                             \
    KAN_UP_SINGLETON_READ (time, kan_time_singleton_t)                                                                 \
                                                                                                                       \
    switch (time->logical_time_ns)                                                                                     \
    {                                                                                                                  \
    case 1000u:                                                                                                        \
        KAN_TEST_CHECK (KAN_TYPED_ID_32_IS_EQUAL (                                                                     \
            create_transform_##DIMENSIONS (&state->utility, KAN_TYPED_ID_32_SET_INVALID (kan_universe_object_id_t),    \
                                           logical_transform_begin, kan_transform_##DIMENSIONS##_get_identity ()),     \
            TRANSFORM_SYNC_PARENT_TRANSFORM_ID))                                                                       \
        KAN_TEST_CHECK (KAN_TYPED_ID_32_IS_EQUAL (                                                                     \
            create_transform_##DIMENSIONS (&state->utility, TRANSFORM_SYNC_PARENT_TRANSFORM_ID,                        \
                                           logical_transform_begin, kan_transform_##DIMENSIONS##_get_identity ()),     \
            TRANSFORM_SYNC_CHILD_TRANSFORM_ID))                                                                        \
        break;                                                                                                         \
                                                                                                                       \
    case 2000u:                                                                                                        \
        set_transform_local_##DIMENSIONS (&state->utility, TRANSFORM_SYNC_PARENT_TRANSFORM_ID, logical_transform_end,  \
                                          logical_transform_begin);                                                    \
                                                                                                                       \
        set_transform_local_##DIMENSIONS (&state->utility, TRANSFORM_SYNC_CHILD_TRANSFORM_ID, logical_transform_end,   \
                                          logical_transform_begin);                                                    \
        break;                                                                                                         \
                                                                                                                       \
    case 3000u:                                                                                                        \
        break;                                                                                                         \
                                                                                                                       \
    default:                                                                                                           \
        KAN_TEST_ASSERT (KAN_FALSE);                                                                                   \
        break;                                                                                                         \
    }

    TRANSFORM_SYNC_LOGICAL (2);
    KAN_UP_MUTATOR_RETURN;
}

struct test_sync_logical_3_state_t
{
    struct test_utility_queries_3_t utility;
};

static inline struct kan_transform_3_t make_transform_3_x_only (float x)
{
    return (struct kan_transform_3_t) {
        .location = {x, 0.0f, 0.0f},
        .rotation = kan_make_float_vector_4_t (0.0f, 0.0f, 0.0f, 1.0f),
        .scale = {1.0f, 1.0f, 1.0f},
    };
}

TEST_UNIVERSE_TRANSFORM_API void kan_universe_mutator_execute_test_sync_logical_3 (
    kan_cpu_job_t job, struct test_sync_logical_3_state_t *state)
{
    struct kan_transform_3_t logical_transform_begin = make_transform_3_x_only (TRANSFORM_SYNC_BEGIN_X);
    struct kan_transform_3_t logical_transform_end = make_transform_3_x_only (TRANSFORM_SYNC_END_X);

    TRANSFORM_SYNC_LOGICAL (3);
#undef TRANSFORM_SYNC_LOGICAL
    KAN_UP_MUTATOR_RETURN;
}

struct test_sync_visual_2_state_t
{
    struct test_utility_queries_2_t utility;
};

TEST_UNIVERSE_TRANSFORM_API void kan_universe_mutator_deploy_test_sync_visual_2 (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct test_sync_visual_2_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_TRANSFORM_VISUAL_SYNC_END_CHECKPOINT);
}

TEST_UNIVERSE_TRANSFORM_API void kan_universe_mutator_execute_test_sync_visual_2 (
    kan_cpu_job_t job, struct test_sync_visual_2_state_t *state)
{
#define TRANSFORM_SYNC_VISUAL(DIMENSIONS)                                                                              \
    KAN_UP_BIND_STATE (test_utility_queries_##DIMENSIONS, &state->utility)                                             \
    KAN_UP_SINGLETON_READ (time, kan_time_singleton_t)                                                                 \
                                                                                                                       \
    switch (time->visual_time_ns)                                                                                      \
    {                                                                                                                  \
    case 1000u:                                                                                                        \
        KAN_TEST_CHECK (                                                                                               \
            check_transform_global_##DIMENSIONS (&state->utility, TRANSFORM_SYNC_PARENT_TRANSFORM_ID, KAN_FALSE,       \
                                                 make_transform_##DIMENSIONS##_x_only (TRANSFORM_SYNC_BEGIN_X)))       \
                                                                                                                       \
        KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (                                                          \
            &state->utility, TRANSFORM_SYNC_CHILD_TRANSFORM_ID, KAN_FALSE,                                             \
            make_transform_##DIMENSIONS##_x_only (TRANSFORM_SYNC_BEGIN_X * 2.0f)))                                     \
        break;                                                                                                         \
                                                                                                                       \
    case 1100u:                                                                                                        \
        KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (                                                          \
            &state->utility, TRANSFORM_SYNC_PARENT_TRANSFORM_ID, KAN_FALSE,                                            \
            make_transform_##DIMENSIONS##_x_only (TRANSFORM_SYNC_BEGIN_X * 1.1f)))                                     \
                                                                                                                       \
        KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (                                                          \
            &state->utility, TRANSFORM_SYNC_CHILD_TRANSFORM_ID, KAN_FALSE,                                             \
            make_transform_##DIMENSIONS##_x_only (TRANSFORM_SYNC_BEGIN_X * 2.2f)))                                     \
        break;                                                                                                         \
                                                                                                                       \
    case 1200u:                                                                                                        \
        KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (                                                          \
            &state->utility, TRANSFORM_SYNC_PARENT_TRANSFORM_ID, KAN_FALSE,                                            \
            make_transform_##DIMENSIONS##_x_only (TRANSFORM_SYNC_BEGIN_X * 1.2f)))                                     \
                                                                                                                       \
        KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (                                                          \
            &state->utility, TRANSFORM_SYNC_CHILD_TRANSFORM_ID, KAN_FALSE,                                             \
            make_transform_##DIMENSIONS##_x_only (TRANSFORM_SYNC_BEGIN_X * 2.4f)))                                     \
        break;                                                                                                         \
                                                                                                                       \
    case 1500u:                                                                                                        \
        KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (                                                          \
            &state->utility, TRANSFORM_SYNC_PARENT_TRANSFORM_ID, KAN_FALSE,                                            \
            make_transform_##DIMENSIONS##_x_only (TRANSFORM_SYNC_BEGIN_X * 1.5f)))                                     \
                                                                                                                       \
        KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (                                                          \
            &state->utility, TRANSFORM_SYNC_CHILD_TRANSFORM_ID, KAN_FALSE,                                             \
            make_transform_##DIMENSIONS##_x_only (TRANSFORM_SYNC_BEGIN_X * 3.0f)))                                     \
        break;                                                                                                         \
                                                                                                                       \
    case 1750u:                                                                                                        \
        KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (                                                          \
            &state->utility, TRANSFORM_SYNC_PARENT_TRANSFORM_ID, KAN_FALSE,                                            \
            make_transform_##DIMENSIONS##_x_only (TRANSFORM_SYNC_BEGIN_X * 1.75f)))                                    \
                                                                                                                       \
        KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (                                                          \
            &state->utility, TRANSFORM_SYNC_CHILD_TRANSFORM_ID, KAN_FALSE,                                             \
            make_transform_##DIMENSIONS##_x_only (TRANSFORM_SYNC_BEGIN_X * 3.5f)))                                     \
        break;                                                                                                         \
                                                                                                                       \
    case 2100u:                                                                                                        \
        KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (                                                          \
            &state->utility, TRANSFORM_SYNC_PARENT_TRANSFORM_ID, KAN_FALSE,                                            \
            make_transform_##DIMENSIONS##_x_only (TRANSFORM_SYNC_BEGIN_X * 2.0f)))                                     \
                                                                                                                       \
        KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (                                                          \
            &state->utility, TRANSFORM_SYNC_CHILD_TRANSFORM_ID, KAN_FALSE,                                             \
            make_transform_##DIMENSIONS##_x_only (TRANSFORM_SYNC_BEGIN_X * 4.0f)))                                     \
        break;                                                                                                         \
                                                                                                                       \
    default:                                                                                                           \
        KAN_TEST_ASSERT (KAN_FALSE);                                                                                   \
        break;                                                                                                         \
    }

    TRANSFORM_SYNC_VISUAL (2);
    KAN_UP_MUTATOR_RETURN;
}

struct test_sync_visual_3_state_t
{
    struct test_utility_queries_3_t utility;
};

TEST_UNIVERSE_TRANSFORM_API void kan_universe_mutator_deploy_test_sync_visual_3 (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct test_sync_visual_3_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_TRANSFORM_VISUAL_SYNC_END_CHECKPOINT);
}

TEST_UNIVERSE_TRANSFORM_API void kan_universe_mutator_execute_test_sync_visual_3 (
    kan_cpu_job_t job, struct test_sync_visual_3_state_t *state)
{
    TRANSFORM_SYNC_VISUAL (3);
#undef TRANSFORM_SYNC_VISUAL
    KAN_UP_MUTATOR_RETURN;
}

static void test_sync (kan_interned_string_t test_logical_mutator,
                       kan_interned_string_t test_visual_mutator,
                       kan_interned_string_t sync_group)
{
    kan_context_t context = create_context ();
    kan_context_system_t universe_system_handle = kan_context_query (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe_system_handle))

    kan_universe_t universe = kan_universe_system_get_universe (universe_system_handle);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe))

    struct kan_universe_world_definition_t definition;
    kan_universe_world_definition_init (&definition);
    definition.world_name = kan_string_intern ("root_world");
    definition.scheduler_name = kan_string_intern ("test_sync_scheduler");

    kan_dynamic_array_set_capacity (&definition.pipelines, 2u);
    struct kan_universe_world_pipeline_definition_t *logical_pipeline =
        kan_dynamic_array_add_last (&definition.pipelines);

    kan_universe_world_pipeline_definition_init (logical_pipeline);
    logical_pipeline->name = kan_string_intern (LOGICAL_PIPELINE_NAME);

    kan_dynamic_array_set_capacity (&logical_pipeline->mutators, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&logical_pipeline->mutators) = test_logical_mutator;

    struct kan_universe_world_pipeline_definition_t *visual_pipeline =
        kan_dynamic_array_add_last (&definition.pipelines);

    kan_universe_world_pipeline_definition_init (visual_pipeline);
    visual_pipeline->name = kan_string_intern (VISUAL_PIPELINE_NAME);

    kan_dynamic_array_set_capacity (&visual_pipeline->mutators, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&visual_pipeline->mutators) = test_visual_mutator;

    kan_dynamic_array_set_capacity (&visual_pipeline->mutator_groups, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&visual_pipeline->mutator_groups) = sync_group;

    kan_universe_deploy_root (universe, &definition);
    kan_universe_world_definition_shutdown (&definition);

    kan_context_system_t update_system = kan_context_query (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (update_system))
    kan_update_system_run (update_system);
}

KAN_TEST_CASE (sync_2)
{
    test_sync (kan_string_intern ("test_sync_logical_2"), kan_string_intern ("test_sync_visual_2"),
               kan_string_intern (KAN_TRANSFORM_VISUAL_SYNC_2_MUTATOR_GROUP));
}

KAN_TEST_CASE (sync_3)
{
    test_sync (kan_string_intern ("test_sync_logical_3"), kan_string_intern ("test_sync_visual_3"),
               kan_string_intern (KAN_TRANSFORM_VISUAL_SYNC_3_MUTATOR_GROUP));
}
