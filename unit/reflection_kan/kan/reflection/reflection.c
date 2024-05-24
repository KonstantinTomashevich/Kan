#define _CRT_SECURE_NO_WARNINGS

#include <memory.h>
#include <qsort.h>
#include <stddef.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/api_common/type_punning.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/hash_storage.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/cpu_dispatch/job.h>
#include <kan/error/critical.h>
#include <kan/hash/hash.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/reflection/field_visibility_iterator.h>
#include <kan/reflection/migration.h>
#include <kan/reflection/patch.h>
#include <kan/reflection/registry.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (reflection_registry);
KAN_LOG_DEFINE_CATEGORY (reflection_patch_builder);
KAN_LOG_DEFINE_CATEGORY (reflection_migration_seed);
KAN_LOG_DEFINE_CATEGORY (reflection_migrator);

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

struct function_node_t
{
    struct kan_hash_storage_node_t node;
    const struct kan_reflection_function_t *function_reflection;
};

struct enum_meta_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t enum_name;
    kan_interned_string_t meta_type_name;
    const void *meta;
};

struct enum_meta_iterator_t
{
    struct enum_meta_node_t *current;
    struct enum_meta_node_t *end;
    kan_interned_string_t enum_name;
    kan_interned_string_t meta_type_name;
};

_Static_assert (sizeof (struct enum_meta_iterator_t) == sizeof (struct kan_reflection_enum_meta_iterator_t),
                "Iterator sizes match.");
_Static_assert (_Alignof (struct enum_meta_iterator_t) == _Alignof (struct kan_reflection_enum_meta_iterator_t),
                "Iterator alignments match.");

struct enum_value_meta_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t enum_name;
    kan_interned_string_t enum_value_name;
    kan_interned_string_t meta_type_name;
    const void *meta;
};

struct enum_value_meta_iterator_t
{
    struct enum_value_meta_node_t *current;
    struct enum_value_meta_node_t *end;
    kan_interned_string_t enum_name;
    kan_interned_string_t enum_value_name;
    kan_interned_string_t meta_type_name;
};

_Static_assert (sizeof (struct enum_value_meta_iterator_t) == sizeof (struct kan_reflection_enum_value_meta_iterator_t),
                "Iterator sizes match.");
_Static_assert (_Alignof (struct enum_value_meta_iterator_t) ==
                    _Alignof (struct kan_reflection_enum_value_meta_iterator_t),
                "Iterator alignments match.");

struct struct_meta_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t struct_name;
    kan_interned_string_t meta_type_name;
    const void *meta;
};

struct struct_meta_iterator_t
{
    struct struct_meta_node_t *current;
    struct struct_meta_node_t *end;
    kan_interned_string_t struct_name;
    kan_interned_string_t meta_type_name;
};

_Static_assert (sizeof (struct struct_meta_iterator_t) == sizeof (struct kan_reflection_struct_meta_iterator_t),
                "Iterator sizes match.");
_Static_assert (_Alignof (struct struct_meta_iterator_t) == _Alignof (struct kan_reflection_struct_meta_iterator_t),
                "Iterator alignments match.");

struct struct_field_meta_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t struct_name;
    kan_interned_string_t struct_field_name;
    kan_interned_string_t meta_type_name;
    const void *meta;
};

struct struct_field_meta_iterator_t
{
    struct struct_field_meta_node_t *current;
    struct struct_field_meta_node_t *end;
    kan_interned_string_t struct_name;
    kan_interned_string_t struct_field_name;
    kan_interned_string_t meta_type_name;
};

_Static_assert (sizeof (struct struct_field_meta_iterator_t) ==
                    sizeof (struct kan_reflection_struct_field_meta_iterator_t),
                "Iterator sizes match.");
_Static_assert (_Alignof (struct struct_field_meta_iterator_t) ==
                    _Alignof (struct kan_reflection_struct_field_meta_iterator_t),
                "Iterator alignments match.");

struct function_meta_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t function_name;
    kan_interned_string_t meta_type_name;
    const void *meta;
};

struct function_meta_iterator_t
{
    struct function_meta_node_t *current;
    struct function_meta_node_t *end;
    kan_interned_string_t function_name;
    kan_interned_string_t meta_type_name;
};

_Static_assert (sizeof (struct function_meta_iterator_t) == sizeof (struct kan_reflection_function_meta_iterator_t),
                "Iterator sizes match.");
_Static_assert (_Alignof (struct function_meta_iterator_t) == _Alignof (struct kan_reflection_function_meta_iterator_t),
                "Iterator alignments match.");

struct function_argument_meta_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t function_name;
    kan_interned_string_t function_argument_name;
    kan_interned_string_t meta_type_name;
    const void *meta;
};

struct function_argument_meta_iterator_t
{
    struct function_argument_meta_node_t *current;
    struct function_argument_meta_node_t *end;
    kan_interned_string_t function_name;
    kan_interned_string_t function_argument_name;
    kan_interned_string_t meta_type_name;
};

_Static_assert (sizeof (struct function_argument_meta_iterator_t) ==
                    sizeof (struct kan_reflection_function_argument_meta_iterator_t),
                "Iterator sizes match.");
_Static_assert (_Alignof (struct function_argument_meta_iterator_t) ==
                    _Alignof (struct kan_reflection_function_argument_meta_iterator_t),
                "Iterator alignments match.");

struct registry_t
{
    kan_allocation_group_t allocation_group;
    struct kan_hash_storage_t enum_storage;
    struct kan_hash_storage_t struct_storage;
    struct kan_hash_storage_t function_storage;
    struct kan_hash_storage_t enum_meta_storage;
    struct kan_hash_storage_t enum_value_meta_storage;
    struct kan_hash_storage_t struct_meta_storage;
    struct kan_hash_storage_t struct_field_meta_storage;
    struct kan_hash_storage_t function_meta_storage;
    struct kan_hash_storage_t function_argument_meta_storage;
    struct kan_atomic_int_t patch_addition_lock;
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
    uint64_t node_count;
    struct compiled_patch_node_t *begin;
    struct compiled_patch_node_t *end;

    struct registry_t *registry;
    struct compiled_patch_t *next;
    struct compiled_patch_t *previous;
};

struct enum_migration_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t type_name;
    struct kan_reflection_enum_migration_seed_t seed;
};

struct struct_migration_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t type_name;
    struct kan_reflection_struct_migration_seed_t seed;
};

struct migration_seed_t
{
    struct registry_t *source_registry;
    struct registry_t *target_registry;
    struct kan_hash_storage_t enums;
    struct kan_hash_storage_t structs;
};

#define MIGRATOR_CONDITION_INDEX_NONE UINT64_MAX

struct migrator_condition_t
{
    uint64_t absolute_source_offset;
    const struct kan_reflection_field_t *condition_field;
    uint64_t condition_values_count;
    int64_t *condition_values;
    uint64_t parent_condition_index;
};

struct migrator_command_copy_t
{
    uint64_t absolute_source_offset;
    uint64_t absolute_target_offset;
    uint64_t size;
    uint64_t condition_index;
};

struct migrator_command_adapt_numeric_t
{
    uint64_t absolute_source_offset;
    uint64_t absolute_target_offset;
    uint64_t source_size;
    uint64_t target_size;
    enum kan_reflection_archetype_t archetype;
    uint64_t condition_index;
};

struct migrator_command_adapt_enum_t
{
    uint64_t absolute_source_offset;
    uint64_t absolute_target_offset;
    kan_interned_string_t type_name;
    uint64_t condition_index;
};

struct migrator_command_adapt_dynamic_array_t
{
    uint64_t absolute_source_offset;
    const struct kan_reflection_field_t *source_field;
    uint64_t absolute_target_offset;
    const struct kan_reflection_field_t *target_field;
    uint64_t condition_index;
};

struct migrator_command_set_zero_t
{
    uint64_t absolute_source_offset;
    uint64_t size;
    uint64_t condition_index;
};

struct migrator_temporary_node_t
{
    struct migrator_temporary_node_t *next;
    union
    {
        struct migrator_condition_t condition;
        struct migrator_command_copy_t copy;
        struct migrator_command_adapt_numeric_t adapt_numeric;
        struct migrator_command_adapt_enum_t adapt_enum;
        struct migrator_command_adapt_dynamic_array_t adapt_dynamic_array;
        struct migrator_command_set_zero_t set_zero;
    };
};

struct migrator_command_temporary_queues_t
{
    uint64_t condition_count;
    struct migrator_temporary_node_t *condition_first;
    struct migrator_temporary_node_t *condition_last;
    uint64_t copy_count;
    struct migrator_temporary_node_t *copy_first;
    struct migrator_temporary_node_t *copy_last;
    uint64_t adapt_numeric_count;
    struct migrator_temporary_node_t *adapt_numeric_first;
    struct migrator_temporary_node_t *adapt_numeric_last;
    uint64_t adapt_enum_count;
    struct migrator_temporary_node_t *adapt_enum_first;
    struct migrator_temporary_node_t *adapt_enum_last;
    uint64_t adapt_dynamic_array_count;
    struct migrator_temporary_node_t *adapt_dynamic_array_first;
    struct migrator_temporary_node_t *adapt_dynamic_array_last;
    uint64_t set_zero_count;
    struct migrator_temporary_node_t *set_zero_first;
    struct migrator_temporary_node_t *set_zero_last;
};

struct struct_migrator_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t type_name;
    uint64_t conditions_count;
    struct migrator_condition_t *conditions;
    uint64_t copy_commands_count;
    struct migrator_command_copy_t *copy_commands;
    uint64_t adapt_numeric_commands_count;
    struct migrator_command_adapt_numeric_t *adapt_numeric_commands;
    uint64_t adapt_enum_commands_count;
    struct migrator_command_adapt_enum_t *adapt_enum_commands;
    uint64_t adapt_dynamic_array_commands_count;
    struct migrator_command_adapt_dynamic_array_t *adapt_dynamic_array_commands;
    uint64_t set_zero_commands_count;
    struct migrator_command_set_zero_t *set_zero_commands;
};

struct migrator_t
{
    struct migration_seed_t *source_seed;
    struct kan_hash_storage_t struct_migrators;
};

static kan_bool_t compiled_patch_allocation_group_ready = KAN_FALSE;
static kan_allocation_group_t compiled_patch_allocation_group;

static kan_allocation_group_t get_compiled_patch_allocation_group (void)
{
    if (!compiled_patch_allocation_group_ready)
    {
        compiled_patch_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "reflection_compiled_patch");
        compiled_patch_allocation_group_ready = KAN_TRUE;
    }

    return compiled_patch_allocation_group;
}

kan_reflection_registry_t kan_reflection_registry_create (void)
{
    const kan_allocation_group_t group =
        kan_allocation_group_get_child (kan_allocation_group_stack_get (), "reflection_registry");
    struct registry_t *registry = (struct registry_t *) kan_allocate_batched (group, sizeof (struct registry_t));
    registry->allocation_group = group;
    registry->patch_addition_lock = kan_atomic_int_init (0);
    registry->first_patch = NULL;

    kan_hash_storage_init (&registry->enum_storage, group, KAN_REFLECTION_ENUM_INITIAL_BUCKETS);
    kan_hash_storage_init (&registry->struct_storage, group, KAN_REFLECTION_STRUCT_INITIAL_BUCKETS);
    kan_hash_storage_init (&registry->function_storage, group, KAN_REFLECTION_FUNCTION_INITIAL_BUCKETS);
    kan_hash_storage_init (&registry->enum_meta_storage, group, KAN_REFLECTION_ENUM_META_INITIAL_BUCKETS);
    kan_hash_storage_init (&registry->enum_value_meta_storage, group, KAN_REFLECTION_ENUM_VALUE_META_INITIAL_BUCKETS);
    kan_hash_storage_init (&registry->struct_meta_storage, group, KAN_REFLECTION_STRUCT_META_INITIAL_BUCKETS);
    kan_hash_storage_init (&registry->struct_field_meta_storage, group,
                           KAN_REFLECTION_STRUCT_FIELD_META_INITIAL_BUCKETS);
    kan_hash_storage_init (&registry->function_meta_storage, group, KAN_REFLECTION_FUNCTION_META_INITIAL_BUCKETS);
    kan_hash_storage_init (&registry->function_argument_meta_storage, group,
                           KAN_REFLECTION_FUNCTION_ARGUMENT_META_INITIAL_BUCKETS);

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

    kan_hash_storage_update_bucket_count_default (&registry_struct->enum_storage, KAN_REFLECTION_ENUM_INITIAL_BUCKETS);
    kan_hash_storage_add (&registry_struct->enum_storage, &node->node);
    return KAN_TRUE;
}

void kan_reflection_registry_add_enum_meta (kan_reflection_registry_t registry,
                                            kan_interned_string_t enum_name,
                                            kan_interned_string_t meta_type_name,
                                            const void *meta)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct enum_meta_node_t *node = (struct enum_meta_node_t *) kan_allocate_batched (registry_struct->allocation_group,
                                                                                      sizeof (struct enum_meta_node_t));

    node->node.hash = kan_hash_combine ((uint64_t) enum_name, (uint64_t) meta_type_name);
    node->enum_name = enum_name;
    node->meta_type_name = meta_type_name;
    node->meta = meta;

    kan_hash_storage_update_bucket_count_default (&registry_struct->enum_meta_storage,
                                                  KAN_REFLECTION_ENUM_META_INITIAL_BUCKETS);
    kan_hash_storage_add (&registry_struct->enum_meta_storage, &node->node);
}

void kan_reflection_registry_add_enum_value_meta (kan_reflection_registry_t registry,
                                                  kan_interned_string_t enum_name,
                                                  kan_interned_string_t enum_value_name,
                                                  kan_interned_string_t meta_type_name,
                                                  const void *meta)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct enum_value_meta_node_t *node = (struct enum_value_meta_node_t *) kan_allocate_batched (
        registry_struct->allocation_group, sizeof (struct enum_value_meta_node_t));

    node->node.hash = kan_hash_combine (kan_hash_combine ((uint64_t) enum_name, (uint64_t) enum_value_name),
                                        (uint64_t) meta_type_name);
    node->enum_name = enum_name;
    node->enum_value_name = enum_value_name;
    node->meta_type_name = meta_type_name;
    node->meta = meta;

    kan_hash_storage_update_bucket_count_default (&registry_struct->enum_value_meta_storage,
                                                  KAN_REFLECTION_ENUM_VALUE_META_INITIAL_BUCKETS);
    kan_hash_storage_add (&registry_struct->enum_value_meta_storage, &node->node);
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

        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        case KAN_REFLECTION_ARCHETYPE_STRUCT:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        case KAN_REFLECTION_ARCHETYPE_PATCH:
            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            KAN_ASSERT (field_reflection->archetype_inline_array.item_count > 0u)
            KAN_ASSERT (
                field_reflection->archetype_inline_array.item_archetype != KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY &&
                field_reflection->archetype_inline_array.item_archetype != KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY)
            break;

        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            KAN_ASSERT (
                field_reflection->archetype_dynamic_array.item_archetype != KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY &&
                field_reflection->archetype_dynamic_array.item_archetype != KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY)
            break;
        }
    }
#endif

    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct struct_node_t *node = (struct struct_node_t *) kan_allocate_batched (registry_struct->allocation_group,
                                                                                sizeof (struct struct_node_t));
    node->node.hash = (uint64_t) struct_reflection->name;
    node->struct_reflection = struct_reflection;

    kan_hash_storage_update_bucket_count_default (&registry_struct->struct_storage,
                                                  KAN_REFLECTION_STRUCT_INITIAL_BUCKETS);
    kan_hash_storage_add (&registry_struct->struct_storage, &node->node);
    return KAN_TRUE;
}

void kan_reflection_registry_add_struct_meta (kan_reflection_registry_t registry,
                                              kan_interned_string_t struct_name,
                                              kan_interned_string_t meta_type_name,
                                              const void *meta)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct struct_meta_node_t *node = (struct struct_meta_node_t *) kan_allocate_batched (
        registry_struct->allocation_group, sizeof (struct struct_meta_node_t));

    node->node.hash = kan_hash_combine ((uint64_t) struct_name, (uint64_t) meta_type_name);
    node->struct_name = struct_name;
    node->meta_type_name = meta_type_name;
    node->meta = meta;

    kan_hash_storage_update_bucket_count_default (&registry_struct->struct_meta_storage,
                                                  KAN_REFLECTION_STRUCT_META_INITIAL_BUCKETS);
    kan_hash_storage_add (&registry_struct->struct_meta_storage, &node->node);
}

