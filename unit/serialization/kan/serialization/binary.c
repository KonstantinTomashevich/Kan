#define _CRT_SECURE_NO_WARNINGS

#include <stddef.h>
#include <string.h>

#include <kan/api_common/alignment.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/hash_storage.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/reflection/field_visibility_iterator.h>
#include <kan/reflection/patch.h>
#include <kan/serialization/binary.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (serialization_binary);

enum script_command_type_t
{
    SCRIPT_COMMAND_BLOCK = 0u,
    SCRIPT_COMMAND_STRING,
    SCRIPT_COMMAND_INTERNED_STRING,
    SCRIPT_COMMAND_BLOCK_DYNAMIC_ARRAY,
    SCRIPT_COMMAND_STRING_DYNAMIC_ARRAY,
    SCRIPT_COMMAND_INTERNED_STRING_DYNAMIC_ARRAY,
    SCRIPT_COMMAND_STRUCT_DYNAMIC_ARRAY,
    SCRIPT_COMMAND_PATCH_DYNAMIC_ARRAY,
    SCRIPT_COMMAND_PATCH,
};

struct script_command_block_t
{
    uint32_t size;
};

struct script_command_struct_dynamic_array_t
{
    kan_interned_string_t type_name;
};

#define SCRIPT_NO_CONDITION UINT32_MAX

struct script_command_t
{
    enum script_command_type_t type;
    uint32_t condition_index;
    uint32_t offset;

    union
    {
        struct script_command_block_t block;
        struct script_command_struct_dynamic_array_t struct_dynamic_array;
    };
};

struct script_condition_t
{
    struct kan_reflection_field_t *condition_value_field;
    uint32_t absolute_source_offset;
    uint32_t condition_values_count;
    int64_t *condition_values;
    uint32_t parent_condition_index;
};

struct script_t
{
    uint32_t conditions_count;
    uint32_t commands_count;

    // First conditions, then commands.
    uint64_t data[];
};

struct script_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t type_name;
    struct kan_atomic_int_t script_generation_lock;
    struct script_t *script;
};

struct interned_string_lookup_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t type_name;
    struct kan_atomic_int_t interned_string_absolute_positions_generation_lock;
    struct kan_atomic_int_t interned_string_absolute_positions_generated;
    uint32_t interned_string_absolute_positions_count;
    uint32_t *interned_string_absolute_positions;
};

struct script_storage_t
{
    kan_reflection_registry_t registry;

    struct kan_atomic_int_t script_storage_lock;
    struct kan_hash_storage_t script_storage;

    struct kan_atomic_int_t interned_string_lookup_storage_lock;
    struct kan_hash_storage_t interned_string_lookup_storage;
};

struct interned_string_registry_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t value;
    uint32_t index;
};

struct interned_string_registry_t
{
    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t index_to_value;

    kan_bool_t load_only;
    struct kan_atomic_int_t store_lock;
    struct kan_hash_storage_t value_to_index;
};

struct interned_string_registry_reader_t
{
    struct interned_string_registry_t *registry;
    struct kan_stream_t *stream;

    uint32_t strings_total;
    uint32_t strings_read;
};

struct interned_string_registry_writer_t
{
    struct interned_string_registry_t *registry;
    struct kan_stream_t *stream;

    uint32_t strings_total;
    uint32_t strings_written;
};

enum serialization_condition_value_t
{
    SERIALIZATION_CONDITION_NOT_CALCULATED = 0,
    SERIALIZATION_CONDITION_FAILED,
    SERIALIZATION_CONDITION_PASSED,
};

struct script_state_dynamic_array_suffix_t
{
    uint32_t items_total;
    uint32_t items_processed;
};

struct script_state_patch_read_suffix_t
{
    uint32_t blocks_total;
    uint32_t blocks_processed;
};

struct script_state_patch_write_suffix_t
{
    kan_reflection_patch_iterator_t current_iterator;
    kan_reflection_patch_iterator_t end_iterator;
};

struct script_state_patch_suffix_t
{
    kan_interned_string_t type_name;

    union
    {
        struct script_state_patch_read_suffix_t read;
        struct script_state_patch_write_suffix_t write;
    };
};

struct script_state_patch_dynamic_array_suffix_t
{
    struct script_state_dynamic_array_suffix_t array;
    struct script_state_patch_suffix_t current_patch;
};

struct script_state_t
{
    struct script_t *script;
    void *instance;

    enum serialization_condition_value_t *condition_values;
    uint32_t command_to_process_index;
    kan_bool_t condition_checked;
    kan_bool_t suffix_initialized;

    union
    {
        struct script_state_dynamic_array_suffix_t suffix_dynamic_array;
        struct script_state_patch_suffix_t suffix_patch;
        struct script_state_patch_dynamic_array_suffix_t suffix_patch_dynamic_array;
    };
};

struct serialization_common_state_t
{
    struct script_storage_t *script_storage;
    struct interned_string_registry_t *optional_string_registry;
    struct kan_stream_t *stream;

    /// \meta reflection_dynamic_array_type = "struct script_state_t"
    struct kan_dynamic_array_t script_state_stack;
};

struct serialization_read_state_t
{
    struct serialization_common_state_t common;

    uint64_t buffer_size;
    void *buffer;

    kan_reflection_patch_builder_t patch_builder;
    kan_allocation_group_t child_allocation_group;
};

struct serialization_write_state_t
{
    struct serialization_common_state_t common;
};

static kan_allocation_group_t script_storage_allocation_group;
static kan_allocation_group_t script_allocation_group;
static kan_allocation_group_t interned_string_lookup_allocation_group;
static kan_allocation_group_t script_generation_allocation_group;
static kan_allocation_group_t interned_string_lookup_generation_allocation_group;
static kan_allocation_group_t interned_string_registry_allocation_group;
static kan_allocation_group_t interned_string_registry_read_allocation_group;
static kan_allocation_group_t interned_string_registry_write_allocation_group;
static kan_allocation_group_t serialization_allocation_group;

static kan_bool_t statics_initialized = KAN_FALSE;
static struct kan_atomic_int_t statics_initialization_lock = {.value = 0};

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        kan_atomic_int_lock (&statics_initialization_lock);
        if (!statics_initialized)
        {
            script_storage_allocation_group =
                kan_allocation_group_get_child (kan_allocation_group_root (), "serialization_binary_script_storage");
            script_allocation_group = kan_allocation_group_get_child (script_storage_allocation_group, "script");
            interned_string_lookup_allocation_group =
                kan_allocation_group_get_child (script_storage_allocation_group, "interned_string_lookup");
            script_generation_allocation_group =
                kan_allocation_group_get_child (script_storage_allocation_group, "script_generation");
            interned_string_lookup_generation_allocation_group =
                kan_allocation_group_get_child (script_storage_allocation_group, "interned_string_lookup_generation");
            interned_string_registry_allocation_group = kan_allocation_group_get_child (
                kan_allocation_group_root (), "serialization_binary_interned_string_registry");
            interned_string_registry_read_allocation_group =
                kan_allocation_group_get_child (interned_string_registry_allocation_group, "read");
            interned_string_registry_write_allocation_group =
                kan_allocation_group_get_child (interned_string_registry_allocation_group, "write");
            serialization_allocation_group =
                kan_allocation_group_get_child (kan_allocation_group_root (), "serialization_binary");
            statics_initialized = KAN_TRUE;
        }

        kan_atomic_int_unlock (&statics_initialization_lock);
    }
}

static struct script_node_t *script_storage_get_script_internal (struct script_storage_t *storage,
                                                                 kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&storage->script_storage, (uint64_t) type_name);
    struct script_node_t *node = (struct script_node_t *) bucket->first;
    const struct script_node_t *node_end = (struct script_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->type_name == type_name)
        {
            return node;
        }

        node = (struct script_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static struct script_node_t *script_storage_get_or_create_script (struct script_storage_t *storage,
                                                                  kan_interned_string_t type_name)
{
    kan_atomic_int_lock (&storage->script_storage_lock);
    struct script_node_t *node = script_storage_get_script_internal (storage, type_name);

    if (!node)
    {
        ensure_statics_initialized ();
        node = (struct script_node_t *) kan_allocate_batched (script_allocation_group, sizeof (struct script_node_t));
        node->node.hash = (uint64_t) type_name;
        node->type_name = type_name;
        node->script_generation_lock = kan_atomic_int_init (0);
        node->script = NULL;

        if (storage->script_storage.items.size >=
            storage->script_storage.bucket_count * KAN_SERIALIZATION_BINARY_SCRIPT_LOAD_FACTOR)
        {
            kan_hash_storage_set_bucket_count (&storage->script_storage, storage->script_storage.bucket_count * 2u);
        }

        kan_hash_storage_add (&storage->script_storage, &node->node);
    }

    kan_atomic_int_unlock (&storage->script_storage_lock);
    return node;
}

struct script_condition_temporary_node_t
{
    struct script_condition_temporary_node_t *next;
    struct script_condition_t condition;
};

struct script_command_temporary_node_t
{
    struct script_command_temporary_node_t *next;
    struct script_command_t command;
};

struct generation_temporary_state_t
{
    struct script_storage_t *storage;
    const struct kan_reflection_struct_t *struct_data;

    uint32_t conditions_count;
    uint32_t commands_count;

    struct script_condition_temporary_node_t *first_condition;
    struct script_condition_temporary_node_t *last_condition;

    struct script_command_temporary_node_t *first_command;
    struct script_command_temporary_node_t *last_command;

    struct kan_stack_group_allocator_t temporary_allocator;
};

static inline struct script_condition_t build_condition_from_reflection (struct kan_reflection_field_t *source_field)
{
    return (struct script_condition_t) {
        .condition_value_field = source_field->visibility_condition_field,
        .absolute_source_offset = (uint32_t) source_field->visibility_condition_field->offset,
        .condition_values_count = (uint32_t) source_field->visibility_condition_values_count,
        .condition_values = source_field->visibility_condition_values,
        .parent_condition_index = SCRIPT_NO_CONDITION,
    };
}

static inline uint32_t find_condition (struct generation_temporary_state_t *state, struct script_condition_t condition)
{
    struct script_condition_temporary_node_t *other_condition_node = state->first_condition;
    uint32_t condition_index = 0u;

    while (other_condition_node)
    {
        if (other_condition_node->condition.condition_value_field == condition.condition_value_field &&
            other_condition_node->condition.absolute_source_offset == condition.absolute_source_offset &&
            other_condition_node->condition.condition_values_count == condition.condition_values_count &&
            // We can just compare values pointers, because we are pointing to reflection data.
            other_condition_node->condition.condition_values == condition.condition_values &&
            other_condition_node->condition.parent_condition_index == condition.parent_condition_index)
        {
            return condition_index;
        }

        other_condition_node = other_condition_node->next;
        ++condition_index;
    }

    return SCRIPT_NO_CONDITION;
}

static inline void add_condition (struct generation_temporary_state_t *state, struct script_condition_t condition)
{
    struct script_condition_temporary_node_t *new_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &state->temporary_allocator, struct script_condition_temporary_node_t);
    new_node->next = NULL;
    new_node->condition = condition;

    if (state->last_condition)
    {
        state->last_condition->next = new_node;
        state->last_condition = new_node;
    }
    else
    {
        state->first_condition = new_node;
        state->last_condition = new_node;
    }

    ++state->conditions_count;
}

