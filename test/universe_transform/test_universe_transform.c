#include <test_universe_transform_api.h>

#include <stddef.h>

#include <kan/context/all_system_names.h>
#include <kan/context/reflection_system.h>
#include <kan/context/universe_system.h>
#include <kan/context/update_system.h>
#include <kan/testing/testing.h>
#include <kan/universe/macro.h>
#include <kan/universe_single_pipeline_scheduler/universe_single_pipeline_scheduler.h>
#include <kan/universe_transform/universe_transform.h>

struct test_utility_queries_2_t
{
    KAN_UM_GENERATE_STATE_QUERIES (test_utility_queries_2)
    struct kan_transform_2_queries_t inner_queries;
};

struct test_utility_queries_3_t
{
    KAN_UM_GENERATE_STATE_QUERIES (test_utility_queries_3)
    struct kan_transform_3_queries_t inner_queries;
};

#define TEST_UTILITY_CREATE_TRANSFORM(DIMENSIONS)                                                                      \
    static inline kan_universe_object_id_t create_transform_##DIMENSIONS (                                             \
        struct test_utility_queries_##DIMENSIONS##_t *queries, kan_universe_object_id_t parent_object_id,              \
        struct kan_transform_##DIMENSIONS##_t local)                                                                   \
    {                                                                                                                  \
        KAN_UM_BIND_STATE (test_utility_queries_##DIMENSIONS, queries)                                                 \
        KAN_UMI_SINGLETON_READ (singleton, kan_object_id_generator_singleton_t)                                        \
                                                                                                                       \
        KAN_UMO_INDEXED_INSERT (component, kan_transform_##DIMENSIONS##_component_t)                                   \
        {                                                                                                              \
            component->object_id = kan_universe_object_id_generate (singleton);                                        \
            component->parent_object_id = parent_object_id;                                                            \
            component->local = local;                                                                                  \
            return component->object_id;                                                                               \
        }                                                                                                              \
                                                                                                                       \
        return KAN_TYPED_ID_32_SET_INVALID (kan_universe_object_id_t);                                                 \
    }

TEST_UTILITY_CREATE_TRANSFORM (2)
TEST_UTILITY_CREATE_TRANSFORM (3)
#undef TEST_UTILITY_CREATE_TRANSFORM

static inline bool check_float (float value, float expected)
{
#define TOLERANCE 0.0001f
    return fabs (value - expected) <= TOLERANCE;
#undef TOLERANCE
}

static inline bool check_transform_equality_2 (struct kan_transform_2_t value, struct kan_transform_2_t expected)
{
    return check_float (value.location.x, expected.location.x) && check_float (value.location.y, expected.location.y) &&
           check_float (value.rotation, expected.rotation) && check_float (value.scale.x, expected.scale.x) &&
           check_float (value.scale.y, expected.scale.y);
}

static inline bool check_transform_equality_3 (struct kan_transform_3_t value, struct kan_transform_3_t expected)
{
    return check_float (value.location.x, expected.location.x) && check_float (value.location.y, expected.location.y) &&
           check_float (value.location.z, expected.location.z) && check_float (value.rotation.x, expected.rotation.x) &&
           check_float (value.rotation.y, expected.rotation.y) && check_float (value.rotation.z, expected.rotation.z) &&
           check_float (value.rotation.w, expected.rotation.w) && check_float (value.scale.x, expected.scale.x) &&
           check_float (value.scale.y, expected.scale.y) && check_float (value.scale.z, expected.scale.z);
}

#define TEST_UTILITY_CHECK_TRANSFORM_GLOBAL(DIMENSIONS)                                                                \
    static inline bool check_transform_global_##DIMENSIONS (struct test_utility_queries_##DIMENSIONS##_t *queries,     \
                                                            kan_universe_object_id_t object_id,                        \
                                                            struct kan_transform_##DIMENSIONS##_t expected)            \
    {                                                                                                                  \
        KAN_UM_BIND_STATE (test_utility_queries_##DIMENSIONS, queries)                                                 \
        KAN_UMI_VALUE_READ_REQUIRED (component, kan_transform_##DIMENSIONS##_component_t, object_id, &object_id)       \
                                                                                                                       \
        struct kan_transform_##DIMENSIONS##_t value =                                                                  \
            kan_transform_##DIMENSIONS##_component_get_global (&queries->inner_queries, component);                    \
                                                                                                                       \
        return check_transform_equality_##DIMENSIONS (value, expected);                                                \
    }

TEST_UTILITY_CHECK_TRANSFORM_GLOBAL (2)
TEST_UTILITY_CHECK_TRANSFORM_GLOBAL (3)
#undef TEST_UTILITY_CHECK_TRANSFORM_GLOBAL

#define TEST_UTILITY_SET_TRANSFORM(SCOPE, DIMENSIONS)                                                                  \
    static inline void set_transform_##SCOPE##_##DIMENSIONS (struct test_utility_queries_##DIMENSIONS##_t *queries,    \
                                                             kan_universe_object_id_t object_id,                       \
                                                             struct kan_transform_##DIMENSIONS##_t transform)          \
    {                                                                                                                  \
        KAN_UM_BIND_STATE (test_utility_queries_##DIMENSIONS, queries)                                                 \
        KAN_UMI_VALUE_UPDATE_REQUIRED (component, kan_transform_##DIMENSIONS##_component_t, object_id, &object_id)     \
        kan_transform_##DIMENSIONS##_component_set_##SCOPE (&queries->inner_queries, component, &transform);           \
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
        KAN_UM_BIND_STATE (test_utility_queries_##DIMENSIONS, queries)                                                 \
        KAN_UMI_VALUE_UPDATE_REQUIRED (component, kan_transform_##DIMENSIONS##_component_t, object_id, &object_id)     \
        kan_transform_##DIMENSIONS##_component_set_parent_object_id (&queries->inner_queries, component,               \
                                                                     new_parent_object_id);                            \
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

TEST_UNIVERSE_TRANSFORM_API KAN_UM_MUTATOR_EXECUTE (test_global_2)
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
    transform_ids[0u] = create_transform_##DIMENSIONS (                                                                \
        &state->utility, KAN_TYPED_ID_32_SET_INVALID (kan_universe_object_id_t), initial_transform_0);                 \
    transform_ids[1u] = create_transform_##DIMENSIONS (&state->utility, transform_ids[0u], initial_transform_1);       \
    transform_ids[2u] = create_transform_##DIMENSIONS (&state->utility, transform_ids[1u], initial_transform_2);       \
    transform_ids[3u] = create_transform_##DIMENSIONS (&state->utility, transform_ids[2u], initial_transform_3);       \
    transform_ids[4u] = create_transform_##DIMENSIONS (                                                                \
        &state->utility, KAN_TYPED_ID_32_SET_INVALID (kan_universe_object_id_t), initial_transform_4);                 \
    transform_ids[5u] = create_transform_##DIMENSIONS (&state->utility, transform_ids[4u], initial_transform_5);       \
                                                                                                                       \
    KAN_TEST_CHECK (                                                                                                   \
        check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], expected_global_transform_3))         \
    KAN_TEST_CHECK (                                                                                                   \
        check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], expected_global_transform_3))         \
    KAN_TEST_CHECK (                                                                                                   \
        check_transform_global_##DIMENSIONS (&state->utility, transform_ids[5u], expected_global_transform_5))         \
    KAN_TEST_CHECK (                                                                                                   \
        check_transform_global_##DIMENSIONS (&state->utility, transform_ids[5u], expected_global_transform_5))         \
                                                                                                                       \
    set_transform_local_##DIMENSIONS (&state->utility, transform_ids[2u], transform_local_change_2);                   \
    KAN_TEST_CHECK (                                                                                                   \
        check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], expected_global_transform_changed_3)) \
    KAN_TEST_CHECK (                                                                                                   \
        check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u], expected_global_transform_changed_3)) \
                                                                                                                       \
    set_transform_parent_##DIMENSIONS (&state->utility, transform_ids[3u], transform_ids[4u]);                         \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u],                           \
                                                         expected_global_transform_other_parent_3))                    \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u],                           \
                                                         expected_global_transform_other_parent_3))                    \
                                                                                                                       \
    set_transform_global_##DIMENSIONS (&state->utility, transform_ids[4u], transform_global_change_4);                 \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u],                           \
                                                         expected_global_transform_changed_again_3))                   \
    KAN_TEST_CHECK (check_transform_global_##DIMENSIONS (&state->utility, transform_ids[3u],                           \
                                                         expected_global_transform_changed_again_3))

    TRANSFORM_GLOBAL_TEST (2)
}

struct test_global_3_state_t
{
    struct test_utility_queries_3_t utility;
};

TEST_UNIVERSE_TRANSFORM_API KAN_UM_MUTATOR_EXECUTE (test_global_3)
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

KAN_TEST_CASE (global_2) { test_global (kan_string_intern ("test_global_2")); }

KAN_TEST_CASE (global_3) { test_global (kan_string_intern ("test_global_3")); }