void kan_reflection_registry_add_struct_field_meta (kan_reflection_registry_t registry,
                                                    kan_interned_string_t struct_name,
                                                    kan_interned_string_t struct_field_name,
                                                    kan_interned_string_t meta_type_name,
                                                    const void *meta)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct struct_field_meta_node_t *node = (struct struct_field_meta_node_t *) kan_allocate_batched (
        registry_struct->allocation_group, sizeof (struct struct_field_meta_node_t));

    node->node.hash = kan_hash_combine (kan_hash_combine ((uint64_t) struct_name, (uint64_t) struct_field_name),
                                        (uint64_t) meta_type_name);
    node->struct_name = struct_name;
    node->struct_field_name = struct_field_name;
    node->meta_type_name = meta_type_name;
    node->meta = meta;

    kan_hash_storage_update_bucket_count_default (&registry_struct->struct_field_meta_storage,
                                                  KAN_REFLECTION_STRUCT_FIELD_META_INITIAL_BUCKETS);
    kan_hash_storage_add (&registry_struct->struct_field_meta_storage, &node->node);
}

#if defined(KAN_REFLECTION_WITH_VALIDATION) && defined(KAN_WITH_ASSERT)
static inline void reflection_function_validate_archetype (enum kan_reflection_archetype_t archetype,
                                                           uint64_t size,
                                                           kan_bool_t return_type)
{
    switch (archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        KAN_ASSERT (size == sizeof (int8_t) || size == sizeof (int16_t) || size == sizeof (int32_t) ||
                    size == sizeof (int64_t) || (return_type && size == 0u))
        break;

    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        KAN_ASSERT (size == sizeof (uint8_t) || size == sizeof (uint16_t) || size == sizeof (uint32_t) ||
                    size == sizeof (uint64_t))
        break;

    case KAN_REFLECTION_ARCHETYPE_FLOATING:
        KAN_ASSERT (size == sizeof (float) || size == sizeof (double))
        break;

    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        KAN_ASSERT (size == sizeof (char *))
        break;

    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        KAN_ASSERT (size == sizeof (kan_interned_string_t))
        break;

    case KAN_REFLECTION_ARCHETYPE_ENUM:
        KAN_ASSERT (size == sizeof (int))
        break;

    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
    case KAN_REFLECTION_ARCHETYPE_STRUCT:
    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
    case KAN_REFLECTION_ARCHETYPE_PATCH:
        break;

    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        // Not supported as function argument or return type.
        KAN_ASSERT (KAN_FALSE)
        break;
    }
}
#endif

kan_bool_t kan_reflection_registry_add_function (kan_reflection_registry_t registry,
                                                 const struct kan_reflection_function_t *function_reflection)
{
    if (kan_reflection_registry_query_function (registry, function_reflection->name))
    {
        return KAN_FALSE;
    }

#if defined(KAN_REFLECTION_WITH_VALIDATION) && defined(KAN_WITH_ASSERT)
    KAN_ASSERT (function_reflection->call)
    reflection_function_validate_archetype (function_reflection->return_type.archetype,
                                            function_reflection->return_type.size, KAN_TRUE);

    for (uint64_t index = 0u; index < function_reflection->arguments_count; ++index)
    {
        const struct kan_reflection_argument_t *argument_reflection = &function_reflection->arguments[index];
        KAN_ASSERT (argument_reflection->name)
        KAN_ASSERT (argument_reflection->size > 0u)
        reflection_function_validate_archetype (argument_reflection->archetype, argument_reflection->size, KAN_FALSE);
    }
#endif

    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct function_node_t *node = (struct function_node_t *) kan_allocate_batched (registry_struct->allocation_group,
                                                                                    sizeof (struct function_node_t));
    node->node.hash = (uint64_t) function_reflection->name;
    node->function_reflection = function_reflection;

    kan_hash_storage_update_bucket_count_default (&registry_struct->function_storage,
                                                  KAN_REFLECTION_FUNCTION_INITIAL_BUCKETS);
    kan_hash_storage_add (&registry_struct->function_storage, &node->node);
    return KAN_TRUE;
}

void kan_reflection_registry_add_function_meta (kan_reflection_registry_t registry,
                                                kan_interned_string_t function_name,
                                                kan_interned_string_t meta_type_name,
                                                const void *meta)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct function_meta_node_t *node = (struct function_meta_node_t *) kan_allocate_batched (
        registry_struct->allocation_group, sizeof (struct function_meta_node_t));

    node->node.hash = kan_hash_combine ((uint64_t) function_name, (uint64_t) meta_type_name);
    node->function_name = function_name;
    node->meta_type_name = meta_type_name;
    node->meta = meta;

    kan_hash_storage_update_bucket_count_default (&registry_struct->function_meta_storage,
                                                  KAN_REFLECTION_FUNCTION_META_INITIAL_BUCKETS);
    kan_hash_storage_add (&registry_struct->function_meta_storage, &node->node);
}

void kan_reflection_registry_add_function_argument_meta (kan_reflection_registry_t registry,
                                                         kan_interned_string_t function_name,
                                                         kan_interned_string_t function_argument_name,
                                                         kan_interned_string_t meta_type_name,
                                                         const void *meta)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct function_argument_meta_node_t *node = (struct function_argument_meta_node_t *) kan_allocate_batched (
        registry_struct->allocation_group, sizeof (struct function_argument_meta_node_t));

    node->node.hash = kan_hash_combine (kan_hash_combine ((uint64_t) function_name, (uint64_t) function_argument_name),
                                        (uint64_t) meta_type_name);
    node->function_name = function_name;
    node->function_argument_name = function_argument_name;
    node->meta_type_name = meta_type_name;
    node->meta = meta;

    kan_hash_storage_update_bucket_count_default (&registry_struct->function_argument_meta_storage,
                                                  KAN_REFLECTION_FUNCTION_ARGUMENT_META_INITIAL_BUCKETS);
    kan_hash_storage_add (&registry_struct->function_argument_meta_storage, &node->node);
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

struct kan_reflection_enum_meta_iterator_t kan_reflection_registry_query_enum_meta (
    kan_reflection_registry_t registry, kan_interned_string_t enum_name, kan_interned_string_t meta_type_name)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    const uint64_t hash = kan_hash_combine ((uint64_t) enum_name, (uint64_t) meta_type_name);
    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (&registry_struct->enum_meta_storage, hash);

    struct enum_meta_iterator_t iterator = {
        .current = (struct enum_meta_node_t *) bucket->first,
        .end = (struct enum_meta_node_t *) (bucket->last ? bucket->last->next : NULL),
        .enum_name = enum_name,
        .meta_type_name = meta_type_name,
    };

    while (iterator.current != iterator.end)
    {
        if (iterator.current->enum_name == enum_name && iterator.current->meta_type_name == meta_type_name)
        {
            return KAN_PUN_TYPE (struct enum_meta_iterator_t, struct kan_reflection_enum_meta_iterator_t, iterator);
        }

        iterator.current = (struct enum_meta_node_t *) iterator.current->node.list_node.next;
    }

    return KAN_PUN_TYPE (struct enum_meta_iterator_t, struct kan_reflection_enum_meta_iterator_t, iterator);
}

const void *kan_reflection_enum_meta_iterator_get (struct kan_reflection_enum_meta_iterator_t *iterator)
{
    struct enum_meta_node_t *node = ((struct enum_meta_iterator_t *) iterator)->current;
    return node && node != ((struct enum_meta_iterator_t *) iterator)->end ? node->meta : NULL;
}

void kan_reflection_enum_meta_iterator_next (struct kan_reflection_enum_meta_iterator_t *iterator)
{
    struct enum_meta_iterator_t *data = (struct enum_meta_iterator_t *) iterator;
    if (data->current == data->end)
    {
        return;
    }

    do
    {
        data->current = (struct enum_meta_node_t *) data->current->node.list_node.next;
    } while (data->current != data->end &&
             (data->current->enum_name != data->enum_name || data->current->meta_type_name != data->meta_type_name));
}

struct kan_reflection_enum_value_meta_iterator_t kan_reflection_registry_query_enum_value_meta (
    kan_reflection_registry_t registry,
    kan_interned_string_t enum_name,
    kan_interned_string_t enum_value_name,
    kan_interned_string_t meta_type_name)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    const uint64_t hash = kan_hash_combine (kan_hash_combine ((uint64_t) enum_name, (uint64_t) enum_value_name),
                                            (uint64_t) meta_type_name);
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&registry_struct->enum_value_meta_storage, hash);

    struct enum_value_meta_iterator_t iterator = {
        .current = (struct enum_value_meta_node_t *) bucket->first,
        .end = (struct enum_value_meta_node_t *) (bucket->last ? bucket->last->next : NULL),
        .enum_name = enum_name,
        .enum_value_name = enum_value_name,
        .meta_type_name = meta_type_name,
    };

    while (iterator.current != iterator.end)
    {
        if (iterator.current->enum_name == enum_name && iterator.current->enum_value_name == enum_value_name &&
            iterator.current->meta_type_name == meta_type_name)
        {
            return KAN_PUN_TYPE (struct enum_value_meta_iterator_t, struct kan_reflection_enum_value_meta_iterator_t,
                                 iterator);
        }

        iterator.current = (struct enum_value_meta_node_t *) iterator.current->node.list_node.next;
    }

    return KAN_PUN_TYPE (struct enum_value_meta_iterator_t, struct kan_reflection_enum_value_meta_iterator_t, iterator);
}

const void *kan_reflection_enum_value_meta_iterator_get (struct kan_reflection_enum_value_meta_iterator_t *iterator)
{
    struct enum_value_meta_node_t *node = ((struct enum_value_meta_iterator_t *) iterator)->current;
    return node && node != ((struct enum_value_meta_iterator_t *) iterator)->end ? node->meta : NULL;
}

void kan_reflection_enum_value_meta_iterator_next (struct kan_reflection_enum_value_meta_iterator_t *iterator)
{
    struct enum_value_meta_iterator_t *data = (struct enum_value_meta_iterator_t *) iterator;
    if (data->current == data->end)
    {
        return;
    }

    do
    {
        data->current = (struct enum_value_meta_node_t *) data->current->node.list_node.next;
    } while (data->current != data->end &&
             (data->current->enum_name != data->enum_name || data->current->enum_value_name != data->enum_value_name ||
              data->current->meta_type_name != data->meta_type_name));
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

struct kan_reflection_struct_meta_iterator_t kan_reflection_registry_query_struct_meta (
    kan_reflection_registry_t registry, kan_interned_string_t struct_name, kan_interned_string_t meta_type_name)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    const uint64_t hash = kan_hash_combine ((uint64_t) struct_name, (uint64_t) meta_type_name);
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&registry_struct->struct_meta_storage, hash);

    struct struct_meta_iterator_t iterator = {
        .current = (struct struct_meta_node_t *) bucket->first,
        .end = (struct struct_meta_node_t *) (bucket->last ? bucket->last->next : NULL),
        .struct_name = struct_name,
        .meta_type_name = meta_type_name,
    };

    while (iterator.current != iterator.end)
    {
        if (iterator.current->struct_name == struct_name && iterator.current->meta_type_name == meta_type_name)
        {
            return KAN_PUN_TYPE (struct struct_meta_iterator_t, struct kan_reflection_struct_meta_iterator_t, iterator);
        }

        iterator.current = (struct struct_meta_node_t *) iterator.current->node.list_node.next;
    }

    return KAN_PUN_TYPE (struct struct_meta_iterator_t, struct kan_reflection_struct_meta_iterator_t, iterator);
}

const void *kan_reflection_struct_meta_iterator_get (struct kan_reflection_struct_meta_iterator_t *iterator)
{
    struct struct_meta_node_t *node = ((struct struct_meta_iterator_t *) iterator)->current;
    return node && node != ((struct struct_meta_iterator_t *) iterator)->end ? node->meta : NULL;
}

void kan_reflection_struct_meta_iterator_next (struct kan_reflection_struct_meta_iterator_t *iterator)
{
    struct struct_meta_iterator_t *data = (struct struct_meta_iterator_t *) iterator;
    if (data->current == data->end)
    {
        return;
    }

    do
    {
        data->current = (struct struct_meta_node_t *) data->current->node.list_node.next;
    } while (data->current != data->end && (data->current->struct_name != data->struct_name ||
                                            data->current->meta_type_name != data->meta_type_name));
}

struct kan_reflection_struct_field_meta_iterator_t kan_reflection_registry_query_struct_field_meta (
    kan_reflection_registry_t registry,
    kan_interned_string_t struct_name,
    kan_interned_string_t struct_field_name,
    kan_interned_string_t meta_type_name)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    const uint64_t hash = kan_hash_combine (kan_hash_combine ((uint64_t) struct_name, (uint64_t) struct_field_name),
                                            (uint64_t) meta_type_name);
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&registry_struct->struct_field_meta_storage, hash);

    struct struct_field_meta_iterator_t iterator = {
        .current = (struct struct_field_meta_node_t *) bucket->first,
        .end = (struct struct_field_meta_node_t *) (bucket->last ? bucket->last->next : NULL),
        .struct_name = struct_name,
        .struct_field_name = struct_field_name,
        .meta_type_name = meta_type_name,
    };

    while (iterator.current != iterator.end)
    {
        if (iterator.current->struct_name == struct_name && iterator.current->struct_field_name == struct_field_name &&
            iterator.current->meta_type_name == meta_type_name)
        {
            return KAN_PUN_TYPE (struct struct_field_meta_iterator_t,
                                 struct kan_reflection_struct_field_meta_iterator_t, iterator);
        }

        iterator.current = (struct struct_field_meta_node_t *) iterator.current->node.list_node.next;
    }

    return KAN_PUN_TYPE (struct struct_field_meta_iterator_t, struct kan_reflection_struct_field_meta_iterator_t,
                         iterator);
}

const void *kan_reflection_struct_field_meta_iterator_get (struct kan_reflection_struct_field_meta_iterator_t *iterator)
{
    struct struct_field_meta_node_t *node = ((struct struct_field_meta_iterator_t *) iterator)->current;
    return node && node != ((struct struct_field_meta_iterator_t *) iterator)->end ? node->meta : NULL;
}

void kan_reflection_struct_field_meta_iterator_next (struct kan_reflection_struct_field_meta_iterator_t *iterator)
{
    struct struct_field_meta_iterator_t *data = (struct struct_field_meta_iterator_t *) iterator;
    if (data->current == data->end)
    {
        return;
    }

    do
    {
        data->current = (struct struct_field_meta_node_t *) data->current->node.list_node.next;
    } while (data->current != data->end && (data->current->struct_name != data->struct_name ||
                                            data->current->struct_field_name != data->struct_field_name ||
                                            data->current->meta_type_name != data->meta_type_name));
}

const struct kan_reflection_function_t *kan_reflection_registry_query_function (kan_reflection_registry_t registry,
                                                                                kan_interned_string_t function_name)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&registry_struct->function_storage, (uint64_t) function_name);
    struct function_node_t *node = (struct function_node_t *) bucket->first;
    const struct function_node_t *end = (struct function_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->function_reflection->name == function_name)
        {
            return node->function_reflection;
        }

        node = (struct function_node_t *) node->node.list_node.next;
    }

    return NULL;
}

struct kan_reflection_function_meta_iterator_t kan_reflection_registry_query_function_meta (
    kan_reflection_registry_t registry, kan_interned_string_t function_name, kan_interned_string_t meta_type_name)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    const uint64_t hash = kan_hash_combine ((uint64_t) function_name, (uint64_t) meta_type_name);
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&registry_struct->function_meta_storage, hash);

    struct function_meta_iterator_t iterator = {
        .current = (struct function_meta_node_t *) bucket->first,
        .end = (struct function_meta_node_t *) (bucket->last ? bucket->last->next : NULL),
        .function_name = function_name,
        .meta_type_name = meta_type_name,
    };

    while (iterator.current != iterator.end)
    {
        if (iterator.current->function_name == function_name && iterator.current->meta_type_name == meta_type_name)
        {
            return KAN_PUN_TYPE (struct function_meta_iterator_t, struct kan_reflection_function_meta_iterator_t,
                                 iterator);
        }

        iterator.current = (struct function_meta_node_t *) iterator.current->node.list_node.next;
    }

    return KAN_PUN_TYPE (struct function_meta_iterator_t, struct kan_reflection_function_meta_iterator_t, iterator);
}