static inline struct script_command_t build_block_command (uint32_t condition_index, uint32_t offset, uint32_t size)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_BLOCK,
        .condition_index = condition_index,
        .offset = offset,
        .block =
            {
                .size = size,
            },
    };
}

static inline struct script_command_t build_string_command (uint32_t condition_index, uint32_t offset)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_STRING,
        .condition_index = condition_index,
        .offset = offset,
    };
}

static inline struct script_command_t build_interned_string_command (uint32_t condition_index, uint32_t offset)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_INTERNED_STRING,
        .condition_index = condition_index,
        .offset = offset,
    };
}

static inline struct script_command_t build_block_dynamic_array_command (uint32_t condition_index,
                                                                         uint32_t offset,
                                                                         uint32_t item_size)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_BLOCK_DYNAMIC_ARRAY,
        .condition_index = condition_index,
        .offset = offset,
    };
}

static inline struct script_command_t build_string_dynamic_array_command (uint32_t condition_index, uint32_t offset)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_STRING_DYNAMIC_ARRAY,
        .condition_index = condition_index,
        .offset = offset,
    };
}

static inline struct script_command_t build_interned_string_dynamic_array_command (uint32_t condition_index,
                                                                                   uint32_t offset)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_INTERNED_STRING_DYNAMIC_ARRAY,
        .condition_index = condition_index,
        .offset = offset,
    };
}

static inline struct script_command_t build_struct_dynamic_array_command (uint32_t condition_index,
                                                                          uint32_t offset,
                                                                          kan_interned_string_t type_name)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_STRUCT_DYNAMIC_ARRAY,
        .condition_index = condition_index,
        .offset = offset,
        .struct_dynamic_array =
            {
                .type_name = type_name,
            },
    };
}

static inline struct script_command_t build_patch_dynamic_array_command (uint32_t condition_index, uint32_t offset)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_PATCH_DYNAMIC_ARRAY,
        .condition_index = condition_index,
        .offset = offset,
    };
}

static inline struct script_command_t build_patch_command (uint32_t condition_index, uint32_t offset)
{
    return (struct script_command_t) {
        .type = SCRIPT_COMMAND_PATCH,
        .condition_index = condition_index,
        .offset = offset,
    };
}

static inline void add_command (struct generation_temporary_state_t *state, struct script_command_t command)
{
    // We merge block commands in order to perform serialization more effectively.
    if (command.type == SCRIPT_COMMAND_BLOCK && state->last_command &&
        state->last_command->command.type == SCRIPT_COMMAND_BLOCK &&
        state->last_command->command.condition_index == command.condition_index &&
        state->last_command->command.offset + state->last_command->command.block.size == command.offset)
    {
        state->last_command->command.block.size += command.block.size;
        return;
    }

    struct script_command_temporary_node_t *new_node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&state->temporary_allocator, struct script_command_temporary_node_t);

    new_node->next = NULL;
    new_node->command.type = command.type;
    new_node->command.condition_index = command.condition_index;
    new_node->command.offset = command.offset;

    switch (command.type)
    {
    case SCRIPT_COMMAND_BLOCK:
        new_node->command.block = command.block;
        break;

    case SCRIPT_COMMAND_STRING:
    case SCRIPT_COMMAND_INTERNED_STRING:
    case SCRIPT_COMMAND_BLOCK_DYNAMIC_ARRAY:
    case SCRIPT_COMMAND_STRING_DYNAMIC_ARRAY:
    case SCRIPT_COMMAND_INTERNED_STRING_DYNAMIC_ARRAY:
    case SCRIPT_COMMAND_PATCH_DYNAMIC_ARRAY:
    case SCRIPT_COMMAND_PATCH:
        break;

    case SCRIPT_COMMAND_STRUCT_DYNAMIC_ARRAY:
        new_node->command.struct_dynamic_array = command.struct_dynamic_array;
        break;
    }

    if (state->last_command)
    {
        state->last_command->next = new_node;
        state->last_command = new_node;
    }
    else
    {
        state->first_command = new_node;
        state->last_command = new_node;
    }

    ++state->commands_count;
}

static void script_storage_ensure_script_generated (struct script_storage_t *storage, struct script_node_t *node);

static inline void add_struct_commands (struct generation_temporary_state_t *state,
                                        kan_interned_string_t type_name,
                                        uint64_t field_offset,
                                        uint32_t condition_index)
{
    struct script_node_t *script_node = script_storage_get_or_create_script (state->storage, type_name);
    script_storage_ensure_script_generated (state->storage, script_node);

    const uint32_t condition_index_offset = state->conditions_count;
    const struct script_condition_t *condition = (const struct script_condition_t *) script_node->script->data;

    for (uint32_t index = 0u; index < script_node->script->conditions_count; ++index, ++condition)
    {
        struct script_condition_t new_condition = *condition;
        new_condition.absolute_source_offset += field_offset;

        if (new_condition.parent_condition_index == SCRIPT_NO_CONDITION)
        {
            new_condition.parent_condition_index = condition_index;
        }
        else
        {
            new_condition.parent_condition_index += condition_index_offset;
        }

        add_condition (state, new_condition);
    }

    const struct script_command_t *command = (const struct script_command_t *) condition;
    for (uint32_t index = 0u; index < script_node->script->commands_count; ++index, ++command)
    {
        struct script_command_t new_command = *command;
        if (new_command.condition_index == SCRIPT_NO_CONDITION)
        {
            new_command.condition_index = condition_index;
        }
        else
        {
            new_command.condition_index += condition_index_offset;
        }

        new_command.offset += field_offset;
        add_command (state, new_command);
    }
}

static inline void add_field_to_commands (struct generation_temporary_state_t *state,
                                          struct kan_reflection_field_t *field,
                                          uint32_t padding_to_include,
                                          uint32_t condition_index)
{
    switch (field->archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_FLOATING:
    case KAN_REFLECTION_ARCHETYPE_ENUM:
        add_command (state, build_block_command (condition_index, field->offset, field->size));
        break;

    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        add_command (state, build_string_command (condition_index, field->offset));
        break;

    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
        add_command (state, build_interned_string_command (condition_index, field->offset));
        break;

    case KAN_REFLECTION_ARCHETYPE_STRUCT:
        add_struct_commands (state, field->archetype_struct.type_name, field->offset, condition_index);
        break;

    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        switch (field->archetype_inline_array.item_archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
            add_command (state, build_block_command (condition_index, field->offset,
                                                     field->archetype_inline_array.item_size *
                                                         field->archetype_inline_array.item_count));
            break;

        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            for (uint64_t index = 0u; index < field->archetype_inline_array.item_count; ++index)
            {
                add_command (state,
                             build_string_command (condition_index,
                                                   field->offset + field->archetype_inline_array.item_size * index));
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            for (uint64_t index = 0u; index < field->archetype_inline_array.item_count; ++index)
            {
                add_command (state,
                             build_interned_string_command (
                                 condition_index, field->offset + field->archetype_inline_array.item_size * index));
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
            for (uint64_t index = 0u; index < field->archetype_inline_array.item_count; ++index)
            {
                add_struct_commands (state, field->archetype_inline_array.item_archetype_struct.type_name,
                                     field->offset + field->archetype_inline_array.item_size * index, condition_index);
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                     "Struct \"%s\" contains field \"%s\" that is an inline array of external pointers and cannot be "
                     "serialized.",
                     state->struct_data->name, field->name)
            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                     "Struct \"%s\" contains field \"%s\" that is an inline array of struct pointers and cannot be "
                     "serialized.",
                     state->struct_data->name, field->name)
            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            KAN_ASSERT (KAN_FALSE)
            break;

        case KAN_REFLECTION_ARCHETYPE_PATCH:
            for (uint64_t index = 0u; index < field->archetype_inline_array.item_count; ++index)
            {
                add_command (state,
                             build_patch_command (condition_index,
                                                  field->offset + field->archetype_inline_array.item_size * index));
            }

            break;
        }

        break;

    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        switch (field->archetype_dynamic_array.item_archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
            add_command (state, build_block_dynamic_array_command (condition_index, field->offset,
                                                                   field->archetype_dynamic_array.item_size));
            break;

        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            add_command (state, build_string_dynamic_array_command (condition_index, field->offset));
            break;

        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            add_command (state, build_interned_string_dynamic_array_command (condition_index, field->offset));
            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
            add_command (state, build_struct_dynamic_array_command (
                                    condition_index, field->offset,
                                    field->archetype_dynamic_array.item_archetype_struct.type_name));
            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                     "Struct \"%s\" contains field \"%s\" that is a dynamic array of struct pointers and cannot be "
                     "serialized.",
                     state->struct_data->name, field->name)
            break;

        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                     "Struct \"%s\" contains field \"%s\" that is a dynamic array of external pointers and cannot be "
                     "serialized.",
                     state->struct_data->name, field->name)
            break;

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            KAN_ASSERT (KAN_FALSE)
            break;

        case KAN_REFLECTION_ARCHETYPE_PATCH:
            add_command (state, build_patch_dynamic_array_command (condition_index, field->offset));
            break;
        }

        break;

    case KAN_REFLECTION_ARCHETYPE_PATCH:
        add_command (state, build_patch_command (condition_index, field->offset));
        break;

    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                 "Struct \"%s\" contains field \"%s\" that is an external pointer and cannot be serialized.",
                 state->struct_data->name, field->name)
        break;

    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                 "Struct \"%s\" contains field \"%s\" that is a struct pointer and cannot be serialized.",
                 state->struct_data->name, field->name)
        break;
    }

    if (state->last_command && state->last_command->command.type == SCRIPT_COMMAND_BLOCK)
    {
        state->last_command->command.block.size += padding_to_include;
    }
}

