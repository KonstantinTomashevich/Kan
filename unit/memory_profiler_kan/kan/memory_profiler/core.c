#define _CRT_SECURE_NO_WARNINGS

#include <string.h>

#include <kan/memory/allocation.h>
#include <kan/memory_profiler/core.h>
#include <kan/threading/atomic.h>

static struct kan_atomic_int_t memory_profiling_lock = {.value = 0u};

static struct allocation_group_t *root_allocation_group = NULL;

struct allocation_group_t *retrieve_root_allocation_group ()
{
    lock_memory_profiling_context ();
    if (!root_allocation_group)
    {
        root_allocation_group = create_allocation_group_unguarded (NULL, NULL, "root");
    }

    unlock_memory_profiling_context ();
    return root_allocation_group;
}

struct allocation_group_t *create_allocation_group_unguarded (struct allocation_group_t *parent,
                                                    struct allocation_group_t *next_on_level,
                                                    const char *name)
{
    struct allocation_group_t *group = (struct allocation_group_t *) kan_allocate_general_no_profiling (
        sizeof (struct allocation_group_t) + strlen (name) + 1u, _Alignof (struct allocation_group_t));
    group->allocated = 0u;
    group->parent = parent;
    group->next_on_level = next_on_level;
    group->first_child = NULL;
    strcpy (group->name, name);
    return group;
}

void lock_memory_profiling_context ()
{
    kan_atomic_int_lock (&memory_profiling_lock);
}

void unlock_memory_profiling_context ()
{
    kan_atomic_int_unlock (&memory_profiling_lock);
}
