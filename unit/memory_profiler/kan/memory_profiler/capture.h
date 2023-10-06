#pragma once

#include <memory_profiler_api.h>

#include <stdint.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/memory_profiler/allocation_group.h>

/// \file
/// \brief Provides low level utilities for capturing memory usage.
///
/// \par Memory events
/// \parblock
/// If there are any memory event iterators, every allocation group operation creates an event that can be later parsed
/// by any event iterator. Events are automatically destroyed when every existing event iterator has read this event or
/// if there are no existing event iterators.
/// \endparblock
///
/// \par Captured groups
/// \parblock
/// Group capture feature creates snapshot of full allocation group tree at required moment and provides functions
/// to traverse it and extract all required data.
/// \endparblock
///
/// \par Capture context
/// \parblock
/// To correctly capture all data you need you must capture groups and create event iterator at the same time without
/// interruptions, which is done through `kan_allocation_group_begin_capture`, that guarantees that this requirement
/// will be met. After that, event iterator and captured group hierarchy can be managed separately and can be destroyed
/// at any time.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Enumerates types of profiled memory events.
enum kan_allocation_group_event_type_t
{
    /// \brief New allocation group is created. Its name is stored in `name` field of `kan_allocation_group_event_t`.
    KAN_ALLOCATION_GROUP_EVENT_NEW_GROUP = 0u,

    /// \brief Memory allocated in allocation group. Size is stored in `amount` field of `kan_allocation_group_event_t`.
    KAN_ALLOCATION_GROUP_EVENT_ALLOCATE,

    /// \brief Memory freed from allocation group. Size is stored in `amount` field of `kan_allocation_group_event_t`.
    KAN_ALLOCATION_GROUP_EVENT_FREE,

    /// \brief Custom marker is reported for allocation group.
    ///        Its name is stored in `name` field of `kan_allocation_group_event_t`.
    KAN_ALLOCATION_GROUP_EVENT_MARKER,
};

/// \brief Describes allocation group event.
struct kan_allocation_group_event_t
{
    /// \brief Type of event that occurred.
    enum kan_allocation_group_event_type_t type;

    /// \brief Related allocation group.
    kan_allocation_group_t group;
    union
    {
        uint64_t amount;

        char *name;
    };
};

typedef uint64_t kan_captured_allocation_group_t;

/// \brief Returns name of given captured allocation group.
MEMORY_PROFILER_API const char *kan_captured_allocation_group_get_name (kan_captured_allocation_group_t group);

/// \brief Returns allocation group from which given allocation group was captured.
MEMORY_PROFILER_API kan_allocation_group_t
kan_captured_allocation_group_get_source (kan_captured_allocation_group_t group);

/// \brief Returns amount of memory inside captured allocation group including its children.
MEMORY_PROFILER_API uint64_t kan_captured_allocation_group_get_total_allocated (kan_captured_allocation_group_t group);

/// \brief Returns amount of memory inside captured allocation group excluding its children.
MEMORY_PROFILER_API uint64_t
kan_captured_allocation_group_get_directly_allocated (kan_captured_allocation_group_t group);

typedef uint64_t kan_captured_allocation_group_iterator_t;

/// \brief Returns iterator that points to first captured allocation group child if any.
MEMORY_PROFILER_API kan_captured_allocation_group_iterator_t
kan_captured_allocation_group_children_begin (kan_captured_allocation_group_t group);

/// \brief Moves given captured allocation group iterator to the next child.
MEMORY_PROFILER_API kan_captured_allocation_group_iterator_t
kan_captured_allocation_group_children_next (kan_captured_allocation_group_iterator_t current);

/// \brief Returns captured allocation group to which given iterator points.
MEMORY_PROFILER_API kan_captured_allocation_group_t
kan_captured_allocation_group_children_get (kan_captured_allocation_group_iterator_t current);

/// \brief Returns iterator that points to the end of captured allocation group children.
MEMORY_PROFILER_API kan_captured_allocation_group_iterator_t
kan_captured_allocation_group_children_end (kan_captured_allocation_group_t group);

/// \brief Destroys captured allocation group and all its children. Should only be called on captured root!
MEMORY_PROFILER_API void kan_captured_allocation_group_destroy (kan_captured_allocation_group_t group);

typedef uint64_t kan_allocation_group_event_iterator_t;

/// \brief Returns memory event to which iterator is pointing right now
///        or `NULL` if iterator has reached end of events queue.
MEMORY_PROFILER_API const struct kan_allocation_group_event_t *kan_allocation_group_event_iterator_get (
    kan_allocation_group_event_iterator_t iterator);

/// \brief Moves iterator to the next event and returns new iterator value.
MEMORY_PROFILER_API kan_allocation_group_event_iterator_t
kan_allocation_group_event_iterator_advance (kan_allocation_group_event_iterator_t iterator);

/// \brief Destroys given memory event iterator.
MEMORY_PROFILER_API void kan_allocation_group_event_iterator_destroy (kan_allocation_group_event_iterator_t iterator);

/// \brief Contains result of `kan_allocation_group_begin_capture`.
struct kan_allocation_group_capture_t
{
    kan_captured_allocation_group_t captured_root;
    kan_allocation_group_event_iterator_t event_iterator;
};

/// \brief Begins memory usage capture by snapshotting allocation groups and creating memory event iterator.
MEMORY_PROFILER_API struct kan_allocation_group_capture_t kan_allocation_group_begin_capture ();

KAN_C_HEADER_END
