#pragma once

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/memory_profiler/allocation_group.h>

KAN_C_HEADER_BEGIN

struct allocation_group_t
{
    uint64_t allocated;
    struct allocation_group_t *parent;
    struct allocation_group_t *next_on_level;
    struct allocation_group_t *first_child;
    char name[0u];
};

_Static_assert (sizeof (kan_allocation_group_t) >= sizeof (uintptr_t), "Allocation group handle can fit pointer.");

inline struct allocation_group_t *retrieve_allocation_group (kan_allocation_group_t group)
{
    return (struct allocation_group_t *) group;
}

struct allocation_group_t *retrieve_root_allocation_group ();

struct allocation_group_t *create_allocation_group_unguarded (struct allocation_group_t *parent,
                                                              struct allocation_group_t *next_on_level,
                                                              const char *name);

void lock_memory_profiling_context ();

void unlock_memory_profiling_context ();

KAN_C_HEADER_END