const void *kan_reflection_function_meta_iterator_get (struct kan_reflection_function_meta_iterator_t *iterator)
{
    struct function_meta_node_t *node = ((struct function_meta_iterator_t *) iterator)->current;
    return node && node != ((struct function_meta_iterator_t *) iterator)->end ? node->meta : NULL;
}

void kan_reflection_function_meta_iterator_next (struct kan_reflection_function_meta_iterator_t *iterator)
{
    struct function_meta_iterator_t *data = (struct function_meta_iterator_t *) iterator;
    if (data->current == data->end)
    {
        return;
    }

    do
    {
        data->current = (struct function_meta_node_t *) data->current->node.list_node.next;
    } while (data->current != data->end && (data->current->function_name != data->function_name ||
                                            data->current->meta_type_name != data->meta_type_name));
}

struct kan_reflection_function_argument_meta_iterator_t kan_reflection_registry_query_function_argument_meta (
    kan_reflection_registry_t registry,
    kan_interned_string_t function_name,
    kan_interned_string_t function_argument_name,
    kan_interned_string_t meta_type_name)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    const uint64_t hash = kan_hash_combine (
        kan_hash_combine ((uint64_t) function_name, (uint64_t) function_argument_name), (uint64_t) meta_type_name);
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&registry_struct->function_argument_meta_storage, hash);

    struct function_argument_meta_iterator_t iterator = {
        .current = (struct function_argument_meta_node_t *) bucket->first,
        .end = (struct function_argument_meta_node_t *) (bucket->last ? bucket->last->next : NULL),
        .function_name = function_name,
        .function_argument_name = function_argument_name,
        .meta_type_name = meta_type_name,
    };

    while (iterator.current != iterator.end)
    {
        if (iterator.current->function_name == function_name &&
            iterator.current->function_argument_name == function_argument_name &&
            iterator.current->meta_type_name == meta_type_name)
        {
            return KAN_PUN_TYPE (struct function_argument_meta_iterator_t,
                                 struct kan_reflection_function_argument_meta_iterator_t, iterator);
        }

        iterator.current = (struct function_argument_meta_node_t *) iterator.current->node.list_node.next;
    }

    return KAN_PUN_TYPE (struct function_argument_meta_iterator_t,
                         struct kan_reflection_function_argument_meta_iterator_t, iterator);
}

const void *kan_reflection_function_argument_meta_iterator_get (
    struct kan_reflection_function_argument_meta_iterator_t *iterator)
{
    struct function_argument_meta_node_t *node = ((struct function_argument_meta_iterator_t *) iterator)->current;
    return node && node != ((struct function_argument_meta_iterator_t *) iterator)->end ? node->meta : NULL;
}

void kan_reflection_function_argument_meta_iterator_next (
    struct kan_reflection_function_argument_meta_iterator_t *iterator)
{
    struct function_argument_meta_iterator_t *data = (struct function_argument_meta_iterator_t *) iterator;
    if (data->current == data->end)
    {
        return;
    }

    do
    {
        data->current = (struct function_argument_meta_node_t *) data->current->node.list_node.next;
    } while (data->current != data->end && (data->current->function_name != data->function_name ||
                                            data->current->function_argument_name != data->function_argument_name ||
                                            data->current->meta_type_name != data->meta_type_name));
}

const struct kan_reflection_field_t *kan_reflection_registry_query_local_field (kan_reflection_registry_t registry,
                                                                                kan_interned_string_t struct_name,
                                                                                uint64_t path_length,
                                                                                kan_interned_string_t *path,
                                                                                uint64_t *absolute_offset_output,
                                                                                uint64_t *size_with_padding_output)

{
    KAN_ASSERT (path_length > 0u)
    KAN_ASSERT (absolute_offset_output)
    KAN_ASSERT (size_with_padding_output)
    *absolute_offset_output = 0u;
    const struct kan_reflection_struct_t *struct_reflection =
        kan_reflection_registry_query_struct (registry, struct_name);

    if (!struct_reflection)
    {
        KAN_LOG (reflection_registry, KAN_LOG_WARNING, "Struct \"%s\" is not registered.", path[0u])
        return NULL;
    }

    *size_with_padding_output = struct_reflection->size;
    const struct kan_reflection_field_t *field_reflection = NULL;

    for (uint64_t path_element_index = 0u; path_element_index < path_length; ++path_element_index)
    {
        kan_interned_string_t path_element = path[path_element_index];
        field_reflection = NULL;
        uint64_t field_index;

        for (field_index = 0u; field_index < struct_reflection->fields_count; ++field_index)
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

        // Calculate size with padding.
        uint64_t element_size_with_padding = 0u;

        // Find next field in layout.
        for (field_index = field_index + 1u; field_index < struct_reflection->fields_count; ++field_index)
        {
            const struct kan_reflection_field_t *field_reflection_to_check = &struct_reflection->fields[field_index];

            // We skip fields with the same offset, because they are most likely part of the union and therefore
            // cannot be used for full size calculation in layout.
            if (field_reflection_to_check->offset > field_reflection->offset)
            {
                element_size_with_padding = field_reflection_to_check->offset - field_reflection->offset;
                break;
            }
        }

        // No next field, padding is described by structure size and its field padding.
        if (element_size_with_padding == 0u)
        {
            element_size_with_padding = *size_with_padding_output - field_reflection->offset;
        }

        *size_with_padding_output = element_size_with_padding;
        KAN_ASSERT (*size_with_padding_output >= field_reflection->size)
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

            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
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

kan_reflection_registry_function_iterator_t kan_reflection_registry_function_iterator_create (
    kan_reflection_registry_t registry)
{
    struct registry_t *registry_struct = (struct registry_t *) registry;
    return (kan_reflection_registry_function_iterator_t) registry_struct->function_storage.items.first;
}

const struct kan_reflection_function_t *kan_reflection_registry_function_iterator_get (
    kan_reflection_registry_function_iterator_t iterator)
{
    const struct function_node_t *node = (struct function_node_t *) iterator;
    if (node)
    {
        return node->function_reflection;
    }

    return NULL;
}

kan_reflection_registry_function_iterator_t kan_reflection_registry_function_iterator_next (
    kan_reflection_registry_function_iterator_t iterator)
{
    const struct function_node_t *node = (struct function_node_t *) iterator;
    if (node)
    {
        return (kan_reflection_registry_function_iterator_t) node->node.list_node.next;
    }

    return (kan_reflection_registry_function_iterator_t) NULL;
}

static void compiled_patch_destroy (struct compiled_patch_t *patch)
{
    const kan_allocation_group_t group = get_compiled_patch_allocation_group ();
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

    node = registry_struct->function_storage.items.first;
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

    node = registry_struct->function_meta_storage.items.first;
    while (node)
    {
        struct kan_bd_list_node_t *next = node->next;
        kan_free_batched (registry_struct->allocation_group, node);
        node = next;
    }

    node = registry_struct->function_argument_meta_storage.items.first;
    while (node)
    {
        struct kan_bd_list_node_t *next = node->next;
        kan_free_batched (registry_struct->allocation_group, node);
        node = next;
    }

    kan_hash_storage_shutdown (&registry_struct->enum_storage);
    kan_hash_storage_shutdown (&registry_struct->struct_storage);
    kan_hash_storage_shutdown (&registry_struct->function_storage);
    kan_hash_storage_shutdown (&registry_struct->enum_meta_storage);
    kan_hash_storage_shutdown (&registry_struct->enum_value_meta_storage);
    kan_hash_storage_shutdown (&registry_struct->struct_meta_storage);
    kan_hash_storage_shutdown (&registry_struct->struct_field_meta_storage);
    kan_hash_storage_shutdown (&registry_struct->function_meta_storage);
    kan_hash_storage_shutdown (&registry_struct->function_argument_meta_storage);

    kan_free_batched (registry_struct->allocation_group, registry_struct);
}

static kan_bool_t patch_builder_allocation_group_ready = KAN_FALSE;
static kan_allocation_group_t patch_builder_allocation_group;

static kan_allocation_group_t get_patch_builder_allocation_group (void)
{
    if (!patch_builder_allocation_group_ready)
    {
        patch_builder_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "reflection_patch_builder");
        patch_builder_allocation_group_ready = KAN_TRUE;
    }

    return patch_builder_allocation_group;
}

kan_reflection_patch_builder_t kan_reflection_patch_builder_create (void)
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
                                             const void *data)
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
            {
                const struct kan_reflection_struct_t *element_type = kan_reflection_registry_query_struct (
                    registry, field->archetype_inline_array.item_archetype_struct.type_name);

                for (uint64_t element_index = 0u; element_index < field->archetype_inline_array.item_count;
                     ++element_index)
                {
                    validate_compiled_node_internal (node, registry, element_type,
                                                     field->offset + element_index * element_type->size);
                }

                break;
            }

            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
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
        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
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

static kan_bool_t compiled_patch_build_into (struct patch_builder_t *patch_builder,
                                             struct registry_t *registry_struct,
                                             const struct kan_reflection_struct_t *type,
                                             struct compiled_patch_t *output_patch)
{
    struct patch_builder_node_t **nodes_array = (struct patch_builder_node_t **) kan_stack_allocator_allocate (
        patch_builder->stack_allocator, patch_builder->node_count * sizeof (struct patch_builder_node_t *),
        _Alignof (struct patch_builder_node_t *));
    KAN_ASSERT (nodes_array)

    if (!nodes_array)
    {
        patch_builder_reset (patch_builder);
        KAN_LOG (reflection_patch_builder, KAN_LOG_ERROR, "Patch builder memory buffer overflow.")
        return KAN_FALSE;
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
    uint64_t node_count = 0u;

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
                return KAN_FALSE;
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
            patch_data_size = kan_apply_alignment (patch_data_size, _Alignof (struct compiled_patch_node_t));
            patch_data_size += sizeof (struct compiled_patch_node_t);
            ++node_count;
        }

