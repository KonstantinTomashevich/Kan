#define _CRT_SECURE_NO_WARNINGS

#include <memory.h>
#include <qsort.h>
#include <stddef.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/container/event_queue.h>
#include <kan/container/hash_storage.h>
#include <kan/container/list.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/cpu_dispatch/job.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/repository/meta.h>
#include <kan/repository/repository.h>
#include <kan/threading/atomic.h>

// TODO: After everything is implemented, think about integration cpu profiler here if it is appropriate.

KAN_LOG_DEFINE_CATEGORY (repository);

struct interned_field_path_t
{
    uint64_t length;
    kan_interned_string_t *path;
};

#define OBSERVATION_BUFFER_ALIGNMENT _Alignof (uint64_t)
#define OBSERVATION_BUFFER_CHUNK_ALIGNMENT _Alignof (uint64_t)

struct observation_buffer_scenario_chunk_t
{
    uint64_t source_offset;
    uint64_t size;
    uint64_t flags;
};

struct observation_buffer_scenario_chunk_list_node_t
{
    struct observation_buffer_scenario_chunk_list_node_t *next;
    uint64_t source_offset;
    uint64_t size;
    uint64_t flags;
};

struct copy_out_t
{
    uint64_t source_offset;
    uint64_t target_offset;
    uint64_t size;
};

struct copy_out_list_node_t
{
    struct copy_out_list_node_t *next;
    uint64_t source_offset;
    uint64_t target_offset;
    uint64_t size;
};

struct observation_buffer_definition_t
{
    uint64_t buffer_size;
    uint64_t scenario_chunks_count;
    struct observation_buffer_scenario_chunk_t *scenario_chunks;
};

struct observation_event_trigger_t
{
    uint64_t flags;
    struct kan_repository_event_insert_query_t event_insert_query;
    uint64_t buffer_copy_outs_count;
    uint64_t record_copy_outs_count;
    struct copy_out_t copy_outs[];
};

struct observation_event_trigger_list_node_t
{
    struct observation_event_trigger_list_node_t *next;
    uint64_t flags;
    struct kan_repository_event_insert_query_t event_insert_query;
    uint64_t buffer_copy_outs_count;
    struct copy_out_list_node_t *buffer_copy_outs;
    uint64_t record_copy_outs_count;
    struct copy_out_list_node_t *record_copy_outs;
};

struct observation_event_triggers_definition_t
{
    uint64_t event_triggers_count;
    struct observation_event_trigger_t *event_triggers;
};

struct lifetime_event_trigger_t
{
    struct kan_repository_event_insert_query_t event_insert_query;
    uint64_t copy_outs_count;
    struct copy_out_t copy_outs[];
};

struct lifetime_event_trigger_list_node_t
{
    struct lifetime_event_trigger_list_node_t *next;
    struct kan_repository_event_insert_query_t event_insert_query;
    uint64_t copy_outs_count;
    struct copy_out_list_node_t *copy_outs;
};

struct lifetime_event_triggers_definition_t
{
    uint64_t event_triggers_count;
    struct lifetime_event_trigger_t *event_triggers;
};

enum lifetime_event_trigger_type_t
{
    LIFETIME_EVENT_TRIGGER_ON_INSERT = 0u,
    LIFETIME_EVENT_TRIGGER_ON_DELETE,
};

struct cascade_deleter_t
{
    struct kan_repository_indexed_value_delete_query_t query;
    uint64_t absolute_input_value_offset;
};

struct cascade_deleter_node_t
{
    struct cascade_deleter_node_t *next;
    struct kan_repository_indexed_value_delete_query_t query;
    uint64_t absolute_input_value_offset;
};

struct cascade_deleters_definition_t
{
    uint64_t cascade_deleters_count;
    struct cascade_deleter_t *cascade_deleters;
};

struct singleton_storage_node_t
{
    struct kan_hash_storage_node_t node;
    const struct kan_reflection_struct_t *type;
    struct kan_atomic_int_t queries_count;
    void *singleton;

    struct observation_buffer_definition_t observation_buffer;
    void *observation_buffer_memory;
    struct observation_event_triggers_definition_t observation_events_triggers;

    kan_allocation_group_t allocation_group;
    kan_allocation_group_t automation_allocation_group;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    struct kan_atomic_int_t safeguard_access_status;
#endif
};

struct singleton_query_t
{
    struct singleton_storage_node_t *storage;
};

_Static_assert (sizeof (struct singleton_query_t) <= sizeof (struct kan_repository_singleton_read_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct singleton_query_t) <= _Alignof (struct kan_repository_singleton_read_query_t),
                "Query alignments match.");
_Static_assert (sizeof (struct singleton_query_t) <= sizeof (struct kan_repository_singleton_write_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct singleton_query_t) <= _Alignof (struct kan_repository_singleton_write_query_t),
                "Query alignments match.");

struct indexed_storage_record_node_t
{
    struct kan_bd_list_node_t list_node;
    void *record;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    struct kan_atomic_int_t safeguard_access_status;
#endif
};

enum indexed_storage_dirty_record_type_t
{
    INDEXED_STORAGE_DIRTY_RECORD_CHANGED = 0u,
    INDEXED_STORAGE_DIRTY_RECORD_INSERTED,
    INDEXED_STORAGE_DIRTY_RECORD_DELETED,
};

struct indexed_storage_dirty_record_node_t
{
    struct indexed_storage_dirty_record_node_t *next;
    struct indexed_storage_record_node_t *source_node;

    enum indexed_storage_dirty_record_type_t type;
    void *observation_buffer_memory;
    uint64_t observation_comparison_flags;

    void *dirt_source_index;
    void *dirt_source_index_node;
    void *dirt_source_index_sub_node;
};

struct indexed_storage_node_t
{
    struct kan_hash_storage_node_t node;
    struct repository_t *repository;
    const struct kan_reflection_struct_t *type;
    struct kan_atomic_int_t queries_count;

    struct kan_bd_list_t records;
    struct kan_atomic_int_t access_status;

    /// \brief Multi-use lock for different tasks connected to storage coherence maintenance.
    /// \details Can be use for several cases:
    ///          - During planning mode, locks index creating and querying logic.
    ///          - During serving mode, when access_status is logically guaranteed to be above zero,
    ///            used to guard temporary_allocator and dirty_records.
    ///          - During serving mode, in cases when access_status falls to zero and below,
    ///            used to guard coherence maintenance execution.
    struct kan_atomic_int_t maintenance_lock;

    struct indexed_storage_dirty_record_node_t *dirty_records;
    struct kan_stack_group_allocator_t temporary_allocator;

    struct observation_buffer_definition_t observation_buffer;
    struct observation_event_triggers_definition_t observation_events_triggers;
    struct lifetime_event_triggers_definition_t on_insert_events_triggers;
    struct lifetime_event_triggers_definition_t on_delete_events_triggers;
    struct cascade_deleters_definition_t cascade_deleters;

    struct value_index_t *first_value_index;
    struct signal_index_t *first_signal_index;
    struct interval_index_t *first_interval_index;
    struct space_index_t *first_space_index;

    kan_allocation_group_t allocation_group;
    kan_allocation_group_t records_allocation_group;
    kan_allocation_group_t nodes_allocation_group;
    kan_allocation_group_t automation_allocation_group;

    kan_allocation_group_t value_index_allocation_group;
    kan_allocation_group_t signal_index_allocation_group;
    kan_allocation_group_t interval_index_allocation_group;
    kan_allocation_group_t space_index_allocation_group;
};

struct value_index_sub_node_t
{
    struct value_index_sub_node_t *next;
    struct value_index_sub_node_t *previous;
    struct indexed_storage_record_node_t *record;
};

struct value_index_node_t
{
    struct kan_hash_storage_node_t node;
    struct value_index_sub_node_t *first_sub_node;
};

struct value_index_t
{
    struct value_index_t *next;
    struct indexed_storage_node_t *storage;
    struct kan_atomic_int_t queries_count;

    uint32_t backed_field_absolute_offset;
    uint16_t backed_field_offset_in_buffer;
    uint8_t backed_field_size;
    uint8_t backed_field_size_with_padding;

    uint64_t observation_flags;
    struct kan_hash_storage_t hash_storage;
    struct interned_field_path_t source_path;
};

struct indexed_insert_query_t
{
    struct indexed_storage_node_t *storage;
};

_Static_assert (sizeof (struct indexed_insert_query_t) <= sizeof (struct kan_repository_indexed_insert_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_insert_query_t) <= _Alignof (struct kan_repository_indexed_insert_query_t),
                "Query alignments match.");

struct indexed_insertion_package_t
{
    struct indexed_storage_node_t *storage;
    void *record;
};

_Static_assert (sizeof (struct indexed_insertion_package_t) <=
                    sizeof (struct kan_repository_indexed_insertion_package_t),
                "Insertion package sizes match.");
_Static_assert (_Alignof (struct indexed_insertion_package_t) <=
                    _Alignof (struct kan_repository_indexed_insertion_package_t),
                "Insertion package alignments match.");

struct indexed_sequence_query_t
{
    struct indexed_storage_node_t *storage;
};

_Static_assert (sizeof (struct indexed_sequence_query_t) <=
                    sizeof (struct kan_repository_indexed_sequence_read_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_sequence_query_t) <=
                    _Alignof (struct kan_repository_indexed_sequence_read_query_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_sequence_query_t) <=
                    sizeof (struct kan_repository_indexed_sequence_update_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_sequence_query_t) <=
                    _Alignof (struct kan_repository_indexed_sequence_update_query_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_sequence_query_t) <=
                    sizeof (struct kan_repository_indexed_sequence_delete_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_sequence_query_t) <=
                    _Alignof (struct kan_repository_indexed_sequence_delete_query_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_sequence_query_t) <=
                    sizeof (struct kan_repository_indexed_sequence_write_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_sequence_query_t) <=
                    _Alignof (struct kan_repository_indexed_sequence_write_query_t),
                "Query alignments match.");

struct indexed_sequence_cursor_t
{
    struct indexed_storage_node_t *storage;
    struct indexed_storage_record_node_t *node;
};

_Static_assert (sizeof (struct indexed_sequence_cursor_t) <=
                    sizeof (struct kan_repository_indexed_sequence_read_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_sequence_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_sequence_read_cursor_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_sequence_cursor_t) <=
                    sizeof (struct kan_repository_indexed_sequence_update_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_sequence_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_sequence_update_cursor_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_sequence_cursor_t) <=
                    sizeof (struct kan_repository_indexed_sequence_delete_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_sequence_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_sequence_delete_cursor_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_sequence_cursor_t) <=
                    sizeof (struct kan_repository_indexed_sequence_write_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_sequence_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_sequence_write_cursor_t),
                "Query alignments match.");

struct indexed_sequence_constant_access_t
{
    struct indexed_storage_node_t *storage;
    struct indexed_storage_record_node_t *node;
};

_Static_assert (sizeof (struct indexed_sequence_constant_access_t) <=
                    sizeof (struct kan_repository_indexed_sequence_read_access_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_sequence_constant_access_t) <=
                    _Alignof (struct kan_repository_indexed_sequence_read_access_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_sequence_constant_access_t) <=
                    sizeof (struct kan_repository_indexed_sequence_delete_access_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_sequence_constant_access_t) <=
                    _Alignof (struct kan_repository_indexed_sequence_delete_access_t),
                "Query alignments match.");

struct indexed_sequence_mutable_access_t
{
    struct indexed_storage_node_t *storage;
    struct indexed_storage_dirty_record_node_t *dirty_node;
};

_Static_assert (sizeof (struct indexed_sequence_mutable_access_t) <=
                    sizeof (struct kan_repository_indexed_sequence_update_access_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_sequence_mutable_access_t) <=
                    _Alignof (struct kan_repository_indexed_sequence_update_access_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_sequence_mutable_access_t) <=
                    sizeof (struct kan_repository_indexed_sequence_write_access_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_sequence_mutable_access_t) <=
                    _Alignof (struct kan_repository_indexed_sequence_write_access_t),
                "Query alignments match.");

struct indexed_value_query_t
{
    struct value_index_t *index;
};

_Static_assert (sizeof (struct indexed_value_query_t) <= sizeof (struct kan_repository_indexed_value_read_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_value_query_t) <= _Alignof (struct kan_repository_indexed_value_read_query_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_value_query_t) <= sizeof (struct kan_repository_indexed_value_update_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_value_query_t) <=
                    _Alignof (struct kan_repository_indexed_value_update_query_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_value_query_t) <= sizeof (struct kan_repository_indexed_value_delete_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_value_query_t) <=
                    _Alignof (struct kan_repository_indexed_value_delete_query_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_value_query_t) <= sizeof (struct kan_repository_indexed_value_write_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_value_query_t) <= _Alignof (struct kan_repository_indexed_value_write_query_t),
                "Query alignments match.");

struct indexed_value_cursor_t
{
    struct value_index_t *index;
    struct value_index_node_t *node;
    struct value_index_sub_node_t *sub_node;
};

_Static_assert (sizeof (struct indexed_value_cursor_t) <= sizeof (struct kan_repository_indexed_value_read_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_value_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_value_read_cursor_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_value_cursor_t) <= sizeof (struct kan_repository_indexed_value_update_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_value_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_value_update_cursor_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_value_cursor_t) <= sizeof (struct kan_repository_indexed_value_delete_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_value_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_value_delete_cursor_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_value_cursor_t) <= sizeof (struct kan_repository_indexed_value_write_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_value_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_value_write_cursor_t),
                "Query alignments match.");

struct indexed_value_constant_access_t
{
    struct value_index_t *index;
    struct value_index_node_t *node;
    struct value_index_sub_node_t *sub_node;
};

_Static_assert (sizeof (struct indexed_value_constant_access_t) <=
                    sizeof (struct kan_repository_indexed_value_read_access_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_value_constant_access_t) <=
                    _Alignof (struct kan_repository_indexed_value_read_access_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_value_constant_access_t) <=
                    sizeof (struct kan_repository_indexed_value_delete_access_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_value_constant_access_t) <=
                    _Alignof (struct kan_repository_indexed_value_delete_access_t),
                "Query alignments match.");

struct indexed_value_mutable_access_t
{
    struct value_index_t *index;
    struct value_index_node_t *node;
    struct value_index_sub_node_t *sub_node;
    struct indexed_storage_dirty_record_node_t *dirty_node;
};

_Static_assert (sizeof (struct indexed_value_mutable_access_t) <=
                    sizeof (struct kan_repository_indexed_value_update_access_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_value_mutable_access_t) <=
                    _Alignof (struct kan_repository_indexed_value_update_access_t),
                "Query alignments match.");

_Static_assert (sizeof (struct indexed_value_mutable_access_t) <=
                    sizeof (struct kan_repository_indexed_value_write_access_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_value_mutable_access_t) <=
                    _Alignof (struct kan_repository_indexed_value_write_access_t),
                "Query alignments match.");

struct event_queue_node_t
{
    struct kan_event_queue_node_t node;
    void *event;
};

struct event_storage_node_t
{
    struct kan_hash_storage_node_t node;
    kan_allocation_group_t allocation_group;
    const struct kan_reflection_struct_t *type;
    struct kan_atomic_int_t queries_count;
    struct kan_atomic_int_t single_threaded_operations_lock;
    struct kan_event_queue_t event_queue;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    struct kan_atomic_int_t safeguard_access_status;
#endif
};

struct event_insert_query_t
{
    struct event_storage_node_t *storage;
};

_Static_assert (sizeof (struct event_insert_query_t) <= sizeof (struct kan_repository_event_insert_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct event_insert_query_t) <= _Alignof (struct kan_repository_event_insert_query_t),
                "Query alignments match.");

struct event_insertion_package_t
{
    struct event_storage_node_t *storage;
    void *event;
};

_Static_assert (sizeof (struct event_insertion_package_t) <= sizeof (struct kan_repository_event_insertion_package_t),
                "Insertion package sizes match.");
_Static_assert (_Alignof (struct event_insertion_package_t) <=
                    _Alignof (struct kan_repository_event_insertion_package_t),
                "Insertion package alignments match.");

struct event_fetch_query_t
{
    struct event_storage_node_t *storage;
    kan_event_queue_iterator_t iterator;
};

_Static_assert (sizeof (struct event_fetch_query_t) <= sizeof (struct kan_repository_event_fetch_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct event_fetch_query_t) <= _Alignof (struct kan_repository_event_fetch_query_t),
                "Query alignments match.");

struct event_read_access_t
{
    struct event_storage_node_t *storage;
    kan_event_queue_iterator_t iterator;
};

_Static_assert (sizeof (struct event_read_access_t) <= sizeof (struct kan_repository_event_read_access_t),
                "Read access sizes match.");
_Static_assert (_Alignof (struct event_read_access_t) <= _Alignof (struct kan_repository_event_read_access_t),
                "Read access alignments match.");

enum repository_mode_t
{
    REPOSITORY_MODE_PLANNING,
    REPOSITORY_MODE_SERVING,
};

struct repository_t
{
    struct repository_t *parent;
    struct repository_t *next;
    struct repository_t *first;

    union
    {
        struct kan_atomic_int_t shared_storage_access_lock;
        struct kan_atomic_int_t *shared_storage_access_lock_pointer;
    };

    kan_interned_string_t name;
    kan_reflection_registry_t registry;
    kan_allocation_group_t allocation_group;
    enum repository_mode_t mode;

    struct kan_hash_storage_t singleton_storages;
    struct kan_hash_storage_t indexed_storages;
    struct kan_hash_storage_t event_storages;
};

struct migration_context_t
{
    struct kan_stack_group_allocator_t allocator;
    struct kan_cpu_task_list_node_t *task_list;
};

struct record_migration_user_data_t
{
    void **record_pointer;
    kan_allocation_group_t allocation_group;
    kan_bool_t batched_allocation;
    kan_reflection_struct_migrator_t migrator;
    const struct kan_reflection_struct_t *old_type;
    const struct kan_reflection_struct_t *new_type;
};

struct switch_to_serving_context_t
{
    struct kan_stack_group_allocator_t allocator;
    struct kan_cpu_task_list_node_t *task_list;
};

struct singleton_switch_to_serving_user_data_t
{
    struct singleton_storage_node_t *storage;
    struct repository_t *repository;
};

struct indexed_switch_to_serving_user_data_t
{
    struct indexed_storage_node_t *storage;
    struct repository_t *repository;
};

static kan_bool_t interned_strings_ready = KAN_FALSE;
static struct kan_atomic_int_t interned_strings_initialization_lock = {.value = 0};
static kan_interned_string_t migration_task_name;
static kan_interned_string_t switch_to_serving_task_name;
static kan_interned_string_t meta_automatic_on_change_event_name;
static kan_interned_string_t meta_automatic_on_insert_event_name;
static kan_interned_string_t meta_automatic_on_delete_event_name;
static kan_interned_string_t meta_automatic_cascade_deletion_name;

static void ensure_interned_strings_ready (void)
{
    if (!interned_strings_ready)
    {
        kan_atomic_int_lock (&interned_strings_initialization_lock);
        if (!interned_strings_ready)
        {
            migration_task_name = kan_string_intern ("repository_migration_task");
            switch_to_serving_task_name = kan_string_intern ("repository_switch_to_serving_task");
            meta_automatic_on_change_event_name = kan_string_intern ("kan_repository_meta_automatic_on_change_event_t");
            meta_automatic_on_insert_event_name = kan_string_intern ("kan_repository_meta_automatic_on_insert_event_t");
            meta_automatic_on_delete_event_name = kan_string_intern ("kan_repository_meta_automatic_on_delete_event_t");
            meta_automatic_cascade_deletion_name =
                kan_string_intern ("kan_repository_meta_automatic_cascade_deletion_t");
        }

        kan_atomic_int_unlock (&interned_strings_initialization_lock);
    }
}

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
KAN_LOG_DEFINE_CATEGORY (repository_safeguards);

static kan_bool_t safeguard_singleton_write_access_try_create (struct singleton_storage_node_t *singleton_storage)
{
    while (KAN_TRUE)
    {
        int old_value = kan_atomic_int_get (&singleton_storage->safeguard_access_status);
        if (old_value < 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Singleton type \"%s\". Unable to create write access because read accesses detected.",
                     singleton_storage->type->name);
            return KAN_FALSE;
        }

        if (old_value > 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Singleton type \"%s\". Unable to create write access because other write access detected.",
                     singleton_storage->type->name);
            return KAN_FALSE;
        }

        int new_value = old_value + 1;
        if (kan_atomic_int_compare_and_set (&singleton_storage->safeguard_access_status, old_value, new_value))
        {
            return KAN_TRUE;
        }
    }
}

static void safeguard_singleton_write_access_destroyed (struct singleton_storage_node_t *singleton_storage)
{
    kan_atomic_int_add (&singleton_storage->safeguard_access_status, -1);
}

static kan_bool_t safeguard_singleton_read_access_try_create (struct singleton_storage_node_t *singleton_storage)
{
    while (KAN_TRUE)
    {
        int old_value = kan_atomic_int_get (&singleton_storage->safeguard_access_status);
        if (old_value > 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Singleton type \"%s\". Unable to create read access because write detected.",
                     singleton_storage->type->name);
            return KAN_FALSE;
        }

        int new_value = old_value - 1;
        if (kan_atomic_int_compare_and_set (&singleton_storage->safeguard_access_status, old_value, new_value))
        {
            return KAN_TRUE;
        }
    }
}

static void safeguard_singleton_read_access_destroyed (struct singleton_storage_node_t *singleton_storage)
{
    kan_atomic_int_add (&singleton_storage->safeguard_access_status, 1);
}

static kan_bool_t safeguard_indexed_write_access_try_create (struct indexed_storage_node_t *storage,
                                                             struct indexed_storage_record_node_t *node)
{
    while (KAN_TRUE)
    {
        int old_value = kan_atomic_int_get (&node->safeguard_access_status);
        if (old_value < 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Indexed type \"%s\". Unable to create update/delete/write access because read accesses to the "
                     "same record detected.",
                     storage->type->name);
            return KAN_FALSE;
        }

        if (old_value > 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Indexed type \"%s\". Unable to create update/delete/write access because update/delete/write "
                     "access to the same record detected.",
                     storage->type->name);
            return KAN_FALSE;
        }

        int new_value = old_value + 1;
        if (kan_atomic_int_compare_and_set (&node->safeguard_access_status, old_value, new_value))
        {
            return KAN_TRUE;
        }
    }
}

