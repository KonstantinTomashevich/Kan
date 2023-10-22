#define _CRT_SECURE_NO_WARNINGS

#include <memory.h>
#include <qsort.h>

#include <kan/container/hash_storage.h>
#include <kan/error/critical.h>
#include <kan/hash/hash.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/reflection/patch.h>
#include <kan/reflection/registry.h>

KAN_LOG_DEFINE_CATEGORY (reflection_registry);
KAN_LOG_DEFINE_CATEGORY (reflection_patch_builder);

struct enum_node_t
{
    struct kan_hash_storage_node_t node;
    const struct kan_reflection_enum_t *enum_reflection;
};

struct struct_node_t
{
    struct kan_hash_storage_node_t node;
    const struct kan_reflection_struct_t *struct_reflection;
};

struct enum_meta_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t enum_name;
    kan_interned_string_t meta_type_name;
    const void *meta;
};

struct enum_value_meta_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t enum_name;
    kan_interned_string_t enum_value_name;
    kan_interned_string_t meta_type_name;
    const void *meta;
};

struct struct_meta_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t struct_name;
    kan_interned_string_t meta_type_name;
    const void *meta;
};

struct struct_field_meta_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t struct_name;
    kan_interned_string_t struct_field_name;
    kan_interned_string_t meta_type_name;
    const void *meta;
};

struct registry_t
{
    kan_allocation_group_t allocation_group;
    kan_allocation_group_t patch_allocation_group;
    struct kan_hash_storage_t enum_storage;
    struct kan_hash_storage_t struct_storage;
    struct kan_hash_storage_t enum_meta_storage;
    struct kan_hash_storage_t enum_value_meta_storage;
    struct kan_hash_storage_t struct_meta_storage;
    struct kan_hash_storage_t struct_field_meta_storage;
    struct compiled_patch_t *first_patch;
};

struct patch_builder_node_t
{
    struct patch_builder_node_t *next;
    uint16_t offset;
    uint16_t size;
    uint8_t data[];
};

struct patch_builder_t
{
    struct patch_builder_node_t *first_node;
    struct patch_builder_node_t *last_node;
    uint64_t node_count;
    kan_stack_allocator_t stack_allocator;
};

struct compiled_patch_node_t
{
    uint16_t offset;
    uint16_t size;
    uint8_t data[];
};

struct compiled_patch_t
{
    const struct kan_reflection_struct_t *type;
    struct compiled_patch_node_t *begin;
    struct compiled_patch_node_t *end;

    struct registry_t *registry;
    struct compiled_patch_t *next;
    struct compiled_patch_t *previous;
};

kan_reflection_registry_t kan_reflection_registry_create ()
{
    const kan_allocation_group_t group =
        kan_allocation_group_get_child (kan_allocation_group_stack_get (), "reflection_registry");
    struct registry_t *registry = (struct registry_t *) kan_allocate_batched (group, sizeof (struct registry_t));
    registry->allocation_group = group;
    registry->patch_allocation_group = kan_allocation_group_get_child (group, "patch");
    registry->first_patch = NULL;

    kan_hash_storage_init (&registry->enum_storage, group, KAN_REFLECTION_ENUM_INITIAL_BUCKETS);
    kan_hash_storage_init (&registry->struct_storage, group, KAN_REFLECTION_STRUCT_INITIAL_BUCKETS);
    kan_hash_storage_init (&registry->enum_meta_storage, group, KAN_REFLECTION_ENUM_META_INITIAL_BUCKETS);
    kan_hash_storage_init (&registry->enum_value_meta_storage, group, KAN_REFLECTION_ENUM_VALUE_META_INITIAL_BUCKETS);
    kan_hash_storage_init (&registry->struct_meta_storage, group, KAN_REFLECTION_STRUCT_META_INITIAL_BUCKETS);
    kan_hash_storage_init (&registry->struct_field_meta_storage, group,
                           KAN_REFLECTION_STRUCT_FIELD_META_INITIAL_BUCKETS);

    return (uint64_t) registry;
}

kan_bool_t kan_reflection_registry_add_enum (kan_reflection_registry_t registry,
                                             const struct kan_reflection_enum_t *enum_reflection)
{
    if (kan_reflection_registry_query_enum (registry, enum_reflection->name))
    {
        return KAN_FALSE;
    }

#if defined(KAN_REFLECTION_WITH_VALIDATION) && defined(KAN_WITH_ASSERT)
    KAN_ASSERT (enum_reflection->values_count > 0u)
    KAN_ASSERT (enum_reflection->values)
#endif

    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct enum_node_t *node =
        (struct enum_node_t *) kan_allocate_batched (registry_struct->allocation_group, sizeof (struct enum_node_t));
    node->node.hash = (uint64_t) enum_reflection->name;
    node->enum_reflection = enum_reflection;

    if (registry_struct->enum_storage.items.size >=
        registry_struct->enum_storage.bucket_count * KAN_REFLECTION_ENUM_LOAD_FACTOR)
    {
        kan_hash_storage_set_bucket_count (&registry_struct->enum_storage,
                                           registry_struct->enum_storage.bucket_count * 2u);
    }

    kan_hash_storage_add (&registry_struct->enum_storage, &node->node);
    return KAN_TRUE;
}

