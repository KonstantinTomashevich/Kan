#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/reflection/field_visibility_iterator.h>
#include <kan/reflection/patch.h>
#include <kan/reflection/struct_helpers.h>
#include <kan/resource_pipeline/reflected_data.h>

KAN_LOG_DEFINE_CATEGORY (resource_pipeline_detect_references);

static bool statics_initialized = false;
static kan_allocation_group_t allocation_group;
KAN_USE_STATIC_INTERNED_IDS

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "resource_pipeline_reflected_data");
        kan_static_interned_ids_ensure_initialized ();
        statics_initialized = true;
    }
}

kan_allocation_group_t kan_resource_reflected_data_get_allocation_group (void)
{
    ensure_statics_initialized ();
    return allocation_group;
}

void kan_resource_reflected_data_resource_type_init (struct kan_resource_reflected_data_resource_type_t *instance)
{
    instance->name = NULL;
    instance->resource_type_meta = NULL;

    instance->produced_from_build_rule = false;
    instance->build_rule_primary_input_type = NULL;
    instance->build_rule_platform_configuration_type = NULL;

    instance->name = NULL;
    kan_dynamic_array_init (&instance->build_rule_secondary_types, 0u, sizeof (kan_interned_string_t),
                            alignof (kan_interned_string_t), allocation_group);

    instance->build_rule_functor = NULL;
    instance->build_rule_version = 0u;
}

void kan_resource_reflected_data_resource_type_shutdown (struct kan_resource_reflected_data_resource_type_t *instance)
{
    kan_dynamic_array_shutdown (&instance->build_rule_secondary_types);
}

void kan_resource_reflected_data_referencer_struct_init (
    struct kan_resource_reflected_data_referencer_struct_t *instance)
{
    instance->name = NULL;
    kan_dynamic_array_init (&instance->fields_to_check, 0u,
                            sizeof (struct kan_resource_reflected_data_referencer_field_t),
                            alignof (struct kan_resource_reflected_data_referencer_field_t), allocation_group);
}

void kan_resource_reflected_data_referencer_struct_shutdown (
    struct kan_resource_reflected_data_referencer_struct_t *instance)
{
    kan_dynamic_array_shutdown (&instance->fields_to_check);
}