static void safeguard_indexed_write_access_destroyed (struct indexed_storage_record_node_t *node)
{
    kan_atomic_int_add (&node->safeguard_access_status, -1);
}

static kan_bool_t safeguard_indexed_read_access_try_create (struct indexed_storage_node_t *storage,
                                                            struct indexed_storage_record_node_t *node)
{
    while (KAN_TRUE)
    {
        int old_value = kan_atomic_int_get (&node->safeguard_access_status);
        if (old_value > 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Indexed type \"%s\". Unable to create read access because update/delete/write access to the same "
                     "record detected.",
                     storage->type->name);
            return KAN_FALSE;
        }

        int new_value = old_value - 1;
        if (kan_atomic_int_compare_and_set (&node->safeguard_access_status, old_value, new_value))
        {
            return KAN_TRUE;
        }
    }
}

static void safeguard_indexed_read_access_destroyed (struct indexed_storage_record_node_t *node)
{
    kan_atomic_int_add (&node->safeguard_access_status, 1);
}

static kan_bool_t safeguard_event_insertion_package_try_create (struct event_storage_node_t *event_storage)
{
    while (KAN_TRUE)
    {
        int old_value = kan_atomic_int_get (&event_storage->safeguard_access_status);
        if (old_value < 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Event type \"%s\". Unable to create event insertion package because existing event read accesses "
                     "detected.",
                     event_storage->type->name);
            return KAN_FALSE;
        }

        int new_value = old_value + 1;
        if (kan_atomic_int_compare_and_set (&event_storage->safeguard_access_status, old_value, new_value))
        {
            return KAN_TRUE;
        }
    }
}

static void safeguard_event_insertion_package_destroyed (struct event_storage_node_t *event_storage)
{
    kan_atomic_int_add (&event_storage->safeguard_access_status, -1);
}

static kan_bool_t safeguard_event_read_access_try_create (struct event_storage_node_t *event_storage)
{
    while (KAN_TRUE)
    {
        int old_value = kan_atomic_int_get (&event_storage->safeguard_access_status);
        if (old_value > 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Event type \"%s\". Unable to create event read access because existing event insertion packages "
                     "detected.",
                     event_storage->type->name);
            return KAN_FALSE;
        }

        int new_value = old_value - 1;
        if (kan_atomic_int_compare_and_set (&event_storage->safeguard_access_status, old_value, new_value))
        {
            return KAN_TRUE;
        }
    }
}

static void safeguard_event_read_access_destroyed (struct event_storage_node_t *event_storage)
{
    kan_atomic_int_add (&event_storage->safeguard_access_status, 1);
}
#endif

#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
KAN_LOG_DEFINE_CATEGORY (repository_validation);

