#include <kan/universe_time/universe_time.h>

void kan_time_singleton_init (struct kan_time_singleton_t *instance)
{
    instance->logical_time_ns = 0u;
    instance->logical_delta_ns = 0u;

    instance->visual_time_ns = 0u;
    instance->visual_delta_ns = 0u;
    instance->visual_unscaled_delta_ns = 0u;

    instance->scale = 1.0f;
}
