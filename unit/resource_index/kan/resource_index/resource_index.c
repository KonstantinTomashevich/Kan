#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <stddef.h>
#include <string.h>

#include <kan/memory/allocation.h>
#include <kan/resource_index/resource_index.h>

static bool statics_initialized = false;
static kan_allocation_group_t allocation_group;

static void ensure_statics_initialized (void)
{
    // We can avoid locking, because initialization can safely be executed several times.
    if (!statics_initialized)
    {
        allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "resource_index");
        statics_initialized = true;
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
    for (kan_loop_size_t index = 0u; index < container->items.size; ++index)
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
    for (kan_loop_size_t native_index = 0u; native_index < index->native.size; ++native_index)
    {
        kan_resource_index_native_container_shutdown (
            &((struct kan_resource_index_native_container_t *) index->native.data)[native_index]);
    }

    for (kan_loop_size_t third_party_index = 0u; third_party_index < index->third_party.size; ++third_party_index)
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

RESOURCE_INDEX_API void kan_resource_index_add_native_entry (struct kan_resource_index_t *index,
                                                             kan_interned_string_t type,
                                                             kan_interned_string_t name,
                                                             enum kan_resource_index_native_item_format_t format,
                                                             const char *path)
{
    struct kan_dynamic_array_t *items_array = NULL;
    for (kan_loop_size_t type_index = 0u; type_index < index->native.size; ++type_index)
    {
        struct kan_resource_index_native_container_t *container =
            &((struct kan_resource_index_native_container_t *) index->native.data)[type_index];

        if (container->type == type)
        {
            items_array = &container->items;
            break;
        }
    }

    if (!items_array)
    {
        void *spot = kan_dynamic_array_add_last (&index->native);
        if (!spot)
        {
            kan_dynamic_array_set_capacity (&index->native, index->native.capacity * 2u);
            spot = kan_dynamic_array_add_last (&index->native);
        }

        struct kan_resource_index_native_container_t *new_container =
            (struct kan_resource_index_native_container_t *) spot;
        kan_resource_index_native_container_init (new_container);
        new_container->type = type;
        items_array = &new_container->items;
    }

    void *spot = kan_dynamic_array_add_last (items_array);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (items_array, items_array->capacity * 2u);
        spot = kan_dynamic_array_add_last (items_array);
    }

    struct kan_resource_index_native_item_t *item = (struct kan_resource_index_native_item_t *) spot;
    kan_resource_index_native_item_init (item);
    item->name = name;
    item->format = format;

    const kan_instance_size_t path_length = (kan_instance_size_t) strlen (path);
    item->path =
        kan_allocate_general (kan_resource_index_get_string_allocation_group (), path_length + 1u, _Alignof (char));
    memcpy (item->path, path, path_length + 1u);
}

RESOURCE_INDEX_API void kan_resource_index_add_third_party_entry (struct kan_resource_index_t *index,
                                                                  kan_interned_string_t name,
                                                                  const char *path,
                                                                  kan_file_size_t size)
{
    void *spot = kan_dynamic_array_add_last (&index->third_party);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (&index->third_party, index->third_party.capacity * 2u);
        spot = kan_dynamic_array_add_last (&index->third_party);
    }

    struct kan_resource_index_third_party_item_t *item = (struct kan_resource_index_third_party_item_t *) spot;
    kan_resource_index_third_party_item_init (item);
    item->name = name;
    item->size = size;

    const kan_instance_size_t path_length = (kan_instance_size_t) strlen (path);
    item->path =
        kan_allocate_general (kan_resource_index_get_string_allocation_group (), path_length + 1u, _Alignof (char));
    memcpy (item->path, path, path_length + 1u);
}

void kan_resource_index_extract_info_from_path (const char *path, struct kan_resource_index_info_from_path_t *output)
{
    const char *path_end = path;
    const char *last_separator = NULL;

    while (*path_end)
    {
        if (*path_end == '/')
        {
            last_separator = path_end;
        }

        ++path_end;
    }

    const char *name_begin = last_separator ? last_separator + 1u : path;
    const char *name_end = path_end;

    const kan_file_size_t path_length = path_end - path;
    if (path_length > 4u && *(path_end - 4u) == '.' && *(path_end - 3u) == 'b' && *(path_end - 2u) == 'i' &&
        *(path_end - 1u) == 'n')
    {
        name_end = path_end - 4u;
        output->native = true;
        output->native_format = KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY;
    }
    else if (path_length > 3u && *(path_end - 3u) == '.' && *(path_end - 2u) == 'r' && *(path_end - 1u) == 'd')
    {
        name_end = path_end - 3u;
        output->native = true;
        output->native_format = KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA;
    }
    else
    {
        output->native = false;
    }

    output->name = kan_char_sequence_intern (name_begin, name_end);
}
