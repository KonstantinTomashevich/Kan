#include <kan/memory_profiler/capture.h>

const char *kan_captured_allocation_group_get_name (kan_captured_allocation_group_t group)
{
    // TODO: Implement.
    return "";
}

kan_allocation_group_t kan_captured_allocation_group_get_source (kan_captured_allocation_group_t group)
{
    // TODO: Implement.
    return KAN_ALLOCATION_GROUP_IGNORE;
}

uint64_t kan_captured_allocation_group_get_allocated (kan_captured_allocation_group_t group)
{
    // TODO: Implement.
    return 0u;
}

kan_captured_allocation_group_iterator_t kan_captured_allocation_group_children_begin (
    kan_captured_allocation_group_t group)
{
    // TODO: Implement.
    return 0u;
}

kan_captured_allocation_group_iterator_t kan_captured_allocation_group_children_next (
    kan_captured_allocation_group_iterator_t current)
{
    // TODO: Implement.
    return 0u;
}

kan_captured_allocation_group_t kan_captured_allocation_group_children_get (
    kan_captured_allocation_group_iterator_t current)
{
    // TODO: Implement.
    return 0u;
}

kan_captured_allocation_group_iterator_t kan_captured_allocation_group_children_end (
    kan_captured_allocation_group_t group)
{
    // TODO: Implement.
    return 0u;
}

void kan_captured_allocation_group_destroy (kan_captured_allocation_group_t group)
{
    // TODO: Implement.
}

const struct kan_allocation_group_event_t *kan_allocation_group_observer_get_current_event (
    kan_allocation_group_observer_t observer)
{
    // TODO: Implement.
    return NULL;
}

kan_bool_t kan_allocation_group_observer_next (kan_allocation_group_observer_t observer)
{
    // TODO: Implement.
    return KAN_FALSE;
}

void kan_allocation_group_observer_destroy (kan_allocation_group_observer_t observer)
{
    // TODO: Implement.
}

struct kan_allocation_group_capture_t kan_allocation_group_begin_capture ()
{
    // TODO: Implement.
    struct kan_allocation_group_capture_t capture;
    capture.captured_root = 0u;
    capture.observer = 0u;
    return capture;
}