static kan_bool_t validation_field_is_observable (kan_reflection_registry_t registry,
                                                  enum kan_reflection_archetype_t field_archetype,
                                                  kan_interned_string_t field_name,
                                                  const void *archetype_suffix)
{
    switch (field_archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_FLOATING:
    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
    case KAN_REFLECTION_ARCHETYPE_ENUM:
        return KAN_TRUE;

    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
        KAN_LOG (repository_validation, KAN_LOG_ERROR,
                 "Found attempt to observe string pointer field \"%s\". String pointers are not observable.",
                 field_name)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
        KAN_LOG (repository_validation, KAN_LOG_ERROR,
                 "Found attempt to observe external pointer field \"%s\". External pointers are not observable.",
                 field_name)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_STRUCT:
    {
        KAN_ASSERT (archetype_suffix)
        const struct kan_reflection_archetype_struct_suffix_t *suffix =
            (const struct kan_reflection_archetype_struct_suffix_t *) archetype_suffix;

        const struct kan_reflection_struct_t *reflection_struct =
            kan_reflection_registry_query_struct (registry, suffix->type_name);

        if (!reflection_struct)
        {
            KAN_LOG (repository_validation, KAN_LOG_ERROR,
                     "Found attempt to observe struct field \"%s\". It's struct type is not found.", field_name)
            return KAN_FALSE;
        }

        for (uint64_t field_index = 0u; field_index < reflection_struct->fields_count; ++field_index)
        {
            if (!validation_field_is_observable (registry, reflection_struct->fields[field_index].archetype,
                                                 reflection_struct->fields[field_index].name,
                                                 &reflection_struct->fields[field_index].archetype_struct))
            {
                return KAN_FALSE;
            }
        }

        return KAN_TRUE;
    }

    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
        KAN_LOG (repository_validation, KAN_LOG_ERROR,
                 "Found attempt to observe struct pointer field \"%s\". Struct pointers are not observable.",
                 field_name)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
    {
        KAN_ASSERT (archetype_suffix)
        const struct kan_reflection_archetype_inline_array_suffix_t *suffix =
            (const struct kan_reflection_archetype_inline_array_suffix_t *) archetype_suffix;

        return validation_field_is_observable (registry, suffix->item_archetype, field_name,
                                               &suffix->item_archetype_struct);
    }

    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
        KAN_LOG (repository_validation, KAN_LOG_ERROR,
                 "Found attempt to observe dynamic array field \"%s\". Dynamic arrays are not observable.", field_name)
        return KAN_FALSE;

    case KAN_REFLECTION_ARCHETYPE_PATCH:
        KAN_LOG (repository_validation, KAN_LOG_ERROR,
                 "Found attempt to observe reflection patch field \"%s\". Reflection patches are not observable.",
                 field_name)
        return KAN_FALSE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static kan_bool_t validation_copy_out_is_possible (kan_reflection_registry_t registry,
                                                   const struct kan_reflection_field_t *source,
                                                   const struct kan_reflection_field_t *target)
{
    if (source->archetype != target->archetype)
    {
        KAN_LOG (repository_validation, KAN_LOG_ERROR,
                 "Found copy out attempt for fields of different archetypes with names \"%s\" and \"%s\".",
                 source->name, target->name)
        return KAN_FALSE;
    }

    if (source->size != target->size)
    {
        KAN_LOG (
            repository_validation, KAN_LOG_ERROR,
            "Found copy out attempt for fields of different sizes with names \"%s\" and \"%s\" and sizes %lu and %lu.",
            source->name, target->name, (unsigned long) source->size, (unsigned long) target->size)
        return KAN_FALSE;
    }

    if (!validation_field_is_observable (registry, source->archetype, source->name, &source->archetype_struct))
    {
        KAN_LOG (repository_validation, KAN_LOG_ERROR, "Only observable fields are supported for copy out.")
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static kan_bool_t validation_value_index_is_possible (const struct kan_reflection_field_t *field)
{
    if (field->visibility_condition_values_count > 0u)
    {
        KAN_LOG (repository_validation, KAN_LOG_ERROR,
                 "Passed field \"%s\" has visibility conditions. Fields with visibility conditions cannot be indexed.",
                 field->name)
        return KAN_FALSE;
    }

    switch (field->archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_FLOATING:
    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
    case KAN_REFLECTION_ARCHETYPE_ENUM:
        if (field->size > sizeof (uint64_t))
        {
            KAN_LOG (repository_validation, KAN_LOG_ERROR,
                     "Passed field \"%s\" to value index with supported archetype, but suspicious size %lu. Broken "
                     "reflection?",
                     field->name, (unsigned long) field->size)
            return KAN_FALSE;
        }

        return KAN_TRUE;

    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
    case KAN_REFLECTION_ARCHETYPE_STRUCT:
    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
    case KAN_REFLECTION_ARCHETYPE_PATCH:
        KAN_LOG (repository_validation, KAN_LOG_ERROR, "Passed field \"%s\" to value index with unsupported archetype.",
                 field->name)
        return KAN_FALSE;
    }

    return KAN_FALSE;
}
#endif

static struct interned_field_path_t copy_field_path (struct kan_repository_field_path_t input,
                                                     kan_allocation_group_t allocation_group)
{
    struct interned_field_path_t result;
    result.length = input.reflection_path_length;
    result.path = kan_allocate_batched (allocation_group, sizeof (kan_interned_string_t) * result.length);

    for (uint64_t index = 0u; index < input.reflection_path_length; ++index)
    {
        result.path[index] = kan_string_intern (input.reflection_path[index]);
    }

    return result;
}

static kan_bool_t is_field_path_equal (struct kan_repository_field_path_t input, struct interned_field_path_t interned)
{
    if (input.reflection_path_length != interned.length)
    {
        return KAN_FALSE;
    }

    for (uint64_t index = 0u; index < input.reflection_path_length; ++index)
    {
        if (strcmp (input.reflection_path[index], interned.path[index]) != 0)
        {
            return KAN_FALSE;
        }
    }

    return KAN_TRUE;
}

static void shutdown_field_path (struct interned_field_path_t interned, kan_allocation_group_t group)
{
    kan_free_batched (group, interned.path);
}

static void apply_copy_outs (uint64_t copy_outs_count, struct copy_out_t *copy_outs, const void *source, void *target)
{
    for (uint64_t index = 0u; index < copy_outs_count; ++index)
    {
        struct copy_out_t *copy_out = &copy_outs[index];
        memcpy ((uint8_t *) target + copy_out->target_offset, (uint8_t *) source + copy_out->source_offset,
                copy_out->size);
    }
}

static const struct kan_reflection_field_t *query_field_for_automatic_event_from_path (
    kan_interned_string_t struct_name,
    struct kan_repository_field_path_t *path,
    kan_reflection_registry_t registry,
    struct kan_stack_group_allocator_t *temporary_allocator,
    uint64_t *output_absolute_offset,
    uint64_t *output_size_with_padding)
{
    kan_interned_string_t *interned_path = kan_stack_group_allocator_allocate (
        temporary_allocator, sizeof (kan_interned_string_t) * path->reflection_path_length,
        _Alignof (kan_interned_string_t));

    for (uint64_t index = 0u; index < path->reflection_path_length; ++index)
    {
        interned_path[index] = kan_string_intern (path->reflection_path[index]);
    }

    const struct kan_reflection_field_t *field =
        kan_reflection_registry_query_local_field (registry, struct_name, path->reflection_path_length, interned_path,
                                                   output_absolute_offset, output_size_with_padding);

    if (!field)
    {
        KAN_LOG (repository, KAN_LOG_ERROR,
                 "Unable to query field for automatic event from path: it is either non-local or it does "
                 "not exist. Path:")

        for (uint64_t path_element_index = 0u; path_element_index < path->reflection_path_length; ++path_element_index)
        {
            KAN_LOG (repository, KAN_LOG_ERROR, "    - \"%s\"", path->reflection_path[path_element_index])
        }
    }

    return field;
}

static struct copy_out_list_node_t *extract_raw_copy_outs (kan_interned_string_t struct_name,
                                                           uint64_t copy_outs_count,
                                                           struct kan_repository_copy_out_t *copy_outs,
                                                           kan_reflection_registry_t registry,
                                                           struct kan_stack_group_allocator_t *temporary_allocator)
{
    struct copy_out_list_node_t *first = NULL;
    struct copy_out_list_node_t *last = NULL;

    for (uint64_t index = 0u; index < copy_outs_count; ++index)
    {
        struct kan_repository_copy_out_t *copy_out = &copy_outs[index];

        uint64_t source_absolute_offset;
        uint64_t source_size_with_padding;
        const struct kan_reflection_field_t *source_field = query_field_for_automatic_event_from_path (
            struct_name, &copy_out->source_path, registry, temporary_allocator, &source_absolute_offset,
            &source_size_with_padding);

        uint64_t target_absolute_offset;
        uint64_t target_size_with_padding;
        const struct kan_reflection_field_t *target_field = query_field_for_automatic_event_from_path (
            struct_name, &copy_out->target_path, registry, temporary_allocator, &target_absolute_offset,
            &target_size_with_padding);

        if (!source_field || !target_field)
        {
            continue;
        }

#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
        if (!validation_copy_out_is_possible (registry, source_field, target_field))
        {
            continue;
        }
#endif

        struct copy_out_list_node_t *node = (struct copy_out_list_node_t *) kan_stack_group_allocator_allocate (
            temporary_allocator, sizeof (struct copy_out_list_node_t), _Alignof (struct copy_out_list_node_t));

        node->next = NULL;
        node->source_offset = source_absolute_offset;
        node->target_offset = target_absolute_offset;
        node->size = KAN_MIN (source_size_with_padding, target_size_with_padding);

        if (last)
        {
            last->next = node;
            last = node;
        }
        else
        {
            first = node;
            last = node;
        }
    }

    return first;
}

static struct copy_out_list_node_t *convert_to_buffer_copy_outs (
    struct copy_out_list_node_t *input,
    struct observation_buffer_definition_t *buffer,
    struct kan_stack_group_allocator_t *temporary_allocator)
{
    struct copy_out_list_node_t *first = NULL;
    struct copy_out_list_node_t *last = NULL;

    while (input)
    {
        uint64_t buffer_offset = 0u;
        for (uint64_t index = 0u; index < buffer->scenario_chunks_count; ++index)
        {
            struct observation_buffer_scenario_chunk_t *chunk = &buffer->scenario_chunks[index];
            if (input->source_offset < chunk->source_offset)
            {
                KAN_LOG (repository, KAN_LOG_ERROR,
                         "Internal error: failed to map copy out to observation buffer. Double-check whether your "
                         "unchanged copy outs are tracked as observed fields.")
                break;
            }

            if (input->source_offset < chunk->source_offset + chunk->size)
            {
                const uint64_t offset = input->source_offset - chunk->source_offset;
                const uint64_t size_in_chunk = KAN_MIN (input->size, chunk->size - offset);

                struct copy_out_list_node_t *node = (struct copy_out_list_node_t *) kan_stack_group_allocator_allocate (
                    temporary_allocator, sizeof (struct copy_out_list_node_t), _Alignof (struct copy_out_list_node_t));

                node->next = NULL;
                node->source_offset = buffer_offset + offset;
                node->target_offset = input->target_offset;
                node->size = size_in_chunk;

                if (last)
                {
                    last->next = node;
                    last = node;
                }
                else
                {
                    first = node;
                    last = node;
                }

                input->source_offset += size_in_chunk;
                input->target_offset += size_in_chunk;
                input->size -= size_in_chunk;

                if (input->size == 0u)
                {
                    break;
                }
            }

            buffer_offset = kan_apply_alignment (buffer_offset + chunk->size, OBSERVATION_BUFFER_ALIGNMENT);
        }

        input = input->next;
    }

    return first;
}

static struct copy_out_list_node_t *merge_copy_outs (struct copy_out_list_node_t *input,
                                                     struct kan_stack_group_allocator_t *temporary_allocator,
                                                     uint64_t *output_count)
{
    if (!input)
    {
        return NULL;
    }

    uint64_t count = 0u;
    struct copy_out_list_node_t *copy_out = input;

    while (copy_out)
    {
        ++count;
        copy_out = copy_out->next;
    }

    struct copy_out_list_node_t **input_array =
        kan_stack_group_allocator_allocate (temporary_allocator, count * sizeof (void *), _Alignof (void *));

    copy_out = input;
    uint64_t copy_out_index = 0u;

    while (copy_out)
    {
        input_array[copy_out_index] = copy_out;
        ++copy_out_index;
        copy_out = copy_out->next;
    }

    {
#define LESS(first_index, second_index)                                                                                \
    (input_array[first_index]->source_offset < input_array[second_index]->source_offset)

#define SWAP(first_index, second_index)                                                                                \
    copy_out = input_array[first_index], input_array[first_index] = input_array[second_index],                         \
    input_array[second_index] = copy_out

        QSORT (count, LESS, SWAP);
#undef LESS
#undef SWAP
    }

    struct copy_out_list_node_t *first = NULL;
    struct copy_out_list_node_t *last = NULL;
    *output_count = 0u;

    while (input)
    {
        // We don't handle intersections here (they will be just left as it is, no optimization).
        if (last && last->source_offset + last->size == input->source_offset &&
            last->target_offset + last->size == input->target_offset)
        {
            last->size += input->size;
        }
        else
        {
            struct copy_out_list_node_t *node = (struct copy_out_list_node_t *) kan_stack_group_allocator_allocate (
                temporary_allocator, sizeof (struct copy_out_list_node_t), _Alignof (struct copy_out_list_node_t));

            node->next = NULL;
            node->source_offset = input->source_offset;
            node->target_offset = input->target_offset;
            node->size = input->size;

            if (last)
            {
                last->next = node;
                last = node;
            }
            else
            {
                first = node;
                last = node;
            }

            ++*output_count;
        }

        input = input->next;
    }

    return first;
}

static void observation_buffer_definition_init (struct observation_buffer_definition_t *definition)
{
    definition->buffer_size = 0u;
    definition->scenario_chunks_count = 0u;
    definition->scenario_chunks = NULL;
}

static void observation_buffer_definition_build (struct observation_buffer_definition_t *definition,
                                                 struct observation_buffer_scenario_chunk_list_node_t *first_chunk,
                                                 struct kan_stack_group_allocator_t *temporary_allocator,
                                                 kan_allocation_group_t result_allocation_group)
{
    uint64_t initial_chunks_count = 0u;
    struct observation_buffer_scenario_chunk_list_node_t *chunk = first_chunk;

    while (chunk)
    {
        ++initial_chunks_count;
        chunk = chunk->next;
    }

    struct observation_buffer_scenario_chunk_list_node_t **initial_chunks = kan_stack_group_allocator_allocate (
        temporary_allocator, initial_chunks_count * sizeof (void *), _Alignof (void *));

    chunk = first_chunk;
    uint64_t chunk_index = 0u;

    while (chunk)
    {
        initial_chunks[chunk_index] = chunk;
        ++chunk_index;
        chunk = chunk->next;
    }

    {
#define LESS(first_index, second_index)                                                                                \
    (initial_chunks[first_index]->source_offset == initial_chunks[second_index]->source_offset ?                       \
         (initial_chunks[first_index]->size < initial_chunks[second_index]->size) :                                    \
         (initial_chunks[first_index]->source_offset < initial_chunks[second_index]->source_offset))

#define SWAP(first_index, second_index)                                                                                \
    chunk = initial_chunks[first_index], initial_chunks[first_index] = initial_chunks[second_index],                   \
    initial_chunks[second_index] = chunk

        QSORT (initial_chunks_count, LESS, SWAP);
#undef LESS
#undef SWAP
    }

    // Start by splitting all chunks into a list of chunks with zero intersections.
    struct observation_buffer_scenario_chunk_list_node_t *first_no_intersection_node = NULL;
    struct observation_buffer_scenario_chunk_list_node_t *last_no_intersection_node = NULL;
    chunk_index = 0u;

    while (chunk_index < initial_chunks_count)
    {
        struct observation_buffer_scenario_chunk_list_node_t *initial_node = initial_chunks[chunk_index];
        if (initial_node->size == 0u)
        {
            ++chunk_index;
            continue;
        }

        uint64_t no_intersection_size = initial_node->size;
        uint64_t no_intersection_flags = initial_node->flags;
        uint64_t affected_stop_index = initial_chunks_count;

        for (uint64_t next_index = chunk_index + 1u; next_index < initial_chunks_count; ++next_index)
        {
            struct observation_buffer_scenario_chunk_list_node_t *next_node = initial_chunks[next_index];
            if (next_node->source_offset == initial_node->source_offset)
            {
                KAN_ASSERT (next_node->size >= initial_node->size)
                no_intersection_flags |= next_node->flags;
            }
            else
            {
                KAN_ASSERT (next_node->source_offset > initial_node->source_offset)
                no_intersection_size =
                    KAN_MIN (no_intersection_size, next_node->source_offset - initial_node->source_offset);
                affected_stop_index = next_index;
                break;
            }
        }

        KAN_ASSERT (no_intersection_size > 0u)
        for (uint64_t affected_index = chunk_index; affected_index < affected_stop_index; ++affected_index)
        {
            struct observation_buffer_scenario_chunk_list_node_t *affected_node = initial_chunks[affected_index];
            affected_node->size -= no_intersection_size;
            affected_node->source_offset += no_intersection_size;
        }

        struct observation_buffer_scenario_chunk_list_node_t *no_intersection_node =
            kan_stack_group_allocator_allocate (temporary_allocator,
                                                sizeof (struct observation_buffer_scenario_chunk_list_node_t),
                                                _Alignof (struct observation_buffer_scenario_chunk_list_node_t));

        *no_intersection_node = (struct observation_buffer_scenario_chunk_list_node_t) {
            .source_offset = initial_node->source_offset - no_intersection_size,
            .size = no_intersection_size,
            .flags = no_intersection_flags,
            .next = NULL,
        };

        if (first_no_intersection_node == NULL)
        {
            first_no_intersection_node = no_intersection_node;
            last_no_intersection_node = no_intersection_node;
        }
        else
        {
            last_no_intersection_node->next = no_intersection_node;
            last_no_intersection_node = no_intersection_node;
        }
    }

    // We don't have intersections now, therefore we can try to safely merge nodes.
    struct observation_buffer_scenario_chunk_list_node_t *first_merged_node = NULL;
    struct observation_buffer_scenario_chunk_list_node_t *last_merged_node = NULL;
    uint64_t merged_nodes_count = 0u;
    chunk = first_no_intersection_node;

    while (chunk)
    {
        if (last_merged_node && last_merged_node->flags == chunk->flags &&
            last_merged_node->source_offset + last_merged_node->size == chunk->source_offset)
        {
            last_merged_node->size += chunk->size;
        }
        else
        {
            struct observation_buffer_scenario_chunk_list_node_t *merged_node = kan_stack_group_allocator_allocate (
                temporary_allocator, sizeof (struct observation_buffer_scenario_chunk_list_node_t),
                _Alignof (struct observation_buffer_scenario_chunk_list_node_t));

            *merged_node = *chunk;
            merged_node->next = NULL;
            ++merged_nodes_count;

            if (first_merged_node == NULL)
            {
                first_merged_node = merged_node;
                last_merged_node = merged_node;
            }
            else
            {
                last_merged_node->next = merged_node;
                last_merged_node = merged_node;
            }
        }

        chunk = chunk->next;
    }

    definition->buffer_size = 0u;
    definition->scenario_chunks_count = merged_nodes_count;

    definition->scenario_chunks =
        kan_allocate_general (result_allocation_group,
                              definition->scenario_chunks_count * sizeof (struct observation_buffer_scenario_chunk_t),
                              _Alignof (struct observation_buffer_scenario_chunk_t));

    chunk = first_merged_node;
    chunk_index = 0u;

    while (chunk)
    {
        definition->buffer_size =
            kan_apply_alignment (definition->buffer_size + chunk->size, OBSERVATION_BUFFER_ALIGNMENT);

        definition->scenario_chunks[chunk_index] = (struct observation_buffer_scenario_chunk_t) {
            .source_offset = chunk->source_offset,
            .size = chunk->size,
            .flags = chunk->flags,
        };

        chunk = chunk->next;
        ++chunk_index;
    }
}

static void observation_buffer_definition_import (struct observation_buffer_definition_t *definition,
                                                  void *observation_buffer_memory,
                                                  void *record)
{
    if (!observation_buffer_memory)
    {
        return;
    }

    KAN_ASSERT (definition->buffer_size > 0u)
    KAN_ASSERT (definition->scenario_chunks_count > 0u)
    KAN_ASSERT (definition->scenario_chunks)

    uint8_t *output = (uint8_t *) observation_buffer_memory;
    const struct observation_buffer_scenario_chunk_t *chunk = definition->scenario_chunks;
    const struct observation_buffer_scenario_chunk_t *end =
        definition->scenario_chunks + definition->scenario_chunks_count;

    while (chunk != end)
    {
        memcpy (output, (uint8_t *) record + chunk->source_offset, chunk->size);
        output += kan_apply_alignment (chunk->size, OBSERVATION_BUFFER_CHUNK_ALIGNMENT);
        ++chunk;
    }

    KAN_ASSERT ((uint64_t) (output - (uint8_t *) observation_buffer_memory) == definition->buffer_size)
}

static uint64_t observation_buffer_definition_compare (struct observation_buffer_definition_t *definition,
                                                       void *observation_buffer_memory,
                                                       void *record)
{
    if (!observation_buffer_memory)
    {
        return 0u;
    }

    uint64_t result = 0u;
    const uint8_t *input = (uint8_t *) observation_buffer_memory;
    const struct observation_buffer_scenario_chunk_t *chunk = definition->scenario_chunks;
    const struct observation_buffer_scenario_chunk_t *end =
        definition->scenario_chunks + definition->scenario_chunks_count;

    while (chunk != end)
    {
        if (memcmp (input, (uint8_t *) record + chunk->source_offset, chunk->size) != 0)
        {
            result |= chunk->flags;
        }

        input += kan_apply_alignment (chunk->size, OBSERVATION_BUFFER_CHUNK_ALIGNMENT);
        ++chunk;
    }

    return result;
}

static void observation_buffer_definition_shutdown (struct observation_buffer_definition_t *definition,
                                                    kan_allocation_group_t allocation_group)
{
    if (definition->scenario_chunks)
    {
        kan_free_general (allocation_group, definition->scenario_chunks,
                          definition->scenario_chunks_count * sizeof (struct observation_buffer_scenario_chunk_t));
    }
}

static struct observation_event_trigger_t *observation_event_trigger_next (struct observation_event_trigger_t *trigger)
{
    uint8_t *raw_pointer = (uint8_t *) trigger;
    const uint64_t trigger_size =
        sizeof (struct observation_event_trigger_t) +
        (trigger->buffer_copy_outs_count + trigger->record_copy_outs_count) * sizeof (struct copy_out_t);

    return (struct observation_event_trigger_t *) (raw_pointer +
                                                   kan_apply_alignment (trigger_size,
                                                                        _Alignof (struct observation_event_trigger_t)));
}

static void observation_event_triggers_definition_init (struct observation_event_triggers_definition_t *definition)
{
    definition->event_triggers_count = 0u;
    definition->event_triggers = NULL;
}

static struct event_storage_node_t *query_event_storage_across_hierarchy (struct repository_t *repository,
                                                                          kan_interned_string_t type_name);

static void observation_event_triggers_definition_build (struct observation_event_triggers_definition_t *definition,
                                                         uint64_t *event_flag,
                                                         struct repository_t *repository,
                                                         const struct kan_reflection_struct_t *observed_struct,
                                                         struct observation_buffer_definition_t *buffer,
                                                         struct kan_stack_group_allocator_t *temporary_allocator,
                                                         kan_allocation_group_t result_allocation_group)
{
    ensure_interned_strings_ready ();
    struct kan_reflection_struct_meta_iterator_t iterator = kan_reflection_registry_query_struct_meta (
        repository->registry, observed_struct->name, meta_automatic_on_change_event_name);

    struct kan_repository_meta_automatic_on_change_event_t *event =
        (struct kan_repository_meta_automatic_on_change_event_t *) kan_reflection_struct_meta_iterator_get (&iterator);

    uint64_t triggers_count = 0u;
    uint64_t triggers_array_size = 0u;
    struct observation_event_trigger_list_node_t *first_event_node = NULL;

    while (event)
    {
        const kan_interned_string_t event_type = kan_string_intern (event->event_type);
        struct event_storage_node_t *event_storage =
            query_event_storage_across_hierarchy (repository, kan_string_intern (event_type));

        if (event_storage)
        {
            struct copy_out_list_node_t *raw_buffer_copy_outs =
                extract_raw_copy_outs (event_type, event->unchanged_copy_outs_count, event->unchanged_copy_outs,
                                       repository->registry, temporary_allocator);

            struct copy_out_list_node_t *retargeted_buffer_copy_outs =
                convert_to_buffer_copy_outs (raw_buffer_copy_outs, buffer, temporary_allocator);

            uint64_t merged_buffer_copy_outs_count;
            struct copy_out_list_node_t *merged_buffer_copy_outs =
                merge_copy_outs (retargeted_buffer_copy_outs, temporary_allocator, &merged_buffer_copy_outs_count);

            struct copy_out_list_node_t *raw_record_copy_outs =
                extract_raw_copy_outs (event_type, event->changed_copy_outs_count, event->changed_copy_outs,
                                       repository->registry, temporary_allocator);

            uint64_t merged_record_copy_outs_count;
            struct copy_out_list_node_t *merged_record_copy_outs =
                merge_copy_outs (raw_record_copy_outs, temporary_allocator, &merged_record_copy_outs_count);

            struct observation_event_trigger_list_node_t *event_node = kan_stack_group_allocator_allocate (
                temporary_allocator, sizeof (struct observation_event_trigger_list_node_t),
                _Alignof (struct observation_event_trigger_list_node_t));

            event_node->flags = *event_flag;
            kan_repository_event_insert_query_init (&event_node->event_insert_query,
                                                    (kan_repository_event_storage_t) event_storage);
            event_node->buffer_copy_outs_count = merged_buffer_copy_outs_count;
            event_node->buffer_copy_outs = merged_buffer_copy_outs;
            event_node->record_copy_outs_count = merged_record_copy_outs_count;
            event_node->record_copy_outs = merged_record_copy_outs;

            event_node->next = first_event_node;
            first_event_node = event_node;

            ++triggers_count;
            triggers_array_size = kan_apply_alignment (
                triggers_array_size + sizeof (struct observation_event_trigger_t) +
                    (merged_buffer_copy_outs_count + merged_record_copy_outs_count) * sizeof (struct copy_out_t),
                _Alignof (struct observation_event_trigger_t));

            *event_flag <<= 1u;
            KAN_ASSERT (*event_flag > 0u)
        }

        kan_reflection_struct_meta_iterator_next (&iterator);
        event = (struct kan_repository_meta_automatic_on_change_event_t *) kan_reflection_struct_meta_iterator_get (
            &iterator);
    }

    definition->event_triggers_count = triggers_count;
    if (triggers_count > 0u)
    {
        definition->event_triggers = kan_allocate_general (result_allocation_group, triggers_array_size,
                                                           _Alignof (struct observation_event_trigger_t));
    }
    else
    {
        definition->event_triggers = NULL;
    }

    struct observation_event_trigger_t *trigger = (struct observation_event_trigger_t *) definition->event_triggers;
    while (first_event_node)
    {
        trigger->flags = first_event_node->flags;
        trigger->event_insert_query = first_event_node->event_insert_query;
        trigger->buffer_copy_outs_count = first_event_node->buffer_copy_outs_count;
        trigger->record_copy_outs_count = first_event_node->record_copy_outs_count;

        uint64_t copy_out_index = 0u;
        struct copy_out_list_node_t *copy_out = first_event_node->buffer_copy_outs;

        while (copy_out)
        {
            trigger->copy_outs[copy_out_index] = (struct copy_out_t) {
                .source_offset = copy_out->source_offset,
                .target_offset = copy_out->target_offset,
                .size = copy_out->size,
            };

            ++copy_out_index;
            copy_out = copy_out->next;
        }

        copy_out = first_event_node->record_copy_outs;
        while (copy_out)
        {
            trigger->copy_outs[copy_out_index] = (struct copy_out_t) {
                .source_offset = copy_out->source_offset,
                .target_offset = copy_out->target_offset,
                .size = copy_out->size,
            };

            ++copy_out_index;
            copy_out = copy_out->next;
        }

        trigger = observation_event_trigger_next (trigger);
        first_event_node = first_event_node->next;
    }
}

static void observation_event_triggers_definition_fire (struct observation_event_triggers_definition_t *definition,
                                                        uint64_t flags,
                                                        struct observation_buffer_definition_t *buffer,
                                                        void *observation_buffer_memory,
                                                        void *record)
{
    if (!definition->event_triggers)
    {
        return;
    }

    KAN_ASSERT (definition->event_triggers_count > 0u)
    KAN_ASSERT (buffer->buffer_size > 0u)
    KAN_ASSERT (observation_buffer_memory)

    struct observation_event_trigger_t *current_trigger = definition->event_triggers;
    for (uint64_t trigger_index = 0u; trigger_index < definition->event_triggers_count; ++trigger_index)
    {
        if (current_trigger->flags & flags)
        {
            struct kan_repository_event_insertion_package_t package =
                kan_repository_event_insert_query_execute (&current_trigger->event_insert_query);
            void *event = kan_repository_event_insertion_package_get (&package);

            if (event)
            {
                apply_copy_outs (current_trigger->buffer_copy_outs_count, current_trigger->copy_outs,
                                 observation_buffer_memory, event);
                apply_copy_outs (current_trigger->record_copy_outs_count,
                                 current_trigger->copy_outs + current_trigger->buffer_copy_outs_count, record, event);
                kan_repository_event_insertion_package_submit (&package);
            }
        }

        if (trigger_index != definition->event_triggers_count - 1u)
        {
            current_trigger = observation_event_trigger_next (current_trigger);
        }
    }
}

static void observation_event_triggers_definition_shutdown (struct observation_event_triggers_definition_t *definition,
                                                            kan_allocation_group_t allocation_group)
{
    if (definition->event_triggers)
    {
        struct observation_event_trigger_t *first_trigger = definition->event_triggers;
        struct observation_event_trigger_t *current_trigger = first_trigger;

        for (uint64_t trigger_index = 0u; trigger_index < definition->event_triggers_count; ++trigger_index)
        {
            kan_repository_event_insert_query_shutdown (&current_trigger->event_insert_query);
            current_trigger = observation_event_trigger_next (current_trigger);
        }

        uint64_t triggers_size = (uint8_t *) current_trigger - (uint8_t *) first_trigger;
        kan_free_general (allocation_group, definition->event_triggers, triggers_size);
    }
}

static struct lifetime_event_trigger_t *lifetime_event_trigger_next (struct lifetime_event_trigger_t *trigger)
{
    uint8_t *raw_pointer = (uint8_t *) trigger;
    const uint64_t trigger_size =
        sizeof (struct lifetime_event_trigger_t) + trigger->copy_outs_count * sizeof (struct copy_out_t);

    return (struct lifetime_event_trigger_t *) (raw_pointer +
                                                kan_apply_alignment (trigger_size,
                                                                     _Alignof (struct lifetime_event_trigger_t)));
}

static void lifetime_event_triggers_definition_init (struct lifetime_event_triggers_definition_t *definition)
{
    definition->event_triggers_count = 0u;
    definition->event_triggers = NULL;
}

static struct lifetime_event_trigger_list_node_t *lifetime_event_triggers_extract_on_insert (
    struct repository_t *repository,
    const struct kan_reflection_struct_t *observed_struct,
    struct kan_stack_group_allocator_t *temporary_allocator,
    uint64_t *count_output,
    uint64_t *array_size_output)
{
    ensure_interned_strings_ready ();
    struct kan_reflection_struct_meta_iterator_t iterator = kan_reflection_registry_query_struct_meta (
        repository->registry, observed_struct->name, meta_automatic_on_insert_event_name);

    struct kan_repository_meta_automatic_on_insert_event_t *event =
        (struct kan_repository_meta_automatic_on_insert_event_t *) kan_reflection_struct_meta_iterator_get (&iterator);

    struct lifetime_event_trigger_list_node_t *first_event_node = NULL;
    *count_output = 0u;
    *array_size_output = 0u;

    while (event)
    {
        const kan_interned_string_t event_type = kan_string_intern (event->event_type);
        struct event_storage_node_t *event_storage = query_event_storage_across_hierarchy (repository, event_type);

        if (event_storage)
        {
            struct copy_out_list_node_t *raw_copy_outs = extract_raw_copy_outs (
                event_type, event->copy_outs_count, event->copy_outs, repository->registry, temporary_allocator);

            uint64_t merged_copy_outs_count;
            struct copy_out_list_node_t *merged_copy_outs =
                merge_copy_outs (raw_copy_outs, temporary_allocator, &merged_copy_outs_count);

            struct lifetime_event_trigger_list_node_t *event_node = kan_stack_group_allocator_allocate (
                temporary_allocator, sizeof (struct lifetime_event_trigger_list_node_t),
                _Alignof (struct lifetime_event_trigger_list_node_t));

            kan_repository_event_insert_query_init (&event_node->event_insert_query,
                                                    (kan_repository_event_storage_t) event_storage);
            event_node->copy_outs_count = merged_copy_outs_count;
            event_node->copy_outs = merged_copy_outs;

            event_node->next = first_event_node;
            first_event_node = event_node;

            ++*count_output;
            *array_size_output = kan_apply_alignment (*array_size_output + sizeof (struct lifetime_event_trigger_t) +
                                                          merged_copy_outs_count * sizeof (struct copy_out_t),
                                                      _Alignof (struct lifetime_event_trigger_t));
        }

        kan_reflection_struct_meta_iterator_next (&iterator);
        event = (struct kan_repository_meta_automatic_on_insert_event_t *) kan_reflection_struct_meta_iterator_get (
            &iterator);
    }

    return first_event_node;
}

static struct lifetime_event_trigger_list_node_t *lifetime_event_triggers_extract_on_delete (
    struct repository_t *repository,
    const struct kan_reflection_struct_t *observed_struct,
    struct kan_stack_group_allocator_t *temporary_allocator,
    uint64_t *count_output,
    uint64_t *array_size_output)
{
    ensure_interned_strings_ready ();
    struct kan_reflection_struct_meta_iterator_t iterator = kan_reflection_registry_query_struct_meta (
        repository->registry, observed_struct->name, meta_automatic_on_delete_event_name);

    struct kan_repository_meta_automatic_on_delete_event_t *event =
        (struct kan_repository_meta_automatic_on_delete_event_t *) kan_reflection_struct_meta_iterator_get (&iterator);

    struct lifetime_event_trigger_list_node_t *first_event_node = NULL;
    *count_output = 0u;
    *array_size_output = 0u;

    while (event)
    {
        const kan_interned_string_t event_type = kan_string_intern (event->event_type);
        struct event_storage_node_t *event_storage = query_event_storage_across_hierarchy (repository, event_type);

        if (event_storage)
        {
            struct copy_out_list_node_t *raw_copy_outs = extract_raw_copy_outs (
                event_type, event->copy_outs_count, event->copy_outs, repository->registry, temporary_allocator);

            uint64_t merged_copy_outs_count;
            struct copy_out_list_node_t *merged_copy_outs =
                merge_copy_outs (raw_copy_outs, temporary_allocator, &merged_copy_outs_count);

            struct lifetime_event_trigger_list_node_t *event_node = kan_stack_group_allocator_allocate (
                temporary_allocator, sizeof (struct lifetime_event_trigger_list_node_t),
                _Alignof (struct lifetime_event_trigger_list_node_t));

            kan_repository_event_insert_query_init (&event_node->event_insert_query,
                                                    (kan_repository_event_storage_t) event_storage);
            event_node->copy_outs_count = merged_copy_outs_count;
            event_node->copy_outs = merged_copy_outs;

            event_node->next = first_event_node;
            first_event_node = event_node;

            ++*count_output;
            *array_size_output = kan_apply_alignment (*array_size_output + sizeof (struct lifetime_event_trigger_t) +
                                                          merged_copy_outs_count * sizeof (struct copy_out_t),
                                                      _Alignof (struct lifetime_event_trigger_t));
        }

        kan_reflection_struct_meta_iterator_next (&iterator);
        event = (struct kan_repository_meta_automatic_on_delete_event_t *) kan_reflection_struct_meta_iterator_get (
            &iterator);
    }

    return first_event_node;
}

static void lifetime_event_triggers_definition_build (struct lifetime_event_triggers_definition_t *definition,
                                                      struct repository_t *repository,
                                                      const struct kan_reflection_struct_t *observed_struct,
                                                      enum lifetime_event_trigger_type_t type,
                                                      struct kan_stack_group_allocator_t *temporary_allocator,
                                                      kan_allocation_group_t result_allocation_group)
{
    uint64_t triggers_count = 0u;
    uint64_t triggers_array_size = 0u;
    struct lifetime_event_trigger_list_node_t *first_event_node = NULL;

    switch (type)
    {
    case LIFETIME_EVENT_TRIGGER_ON_INSERT:
        first_event_node = lifetime_event_triggers_extract_on_insert (repository, observed_struct, temporary_allocator,
                                                                      &triggers_count, &triggers_array_size);
        break;

    case LIFETIME_EVENT_TRIGGER_ON_DELETE:
        first_event_node = lifetime_event_triggers_extract_on_delete (repository, observed_struct, temporary_allocator,
                                                                      &triggers_count, &triggers_array_size);
        break;
    }

    definition->event_triggers_count = triggers_count;
    if (triggers_count > 0u)
    {
        definition->event_triggers = kan_allocate_general (result_allocation_group, triggers_array_size,
                                                           _Alignof (struct lifetime_event_trigger_t));
    }
    else
    {
        definition->event_triggers = NULL;
    }

    struct lifetime_event_trigger_t *trigger = (struct lifetime_event_trigger_t *) definition->event_triggers;
    while (first_event_node)
    {
        trigger->event_insert_query = first_event_node->event_insert_query;
        trigger->copy_outs_count = first_event_node->copy_outs_count;

        uint64_t copy_out_index = 0u;
        struct copy_out_list_node_t *copy_out = first_event_node->copy_outs;

        while (copy_out)
        {
            trigger->copy_outs[copy_out_index] = (struct copy_out_t) {
                .source_offset = copy_out->source_offset,
                .target_offset = copy_out->target_offset,
                .size = copy_out->size,
            };

            ++copy_out_index;
            copy_out = copy_out->next;
        }

        trigger = lifetime_event_trigger_next (trigger);
        first_event_node = first_event_node->next;
    }
}

static void lifetime_event_triggers_definition_fire (struct lifetime_event_triggers_definition_t *definition,
                                                     void *record)
{
    if (!definition->event_triggers)
    {
        return;
    }

    KAN_ASSERT (definition->event_triggers_count > 0u)
    KAN_ASSERT (record)

    struct lifetime_event_trigger_t *current_trigger = definition->event_triggers;
    for (uint64_t trigger_index = 0u; trigger_index < definition->event_triggers_count; ++trigger_index)
    {
        struct kan_repository_event_insertion_package_t package =
            kan_repository_event_insert_query_execute (&current_trigger->event_insert_query);
        void *event = kan_repository_event_insertion_package_get (&package);

        if (event)
        {
            apply_copy_outs (current_trigger->copy_outs_count, current_trigger->copy_outs, record, event);
            kan_repository_event_insertion_package_submit (&package);
        }

        if (trigger_index != definition->event_triggers_count - 1u)
        {
            current_trigger = lifetime_event_trigger_next (current_trigger);
        }
    }
}

static void lifetime_event_triggers_definition_shutdown (struct lifetime_event_triggers_definition_t *definition,
                                                         kan_allocation_group_t allocation_group)
{
    if (definition->event_triggers)
    {
        struct lifetime_event_trigger_t *first_trigger = definition->event_triggers;
        struct lifetime_event_trigger_t *current_trigger = first_trigger;

        for (uint64_t trigger_index = 0u; trigger_index < definition->event_triggers_count; ++trigger_index)
        {
            kan_repository_event_insert_query_shutdown (&current_trigger->event_insert_query);
            current_trigger = lifetime_event_trigger_next (current_trigger);
        }

        uint64_t triggers_size = (uint8_t *) current_trigger - (uint8_t *) first_trigger;
        kan_free_general (allocation_group, definition->event_triggers, triggers_size);
    }
}

static void cascade_deleters_definition_init (struct cascade_deleters_definition_t *definition)
{
    definition->cascade_deleters_count = 0u;
    definition->cascade_deleters = NULL;
}

static struct indexed_storage_node_t *query_indexed_storage_across_hierarchy (struct repository_t *repository,
                                                                              kan_interned_string_t type_name);

static void cascade_deleters_definition_build (struct cascade_deleters_definition_t *definition,
                                               kan_interned_string_t parent_type_name,
                                               struct repository_t *repository,
                                               struct kan_stack_group_allocator_t *temporary_allocator,
                                               kan_allocation_group_t result_allocation_group)
{
    ensure_interned_strings_ready ();
    KAN_ASSERT (!definition->cascade_deleters)

    struct kan_reflection_struct_meta_iterator_t iterator = kan_reflection_registry_query_struct_meta (
        repository->registry, parent_type_name, meta_automatic_cascade_deletion_name);

    struct kan_repository_meta_automatic_cascade_deletion_t *meta =
        (struct kan_repository_meta_automatic_cascade_deletion_t *) kan_reflection_struct_meta_iterator_get (&iterator);

    struct cascade_deleter_node_t *first_node = NULL;
    uint64_t nodes_count = 0u;

    while (meta)
    {
        kan_interned_string_t child_type_name = kan_string_intern (meta->child_type_name);
        struct indexed_storage_node_t *child_storage =
            query_indexed_storage_across_hierarchy (repository, child_type_name);

        uint64_t parent_absolute_offset;
        uint64_t parent_size_with_padding;
        const struct kan_reflection_field_t *parent_field = query_field_for_automatic_event_from_path (
            parent_type_name, &meta->parent_key_path, repository->registry, temporary_allocator,
            &parent_absolute_offset, &parent_size_with_padding);

        if (!child_storage || !parent_field)
        {
            kan_reflection_struct_meta_iterator_next (&iterator);
            meta = (struct kan_repository_meta_automatic_cascade_deletion_t *) kan_reflection_struct_meta_iterator_get (
                &iterator);
            continue;
        }

        struct cascade_deleter_node_t *node = kan_stack_group_allocator_allocate (
            temporary_allocator, sizeof (struct cascade_deleter_node_t), _Alignof (struct cascade_deleter_node_t));

        node->next = first_node;
        first_node = node;
        ++nodes_count;

        kan_repository_indexed_value_delete_query_init (&node->query, (kan_repository_indexed_storage_t) child_storage,
                                                        meta->child_key_path);
        node->absolute_input_value_offset = parent_absolute_offset;

        kan_reflection_struct_meta_iterator_next (&iterator);
        meta = (struct kan_repository_meta_automatic_cascade_deletion_t *) kan_reflection_struct_meta_iterator_get (
            &iterator);
    }

    if (nodes_count == 0u)
    {
        return;
    }

    definition->cascade_deleters_count = nodes_count;
    definition->cascade_deleters = kan_allocate_general (
        result_allocation_group, sizeof (struct cascade_deleter_t) * nodes_count, _Alignof (struct cascade_deleter_t));
    uint64_t index = 0u;

    while (first_node)
    {
        definition->cascade_deleters[index].query = first_node->query;
        definition->cascade_deleters[index].absolute_input_value_offset = first_node->absolute_input_value_offset;
        ++index;
        first_node = first_node->next;
    }
}

static void cascade_deleters_definition_fire (struct cascade_deleters_definition_t *definition,
                                              const void *deleted_record)
{
    for (uint64_t index = 0u; index < definition->cascade_deleters_count; ++index)
    {
        struct cascade_deleter_t *deleter = &definition->cascade_deleters[index];
        const uint8_t *deleted_key = (const uint8_t *) deleted_record + deleter->absolute_input_value_offset;

        struct kan_repository_indexed_value_delete_cursor_t cursor =
            kan_repository_indexed_value_delete_query_execute (&deleter->query, deleted_key);

        struct kan_repository_indexed_value_delete_access_t access =
            kan_repository_indexed_value_delete_cursor_next (&cursor);

        while (kan_repository_indexed_value_delete_access_resolve (&access))
        {
            kan_repository_indexed_value_delete_access_delete (&access);
            access = kan_repository_indexed_value_delete_cursor_next (&cursor);
        }

        kan_repository_indexed_value_delete_cursor_close (&cursor);
    }
}

static void cascade_deleters_definition_shutdown (struct cascade_deleters_definition_t *definition,
                                                  kan_allocation_group_t allocation_group)
{
    if (definition->cascade_deleters)
    {
        kan_free_general (allocation_group, definition->cascade_deleters,
                          sizeof (struct cascade_deleter_t) * definition->cascade_deleters_count);
    }
}

static void singleton_storage_node_shutdown_and_free (struct singleton_storage_node_t *node,
                                                      struct repository_t *repository)
{
    KAN_ASSERT (kan_atomic_int_get (&node->queries_count) == 0)
    KAN_ASSERT (node->singleton)

    if (node->type->shutdown)
    {
        node->type->shutdown (node->type->functor_user_data, node->singleton);
    }

    kan_free_general (node->allocation_group, node->singleton, node->type->size);
    if (node->observation_buffer_memory)
    {
        kan_free_general (node->allocation_group, node->observation_buffer_memory,
                          node->observation_buffer.buffer_size);
        node->observation_buffer_memory = NULL;
    }

    observation_buffer_definition_shutdown (&node->observation_buffer, node->automation_allocation_group);
    observation_event_triggers_definition_shutdown (&node->observation_events_triggers,
                                                    node->automation_allocation_group);

    kan_hash_storage_remove (&repository->singleton_storages, &node->node);
    kan_free_batched (node->allocation_group, node);
}

static void indexed_storage_shutdown_and_free_record_node (struct indexed_storage_node_t *storage,
                                                           struct indexed_storage_record_node_t *record)
{
    if (storage->type->shutdown)
    {
        storage->type->shutdown (storage->type->functor_user_data, record->record);
    }

    kan_free_batched (storage->records_allocation_group, record->record);
    kan_free_batched (storage->nodes_allocation_group, record);
}

static void value_index_shutdown_and_free (struct value_index_t *value_index);

static void indexed_storage_node_shutdown_and_free (struct indexed_storage_node_t *node,
                                                    struct repository_t *repository)
{
    KAN_ASSERT (kan_atomic_int_get (&node->access_status) == 0)
    KAN_ASSERT (kan_atomic_int_get (&node->queries_count) == 0)
    KAN_ASSERT (!node->dirty_records)

    struct value_index_t *value_index = node->first_value_index;
    while (value_index)
    {
        struct value_index_t *next = value_index->next;
        value_index_shutdown_and_free (value_index);
        value_index = next;
    }

    // TODO: Do not forget about indices here.

    struct indexed_storage_record_node_t *record = (struct indexed_storage_record_node_t *) node->records.first;
    while (record)
    {
        struct indexed_storage_record_node_t *next = (struct indexed_storage_record_node_t *) record->list_node.next;
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        KAN_ASSERT (kan_atomic_int_get (&record->safeguard_access_status) == 0)
#endif

        indexed_storage_shutdown_and_free_record_node (node, record);
        record = next;
    }

    kan_stack_group_allocator_shutdown (&node->temporary_allocator);
    observation_buffer_definition_shutdown (&node->observation_buffer, node->automation_allocation_group);
    observation_event_triggers_definition_shutdown (&node->observation_events_triggers,
                                                    node->automation_allocation_group);

    lifetime_event_triggers_definition_shutdown (&node->on_insert_events_triggers, node->automation_allocation_group);
    lifetime_event_triggers_definition_shutdown (&node->on_delete_events_triggers, node->automation_allocation_group);
    cascade_deleters_definition_shutdown (&node->cascade_deleters, node->automation_allocation_group);

    kan_hash_storage_remove (&repository->indexed_storages, &node->node);
    kan_free_batched (node->allocation_group, node);
}

static kan_bool_t value_index_prepare_backed_data (kan_reflection_registry_t registry,
                                                   kan_interned_string_t type_name,
                                                   struct interned_field_path_t path,
                                                   uint32_t *backed_offset_output,
                                                   uint8_t *backed_size_output,
                                                   uint8_t *backed_size_with_padding_output)
{
    uint64_t absolute_offset;
    uint64_t size_with_padding;

    const struct kan_reflection_field_t *field = kan_reflection_registry_query_local_field (
        registry, type_name, path.length, path.path, &absolute_offset, &size_with_padding);

    if (field)
    {
        // Field exists, just re-bake then.
#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
        if (!validation_value_index_is_possible (field))
        {
            return KAN_FALSE;
        }
#endif

        KAN_ASSERT (absolute_offset < UINT32_MAX)
        KAN_ASSERT (field->size < UINT8_MAX)
        KAN_ASSERT (size_with_padding < UINT8_MAX)

        *backed_offset_output = (uint32_t) absolute_offset;
        *backed_size_output = (uint8_t) field->size;
        *backed_size_with_padding_output = (uint8_t) size_with_padding;
        return KAN_TRUE;
    }

    return KAN_FALSE;
}

static struct value_index_node_t *value_index_query_node_from_hash (struct value_index_t *index, uint64_t hash)
{
    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (&index->hash_storage, hash);
    struct value_index_node_t *node = (struct value_index_node_t *) bucket->first;
    struct value_index_node_t *end = (struct value_index_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->node.hash == hash)
        {
            return node;
        }

        node = (struct value_index_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static uint64_t value_index_extract_hash_from_record (struct value_index_t *index, const void *record)
{
    const uint8_t *with_offset = (const uint8_t *) record + index->backed_field_absolute_offset;
    switch (index->backed_field_size)
    {
    case 1u:
        return *with_offset;
    case 2u:
        return *(const uint16_t *) with_offset;
    case 4u:
        return *(const uint32_t *) with_offset;
    case 8u:
        return *(const uint64_t *) with_offset;
    }

    KAN_ASSERT (KAN_FALSE);
    return 0u;
}

static uint64_t value_index_extract_hash_from_buffer (struct value_index_t *index, const void *buffer_memory)
{
    const uint8_t *with_offset = (const uint8_t *) buffer_memory + index->backed_field_offset_in_buffer;
    switch (index->backed_field_size)
    {
    case 1u:
        return *with_offset;
    case 2u:
        return *(const uint16_t *) with_offset;
    case 4u:
        return *(const uint32_t *) with_offset;
    case 8u:
        return *(const uint64_t *) with_offset;
    }

    KAN_ASSERT (KAN_FALSE);
    return 0u;
}

static void value_index_reset_buckets_if_needed (struct value_index_t *index)
{
    if (index->hash_storage.bucket_count * KAN_REPOSITORY_VALUE_INDEX_LOAD_FACTOR <= index->hash_storage.items.size)
    {
        kan_hash_storage_set_bucket_count (&index->hash_storage, index->hash_storage.bucket_count * 2u);
    }

    // TODO: Set smaller bucket count if there is too many buckets?
}

static void value_index_insert_record (struct value_index_t *index, struct indexed_storage_record_node_t *record_node)
{
    kan_allocation_group_t value_index_allocation_group = index->storage->value_index_allocation_group;
    const uint64_t hash = value_index_extract_hash_from_record (index, record_node->record);
    struct value_index_node_t *node = value_index_query_node_from_hash (index, hash);

    if (!node)
    {
        node = (struct value_index_node_t *) kan_allocate_batched (value_index_allocation_group,
                                                                   sizeof (struct value_index_node_t));
        node->node.hash = hash;
        node->first_sub_node = NULL;
        kan_hash_storage_add (&index->hash_storage, &node->node);
    }

    struct value_index_sub_node_t *sub_node = (struct value_index_sub_node_t *) kan_allocate_batched (
        value_index_allocation_group, sizeof (struct value_index_sub_node_t));
    sub_node->next = node->first_sub_node;
    sub_node->previous = NULL;
    sub_node->record = record_node;

    if (node->first_sub_node)
    {
        node->first_sub_node->previous = sub_node;
    }

    node->first_sub_node = sub_node;
}

static void value_index_delete_by_sub_node (struct value_index_t *index,
                                            struct value_index_node_t *node,
                                            struct value_index_sub_node_t *sub_node)
{
    kan_allocation_group_t value_index_allocation_group = index->storage->value_index_allocation_group;
    if (sub_node->next)
    {
        sub_node->next->previous = sub_node->previous;
    }

    if (sub_node->previous)
    {
        sub_node->previous->next = sub_node->next;
    }
    else
    {
        KAN_ASSERT (node->first_sub_node == sub_node)
        node->first_sub_node = sub_node->next;
    }

    kan_free_batched (value_index_allocation_group, sub_node);
    if (!node->first_sub_node)
    {
        kan_hash_storage_remove (&index->hash_storage, &node->node);
        kan_free_batched (value_index_allocation_group, node);
    }
}

static void value_index_delete_by_hash (struct value_index_t *index,
                                        struct indexed_storage_record_node_t *record_node,
                                        uint64_t hash)
{
    struct value_index_node_t *node = value_index_query_node_from_hash (index, hash);
    // Node must be present, otherwise there is an error in repository logic.
    KAN_ASSERT (node)

    struct value_index_sub_node_t *sub_node = node->first_sub_node;
    while (sub_node)
    {
        if (sub_node->record == record_node)
        {
            value_index_delete_by_sub_node (index, node, sub_node);
            return;
        }

        sub_node = sub_node->next;
    }

    // Unable to find sub node, if it hits, then there is something wrong with repository logic.
    KAN_ASSERT (KAN_FALSE)
}

static void value_index_shutdown_and_free (struct value_index_t *value_index)
{
    KAN_ASSERT (kan_atomic_int_get (&value_index->queries_count) == 0)
    kan_allocation_group_t value_index_allocation_group = value_index->storage->value_index_allocation_group;
    struct value_index_node_t *index_node = (struct value_index_node_t *) value_index->hash_storage.items.first;

    while (index_node)
    {
        struct value_index_node_t *next_node = (struct value_index_node_t *) index_node->node.list_node.next;
        struct value_index_sub_node_t *sub_node = next_node->first_sub_node;

        while (sub_node)
        {
            struct value_index_sub_node_t *next_sub_node = sub_node->next;
            kan_free_batched (value_index_allocation_group, sub_node);
            sub_node = next_sub_node;
        }

        kan_free_batched (value_index_allocation_group, index_node);
        index_node = next_node;
    }

    kan_hash_storage_shutdown (&value_index->hash_storage);
    shutdown_field_path (value_index->source_path, value_index->storage->value_index_allocation_group);
    kan_free_batched (value_index_allocation_group, value_index);
}

static void event_queue_node_shutdown_and_free (struct event_queue_node_t *node, struct event_storage_node_t *storage)
{
    KAN_ASSERT (node->event || &node->node == storage->event_queue.next_placeholder)
    if (node->event)
    {
        if (storage->type->shutdown)
        {
            storage->type->shutdown (storage->type->functor_user_data, node->event);
        }

        kan_free_batched (storage->allocation_group, node->event);
    }

    kan_free_batched (storage->allocation_group, node);
}

static void event_storage_node_shutdown_and_free (struct event_storage_node_t *node, struct repository_t *repository)
{
    KAN_ASSERT (kan_atomic_int_get (&node->queries_count) == 0)
    struct event_queue_node_t *queue_node = (struct event_queue_node_t *) node->event_queue.oldest;

    while (queue_node)
    {
        struct event_queue_node_t *next = (struct event_queue_node_t *) queue_node->node.next;
        event_queue_node_shutdown_and_free (queue_node, node);
        queue_node = next;
    }

    kan_hash_storage_remove (&repository->event_storages, &node->node);
    kan_free_batched (node->allocation_group, node);
}

static void repository_storage_map_access_lock (struct repository_t *caller)
{
    kan_atomic_int_lock (caller->parent ? caller->shared_storage_access_lock_pointer :
                                          &caller->shared_storage_access_lock);
}

static void repository_storage_map_access_unlock (struct repository_t *caller)
{
    kan_atomic_int_unlock (caller->parent ? caller->shared_storage_access_lock_pointer :
                                            &caller->shared_storage_access_lock);
}

static struct repository_t *repository_create (kan_allocation_group_t allocation_group,
                                               struct repository_t *parent,
                                               kan_interned_string_t name,
                                               kan_reflection_registry_t registry)
{
    struct repository_t *repository = (struct repository_t *) kan_allocate_general (
        allocation_group, sizeof (struct repository_t), _Alignof (struct repository_t));
    repository->parent = parent;
    repository->first = NULL;

    if (parent)
    {
        repository->next = parent->first;
        parent->first = repository;

        if (parent->parent)
        {
            repository->shared_storage_access_lock_pointer = parent->shared_storage_access_lock_pointer;
        }
        else
        {
            repository->shared_storage_access_lock_pointer = &parent->shared_storage_access_lock;
        }
    }
    else
    {
        repository->next = NULL;
        repository->shared_storage_access_lock = kan_atomic_int_init (0);
    }

    repository->name = name;
    repository->registry = registry;
    repository->allocation_group = allocation_group;
    repository->mode = REPOSITORY_MODE_PLANNING;

    kan_hash_storage_init (&repository->singleton_storages, repository->allocation_group,
                           KAN_REPOSITORY_SINGLETON_STORAGE_INITIAL_BUCKETS);

    kan_hash_storage_init (&repository->event_storages, repository->allocation_group,
                           KAN_REPOSITORY_EVENT_STORAGE_INITIAL_BUCKETS);
    return repository;
}

kan_repository_t kan_repository_create_root (kan_allocation_group_t allocation_group,
                                             kan_reflection_registry_t registry)
{
    return (kan_repository_t) repository_create (allocation_group, NULL, kan_string_intern ("root"), registry);
}

kan_repository_t kan_repository_create_child (kan_repository_t parent, const char *name)
{
    struct repository_t *parent_repository = (struct repository_t *) parent;

    const kan_allocation_group_t allocation_group = kan_allocation_group_get_child (
        kan_allocation_group_get_child (parent_repository->allocation_group, "child_repositories"), name);

    const kan_interned_string_t interned_name = kan_string_intern (name);
    return (kan_repository_t) repository_create (allocation_group, parent_repository, interned_name,
                                                 parent_repository->registry);
}

static void repository_enter_planning_mode_internal (struct repository_t *repository)
{
    KAN_ASSERT (repository->mode == REPOSITORY_MODE_SERVING)
    repository->mode = REPOSITORY_MODE_PLANNING;

    struct singleton_storage_node_t *singleton_storage_node =
        (struct singleton_storage_node_t *) repository->singleton_storages.items.first;

    while (singleton_storage_node)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (kan_atomic_int_get (&singleton_storage_node->safeguard_access_status) != 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Unsafe switch to planning mode. Singleton \"%s\" is still accessed.",
                     singleton_storage_node->type->name);
        }
#endif

        if (singleton_storage_node->observation_buffer_memory)
        {
            kan_free_general (singleton_storage_node->automation_allocation_group,
                              singleton_storage_node->observation_buffer_memory,
                              singleton_storage_node->observation_buffer.buffer_size);
            singleton_storage_node->observation_buffer_memory = NULL;
        }

        observation_buffer_definition_shutdown (&singleton_storage_node->observation_buffer,
                                                singleton_storage_node->automation_allocation_group);
        observation_buffer_definition_init (&singleton_storage_node->observation_buffer);

        observation_event_triggers_definition_shutdown (&singleton_storage_node->observation_events_triggers,
                                                        singleton_storage_node->automation_allocation_group);
        observation_event_triggers_definition_init (&singleton_storage_node->observation_events_triggers);

        singleton_storage_node = (struct singleton_storage_node_t *) singleton_storage_node->node.list_node.next;
    }

    struct indexed_storage_node_t *indexed_storage_node =
        (struct indexed_storage_node_t *) repository->indexed_storages.items.first;

    while (indexed_storage_node)
    {
        observation_buffer_definition_shutdown (&indexed_storage_node->observation_buffer,
                                                indexed_storage_node->automation_allocation_group);
        observation_buffer_definition_init (&indexed_storage_node->observation_buffer);

        observation_event_triggers_definition_shutdown (&indexed_storage_node->observation_events_triggers,
                                                        indexed_storage_node->automation_allocation_group);
        observation_event_triggers_definition_init (&indexed_storage_node->observation_events_triggers);

        lifetime_event_triggers_definition_shutdown (&indexed_storage_node->on_insert_events_triggers,
                                                     indexed_storage_node->automation_allocation_group);
        lifetime_event_triggers_definition_init (&indexed_storage_node->on_insert_events_triggers);

        lifetime_event_triggers_definition_shutdown (&indexed_storage_node->on_delete_events_triggers,
                                                     indexed_storage_node->automation_allocation_group);
        lifetime_event_triggers_definition_init (&indexed_storage_node->on_delete_events_triggers);

        cascade_deleters_definition_shutdown (&indexed_storage_node->cascade_deleters,
                                              indexed_storage_node->automation_allocation_group);
        cascade_deleters_definition_init (&indexed_storage_node->cascade_deleters);

        indexed_storage_node = (struct indexed_storage_node_t *) indexed_storage_node->node.list_node.next;
    }

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    struct event_storage_node_t *event_storage_node =
        (struct event_storage_node_t *) repository->event_storages.items.first;

    while (event_storage_node)
    {
        if (kan_atomic_int_get (&event_storage_node->safeguard_access_status) != 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Unsafe switch to planning mode. Events \"%s\" are still accessed.",
                     singleton_storage_node->type->name);
        }

        event_storage_node = (struct event_storage_node_t *) event_storage_node->node.list_node.next;
    }
#endif

    repository = repository->first;
    while (repository)
    {
        repository_enter_planning_mode_internal (repository);
        repository = repository->next;
    }
}

void kan_repository_enter_planning_mode (kan_repository_t root_repository)
{
    struct repository_t *repository = (struct repository_t *) root_repository;
    KAN_ASSERT (!repository->parent)
    repository_enter_planning_mode_internal (repository);
}

static void execute_migration (uint64_t user_data)
{
    // We can migrate several records in one task if instance-per-task model is too slow.
    struct record_migration_user_data_t *data = (struct record_migration_user_data_t *) user_data;
    void *old_object = *data->record_pointer;

    void *new_object = data->batched_allocation ? kan_allocate_batched (data->allocation_group, data->new_type->size) :
                                                  kan_allocate_general (data->allocation_group, data->new_type->size,
                                                                        data->new_type->alignment);

    if (data->new_type->init)
    {
        data->new_type->init (data->new_type->functor_user_data, new_object);
    }

    kan_reflection_struct_migrator_migrate_instance (data->migrator, data->new_type->name, old_object, new_object);
    if (data->old_type->shutdown)
    {
        data->old_type->shutdown (data->old_type->functor_user_data, old_object);
    }

    if (data->batched_allocation)
    {
        kan_free_batched (data->allocation_group, old_object);
    }
    else
    {
        kan_free_general (data->allocation_group, old_object, data->old_type->size);
    }

    *data->record_pointer = new_object;
}

static struct record_migration_user_data_t *allocate_migration_user_data (struct migration_context_t *context)
{
    return (struct record_migration_user_data_t *) kan_stack_group_allocator_allocate (
        &context->allocator, sizeof (struct record_migration_user_data_t),
        _Alignof (struct record_migration_user_data_t));
}

static void spawn_migration_task (struct migration_context_t *context, struct record_migration_user_data_t *user_data)
{
    struct kan_cpu_task_list_node_t *node = (struct kan_cpu_task_list_node_t *) kan_stack_group_allocator_allocate (
        &context->allocator, sizeof (struct kan_cpu_task_list_node_t), _Alignof (struct kan_cpu_task_list_node_t));

    node->queue = KAN_CPU_DISPATCH_QUEUE_FOREGROUND;
    ensure_interned_strings_ready ();

    node->task = (struct kan_cpu_task_t) {
        .name = migration_task_name,
        .function = execute_migration,
        .user_data = (uint64_t) user_data,
    };

    node->next = context->task_list;
    context->task_list = node;
}

static void repository_migrate_internal (struct repository_t *repository,
                                         struct migration_context_t *context,
                                         kan_reflection_registry_t new_registry,
                                         kan_reflection_migration_seed_t migration_seed,
                                         kan_reflection_struct_migrator_t migrator)
{
    KAN_ASSERT (repository->mode == REPOSITORY_MODE_PLANNING)
    struct singleton_storage_node_t *singleton_storage_node =
        (struct singleton_storage_node_t *) repository->singleton_storages.items.first;

    while (singleton_storage_node)
    {
        struct singleton_storage_node_t *next =
            (struct singleton_storage_node_t *) singleton_storage_node->node.list_node.next;

        const struct kan_reflection_struct_t *old_type = singleton_storage_node->type;
        const struct kan_reflection_struct_t *new_type =
            kan_reflection_registry_query_struct (new_registry, old_type->name);
        singleton_storage_node->type = new_type;

        const struct kan_reflection_struct_migration_seed_t *seed =
            kan_reflection_migration_seed_get_for_struct (migration_seed, old_type->name);

        switch (seed->status)
        {
        case KAN_REFLECTION_MIGRATION_NEEDED:
        {
            struct record_migration_user_data_t *user_data = allocate_migration_user_data (context);
            *user_data = (struct record_migration_user_data_t) {
                .record_pointer = &singleton_storage_node->singleton,
                .allocation_group = singleton_storage_node->allocation_group,
                .batched_allocation = KAN_FALSE,
                .migrator = migrator,
                .old_type = old_type,
                .new_type = new_type,
            };

            spawn_migration_task (context, user_data);
            break;
        }

        case KAN_REFLECTION_MIGRATION_NOT_NEEDED:
            // No migration, therefore don't need to do anything.
            break;

        case KAN_REFLECTION_MIGRATION_REMOVED:
            singleton_storage_node_shutdown_and_free (singleton_storage_node, repository);
            break;
        }

        singleton_storage_node = next;
    }

    struct indexed_storage_node_t *indexed_storage_node =
        (struct indexed_storage_node_t *) repository->indexed_storages.items.first;

    while (indexed_storage_node)
    {
        struct indexed_storage_node_t *next =
            (struct indexed_storage_node_t *) indexed_storage_node->node.list_node.next;

        const struct kan_reflection_struct_t *old_type = indexed_storage_node->type;
        const struct kan_reflection_struct_t *new_type =
            kan_reflection_registry_query_struct (new_registry, old_type->name);
        indexed_storage_node->type = new_type;

        const struct kan_reflection_struct_migration_seed_t *seed =
            kan_reflection_migration_seed_get_for_struct (migration_seed, old_type->name);

        switch (seed->status)
        {
        case KAN_REFLECTION_MIGRATION_NEEDED:
        {
            struct value_index_t *value_index = indexed_storage_node->first_value_index;
            struct value_index_t *previous_value_index = NULL;

            while (value_index)
            {
                struct value_index_t *next_value_index = value_index->next;
                if (!value_index_prepare_backed_data (new_registry, old_type->name, value_index->source_path,
                                                      &value_index->backed_field_absolute_offset,
                                                      &value_index->backed_field_size,
                                                      &value_index->backed_field_size_with_padding))
                {
                    // Baking failed -- it means that provided path is no longer valid. Therefore, drop index.
                    value_index_shutdown_and_free (value_index);

                    if (previous_value_index)
                    {
                        previous_value_index->next = next_value_index;
                    }
                    else
                    {
                        indexed_storage_node->first_value_index = next_value_index;
                    }
                }
                else
                {
                    previous_value_index = value_index;
                }

                value_index = next_value_index;
            }

            // TODO: Drop indices if their fields are deleted.
            // TODO: Schedule index reflection data re-bake if their fields are affected. Or re-bake it here.

            struct indexed_storage_record_node_t *node =
                (struct indexed_storage_record_node_t *) indexed_storage_node->records.first;

            while (node)
            {
                struct record_migration_user_data_t *user_data = allocate_migration_user_data (context);
                *user_data = (struct record_migration_user_data_t) {
                    .record_pointer = &node->record,
                    .allocation_group = indexed_storage_node->allocation_group,
                    .batched_allocation = KAN_TRUE,
                    .migrator = migrator,
                    .old_type = old_type,
                    .new_type = new_type,
                };

                spawn_migration_task (context, user_data);
                node = (struct indexed_storage_record_node_t *) node->list_node.next;
            }

            break;
        }

        case KAN_REFLECTION_MIGRATION_NOT_NEEDED:
            // No migration, therefore don't need to do anything.
            break;

        case KAN_REFLECTION_MIGRATION_REMOVED:
            indexed_storage_node_shutdown_and_free (indexed_storage_node, repository);
            break;
        }

        indexed_storage_node = next;
    }

    struct event_storage_node_t *event_storage_node =
        (struct event_storage_node_t *) repository->event_storages.items.first;

    while (event_storage_node)
    {
        struct event_storage_node_t *next = (struct event_storage_node_t *) event_storage_node->node.list_node.next;

        const struct kan_reflection_struct_t *old_type = event_storage_node->type;
        const struct kan_reflection_struct_t *new_type =
            kan_reflection_registry_query_struct (new_registry, old_type->name);
        event_storage_node->type = new_type;

        const struct kan_reflection_struct_migration_seed_t *seed =
            kan_reflection_migration_seed_get_for_struct (migration_seed, old_type->name);

        switch (seed->status)
        {
        case KAN_REFLECTION_MIGRATION_NEEDED:
        {
            struct event_queue_node_t *node = (struct event_queue_node_t *) event_storage_node->event_queue.oldest;
            while (&node->node != event_storage_node->event_queue.next_placeholder)
            {
                struct record_migration_user_data_t *user_data = allocate_migration_user_data (context);
                *user_data = (struct record_migration_user_data_t) {
                    .record_pointer = &node->event,
                    .allocation_group = event_storage_node->allocation_group,
                    .batched_allocation = KAN_TRUE,
                    .migrator = migrator,
                    .old_type = old_type,
                    .new_type = new_type,
                };

                spawn_migration_task (context, user_data);
                node = (struct event_queue_node_t *) node->node.next;
            }

            break;
        }

        case KAN_REFLECTION_MIGRATION_NOT_NEEDED:
            // No migration, therefore don't need to do anything.
            break;

        case KAN_REFLECTION_MIGRATION_REMOVED:
            event_storage_node_shutdown_and_free (event_storage_node, repository);
            break;
        }

        event_storage_node = next;
    }

    repository->registry = new_registry;
    repository = repository->first;

    while (repository)
    {
        repository_migrate_internal (repository, context, new_registry, migration_seed, migrator);
        repository = repository->next;
    }
}

void kan_repository_migrate (kan_repository_t root_repository,
                             kan_reflection_registry_t new_registry,
                             kan_reflection_migration_seed_t migration_seed,
                             kan_reflection_struct_migrator_t migrator)
{
    struct repository_t *repository = (struct repository_t *) root_repository;
    KAN_ASSERT (!repository->parent)
    kan_cpu_job_t job = kan_cpu_job_create ();
    KAN_ASSERT (job != KAN_INVALID_CPU_JOB)

    struct migration_context_t context;
    kan_stack_group_allocator_init (&context.allocator,
                                    kan_allocation_group_get_child (repository->allocation_group, "migration"),
                                    KAN_REPOSITORY_MIGRATION_STACK_INITIAL_SIZE);

    context.task_list = NULL;
    repository_migrate_internal (repository, &context, new_registry, migration_seed, migrator);

    kan_cpu_job_dispatch_and_detach_task_list (job, context.task_list);
    kan_cpu_job_release (job);
    kan_cpu_job_wait (job);
    kan_stack_group_allocator_shutdown (&context.allocator);
}

static struct singleton_storage_node_t *query_singleton_storage_across_hierarchy (struct repository_t *repository,
                                                                                  kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&repository->singleton_storages, (uint64_t) type_name);
    struct singleton_storage_node_t *node = (struct singleton_storage_node_t *) bucket->first;
    const struct singleton_storage_node_t *end =
        (struct singleton_storage_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->type->name == type_name)
        {
            return node;
        }

        node = (struct singleton_storage_node_t *) node->node.list_node.next;
    }

    return repository->parent ? query_singleton_storage_across_hierarchy (repository->parent, type_name) : NULL;
}

kan_repository_singleton_storage_t kan_repository_singleton_storage_open (kan_repository_t repository,
                                                                          const char *type_name)
{
    struct repository_t *repository_data = (struct repository_t *) repository;
    KAN_ASSERT (repository_data->mode == REPOSITORY_MODE_PLANNING)
    repository_storage_map_access_lock (repository_data);
    const kan_interned_string_t interned_type_name = kan_string_intern (type_name);
    struct singleton_storage_node_t *storage =
        query_singleton_storage_across_hierarchy (repository_data, interned_type_name);

    if (!storage)
    {
        const struct kan_reflection_struct_t *singleton_type =
            kan_reflection_registry_query_struct (repository_data->registry, interned_type_name);

        if (!singleton_type)
        {
            KAN_LOG (repository, KAN_LOG_ERROR, "Singleton type \"%s\" not found.", interned_type_name)
            repository_storage_map_access_unlock (repository_data);
            return KAN_INVALID_REPOSITORY_SINGLETON_STORAGE;
        }

        const kan_allocation_group_t storage_allocation_group = kan_allocation_group_get_child (
            kan_allocation_group_get_child (repository_data->allocation_group, "singletons"), interned_type_name);

        storage = (struct singleton_storage_node_t *) kan_allocate_batched (storage_allocation_group,
                                                                            sizeof (struct singleton_storage_node_t));

        storage->node.hash = (uint64_t) interned_type_name;
        storage->type = singleton_type;
        storage->queries_count = kan_atomic_int_init (0);

        storage->singleton =
            kan_allocate_general (storage_allocation_group, storage->type->size, storage->type->alignment);

        if (storage->type->init)
        {
            storage->type->init (storage->type->functor_user_data, storage->singleton);
        }

        observation_buffer_definition_init (&storage->observation_buffer);
        storage->observation_buffer_memory = NULL;
        observation_event_triggers_definition_init (&storage->observation_events_triggers);

        storage->allocation_group = storage_allocation_group;
        storage->automation_allocation_group = kan_allocation_group_get_child (storage_allocation_group, "automation");

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        storage->safeguard_access_status = kan_atomic_int_init (0);
#endif

        if (repository_data->singleton_storages.bucket_count * KAN_REPOSITORY_SINGLETON_STORAGE_LOAD_FACTOR <=
            repository_data->singleton_storages.items.size)
        {
            kan_hash_storage_set_bucket_count (&repository_data->singleton_storages,
                                               repository_data->singleton_storages.bucket_count * 2u);
        }

        kan_hash_storage_add (&repository_data->singleton_storages, &storage->node);
    }

    repository_storage_map_access_unlock (repository_data);
    return (kan_repository_singleton_storage_t) storage;
}

void kan_repository_singleton_read_query_init (struct kan_repository_singleton_read_query_t *query,
                                               kan_repository_singleton_storage_t storage)
{
    struct singleton_storage_node_t *storage_data = (struct singleton_storage_node_t *) storage;
    kan_atomic_int_add (&storage_data->queries_count, 1);
    *(struct singleton_query_t *) query = (struct singleton_query_t) {.storage = storage_data};
}

kan_repository_singleton_read_access_t kan_repository_singleton_read_query_execute (
    struct kan_repository_singleton_read_query_t *query)
{
    struct singleton_query_t *query_data = (struct singleton_query_t *) query;
    KAN_ASSERT (query_data->storage)

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    if (!safeguard_singleton_read_access_try_create (query_data->storage))
    {
        return (kan_repository_singleton_read_access_t) NULL;
    }
#endif

    return (kan_repository_singleton_read_access_t) query_data->storage;
}

const void *kan_repository_singleton_read_access_resolve (kan_repository_singleton_read_access_t access)
{
    struct singleton_storage_node_t *storage = (struct singleton_storage_node_t *) access;
    return storage ? storage->singleton : NULL;
}

void kan_repository_singleton_read_access_close (kan_repository_singleton_read_access_t access)
{
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    struct singleton_storage_node_t *storage = (struct singleton_storage_node_t *) access;
    if (storage)
    {
        safeguard_singleton_read_access_destroyed (storage);
    }
#endif
}

void kan_repository_singleton_read_query_shutdown (struct kan_repository_singleton_read_query_t *query)
{
    struct singleton_query_t *query_data = (struct singleton_query_t *) query;
    if (query_data->storage)
    {
        kan_atomic_int_add (&query_data->storage->queries_count, -1);
        query_data->storage = NULL;
    }
}

void kan_repository_singleton_write_query_init (struct kan_repository_singleton_write_query_t *query,
                                                kan_repository_singleton_storage_t storage)
{
    struct singleton_storage_node_t *storage_data = (struct singleton_storage_node_t *) storage;
    kan_atomic_int_add (&storage_data->queries_count, 1);
    *(struct singleton_query_t *) query = (struct singleton_query_t) {.storage = storage_data};
}

kan_repository_singleton_write_access_t kan_repository_singleton_write_query_execute (
    struct kan_repository_singleton_write_query_t *query)
{
    struct singleton_query_t *query_data = (struct singleton_query_t *) query;
    KAN_ASSERT (query_data->storage)

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    if (!safeguard_singleton_write_access_try_create (query_data->storage))
    {
        return (kan_repository_singleton_write_access_t) NULL;
    }
#endif

    observation_buffer_definition_import (&query_data->storage->observation_buffer,
                                          query_data->storage->observation_buffer_memory,
                                          query_data->storage->singleton);
    return (kan_repository_singleton_write_access_t) query_data->storage;
}

void *kan_repository_singleton_write_access_resolve (kan_repository_singleton_write_access_t access)
{
    struct singleton_storage_node_t *storage = (struct singleton_storage_node_t *) access;
    return storage ? storage->singleton : NULL;
}

void kan_repository_singleton_write_access_close (kan_repository_singleton_write_access_t access)
{
    struct singleton_storage_node_t *storage = (struct singleton_storage_node_t *) access;
    if (storage)
    {
        const uint64_t change_flags = observation_buffer_definition_compare (
            &storage->observation_buffer, storage->observation_buffer_memory, storage->singleton);

        if (change_flags != 0u)
        {
            observation_event_triggers_definition_fire (&storage->observation_events_triggers, change_flags,
                                                        &storage->observation_buffer,
                                                        storage->observation_buffer_memory, storage->singleton);
        }

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_singleton_write_access_destroyed (storage);
#endif
    }
}

void kan_repository_singleton_write_query_shutdown (struct kan_repository_singleton_write_query_t *query)
{
    struct singleton_query_t *query_data = (struct singleton_query_t *) query;
    if (query_data->storage)
    {
        kan_atomic_int_add (&query_data->storage->queries_count, -1);
        query_data->storage = NULL;
    }
}

static struct indexed_storage_node_t *query_indexed_storage_across_hierarchy (struct repository_t *repository,
                                                                              kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&repository->indexed_storages, (uint64_t) type_name);
    struct indexed_storage_node_t *node = (struct indexed_storage_node_t *) bucket->first;
    const struct indexed_storage_node_t *end =
        (struct indexed_storage_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->type->name == type_name)
        {
            return node;
        }

        node = (struct indexed_storage_node_t *) node->node.list_node.next;
    }

    return repository->parent ? query_indexed_storage_across_hierarchy (repository->parent, type_name) : NULL;
}

