#define _CRT_SECURE_NO_WARNINGS

#include <stddef.h>
#include <string.h>

#include <kan/container/hash_storage.h>
#include <kan/hash/hash.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/reflection/generated_reflection.h>
#include <kan/resource_index/resource_index.h>

KAN_REFLECTION_EXPECT_UNIT_REGISTRAR_LOCAL (resource_index);

KAN_LOG_DEFINE_CATEGORY (resource_index);

struct resource_index_native_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t type;
    kan_interned_string_t name;
    char *path;
};

struct resource_index_third_party_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
    char *path;
    uint64_t size;
};

struct resource_index_t
{
    struct kan_hash_storage_t native;
    struct kan_hash_storage_t third_party;
};

static kan_bool_t statics_initialized = KAN_FALSE;
static kan_allocation_group_t main_group;
static kan_allocation_group_t serialized_group;
static kan_allocation_group_t instanced_group;

static void ensure_statics_initialized (void)
{
    // We can avoid locking, because initialization can safely be executed several times.
    if (!statics_initialized)
    {
        main_group = kan_allocation_group_get_child (kan_allocation_group_root (), "resource_index");
        serialized_group = kan_allocation_group_get_child (main_group, "serialized");
        instanced_group = kan_allocation_group_get_child (main_group, "instanced");
        statics_initialized = KAN_TRUE;
    }
}

void kan_serialized_resource_index_native_item_init (struct kan_serialized_resource_index_native_item_t *item)
{
    item->name = NULL;
    item->path = NULL;
}

void kan_serialized_resource_index_native_item_shutdown (struct kan_serialized_resource_index_native_item_t *item)
{
    if (item->path)
    {
        kan_free_general (serialized_group, item->path, strlen (item->path) + 1u);
    }
}

void kan_serialized_resource_index_native_container_init (
    struct kan_serialized_resource_index_native_container_t *container)
{
    container->type = NULL;
    kan_dynamic_array_init (&container->items, KAN_RESOURCE_INDEX_NATIVE_CONTAINER_INITIAL,
                            sizeof (struct kan_serialized_resource_index_native_item_t),
                            _Alignof (struct kan_serialized_resource_index_native_item_t), serialized_group);
}

void kan_serialized_resource_index_native_container_shutdown (
    struct kan_serialized_resource_index_native_container_t *container)
{
    for (uint64_t index = 0u; index < container->items.size; ++index)
    {
        kan_serialized_resource_index_native_item_shutdown (
            &((struct kan_serialized_resource_index_native_item_t *) container->items.data)[index]);
    }

    kan_dynamic_array_shutdown (&container->items);
}

void kan_serialized_resource_index_third_party_item_init (struct kan_serialized_resource_index_third_party_item_t *item)
{
    item->name = NULL;
    item->path = NULL;
    item->size = 0u;
}

void kan_serialized_resource_index_third_party_item_shutdown (
    struct kan_serialized_resource_index_third_party_item_t *item)
{
    if (item->path)
    {
        kan_free_general (serialized_group, item->path, strlen (item->path) + 1u);
    }
}

void kan_serialized_resource_index_init (struct kan_serialized_resource_index_t *index)
{
    ensure_statics_initialized ();
    kan_dynamic_array_init (&index->native, KAN_RESOURCE_INDEX_NATIVE_ARRAY_INITIAL,
                            sizeof (struct kan_serialized_resource_index_native_container_t),
                            _Alignof (struct kan_serialized_resource_index_native_container_t), serialized_group);

    kan_dynamic_array_init (&index->third_party, KAN_RESOURCE_INDEX_THIRD_PARTY_ARRAY_INITIAL,
                            sizeof (struct kan_serialized_resource_index_third_party_item_t),
                            _Alignof (struct kan_serialized_resource_index_third_party_item_t), serialized_group);
}

void kan_serialized_resource_index_shutdown (struct kan_serialized_resource_index_t *index)
{
    for (uint64_t native_index = 0u; native_index < index->native.size; ++native_index)
    {
        kan_serialized_resource_index_native_container_shutdown (
            &((struct kan_serialized_resource_index_native_container_t *) index->native.data)[native_index]);
    }

    for (uint64_t third_party_index = 0u; third_party_index < index->third_party.size; ++third_party_index)
    {
        kan_serialized_resource_index_third_party_item_shutdown (
            &((struct kan_serialized_resource_index_third_party_item_t *) index->third_party.data)[third_party_index]);
    }

    kan_dynamic_array_shutdown (&index->native);
    kan_dynamic_array_shutdown (&index->third_party);
}

kan_allocation_group_t kan_serialized_resource_index_get_string_allocation_group (void)
{
    ensure_statics_initialized ();
    return serialized_group;
}