static void scan_potential_resource_type (struct kan_resource_reflected_data_storage_t *output,
                                          const struct kan_reflection_struct_t *struct_to_scan)
{
    struct kan_reflection_struct_meta_iterator_t iterator = kan_reflection_registry_query_struct_meta (
        output->registry, struct_to_scan->name, KAN_STATIC_INTERNED_ID_GET (kan_resource_type_meta_t));

    const struct kan_resource_type_meta_t *resource_type = kan_reflection_struct_meta_iterator_get (&iterator);
    if (resource_type)
    {
#if defined(KAN_WITH_ASSERT)
        kan_reflection_struct_meta_iterator_next (&iterator);
        KAN_ASSERT_FORMATTED (!kan_reflection_struct_meta_iterator_get (&iterator),
                              "Resource type \"%s\" has several resource type metas.", struct_to_scan->name)
#endif

        iterator = kan_reflection_registry_query_struct_meta (output->registry, struct_to_scan->name,
                                                              KAN_STATIC_INTERNED_ID_GET (kan_resource_build_rule_t));
        const struct kan_resource_build_rule_t *build_rule = kan_reflection_struct_meta_iterator_get (&iterator);

#if defined(KAN_WITH_ASSERT)
        kan_reflection_struct_meta_iterator_next (&iterator);
        KAN_ASSERT_FORMATTED (!kan_reflection_struct_meta_iterator_get (&iterator),
                              "Resource type \"%s\" has several build rule metas.", struct_to_scan->name)

        // If this is a build rule, primary input type should not be root.
        if (build_rule)
        {
            if (build_rule->primary_input_type)
            {
                // Assert that primary input is resource type and not root.
                iterator = kan_reflection_registry_query_struct_meta (
                    output->registry, kan_string_intern (build_rule->primary_input_type),
                    KAN_STATIC_INTERNED_ID_GET (kan_resource_type_meta_t));

                const struct kan_resource_type_meta_t *primary_input_resource_type =
                    kan_reflection_struct_meta_iterator_get (&iterator);

                KAN_ASSERT_FORMATTED (
                    primary_input_resource_type,
                    "Resource type \"%s\" is registered as primary input for build rule for \"%s\" resource "
                    "type, but it does not exist.",
                    build_rule->primary_input_type, struct_to_scan->name)

                if (primary_input_resource_type)
                {
                    KAN_ASSERT_FORMATTED (
                        (primary_input_resource_type->flags & KAN_RESOURCE_TYPE_ROOT) == 0u,
                        "Resource type \"%s\" is registered as primary input for build rule for \"%s\" resource "
                        "type, but it is a root resource type. Building root types is not allowed.",
                        build_rule->primary_input_type, struct_to_scan->name)
                }

                // Assert that secondary input types are resource types.
                for (kan_instance_size_t index = 0u; index < build_rule->secondary_types_count; ++index)
                {
                    iterator = kan_reflection_registry_query_struct_meta (
                        output->registry, kan_string_intern (build_rule->secondary_types[index]),
                        KAN_STATIC_INTERNED_ID_GET (kan_resource_type_meta_t));

                    KAN_ASSERT_FORMATTED (
                        kan_reflection_struct_meta_iterator_get (&iterator),
                        "Resource type \"%s\" is registered as secondary input for build rule for \"%s\" resource "
                        "type, but it does not exist.",
                        build_rule->secondary_types[index], struct_to_scan->name)
                }
            }
            else
            {
                KAN_ASSERT_FORMATTED (build_rule->secondary_types_count == 0u,
                                      "Primary input for build rule for \"%s\" resource type is declared as import "
                                      "build rule and therefore should not have secondary inputs at all.",
                                      struct_to_scan->name)
            }
        }
#endif

        struct kan_resource_reflected_data_resource_type_t *node =
            kan_allocate_batched (allocation_group, sizeof (struct kan_resource_reflected_data_resource_type_t));
        kan_resource_reflected_data_resource_type_init (node);

        node->name = struct_to_scan->name;
        node->struct_type = struct_to_scan;
        node->resource_type_meta = resource_type;

        if (build_rule)
        {
            node->produced_from_build_rule = true;
            node->build_rule_primary_input_type = kan_string_intern (build_rule->primary_input_type);
            node->build_rule_platform_configuration_type = kan_string_intern (build_rule->platform_configuration_type);
            kan_dynamic_array_set_capacity (&node->build_rule_secondary_types, build_rule->secondary_types_count);

            for (kan_instance_size_t index = 0u; index < build_rule->secondary_types_count; ++index)
            {
                *(kan_interned_string_t *) kan_dynamic_array_add_last (&node->build_rule_secondary_types) =
                    kan_string_intern (build_rule->secondary_types[index]);
            }

            node->build_rule_functor = build_rule->functor;
            node->build_rule_version = build_rule->version;
        }

        node->node.hash = KAN_HASH_OBJECT_POINTER (node->name);
        kan_hash_storage_update_bucket_count_default (&output->resource_types,
                                                      KAN_RESOURCE_PIPELINE_RD_RESOURCE_TYPE_BUCKETS);
        kan_hash_storage_add (&output->resource_types, &node->node);

        if (resource_type->flags & KAN_RESOURCE_TYPE_ROOT)
        {
            kan_interned_string_t *spot = kan_dynamic_array_add_last (&output->root_resource_type_names);
            if (!spot)
            {
                kan_dynamic_array_set_capacity (&output->root_resource_type_names,
                                                KAN_MAX (1u, output->root_resource_type_names.size * 2u));
                spot = kan_dynamic_array_add_last (&output->root_resource_type_names);
                KAN_ASSERT (spot)
            }

            *spot = struct_to_scan->name;
        }
    }
#if defined(KAN_WITH_ASSERT)
    else
    {
        // Non-resource type cannot have build rule attached to it, assert this.
        iterator = kan_reflection_registry_query_struct_meta (output->registry, struct_to_scan->name,
                                                              KAN_STATIC_INTERNED_ID_GET (kan_resource_build_rule_t));
        KAN_ASSERT_FORMATTED (!kan_reflection_struct_meta_iterator_get (&iterator),
                              "Type \"%s\" has build rule meta, but has no resource type meta.", struct_to_scan->name)
    }
#endif
}

