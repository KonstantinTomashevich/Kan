#pragma once

#include <container_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/api_common/min_max.h>
#include <kan/container/list.h>
#include <kan/hash/hash.h>
#include <kan/memory_profiler/allocation_group.h>

/// \file
/// \brief Implements simplistic hash storage container logic.
///
/// \par Definition
/// \parblock
/// Hash storage is a node-based container that provides hash driven lookups. It stores all the data as nodes of
/// bidirectional linked list and maintains additional bucket array to quickly locate subsequences that may contain
/// nodes with required hash.
/// \endparblock
///
/// \par Allocation policy
/// \parblock
/// User is expected to correctly allocate required memory blocks. It makes it possible for user to control every
/// aspect of data allocation without making linked list implementation excessively complex.
///
/// On the other hand, bucket array is managed internally and allocated inside given allocation group.
/// \endparblock
///
/// \par Limitations
/// \parblock
/// Hash storage is neither hash map nor hash multimap nor any other advanced hash container from C++ STL. It is
/// intentionally made overly simplistic to avoid usual issues of repeating templated containers from C++ in C.
/// It would be rational to list its most obvious limitations:
///
/// - It knows nothing about hashing and comparison functions,
///   therefore it does not check for uniqueness during addition.
/// - For the same reason, query just returns bucket that **may** contain **some** nodes with required hash.
///   No overhead, but no syntax sugar either. Iterate and try to find what you need.
/// - Removal requires user to manually find the node to be removed as there is neither hash nor equality functions.
///   Same goes for addition if you need to support uniqueness.
/// - Bucket count is expected to be managed by user as only user knows details about stored data.
/// \endparblock
///
/// \par Usage
/// \parblock
/// Hash storage can be allocated anywhere as `kan_hash_storage_t` and then initialized using `kan_hash_storage_init`.
/// To free resources after usage, call `kan_hash_storage_shutdown`. Keep in mind that nodes lifetime is expected to
/// be managed by user, therefore you need to manually free all the nodes before shutting down the hash storage.
///
/// To add new node to the list, user must allocate compatible node structure using any allocator and then call
/// `kan_hash_storage_add` function, for example:
///
/// ```c
/// // Firstly, define your node structure that starts with hash storage node data.
/// struct my_node_t
/// {
///     struct kan_hash_storage_node_t node;
///     struct my_data_t data;
/// };
///
/// // Then define allocator function for your nodes.
/// struct my_node_t *allocate_my_node ();
///
/// // Then you can create new node like that.
/// struct my_node_t *node = allocate_my_node ();
/// // You need to initialize hash value of your node.
/// node->node.hash = my_node_hash;
/// // And then it can be added to the hash storage.
/// kan_hash_storage_add (&list, &node->node);
/// ```
///
/// Removal is pretty straightforward and can be done using `kan_hash_storage_remove`, but you need to find node to
/// remove first. It can be done by either iterating the node list or querying by hash.
///
/// Hash-driven query can be executed by `kan_hash_storage_query` that will return appropriate bucket that **may**
/// contain nodes with required hash **among other nodes**.
///
/// Do not forget to update bucket count while using hash storage if its size changes drastically. The easiest method
/// is to calculate optimal bucket count from linked list size, but it is not always appropriate. For example, if you
/// expect to have lots of nodes with the same hash (multimap-like case with lots of duplicates), it is better to
/// separately calculate count of unique hashes and use it instead of list size to calculate optimal bucket count.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// This hash storage implementation is not thread safe.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Contains structural data of hash storage node.
struct kan_hash_storage_node_t
{
    struct kan_bd_list_node_t list_node;
    kan_hash_t hash;
};

/// \brief Describes hash storage bucket.
struct kan_hash_storage_bucket_t
{
    struct kan_bd_list_node_t *first;
    struct kan_bd_list_node_t *last;
};

/// Contains hash storage structural data.
struct kan_hash_storage_t
{
    kan_allocation_group_t bucket_allocation_group;
    kan_instance_size_t bucket_count;
    kan_instance_size_t empty_buckets;
    struct kan_hash_storage_bucket_t *buckets;
    struct kan_bd_list_t items;
    kan_memory_offset_t balance_since_last_resize;
};

/// \brief Initializes given hash storage with given count of buckets and given allocation group for buckets.
CONTAINER_API void kan_hash_storage_init (struct kan_hash_storage_t *storage,
                                          kan_allocation_group_t bucket_allocation_group,
                                          kan_instance_size_t initial_bucket_count);

/// \brief Adds given node to appropriate position in hash storage.
CONTAINER_API void kan_hash_storage_add (struct kan_hash_storage_t *storage, struct kan_hash_storage_node_t *node);

/// \brief Removes given node from hash storage.
CONTAINER_API void kan_hash_storage_remove (struct kan_hash_storage_t *storage, struct kan_hash_storage_node_t *node);

/// \brief Searches for the bucket that may contain nodes with given hash value.
CONTAINER_API const struct kan_hash_storage_bucket_t *kan_hash_storage_query (const struct kan_hash_storage_t *storage,
                                                                              kan_hash_t hash);

/// \brief Sets new bucket count and fully restructures given hash storage.
CONTAINER_API void kan_hash_storage_set_bucket_count (struct kan_hash_storage_t *storage,
                                                      kan_instance_size_t bucket_count);

/// \brief Shuts down given hash storage and frees its resources.
/// \details Keep in mind that nodes lifetime is managed by user and therefore all nodes should be manually freed
///          before shutting down hash storage.
CONTAINER_API void kan_hash_storage_shutdown (struct kan_hash_storage_t *storage);

/// \brief Implements default strategy for update hash storage bucket count to appropriate values.
static inline void kan_hash_storage_update_bucket_count_default (struct kan_hash_storage_t *storage,
                                                                 kan_instance_size_t min_bucket_count_to_preserve)
{
    const bool can_grow = storage->empty_buckets * KAN_CONTAINER_HASH_STORAGE_DEFAULT_EBM < storage->bucket_count ||
                          storage->bucket_count <= KAN_CONTAINER_HASH_STORAGE_DEFAULT_MIN_FOR_EBM;

    const bool has_many_items =
        storage->items.size >= storage->bucket_count * KAN_CONTAINER_HASH_STORAGE_DEFAULT_LOAD_FACTOR;

    const bool can_shrink = storage->empty_buckets * KAN_CONTAINER_HASH_STORAGE_DEFAULT_EBM >= storage->bucket_count &&
                            storage->bucket_count > min_bucket_count_to_preserve;

    const bool reducing_item_count =
        storage->balance_since_last_resize <=
        -(kan_memory_offset_t) storage->bucket_count / KAN_CONTAINER_HASH_STORAGE_DEFAULT_EBM;

    if (can_grow && has_many_items)
    {
        kan_hash_storage_set_bucket_count (
            storage, storage->bucket_count * KAN_MAX (2u, storage->items.size / storage->bucket_count *
                                                              KAN_CONTAINER_HASH_STORAGE_DEFAULT_LOAD_FACTOR));
    }
    else if (can_shrink && reducing_item_count)
    {
        kan_hash_storage_set_bucket_count (
            storage, KAN_MAX (min_bucket_count_to_preserve, storage->bucket_count - storage->empty_buckets));
    }
}

KAN_C_HEADER_END
