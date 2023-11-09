#define _CRT_SECURE_NO_WARNINGS

#include <string.h>

#include <kan/api_common/bool.h>
#include <kan/container/hash_storage.h>
#include <kan/container/interned_string.h>
#include <kan/error/critical.h>
#include <kan/hash/hash.h>
#include <kan/memory/allocation.h>
#include <kan/threading/atomic.h>

struct context_t
{
    kan_allocation_group_t allocation_group;
    struct kan_hash_storage_t hash_storage;
    kan_stack_allocator_t last_stack;
};

struct node_t
{
    struct kan_hash_storage_node_t node;
    uint64_t length;
    char string[];
};

static kan_bool_t initialized = KAN_FALSE;
static struct kan_atomic_int_t lock = {0u};
static struct context_t context;

// TODO: Performance can be improved by using fine-tuned hash map for this task: store hashes separately from strings in
//       dense arrays to make iteration over them more cache coherent. In separate experiment it gave up to 20% speedup
//       for interning.

kan_interned_string_t kan_string_intern (const char *null_terminated_string)
{
    if (!null_terminated_string)
    {
        return NULL;
    }

    const char *end = null_terminated_string;
    while (*end != '\0')
    {
        ++end;
    }

    return kan_char_sequence_intern (null_terminated_string, end);
}

kan_interned_string_t kan_char_sequence_intern (const char *begin, const char *end)
{
    if (!begin || begin == end)
    {
        return NULL;
    }

    uint64_t string_length = end - begin;
    const uint64_t hash = kan_char_sequence_hash (begin, end);
    kan_atomic_int_lock (&lock);

    if (!initialized)
    {
        context.allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "string_interning");
        kan_hash_storage_init (&context.hash_storage, context.allocation_group,
                               KAN_CONTAINER_STRING_INTERNING_INITIAL_BUCKETS);
        context.last_stack =
            kan_stack_allocator_create (context.allocation_group, KAN_CONTAINER_STRING_INTERNING_STACK_SIZE);
        initialized = KAN_TRUE;
    }

    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (&context.hash_storage, hash);
    struct node_t *node = (struct node_t *) bucket->first;
    const struct node_t *node_end = (struct node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->node.hash == hash && node->length == string_length &&
            strncmp (node->string, begin, string_length) == 0)
        {
            // Already interned.
            kan_atomic_int_unlock (&lock);
            return node->string;
        }

        node = (struct node_t *) node->node.list_node.next;
    }

    // Not interned.
    node = kan_stack_allocator_allocate (context.last_stack, sizeof (struct node_t) + string_length + 1u,
                                         _Alignof (struct node_t));

    if (!node)
    {
        context.last_stack =
            kan_stack_allocator_create (context.allocation_group, KAN_CONTAINER_STRING_INTERNING_STACK_SIZE);

        node = kan_stack_allocator_allocate (context.last_stack, sizeof (struct node_t) + string_length + 1u,
                                             _Alignof (struct node_t));
        KAN_ASSERT (node)
    }

    node->node.hash = hash;
    node->length = string_length;
    strncpy (node->string, begin, string_length);
    node->string[string_length] = '\0';

    if (context.hash_storage.items.size >=
        context.hash_storage.bucket_count * KAN_CONTAINER_STRING_INTERNING_LOAD_FACTOR)
    {
        kan_hash_storage_set_bucket_count (&context.hash_storage, context.hash_storage.bucket_count * 2u);
    }

    kan_hash_storage_add (&context.hash_storage, &node->node);
    kan_atomic_int_unlock (&lock);
    return node->string;
}
