#include <stddef.h>

#include <kan/memory_profiler/capture.h>

const char *kan_captured_allocation_group_get_name (kan_captured_allocation_group_t group)
{
    return "";
}

kan_allocation_group_t kan_captured_allocation_group_get_source (kan_captured_allocation_group_t group)
{
    return KAN_ALLOCATION_GROUP_IGNORE;
}

uint64_t kan_captured_allocation_group_get_total_allocated (kan_captured_allocation_group_t group)
{
    return 0u;
}

uint64_t kan_captured_allocation_group_get_directly_allocated (kan_captured_allocation_group_t group)
{
    return 0u;
}

kan_captured_allocation_group_iterator_t kan_captured_allocation_group_children_begin (
    kan_captured_allocation_group_t group)
{
    return 0u;
}

kan_captured_allocation_group_iterator_t kan_captured_allocation_group_children_next (
    kan_captured_allocation_group_iterator_t current)
{
    return 0u;
}

kan_captured_allocation_group_t kan_captured_allocation_group_children_get (
    kan_captured_allocation_group_iterator_t current)
{
    return 0u;
}

kan_captured_allocation_group_iterator_t kan_captured_allocation_group_children_end (
    kan_captured_allocation_group_t group)
{
    return 0u;
}

void kan_captured_allocation_group_destroy (kan_captured_allocation_group_t group)
{
}

const struct kan_allocation_group_event_t *kan_allocation_group_event_iterator_get_current_event (
    kan_allocation_group_event_iterator_t iterator)
{
    return NULL;
}

kan_allocation_group_event_iterator_t kan_allocation_group_event_iterator_advance (
    kan_allocation_group_event_iterator_t iterator)
{
    return 0u;
}

void kan_allocation_group_event_iterator_destroy (kan_allocation_group_event_iterator_t iterator)
{
}

struct kan_allocation_group_capture_t kan_allocation_group_begin_capture ()
{
    struct kan_allocation_group_capture_t capture;
    capture.captured_root = 0u;
    capture.event_iterator = 0u;
    return capture;
}