kan_bool_t kan_reflection_registry_add_enum_meta (kan_reflection_registry_t registry,
                                                  kan_interned_string_t enum_name,
                                                  kan_interned_string_t meta_type_name,
                                                  const void *meta)
{
    if (kan_reflection_registry_query_enum_meta (registry, enum_name, meta_type_name))
    {
        return KAN_FALSE;
    }

    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct enum_meta_node_t *node = (struct enum_meta_node_t *) kan_allocate_batched (registry_struct->allocation_group,
                                                                                      sizeof (struct enum_meta_node_t));

    node->node.hash = kan_hash_combine ((uint64_t) enum_name, (uint64_t) meta_type_name);
    node->enum_name = enum_name;
    node->meta_type_name = meta_type_name;
    node->meta = meta;

    if (registry_struct->enum_meta_storage.items.size >=
        registry_struct->enum_meta_storage.bucket_count * KAN_REFLECTION_ENUM_META_LOAD_FACTOR)
    {
        kan_hash_storage_set_bucket_count (&registry_struct->enum_meta_storage,
                                           registry_struct->enum_meta_storage.bucket_count * 2u);
    }

    kan_hash_storage_add (&registry_struct->enum_meta_storage, &node->node);
    return KAN_TRUE;
}

kan_bool_t kan_reflection_registry_add_enum_value_meta (kan_reflection_registry_t registry,
                                                        kan_interned_string_t enum_name,
                                                        kan_interned_string_t enum_value_name,
                                                        kan_interned_string_t meta_type_name,
                                                        const void *meta)
{
    if (kan_reflection_registry_query_enum_value_meta (registry, enum_name, enum_value_name, meta_type_name))
    {
        return KAN_FALSE;
    }

    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct enum_value_meta_node_t *node = (struct enum_value_meta_node_t *) kan_allocate_batched (
        registry_struct->allocation_group, sizeof (struct enum_value_meta_node_t));

    node->node.hash = kan_hash_combine (kan_hash_combine ((uint64_t) enum_name, (uint64_t) enum_value_name),
                                        (uint64_t) meta_type_name);
    node->enum_name = enum_name;
    node->enum_value_name = enum_value_name;
    node->meta_type_name = meta_type_name;
    node->meta = meta;

    if (registry_struct->enum_value_meta_storage.items.size >=
        registry_struct->enum_value_meta_storage.bucket_count * KAN_REFLECTION_ENUM_VALUE_META_LOAD_FACTOR)
    {
        kan_hash_storage_set_bucket_count (&registry_struct->enum_value_meta_storage,
                                           registry_struct->enum_value_meta_storage.bucket_count * 2u);
    }

    kan_hash_storage_add (&registry_struct->enum_value_meta_storage, &node->node);
    return KAN_TRUE;
}

