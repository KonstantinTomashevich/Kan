#include <test_universe_post_migration_api.h>

#include <kan/testing/testing.h>
#include <kan/universe/universe.h>

// We need those defines to trick the reflection generator and generate reflection with the fake equal names.
#define migration_counters_singleton_init migration_counters_singleton_init_1
#define kan_universe_scheduler_execute_migration_scheduler kan_universe_scheduler_execute_migration_scheduler_1
#define kan_universe_mutator_execute_migration_mutator kan_universe_mutator_execute_migration_mutator_1

struct migration_counters_singleton_t
{
    uint64_t pre_migration_scheduler_counter;
    uint64_t pre_migration_mutator_counter;
    uint64_t post_migration_scheduler_counter;
    uint64_t post_migration_mutator_counter;
};

TEST_UNIVERSE_POST_MIGRATION_API void migration_counters_singleton_init (struct migration_counters_singleton_t *data)
{
    data->pre_migration_scheduler_counter = 0u;
    data->pre_migration_mutator_counter = 0u;
    data->post_migration_scheduler_counter = 0u;
    data->post_migration_mutator_counter = 0u;
}

struct migration_scheduler_state_t
{
    struct kan_repository_singleton_write_query_t write__migration_counters_singleton;
};

TEST_UNIVERSE_POST_MIGRATION_API void kan_universe_scheduler_execute_migration_scheduler (
    kan_universe_scheduler_interface_t interface, struct migration_scheduler_state_t *state)
{
    kan_repository_singleton_write_access_t access =
        kan_repository_singleton_write_query_execute (&state->write__migration_counters_singleton);

    struct migration_counters_singleton_t *counters =
        (struct migration_counters_singleton_t *) kan_repository_singleton_write_access_resolve (access);
    KAN_TEST_ASSERT (counters)

    KAN_TEST_CHECK (counters->post_migration_scheduler_counter == counters->post_migration_mutator_counter)
    ++counters->post_migration_scheduler_counter;
    kan_repository_singleton_write_access_close (access);

    // We need to close all accesses before running pipelines.
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern ("update"));

    access = kan_repository_singleton_write_query_execute (&state->write__migration_counters_singleton);
    counters = (struct migration_counters_singleton_t *) kan_repository_singleton_write_access_resolve (access);
    KAN_TEST_ASSERT (counters)
    KAN_TEST_CHECK (counters->post_migration_scheduler_counter == counters->post_migration_mutator_counter)
    KAN_TEST_CHECK (counters->pre_migration_scheduler_counter == 2u)
    KAN_TEST_CHECK (counters->pre_migration_mutator_counter == 2u)
    kan_repository_singleton_write_access_close (access);
}

struct migration_mutator_state_t
{
    struct kan_repository_singleton_write_query_t write__migration_counters_singleton;
};

TEST_UNIVERSE_POST_MIGRATION_API void kan_universe_mutator_execute_migration_mutator (
    kan_cpu_job_t job, struct migration_mutator_state_t *state)
{
    kan_repository_singleton_write_access_t access =
        kan_repository_singleton_write_query_execute (&state->write__migration_counters_singleton);

    struct migration_counters_singleton_t *counters =
        (struct migration_counters_singleton_t *) kan_repository_singleton_write_access_resolve (access);
    KAN_TEST_ASSERT (counters)

    ++counters->post_migration_mutator_counter;
    kan_repository_singleton_write_access_close (access);

    kan_cpu_job_release (job);
}