static void script_storage_ensure_script_generated (struct script_storage_t *storage, struct script_node_t *node)
{
    if (node->script)
    {
        return;
    }

    kan_atomic_int_lock (&node->script_generation_lock);
    if (node->script)
    {
        kan_atomic_int_unlock (&node->script_generation_lock);
        return;
    }

    ensure_statics_initialized ();
    struct generation_temporary_state_t state;
    state.storage = storage;

    state.struct_data = kan_reflection_registry_query_struct (storage->registry, node->type_name);
    KAN_ASSERT (state.struct_data)

    state.conditions_count = 0u;
    state.commands_count = 0u;

    state.first_condition = NULL;
    state.last_condition = NULL;

    state.first_command = NULL;
    state.last_command = NULL;

    kan_stack_group_allocator_init (&state.temporary_allocator, script_generation_allocation_group,
                                    KAN_SERIALIZATION_BINARY_GENERATION_INITIAL_STACK);

    for (uint64_t field_index = 0u; field_index < state.struct_data->fields_count; ++field_index)
    {
        struct kan_reflection_field_t *field = &state.struct_data->fields[field_index];
        uint32_t condition_index = SCRIPT_NO_CONDITION;

        if (field->visibility_condition_field)
        {
            const struct script_condition_t condition = build_condition_from_reflection (field);
            condition_index = find_condition (&state, condition);

            if (condition_index == SCRIPT_NO_CONDITION)
            {
                add_condition (&state, condition);
            }
        }

        uint32_t padding_to_include = 0u;

        // It only makes sense to include paddings if we're not part of the union.
        if (condition_index == SCRIPT_NO_CONDITION)
        {
            const uint32_t field_end = field->offset + field->size;
            if (field_index + 1u != state.struct_data->fields_count)
            {
                struct kan_reflection_field_t *next_field = &state.struct_data->fields[field_index + 1u];
                if (!next_field->visibility_condition_field)
                {
                    padding_to_include = next_field->offset - field_end;
                }
            }
            else if (field_end % state.struct_data->alignment != 0u)
            {
                padding_to_include = state.struct_data->alignment - (field_end % state.struct_data->alignment);
            }
        }

        add_field_to_commands (&state, field, padding_to_include, condition_index);
    }

    _Static_assert (_Alignof (struct script_t) == _Alignof (struct script_condition_t),
                    "Script parts alignment match.");
    _Static_assert (_Alignof (struct script_t) == _Alignof (struct script_command_t), "Script parts alignment match.");

    struct script_t *script = (struct script_t *) kan_allocate_general (
        script_allocation_group,
        sizeof (struct script_t) + state.conditions_count * sizeof (struct script_condition_t) +
            state.commands_count * sizeof (struct script_command_t),
        _Alignof (struct script_t));

    script->conditions_count = state.conditions_count;
    script->commands_count = state.commands_count;

    struct script_condition_t *condition_output = (struct script_condition_t *) script->data;
    struct script_condition_temporary_node_t *condition = state.first_condition;

    while (condition)
    {
        *condition_output = condition->condition;
        ++condition_output;
        condition = condition->next;
    }

    struct script_command_t *command_output = (struct script_command_t *) condition_output;
    struct script_command_temporary_node_t *command = state.first_command;

    while (command)
    {
        command_output->type = command->command.type;
        command_output->condition_index = command->command.condition_index;
        command_output->offset = command->command.offset;

        switch (command->command.type)
        {
        case SCRIPT_COMMAND_BLOCK:
            command_output->block = command->command.block;
            break;

        case SCRIPT_COMMAND_STRING:
        case SCRIPT_COMMAND_INTERNED_STRING:
        case SCRIPT_COMMAND_BLOCK_DYNAMIC_ARRAY:
        case SCRIPT_COMMAND_STRING_DYNAMIC_ARRAY:
        case SCRIPT_COMMAND_INTERNED_STRING_DYNAMIC_ARRAY:
        case SCRIPT_COMMAND_PATCH_DYNAMIC_ARRAY:
        case SCRIPT_COMMAND_PATCH:
            break;

        case SCRIPT_COMMAND_STRUCT_DYNAMIC_ARRAY:
            command_output->struct_dynamic_array = command->command.struct_dynamic_array;
            break;
        }

        ++command_output;
        command = command->next;
    }

    node->script = script;
    kan_stack_group_allocator_shutdown (&state.temporary_allocator);
    kan_atomic_int_unlock (&node->script_generation_lock);
}

static struct interned_string_lookup_node_t *script_storage_get_interned_string_lookup_internal (
    struct script_storage_t *storage, kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&storage->interned_string_lookup_storage, (uint64_t) type_name);
    struct interned_string_lookup_node_t *node = (struct interned_string_lookup_node_t *) bucket->first;
    const struct interned_string_lookup_node_t *node_end =
        (struct interned_string_lookup_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->type_name == type_name)
        {
            return node;
        }

        node = (struct interned_string_lookup_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static struct interned_string_lookup_node_t *script_storage_get_or_create_interned_string_lookup (
    struct script_storage_t *storage, kan_interned_string_t type_name)
{
    kan_atomic_int_lock (&storage->interned_string_lookup_storage_lock);
    struct interned_string_lookup_node_t *node =
        script_storage_get_interned_string_lookup_internal (storage, type_name);

    if (!node)
    {
        ensure_statics_initialized ();
        node = (struct interned_string_lookup_node_t *) kan_allocate_batched (
            interned_string_lookup_allocation_group, sizeof (struct interned_string_lookup_node_t));

        node->node.hash = (uint64_t) type_name;
        node->type_name = type_name;
        node->interned_string_absolute_positions_generation_lock = kan_atomic_int_init (0);
        node->interned_string_absolute_positions_generated = kan_atomic_int_init (0);
        node->interned_string_absolute_positions_count = 0u;
        node->interned_string_absolute_positions = NULL;

        if (storage->interned_string_lookup_storage.items.size >=
            storage->interned_string_lookup_storage.bucket_count * KAN_SERIALIZATION_BINARY_INTERNED_STRING_LOAD_FACTOR)
        {
            kan_hash_storage_set_bucket_count (&storage->interned_string_lookup_storage,
                                               storage->interned_string_lookup_storage.bucket_count * 2u);
        }

        kan_hash_storage_add (&storage->interned_string_lookup_storage, &node->node);
    }

    kan_atomic_int_unlock (&storage->interned_string_lookup_storage_lock);
    return node;
}

static inline void add_to_unsigned_32_array (struct kan_dynamic_array_t *array, uint32_t value)
{
    void *spot = kan_dynamic_array_add_last (array);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (array, array->capacity * 2u);
        spot = kan_dynamic_array_add_last (array);
    }

    KAN_ASSERT (spot)
    *(uint32_t *) spot = value;
}

static void script_storage_ensure_interned_string_lookup_generated (struct script_storage_t *storage,
                                                                    struct interned_string_lookup_node_t *node);

static inline void add_struct_interned_string_lookup (struct kan_dynamic_array_t *temporary_array,
                                                      struct script_storage_t *storage,
                                                      kan_interned_string_t type_name,
                                                      uint32_t offset)
{
    struct interned_string_lookup_node_t *other_node =
        script_storage_get_or_create_interned_string_lookup (storage, type_name);
    script_storage_ensure_interned_string_lookup_generated (storage, other_node);

    for (uint32_t index = 0u; index < other_node->interned_string_absolute_positions_count; ++index)
    {
        add_to_unsigned_32_array (temporary_array, other_node->interned_string_absolute_positions[index] + offset);
    }
}

static inline kan_bool_t error_if_struct_has_interned_strings (struct script_storage_t *storage,
                                                               kan_interned_string_t type_name)
{
    const struct kan_reflection_struct_t *struct_data =
        kan_reflection_registry_query_struct (storage->registry, type_name);
    KAN_ASSERT (struct_data)

    for (uint64_t field_index = 0u; field_index < struct_data->fields_count; ++field_index)
    {
        if (struct_data->fields[field_index].archetype == KAN_REFLECTION_ARCHETYPE_INTERNED_STRING ||
            (struct_data->fields[field_index].archetype == KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY &&
             struct_data->fields[field_index].archetype_inline_array.item_archetype ==
                 KAN_REFLECTION_ARCHETYPE_INTERNED_STRING))
        {
            KAN_LOG (serialization_binary, KAN_LOG_ERROR, "    (field path for the error below) %s.%s",
                     struct_data->name, struct_data->fields[field_index].name)
            return KAN_TRUE;
        }
    }

    return KAN_FALSE;
}

static void script_storage_ensure_interned_string_lookup_generated (struct script_storage_t *storage,
                                                                    struct interned_string_lookup_node_t *node)
{
    if (kan_atomic_int_get (&node->interned_string_absolute_positions_generated))
    {
        return;
    }

    kan_atomic_int_lock (&node->interned_string_absolute_positions_generation_lock);
    if (kan_atomic_int_get (&node->interned_string_absolute_positions_generated))
    {
        kan_atomic_int_unlock (&node->interned_string_absolute_positions_generation_lock);
        return;
    }

    ensure_statics_initialized ();
    struct kan_dynamic_array_t temporary_array;
    kan_dynamic_array_init (&temporary_array, KAN_SERIALIZATION_BINARY_INTERNED_STRING_CAPACITY, sizeof (uint32_t),
                            _Alignof (uint32_t), interned_string_lookup_generation_allocation_group);

    const struct kan_reflection_struct_t *struct_data =
        kan_reflection_registry_query_struct (storage->registry, node->type_name);
    KAN_ASSERT (struct_data)

    for (uint64_t field_index = 0u; field_index < struct_data->fields_count; ++field_index)
    {
        struct kan_reflection_field_t *field = &struct_data->fields[field_index];
        if (field->visibility_condition_field)
        {
            switch (field->archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                         "Struct \"%s\" contains field \"%s\" that is an interned string and has visibility condition "
                         "(for example, part of a union). Interned string lookup sequence was requested for this "
                         "struct (possibly due to usage in patches), but this field cannot be safely included due to"
                         "the fact that visibility condition is not guarantied to be stored inside patch.",
                         struct_data->name, field->name)
                break;

            case KAN_REFLECTION_ARCHETYPE_STRUCT:
            {
                if (error_if_struct_has_interned_strings (storage, field->archetype_struct.type_name))
                {
                    KAN_LOG (
                        serialization_binary, KAN_LOG_ERROR,
                        "Struct \"%s\" contains field \"%s\" that is a struct with interned string and has visibility "
                        "condition (for example, part of a union). Interned string lookup sequence was requested for "
                        "this struct (possibly due to usage in patches), but this field cannot be safely included due "
                        "to the fact that visibility condition is not guarantied to be stored inside patch.",
                        struct_data->name, field->name)
                }

                break;
            }

            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                switch (field->archetype_inline_array.item_archetype)
                {
                case KAN_REFLECTION_ARCHETYPE_STRUCT:
                    if (error_if_struct_has_interned_strings (
                            storage, field->archetype_inline_array.item_archetype_struct.type_name))
                    {
                        KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                                 "Struct \"%s\" contains field \"%s\" that is a struct with interned string and has "
                                 "visibility condition (for example, part of a union). Interned string lookup sequence "
                                 "was requested for his struct (possibly due to usage in patches), but this field "
                                 "cannot be safely included due to the fact that visibility condition is not "
                                 "guarantied to be stored inside patch.",
                                 struct_data->name, field->name)
                    }

                    break;

                case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                    KAN_LOG (
                        serialization_binary, KAN_LOG_ERROR,
                        "Struct \"%s\" contains field \"%s\" that is an interned string array and has visibility "
                        "condition (for example, part of a union). Interned string lookup sequence was requested for "
                        "this struct (possibly due to usage in patches), but this field cannot be safely included due "
                        "to the fact that visibility condition is not guarantied to be stored inside patch.",
                        struct_data->name, field->name)

                    break;

                case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
                case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
                case KAN_REFLECTION_ARCHETYPE_FLOATING:
                case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
                case KAN_REFLECTION_ARCHETYPE_ENUM:
                case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
                case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
                case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
                case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
                case KAN_REFLECTION_ARCHETYPE_PATCH:
                    break;
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_FLOATING:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_PATCH:
                break;
            }

            continue;
        }

