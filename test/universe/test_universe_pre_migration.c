#include <test_universe_pre_migration_api.h>

#include <kan/testing/testing.h>
#include <kan/universe/macro.h>
#include <kan/universe/universe.h>

struct migration_counters_singleton_t
{
    kan_instance_size_t pre_migration_scheduler_counter;
    kan_instance_size_t pre_migration_mutator_counter;
};

TEST_UNIVERSE_PRE_MIGRATION_API void migration_counters_singleton_init (struct migration_counters_singleton_t *data)
{
    data->pre_migration_scheduler_counter = 0u;
    data->pre_migration_mutator_counter = 0u;
}

struct migration_scheduler_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (migration_scheduler)
    KAN_UM_BIND_STATE (migration_scheduler, state)
};

TEST_UNIVERSE_PRE_MIGRATION_API void kan_universe_scheduler_execute_migration_scheduler (
    kan_universe_scheduler_interface_t interface, struct migration_scheduler_state_t *state)
{
    {
        KAN_UMI_SINGLETON_WRITE (counters, migration_counters_singleton_t)
        KAN_TEST_CHECK (counters->pre_migration_scheduler_counter == counters->pre_migration_mutator_counter)
        ++counters->pre_migration_scheduler_counter;
    }

    // We need to close all accesses before running pipelines.
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern ("update"));

    {
        KAN_UMI_SINGLETON_WRITE (counters, migration_counters_singleton_t)
        KAN_TEST_ASSERT (counters)
        KAN_TEST_CHECK (counters->pre_migration_scheduler_counter == counters->pre_migration_mutator_counter)
    }
}

struct migration_mutator_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (migration_mutator)
    KAN_UM_BIND_STATE (migration_mutator, state)
};

TEST_UNIVERSE_PRE_MIGRATION_API void kan_universe_mutator_execute_migration_mutator (
    kan_cpu_job_t job, struct migration_mutator_state_t *state)
{
    KAN_UMI_SINGLETON_WRITE (counters, migration_counters_singleton_t)
    ++counters->pre_migration_mutator_counter;
}
