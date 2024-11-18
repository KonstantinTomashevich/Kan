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

static kan_bool_t subsystem_initialized = KAN_FALSE;
static struct kan_atomic_int_t subsystem_initialized_lock = {.value = 0};

static void shutdown_sdl (void)
{
    SDL_QuitSubSystem (SDL_INIT_TIMER);
}

static void ensure_sdl_ready (void)
{
    if (!subsystem_initialized)
    {
        kan_atomic_int_lock (&subsystem_initialized_lock);
        if (!subsystem_initialized)
        {
            ensure_sdl_allocation_adapter_installed ();
            if (!SDL_WasInit (SDL_INIT_TIMER))
            {
                if (SDL_InitSubSystem (SDL_INIT_TIMER) != 0)
                {
                    kan_critical_error ("Failed to initialize SDL time subsystem.", __FILE__, __LINE__);
                }
                else
                {
                    atexit (shutdown_sdl);
                }
            }

            subsystem_initialized = KAN_TRUE;
        }

        kan_atomic_int_unlock (&subsystem_initialized_lock);
    }
}

kan_time_size_t kan_platform_get_elapsed_nanoseconds (void)
{
    ensure_sdl_ready ();
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
    ensure_sdl_ready ();
    SDL_DelayNS (nanoseconds);
}