        KAN_ASSERT (field->offset < UINT32_MAX)
        switch (field->archetype)
        {
        case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
            add_to_unsigned_32_array (&temporary_array, (uint32_t) field->offset);
            break;

        case KAN_REFLECTION_ARCHETYPE_STRUCT:
        {
            add_struct_interned_string_lookup (&temporary_array, storage, field->archetype_struct.type_name,
                                               (uint32_t) field->offset);
            break;
        }

        case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            switch (field->archetype_inline_array.item_archetype)
            {
            case KAN_REFLECTION_ARCHETYPE_STRUCT:
                for (uint64_t index = 0u; index < field->archetype_inline_array.item_count; ++index)
                {
                    add_struct_interned_string_lookup (
                        &temporary_array, storage, field->archetype_inline_array.item_archetype_struct.type_name,
                        (uint32_t) (field->offset + index * field->archetype_inline_array.item_size));
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
                for (uint64_t index = 0u; index < field->archetype_inline_array.item_count; ++index)
                {
                    add_to_unsigned_32_array (&temporary_array,
                                              (uint32_t) (field->offset + index * sizeof (kan_interned_string_t)));
                }

                break;

            case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
            case KAN_REFLECTION_ARCHETYPE_FLOATING:
            case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
            case KAN_REFLECTION_ARCHETYPE_ENUM:
            case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
            case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
            case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
            case KAN_REFLECTION_ARCHETYPE_PATCH:
                break;
            }

            break;

        case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
        case KAN_REFLECTION_ARCHETYPE_FLOATING:
        case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        case KAN_REFLECTION_ARCHETYPE_ENUM:
        case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        case KAN_REFLECTION_ARCHETYPE_PATCH:
            break;
        }
    }

    node->interned_string_absolute_positions_count = (uint32_t) temporary_array.size;
    if (node->interned_string_absolute_positions_count > 0u)
    {
        node->interned_string_absolute_positions = kan_allocate_general (
            interned_string_lookup_allocation_group, sizeof (uint32_t) * node->interned_string_absolute_positions_count,
            _Alignof (uint32_t));

        memcpy (node->interned_string_absolute_positions, temporary_array.data,
                sizeof (uint32_t) * node->interned_string_absolute_positions_count);
    }

    kan_atomic_int_set (&node->interned_string_absolute_positions_generated, 1);
    kan_dynamic_array_shutdown (&temporary_array);
    kan_atomic_int_unlock (&node->interned_string_absolute_positions_generation_lock);
}

static struct interned_string_registry_t *interned_string_registry_create (kan_bool_t load_only)
{
    ensure_statics_initialized ();
    struct interned_string_registry_t *registry = (struct interned_string_registry_t *) kan_allocate_general (
        interned_string_registry_allocation_group, sizeof (struct interned_string_registry_t),
        _Alignof (struct interned_string_registry_t));

    kan_dynamic_array_init (
        &registry->index_to_value,
        KAN_SERIALIZATION_BINARY_INTERNED_REGISTRY_BUCKETS * KAN_SERIALIZATION_BINARY_INTERNED_REGISTRY_LOAD_FACTOR,
        sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), interned_string_registry_allocation_group);

    registry->load_only = load_only;
    if (!registry->load_only)
    {
        registry->store_lock = kan_atomic_int_init (0);
        kan_hash_storage_init (&registry->value_to_index, interned_string_registry_allocation_group,
                               KAN_SERIALIZATION_BINARY_INTERNED_REGISTRY_BUCKETS);
    }

    return registry;
}

static uint32_t interned_string_registry_add_string_internal (struct interned_string_registry_t *registry,
                                                              kan_interned_string_t interned_string)
{
    KAN_ASSERT (registry->index_to_value.size <= UINT32_MAX)
    const uint32_t index = (uint32_t) registry->index_to_value.size;
    void *spot = kan_dynamic_array_add_last (&registry->index_to_value);

    if (!spot)
    {
        kan_dynamic_array_set_capacity (&registry->index_to_value, registry->index_to_value.capacity * 2u);
        spot = kan_dynamic_array_add_last (&registry->index_to_value);
    }

    KAN_ASSERT (spot)
    *(kan_interned_string_t *) spot = interned_string;

    if (!registry->load_only)
    {
        ensure_statics_initialized ();
        struct interned_string_registry_node_t *node = kan_allocate_batched (
            interned_string_registry_allocation_group, sizeof (struct interned_string_registry_node_t));

        node->node.hash = (uint64_t) interned_string;
        node->value = interned_string;
        node->index = index;

        if (registry->value_to_index.items.size >=
            registry->value_to_index.bucket_count * KAN_SERIALIZATION_BINARY_INTERNED_REGISTRY_LOAD_FACTOR)
        {
            kan_hash_storage_set_bucket_count (&registry->value_to_index, registry->value_to_index.bucket_count * 2u);
        }

        kan_hash_storage_add (&registry->value_to_index, &node->node);
    }

    return index;
}

static uint32_t interned_string_registry_store_string (struct interned_string_registry_t *registry,
                                                       kan_interned_string_t interned_string)
{
    KAN_ASSERT (!registry->load_only)
    kan_atomic_int_lock (&registry->store_lock);

    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&registry->value_to_index, (uint64_t) interned_string);
    struct interned_string_registry_node_t *node = (struct interned_string_registry_node_t *) bucket->first;
    const struct interned_string_registry_node_t *node_end =
        (struct interned_string_registry_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->value == interned_string)
        {
            kan_atomic_int_unlock (&registry->store_lock);
            return node->index;
        }

        node = (struct interned_string_registry_node_t *) node->node.list_node.next;
    }

    const int32_t index = interned_string_registry_add_string_internal (registry, interned_string);
    kan_atomic_int_unlock (&registry->store_lock);
    return index;
}

static kan_interned_string_t interned_string_registry_load_string (struct interned_string_registry_t *registry,
                                                                   uint32_t index)
{
    KAN_ASSERT (registry->load_only || kan_atomic_int_get (&registry->store_lock) == 0)
    if (index >= registry->index_to_value.size)
    {
        KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                 "Encountered interned string index that is too big for the registry: %u.", (unsigned) index);
        return NULL;
    }

    return ((kan_interned_string_t *) registry->index_to_value.data)[index];
}

static inline void script_state_go_to_next_command (struct script_state_t *state)
{
    ++state->command_to_process_index;
    state->condition_checked = KAN_FALSE;
    state->suffix_initialized = KAN_FALSE;
}

static void calculate_condition (void *instance,
                                 enum serialization_condition_value_t *condition_array,
                                 uint32_t condition_index,
                                 struct script_condition_t *script_conditions)
{
    KAN_ASSERT (condition_array[condition_index] == SERIALIZATION_CONDITION_NOT_CALCULATED)
    kan_bool_t condition_valid = KAN_TRUE;
    const uint32_t parent_condition = script_conditions[condition_index].parent_condition_index;

    if (parent_condition != SCRIPT_NO_CONDITION)
    {
        if (condition_array[parent_condition] == SERIALIZATION_CONDITION_NOT_CALCULATED)
        {
            calculate_condition (instance, condition_array, parent_condition, script_conditions);
        }

        if (condition_array[parent_condition] == SERIALIZATION_CONDITION_FAILED)
        {
            condition_valid = KAN_FALSE;
        }
    }

    condition_valid &= kan_reflection_check_visibility (
        script_conditions[condition_index].condition_value_field,
        script_conditions[condition_index].condition_values_count, script_conditions[condition_index].condition_values,
        ((const uint8_t *) instance) + script_conditions[condition_index].absolute_source_offset);

    if (condition_valid)
    {
        condition_array[condition_index] = SERIALIZATION_CONDITION_PASSED;
    }
    else
    {
        condition_array[condition_index] = SERIALIZATION_CONDITION_FAILED;
    }
}

static inline void serialization_common_state_init (
    struct serialization_common_state_t *state,
    struct kan_stream_t *stream,
    kan_serialization_binary_script_storage_t script_storage,
    kan_serialization_interned_string_registry_t interned_string_registry)
{
    state->script_storage = (struct script_storage_t *) script_storage;
    KAN_ASSERT (state->script_storage != NULL);
    state->optional_string_registry = (struct interned_string_registry_t *) interned_string_registry;
    state->stream = stream;

    kan_dynamic_array_init (&state->script_state_stack, 4u, sizeof (struct script_state_t),
                            _Alignof (struct script_state_t), serialization_allocation_group);
}

static inline void serialization_common_state_push_script_state (
    struct serialization_common_state_t *serialization_state,
    struct script_t *script,
    void *instance,
    kan_bool_t calculate_conditions)
{
    struct script_state_t *script_state =
        (struct script_state_t *) kan_dynamic_array_add_last (&serialization_state->script_state_stack);
    if (!script_state)
    {
        kan_dynamic_array_set_capacity (&serialization_state->script_state_stack,
                                        serialization_state->script_state_stack.capacity * 2u);
    }

    KAN_ASSERT (script_state)
    script_state->script = script;
    script_state->instance = instance;

    if (script->conditions_count > 0u)
    {
        ensure_statics_initialized ();
        script_state->condition_values = kan_allocate_general (
            serialization_allocation_group, sizeof (enum serialization_condition_value_t) * script->conditions_count,
            _Alignof (enum serialization_condition_value_t));

        if (calculate_conditions)
        {
            for (uint64_t index = 0u; index < script->conditions_count; ++index)
            {
                calculate_condition (instance, script_state->condition_values, index,
                                     (struct script_condition_t *) script->data);
            }
        }
        else
        {
            for (uint64_t index = 0u; index < script->conditions_count; ++index)
            {
                script_state->condition_values[index] = SERIALIZATION_CONDITION_NOT_CALCULATED;
            }
        }
    }
    else
    {
        script_state->condition_values = NULL;
    }

    script_state->command_to_process_index = 0u;
    script_state->suffix_initialized = KAN_FALSE;
}

static inline void serialization_common_state_pop_script_state (
    struct serialization_common_state_t *serialization_state)
{
    KAN_ASSERT (serialization_state->script_state_stack.size > 0u)
    struct script_state_t *last_state =
        &((struct script_state_t *)
              serialization_state->script_state_stack.data)[serialization_state->script_state_stack.size - 1u];

    kan_free_general (serialization_allocation_group, last_state->condition_values,
                      sizeof (enum serialization_condition_value_t) * last_state->script->conditions_count);

    kan_dynamic_array_remove_swap_at (&serialization_state->script_state_stack,
                                      serialization_state->script_state_stack.size - 1u);
}

static inline void serialization_common_state_shutdown (struct serialization_common_state_t *state)
{
    while (state->script_state_stack.size > 0u)
    {
        serialization_common_state_pop_script_state (state);
    }

    kan_dynamic_array_shutdown (&state->script_state_stack);
}

