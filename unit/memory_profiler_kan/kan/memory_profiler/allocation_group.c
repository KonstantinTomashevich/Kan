#define _CRT_SECURE_NO_WARNINGS

#include <string.h>

#include <kan/error/critical.h>
#include <kan/memory/allocation.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/memory_profiler/core.h>
#include <kan/threading/atomic.h>
#include <kan/threading/thread.h>

kan_allocation_group_t kan_allocation_group_root (void)
{
    lock_memory_profiling_context ();
    kan_allocation_group_t result =
        KAN_HANDLE_SET (kan_allocation_group_t, retrieve_root_allocation_group_unguarded ());
    unlock_memory_profiling_context ();
    return result;
}

kan_allocation_group_t kan_allocation_group_get_child (kan_allocation_group_t parent, const char *name)
{
    if (!KAN_HANDLE_IS_VALID (parent))
    {
        return KAN_ALLOCATION_GROUP_IGNORE;
    }

    lock_memory_profiling_context ();
    struct allocation_group_t *parent_group = KAN_HANDLE_GET (parent);
    KAN_ASSERT (parent_group)
    struct allocation_group_t *child = parent_group->first_child;

    while (child)
    {
        if (strcmp (child->name, name) == 0)
        {
            unlock_memory_profiling_context ();
            return KAN_HANDLE_SET (kan_allocation_group_t, child);
        }

        child = child->next_on_level;
    }

    child = create_allocation_group_unguarded (parent_group->first_child, name);
    parent_group->first_child = child;
    unlock_memory_profiling_context ();
    return KAN_HANDLE_SET (kan_allocation_group_t, child);
}

void kan_allocation_group_allocate (kan_allocation_group_t group, uint64_t amount)
{
    if (!KAN_HANDLE_IS_VALID (group))
    {
        return;
    }

    lock_memory_profiling_context ();
    struct allocation_group_t *allocation_group = KAN_HANDLE_GET (group);
    allocation_group->allocated_here += amount;
    queue_allocate_event_unguarded (allocation_group, amount);
    unlock_memory_profiling_context ();
}

void kan_allocation_group_free (kan_allocation_group_t group, uint64_t amount)
{
    if (!KAN_HANDLE_IS_VALID (group))
    {
        return;
    }

    lock_memory_profiling_context ();
    struct allocation_group_t *allocation_group = KAN_HANDLE_GET (group);
    KAN_ASSERT (allocation_group->allocated_here >= amount)
    allocation_group->allocated_here -= amount;
    queue_free_event_unguarded (allocation_group, amount);
    unlock_memory_profiling_context ();
}

void kan_allocation_group_marker (kan_allocation_group_t group, const char *name)
{
    if (!KAN_HANDLE_IS_VALID (group))
    {
        return;
    }

    lock_memory_profiling_context ();
    queue_marker_event_unguarded (KAN_HANDLE_GET (group), name);
    unlock_memory_profiling_context ();
}

#define KAN_ALLOCATION_GROUP_STACK_SIZE 32u

static struct kan_atomic_int_t thread_local_storage_initialization_lock;
static kan_thread_local_storage_t thread_local_storage = KAN_TYPED_ID_32_INITIALIZE_INVALID;

struct thread_local_storage_stack_t
{
    uint64_t stack_size;
    kan_allocation_group_t stack[KAN_ALLOCATION_GROUP_STACK_SIZE];
};

static struct thread_local_storage_stack_t *allocate_thread_local_storage_stack (void)
{
    struct thread_local_storage_stack_t *storage =
        (struct thread_local_storage_stack_t *) kan_allocate_general_no_profiling (
            sizeof (struct thread_local_storage_stack_t), _Alignof (struct thread_local_storage_stack_t));

    storage->stack_size = 0u;
    return storage;
}

static void free_thread_local_storage_stack (void *memory)
{
    kan_free_general_no_profiling (memory);
}

static struct thread_local_storage_stack_t *ensure_thread_local_storage (void)
{
    if (!KAN_TYPED_ID_32_IS_VALID (thread_local_storage))
    {
        kan_atomic_int_lock (&thread_local_storage_initialization_lock);
        if (!KAN_TYPED_ID_32_IS_VALID (thread_local_storage))
        {
            thread_local_storage = kan_thread_local_storage_create ();
        }

        kan_atomic_int_unlock (&thread_local_storage_initialization_lock);
    }

    if (!kan_thread_local_storage_get (thread_local_storage))
    {
        kan_thread_local_storage_set (thread_local_storage, allocate_thread_local_storage_stack (),
                                      free_thread_local_storage_stack);
    }

    return kan_thread_local_storage_get (thread_local_storage);
}

kan_allocation_group_t kan_allocation_group_stack_get (void)
{
    struct thread_local_storage_stack_t *storage = ensure_thread_local_storage ();
    if (storage->stack_size == 0u)
    {
        return kan_allocation_group_root ();
    }

    return storage->stack[storage->stack_size - 1u];
}

void kan_allocation_group_stack_push (kan_allocation_group_t group)
{
    struct thread_local_storage_stack_t *storage = ensure_thread_local_storage ();
    KAN_ASSERT (storage->stack_size < KAN_ALLOCATION_GROUP_STACK_SIZE)
    storage->stack[storage->stack_size] = group;
    ++storage->stack_size;
}

void kan_allocation_group_stack_pop (void)
{
    struct thread_local_storage_stack_t *storage = ensure_thread_local_storage ();
    KAN_ASSERT (storage->stack_size > 0)
    --storage->stack_size;
}
