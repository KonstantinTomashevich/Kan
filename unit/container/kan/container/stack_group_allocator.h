#pragma once

#include <container_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/memory/allocation.h>
#include <kan/memory_profiler/allocation_group.h>

/// \file
/// \brief Provides stack allocator proxy for cases when stack size cannot be fixed.
///
/// \par Stack group allocator
/// \parblock
/// In some cases we need allocation speed close to stack allocation speed, but we cannot limit total allocation size
/// as fixed constant which is needed to allocate stack. For this particular case stack group allocator is created: it
/// manages one or more stacks and provides allocation from them. If current stack is unable to allocate memory, it
/// switches to next stack (creating it if needed). All stacks are preserved during kan_stack_group_allocator_reset,
/// but excessive stacks can be freed using kan_stack_group_allocator_shrink.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Node of stack group with its stack.
struct kan_stack_group_allocator_node_t
{
    struct kan_stack_group_allocator_node_t *next;
    kan_stack_allocator_t stack;
};

/// \brief Contains stack group allocator data.
struct kan_stack_group_allocator_t
{
    struct kan_stack_group_allocator_node_t *first_stack;
    struct kan_stack_group_allocator_node_t *current_stack;
    kan_allocation_group_t group;
};

/// \brief Initializes stack group allocator with given allocation group and given stack size.
CONTAINER_API void kan_stack_group_allocator_init (struct kan_stack_group_allocator_t *allocator,
                                                   kan_allocation_group_t group,
                                                   uint64_t initial_stack_size);

/// \brief Allocates memory block from given stack group allocator.
CONTAINER_API void *kan_stack_group_allocator_allocate (struct kan_stack_group_allocator_t *allocator,
                                                        uint64_t amount,
                                                        uint64_t alignment);

/// \brief Syntax sugar helper for kan_stack_group_allocator_allocate that avoid repeating allocated type.
#define KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED(ALLOCATOR, TYPE)                                                      \
    (TYPE *) kan_stack_group_allocator_allocate (ALLOCATOR, sizeof (TYPE), _Alignof (TYPE))

/// \brief Resets stack group allocator to initial state without deallocating stacks.
CONTAINER_API void kan_stack_group_allocator_reset (struct kan_stack_group_allocator_t *allocator);

/// \brief Frees all stacks that aren't used for allocation.
CONTAINER_API void kan_stack_group_allocator_shrink (struct kan_stack_group_allocator_t *allocator);

/// \brief Destroys all stacks and shuts down stack group allocator.
CONTAINER_API void kan_stack_group_allocator_shutdown (struct kan_stack_group_allocator_t *allocator);

KAN_C_HEADER_END