kan_serialization_binary_script_storage_t kan_serialization_binary_script_storage_create (
    kan_reflection_registry_t registry)
{
    ensure_statics_initialized ();
    struct script_storage_t *script_storage = (struct script_storage_t *) kan_allocate_general (
        script_storage_allocation_group, sizeof (struct script_storage_t), _Alignof (struct script_storage_t));

    script_storage->registry = registry;
    script_storage->script_storage_lock = kan_atomic_int_init (0);

    kan_hash_storage_init (&script_storage->script_storage, script_allocation_group,
                           KAN_SERIALIZATION_BINARY_SCRIPT_INITIAL_BUCKETS);

    script_storage->interned_string_lookup_storage_lock = kan_atomic_int_init (0);
    kan_hash_storage_init (&script_storage->interned_string_lookup_storage, interned_string_lookup_allocation_group,
                           KAN_SERIALIZATION_BINARY_INTERNED_STRING_BUCKETS);

    return (kan_serialization_binary_script_storage_t) script_storage;
}

void kan_serialization_binary_script_storage_destroy (kan_serialization_binary_script_storage_t storage)
{
    ensure_statics_initialized ();
    struct script_storage_t *script_storage = (struct script_storage_t *) storage;
    struct script_node_t *script_node = (struct script_node_t *) script_storage->script_storage.items.first;

    while (script_node)
    {
        struct script_node_t *next = (struct script_node_t *) script_node->node.list_node.next;
        if (script_node->script)
        {
            kan_free_general (script_allocation_group, script_node->script,
                              sizeof (struct script_t) +
                                  sizeof (struct script_condition_t) * script_node->script->conditions_count +
                                  sizeof (struct script_command_t) * script_node->script->commands_count);
        }

        kan_free_batched (script_allocation_group, script_node);
        script_node = next;
    }

    struct interned_string_lookup_node_t *lookup_node =
        (struct interned_string_lookup_node_t *) script_storage->interned_string_lookup_storage.items.first;

    while (lookup_node)
    {
        struct interned_string_lookup_node_t *next =
            (struct interned_string_lookup_node_t *) lookup_node->node.list_node.next;

        if (lookup_node->interned_string_absolute_positions)
        {
            kan_free_general (interned_string_lookup_allocation_group, lookup_node->interned_string_absolute_positions,
                              sizeof (uint32_t) * lookup_node->interned_string_absolute_positions_count);
        }

        kan_free_batched (interned_string_lookup_allocation_group, lookup_node);
        lookup_node = next;
    }

    kan_hash_storage_shutdown (&script_storage->script_storage);
    kan_hash_storage_shutdown (&script_storage->interned_string_lookup_storage);
    kan_free_general (script_storage_allocation_group, script_storage, sizeof (struct script_storage_t));
}

kan_serialization_interned_string_registry_t kan_serialization_interned_string_registry_create_empty (void)
{
    return (kan_serialization_interned_string_registry_t) interned_string_registry_create (KAN_FALSE);
}

void kan_serialization_interned_string_registry_destroy (kan_serialization_interned_string_registry_t registry)
{
    struct interned_string_registry_t *data = (struct interned_string_registry_t *) registry;
    kan_dynamic_array_shutdown (&data->index_to_value);

    if (!data->load_only)
    {
        struct interned_string_registry_node_t *node =
            (struct interned_string_registry_node_t *) data->value_to_index.items.first;

        while (node)
        {
            struct interned_string_registry_node_t *next =
                (struct interned_string_registry_node_t *) node->node.list_node.next;
            kan_free_batched (interned_string_registry_allocation_group, node);
            node = next;
        }

        kan_hash_storage_shutdown (&data->value_to_index);
    }

    kan_free_general (interned_string_registry_allocation_group, data, sizeof (struct interned_string_registry_t));
}

kan_serialization_interned_string_registry_reader_t kan_serialization_interned_string_registry_reader_create (
    struct kan_stream_t *stream, kan_bool_t load_only_registry)
{
    ensure_statics_initialized ();
    struct interned_string_registry_reader_t *reader =
        (struct interned_string_registry_reader_t *) kan_allocate_general (
            interned_string_registry_read_allocation_group, sizeof (struct interned_string_registry_reader_t),
            _Alignof (struct interned_string_registry_reader_t));

    reader->registry = interned_string_registry_create (load_only_registry);
    reader->stream = stream;
    KAN_ASSERT (kan_stream_is_readable (stream));

    if (reader->stream->operations->read (reader->stream, sizeof (uint32_t), &reader->strings_total) !=
        sizeof (uint32_t))
    {
        reader->strings_total = 0u;
        KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                 "Failed to initialize interned string registry reader: unable to read strings count from stream.")
    }

    reader->strings_read = 0u;
    kan_dynamic_array_set_capacity (&reader->registry->index_to_value, reader->strings_total);
    return (kan_serialization_interned_string_registry_reader_t) reader;
}

enum kan_serialization_state_t kan_serialization_interned_string_registry_reader_step (
    kan_serialization_interned_string_registry_reader_t reader)
{
    struct interned_string_registry_reader_t *data = (struct interned_string_registry_reader_t *) reader;
    if (data->strings_read >= data->strings_total)
    {
        return KAN_SERIALIZATION_FINISHED;
    }

    uint32_t string_length;
    if (data->stream->operations->read (data->stream, sizeof (uint32_t), &string_length) != sizeof (uint32_t))
    {
        return KAN_SERIALIZATION_FAILED;
    }

    if (string_length == 0u)
    {
        return KAN_SERIALIZATION_FAILED;
    }

#define MAX_SIZE_ON_STACK 1023u
    ensure_statics_initialized ();
    char read_buffer_on_stack[MAX_SIZE_ON_STACK + 1u];
    char *read_buffer;

    if (string_length >= MAX_SIZE_ON_STACK)
    {
        read_buffer =
            kan_allocate_general (interned_string_registry_read_allocation_group, string_length + 1u, _Alignof (char));
    }
    else
    {
        read_buffer = read_buffer_on_stack;
    }
#undef MAX_SIZE_ON_STACK

    const uint32_t read = data->stream->operations->read (data->stream, string_length, read_buffer);
    if (read == string_length)
    {
        read_buffer[string_length] = '\0';
        interned_string_registry_add_string_internal (data->registry, kan_string_intern (read_buffer));
        ++data->strings_read;
    }

    if (read_buffer != read_buffer_on_stack)
    {
        kan_free_general (interned_string_registry_read_allocation_group, read_buffer, string_length + 1u);
    }

    if (read == string_length)
    {
        return data->strings_read >= data->strings_total ? KAN_SERIALIZATION_FINISHED : KAN_SERIALIZATION_IN_PROGRESS;
    }
    else
    {
        return KAN_SERIALIZATION_FAILED;
    }
}

kan_serialization_interned_string_registry_t kan_serialization_interned_string_registry_reader_get (
    kan_serialization_interned_string_registry_reader_t reader)
{
    return (kan_serialization_interned_string_registry_t) ((struct interned_string_registry_reader_t *) reader)
        ->registry;
}

void kan_serialization_interned_string_registry_reader_destroy (
    kan_serialization_interned_string_registry_reader_t reader)
{
    kan_free_general (interned_string_registry_read_allocation_group,
                      (struct interned_string_registry_reader_t *) reader,
                      sizeof (struct interned_string_registry_reader_t));
}

kan_serialization_interned_string_registry_writer_t kan_serialization_interned_string_registry_writer_create (
    struct kan_stream_t *stream, kan_serialization_interned_string_registry_t registry)
{
    ensure_statics_initialized ();
    struct interned_string_registry_writer_t *writer =
        (struct interned_string_registry_writer_t *) kan_allocate_general (
            interned_string_registry_write_allocation_group, sizeof (struct interned_string_registry_writer_t),
            _Alignof (struct interned_string_registry_writer_t));

    KAN_ASSERT (registry != KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY)
    writer->registry = (struct interned_string_registry_t *) registry;
    KAN_ASSERT (kan_stream_is_writeable (stream));
    writer->stream = stream;

    KAN_ASSERT (writer->registry->index_to_value.size <= UINT32_MAX)
    writer->strings_total = (uint32_t) writer->registry->index_to_value.size;
    writer->strings_written = 0u;

    if (writer->stream->operations->write (writer->stream, sizeof (uint32_t), &writer->strings_total) !=
        sizeof (uint32_t))
    {
        writer->strings_total = 0u;
        KAN_LOG (serialization_binary, KAN_LOG_ERROR,
                 "Failed to initialize interned string registry writer: unable to write strings count into stream.")
    }

    return (kan_serialization_interned_string_registry_writer_t) writer;
}

enum kan_serialization_state_t kan_serialization_interned_string_registry_writer_step (
    kan_serialization_interned_string_registry_writer_t writer)
{
    struct interned_string_registry_writer_t *data = (struct interned_string_registry_writer_t *) writer;
    if (data->strings_written >= data->strings_total)
    {
        return KAN_SERIALIZATION_FINISHED;
    }

    kan_interned_string_t string =
        ((kan_interned_string_t *) data->registry->index_to_value.data)[data->strings_written];

    uint64_t string_length_wide = strlen (string);
    KAN_ASSERT (string_length_wide <= UINT32_MAX)
    uint32_t string_length = (uint32_t) string_length_wide;

    if (data->stream->operations->write (data->stream, sizeof (uint32_t), &string_length) != sizeof (uint32_t))
    {
        return KAN_SERIALIZATION_FAILED;
    }

    if (data->stream->operations->write (data->stream, string_length, string) != string_length)
    {
        return KAN_SERIALIZATION_FAILED;
    }

    ++data->strings_written;
    return data->strings_written >= data->strings_total ? KAN_SERIALIZATION_FINISHED : KAN_SERIALIZATION_IN_PROGRESS;
}

void kan_serialization_interned_string_registry_writer_destroy (
    kan_serialization_interned_string_registry_writer_t writer)
{
    kan_free_general (interned_string_registry_read_allocation_group,
                      (struct interned_string_registry_writer_t *) writer,
                      sizeof (struct interned_string_registry_writer_t));
}

kan_serialization_binary_reader_t kan_serialization_binary_reader_create (
    struct kan_stream_t *stream,
    void *instance,
    kan_interned_string_t type_name,
    kan_serialization_binary_script_storage_t script_storage,
    kan_serialization_interned_string_registry_t interned_string_registry,
    kan_allocation_group_t deserialized_string_allocation_group)
{
    ensure_statics_initialized ();
    KAN_ASSERT (kan_stream_is_readable (stream))

    struct serialization_read_state_t *state = (struct serialization_read_state_t *) kan_allocate_general (
        serialization_allocation_group, sizeof (struct serialization_read_state_t),
        _Alignof (struct serialization_read_state_t));

    serialization_common_state_init (&state->common, stream, script_storage, interned_string_registry);
    struct script_node_t *script_node = script_storage_get_or_create_script (state->common.script_storage, type_name);
    script_storage_ensure_script_generated (state->common.script_storage, script_node);
    serialization_common_state_push_script_state (&state->common, script_node->script, instance, KAN_FALSE);

    state->buffer_size = 0u;
    state->buffer = NULL;
    state->patch_builder = KAN_INVALID_REFLECTION_PATCH_BUILDER;
    state->child_allocation_group = deserialized_string_allocation_group;
    return (kan_serialization_binary_reader_t) state;
}

