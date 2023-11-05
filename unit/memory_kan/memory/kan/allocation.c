#include <stdlib.h>

#include <kan/error/critical.h>
#include <kan/memory/allocation.h>
#include <kan/threading/atomic.h>

void *kan_allocate_general_no_profiling (uint64_t amount, uint64_t alignment)
{
#if defined(_MSC_VER)
    return _aligned_malloc (amount, alignment);
#else
    return aligned_alloc (alignment, amount);
#endif
}

void kan_free_general_no_profiling (void *memory)
{
#if defined(_MSC_VER)
    _aligned_free (memory);
#else
    free (memory);
#endif
}

void *kan_allocate_general (kan_allocation_group_t group, uint64_t amount, uint64_t alignment)
{
    void *memory = kan_allocate_general_no_profiling (amount, alignment);
    kan_allocation_group_allocate (group, amount);
    return memory;
}

void kan_free_general (kan_allocation_group_t group, void *memory, uint64_t amount)
{
    kan_free_general_no_profiling (memory);
    kan_allocation_group_free (group, amount);
}

struct batched_allocator_item_t
{
    void *next_free;
};

struct batched_allocator_page_t
{
    struct batched_allocator_page_t *next_free_page;
    struct batched_allocator_item_t *first_free;
    uint64_t acquired_count;
    uint64_t item_size;
    uint8_t data[];
};

struct batched_allocator_t
{
    // TODO: We can theoretically improve parallelism by introducing per-page locking, but it makes implementation
    //       difficult. Return to this idea if it turns out that allocation performance needs improvements.
    struct kan_atomic_int_t lock;
    struct batched_allocator_page_t *first_free_page;
};

#define MIN_RATIONAL_ITEMS_PER_PAGE 128u
#define MAX_RATIONAL_ITEM_SIZE (KAN_MEMORY_PAGED_ALLOCATOR_PAGE_SIZE / MIN_RATIONAL_ITEMS_PER_PAGE)
#define BATCHED_ALLOCATORS_COUNT (MAX_RATIONAL_ITEM_SIZE / 8u)

struct batched_allocator_context_t
{
    kan_allocation_group_t main_group;
    kan_allocation_group_t reserve_group;
    struct batched_allocator_t allocators[BATCHED_ALLOCATORS_COUNT];
};

static struct kan_atomic_int_t batched_allocator_context_initialization_lock = {.value = 0u};
static struct batched_allocator_context_t *batched_allocator_context = NULL;

static inline uint8_t *get_page_data_begin (struct batched_allocator_page_t *page)
{
    uint8_t *data_begin = page->data;
    if (((uintptr_t) data_begin) % page->item_size != 0u)
    {
        data_begin += page->item_size - ((uintptr_t) data_begin) % page->item_size;
    }

    return data_begin;
}

uint64_t kan_get_batched_allocation_max_size (void)
{
    return MAX_RATIONAL_ITEM_SIZE;
}

void *kan_allocate_batched (kan_allocation_group_t group, uint64_t item_size)
{
    // Super rare, therefore wrapped in double if for optimization: avoid atomic lock operations unless necessary.
    if (!batched_allocator_context)
    {
        kan_atomic_int_lock (&batched_allocator_context_initialization_lock);
        if (!batched_allocator_context)
        {
            const kan_allocation_group_t main_group =
                kan_allocation_group_get_child (kan_allocation_group_root (), "batched_allocator_context");
            const kan_allocation_group_t reserve_group = kan_allocation_group_get_child (main_group, "reserve");

            batched_allocator_context = kan_allocate_general (main_group, sizeof (struct batched_allocator_context_t),
                                                              _Alignof (struct batched_allocator_context_t));
            batched_allocator_context->main_group = main_group;
            batched_allocator_context->reserve_group = reserve_group;

            for (uint64_t index = 0u; index < BATCHED_ALLOCATORS_COUNT; ++index)
            {
                batched_allocator_context->allocators[index].lock = kan_atomic_int_init (0u);
                batched_allocator_context->allocators[index].first_free_page = NULL;
            }
        }

        kan_atomic_int_unlock (&batched_allocator_context_initialization_lock);
    }

    KAN_ASSERT (item_size <= MAX_RATIONAL_ITEM_SIZE)
    KAN_ASSERT (item_size >= 8u)

    struct batched_allocator_t *allocator = &batched_allocator_context->allocators[item_size / 8u - 1u];
    kan_atomic_int_lock (&allocator->lock);

    if (!allocator->first_free_page)
    {
        // Create new page. Should be rare.

        struct batched_allocator_page_t *page = (struct batched_allocator_page_t *) kan_allocate_general_no_profiling (
            KAN_MEMORY_PAGED_ALLOCATOR_PAGE_SIZE, KAN_MEMORY_PAGED_ALLOCATOR_PAGE_SIZE);

        page->next_free_page = NULL;
        page->acquired_count = 0u;
        page->item_size = item_size;
        uint8_t *data_begin = get_page_data_begin (page);

        const uint64_t page_meta_size = data_begin - (uint8_t *) page;
        kan_allocation_group_allocate (batched_allocator_context->main_group, page_meta_size);
        kan_allocation_group_allocate (batched_allocator_context->reserve_group,
                                       KAN_MEMORY_PAGED_ALLOCATOR_PAGE_SIZE - page_meta_size);

        uint8_t *item_data = data_begin;
        uint8_t *page_end = ((uint8_t *) page) + KAN_MEMORY_PAGED_ALLOCATOR_PAGE_SIZE;
        page->first_free = (struct batched_allocator_item_t *) item_data;

        while (item_data < page_end)
        {
            struct batched_allocator_item_t *item = (struct batched_allocator_item_t *) item_data;
            uint8_t *next_data = item_data + item_size;
            item->next_free = next_data >= page_end ? NULL : next_data;
            item_data = next_data;
        }

        allocator->first_free_page = page;
    }

    struct batched_allocator_page_t *page = allocator->first_free_page;
    void *chunk = page->first_free;
    KAN_ASSERT (chunk)
    page->first_free = *((void **) chunk);

    ++page->acquired_count;
    kan_allocation_group_allocate (group, item_size);
    kan_allocation_group_free (batched_allocator_context->reserve_group, item_size);

    if (!page->first_free)
    {
        allocator->first_free_page = page->next_free_page;
    }

    kan_atomic_int_unlock (&allocator->lock);
    return chunk;
}

