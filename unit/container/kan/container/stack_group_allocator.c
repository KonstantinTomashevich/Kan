#include <stddef.h>

#include <kan/container/stack_group_allocator.h>
#include <kan/error/critical.h>

void kan_stack_group_allocator_init (struct kan_stack_group_allocator_t *allocator,
                                     kan_allocation_group_t group,
                                     uint64_t initial_stack_size)
{
    allocator->group = group;
    struct kan_stack_group_allocator_node_t *node =
        kan_allocate_batched (allocator->group, sizeof (struct kan_stack_group_allocator_node_t));

    node->next = NULL;
    node->stack = kan_stack_allocator_create (allocator->group, initial_stack_size);

    allocator->first_stack = node;
    allocator->current_stack = node;
}

void *kan_stack_group_allocator_allocate (struct kan_stack_group_allocator_t *allocator,
                                          uint64_t amount,
                                          uint64_t alignment)
{
    void *allocated = kan_stack_allocator_allocate (allocator->current_stack->stack, amount, alignment);
    if (!allocated)
    {
        struct kan_stack_group_allocator_node_t *next = allocator->current_stack->next;
        if (!next)
        {
            next = kan_allocate_batched (allocator->group, sizeof (struct kan_stack_group_allocator_node_t));
            next->next = NULL;
            next->stack = kan_stack_allocator_create (allocator->group,
                                                      kan_stack_allocator_get_size (allocator->current_stack->stack));
            allocator->current_stack->next = next;
        }

        allocator->current_stack = next;
        allocated = kan_stack_allocator_allocate (allocator->current_stack->stack, amount, alignment);
    }

    KAN_ASSERT (allocated)
    return allocated;
}

void kan_stack_group_allocator_reset (struct kan_stack_group_allocator_t *allocator)
{
    struct kan_stack_group_allocator_node_t *node = allocator->first_stack;
    while (node != allocator->current_stack)
    {
        kan_stack_allocator_reset (node->stack);
        node = node->next;
    }

    kan_stack_allocator_reset (allocator->current_stack->stack);
    allocator->current_stack = allocator->first_stack;
}

void kan_stack_group_allocator_shrink (struct kan_stack_group_allocator_t *allocator)
{
    struct kan_stack_group_allocator_node_t *node = allocator->current_stack->next;
    allocator->current_stack->next = NULL;

    while (node)
    {
        struct kan_stack_group_allocator_node_t *next = node->next;
        kan_stack_allocator_destroy (node->stack);
        kan_free_batched (allocator->group, node);
        node = next;
    }
}

void kan_stack_group_shutdown (struct kan_stack_group_allocator_t *allocator)
{
    struct kan_stack_group_allocator_node_t *node = allocator->first_stack;
    while (node)
    {
        struct kan_stack_group_allocator_node_t *next = node->next;
        kan_stack_allocator_destroy (node->stack);
        kan_free_batched (allocator->group, node);
        node = next;
    }
}