static inline struct kan_resource_reflected_data_referencer_field_t *referencer_struct_scan_add_field (
    struct kan_resource_reflected_data_referencer_struct_t *data, const struct kan_reflection_struct_t *scanned_struct)
{
    struct kan_resource_reflected_data_referencer_field_t *spot = kan_dynamic_array_add_last (&data->fields_to_check);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (
            &data->fields_to_check, KAN_MAX (1u + scanned_struct->fields_count / 4u, data->fields_to_check.size * 2u));
        spot = kan_dynamic_array_add_last (&data->fields_to_check);
    }

    KAN_ASSERT (spot)
    return spot;
}

static bool scan_potential_referencer_struct (struct kan_resource_reflected_data_storage_t *output,
                                              const struct kan_reflection_struct_t *struct_to_scan)
{
    // Any struct could be a referencer as any struct could be inside a patch, so we need to just scan all structs,
    // not only structs that are resource types and are nested in resource types.

    const struct kan_resource_reflected_data_referencer_struct_t *existent_data =
        kan_resource_reflected_data_storage_query_referencer_struct (output, struct_to_scan->name);

    if (existent_data)
    {
        return existent_data->fields_to_check.size > 0u;
    }

    struct kan_resource_reflected_data_referencer_struct_t *data =
        kan_allocate_batched (allocation_group, sizeof (struct kan_resource_reflected_data_referencer_struct_t));
    kan_resource_reflected_data_referencer_struct_init (data);

    data->name = struct_to_scan->name;
    data->node.hash = KAN_HASH_OBJECT_POINTER (struct_to_scan->name);
    kan_hash_storage_update_bucket_count_default (&output->referencer_structs,
                                                  KAN_RESOURCE_PIPELINE_RD_REFERENCER_STRUCTS_BUCKETS);
    kan_hash_storage_add (&output->referencer_structs, &data->node);

    for (kan_instance_size_t index = 0u; index < struct_to_scan->fields_count; ++index)
    {
        const struct kan_reflection_field_t *field = &struct_to_scan->fields[index];
        struct kan_reflection_struct_field_meta_iterator_t iterator = kan_reflection_registry_query_struct_field_meta (
            output->registry, struct_to_scan->name, field->name,
            KAN_STATIC_INTERNED_ID_GET (kan_resource_reference_meta_t));

        const struct kan_resource_reference_meta_t *resource_reference =
            kan_reflection_struct_field_meta_iterator_get (&iterator);

        if (!resource_reference)
        {
            kan_interned_string_t contains_struct_with_name = NULL;
            switch (field->archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                contains_struct_with_name = field->archetype_struct.type_name;
                break;

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                if (field->archetype_inline_array.item_archetype == KAN_REFLECTION_ARCHETYPE_STRUCT)
                {
                    contains_struct_with_name = field->archetype_inline_array.item_archetype_struct.type_name;
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                if (field->archetype_dynamic_array.item_archetype == KAN_REFLECTION_ARCHETYPE_STRUCT)
                {
                    contains_struct_with_name = field->archetype_dynamic_array.item_archetype_struct.type_name;
                }

                break;

            default:
                break;
            }

            if (contains_struct_with_name &&
                scan_potential_referencer_struct (
                    output, kan_reflection_registry_query_struct (output->registry, contains_struct_with_name)))
            {
                *referencer_struct_scan_add_field (data, struct_to_scan) =
                    (struct kan_resource_reflected_data_referencer_field_t) {
                        .field = field,
                        .referenced_type = NULL,
                        .flags = 0u,
                    };
            }

            continue;
        }

#if defined(KAN_WITH_ASSERT)
        // Assert that there is no several metas.
        kan_reflection_struct_field_meta_iterator_next (&iterator);
        KAN_ASSERT (!kan_reflection_struct_field_meta_iterator_get (&iterator));
        const bool single_resource_id = field->archetype == KAN_REFLECTION_ARCHETYPE_INTERNED_STRING;

        const bool inline_resource_id_array =
            field->archetype == KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY &&
            field->archetype_inline_array.item_archetype == KAN_REFLECTION_ARCHETYPE_INTERNED_STRING;

        const bool dynamic_resource_id_array =
            field->archetype == KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY &&
            field->archetype_dynamic_array.item_archetype == KAN_REFLECTION_ARCHETYPE_INTERNED_STRING;

        KAN_ASSERT_FORMATTED (single_resource_id || inline_resource_id_array || dynamic_resource_id_array,
                              "Field \"%s\" of struct \"%s\" is marked as resource reference field, but it does not "
                              "contain interned strings.",
                              field->name, struct_to_scan->name)

        if (resource_reference->type_name)
        {
            KAN_ASSERT_FORMATTED (kan_reflection_registry_query_struct (
                                      output->registry, kan_string_intern (resource_reference->type_name)),
                                  "Resource type \"%s\" referenced by field \"%s\" of struct \"%s\" is not found.",
                                  resource_reference->type_name, field->name, struct_to_scan->name)

            struct kan_reflection_struct_meta_iterator_t struct_meta_iterator =
                kan_reflection_registry_query_struct_meta (output->registry, struct_to_scan->name,
                                                           KAN_STATIC_INTERNED_ID_GET (kan_resource_type_meta_t));

            KAN_ASSERT_FORMATTED (
                kan_reflection_struct_meta_iterator_get (&struct_meta_iterator),
                "Struct \"%s\" referenced as resource type by field \"%s\" of struct \"%s\" is not a resource type.",
                resource_reference->type_name, field->name, struct_to_scan->name)
        }
#endif

        *referencer_struct_scan_add_field (data, struct_to_scan) =
            (struct kan_resource_reflected_data_referencer_field_t) {
                .field = field,
                .referenced_type = kan_string_intern (resource_reference->type_name),
                .flags = resource_reference->flags,
            };
    }

    kan_dynamic_array_set_capacity (&data->fields_to_check, data->fields_to_check.size);
    return data->fields_to_check.size > 0u;
}

void kan_resource_reflected_data_storage_build (struct kan_resource_reflected_data_storage_t *output,
                                                kan_reflection_registry_t registry)
{
    ensure_statics_initialized ();
    output->registry = registry;
    kan_hash_storage_init (&output->resource_types, allocation_group, KAN_RESOURCE_PIPELINE_RD_RESOURCE_TYPE_BUCKETS);
    kan_hash_storage_init (&output->referencer_structs, allocation_group,
                           KAN_RESOURCE_PIPELINE_RD_REFERENCER_STRUCTS_BUCKETS);
    kan_dynamic_array_init (&output->root_resource_type_names, 0u, sizeof (kan_interned_string_t),
                            alignof (kan_interned_string_t), allocation_group);

    kan_reflection_registry_struct_iterator_t iterator = kan_reflection_registry_struct_iterator_create (registry);
    const struct kan_reflection_struct_t *struct_to_scan;

    while ((struct_to_scan = kan_reflection_registry_struct_iterator_get (iterator)))
    {
        scan_potential_resource_type (output, struct_to_scan);
        scan_potential_referencer_struct (output, struct_to_scan);
        iterator = kan_reflection_registry_struct_iterator_next (iterator);
    }

    kan_dynamic_array_set_capacity (&output->root_resource_type_names, output->root_resource_type_names.size);

    // Cleanup referencer structs that do not actually have references, because during scan we need to create entries
    // for everything to avoid double-checking.
    struct kan_resource_reflected_data_referencer_struct_t *referencer_struct =
        (struct kan_resource_reflected_data_referencer_struct_t *) output->referencer_structs.items.first;

    while (referencer_struct)
    {
        struct kan_resource_reflected_data_referencer_struct_t *next =
            (struct kan_resource_reflected_data_referencer_struct_t *) referencer_struct->node.list_node.next;

        if (referencer_struct->fields_to_check.size == 0u)
        {
            kan_hash_storage_remove (&output->referencer_structs, &referencer_struct->node);
            kan_resource_reflected_data_referencer_struct_shutdown (referencer_struct);
            kan_free_batched (allocation_group, referencer_struct);
        }

        referencer_struct = next;
    }

    kan_hash_storage_update_bucket_count_default (&output->referencer_structs,
                                                  KAN_RESOURCE_PIPELINE_RD_REFERENCER_STRUCTS_BUCKETS);
}

const struct kan_resource_reflected_data_resource_type_t *kan_resource_reflected_data_storage_query_resource_type (
    const struct kan_resource_reflected_data_storage_t *storage, kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&storage->resource_types, KAN_HASH_OBJECT_POINTER (type_name));

    struct kan_resource_reflected_data_resource_type_t *node =
        (struct kan_resource_reflected_data_resource_type_t *) bucket->first;

    const struct kan_resource_reflected_data_resource_type_t *node_end =
        (struct kan_resource_reflected_data_resource_type_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == type_name)
        {
            return node;
        }

        node = (struct kan_resource_reflected_data_resource_type_t *) node->node.list_node.next;
    }

    return NULL;
}

