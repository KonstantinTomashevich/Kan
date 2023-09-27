#include <memory.h>

#include <kan/memory/allocation.h>
#include <kan/testing/testing.h>

KAN_TEST_CASE (general_no_profiling)
{
    for (uint64_t size = 16u; size <= 1024u; size *= 2u)
    {
        for (uint64_t alignment = 8u; alignment <= 32u; alignment *= 2u)
        {
            void *data = kan_allocate_general_no_profiling (size, alignment);
            KAN_TEST_CHECK (data)
            KAN_TEST_CHECK ((uintptr_t) data % alignment == 0u)
            memset (data, 42u, size);
            kan_free_general_no_profiling (data);
        }
    }
}

KAN_TEST_CASE (general)
{
    for (uint64_t size = 16u; size <= 1024u; size *= 2u)
    {
        for (uint64_t alignment = 8u; alignment <= 32u; alignment *= 2u)
        {
            void *data = kan_allocate_general (KAN_ALLOCATION_GROUP_IGNORE, size, alignment);
            KAN_TEST_CHECK (data)
            KAN_TEST_CHECK ((uintptr_t) data % alignment == 0u)
            memset (data, 42u, size);
            kan_free_general (KAN_ALLOCATION_GROUP_IGNORE, data, size);
        }
    }
}

KAN_TEST_CASE (batched)
{
    for (uint64_t size = 16u; size <= 256u; size *= 2u)
    {
        void *data = kan_allocate_batched (KAN_ALLOCATION_GROUP_IGNORE, size);
        KAN_TEST_CHECK (data)
        KAN_TEST_CHECK ((uintptr_t) data % size == 0u)
        memset (data, 42u, size);
        kan_free_batched (KAN_ALLOCATION_GROUP_IGNORE, data);
    }
}

KAN_TEST_CASE (batched_stress)
{
#define BATCHED_STRESS_ALLOCATIONS (1024u * 1024u)
    void **pointers =
        kan_allocate_general_no_profiling (BATCHED_STRESS_ALLOCATIONS * sizeof (void *), _Alignof (void *));

    for (uint64_t index = 0; index < BATCHED_STRESS_ALLOCATIONS; ++index)
    {
        pointers[index] = kan_allocate_batched (KAN_ALLOCATION_GROUP_IGNORE, 256u);
        KAN_TEST_CHECK (pointers[index])
        memset (pointers[index], 42u, 256u);
    }

    for (uint64_t index = BATCHED_STRESS_ALLOCATIONS / 2u; index < BATCHED_STRESS_ALLOCATIONS; ++index)
    {
        kan_free_batched (KAN_ALLOCATION_GROUP_IGNORE, pointers[index]);
    }

    for (uint64_t index = BATCHED_STRESS_ALLOCATIONS / 2u; index < BATCHED_STRESS_ALLOCATIONS; ++index)
    {
        pointers[index] = kan_allocate_batched (KAN_ALLOCATION_GROUP_IGNORE, 256u);
        KAN_TEST_CHECK (pointers[index])
        memset (pointers[index], 42u, 256u);
    }

    for (uint64_t index = 0; index < BATCHED_STRESS_ALLOCATIONS; ++index)
    {
        kan_free_batched (KAN_ALLOCATION_GROUP_IGNORE, pointers[index]);
    }

    kan_free_general_no_profiling (pointers);
}

KAN_TEST_CASE (stack)
{
#define TEST_STACK_SIZE 1024u
    kan_stack_allocator_t stack = kan_stack_allocator_create (KAN_ALLOCATION_GROUP_IGNORE, TEST_STACK_SIZE);

    uint8_t *aligned8 = (uint8_t *) kan_stack_allocator_allocate (stack, 8u, 8u);
    KAN_TEST_CHECK ((uintptr_t) aligned8 % 8u == 0u)

    uint8_t *aligned16 = (uint8_t *) kan_stack_allocator_allocate (stack, 16u, 16u);
    KAN_TEST_CHECK ((uintptr_t) aligned16 % 16u == 0u)
    KAN_TEST_CHECK (aligned8 != aligned16)

    uint8_t *aligned32 = (uint8_t *) kan_stack_allocator_allocate (stack, 32u, 32u);
    KAN_TEST_CHECK ((uintptr_t) aligned32 % 32u == 0u)
    KAN_TEST_CHECK (aligned16 != aligned32)

    kan_stack_allocator_reset (stack);
    KAN_TEST_CHECK (kan_stack_allocator_allocate (stack, TEST_STACK_SIZE * 2u, 8u) == NULL)

    for (uint64_t index = 0u; index < 8u; ++index)
    {
        KAN_TEST_CHECK (kan_stack_allocator_allocate (stack, TEST_STACK_SIZE / 8u, 8u) != NULL)
    }

    KAN_TEST_CHECK (kan_stack_allocator_allocate (stack, TEST_STACK_SIZE / 8u, 8u) == NULL)
    kan_stack_allocator_destroy (stack);
}
