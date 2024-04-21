#define _CRT_SECURE_NO_WARNINGS

#include <stddef.h>
#include <string.h>

#include <kan/memory/allocation.h>
#include <kan/resource_index/resource_index.h>

static kan_bool_t statics_initialized = KAN_FALSE;
static kan_allocation_group_t allocation_group;

static void ensure_statics_initialized (void)
{
    // We can avoid locking, because initialization can safely be executed several times.
    if (!statics_initialized)
    {
        allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "resource_index");
        statics_initialized = KAN_TRUE;
    }
}

void kan_resource_index_native_item_init (struct kan_resource_index_native_item_t *item)
{
    item->name = NULL;
    item->path = NULL;
}

void kan_resource_index_native_item_shutdown (struct kan_resource_index_native_item_t *item)
{
    if (item->path)
    {
        kan_free_general (allocation_group, item->path, strlen (item->path) + 1u);
    }
}

void kan_resource_index_native_container_init (struct kan_resource_index_native_container_t *container)
{
    container->type = NULL;
    kan_dynamic_array_init (&container->items, KAN_RESOURCE_INDEX_NATIVE_CONTAINER_INITIAL,
                            sizeof (struct kan_resource_index_native_item_t),
                            _Alignof (struct kan_resource_index_native_item_t), allocation_group);
}

void kan_resource_index_native_container_shutdown (struct kan_resource_index_native_container_t *container)
{
    for (uint64_t index = 0u; index < container->items.size; ++index)
    {
        kan_resource_index_native_item_shutdown (
            &((struct kan_resource_index_native_item_t *) container->items.data)[index]);
    }

    kan_dynamic_array_shutdown (&container->items);
}

void kan_resource_index_third_party_item_init (struct kan_resource_index_third_party_item_t *item)
{
    item->name = NULL;
    item->path = NULL;
    item->size = 0u;
}

void kan_resource_index_third_party_item_shutdown (struct kan_resource_index_third_party_item_t *item)
{
    if (item->path)
    {
        kan_free_general (allocation_group, item->path, strlen (item->path) + 1u);
    }
}

void kan_resource_index_init (struct kan_resource_index_t *index)
{
    ensure_statics_initialized ();
    kan_dynamic_array_init (&index->native, KAN_RESOURCE_INDEX_NATIVE_ARRAY_INITIAL,
                            sizeof (struct kan_resource_index_native_container_t),
                            _Alignof (struct kan_resource_index_native_container_t), allocation_group);

    kan_dynamic_array_init (&index->third_party, KAN_RESOURCE_INDEX_THIRD_PARTY_ARRAY_INITIAL,
                            sizeof (struct kan_resource_index_third_party_item_t),
                            _Alignof (struct kan_resource_index_third_party_item_t), allocation_group);
}

void kan_resource_index_shutdown (struct kan_resource_index_t *index)
{
    for (uint64_t native_index = 0u; native_index < index->native.size; ++native_index)
    {
        kan_resource_index_native_container_shutdown (
            &((struct kan_resource_index_native_container_t *) index->native.data)[native_index]);
    }

    for (uint64_t third_party_index = 0u; third_party_index < index->third_party.size; ++third_party_index)
    {
        kan_resource_index_third_party_item_shutdown (
            &((struct kan_resource_index_third_party_item_t *) index->third_party.data)[third_party_index]);
    }

    kan_dynamic_array_shutdown (&index->native);
    kan_dynamic_array_shutdown (&index->third_party);
}

kan_allocation_group_t kan_resource_index_get_string_allocation_group (void)
{
    ensure_statics_initialized ();
    return allocation_group;
}
