#include <stdlib.h>

#include <SDL_init.h>
#include <SDL_timer.h>

#include <kan/api_common/bool.h>
#include <kan/error/critical.h>
#include <kan/platform/precise_time.h>
#include <kan/platform/sdl_allocation_adapter.h>
#include <kan/threading/atomic.h>

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

uint64_t kan_platform_get_elapsed_nanoseconds (void)
{
    ensure_sdl_ready ();
    return (uint64_t) SDL_GetTicksNS ();
}

void kan_platform_sleep (uint64_t nanoseconds)
{
    ensure_sdl_ready ();
    SDL_DelayNS (nanoseconds);
}
