#pragma once

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/memory_profiler/capture.h>

KAN_C_HEADER_BEGIN

void lock_memory_profiling_context ();

void unlock_memory_profiling_context ();

struct allocation_group_t
{
    uint64_t allocated_here;
    struct allocation_group_t *next_on_level;
    struct allocation_group_t *first_child;
    char name[];
};

_Static_assert (sizeof (kan_allocation_group_t) >= sizeof (uintptr_t), "Allocation group handle can fit pointer.");

static inline struct allocation_group_t *retrieve_allocation_group (kan_allocation_group_t group)
{
    return (struct allocation_group_t *) group;
}

struct allocation_group_t *retrieve_root_allocation_group_unguarded ();

struct allocation_group_t *create_allocation_group_unguarded (struct allocation_group_t *next_on_level,
                                                              const char *name);

_Static_assert (sizeof (kan_allocation_group_event_iterator_t) >= sizeof (uintptr_t),
                "Event iterator can fit pointer.");

kan_allocation_group_event_iterator_t event_iterator_create_unguarded ();

const struct kan_allocation_group_event_t *event_iterator_get_unguarded (
    kan_allocation_group_event_iterator_t event_iterator);

kan_allocation_group_event_iterator_t event_iterator_advance_unguarded (
    kan_allocation_group_event_iterator_t event_iterator);

void event_iterator_destroy_unguarded (kan_allocation_group_event_iterator_t event_iterator);

void queue_new_allocation_group_event_unguarded (kan_allocation_group_t group);

void queue_allocate_event_unguarded (kan_allocation_group_t group, uint64_t amount);

void queue_free_event_unguarded (kan_allocation_group_t group, uint64_t amount);

void queue_marker_event_unguarded (kan_allocation_group_t group, const char *marker);

KAN_C_HEADER_END