kan_resource_index_t kan_resource_index_create (void)
{
    ensure_statics_initialized ();
    struct resource_index_t *index = kan_allocate_batched (instanced_group, sizeof (struct resource_index_t));
    kan_hash_storage_init (&index->native, instanced_group, KAN_RESOURCE_INDEX_NATIVE_INITIAL_BUCKETS);
    kan_hash_storage_init (&index->third_party, instanced_group, KAN_RESOURCE_INDEX_THIRD_PARTY_INITIAL_BUCKETS);
    return (kan_resource_index_t) index;
}

void kan_resource_index_load (kan_resource_index_t index, const struct kan_serialized_resource_index_t *serialized)
{
    for (uint64_t type_index = 0u; type_index < serialized->native.size; ++type_index)
    {
        struct kan_serialized_resource_index_native_container_t *container =
            &((struct kan_serialized_resource_index_native_container_t *) serialized->native.data)[type_index];

        for (uint64_t item_index = 0u; item_index < container->items.size; ++item_index)
        {
            struct kan_serialized_resource_index_native_item_t *item =
                &((struct kan_serialized_resource_index_native_item_t *) container->items.data)[item_index];

            kan_resource_index_add_native (index, container->type, item->name, item->path);
        }
    }

    for (uint64_t item_index = 0u; item_index < serialized->third_party.size; ++item_index)
    {
        struct kan_serialized_resource_index_third_party_item_t *item =
            &((struct kan_serialized_resource_index_third_party_item_t *) serialized->third_party.data)[item_index];

        kan_resource_index_add_third_party (index, item->name, item->path, item->size);
    }
}

static inline struct kan_serialized_resource_index_native_container_t *kan_serialized_resource_index_get_or_add_type (
    struct kan_serialized_resource_index_t *index, kan_interned_string_t type)
{
    for (uint64_t type_index = 0u; type_index < index->native.size; ++type_index)
    {
        struct kan_serialized_resource_index_native_container_t *container =
            &((struct kan_serialized_resource_index_native_container_t *) index->native.data)[type_index];

        if (container->type == type)
        {
            return container;
        }
    }

    struct kan_serialized_resource_index_native_container_t *container =
        (struct kan_serialized_resource_index_native_container_t *) kan_dynamic_array_add_last (&index->native);

    if (!container)
    {
        kan_dynamic_array_set_capacity (&index->native, index->native.capacity * 2u);
        container =
            (struct kan_serialized_resource_index_native_container_t *) kan_dynamic_array_add_last (&index->native);
    }

    kan_serialized_resource_index_native_container_init (container);
    container->type = type;
    return container;
}

void kan_resource_index_save (kan_resource_index_t index, struct kan_serialized_resource_index_t *serialized)
{
    struct resource_index_t *index_data = (struct resource_index_t *) index;
    struct resource_index_native_node_t *native_node =
        (struct resource_index_native_node_t *) index_data->native.items.first;

    while (native_node)
    {
        struct kan_serialized_resource_index_native_container_t *container =
            kan_serialized_resource_index_get_or_add_type (serialized, native_node->type);

        struct kan_serialized_resource_index_native_item_t *item =
            (struct kan_serialized_resource_index_native_item_t *) kan_dynamic_array_add_last (&container->items);

        if (!item)
        {
            kan_dynamic_array_set_capacity (&container->items, container->items.capacity * 2u);
            item =
                (struct kan_serialized_resource_index_native_item_t *) kan_dynamic_array_add_last (&container->items);
        }

        kan_serialized_resource_index_native_item_init (item);
        item->name = native_node->name;

        const uint64_t length = strlen (native_node->path);
        item->path = kan_allocate_general (serialized_group, length + 1u, _Alignof (char));
        memcpy (item->path, native_node->path, length + 1u);
        native_node = (struct resource_index_native_node_t *) native_node->node.list_node.next;
    }

    struct resource_index_third_party_node_t *third_party_node =
        (struct resource_index_third_party_node_t *) index_data->third_party.items.first;

    while (third_party_node)
    {
        struct kan_serialized_resource_index_third_party_item_t *item =
            (struct kan_serialized_resource_index_third_party_item_t *) kan_dynamic_array_add_last (
                &serialized->third_party);

        if (!item)
        {
            kan_dynamic_array_set_capacity (&serialized->third_party, serialized->third_party.capacity * 2u);
            item = (struct kan_serialized_resource_index_third_party_item_t *) kan_dynamic_array_add_last (
                &serialized->third_party);
        }

        kan_serialized_resource_index_third_party_item_init (item);
        item->name = third_party_node->name;
        item->size = third_party_node->size;

        const uint64_t length = strlen (third_party_node->path);
        item->path = kan_allocate_general (serialized_group, length + 1u, _Alignof (char));
        memcpy (item->path, third_party_node->path, length + 1u);
        third_party_node = (struct resource_index_third_party_node_t *) third_party_node->node.list_node.next;
    }
}

