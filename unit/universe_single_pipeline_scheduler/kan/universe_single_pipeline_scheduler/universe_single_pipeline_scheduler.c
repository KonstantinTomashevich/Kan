#include <kan/precise_time/precise_time.h>
#include <kan/universe/macro.h>
#include <kan/universe/universe.h>
#include <kan/universe_single_pipeline_scheduler/universe_single_pipeline_scheduler.h>
#include <kan/universe_time/universe_time.h>

struct universe_single_pipeline_scheduler_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (universe_single_pipeline_scheduler)
    KAN_UP_BIND_STATE (universe_single_pipeline_scheduler, state)

    kan_time_size_t last_update_time_ns;
    kan_interned_string_t pipeline_name;
};

UNIVERSE_SINGLE_PIPELINE_SCHEDULER_API void universe_single_pipeline_scheduler_state_init (
    struct universe_single_pipeline_scheduler_state_t *instance)
{
    instance->last_update_time_ns = KAN_INT_MAX (kan_time_size_t);
    instance->pipeline_name = kan_string_intern (KAN_UNIVERSE_SINGLE_PIPELINE_SCHEDULER_PIPELINE_NAME);
}

UNIVERSE_SINGLE_PIPELINE_SCHEDULER_API void kan_universe_scheduler_execute_single_pipeline (
    kan_universe_scheduler_interface_t interface, struct universe_single_pipeline_scheduler_state_t *state)
{
    const kan_time_size_t current_time = kan_precise_time_get_elapsed_nanoseconds ();
    // First update is intentionally zero.
    const kan_time_offset_t delta_ns =
        (kan_time_offset_t) (state->last_update_time_ns == KAN_INT_MAX (kan_time_size_t) ?
                                 0u :
                                 current_time - state->last_update_time_ns);
    state->last_update_time_ns = current_time;

    {
        KAN_UP_SINGLETON_WRITE (time, kan_time_singleton_t)

        const kan_time_offset_t scaled_delta_ns = (kan_time_offset_t) (((float) delta_ns) * time->scale);
        time->logical_time_ns += scaled_delta_ns;
        time->logical_delta_ns = scaled_delta_ns;

        time->visual_time_ns += scaled_delta_ns;
        time->visual_delta_ns = scaled_delta_ns;
        time->visual_unscaled_delta_ns = delta_ns;
    }

    kan_universe_scheduler_interface_run_pipeline (interface, state->pipeline_name);
    kan_universe_scheduler_interface_update_all_children (interface);
}

struct universe_single_pipeline_no_time_scheduler_state_t
{
    kan_interned_string_t pipeline_name;
};

UNIVERSE_SINGLE_PIPELINE_SCHEDULER_API void universe_single_pipeline_no_time_scheduler_state_init (
    struct universe_single_pipeline_no_time_scheduler_state_t *instance)
{
    instance->pipeline_name = kan_string_intern (KAN_UNIVERSE_SINGLE_PIPELINE_SCHEDULER_PIPELINE_NAME);
}

UNIVERSE_SINGLE_PIPELINE_SCHEDULER_API void kan_universe_scheduler_execute_single_pipeline_no_time (
    kan_universe_scheduler_interface_t interface, struct universe_single_pipeline_no_time_scheduler_state_t *state)
{
    kan_universe_scheduler_interface_run_pipeline (interface, state->pipeline_name);
    kan_universe_scheduler_interface_update_all_children (interface);
}
