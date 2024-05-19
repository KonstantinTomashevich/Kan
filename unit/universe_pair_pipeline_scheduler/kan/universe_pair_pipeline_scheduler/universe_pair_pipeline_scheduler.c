#include <kan/platform/precise_time.h>
#include <kan/universe/universe.h>
#include <kan/universe_pair_pipeline_scheduler/universe_pair_pipeline_scheduler.h>
#include <kan/universe_time/universe_time.h>

struct universe_pair_pipeline_scheduler_state_t
{
    struct kan_repository_singleton_read_query_t read__kan_pair_pipeline_settings_singleton;
    struct kan_repository_singleton_write_query_t write__kan_time_singleton;

    uint64_t last_update_time_ns;
    kan_interned_string_t logical_pipeline_name;
    kan_interned_string_t visual_pipeline_name;
};

UNIVERSE_PAIR_PIPELINE_SCHEDULER_API void universe_pair_pipeline_scheduler_state_init (
    struct universe_pair_pipeline_scheduler_state_t *instance)
{
    instance->last_update_time_ns = UINT64_MAX;
    instance->logical_pipeline_name = kan_string_intern (KAN_UNIVERSE_PAIR_PIPELINE_SCHEDULER_LOGICAL_PIPELINE_NAME);
    instance->visual_pipeline_name = kan_string_intern (KAN_UNIVERSE_PAIR_PIPELINE_SCHEDULER_VISUAL_PIPELINE_NAME);
}

UNIVERSE_PAIR_PIPELINE_SCHEDULER_API void kan_universe_scheduler_execute_pair_pipeline (
    kan_universe_scheduler_interface_t interface, struct universe_pair_pipeline_scheduler_state_t *state)
{
    const uint64_t current_time = kan_platform_get_elapsed_nanoseconds ();
    // First update is intentionally zero.
    const uint64_t delta_ns = state->last_update_time_ns == UINT64_MAX ? 0u : current_time - state->last_update_time_ns;
    state->last_update_time_ns = current_time;

    kan_repository_singleton_read_access_t settings_access =
        kan_repository_singleton_read_query_execute (&state->read__kan_pair_pipeline_settings_singleton);

    const struct kan_pair_pipeline_settings_singleton_t *settings =
        kan_repository_singleton_read_access_resolve (settings_access);

    const uint64_t logical_step_ns = settings->logical_time_step_ns;
    const uint64_t logical_advance_max_ns = settings->max_logical_advance_time_ns;
    kan_repository_singleton_read_access_close (settings_access);

    // Update visual time.

    kan_repository_singleton_write_access_t time_access =
        kan_repository_singleton_write_query_execute (&state->write__kan_time_singleton);
    struct kan_time_singleton_t *time = kan_repository_singleton_write_access_resolve (time_access);

    const uint64_t scaled_delta_ns = (uint64_t) (((float) delta_ns) * time->scale);
    time->visual_time_ns += scaled_delta_ns;
    time->visual_delta_ns = scaled_delta_ns;
    time->visual_unscaled_delta_ns = delta_ns;
    kan_repository_singleton_write_access_close (time_access);

    // Advance logical time until logical time is ahead.
    const uint64_t logical_advance_begin_ns = kan_platform_get_elapsed_nanoseconds ();
    const uint64_t logical_advance_begin_time = time->logical_time_ns;

    while (KAN_TRUE)
    {
        time_access = kan_repository_singleton_write_query_execute (&state->write__kan_time_singleton);
        time = kan_repository_singleton_write_access_resolve (time_access);
        const uint64_t advance_time_spent = kan_platform_get_elapsed_nanoseconds () - logical_advance_begin_ns;

        // Critical case: unable to advance properly due to not performant enough hardware.
        // Slow down to avoid death spiral.
        if (advance_time_spent > logical_advance_max_ns)
        {
            time->visual_time_ns = time->logical_time_ns;
            time->visual_delta_ns = time->logical_time_ns - logical_advance_begin_time;

            kan_repository_singleton_write_access_close (time_access);
            break;
        }

        if (time->logical_time_ns <= time->visual_time_ns)
        {
            time->logical_time_ns += logical_step_ns;
            time->logical_delta_ns = logical_step_ns;
            kan_repository_singleton_write_access_close (time_access);
            kan_universe_scheduler_interface_run_pipeline (interface, state->logical_pipeline_name);
        }
        else
        {
            kan_repository_singleton_write_access_close (time_access);
            break;
        }
    }

    // Finally run visual pipeline.

    kan_universe_scheduler_interface_run_pipeline (interface, state->visual_pipeline_name);
    kan_universe_scheduler_interface_update_all_children (interface);
}
