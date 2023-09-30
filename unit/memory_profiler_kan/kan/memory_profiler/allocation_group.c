#define _CRT_SECURE_NO_WARNINGS

#include <string.h>

#include <kan/error/critical.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/memory_profiler/core.h>

kan_allocation_group_t kan_allocation_group_root ()
{
    lock_memory_profiling_context ();
    kan_allocation_group_t result = (kan_allocation_group_t) retrieve_root_allocation_group_unguarded ();
    unlock_memory_profiling_context ();
    return result;
}

kan_allocation_group_t kan_allocation_group_get_child (kan_allocation_group_t parent, const char *name)
{
    if (parent == KAN_ALLOCATION_GROUP_IGNORE)
    {
        return KAN_ALLOCATION_GROUP_IGNORE;
    }

    lock_memory_profiling_context ();
    struct allocation_group_t *parent_group = retrieve_allocation_group (parent);
    KAN_ASSERT (parent_group);
    struct allocation_group_t *child = parent_group->first_child;

    while (child)
    {
        if (strcmp (child->name, name) == 0)
        {
            unlock_memory_profiling_context ();
            return (kan_allocation_group_t) child;
        }

        child = child->next_on_level;
    }

    child = create_allocation_group_unguarded (parent_group->first_child, name);
    parent_group->first_child = child;
    unlock_memory_profiling_context ();
    return (kan_allocation_group_t) child;
}

void kan_allocation_group_allocate (kan_allocation_group_t group, uint64_t amount)
{
    if (group == KAN_ALLOCATION_GROUP_IGNORE)
    {
        return;
    }

    lock_memory_profiling_context ();
    struct allocation_group_t *allocation_group = retrieve_allocation_group (group);
    allocation_group->allocated_here += amount;
    queue_allocate_event_unguarded (group, amount);
    unlock_memory_profiling_context ();
}

void kan_allocation_group_free (kan_allocation_group_t group, uint64_t amount)
{
    if (group == KAN_ALLOCATION_GROUP_IGNORE)
    {
        return;
    }

    lock_memory_profiling_context ();
    struct allocation_group_t *allocation_group = retrieve_allocation_group (group);
    KAN_ASSERT (allocation_group->allocated_here >= amount)
    allocation_group->allocated_here -= amount;
    queue_free_event_unguarded (group, amount);
    unlock_memory_profiling_context ();
}

void kan_allocation_group_marker (kan_allocation_group_t group, const char *name)
{
    if (group == KAN_ALLOCATION_GROUP_IGNORE)
    {
        return;
    }

    lock_memory_profiling_context ();
    queue_marker_event_unguarded (group, name);
    unlock_memory_profiling_context ();
}

#define KAN_ALLOCATION_GROUP_STACK_SIZE 32u

static _Thread_local uint64_t stack_size = 0u;
static _Thread_local kan_allocation_group_t stack[KAN_ALLOCATION_GROUP_STACK_SIZE];

kan_allocation_group_t kan_allocation_group_stack_get ()
{
    if (stack_size == 0u)
    {
        return kan_allocation_group_root ();
    }

    return stack[stack_size - 1u];
}

void kan_allocation_group_stack_push (kan_allocation_group_t group)
{
    KAN_ASSERT (stack_size < KAN_ALLOCATION_GROUP_STACK_SIZE)
    stack[stack_size] = group;
    ++stack_size;
}

void kan_allocation_group_stack_pop ()
{
    KAN_ASSERT (stack_size > 0)
    --stack_size;
}
