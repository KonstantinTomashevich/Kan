#include <kan/container/event_queue.h>

void kan_event_queue_init (struct kan_event_queue_t *queue, struct kan_event_queue_node_t *next_placeholder)
{
    queue->total_iterators = 0u;
    queue->next_placeholder = next_placeholder;
    next_placeholder->next = NULL;
    next_placeholder->iterators_here = 0u;
    queue->oldest = queue->next_placeholder;
}

struct kan_event_queue_node_t *kan_event_queue_submit_begin (struct kan_event_queue_t *queue)
{
    return queue->total_iterators > 0u ? queue->next_placeholder : NULL;
}

void kan_event_queue_submit_end (struct kan_event_queue_t *queue, struct kan_event_queue_node_t *next_placeholder)
{
    queue->next_placeholder->next = next_placeholder;
    queue->next_placeholder = next_placeholder;
    next_placeholder->next = NULL;
    next_placeholder->iterators_here = 0u;
}

struct kan_event_queue_node_t *kan_event_queue_clean_oldest (struct kan_event_queue_t *queue)
{
    struct kan_event_queue_node_t *oldest = queue->oldest;
    if (oldest->iterators_here == 0u && oldest != queue->next_placeholder)
    {
        queue->oldest = oldest->next;
        return oldest;
    }

    return NULL;
}

typedef uint64_t kan_event_queue_iterator_t;

kan_event_queue_iterator_t kan_event_queue_iterator_create (struct kan_event_queue_t *queue)
{
    ++queue->total_iterators;
    ++queue->next_placeholder->iterators_here;
    return (kan_event_queue_iterator_t) queue->next_placeholder;
}

const struct kan_event_queue_node_t *kan_event_queue_iterator_get (struct kan_event_queue_t *queue,
                                                                   kan_event_queue_iterator_t iterator)
{
    const struct kan_event_queue_node_t *node = (const struct kan_event_queue_node_t *) iterator;
    return queue->next_placeholder == node ? NULL : node;
}

kan_event_queue_iterator_t kan_event_queue_iterator_advance (kan_event_queue_iterator_t iterator)
{
    const struct kan_event_queue_node_t *node = (const struct kan_event_queue_node_t *) iterator;
    return node->next ? (kan_event_queue_iterator_t) node->next : iterator;
}

void kan_event_queue_iterator_destroy (struct kan_event_queue_t *queue, kan_event_queue_iterator_t iterator)
{
    --queue->total_iterators;
    struct kan_event_queue_node_t *node = (struct kan_event_queue_node_t *) iterator;
    --node->iterators_here;
}
