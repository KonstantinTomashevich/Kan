#include <kan/universe/macro.h>
#include <kan/universe/universe.h>
#include <kan/universe_trivial_scheduler/universe_trivial_scheduler.h>

struct trivial_scheduler_state_t
{
    kan_interned_string_t pipeline_name;
};

UNIVERSE_TRIVIAL_SCHEDULER_API void trivial_scheduler_state_init (struct trivial_scheduler_state_t *instance)
{
    instance->pipeline_name = kan_string_intern (KAN_UNIVERSE_TRIVIAL_SCHEDULER_PIPELINE_NAME);
}

UNIVERSE_TRIVIAL_SCHEDULER_API KAN_UM_SCHEDULER_EXECUTE (trivial)
{
    kan_universe_scheduler_interface_run_pipeline (interface, state->pipeline_name);
    kan_universe_scheduler_interface_update_all_children (interface);
}
