#define _CRT_SECURE_NO_WARNINGS

#include <string.h>

#include <kan/container/event_queue.h>
#include <kan/error/critical.h>
#include <kan/memory/allocation.h>
#include <kan/memory_profiler/core.h>
#include <kan/threading/atomic.h>

static struct kan_atomic_int_t memory_profiling_lock = {.value = 0u};

static struct allocation_group_t *root_allocation_group = NULL;

void lock_memory_profiling_context ()
{
    kan_atomic_int_lock (&memory_profiling_lock);
}

void unlock_memory_profiling_context ()
{
    kan_atomic_int_unlock (&memory_profiling_lock);
}

struct allocation_group_t *retrieve_root_allocation_group_unguarded ()
{
    if (!root_allocation_group)
    {
        root_allocation_group = create_allocation_group_unguarded (NULL, "root");
    }

    return root_allocation_group;
}

struct allocation_group_t *create_allocation_group_unguarded (struct allocation_group_t *next_on_level,
                                                              const char *name)
{
    struct allocation_group_t *group = (struct allocation_group_t *) kan_allocate_general_no_profiling (
        sizeof (struct allocation_group_t) + strlen (name) + 1u, _Alignof (struct allocation_group_t));
    group->allocated_here = 0u;
    group->next_on_level = next_on_level;
    group->first_child = NULL;
    strcpy (group->name, name);

    queue_new_allocation_group_event_unguarded ((kan_allocation_group_t) group);
    return group;
}

struct memory_event_node_t
{
    struct kan_event_queue_node_t node;
    struct kan_allocation_group_event_t event;
};

static kan_bool_t event_queue_initialized = KAN_FALSE;
static struct kan_event_queue_t event_queue;

static struct memory_event_node_t *create_event_node_unguarded ()
{
    // We cannot use batched allocators inside memory profiling as they're reporting reserved memory
    // and it results in deadlock when new page is being allocated to hold new profiling objects.
    return (struct memory_event_node_t *) kan_allocate_general_no_profiling (sizeof (struct memory_event_node_t),
                                                                             _Alignof (struct memory_event_node_t));
}

kan_allocation_group_event_iterator_t event_iterator_create_unguarded ()
{
    if (!event_queue_initialized)
    {
        kan_event_queue_init (&event_queue, &create_event_node_unguarded ()->node);
        event_queue_initialized = KAN_TRUE;
    }

    return (kan_allocation_group_event_iterator_t) kan_event_queue_iterator_create (&event_queue);
}

const struct kan_allocation_group_event_t *event_iterator_get_unguarded (
    kan_allocation_group_event_iterator_t event_iterator)
{
    KAN_ASSERT (event_queue_initialized)
    const struct memory_event_node_t *node = (struct memory_event_node_t *) kan_event_queue_iterator_get (
        &event_queue, (kan_event_queue_iterator_t) event_iterator);
    return node ? &node->event : NULL;
}

static void cleanup_event_queue ()
{
    struct memory_event_node_t *node;
    while ((node = (struct memory_event_node_t *) kan_event_queue_clean_oldest (&event_queue)))
    {
        if (node->event.type == KAN_ALLOCATION_GROUP_EVENT_MARKER)
        {
            kan_free_general_no_profiling (node->event.name);
        }

        kan_free_general_no_profiling (node);
    }
}

kan_allocation_group_event_iterator_t event_iterator_advance_unguarded (
    kan_allocation_group_event_iterator_t event_iterator)
{
    kan_event_queue_iterator_t iterator = (kan_event_queue_iterator_t) event_iterator;
    iterator = kan_event_queue_iterator_advance (iterator);
    cleanup_event_queue ();
    return (kan_allocation_group_event_iterator_t) iterator;
}

void event_iterator_destroy_unguarded (kan_allocation_group_event_iterator_t event_iterator)
{
    KAN_ASSERT (event_queue_initialized)
    kan_event_queue_iterator_destroy (&event_queue, (kan_event_queue_iterator_t) event_iterator);
    cleanup_event_queue ();
}

void queue_new_allocation_group_event_unguarded (kan_allocation_group_t group)
{
    if (!event_queue_initialized)
    {
        return;
    }

    struct memory_event_node_t *node = (struct memory_event_node_t *) kan_event_queue_submit_begin (&event_queue);
    if (node)
    {
        node->event.type = KAN_ALLOCATION_GROUP_EVENT_NEW_GROUP;
        node->event.group = group;
        node->event.name = retrieve_allocation_group (group)->name;
        kan_event_queue_submit_end (&event_queue, &create_event_node_unguarded ()->node);
    }
}

void queue_allocate_event_unguarded (kan_allocation_group_t group, uint64_t amount)
{
    if (!event_queue_initialized)
    {
        return;
    }

    struct memory_event_node_t *node = (struct memory_event_node_t *) kan_event_queue_submit_begin (&event_queue);
    if (node)
    {
        node->event.type = KAN_ALLOCATION_GROUP_EVENT_ALLOCATE;
        node->event.group = group;
        node->event.amount = amount;
        kan_event_queue_submit_end (&event_queue, &create_event_node_unguarded ()->node);
    }
}

void queue_free_event_unguarded (kan_allocation_group_t group, uint64_t amount)
{
    if (!event_queue_initialized)
    {
        return;
    }

    struct memory_event_node_t *node = (struct memory_event_node_t *) kan_event_queue_submit_begin (&event_queue);
    if (node)
    {
        node->event.type = KAN_ALLOCATION_GROUP_EVENT_FREE;
        node->event.group = group;
        node->event.amount = amount;
        kan_event_queue_submit_end (&event_queue, &create_event_node_unguarded ()->node);
    }
}

void queue_marker_event_unguarded (kan_allocation_group_t group, const char *marker)
{
    if (!event_queue_initialized)
    {
        return;
    }

    struct memory_event_node_t *node = (struct memory_event_node_t *) kan_event_queue_submit_begin (&event_queue);
    if (node)
    {
        node->event.type = KAN_ALLOCATION_GROUP_EVENT_MARKER;
        node->event.group = group;
        node->event.name = kan_allocate_general_no_profiling (strlen (marker) + 1u, _Alignof (char));
        strcpy (node->event.name, marker);
        kan_event_queue_submit_end (&event_queue, &create_event_node_unguarded ()->node);
    }
}
