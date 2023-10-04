#include <kan/container/hash_storage.h>
#include <kan/memory/allocation.h>

void kan_hash_storage_init (struct kan_hash_storage_t *storage,
                            kan_allocation_group_t bucket_allocation_group,
                            uint64_t initial_bucket_count)
{
    storage->bucket_allocation_group = bucket_allocation_group;
    storage->bucket_count = initial_bucket_count;

    // General allocation as we're allocation potentially huge block of memory of non-regular size.
    storage->buckets =
        kan_allocate_general (bucket_allocation_group, initial_bucket_count * sizeof (struct kan_hash_storage_bucket_t),
                              _Alignof (struct kan_hash_storage_bucket_t));

    for (uint64_t index = 0; index < storage->bucket_count; ++index)
    {
        storage->buckets[index].first = NULL;
        storage->buckets[index].last = NULL;
    }

    kan_bd_list_init (&storage->items);
}

void kan_hash_storage_add (struct kan_hash_storage_t *storage, struct kan_hash_storage_node_t *node)
{
    struct kan_hash_storage_bucket_t *bucket = &storage->buckets[node->hash % storage->bucket_count];
    kan_bd_list_add (&storage->items, bucket->first, &node->list_node);
    bucket->first = &node->list_node;

    if (!bucket->last)
    {
        bucket->last = &node->list_node;
    }
}

void kan_hash_storage_remove (struct kan_hash_storage_t *storage, struct kan_hash_storage_node_t *node)
{
    struct kan_hash_storage_bucket_t *bucket = &storage->buckets[node->hash % storage->bucket_count];
    struct kan_bd_list_node_t *list_node = &node->list_node;

    if (list_node == bucket->first && list_node == bucket->last)
    {
        bucket->first = NULL;
        bucket->last = NULL;
    }
    else if (list_node == bucket->first)
    {
        bucket->first = list_node->next;
    }
    else if (list_node == bucket->last)
    {
        bucket->last = list_node->previous;
    }

    kan_bd_list_remove (&storage->items, &node->list_node);
}

const struct kan_hash_storage_bucket_t *kan_hash_storage_query (struct kan_hash_storage_t *storage, uint64_t hash)
{
    return &storage->buckets[hash % storage->bucket_count];
}

void kan_hash_storage_set_bucket_count (struct kan_hash_storage_t *storage, uint64_t bucket_count)
{
    struct kan_hash_storage_t temporary_storage = *storage;
    kan_hash_storage_init (storage, temporary_storage.bucket_allocation_group, bucket_count);

    struct kan_bd_list_node_t *node = temporary_storage.items.first;
    while (node)
    {
        // Cache the next pointer as it will be changed after insertion into the new storage.
        struct kan_bd_list_node_t *next = node->next;

        kan_hash_storage_add (storage, (struct kan_hash_storage_node_t *) node);
        node = next;
    }

    kan_hash_storage_shutdown (&temporary_storage);
}

void kan_hash_storage_shutdown (struct kan_hash_storage_t *storage)
{
    kan_free_general (storage->bucket_allocation_group, storage->buckets,
                      storage->bucket_count * sizeof (struct kan_hash_storage_bucket_t));
}