kan_repository_indexed_storage_t kan_repository_indexed_storage_open (kan_repository_t repository,
                                                                      kan_interned_string_t type_name)
{
    struct repository_t *repository_data = (struct repository_t *) repository;
    KAN_ASSERT (repository_data->mode == REPOSITORY_MODE_PLANNING)
    repository_storage_map_access_lock (repository_data);
    const kan_interned_string_t interned_type_name = kan_string_intern (type_name);
    struct indexed_storage_node_t *storage =
        query_indexed_storage_across_hierarchy (repository_data, interned_type_name);

    if (!storage)
    {
        const struct kan_reflection_struct_t *indexed_type =
            kan_reflection_registry_query_struct (repository_data->registry, interned_type_name);

        if (!indexed_type)
        {
            KAN_LOG (repository, KAN_LOG_ERROR, "Indexed record type \"%s\" not found.", interned_type_name)
            repository_storage_map_access_unlock (repository_data);
            return KAN_INVALID_REPOSITORY_INDEXED_STORAGE;
        }

        const kan_allocation_group_t storage_allocation_group = kan_allocation_group_get_child (
            kan_allocation_group_get_child (repository_data->allocation_group, "indexed"), interned_type_name);

        storage = (struct indexed_storage_node_t *) kan_allocate_batched (storage_allocation_group,
                                                                          sizeof (struct indexed_storage_node_t));

        storage->node.hash = (uint64_t) interned_type_name;
        storage->repository = repository_data;
        storage->type = indexed_type;

        kan_bd_list_init (&storage->records);
        storage->access_status = kan_atomic_int_init (0);
        storage->maintenance_lock = kan_atomic_int_init (0);
        storage->dirty_records = NULL;
        kan_stack_group_allocator_init (&storage->temporary_allocator,
                                        kan_allocation_group_get_child (storage_allocation_group, "temporary"),
                                        KAN_REPOSITORY_INDEX_STORAGE_STACK_INITIAL_SIZE);

        observation_buffer_definition_init (&storage->observation_buffer);
        observation_event_triggers_definition_init (&storage->observation_events_triggers);
        lifetime_event_triggers_definition_init (&storage->on_insert_events_triggers);
        lifetime_event_triggers_definition_init (&storage->on_delete_events_triggers);
        cascade_deleters_definition_init (&storage->cascade_deleters);

        storage->first_value_index = NULL;
        storage->first_signal_index = NULL;
        storage->first_interval_index = NULL;
        storage->first_space_index = NULL;

        storage->allocation_group = storage_allocation_group;
        storage->records_allocation_group = kan_allocation_group_get_child (storage_allocation_group, "records");
        storage->nodes_allocation_group = kan_allocation_group_get_child (storage_allocation_group, "nodes");
        storage->automation_allocation_group = kan_allocation_group_get_child (storage_allocation_group, "automation");

        kan_allocation_group_t indices_group = kan_allocation_group_get_child (storage_allocation_group, "indices");
        storage->value_index_allocation_group = kan_allocation_group_get_child (indices_group, "value");
        storage->signal_index_allocation_group = kan_allocation_group_get_child (indices_group, "signal");
        storage->interval_index_allocation_group = kan_allocation_group_get_child (indices_group, "interval");
        storage->space_index_allocation_group = kan_allocation_group_get_child (indices_group, "space");

        if (repository_data->indexed_storages.bucket_count * KAN_REPOSITORY_EVENT_STORAGE_LOAD_FACTOR <=
            repository_data->indexed_storages.items.size)
        {
            kan_hash_storage_set_bucket_count (&repository_data->indexed_storages,
                                               repository_data->indexed_storages.bucket_count * 2u);
        }

        kan_hash_storage_add (&repository_data->indexed_storages, &storage->node);
    }

    repository_storage_map_access_unlock (repository_data);
    return (kan_repository_indexed_storage_t) storage;
}