const struct kan_resource_reflected_data_referencer_struct_t *
kan_resource_reflected_data_storage_query_referencer_struct (
    const struct kan_resource_reflected_data_storage_t *storage, kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&storage->referencer_structs, KAN_HASH_OBJECT_POINTER (type_name));

    struct kan_resource_reflected_data_referencer_struct_t *node =
        (struct kan_resource_reflected_data_referencer_struct_t *) bucket->first;

    const struct kan_resource_reflected_data_referencer_struct_t *node_end =
        (struct kan_resource_reflected_data_referencer_struct_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == type_name)
        {
            return node;
        }

        node = (struct kan_resource_reflected_data_referencer_struct_t *) node->node.list_node.next;
    }

    return NULL;
}

static void add_detected_reference (struct kan_dynamic_array_t *output,
                                    kan_interned_string_t type,
                                    kan_interned_string_t name,
                                    enum kan_resource_reference_meta_flags_t flags)
{
    if (!name)
    {
        return;
    }

    enum kan_resource_reference_flags_t log_flags = 0u;
    if ((flags & KAN_RESOURCE_REFERENCE_META_PLATFORM_OPTIONAL) == 0u)
    {
        log_flags |= KAN_RESOURCE_REFERENCE_REQUIRED;
    }

    for (kan_loop_size_t index = 0u; index < output->size; ++index)
    {
        struct kan_resource_log_reference_t *reference = &((struct kan_resource_log_reference_t *) output->data)[index];
        if (reference->name == name && reference->type == type)
        {
            reference->flags |= log_flags;
            return;
        }
    }

