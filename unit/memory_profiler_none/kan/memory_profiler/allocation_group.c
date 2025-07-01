#include <kan/memory_profiler/allocation_group.h>

kan_allocation_group_t kan_allocation_group_root (void) { return KAN_ALLOCATION_GROUP_IGNORE; }

kan_allocation_group_t kan_allocation_group_get_child (kan_allocation_group_t parent, const char *name)
{
    return KAN_ALLOCATION_GROUP_IGNORE;
}

void kan_allocation_group_allocate (kan_allocation_group_t group, kan_memory_size_t amount) {}

void kan_allocation_group_free (kan_allocation_group_t group, kan_memory_size_t amount) {}

kan_allocation_group_t kan_allocation_group_stack_get (void) { return KAN_ALLOCATION_GROUP_IGNORE; }

void kan_allocation_group_stack_push (kan_allocation_group_t group) {}

void kan_allocation_group_stack_pop (void) {}