static inline void ensure_read_buffer_size (struct serialization_read_state_t *state, uint64_t required_size)
{
    if (state->buffer_size < required_size)
    {
        if (state->buffer)
        {
            kan_free_general (serialization_allocation_group, state->buffer, state->buffer_size);
        }

        state->buffer_size = kan_apply_alignment (required_size, sizeof (uint64_t));
        state->buffer = kan_allocate_general (serialization_allocation_group, state->buffer_size, _Alignof (uint64_t));
    }
}

static inline kan_bool_t read_string_to_buffer (struct serialization_read_state_t *state,
                                                uint64_t *string_length_output)
{
    uint32_t string_length;
    if (state->common.stream->operations->read (state->common.stream, sizeof (uint32_t), &string_length) !=
        sizeof (uint32_t))
    {
        return KAN_FALSE;
    }

    ensure_read_buffer_size (state, string_length);
    if (state->common.stream->operations->read (state->common.stream, string_length, state->buffer) != string_length)
    {
        return KAN_FALSE;
    }

    *string_length_output = string_length;
    return KAN_TRUE;
}

static inline kan_bool_t read_string_to_new_allocation (struct serialization_read_state_t *state, char **string_output)
{
    uint32_t string_length;
    if (state->common.stream->operations->read (state->common.stream, sizeof (uint32_t), &string_length) !=
        sizeof (uint32_t))
    {
        return KAN_FALSE;
    }

    char *string_memory = kan_allocate_general (state->child_allocation_group, string_length + 1u, _Alignof (char));
    if (state->common.stream->operations->read (state->common.stream, string_length, string_memory) != string_length)
    {
        return KAN_FALSE;
    }

    string_memory[string_length] = '\0';
    *string_output = string_memory;
    return KAN_TRUE;
}

static inline kan_bool_t read_interned_string_stateless (struct kan_stream_t *stream,
                                                         struct interned_string_registry_t *string_registry,
                                                         kan_interned_string_t *output)
{
    if (string_registry)
    {
        uint32_t index;
        if (stream->operations->read (stream, sizeof (uint32_t), &index) != sizeof (uint32_t))
        {
            return KAN_FALSE;
        }

        *output = interned_string_registry_load_string (string_registry, index);
        return KAN_TRUE;
    }
    else
    {
        uint32_t string_length;
        if (stream->operations->read (stream, sizeof (uint32_t), &string_length) != sizeof (uint32_t))
        {
            return KAN_FALSE;
        }

        char *string_memory = kan_allocate_general (serialization_allocation_group, string_length, _Alignof (char));
        if (stream->operations->read (stream, string_length, string_memory) != string_length)
        {
            kan_free_general (serialization_allocation_group, string_memory, string_length);
            return KAN_FALSE;
        }

        *output = kan_char_sequence_intern (string_memory, string_memory + string_length);
        kan_free_general (serialization_allocation_group, string_memory, string_length);
        return KAN_TRUE;
    }
}

static inline kan_bool_t read_interned_string (struct serialization_read_state_t *state, kan_interned_string_t *output)
{
    if (state->common.optional_string_registry)
    {
        uint32_t index;
        if (state->common.stream->operations->read (state->common.stream, sizeof (uint32_t), &index) !=
            sizeof (uint32_t))
        {
            return KAN_FALSE;
        }

        *output = interned_string_registry_load_string (state->common.optional_string_registry, index);
        return KAN_TRUE;
    }
    else
    {
        uint64_t length;
        if (!read_string_to_buffer (state, &length))
        {
            return KAN_FALSE;
        }

        *output = kan_char_sequence_intern ((const char *) state->buffer, (const char *) state->buffer + length);
        return KAN_TRUE;
    }
}

static inline kan_bool_t read_array_or_patch_size (struct serialization_read_state_t *state, uint32_t *output)
{
    return state->common.stream->operations->read (state->common.stream, sizeof (uint32_t), output) ==
           sizeof (uint32_t);
}

static inline kan_bool_t ensure_dynamic_array_read_suffix_ready (struct serialization_read_state_t *state,
                                                                 struct script_state_t *top_state,
                                                                 struct kan_dynamic_array_t *array)
{
    if (!top_state->suffix_initialized)
    {
        if (!read_array_or_patch_size (state, &top_state->suffix_dynamic_array.items_total))
        {
            return KAN_FALSE;
        }

        top_state->suffix_dynamic_array.items_processed = 0u;
        kan_dynamic_array_set_capacity (array, top_state->suffix_dynamic_array.items_total);
        array->size = top_state->suffix_dynamic_array.items_total;
        top_state->suffix_initialized = KAN_TRUE;
    }

    return KAN_TRUE;
}

static inline kan_bool_t init_patch_read_suffix (struct serialization_read_state_t *state,
                                                 struct script_state_patch_suffix_t *suffix)
{
    if (!read_interned_string (state, &suffix->type_name) ||
        !read_array_or_patch_size (state, &suffix->read.blocks_total))
    {
        return KAN_FALSE;
    }

    suffix->read.blocks_processed = 0u;
    return KAN_TRUE;
}

static inline uint64_t upper_or_equal_bound_index (const uint32_t *positions, uint64_t positions_count, uint32_t value)
{
    uint64_t first = 0u;
    uint64_t last = positions_count;

    while (first < last)
    {
        uint64_t middle = (first + last) / 2u;
        if (positions[middle] == value)
        {
            return middle;
        }
        else if (positions[middle] < value)
        {
            first = middle + 1u;
        }
        else
        {
            last = middle;
        }
    }

    return first;
}

struct patch_block_info_t
{
    uint32_t offset;
    uint32_t size;
};

static inline kan_bool_t read_patch_block (struct serialization_read_state_t *state,
                                           struct script_state_patch_suffix_t *suffix)
{
    struct patch_block_info_t block_info;
    if (state->common.stream->operations->read (state->common.stream, sizeof (struct patch_block_info_t),
                                                &block_info) != sizeof (struct patch_block_info_t))
    {
        return KAN_FALSE;
    }

    if (block_info.size == 0u)
    {
        return KAN_TRUE;
    }

    struct interned_string_lookup_node_t *interned_string_lookup_node =
        script_storage_get_or_create_interned_string_lookup (state->common.script_storage, suffix->type_name);
    script_storage_ensure_interned_string_lookup_generated (state->common.script_storage, interned_string_lookup_node);

    uint32_t current_offset = block_info.offset;
    const uint32_t end_offset = block_info.offset + block_info.size;

    uint64_t next_interned_string_index = upper_or_equal_bound_index (
        interned_string_lookup_node->interned_string_absolute_positions,
        interned_string_lookup_node->interned_string_absolute_positions_count, current_offset);

    while (current_offset < end_offset)
    {
        uint32_t serialized_block_end = end_offset;
        if (next_interned_string_index < interned_string_lookup_node->interned_string_absolute_positions_count)
        {
            uint32_t next_string_offset =
                interned_string_lookup_node->interned_string_absolute_positions[next_interned_string_index];

            if (next_string_offset == current_offset)
            {
                kan_interned_string_t string;
                if (!read_interned_string (state, &string))
                {
                    return KAN_FALSE;
                }

                kan_reflection_patch_builder_add_chunk (state->patch_builder, current_offset,
                                                        sizeof (kan_interned_string_t), &string);

                current_offset += sizeof (kan_interned_string_t);
                ++next_interned_string_index;
                continue;
            }
            else if (next_string_offset < serialized_block_end)
            {
                KAN_ASSERT (next_string_offset > current_offset)
                serialized_block_end = next_string_offset;
            }
        }

        const uint32_t size = serialized_block_end - current_offset;
        ensure_read_buffer_size (state, size);

        if (state->common.stream->operations->read (state->common.stream, size, state->buffer) != size)
        {
            return KAN_FALSE;
        }

        kan_reflection_patch_builder_add_chunk (state->patch_builder, current_offset, size, state->buffer);
        current_offset = serialized_block_end;
    }

    return KAN_TRUE;
}

enum kan_serialization_state_t kan_serialization_binary_reader_step (kan_serialization_binary_reader_t reader)
{
    struct serialization_read_state_t *state = (struct serialization_read_state_t *) reader;
    if (state->common.script_state_stack.size == 0u)
    {
        return KAN_SERIALIZATION_FINISHED;
    }

    struct script_state_t *top_state =
        &((struct script_state_t *) state->common.script_state_stack.data)[state->common.script_state_stack.size - 1u];
    KAN_ASSERT (top_state->command_to_process_index < top_state->script->commands_count)

    struct script_condition_t *script_conditions = (struct script_condition_t *) top_state->script->data;
    struct script_command_t *script_commands =
        (struct script_command_t *) (script_conditions + top_state->script->conditions_count);

    struct script_command_t *command_to_process = script_commands + top_state->command_to_process_index;
    kan_bool_t should_be_processed = KAN_TRUE;

    if (command_to_process->condition_index != SCRIPT_NO_CONDITION && !top_state->condition_checked)
    {
        if (top_state->condition_values[command_to_process->condition_index] == SERIALIZATION_CONDITION_NOT_CALCULATED)
        {
            calculate_condition (top_state->instance, top_state->condition_values, command_to_process->condition_index,
                                 script_conditions);
        }

        should_be_processed =
            top_state->condition_values[command_to_process->condition_index] == SERIALIZATION_CONDITION_PASSED;
        top_state->condition_checked = KAN_TRUE;
    }

