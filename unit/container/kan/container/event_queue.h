#pragma once

#include <container_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Provides lightweight implementation for event queue container.
///
/// \par Definition
/// \parblock
/// Event queue is used when some logic makes observable changes (events) that should be later processed by some
/// external logic. For example, memory allocations and deallocations can be stored as events to be later processed
/// by memory profiler. Event queue should be preferred over delegates, because it allows core logic to proceed
/// without being interrupted and prevents so-called listener hell where extensive usage of callbacks makes every
/// operations unpredictable. Event queue stores data only if there are active event iterators and should discard
/// data after every existing iterator passed over it.
/// \endparblock
///
/// \par Allocation policy
/// \parblock
/// This event queue implementation does not controls any allocations and expects user to correctly allocate required
/// memory blocks. It makes it possible for user to control every aspect of allocation without making event queue
/// implementation excessively complex.
/// \endparblock
///
/// \par Usage
/// \parblock
/// Queue can be allocated anywhere as `kan_event_queue_t` and then initialized using `kan_event_queue_init`.
/// Initialization requires user to allocate placeholder memory for next event node. Allocated event blocks must start
/// with `kan_event_queue_node_t`, everything after `sizeof (kan_event_queue_node_t)` is user event data space.
/// Example:
///
/// ```c
/// // Define your event node structure.
/// struct memory_event_node_t
/// {
///     struct kan_event_queue_node_t node;
///     struct kan_allocation_group_event_t event;
/// };
///
/// // Write allocator function for event node.
/// struct memory_event_node_t *allocate_memory_event_node ();
///
/// // Event queue initialization will look like that.
/// kan_event_queue_init (&event_queue, &allocate_memory_event_node ()->node);
/// ```
///
/// Event submission is split into three sections: beginning, user code and ending. Example:
///
/// ```c
/// struct memory_event_node_t *node = (struct memory_event_node_t *) kan_event_queue_submit_begin (&event_queue);
/// if (node)
/// {
///     // Now user can fill node with required data.
///     // And then finish submission. Next node placeholder is done the same way as it is done for initialization.
///     kan_event_queue_submit_end (&event_queue, &allocate_memory_event_node ()->node);
/// }
/// else
/// {
///     // Submission was denied due to absence of event iterators: there is no sense to submit when nobody is reading.
/// }
/// ```
///
/// To start observing events, user code needs to create event iterator using `kan_event_queue_iterator_create`. Newly
/// created iterator won't point to any events as nothing happened yet. Use `kan_event_queue_iterator_get` to request
/// node under iterator and `kan_event_queue_iterator_advance` to move iterator forward. When iterator is no longer
/// needed, free it by `kan_event_queue_iterator_destroy` to inform queue that it is no longer watched by this iterator.
///
/// After execution of `kan_event_queue_iterator_advance` and `kan_event_queue_iterator_destroy`, event queue should
/// clean oldest event nodes that are no longer used and won't event be used again. It should be done like that:
///
/// ```c
/// struct memory_event_node_t *node;
/// while ((node = (struct memory_event_node_t *) kan_event_queue_clean_oldest (&event_queue)))
/// {
///     // User deallocation code.
/// }
/// ```
/// \endparblock
///
/// \par Usage for API
/// \parblock
/// Event queue should not be publicly exported as API, because it leaves memory management up to user for performance
/// and flexibility reasons, which is unsafe and impractical for public APIs. Prefer leaving event queue usage under the
/// hood and provide tailored API functions to the end user.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// This event queue implementation is not thread safe.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Contains structural information about event queue node,
struct kan_event_queue_node_t
{
    struct kan_event_queue_node_t *next;
    uint64_t iterators_here;
};

/// \brief Contains event queue internal data.
struct kan_event_queue_t
{
    uint64_t total_iterators;
    struct kan_event_queue_node_t *next_placeholder;
    struct kan_event_queue_node_t *oldest;
};

/// \brief Initializes given event queue instance using given placeholder for next event.
CONTAINER_API void kan_event_queue_init (struct kan_event_queue_t *queue,
                                         struct kan_event_queue_node_t *next_placeholder);

/// \brief Attempts to begin submission procedure. Returns `NULL` if there are no iterators.
CONTAINER_API struct kan_event_queue_node_t *kan_event_queue_submit_begin (struct kan_event_queue_t *queue);

/// \brief Ends submission procedure and updates event queue structure.
CONTAINER_API void kan_event_queue_submit_end (struct kan_event_queue_t *queue,
                                               struct kan_event_queue_node_t *next_placeholder);

/// \brief Returns oldest node to be cleaned out or `NULL` if there is no nodes to be cleaned out.
CONTAINER_API struct kan_event_queue_node_t *kan_event_queue_clean_oldest (struct kan_event_queue_t *queue);

typedef uint64_t kan_event_queue_iterator_t;

/// \brief Creates iterator for listening to events that happen after its creation.
CONTAINER_API kan_event_queue_iterator_t kan_event_queue_iterator_create (struct kan_event_queue_t *queue);

/// \brief Returns event node under the iterator or `NULL` if there are no new events.
CONTAINER_API const struct kan_event_queue_node_t *kan_event_queue_iterator_get (struct kan_event_queue_t *queue,
                                                                                 kan_event_queue_iterator_t iterator);

/// \brief Moves iterator to next event if possible and returns new iterator handle.
CONTAINER_API kan_event_queue_iterator_t kan_event_queue_iterator_advance (kan_event_queue_iterator_t iterator);

/// \brief Destroys given iterator and informs queue that it won't listen to events anymore.
CONTAINER_API void kan_event_queue_iterator_destroy (struct kan_event_queue_t *queue,
                                                     kan_event_queue_iterator_t iterator);

KAN_C_HEADER_END