static void indexed_storage_acquire_access (struct indexed_storage_node_t *storage)
{
    while (KAN_TRUE)
    {
        int old_status = kan_atomic_int_get (&storage->access_status);
        if (old_status < 0)
        {
            // We're on maintenance, wait until it ends.
            kan_atomic_int_lock (&storage->maintenance_lock);
            kan_atomic_int_unlock (&storage->maintenance_lock);
            continue;
        }

        int new_status = old_status + 1;
        if (kan_atomic_int_compare_and_set (&storage->access_status, old_status, new_status))
        {
            break;
        }
    }
}

static void indexed_storage_perform_maintenance (struct indexed_storage_node_t *storage)
{
    while (storage->dirty_records)
    {
        struct indexed_storage_record_node_t *node = storage->dirty_records->source_node;
        switch (storage->dirty_records->type)
        {
        case INDEXED_STORAGE_DIRTY_RECORD_CHANGED:
        {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
            // Safeguards for write access are destroyed only after maintenance
            // as technically access persists till maintenance is finished.
            safeguard_indexed_write_access_destroyed (storage->dirty_records->source_node);
#endif

            if (storage->dirty_records->observation_comparison_flags)
            {
                struct value_index_t *value_index = storage->first_value_index;
                while (value_index)
                {
                    if (value_index->observation_flags & storage->dirty_records->observation_comparison_flags)
                    {
                        if (value_index == storage->dirty_records->dirt_source_index)
                        {
                            value_index_delete_by_sub_node (
                                value_index,
                                (struct value_index_node_t *) storage->dirty_records->dirt_source_index_node,
                                (struct value_index_sub_node_t *) storage->dirty_records->dirt_source_index_sub_node);
                        }
                        else
                        {
                            const uint64_t old_hash = value_index_extract_hash_from_buffer (
                                value_index, storage->dirty_records->observation_buffer_memory);
                            value_index_delete_by_hash (value_index, node, old_hash);
                        }

                        value_index_insert_record (value_index, node);
                    }

                    value_index = value_index->next;
                }

                // TODO: Update indices.

                observation_event_triggers_definition_fire (
                    &storage->observation_events_triggers, storage->dirty_records->observation_comparison_flags,
                    &storage->observation_buffer, storage->dirty_records->observation_buffer_memory, node->record);
            }

            break;
        }

        case INDEXED_STORAGE_DIRTY_RECORD_INSERTED:
        {
            kan_bd_list_add (&storage->records, NULL, &node->list_node);
            struct value_index_t *value_index = storage->first_value_index;

            while (value_index)
            {
                value_index_insert_record (value_index, node);
                value_index = value_index->next;
            }

            // TODO: Add to indices.

            lifetime_event_triggers_definition_fire (&storage->on_insert_events_triggers, node->record);
            break;
        }

        case INDEXED_STORAGE_DIRTY_RECORD_DELETED:
        {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
            // Safeguards for write access are destroyed only after maintenance
            // as technically access persists till maintenance is finished.
            safeguard_indexed_write_access_destroyed (storage->dirty_records->source_node);
#endif

            if (storage->dirty_records->observation_comparison_flags)
            {
                // If something was changed, we need to still fire events.
                // Imagine situation: asset references were changed and then record was deleted. If we only report
                // deletion, it would be reported with incorrect asset references (that were never added for this record
                // previously). But if we send change event too, then asset manager would be able to properly process
                // both asset references change and deletion.
                observation_event_triggers_definition_fire (
                    &storage->observation_events_triggers, storage->dirty_records->observation_comparison_flags,
                    &storage->observation_buffer, storage->dirty_records->observation_buffer_memory, node->record);
            }

            lifetime_event_triggers_definition_fire (&storage->on_delete_events_triggers, node->record);
            struct value_index_t *value_index = storage->first_value_index;

            while (value_index)
            {
                if (value_index == storage->dirty_records->dirt_source_index)
                {
                    value_index_delete_by_sub_node (
                        value_index, (struct value_index_node_t *) storage->dirty_records->dirt_source_index_node,
                        (struct value_index_sub_node_t *) storage->dirty_records->dirt_source_index_sub_node);
                }
                else
                {
                    const uint64_t old_hash = storage->dirty_records->observation_buffer_memory ?
                                                  value_index_extract_hash_from_buffer (
                                                      value_index, storage->dirty_records->observation_buffer_memory) :
                                                  value_index_extract_hash_from_record (value_index, node->record);
                    value_index_delete_by_hash (value_index, node, old_hash);
                }

                value_index = value_index->next;
            }

            // TODO: Remove from indices.

            kan_bd_list_remove (&storage->records, &node->list_node);
            indexed_storage_shutdown_and_free_record_node (storage, node);
            break;
        }
        }

        storage->dirty_records = storage->dirty_records->next;
    }

    struct value_index_t *value_index = storage->first_value_index;
    while (value_index)
    {
        value_index_reset_buckets_if_needed (value_index);
        value_index = value_index->next;
    }

    kan_stack_group_allocator_shrink (&storage->temporary_allocator);
    kan_stack_group_allocator_reset (&storage->temporary_allocator);
}

