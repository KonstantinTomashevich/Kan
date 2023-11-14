#include <stdlib.h>

#include <SDL_init.h>
#include <SDL_timer.h>

#include <kan/api_common/bool.h>
#include <kan/error/critical.h>
#include <kan/platform/precise_time.h>
#include <kan/threading/atomic.h>

static kan_bool_t subsystem_initialized = KAN_FALSE;
static struct kan_atomic_int_t subsystem_initialized_lock = {.value = 0};

static void shutdown_sdl (void)
{
    SDL_QuitSubSystem (SDL_INIT_TIMER);
}

uint64_t kan_platform_get_elapsed_nanoseconds (void)
{
    if (!subsystem_initialized)
    {
        kan_atomic_int_lock (&subsystem_initialized_lock);
        if (!subsystem_initialized)
        {
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

    return (uint64_t) SDL_GetTicksNS ();
}