    struct kan_resource_log_reference_t *spot = kan_dynamic_array_add_last (output);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (output, KAN_MAX (1u, output->capacity * 2u));
        spot = kan_dynamic_array_add_last (output);
        KAN_ASSERT (spot)
    }

    spot->type = type;
    spot->name = name;
    spot->flags = log_flags;
}

static void detect_references_inside_data_chunk_for_struct_instance (
    const struct kan_resource_reflected_data_storage_t *storage,
    kan_instance_size_t part_offset,
    kan_interned_string_t part_type_name,
    struct kan_reflection_patch_chunk_info_t *chunk,
    struct kan_dynamic_array_t *output)
{
    const struct kan_resource_reflected_data_referencer_struct_t *type_data =
        kan_resource_reflected_data_storage_query_referencer_struct (storage, part_type_name);

    if (!type_data)
    {
        // Does not reference anything.
        return;
    }

    for (kan_loop_size_t field_index = 0u; field_index < type_data->fields_to_check.size; ++field_index)
    {
        const struct kan_resource_reflected_data_referencer_field_t *field =
            &((struct kan_resource_reflected_data_referencer_field_t *) type_data->fields_to_check.data)[field_index];

        if (field->field->visibility_condition_field)
        {
            KAN_LOG (resource_pipeline_detect_references, KAN_LOG_ERROR,
                     "Field \"%s\" of type \"%s\" is marked as resource reference and found inside patch, but it has "
                     "visibility condition and patches do not fully support it!",
                     field->field->name, part_type_name)
            continue;
        }

        kan_instance_size_t field_offset = part_offset + field->field->offset;
        if (field_offset + field->field->size <= chunk->offset || field_offset >= chunk->offset + chunk->size)
        {
            continue;
        }

        switch (field->field->archetype)
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

            add_detected_reference (output, field->referenced_type, *(kan_interned_string_t *) address, field->flags);
            break;
        }

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
            detect_references_inside_data_chunk_for_struct_instance (
                storage, field_offset, field->field->archetype_struct.type_name, chunk, output);
            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        {
            // Inline arrays in patches are always treated as full.
            const kan_instance_size_t size = field->field->archetype_inline_array.item_count;
            kan_instance_size_t current_offset = field_offset;
            const kan_instance_size_t end_offset =
                KAN_MIN (chunk->offset + chunk->size, field_offset + field->field->size);

            switch (field->field->archetype_inline_array.item_archetype)
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

                    add_detected_reference (output, field->referenced_type, *(kan_interned_string_t *) address,
                                            field->flags);
                    current_offset += size;
                }