static void indexed_storage_release_access (struct indexed_storage_node_t *storage)
{
    while (KAN_TRUE)
    {
        int old_status = kan_atomic_int_get (&storage->access_status);
        kan_bool_t start_maintenance = KAN_FALSE;
        int new_status;

        if (old_status == 1)
        {
            new_status = -1;
            start_maintenance = KAN_TRUE;
            kan_atomic_int_lock (&storage->maintenance_lock);
        }
        else
        {
            new_status = old_status - 1;
        }

        if (kan_atomic_int_compare_and_set (&storage->access_status, old_status, new_status))
        {
            if (start_maintenance)
            {
                indexed_storage_perform_maintenance (storage);
                kan_atomic_int_set (&storage->access_status, 0);
                kan_atomic_int_unlock (&storage->maintenance_lock);
            }

            break;
        }
        else if (start_maintenance)
        {
            kan_atomic_int_unlock (&storage->maintenance_lock);
        }
    }
}

static struct indexed_storage_dirty_record_node_t *indexed_storage_allocate_dirty_record (
    struct indexed_storage_node_t *storage, kan_bool_t with_observation_buffer_memory)
{
    KAN_ASSERT (kan_atomic_int_get (&storage->access_status) > 0)
    // When maintenance is not possible, maintenance lock is used to restrict dirty record creation.
    kan_atomic_int_lock (&storage->maintenance_lock);

    struct indexed_storage_dirty_record_node_t *node = kan_stack_group_allocator_allocate (
        &storage->temporary_allocator, sizeof (struct indexed_storage_dirty_record_node_t),
        _Alignof (struct indexed_storage_dirty_record_node_t));

    node->next = storage->dirty_records;
    storage->dirty_records = node;

    if (with_observation_buffer_memory && storage->observation_buffer.buffer_size > 0u)
    {
        node->observation_buffer_memory = kan_stack_group_allocator_allocate (
            &storage->temporary_allocator, storage->observation_buffer.buffer_size, OBSERVATION_BUFFER_CHUNK_ALIGNMENT);
    }

    kan_atomic_int_unlock (&storage->maintenance_lock);
    return node;
}

static void indexed_storage_report_insertion (struct indexed_storage_node_t *storage, void *inserted_record)
{
    struct indexed_storage_dirty_record_node_t *record = indexed_storage_allocate_dirty_record (storage, KAN_FALSE);
    record->source_node =
        kan_allocate_batched (storage->nodes_allocation_group, sizeof (struct indexed_storage_record_node_t));
    record->source_node->record = inserted_record;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    record->source_node->safeguard_access_status = kan_atomic_int_init (0);
#endif

    record->type = INDEXED_STORAGE_DIRTY_RECORD_INSERTED;
}

static struct indexed_storage_dirty_record_node_t *indexed_storage_report_mutable_access_begin (
    struct indexed_storage_node_t *storage,
    struct indexed_storage_record_node_t *node,
    void *from_index,
    void *from_index_node,
    void *from_index_sub_node)
{
    struct indexed_storage_dirty_record_node_t *record = indexed_storage_allocate_dirty_record (storage, KAN_TRUE);
    record->source_node = node;

    observation_buffer_definition_import (&storage->observation_buffer, record->observation_buffer_memory,
                                          node->record);

    record->dirt_source_index = from_index;
    record->dirt_source_index_node = from_index_node;
    record->dirt_source_index_sub_node = from_index_sub_node;
    return record;
}

static void indexed_storage_report_mutable_access_end (struct indexed_storage_node_t *storage,
                                                       struct indexed_storage_dirty_record_node_t *dirty_node)
{
    dirty_node->type = INDEXED_STORAGE_DIRTY_RECORD_CHANGED;
    dirty_node->observation_comparison_flags = observation_buffer_definition_compare (
        &storage->observation_buffer, dirty_node->observation_buffer_memory, dirty_node->source_node->record);
}

static void indexed_storage_report_delete_from_mutable_access (struct indexed_storage_node_t *storage,
                                                               struct indexed_storage_dirty_record_node_t *dirty_node)
{
    dirty_node->type = INDEXED_STORAGE_DIRTY_RECORD_DELETED;
    dirty_node->observation_comparison_flags = observation_buffer_definition_compare (
        &storage->observation_buffer, dirty_node->observation_buffer_memory, dirty_node->source_node->record);
}

static void indexed_storage_report_delete_from_constant_access (struct indexed_storage_node_t *storage,
                                                                struct indexed_storage_record_node_t *node,
                                                                void *from_index,
                                                                void *from_index_node,
                                                                void *from_index_sub_node)
{
    struct indexed_storage_dirty_record_node_t *record = indexed_storage_allocate_dirty_record (storage, KAN_FALSE);
    record->source_node = node;
    record->type = INDEXED_STORAGE_DIRTY_RECORD_DELETED;
    record->observation_buffer_memory = NULL;
    record->observation_comparison_flags = 0u;
    record->dirt_source_index = from_index;
    record->dirt_source_index_node = from_index_node;
    record->dirt_source_index_sub_node = from_index_sub_node;
}

void kan_repository_indexed_insert_query_init (struct kan_repository_indexed_insert_query_t *query,
                                               kan_repository_indexed_storage_t storage)
{
    struct indexed_storage_node_t *storage_data = (struct indexed_storage_node_t *) storage;
    *(struct indexed_insert_query_t *) query = (struct indexed_insert_query_t) {.storage = storage_data};
    kan_atomic_int_add (&storage_data->queries_count, 1);
}

struct kan_repository_indexed_insertion_package_t kan_repository_indexed_insert_query_execute (
    struct kan_repository_indexed_insert_query_t *query)
{
    struct indexed_insert_query_t *query_data = (struct indexed_insert_query_t *) query;
    KAN_ASSERT (query_data->storage)
    indexed_storage_acquire_access (query_data->storage);

    struct indexed_insertion_package_t package;
    package.storage = query_data->storage;
    package.record =
        kan_allocate_batched (query_data->storage->records_allocation_group, query_data->storage->type->size);

    if (query_data->storage->type->init)
    {
        query_data->storage->type->init (query_data->storage->type->functor_user_data, package.record);
    }

    return *(struct kan_repository_indexed_insertion_package_t *) &package;
}

void *kan_repository_indexed_insertion_package_get (struct kan_repository_indexed_insertion_package_t *package)
{
    return ((struct indexed_insertion_package_t *) package)->record;
}

void kan_repository_indexed_insertion_package_undo (struct kan_repository_indexed_insertion_package_t *package)
{
    struct indexed_insertion_package_t *package_data = (struct indexed_insertion_package_t *) package;
    if (package_data->record)
    {
        if (package_data->storage->type->shutdown)
        {
            package_data->storage->type->shutdown (package_data->storage->type->functor_user_data,
                                                   package_data->record);
        }

        kan_free_batched (package_data->storage->records_allocation_group, package_data->record);
    }

    indexed_storage_release_access (package_data->storage);
}

void kan_repository_indexed_insertion_package_submit (struct kan_repository_indexed_insertion_package_t *package)
{
    struct indexed_insertion_package_t *package_data = (struct indexed_insertion_package_t *) package;
    indexed_storage_report_insertion (package_data->storage, package_data->record);
    indexed_storage_release_access (package_data->storage);
}

void kan_repository_indexed_insert_query_shutdown (struct kan_repository_indexed_insert_query_t *query)
{
    struct indexed_insert_query_t *query_data = (struct indexed_insert_query_t *) query;
    if (query_data->storage)
    {
        kan_atomic_int_add (&query_data->storage->queries_count, -1);
        query_data->storage = NULL;
    }
}

static inline void indexed_storage_sequence_query_init (struct indexed_storage_node_t *storage,
                                                        struct indexed_sequence_query_t *query)
{
    *query = (struct indexed_sequence_query_t) {.storage = storage};
    kan_atomic_int_add (&storage->queries_count, 1);
}

static inline struct indexed_sequence_cursor_t indexed_storage_sequence_query_execute (
    struct indexed_sequence_query_t *query)
{
    KAN_ASSERT (query->storage)
    indexed_storage_acquire_access (query->storage);

    return (struct indexed_sequence_cursor_t) {
        .storage = query->storage,
        .node = (struct indexed_storage_record_node_t *) query->storage->records.first,
    };
}

static inline void indexed_storage_sequence_cursor_close (struct indexed_sequence_cursor_t *cursor)
{
    indexed_storage_release_access (cursor->storage);
}

static inline void indexed_storage_sequence_query_shutdown (struct indexed_sequence_query_t *query)
{
    if (query->storage)
    {
        kan_atomic_int_add (&query->storage->queries_count, -1);
        query->storage = NULL;
    }
}

void kan_repository_indexed_sequence_read_query_init (struct kan_repository_indexed_sequence_read_query_t *query,
                                                      kan_repository_indexed_storage_t storage)
{
    indexed_storage_sequence_query_init ((struct indexed_storage_node_t *) storage,
                                         (struct indexed_sequence_query_t *) query);
}

struct kan_repository_indexed_sequence_read_cursor_t kan_repository_indexed_sequence_read_query_execute (
    struct kan_repository_indexed_sequence_read_query_t *query)
{
    struct indexed_sequence_cursor_t cursor =
        indexed_storage_sequence_query_execute ((struct indexed_sequence_query_t *) query);
    return *(struct kan_repository_indexed_sequence_read_cursor_t *) &cursor;
}

struct kan_repository_indexed_sequence_read_access_t kan_repository_indexed_sequence_read_cursor_next (
    struct kan_repository_indexed_sequence_read_cursor_t *cursor)
{
    struct indexed_sequence_cursor_t *cursor_data = (struct indexed_sequence_cursor_t *) cursor;
    struct indexed_sequence_constant_access_t access = {
        .storage = cursor_data->storage,
        .node = cursor_data->node,
    };

    if (cursor_data->node)
    {
        cursor_data->node = (struct indexed_storage_record_node_t *) cursor_data->node->list_node.next;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_read_access_try_create (access.storage, access.node))
        {
            access.node = NULL;
        }
        else
#endif
        {
            indexed_storage_acquire_access (cursor_data->storage);
        }
    }

    return *(struct kan_repository_indexed_sequence_read_access_t *) &access;
}

const void *kan_repository_indexed_sequence_read_access_resolve (
    struct kan_repository_indexed_sequence_read_access_t *access)
{
    struct indexed_sequence_constant_access_t *access_data = (struct indexed_sequence_constant_access_t *) access;
    return access_data->node ? access_data->node->record : NULL;
}

void kan_repository_indexed_sequence_read_access_close (struct kan_repository_indexed_sequence_read_access_t *access)
{
    struct indexed_sequence_constant_access_t *access_data = (struct indexed_sequence_constant_access_t *) access;
    if (access_data->node)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_indexed_read_access_destroyed (access_data->node);
#endif
        indexed_storage_release_access (access_data->storage);
    }
}

void kan_repository_indexed_sequence_read_cursor_close (struct kan_repository_indexed_sequence_read_cursor_t *cursor)
{
    indexed_storage_sequence_cursor_close ((struct indexed_sequence_cursor_t *) cursor);
}

void kan_repository_indexed_sequence_read_query_shutdown (struct kan_repository_indexed_sequence_read_query_t *query)
{
    indexed_storage_sequence_query_shutdown ((struct indexed_sequence_query_t *) query);
}

void kan_repository_indexed_sequence_update_query_init (struct kan_repository_indexed_sequence_update_query_t *query,
                                                        kan_repository_indexed_storage_t storage)
{
    indexed_storage_sequence_query_init ((struct indexed_storage_node_t *) storage,
                                         (struct indexed_sequence_query_t *) query);
}

struct kan_repository_indexed_sequence_update_cursor_t kan_repository_indexed_sequence_update_query_execute (
    struct kan_repository_indexed_sequence_update_query_t *query)
{
    struct indexed_sequence_cursor_t cursor =
        indexed_storage_sequence_query_execute ((struct indexed_sequence_query_t *) query);
    return *(struct kan_repository_indexed_sequence_update_cursor_t *) &cursor;
}

struct kan_repository_indexed_sequence_update_access_t kan_repository_indexed_sequence_update_cursor_next (
    struct kan_repository_indexed_sequence_update_cursor_t *cursor)
{
    struct indexed_sequence_cursor_t *cursor_data = (struct indexed_sequence_cursor_t *) cursor;
    struct indexed_sequence_mutable_access_t access = {
        .storage = cursor_data->storage,
        .dirty_node = NULL,
    };

    if (cursor_data->node)
    {
        cursor_data->node = (struct indexed_storage_record_node_t *) cursor_data->node->list_node.next;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_write_access_try_create (access.storage, cursor_data->node))
        {
            access.dirty_node = NULL;
        }
        else
#endif
        {
            access.dirty_node =
                indexed_storage_report_mutable_access_begin (cursor_data->storage, cursor_data->node, NULL, NULL, NULL);
            indexed_storage_acquire_access (cursor_data->storage);
        }
    }

    return *(struct kan_repository_indexed_sequence_update_access_t *) &access;
}

void *kan_repository_indexed_sequence_update_access_resolve (
    struct kan_repository_indexed_sequence_update_access_t *access)
{
    struct indexed_sequence_mutable_access_t *access_data = (struct indexed_sequence_mutable_access_t *) access;
    return access_data->dirty_node ? access_data->dirty_node->next : NULL;
}

void kan_repository_indexed_sequence_update_access_close (
    struct kan_repository_indexed_sequence_update_access_t *access)
{
    struct indexed_sequence_mutable_access_t *access_data = (struct indexed_sequence_mutable_access_t *) access;
    if (access_data->dirty_node)
    {
        indexed_storage_report_mutable_access_end (access_data->storage, access_data->dirty_node);
        indexed_storage_release_access (access_data->storage);
    }
}

void kan_repository_indexed_sequence_update_cursor_close (
    struct kan_repository_indexed_sequence_update_cursor_t *cursor)
{
    indexed_storage_sequence_cursor_close ((struct indexed_sequence_cursor_t *) cursor);
}

void kan_repository_indexed_sequence_update_query_shutdown (
    struct kan_repository_indexed_sequence_update_query_t *query)
{
    indexed_storage_sequence_query_shutdown ((struct indexed_sequence_query_t *) query);
}

void kan_repository_indexed_sequence_delete_query_init (struct kan_repository_indexed_sequence_delete_query_t *query,
                                                        kan_repository_indexed_storage_t storage)
{
    indexed_storage_sequence_query_init ((struct indexed_storage_node_t *) storage,
                                         (struct indexed_sequence_query_t *) query);
}

struct kan_repository_indexed_sequence_delete_cursor_t kan_repository_indexed_sequence_delete_query_execute (
    struct kan_repository_indexed_sequence_delete_query_t *query)
{
    struct indexed_sequence_cursor_t cursor =
        indexed_storage_sequence_query_execute ((struct indexed_sequence_query_t *) query);
    return *(struct kan_repository_indexed_sequence_delete_cursor_t *) &cursor;
}

struct kan_repository_indexed_sequence_delete_access_t kan_repository_indexed_sequence_delete_cursor_next (
    struct kan_repository_indexed_sequence_delete_cursor_t *cursor)
{
    struct indexed_sequence_cursor_t *cursor_data = (struct indexed_sequence_cursor_t *) cursor;
    struct indexed_sequence_constant_access_t access = {
        .storage = cursor_data->storage,
        .node = cursor_data->node,
    };

    if (cursor_data->node)
    {
        cursor_data->node = (struct indexed_storage_record_node_t *) cursor_data->node->list_node.next;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_write_access_try_create (access.storage, access.node))
        {
            access.node = NULL;
        }
        else
#endif
        {
            indexed_storage_acquire_access (cursor_data->storage);
        }
    }

    return *(struct kan_repository_indexed_sequence_delete_access_t *) &access;
}

const void *kan_repository_indexed_sequence_delete_access_resolve (
    struct kan_repository_indexed_sequence_delete_access_t *access)
{
    struct indexed_sequence_constant_access_t *access_data = (struct indexed_sequence_constant_access_t *) access;
    return access_data->node ? access_data->node->record : NULL;
}

void kan_repository_indexed_sequence_delete_access_delete (
    struct kan_repository_indexed_sequence_delete_access_t *access)
{
    struct indexed_sequence_constant_access_t *access_data = (struct indexed_sequence_constant_access_t *) access;
    indexed_storage_report_delete_from_constant_access (access_data->storage, access_data->node, NULL, NULL, NULL);
    cascade_deleters_definition_fire (&access_data->storage->cascade_deleters, access_data->node->record);
    indexed_storage_release_access (access_data->storage);
}

void kan_repository_indexed_sequence_delete_access_close (
    struct kan_repository_indexed_sequence_delete_access_t *access)
{
    struct indexed_sequence_constant_access_t *access_data = (struct indexed_sequence_constant_access_t *) access;
    if (access_data->node)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_indexed_write_access_destroyed (access_data->node);
#endif
        indexed_storage_release_access (access_data->storage);
    }
}

void kan_repository_indexed_sequence_delete_cursor_close (
    struct kan_repository_indexed_sequence_delete_cursor_t *cursor)
{
    indexed_storage_sequence_cursor_close ((struct indexed_sequence_cursor_t *) cursor);
}

void kan_repository_indexed_sequence_delete_query_shutdown (
    struct kan_repository_indexed_sequence_delete_query_t *query)
{
    indexed_storage_sequence_query_shutdown ((struct indexed_sequence_query_t *) query);
}

void kan_repository_indexed_sequence_write_query_init (struct kan_repository_indexed_sequence_write_query_t *query,
                                                       kan_repository_indexed_storage_t storage)
{
    indexed_storage_sequence_query_init ((struct indexed_storage_node_t *) storage,
                                         (struct indexed_sequence_query_t *) query);
}

struct kan_repository_indexed_sequence_write_cursor_t kan_repository_indexed_sequence_write_query_execute (
    struct kan_repository_indexed_sequence_write_query_t *query)
{
    struct indexed_sequence_cursor_t cursor =
        indexed_storage_sequence_query_execute ((struct indexed_sequence_query_t *) query);
    return *(struct kan_repository_indexed_sequence_write_cursor_t *) &cursor;
}

struct kan_repository_indexed_sequence_write_access_t kan_repository_indexed_sequence_write_cursor_next (
    struct kan_repository_indexed_sequence_write_cursor_t *cursor)
{
    struct indexed_sequence_cursor_t *cursor_data = (struct indexed_sequence_cursor_t *) cursor;
    struct indexed_sequence_mutable_access_t access = {
        .storage = cursor_data->storage,
        .dirty_node = NULL,
    };

    if (cursor_data->node)
    {
        cursor_data->node = (struct indexed_storage_record_node_t *) cursor_data->node->list_node.next;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_write_access_try_create (access.storage, cursor_data->node))
        {
            access.dirty_node = NULL;
        }
        else
#endif
        {
            access.dirty_node =
                indexed_storage_report_mutable_access_begin (cursor_data->storage, cursor_data->node, NULL, NULL, NULL);
            indexed_storage_acquire_access (cursor_data->storage);
        }
    }

    return *(struct kan_repository_indexed_sequence_write_access_t *) &access;
}

void *kan_repository_indexed_sequence_write_access_resolve (
    struct kan_repository_indexed_sequence_write_access_t *access)
{
    struct indexed_sequence_mutable_access_t *access_data = (struct indexed_sequence_mutable_access_t *) access;
    return access_data->dirty_node ? access_data->dirty_node->next : NULL;
}

void kan_repository_indexed_sequence_write_access_delete (struct kan_repository_indexed_sequence_write_access_t *access)
{
    struct indexed_sequence_mutable_access_t *access_data = (struct indexed_sequence_mutable_access_t *) access;
    KAN_ASSERT (access_data->dirty_node)
    indexed_storage_report_delete_from_mutable_access (access_data->storage, access_data->dirty_node);
    cascade_deleters_definition_fire (&access_data->storage->cascade_deleters,
                                      access_data->dirty_node->source_node->record);
    indexed_storage_release_access (access_data->storage);
}

