#include <stddef.h>

#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/reflection/field_visibility_iterator.h>
#include <kan/reflection/patch.h>
#include <kan/resource_pipeline/resource_pipeline.h>

KAN_LOG_DEFINE_CATEGORY (resource_reference);

static kan_allocation_group_t detected_references_container_allocation_group;
static kan_bool_t statics_initialized = KAN_FALSE;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        detected_references_container_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "kan_resource_pipeline_detected_container");
        statics_initialized = KAN_TRUE;
    }
}

static inline struct kan_resource_pipeline_reference_type_info_node_t *
kan_resource_pipeline_type_info_storage_query_type_node (
    struct kan_resource_pipeline_reference_type_info_storage_t *storage, kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&storage->scanned_types, (uint64_t) type_name);
    struct kan_resource_pipeline_reference_type_info_node_t *node =
        (struct kan_resource_pipeline_reference_type_info_node_t *) bucket->first;
    const struct kan_resource_pipeline_reference_type_info_node_t *node_end =
        (struct kan_resource_pipeline_reference_type_info_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->type_name == type_name)
        {
            return node;
        }

        node = (struct kan_resource_pipeline_reference_type_info_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static inline kan_bool_t is_resource_type (kan_reflection_registry_t registry,
                                           const struct kan_reflection_struct_t *struct_data,
                                           kan_interned_string_t interned_kan_resource_pipeline_resource_type_meta_t)

{
    struct kan_reflection_struct_meta_iterator_t meta_iterator = kan_reflection_registry_query_struct_meta (
        registry, struct_data->name, interned_kan_resource_pipeline_resource_type_meta_t);
    return kan_reflection_struct_meta_iterator_get (&meta_iterator) ? KAN_TRUE : KAN_FALSE;
}

static inline void kan_resource_pipeline_type_info_node_add_field (
    struct kan_resource_pipeline_reference_type_info_node_t *type_node,
    const struct kan_reflection_field_t *field,
    const struct kan_resource_pipeline_reference_meta_t *meta)
{
    void *spot = kan_dynamic_array_add_last (&type_node->fields_to_check);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (&type_node->fields_to_check, type_node->fields_to_check.size);
        spot = kan_dynamic_array_add_last (&type_node->fields_to_check);
    }

    *(struct kan_resource_pipeline_reference_field_info_t *) spot =
        (struct kan_resource_pipeline_reference_field_info_t) {
            .field = field,
            .is_leaf_field = meta ? KAN_TRUE : KAN_FALSE,
            .type = meta ? kan_string_intern (meta->type) : NULL,
            .compilation_usage =
                meta ? meta->compilation_usage : KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
        };
}

static inline void kan_resource_pipeline_type_info_node_add_referencer (
    struct kan_resource_pipeline_reference_type_info_node_t *referenced_type_node,
    kan_interned_string_t referencer_name)
{
    void *spot = kan_dynamic_array_add_last (&referenced_type_node->referencer_types);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (&referenced_type_node->referencer_types,
                                        referenced_type_node->referencer_types.size * 2u);
        spot = kan_dynamic_array_add_last (&referenced_type_node->referencer_types);
    }

    *(kan_interned_string_t *) spot = referencer_name;
}

