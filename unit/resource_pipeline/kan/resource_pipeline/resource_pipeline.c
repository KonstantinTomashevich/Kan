#include <stddef.h>
#include <string.h>

#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/reflection/field_visibility_iterator.h>
#include <kan/reflection/patch.h>
#include <kan/reflection/struct_helpers.h>
#include <kan/resource_pipeline/resource_pipeline.h>

KAN_LOG_DEFINE_CATEGORY (resource_reference);

static kan_allocation_group_t detected_references_container_allocation_group;
static kan_allocation_group_t platform_configuration_allocation_group;
static kan_allocation_group_t resource_import_rule_allocation_group;
static kan_interned_string_t interned_kan_resource_resource_type_meta_t;
static kan_interned_string_t interned_kan_resource_byproduct_type_meta_t;
static kan_interned_string_t interned_kan_resource_reference_meta_t;
static bool statics_initialized = false;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        detected_references_container_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "kan_resource_detected_container");
        platform_configuration_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "kan_resource_platform_configuration_t");
        resource_import_rule_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "kan_resource_import_rule_t");
        interned_kan_resource_resource_type_meta_t = kan_string_intern ("kan_resource_resource_type_meta_t");
        interned_kan_resource_byproduct_type_meta_t = kan_string_intern ("kan_resource_byproduct_type_meta_t");
        interned_kan_resource_reference_meta_t = kan_string_intern ("kan_resource_reference_meta_t");
        statics_initialized = true;
    }
}

void kan_resource_platform_configuration_init (struct kan_resource_platform_configuration_t *instance)
{
    ensure_statics_initialized ();
    instance->parent = NULL;
    kan_dynamic_array_init (&instance->configuration, 0u, sizeof (kan_reflection_patch_t),
                            _Alignof (kan_reflection_patch_t), platform_configuration_allocation_group);
}

