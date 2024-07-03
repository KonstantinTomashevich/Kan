#include <kan/api_common/mute_warnings.h>

#include <string.h>

KAN_MUTE_UNINITIALIZED_WARNINGS_BEGIN
#include <SDL3/SDL_stdinc.h>
KAN_MUTE_UNINITIALIZED_WARNINGS_END

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/memory/allocation.h>
#include <kan/platform/sdl_allocation_adapter.h>
#include <kan/threading/atomic.h>

static struct kan_atomic_int_t initialization_lock = {0};
static kan_bool_t initialized = KAN_FALSE;
static kan_allocation_group_t sdl_allocation_group;

static void *sdl_malloc (size_t size)
{
    const uint64_t real_size = kan_apply_alignment (size + sizeof (uint64_t), _Alignof (uint64_t));
    uint64_t *data = kan_allocate_general (sdl_allocation_group, real_size, _Alignof (uint64_t));
    *data = size;
    return data + 1u;
}

static void *sdl_calloc (size_t count, size_t size)
{
    size_t total = count * size;
    size_t *memory = sdl_malloc (total);
    memset (memory, 0, total);
    return memory;
}

static void sdl_free (void *memory)
{
    uint64_t *real_memory = ((uint64_t *) memory) - 1u;
    const uint64_t real_size = kan_apply_alignment (*real_memory + sizeof (uint64_t), _Alignof (uint64_t));
    kan_free_general (sdl_allocation_group, real_memory, real_size);
}

static void *sdl_realloc (void *memory, uint64_t new_size)
{
    if (memory)
    {
        if (new_size > 0u)
        {
            const uint64_t *real_memory = ((uint64_t *) memory) - 1u;
            void *new_memory = sdl_malloc (new_size);
            const uint64_t old_size = *real_memory;
            memcpy (new_memory, memory, KAN_MIN (old_size, new_size));
            sdl_free (memory);
            return new_memory;
        }

        sdl_free (memory);
        return NULL;
    }

    return sdl_malloc (new_size);
}

void ensure_sdl_allocation_adapter_installed (void)
{
    if (!initialized)
    {
        kan_atomic_int_lock (&initialization_lock);
        if (!initialized)
        {
            sdl_allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "sdl");

            // Do not use custom allocators if profiling is disabled.
            if (sdl_allocation_group != KAN_ALLOCATION_GROUP_IGNORE)
            {
                SDL_SetMemoryFunctions (sdl_malloc, sdl_calloc, sdl_realloc, sdl_free);
            }

            initialized = KAN_TRUE;
        }

        kan_atomic_int_unlock (&initialization_lock);
    }
}