static struct kan_resource_pipeline_reference_type_info_node_t *
kan_resource_pipeline_type_info_storage_get_or_create_node (
    struct kan_resource_pipeline_reference_type_info_storage_t *storage,
    kan_reflection_registry_t registry,
    const struct kan_reflection_struct_t *root_struct_data,
    const struct kan_reflection_struct_t *struct_data,
    kan_interned_string_t interned_kan_resource_pipeline_resource_type_meta_t,
    kan_interned_string_t interned_kan_resource_reference_pipeline_meta_t)
{
    struct kan_resource_pipeline_reference_type_info_node_t *type_node =
        kan_resource_pipeline_type_info_storage_query_type_node (storage, struct_data->name);

    if (type_node)
    {
        return type_node;
    }

    type_node = kan_allocate_batched (storage->scanned_allocation_group,
                                      sizeof (struct kan_resource_pipeline_reference_type_info_node_t));
    type_node->node.hash = (uint64_t) struct_data->name;
    type_node->type_name = struct_data->name;

    kan_dynamic_array_init (&type_node->fields_to_check, KAN_RESOURCE_PIPELINE_SCAN_ARRAY_INITIAL_SIZE,
                            sizeof (struct kan_resource_pipeline_reference_field_info_t),
                            _Alignof (struct kan_resource_pipeline_reference_field_info_t),
                            storage->scanned_allocation_group);

    kan_dynamic_array_init (&type_node->referencer_types, KAN_RESOURCE_PIPELINE_SCAN_ARRAY_INITIAL_SIZE,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            storage->scanned_allocation_group);

    // Add right away to correctly process cycles if they appear.
    kan_hash_storage_update_bucket_count_default (&storage->scanned_types, KAN_RESOURCE_PIPELINE_SCAN_BUCKETS);
    kan_hash_storage_add (&storage->scanned_types, &type_node->node);

    const kan_bool_t root_is_resource_type =
        is_resource_type (registry, root_struct_data, interned_kan_resource_pipeline_resource_type_meta_t);
    type_node->is_resource_type = root_struct_data == struct_data && root_is_resource_type;
    type_node->contains_patches = KAN_FALSE;

    for (uint64_t field_index = 0u; field_index < struct_data->fields_count; ++field_index)
    {
        const struct kan_reflection_field_t *field_data = &struct_data->fields[field_index];
        kan_bool_t check_is_reference_field = KAN_FALSE;
        kan_interned_string_t internal_type_name_to_check_is_parent_of_references = NULL;

        switch (field_data->archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            // We're not interested in field types above.
            break;

        case KAN_REFLECTION_ARCHETYPE_PATCH:
            // Patches can contain anything.
            kan_resource_pipeline_type_info_node_add_field (type_node, field_data, NULL);
            type_node->contains_patches = KAN_TRUE;
            break;

        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            check_is_reference_field = KAN_TRUE;
            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
            internal_type_name_to_check_is_parent_of_references = field_data->archetype_struct.type_name;
            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            switch (field_data->archetype_inline_array.item_archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_FLOATING:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                // We're not interested in field types above.
                break;

            case KAN_REFLECTION_ARCHETYPE_PATCH:
                // Patches can contain anything.
                kan_resource_pipeline_type_info_node_add_field (type_node, field_data, NULL);
                type_node->contains_patches = KAN_TRUE;
                break;

            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                check_is_reference_field = KAN_TRUE;
                break;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                internal_type_name_to_check_is_parent_of_references =
                    field_data->archetype_inline_array.item_archetype_struct.type_name;
                break;

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                KAN_ASSERT (KAN_FALSE)
                break;
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            switch (field_data->archetype_dynamic_array.item_archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_FLOATING:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                // We're not interested in field types above.
                break;

            case KAN_REFLECTION_ARCHETYPE_PATCH:
                // Patches can contain anything.
                kan_resource_pipeline_type_info_node_add_field (type_node, field_data, NULL);
                type_node->contains_patches = KAN_TRUE;
                break;

            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                check_is_reference_field = KAN_TRUE;
                break;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                internal_type_name_to_check_is_parent_of_references =
                    field_data->archetype_dynamic_array.item_archetype_struct.type_name;
                break;

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                KAN_ASSERT (KAN_FALSE)
                break;
            }

            break;
        }

        if (check_is_reference_field)
        {
            struct kan_reflection_struct_field_meta_iterator_t meta_iterator =
                kan_reflection_registry_query_struct_field_meta (registry, struct_data->name, field_data->name,
                                                                 interned_kan_resource_reference_pipeline_meta_t);

            const struct kan_resource_pipeline_reference_meta_t *meta =
                kan_reflection_struct_field_meta_iterator_get (&meta_iterator);

            if (meta)
            {
                kan_resource_pipeline_type_info_node_add_field (type_node, field_data, meta);

                // Add to references only if resource type.
                // Otherwise, root struct is possibly a patch-only data and will add references through resources with
                // patches.
                if (root_is_resource_type)
                {
                    const struct kan_reflection_struct_t *referenced_type =
                        kan_reflection_registry_query_struct (registry, kan_string_intern (meta->type));

                    if (referenced_type && is_resource_type (registry, referenced_type,
                                                             interned_kan_resource_pipeline_resource_type_meta_t))
                    {
                        struct kan_resource_pipeline_reference_type_info_node_t *referenced_type_node =
                            kan_resource_pipeline_type_info_storage_get_or_create_node (
                                storage, registry, referenced_type, referenced_type,
                                interned_kan_resource_pipeline_resource_type_meta_t,
                                interned_kan_resource_reference_pipeline_meta_t);

                        kan_resource_pipeline_type_info_node_add_referencer (referenced_type_node,
                                                                             root_struct_data->name);
                    }
                    else
                    {
                        KAN_LOG (
                            resource_reference, KAN_LOG_ERROR,
                            "Field \"%s\" of type \"%s\" is marked as resource reference, but specified type \"%s\" "
                            "is not a resource type.",
                            field_data->name, struct_data->name, meta->type)
                    }
                }
            }
        }
        else if (internal_type_name_to_check_is_parent_of_references)
        {
            const struct kan_reflection_struct_t *child_type =
                kan_reflection_registry_query_struct (registry, internal_type_name_to_check_is_parent_of_references);
            KAN_ASSERT (child_type)

            struct kan_resource_pipeline_reference_type_info_node_t *child_type_node =
                kan_resource_pipeline_type_info_storage_get_or_create_node (
                    storage, registry, root_struct_data, child_type,
                    interned_kan_resource_pipeline_resource_type_meta_t,
                    interned_kan_resource_reference_pipeline_meta_t);

            if (child_type_node->fields_to_check.size > 0u)
            {
                kan_resource_pipeline_type_info_node_add_field (type_node, field_data, NULL);
                type_node->contains_patches |= child_type_node->contains_patches;
            }
        }
    }

    return type_node;
}

static inline void kan_resource_pipeline_type_info_node_free (
    struct kan_resource_pipeline_reference_type_info_storage_t *storage,
    struct kan_resource_pipeline_reference_type_info_node_t *type_node)
{
    kan_dynamic_array_shutdown (&type_node->fields_to_check);
    kan_dynamic_array_shutdown (&type_node->referencer_types);
    kan_free_batched (storage->scanned_allocation_group, type_node);
}

static inline void kan_resource_pipeline_type_info_storage_scan (
    struct kan_resource_pipeline_reference_type_info_storage_t *storage, kan_reflection_registry_t registry)
{
    kan_reflection_registry_struct_iterator_t iterator = kan_reflection_registry_struct_iterator_create (registry);
    const kan_interned_string_t interned_kan_resource_pipeline_resource_type_meta_t =
        kan_string_intern ("kan_resource_pipeline_resource_type_meta_t");
    const kan_interned_string_t interned_kan_resource_pipeline_reference_meta_t =
        kan_string_intern ("kan_resource_pipeline_reference_meta_t");
    const struct kan_reflection_struct_t *struct_data;

    // Prepare data.
    while ((struct_data = kan_reflection_registry_struct_iterator_get (iterator)))
    {
        kan_resource_pipeline_type_info_storage_get_or_create_node (storage, registry, struct_data, struct_data,
                                                                    interned_kan_resource_pipeline_resource_type_meta_t,
                                                                    interned_kan_resource_pipeline_reference_meta_t);
        iterator = kan_reflection_registry_struct_iterator_next (iterator);
    }

    // Make patchers references to all resource types.
    struct kan_resource_pipeline_reference_type_info_node_t *type_node =
        (struct kan_resource_pipeline_reference_type_info_node_t *) storage->scanned_types.items.first;

    while (type_node)
    {
        if (type_node->is_resource_type && type_node->contains_patches)
        {
            struct kan_resource_pipeline_reference_type_info_node_t *other_type_node =
                (struct kan_resource_pipeline_reference_type_info_node_t *) storage->scanned_types.items.first;

            while (other_type_node)
            {
                if (other_type_node->is_resource_type)
                {
                    kan_resource_pipeline_type_info_node_add_referencer (other_type_node, type_node->type_name);
                }

                other_type_node =
                    (struct kan_resource_pipeline_reference_type_info_node_t *) other_type_node->node.list_node.next;
            }
        }

        type_node = (struct kan_resource_pipeline_reference_type_info_node_t *) type_node->node.list_node.next;
    }

    // Cleanup excessive types from storage.
    type_node = (struct kan_resource_pipeline_reference_type_info_node_t *) storage->scanned_types.items.first;

    while (type_node)
    {
        struct kan_resource_pipeline_reference_type_info_node_t *next =
            (struct kan_resource_pipeline_reference_type_info_node_t *) type_node->node.list_node.next;

        if (type_node->fields_to_check.size == 0u && type_node->referencer_types.size == 0u)
        {
            kan_hash_storage_remove (&storage->scanned_types, &type_node->node);
            kan_resource_pipeline_type_info_node_free (storage, type_node);
        }

        kan_dynamic_array_set_capacity (&type_node->fields_to_check, type_node->fields_to_check.size);
        kan_dynamic_array_set_capacity (&type_node->referencer_types, type_node->referencer_types.size);
        type_node = next;
    }

    kan_hash_storage_update_bucket_count_default (&storage->scanned_types, KAN_RESOURCE_PIPELINE_SCAN_BUCKETS);
}

void kan_resource_pipeline_reference_type_info_storage_build (
    struct kan_resource_pipeline_reference_type_info_storage_t *storage,
    kan_allocation_group_t allocation_group,
    kan_reflection_registry_t registry)
{
    storage->scanned_allocation_group = allocation_group;
    kan_hash_storage_init (&storage->scanned_types, storage->scanned_allocation_group,
                           KAN_RESOURCE_PIPELINE_SCAN_BUCKETS);
    kan_resource_pipeline_type_info_storage_scan (storage, registry);
}

const struct kan_resource_pipeline_reference_type_info_node_t *kan_resource_pipeline_reference_type_info_storage_query (
    struct kan_resource_pipeline_reference_type_info_storage_t *storage, kan_interned_string_t type_name)
{
    return kan_resource_pipeline_type_info_storage_query_type_node (storage, type_name);
}

void kan_resource_pipeline_reference_type_info_storage_shutdown (
    struct kan_resource_pipeline_reference_type_info_storage_t *storage)
{
    struct kan_resource_pipeline_reference_type_info_node_t *type_node =
        (struct kan_resource_pipeline_reference_type_info_node_t *) storage->scanned_types.items.first;

    while (type_node)
    {
        struct kan_resource_pipeline_reference_type_info_node_t *next =
            (struct kan_resource_pipeline_reference_type_info_node_t *) type_node->node.list_node.next;

        kan_resource_pipeline_type_info_node_free (storage, type_node);
        type_node = next;
    }

    kan_hash_storage_shutdown (&storage->scanned_types);
}

void kan_resource_pipeline_detected_reference_container_init (
    struct kan_resource_pipeline_detected_reference_container_t *instance)
{
    ensure_statics_initialized ();
    kan_dynamic_array_init (&instance->detected_references, KAN_RESOURCE_PIPELINE_DETECTED_ARRAY_INITIAL_SIZE,
                            sizeof (struct kan_resource_pipeline_detected_reference_t),
                            _Alignof (struct kan_resource_pipeline_detected_reference_t),
                            detected_references_container_allocation_group);
}

void kan_resource_pipeline_detected_reference_container_shutdown (
    struct kan_resource_pipeline_detected_reference_container_t *instance)
{
    kan_dynamic_array_shutdown (&instance->detected_references);
}

static inline void kan_resource_pipeline_detected_container_add_reference (
    struct kan_resource_pipeline_detected_reference_container_t *container,
    kan_interned_string_t type,
    kan_interned_string_t name,
    enum kan_resource_pipeline_compilation_usage_type_t compilation_usage)
{
    if (!name)
    {
        return;
    }

    struct kan_resource_pipeline_detected_reference_t *spot =
        kan_dynamic_array_add_last (&container->detected_references);

    if (!spot)
    {
        kan_dynamic_array_set_capacity (&container->detected_references, container->detected_references.capacity * 2u);
        spot = kan_dynamic_array_add_last (&container->detected_references);
        KAN_ASSERT (spot)
    }

    spot->type = type;
    spot->name = name;
    spot->compilation_usage = compilation_usage;
}

static inline uint64_t extract_inline_array_size (const void *struct_data,
                                                  const struct kan_reflection_field_t *array_field)
{
    if (array_field->archetype_inline_array.size_field)
    {
        const struct kan_reflection_field_t *size_field = array_field->archetype_inline_array.size_field;
        const void *size_field_address = ((const uint8_t *) struct_data) + size_field->offset;

        switch (size_field->archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            switch (size_field->size)
            {
            case 1u:
                return *(int8_t *) size_field_address;
            case 2u:
                return *(int16_t *) size_field_address;
            case 4u:
                return *(int32_t *) size_field_address;
            case 8u:
                return *(int64_t *) size_field_address;
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            switch (size_field->size)
            {
            case 1u:
                return *(uint8_t *) size_field_address;
            case 2u:
                return *(uint16_t *) size_field_address;
            case 4u:
                return *(uint32_t *) size_field_address;
            case 8u:
                return *(uint64_t *) size_field_address;
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        case KAN_REFLECTION_ARCHETYPE_STRUCT:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_PATCH:
            KAN_ASSERT (KAN_FALSE)
            break;
        }
    }

    return array_field->archetype_inline_array.item_count;
}

static void kan_resource_pipeline_detect_inside_patch_part (
    struct kan_resource_pipeline_reference_type_info_storage_t *storage,
    kan_reflection_patch_t patch,
    uint64_t part_offset,
    kan_interned_string_t part_type_name,
    kan_reflection_patch_iterator_t search_since_iterator,
    struct kan_resource_pipeline_detected_reference_container_t *output_container)
{
    if (search_since_iterator == kan_reflection_patch_end (patch))
    {
        // Nothing in patch.
        return;
    }

    struct kan_resource_pipeline_reference_type_info_node_t *type_node =
        kan_resource_pipeline_type_info_storage_query_type_node (storage, part_type_name);

    if (!type_node)
    {
        // Does not reference anything.
        return;
    }

    for (uint64_t field_index = 0u; field_index < type_node->fields_to_check.size; ++field_index)
    {
        struct kan_resource_pipeline_reference_field_info_t *field_info =
            &((struct kan_resource_pipeline_reference_field_info_t *) type_node->fields_to_check.data)[field_index];

        if (field_info->field->visibility_condition_field)
        {
            KAN_LOG (resource_reference, KAN_LOG_ERROR,
                     "Field \"%s\" of type \"%s\" is marked as resource reference and found inside patch, but it has "
                     "visibility condition and patches do not fully support it!",
                     field_info->field->name, part_type_name)
            continue;
        }

        uint64_t field_offset = part_offset + field_info->field->offset;
        kan_reflection_patch_iterator_t field_search_iterator = search_since_iterator;
        struct kan_reflection_patch_chunk_info_t chunk_info = kan_reflection_patch_iterator_get (field_search_iterator);

        while (field_offset + field_info->field->size < chunk_info.offset ||
               field_offset >= chunk_info.offset + chunk_info.size)
        {
            field_search_iterator = kan_reflection_patch_iterator_next (field_search_iterator);
            chunk_info = kan_reflection_patch_iterator_get (field_search_iterator);

            if (field_search_iterator == kan_reflection_patch_end (patch))
            {
                break;
            }
        }

        if (field_search_iterator == kan_reflection_patch_end (patch))
        {
            // Field does not present in patch even partially.
            continue;
        }

        switch (field_info->field->archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_PATCH:
            KAN_ASSERT (KAN_FALSE)
            break;

        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        {
            KAN_ASSERT (field_offset >= chunk_info.offset &&
                        field_offset + sizeof (kan_interned_string_t) <= chunk_info.offset + chunk_info.size)
            const uint64_t offset_in_chunk = field_offset - chunk_info.offset;
            const void *address = (const uint8_t *) chunk_info.data + offset_in_chunk;

            kan_resource_pipeline_detected_container_add_reference (
                output_container, field_info->type, *(kan_interned_string_t *) address, field_info->compilation_usage);
            break;
        }

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
            kan_resource_pipeline_detect_inside_patch_part (storage, patch, field_offset,
                                                            field_info->field->archetype_struct.type_name,
                                                            field_search_iterator, output_container);
            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        {
            // Inline arrays in patches are always treated as full.
            const uint64_t size = field_info->field->archetype_inline_array.item_count;

            switch (field_info->field->archetype_inline_array.item_archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_FLOATING:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                KAN_ASSERT (KAN_FALSE)
                break;

            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            {
                for (uint64_t index = 0u; index < size; ++index)
                {
                    const uint64_t item_offset =
                        field_offset + index * field_info->field->archetype_inline_array.item_size;

                    while (item_offset >= chunk_info.offset + chunk_info.size)
                    {
                        field_search_iterator = kan_reflection_patch_iterator_next (field_search_iterator);
                        if (field_search_iterator == kan_reflection_patch_end (patch))
                        {
                            break;
                        }

                        chunk_info = kan_reflection_patch_iterator_get (field_search_iterator);
                    }

                    if (field_search_iterator == kan_reflection_patch_end (patch))
                    {
                        break;
                    }

                    if (item_offset < chunk_info.offset)
                    {
                        continue;
                    }

                    KAN_ASSERT (item_offset + sizeof (kan_interned_string_t) <= chunk_info.offset + chunk_info.size)
                    const uint64_t offset_in_chunk = item_offset - chunk_info.offset;
                    const void *address = (const uint8_t *) chunk_info.data + offset_in_chunk;

                    kan_resource_pipeline_detected_container_add_reference (output_container, field_info->type,
                                                                            *(kan_interned_string_t *) address,
                                                                            field_info->compilation_usage);
                }

                break;
            }

            case KAN_REFLECTION_ARCHETYPE_PATCH:
                for (uint64_t index = 0u; index < size; ++index)
                {
                    const uint64_t item_offset =
                        field_offset + index * field_info->field->archetype_inline_array.item_size;

                    kan_resource_pipeline_detect_inside_patch_part (
                        storage, patch, item_offset,
                        field_info->field->archetype_inline_array.item_archetype_struct.type_name,
                        field_search_iterator, output_container);
                }
                break;
            }

            break;
        }
        }
    }
}

static inline void kan_resource_pipeline_detect_inside_patch (
    struct kan_resource_pipeline_reference_type_info_storage_t *storage,
    kan_reflection_patch_t patch,
    struct kan_resource_pipeline_detected_reference_container_t *output_container)
{
    kan_resource_pipeline_detect_inside_patch_part (storage, patch, 0u, kan_reflection_patch_get_type (patch)->name,
                                                    kan_reflection_patch_begin (patch), output_container);
}

void kan_resource_pipeline_detect_references (
    struct kan_resource_pipeline_reference_type_info_storage_t *storage,
    kan_interned_string_t referencer_type_name,
    const void *referencer_data,
    struct kan_resource_pipeline_detected_reference_container_t *output_container)
{
    struct kan_resource_pipeline_reference_type_info_node_t *type_node =
        kan_resource_pipeline_type_info_storage_query_type_node (storage, referencer_type_name);
    if (!type_node)
    {
        // Does not reference anything.
        return;
    }

    for (uint64_t field_index = 0u; field_index < type_node->fields_to_check.size; ++field_index)
    {
        struct kan_resource_pipeline_reference_field_info_t *field_info =
            &((struct kan_resource_pipeline_reference_field_info_t *) type_node->fields_to_check.data)[field_index];
        const void *field_address = ((const uint8_t *) referencer_data) + field_info->field->offset;

        if (!kan_reflection_check_visibility (
                field_info->field->visibility_condition_field, field_info->field->visibility_condition_values_count,
                field_info->field->visibility_condition_values,
                ((uint8_t *) referencer_data) + (field_info->field->visibility_condition_field ?
                                                     field_info->field->visibility_condition_field->offset :
                                                     0u)))
        {
            // Skip invisible field.
            continue;
        }

        if (field_info->is_leaf_field)
        {
            switch (field_info->field->archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_FLOATING:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT:
            case KAN_REFLECTION_ARCHETYPE_PATCH:
                KAN_ASSERT (KAN_FALSE)
                break;

            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                kan_resource_pipeline_detected_container_add_reference (output_container, field_info->type,
                                                                        *(kan_interned_string_t *) field_address,
                                                                        field_info->compilation_usage);
                break;

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            {
                const uint64_t size = extract_inline_array_size (referencer_data, field_info->field);
                for (uint64_t index = 0u; index < size; ++index)
                {
                    kan_resource_pipeline_detected_container_add_reference (
                        output_container, field_info->type, ((kan_interned_string_t *) field_address)[index],
                        field_info->compilation_usage);
                }

                break;
            }

            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            {
                KAN_ASSERT (field_info->field->archetype_dynamic_array.item_archetype ==
                            KAN_REFLECTION_ARCHETYPE_INTERNED_STRING)
                const struct kan_dynamic_array_t *array = field_address;

                for (uint64_t index = 0u; index < array->size; ++index)
                {
                    kan_resource_pipeline_detected_container_add_reference (
                        output_container, field_info->type, ((kan_interned_string_t *) array->data)[index],
                        field_info->compilation_usage);
                }

                break;
            }
            }
        }
        else
        {
            switch (field_info->field->archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_FLOATING:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                KAN_ASSERT (KAN_FALSE)
                break;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                kan_resource_pipeline_detect_references (storage, field_info->field->archetype_struct.type_name,
                                                         field_address, output_container);
                break;

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            {
                const uint64_t size = extract_inline_array_size (referencer_data, field_info->field);
                switch (field_info->field->archetype_inline_array.item_archetype)
                {
                case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                case KAN_REFLECTION_ARCHETYPE_FLOATING:
                case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
                case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                case KAN_REFLECTION_ARCHETYPE_ENUM:
                case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                    KAN_ASSERT (KAN_FALSE)
                    break;

                case KAN_REFLECTION_ARCHETYPE_STRUCT:
                    for (uint64_t index = 0u; index < size; ++index)
                    {
                        kan_resource_pipeline_detect_references (
                            storage, field_info->field->archetype_inline_array.item_archetype_struct.type_name,
                            ((uint8_t *) field_address) + index * field_info->field->archetype_inline_array.item_size,
                            output_container);
                    }

                    break;

                case KAN_REFLECTION_ARCHETYPE_PATCH:
                    for (uint64_t index = 0u; index < size; ++index)
                    {
                        kan_resource_pipeline_detect_inside_patch (
                            storage, ((kan_reflection_patch_t *) field_address)[index], output_container);
                    }

                    break;
                }

                break;
            }

            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            {
                const struct kan_dynamic_array_t *array = field_address;
                switch (field_info->field->archetype_dynamic_array.item_archetype)
                {
                case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                case KAN_REFLECTION_ARCHETYPE_FLOATING:
                case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
                case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                case KAN_REFLECTION_ARCHETYPE_ENUM:
                case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                    KAN_ASSERT (KAN_FALSE)
                    break;

                case KAN_REFLECTION_ARCHETYPE_STRUCT:
                    for (uint64_t index = 0u; index < array->size; ++index)
                    {
                        kan_resource_pipeline_detect_references (
                            storage, field_info->field->archetype_dynamic_array.item_archetype_struct.type_name,
                            array->data + index * array->item_size, output_container);
                    }

                    break;

                case KAN_REFLECTION_ARCHETYPE_PATCH:
                    for (uint64_t index = 0u; index < array->size; ++index)
                    {
                        kan_resource_pipeline_detect_inside_patch (
                            storage, ((kan_reflection_patch_t *) array->data)[index], output_container);
                    }

                    break;
                }

                break;
            }

            case KAN_REFLECTION_ARCHETYPE_PATCH:
                kan_resource_pipeline_detect_inside_patch (storage, *(kan_reflection_patch_t *) field_address,
                                                           output_container);
                break;
            }
        }
    }
}
