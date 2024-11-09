#include <SDL3/SDL_cpuinfo.h>

#include <kan/platform/hardware.h>

kan_instance_size_t kan_platform_get_cpu_count (void)
{
    return (uint64_t) SDL_GetCPUCount ();
}

kan_memory_size_t kan_platform_get_random_access_memory (void)
{
    return (kan_memory_size_t) SDL_GetSystemRAM ();
}