kan_bool_t kan_reflection_registry_add_struct (kan_reflection_registry_t registry,
                                               const struct kan_reflection_struct_t *struct_reflection)
{
    if (kan_reflection_registry_query_struct (registry, struct_reflection->name))
    {
        return KAN_FALSE;
    }

#if defined(KAN_REFLECTION_WITH_VALIDATION) && defined(KAN_WITH_ASSERT)
    KAN_ASSERT (struct_reflection->size > 0u)
    KAN_ASSERT (struct_reflection->alignment > 0u)
    KAN_ASSERT (struct_reflection->size % struct_reflection->alignment == 0u)
    KAN_ASSERT (struct_reflection->fields_count > 0u)
    KAN_ASSERT (struct_reflection->fields)

    for (uint64_t index = 0u; index < struct_reflection->fields_count; ++index)
    {
        const struct kan_reflection_field_t *field_reflection = &struct_reflection->fields[index];
        if (index > 0u)
        {
            KAN_ASSERT (field_reflection->offset >= struct_reflection->fields[index - 1u].offset)
        }

        KAN_ASSERT (field_reflection->name)
        KAN_ASSERT (field_reflection->size > 0u)

        switch (field_reflection->archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            KAN_ASSERT (field_reflection->size == sizeof (int8_t) || field_reflection->size == sizeof (int16_t) ||
                        field_reflection->size == sizeof (int32_t) || field_reflection->size == sizeof (int64_t))
            break;

        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            KAN_ASSERT (field_reflection->size == sizeof (uint8_t) || field_reflection->size == sizeof (uint16_t) ||
                        field_reflection->size == sizeof (uint32_t) || field_reflection->size == sizeof (uint64_t))
            break;

        case KAN_REFLECTION_ARCHETYPE_FLOATING:
            KAN_ASSERT (field_reflection->size == sizeof (float) || field_reflection->size == sizeof (double))
            break;

        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            KAN_ASSERT (field_reflection->size == sizeof (char *))
            break;

        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            KAN_ASSERT (field_reflection->size == sizeof (kan_interned_string_t))
            break;

        case KAN_REFLECTION_ARCHETYPE_ENUM:
            KAN_ASSERT (field_reflection->size == sizeof (int))
            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        case KAN_REFLECTION_ARCHETYPE_PATCH:
            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            KAN_ASSERT (field_reflection->archetype_inline_array.items_count > 0u)
            KAN_ASSERT (
                field_reflection->archetype_inline_array.item_archetype != KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY &&
                field_reflection->archetype_inline_array.item_archetype != KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY)
            break;

        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            KAN_ASSERT (field_reflection->archetype_dynamic_array.archetype != KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY &&
                        field_reflection->archetype_dynamic_array.archetype != KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY)
            break;
        }
    }
#endif

    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct struct_node_t *node = (struct struct_node_t *) kan_allocate_batched (registry_struct->allocation_group,
                                                                                sizeof (struct struct_node_t));
    node->node.hash = (uint64_t) struct_reflection->name;
    node->struct_reflection = struct_reflection;

    if (registry_struct->struct_storage.items.size >=
        registry_struct->struct_storage.bucket_count * KAN_REFLECTION_STRUCT_LOAD_FACTOR)
    {
        kan_hash_storage_set_bucket_count (&registry_struct->struct_storage,
                                           registry_struct->struct_storage.bucket_count * 2u);
    }

    kan_hash_storage_add (&registry_struct->struct_storage, &node->node);
    return KAN_TRUE;
}

kan_bool_t kan_reflection_registry_add_struct_meta (kan_reflection_registry_t registry,
                                                    kan_interned_string_t struct_name,
                                                    kan_interned_string_t meta_type_name,
                                                    const void *meta)
{
    if (kan_reflection_registry_query_struct_meta (registry, struct_name, meta_type_name))
    {
        return KAN_FALSE;
    }

    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct struct_meta_node_t *node = (struct struct_meta_node_t *) kan_allocate_batched (
        registry_struct->allocation_group, sizeof (struct struct_meta_node_t));

    node->node.hash = kan_hash_combine ((uint64_t) struct_name, (uint64_t) meta_type_name);
    node->struct_name = struct_name;
    node->meta_type_name = meta_type_name;
    node->meta = meta;

    if (registry_struct->struct_meta_storage.items.size >=
        registry_struct->struct_meta_storage.bucket_count * KAN_REFLECTION_STRUCT_META_LOAD_FACTOR)
    {
        kan_hash_storage_set_bucket_count (&registry_struct->struct_meta_storage,
                                           registry_struct->struct_meta_storage.bucket_count * 2u);
    }

    kan_hash_storage_add (&registry_struct->struct_meta_storage, &node->node);
    return KAN_TRUE;
}

kan_bool_t kan_reflection_registry_add_struct_field_meta (kan_reflection_registry_t registry,
                                                          kan_interned_string_t struct_name,
                                                          kan_interned_string_t struct_field_name,
                                                          kan_interned_string_t meta_type_name,
                                                          const void *meta)
{
    if (kan_reflection_registry_query_struct_field_meta (registry, struct_name, struct_field_name, meta_type_name))
    {
        return KAN_FALSE;
    }

    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct struct_field_meta_node_t *node = (struct struct_field_meta_node_t *) kan_allocate_batched (
        registry_struct->allocation_group, sizeof (struct struct_field_meta_node_t));

    node->node.hash = kan_hash_combine (kan_hash_combine ((uint64_t) struct_name, (uint64_t) struct_field_name),
                                        (uint64_t) meta_type_name);
    node->struct_name = struct_name;
    node->struct_field_name = struct_field_name;
    node->meta_type_name = meta_type_name;
    node->meta = meta;

    if (registry_struct->struct_field_meta_storage.items.size >=
        registry_struct->struct_field_meta_storage.bucket_count * KAN_REFLECTION_STRUCT_FIELD_META_LOAD_FACTOR)
    {
        kan_hash_storage_set_bucket_count (&registry_struct->struct_field_meta_storage,
                                           registry_struct->struct_field_meta_storage.bucket_count * 2u);
    }

    kan_hash_storage_add (&registry_struct->struct_field_meta_storage, &node->node);
    return KAN_TRUE;
}

const struct kan_reflection_enum_t *kan_reflection_registry_query_enum (kan_reflection_registry_t registry,
                                                                        kan_interned_string_t enum_name)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&registry_struct->enum_storage, (uint64_t) enum_name);
    struct enum_node_t *node = (struct enum_node_t *) bucket->first;
    const struct enum_node_t *end = (struct enum_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->enum_reflection->name == enum_name)
        {
            return node->enum_reflection;
        }

        node = (struct enum_node_t *) node->node.list_node.next;
    }

    return NULL;
}

