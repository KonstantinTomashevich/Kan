#include <test_universe_post_migration_api.h>

#include <kan/testing/testing.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe/universe.h>

KAN_REFLECTION_EXPLICIT_INIT_FUNCTOR (migration_counters_singleton_init_migrated)
struct migration_counters_singleton_t
{
    kan_instance_size_t pre_migration_scheduler_counter;
    kan_instance_size_t pre_migration_mutator_counter;
    kan_instance_size_t post_migration_scheduler_counter;
    kan_instance_size_t post_migration_mutator_counter;
};

TEST_UNIVERSE_POST_MIGRATION_API void migration_counters_singleton_init_migrated (
    struct migration_counters_singleton_t *data)
{
    data->pre_migration_scheduler_counter = 0u;
    data->pre_migration_mutator_counter = 0u;
    data->post_migration_scheduler_counter = 0u;
    data->post_migration_mutator_counter = 0u;
}

struct migration_scheduler_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (migration_scheduler)
    KAN_UP_BIND_STATE (migration_scheduler, state)
};

KAN_REFLECTION_EXPLICIT_REGISTRATION_NAME (kan_universe_scheduler_execute_migration_scheduler)
TEST_UNIVERSE_POST_MIGRATION_API void kan_universe_scheduler_execute_migration_scheduler_migrated (
    kan_universe_scheduler_interface_t interface, struct migration_scheduler_state_t *state)
{
    {
        KAN_UP_SINGLETON_WRITE (counters, migration_counters_singleton_t)
        {
            KAN_TEST_CHECK (counters->post_migration_scheduler_counter == counters->post_migration_mutator_counter)
            ++counters->post_migration_scheduler_counter;
        }
    }

    // We need to close all accesses before running pipelines.
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern ("update"));

    {
        KAN_UP_SINGLETON_WRITE (counters, migration_counters_singleton_t)
        {
            KAN_TEST_CHECK (counters->post_migration_scheduler_counter == counters->post_migration_mutator_counter)
            KAN_TEST_CHECK (counters->pre_migration_scheduler_counter == 2u)
            KAN_TEST_CHECK (counters->pre_migration_mutator_counter == 2u)
        }
    }
}

struct migration_mutator_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (migration_mutator)
    KAN_UP_BIND_STATE (migration_mutator, state)
};

KAN_REFLECTION_EXPLICIT_REGISTRATION_NAME (kan_universe_mutator_execute_migration_mutator)
TEST_UNIVERSE_POST_MIGRATION_API void kan_universe_mutator_execute_migration_mutator_migrated (
    kan_cpu_job_t job, struct migration_mutator_state_t *state)
{
    KAN_UP_SINGLETON_WRITE (counters, migration_counters_singleton_t)
    {
        ++counters->post_migration_mutator_counter;
    }

    KAN_UP_MUTATOR_RETURN;
}