    if (should_be_processed)
    {
        uint8_t *address = ((uint8_t *) top_state->instance) + command_to_process->offset;
        switch (command_to_process->type)
        {
        case SCRIPT_COMMAND_BLOCK:
            if (state->common.stream->operations->read (state->common.stream, command_to_process->block.size,
                                                        address) != command_to_process->block.size)
            {
                return KAN_SERIALIZATION_FAILED;
            }

            script_state_go_to_next_command (top_state);
            break;

        case SCRIPT_COMMAND_STRING:
        {
            char *string;
            if (!read_string_to_new_allocation (state, &string))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            *((char **) address) = string;
            script_state_go_to_next_command (top_state);
            break;
        }

        case SCRIPT_COMMAND_INTERNED_STRING:
            if (!read_interned_string (state, (kan_interned_string_t *) address))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            script_state_go_to_next_command (top_state);
            break;

        case SCRIPT_COMMAND_BLOCK_DYNAMIC_ARRAY:
        {
            uint32_t size;
            if (!read_array_or_patch_size (state, &size))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            kan_dynamic_array_set_capacity (array, size);
            array->size = size;

            if (state->common.stream->operations->read (state->common.stream, array->size * array->item_size,
                                                        array->data) != array->size * array->item_size)
            {
                return KAN_SERIALIZATION_FAILED;
            }

            script_state_go_to_next_command (top_state);
            break;
        }

        case SCRIPT_COMMAND_STRING_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!ensure_dynamic_array_read_suffix_ready (state, top_state, array))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            if (top_state->suffix_dynamic_array.items_processed < top_state->suffix_dynamic_array.items_total)
            {
                if (!read_string_to_new_allocation (
                        state, ((char **) array->data) + top_state->suffix_dynamic_array.items_processed))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                ++top_state->suffix_dynamic_array.items_processed;
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_INTERNED_STRING_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!ensure_dynamic_array_read_suffix_ready (state, top_state, array))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            if (top_state->suffix_dynamic_array.items_processed < top_state->suffix_dynamic_array.items_total)
            {
                if (!read_interned_string (state, ((kan_interned_string_t *) array->data) +
                                                      top_state->suffix_dynamic_array.items_processed))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                ++top_state->suffix_dynamic_array.items_processed;
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_STRUCT_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!ensure_dynamic_array_read_suffix_ready (state, top_state, array))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            if (top_state->suffix_dynamic_array.items_processed < top_state->suffix_dynamic_array.items_total)
            {
                struct script_node_t *script_node = script_storage_get_or_create_script (
                    state->common.script_storage, command_to_process->struct_dynamic_array.type_name);
                script_storage_ensure_script_generated (state->common.script_storage, script_node);

                const struct kan_reflection_struct_t *child_struct_data = kan_reflection_registry_query_struct (
                    state->common.script_storage->registry, command_to_process->struct_dynamic_array.type_name);
                KAN_ASSERT (child_struct_data)

                uint8_t *instance_address =
                    ((uint8_t *) array->data) + array->item_size * top_state->suffix_dynamic_array.items_processed;

                if (child_struct_data->init)
                {
                    child_struct_data->init (child_struct_data->functor_user_data, instance_address);
                }

                serialization_common_state_push_script_state (&state->common, script_node->script, instance_address,
                                                              KAN_FALSE);

                ++top_state->suffix_dynamic_array.items_processed;
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_PATCH_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!top_state->suffix_initialized)
            {
                if (!read_array_or_patch_size (state, &top_state->suffix_patch_dynamic_array.array.items_total))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                if (top_state->suffix_patch_dynamic_array.array.items_total > 0u)
                {
                    if (!init_patch_read_suffix (state, &top_state->suffix_patch_dynamic_array.current_patch))
                    {
                        return KAN_SERIALIZATION_FAILED;
                    }

                    if (state->patch_builder == KAN_INVALID_REFLECTION_PATCH_BUILDER)
                    {
                        state->patch_builder = kan_reflection_patch_builder_create ();
                    }
                }

                top_state->suffix_patch_dynamic_array.array.items_processed = 0u;
                kan_dynamic_array_set_capacity (array, top_state->suffix_patch_dynamic_array.array.items_total);
                array->size = top_state->suffix_patch_dynamic_array.array.items_total;
                top_state->suffix_initialized = KAN_TRUE;
            }

            if (top_state->suffix_patch_dynamic_array.array.items_processed <
                top_state->suffix_patch_dynamic_array.array.items_total)
            {
                if (top_state->suffix_patch_dynamic_array.current_patch.read.blocks_processed <
                    top_state->suffix_patch_dynamic_array.current_patch.read.blocks_total)
                {
                    if (!read_patch_block (state, &top_state->suffix_patch_dynamic_array.current_patch))
                    {
                        return KAN_SERIALIZATION_FAILED;
                    }

                    ++top_state->suffix_patch_dynamic_array.current_patch.read.blocks_processed;
                }

                if (top_state->suffix_patch_dynamic_array.current_patch.read.blocks_processed >=
                    top_state->suffix_patch_dynamic_array.current_patch.read.blocks_total)
                {
                    kan_reflection_patch_t *patch =
                        (kan_reflection_patch_t *) (((uint8_t *) array->data) +
                                                    sizeof (kan_reflection_patch_t) *
                                                        top_state->suffix_patch_dynamic_array.array.items_processed);

                    *patch = kan_reflection_patch_builder_build (
                        state->patch_builder, state->common.script_storage->registry,
                        kan_reflection_registry_query_struct (
                            state->common.script_storage->registry,
                            top_state->suffix_patch_dynamic_array.current_patch.type_name));
                    ++top_state->suffix_patch_dynamic_array.array.items_processed;

                    if (top_state->suffix_patch_dynamic_array.array.items_processed <
                        top_state->suffix_patch_dynamic_array.array.items_total)
                    {
                        if (!init_patch_read_suffix (state, &top_state->suffix_patch_dynamic_array.current_patch))
                        {
                            return KAN_SERIALIZATION_FAILED;
                        }
                    }
                }
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_PATCH:
        {
            if (!top_state->suffix_initialized)
            {
                if (!init_patch_read_suffix (state, &top_state->suffix_patch))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                if (state->patch_builder == KAN_INVALID_REFLECTION_PATCH_BUILDER)
                {
                    state->patch_builder = kan_reflection_patch_builder_create ();
                }

                top_state->suffix_initialized = KAN_TRUE;
            }

            if (top_state->suffix_patch.read.blocks_processed < top_state->suffix_patch.read.blocks_total)
            {
                if (!read_patch_block (state, &top_state->suffix_patch))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                ++top_state->suffix_patch.read.blocks_processed;
            }

            if (top_state->suffix_patch.read.blocks_processed >= top_state->suffix_patch.read.blocks_total)
            {
                kan_reflection_patch_t *patch = (kan_reflection_patch_t *) address;
                *patch = kan_reflection_patch_builder_build (
                    state->patch_builder, state->common.script_storage->registry,
                    kan_reflection_registry_query_struct (state->common.script_storage->registry,
                                                          top_state->suffix_patch.type_name));
                script_state_go_to_next_command (top_state);
            }

            break;
        }
        }
    }
    else
    {
        script_state_go_to_next_command (top_state);
    }

    while (KAN_TRUE)
    {
        // Update top state pointer in case if new state was pushed on top.
        top_state = &((struct script_state_t *)
                          state->common.script_state_stack.data)[state->common.script_state_stack.size - 1u];

        if (top_state->command_to_process_index >= top_state->script->commands_count)
        {
            serialization_common_state_pop_script_state (&state->common);
            if (state->common.script_state_stack.size == 0u)
            {
                return KAN_SERIALIZATION_FINISHED;
            }
        }
        else
        {
            break;
        }
    }

    return KAN_SERIALIZATION_IN_PROGRESS;
}

void kan_serialization_binary_reader_destroy (kan_serialization_binary_reader_t reader)
{
    struct serialization_read_state_t *state = (struct serialization_read_state_t *) reader;
    serialization_common_state_shutdown (&state->common);

    if (state->buffer)
    {
        kan_free_general (serialization_allocation_group, state->buffer, state->buffer_size);
    }

    if (state->patch_builder != KAN_INVALID_REFLECTION_PATCH_BUILDER)
    {
        kan_reflection_patch_builder_destroy (state->patch_builder);
    }

    kan_free_general (serialization_allocation_group, state, sizeof (struct serialization_read_state_t));
}

kan_serialization_binary_writer_t kan_serialization_binary_writer_create (
    struct kan_stream_t *stream,
    const void *instance,
    kan_interned_string_t type_name,
    kan_serialization_binary_script_storage_t script_storage,
    kan_serialization_interned_string_registry_t interned_string_registry)
{
    ensure_statics_initialized ();
    KAN_ASSERT (kan_stream_is_writeable (stream))

    struct serialization_write_state_t *state = (struct serialization_write_state_t *) kan_allocate_general (
        serialization_allocation_group, sizeof (struct serialization_write_state_t),
        _Alignof (struct serialization_write_state_t));

    serialization_common_state_init (&state->common, stream, script_storage, interned_string_registry);
    struct script_node_t *script_node = script_storage_get_or_create_script (state->common.script_storage, type_name);
    script_storage_ensure_script_generated (state->common.script_storage, script_node);
    serialization_common_state_push_script_state (&state->common, script_node->script, (void *) instance, KAN_TRUE);

    return (kan_serialization_binary_writer_t) state;
}