const void *kan_reflection_registry_query_enum_meta (kan_reflection_registry_t registry,
                                                     kan_interned_string_t enum_name,
                                                     kan_interned_string_t meta_type_name)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    const uint64_t hash = kan_hash_combine ((uint64_t) enum_name, (uint64_t) meta_type_name);

    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (&registry_struct->enum_meta_storage, hash);
    struct enum_meta_node_t *node = (struct enum_meta_node_t *) bucket->first;
    const struct enum_meta_node_t *end = (struct enum_meta_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->enum_name == enum_name && node->meta_type_name == meta_type_name)
        {
            return node->meta;
        }

        node = (struct enum_meta_node_t *) node->node.list_node.next;
    }

    return NULL;
}

const void *kan_reflection_registry_query_enum_value_meta (kan_reflection_registry_t registry,
                                                           kan_interned_string_t enum_name,
                                                           kan_interned_string_t enum_value_name,
                                                           kan_interned_string_t meta_type_name)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    const uint64_t hash = kan_hash_combine (kan_hash_combine ((uint64_t) enum_name, (uint64_t) enum_value_name),
                                            (uint64_t) meta_type_name);

    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&registry_struct->enum_value_meta_storage, hash);
    struct enum_value_meta_node_t *node = (struct enum_value_meta_node_t *) bucket->first;
    const struct enum_value_meta_node_t *end =
        (struct enum_value_meta_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->enum_name == enum_name && node->enum_value_name == enum_value_name &&
            node->meta_type_name == meta_type_name)
        {
            return node->meta;
        }

        node = (struct enum_value_meta_node_t *) node->node.list_node.next;
    }

    return NULL;
}

const struct kan_reflection_struct_t *kan_reflection_registry_query_struct (kan_reflection_registry_t registry,
                                                                            kan_interned_string_t struct_name)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&registry_struct->struct_storage, (uint64_t) struct_name);
    struct struct_node_t *node = (struct struct_node_t *) bucket->first;
    const struct struct_node_t *end = (struct struct_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->struct_reflection->name == struct_name)
        {
            return node->struct_reflection;
        }

        node = (struct struct_node_t *) node->node.list_node.next;
    }

    return NULL;
}

const void *kan_reflection_registry_query_struct_meta (kan_reflection_registry_t registry,
                                                       kan_interned_string_t struct_name,
                                                       kan_interned_string_t meta_type_name)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    const uint64_t hash = kan_hash_combine ((uint64_t) struct_name, (uint64_t) meta_type_name);

    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&registry_struct->struct_meta_storage, hash);
    struct struct_meta_node_t *node = (struct struct_meta_node_t *) bucket->first;
    const struct struct_meta_node_t *end = (struct struct_meta_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->struct_name == struct_name && node->meta_type_name == meta_type_name)
        {
            return node->meta;
        }

        node = (struct struct_meta_node_t *) node->node.list_node.next;
    }

    return NULL;
}

const void *kan_reflection_registry_query_struct_field_meta (kan_reflection_registry_t registry,
                                                             kan_interned_string_t struct_name,
                                                             kan_interned_string_t struct_field_name,
                                                             kan_interned_string_t meta_type_name)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    const uint64_t hash = kan_hash_combine (kan_hash_combine ((uint64_t) struct_name, (uint64_t) struct_field_name),
                                            (uint64_t) meta_type_name);

    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&registry_struct->struct_field_meta_storage, hash);
    struct struct_field_meta_node_t *node = (struct struct_field_meta_node_t *) bucket->first;
    const struct struct_field_meta_node_t *end =
        (struct struct_field_meta_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->struct_name == struct_name && node->struct_field_name == struct_field_name &&
            node->meta_type_name == meta_type_name)
        {
            return node->meta;
        }

        node = (struct struct_field_meta_node_t *) node->node.list_node.next;
    }

    return NULL;
}

const struct kan_reflection_field_t *kan_reflection_registry_query_local_field (kan_reflection_registry_t registry,
                                                                                uint64_t path_length,
                                                                                kan_interned_string_t *path,
                                                                                uint64_t *absolute_offset_output)