void kan_repository_indexed_sequence_write_access_close (struct kan_repository_indexed_sequence_write_access_t *access)
{
    struct indexed_sequence_mutable_access_t *access_data = (struct indexed_sequence_mutable_access_t *) access;
    if (access_data->dirty_node)
    {
        indexed_storage_report_mutable_access_end (access_data->storage, access_data->dirty_node);
        indexed_storage_release_access (access_data->storage);
    }
}

void kan_repository_indexed_sequence_write_cursor_close (struct kan_repository_indexed_sequence_write_cursor_t *cursor)
{
    indexed_storage_sequence_cursor_close ((struct indexed_sequence_cursor_t *) cursor);
}

void kan_repository_indexed_sequence_write_query_shutdown (struct kan_repository_indexed_sequence_write_query_t *query)
{
    indexed_storage_sequence_query_shutdown ((struct indexed_sequence_query_t *) query);
}

static struct value_index_t *indexed_storage_find_or_create_value_index (struct indexed_storage_node_t *storage,
                                                                         struct kan_repository_field_path_t path)
{
    kan_atomic_int_lock (&storage->maintenance_lock);
    struct value_index_t *index = storage->first_value_index;

    while (index)
    {
        if (is_field_path_equal (path, index->source_path))
        {
            kan_atomic_int_unlock (&storage->maintenance_lock);
            kan_atomic_int_add (&index->storage->queries_count, 1);
            kan_atomic_int_add (&index->queries_count, 1);
            return index;
        }

        index = index->next;
    }

    uint32_t backed_absolute_offset;
    uint8_t backed_size;
    uint8_t backed_size_with_padding;
    struct interned_field_path_t interned_path = copy_field_path (path, storage->value_index_allocation_group);

    if (!value_index_prepare_backed_data (storage->repository->registry, storage->type->name, interned_path,
                                          &backed_absolute_offset, &backed_size, &backed_size_with_padding))
    {
        KAN_LOG (repository, KAN_LOG_ERROR,
                 "Unable to create value index for path (inside struct \"%s\":", storage->type->name);

        for (uint32_t path_index = 0; path_index < path.reflection_path_length; ++path_index)
        {
            KAN_LOG (repository, KAN_LOG_ERROR, "    - %s", path.reflection_path[path_index]);
        }

        shutdown_field_path (interned_path, storage->value_index_allocation_group);
        kan_atomic_int_unlock (&storage->maintenance_lock);
        return NULL;
    }

    index = kan_allocate_batched (storage->value_index_allocation_group, sizeof (struct value_index_t));
    index->next = storage->first_value_index;
    storage->first_value_index = index;

    index->storage = storage;
    index->queries_count = kan_atomic_int_init (0);
    index->backed_field_absolute_offset = backed_absolute_offset;
    index->backed_field_size = backed_size;
    index->backed_field_size_with_padding = backed_size_with_padding;

    kan_hash_storage_init (&index->hash_storage, storage->value_index_allocation_group,
                           KAN_REPOSITORY_VALUE_INDEX_INITIAL_BUCKETS);
    index->source_path = interned_path;

    kan_atomic_int_lock (&storage->maintenance_lock);
    kan_atomic_int_add (&index->storage->queries_count, 1);
    kan_atomic_int_add (&index->queries_count, 1);
    return index;
}

static inline void indexed_storage_value_query_init (struct indexed_value_query_t *query,
                                                     struct indexed_storage_node_t *storage,
                                                     struct kan_repository_field_path_t path)
{
    *query = (struct indexed_value_query_t) {.index = indexed_storage_find_or_create_value_index (storage, path)};
    kan_atomic_int_add (&storage->queries_count, 1);
}

static inline struct indexed_value_cursor_t indexed_storage_value_query_execute (struct indexed_value_query_t *query,
                                                                                 const void *value)
{
    struct indexed_value_query_t *query_data = (struct indexed_value_query_t *) query;

    // Handle queries for invalid indices.
    if (!query_data->index)
    {
        return (struct indexed_value_cursor_t) {
            .index = NULL,
            .node = NULL,
            .sub_node = NULL,
        };
    }

    indexed_storage_acquire_access (query_data->index->storage);
    uint64_t hash = 0u;

    switch (query_data->index->backed_field_size)
    {
    case 1u:
        hash = *(const uint8_t *) value;
        break;
    case 2u:
        hash = *(const uint16_t *) value;
        break;
    case 4u:
        hash = *(const uint32_t *) value;
        break;
    case 8u:
        hash = *(const uint64_t *) value;
        break;
    }

    struct value_index_node_t *node = value_index_query_node_from_hash (query_data->index, hash);
    return (struct indexed_value_cursor_t) {
        .index = query_data->index,
        .node = node,
        .sub_node = node ? node->first_sub_node : NULL,
    };
}

static inline void indexed_storage_value_cursor_close (struct indexed_value_cursor_t *cursor)
{
    if (cursor->index)
    {
        indexed_storage_release_access (cursor->index->storage);
    }
}

static inline void indexed_storage_value_query_shutdown (struct indexed_value_query_t *query)
{
    if (query->index)
    {
        kan_atomic_int_add (&query->index->queries_count, -1);
        kan_atomic_int_add (&query->index->storage->queries_count, -1);
        query->index = NULL;
    }
}

void kan_repository_indexed_value_read_query_init (struct kan_repository_indexed_value_read_query_t *query,
                                                   kan_repository_indexed_storage_t storage,
                                                   struct kan_repository_field_path_t path)
{
    indexed_storage_value_query_init ((struct indexed_value_query_t *) query, (struct indexed_storage_node_t *) storage,
                                      path);
}

struct kan_repository_indexed_value_read_cursor_t kan_repository_indexed_value_read_query_execute (
    struct kan_repository_indexed_value_read_query_t *query, const void *value)
{
    struct indexed_value_cursor_t cursor =
        indexed_storage_value_query_execute ((struct indexed_value_query_t *) query, value);
    return *(struct kan_repository_indexed_value_read_cursor_t *) &cursor;
}

struct kan_repository_indexed_value_read_access_t kan_repository_indexed_value_read_cursor_next (
    struct kan_repository_indexed_value_read_cursor_t *cursor)
{
    struct indexed_value_cursor_t *cursor_data = (struct indexed_value_cursor_t *) cursor;
    struct indexed_value_constant_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->node,
        .sub_node = cursor_data->sub_node,
    };

    if (cursor_data->index && cursor_data->sub_node)
    {
        cursor_data->sub_node = cursor_data->sub_node->next;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_read_access_try_create (access.index->storage, access.sub_node->record))
        {
            access.sub_node = NULL;
        }
        else
#endif
        {
            indexed_storage_acquire_access (cursor_data->index->storage);
        }
    }

    return *(struct kan_repository_indexed_value_read_access_t *) &access;
}

const void *kan_repository_indexed_value_read_access_resolve (struct kan_repository_indexed_value_read_access_t *access)
{
    struct indexed_value_constant_access_t *access_data = (struct indexed_value_constant_access_t *) access;
    return access_data->sub_node ? access_data->sub_node->record->record : NULL;
}

void kan_repository_indexed_value_read_access_close (struct kan_repository_indexed_value_read_access_t *access)
{
    struct indexed_value_constant_access_t *access_data = (struct indexed_value_constant_access_t *) access;
    if (access_data->sub_node)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_indexed_read_access_destroyed (access_data->sub_node->record);
#endif
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_value_read_cursor_close (struct kan_repository_indexed_value_read_cursor_t *cursor)
{
    indexed_storage_value_cursor_close ((struct indexed_value_cursor_t *) cursor);
}

void kan_repository_indexed_value_read_query_shutdown (struct kan_repository_indexed_value_read_query_t *query)
{
    indexed_storage_value_query_shutdown ((struct indexed_value_query_t *) query);
}

void kan_repository_indexed_value_update_query_init (struct kan_repository_indexed_value_update_query_t *query,
                                                     kan_repository_indexed_storage_t storage,
                                                     struct kan_repository_field_path_t path)
{
    indexed_storage_value_query_init ((struct indexed_value_query_t *) query, (struct indexed_storage_node_t *) storage,
                                      path);
}

struct kan_repository_indexed_value_update_cursor_t kan_repository_indexed_value_update_query_execute (
    struct kan_repository_indexed_value_update_query_t *query, const void *value)
{
    struct indexed_value_cursor_t cursor =
        indexed_storage_value_query_execute ((struct indexed_value_query_t *) query, value);
    return *(struct kan_repository_indexed_value_update_cursor_t *) &cursor;
}

struct kan_repository_indexed_value_update_access_t kan_repository_indexed_value_update_cursor_next (
    struct kan_repository_indexed_value_update_cursor_t *cursor)
{
    struct indexed_value_cursor_t *cursor_data = (struct indexed_value_cursor_t *) cursor;
    struct indexed_value_mutable_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->node,
        .sub_node = cursor_data->sub_node,
        .dirty_node = NULL,
    };

    if (cursor_data->index && cursor_data->sub_node)
    {
        cursor_data->sub_node = cursor_data->sub_node->next;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_write_access_try_create (access.index->storage, access.sub_node->record))
        {
            access.sub_node = NULL;
        }
        else
#endif
        {
            access.dirty_node = indexed_storage_report_mutable_access_begin (
                access.index->storage, access.sub_node->record, access.index, access.node, access.sub_node);
            indexed_storage_acquire_access (cursor_data->index->storage);
        }
    }

    return *(struct kan_repository_indexed_value_update_access_t *) &access;
}

void *kan_repository_indexed_value_update_access_resolve (struct kan_repository_indexed_value_update_access_t *access)
{
    struct indexed_value_mutable_access_t *access_data = (struct indexed_value_mutable_access_t *) access;
    return access_data->sub_node ? access_data->sub_node->record->record : NULL;
}

void kan_repository_indexed_value_update_access_close (struct kan_repository_indexed_value_update_access_t *access)
{
    struct indexed_value_mutable_access_t *access_data = (struct indexed_value_mutable_access_t *) access;
    if (access_data->dirty_node)
    {
        indexed_storage_report_mutable_access_end (access_data->index->storage, access_data->dirty_node);
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_value_update_cursor_close (struct kan_repository_indexed_value_update_cursor_t *cursor)
{
    indexed_storage_value_cursor_close ((struct indexed_value_cursor_t *) cursor);
}

void kan_repository_indexed_value_update_query_shutdown (struct kan_repository_indexed_value_update_query_t *query)
{
    indexed_storage_value_query_shutdown ((struct indexed_value_query_t *) query);
}

void kan_repository_indexed_value_delete_query_init (struct kan_repository_indexed_value_delete_query_t *query,
                                                     kan_repository_indexed_storage_t storage,
                                                     struct kan_repository_field_path_t path)
{
    indexed_storage_value_query_init ((struct indexed_value_query_t *) query, (struct indexed_storage_node_t *) storage,
                                      path);
}

struct kan_repository_indexed_value_delete_cursor_t kan_repository_indexed_value_delete_query_execute (
    struct kan_repository_indexed_value_delete_query_t *query, const void *value)
{
    struct indexed_value_cursor_t cursor =
        indexed_storage_value_query_execute ((struct indexed_value_query_t *) query, value);
    return *(struct kan_repository_indexed_value_delete_cursor_t *) &cursor;
}

struct kan_repository_indexed_value_delete_access_t kan_repository_indexed_value_delete_cursor_next (
    struct kan_repository_indexed_value_delete_cursor_t *cursor)
{
    struct indexed_value_cursor_t *cursor_data = (struct indexed_value_cursor_t *) cursor;
    struct indexed_value_constant_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->node,
        .sub_node = cursor_data->sub_node,
    };

    if (cursor_data->index && cursor_data->sub_node)
    {
        cursor_data->sub_node = cursor_data->sub_node->next;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_write_access_try_create (access.index->storage, access.sub_node->record))
        {
            access.sub_node = NULL;
        }
        else
#endif
        {
            indexed_storage_acquire_access (cursor_data->index->storage);
        }
    }

    return *(struct kan_repository_indexed_value_delete_access_t *) &access;
}

const void *kan_repository_indexed_value_delete_access_resolve (
    struct kan_repository_indexed_value_delete_access_t *access)
{
    struct indexed_value_constant_access_t *access_data = (struct indexed_value_constant_access_t *) access;
    return access_data->sub_node ? access_data->sub_node->record->record : NULL;
}

void kan_repository_indexed_value_delete_access_delete (struct kan_repository_indexed_value_delete_access_t *access)
{
    struct indexed_value_constant_access_t *access_data = (struct indexed_value_constant_access_t *) access;
    indexed_storage_report_delete_from_constant_access (access_data->index->storage, access_data->sub_node->record,
                                                        access_data->index, access_data->node, access_data->sub_node);
    cascade_deleters_definition_fire (&access_data->index->storage->cascade_deleters,
                                      access_data->sub_node->record->record);
    indexed_storage_release_access (access_data->index->storage);
}

void kan_repository_indexed_value_delete_access_close (struct kan_repository_indexed_value_delete_access_t *access)
{
    struct indexed_value_constant_access_t *access_data = (struct indexed_value_constant_access_t *) access;
    if (access_data->sub_node)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_indexed_write_access_destroyed (access_data->sub_node->record);
#endif
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_value_delete_cursor_close (struct kan_repository_indexed_value_delete_cursor_t *cursor)
{
    indexed_storage_value_cursor_close ((struct indexed_value_cursor_t *) cursor);
}

void kan_repository_indexed_value_delete_query_shutdown (struct kan_repository_indexed_value_delete_query_t *query)
{
    indexed_storage_value_query_shutdown ((struct indexed_value_query_t *) query);
}

void kan_repository_indexed_value_write_query_init (struct kan_repository_indexed_value_write_query_t *query,
                                                    kan_repository_indexed_storage_t storage,
                                                    struct kan_repository_field_path_t path)
{
    indexed_storage_value_query_init ((struct indexed_value_query_t *) query, (struct indexed_storage_node_t *) storage,
                                      path);
}

struct kan_repository_indexed_value_write_cursor_t kan_repository_indexed_value_write_query_execute (
    struct kan_repository_indexed_value_write_query_t *query, const void *value)
{
    struct indexed_value_cursor_t cursor =
        indexed_storage_value_query_execute ((struct indexed_value_query_t *) query, value);
    return *(struct kan_repository_indexed_value_write_cursor_t *) &cursor;
}

struct kan_repository_indexed_value_write_access_t kan_repository_indexed_value_write_cursor_next (
    struct kan_repository_indexed_value_write_cursor_t *cursor)
{
    struct indexed_value_cursor_t *cursor_data = (struct indexed_value_cursor_t *) cursor;
    struct indexed_value_mutable_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->node,
        .sub_node = cursor_data->sub_node,
        .dirty_node = NULL,
    };

    if (cursor_data->index && cursor_data->sub_node)
    {
        cursor_data->sub_node = cursor_data->sub_node->next;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_write_access_try_create (access.index->storage, access.sub_node->record))
        {
            access.sub_node = NULL;
        }
        else
#endif
        {
            access.dirty_node = indexed_storage_report_mutable_access_begin (
                access.index->storage, access.sub_node->record, access.index, access.node, access.sub_node);
            indexed_storage_acquire_access (cursor_data->index->storage);
        }
    }

    return *(struct kan_repository_indexed_value_write_access_t *) &access;
}

void *kan_repository_indexed_value_write_access_resolve (struct kan_repository_indexed_value_write_access_t *access)
{
    struct indexed_value_mutable_access_t *access_data = (struct indexed_value_mutable_access_t *) access;
    return access_data->sub_node ? access_data->sub_node->record->record : NULL;
}

void kan_repository_indexed_value_write_access_delete (struct kan_repository_indexed_value_write_access_t *access)
{
    struct indexed_value_mutable_access_t *access_data = (struct indexed_value_mutable_access_t *) access;
    KAN_ASSERT (access_data->dirty_node)
    indexed_storage_report_delete_from_mutable_access (access_data->index->storage, access_data->dirty_node);
    cascade_deleters_definition_fire (&access_data->index->storage->cascade_deleters,
                                      access_data->sub_node->record->record);
    indexed_storage_release_access (access_data->index->storage);
}

void kan_repository_indexed_value_write_access_close (struct kan_repository_indexed_value_write_access_t *access)
{
    struct indexed_value_mutable_access_t *access_data = (struct indexed_value_mutable_access_t *) access;
    if (access_data->dirty_node)
    {
        indexed_storage_report_mutable_access_end (access_data->index->storage, access_data->dirty_node);
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_value_write_cursor_close (struct kan_repository_indexed_value_write_cursor_t *cursor)
{
    indexed_storage_value_cursor_close ((struct indexed_value_cursor_t *) cursor);
}

void kan_repository_indexed_value_write_query_shutdown (struct kan_repository_indexed_value_write_query_t *query)
{
    indexed_storage_value_query_shutdown ((struct indexed_value_query_t *) query);
}

static struct event_storage_node_t *query_event_storage_across_hierarchy (struct repository_t *repository,
                                                                          kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&repository->event_storages, (uint64_t) type_name);
    struct event_storage_node_t *node = (struct event_storage_node_t *) bucket->first;
    const struct event_storage_node_t *end = (struct event_storage_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->type->name == type_name)
        {
            return node;
        }

        node = (struct event_storage_node_t *) node->node.list_node.next;
    }

    return repository->parent ? query_event_storage_across_hierarchy (repository->parent, type_name) : NULL;
}

static struct event_queue_node_t *event_queue_node_allocate (kan_allocation_group_t storage_allocation_group)
{
    struct event_queue_node_t *node = (struct event_queue_node_t *) kan_allocate_batched (
        storage_allocation_group, sizeof (struct event_queue_node_t));
    node->event = NULL;
    return node;
}

kan_repository_event_storage_t kan_repository_event_storage_open (kan_repository_t repository,
                                                                  kan_interned_string_t type_name)
{
    struct repository_t *repository_data = (struct repository_t *) repository;
    KAN_ASSERT (repository_data->mode == REPOSITORY_MODE_PLANNING)
    repository_storage_map_access_lock (repository_data);
    const kan_interned_string_t interned_type_name = kan_string_intern (type_name);
    struct event_storage_node_t *storage = query_event_storage_across_hierarchy (repository_data, interned_type_name);

    if (!storage)
    {
        const struct kan_reflection_struct_t *event_type =
            kan_reflection_registry_query_struct (repository_data->registry, interned_type_name);

        if (!event_type)
        {
            KAN_LOG (repository, KAN_LOG_ERROR, "Event type \"%s\" not found.", interned_type_name)
            repository_storage_map_access_unlock (repository_data);
            return KAN_INVALID_REPOSITORY_EVENT_STORAGE;
        }

        const kan_allocation_group_t storage_allocation_group = kan_allocation_group_get_child (
            kan_allocation_group_get_child (repository_data->allocation_group, "events"), interned_type_name);

        storage = (struct event_storage_node_t *) kan_allocate_batched (storage_allocation_group,
                                                                        sizeof (struct event_storage_node_t));

        storage->node.hash = (uint64_t) interned_type_name;
        storage->allocation_group = storage_allocation_group;
        storage->type = event_type;
        storage->single_threaded_operations_lock = kan_atomic_int_init (0);
        storage->queries_count = kan_atomic_int_init (0);
        kan_event_queue_init (&storage->event_queue, &event_queue_node_allocate (storage_allocation_group)->node);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        storage->safeguard_access_status = kan_atomic_int_init (0);
#endif

        if (repository_data->event_storages.bucket_count * KAN_REPOSITORY_EVENT_STORAGE_LOAD_FACTOR <=
            repository_data->event_storages.items.size)
        {
            kan_hash_storage_set_bucket_count (&repository_data->event_storages,
                                               repository_data->event_storages.bucket_count * 2u);
        }

        kan_hash_storage_add (&repository_data->event_storages, &storage->node);
    }

    repository_storage_map_access_unlock (repository_data);
    return (kan_repository_event_storage_t) storage;
}

void kan_repository_event_insert_query_init (struct kan_repository_event_insert_query_t *query,
                                             kan_repository_event_storage_t storage)
{
    struct event_storage_node_t *storage_data = (struct event_storage_node_t *) storage;
    kan_atomic_int_add (&storage_data->queries_count, 1);
    *(struct event_insert_query_t *) query = (struct event_insert_query_t) {.storage = storage_data};
}

struct kan_repository_event_insertion_package_t kan_repository_event_insert_query_execute (
    struct kan_repository_event_insert_query_t *query)
{
    struct event_insert_query_t *query_data = (struct event_insert_query_t *) query;
    KAN_ASSERT (query_data->storage)

    struct event_insertion_package_t package;
    package.storage = query_data->storage;

    // No subscribers -- no need to create event. Initial subscribers are fetch queries,
    // therefore if there are any queries, there always will be more than zero iterators.
    if (kan_atomic_int_get (&query_data->storage->event_queue.total_iterators) == 0)
    {
        package.event = NULL;
        return *(struct kan_repository_event_insertion_package_t *) &package;
    }

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    if (!safeguard_event_insertion_package_try_create (query_data->storage))
    {
        package.event = NULL;
        return *(struct kan_repository_event_insertion_package_t *) &package;
    }
#endif

    package.event = kan_allocate_batched (query_data->storage->allocation_group, query_data->storage->type->size);
    if (query_data->storage->type->init)
    {
        query_data->storage->type->init (query_data->storage->type->functor_user_data, package.event);
    }

    return *(struct kan_repository_event_insertion_package_t *) &package;
}

void *kan_repository_event_insertion_package_get (struct kan_repository_event_insertion_package_t *package)
{
    return ((struct event_insertion_package_t *) package)->event;
}

void kan_repository_event_insertion_package_undo (struct kan_repository_event_insertion_package_t *package)
{
    struct event_insertion_package_t *package_data = (struct event_insertion_package_t *) package;
    KAN_ASSERT (package_data->storage)
    KAN_ASSERT (package_data->event)

    if (package_data->storage->type->shutdown)
    {
        package_data->storage->type->shutdown (package_data->storage->type->functor_user_data, package_data->event);
    }

    kan_free_batched (package_data->storage->allocation_group, package_data->event);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    safeguard_event_insertion_package_destroyed (package_data->storage);
#endif
}

