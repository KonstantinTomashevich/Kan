#include <string.h>

#include <kan/api_common/min_max.h>
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
}

void kan_resource_index_add_entry (struct kan_resource_index_t *index,
                                   kan_interned_string_t type,
                                   kan_interned_string_t name,
                                   const char *path)
{
    struct kan_dynamic_array_t *items_array = NULL;
    for (kan_loop_size_t type_index = 0u; type_index < index->containers.size; ++type_index)
    {
        struct kan_resource_index_container_t *container =
            &((struct kan_resource_index_container_t *) index->containers.data)[type_index];

        if (container->type == type)
        {
            items_array = &container->items;
            break;
        }
    }

    if (!items_array)
    {
        struct kan_resource_index_container_t *new_container = kan_dynamic_array_add_last (&index->containers);
        if (!new_container)
        {
            kan_dynamic_array_set_capacity (&index->containers, KAN_MAX (KAN_RESOURCE_PIPELINE_RUNTIME_INDEX_BASE_TYPES,
                                                                         index->containers.capacity * 2u));
            new_container = kan_dynamic_array_add_last (&index->containers);
        }

        kan_resource_index_container_init (new_container);
        new_container->type = type;
        items_array = &new_container->items;
    }

    struct kan_resource_index_item_t *item = kan_dynamic_array_add_last (items_array);
    if (!item)
    {
        kan_dynamic_array_set_capacity (
            items_array, KAN_MAX (KAN_RESOURCE_PIPELINE_RUNTIME_INDEX_BASE_ITEMS, items_array->capacity * 2u));
        item = kan_dynamic_array_add_last (items_array);
    }

    kan_resource_index_item_init (item);
    item->name = name;

    const kan_instance_size_t path_length = (kan_instance_size_t) strlen (path);
    item->path = kan_allocate_general (kan_resource_index_get_allocation_group (), path_length + 1u, alignof (char));
    memcpy (item->path, path, path_length + 1u);
}

void kan_resource_index_shutdown (struct kan_resource_index_t *index)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (index->containers, kan_resource_index_container);
}