        patch_data_size += nodes_array[index]->size;
    }

    patch_data_size = kan_apply_alignment (patch_data_size, _Alignof (struct compiled_patch_node_t));
    output_patch->type = type;
    output_patch->node_count = node_count;
    output_patch->begin = kan_allocate_general (get_compiled_patch_allocation_group (), patch_data_size,
                                                _Alignof (struct compiled_patch_node_t));
    output_patch->end = (struct compiled_patch_node_t *) (((uint8_t *) output_patch->begin) + patch_data_size);

    output_patch->registry = registry_struct;
    output_patch->previous = NULL;
    output_patch->next = registry_struct->first_patch;

    kan_atomic_int_lock (&registry_struct->patch_addition_lock);
    if (registry_struct->first_patch)
    {
        registry_struct->first_patch->previous = output_patch;
    }

    registry_struct->first_patch = output_patch;
    kan_atomic_int_unlock (&registry_struct->patch_addition_lock);

    uint8_t *output = (uint8_t *) output_patch->begin;
    struct compiled_patch_node_t *output_node = NULL;

    for (uint64_t index = 0u; index < patch_builder->node_count; ++index)
    {
        if (index == 0u ||
            nodes_array[index - 1u]->offset + nodes_array[index - 1u]->size != nodes_array[index]->offset)
        {
            output = (uint8_t *) kan_apply_alignment ((uint64_t) output, _Alignof (struct compiled_patch_node_t));
#if defined(KAN_REFLECTION_WITH_VALIDATION) && defined(KAN_WITH_ASSERT)
            validate_compiled_node (output_node, (kan_reflection_registry_t) registry_struct, type);
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
    validate_compiled_node (output_node, (kan_reflection_registry_t) registry_struct, type);
#endif

#if defined(KAN_WITH_ASSERT)
    output = (uint8_t *) kan_apply_alignment ((uint64_t) output, _Alignof (struct compiled_patch_node_t));
    KAN_ASSERT ((struct compiled_patch_node_t *) output == output_patch->end)
#endif

    patch_builder_reset (patch_builder);
    return KAN_TRUE;
}

kan_reflection_patch_t kan_reflection_patch_builder_build (kan_reflection_patch_builder_t builder,
                                                           kan_reflection_registry_t registry,
                                                           const struct kan_reflection_struct_t *type)
{
    struct patch_builder_t *patch_builder = (struct patch_builder_t *) builder;
    struct registry_t *registry_struct = (struct registry_t *) registry;
    struct compiled_patch_t *patch =
        kan_allocate_batched (get_compiled_patch_allocation_group (), sizeof (struct compiled_patch_t));

    if (!compiled_patch_build_into (patch_builder, registry_struct, type, patch))
    {
        kan_free_batched (get_compiled_patch_allocation_group (), patch);
        return KAN_INVALID_REFLECTION_PATCH;
    }

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

uint64_t kan_reflection_patch_get_chunks_count (kan_reflection_patch_t patch)
{
    struct compiled_patch_t *patch_data = (struct compiled_patch_t *) patch;
    return patch_data->node_count;
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
        data_end = (uint8_t *) kan_apply_alignment ((uint64_t) data_end, _Alignof (struct compiled_patch_node_t));
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
    data_end = (uint8_t *) kan_apply_alignment ((uint64_t) data_end, _Alignof (struct compiled_patch_node_t));
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

static kan_bool_t migration_seed_allocation_group_ready = KAN_FALSE;
static kan_allocation_group_t migration_seed_allocation_group;

static kan_allocation_group_t get_migration_seed_allocation_group (void)
{
    if (!migration_seed_allocation_group_ready)
    {
        migration_seed_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "reflection_migration_seed");
        migration_seed_allocation_group_ready = KAN_TRUE;
    }

    return migration_seed_allocation_group;
}

static void migration_seed_add_enums (struct migration_seed_t *migration_seed,
                                      kan_reflection_registry_t source_registry,
                                      kan_reflection_registry_t target_registry)
{
    const kan_allocation_group_t allocation_group = get_migration_seed_allocation_group ();
    kan_reflection_registry_enum_iterator_t enum_iterator =
        kan_reflection_registry_enum_iterator_create (source_registry);
    const struct kan_reflection_enum_t *source_enum_data;

    while ((source_enum_data = kan_reflection_registry_enum_iterator_get (enum_iterator)))
    {
        const uint64_t node_size = sizeof (struct enum_migration_node_t) +
                                   sizeof (struct kan_reflection_enum_value_t *) * source_enum_data->values_count;
        struct enum_migration_node_t *node =
            kan_allocate_general (allocation_group, node_size, _Alignof (struct enum_migration_node_t));

        node->node.hash = (uint64_t) source_enum_data->name;
        node->type_name = source_enum_data->name;

        const struct kan_reflection_enum_t *target_enum_data =
            kan_reflection_registry_query_enum (target_registry, source_enum_data->name);

        if (target_enum_data)
        {
            node->seed.status = source_enum_data->flags == target_enum_data->flags ?
                                    KAN_REFLECTION_MIGRATION_NOT_NEEDED :
                                    KAN_REFLECTION_MIGRATION_NEEDED;

            for (uint64_t source_value_index = 0u; source_value_index < source_enum_data->values_count;
                 ++source_value_index)
            {
                const struct kan_reflection_enum_value_t *source_value = &source_enum_data->values[source_value_index];
                node->seed.value_remap[source_value_index] = NULL;

                for (uint64_t target_value_index = 0u; target_value_index < target_enum_data->values_count;
                     ++target_value_index)
                {
                    const struct kan_reflection_enum_value_t *target_value =
                        &target_enum_data->values[target_value_index];

                    if (target_value->name == source_value->name)
                    {
                        node->seed.value_remap[source_value_index] = target_value;
                        if (source_value->value != target_value->value)
                        {
                            node->seed.status = KAN_REFLECTION_MIGRATION_NEEDED;
                        }

                        break;
                    }
                }

                if (!node->seed.value_remap[source_value_index])
                {
                    KAN_LOG (reflection_migration_seed, KAN_LOG_WARNING,
                             "Failed to find replacement target for value \"%s\" of enum \"%s\". Will be replaced by "
                             "\"%s\".",
                             source_value->name, source_enum_data->name, target_enum_data->values[0].name)
                    node->seed.value_remap[source_value_index] = &target_enum_data->values[0];
                }
            }
        }
        else
        {
            node->seed.status = KAN_REFLECTION_MIGRATION_REMOVED;
        }

        kan_hash_storage_update_bucket_count_default (&migration_seed->enums,
                                                      KAN_REFLECTION_MIGRATION_ENUM_SEED_INITIAL_BUCKETS);
        kan_hash_storage_add (&migration_seed->enums, &node->node);
        enum_iterator = kan_reflection_registry_enum_iterator_next (enum_iterator);
    }
}

static struct enum_migration_node_t *migration_seed_query_enum (struct migration_seed_t *migration_seed,
                                                                kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&migration_seed->enums, (uint64_t) type_name);

    struct enum_migration_node_t *node = (struct enum_migration_node_t *) bucket->first;
    const struct enum_migration_node_t *end =
        (struct enum_migration_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->type_name == type_name)
        {
            return node;
        }

        node = (struct enum_migration_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static struct struct_migration_node_t *migration_seed_query_struct (struct migration_seed_t *migration_seed,
                                                                    kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&migration_seed->structs, (uint64_t) type_name);

    struct struct_migration_node_t *node = (struct struct_migration_node_t *) bucket->first;
    const struct struct_migration_node_t *end =
        (struct struct_migration_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->type_name == type_name)
        {
            return node;
        }

        node = (struct struct_migration_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static inline kan_bool_t check_is_enum_mappable (struct migration_seed_t *migration_seed,
                                                 kan_interned_string_t source_type_name,
                                                 kan_interned_string_t target_type_name,
                                                 enum kan_reflection_migration_status_t *status_output)
{
    kan_bool_t mappable = source_type_name == target_type_name;
    if (mappable)
    {
        struct enum_migration_node_t *enum_node = migration_seed_query_enum (migration_seed, source_type_name);

        if (enum_node)
        {
            switch (enum_node->seed.status)
            {
            case KAN_REFLECTION_MIGRATION_NEEDED:
                *status_output = KAN_REFLECTION_MIGRATION_NEEDED;
                break;

            case KAN_REFLECTION_MIGRATION_NOT_NEEDED:
                break;

            case KAN_REFLECTION_MIGRATION_REMOVED:
                KAN_LOG (reflection_migration_seed, KAN_LOG_ERROR,
                         "Enum \"%s\" is marked removed in migration, but still found "
                         "in target type.",
                         source_type_name)
                break;
            }
        }
        else
        {
            KAN_LOG (reflection_migration_seed, KAN_LOG_ERROR, "Unable to find migrated enum \"%s\".", source_type_name)
        }
    }

    return mappable;
}

static struct struct_migration_node_t *migration_seed_request_struct (struct migration_seed_t *migration_seed,
                                                                      kan_interned_string_t type_name,
                                                                      kan_reflection_registry_t source_registry,
                                                                      kan_reflection_registry_t target_registry);

static inline kan_bool_t check_is_struct_mappable (struct migration_seed_t *migration_seed,
                                                   kan_interned_string_t source_type_name,
                                                   kan_interned_string_t target_type_name,
                                                   kan_reflection_registry_t source_registry,
                                                   kan_reflection_registry_t target_registry,
                                                   enum kan_reflection_migration_status_t *status_output)
{
    kan_bool_t mappable = source_type_name == target_type_name;
    if (mappable)
    {
        struct struct_migration_node_t *nested =
            migration_seed_request_struct (migration_seed, source_type_name, source_registry, target_registry);

        switch (nested->seed.status)
        {
        case KAN_REFLECTION_MIGRATION_NEEDED:
            *status_output = KAN_REFLECTION_MIGRATION_NEEDED;
            break;

        case KAN_REFLECTION_MIGRATION_NOT_NEEDED:
            break;

        case KAN_REFLECTION_MIGRATION_REMOVED:
            KAN_LOG (reflection_migration_seed, KAN_LOG_ERROR,
                     "Struct \"%s\" is marked removed in migration, but still found "
                     "in target type.",
                     source_type_name);
            break;
        }
    }

    return mappable;
}

static struct struct_migration_node_t *migration_seed_add_struct (
    struct migration_seed_t *migration_seed,
    const struct kan_reflection_struct_t *source_struct_data,
    kan_reflection_registry_t source_registry,
    kan_reflection_registry_t target_registry)
{
    const kan_allocation_group_t allocation_group = get_migration_seed_allocation_group ();
    const uint64_t node_size = sizeof (struct struct_migration_node_t) +
                               sizeof (struct kan_reflection_field_t *) * source_struct_data->fields_count;
    struct struct_migration_node_t *node =
        kan_allocate_general (allocation_group, node_size, _Alignof (struct struct_migration_node_t));

    node->node.hash = (uint64_t) source_struct_data->name;
    node->type_name = source_struct_data->name;
    const struct kan_reflection_struct_t *target_struct_data =
        kan_reflection_registry_query_struct (target_registry, source_struct_data->name);

    if (target_struct_data)
    {
        node->seed.status = source_struct_data->size == target_struct_data->size &&
                                    source_struct_data->alignment == target_struct_data->alignment &&
                                    source_struct_data->fields_count == target_struct_data->fields_count ?
                                KAN_REFLECTION_MIGRATION_NOT_NEEDED :
                                KAN_REFLECTION_MIGRATION_NEEDED;

        for (uint64_t source_field_index = 0u; source_field_index < source_struct_data->fields_count;
             ++source_field_index)
        {
            const struct kan_reflection_field_t *source_field = &source_struct_data->fields[source_field_index];
            node->seed.field_remap[source_field_index] = NULL;

            for (uint64_t target_field_index = 0u; target_field_index < target_struct_data->fields_count;
                 ++target_field_index)
            {
                const struct kan_reflection_field_t *target_field = &target_struct_data->fields[target_field_index];
                if (target_field->name == source_field->name)
                {
                    kan_bool_t mappable = KAN_FALSE;
                    // TODO: Adaption between numeric primitives (uint, int, float).

                    if (source_field->archetype == target_field->archetype)
                    {
                        switch (source_field->archetype)
                        {
                        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                        case KAN_REFLECTION_ARCHETYPE_FLOATING:
                        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
                        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                        case KAN_REFLECTION_ARCHETYPE_PATCH:
                            mappable = KAN_TRUE;
                            break;

                        case KAN_REFLECTION_ARCHETYPE_ENUM:
                            mappable =
                                check_is_enum_mappable (migration_seed, source_field->archetype_enum.type_name,
                                                        target_field->archetype_enum.type_name, &node->seed.status);
                            break;

                        case KAN_REFLECTION_ARCHETYPE_STRUCT:
                            mappable =
                                check_is_struct_mappable (migration_seed, source_field->archetype_struct.type_name,
                                                          target_field->archetype_struct.type_name, source_registry,
                                                          target_registry, &node->seed.status);
                            break;

                        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                            if (source_field->archetype_inline_array.item_archetype ==
                                target_field->archetype_inline_array.item_archetype)
                            {
                                switch (source_field->archetype_inline_array.item_archetype)
                                {
                                case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                                case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                                case KAN_REFLECTION_ARCHETYPE_FLOATING:
                                case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
                                case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                                case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                                case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                                case KAN_REFLECTION_ARCHETYPE_PATCH:
                                    mappable = KAN_TRUE;
                                    break;

                                case KAN_REFLECTION_ARCHETYPE_ENUM:
                                    mappable = check_is_enum_mappable (
                                        migration_seed,
                                        source_field->archetype_inline_array.item_archetype_enum.type_name,
                                        target_field->archetype_inline_array.item_archetype_enum.type_name,
                                        &node->seed.status);
                                    break;

                                case KAN_REFLECTION_ARCHETYPE_STRUCT:
                                    mappable = check_is_struct_mappable (
                                        migration_seed,
                                        source_field->archetype_inline_array.item_archetype_struct.type_name,
                                        target_field->archetype_inline_array.item_archetype_struct.type_name,
                                        source_registry, target_registry, &node->seed.status);
                                    break;

                                case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                                case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                                    KAN_ASSERT (KAN_FALSE)
                                    break;
                                }
                            }

                            if (source_field->archetype_inline_array.item_size !=
                                    target_field->archetype_inline_array.item_size ||
                                source_field->archetype_inline_array.item_count !=
                                    target_field->archetype_inline_array.item_count)
                            {
                                node->seed.status = KAN_REFLECTION_MIGRATION_NEEDED;
                            }

                            break;

                        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                            if (source_field->archetype_dynamic_array.item_archetype ==
                                target_field->archetype_dynamic_array.item_archetype)
                            {
                                switch (source_field->archetype_dynamic_array.item_archetype)
                                {
                                case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                                case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                                case KAN_REFLECTION_ARCHETYPE_FLOATING:
                                case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
                                case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                                case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                                case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                                case KAN_REFLECTION_ARCHETYPE_PATCH:
                                    mappable = KAN_TRUE;
                                    break;

                                case KAN_REFLECTION_ARCHETYPE_ENUM:
                                    mappable = check_is_enum_mappable (
                                        migration_seed,
                                        source_field->archetype_dynamic_array.item_archetype_enum.type_name,
                                        target_field->archetype_dynamic_array.item_archetype_enum.type_name,
                                        &node->seed.status);
                                    break;

                                case KAN_REFLECTION_ARCHETYPE_STRUCT:
                                    if (source_field->archetype_dynamic_array.item_archetype_struct.type_name ==
                                            target_field->archetype_dynamic_array.item_archetype_struct.type_name &&
                                        target_field->archetype_dynamic_array.item_archetype_struct.type_name ==
                                            target_struct_data->name)
                                    {
                                        break;
                                    }

                                    mappable = check_is_struct_mappable (
                                        migration_seed,
                                        source_field->archetype_dynamic_array.item_archetype_struct.type_name,
                                        target_field->archetype_dynamic_array.item_archetype_struct.type_name,
                                        source_registry, target_registry, &node->seed.status);
                                    break;

                                case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                                case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                                    KAN_ASSERT (KAN_FALSE)
                                    break;
                                }
                            }

                            if (source_field->archetype_dynamic_array.item_size !=
                                target_field->archetype_dynamic_array.item_size)
                            {
                                node->seed.status = KAN_REFLECTION_MIGRATION_NEEDED;
                            }

                            break;
                        }
                    }

                    if (mappable)
                    {
                        node->seed.field_remap[source_field_index] = target_field;
                        if (source_field->size != target_field->size || source_field->offset != target_field->offset)
                        {
                            node->seed.status = KAN_REFLECTION_MIGRATION_NEEDED;
                        }
                    }
                    else
                    {
                        node->seed.field_remap[source_field_index] = NULL;
                        node->seed.status = KAN_REFLECTION_MIGRATION_NEEDED;
                    }

                    break;
                }
            }
        }
    }
    else
    {
        node->seed.status = KAN_REFLECTION_MIGRATION_REMOVED;
    }

    kan_hash_storage_update_bucket_count_default (&migration_seed->structs,
                                                  KAN_REFLECTION_MIGRATION_STRUCT_SEED_INITIAL_BUCKETS);
    kan_hash_storage_add (&migration_seed->structs, &node->node);
    return node;
}

static struct struct_migration_node_t *migration_seed_request_struct (struct migration_seed_t *migration_seed,
                                                                      kan_interned_string_t type_name,
                                                                      kan_reflection_registry_t source_registry,
                                                                      kan_reflection_registry_t target_registry)
{
    struct struct_migration_node_t *node = migration_seed_query_struct (migration_seed, type_name);
    if (!node)
    {
        const struct kan_reflection_struct_t *source_struct_data =
            kan_reflection_registry_query_struct (source_registry, type_name);

        if (!source_struct_data)
        {
            KAN_LOG (reflection_migration_seed, KAN_LOG_ERROR,
                     "Unable to find source struct \"%s\". Corrupted source registry?", type_name)
            KAN_ASSERT (KAN_FALSE)
            return NULL;
        }

        node = migration_seed_add_struct (migration_seed, source_struct_data, source_registry, target_registry);
    }

    return node;
}

static void migration_seed_add_structs (struct migration_seed_t *migration_seed,
                                        kan_reflection_registry_t source_registry,
                                        kan_reflection_registry_t target_registry)
{
    kan_reflection_registry_struct_iterator_t struct_iterator =
        kan_reflection_registry_struct_iterator_create (source_registry);
    const struct kan_reflection_struct_t *source_struct_data;

    while ((source_struct_data = kan_reflection_registry_struct_iterator_get (struct_iterator)))
    {
        if (migration_seed_query_struct (migration_seed, source_struct_data->name) == NULL)
        {
            migration_seed_add_struct (migration_seed, source_struct_data, source_registry, target_registry);
        }

        struct_iterator = kan_reflection_registry_struct_iterator_next (struct_iterator);
    }
}

kan_reflection_migration_seed_t kan_reflection_migration_seed_build (kan_reflection_registry_t source_registry,
                                                                     kan_reflection_registry_t target_registry)
{
    const kan_allocation_group_t allocation_group = get_migration_seed_allocation_group ();
    struct migration_seed_t *migration_seed =
        (struct migration_seed_t *) kan_allocate_batched (allocation_group, sizeof (struct migration_seed_t));
    migration_seed->source_registry = (struct registry_t *) source_registry;
    migration_seed->target_registry = (struct registry_t *) target_registry;

    kan_hash_storage_init (&migration_seed->enums, allocation_group,
                           KAN_REFLECTION_MIGRATION_ENUM_SEED_INITIAL_BUCKETS);
    kan_hash_storage_init (&migration_seed->structs, allocation_group,
                           KAN_REFLECTION_MIGRATION_STRUCT_SEED_INITIAL_BUCKETS);

    migration_seed_add_enums (migration_seed, source_registry, target_registry);
    migration_seed_add_structs (migration_seed, source_registry, target_registry);
    return (kan_reflection_migration_seed_t) migration_seed;
}

const struct kan_reflection_enum_migration_seed_t *kan_reflection_migration_seed_get_for_enum (
    kan_reflection_migration_seed_t seed, kan_interned_string_t type_name)
{
    struct migration_seed_t *migration_seed = (struct migration_seed_t *) seed;
    struct enum_migration_node_t *node = migration_seed_query_enum (migration_seed, type_name);
    return node ? &node->seed : NULL;
}

const struct kan_reflection_struct_migration_seed_t *kan_reflection_migration_seed_get_for_struct (
    kan_reflection_migration_seed_t seed, kan_interned_string_t type_name)
{
    struct migration_seed_t *migration_seed = (struct migration_seed_t *) seed;
    struct struct_migration_node_t *node = migration_seed_query_struct (migration_seed, type_name);
    return node ? &node->seed : NULL;
}

void kan_reflection_migration_seed_destroy (kan_reflection_migration_seed_t seed)
{
    const kan_allocation_group_t allocation_group = get_migration_seed_allocation_group ();
    struct migration_seed_t *migration_seed = (struct migration_seed_t *) seed;

    struct kan_bd_list_node_t *node = migration_seed->enums.items.first;
    while (node)
    {
        struct kan_bd_list_node_t *next = node->next;
        struct enum_migration_node_t *migration_node = (struct enum_migration_node_t *) node;
        const struct kan_reflection_enum_t *enum_data = kan_reflection_registry_query_enum (
            (kan_reflection_registry_t) migration_seed->source_registry, migration_node->type_name);
        KAN_ASSERT (enum_data)

        kan_free_general (allocation_group, node,
                          sizeof (struct enum_migration_node_t) +
                              sizeof (struct kan_reflection_enum_value_t *) * enum_data->values_count);
        node = next;
    }

    node = migration_seed->structs.items.first;
    while (node)
    {
        struct kan_bd_list_node_t *next = node->next;
        struct struct_migration_node_t *migration_node = (struct struct_migration_node_t *) node;
        const struct kan_reflection_struct_t *struct_data = kan_reflection_registry_query_struct (
            (kan_reflection_registry_t) migration_seed->source_registry, migration_node->type_name);
        KAN_ASSERT (struct_data)

        kan_free_general (allocation_group, node,
                          sizeof (struct struct_migration_node_t) +
                              sizeof (struct kan_reflection_field_t *) * struct_data->fields_count);
        node = next;
    }

    kan_hash_storage_shutdown (&migration_seed->enums);
    kan_hash_storage_shutdown (&migration_seed->structs);
    kan_free_batched (allocation_group, migration_seed);
}

static kan_bool_t migrator_allocation_group_ready = KAN_FALSE;
static kan_allocation_group_t migrator_allocation_group;

static kan_allocation_group_t get_migrator_allocation_group (void)
{
    if (!migrator_allocation_group_ready)
    {
        migrator_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "reflection_migrator");
        migrator_allocation_group_ready = KAN_TRUE;
    }

    return migrator_allocation_group;
}

static struct struct_migrator_node_t *migrator_query_struct (struct migrator_t *migrator,
                                                             kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&migrator->struct_migrators, (uint64_t) type_name);

    struct struct_migrator_node_t *node = (struct struct_migrator_node_t *) bucket->first;
    const struct struct_migrator_node_t *end =
        (struct struct_migrator_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->type_name == type_name)
        {
            return node;
        }

        node = (struct struct_migrator_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static inline void migrator_add_node (struct migrator_temporary_node_t *command,
                                      struct migrator_temporary_node_t **first,
                                      struct migrator_temporary_node_t **last)
{
    command->next = NULL;
    if (*last)
    {
        (*last)->next = command;
        *last = command;
    }
    else
    {
        *first = command;
        *last = command;
    }
}

static inline uint64_t migrator_add_condition (struct migrator_condition_t condition,
                                               kan_stack_allocator_t algorithm_allocator,
                                               struct migrator_command_temporary_queues_t *queues)
{
    // Condition existence check is slow for now, but it should be okay as there is not a lot of conditions.
    struct migrator_temporary_node_t *search_node = queues->condition_first;
    uint64_t node_index = 0u;

    while (search_node)
    {
        if (search_node->condition.absolute_source_offset == condition.absolute_source_offset &&
            search_node->condition.condition_field == condition.condition_field &&
            search_node->condition.condition_values_count == condition.condition_values_count &&
            search_node->condition.condition_values == condition.condition_values &&
            search_node->condition.parent_condition_index == condition.parent_condition_index)
        {
            return node_index;
        }

        search_node = search_node->next;
        ++node_index;
    }

    struct migrator_temporary_node_t *node = (struct migrator_temporary_node_t *) kan_stack_allocator_allocate (
        algorithm_allocator, sizeof (struct migrator_temporary_node_t), _Alignof (struct migrator_temporary_node_t));

    KAN_ASSERT (condition.condition_field)
    node->condition = condition;
    migrator_add_node (node, &queues->condition_first, &queues->condition_last);
    node_index = queues->condition_count;
    ++queues->condition_count;
    return node_index;
}

static inline void migrator_add_copy_command (struct migrator_command_copy_t copy_command,
                                              kan_stack_allocator_t algorithm_allocator,
                                              struct migrator_command_temporary_queues_t *queues)
{
    if (queues->copy_last)
    {
        const kan_bool_t source_end_is_start =
            queues->copy_last->copy.absolute_source_offset + queues->copy_last->copy.size ==
            copy_command.absolute_source_offset;

        const kan_bool_t target_end_is_start =
            queues->copy_last->copy.absolute_target_offset + queues->copy_last->copy.size ==
            copy_command.absolute_target_offset;

        const kan_bool_t matching_visibility = queues->copy_last->copy.condition_index == copy_command.condition_index;

        if (source_end_is_start && target_end_is_start && matching_visibility)
        {
            queues->copy_last->copy.size += copy_command.size;
            return;
        }
    }

    struct migrator_temporary_node_t *command = (struct migrator_temporary_node_t *) kan_stack_allocator_allocate (
        algorithm_allocator, sizeof (struct migrator_temporary_node_t), _Alignof (struct migrator_temporary_node_t));

    command->copy = copy_command;
    migrator_add_node (command, &queues->copy_first, &queues->copy_last);
    ++queues->copy_count;
}

static inline void migrator_add_adapt_numeric_command (struct migrator_command_adapt_numeric_t adapt_numeric_command,
                                                       kan_stack_allocator_t algorithm_allocator,
                                                       struct migrator_command_temporary_queues_t *queues)
{
    struct migrator_temporary_node_t *command = (struct migrator_temporary_node_t *) kan_stack_allocator_allocate (
        algorithm_allocator, sizeof (struct migrator_temporary_node_t), _Alignof (struct migrator_temporary_node_t));
    command->adapt_numeric = adapt_numeric_command;
    migrator_add_node (command, &queues->adapt_numeric_first, &queues->adapt_numeric_last);
    ++queues->adapt_numeric_count;
}

static inline void migrator_add_adapt_enum_command (struct migrator_command_adapt_enum_t adapt_enum_command,
                                                    kan_stack_allocator_t algorithm_allocator,
                                                    struct migrator_command_temporary_queues_t *queues)
{
    struct migrator_temporary_node_t *command = (struct migrator_temporary_node_t *) kan_stack_allocator_allocate (
        algorithm_allocator, sizeof (struct migrator_temporary_node_t), _Alignof (struct migrator_temporary_node_t));
    command->adapt_enum = adapt_enum_command;
    migrator_add_node (command, &queues->adapt_enum_first, &queues->adapt_enum_last);
    ++queues->adapt_enum_count;
}

static inline void migrator_add_adapt_dynamic_array_command (
    struct migrator_command_adapt_dynamic_array_t adapt_dynamic_array_command,
    kan_stack_allocator_t algorithm_allocator,
    struct migrator_command_temporary_queues_t *queues)
{
    struct migrator_temporary_node_t *command = (struct migrator_temporary_node_t *) kan_stack_allocator_allocate (
        algorithm_allocator, sizeof (struct migrator_temporary_node_t), _Alignof (struct migrator_temporary_node_t));
    command->adapt_dynamic_array = adapt_dynamic_array_command;
    migrator_add_node (command, &queues->adapt_dynamic_array_first, &queues->adapt_dynamic_array_last);
    ++queues->adapt_dynamic_array_count;
}

static inline void migrator_add_set_zero_command (struct migrator_command_set_zero_t set_zero_command,
                                                  kan_stack_allocator_t algorithm_allocator,
                                                  struct migrator_command_temporary_queues_t *queues)
{
    if (queues->set_zero_last)
    {
        const kan_bool_t end_is_start =
            queues->set_zero_last->set_zero.absolute_source_offset + queues->set_zero_last->set_zero.size ==
            set_zero_command.absolute_source_offset;

        const kan_bool_t matching_visibility =
            queues->copy_last->copy.condition_index == set_zero_command.condition_index;

        if (end_is_start && matching_visibility)
        {
            queues->set_zero_last->set_zero.size += set_zero_command.size;
            return;
        }
    }

    struct migrator_temporary_node_t *command = (struct migrator_temporary_node_t *) kan_stack_allocator_allocate (
        algorithm_allocator, sizeof (struct migrator_temporary_node_t), _Alignof (struct migrator_temporary_node_t));

    command->set_zero = set_zero_command;
    migrator_add_node (command, &queues->set_zero_first, &queues->set_zero_last);
    ++queues->set_zero_count;
}

static inline struct migrator_condition_t migrator_construct_condition (
    const struct kan_reflection_field_t *source_field)
{
    KAN_ASSERT (source_field->visibility_condition_field)
    const struct migrator_condition_t condition = {
        .absolute_source_offset = source_field->visibility_condition_field->offset,
        .condition_field = source_field->visibility_condition_field,
        .condition_values = source_field->visibility_condition_values,
        .condition_values_count = source_field->visibility_condition_values_count,
        .parent_condition_index = MIGRATOR_CONDITION_INDEX_NONE,
    };

    return condition;
}

static inline kan_bool_t migrator_query_is_enum_copyable (struct migration_seed_t *migration_seed,
                                                          kan_interned_string_t type_name)
{
    struct enum_migration_node_t *enum_node = migration_seed_query_enum (migration_seed, type_name);
    KAN_ASSERT (enum_node)

    switch (enum_node->seed.status)
    {
    case KAN_REFLECTION_MIGRATION_NEEDED:
        return KAN_FALSE;

    case KAN_REFLECTION_MIGRATION_NOT_NEEDED:
        return KAN_TRUE;

    case KAN_REFLECTION_MIGRATION_REMOVED:
        KAN_ASSERT (KAN_FALSE)
        break;
    }

    return KAN_FALSE;
}

static inline kan_bool_t migrator_query_is_struct_copyable (struct migration_seed_t *migration_seed,
                                                            kan_interned_string_t type_name)
{
    struct struct_migration_node_t *struct_node = migration_seed_query_struct (migration_seed, type_name);
    KAN_ASSERT (struct_node)

    switch (struct_node->seed.status)
    {
    case KAN_REFLECTION_MIGRATION_NEEDED:
        return KAN_FALSE;

    case KAN_REFLECTION_MIGRATION_NOT_NEEDED:
        return KAN_TRUE;

    case KAN_REFLECTION_MIGRATION_REMOVED:
        KAN_ASSERT (KAN_FALSE)
        break;
    }

    return KAN_FALSE;
}

static inline void migrator_add_numeric_commands (uint64_t source_offset,
                                                  uint64_t target_offset,
                                                  uint64_t source_size,
                                                  uint64_t target_size,
                                                  enum kan_reflection_archetype_t archetype,
                                                  uint64_t condition_index,
                                                  kan_stack_allocator_t algorithm_allocator,
                                                  struct migrator_command_temporary_queues_t *queues)
{
    if (source_size == target_size)
    {
        struct migrator_command_copy_t command;
        command.absolute_source_offset = source_offset;
        command.absolute_target_offset = target_offset;
        command.size = source_size;
        command.condition_index = condition_index;
        migrator_add_copy_command (command, algorithm_allocator, queues);
    }
    else
    {
        struct migrator_command_adapt_numeric_t command;
        command.absolute_source_offset = source_offset;
        command.absolute_target_offset = target_offset;
        command.source_size = source_size;
        command.target_size = target_size;
        command.archetype = archetype;
        command.condition_index = condition_index;
        migrator_add_adapt_numeric_command (command, algorithm_allocator, queues);
    }
}

static inline void migrator_add_handle_commands (uint64_t source_offset,
                                                 uint64_t target_offset,
                                                 uint64_t size,
                                                 uint64_t condition_index,
                                                 kan_stack_allocator_t algorithm_allocator,
                                                 struct migrator_command_temporary_queues_t *queues)
{
    struct migrator_command_copy_t copy_command;
    copy_command.absolute_source_offset = source_offset;
    copy_command.absolute_target_offset = target_offset;
    copy_command.size = size;
    copy_command.condition_index = condition_index;
    migrator_add_copy_command (copy_command, algorithm_allocator, queues);

    struct migrator_command_set_zero_t set_zero_command;
    set_zero_command.absolute_source_offset = source_offset;
    set_zero_command.size = size;
    set_zero_command.condition_index = condition_index;
    migrator_add_set_zero_command (set_zero_command, algorithm_allocator, queues);
}

static inline void migrator_add_interned_string_commands (uint64_t source_offset,
                                                          uint64_t target_offset,
                                                          uint64_t condition_index,
                                                          kan_stack_allocator_t algorithm_allocator,
                                                          struct migrator_command_temporary_queues_t *queues)
{
    struct migrator_command_copy_t command;
    command.absolute_source_offset = source_offset;
    command.absolute_target_offset = target_offset;
    command.size = sizeof (kan_interned_string_t);
    command.condition_index = condition_index;
    migrator_add_copy_command (command, algorithm_allocator, queues);
}

static inline void migrator_add_enum_commands (uint64_t source_offset,
                                               uint64_t target_offset,
                                               kan_interned_string_t type_name,
                                               struct migration_seed_t *migration_seed,
                                               uint64_t condition_index,
                                               kan_stack_allocator_t algorithm_allocator,
                                               struct migrator_command_temporary_queues_t *queues)
{
    if (migrator_query_is_enum_copyable (migration_seed, type_name))
    {
        struct migrator_command_copy_t command;
        command.absolute_source_offset = source_offset;
        command.absolute_target_offset = target_offset;
        command.size = sizeof (int);
        command.condition_index = condition_index;
        migrator_add_copy_command (command, algorithm_allocator, queues);
    }
    else
    {
        struct migrator_command_adapt_enum_t command;
        command.absolute_source_offset = source_offset;
        command.absolute_target_offset = target_offset;
        command.type_name = type_name;
        command.condition_index = condition_index;
        migrator_add_adapt_enum_command (command, algorithm_allocator, queues);
    }
}

static struct struct_migrator_node_t *migrator_request_struct_by_type_name (struct migrator_t *migrator,
                                                                            struct migration_seed_t *migration_seed,
                                                                            kan_interned_string_t type_name,
                                                                            kan_stack_allocator_t algorithm_allocator);

static inline void migrator_add_struct_commands (uint64_t source_offset,
                                                 uint64_t target_offset,
                                                 kan_interned_string_t type_name,
                                                 struct migrator_t *migrator,
                                                 struct migration_seed_t *migration_seed,
                                                 uint64_t condition_index,
                                                 kan_stack_allocator_t algorithm_allocator,
                                                 struct migrator_command_temporary_queues_t *queues)
{
    struct struct_migrator_node_t *sub_migrator =
        migrator_request_struct_by_type_name (migrator, migration_seed, type_name, algorithm_allocator);
    KAN_ASSERT (sub_migrator)
    const uint64_t own_conditions_count = queues->condition_count;

    for (uint64_t index = 0u; index < sub_migrator->conditions_count; ++index)
    {
        struct migrator_condition_t condition = sub_migrator->conditions[index];
        condition.absolute_source_offset += source_offset;

        if (condition.parent_condition_index == MIGRATOR_CONDITION_INDEX_NONE)
        {
            condition.parent_condition_index = condition_index;
        }

        const uint64_t imported_condition_index = migrator_add_condition (condition, algorithm_allocator, queues);
        // All conditions belong to the internal ecosystem of given struct, therefore must be unique.
        KAN_ASSERT (imported_condition_index == own_conditions_count + index)
    }

    for (uint64_t index = 0u; index < sub_migrator->copy_commands_count; ++index)
    {
        struct migrator_command_copy_t command = sub_migrator->copy_commands[index];
        command.absolute_source_offset += source_offset;
        command.absolute_target_offset += target_offset;

        if (command.condition_index == MIGRATOR_CONDITION_INDEX_NONE)
        {
            command.condition_index = condition_index;
        }
        else
        {
            command.condition_index += own_conditions_count;
        }

        migrator_add_copy_command (command, algorithm_allocator, queues);
    }

    for (uint64_t index = 0u; index < sub_migrator->adapt_numeric_commands_count; ++index)
    {
        struct migrator_command_adapt_numeric_t command = sub_migrator->adapt_numeric_commands[index];
        command.absolute_source_offset += source_offset;
        command.absolute_target_offset += target_offset;

        if (command.condition_index == MIGRATOR_CONDITION_INDEX_NONE)
        {
            command.condition_index = condition_index;
        }
        else
        {
            command.condition_index += own_conditions_count;
        }

        migrator_add_adapt_numeric_command (command, algorithm_allocator, queues);
    }

    for (uint64_t index = 0u; index < sub_migrator->adapt_enum_commands_count; ++index)
    {
        struct migrator_command_adapt_enum_t command = sub_migrator->adapt_enum_commands[index];
        command.absolute_source_offset += source_offset;
        command.absolute_target_offset += target_offset;

        if (command.condition_index == MIGRATOR_CONDITION_INDEX_NONE)
        {
            command.condition_index = condition_index;
        }
        else
        {
            command.condition_index += own_conditions_count;
        }

        migrator_add_adapt_enum_command (command, algorithm_allocator, queues);
    }

    for (uint64_t index = 0u; index < sub_migrator->adapt_dynamic_array_commands_count; ++index)
    {
        struct migrator_command_adapt_dynamic_array_t command = sub_migrator->adapt_dynamic_array_commands[index];
        command.absolute_source_offset += source_offset;
        command.absolute_target_offset += target_offset;

        if (command.condition_index == MIGRATOR_CONDITION_INDEX_NONE)
        {
            command.condition_index = condition_index;
        }
        else
        {
            command.condition_index += own_conditions_count;
        }

        migrator_add_adapt_dynamic_array_command (command, algorithm_allocator, queues);
    }

    for (uint64_t index = 0u; index < sub_migrator->set_zero_commands_count; ++index)
    {
        struct migrator_command_set_zero_t command = sub_migrator->set_zero_commands[index];
        command.absolute_source_offset += source_offset;

        if (command.condition_index == MIGRATOR_CONDITION_INDEX_NONE)
        {
            command.condition_index = condition_index;
        }
        else
        {
            command.condition_index += own_conditions_count;
        }

        migrator_add_set_zero_command (command, algorithm_allocator, queues);
    }
}

static struct struct_migrator_node_t *migrator_add_struct (struct migrator_t *migrator,
                                                           struct migration_seed_t *migration_seed,
                                                           const struct kan_reflection_struct_t *source_struct,
                                                           struct struct_migration_node_t *struct_migration_node,
                                                           kan_stack_allocator_t algorithm_allocator)
{
    void *saved_stack_top = kan_stack_allocator_save_top (algorithm_allocator);
    struct migrator_command_temporary_queues_t queues = {0u, NULL, NULL, 0u, NULL, NULL, 0u, NULL, NULL,
                                                         0u, NULL, NULL, 0u, NULL, NULL, 0u, NULL, NULL};
    KAN_ASSERT (struct_migration_node->seed.status != KAN_REFLECTION_MIGRATION_REMOVED)

    if (struct_migration_node->seed.status == KAN_REFLECTION_MIGRATION_NOT_NEEDED)
    {
        struct migrator_command_copy_t copy_command;
        copy_command.absolute_source_offset = 0u;
        copy_command.absolute_target_offset = 0u;
        copy_command.size = source_struct->size;
        copy_command.condition_index = MIGRATOR_CONDITION_INDEX_NONE;
        migrator_add_copy_command (copy_command, algorithm_allocator, &queues);
    }
    else
    {
        for (uint64_t field_index = 0u; field_index < source_struct->fields_count; ++field_index)
        {
            const struct kan_reflection_field_t *source_field = &source_struct->fields[field_index];
            const struct kan_reflection_field_t *target_field = struct_migration_node->seed.field_remap[field_index];

            if (!target_field)
            {
                // No field mapped, skipping migration.
                continue;
            }

            // TODO: It is unknown how to react to visibility condition changes.
            //       Is it even possible to implement properly?

            uint64_t condition_index = MIGRATOR_CONDITION_INDEX_NONE;
            if (source_field->visibility_condition_field)
            {
                condition_index =
                    migrator_add_condition (migrator_construct_condition (source_field), algorithm_allocator, &queues);
            }

            switch (source_field->archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_FLOATING:
                migrator_add_numeric_commands (source_field->offset, target_field->offset, source_field->size,
                                               target_field->size, source_field->archetype, condition_index,
                                               algorithm_allocator, &queues);
                break;

            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            case KAN_REFLECTION_ARCHETYPE_PATCH:
                KAN_ASSERT (source_field->size == target_field->size)
                migrator_add_handle_commands (source_field->offset, target_field->offset, source_field->size,
                                              condition_index, algorithm_allocator, &queues);
                break;

            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                migrator_add_interned_string_commands (source_field->offset, target_field->offset, condition_index,
                                                       algorithm_allocator, &queues);
                break;

            case KAN_REFLECTION_ARCHETYPE_ENUM:
                KAN_ASSERT (source_field->archetype_enum.type_name == target_field->archetype_enum.type_name)
                migrator_add_enum_commands (source_field->offset, target_field->offset,
                                            source_field->archetype_enum.type_name, migration_seed, condition_index,
                                            algorithm_allocator, &queues);
                break;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                KAN_ASSERT (source_field->archetype_struct.type_name == target_field->archetype_struct.type_name)
                migrator_add_struct_commands (source_field->offset, target_field->offset,
                                              source_field->archetype_struct.type_name, migrator, migration_seed,
                                              condition_index, algorithm_allocator, &queues);
                break;

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            {
                // TODO: Currently we treat inline arrays as always full.
                //       It should be technically okay unless inline array contains structs with dynamic arrays.
                //       We can rewrite inline array migration as separate command in order to take size field into
                //       account, but it will make migration slower as it will block current command-level
                //       optimizations from happening.
                const uint64_t items_count =
                    source_field->archetype_inline_array.item_count < target_field->archetype_inline_array.item_count ?
                        source_field->archetype_inline_array.item_count :
                        target_field->archetype_inline_array.item_count;

                for (uint64_t item_index = 0u; item_index < items_count; ++item_index)
                {
                    const uint64_t source_offset =
                        source_field->offset + source_field->archetype_inline_array.item_size * item_index;
                    const uint64_t target_offset =
                        target_field->offset + target_field->archetype_inline_array.item_size * item_index;

                    switch (source_field->archetype_inline_array.item_archetype)
                    {
                    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                    case KAN_REFLECTION_ARCHETYPE_FLOATING:
                        migrator_add_numeric_commands (source_offset, target_offset,
                                                       source_field->archetype_inline_array.item_size,
                                                       target_field->archetype_inline_array.item_size,
                                                       source_field->archetype_inline_array.item_archetype,
                                                       condition_index, algorithm_allocator, &queues);
                        break;

                    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
                    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                    case KAN_REFLECTION_ARCHETYPE_PATCH:
                        KAN_ASSERT (source_field->archetype_inline_array.item_size ==
                                    target_field->archetype_inline_array.item_size)
                        migrator_add_handle_commands (source_offset, target_offset,
                                                      source_field->archetype_inline_array.item_size, condition_index,
                                                      algorithm_allocator, &queues);
                        break;

                    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                        migrator_add_interned_string_commands (source_offset, target_offset, condition_index,
                                                               algorithm_allocator, &queues);
                        break;

                    case KAN_REFLECTION_ARCHETYPE_ENUM:
                        KAN_ASSERT (source_field->archetype_inline_array.item_archetype_enum.type_name ==
                                    target_field->archetype_inline_array.item_archetype_enum.type_name)
                        migrator_add_enum_commands (source_offset, target_offset,
                                                    source_field->archetype_inline_array.item_archetype_enum.type_name,
                                                    migration_seed, condition_index, algorithm_allocator, &queues);
                        break;

                    case KAN_REFLECTION_ARCHETYPE_STRUCT:
                        KAN_ASSERT (source_field->archetype_inline_array.item_archetype_struct.type_name ==
                                    target_field->archetype_inline_array.item_archetype_struct.type_name)
                        migrator_add_struct_commands (
                            source_offset, target_offset,
                            source_field->archetype_inline_array.item_archetype_struct.type_name, migrator,
                            migration_seed, condition_index, algorithm_allocator, &queues);
                        break;

                    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                        KAN_ASSERT (KAN_FALSE)
                        break;
                    }
                }

                break;
            }

            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            {
                kan_bool_t can_copy = KAN_FALSE;
                switch (source_field->archetype_dynamic_array.item_archetype)
                {
                case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                case KAN_REFLECTION_ARCHETYPE_FLOATING:
                    can_copy = source_field->archetype_dynamic_array.item_size ==
                               target_field->archetype_dynamic_array.item_size;
                    break;

                case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
                case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                case KAN_REFLECTION_ARCHETYPE_PATCH:
                    can_copy = KAN_TRUE;
                    break;

                case KAN_REFLECTION_ARCHETYPE_ENUM:
                    can_copy = migrator_query_is_enum_copyable (
                        migration_seed, source_field->archetype_dynamic_array.item_archetype_enum.type_name);
                    break;

                case KAN_REFLECTION_ARCHETYPE_STRUCT:
                    can_copy = migrator_query_is_struct_copyable (
                        migration_seed, source_field->archetype_dynamic_array.item_archetype_struct.type_name);
                    break;

                case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                    KAN_ASSERT (KAN_FALSE)
                    break;
                }

                if (can_copy)
                {
                    struct migrator_command_copy_t copy_command;
                    copy_command.absolute_source_offset = source_field->offset;
                    copy_command.absolute_target_offset = target_field->offset;
                    copy_command.size = source_field->size;
                    copy_command.condition_index = condition_index;
                    migrator_add_copy_command (copy_command, algorithm_allocator, &queues);

                    struct migrator_command_set_zero_t set_zero_command;
                    set_zero_command.absolute_source_offset = source_field->offset;
                    set_zero_command.size = sizeof (uint64_t) + sizeof (uint64_t) + sizeof (uint8_t *);
                    set_zero_command.condition_index = condition_index;
                    migrator_add_set_zero_command (set_zero_command, algorithm_allocator, &queues);
                }
                else
                {
                    struct migrator_command_adapt_dynamic_array_t adapt_command;
                    adapt_command.source_field = source_field;
                    adapt_command.absolute_source_offset = source_field->offset;
                    adapt_command.target_field = target_field;
                    adapt_command.absolute_target_offset = target_field->offset;
                    adapt_command.condition_index = condition_index;
                    migrator_add_adapt_dynamic_array_command (adapt_command, algorithm_allocator, &queues);
                }

                break;
            }
            }
        }
    }

    struct struct_migrator_node_t *struct_migrator =
        kan_allocate_batched (get_migrator_allocation_group (), sizeof (struct struct_migrator_node_t));
    struct_migrator->node.hash = (uint64_t) source_struct->name;
    struct_migrator->type_name = source_struct->name;

    _Static_assert (_Alignof (struct migrator_condition_t) == _Alignof (struct migrator_command_copy_t),
                    "Commands have same alignment.");
    _Static_assert (_Alignof (struct migrator_command_copy_t) == _Alignof (struct migrator_command_adapt_numeric_t),
                    "Commands have same alignment.");
    _Static_assert (
        _Alignof (struct migrator_command_adapt_numeric_t) == _Alignof (struct migrator_command_adapt_enum_t),
        "Commands have same alignment.");
    _Static_assert (
        _Alignof (struct migrator_command_adapt_enum_t) == _Alignof (struct migrator_command_adapt_dynamic_array_t),
        "Commands have same alignment.");
    _Static_assert (
        _Alignof (struct migrator_command_adapt_dynamic_array_t) == _Alignof (struct migrator_command_set_zero_t),
        "Commands have same alignment.");

    const uint64_t command_line_size =
        sizeof (struct migrator_condition_t) * queues.condition_count +
        sizeof (struct migrator_command_copy_t) * queues.copy_count +
        sizeof (struct migrator_command_adapt_numeric_t) * queues.adapt_numeric_count +
        sizeof (struct migrator_command_adapt_enum_t) * queues.adapt_enum_count +
        sizeof (struct migrator_command_adapt_dynamic_array_t) * queues.adapt_dynamic_array_count +
        sizeof (struct migrator_command_set_zero_t) * queues.set_zero_count;

    uint8_t *command_line = kan_allocate_general (get_migrator_allocation_group (), command_line_size,
                                                  _Alignof (struct migrator_condition_t));

    struct_migrator->conditions_count = queues.condition_count;
    struct_migrator->conditions = (struct migrator_condition_t *) command_line;
    command_line += sizeof (struct migrator_condition_t) * queues.condition_count;

    struct migrator_temporary_node_t *node = queues.condition_first;
    for (uint64_t index = 0u; index < struct_migrator->conditions_count; ++index)
    {
        KAN_ASSERT (node)
        struct_migrator->conditions[index] = node->condition;
        node = node->next;
    }

    struct_migrator->copy_commands_count = queues.copy_count;
    struct_migrator->copy_commands = (struct migrator_command_copy_t *) command_line;
    command_line += sizeof (struct migrator_command_copy_t) * queues.copy_count;

    node = queues.copy_first;
    for (uint64_t index = 0u; index < struct_migrator->copy_commands_count; ++index)
    {
        KAN_ASSERT (node)
        struct_migrator->copy_commands[index] = node->copy;
        node = node->next;
    }

    struct_migrator->adapt_numeric_commands_count = queues.adapt_numeric_count;
    struct_migrator->adapt_numeric_commands = (struct migrator_command_adapt_numeric_t *) command_line;
    command_line += sizeof (struct migrator_command_adapt_numeric_t) * queues.adapt_numeric_count;

    node = queues.adapt_numeric_first;
    for (uint64_t index = 0u; index < struct_migrator->adapt_numeric_commands_count; ++index)
    {
        KAN_ASSERT (node)
        struct_migrator->adapt_numeric_commands[index] = node->adapt_numeric;
        node = node->next;
    }

    struct_migrator->adapt_enum_commands_count = queues.adapt_enum_count;
    struct_migrator->adapt_enum_commands = (struct migrator_command_adapt_enum_t *) command_line;
    command_line += sizeof (struct migrator_command_adapt_enum_t) * queues.adapt_enum_count;

    node = queues.adapt_enum_first;
    for (uint64_t index = 0u; index < struct_migrator->adapt_enum_commands_count; ++index)
    {
        KAN_ASSERT (node)
        struct_migrator->adapt_enum_commands[index] = node->adapt_enum;
        node = node->next;
    }

    struct_migrator->adapt_dynamic_array_commands_count = queues.adapt_dynamic_array_count;
    struct_migrator->adapt_dynamic_array_commands = (struct migrator_command_adapt_dynamic_array_t *) command_line;
    command_line += sizeof (struct migrator_command_adapt_dynamic_array_t) * queues.adapt_dynamic_array_count;

    node = queues.adapt_dynamic_array_first;
    for (uint64_t index = 0u; index < struct_migrator->adapt_dynamic_array_commands_count; ++index)
    {
        KAN_ASSERT (node)
        struct_migrator->adapt_dynamic_array_commands[index] = node->adapt_dynamic_array;
        node = node->next;
    }

    struct_migrator->set_zero_commands_count = queues.set_zero_count;
    struct_migrator->set_zero_commands = (struct migrator_command_set_zero_t *) command_line;

    node = queues.set_zero_first;
    for (uint64_t index = 0u; index < struct_migrator->set_zero_commands_count; ++index)
    {
        KAN_ASSERT (node)
        struct_migrator->set_zero_commands[index] = node->set_zero;
        node = node->next;
    }

    kan_hash_storage_update_bucket_count_default (&migrator->struct_migrators,
                                                  KAN_REFLECTION_MIGRATION_STRUCT_SEED_INITIAL_BUCKETS);
    kan_hash_storage_add (&migrator->struct_migrators, &struct_migrator->node);
    kan_stack_allocator_load_top (algorithm_allocator, saved_stack_top);
    return struct_migrator;
}

static struct struct_migrator_node_t *migrator_request_struct_by_type_name (struct migrator_t *migrator,
                                                                            struct migration_seed_t *migration_seed,
                                                                            kan_interned_string_t type_name,
                                                                            kan_stack_allocator_t algorithm_allocator)
{
    struct struct_migrator_node_t *node = migrator_query_struct (migrator, type_name);
    if (!node)
    {
        struct struct_migration_node_t *migration_node = migration_seed_query_struct (migration_seed, type_name);
        if (!migration_node)
        {
            KAN_LOG (reflection_migrator, KAN_LOG_ERROR,
                     "Unable to find migration seed node for struct \"%s\". Corrupted seed?", type_name)
            KAN_ASSERT (KAN_FALSE)
            return NULL;
        }

        const struct kan_reflection_struct_t *source_struct = kan_reflection_registry_query_struct (
            (kan_reflection_registry_t) migration_seed->source_registry, type_name);

        if (!source_struct)
        {
            KAN_LOG (reflection_migrator, KAN_LOG_ERROR, "Unable to find source struct \"%s\". Corrupted seed?",
                     type_name)
            KAN_ASSERT (KAN_FALSE)
            return NULL;
        }

        node = migrator_add_struct (migrator, migration_seed, source_struct, migration_node, algorithm_allocator);
    }

    return node;
}

kan_reflection_struct_migrator_t kan_reflection_struct_migrator_build (kan_reflection_migration_seed_t seed)
{
    struct migration_seed_t *migration_seed = (struct migration_seed_t *) seed;
    struct migrator_t *migrator = kan_allocate_batched (get_migrator_allocation_group (), sizeof (struct migrator_t));
    migrator->source_seed = migration_seed;

    kan_hash_storage_init (&migrator->struct_migrators, get_migrator_allocation_group (),
                           migration_seed->structs.bucket_count);

    kan_stack_allocator_t algorithm_allocator =
        kan_stack_allocator_create (get_migrator_allocation_group (), KAN_REFLECTION_MIGRATOR_BUILD_STACK);

    struct struct_migration_node_t *node = (struct struct_migration_node_t *) migration_seed->structs.items.first;
    while (node)
    {
        if (node->seed.status != KAN_REFLECTION_MIGRATION_REMOVED && !migrator_query_struct (migrator, node->type_name))
        {
            const struct kan_reflection_struct_t *source_struct = kan_reflection_registry_query_struct (
                (kan_reflection_registry_t) migration_seed->source_registry, node->type_name);

            if (source_struct)
            {
                migrator_add_struct (migrator, migration_seed, source_struct, node, algorithm_allocator);
            }
            else
            {
                KAN_LOG (reflection_migrator, KAN_LOG_ERROR, "Unable to find source struct \"%s\". Corrupted seed?",
                         node->type_name)
                KAN_ASSERT (KAN_FALSE)
            }
        }

        node = (struct struct_migration_node_t *) node->node.list_node.next;
    }

    kan_stack_allocator_destroy (algorithm_allocator);
    return (kan_reflection_struct_migrator_t) migrator;
}

static void migrator_adapt_numeric (uint64_t source_size,
                                    uint64_t target_size,
                                    enum kan_reflection_archetype_t archetype,
                                    const void *located_input,
                                    void *located_output)
{
    KAN_ASSERT (source_size != target_size)
    switch (archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        switch (source_size)
        {
        case sizeof (int8_t):
            switch (target_size)
            {
            case sizeof (int16_t):
                *(int16_t *) located_output = (int16_t) * (const int8_t *) located_input;
                return;

            case sizeof (int32_t):
                *(int32_t *) located_output = (int32_t) * (const int8_t *) located_input;
                return;

            case sizeof (int64_t):
                *(int64_t *) located_output = (int64_t) * (const int8_t *) located_input;
                return;
            }

            break;

        case sizeof (int16_t):
            switch (target_size)
            {
            case sizeof (int8_t):
                *(int8_t *) located_output = (int8_t) * (const int16_t *) located_input;
                return;

            case sizeof (int32_t):
                *(int32_t *) located_output = (int32_t) * (const int16_t *) located_input;
                return;

            case sizeof (int64_t):
                *(int64_t *) located_output = (int64_t) * (const int16_t *) located_input;
                return;
            }

            break;

        case sizeof (int32_t):
            switch (target_size)
            {
            case sizeof (int8_t):
                *(int8_t *) located_output = (int8_t) * (const int32_t *) located_input;
                return;

            case sizeof (int16_t):
                *(int16_t *) located_output = (int16_t) * (const int32_t *) located_input;
                return;

            case sizeof (int64_t):
                *(int64_t *) located_output = (int64_t) * (const int32_t *) located_input;
                return;
            }

            break;

        case sizeof (int64_t):
            switch (target_size)
            {
            case sizeof (int8_t):
                *(int8_t *) located_output = (int8_t) * (const int64_t *) located_input;
                return;

            case sizeof (int16_t):
                *(int16_t *) located_output = (int16_t) * (const int64_t *) located_input;
                return;

            case sizeof (int32_t):
                *(int32_t *) located_output = (int32_t) * (const int64_t *) located_input;
                return;
            }

            break;
        }

        break;

    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        switch (source_size)
        {
        case sizeof (uint8_t):
            switch (target_size)
            {
            case sizeof (uint16_t):
                *(uint16_t *) located_output = (uint16_t) * (const uint8_t *) located_input;
                return;

            case sizeof (uint32_t):
                *(uint32_t *) located_output = (uint32_t) * (const uint8_t *) located_input;
                return;

            case sizeof (uint64_t):
                *(uint64_t *) located_output = (uint64_t) * (const uint8_t *) located_input;
                return;
            }

            break;

        case sizeof (uint16_t):
            switch (target_size)
            {
            case sizeof (uint8_t):
                *(uint8_t *) located_output = (uint8_t) * (const uint16_t *) located_input;
                return;

            case sizeof (uint32_t):
                *(uint32_t *) located_output = (uint32_t) * (const uint16_t *) located_input;
                return;

            case sizeof (uint64_t):
                *(uint64_t *) located_output = (uint64_t) * (const uint16_t *) located_input;
                return;
            }

            break;

        case sizeof (uint32_t):
            switch (target_size)
            {
            case sizeof (uint8_t):
                *(uint8_t *) located_output = (uint8_t) * (const uint32_t *) located_input;
                return;

            case sizeof (uint16_t):
                *(uint16_t *) located_output = (uint16_t) * (const uint32_t *) located_input;
                return;

            case sizeof (uint64_t):
                *(uint64_t *) located_output = (uint64_t) * (const uint32_t *) located_input;
                return;
            }

            break;

        case sizeof (uint64_t):
            switch (target_size)
            {
            case sizeof (uint8_t):
                *(uint8_t *) located_output = (uint8_t) * (const uint64_t *) located_input;
                return;

            case sizeof (uint16_t):
                *(uint16_t *) located_output = (uint16_t) * (const uint64_t *) located_input;
                return;

            case sizeof (uint32_t):
                *(uint32_t *) located_output = (uint32_t) * (const uint64_t *) located_input;
                return;
            }

            break;
        }

        break;

    case KAN_REFLECTION_ARCHETYPE_FLOATING:
        switch (source_size)
        {
        case sizeof (float):
            KAN_ASSERT (target_size == sizeof (double))
            *(float *) located_output = (float) *(const double *) located_input;
            return;

        case sizeof (double):
            KAN_ASSERT (target_size == sizeof (float))
            *(double *) located_output = (double) *(const float *) located_input;
            return;
        }
        break;

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

    KAN_ASSERT (KAN_FALSE)
}

static void migrator_adapt_enum (const struct migrator_t *migrator,
                                 kan_interned_string_t type_name,
                                 const void *located_input,
                                 void *located_output)
{
    struct enum_migration_node_t *migration_node = migration_seed_query_enum (migrator->source_seed, type_name);
    KAN_ASSERT (migration_node)
    KAN_ASSERT (migration_node->seed.status == KAN_REFLECTION_MIGRATION_NEEDED)

    const struct kan_reflection_enum_t *source_enum_data = kan_reflection_registry_query_enum (
        (kan_reflection_registry_t) migrator->source_seed->source_registry, type_name);
    KAN_ASSERT (source_enum_data)

    const struct kan_reflection_enum_t *target_enum_data = kan_reflection_registry_query_enum (
        (kan_reflection_registry_t) migrator->source_seed->target_registry, type_name);
    KAN_ASSERT (source_enum_data)

    const kan_bool_t migration_single_to_single = !source_enum_data->flags && !target_enum_data->flags;
    const kan_bool_t migration_single_to_flags = !source_enum_data->flags && target_enum_data->flags;
    const kan_bool_t migration_flags_to_single = source_enum_data->flags && !target_enum_data->flags;
    const kan_bool_t migration_flags_to_flags = source_enum_data->flags && target_enum_data->flags;

    if (migration_single_to_single || migration_single_to_flags)
    {
        const int enum_value = *(const int *) located_input;
        for (uint64_t value_index = 0u; value_index < source_enum_data->values_count; ++value_index)
        {
            if (source_enum_data->values[value_index].value == (int64_t) enum_value)
            {
                if (migration_single_to_single)
                {
                    *(int *) located_output = (int) migration_node->seed.value_remap[value_index]->value;
                }
                else
                {
                    *(unsigned int *) located_output =
                        (unsigned int) migration_node->seed.value_remap[value_index]->value;
                }

                return;
            }
        }

        KAN_LOG (reflection_migrator, KAN_LOG_ERROR,
                 "Encountered unknown value \"%d\" of enum \"%s\". Resetting it to first correct value \"%s\".",
                 enum_value, type_name, target_enum_data->values[0].name)

        if (migration_single_to_single)
        {
            *(int *) located_output = (int) target_enum_data->values[0].value;
        }
        else
        {
            *(unsigned int *) located_output = (unsigned int) target_enum_data->values[0].value;
        }
    }
    else if (migration_flags_to_flags)
    {
        // Redirect all the flags.
        *(unsigned int *) located_output = 0u;
        const unsigned int enum_value = *(const unsigned int *) located_input;

        for (uint64_t value_index = 0u; value_index < source_enum_data->values_count; ++value_index)
        {
            if (enum_value & (unsigned int) source_enum_data->values[value_index].value)
            {
                *(unsigned int *) located_output |= (unsigned int) migration_node->seed.value_remap[value_index]->value;
            }
        }
    }
    else if (migration_flags_to_single)
    {
        // Find first active flag and convert it to resulting single.
        const unsigned int enum_value = *(const unsigned int *) located_input;

        for (uint64_t value_index = 0u; value_index < source_enum_data->values_count; ++value_index)
        {
            if (enum_value & (unsigned int) source_enum_data->values[value_index].value)
            {
                *(int *) located_output = (int) migration_node->seed.value_remap[value_index]->value;
                return;
            }
        }

        KAN_LOG (reflection_migrator, KAN_LOG_ERROR,
                 "Encountered empty value of flags-based enum \"%s\" while converting it to single value enum. "
                 "Resetting it to first correct value \"%s\".",
                 type_name, target_enum_data->values[0].name)
        *(int *) located_output = (int) target_enum_data->values[0].value;
    }
}

static void migrator_adapt_dynamic_array (struct migrator_t *migrator,
                                          const struct migrator_command_adapt_dynamic_array_t *command,
                                          const void *located_input,
                                          void *located_output)
{
    KAN_ASSERT (command->source_field->archetype_dynamic_array.item_archetype ==
                command->target_field->archetype_dynamic_array.item_archetype)

    struct kan_dynamic_array_t *input_array = (struct kan_dynamic_array_t *) located_input;
    struct kan_dynamic_array_t *output_array = (struct kan_dynamic_array_t *) located_output;
    KAN_ASSERT (output_array->size == 0u)
    kan_dynamic_array_set_capacity (output_array, input_array->capacity);
    uint8_t *input_data = input_array->data;

    for (uint64_t index = 0u; index < input_array->size; ++index)
    {
        void *output = kan_dynamic_array_add_last (output_array);
        KAN_ASSERT (output)

        switch (command->source_field->archetype_dynamic_array.item_archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
            KAN_ASSERT (command->source_field->archetype_dynamic_array.item_size !=
                        command->target_field->archetype_dynamic_array.item_size)

            migrator_adapt_numeric (command->source_field->archetype_dynamic_array.item_size,
                                    command->target_field->archetype_dynamic_array.item_size,
                                    command->source_field->archetype_dynamic_array.item_archetype, input_data, output);
            break;

        case KAN_REFLECTION_ARCHETYPE_ENUM:
            KAN_ASSERT (command->source_field->archetype_dynamic_array.item_archetype_enum.type_name ==
                        command->target_field->archetype_dynamic_array.item_archetype_enum.type_name)

            migrator_adapt_enum (migrator, command->source_field->archetype_dynamic_array.item_archetype_enum.type_name,
                                 input_data, output);
            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
            KAN_ASSERT (command->source_field->archetype_dynamic_array.item_archetype_struct.type_name ==
                        command->target_field->archetype_dynamic_array.item_archetype_struct.type_name)

            kan_reflection_struct_migrator_migrate_instance (
                (kan_reflection_struct_migrator_t) migrator,
                command->source_field->archetype_dynamic_array.item_archetype_struct.type_name, input_data, output);
            break;

            // Archetypes below do not need any adaptation, therefore this command should not be issued.
        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        case KAN_REFLECTION_ARCHETYPE_PATCH:
            // Archetypes below cannot be inside dynamic array.
        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            KAN_ASSERT (KAN_FALSE)
            break;
        }

        input_data += command->source_field->archetype_dynamic_array.item_size;
    }
}

void kan_reflection_struct_migrator_migrate_instance (kan_reflection_struct_migrator_t migrator,
                                                      kan_interned_string_t type_name,
                                                      void *source,
                                                      void *target)
{
    struct migrator_t *migrator_data = (struct migrator_t *) migrator;
    struct struct_migrator_node_t *struct_node = migrator_query_struct (migrator_data, type_name);

    if (!struct_node)
    {
        KAN_LOG (reflection_migrator, KAN_LOG_ERROR, "Unable to find migrator for struct \"%s\".", type_name)
        return;
    }

    kan_bool_t conditions_fixed[KAN_REFLECTION_MIGRATOR_MAX_CONDITIONS];
    kan_bool_t *conditions = conditions_fixed;

    if (struct_node->conditions_count > KAN_REFLECTION_MIGRATOR_MAX_CONDITIONS)
    {
        conditions = kan_allocate_general (get_migrator_allocation_group (),
                                           sizeof (kan_bool_t) * struct_node->conditions_count, _Alignof (kan_bool_t));
    }

    for (uint64_t condition_index = 0u; condition_index < struct_node->conditions_count; ++condition_index)
    {
        const struct migrator_condition_t *condition = &struct_node->conditions[condition_index];
        if (condition->parent_condition_index != MIGRATOR_CONDITION_INDEX_NONE)
        {
            KAN_ASSERT (condition->parent_condition_index < condition_index)
            if (!(conditions[condition_index] = conditions[condition->parent_condition_index]))
            {
                continue;
            }
        }

        conditions[condition_index] = kan_reflection_check_visibility (
            condition->condition_field, condition->condition_values_count, condition->condition_values,
            ((uint8_t *) source) + condition->absolute_source_offset);
    }

    for (uint64_t command_index = 0u; command_index < struct_node->copy_commands_count; ++command_index)
    {
        const struct migrator_command_copy_t *copy_command = &struct_node->copy_commands[command_index];
        if (copy_command->condition_index != MIGRATOR_CONDITION_INDEX_NONE &&
            !conditions[copy_command->condition_index])
        {
            continue;
        }

        memcpy (((uint8_t *) target) + copy_command->absolute_target_offset,
                ((uint8_t *) source) + copy_command->absolute_source_offset, copy_command->size);
    }

    for (uint64_t command_index = 0u; command_index < struct_node->adapt_numeric_commands_count; ++command_index)
    {
        const struct migrator_command_adapt_numeric_t *adapt_numeric_command =
            &struct_node->adapt_numeric_commands[command_index];

        if (adapt_numeric_command->condition_index != MIGRATOR_CONDITION_INDEX_NONE &&
            !conditions[adapt_numeric_command->condition_index])
        {
            continue;
        }

        migrator_adapt_numeric (adapt_numeric_command->source_size, adapt_numeric_command->target_size,
                                adapt_numeric_command->archetype,
                                ((uint8_t *) source) + adapt_numeric_command->absolute_source_offset,
                                ((uint8_t *) target) + adapt_numeric_command->absolute_target_offset);
    }

    for (uint64_t command_index = 0u; command_index < struct_node->adapt_enum_commands_count; ++command_index)
    {
        const struct migrator_command_adapt_enum_t *adapt_enum_command =
            &struct_node->adapt_enum_commands[command_index];

        if (adapt_enum_command->condition_index != MIGRATOR_CONDITION_INDEX_NONE &&
            !conditions[adapt_enum_command->condition_index])
        {
            continue;
        }

        migrator_adapt_enum (migrator_data, adapt_enum_command->type_name,
                             ((uint8_t *) source) + adapt_enum_command->absolute_source_offset,
                             ((uint8_t *) target) + adapt_enum_command->absolute_target_offset);
    }

    for (uint64_t command_index = 0u; command_index < struct_node->adapt_dynamic_array_commands_count; ++command_index)
    {
        const struct migrator_command_adapt_dynamic_array_t *adapt_dynamic_array_command =
            &struct_node->adapt_dynamic_array_commands[command_index];

        if (adapt_dynamic_array_command->condition_index != MIGRATOR_CONDITION_INDEX_NONE &&
            !conditions[adapt_dynamic_array_command->condition_index])
        {
            continue;
        }

        migrator_adapt_dynamic_array (migrator_data, adapt_dynamic_array_command,
                                      ((uint8_t *) source) + adapt_dynamic_array_command->absolute_source_offset,
                                      ((uint8_t *) target) + adapt_dynamic_array_command->absolute_target_offset);
    }

    for (uint64_t command_index = 0u; command_index < struct_node->set_zero_commands_count; ++command_index)
    {
        const struct migrator_command_set_zero_t *set_zero_command = &struct_node->set_zero_commands[command_index];
        if (set_zero_command->condition_index != MIGRATOR_CONDITION_INDEX_NONE &&
            !conditions[set_zero_command->condition_index])
        {
            continue;
        }

        memset (((uint8_t *) source) + set_zero_command->absolute_source_offset, 0u, set_zero_command->size);
    }

    if (conditions != conditions_fixed)
    {
        kan_free_general (get_migrator_allocation_group (), conditions,
                          sizeof (kan_bool_t) * struct_node->conditions_count);
    }
}

enum patch_condition_status_t
{
    PATCH_CONDITION_STATUS_TRUE = 0,
    PATCH_CONDITION_STATUS_FALSE,
    PATCH_CONDITION_STATUS_NOT_CALCULATED,
};

static inline kan_bool_t patch_migration_evaluate_condition (uint64_t condition_index,
                                                             struct migrator_condition_t *conditions,
                                                             enum patch_condition_status_t *condition_statuses,
                                                             uint64_t node_index,
                                                             struct compiled_patch_node_t **nodes)
{
    if (condition_index == MIGRATOR_CONDITION_INDEX_NONE)
    {
        return KAN_TRUE;
    }

    if (condition_statuses[condition_index] == PATCH_CONDITION_STATUS_NOT_CALCULATED)
    {
        const struct migrator_condition_t *condition = &conditions[condition_index];
        if (condition->parent_condition_index != MIGRATOR_CONDITION_INDEX_NONE)
        {
            if (!patch_migration_evaluate_condition (condition->parent_condition_index, conditions, condition_statuses,
                                                     node_index, nodes))
            {
                condition_statuses[condition_index] = PATCH_CONDITION_STATUS_FALSE;
                return KAN_FALSE;
            }
        }

        struct compiled_patch_node_t *node_with_condition_value = NULL;

        // Search for node with condition value.
        while (KAN_TRUE)
        {
            struct compiled_patch_node_t *node = nodes[node_index];
            if (condition->absolute_source_offset >= node->offset &&
                condition->absolute_source_offset < node->offset + node->size)
            {
                node_with_condition_value = node;
                break;
            }

            if (node_index == 0u)
            {
                break;
            }
            else
            {
                --node_index;
            }
        }

        if (node_with_condition_value)
        {
            const uint64_t offset_in_node = condition->absolute_source_offset - node_with_condition_value->offset;
            const kan_bool_t check_result = kan_reflection_check_visibility (
                condition->condition_field, condition->condition_values_count, condition->condition_values,
                node_with_condition_value->data + offset_in_node);

            condition_statuses[condition_index] =
                check_result ? PATCH_CONDITION_STATUS_TRUE : PATCH_CONDITION_STATUS_FALSE;
        }
        else
        {
            // As a fallback when value is absent, if it is first condition for that value, then we treat it as true,
            // otherwise, we treat it as false to avoid multiple command execution.
            const kan_bool_t first_on_this_address =
                condition_index == 0u ||
                conditions[condition_index - 1].absolute_source_offset != condition->absolute_source_offset;

            condition_statuses[condition_index] =
                first_on_this_address ? PATCH_CONDITION_STATUS_TRUE : PATCH_CONDITION_STATUS_FALSE;
        }
    }

    return condition_statuses[condition_index] == PATCH_CONDITION_STATUS_TRUE;
}

struct patch_migration_task_data_t
{
    kan_reflection_registry_t target_registry;
    kan_reflection_struct_migrator_t migrator;
    struct compiled_patch_t *patch_begin;
    struct compiled_patch_t *patch_end;
};

static void migrate_patch_task (kan_cpu_task_user_data_t user_data)
{
    const kan_allocation_group_t group = get_compiled_patch_allocation_group ();
    kan_reflection_patch_builder_t patch_builder = kan_reflection_patch_builder_create ();
    struct patch_builder_t *patch_builder_data = (struct patch_builder_t *) patch_builder;
    struct patch_migration_task_data_t *data = (struct patch_migration_task_data_t *) user_data;

    struct registry_t *target_registry_data = (struct registry_t *) data->target_registry;
    struct migrator_t *migrator_data = (struct migrator_t *) data->migrator;
    struct compiled_patch_t *patch = data->patch_begin;

    while (patch != data->patch_end)
    {
        struct compiled_patch_t *next = patch->next;
        struct struct_migrator_node_t *migrator_node = migrator_query_struct (migrator_data, patch->type->name);

        if (migrator_node)
        {
            struct compiled_patch_node_t *nodes_fixed[KAN_REFLECTION_MIGRATOR_PATCH_MAX_NODES];
            struct compiled_patch_node_t **nodes = nodes_fixed;

            if (patch->node_count > KAN_REFLECTION_MIGRATOR_PATCH_MAX_NODES)
            {
                nodes = kan_allocate_general (group, sizeof (struct compiled_patch_node_t *) * patch->node_count,
                                              _Alignof (struct compiled_patch_node_t *));
            }

            enum patch_condition_status_t conditions_fixed[KAN_REFLECTION_MIGRATOR_MAX_CONDITIONS];
            enum patch_condition_status_t *conditions = conditions_fixed;

            if (migrator_node->conditions_count > KAN_REFLECTION_MIGRATOR_MAX_CONDITIONS)
            {
                conditions = kan_allocate_general (
                    group, sizeof (enum patch_condition_status_t) * migrator_node->conditions_count,
                    _Alignof (enum patch_condition_status_t));
            }

            for (uint64_t condition_index = 0u; condition_index < migrator_node->conditions_count; ++condition_index)
            {
                conditions[condition_index] = PATCH_CONDITION_STATUS_NOT_CALCULATED;
            }

            struct compiled_patch_node_t *node = patch->begin;
            const struct compiled_patch_node_t *end = patch->end;
            uint64_t node_index = 0u;

            struct migrator_command_copy_t *copy_command = migrator_node->copy_commands;
            const struct migrator_command_copy_t *copy_command_end =
                migrator_node->copy_commands + migrator_node->copy_commands_count;

            struct migrator_command_adapt_numeric_t *adapt_numeric_command = migrator_node->adapt_numeric_commands;
            const struct migrator_command_adapt_numeric_t *adapt_numeric_command_end =
                migrator_node->adapt_numeric_commands + migrator_node->adapt_numeric_commands_count;

            struct migrator_command_adapt_enum_t *adapt_enum_command = migrator_node->adapt_enum_commands;
            const struct migrator_command_adapt_enum_t *adapt_enum_command_end =
                migrator_node->adapt_enum_commands + migrator_node->adapt_enum_commands_count;

            while (node != end)
            {
                nodes[node_index] = node;
                uint64_t offset = node->offset;

                while (offset < node->size + node->offset)
                {
                    // Skip commands until we reach our address space. Also take care of conditions.
                    while (
                        copy_command != copy_command_end &&
                        (copy_command->absolute_source_offset + copy_command->size < offset ||
                         !patch_migration_evaluate_condition (copy_command->condition_index, migrator_node->conditions,
                                                              conditions, node_index, nodes)))
                    {
                        ++copy_command;
                    }

                    while (
                        adapt_numeric_command != adapt_numeric_command_end &&
                        (adapt_numeric_command->absolute_source_offset + adapt_numeric_command->source_size < offset ||
                         !patch_migration_evaluate_condition (adapt_numeric_command->condition_index,
                                                              migrator_node->conditions, conditions, node_index,
                                                              nodes)))
                    {
                        ++adapt_numeric_command;
                    }

                    while (adapt_enum_command != adapt_enum_command_end &&
                           (adapt_enum_command->absolute_source_offset + sizeof (int) < offset ||
                            !patch_migration_evaluate_condition (adapt_enum_command->condition_index,
                                                                 migrator_node->conditions, conditions, node_index,
                                                                 nodes)))
                    {
                        ++adapt_enum_command;
                    }

                    if (copy_command != copy_command_end && offset >= copy_command->absolute_source_offset &&
                        offset < copy_command->absolute_source_offset + copy_command->size)
                    {
                        const uint64_t offset_from_source = offset - copy_command->absolute_source_offset;
                        const uint64_t size = copy_command->size - offset_from_source;

                        kan_reflection_patch_builder_add_chunk (
                            patch_builder, copy_command->absolute_target_offset + offset_from_source,
                            copy_command->size - offset_from_source, node->data + (offset - node->offset));
                        offset += size;
                        ++copy_command;
                    }
                    else if (adapt_numeric_command != adapt_numeric_command_end &&
                             offset == adapt_numeric_command->absolute_source_offset)
                    {
                        uint64_t output_buffer;
                        migrator_adapt_numeric (adapt_numeric_command->source_size, adapt_numeric_command->target_size,
                                                adapt_numeric_command->archetype, node->data + (offset - node->offset),
                                                &output_buffer);

                        kan_reflection_patch_builder_add_chunk (patch_builder,
                                                                adapt_numeric_command->absolute_target_offset,
                                                                adapt_numeric_command->target_size, &output_buffer);
                        offset += adapt_numeric_command->source_size;
                        ++adapt_numeric_command;
                    }
                    else if (adapt_enum_command != adapt_enum_command_end &&
                             offset == adapt_enum_command->absolute_source_offset)
                    {
                        int output_buffer;
                        migrator_adapt_enum (migrator_data, adapt_enum_command->type_name,
                                             node->data + (offset - node->offset), &output_buffer);

                        kan_reflection_patch_builder_add_chunk (
                            patch_builder, adapt_enum_command->absolute_target_offset, sizeof (int), &output_buffer);
                        offset += sizeof (int);
                        ++adapt_enum_command;
                    }
                    else
                    {
                        // Partition is absent in target registry and therefore should be dropped.
                        // Move offset to the first next command.

                        const uint64_t copy_offset =
                            copy_command != copy_command_end ? copy_command->absolute_source_offset : patch->type->size;

                        const uint64_t adapt_numeric_offset = adapt_numeric_command != adapt_numeric_command_end ?
                                                                  adapt_numeric_command->absolute_source_offset :
                                                                  patch->type->size;

                        const uint64_t adapt_enum_offset = adapt_enum_command != adapt_enum_command_end ?
                                                               adapt_enum_command->absolute_source_offset :
                                                               patch->type->size;

                        offset = KAN_MIN (copy_offset, KAN_MIN (adapt_numeric_offset, adapt_enum_offset));
                    }
                }

                uint8_t *data_end = node->data + node->size;
                data_end =
                    (uint8_t *) kan_apply_alignment ((uint64_t) data_end, _Alignof (struct compiled_patch_node_t));
                node = (struct compiled_patch_node_t *) data_end;
                ++node_index;
            }

            if (conditions != conditions_fixed)
            {
                kan_free_general (group, conditions,
                                  sizeof (enum patch_condition_status_t) * migrator_node->conditions_count);
            }

            if (nodes != nodes_fixed)
            {
                kan_free_general (group, nodes, sizeof (struct compiled_patch_node_t *) * patch->node_count);
            }

            // Destroy old nodes.
            kan_free_general (group, patch->begin, ((uint8_t *) patch->end) - (uint8_t *) patch->begin);

            const struct kan_reflection_struct_t *target_type =
                kan_reflection_registry_query_struct (data->target_registry, patch->type->name);

            if (!compiled_patch_build_into (patch_builder_data, target_registry_data, target_type, patch))
            {
                KAN_LOG (reflection_migrator, KAN_LOG_ERROR, "Failed to migrate patch under address %p.",
                         (void *) patch)
                kan_free_batched (get_compiled_patch_allocation_group (), patch);
            }
        }
        else
        {
            // Type is deleted, therefore patch should be destroyed too.
            compiled_patch_destroy (patch);
        }

        patch = next;
    }

    kan_reflection_patch_builder_destroy (patch_builder);
}

void kan_reflection_struct_migrator_migrate_patches (kan_reflection_struct_migrator_t migrator,
                                                     kan_reflection_registry_t source_registry,
                                                     kan_reflection_registry_t target_registry)
{
    const kan_allocation_group_t group = get_compiled_patch_allocation_group ();
    struct registry_t *source_registry_data = (struct registry_t *) source_registry;
    struct compiled_patch_t *patch = source_registry_data->first_patch;

    if (patch == NULL)
    {
        return;
    }

    struct patch_migration_task_data_t *next_task_data = NULL;
    uint64_t patches_in_task = 0u;
    struct kan_cpu_task_list_node_t *task_list = NULL;
    const kan_interned_string_t task_name = kan_string_intern ("reflection_patch_migration");

    struct kan_stack_group_allocator_t allocator;
    kan_stack_group_allocator_init (&allocator, group, KAN_REFLECTION_MIGRATOR_PATCH_TASK_STACK_INITIAL_SIZE);

    while (patch)
    {
        struct compiled_patch_t *next = patch->next;
        if (!next_task_data)
        {
            next_task_data = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&allocator, struct patch_migration_task_data_t);
            next_task_data->target_registry = target_registry;
            next_task_data->migrator = migrator;
            next_task_data->patch_begin = patch;
        }

        ++patches_in_task;
        if (patches_in_task > KAN_REFLECTION_MIGRATOR_PATCH_BUNDLE_SIZE || !next)
        {
            next_task_data->patch_end = next;
            struct kan_cpu_task_list_node_t *task_list_node =
                KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&allocator, struct kan_cpu_task_list_node_t);

            task_list_node->next = task_list;
            task_list_node->task = (struct kan_cpu_task_t) {
                .name = task_name,
                .function = migrate_patch_task,
                .user_data = (uint64_t) next_task_data,
            };

            task_list_node->queue = KAN_CPU_DISPATCH_QUEUE_FOREGROUND;
            task_list = task_list_node;

            next_task_data = NULL;
            patches_in_task = 0u;
        }

        patch = next;
    }

    kan_cpu_job_t job = kan_cpu_job_create ();
    kan_cpu_job_dispatch_and_detach_task_list (job, task_list);
    kan_cpu_job_release (job);
    kan_cpu_job_wait (job);

    kan_stack_group_allocator_shutdown (&allocator);
    source_registry_data->first_patch = NULL;
}

void kan_reflection_struct_migrator_destroy (kan_reflection_struct_migrator_t migrator)
{
    const kan_allocation_group_t allocation_group = get_migrator_allocation_group ();
    struct migrator_t *migrator_data = (struct migrator_t *) migrator;

    struct struct_migrator_node_t *node = (struct struct_migrator_node_t *) migrator_data->struct_migrators.items.first;
    while (node)
    {
        const uint64_t command_line_size =
            sizeof (struct migrator_condition_t) * node->conditions_count +
            sizeof (struct migrator_command_copy_t) * node->copy_commands_count +
            sizeof (struct migrator_command_adapt_numeric_t) * node->adapt_numeric_commands_count +
            sizeof (struct migrator_command_adapt_enum_t) * node->adapt_enum_commands_count +
            sizeof (struct migrator_command_adapt_dynamic_array_t) * node->adapt_dynamic_array_commands_count +
            sizeof (struct migrator_command_set_zero_t) * node->set_zero_commands_count;
        kan_free_general (allocation_group, node->conditions, command_line_size);

        struct struct_migrator_node_t *next = (struct struct_migrator_node_t *) node->node.list_node.next;
        kan_free_batched (allocation_group, node);
        node = next;
    }

    kan_hash_storage_shutdown (&migrator_data->struct_migrators);
    kan_free_batched (allocation_group, migrator_data);
}