void kan_repository_event_insertion_package_submit (struct kan_repository_event_insertion_package_t *package)
{
    struct event_insertion_package_t *package_data = (struct event_insertion_package_t *) package;
    KAN_ASSERT (package_data->storage)
    KAN_ASSERT (package_data->event)

    kan_atomic_int_lock (&package_data->storage->single_threaded_operations_lock);
    struct event_queue_node_t *node =
        (struct event_queue_node_t *) kan_event_queue_submit_begin (&package_data->storage->event_queue);

    KAN_ASSERT (node);
    node->event = package_data->event;

    kan_event_queue_submit_end (&package_data->storage->event_queue,
                                &event_queue_node_allocate (package_data->storage->allocation_group)->node);
    kan_atomic_int_unlock (&package_data->storage->single_threaded_operations_lock);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    safeguard_event_insertion_package_destroyed (package_data->storage);
#endif
}

void kan_repository_event_insert_query_shutdown (struct kan_repository_event_insert_query_t *query)
{
    struct event_insert_query_t *query_data = (struct event_insert_query_t *) query;
    if (query_data->storage)
    {
        kan_atomic_int_add (&query_data->storage->queries_count, -1);
        query_data->storage = NULL;
    }
}

void kan_repository_event_fetch_query_init (struct kan_repository_event_fetch_query_t *query,
                                            kan_repository_event_storage_t storage)
{
    struct event_storage_node_t *storage_data = (struct event_storage_node_t *) storage;
    kan_atomic_int_add (&storage_data->queries_count, 1);

    *(struct event_fetch_query_t *) query = (struct event_fetch_query_t) {
        .storage = storage_data,
        .iterator = kan_event_queue_iterator_create (&storage_data->event_queue),
    };
}

struct kan_repository_event_read_access_t kan_repository_event_fetch_query_next (
    struct kan_repository_event_fetch_query_t *query)
{
    struct event_fetch_query_t *query_data = (struct event_fetch_query_t *) query;
    KAN_ASSERT (query_data->storage)
    struct event_read_access_t access;
    access.storage = query_data->storage;

    if (!kan_event_queue_iterator_get (&query_data->storage->event_queue, query_data->iterator))
    {
        access.iterator = KAN_INVALID_EVENT_QUEUE_ITERATOR;
        return *(struct kan_repository_event_read_access_t *) &access;
    }

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    if (!safeguard_event_read_access_try_create (query_data->storage))
    {
        access.iterator = KAN_INVALID_EVENT_QUEUE_ITERATOR;
        return *(struct kan_repository_event_read_access_t *) &access;
    }
#endif

    access.iterator = query_data->iterator;
    query_data->iterator =
        kan_event_queue_iterator_create_next (&query_data->storage->event_queue, query_data->iterator);
    return *(struct kan_repository_event_read_access_t *) &access;
}

const void *kan_repository_event_read_access_resolve (struct kan_repository_event_read_access_t *access)
{
    struct event_read_access_t *access_data = (struct event_read_access_t *) access;
    KAN_ASSERT (access_data->storage)

    if (access_data->iterator != KAN_INVALID_EVENT_QUEUE_ITERATOR)
    {
        struct event_queue_node_t *node = (struct event_queue_node_t *) kan_event_queue_iterator_get (
            &access_data->storage->event_queue, access_data->iterator);

        if (node)
        {
            KAN_ASSERT (node->event);
            return node->event;
        }
    }

    return NULL;
}

void kan_repository_event_read_access_close (struct kan_repository_event_read_access_t *access)
{
    struct event_read_access_t *access_data = (struct event_read_access_t *) access;
    KAN_ASSERT (access_data->storage)

    if (access_data->iterator != KAN_INVALID_EVENT_QUEUE_ITERATOR)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_event_read_access_destroyed (access_data->storage);
#endif

        const kan_bool_t should_attempt_cleanup =
            kan_event_queue_iterator_destroy (&access_data->storage->event_queue, access_data->iterator);

        if (should_attempt_cleanup)
        {
            kan_atomic_int_lock (&access_data->storage->single_threaded_operations_lock);
            struct event_queue_node_t *oldest =
                (struct event_queue_node_t *) kan_event_queue_clean_oldest (&access_data->storage->event_queue);

            while (oldest)
            {
                event_queue_node_shutdown_and_free (oldest, access_data->storage);
                oldest =
                    (struct event_queue_node_t *) kan_event_queue_clean_oldest (&access_data->storage->event_queue);
            }

            kan_atomic_int_unlock (&access_data->storage->single_threaded_operations_lock);
        }
    }
}

void kan_repository_event_fetch_query_shutdown (struct kan_repository_event_fetch_query_t *query)
{
    struct event_fetch_query_t *query_data = (struct event_fetch_query_t *) query;
    if (query_data->storage)
    {
        kan_atomic_int_add (&query_data->storage->queries_count, -1);
        query_data->storage = NULL;
    }
}

static void repository_clean_storages (struct repository_t *repository)
{
    struct singleton_storage_node_t *singleton_storage_node =
        (struct singleton_storage_node_t *) repository->singleton_storages.items.first;

    while (singleton_storage_node)
    {
        struct singleton_storage_node_t *next =
            (struct singleton_storage_node_t *) singleton_storage_node->node.list_node.next;

        if (kan_atomic_int_get (&singleton_storage_node->queries_count) == 0)
        {
            singleton_storage_node_shutdown_and_free (singleton_storage_node, repository);
        }

        singleton_storage_node = next;
    }

    struct indexed_storage_node_t *indexed_storage_node =
        (struct indexed_storage_node_t *) repository->indexed_storages.items.first;

    while (indexed_storage_node)
    {
        struct indexed_storage_node_t *next =
            (struct indexed_storage_node_t *) indexed_storage_node->node.list_node.next;

        struct value_index_t *value_index = indexed_storage_node->first_value_index;
        struct value_index_t *previous_value_index = NULL;

        while (value_index)
        {
            struct value_index_t *next_value_index = value_index->next;
            if (kan_atomic_int_get (&value_index->queries_count) == 0)
            {
                // No queries for this index -- drop it.
                value_index_shutdown_and_free (value_index);

                if (previous_value_index)
                {
                    previous_value_index->next = next_value_index;
                }
                else
                {
                    indexed_storage_node->first_value_index = next_value_index;
                }
            }
            else
            {
                previous_value_index = value_index;
            }

            value_index = next_value_index;
        }

        // TODO: Clean up unused indices.

        if (kan_atomic_int_get (&indexed_storage_node->queries_count) == 0)
        {
            indexed_storage_node_shutdown_and_free (indexed_storage_node, repository);
        }

        indexed_storage_node = next;
    }

    struct event_storage_node_t *event_storage_node =
        (struct event_storage_node_t *) repository->event_storages.items.first;

    while (event_storage_node)
    {
        struct event_storage_node_t *next = (struct event_storage_node_t *) event_storage_node->node.list_node.next;
        if (kan_atomic_int_get (&event_storage_node->queries_count) == 0)
        {
            event_storage_node_shutdown_and_free (event_storage_node, repository);
        }

        event_storage_node = next;
    }

    repository = repository->first;
    while (repository)
    {
        repository_clean_storages (repository);
        repository = repository->next;
    }
}

static void extract_observation_chunks_from_on_change_events (
    struct repository_t *repository,
    const struct kan_reflection_struct_t *observed_struct,
    uint64_t *event_flag,
    struct kan_stack_group_allocator_t *temporary_allocator,
    struct observation_buffer_scenario_chunk_list_node_t **first,
    struct observation_buffer_scenario_chunk_list_node_t **last)
{
    ensure_interned_strings_ready ();
    struct kan_reflection_struct_meta_iterator_t iterator = kan_reflection_registry_query_struct_meta (
        repository->registry, observed_struct->name, meta_automatic_on_change_event_name);

    struct kan_repository_meta_automatic_on_change_event_t *event =
        (struct kan_repository_meta_automatic_on_change_event_t *) kan_reflection_struct_meta_iterator_get (&iterator);

    while (event)
    {
        // Query for event storage to check if event is used anywhere. No need to observe for unused events.
        const kan_interned_string_t event_type = kan_string_intern (event->event_type);

        if (query_event_storage_across_hierarchy (repository, event_type))
        {
            for (uint64_t index = 0u; index < event->observed_fields_count; ++index)
            {
                struct kan_repository_field_path_t *path = &event->observed_fields[index];
                uint64_t absolute_offset;
                uint64_t size_with_padding;

                const struct kan_reflection_field_t *field = query_field_for_automatic_event_from_path (
                    observed_struct->name, path, repository->registry, temporary_allocator, &absolute_offset,
                    &size_with_padding);

                if (!field)
                {
                    continue;
                }

#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
                if (!validation_field_is_observable (repository->registry, field->archetype, field->name,
                                                     &field->archetype_struct))
                {
                    continue;
                }
#endif

                struct observation_buffer_scenario_chunk_list_node_t *node =
                    (struct observation_buffer_scenario_chunk_list_node_t *) kan_stack_group_allocator_allocate (
                        temporary_allocator, sizeof (struct observation_buffer_scenario_chunk_list_node_t),
                        _Alignof (struct observation_buffer_scenario_chunk_list_node_t));

                node->next = 0u;
                node->source_offset = absolute_offset;
                node->size = size_with_padding;
                node->flags = *event_flag;

                if (*last)
                {
                    (*last)->next = node;
                    *last = node;
                }
                else
                {
                    *first = node;
                    *last = node;
                }
            }

            *event_flag <<= 1u;
            KAN_ASSERT (*event_flag > 0u)
        }

        kan_reflection_struct_meta_iterator_next (&iterator);
        event = (struct kan_repository_meta_automatic_on_change_event_t *) kan_reflection_struct_meta_iterator_get (
            &iterator);
    }
}

static void prepare_singleton_storage (uint64_t user_data)
{
    struct singleton_switch_to_serving_user_data_t *data = (struct singleton_switch_to_serving_user_data_t *) user_data;
    struct kan_stack_group_allocator_t temporary_allocator;

    kan_stack_group_allocator_init (
        &temporary_allocator, kan_allocation_group_get_child (data->storage->allocation_group, "switch_to_serving"),
        KAN_REPOSITORY_SWITCH_TO_SERVING_STACK_INITIAL_SIZE);

    struct observation_buffer_scenario_chunk_list_node_t *first_chunk = NULL;
    struct observation_buffer_scenario_chunk_list_node_t *last_chunk = NULL;

    uint64_t extraction_event_flag = 1u;
    extract_observation_chunks_from_on_change_events (data->repository, data->storage->type, &extraction_event_flag,
                                                      &temporary_allocator, &first_chunk, &last_chunk);

    if (first_chunk)
    {
        observation_buffer_definition_build (&data->storage->observation_buffer, first_chunk, &temporary_allocator,
                                             data->storage->automation_allocation_group);

        if (data->storage->observation_buffer.buffer_size > 0u)
        {
            data->storage->observation_buffer_memory =
                kan_allocate_general (data->storage->automation_allocation_group,
                                      data->storage->observation_buffer.buffer_size, OBSERVATION_BUFFER_ALIGNMENT);
        }
    }

    uint64_t building_event_flag = 1u;
    observation_event_triggers_definition_build (
        &data->storage->observation_events_triggers, &building_event_flag, data->repository, data->storage->type,
        &data->storage->observation_buffer, &temporary_allocator, data->storage->automation_allocation_group);
    kan_stack_group_allocator_shutdown (&temporary_allocator);
}

static void extract_observation_chunks_from_indices (struct indexed_storage_node_t *storage,
                                                     uint64_t *event_flag,
                                                     struct kan_stack_group_allocator_t *temporary_allocator,
                                                     struct observation_buffer_scenario_chunk_list_node_t **first,
                                                     struct observation_buffer_scenario_chunk_list_node_t **last)
{
    struct value_index_t *value_index = storage->first_value_index;
    while (value_index)
    {
        struct observation_buffer_scenario_chunk_list_node_t *node =
            (struct observation_buffer_scenario_chunk_list_node_t *) kan_stack_group_allocator_allocate (
                temporary_allocator, sizeof (struct observation_buffer_scenario_chunk_list_node_t),
                _Alignof (struct observation_buffer_scenario_chunk_list_node_t));

        node->next = 0u;
        node->source_offset = value_index->backed_field_absolute_offset;
        node->size = value_index->backed_field_size_with_padding;
        node->flags = *event_flag;

        if (*last)
        {
            (*last)->next = node;
            *last = node;
        }
        else
        {
            *first = node;
            *last = node;
        }

        *event_flag <<= 1u;
        KAN_ASSERT (*event_flag > 0u)
        value_index = value_index->next;
    }

    // TODO: Extract observation chunks from indices.
}

static void prepare_indices (struct indexed_storage_node_t *storage, uint64_t *event_flag)
{
    struct value_index_t *value_index = storage->first_value_index;
    while (value_index)
    {
        uint64_t buffer_offset = 0u;
        const uint64_t field_begin = value_index->backed_field_absolute_offset;
        const uint64_t field_end = field_begin + value_index->backed_field_size;
        kan_bool_t found_in_buffer = KAN_FALSE;

        for (uint64_t index = 0u; index < storage->observation_buffer.scenario_chunks_count; ++index)
        {
            struct observation_buffer_scenario_chunk_t *chunk = &storage->observation_buffer.scenario_chunks[index];
            if (field_end >= chunk->source_offset)
            {
                // Can only be broken if index field chunk is broken due to invalid internal logic or attempt to
                // make index for field inside union. Either way, it is impossible to fix situation in this case.
                KAN_ASSERT (field_begin >= chunk->source_offset)
                KAN_ASSERT (field_end <= chunk->source_offset + chunk->size)

                const uint64_t offset = buffer_offset + (field_begin - chunk->source_offset);
                KAN_ASSERT (offset < UINT16_MAX)
                value_index->backed_field_offset_in_buffer = (uint16_t) offset;

                found_in_buffer = KAN_TRUE;
                break;
            }

            buffer_offset = kan_apply_alignment (buffer_offset + chunk->size, OBSERVATION_BUFFER_ALIGNMENT);
        }

        KAN_ASSERT (found_in_buffer)
        KAN_ASSERT (value_index->hash_storage.items.size == 0u ||
                    value_index->hash_storage.items.size == storage->records.size)

        if (value_index->hash_storage.items.size == 0u)
        {
            // Fill fresh index with values.
            struct indexed_storage_record_node_t *record =
                (struct indexed_storage_record_node_t *) storage->records.first;

            while (record)
            {
                value_index_insert_record (value_index, record);
                record = (struct indexed_storage_record_node_t *) record->list_node.next;
            }
        }

        value_index->observation_flags = *event_flag;
        *event_flag <<= 1u;
        KAN_ASSERT (*event_flag > 0u)
        value_index = value_index->next;
    }

    // TODO: Fill or update indices if needed. Remember about filling buffer offsets for indexed fields.
}

static void prepare_indexed_storage (uint64_t user_data)
{
    struct indexed_switch_to_serving_user_data_t *data = (struct indexed_switch_to_serving_user_data_t *) user_data;
    struct kan_stack_group_allocator_t temporary_allocator;

    kan_stack_group_allocator_init (
        &temporary_allocator, kan_allocation_group_get_child (data->storage->allocation_group, "switch_to_serving"),
        KAN_REPOSITORY_SWITCH_TO_SERVING_STACK_INITIAL_SIZE);

    struct observation_buffer_scenario_chunk_list_node_t *first_chunk = NULL;
    struct observation_buffer_scenario_chunk_list_node_t *last_chunk = NULL;

    uint64_t extraction_event_flag = 1u;
    extract_observation_chunks_from_on_change_events (data->repository, data->storage->type, &extraction_event_flag,
                                                      &temporary_allocator, &first_chunk, &last_chunk);

    extract_observation_chunks_from_indices (data->storage, &extraction_event_flag, &temporary_allocator, &first_chunk,
                                             &last_chunk);

    if (first_chunk)
    {
        observation_buffer_definition_build (&data->storage->observation_buffer, first_chunk, &temporary_allocator,
                                             data->storage->automation_allocation_group);
    }

    uint64_t building_event_flag = 1u;
    observation_event_triggers_definition_build (
        &data->storage->observation_events_triggers, &building_event_flag, data->repository, data->storage->type,
        &data->storage->observation_buffer, &temporary_allocator, data->storage->automation_allocation_group);

    lifetime_event_triggers_definition_build (&data->storage->on_insert_events_triggers, data->repository,
                                              data->storage->type, LIFETIME_EVENT_TRIGGER_ON_INSERT,
                                              &temporary_allocator, data->storage->automation_allocation_group);

    lifetime_event_triggers_definition_build (&data->storage->on_delete_events_triggers, data->repository,
                                              data->storage->type, LIFETIME_EVENT_TRIGGER_ON_DELETE,
                                              &temporary_allocator, data->storage->automation_allocation_group);

    prepare_indices (data->storage, &building_event_flag);
    cascade_deleters_definition_build (&data->storage->cascade_deleters, data->storage->type->name, data->repository,
                                       &temporary_allocator, data->storage->automation_allocation_group);
    kan_stack_group_allocator_shutdown (&temporary_allocator);
}

static void repository_prepare_storages (struct repository_t *repository, struct switch_to_serving_context_t *context)
{
    struct singleton_storage_node_t *singleton_storage_node =
        (struct singleton_storage_node_t *) repository->singleton_storages.items.first;

    while (singleton_storage_node)
    {
        struct singleton_switch_to_serving_user_data_t *user_data =
            (struct singleton_switch_to_serving_user_data_t *) kan_stack_group_allocator_allocate (
                &context->allocator, sizeof (struct singleton_switch_to_serving_user_data_t),
                _Alignof (struct singleton_switch_to_serving_user_data_t));

        *user_data = (struct singleton_switch_to_serving_user_data_t) {
            .storage = singleton_storage_node,
            .repository = repository,
        };

        struct kan_cpu_task_list_node_t *node = (struct kan_cpu_task_list_node_t *) kan_stack_group_allocator_allocate (
            &context->allocator, sizeof (struct kan_cpu_task_list_node_t), _Alignof (struct kan_cpu_task_list_node_t));

        node->queue = KAN_CPU_DISPATCH_QUEUE_FOREGROUND;
        ensure_interned_strings_ready ();

        node->task = (struct kan_cpu_task_t) {
            .name = switch_to_serving_task_name,
            .function = prepare_singleton_storage,
            .user_data = (uint64_t) user_data,
        };

        node->next = context->task_list;
        context->task_list = node;
        singleton_storage_node = (struct singleton_storage_node_t *) singleton_storage_node->node.list_node.next;
    }

    struct indexed_storage_node_t *indexed_storage_node =
        (struct indexed_storage_node_t *) repository->indexed_storages.items.first;

    while (indexed_storage_node)
    {
        struct indexed_switch_to_serving_user_data_t *user_data =
            (struct indexed_switch_to_serving_user_data_t *) kan_stack_group_allocator_allocate (
                &context->allocator, sizeof (struct indexed_switch_to_serving_user_data_t),
                _Alignof (struct indexed_switch_to_serving_user_data_t));

        *user_data = (struct indexed_switch_to_serving_user_data_t) {
            .storage = indexed_storage_node,
            .repository = repository,
        };

        struct kan_cpu_task_list_node_t *node = (struct kan_cpu_task_list_node_t *) kan_stack_group_allocator_allocate (
            &context->allocator, sizeof (struct kan_cpu_task_list_node_t), _Alignof (struct kan_cpu_task_list_node_t));

        node->queue = KAN_CPU_DISPATCH_QUEUE_FOREGROUND;
        ensure_interned_strings_ready ();

        node->task = (struct kan_cpu_task_t) {
            .name = switch_to_serving_task_name,
            .function = prepare_indexed_storage,
            .user_data = (uint64_t) user_data,
        };

        node->next = context->task_list;
        context->task_list = node;
        indexed_storage_node = (struct indexed_storage_node_t *) indexed_storage_node->node.list_node.next;
    }

    repository = repository->first;
    while (repository)
    {
        repository_prepare_storages (repository, context);
        repository = repository->next;
    }
}

static void repository_complete_switch_to_serving (struct repository_t *repository)
{
    repository->mode = REPOSITORY_MODE_SERVING;
    repository = repository->first;

    while (repository)
    {
        repository_complete_switch_to_serving (repository);
        repository = repository->next;
    }
}

void kan_repository_enter_serving_mode (kan_repository_t root_repository)
{
    struct repository_t *repository = (struct repository_t *) root_repository;
    KAN_ASSERT (!repository->parent)
    KAN_ASSERT (repository->mode == REPOSITORY_MODE_PLANNING)
    repository_clean_storages (repository);

    kan_cpu_job_t job = kan_cpu_job_create ();
    KAN_ASSERT (job != KAN_INVALID_CPU_JOB)

    struct switch_to_serving_context_t context;
    kan_stack_group_allocator_init (&context.allocator,
                                    kan_allocation_group_get_child (repository->allocation_group, "switch_to_serving"),
                                    KAN_REPOSITORY_SWITCH_TO_SERVING_STACK_INITIAL_SIZE);

    context.task_list = NULL;
    repository_prepare_storages (repository, &context);

    kan_cpu_job_dispatch_and_detach_task_list (job, context.task_list);
    kan_cpu_job_release (job);
    kan_cpu_job_wait (job);

    kan_stack_group_allocator_shutdown (&context.allocator);
    repository_complete_switch_to_serving (repository);
}

static void repository_destroy_internal (struct repository_t *repository)
{
    KAN_ASSERT (repository->mode == REPOSITORY_MODE_PLANNING)
    while (repository->first)
    {
        repository_destroy_internal (repository->first);
    }

    while (repository->singleton_storages.items.first)
    {
        singleton_storage_node_shutdown_and_free (
            (struct singleton_storage_node_t *) repository->singleton_storages.items.first, repository);
    }

    while (repository->event_storages.items.first)
    {
        event_storage_node_shutdown_and_free ((struct event_storage_node_t *) repository->event_storages.items.first,
                                              repository);
    }

    kan_hash_storage_shutdown (&repository->singleton_storages);
    kan_hash_storage_shutdown (&repository->event_storages);

    if (repository->parent)
    {
        if (repository->parent->first == repository)
        {
            repository->parent->first = repository->next;
        }
        else
        {
            struct repository_t *sibling = repository->parent->first;
            while (sibling->next != repository)
            {
                sibling = sibling->next;
                KAN_ASSERT (sibling)
            }

            sibling->next = repository->next;
        }
    }
}

void kan_repository_destroy (kan_repository_t repository)
{
    struct repository_t *repository_data = (struct repository_t *) repository;
    repository_destroy_internal (repository_data);
}