{
    KAN_ASSERT (path_length > 1u)
    KAN_ASSERT (absolute_offset_output)
    *absolute_offset_output = 0u;
    const struct kan_reflection_struct_t *struct_reflection = kan_reflection_registry_query_struct (registry, path[0u]);

    if (!struct_reflection)
    {
        KAN_LOG (reflection_registry, KAN_LOG_WARNING, "Struct \"%s\" is not registered.", path[0u])
        return NULL;
    }

    const struct kan_reflection_field_t *field_reflection = NULL;
    for (uint64_t path_element_index = 1u; path_element_index < path_length; ++path_element_index)
    {
        kan_interned_string_t path_element = path[path_element_index];
        field_reflection = NULL;

        for (uint64_t field_index = 0u; field_index < struct_reflection->fields_count; ++field_index)
        {
            const struct kan_reflection_field_t *field_reflection_to_check = &struct_reflection->fields[field_index];
            if (field_reflection_to_check->name == path_element)
            {
                field_reflection = field_reflection_to_check;
                break;
            }
        }

        if (!field_reflection)
        {
            KAN_LOG (reflection_registry, KAN_LOG_WARNING, "Unable to find field \"%s\" of \"%s\".", path_element,
                     struct_reflection->name)
            return NULL;
        }

        *absolute_offset_output += field_reflection->offset;
        if (path_element_index != path_length - 1u)
        {
            switch (field_reflection->archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_FLOATING:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
            case KAN_REFLECTION_ARCHETYPE_PATCH:
                KAN_LOG (reflection_registry, KAN_LOG_WARNING,
                         "Cannot get subfield of field \"%s\" of struct \"%s\" as it has basic archetype.",
                         path_element, struct_reflection->name)
                return NULL;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                struct_reflection =
                    kan_reflection_registry_query_struct (registry, field_reflection->archetype_struct.type_name);

                if (!struct_reflection)
                {
                    KAN_LOG (reflection_registry, KAN_LOG_WARNING, "Struct \"%s\" is not registered.", path[0u])
                    return NULL;
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                KAN_LOG (reflection_registry, KAN_LOG_WARNING,
                         "Cannot get subfield of field \"%s\" of struct \"%s\" as it exits local scope of an object "
                         "and is allocated in separate memory block.",
                         path_element, struct_reflection->name)

                return NULL;

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                KAN_LOG (reflection_registry, KAN_LOG_WARNING,
                         "Cannot get subfield of field \"%s\" of struct \"%s\" as getting subfields of inline arrays "
                         "is not supported.",
                         path_element, struct_reflection->name)
                return NULL;
            }
        }
    }

    return field_reflection;
}

kan_reflection_registry_enum_iterator_t kan_reflection_registry_enum_iterator_create (
    kan_reflection_registry_t registry)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    return (kan_reflection_registry_enum_iterator_t) registry_struct->enum_storage.items.first;
}

const struct kan_reflection_enum_t *kan_reflection_registry_enum_iterator_get (
    kan_reflection_registry_enum_iterator_t iterator)
{
    const struct enum_node_t *node = (struct enum_node_t *) iterator;
    if (node)
    {
        return node->enum_reflection;
    }

    return NULL;
}

kan_reflection_registry_enum_iterator_t kan_reflection_registry_enum_iterator_next (
    kan_reflection_registry_enum_iterator_t iterator)
{
    const struct enum_node_t *node = (struct enum_node_t *) iterator;
    if (node)
    {
        return (kan_reflection_registry_enum_iterator_t) node->node.list_node.next;
    }

    return (kan_reflection_registry_enum_iterator_t) NULL;
}

kan_reflection_registry_struct_iterator_t kan_reflection_registry_struct_iterator_create (
    kan_reflection_registry_t registry)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    return (kan_reflection_registry_struct_iterator_t) registry_struct->struct_storage.items.first;
}

const struct kan_reflection_struct_t *kan_reflection_registry_struct_iterator_get (
    kan_reflection_registry_struct_iterator_t iterator)
{
    const struct struct_node_t *node = (struct struct_node_t *) iterator;
    if (node)
    {
        return node->struct_reflection;
    }

    return NULL;
}

kan_reflection_registry_struct_iterator_t kan_reflection_registry_struct_iterator_next (
    kan_reflection_registry_struct_iterator_t iterator)
{
    const struct struct_node_t *node = (struct struct_node_t *) iterator;
    if (node)
    {
        return (kan_reflection_registry_struct_iterator_t) node->node.list_node.next;
    }

    return (kan_reflection_registry_struct_iterator_t) NULL;
}

static void compiled_patch_destroy (struct compiled_patch_t *patch)
{
    const kan_allocation_group_t group = patch->registry->patch_allocation_group;
    kan_free_general (group, patch->begin, ((uint8_t *) patch->end) - (uint8_t *) patch->begin);
    kan_free_batched (group, patch);
}