const char *kan_resource_index_get_native (kan_resource_index_t index,
                                           kan_interned_string_t type,
                                           kan_interned_string_t name)
{
    struct resource_index_t *index_data = (struct resource_index_t *) index;
    const uint64_t hash = kan_hash_combine ((uint64_t) type, (uint64_t) name);

    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (&index_data->native, hash);
    struct resource_index_native_node_t *node = (struct resource_index_native_node_t *) bucket->first;
    const struct resource_index_native_node_t *node_end =
        (struct resource_index_native_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->type == type && node->name == name)
        {
            return node->path;
        }

        node = (struct resource_index_native_node_t *) node->node.list_node.next;
    }

    return NULL;
}

const char *kan_resource_index_get_third_party (kan_resource_index_t index,
                                                kan_interned_string_t name,
                                                uint64_t *size_output)
{
    struct resource_index_t *index_data = (struct resource_index_t *) index;
    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (&index_data->third_party, (uint64_t) name);
    struct resource_index_third_party_node_t *node = (struct resource_index_third_party_node_t *) bucket->first;
    const struct resource_index_third_party_node_t *node_end =
        (struct resource_index_third_party_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == name)
        {
            if (size_output)
            {
                *size_output = node->size;
            }

            return node->path;
        }

        node = (struct resource_index_third_party_node_t *) node->node.list_node.next;
    }

    return NULL;
}

kan_bool_t kan_resource_index_add_native (kan_resource_index_t index,
                                          kan_interned_string_t type,
                                          kan_interned_string_t name,
                                          const char *path)
{
    if (kan_resource_index_get_native (index, type, name))
    {
        KAN_LOG (resource_index, KAN_LOG_ERROR,
                 "Caught attempt to add native resource \"%s\" of type \"%s\", but native resource with this name and "
                 "type already exists.",
                 name, type)
        return KAN_FALSE;
    }

    struct resource_index_t *index_data = (struct resource_index_t *) index;
    struct resource_index_native_node_t *node =
        kan_allocate_batched (instanced_group, sizeof (struct resource_index_native_node_t));
    node->node.hash = kan_hash_combine ((uint64_t) type, (uint64_t) name);

    node->type = type;
    node->name = name;

    const uint64_t length = strlen (path);
    node->path = kan_allocate_general (instanced_group, length + 1u, _Alignof (char));
    memcpy (node->path, path, length + 1u);

    if (index_data->native.items.size >= index_data->native.bucket_count * KAN_RESOURCE_INDEX_NATIVE_LOAD_FACTOR)
    {
        kan_hash_storage_set_bucket_count (&index_data->native, index_data->native.bucket_count * 2u);
    }

    kan_hash_storage_add (&index_data->native, &node->node);
    return KAN_TRUE;
}

kan_bool_t kan_resource_index_add_third_party (kan_resource_index_t index,
                                               kan_interned_string_t name,
                                               const char *path,
                                               uint64_t size)
{
    if (kan_resource_index_get_third_party (index, name, NULL))
    {
        KAN_LOG (resource_index, KAN_LOG_ERROR,
                 "Caught attempt to add third party resource \"%s\", but third party resource with this name already "
                 "exists.",
                 name)
        return KAN_FALSE;
    }

    struct resource_index_t *index_data = (struct resource_index_t *) index;
    struct resource_index_third_party_node_t *node =
        kan_allocate_batched (instanced_group, sizeof (struct resource_index_third_party_node_t));
    node->node.hash = (uint64_t) name;

    node->name = name;
    node->size = size;

    const uint64_t length = strlen (path);
    node->path = kan_allocate_general (instanced_group, length + 1u, _Alignof (char));
    memcpy (node->path, path, length + 1u);

    if (index_data->third_party.items.size >=
        index_data->third_party.bucket_count * KAN_RESOURCE_INDEX_THIRD_PARTY_LOAD_FACTOR)
    {
        kan_hash_storage_set_bucket_count (&index_data->third_party, index_data->third_party.bucket_count * 2u);
    }

    kan_hash_storage_add (&index_data->third_party, &node->node);
    return KAN_TRUE;
}

void kan_resource_index_destroy (kan_resource_index_t index)
{
    struct resource_index_t *resource_index = (struct resource_index_t *) index;

    struct resource_index_native_node_t *native_node =
        (struct resource_index_native_node_t *) resource_index->native.items.first;

    while (native_node)
    {
        struct resource_index_native_node_t *next =
            (struct resource_index_native_node_t *) native_node->node.list_node.next;

        kan_free_general (instanced_group, native_node->path, strlen (native_node->path) + 1u);
        kan_free_batched (instanced_group, native_node);
        native_node = next;
    }

    struct resource_index_third_party_node_t *third_party_node =
        (struct resource_index_third_party_node_t *) resource_index->third_party.items.first;

    while (third_party_node)
    {
        struct resource_index_third_party_node_t *next =
            (struct resource_index_third_party_node_t *) third_party_node->node.list_node.next;

        kan_free_general (instanced_group, third_party_node->path, strlen (third_party_node->path) + 1u);
        kan_free_batched (instanced_group, third_party_node);
        third_party_node = next;
    }

    kan_hash_storage_shutdown (&resource_index->native);
    kan_hash_storage_shutdown (&resource_index->third_party);
}