void kan_free_batched (kan_allocation_group_t group, void *memory)
{
    KAN_ASSERT (batched_allocator_context)
    struct batched_allocator_page_t *page =
        (struct batched_allocator_page_t *) ((uintptr_t) memory -
                                             (uintptr_t) memory % KAN_MEMORY_PAGED_ALLOCATOR_PAGE_SIZE);

    struct batched_allocator_t *allocator = &batched_allocator_context->allocators[page->item_size / 8u - 1u];

    struct batched_allocator_item_t *item = (struct batched_allocator_item_t *) memory;
    kan_atomic_int_lock (&allocator->lock);

    KAN_ASSERT (page->acquired_count > 0u)
    --page->acquired_count;
    kan_allocation_group_free (group, page->item_size);
    kan_allocation_group_allocate (batched_allocator_context->reserve_group, page->item_size);

    if (page->acquired_count == 0u)
    {
        // Page is fully empty: nothing on it. Therefore, we just deallocate it.

        // Remove page from free page list.
        if (page == allocator->first_free_page)
        {
            allocator->first_free_page = page->next_free_page;
        }
        else
        {
            struct batched_allocator_page_t *other_page = allocator->first_free_page;
            while (other_page->next_free_page != page)
            {
                other_page = other_page->next_free_page;
                KAN_ASSERT (other_page)
            }

            other_page->next_free_page = page->next_free_page;
        }

        const uint64_t page_meta_size = get_page_data_begin (page) - (uint8_t *) page;
        kan_allocation_group_free (batched_allocator_context->reserve_group,
                                   KAN_MEMORY_PAGED_ALLOCATOR_PAGE_SIZE - page_meta_size);
        kan_allocation_group_free (batched_allocator_context->main_group, page_meta_size);
        kan_free_general_no_profiling (page);
    }
    else
    {
        item->next_free = page->first_free;
        page->first_free = item;

        // First free chunk on page: we need to add it to free pages list.
        if (!item->next_free)
        {
            // Free pages list is sorted by page address in order to avoid
            // situation where we have lots of partially full pages.
            if (allocator->first_free_page && allocator->first_free_page < page)
            {
                struct batched_allocator_page_t *sorted_page = allocator->first_free_page;
                while (sorted_page->next_free_page && sorted_page->next_free_page < page)
                {
                    sorted_page = sorted_page->next_free_page;
                }

                page->next_free_page = sorted_page->next_free_page;
                sorted_page->next_free_page = page;
            }
            else
            {
                page->next_free_page = allocator->first_free_page;
                allocator->first_free_page = page;
            }
        }
    }

    kan_atomic_int_unlock (&allocator->lock);
}

struct stack_allocator_t
{
    uint8_t *top;
    uint8_t *end;
    kan_allocation_group_t group;
    uint8_t data[];
};

kan_stack_allocator_t kan_stack_allocator_create (kan_allocation_group_t group, uint64_t amount)
{
    struct stack_allocator_t *stack = (struct stack_allocator_t *) kan_allocate_general (
        group, sizeof (struct stack_allocator_t) + amount, _Alignof (struct stack_allocator_t));
    stack->top = stack->data;
    stack->end = stack->data + amount;
    stack->group = group;
    return (kan_stack_allocator_t) stack;
}

void *kan_stack_allocator_allocate (kan_stack_allocator_t allocator, uint64_t amount, uint64_t alignment)
{
    struct stack_allocator_t *stack = (struct stack_allocator_t *) allocator;
    uint8_t *position = stack->top;

    if ((uintptr_t) position % alignment != 0u)
    {
        position += alignment - (uintptr_t) position % alignment;
    }

    uint8_t *new_top = position + amount;
    if (new_top > stack->end)
    {
        return NULL;
    }

    stack->top = new_top;
    return position;
}

void kan_stack_allocator_reset (kan_stack_allocator_t allocator)
{
    struct stack_allocator_t *stack = (struct stack_allocator_t *) allocator;
    stack->top = stack->data;
}

MEMORY_API void *kan_stack_allocator_save_top (kan_stack_allocator_t allocator)
{
    return ((struct stack_allocator_t *) allocator)->top;
}

MEMORY_API void kan_stack_allocator_load_top (kan_stack_allocator_t allocator, void *top)
{
    struct stack_allocator_t *stack = (struct stack_allocator_t *) allocator;
    KAN_ASSERT ((uint8_t *) top >= stack->data && (uint8_t *) top <= stack->end)
    stack->top = top;
}

void kan_stack_allocator_destroy (kan_stack_allocator_t allocator)
{
    struct stack_allocator_t *stack = (struct stack_allocator_t *) allocator;
    kan_free_general (stack->group, stack, sizeof (struct stack_allocator_t) + stack->end - stack->data);
}