void kan_reflection_registry_destroy (kan_reflection_registry_t registry)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct compiled_patch_t *patch = registry_struct->first_patch;

    while (patch)
    {
        struct compiled_patch_t *next = patch->next;
        compiled_patch_destroy (patch);
        patch = next;
    }

    struct kan_bd_list_node_t *node = registry_struct->enum_storage.items.first;

    while (node)
    {
        struct kan_bd_list_node_t *next = node->next;
        kan_free_batched (registry_struct->allocation_group, node);
        node = next;
    }

    node = registry_struct->struct_storage.items.first;
    while (node)
    {
        struct kan_bd_list_node_t *next = node->next;
        kan_free_batched (registry_struct->allocation_group, node);
        node = next;
    }

    node = registry_struct->enum_meta_storage.items.first;
    while (node)
    {
        struct kan_bd_list_node_t *next = node->next;
        kan_free_batched (registry_struct->allocation_group, node);
        node = next;
    }

    node = registry_struct->enum_value_meta_storage.items.first;
    while (node)
    {
        struct kan_bd_list_node_t *next = node->next;
        kan_free_batched (registry_struct->allocation_group, node);
        node = next;
    }

    node = registry_struct->struct_meta_storage.items.first;
    while (node)
    {
        struct kan_bd_list_node_t *next = node->next;
        kan_free_batched (registry_struct->allocation_group, node);
        node = next;
    }

    node = registry_struct->struct_field_meta_storage.items.first;
    while (node)
    {
        struct kan_bd_list_node_t *next = node->next;
        kan_free_batched (registry_struct->allocation_group, node);
        node = next;
    }

    kan_hash_storage_shutdown (&registry_struct->enum_storage);
    kan_hash_storage_shutdown (&registry_struct->struct_storage);
    kan_hash_storage_shutdown (&registry_struct->enum_meta_storage);
    kan_hash_storage_shutdown (&registry_struct->enum_value_meta_storage);
    kan_hash_storage_shutdown (&registry_struct->struct_meta_storage);
    kan_hash_storage_shutdown (&registry_struct->struct_field_meta_storage);

    kan_free_batched (registry_struct->allocation_group, registry_struct);
}

static kan_bool_t patch_builder_allocation_group_ready = KAN_FALSE;
static kan_allocation_group_t patch_builder_allocation_group;

static kan_allocation_group_t get_patch_builder_allocation_group ()
{
    if (!patch_builder_allocation_group_ready)
    {
        patch_builder_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "reflection_patch_builder");
        patch_builder_allocation_group_ready = KAN_TRUE;
    }

    return patch_builder_allocation_group;
}

kan_reflection_patch_builder_t kan_reflection_patch_builder_create ()
{
    struct patch_builder_t *patch_builder = (struct patch_builder_t *) kan_allocate_batched (
        get_patch_builder_allocation_group (), sizeof (struct patch_builder_t));
    patch_builder->first_node = NULL;
    patch_builder->last_node = NULL;
    patch_builder->node_count = 0u;
    patch_builder->stack_allocator =
        kan_stack_allocator_create (get_patch_builder_allocation_group (), KAN_REFLECTION_PATCH_BUILDER_STACK_SIZE);
    return (kan_reflection_patch_builder_t) patch_builder;
}

void kan_reflection_patch_builder_add_chunk (kan_reflection_patch_builder_t builder,
                                             uint64_t offset,
                                             uint64_t size,
                                             void *data)
{
    struct patch_builder_t *patch_builder = (struct patch_builder_t *) builder;
    const uint64_t node_size = sizeof (struct patch_builder_node_t) + size;

    struct patch_builder_node_t *node = (struct patch_builder_node_t *) kan_stack_allocator_allocate (
        patch_builder->stack_allocator, node_size, _Alignof (struct patch_builder_node_t));
    KAN_ASSERT (node)

    if (!node)
    {
        KAN_LOG (reflection_patch_builder, KAN_LOG_ERROR, "Patch builder memory buffer overflow.")
        return;
    }

    KAN_ASSERT (offset < UINT16_MAX)
    KAN_ASSERT (size < UINT16_MAX)
    node->next = NULL;
    node->offset = (uint16_t) offset;
    node->size = (uint16_t) size;
    memcpy (node->data, data, size);

    if (patch_builder->last_node)
    {
        patch_builder->last_node->next = node;
    }
    else
    {
        patch_builder->first_node = node;
    }

    patch_builder->last_node = node;
    ++patch_builder->node_count;
}

static void patch_builder_reset (struct patch_builder_t *patch_builder)
{
    patch_builder->first_node = NULL;
    patch_builder->last_node = NULL;
    patch_builder->node_count = 0u;
    kan_stack_allocator_reset (patch_builder->stack_allocator);
}