static inline kan_bool_t write_string_stateless (struct kan_stream_t *stream, const char *string_input)
{
    const uint64_t string_length_wide = strlen (string_input);
    KAN_ASSERT (string_length_wide <= UINT32_MAX)
    const uint32_t string_length = (uint32_t) string_length_wide;

    if (stream->operations->write (stream, sizeof (uint32_t), &string_length) != sizeof (uint32_t))
    {
        return KAN_FALSE;
    }

    if (stream->operations->write (stream, string_length, string_input) != string_length)
    {
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline kan_bool_t write_string (struct serialization_write_state_t *state, const char *string_input)
{
    return write_string_stateless (state->common.stream, string_input);
}

static inline kan_bool_t write_interned_string_stateless (struct kan_stream_t *stream,
                                                          struct interned_string_registry_t *string_registry,
                                                          kan_interned_string_t input)
{
    if (string_registry)
    {
        const uint32_t index = interned_string_registry_store_string (string_registry, input);
        return stream->operations->write (stream, sizeof (uint32_t), &index) == sizeof (uint32_t);
    }

    return write_string_stateless (stream, input);
}

static inline kan_bool_t write_interned_string (struct serialization_write_state_t *state, kan_interned_string_t input)
{
    return write_interned_string_stateless (state->common.stream, state->common.optional_string_registry, input);
}

static inline kan_bool_t write_array_or_patch_size (struct serialization_write_state_t *state, uint32_t input)
{
    return state->common.stream->operations->write (state->common.stream, sizeof (uint32_t), &input) ==
           sizeof (uint32_t);
}

static inline kan_bool_t ensure_dynamic_array_write_suffix_ready (struct serialization_write_state_t *state,
                                                                  struct script_state_t *top_state,
                                                                  struct kan_dynamic_array_t *array)
{
    if (!top_state->suffix_initialized)
    {
        top_state->suffix_dynamic_array.items_total = array->size;
        if (!write_array_or_patch_size (state, top_state->suffix_dynamic_array.items_total))
        {
            return KAN_FALSE;
        }

        top_state->suffix_dynamic_array.items_processed = 0u;
        top_state->suffix_initialized = KAN_TRUE;
    }

    return KAN_TRUE;
}

static inline kan_bool_t init_patch_write_suffix (struct serialization_write_state_t *state,
                                                  kan_reflection_patch_t patch,
                                                  struct script_state_patch_suffix_t *suffix)
{
    const uint64_t chunks_count_wide = kan_reflection_patch_get_chunks_count (patch);
    KAN_ASSERT (chunks_count_wide <= UINT32_MAX)

    if (!write_interned_string (state, kan_reflection_patch_get_type (patch)->name) ||
        !write_array_or_patch_size (state, (uint32_t) chunks_count_wide))
    {
        return KAN_FALSE;
    }

    suffix->type_name = kan_reflection_patch_get_type (patch)->name;
    suffix->write.current_iterator = kan_reflection_patch_begin (patch);
    suffix->write.end_iterator = kan_reflection_patch_end (patch);
    return KAN_TRUE;
}

static inline kan_bool_t write_patch_block (struct serialization_write_state_t *state,
                                            struct script_state_patch_suffix_t *suffix)
{
    struct kan_reflection_patch_chunk_info_t chunk = kan_reflection_patch_iterator_get (suffix->write.current_iterator);
    struct patch_block_info_t block_info;
    block_info.offset = chunk.offset;
    block_info.size = chunk.size;

    if (state->common.stream->operations->write (state->common.stream, sizeof (struct patch_block_info_t),
                                                 &block_info) != sizeof (struct patch_block_info_t))
    {
        return KAN_FALSE;
    }

    if (block_info.size == 0u)
    {
        return KAN_TRUE;
    }

    struct interned_string_lookup_node_t *interned_string_lookup_node =
        script_storage_get_or_create_interned_string_lookup (state->common.script_storage, suffix->type_name);
    script_storage_ensure_interned_string_lookup_generated (state->common.script_storage, interned_string_lookup_node);

    uint32_t current_offset = block_info.offset;
    const uint32_t end_offset = block_info.offset + block_info.size;

    uint64_t next_interned_string_index = upper_or_equal_bound_index (
        interned_string_lookup_node->interned_string_absolute_positions,
        interned_string_lookup_node->interned_string_absolute_positions_count, current_offset);

    while (current_offset < end_offset)
    {
        uint32_t serialized_block_end = end_offset;
        const uint8_t *data_begin = ((const uint8_t *) chunk.data) + current_offset;

        if (next_interned_string_index < interned_string_lookup_node->interned_string_absolute_positions_count)
        {
            uint32_t next_string_offset =
                interned_string_lookup_node->interned_string_absolute_positions[next_interned_string_index];

            if (next_string_offset == current_offset)
            {
                if (!write_interned_string (state, *(kan_interned_string_t *) data_begin))
                {
                    return KAN_FALSE;
                }

                current_offset += sizeof (kan_interned_string_t);
                ++next_interned_string_index;
                continue;
            }
            else if (next_string_offset < serialized_block_end)
            {
                KAN_ASSERT (next_string_offset > current_offset)
                serialized_block_end = next_string_offset;
            }
        }

        const uint32_t size = serialized_block_end - current_offset;
        if (state->common.stream->operations->write (state->common.stream, size, data_begin) != size)
        {
            return KAN_FALSE;
        }

        current_offset = serialized_block_end;
    }

    return KAN_TRUE;
}

enum kan_serialization_state_t kan_serialization_binary_writer_step (kan_serialization_binary_writer_t writer)
{
    struct serialization_write_state_t *state = (struct serialization_write_state_t *) writer;
    if (state->common.script_state_stack.size == 0u)
    {
        return KAN_SERIALIZATION_FINISHED;
    }

    struct script_state_t *top_state =
        &((struct script_state_t *) state->common.script_state_stack.data)[state->common.script_state_stack.size - 1u];
    KAN_ASSERT (top_state->command_to_process_index < top_state->script->commands_count)

    struct script_condition_t *script_conditions = (struct script_condition_t *) top_state->script->data;
    struct script_command_t *script_commands =
        (struct script_command_t *) (script_conditions + top_state->script->conditions_count);

    struct script_command_t *command_to_process = script_commands + top_state->command_to_process_index;
    kan_bool_t should_be_processed = KAN_TRUE;

    if (command_to_process->condition_index != SCRIPT_NO_CONDITION && !top_state->condition_checked)
    {
        KAN_ASSERT (top_state->condition_values[command_to_process->condition_index] !=
                    SERIALIZATION_CONDITION_NOT_CALCULATED)
        should_be_processed =
            top_state->condition_values[command_to_process->condition_index] == SERIALIZATION_CONDITION_PASSED;
        top_state->condition_checked = KAN_TRUE;
    }

    if (should_be_processed)
    {
        uint8_t *address = ((uint8_t *) top_state->instance) + command_to_process->offset;
        switch (command_to_process->type)
        {
        case SCRIPT_COMMAND_BLOCK:
            if (state->common.stream->operations->write (state->common.stream, command_to_process->block.size,
                                                         address) != command_to_process->block.size)
            {
                return KAN_SERIALIZATION_FAILED;
            }

            script_state_go_to_next_command (top_state);
            break;

        case SCRIPT_COMMAND_STRING:
        {
            if (!write_string (state, *(char **) address))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            script_state_go_to_next_command (top_state);
            break;
        }

        case SCRIPT_COMMAND_INTERNED_STRING:
            if (!write_interned_string (state, *(kan_interned_string_t *) address))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            script_state_go_to_next_command (top_state);
            break;

        case SCRIPT_COMMAND_BLOCK_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!write_array_or_patch_size (state, array->size))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            if (state->common.stream->operations->write (state->common.stream, array->size * array->item_size,
                                                         array->data) != array->size * array->item_size)
            {
                return KAN_SERIALIZATION_FAILED;
            }

            script_state_go_to_next_command (top_state);
            break;
        }

        case SCRIPT_COMMAND_STRING_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!ensure_dynamic_array_write_suffix_ready (state, top_state, array))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            if (top_state->suffix_dynamic_array.items_processed < top_state->suffix_dynamic_array.items_total)
            {
                if (!write_string (state, *(((char **) array->data) + top_state->suffix_dynamic_array.items_processed)))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                ++top_state->suffix_dynamic_array.items_processed;
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_INTERNED_STRING_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!ensure_dynamic_array_write_suffix_ready (state, top_state, array))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            if (top_state->suffix_dynamic_array.items_processed < top_state->suffix_dynamic_array.items_total)
            {
                if (!write_interned_string (state, *(((kan_interned_string_t *) array->data) +
                                                     top_state->suffix_dynamic_array.items_processed)))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                ++top_state->suffix_dynamic_array.items_processed;
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_STRUCT_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!ensure_dynamic_array_write_suffix_ready (state, top_state, array))
            {
                return KAN_SERIALIZATION_FAILED;
            }

            if (top_state->suffix_dynamic_array.items_processed < top_state->suffix_dynamic_array.items_total)
            {
                struct script_node_t *script_node = script_storage_get_or_create_script (
                    state->common.script_storage, command_to_process->struct_dynamic_array.type_name);
                script_storage_ensure_script_generated (state->common.script_storage, script_node);
                serialization_common_state_push_script_state (
                    &state->common, script_node->script,
                    ((uint8_t *) array->data) + array->item_size * top_state->suffix_dynamic_array.items_processed,
                    KAN_TRUE);

                ++top_state->suffix_dynamic_array.items_processed;
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_PATCH_DYNAMIC_ARRAY:
        {
            struct kan_dynamic_array_t *array = (struct kan_dynamic_array_t *) address;
            if (!top_state->suffix_initialized)
            {
                top_state->suffix_dynamic_array.items_total = array->size;
                if (!write_array_or_patch_size (state, top_state->suffix_patch_dynamic_array.array.items_total))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                if (array->size > 0u)
                {
                    if (!init_patch_write_suffix (state, *(kan_reflection_patch_t *) array->data,
                                                  &top_state->suffix_patch_dynamic_array.current_patch))
                    {
                        return KAN_SERIALIZATION_FAILED;
                    }
                }

                top_state->suffix_patch_dynamic_array.array.items_processed = 0u;
                top_state->suffix_initialized = KAN_TRUE;
            }

            if (top_state->suffix_patch_dynamic_array.array.items_processed <
                top_state->suffix_patch_dynamic_array.array.items_total)
            {
                if (top_state->suffix_patch_dynamic_array.current_patch.write.current_iterator !=
                    top_state->suffix_patch_dynamic_array.current_patch.write.end_iterator)
                {
                    if (!write_patch_block (state, &top_state->suffix_patch_dynamic_array.current_patch))
                    {
                        return KAN_SERIALIZATION_FAILED;
                    }

                    top_state->suffix_patch_dynamic_array.current_patch.write.current_iterator =
                        kan_reflection_patch_iterator_next (
                            top_state->suffix_patch_dynamic_array.current_patch.write.current_iterator);
                }

                if (top_state->suffix_patch_dynamic_array.current_patch.write.current_iterator ==
                    top_state->suffix_patch_dynamic_array.current_patch.write.end_iterator)
                {
                    ++top_state->suffix_patch_dynamic_array.array.items_processed;
                    if (top_state->suffix_patch_dynamic_array.array.items_processed <
                        top_state->suffix_patch_dynamic_array.array.items_total)
                    {
                        kan_reflection_patch_t next_patch = *(
                            kan_reflection_patch_t *) (((uint8_t *) array->data) +
                                                       sizeof (kan_reflection_patch_t) *
                                                           top_state->suffix_patch_dynamic_array.array.items_processed);

                        if (!init_patch_write_suffix (state, next_patch,
                                                      &top_state->suffix_patch_dynamic_array.current_patch))
                        {
                            return KAN_SERIALIZATION_FAILED;
                        }
                    }
                }
            }

            if (top_state->suffix_dynamic_array.items_processed >= top_state->suffix_dynamic_array.items_total)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }

        case SCRIPT_COMMAND_PATCH:
        {
            kan_reflection_patch_t patch = *(kan_reflection_patch_t *) address;
            if (!top_state->suffix_initialized)
            {
                if (!init_patch_write_suffix (state, patch, &top_state->suffix_patch))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                top_state->suffix_initialized = KAN_TRUE;
            }

            if (top_state->suffix_patch.write.current_iterator != top_state->suffix_patch.write.end_iterator)
            {
                if (!write_patch_block (state, &top_state->suffix_patch))
                {
                    return KAN_SERIALIZATION_FAILED;
                }

                top_state->suffix_patch.write.current_iterator =
                    kan_reflection_patch_iterator_next (top_state->suffix_patch.write.current_iterator);
            }

            if (top_state->suffix_patch.write.current_iterator == top_state->suffix_patch.write.end_iterator)
            {
                script_state_go_to_next_command (top_state);
            }

            break;
        }
        }
    }
    else
    {
        script_state_go_to_next_command (top_state);
    }

    while (KAN_TRUE)
    {
        // Update top state pointer in case if new state was pushed on top.
        top_state = &((struct script_state_t *)
                          state->common.script_state_stack.data)[state->common.script_state_stack.size - 1u];

        if (top_state->command_to_process_index >= top_state->script->commands_count)
        {
            serialization_common_state_pop_script_state (&state->common);
            if (state->common.script_state_stack.size == 0u)
            {
                return KAN_SERIALIZATION_FINISHED;
            }
        }
        else
        {
            break;
        }
    }

    return KAN_SERIALIZATION_IN_PROGRESS;
}

void kan_serialization_binary_writer_destroy (kan_serialization_binary_writer_t writer)
{
    struct serialization_write_state_t *state = (struct serialization_write_state_t *) writer;
    serialization_common_state_shutdown (&state->common);
    kan_free_general (serialization_allocation_group, state, sizeof (struct serialization_write_state_t));
}

kan_bool_t kan_serialization_binary_read_type_header (
    struct kan_stream_t *stream,
    kan_interned_string_t *type_name_output,
    kan_serialization_interned_string_registry_t interned_string_registry)
{
    ensure_statics_initialized ();
    return read_interned_string_stateless (stream, (struct interned_string_registry_t *) interned_string_registry,
                                           type_name_output);
}

kan_bool_t kan_serialization_binary_write_type_header (
    struct kan_stream_t *stream,
    kan_interned_string_t type_name,
    kan_serialization_interned_string_registry_t interned_string_registry)
{
    return write_interned_string_stateless (stream, (struct interned_string_registry_t *) interned_string_registry,
                                            type_name);
}
