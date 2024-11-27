#include <stdlib.h>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_time.h>
#include <SDL3/SDL_timer.h>

#include <kan/api_common/core_types.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/platform/precise_time.h>
#include <kan/platform/sdl_allocation_adapter.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (precise_time);

kan_time_size_t kan_platform_get_elapsed_nanoseconds (void)
{
    return (kan_time_size_t) SDL_GetTicksNS ();
}

kan_time_size_t kan_platform_get_epoch_nanoseconds_utc (void)
{
    SDL_Time output;
    if (SDL_GetCurrentTime (&output) == 0)
    {
        return (kan_time_size_t) output;
    }

    KAN_LOG (precise_time, KAN_LOG_ERROR, "Failed to get epoch time: %s.", SDL_GetError ());
    return 0u;
}

void kan_platform_sleep (kan_time_offset_t nanoseconds)
{
    SDL_DelayNS (nanoseconds);
}
