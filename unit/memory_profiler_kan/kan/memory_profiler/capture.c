#include <stddef.h>

#include <kan/error/critical.h>
#include <kan/memory/allocation.h>
#include <kan/memory_profiler/capture.h>
#include <kan/memory_profiler/core.h>

struct captured_allocation_group_t
{
    kan_memory_size_t allocated_here;
    kan_memory_size_t allocated_total;
    struct captured_allocation_group_t *next_on_level;
    struct captured_allocation_group_t *first_child;
    kan_allocation_group_t source;
};

const char *kan_captured_allocation_group_get_name (kan_captured_allocation_group_t group)
{
    struct captured_allocation_group_t *captured = KAN_HANDLE_GET (group);
    struct allocation_group_t *source = KAN_HANDLE_GET (captured->source);
    return source->name;
}

kan_allocation_group_t kan_captured_allocation_group_get_source (kan_captured_allocation_group_t group)
{
    struct captured_allocation_group_t *captured = KAN_HANDLE_GET (group);
    return captured->source;
}

kan_memory_size_t kan_captured_allocation_group_get_total_allocated (kan_captured_allocation_group_t group)
{
    struct captured_allocation_group_t *captured = KAN_HANDLE_GET (group);
    return captured->allocated_total;
}

kan_memory_size_t kan_captured_allocation_group_get_directly_allocated (kan_captured_allocation_group_t group)
{
    struct captured_allocation_group_t *captured = KAN_HANDLE_GET (group);
    return captured->allocated_here;
}

static_assert (sizeof (kan_captured_allocation_group_iterator_t) >= sizeof (uintptr_t),
               "Captured group iterator can fit pointer.");

kan_captured_allocation_group_iterator_t kan_captured_allocation_group_children_begin (
    kan_captured_allocation_group_t group)
{
    struct captured_allocation_group_t *captured = KAN_HANDLE_GET (group);
    return KAN_HANDLE_SET (kan_captured_allocation_group_iterator_t, captured->first_child);
}

kan_captured_allocation_group_iterator_t kan_captured_allocation_group_children_next (
    kan_captured_allocation_group_iterator_t current)
{
    struct captured_allocation_group_t *captured = KAN_HANDLE_GET (current);
    return KAN_HANDLE_SET (kan_captured_allocation_group_iterator_t, captured->next_on_level);
}

kan_captured_allocation_group_t kan_captured_allocation_group_children_get (
    kan_captured_allocation_group_iterator_t current)
{
    return KAN_HANDLE_TRANSIT (kan_captured_allocation_group_t, current);
}

static void captured_group_destroy (struct captured_allocation_group_t *group)
{
    struct captured_allocation_group_t *child = group->first_child;
    while (child)
    {
        struct captured_allocation_group_t *next = child->next_on_level;
        captured_group_destroy (child);
        child = next;
    }

    kan_free_general_no_profiling (group);
}

void kan_captured_allocation_group_destroy (kan_captured_allocation_group_t group)
{
    struct captured_allocation_group_t *captured = KAN_HANDLE_GET (group);
    KAN_ASSERT (!captured->next_on_level)
    captured_group_destroy (captured);
}

const struct kan_allocation_group_event_t *kan_allocation_group_event_iterator_get (
    kan_allocation_group_event_iterator_t iterator)
{
    lock_memory_profiling_context ();
    const struct kan_allocation_group_event_t *result = event_iterator_get_unguarded (iterator);
    unlock_memory_profiling_context ();
    return result;
}

kan_allocation_group_event_iterator_t kan_allocation_group_event_iterator_advance (
    kan_allocation_group_event_iterator_t iterator)
{
    lock_memory_profiling_context ();
    kan_allocation_group_event_iterator_t result = event_iterator_advance_unguarded (iterator);
    unlock_memory_profiling_context ();
    return result;
}

void kan_allocation_group_event_iterator_destroy (kan_allocation_group_event_iterator_t iterator)
{
    lock_memory_profiling_context ();
    event_iterator_destroy_unguarded (iterator);
    unlock_memory_profiling_context ();
}

static struct captured_allocation_group_t *capture_allocation_group_snapshot (struct allocation_group_t *group)
{
    // We cannot use batched allocators inside memory profiling as they're reporting reserved memory
    // and it results in deadlock when new page is being allocated to hold new profiling objects.
    struct captured_allocation_group_t *captured = kan_allocate_general_no_profiling (
        sizeof (struct captured_allocation_group_t), alignof (struct captured_allocation_group_t));

    captured->allocated_here = group->allocated_here;
    captured->allocated_total = group->allocated_here;
    captured->next_on_level = NULL;
    captured->first_child = NULL;
    captured->source = KAN_HANDLE_SET (kan_allocation_group_t, group);

    struct allocation_group_t *real_child = group->first_child;
    while (real_child)
    {
        struct captured_allocation_group_t *child = capture_allocation_group_snapshot (real_child);
        child->next_on_level = captured->first_child;
        captured->first_child = child;
        captured->allocated_total += child->allocated_total;
        real_child = real_child->next_on_level;
    }

    return captured;
}

struct kan_allocation_group_capture_t kan_allocation_group_begin_capture (void)
{
    lock_memory_profiling_context ();
    struct kan_allocation_group_capture_t capture;
    capture.captured_root =
        KAN_HANDLE_SET (kan_captured_allocation_group_t,
                        capture_allocation_group_snapshot (retrieve_root_allocation_group_unguarded ()));
    capture.event_iterator = event_iterator_create_unguarded ();
    unlock_memory_profiling_context ();
    return capture;
}
