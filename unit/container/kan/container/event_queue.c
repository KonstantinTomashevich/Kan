#include <stddef.h>

#include <kan/container/event_queue.h>
#include <kan/error/critical.h>

void kan_event_queue_init (struct kan_event_queue_t *queue, struct kan_event_queue_node_t *next_placeholder)
{
    queue->total_iterators = kan_atomic_int_init (0);
    queue->next_placeholder = next_placeholder;
    next_placeholder->next = NULL;
    next_placeholder->iterators_here = kan_atomic_int_init (0);
    queue->oldest = queue->next_placeholder;
}

struct kan_event_queue_node_t *kan_event_queue_submit_begin (struct kan_event_queue_t *queue)
{
    return kan_atomic_int_get (&queue->total_iterators) > 0 ? queue->next_placeholder : NULL;
}

void kan_event_queue_submit_end (struct kan_event_queue_t *queue, struct kan_event_queue_node_t *next_placeholder)
{
    queue->next_placeholder->next = next_placeholder;
    queue->next_placeholder = next_placeholder;
    next_placeholder->next = NULL;
    next_placeholder->iterators_here = kan_atomic_int_init (0);
}

struct kan_event_queue_node_t *kan_event_queue_clean_oldest (struct kan_event_queue_t *queue)
{
    struct kan_event_queue_node_t *oldest = queue->oldest;
    if (kan_atomic_int_get (&oldest->iterators_here) == 0 && oldest != queue->next_placeholder)
    {
        queue->oldest = oldest->next;
        return oldest;
    }

    return NULL;
}

typedef uint64_t kan_event_queue_iterator_t;

kan_event_queue_iterator_t kan_event_queue_iterator_create (struct kan_event_queue_t *queue)
{
    kan_atomic_int_add (&queue->total_iterators, 1);
    kan_atomic_int_add (&queue->next_placeholder->iterators_here, 1);
    return (kan_event_queue_iterator_t) queue->next_placeholder;
}

kan_event_queue_iterator_t kan_event_queue_iterator_create_next (struct kan_event_queue_t *queue,
                                                                 kan_event_queue_iterator_t iterator)
{
    kan_atomic_int_add (&queue->total_iterators, 1);
    const struct kan_event_queue_node_t *node = (const struct kan_event_queue_node_t *) iterator;
    KAN_ASSERT (node != queue->next_placeholder)
    struct kan_event_queue_node_t *next = node->next;
    kan_atomic_int_add (&next->iterators_here, 1);
    return (kan_event_queue_iterator_t) next;
}

const struct kan_event_queue_node_t *kan_event_queue_iterator_get (struct kan_event_queue_t *queue,
                                                                   kan_event_queue_iterator_t iterator)
{
    const struct kan_event_queue_node_t *node = (const struct kan_event_queue_node_t *) iterator;
    return queue->next_placeholder == node ? NULL : node;
}

kan_event_queue_iterator_t kan_event_queue_iterator_advance (kan_event_queue_iterator_t iterator)
{
    struct kan_event_queue_node_t *node = (struct kan_event_queue_node_t *) iterator;
    if (node->next)
    {
        kan_atomic_int_add (&node->iterators_here, -1);
        kan_atomic_int_add (&node->next->iterators_here, 1);
    }

    return node->next ? (kan_event_queue_iterator_t) node->next : iterator;
}

kan_bool_t kan_event_queue_iterator_destroy (struct kan_event_queue_t *queue, kan_event_queue_iterator_t iterator)
{
    kan_atomic_int_add (&queue->total_iterators, -1);
    struct kan_event_queue_node_t *node = (struct kan_event_queue_node_t *) iterator;
    return kan_atomic_int_add (&node->iterators_here, -1) == 1;
}