void kan_resource_platform_configuration_shutdown (struct kan_resource_platform_configuration_t *instance)
{
    for (kan_loop_size_t index = 0u; index < instance->configuration.size; ++index)
    {
        kan_reflection_patch_destroy (((kan_reflection_patch_t *) instance->configuration.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->configuration);
}

static inline struct kan_resource_reference_type_info_node_t *kan_resource_type_info_storage_query_type_node (
    struct kan_resource_reference_type_info_storage_t *storage, kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&storage->scanned_types, KAN_HASH_OBJECT_POINTER (type_name));
    struct kan_resource_reference_type_info_node_t *node =
        (struct kan_resource_reference_type_info_node_t *) bucket->first;
    const struct kan_resource_reference_type_info_node_t *node_end =
        (struct kan_resource_reference_type_info_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->type_name == type_name)
        {
            return node;
        }

        node = (struct kan_resource_reference_type_info_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static inline bool is_resource_or_byproduct_type (kan_reflection_registry_t registry,
                                                  const struct kan_reflection_struct_t *struct_data)

{
    struct kan_reflection_struct_meta_iterator_t meta_iterator = kan_reflection_registry_query_struct_meta (
        registry, struct_data->name, interned_kan_resource_resource_type_meta_t);

    if (kan_reflection_struct_meta_iterator_get (&meta_iterator))
    {
        return true;
    }

    meta_iterator = kan_reflection_registry_query_struct_meta (registry, struct_data->name,
                                                               interned_kan_resource_byproduct_type_meta_t);

    if (kan_reflection_struct_meta_iterator_get (&meta_iterator))
    {
        return true;
    }

    return false;
}

static inline void kan_resource_type_info_node_add_field (struct kan_resource_reference_type_info_node_t *type_node,
                                                          const struct kan_reflection_field_t *field,
                                                          const struct kan_resource_reference_meta_t *meta)
{
    void *spot = kan_dynamic_array_add_last (&type_node->fields_to_check);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (&type_node->fields_to_check, type_node->fields_to_check.size * 2u);
        spot = kan_dynamic_array_add_last (&type_node->fields_to_check);
    }

    KAN_ASSERT (spot)
    *(struct kan_resource_reference_field_info_t *) spot = (struct kan_resource_reference_field_info_t) {
        .field = field,
        .is_leaf_field = meta ? true : false,
        .type = meta ? kan_string_intern (meta->type) : NULL,
        .compilation_usage = meta ? meta->compilation_usage : KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
    };
}

static inline void add_unique_reference_name (struct kan_dynamic_array_t *target_array,
                                              kan_interned_string_t referencer_name)
{
    for (kan_loop_size_t index = 0u; index < target_array->size; ++index)
    {
        if (referencer_name == ((kan_interned_string_t *) target_array->data)[index])
        {
            return;
        }
    }

    void *spot = kan_dynamic_array_add_last (target_array);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (target_array, target_array->size * 2u);
        spot = kan_dynamic_array_add_last (target_array);
    }

    *(kan_interned_string_t *) spot = referencer_name;
}

static struct kan_resource_reference_type_info_node_t *kan_resource_type_info_storage_get_or_create_node (
    struct kan_resource_reference_type_info_storage_t *storage,
    kan_reflection_registry_t registry,
    const struct kan_reflection_struct_t *root_struct_data,
    const struct kan_reflection_struct_t *struct_data)
{
    struct kan_resource_reference_type_info_node_t *type_node =
        kan_resource_type_info_storage_query_type_node (storage, struct_data->name);

    if (type_node)
    {
        return type_node;
    }

    type_node = kan_allocate_batched (storage->scanned_allocation_group,
                                      sizeof (struct kan_resource_reference_type_info_node_t));
    type_node->node.hash = KAN_HASH_OBJECT_POINTER (struct_data->name);
    type_node->type_name = struct_data->name;

    kan_dynamic_array_init (&type_node->fields_to_check, KAN_RESOURCE_PIPELINE_SCAN_ARRAY_INITIAL_SIZE,
                            sizeof (struct kan_resource_reference_field_info_t),
                            _Alignof (struct kan_resource_reference_field_info_t), storage->scanned_allocation_group);

    kan_dynamic_array_init (&type_node->referencer_types, KAN_RESOURCE_PIPELINE_SCAN_ARRAY_INITIAL_SIZE,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            storage->scanned_allocation_group);

    // Add right away to correctly process cycles if they appear.
    kan_hash_storage_update_bucket_count_default (&storage->scanned_types, KAN_RESOURCE_PIPELINE_SCAN_BUCKETS);
    kan_hash_storage_add (&storage->scanned_types, &type_node->node);

    const bool root_is_resource_type = is_resource_or_byproduct_type (registry, root_struct_data);
    type_node->is_resource_type = root_struct_data == struct_data && root_is_resource_type;
    type_node->contains_patches = false;

    for (kan_loop_size_t field_index = 0u; field_index < struct_data->fields_count; ++field_index)
    {
        const struct kan_reflection_field_t *field_data = &struct_data->fields[field_index];
        bool check_is_reference_field = false;
        kan_interned_string_t internal_type_name_to_check_is_parent_of_references = NULL;

        switch (field_data->archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            // We're not interested in field types above.
            break;

        case KAN_REFLECTION_ARCHETYPE_PATCH:
            // Patches can contain anything.
            kan_resource_type_info_node_add_field (type_node, field_data, NULL);
            type_node->contains_patches = true;
            break;

        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            check_is_reference_field = true;
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
            case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                // We're not interested in field types above.
                break;

            case KAN_REFLECTION_ARCHETYPE_PATCH:
                // Patches can contain anything.
                kan_resource_type_info_node_add_field (type_node, field_data, NULL);
                type_node->contains_patches = true;
                break;

            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                check_is_reference_field = true;
                break;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                internal_type_name_to_check_is_parent_of_references =
                    field_data->archetype_inline_array.item_archetype_struct.type_name;
                break;

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                KAN_ASSERT (false)
                break;
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            switch (field_data->archetype_dynamic_array.item_archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_FLOATING:
            case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                // We're not interested in field types above.
                break;

            case KAN_REFLECTION_ARCHETYPE_PATCH:
                // Patches can contain anything.
                kan_resource_type_info_node_add_field (type_node, field_data, NULL);
                type_node->contains_patches = true;
                break;

            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                check_is_reference_field = true;
                break;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                internal_type_name_to_check_is_parent_of_references =
                    field_data->archetype_dynamic_array.item_archetype_struct.type_name;
                break;

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                KAN_ASSERT (false)
                break;
            }

            break;
        }

        if (check_is_reference_field)
        {
            struct kan_reflection_struct_field_meta_iterator_t meta_iterator =
                kan_reflection_registry_query_struct_field_meta (registry, struct_data->name, field_data->name,
                                                                 interned_kan_resource_reference_meta_t);

            const struct kan_resource_reference_meta_t *meta =
                kan_reflection_struct_field_meta_iterator_get (&meta_iterator);

            if (meta)
            {
                kan_resource_type_info_node_add_field (type_node, field_data, meta);

                // Add to references only if resource type.
                // Otherwise, root struct is possibly a patch-only data and will add references through resources with
                // patches.
                if (root_is_resource_type)
                {
                    if (meta->type)
                    {
                        const struct kan_reflection_struct_t *referenced_type =
                            kan_reflection_registry_query_struct (registry, kan_string_intern (meta->type));

                        if (referenced_type && is_resource_or_byproduct_type (registry, referenced_type))
                        {
                            struct kan_resource_reference_type_info_node_t *referenced_type_node =
                                kan_resource_type_info_storage_get_or_create_node (storage, registry, referenced_type,
                                                                                   referenced_type);

                            add_unique_reference_name (&referenced_type_node->referencer_types, root_struct_data->name);
                        }
                        else
                        {
                            KAN_LOG (resource_reference, KAN_LOG_ERROR,
                                     "Field \"%s\" of type \"%s\" is marked as resource reference, but specified type "
                                     "\"%s\" "
                                     "is not a resource type.",
                                     field_data->name, struct_data->name, meta->type)
                        }
                    }
                    else
                    {
                        add_unique_reference_name (&storage->third_party_referencers, root_struct_data->name);
                    }
                }
            }
        }
        else if (internal_type_name_to_check_is_parent_of_references)
        {
            const struct kan_reflection_struct_t *child_type =
                kan_reflection_registry_query_struct (registry, internal_type_name_to_check_is_parent_of_references);
            KAN_ASSERT (child_type)

            struct kan_resource_reference_type_info_node_t *child_type_node =
                kan_resource_type_info_storage_get_or_create_node (storage, registry, root_struct_data, child_type);

            if (child_type_node->fields_to_check.size > 0u)
            {
                kan_resource_type_info_node_add_field (type_node, field_data, NULL);
                type_node->contains_patches |= child_type_node->contains_patches;
            }
        }
    }

    return type_node;
}

static inline void kan_resource_type_info_node_free (struct kan_resource_reference_type_info_storage_t *storage,
                                                     struct kan_resource_reference_type_info_node_t *type_node)
{
    kan_dynamic_array_shutdown (&type_node->fields_to_check);
    kan_dynamic_array_shutdown (&type_node->referencer_types);
    kan_free_batched (storage->scanned_allocation_group, type_node);
}

static inline void kan_resource_type_info_storage_scan (struct kan_resource_reference_type_info_storage_t *storage,
                                                        kan_reflection_registry_t registry)
{
    kan_reflection_registry_struct_iterator_t iterator = kan_reflection_registry_struct_iterator_create (registry);
    const struct kan_reflection_struct_t *struct_data;

    // Start by scanning only resource types to avoid issues when resource types are scanned as part of records that
    // store them in runtime (for example resource provider containers).
    while ((struct_data = kan_reflection_registry_struct_iterator_get (iterator)))
    {
        if (is_resource_or_byproduct_type (registry, struct_data))
        {
            kan_resource_type_info_storage_get_or_create_node (storage, registry, struct_data, struct_data);
        }

        iterator = kan_reflection_registry_struct_iterator_next (iterator);
    }

    // Add everything else, otherwise we can miss on patchable types.
    iterator = kan_reflection_registry_struct_iterator_create (registry);

    while ((struct_data = kan_reflection_registry_struct_iterator_get (iterator)))
    {
        kan_resource_type_info_storage_get_or_create_node (storage, registry, struct_data, struct_data);
        iterator = kan_reflection_registry_struct_iterator_next (iterator);
    }

    // Make patchers references to all resource types including third party ones.
    struct kan_resource_reference_type_info_node_t *type_node =
        (struct kan_resource_reference_type_info_node_t *) storage->scanned_types.items.first;

    while (type_node)
    {
        if (type_node->is_resource_type && type_node->contains_patches)
        {
            add_unique_reference_name (&storage->third_party_referencers, type_node->type_name);
            struct kan_resource_reference_type_info_node_t *other_type_node =
                (struct kan_resource_reference_type_info_node_t *) storage->scanned_types.items.first;

            while (other_type_node)
            {
                if (other_type_node->is_resource_type)
                {
                    add_unique_reference_name (&other_type_node->referencer_types, type_node->type_name);
                }

                other_type_node =
                    (struct kan_resource_reference_type_info_node_t *) other_type_node->node.list_node.next;
            }
        }

        type_node = (struct kan_resource_reference_type_info_node_t *) type_node->node.list_node.next;
    }

    // Cleanup excessive types from storage.
    type_node = (struct kan_resource_reference_type_info_node_t *) storage->scanned_types.items.first;

    while (type_node)
    {
        struct kan_resource_reference_type_info_node_t *next =
            (struct kan_resource_reference_type_info_node_t *) type_node->node.list_node.next;

        if (type_node->fields_to_check.size == 0u && type_node->referencer_types.size == 0u)
        {
            kan_hash_storage_remove (&storage->scanned_types, &type_node->node);
            kan_resource_type_info_node_free (storage, type_node);
        }
        else
        {
            kan_dynamic_array_set_capacity (&type_node->fields_to_check, type_node->fields_to_check.size);
            kan_dynamic_array_set_capacity (&type_node->referencer_types, type_node->referencer_types.size);
        }

        type_node = next;
    }

    kan_hash_storage_update_bucket_count_default (&storage->scanned_types, KAN_RESOURCE_PIPELINE_SCAN_BUCKETS);
}

void kan_resource_reference_type_info_storage_build (struct kan_resource_reference_type_info_storage_t *storage,
                                                     kan_reflection_registry_t registry,
                                                     kan_allocation_group_t allocation_group)
{
    ensure_statics_initialized ();
    storage->registry = registry;
    storage->scanned_allocation_group = allocation_group;
    kan_hash_storage_init (&storage->scanned_types, storage->scanned_allocation_group,
                           KAN_RESOURCE_PIPELINE_SCAN_BUCKETS);
    kan_dynamic_array_init (&storage->third_party_referencers, KAN_RESOURCE_PIPELINE_SCAN_ARRAY_INITIAL_SIZE,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            storage->scanned_allocation_group);
    kan_resource_type_info_storage_scan (storage, registry);
}

const struct kan_resource_reference_type_info_node_t *kan_resource_reference_type_info_storage_query (
    struct kan_resource_reference_type_info_storage_t *storage, kan_interned_string_t type_name)
{
    return kan_resource_type_info_storage_query_type_node (storage, type_name);
}

void kan_resource_reference_type_info_storage_shutdown (struct kan_resource_reference_type_info_storage_t *storage)
{
    struct kan_resource_reference_type_info_node_t *type_node =
        (struct kan_resource_reference_type_info_node_t *) storage->scanned_types.items.first;

    while (type_node)
    {
        struct kan_resource_reference_type_info_node_t *next =
            (struct kan_resource_reference_type_info_node_t *) type_node->node.list_node.next;

        kan_resource_type_info_node_free (storage, type_node);
        type_node = next;
    }

    kan_hash_storage_shutdown (&storage->scanned_types);
    kan_dynamic_array_shutdown (&storage->third_party_referencers);
}

void kan_resource_detected_reference_container_init (struct kan_resource_detected_reference_container_t *instance)
{
    ensure_statics_initialized ();
    kan_dynamic_array_init (&instance->detected_references, KAN_RESOURCE_PIPELINE_DETECTED_ARRAY_INITIAL_SIZE,
                            sizeof (struct kan_resource_detected_reference_t),
                            _Alignof (struct kan_resource_detected_reference_t),
                            detected_references_container_allocation_group);
}

void kan_resource_detected_reference_container_shutdown (struct kan_resource_detected_reference_container_t *instance)
{
    kan_dynamic_array_shutdown (&instance->detected_references);
}

static inline void kan_resource_detected_container_add_reference (
    struct kan_resource_detected_reference_container_t *container,
    kan_interned_string_t type,
    kan_interned_string_t name,
    enum kan_resource_compilation_usage_type_t compilation_usage)
{
    if (!name)
    {
        return;
    }

    for (kan_loop_size_t index = 0u; index < container->detected_references.size; ++index)
    {
        struct kan_resource_detected_reference_t *reference =
            &((struct kan_resource_detected_reference_t *) container->detected_references.data)[index];

        if (reference->name == name && reference->type == type && reference->compilation_usage == compilation_usage)
        {
            return;
        }
    }

    struct kan_resource_detected_reference_t *spot = kan_dynamic_array_add_last (&container->detected_references);
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

static void kan_resource_detect_inside_data_chunk_for_struct_instance (
    struct kan_resource_reference_type_info_storage_t *storage,
    kan_instance_size_t part_offset,
    kan_interned_string_t part_type_name,
    struct kan_reflection_patch_chunk_info_t *chunk,
    struct kan_resource_detected_reference_container_t *output_container)
{
    struct kan_resource_reference_type_info_node_t *type_node =
        kan_resource_type_info_storage_query_type_node (storage, part_type_name);

    if (!type_node)
    {
        // Does not reference anything.
        return;
    }

    for (kan_loop_size_t field_index = 0u; field_index < type_node->fields_to_check.size; ++field_index)
    {
        struct kan_resource_reference_field_info_t *field_info =
            &((struct kan_resource_reference_field_info_t *) type_node->fields_to_check.data)[field_index];

        if (field_info->field->visibility_condition_field)
        {
            KAN_LOG (resource_reference, KAN_LOG_ERROR,
                     "Field \"%s\" of type \"%s\" is marked as resource reference and found inside patch, but it has "
                     "visibility condition and patches do not fully support it!",
                     field_info->field->name, part_type_name)
            continue;
        }

        kan_instance_size_t field_offset = part_offset + field_info->field->offset;
        if (field_offset + field_info->field->size <= chunk->offset || field_offset >= chunk->offset + chunk->size)
        {
            continue;
        }

        switch (field_info->field->archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_PATCH:
            KAN_ASSERT (false)
            break;

        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        {
            KAN_ASSERT (field_offset >= chunk->offset &&
                        field_offset + sizeof (kan_interned_string_t) <= chunk->offset + chunk->size)
            const kan_instance_size_t offset_in_chunk = field_offset - chunk->offset;
            const void *address = ((const uint8_t *) chunk->data) + offset_in_chunk;

            kan_resource_detected_container_add_reference (
                output_container, field_info->type, *(kan_interned_string_t *) address, field_info->compilation_usage);
            break;
        }

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
            kan_resource_detect_inside_data_chunk_for_struct_instance (
                storage, field_offset, field_info->field->archetype_struct.type_name, chunk, output_container);
            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        {
            // Inline arrays in patches are always treated as full.
            const kan_instance_size_t size = field_info->field->archetype_inline_array.item_count;
            kan_instance_size_t current_offset = field_offset;
            const kan_instance_size_t end_offset =
                KAN_MIN (chunk->offset + chunk->size, field_offset + field_info->field->size);

            switch (field_info->field->archetype_inline_array.item_archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_FLOATING:
            case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_PATCH:
                KAN_ASSERT (false)
                break;

            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            {
                while (current_offset < end_offset)
                {
                    const kan_instance_size_t offset_in_chunk = current_offset - chunk->offset;
                    const void *address = ((const uint8_t *) chunk->data) + offset_in_chunk;

                    kan_resource_detected_container_add_reference (output_container, field_info->type,
                                                                   *(kan_interned_string_t *) address,
                                                                   field_info->compilation_usage);
                    current_offset += size;
                }

                break;
            }

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                while (current_offset < end_offset)
                {
                    kan_resource_detect_inside_data_chunk_for_struct_instance (
                        storage, current_offset, field_info->field->archetype_struct.type_name, chunk,
                        output_container);

                    current_offset += size;
                }

                break;
            }

            break;
        }
        }
    }
}

KAN_REFLECTION_IGNORE
struct patch_section_stack_item_t
{
    kan_reflection_patch_serializable_section_id_t id;
    enum kan_reflection_patch_section_type_t type;
    const struct kan_reflection_struct_t *source_field_type;
    const struct kan_reflection_field_t *source_field;
};

KAN_REFLECTION_IGNORE
struct patch_section_stack_t
{
    struct patch_section_stack_item_t *stack_end;
    struct patch_section_stack_item_t stack[KAN_RESOURCE_PIPELINE_PATCH_SECTION_STACK_SIZE];
};

static inline void kan_resource_detect_inside_patch (
    struct kan_resource_reference_type_info_storage_t *storage,
    kan_reflection_patch_t patch,
    struct kan_resource_detected_reference_container_t *output_container)
{
    const struct kan_reflection_struct_t *patch_type = kan_reflection_patch_get_type (patch);
    if (!patch_type)
    {
        return;
    }

    struct patch_section_stack_t stack;
    stack.stack_end = stack.stack;

    kan_reflection_patch_iterator_t patch_iterator = kan_reflection_patch_begin (patch);
    const kan_reflection_patch_iterator_t patch_end = kan_reflection_patch_end (patch);

    while (!KAN_HANDLE_IS_EQUAL (patch_iterator, patch_end))
    {
        struct kan_reflection_patch_node_info_t node = kan_reflection_patch_iterator_get (patch_iterator);
        if (node.is_data_chunk)
        {
            if (stack.stack_end > stack.stack)
            {
                struct patch_section_stack_item_t *current_stack_item = stack.stack_end - 1u;
                enum kan_reflection_archetype_t item_archetype = KAN_REFLECTION_ARCHETYPE_STRUCT;
                kan_instance_size_t item_size = 0u;
                kan_interned_string_t item_type_name = NULL;

                switch (current_stack_item->type)
                {
                case KAN_REFLECTION_PATCH_SECTION_TYPE_DYNAMIC_ARRAY_SET:
                case KAN_REFLECTION_PATCH_SECTION_TYPE_DYNAMIC_ARRAY_APPEND:
                    KAN_ASSERT (current_stack_item->source_field->archetype == KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY)
                    item_archetype = current_stack_item->source_field->archetype_dynamic_array.item_archetype;
                    item_size = current_stack_item->source_field->archetype_dynamic_array.item_size;

                    switch (item_archetype)
                    {
                    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                    case KAN_REFLECTION_ARCHETYPE_FLOATING:
                    case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
                    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
                    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                    case KAN_REFLECTION_ARCHETYPE_PATCH:
                        break;

                    case KAN_REFLECTION_ARCHETYPE_ENUM:
                        item_type_name =
                            current_stack_item->source_field->archetype_dynamic_array.item_archetype_enum.type_name;
                        break;

                    case KAN_REFLECTION_ARCHETYPE_STRUCT:
                        item_type_name =
                            current_stack_item->source_field->archetype_dynamic_array.item_archetype_struct.type_name;
                        break;
                    }

                    break;
                }

                switch (current_stack_item->type)
                {
                case KAN_REFLECTION_PATCH_SECTION_TYPE_DYNAMIC_ARRAY_SET:
                    switch (item_archetype)
                    {
                    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                    case KAN_REFLECTION_ARCHETYPE_FLOATING:
                    case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
                    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
                    case KAN_REFLECTION_ARCHETYPE_ENUM:
                    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                    case KAN_REFLECTION_ARCHETYPE_PATCH:
                        break;

                    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                    {
                        // Special case. Interned string array might be reference array.
                        struct kan_resource_reference_type_info_node_t *type_node =
                            kan_resource_type_info_storage_query_type_node (
                                storage, current_stack_item->source_field_type->name);

                        for (kan_loop_size_t field_index = 0u; field_index < type_node->fields_to_check.size;
                             ++field_index)
                        {
                            struct kan_resource_reference_field_info_t *field_info =
                                &((struct kan_resource_reference_field_info_t *)
                                      type_node->fields_to_check.data)[field_index];

                            if (field_info->field == current_stack_item->source_field)
                            {
                                // This interned string array is actually a reference array.
                                kan_instance_size_t offset = 0u;

                                while (offset < node.chunk_info.size)
                                {
                                    const void *address = ((const uint8_t *) node.chunk_info.data) + offset;
                                    kan_resource_detected_container_add_reference (output_container, field_info->type,
                                                                                   *(kan_interned_string_t *) address,
                                                                                   field_info->compilation_usage);
                                    offset += sizeof (kan_interned_string_t);
                                }

                                break;
                            }
                        }

                        break;
                    }

                    case KAN_REFLECTION_ARCHETYPE_STRUCT:
                    {
                        kan_instance_size_t current_offset = node.chunk_info.offset;
                        const kan_instance_size_t end_offset = node.chunk_info.offset + node.chunk_info.size;

                        while (current_offset < end_offset)
                        {
                            const kan_instance_size_t struct_begin_offset = current_offset - current_offset % item_size;
                            const kan_instance_size_t struct_end_offset = struct_begin_offset + item_size;

                            kan_resource_detect_inside_data_chunk_for_struct_instance (
                                storage, struct_begin_offset, item_type_name, &node.chunk_info, output_container);
                            current_offset = struct_end_offset;
                        }

                        break;
                    }
                    }

                    break;

                case KAN_REFLECTION_PATCH_SECTION_TYPE_DYNAMIC_ARRAY_APPEND:
                    // Only structs can be appended, otherwise patch is malformed or this code is outdated.
                    KAN_ASSERT (item_archetype == KAN_REFLECTION_ARCHETYPE_STRUCT)
                    kan_resource_detect_inside_data_chunk_for_struct_instance (storage, 0u, item_type_name,
                                                                               &node.chunk_info, output_container);
                    break;
                }
            }
            else
            {
                // We're inside main section and therefore are able to use simplified logic.
                kan_resource_detect_inside_data_chunk_for_struct_instance (storage, 0u, patch_type->name,
                                                                           &node.chunk_info, output_container);
            }
        }
        else
        {
            while (stack.stack_end > stack.stack)
            {
                struct patch_section_stack_item_t *current_stack_item = stack.stack_end - 1u;
                if (KAN_TYPED_ID_32_IS_EQUAL (current_stack_item->id, node.section_info.parent_section_id))
                {
                    break;
                }

                --stack.stack_end;
            }

            struct patch_section_stack_item_t *parent_section =
                stack.stack_end > stack.stack ? stack.stack_end - 1u : NULL;

            kan_interned_string_t parent_struct_name = patch_type->name;
            kan_instance_size_t parent_struct_size = patch_type->size;

            if (parent_section)
            {
                switch (parent_section->type)
                {
                case KAN_REFLECTION_PATCH_SECTION_TYPE_DYNAMIC_ARRAY_SET:
                case KAN_REFLECTION_PATCH_SECTION_TYPE_DYNAMIC_ARRAY_APPEND:
                    KAN_ASSERT (parent_section->source_field->archetype == KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY)
                    // Only structs can be parents to sections.
                    KAN_ASSERT (parent_section->source_field->archetype_dynamic_array.item_archetype ==
                                KAN_REFLECTION_ARCHETYPE_STRUCT)
                    parent_struct_name =
                        parent_section->source_field->archetype_dynamic_array.item_archetype_struct.type_name;
                    parent_struct_size = parent_section->source_field->archetype_dynamic_array.item_size;
                    break;
                }
            }

            KAN_ASSERT (stack.stack_end < stack.stack + KAN_RESOURCE_PIPELINE_PATCH_SECTION_STACK_SIZE)
            stack.stack_end->id = node.section_info.section_id;
            stack.stack_end->type = node.section_info.type;

            stack.stack_end->source_field = kan_reflection_registry_query_local_field_by_offset (
                storage->registry, parent_struct_name, node.section_info.source_offset_in_parent % parent_struct_size,
                &stack.stack_end->source_field_type);
            KAN_ASSERT (stack.stack_end->source_field)
            ++stack.stack_end;
        }

        patch_iterator = kan_reflection_patch_iterator_next (patch_iterator);
    }
}

void kan_resource_detect_references (struct kan_resource_reference_type_info_storage_t *storage,
                                     kan_interned_string_t referencer_type_name,
                                     const void *referencer_data,
                                     struct kan_resource_detected_reference_container_t *output_container)
{
    struct kan_resource_reference_type_info_node_t *type_node =
        kan_resource_type_info_storage_query_type_node (storage, referencer_type_name);

    if (!type_node)
    {
        // Does not reference anything.
        return;
    }

    for (kan_loop_size_t field_index = 0u; field_index < type_node->fields_to_check.size; ++field_index)
    {
        struct kan_resource_reference_field_info_t *field_info =
            &((struct kan_resource_reference_field_info_t *) type_node->fields_to_check.data)[field_index];
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
            case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT:
            case KAN_REFLECTION_ARCHETYPE_PATCH:
                KAN_ASSERT (false)
                break;

            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                kan_resource_detected_container_add_reference (output_container, field_info->type,
                                                               *(kan_interned_string_t *) field_address,
                                                               field_info->compilation_usage);
                break;

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            {
                const kan_instance_size_t size =
                    kan_reflection_get_inline_array_size (field_info->field, referencer_data);

                for (kan_loop_size_t index = 0u; index < size; ++index)
                {
                    kan_resource_detected_container_add_reference (output_container, field_info->type,
                                                                   ((kan_interned_string_t *) field_address)[index],
                                                                   field_info->compilation_usage);
                }

                break;
            }

            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            {
                KAN_ASSERT (field_info->field->archetype_dynamic_array.item_archetype ==
                            KAN_REFLECTION_ARCHETYPE_INTERNED_STRING)
                const struct kan_dynamic_array_t *array = field_address;

                for (kan_loop_size_t index = 0u; index < array->size; ++index)
                {
                    kan_resource_detected_container_add_reference (output_container, field_info->type,
                                                                   ((kan_interned_string_t *) array->data)[index],
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
            case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                KAN_ASSERT (false)
                break;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                kan_resource_detect_references (storage, field_info->field->archetype_struct.type_name, field_address,
                                                output_container);
                break;

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            {
                const kan_instance_size_t size =
                    kan_reflection_get_inline_array_size (field_info->field, referencer_data);

                switch (field_info->field->archetype_inline_array.item_archetype)
                {
                case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                case KAN_REFLECTION_ARCHETYPE_FLOATING:
                case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
                case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
                case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                case KAN_REFLECTION_ARCHETYPE_ENUM:
                case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                    KAN_ASSERT (false)
                    break;

                case KAN_REFLECTION_ARCHETYPE_STRUCT:
                    for (kan_loop_size_t index = 0u; index < size; ++index)
                    {
                        kan_resource_detect_references (
                            storage, field_info->field->archetype_inline_array.item_archetype_struct.type_name,
                            ((uint8_t *) field_address) + index * field_info->field->archetype_inline_array.item_size,
                            output_container);
                    }

                    break;

                case KAN_REFLECTION_ARCHETYPE_PATCH:
                    for (kan_loop_size_t index = 0u; index < size; ++index)
                    {
                        kan_resource_detect_inside_patch (storage, ((kan_reflection_patch_t *) field_address)[index],
                                                          output_container);
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
                case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
                case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
                case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                case KAN_REFLECTION_ARCHETYPE_ENUM:
                case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                    KAN_ASSERT (false)
                    break;

                case KAN_REFLECTION_ARCHETYPE_STRUCT:
                    for (kan_loop_size_t index = 0u; index < array->size; ++index)
                    {
                        kan_resource_detect_references (
                            storage, field_info->field->archetype_dynamic_array.item_archetype_struct.type_name,
                            array->data + index * array->item_size, output_container);
                    }

                    break;

                case KAN_REFLECTION_ARCHETYPE_PATCH:
                    for (kan_loop_size_t index = 0u; index < array->size; ++index)
                    {
                        kan_resource_detect_inside_patch (storage, ((kan_reflection_patch_t *) array->data)[index],
                                                          output_container);
                    }

                    break;
                }

                break;
            }

            case KAN_REFLECTION_ARCHETYPE_PATCH:
                kan_resource_detect_inside_patch (storage, *(kan_reflection_patch_t *) field_address, output_container);
                break;
            }
        }
    }
}

/// \brief We need to make import rule resource type as it resides in resource directories.
KAN_REFLECTION_STRUCT_META (kan_resource_import_rule_t)
RESOURCE_PIPELINE_API struct kan_resource_resource_type_meta_t kan_resource_import_rule_resource_type = {
    .root = false,
};

kan_allocation_group_t kan_resource_import_rule_get_allocation_group (void)
{
    ensure_statics_initialized ();
    return resource_import_rule_allocation_group;
}

void kan_resource_import_input_init (struct kan_resource_import_input_t *instance)
{
    instance->source_path = NULL;
    instance->checksum = 0u;
    kan_dynamic_array_init (&instance->outputs, 0u, sizeof (char *), _Alignof (char *),
                            resource_import_rule_allocation_group);
}

void kan_resource_import_input_shutdown (struct kan_resource_import_input_t *instance)
{
    if (instance->source_path)
    {
        kan_free_general (resource_import_rule_allocation_group, instance->source_path,
                          strlen (instance->source_path) + 1u);
    }

    for (kan_loop_size_t index = 0u; index < instance->outputs.size; ++index)
    {
        char *output = ((char **) instance->outputs.data)[index];
        kan_free_general (resource_import_rule_allocation_group, output, strlen (output) + 1u);
    }

    kan_dynamic_array_shutdown (&instance->outputs);
}

void kan_resource_import_rule_init (struct kan_resource_import_rule_t *instance)
{
    ensure_statics_initialized ();
    instance->source_path_root = KAN_RESOURCE_IMPORT_SOURCE_PATH_ROOT_FILE_DIRECTORY;
    instance->source_path_rule = KAN_RESOURCE_IMPORT_SOURCE_PATH_RULE_EXACT;
    instance->source_path = NULL;

    instance->extension_filter = NULL;
    instance->configuration = KAN_HANDLE_SET_INVALID (kan_reflection_patch_t);

    kan_dynamic_array_init (&instance->last_import, 0u, sizeof (struct kan_resource_import_input_t),
                            _Alignof (struct kan_resource_import_input_t), resource_import_rule_allocation_group);
}

void kan_resource_import_rule_shutdown (struct kan_resource_import_rule_t *instance)
{
    if (instance->source_path)
    {
        kan_free_general (resource_import_rule_allocation_group, instance->source_path,
                          strlen (instance->source_path) + 1u);
    }

    if (KAN_HANDLE_IS_VALID (instance->configuration))
    {
        kan_reflection_patch_destroy (instance->configuration);
    }

    for (kan_loop_size_t index = 0u; index < instance->last_import.size; ++index)
    {
        kan_resource_import_input_shutdown (
            &((struct kan_resource_import_input_t *) instance->last_import.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->last_import);
}