#if defined(KAN_REFLECTION_WITH_VALIDATION) && defined(KAN_WITH_ASSERT)
static void validate_compiled_node_internal (const struct compiled_patch_node_t *node,
                                             kan_reflection_registry_t registry,
                                             const struct kan_reflection_struct_t *type,
                                             uint64_t offset)
{
    KAN_ASSERT (type)
    for (uint64_t index = 0u; index < type->fields_count; ++index)
    {
        const struct kan_reflection_field_t *field = &type->fields[index];
        const uint64_t field_begin = field->offset + offset;

        if (field_begin > node->offset)
        {
            // We're over, no need to check the rest.
            return;
        }

        const uint64_t field_end = field_begin + field->size;
        if (field_end < node->offset)
        {
            // We can technically use binary search, but it looks like an overkill for validation.
            continue;
        }

        switch (field->archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
            // Supported archetype.
            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
            // We need to recursively check structure insides.
            validate_compiled_node_internal (
                node, registry, kan_reflection_registry_query_struct (registry, field->archetype_struct.type_name),
                field->offset);
            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            switch (field->archetype_inline_array.item_archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_FLOATING:
            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
                // Supported archetype.
                break;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                const struct kan_reflection_struct_t *element_type = kan_reflection_registry_query_struct (
                    registry, field->archetype_inline_array.item_archetype_struct.type_name);

                for (uint64_t element_index = 0u; element_index < field->archetype_inline_array.items_count;
                     ++element_index)
                {
                    validate_compiled_node_internal (node, registry, element_type,
                                                     field->offset + element_index * element_type->size);
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_PATCH:
                // Unsupported archetype.
                KAN_ASSERT (KAN_FALSE)
                break;
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_PATCH:
            // Unsupported archetype.
            KAN_ASSERT (KAN_FALSE)
            break;
        }
    }
}

static void validate_compiled_node (const struct compiled_patch_node_t *node,
                                    kan_reflection_registry_t registry,
                                    const struct kan_reflection_struct_t *type)
{
    if (!node)
    {
        return;
    }

    validate_compiled_node_internal (node, registry, type, 0u);
}
#endif

static uint64_t compiled_patch_node_add_alignment (uint64_t offset)
{
    const uint64_t modulo = offset % _Alignof (struct compiled_patch_node_t);
    if (modulo != 0u)
    {
        return _Alignof (struct compiled_patch_node_t) - modulo;
    }

    return 0u;
}

kan_reflection_patch_t kan_reflection_patch_builder_build (kan_reflection_patch_builder_t builder,
                                                           kan_reflection_registry_t registry,
                                                           const struct kan_reflection_struct_t *type)
{
    struct patch_builder_t *patch_builder = (struct patch_builder_t *) builder;
    struct registry_t *registry_struct = (struct registry_t *) registry;

    struct patch_builder_node_t **nodes_array = (struct patch_builder_node_t **) kan_stack_allocator_allocate (
        patch_builder->stack_allocator, patch_builder->node_count * sizeof (struct patch_builder_node_t *),
        _Alignof (struct patch_builder_node_t *));
    KAN_ASSERT (nodes_array)

    if (!nodes_array)
    {
        patch_builder_reset (patch_builder);
        KAN_LOG (reflection_patch_builder, KAN_LOG_ERROR, "Patch builder memory buffer overflow.")
        return KAN_REFLECTION_INVALID_PATCH;
    }

    struct patch_builder_node_t *node = patch_builder->first_node;
    for (uint64_t index = 0u; index < patch_builder->node_count; ++index)
    {
        KAN_ASSERT (node)
        nodes_array[index] = node;
        node = node->next;
    }

    {
        struct patch_builder_node_t *temporary;
        unsigned long sort_length = (unsigned long) patch_builder->node_count;

#define LESS(first_index, second_index) (nodes_array[first_index]->offset < nodes_array[second_index]->offset)
#define SWAP(first_index, second_index)                                                                                \
    temporary = nodes_array[first_index], nodes_array[first_index] = nodes_array[second_index],                        \
    nodes_array[second_index] = temporary
        QSORT (sort_length, LESS, SWAP);
#undef LESS
#undef SWAP
    }

    uint64_t patch_data_size = 0u;
    for (uint64_t index = 0u; index < patch_builder->node_count; ++index)
    {
        kan_bool_t new_node;
        if (index > 0u)
        {
            const uint16_t last_node_end = nodes_array[index - 1u]->offset + nodes_array[index - 1u]->size;
            const uint16_t new_node_begin = nodes_array[index]->offset;

            if (last_node_end == new_node_begin)
            {
                new_node = KAN_FALSE;
            }
            else if (last_node_end > new_node_begin)
            {
                patch_builder_reset (patch_builder);
                KAN_LOG (reflection_patch_builder, KAN_LOG_ERROR, "Found overlapping chunks.")
                return KAN_REFLECTION_INVALID_PATCH;
            }
            else
            {
                new_node = KAN_TRUE;
            }
        }
        else
        {
            new_node = KAN_TRUE;
        }

        if (new_node)
        {
            patch_data_size += compiled_patch_node_add_alignment (patch_data_size);
            patch_data_size += sizeof (struct compiled_patch_node_t);
        }

        patch_data_size += nodes_array[index]->size;
    }

    patch_data_size += compiled_patch_node_add_alignment (patch_data_size);
    struct compiled_patch_t *patch =
        kan_allocate_batched (registry_struct->patch_allocation_group, sizeof (struct compiled_patch_t));

    patch->type = type;
    patch->begin = kan_allocate_general (registry_struct->patch_allocation_group, patch_data_size,
                                         _Alignof (struct compiled_patch_node_t));
    patch->end = (struct compiled_patch_node_t *) (((uint8_t *) patch->begin) + patch_data_size);

    patch->registry = registry_struct;
    patch->previous = NULL;
    patch->next = registry_struct->first_patch;

    if (registry_struct->first_patch)
    {
        registry_struct->first_patch->previous = patch;
    }

    registry_struct->first_patch = patch;
    uint8_t *output = (uint8_t *) patch->begin;
    struct compiled_patch_node_t *output_node = NULL;

    for (uint64_t index = 0u; index < patch_builder->node_count; ++index)
    {
        if (index == 0u ||
            nodes_array[index - 1u]->offset + nodes_array[index - 1u]->size != nodes_array[index]->offset)
        {
            output += compiled_patch_node_add_alignment ((uint64_t) output);
#if defined(KAN_REFLECTION_WITH_VALIDATION) && defined(KAN_WITH_ASSERT)
            validate_compiled_node (output_node, registry, type);
#endif

            output_node = (struct compiled_patch_node_t *) output;
            output_node->offset = nodes_array[index]->offset;
            output_node->size = 0u;
            output += sizeof (struct compiled_patch_node_t);
        }

        memcpy (output, nodes_array[index]->data, nodes_array[index]->size);
        output_node->size += nodes_array[index]->size;
        output += nodes_array[index]->size;
    }

#if defined(KAN_REFLECTION_WITH_VALIDATION) && defined(KAN_WITH_ASSERT)
    validate_compiled_node (output_node, registry, type);
#endif

#if defined(KAN_WITH_ASSERT)
    output += compiled_patch_node_add_alignment ((uint64_t) output);
    KAN_ASSERT ((struct compiled_patch_node_t *) output == patch->end)
#endif

    patch_builder_reset (patch_builder);
    return (kan_reflection_patch_t) patch;
}

void kan_reflection_patch_builder_destroy (kan_reflection_patch_builder_t builder)
{
    struct patch_builder_t *patch_builder = (struct patch_builder_t *) builder;
    kan_stack_allocator_destroy (patch_builder->stack_allocator);
}

const struct kan_reflection_struct_t *kan_reflection_patch_get_type (kan_reflection_patch_t patch)
{
    struct compiled_patch_t *patch_data = (struct compiled_patch_t *) patch;
    return patch_data->type;
}

void kan_reflection_patch_apply (kan_reflection_patch_t patch, void *target)
{
    struct compiled_patch_t *patch_data = (struct compiled_patch_t *) patch;
    struct compiled_patch_node_t *node = patch_data->begin;
    const struct compiled_patch_node_t *end = patch_data->end;

    while (node != end)
    {
        memcpy (((uint8_t *) target) + node->offset, node->data, node->size);
        uint8_t *data_end = node->data + node->size;
        data_end += compiled_patch_node_add_alignment ((uint64_t) data_end);
        node = (struct compiled_patch_node_t *) data_end;
    }
}

kan_reflection_patch_iterator_t kan_reflection_patch_begin (kan_reflection_patch_t patch)
{
    struct compiled_patch_t *patch_data = (struct compiled_patch_t *) patch;
    return (kan_reflection_patch_iterator_t) patch_data->begin;
}

kan_reflection_patch_iterator_t kan_reflection_patch_end (kan_reflection_patch_t patch)
{
    struct compiled_patch_t *patch_data = (struct compiled_patch_t *) patch;
    return (kan_reflection_patch_iterator_t) patch_data->end;
}

kan_reflection_patch_iterator_t kan_reflection_patch_iterator_next (kan_reflection_patch_iterator_t iterator)
{
    struct compiled_patch_node_t *node = (struct compiled_patch_node_t *) iterator;
    uint8_t *data_end = node->data + node->size;
    data_end += compiled_patch_node_add_alignment ((uint64_t) data_end);
    return (kan_reflection_patch_iterator_t) data_end;
}

struct kan_reflection_patch_chunk_info_t kan_reflection_patch_iterator_get (kan_reflection_patch_iterator_t iterator)
{
    struct compiled_patch_node_t *node = (struct compiled_patch_node_t *) iterator;
    struct kan_reflection_patch_chunk_info_t info;
    info.offset = node->offset;
    info.size = node->size;
    info.data = node->data;
    return info;
}

void kan_reflection_patch_destroy (kan_reflection_patch_t patch)
{
    struct compiled_patch_t *patch_data = (struct compiled_patch_t *) patch;
    if (patch_data->previous)
    {
        patch_data->previous->next = patch_data->next;
    }
    else
    {
        KAN_ASSERT (patch_data->registry->first_patch == patch_data)
        patch_data->registry->first_patch = patch_data->next;
    }

    if (patch_data->next)
    {
        patch_data->next->previous = patch_data->previous;
    }

    compiled_patch_destroy (patch_data);
}