                break;
            }

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                while (current_offset < end_offset)
                {
                    detect_references_inside_data_chunk_for_struct_instance (
                        storage, current_offset, field->field->archetype_struct.type_name, chunk, output);
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
    struct patch_section_stack_item_t stack[KAN_RESOURCE_PIPELINE_PATCH_SECTION_STACK];
};

static inline void detect_references_inside_patch (const struct kan_resource_reflected_data_storage_t *storage,
                                                   kan_reflection_patch_t patch,
                                                   struct kan_dynamic_array_t *output)
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
                        const struct kan_resource_reflected_data_referencer_struct_t *type_data =
                            kan_resource_reflected_data_storage_query_referencer_struct (
                                storage, current_stack_item->source_field_type->name);

                        for (kan_loop_size_t field_index = 0u; field_index < type_data->fields_to_check.size;
                             ++field_index)
                        {
                            struct kan_resource_reflected_data_referencer_field_t *field =
                                &((struct kan_resource_reflected_data_referencer_field_t *)
                                      type_data->fields_to_check.data)[field_index];

                            if (field->field == current_stack_item->source_field)
                            {
                                // This interned string array is actually a reference array.
                                kan_instance_size_t offset = 0u;

                                while (offset < node.chunk_info.size)
                                {
                                    const void *address = ((const uint8_t *) node.chunk_info.data) + offset;
                                    add_detected_reference (output, field->referenced_type,
                                                            *(kan_interned_string_t *) address, field->flags);
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

                            detect_references_inside_data_chunk_for_struct_instance (
                                storage, struct_begin_offset, item_type_name, &node.chunk_info, output);
                            current_offset = struct_end_offset;
                        }

                        break;
                    }
                    }

                    break;

                case KAN_REFLECTION_PATCH_SECTION_TYPE_DYNAMIC_ARRAY_APPEND:
                    // Only structs can be appended, otherwise patch is malformed or this code is outdated.
                    KAN_ASSERT (item_archetype == KAN_REFLECTION_ARCHETYPE_STRUCT)
                    detect_references_inside_data_chunk_for_struct_instance (storage, 0u, item_type_name,
                                                                             &node.chunk_info, output);
                    break;
                }
            }
            else
            {
                // We're inside main section and therefore are able to use simplified logic.
                detect_references_inside_data_chunk_for_struct_instance (storage, 0u, patch_type->name,
                                                                         &node.chunk_info, output);
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

            KAN_ASSERT (stack.stack_end < stack.stack + KAN_RESOURCE_PIPELINE_PATCH_SECTION_STACK)
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

void kan_resource_reflected_data_storage_detect_references (const struct kan_resource_reflected_data_storage_t *storage,
                                                            kan_interned_string_t referencer_type_name,
                                                            const void *referencer_data,
                                                            struct kan_dynamic_array_t *output_container)
{
    const struct kan_resource_reflected_data_referencer_struct_t *type_data =
        kan_resource_reflected_data_storage_query_referencer_struct (storage, referencer_type_name);

    if (!type_data)
    {
        // Does not reference anything.
        return;
    }

    for (kan_loop_size_t field_index = 0u; field_index < type_data->fields_to_check.size; ++field_index)
    {
        struct kan_resource_reflected_data_referencer_field_t *field =
            &((struct kan_resource_reflected_data_referencer_field_t *) type_data->fields_to_check.data)[field_index];
        const void *field_address = ((const uint8_t *) referencer_data) + field->field->offset;

        if (!kan_reflection_check_visibility (
                field->field->visibility_condition_field, field->field->visibility_condition_values_count,
                field->field->visibility_condition_values,
                ((uint8_t *) referencer_data) +
                    (field->field->visibility_condition_field ? field->field->visibility_condition_field->offset : 0u)))
        {
            // Skip invisible field.
            continue;
        }

        switch (field->field->archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            KAN_ASSERT (false)
            break;

        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            add_detected_reference (output_container, field->referenced_type, *(kan_interned_string_t *) field_address,
                                    field->flags);
            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
            kan_resource_reflected_data_storage_detect_references (storage, field->field->archetype_struct.type_name,
                                                                   field_address, output_container);
            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        {
            const kan_instance_size_t size = kan_reflection_get_inline_array_size (field->field, referencer_data);
            switch (field->field->archetype_inline_array.item_archetype)
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
                KAN_ASSERT (false)
                break;

            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                for (kan_loop_size_t index = 0u; index < size; ++index)
                {
                    add_detected_reference (output_container, field->referenced_type,
                                            ((kan_interned_string_t *) field_address)[index], field->flags);
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                for (kan_loop_size_t index = 0u; index < size; ++index)
                {
                    kan_resource_reflected_data_storage_detect_references (
                        storage, field->field->archetype_inline_array.item_archetype_struct.type_name,
                        ((uint8_t *) field_address) + index * field->field->archetype_inline_array.item_size,
                        output_container);
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_PATCH:
                for (kan_loop_size_t index = 0u; index < size; ++index)
                {
                    detect_references_inside_patch (storage, ((kan_reflection_patch_t *) field_address)[index],
                                                    output_container);
                }

                break;
            }

            break;
        }

        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        {
            const struct kan_dynamic_array_t *array = field_address;
            switch (field->field->archetype_dynamic_array.item_archetype)
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
                KAN_ASSERT (false)
                break;

            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                for (kan_loop_size_t index = 0u; index < array->size; ++index)
                {
                    add_detected_reference (output_container, field->referenced_type,
                                            ((kan_interned_string_t *) array->data)[index], field->flags);
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                for (kan_loop_size_t index = 0u; index < array->size; ++index)
                {
                    kan_resource_reflected_data_storage_detect_references (
                        storage, field->field->archetype_dynamic_array.item_archetype_struct.type_name,
                        array->data + index * array->item_size, output_container);
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_PATCH:
                for (kan_loop_size_t index = 0u; index < array->size; ++index)
                {
                    detect_references_inside_patch (storage, ((kan_reflection_patch_t *) array->data)[index],
                                                    output_container);
                }

                break;
            }

            break;
        }

        case KAN_REFLECTION_ARCHETYPE_PATCH:
            detect_references_inside_patch (storage, *(kan_reflection_patch_t *) field_address, output_container);
            break;
        }
    }
}

void kan_resource_reflected_data_storage_shutdown (struct kan_resource_reflected_data_storage_t *instance)
{
    struct kan_resource_reflected_data_resource_type_t *resource_type_data =
        (struct kan_resource_reflected_data_resource_type_t *) instance->resource_types.items.first;

    while (resource_type_data)
    {
        struct kan_resource_reflected_data_resource_type_t *next =
            (struct kan_resource_reflected_data_resource_type_t *) resource_type_data->node.list_node.next;

        kan_resource_reflected_data_resource_type_shutdown (resource_type_data);
        kan_free_batched (allocation_group, resource_type_data);
        resource_type_data = next;
    }

    struct kan_resource_reflected_data_referencer_struct_t *referencer_struct_data =
        (struct kan_resource_reflected_data_referencer_struct_t *) instance->referencer_structs.items.first;

    while (referencer_struct_data)
    {
        struct kan_resource_reflected_data_referencer_struct_t *next =
            (struct kan_resource_reflected_data_referencer_struct_t *) referencer_struct_data->node.list_node.next;

        kan_resource_reflected_data_referencer_struct_shutdown (referencer_struct_data);
        kan_free_batched (allocation_group, referencer_struct_data);
        referencer_struct_data = next;
    }

    kan_hash_storage_shutdown (&instance->resource_types);
    kan_hash_storage_shutdown (&instance->referencer_structs);
    kan_dynamic_array_shutdown (&instance->root_resource_type_names);
}
