#include <string.h>

#include <kan/memory/allocation.h>
#include <kan/resource_pipeline/index.h>

static bool statics_initialized = false;
static kan_allocation_group_t allocation_group;

static void ensure_statics_initialized (void)
{
    // We can avoid locking, because initialization can safely be executed several times.
    if (!statics_initialized)
    {
        allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "resource_pipeline_index");
        statics_initialized = true;
    }
}

kan_allocation_group_t kan_resource_index_get_allocation_group (void)
{
    ensure_statics_initialized ();
    return allocation_group;
}

void kan_resource_index_item_init (struct kan_resource_index_item_t *item)
{
    item->name = NULL;
    item->path = NULL;
}

void kan_resource_index_item_shutdown (struct kan_resource_index_item_t *item)
{
    if (item->path)
    {
        kan_free_general (allocation_group, item->path, strlen (item->path) + 1u);
    }
}

void kan_resource_index_container_init (struct kan_resource_index_container_t *container)
{
    container->type = NULL;
    kan_dynamic_array_init (&container->items, 0u, sizeof (struct kan_resource_index_item_t),
                            alignof (struct kan_resource_index_item_t), allocation_group);
}

void kan_resource_index_container_shutdown (struct kan_resource_index_container_t *container)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (container->items, kan_resource_index_item)
}

void kan_resource_index_init (struct kan_resource_index_t *index)
{
    ensure_statics_initialized ();
    kan_dynamic_array_init (&index->containers, 0u, sizeof (struct kan_resource_index_container_t),
                            alignof (struct kan_resource_index_container_t), allocation_group);
    kan_dynamic_array_init (&index->third_party_items, 0u, sizeof (struct kan_resource_index_item_t),
                            alignof (struct kan_resource_index_item_t), allocation_group);
}

void kan_resource_index_shutdown (struct kan_resource_index_t *index)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (index->containers, kan_resource_index_container);
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (index->third_party_items, kan_resource_index_item)
}
