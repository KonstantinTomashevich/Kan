#include <SDL3/SDL_cpuinfo.h>

#include <kan/platform/hardware.h>

uint64_t kan_platform_get_cpu_count (void)
{
    return (uint64_t) SDL_GetCPUCount ();
}

uint64_t kan_platform_get_random_access_memory (void)
{
    return (uint64_t) SDL_GetSystemRAM ();
}
