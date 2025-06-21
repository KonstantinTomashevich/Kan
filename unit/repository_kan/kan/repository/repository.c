#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <memory.h>
#include <qsort.h>
#include <stddef.h>
#include <string.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/api_common/type_punning.h>
#include <kan/container/avl_tree.h>
#include <kan/container/event_queue.h>
#include <kan/container/hash_storage.h>
#include <kan/container/list.h>
#include <kan/container/space_tree.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/cpu_dispatch/job.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/repository/meta.h>
#include <kan/repository/repository.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (repository);

/// \brief Integer for masks inside repository.
/// \details Always 64 bit as observation support depends on it.
typedef uint64_t kan_repository_mask_t;

/// \brief Biggest unsigned integer supported by repository for indexing.
typedef kan_memory_size_t kan_repository_indexed_unsigned_t;

/// \brief Biggest signed integer supported by repository for indexing.
typedef kan_memory_offset_t kan_repository_indexed_signed_t;

_Static_assert (sizeof (kan_repository_indexed_unsigned_t) >= sizeof (kan_hash_t), "Can index hashes.");
_Static_assert (sizeof (kan_repository_indexed_unsigned_t) >= sizeof (kan_repository_signal_value_t),
                "Can index signals.");
_Static_assert (sizeof (kan_repository_indexed_signed_t) >= sizeof (kan_avl_tree_value_t),
                "Can index items that fit into avl tree.");
_Static_assert (sizeof (kan_repository_indexed_floating_t) == sizeof (kan_space_tree_floating_t),
                "Can index items that fit into space tree.");

struct interned_field_path_t
{
    kan_instance_size_t length;
    kan_interned_string_t *path;
};

#define OBSERVATION_BUFFER_ALIGNMENT _Alignof (kan_memory_size_t)
#define OBSERVATION_BUFFER_CHUNK_ALIGNMENT _Alignof (kan_memory_size_t)

struct observation_buffer_scenario_chunk_t
{
    kan_instance_size_t source_offset;
    kan_instance_size_t size;
    kan_repository_mask_t flags;
};

struct observation_buffer_scenario_chunk_list_node_t
{
    struct observation_buffer_scenario_chunk_list_node_t *next;
    kan_instance_size_t source_offset;
    kan_instance_size_t size;
    kan_repository_mask_t flags;
};

struct copy_out_t
{
    kan_instance_size_t source_offset;
    kan_instance_size_t target_offset;
    kan_instance_size_t size;
};

struct copy_out_list_node_t
{
    struct copy_out_list_node_t *next;
    kan_instance_size_t source_offset;
    kan_instance_size_t target_offset;
    kan_instance_size_t size;
};

struct observation_buffer_definition_t
{
    kan_instance_size_t buffer_size;
    kan_instance_size_t scenario_chunks_count;
    struct observation_buffer_scenario_chunk_t *scenario_chunks;
};

struct observation_event_trigger_t
{
    kan_repository_mask_t flags;
    struct kan_repository_event_insert_query_t event_insert_query;
    kan_instance_size_t buffer_copy_outs_count;
    kan_instance_size_t record_copy_outs_count;
    struct copy_out_t copy_outs[];
};

struct observation_event_trigger_list_node_t
{
    struct observation_event_trigger_list_node_t *next;
    kan_repository_mask_t flags;
    struct kan_repository_event_insert_query_t event_insert_query;
    kan_instance_size_t buffer_copy_outs_count;
    struct copy_out_list_node_t *buffer_copy_outs;
    kan_instance_size_t record_copy_outs_count;
    struct copy_out_list_node_t *record_copy_outs;
};

struct observation_event_triggers_definition_t
{
    kan_instance_size_t event_triggers_count;
    struct observation_event_trigger_t *event_triggers;
};

struct lifetime_event_trigger_t
{
    struct kan_repository_event_insert_query_t event_insert_query;
    kan_instance_size_t copy_outs_count;
    struct copy_out_t copy_outs[];
};

struct lifetime_event_trigger_list_node_t
{
    struct lifetime_event_trigger_list_node_t *next;
    struct kan_repository_event_insert_query_t event_insert_query;
    kan_instance_size_t copy_outs_count;
    struct copy_out_list_node_t *copy_outs;
};

struct lifetime_event_triggers_definition_t
{
    kan_instance_size_t event_triggers_count;
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
    kan_instance_size_t absolute_input_value_offset;
};

struct cascade_deleter_node_t
{
    struct cascade_deleter_node_t *next;
    struct kan_repository_indexed_value_delete_query_t query;
    kan_instance_size_t absolute_input_value_offset;
};

struct cascade_deleters_definition_t
{
    kan_instance_size_t cascade_deleters_count;
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

    kan_bool_t scheduled_for_destroy;
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
    kan_repository_mask_t observation_comparison_flags;

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

    kan_bool_t scheduled_for_destroy;
    kan_bool_t candidate_for_cleanup;
};

struct indexed_field_baked_data_t
{
    uint32_t absolute_offset;
    uint16_t offset_in_buffer;
    uint8_t size;
    uint8_t size_with_padding;
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

    struct indexed_field_baked_data_t baked;
    kan_repository_mask_t observation_flags;

    struct kan_hash_storage_t hash_storage;
    struct interned_field_path_t source_path;
};

struct signal_index_node_t
{
    struct signal_index_node_t *previous;
    struct signal_index_node_t *next;
    struct indexed_storage_record_node_t *record;
};

struct signal_index_t
{
    struct signal_index_t *next;
    struct indexed_storage_node_t *storage;
    struct kan_atomic_int_t queries_count;

    kan_repository_signal_value_t signal_value;
    struct indexed_field_baked_data_t baked;
    kan_repository_mask_t observation_flags;

    struct signal_index_node_t *first_node;
    kan_bool_t initial_fill_executed;
    struct interned_field_path_t source_path;
};

struct interval_index_sub_node_t
{
    struct interval_index_sub_node_t *next;
    struct interval_index_sub_node_t *previous;
    struct indexed_storage_record_node_t *record;
};

struct interval_index_node_t
{
    struct kan_avl_tree_node_t node;
    struct interval_index_sub_node_t *first_sub_node;
};

struct interval_index_t
{
    struct interval_index_t *next;
    struct indexed_storage_node_t *storage;
    struct kan_atomic_int_t queries_count;

    struct indexed_field_baked_data_t baked;
    enum kan_reflection_archetype_t baked_archetype;
    kan_repository_mask_t observation_flags;

    struct kan_avl_tree_t tree;
    struct interned_field_path_t source_path;
};

struct space_index_sub_node_t
{
    struct kan_space_tree_quantized_path_t object_min;
    struct kan_space_tree_quantized_path_t object_max;
    struct indexed_storage_record_node_t *record;
};

struct space_index_t
{
    struct space_index_t *next;
    struct indexed_storage_node_t *storage;
    struct kan_atomic_int_t queries_count;

    struct indexed_field_baked_data_t baked_min;
    struct indexed_field_baked_data_t baked_max;
    enum kan_reflection_archetype_t baked_archetype;
    kan_instance_size_t baked_dimension_count;
    kan_repository_mask_t observation_flags;

    struct kan_space_tree_t tree;
    kan_bool_t initial_fill_executed;
    struct interned_field_path_t source_path_min;
    struct interned_field_path_t source_path_max;
    kan_repository_indexed_floating_t source_global_min;
    kan_repository_indexed_floating_t source_global_max;
    kan_repository_indexed_floating_t source_leaf_size;
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

#define ASSERT_SIZE_FOR_INDEXED_STORAGE(QUERY_TYPE)                                                                    \
    _Static_assert (sizeof (struct indexed_##QUERY_TYPE##_query_t) <=                                                  \
                        sizeof (struct kan_repository_indexed_##QUERY_TYPE##_read_query_t),                            \
                    "Query sizes match.");                                                                             \
    _Static_assert (_Alignof (struct indexed_##QUERY_TYPE##_query_t) <=                                                \
                        _Alignof (struct kan_repository_indexed_##QUERY_TYPE##_read_query_t),                          \
                    "Query alignments match.");                                                                        \
                                                                                                                       \
    _Static_assert (sizeof (struct indexed_##QUERY_TYPE##_query_t) <=                                                  \
                        sizeof (struct kan_repository_indexed_##QUERY_TYPE##_update_query_t),                          \
                    "Query sizes match.");                                                                             \
    _Static_assert (_Alignof (struct indexed_##QUERY_TYPE##_query_t) <=                                                \
                        _Alignof (struct kan_repository_indexed_##QUERY_TYPE##_update_query_t),                        \
                    "Query alignments match.");                                                                        \
                                                                                                                       \
    _Static_assert (sizeof (struct indexed_##QUERY_TYPE##_query_t) <=                                                  \
                        sizeof (struct kan_repository_indexed_##QUERY_TYPE##_delete_query_t),                          \
                    "Query sizes match.");                                                                             \
    _Static_assert (_Alignof (struct indexed_##QUERY_TYPE##_query_t) <=                                                \
                        _Alignof (struct kan_repository_indexed_##QUERY_TYPE##_delete_query_t),                        \
                    "Query alignments match.");                                                                        \
                                                                                                                       \
    _Static_assert (sizeof (struct indexed_##QUERY_TYPE##_query_t) <=                                                  \
                        sizeof (struct kan_repository_indexed_##QUERY_TYPE##_write_query_t),                           \
                    "Query sizes match.");                                                                             \
    _Static_assert (_Alignof (struct indexed_##QUERY_TYPE##_query_t) <=                                                \
                        _Alignof (struct kan_repository_indexed_##QUERY_TYPE##_write_query_t),                         \
                    "Query alignments match.")

ASSERT_SIZE_FOR_INDEXED_STORAGE (sequence);

struct indexed_sequence_cursor_t
{
    struct indexed_storage_node_t *storage;
    struct indexed_storage_record_node_t *node;
};

#define ASSERT_CURSOR_FOR_INDEXED_STORAGE(QUERY_TYPE)                                                                  \
    _Static_assert (sizeof (struct indexed_##QUERY_TYPE##_cursor_t) <=                                                 \
                        sizeof (struct kan_repository_indexed_##QUERY_TYPE##_read_cursor_t),                           \
                    "Query sizes match.");                                                                             \
    _Static_assert (_Alignof (struct indexed_##QUERY_TYPE##_cursor_t) <=                                               \
                        _Alignof (struct kan_repository_indexed_##QUERY_TYPE##_read_cursor_t),                         \
                    "Query alignments match.");                                                                        \
                                                                                                                       \
    _Static_assert (sizeof (struct indexed_##QUERY_TYPE##_cursor_t) <=                                                 \
                        sizeof (struct kan_repository_indexed_##QUERY_TYPE##_update_cursor_t),                         \
                    "Query sizes match.");                                                                             \
    _Static_assert (_Alignof (struct indexed_##QUERY_TYPE##_cursor_t) <=                                               \
                        _Alignof (struct kan_repository_indexed_##QUERY_TYPE##_update_cursor_t),                       \
                    "Query alignments match.");                                                                        \
                                                                                                                       \
    _Static_assert (sizeof (struct indexed_##QUERY_TYPE##_cursor_t) <=                                                 \
                        sizeof (struct kan_repository_indexed_##QUERY_TYPE##_delete_cursor_t),                         \
                    "Query sizes match.");                                                                             \
    _Static_assert (_Alignof (struct indexed_##QUERY_TYPE##_cursor_t) <=                                               \
                        _Alignof (struct kan_repository_indexed_##QUERY_TYPE##_delete_cursor_t),                       \
                    "Query alignments match.");                                                                        \
                                                                                                                       \
    _Static_assert (sizeof (struct indexed_##QUERY_TYPE##_cursor_t) <=                                                 \
                        sizeof (struct kan_repository_indexed_##QUERY_TYPE##_write_cursor_t),                          \
                    "Query sizes match.");                                                                             \
    _Static_assert (_Alignof (struct indexed_##QUERY_TYPE##_cursor_t) <=                                               \
                        _Alignof (struct kan_repository_indexed_##QUERY_TYPE##_write_cursor_t),                        \
                    "Query alignments match.")

ASSERT_CURSOR_FOR_INDEXED_STORAGE (sequence);

struct indexed_sequence_constant_access_t
{
    struct indexed_storage_node_t *storage;
    struct indexed_storage_record_node_t *node;
};

#define ASSERT_CONSTANT_ACCESS_FOR_INDEXED_STORAGE(QUERY_TYPE)                                                         \
    _Static_assert (sizeof (struct indexed_##QUERY_TYPE##_constant_access_t) <=                                        \
                        sizeof (struct kan_repository_indexed_##QUERY_TYPE##_read_access_t),                           \
                    "Query sizes match.");                                                                             \
    _Static_assert (_Alignof (struct indexed_##QUERY_TYPE##_constant_access_t) <=                                      \
                        _Alignof (struct kan_repository_indexed_##QUERY_TYPE##_read_access_t),                         \
                    "Query alignments match.");                                                                        \
                                                                                                                       \
    _Static_assert (sizeof (struct indexed_##QUERY_TYPE##_constant_access_t) <=                                        \
                        sizeof (struct kan_repository_indexed_##QUERY_TYPE##_delete_access_t),                         \
                    "Query sizes match.");                                                                             \
    _Static_assert (_Alignof (struct indexed_##QUERY_TYPE##_constant_access_t) <=                                      \
                        _Alignof (struct kan_repository_indexed_##QUERY_TYPE##_delete_access_t),                       \
                    "Query alignments match.")

ASSERT_CONSTANT_ACCESS_FOR_INDEXED_STORAGE (sequence);

struct indexed_sequence_mutable_access_t
{
    struct indexed_storage_node_t *storage;
    struct indexed_storage_dirty_record_node_t *dirty_node;
};

#define ASSERT_MUTABLE_ACCESS_FOR_INDEXED_STORAGE(QUERY_TYPE)                                                          \
    _Static_assert (sizeof (struct indexed_##QUERY_TYPE##_mutable_access_t) <=                                         \
                        sizeof (struct kan_repository_indexed_##QUERY_TYPE##_update_access_t),                         \
                    "Query sizes match.");                                                                             \
    _Static_assert (_Alignof (struct indexed_##QUERY_TYPE##_mutable_access_t) <=                                       \
                        _Alignof (struct kan_repository_indexed_##QUERY_TYPE##_update_access_t),                       \
                    "Query alignments match.");                                                                        \
                                                                                                                       \
    _Static_assert (sizeof (struct indexed_##QUERY_TYPE##_mutable_access_t) <=                                         \
                        sizeof (struct kan_repository_indexed_##QUERY_TYPE##_write_access_t),                          \
                    "Query sizes match.");                                                                             \
    _Static_assert (_Alignof (struct indexed_##QUERY_TYPE##_mutable_access_t) <=                                       \
                        _Alignof (struct kan_repository_indexed_##QUERY_TYPE##_write_access_t),                        \
                    "Query alignments match.")

ASSERT_MUTABLE_ACCESS_FOR_INDEXED_STORAGE (sequence);

struct indexed_value_query_t
{
    struct value_index_t *index;
};

ASSERT_SIZE_FOR_INDEXED_STORAGE (value);

struct indexed_value_cursor_t
{
    struct value_index_t *index;
    struct value_index_node_t *node;
    struct value_index_sub_node_t *sub_node;
};

ASSERT_CURSOR_FOR_INDEXED_STORAGE (value);

struct indexed_value_constant_access_t
{
    struct value_index_t *index;
    struct value_index_node_t *node;
    struct value_index_sub_node_t *sub_node;
};

ASSERT_CONSTANT_ACCESS_FOR_INDEXED_STORAGE (value);

struct indexed_value_mutable_access_t
{
    struct value_index_t *index;
    struct value_index_node_t *node;
    struct value_index_sub_node_t *sub_node;
    struct indexed_storage_dirty_record_node_t *dirty_node;
};

ASSERT_MUTABLE_ACCESS_FOR_INDEXED_STORAGE (value);

struct indexed_signal_query_t
{
    struct signal_index_t *index;
};

ASSERT_SIZE_FOR_INDEXED_STORAGE (signal);

struct indexed_signal_cursor_t
{
    struct signal_index_t *index;
    struct signal_index_node_t *node;
};

ASSERT_CURSOR_FOR_INDEXED_STORAGE (signal);

struct indexed_signal_constant_access_t
{
    struct signal_index_t *index;
    struct signal_index_node_t *node;
};

ASSERT_CONSTANT_ACCESS_FOR_INDEXED_STORAGE (signal);

struct indexed_signal_mutable_access_t
{
    struct signal_index_t *index;
    struct signal_index_node_t *node;
    struct indexed_storage_dirty_record_node_t *dirty_node;
};

ASSERT_MUTABLE_ACCESS_FOR_INDEXED_STORAGE (signal);

struct indexed_interval_query_t
{
    struct interval_index_t *index;
};

ASSERT_SIZE_FOR_INDEXED_STORAGE (interval);

struct indexed_interval_cursor_t
{
    struct interval_index_t *index;
    struct interval_index_node_t *current_node;
    struct interval_index_node_t *end_node;
    struct interval_index_sub_node_t *sub_node;
    kan_repository_indexed_floating_t min_floating;
    kan_repository_indexed_floating_t max_floating;
};

_Static_assert (sizeof (struct indexed_interval_cursor_t) <=
                    sizeof (struct kan_repository_indexed_interval_ascending_read_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_interval_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_interval_ascending_read_cursor_t),
                "Query alignments match.");
_Static_assert (sizeof (struct indexed_interval_cursor_t) <=
                    sizeof (struct kan_repository_indexed_interval_ascending_update_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_interval_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_interval_ascending_update_cursor_t),
                "Query alignments match.");
_Static_assert (sizeof (struct indexed_interval_cursor_t) <=
                    sizeof (struct kan_repository_indexed_interval_ascending_delete_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_interval_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_interval_ascending_delete_cursor_t),
                "Query alignments match.");
_Static_assert (sizeof (struct indexed_interval_cursor_t) <=
                    sizeof (struct kan_repository_indexed_interval_ascending_write_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_interval_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_interval_ascending_write_cursor_t),
                "Query alignments match.");
_Static_assert (sizeof (struct indexed_interval_cursor_t) <=
                    sizeof (struct kan_repository_indexed_interval_descending_read_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_interval_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_interval_descending_read_cursor_t),
                "Query alignments match.");
_Static_assert (sizeof (struct indexed_interval_cursor_t) <=
                    sizeof (struct kan_repository_indexed_interval_descending_update_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_interval_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_interval_descending_update_cursor_t),
                "Query alignments match.");
_Static_assert (sizeof (struct indexed_interval_cursor_t) <=
                    sizeof (struct kan_repository_indexed_interval_descending_delete_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_interval_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_interval_descending_delete_cursor_t),
                "Query alignments match.");
_Static_assert (sizeof (struct indexed_interval_cursor_t) <=
                    sizeof (struct kan_repository_indexed_interval_descending_write_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_interval_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_interval_descending_write_cursor_t),
                "Query alignments match.");

struct indexed_interval_constant_access_t
{
    struct interval_index_t *index;
    struct interval_index_node_t *node;
    struct interval_index_sub_node_t *sub_node;
};

ASSERT_CONSTANT_ACCESS_FOR_INDEXED_STORAGE (interval);

struct indexed_interval_mutable_access_t
{
    struct interval_index_t *index;
    struct interval_index_node_t *node;
    struct interval_index_sub_node_t *sub_node;
    struct indexed_storage_dirty_record_node_t *dirty_node;
};

ASSERT_MUTABLE_ACCESS_FOR_INDEXED_STORAGE (interval);

struct indexed_space_query_t
{
    struct space_index_t *index;
};

ASSERT_SIZE_FOR_INDEXED_STORAGE (space);

struct indexed_space_shape_cursor_t
{
    struct space_index_t *index;
    struct kan_space_tree_shape_iterator_t iterator;
    kan_instance_size_t current_sub_node_index;
    kan_repository_indexed_floating_t min[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    kan_repository_indexed_floating_t max[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
};

_Static_assert (sizeof (struct indexed_space_shape_cursor_t) <=
                    sizeof (struct kan_repository_indexed_space_shape_read_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_space_shape_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_space_shape_read_cursor_t),
                "Query alignments match.");
_Static_assert (sizeof (struct indexed_space_shape_cursor_t) <=
                    sizeof (struct kan_repository_indexed_space_shape_update_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_space_shape_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_space_shape_update_cursor_t),
                "Query alignments match.");
_Static_assert (sizeof (struct indexed_space_shape_cursor_t) <=
                    sizeof (struct kan_repository_indexed_space_shape_delete_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_space_shape_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_space_shape_delete_cursor_t),
                "Query alignments match.");
_Static_assert (sizeof (struct indexed_space_shape_cursor_t) <=
                    sizeof (struct kan_repository_indexed_space_shape_write_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_space_shape_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_space_shape_write_cursor_t),
                "Query alignments match.");

struct indexed_space_ray_cursor_t
{
    struct space_index_t *index;
    struct kan_space_tree_ray_iterator_t iterator;
    kan_instance_size_t current_sub_node_index;
    kan_repository_indexed_floating_t max_time;
    kan_repository_indexed_floating_t origin[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    kan_repository_indexed_floating_t direction[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
};

_Static_assert (sizeof (struct indexed_space_ray_cursor_t) <=
                    sizeof (struct kan_repository_indexed_space_ray_read_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_space_ray_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_space_ray_read_cursor_t),
                "Query alignments match.");
_Static_assert (sizeof (struct indexed_space_ray_cursor_t) <=
                    sizeof (struct kan_repository_indexed_space_ray_update_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_space_ray_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_space_ray_update_cursor_t),
                "Query alignments match.");
_Static_assert (sizeof (struct indexed_space_ray_cursor_t) <=
                    sizeof (struct kan_repository_indexed_space_ray_delete_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_space_ray_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_space_ray_delete_cursor_t),
                "Query alignments match.");
_Static_assert (sizeof (struct indexed_space_ray_cursor_t) <=
                    sizeof (struct kan_repository_indexed_space_ray_write_cursor_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct indexed_space_ray_cursor_t) <=
                    _Alignof (struct kan_repository_indexed_space_ray_write_cursor_t),
                "Query alignments match.");

struct indexed_space_constant_access_t
{
    struct space_index_t *index;
    struct kan_space_tree_node_t *node;
    struct space_index_sub_node_t *sub_node;
};

ASSERT_CONSTANT_ACCESS_FOR_INDEXED_STORAGE (space);

struct indexed_space_mutable_access_t
{
    struct space_index_t *index;
    struct kan_space_tree_node_t *node;
    struct space_index_sub_node_t *sub_node;
    struct indexed_storage_dirty_record_node_t *dirty_node;
};

ASSERT_MUTABLE_ACCESS_FOR_INDEXED_STORAGE (space);

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

    kan_bool_t scheduled_for_destroy;
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

struct type_name_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t type_name;
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
    kan_bool_t scheduled_for_destroy;

    struct kan_hash_storage_t singleton_storages;
    struct kan_hash_storage_t indexed_storages;
    struct kan_hash_storage_t event_storages;

    /// \details Event storages for automatic events might need to be uplifted from children repositories to
    ///          parent repositories in order to function properly. Uplift happens when event storage is created
    ///          and not after that -- top-down storage creation rule must be followed (which is true for the repository
    ///          storage creation in general). We cannot create event storages for automatic events right after their
    ///          source type storage creation, because events might not be consumed at all.
    struct kan_hash_storage_t event_types_to_uplift;

#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
    /// \details If type is cascade deleted, its storages cannot be created in child repositories as they would be
    ///          invisible to the repository with cascade deletion source type. Therefore, we need to validate that
    ///          it is not happening.
    struct kan_hash_storage_t cascade_deleted_indexed_types;
#endif
};

struct migration_context_t
{
    struct kan_stack_group_allocator_t allocator;
    struct kan_cpu_task_list_node_t *task_list;
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

static kan_bool_t statics_initialized = KAN_FALSE;
static struct kan_atomic_int_t interned_strings_initialization_lock = {.value = 0};
static kan_interned_string_t meta_automatic_on_change_event_name;
static kan_interned_string_t meta_automatic_on_insert_event_name;
static kan_interned_string_t meta_automatic_on_delete_event_name;
static kan_interned_string_t meta_automatic_cascade_deletion_name;

static kan_cpu_section_t migration_task_section;
static kan_cpu_section_t switch_to_serving_task_section;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        kan_atomic_int_lock (&interned_strings_initialization_lock);
        if (!statics_initialized)
        {
            meta_automatic_on_change_event_name = kan_string_intern ("kan_repository_meta_automatic_on_change_event_t");
            meta_automatic_on_insert_event_name = kan_string_intern ("kan_repository_meta_automatic_on_insert_event_t");
            meta_automatic_on_delete_event_name = kan_string_intern ("kan_repository_meta_automatic_on_delete_event_t");
            meta_automatic_cascade_deletion_name =
                kan_string_intern ("kan_repository_meta_automatic_cascade_deletion_t");

            migration_task_section = kan_cpu_section_get ("repository_migration");
            switch_to_serving_task_section = kan_cpu_section_get ("repository_switch_to_serving");
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
                     singleton_storage->type->name)
            return KAN_FALSE;
        }

        if (old_value > 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Singleton type \"%s\". Unable to create write access because other write access detected.",
                     singleton_storage->type->name)
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
                     singleton_storage->type->name)
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
                     storage->type->name)
            return KAN_FALSE;
        }

        if (old_value > 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Indexed type \"%s\". Unable to create update/delete/write access because update/delete/write "
                     "access to the same record detected.",
                     storage->type->name)
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
                     storage->type->name)
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
                     event_storage->type->name)
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
                     event_storage->type->name)
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
    case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
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

        for (kan_loop_size_t field_index = 0u; field_index < reflection_struct->fields_count; ++field_index)
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

static kan_bool_t validation_single_value_index_is_possible (const struct kan_reflection_field_t *field,
                                                             kan_bool_t allow_not_comparable)
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
    case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
    case KAN_REFLECTION_ARCHETYPE_ENUM:
        if (field->size > sizeof (uint64_t))
        {
            KAN_LOG (
                repository_validation, KAN_LOG_ERROR,
                "Passed field \"%s\" to single value index with supported archetype, but suspicious size %lu. Broken "
                "reflection?",
                field->name, (unsigned long) field->size)
            return KAN_FALSE;
        }

        if (!allow_not_comparable && field->archetype != KAN_REFLECTION_ARCHETYPE_SIGNED_INT &&
            field->archetype != KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT &&
            field->archetype != KAN_REFLECTION_ARCHETYPE_FLOATING)
        {
            KAN_LOG (repository_validation, KAN_LOG_ERROR,
                     "Passed field \"%s\" to single value index with unsupported archetype.", field->name)
            return KAN_FALSE;
        }

        if (field->size > sizeof (kan_repository_indexed_unsigned_t))
        {
            KAN_LOG (repository, KAN_LOG_ERROR,
                     "Passed field \"%s\" is too big to be indexed on this platform. Field size is %u which is bigger "
                     "than platform supported %u):",
                     field->name, (unsigned) field->size, (unsigned) sizeof (kan_repository_indexed_unsigned_t))
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
        KAN_LOG (repository_validation, KAN_LOG_ERROR,
                 "Passed field \"%s\" to single value index with unsupported archetype.", field->name)
        return KAN_FALSE;
    }

    return KAN_FALSE;
}

static kan_bool_t validation_multi_value_index_is_possible (const struct kan_reflection_field_t *field)
{
    if (field->visibility_condition_values_count > 0u)
    {
        KAN_LOG (repository_validation, KAN_LOG_ERROR,
                 "Passed field \"%s\" has visibility conditions. Fields with visibility conditions cannot be indexed.",
                 field->name)
        return KAN_FALSE;
    }

    if (field->archetype != KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY)
    {
        KAN_LOG (repository_validation, KAN_LOG_ERROR,
                 "Passed field \"%s\" to multi value index that is not an inline array.", field->name)
    }

    switch (field->archetype_inline_array.item_archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_FLOATING:
    case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
        if (field->archetype_inline_array.item_size > sizeof (uint64_t))
        {
            KAN_LOG (repository_validation, KAN_LOG_ERROR,
                     "Passed field \"%s\" to multi value index with supported archetype, but suspicious item size %lu. "
                     "Broken reflection?",
                     field->name, (unsigned long) field->archetype_inline_array.item_size)
            return KAN_FALSE;
        }

        if (field->archetype_inline_array.item_size > sizeof (kan_repository_indexed_unsigned_t))
        {
            KAN_LOG (repository, KAN_LOG_ERROR,
                     "Passed field \"%s\" is too big to be indexed on this platform. Field size is %u which is bigger "
                     "than platform supported %u):",
                     field->name, (unsigned) field->size, (unsigned) sizeof (kan_repository_indexed_unsigned_t))
            return KAN_FALSE;
        }

        return KAN_TRUE;

    case KAN_REFLECTION_ARCHETYPE_INTERNED_STRING:
    case KAN_REFLECTION_ARCHETYPE_ENUM:
    case KAN_REFLECTION_ARCHETYPE_STRING_POINTER:
    case KAN_REFLECTION_ARCHETYPE_EXTERNAL_POINTER:
    case KAN_REFLECTION_ARCHETYPE_STRUCT:
    case KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER:
    case KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY:
    case KAN_REFLECTION_ARCHETYPE_DYNAMIC_ARRAY:
    case KAN_REFLECTION_ARCHETYPE_PATCH:
        KAN_LOG (repository_validation, KAN_LOG_ERROR,
                 "Passed field \"%s\" to multi value index with unsupported item archetype.", field->name)
        return KAN_FALSE;
    }

    return KAN_FALSE;
}

static inline kan_bool_t check_baked_data_intersects (struct indexed_field_baked_data_t *first,
                                                      struct indexed_field_baked_data_t *second)
{
    return second->absolute_offset + second->size_with_padding >= first->absolute_offset &&
           first->absolute_offset + first->size_with_padding >= second->absolute_offset;
}

static kan_bool_t validation_single_value_index_does_not_intersect_with_multi_value (
    struct indexed_storage_node_t *storage, struct indexed_field_baked_data_t *new_index_data)
{
    struct space_index_t *space_index = storage->first_space_index;
    while (space_index)
    {
        if (check_baked_data_intersects (&space_index->baked_min, new_index_data))
        {
            KAN_LOG (repository_validation, KAN_LOG_ERROR,
                     "Passed field to single value index intersects with multi value index fields.")
            return KAN_FALSE;
        }

        if (check_baked_data_intersects (&space_index->baked_max, new_index_data))
        {
            KAN_LOG (repository_validation, KAN_LOG_ERROR,
                     "Passed field to single value index intersects with multi value index fields.")
            return KAN_FALSE;
        }

        space_index = space_index->next;
    }

    return KAN_TRUE;
}

static kan_bool_t validation_multi_value_index_does_not_intersect_with_single_value (
    struct indexed_storage_node_t *storage, struct indexed_field_baked_data_t *new_index_data)
{
    struct value_index_t *value_index = storage->first_value_index;
    while (value_index)
    {
        if (check_baked_data_intersects (&value_index->baked, new_index_data))
        {
            KAN_LOG (repository_validation, KAN_LOG_ERROR,
                     "Passed field to multi value index intersects with single value index fields.")
            return KAN_FALSE;
        }

        value_index = value_index->next;
    }

    struct signal_index_t *signal_index = storage->first_signal_index;
    while (signal_index)
    {
        if (check_baked_data_intersects (&signal_index->baked, new_index_data))
        {
            KAN_LOG (repository_validation, KAN_LOG_ERROR,
                     "Passed field to multi signal index intersects with single signal index fields.")
            return KAN_FALSE;
        }

        signal_index = signal_index->next;
    }

    struct interval_index_t *interval_index = storage->first_interval_index;
    while (interval_index)
    {
        if (check_baked_data_intersects (&interval_index->baked, new_index_data))
        {
            KAN_LOG (repository_validation, KAN_LOG_ERROR,
                     "Passed field to multi interval index intersects with single interval index fields.")
            return KAN_FALSE;
        }

        interval_index = interval_index->next;
    }

    return KAN_TRUE;
}
#endif

static struct interned_field_path_t copy_field_path (struct kan_repository_field_path_t input,
                                                     kan_allocation_group_t allocation_group)
{
    struct interned_field_path_t result;
    result.length = input.reflection_path_length;
    result.path = kan_allocate_batched (allocation_group, sizeof (kan_interned_string_t) * result.length);

    for (kan_loop_size_t index = 0u; index < input.reflection_path_length; ++index)
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

    for (kan_loop_size_t index = 0u; index < input.reflection_path_length; ++index)
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

static void apply_copy_outs (kan_instance_size_t copy_outs_count,
                             struct copy_out_t *copy_outs,
                             const void *source,
                             void *target)
{
    for (kan_loop_size_t index = 0u; index < copy_outs_count; ++index)
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
    kan_instance_size_t *output_absolute_offset,
    kan_instance_size_t *output_size_with_padding)
{
    kan_interned_string_t *interned_path = kan_stack_group_allocator_allocate (
        temporary_allocator, sizeof (kan_interned_string_t) * path->reflection_path_length,
        _Alignof (kan_interned_string_t));

    for (kan_loop_size_t index = 0u; index < path->reflection_path_length; ++index)
    {
        interned_path[index] = kan_string_intern (path->reflection_path[index]);
    }

    const struct kan_reflection_field_t *field =
        kan_reflection_registry_query_local_field (registry, struct_name, path->reflection_path_length, interned_path,
                                                   output_absolute_offset, output_size_with_padding);

    if (!field)
    {
        KAN_LOG (repository, KAN_LOG_ERROR,
                 "Unable to query field from struct \"%s\" for automatic event from path: it is either non-local or it "
                 "does not exist. Path:",
                 struct_name)

        for (kan_loop_size_t path_element_index = 0u; path_element_index < path->reflection_path_length;
             ++path_element_index)
        {
            KAN_LOG (repository, KAN_LOG_ERROR, "    - \"%s\"", path->reflection_path[path_element_index])
        }
    }

    return field;
}

static struct copy_out_list_node_t *extract_raw_copy_outs (kan_interned_string_t source_struct_name,
                                                           kan_interned_string_t target_struct_name,
                                                           kan_instance_size_t copy_outs_count,
                                                           struct kan_repository_copy_out_t *copy_outs,
                                                           kan_reflection_registry_t registry,
                                                           struct kan_stack_group_allocator_t *temporary_allocator)
{
    struct copy_out_list_node_t *first = NULL;
    struct copy_out_list_node_t *last = NULL;

    for (kan_loop_size_t index = 0u; index < copy_outs_count; ++index)
    {
        struct kan_repository_copy_out_t *copy_out = &copy_outs[index];

        kan_instance_size_t source_absolute_offset;
        kan_instance_size_t source_size_with_padding;
        const struct kan_reflection_field_t *source_field = query_field_for_automatic_event_from_path (
            source_struct_name, &copy_out->source_path, registry, temporary_allocator, &source_absolute_offset,
            &source_size_with_padding);

        kan_instance_size_t target_absolute_offset;
        kan_instance_size_t target_size_with_padding;
        const struct kan_reflection_field_t *target_field = query_field_for_automatic_event_from_path (
            target_struct_name, &copy_out->target_path, registry, temporary_allocator, &target_absolute_offset,
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

        struct copy_out_list_node_t *node =
            KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (temporary_allocator, struct copy_out_list_node_t);

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
        kan_instance_size_t buffer_offset = 0u;
        for (kan_loop_size_t index = 0u; index < buffer->scenario_chunks_count; ++index)
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
                const kan_instance_size_t offset = input->source_offset - chunk->source_offset;
                const kan_instance_size_t size_in_chunk = KAN_MIN (input->size, chunk->size - offset);

                struct copy_out_list_node_t *node =
                    KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (temporary_allocator, struct copy_out_list_node_t);

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

            buffer_offset =
                (kan_instance_size_t) kan_apply_alignment (buffer_offset + chunk->size, OBSERVATION_BUFFER_ALIGNMENT);
        }

        input = input->next;
    }

    return first;
}

static struct copy_out_list_node_t *merge_copy_outs (struct copy_out_list_node_t *input,
                                                     struct kan_stack_group_allocator_t *temporary_allocator,
                                                     kan_instance_size_t *output_count)
{
    if (!input)
    {
        *output_count = 0u;
        return NULL;
    }

    kan_loop_size_t count = 0u;
    struct copy_out_list_node_t *copy_out = input;

    while (copy_out)
    {
        ++count;
        copy_out = copy_out->next;
    }

    struct copy_out_list_node_t **input_array =
        kan_stack_group_allocator_allocate (temporary_allocator, count * sizeof (void *), _Alignof (void *));

    copy_out = input;
    kan_loop_size_t copy_out_index = 0u;

    while (copy_out)
    {
        input_array[copy_out_index] = copy_out;
        ++copy_out_index;
        copy_out = copy_out->next;
    }

    {
#define LESS(first_index, second_index)                                                                                \
    __CUSHION_PRESERVE__ (input_array[first_index]->source_offset < input_array[second_index]->source_offset)

#define SWAP(first_index, second_index)                                                                                \
    __CUSHION_PRESERVE__                                                                                               \
    copy_out = input_array[first_index], input_array[first_index] = input_array[second_index],                         \
    input_array[second_index] = copy_out

        QSORT ((unsigned long) count, LESS, SWAP);
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
            struct copy_out_list_node_t *node =
                KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (temporary_allocator, struct copy_out_list_node_t);

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
    kan_loop_size_t initial_chunks_count = 0u;
    struct observation_buffer_scenario_chunk_list_node_t *chunk = first_chunk;

    while (chunk)
    {
        ++initial_chunks_count;
        chunk = chunk->next;
    }

    struct observation_buffer_scenario_chunk_list_node_t **initial_chunks = kan_stack_group_allocator_allocate (
        temporary_allocator, initial_chunks_count * sizeof (void *), _Alignof (void *));

    chunk = first_chunk;
    kan_loop_size_t chunk_index = 0u;

    while (chunk)
    {
        initial_chunks[chunk_index] = chunk;
        ++chunk_index;
        chunk = chunk->next;
    }

    {
#define LESS(first_index, second_index)                                                                                \
    __CUSHION_PRESERVE__ (                                                                                             \
        initial_chunks[first_index]->source_offset == initial_chunks[second_index]->source_offset ?                    \
            (initial_chunks[first_index]->size < initial_chunks[second_index]->size) :                                 \
            (initial_chunks[first_index]->source_offset < initial_chunks[second_index]->source_offset))

#define SWAP(first_index, second_index)                                                                                \
    __CUSHION_PRESERVE__                                                                                               \
    chunk = initial_chunks[first_index], initial_chunks[first_index] = initial_chunks[second_index],                   \
    initial_chunks[second_index] = chunk

        QSORT ((unsigned long) initial_chunks_count, LESS, SWAP);
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

        kan_instance_size_t no_intersection_size = initial_node->size;
        kan_repository_mask_t no_intersection_flags = initial_node->flags;
        kan_loop_size_t affected_stop_index = initial_chunks_count;

        for (kan_loop_size_t next_index = chunk_index + 1u; next_index < initial_chunks_count; ++next_index)
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
        for (kan_loop_size_t affected_index = chunk_index; affected_index < affected_stop_index; ++affected_index)
        {
            struct observation_buffer_scenario_chunk_list_node_t *affected_node = initial_chunks[affected_index];
            affected_node->size -= no_intersection_size;
            affected_node->source_offset += no_intersection_size;
        }

        struct observation_buffer_scenario_chunk_list_node_t *no_intersection_node =
            KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (temporary_allocator,
                                                      struct observation_buffer_scenario_chunk_list_node_t);

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
    kan_loop_size_t merged_nodes_count = 0u;
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
            struct observation_buffer_scenario_chunk_list_node_t *merged_node =
                KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (temporary_allocator,
                                                          struct observation_buffer_scenario_chunk_list_node_t);

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
        definition->buffer_size = (kan_instance_size_t) kan_apply_alignment (definition->buffer_size + chunk->size,
                                                                             OBSERVATION_BUFFER_ALIGNMENT);

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

    KAN_ASSERT ((kan_instance_size_t) (output - (uint8_t *) observation_buffer_memory) == definition->buffer_size)
}

static kan_repository_mask_t observation_buffer_definition_compare (struct observation_buffer_definition_t *definition,
                                                                    void *observation_buffer_memory,
                                                                    void *record)
{
    if (!observation_buffer_memory)
    {
        return 0u;
    }

    kan_repository_mask_t result = 0u;
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
    const kan_instance_size_t trigger_size =
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
                                                         kan_repository_mask_t *event_flag,
                                                         struct repository_t *repository,
                                                         const struct kan_reflection_struct_t *observed_struct,
                                                         struct observation_buffer_definition_t *buffer,
                                                         struct kan_stack_group_allocator_t *temporary_allocator,
                                                         kan_allocation_group_t result_allocation_group)
{
    struct kan_reflection_struct_meta_iterator_t iterator = kan_reflection_registry_query_struct_meta (
        repository->registry, observed_struct->name, meta_automatic_on_change_event_name);

    struct kan_repository_meta_automatic_on_change_event_t *event =
        (struct kan_repository_meta_automatic_on_change_event_t *) kan_reflection_struct_meta_iterator_get (&iterator);

    kan_loop_size_t triggers_count = 0u;
    kan_instance_size_t triggers_array_size = 0u;
    struct observation_event_trigger_list_node_t *first_event_node = NULL;

    while (event)
    {
        const kan_interned_string_t event_type = kan_string_intern (event->event_type);
        struct event_storage_node_t *event_storage =
            query_event_storage_across_hierarchy (repository, kan_string_intern (event_type));

        if (event_storage)
        {
            struct copy_out_list_node_t *raw_buffer_copy_outs =
                extract_raw_copy_outs (observed_struct->name, event_type, event->unchanged_copy_outs_count,
                                       event->unchanged_copy_outs, repository->registry, temporary_allocator);

            struct copy_out_list_node_t *retargeted_buffer_copy_outs =
                convert_to_buffer_copy_outs (raw_buffer_copy_outs, buffer, temporary_allocator);

            kan_instance_size_t merged_buffer_copy_outs_count;
            struct copy_out_list_node_t *merged_buffer_copy_outs =
                merge_copy_outs (retargeted_buffer_copy_outs, temporary_allocator, &merged_buffer_copy_outs_count);

            struct copy_out_list_node_t *raw_record_copy_outs =
                extract_raw_copy_outs (observed_struct->name, event_type, event->changed_copy_outs_count,
                                       event->changed_copy_outs, repository->registry, temporary_allocator);

            kan_instance_size_t merged_record_copy_outs_count;
            struct copy_out_list_node_t *merged_record_copy_outs =
                merge_copy_outs (raw_record_copy_outs, temporary_allocator, &merged_record_copy_outs_count);

            struct observation_event_trigger_list_node_t *event_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                temporary_allocator, struct observation_event_trigger_list_node_t);

            event_node->flags = *event_flag;
            kan_repository_event_insert_query_init (&event_node->event_insert_query,
                                                    KAN_HANDLE_SET (kan_repository_event_storage_t, event_storage));
            event_node->buffer_copy_outs_count = merged_buffer_copy_outs_count;
            event_node->buffer_copy_outs = merged_buffer_copy_outs;
            event_node->record_copy_outs_count = merged_record_copy_outs_count;
            event_node->record_copy_outs = merged_record_copy_outs;

            event_node->next = first_event_node;
            first_event_node = event_node;

            ++triggers_count;
            triggers_array_size = (kan_instance_size_t) kan_apply_alignment (
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

        kan_loop_size_t copy_out_index = 0u;
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
                                                        kan_repository_mask_t flags,
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
    for (kan_loop_size_t trigger_index = 0u; trigger_index < definition->event_triggers_count; ++trigger_index)
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

        for (kan_loop_size_t trigger_index = 0u; trigger_index < definition->event_triggers_count; ++trigger_index)
        {
            kan_repository_event_insert_query_shutdown (&current_trigger->event_insert_query);
            current_trigger = observation_event_trigger_next (current_trigger);
        }

        kan_instance_size_t triggers_size =
            (kan_instance_size_t) ((uint8_t *) current_trigger - (uint8_t *) first_trigger);
        kan_free_general (allocation_group, definition->event_triggers, triggers_size);
    }
}

static struct lifetime_event_trigger_t *lifetime_event_trigger_next (struct lifetime_event_trigger_t *trigger)
{
    uint8_t *raw_pointer = (uint8_t *) trigger;
    const kan_instance_size_t trigger_size =
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
    kan_instance_size_t *count_output,
    kan_instance_size_t *array_size_output)
{
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
            struct copy_out_list_node_t *raw_copy_outs =
                extract_raw_copy_outs (observed_struct->name, event_type, event->copy_outs_count, event->copy_outs,
                                       repository->registry, temporary_allocator);

            kan_instance_size_t merged_copy_outs_count;
            struct copy_out_list_node_t *merged_copy_outs =
                merge_copy_outs (raw_copy_outs, temporary_allocator, &merged_copy_outs_count);

            struct lifetime_event_trigger_list_node_t *event_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                temporary_allocator, struct lifetime_event_trigger_list_node_t);

            kan_repository_event_insert_query_init (&event_node->event_insert_query,
                                                    KAN_HANDLE_SET (kan_repository_event_storage_t, event_storage));
            event_node->copy_outs_count = merged_copy_outs_count;
            event_node->copy_outs = merged_copy_outs;

            event_node->next = first_event_node;
            first_event_node = event_node;

            ++*count_output;
            *array_size_output = (kan_instance_size_t) kan_apply_alignment (
                *array_size_output + sizeof (struct lifetime_event_trigger_t) +
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
    kan_instance_size_t *count_output,
    kan_instance_size_t *array_size_output)
{
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
            struct copy_out_list_node_t *raw_copy_outs =
                extract_raw_copy_outs (observed_struct->name, event_type, event->copy_outs_count, event->copy_outs,
                                       repository->registry, temporary_allocator);

            kan_instance_size_t merged_copy_outs_count;
            struct copy_out_list_node_t *merged_copy_outs =
                merge_copy_outs (raw_copy_outs, temporary_allocator, &merged_copy_outs_count);

            struct lifetime_event_trigger_list_node_t *event_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                temporary_allocator, struct lifetime_event_trigger_list_node_t);

            kan_repository_event_insert_query_init (&event_node->event_insert_query,
                                                    KAN_HANDLE_SET (kan_repository_event_storage_t, event_storage));
            event_node->copy_outs_count = merged_copy_outs_count;
            event_node->copy_outs = merged_copy_outs;

            event_node->next = first_event_node;
            first_event_node = event_node;

            ++*count_output;
            *array_size_output = (kan_instance_size_t) kan_apply_alignment (
                *array_size_output + sizeof (struct lifetime_event_trigger_t) +
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
    kan_instance_size_t triggers_count = 0u;
    kan_instance_size_t triggers_array_size = 0u;
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

        kan_loop_size_t copy_out_index = 0u;
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

static inline void lifetime_event_trigger_fire (struct lifetime_event_trigger_t *trigger, void *record)
{
    struct kan_repository_event_insertion_package_t package =
        kan_repository_event_insert_query_execute (&trigger->event_insert_query);
    void *event = kan_repository_event_insertion_package_get (&package);

    if (event)
    {
        apply_copy_outs (trigger->copy_outs_count, trigger->copy_outs, record, event);
        kan_repository_event_insertion_package_submit (&package);
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
    for (kan_loop_size_t trigger_index = 0u; trigger_index < definition->event_triggers_count; ++trigger_index)
    {
        lifetime_event_trigger_fire (current_trigger, record);
        if (trigger_index != definition->event_triggers_count - 1u)
        {
            current_trigger = lifetime_event_trigger_next (current_trigger);
        }
    }
}

static void lifetime_event_triggers_definition_fire_with_destroy_check (
    struct lifetime_event_triggers_definition_t *definition, void *record)
{
    if (!definition->event_triggers)
    {
        return;
    }

    KAN_ASSERT (definition->event_triggers_count > 0u)
    KAN_ASSERT (record)

    struct lifetime_event_trigger_t *current_trigger = definition->event_triggers;
    for (kan_loop_size_t trigger_index = 0u; trigger_index < definition->event_triggers_count; ++trigger_index)
    {
        struct event_insert_query_t *query = (struct event_insert_query_t *) &current_trigger->event_insert_query;
        if (!query->storage->scheduled_for_destroy)
        {
            lifetime_event_trigger_fire (current_trigger, record);
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

        for (kan_loop_size_t trigger_index = 0u; trigger_index < definition->event_triggers_count; ++trigger_index)
        {
            kan_repository_event_insert_query_shutdown (&current_trigger->event_insert_query);
            current_trigger = lifetime_event_trigger_next (current_trigger);
        }

        kan_instance_size_t triggers_size =
            (kan_instance_size_t) ((uint8_t *) current_trigger - (uint8_t *) first_trigger);
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
    KAN_ASSERT (!definition->cascade_deleters)
    struct kan_reflection_struct_meta_iterator_t iterator = kan_reflection_registry_query_struct_meta (
        repository->registry, parent_type_name, meta_automatic_cascade_deletion_name);

    struct kan_repository_meta_automatic_cascade_deletion_t *meta =
        (struct kan_repository_meta_automatic_cascade_deletion_t *) kan_reflection_struct_meta_iterator_get (&iterator);

    struct cascade_deleter_node_t *first_node = NULL;
    kan_loop_size_t nodes_count = 0u;

    while (meta)
    {
        kan_interned_string_t child_type_name = kan_string_intern (meta->child_type_name);
        struct indexed_storage_node_t *child_storage =
            query_indexed_storage_across_hierarchy (repository, child_type_name);

        kan_instance_size_t parent_absolute_offset;
        kan_instance_size_t parent_size_with_padding;
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

        struct cascade_deleter_node_t *node =
            KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (temporary_allocator, struct cascade_deleter_node_t);

        node->next = first_node;
        first_node = node;
        ++nodes_count;

        kan_repository_indexed_value_delete_query_init (
            &node->query, KAN_HANDLE_SET (kan_repository_indexed_storage_t, child_storage), meta->child_key_path);
        node->absolute_input_value_offset = parent_absolute_offset;

        kan_reflection_struct_meta_iterator_next (&iterator);
        meta = (struct kan_repository_meta_automatic_cascade_deletion_t *) kan_reflection_struct_meta_iterator_get (
            &iterator);
    }

    if (nodes_count == 0u)
    {
        return;
    }

    definition->cascade_deleters_count = (kan_instance_size_t) nodes_count;
    definition->cascade_deleters = kan_allocate_general (
        result_allocation_group, sizeof (struct cascade_deleter_t) * nodes_count, _Alignof (struct cascade_deleter_t));
    kan_loop_size_t index = 0u;

    while (first_node)
    {
        definition->cascade_deleters[index].query = first_node->query;
        definition->cascade_deleters[index].absolute_input_value_offset = first_node->absolute_input_value_offset;
        ++index;
        first_node = first_node->next;
    }
}

static inline void cascade_deleter_fire (struct cascade_deleter_t *deleter, const void *deleted_record)
{
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

static void cascade_deleters_definition_fire (struct cascade_deleters_definition_t *definition,
                                              const void *deleted_record)
{
    for (kan_loop_size_t index = 0u; index < definition->cascade_deleters_count; ++index)
    {
        struct cascade_deleter_t *deleter = &definition->cascade_deleters[index];
        cascade_deleter_fire (deleter, deleted_record);
    }
}

static void cascade_deleters_definition_fire_with_destroy_check (struct cascade_deleters_definition_t *definition,
                                                                 const void *deleted_record)
{
    for (kan_loop_size_t index = 0u; index < definition->cascade_deleters_count; ++index)
    {
        struct cascade_deleter_t *deleter = &definition->cascade_deleters[index];
        struct indexed_value_query_t *query = (struct indexed_value_query_t *) &deleter->query;

        if (!query->index->storage->scheduled_for_destroy)
        {
            cascade_deleter_fire (deleter, deleted_record);
        }
    }
}

static void cascade_deleters_definition_shutdown (struct cascade_deleters_definition_t *definition,
                                                  kan_allocation_group_t allocation_group)
{
    if (definition->cascade_deleters)
    {
        for (kan_loop_size_t index = 0u; index < definition->cascade_deleters_count; ++index)
        {
            kan_repository_indexed_value_delete_query_shutdown (&definition->cascade_deleters[index].query);
        }

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

static void signal_index_shutdown_and_free (struct signal_index_t *signal_index);

static void interval_index_shutdown_and_free (struct interval_index_t *interval_index);

static void space_index_shutdown_and_free (struct space_index_t *space_index);

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

    struct signal_index_t *signal_index = node->first_signal_index;
    while (signal_index)
    {
        struct signal_index_t *next = signal_index->next;
        signal_index_shutdown_and_free (signal_index);
        signal_index = next;
    }

    struct interval_index_t *interval_index = node->first_interval_index;
    while (interval_index)
    {
        struct interval_index_t *next = interval_index->next;
        interval_index_shutdown_and_free (interval_index);
        interval_index = next;
    }

    struct space_index_t *space_index = node->first_space_index;
    while (space_index)
    {
        struct space_index_t *next = space_index->next;
        space_index_shutdown_and_free (space_index);
        space_index = next;
    }

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

static void indexed_field_baked_data_init (struct indexed_field_baked_data_t *data)
{
    data->absolute_offset = 0u;
    data->offset_in_buffer = 0u;
    data->size = 0u;
    data->size_with_padding = 0u;
}

static inline kan_repository_indexed_unsigned_t indexed_field_baked_data_extract_unsigned_from_pointer (
    struct indexed_field_baked_data_t *data, const void *pointer)
{
    switch (data->size)
    {
    case 1u:
        return *(const uint8_t *) pointer;
    case 2u:
        return *(const uint16_t *) pointer;
    case 4u:
        return *(const uint32_t *) pointer;
    case 8u:
        return (kan_repository_indexed_unsigned_t) * (const uint64_t *) pointer;
    }

    KAN_ASSERT (KAN_FALSE)
    return 0u;
}

static inline kan_repository_indexed_unsigned_t indexed_field_baked_data_extract_unsigned_from_record (
    struct indexed_field_baked_data_t *data, void *record)
{
    return indexed_field_baked_data_extract_unsigned_from_pointer (data,
                                                                   (const uint8_t *) record + data->absolute_offset);
}

static inline kan_repository_indexed_unsigned_t indexed_field_baked_data_extract_unsigned_from_buffer (
    struct indexed_field_baked_data_t *data, void *buffer_memory)
{
    return indexed_field_baked_data_extract_unsigned_from_pointer (
        data, (const uint8_t *) buffer_memory + data->offset_in_buffer);
}

static inline kan_repository_indexed_floating_t indexed_field_baked_data_extract_floating_from_pointer (
    struct indexed_field_baked_data_t *data, const void *pointer)
{
    switch (data->size)
    {
    case 4u:
        return (kan_repository_indexed_floating_t) * (const float *) pointer;
    case 8u:
        return (kan_repository_indexed_floating_t) * (const double *) pointer;
    }

    KAN_ASSERT (KAN_FALSE)
    return (kan_repository_indexed_floating_t) 0.0;
}

static inline kan_repository_indexed_floating_t indexed_field_baked_data_extract_floating_from_record (
    struct indexed_field_baked_data_t *data, void *record)
{
    return indexed_field_baked_data_extract_floating_from_pointer (data,
                                                                   (const uint8_t *) record + data->absolute_offset);
}

static inline kan_repository_indexed_signed_t convert_signed_to_unsigned (kan_repository_indexed_signed_t signed_value,
                                                                          kan_instance_size_t size)
{
    switch (size)
    {
    case 1u:
        return signed_value < 0 ? (uint8_t) (signed_value - INT8_MIN) :
                                  ((uint8_t) signed_value) + ((uint8_t) -INT8_MIN);

    case 2u:
        return signed_value < 0 ? (uint16_t) (signed_value - INT16_MIN) :
                                  ((uint16_t) signed_value) + ((uint16_t) -INT16_MIN);

    case 4u:
        return signed_value < 0 ? (uint32_t) (signed_value - INT32_MIN) :
                                  ((uint32_t) signed_value) + (1u + UINT32_MAX / 2u);

    case 8u:
        return (kan_repository_indexed_signed_t) (signed_value < 0 ?
                                                      (uint64_t) (signed_value - INT64_MIN) :
                                                      ((uint64_t) signed_value) + (1u + UINT64_MAX / 2u));
    }

    KAN_ASSERT (KAN_FALSE)
    return 0u;
}

static inline kan_repository_indexed_unsigned_t indexed_field_baked_data_extract_and_convert_unsigned_from_pointer (
    struct indexed_field_baked_data_t *data, enum kan_reflection_archetype_t archetype, const void *pointer)
{
    switch (archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        switch (data->size)
        {
        case 1u:
            return convert_signed_to_unsigned (*(const int8_t *) pointer, 1u);

        case 2u:
            return convert_signed_to_unsigned (*(const int16_t *) pointer, 2u);

        case 4u:
            return convert_signed_to_unsigned (*(const int32_t *) pointer, 4u);

        case 8u:
            return convert_signed_to_unsigned (*(const int64_t *) pointer, 8u);
        }

        break;

    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
        return indexed_field_baked_data_extract_unsigned_from_pointer (data, pointer);

    case KAN_REFLECTION_ARCHETYPE_FLOATING:
    {
        const kan_repository_indexed_floating_t value =
            indexed_field_baked_data_extract_floating_from_pointer (data, pointer);
        KAN_ASSERT (value <= (kan_repository_indexed_floating_t) KAN_INT_MAX (kan_repository_indexed_signed_t))
        KAN_ASSERT (value >= (kan_repository_indexed_floating_t) KAN_INT_MIN (kan_repository_indexed_signed_t))

        kan_repository_indexed_unsigned_t unsigned_value =
            (kan_repository_indexed_unsigned_t) _Generic (*(kan_repository_indexed_floating_t *) NULL,
            double: llround ((double) value),
            default: lroundf ((float) value));

        return convert_signed_to_unsigned (unsigned_value, sizeof (unsigned_value));
    }

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
    return 0u;
}

static inline kan_repository_indexed_unsigned_t indexed_field_baked_data_extract_and_convert_unsigned_from_record (
    struct indexed_field_baked_data_t *data, enum kan_reflection_archetype_t archetype, void *record)
{
    return indexed_field_baked_data_extract_and_convert_unsigned_from_pointer (
        data, archetype, (const uint8_t *) record + data->absolute_offset);
}

static inline kan_repository_indexed_unsigned_t indexed_field_baked_data_extract_and_convert_unsigned_from_buffer (
    struct indexed_field_baked_data_t *data, enum kan_reflection_archetype_t archetype, void *buffer_memory)
{
    return indexed_field_baked_data_extract_and_convert_unsigned_from_pointer (
        data, archetype, (const uint8_t *) buffer_memory + data->offset_in_buffer);
}

static inline void indexed_field_baked_data_extract_and_convert_floating_array_from_pointer (
    struct indexed_field_baked_data_t *data,
    enum kan_reflection_archetype_t archetype,
    kan_instance_size_t array_size,
    const void *pointer,
    kan_repository_indexed_floating_t *output)
{
    const kan_instance_size_t item_size = data->size / array_size;

#define CONVERT_CASE(SIZE, TYPE)                                                                                       \
    case SIZE:                                                                                                         \
    {                                                                                                                  \
        const TYPE *input = (const TYPE *) pointer;                                                                    \
        for (kan_loop_size_t index = 0u; index < array_size; ++index)                                                  \
        {                                                                                                              \
            output[index] = (kan_repository_indexed_floating_t) input[index];                                          \
        }                                                                                                              \
                                                                                                                       \
        return;                                                                                                        \
    }

    switch (archetype)
    {
    case KAN_REFLECTION_ARCHETYPE_SIGNED_INT:
        switch (item_size)
        {
            CONVERT_CASE (1u, int8_t)
            CONVERT_CASE (2u, int16_t)
            CONVERT_CASE (4u, int32_t)
            CONVERT_CASE (8u, int64_t)
        }

        break;

    case KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT:
    case KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL:
        switch (item_size)
        {
            CONVERT_CASE (1u, uint8_t)
            CONVERT_CASE (2u, uint16_t)
            CONVERT_CASE (4u, uint32_t)
            CONVERT_CASE (8u, uint64_t)
        }

        break;

    case KAN_REFLECTION_ARCHETYPE_FLOATING:
        switch (item_size)
        {
            CONVERT_CASE (4u, float)
            CONVERT_CASE (8u, double)
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

#undef CONVERT_CASE
}

static inline void indexed_field_baked_data_extract_and_convert_floating_array_from_record (
    struct indexed_field_baked_data_t *data,
    enum kan_reflection_archetype_t archetype,
    kan_instance_size_t array_size,
    void *record,
    kan_repository_indexed_floating_t *output)
{
    indexed_field_baked_data_extract_and_convert_floating_array_from_pointer (
        data, archetype, array_size, (const uint8_t *) record + data->absolute_offset, output);
}

static inline void indexed_field_baked_data_extract_and_convert_floating_array_from_buffer (
    struct indexed_field_baked_data_t *data,
    enum kan_reflection_archetype_t archetype,
    kan_instance_size_t array_size,
    void *buffer_memory,
    kan_repository_indexed_floating_t *output)
{
    indexed_field_baked_data_extract_and_convert_floating_array_from_pointer (
        data, archetype, array_size, (const uint8_t *) buffer_memory + data->offset_in_buffer, output);
}

static kan_bool_t indexed_field_baked_data_bake_from_reflection (struct indexed_field_baked_data_t *data,
                                                                 kan_reflection_registry_t registry,
                                                                 kan_interned_string_t type_name,
                                                                 struct interned_field_path_t path,
                                                                 enum kan_reflection_archetype_t *archetype_output,
                                                                 kan_instance_size_t *array_size_output_for_multi_value,
                                                                 kan_bool_t validation_allow_not_comparable)
{
    kan_instance_size_t absolute_offset;
    kan_instance_size_t size_with_padding;

    const struct kan_reflection_field_t *field = kan_reflection_registry_query_local_field (
        registry, type_name, path.length, path.path, &absolute_offset, &size_with_padding);

    if (field)
    {
        // Field exists, just re-bake then.
#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
        if ((!array_size_output_for_multi_value &&
             !validation_single_value_index_is_possible (field, validation_allow_not_comparable)) ||
            (array_size_output_for_multi_value && !validation_multi_value_index_is_possible (field)))
        {
            return KAN_FALSE;
        }
#endif

        KAN_ASSERT (absolute_offset < UINT32_MAX)
        KAN_ASSERT (field->size < UINT8_MAX)
        KAN_ASSERT (size_with_padding < UINT8_MAX)

        data->absolute_offset = (uint32_t) absolute_offset;
        data->size = (uint8_t) field->size;
        data->size_with_padding = (uint8_t) size_with_padding;

        if (array_size_output_for_multi_value)
        {
            KAN_ASSERT (field->archetype == KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY)
            if (archetype_output)
            {
                *archetype_output = field->archetype_inline_array.item_archetype;
            }

            *array_size_output_for_multi_value = (uint8_t) field->archetype_inline_array.item_count;
        }
        else if (archetype_output)
        {
            *archetype_output = field->archetype;
        }

        return KAN_TRUE;
    }

    return KAN_FALSE;
}

static kan_bool_t indexed_field_baked_data_bake_from_buffer (struct indexed_field_baked_data_t *data,
                                                             struct observation_buffer_definition_t *buffer)
{
    kan_instance_size_t buffer_offset = 0u;
    const kan_instance_size_t field_begin = data->absolute_offset;
    const kan_instance_size_t field_end = field_begin + data->size;

    for (kan_loop_size_t index = 0u; index < buffer->scenario_chunks_count; ++index)
    {
        struct observation_buffer_scenario_chunk_t *chunk = &buffer->scenario_chunks[index];
        if (field_end > chunk->source_offset && field_begin < chunk->source_offset + chunk->size)
        {
            // Can only be broken if index field chunk is broken due to invalid internal logic or attempt to
            // make index for field inside union. Either way, it is impossible to fix situation in this case.
            KAN_ASSERT (field_begin >= chunk->source_offset)
            KAN_ASSERT (field_end <= chunk->source_offset + chunk->size)

            const kan_instance_size_t offset =
                buffer_offset + (kan_instance_size_t) (field_begin - chunk->source_offset);
            KAN_ASSERT (offset < UINT16_MAX)
            data->offset_in_buffer = (uint16_t) offset;
            return KAN_TRUE;
        }

        buffer_offset =
            (kan_instance_size_t) kan_apply_alignment (buffer_offset + chunk->size, OBSERVATION_BUFFER_ALIGNMENT);
    }

    return KAN_FALSE;
}

static struct value_index_node_t *value_index_query_node_from_hash (struct value_index_t *index, kan_hash_t hash)
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

static void value_index_insert_record (struct value_index_t *index, struct indexed_storage_record_node_t *record_node)
{
    kan_allocation_group_t value_index_allocation_group = index->storage->value_index_allocation_group;
    const kan_hash_t hash =
        (kan_hash_t) indexed_field_baked_data_extract_unsigned_from_record (&index->baked, record_node->record);
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
                                        kan_hash_t hash)
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
    KAN_ASSERT (value_index->hash_storage.items.size > 0u || !index_node)

    while (index_node)
    {
        struct value_index_node_t *next_node = (struct value_index_node_t *) index_node->node.list_node.next;
        struct value_index_sub_node_t *sub_node = index_node->first_sub_node;

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
    shutdown_field_path (value_index->source_path, value_index_allocation_group);
    kan_free_batched (value_index_allocation_group, value_index);
}

static void signal_index_insert_record (struct signal_index_t *index, struct indexed_storage_record_node_t *record_node)
{
    const kan_repository_signal_value_t value =
        (kan_repository_signal_value_t) indexed_field_baked_data_extract_unsigned_from_record (&index->baked,
                                                                                               record_node->record);

    if (value != index->signal_value)
    {
        return;
    }

    kan_allocation_group_t signal_index_allocation_group = index->storage->signal_index_allocation_group;
    struct signal_index_node_t *node = (struct signal_index_node_t *) kan_allocate_batched (
        signal_index_allocation_group, sizeof (struct signal_index_node_t));

    node->record = record_node;
    node->previous = NULL;
    node->next = index->first_node;

    if (index->first_node)
    {
        index->first_node->previous = node;
    }

    index->first_node = node;
}

static void signal_index_delete_by_node (struct signal_index_t *index, struct signal_index_node_t *node)
{
    if (node->next)
    {
        node->next->previous = node->previous;
    }

    if (node->previous)
    {
        node->previous->next = node->next;
    }
    else
    {
        KAN_ASSERT (index->first_node == node)
        index->first_node = node->next;
    }

    kan_free_batched (index->storage->signal_index_allocation_group, node);
}

static void signal_index_delete_by_record (struct signal_index_t *index,
                                           struct indexed_storage_record_node_t *record_node)
{
    struct signal_index_node_t *node = index->first_node;
    while (node)
    {
        if (node->record == record_node)
        {
            signal_index_delete_by_node (index, node);
            return;
        }

        node = node->next;
    }

    KAN_ASSERT (KAN_FALSE)
}

static void signal_index_shutdown_and_free (struct signal_index_t *signal_index)
{
    KAN_ASSERT (kan_atomic_int_get (&signal_index->queries_count) == 0)
    kan_allocation_group_t signal_index_allocation_group = signal_index->storage->signal_index_allocation_group;
    struct signal_index_node_t *node = signal_index->first_node;

    while (node)
    {
        struct signal_index_node_t *next = node->next;
        kan_free_batched (signal_index_allocation_group, node);
        node = next;
    }

    shutdown_field_path (signal_index->source_path, signal_index_allocation_group);
    kan_free_batched (signal_index_allocation_group, signal_index);
}

static void interval_index_insert_record (struct interval_index_t *index,
                                          struct indexed_storage_record_node_t *record_node)
{
    const kan_avl_tree_value_t converted_value =
        (kan_avl_tree_value_t) indexed_field_baked_data_extract_and_convert_unsigned_from_record (
            &index->baked, index->baked_archetype, record_node->record);

    struct interval_index_node_t *parent_index_node =
        (struct interval_index_node_t *) kan_avl_tree_find_parent_for_insertion (&index->tree, converted_value);
    struct interval_index_node_t *insert_index_node;

    if (kan_avl_tree_can_insert (&parent_index_node->node, converted_value))
    {
        insert_index_node = (struct interval_index_node_t *) kan_allocate_batched (
            index->storage->interval_index_allocation_group, sizeof (struct interval_index_node_t));
        insert_index_node->node.tree_value = converted_value;
        insert_index_node->first_sub_node = NULL;
        kan_avl_tree_insert (&index->tree, &parent_index_node->node, &insert_index_node->node);
    }
    else
    {
        KAN_ASSERT (parent_index_node && parent_index_node->node.tree_value == converted_value)
        insert_index_node = parent_index_node;
    }

    struct interval_index_sub_node_t *sub_node = kan_allocate_batched (index->storage->interval_index_allocation_group,
                                                                       sizeof (struct interval_index_sub_node_t));
    sub_node->previous = NULL;
    sub_node->next = insert_index_node->first_sub_node;
    sub_node->record = record_node;

    if (insert_index_node->first_sub_node)
    {
        insert_index_node->first_sub_node->previous = sub_node;
    }

    insert_index_node->first_sub_node = sub_node;
}

static void interval_index_delete_by_sub_node (struct interval_index_t *index,
                                               struct interval_index_node_t *node,
                                               struct interval_index_sub_node_t *sub_node)
{
    kan_allocation_group_t interval_allocation_group = index->storage->interval_index_allocation_group;
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

    kan_free_batched (interval_allocation_group, sub_node);
    if (!node->first_sub_node)
    {
        kan_avl_tree_delete (&index->tree, &node->node);
        kan_free_batched (interval_allocation_group, node);
    }
}

static void interval_index_delete_by_converted_value (struct interval_index_t *index,
                                                      struct indexed_storage_record_node_t *record_node,
                                                      kan_avl_tree_value_t converted_value)
{
    struct interval_index_node_t *index_node =
        (struct interval_index_node_t *) kan_avl_tree_find_equal (&index->tree, converted_value);
    KAN_ASSERT (index_node)

    struct interval_index_sub_node_t *sub_node = index_node->first_sub_node;
    while (sub_node)
    {
        if (sub_node->record == record_node)
        {
            interval_index_delete_by_sub_node (index, index_node, sub_node);
            return;
        }

        sub_node = sub_node->next;
    }

    KAN_ASSERT (KAN_FALSE)
}

static void interval_index_shutdown_and_free_node (kan_allocation_group_t allocation_group,
                                                   struct interval_index_node_t *node)
{
    if (node)
    {
        struct interval_index_sub_node_t *sub_node = node->first_sub_node;
        while (sub_node)
        {
            struct interval_index_sub_node_t *next = sub_node->next;
            kan_free_batched (allocation_group, sub_node);
            sub_node = next;
        }

        interval_index_shutdown_and_free_node (allocation_group, (struct interval_index_node_t *) node->node.left);
        interval_index_shutdown_and_free_node (allocation_group, (struct interval_index_node_t *) node->node.right);
        kan_free_batched (allocation_group, node);
    }
}

static void interval_index_shutdown_and_free (struct interval_index_t *interval_index)
{
    KAN_ASSERT (kan_atomic_int_get (&interval_index->queries_count) == 0)
    kan_allocation_group_t interval_index_allocation_group = interval_index->storage->interval_index_allocation_group;
    interval_index_shutdown_and_free_node (interval_index_allocation_group,
                                           (struct interval_index_node_t *) interval_index->tree.root);
    shutdown_field_path (interval_index->source_path, interval_index_allocation_group);
    kan_free_batched (interval_index_allocation_group, interval_index);
}

static void space_index_insert_record_with_bounds (struct space_index_t *space_index,
                                                   struct indexed_storage_record_node_t *record_node,
                                                   const kan_repository_indexed_floating_t *min,
                                                   const kan_repository_indexed_floating_t *max)
{
    struct kan_space_tree_insertion_iterator_t iterator = kan_space_tree_insertion_start (&space_index->tree, min, max);
    while (!kan_space_tree_insertion_is_finished (&iterator))
    {
        struct space_index_sub_node_t *sub_node =
            kan_space_tree_insertion_insert_and_move (&space_index->tree, &iterator);
        sub_node->object_min = iterator.base.min_path;
        sub_node->object_max = iterator.base.max_path;
        sub_node->record = record_node;
    }
}

static inline void space_index_get_min_max_from_record (struct space_index_t *space_index,
                                                        struct indexed_storage_record_node_t *record_node,
                                                        kan_repository_indexed_floating_t *min,
                                                        kan_repository_indexed_floating_t *max)
{
    indexed_field_baked_data_extract_and_convert_floating_array_from_record (
        &space_index->baked_min, space_index->baked_archetype, space_index->baked_dimension_count, record_node->record,
        min);

    indexed_field_baked_data_extract_and_convert_floating_array_from_record (
        &space_index->baked_max, space_index->baked_archetype, space_index->baked_dimension_count, record_node->record,
        max);
}

static void space_index_insert_record (struct space_index_t *space_index,
                                       struct indexed_storage_record_node_t *record_node)
{
    kan_repository_indexed_floating_t min[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    kan_repository_indexed_floating_t max[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    KAN_ASSERT (space_index->baked_dimension_count <= KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS)
    space_index_get_min_max_from_record (space_index, record_node, min, max);
    space_index_insert_record_with_bounds (space_index, record_node, min, max);
}

struct space_index_sub_node_deletion_order_t
{
    struct space_index_sub_node_deletion_order_t *next;
    struct kan_space_tree_node_t *node;
    struct space_index_sub_node_t *sub_node;
};

static inline void space_index_delete_all_sub_nodes (struct space_index_t *space_index,
                                                     const kan_repository_indexed_floating_t *min,
                                                     const kan_repository_indexed_floating_t *max,
                                                     struct indexed_storage_record_node_t *record_node,
                                                     struct kan_stack_group_allocator_t *temporary_allocator)
{
    struct space_index_sub_node_deletion_order_t *first_to_delete = NULL;
    struct kan_space_tree_shape_iterator_t iterator = kan_space_tree_shape_start (&space_index->tree, min, max);

    while (!kan_space_tree_shape_is_finished (&iterator))
    {
        struct kan_space_tree_node_t *node = iterator.current_node;
        struct space_index_sub_node_t *sub_nodes = node->sub_nodes;

        for (kan_loop_size_t node_index = 0u; node_index < node->sub_nodes_count; ++node_index)
        {
            if (sub_nodes[node_index].record == record_node)
            {
                struct space_index_sub_node_deletion_order_t *to_delete = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                    temporary_allocator, struct space_index_sub_node_deletion_order_t);

                to_delete->next = first_to_delete;
                to_delete->node = node;
                to_delete->sub_node = &sub_nodes[node_index];
                first_to_delete = to_delete;
                break;
            }
        }

        kan_space_tree_shape_move_to_next_node (&space_index->tree, &iterator);
    }

    while (first_to_delete)
    {
        kan_space_tree_delete (&space_index->tree, first_to_delete->node, first_to_delete->sub_node);
        first_to_delete = first_to_delete->next;
    }
}

static inline void space_index_get_min_max_from_buffer (struct space_index_t *space_index,
                                                        void *observation_buffer_memory,
                                                        kan_repository_indexed_floating_t *min,
                                                        kan_repository_indexed_floating_t *max)
{
    indexed_field_baked_data_extract_and_convert_floating_array_from_buffer (
        &space_index->baked_min, space_index->baked_archetype, space_index->baked_dimension_count,
        observation_buffer_memory, min);

    indexed_field_baked_data_extract_and_convert_floating_array_from_buffer (
        &space_index->baked_max, space_index->baked_archetype, space_index->baked_dimension_count,
        observation_buffer_memory, max);
}

static void space_index_delete_by_buffer (struct space_index_t *space_index,
                                          void *observation_buffer_memory,
                                          struct indexed_storage_record_node_t *record_node,
                                          struct kan_stack_group_allocator_t *temporary_allocator)
{
    kan_repository_indexed_floating_t min[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    kan_repository_indexed_floating_t max[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    KAN_ASSERT (space_index->baked_dimension_count <= KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS)
    space_index_get_min_max_from_buffer (space_index, observation_buffer_memory, min, max);
    space_index_delete_all_sub_nodes (space_index, min, max, record_node, temporary_allocator);
}

static void space_index_delete_by_record (struct space_index_t *space_index,
                                          struct indexed_storage_record_node_t *record_node,
                                          struct kan_stack_group_allocator_t *temporary_allocator)
{
    kan_repository_indexed_floating_t min[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    kan_repository_indexed_floating_t max[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    KAN_ASSERT (space_index->baked_dimension_count <= KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS)
    space_index_get_min_max_from_record (space_index, record_node, min, max);
    space_index_delete_all_sub_nodes (space_index, min, max, record_node, temporary_allocator);
}

static void space_index_delete_by_sub_node (struct space_index_t *space_index,
                                            struct kan_space_tree_node_t *tree_node,
                                            struct space_index_sub_node_t *sub_node,
                                            void *observation_buffer_memory,
                                            struct kan_stack_group_allocator_t *temporary_allocator)
{
    struct indexed_storage_record_node_t *record_node = sub_node->record;
    kan_space_tree_delete (&space_index->tree, tree_node, sub_node);

    kan_repository_indexed_floating_t min[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    kan_repository_indexed_floating_t max[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    KAN_ASSERT (space_index->baked_dimension_count <= KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS)

    if (observation_buffer_memory)
    {
        space_index_get_min_max_from_buffer (space_index, observation_buffer_memory, min, max);
    }
    else
    {
        space_index_get_min_max_from_record (space_index, record_node, min, max);
    }

    if (!kan_space_tree_is_contained_in_one_sub_node (&space_index->tree, min, max))
    {
        space_index_delete_all_sub_nodes (space_index, min, max, record_node, temporary_allocator);
    }
}

static void space_index_update (struct space_index_t *space_index,
                                void *observation_buffer_memory,
                                struct indexed_storage_record_node_t *record_node,
                                struct kan_stack_group_allocator_t *temporary_allocator)
{
    kan_repository_indexed_floating_t old_min[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    kan_repository_indexed_floating_t old_max[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    kan_repository_indexed_floating_t new_min[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    kan_repository_indexed_floating_t new_max[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    KAN_ASSERT (space_index->baked_dimension_count <= KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS)

    space_index_get_min_max_from_buffer (space_index, observation_buffer_memory, old_min, old_max);
    space_index_get_min_max_from_record (space_index, record_node, new_min, new_max);

    if (kan_space_tree_is_re_insert_needed (&space_index->tree, old_min, old_max, new_min, new_max))
    {
        space_index_delete_all_sub_nodes (space_index, old_min, old_max, record_node, temporary_allocator);
        space_index_insert_record_with_bounds (space_index, record_node, new_min, new_max);
    }
}

static void space_index_update_with_sub_node (struct space_index_t *space_index,
                                              struct kan_space_tree_node_t *tree_node,
                                              struct space_index_sub_node_t *sub_node,
                                              void *observation_buffer_memory,
                                              struct kan_stack_group_allocator_t *temporary_allocator)
{
    kan_repository_indexed_floating_t old_min[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    kan_repository_indexed_floating_t old_max[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    kan_repository_indexed_floating_t new_min[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    kan_repository_indexed_floating_t new_max[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
    KAN_ASSERT (space_index->baked_dimension_count <= KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS)

    struct indexed_storage_record_node_t *record_node = sub_node->record;
    space_index_get_min_max_from_buffer (space_index, observation_buffer_memory, old_min, old_max);
    space_index_get_min_max_from_record (space_index, record_node, new_min, new_max);

    if (kan_space_tree_is_re_insert_needed (&space_index->tree, old_min, old_max, new_min, new_max))
    {
        space_index_delete_by_sub_node (space_index, tree_node, sub_node, observation_buffer_memory,
                                        temporary_allocator);
        space_index_insert_record_with_bounds (space_index, record_node, new_min, new_max);
    }
}

static void space_index_shutdown_and_free (struct space_index_t *space_index)
{
    kan_allocation_group_t space_index_allocation_group = space_index->storage->space_index_allocation_group;
    kan_space_tree_shutdown (&space_index->tree);
    shutdown_field_path (space_index->source_path_min, space_index_allocation_group);
    shutdown_field_path (space_index->source_path_max, space_index_allocation_group);
    kan_free_batched (space_index_allocation_group, space_index);
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

static void repository_init_utility_planning_storages (struct repository_t *repository)
{
    kan_hash_storage_init (&repository->event_types_to_uplift, repository->allocation_group,
                           KAN_REPOSITORY_UTILITY_HASHES_INITIAL_BUCKETS);

#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
    kan_hash_storage_init (&repository->cascade_deleted_indexed_types, repository->allocation_group,
                           KAN_REPOSITORY_UTILITY_HASHES_INITIAL_BUCKETS);
#endif
}

static void repository_shutdown_utility_planning_storages (struct repository_t *repository)
{
    struct type_name_node_t *node = (struct type_name_node_t *) repository->event_types_to_uplift.items.first;
    while (node)
    {
        struct type_name_node_t *next = (struct type_name_node_t *) node->node.list_node.next;
        kan_free_batched (repository->allocation_group, node);
        node = next;
    }

#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
    node = (struct type_name_node_t *) repository->cascade_deleted_indexed_types.items.first;
    while (node)
    {
        struct type_name_node_t *next = (struct type_name_node_t *) node->node.list_node.next;
        kan_free_batched (repository->allocation_group, node);
        node = next;
    }
#endif

    kan_hash_storage_shutdown (&repository->event_types_to_uplift);
#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
    kan_hash_storage_shutdown (&repository->cascade_deleted_indexed_types);
#endif
}

static struct repository_t *repository_create (kan_allocation_group_t allocation_group,
                                               struct repository_t *parent,
                                               kan_interned_string_t name,
                                               kan_reflection_registry_t registry)
{
    ensure_statics_initialized ();
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
    repository->scheduled_for_destroy = KAN_FALSE;

    kan_hash_storage_init (&repository->singleton_storages, repository->allocation_group,
                           KAN_REPOSITORY_SINGLETON_STORAGE_INITIAL_BUCKETS);

    kan_hash_storage_init (&repository->indexed_storages, repository->allocation_group,
                           KAN_REPOSITORY_INDEXED_STORAGE_INITIAL_BUCKETS);

    kan_hash_storage_init (&repository->event_storages, repository->allocation_group,
                           KAN_REPOSITORY_EVENT_STORAGE_INITIAL_BUCKETS);

    repository_init_utility_planning_storages (repository);
    return repository;
}

kan_repository_t kan_repository_create_root (kan_allocation_group_t allocation_group,
                                             kan_reflection_registry_t registry)
{
    return KAN_HANDLE_SET (kan_repository_t,
                           repository_create (allocation_group, NULL, kan_string_intern ("root"), registry));
}

kan_repository_t kan_repository_create_child (kan_repository_t parent, const char *name)
{
    struct repository_t *parent_repository = KAN_HANDLE_GET (parent);

    const kan_allocation_group_t allocation_group = kan_allocation_group_get_child (
        kan_allocation_group_get_child (parent_repository->allocation_group, "child_repositories"), name);

    const kan_interned_string_t interned_name = kan_string_intern (name);
    return KAN_HANDLE_SET (kan_repository_t, repository_create (allocation_group, parent_repository, interned_name,
                                                                parent_repository->registry));
}

static void repository_start_scheduled_destroy (struct repository_t *repository)
{
    struct indexed_storage_node_t *indexed_storage_node =
        (struct indexed_storage_node_t *) repository->indexed_storages.items.first;

    while (indexed_storage_node)
    {
        if (repository->scheduled_for_destroy || indexed_storage_node->scheduled_for_destroy)
        {
            struct indexed_storage_record_node_t *record_node =
                (struct indexed_storage_record_node_t *) indexed_storage_node->records.first;

            while (record_node)
            {
                lifetime_event_triggers_definition_fire_with_destroy_check (
                    &indexed_storage_node->on_delete_events_triggers, record_node->record);

                cascade_deleters_definition_fire_with_destroy_check (&indexed_storage_node->cascade_deleters,
                                                                     record_node->record);

                record_node = (struct indexed_storage_record_node_t *) record_node->list_node.next;
            }
        }

        indexed_storage_node = (struct indexed_storage_node_t *) indexed_storage_node->node.list_node.next;
    }
}

static kan_bool_t type_name_storage_contains (struct kan_hash_storage_t *storage, kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (storage, (kan_hash_t) type_name);
    struct type_name_node_t *node = (struct type_name_node_t *) bucket->first;
    const struct type_name_node_t *node_end = (struct type_name_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->type_name == type_name)
        {
            return KAN_TRUE;
        }

        node = (struct type_name_node_t *) node->node.list_node.next;
    }

    return KAN_FALSE;
}

static void type_name_storage_insert (struct kan_hash_storage_t *storage,
                                      kan_interned_string_t type_name,
                                      kan_allocation_group_t allocation_group)
{
    if (type_name_storage_contains (storage, type_name))
    {
        return;
    }

    struct type_name_node_t *new_node = kan_allocate_batched (allocation_group, sizeof (struct type_name_node_t));
    new_node->node.hash = KAN_HASH_OBJECT_POINTER (type_name);
    new_node->type_name = type_name;

    kan_hash_storage_update_bucket_count_default (storage, KAN_REPOSITORY_UTILITY_HASHES_INITIAL_BUCKETS);
    kan_hash_storage_add (storage, &new_node->node);
}

static void repository_register_events_to_uplift (struct repository_t *repository, kan_interned_string_t source_type)
{
    struct kan_reflection_struct_meta_iterator_t iterator = kan_reflection_registry_query_struct_meta (
        repository->registry, source_type, meta_automatic_on_insert_event_name);

    const struct kan_repository_meta_automatic_on_insert_event_t *meta_on_insert =
        kan_reflection_struct_meta_iterator_get (&iterator);

    while (meta_on_insert)
    {
        type_name_storage_insert (&repository->event_types_to_uplift, kan_string_intern (meta_on_insert->event_type),
                                  repository->allocation_group);

        kan_reflection_struct_meta_iterator_next (&iterator);
        meta_on_insert = kan_reflection_struct_meta_iterator_get (&iterator);
    }

    iterator = kan_reflection_registry_query_struct_meta (repository->registry, source_type,
                                                          meta_automatic_on_change_event_name);

    const struct kan_repository_meta_automatic_on_change_event_t *meta_on_change =
        kan_reflection_struct_meta_iterator_get (&iterator);

    while (meta_on_change)
    {
        type_name_storage_insert (&repository->event_types_to_uplift, kan_string_intern (meta_on_change->event_type),
                                  repository->allocation_group);

        kan_reflection_struct_meta_iterator_next (&iterator);
        meta_on_change = kan_reflection_struct_meta_iterator_get (&iterator);
    }

    iterator = kan_reflection_registry_query_struct_meta (repository->registry, source_type,
                                                          meta_automatic_on_delete_event_name);

    const struct kan_repository_meta_automatic_on_delete_event_t *meta_on_delete =
        kan_reflection_struct_meta_iterator_get (&iterator);

    while (meta_on_delete)
    {
        type_name_storage_insert (&repository->event_types_to_uplift, kan_string_intern (meta_on_delete->event_type),
                                  repository->allocation_group);

        kan_reflection_struct_meta_iterator_next (&iterator);
        meta_on_delete = kan_reflection_struct_meta_iterator_get (&iterator);
    }
}

#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
static void repository_register_cascade_deleted_types (struct repository_t *repository,
                                                       kan_interned_string_t source_type)
{
    struct kan_reflection_struct_meta_iterator_t iterator = kan_reflection_registry_query_struct_meta (
        repository->registry, source_type, meta_automatic_cascade_deletion_name);

    const struct kan_repository_meta_automatic_cascade_deletion_t *meta =
        kan_reflection_struct_meta_iterator_get (&iterator);

    while (meta)
    {
        type_name_storage_insert (&repository->cascade_deleted_indexed_types, kan_string_intern (meta->child_type_name),
                                  repository->allocation_group);

        kan_reflection_struct_meta_iterator_next (&iterator);
        meta = kan_reflection_struct_meta_iterator_get (&iterator);
    }
}

static kan_bool_t validate_indexed_type_is_not_forbidden_due_to_cascade_deletion (struct repository_t *repository,
                                                                                  kan_interned_string_t type)
{
    struct repository_t *parent_repository = repository->parent;
    while (parent_repository)
    {
        if (type_name_storage_contains (&parent_repository->cascade_deleted_indexed_types, type))
        {
            KAN_LOG (repository, KAN_LOG_ERROR,
                     "Storage for indexed record type \"%s\" cannot be opened in repository \"%s\", because it is "
                     "already referenced in cascade deleter in parent repository \"%s\". Created storage would "
                     "invisible to parent and cascade deletion would not work.",
                     type, repository->name, parent_repository->name)
            return KAN_FALSE;
        }

        parent_repository = parent_repository->parent;
    }

    return KAN_TRUE;
}
#endif

static void repository_enter_planning_mode_internal (struct repository_t *repository)
{
    repository_init_utility_planning_storages (repository);
    struct repository_t *child_repository = repository->first;

    while (child_repository)
    {
        repository_enter_planning_mode_internal (child_repository);
        child_repository = child_repository->next;
    }

    repository_start_scheduled_destroy (repository);
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
                     singleton_storage_node->type->name)
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

        repository_register_events_to_uplift (repository, singleton_storage_node->type->name);
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

        repository_register_events_to_uplift (repository, indexed_storage_node->type->name);
#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
        repository_register_cascade_deleted_types (repository, indexed_storage_node->type->name);
#endif

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
                     event_storage_node->type->name)
        }

        event_storage_node = (struct event_storage_node_t *) event_storage_node->node.list_node.next;
    }
#endif
}

void kan_repository_enter_planning_mode (kan_repository_t root_repository)
{
    struct repository_t *repository = KAN_HANDLE_GET (root_repository);
    KAN_ASSERT (!repository->parent)

    kan_cpu_section_t section = kan_cpu_section_get ("repository_enter_planning_mode");
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, section);
    repository_enter_planning_mode_internal (repository);
    kan_cpu_section_execution_shutdown (&execution);
}

static void repository_prepare_for_migration_internal (struct repository_t *repository,
                                                       kan_reflection_migration_seed_t migration_seed)
{
    struct singleton_storage_node_t *singleton_storage_node =
        (struct singleton_storage_node_t *) repository->singleton_storages.items.first;

    while (singleton_storage_node)
    {
        const struct kan_reflection_struct_migration_seed_t *seed =
            kan_reflection_migration_seed_get_for_struct (migration_seed, singleton_storage_node->type->name);

        if (seed->status == KAN_REFLECTION_MIGRATION_REMOVED)
        {
            singleton_storage_node->scheduled_for_destroy = KAN_TRUE;
        }

        singleton_storage_node = (struct singleton_storage_node_t *) singleton_storage_node->node.list_node.next;
    }

    struct indexed_storage_node_t *indexed_storage_node =
        (struct indexed_storage_node_t *) repository->indexed_storages.items.first;

    while (indexed_storage_node)
    {
        const struct kan_reflection_struct_migration_seed_t *seed =
            kan_reflection_migration_seed_get_for_struct (migration_seed, indexed_storage_node->type->name);

        if (seed->status == KAN_REFLECTION_MIGRATION_REMOVED)
        {
            indexed_storage_node->scheduled_for_destroy = KAN_TRUE;
            struct indexed_storage_record_node_t *record_node =
                (struct indexed_storage_record_node_t *) indexed_storage_node->records.first;

            while (record_node)
            {
                lifetime_event_triggers_definition_fire_with_destroy_check (
                    &indexed_storage_node->on_delete_events_triggers, record_node->record);

                cascade_deleters_definition_fire_with_destroy_check (&indexed_storage_node->cascade_deleters,
                                                                     record_node->record);
                record_node = (struct indexed_storage_record_node_t *) record_node->list_node.next;
            }
        }

        indexed_storage_node = (struct indexed_storage_node_t *) indexed_storage_node->node.list_node.next;
    }

    struct event_storage_node_t *event_storage_node =
        (struct event_storage_node_t *) repository->event_storages.items.first;

    while (event_storage_node)
    {
        const struct kan_reflection_struct_migration_seed_t *seed =
            kan_reflection_migration_seed_get_for_struct (migration_seed, event_storage_node->type->name);

        if (seed->status == KAN_REFLECTION_MIGRATION_REMOVED)
        {
            event_storage_node->scheduled_for_destroy = KAN_TRUE;
        }

        event_storage_node = (struct event_storage_node_t *) event_storage_node->node.list_node.next;
    }

    repository = repository->first;
    while (repository)
    {
        repository_prepare_for_migration_internal (repository, migration_seed);
        repository = repository->next;
    }
}

void kan_repository_prepare_for_migration (kan_repository_t root_repository,
                                           kan_reflection_migration_seed_t migration_seed)
{
    struct repository_t *repository = KAN_HANDLE_GET (root_repository);
    KAN_ASSERT (!repository->parent)
    repository_prepare_for_migration_internal (repository, migration_seed);
}

KAN_CPU_TASK_BATCHED_HEADER (execute_migration) { kan_reflection_struct_migrator_t migrator; };

KAN_CPU_TASK_BATCHED_BODY (execute_migration)
{
    void **record_pointer;
    kan_allocation_group_t allocation_group;
    kan_bool_t batched_allocation;
    const struct kan_reflection_struct_t *old_type;
    const struct kan_reflection_struct_t *new_type;
};

KAN_CPU_TASK_BATCHED_DEFINE (execute_migration)
{
    void *old_object = *body->record_pointer;
    void *new_object = body->batched_allocation ? kan_allocate_batched (body->allocation_group, body->new_type->size) :
                                                  kan_allocate_general (body->allocation_group, body->new_type->size,
                                                                        body->new_type->alignment);

    if (body->new_type->init)
    {
        kan_allocation_group_stack_push (body->allocation_group);
        body->new_type->init (body->new_type->functor_user_data, new_object);
        kan_allocation_group_stack_pop ();
    }

    kan_reflection_struct_migrator_migrate_instance (header->migrator, body->new_type->name, old_object, new_object);
    if (body->old_type->shutdown)
    {
        body->old_type->shutdown (body->old_type->functor_user_data, old_object);
    }

    if (body->batched_allocation)
    {
        kan_free_batched (body->allocation_group, old_object);
    }
    else
    {
        kan_free_general (body->allocation_group, old_object, body->old_type->size);
    }

    *body->record_pointer = new_object;
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

    KAN_CPU_TASK_LIST_BATCHED_PREPARE (execute_migration, KAN_REPOSITORY_MIGRATION_TASK_BATCH_MIN,
                                       KAN_REPOSITORY_MIGRATION_TASK_BATCH_ATE,
                                       {
                                           .migrator = migrator,
                                       });

    while (singleton_storage_node)
    {
        struct singleton_storage_node_t *next =
            (struct singleton_storage_node_t *) singleton_storage_node->node.list_node.next;

        const struct kan_reflection_struct_t *old_type = singleton_storage_node->type;
        const struct kan_reflection_struct_t *new_type =
            kan_reflection_registry_query_struct (new_registry, old_type->name);

        const struct kan_reflection_struct_migration_seed_t *seed =
            kan_reflection_migration_seed_get_for_struct (migration_seed, old_type->name);

        // Resetting type for removed storages results in incorrect destruction.
        if (seed->status != KAN_REFLECTION_MIGRATION_REMOVED)
        {
            singleton_storage_node->type = new_type;
        }

        switch (seed->status)
        {
        case KAN_REFLECTION_MIGRATION_NEEDED:
        {
            KAN_CPU_TASK_LIST_BATCHED (&context->task_list, &context->allocator, execute_migration,
                                       migration_task_section,
                                       {
                                           .record_pointer = &singleton_storage_node->singleton,
                                           .allocation_group = singleton_storage_node->allocation_group,
                                           .batched_allocation = KAN_FALSE,
                                           .old_type = old_type,
                                           .new_type = new_type,
                                       });
            break;
        }

        case KAN_REFLECTION_MIGRATION_NOT_NEEDED:
            // No migration, therefore don't need to do anything.
            break;

        case KAN_REFLECTION_MIGRATION_REMOVED:
            KAN_ASSERT (singleton_storage_node->scheduled_for_destroy)
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

        const struct kan_reflection_struct_migration_seed_t *seed =
            kan_reflection_migration_seed_get_for_struct (migration_seed, old_type->name);

        // Resetting type for removed storages results in incorrect destruction.
        if (seed->status != KAN_REFLECTION_MIGRATION_REMOVED)
        {
            indexed_storage_node->type = new_type;
        }

        switch (seed->status)
        {
        case KAN_REFLECTION_MIGRATION_NEEDED:
        {
#define HELPER_UPDATE_SINGLE_FIELD_INDEX(INDEX_TYPE, ARCHETYPE_OUTPUT, ALLOW_NOT_COMPARABLE)                           \
    struct INDEX_TYPE##_index_t *INDEX_TYPE##_index = indexed_storage_node->first_##INDEX_TYPE##_index;                \
    struct INDEX_TYPE##_index_t *previous_##INDEX_TYPE##_index = NULL;                                                 \
                                                                                                                       \
    while (INDEX_TYPE##_index)                                                                                         \
    {                                                                                                                  \
        struct INDEX_TYPE##_index_t *next_##INDEX_TYPE##_index = INDEX_TYPE##_index->next;                             \
        if (!indexed_field_baked_data_bake_from_reflection (&INDEX_TYPE##_index->baked, new_registry, old_type->name,  \
                                                            INDEX_TYPE##_index->source_path, ARCHETYPE_OUTPUT, NULL,   \
                                                            ALLOW_NOT_COMPARABLE))                                     \
        {                                                                                                              \
            INDEX_TYPE##_index_shutdown_and_free (INDEX_TYPE##_index);                                                 \
                                                                                                                       \
            if (previous_##INDEX_TYPE##_index)                                                                         \
            {                                                                                                          \
                previous_##INDEX_TYPE##_index->next = next_##INDEX_TYPE##_index;                                       \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                indexed_storage_node->first_##INDEX_TYPE##_index = next_##INDEX_TYPE##_index;                          \
            }                                                                                                          \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            previous_##INDEX_TYPE##_index = INDEX_TYPE##_index;                                                        \
        }                                                                                                              \
                                                                                                                       \
        INDEX_TYPE##_index = next_##INDEX_TYPE##_index;                                                                \
    }

            HELPER_UPDATE_SINGLE_FIELD_INDEX (value, NULL, KAN_TRUE)
            HELPER_UPDATE_SINGLE_FIELD_INDEX (signal, NULL, KAN_TRUE)
            HELPER_UPDATE_SINGLE_FIELD_INDEX (interval, &interval_index->baked_archetype, KAN_FALSE)

#undef HELPER_UPDATE_SINGLE_FIELD_INDEX

            struct space_index_t *space_index = indexed_storage_node->first_space_index;
            struct space_index_t *previous_space_index = NULL;

            while (space_index)
            {
                struct space_index_t *next_space_index = space_index->next;
                kan_instance_size_t min_size = 0u;
                kan_instance_size_t max_size = 0u;

                enum kan_reflection_archetype_t min_archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT;
                enum kan_reflection_archetype_t max_archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT;

                const kan_bool_t min_baked = indexed_field_baked_data_bake_from_reflection (
                    &space_index->baked_min, new_registry, old_type->name, space_index->source_path_min, &min_archetype,
                    &min_size, KAN_FALSE);

                const kan_bool_t max_baked = indexed_field_baked_data_bake_from_reflection (
                    &space_index->baked_max, new_registry, old_type->name, space_index->source_path_max, &max_archetype,
                    &max_size, KAN_FALSE);

                space_index->baked_archetype = min_archetype;
                space_index->baked_dimension_count = min_size;

                if (min_archetype != max_archetype)
                {
                    KAN_LOG (repository, KAN_LOG_ERROR,
                             "Failed to migrate space index on type %s, min and max archetypes mismatch detected.",
                             old_type->name)
                }

                if (min_size != max_size)
                {
                    KAN_LOG (
                        repository, KAN_LOG_ERROR,
                        "Failed to migrate space index on type %s, min and max array sizes do not match: %lu and %lu.",
                        old_type->name, (unsigned long) min_size, (unsigned long) max_size)
                }

                if (min_size > KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS)
                {
                    KAN_LOG (repository, KAN_LOG_ERROR,
                             "Failed to migrate space index on type %s, min array size is bigger that allowed for "
                             "space tree: %lu > %lu.",
                             old_type->name, (unsigned long) min_size,
                             (unsigned long) KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS)
                }

                if (!min_baked || !max_baked || min_archetype != max_archetype || min_size != max_size ||
                    min_size > KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS)
                {
                    space_index_shutdown_and_free (space_index);
                    if (previous_space_index)
                    {
                        previous_space_index->next = next_space_index;
                    }
                    else
                    {
                        indexed_storage_node->first_space_index = next_space_index;
                    }
                }
                else
                {
                    previous_space_index = space_index;
                }
                space_index = next_space_index;
            }

            struct indexed_storage_record_node_t *node =
                (struct indexed_storage_record_node_t *) indexed_storage_node->records.first;

            while (node)
            {
                KAN_CPU_TASK_LIST_BATCHED (&context->task_list, &context->allocator, execute_migration,
                                           migration_task_section,
                                           {
                                               .record_pointer = &node->record,
                                               .allocation_group = indexed_storage_node->records_allocation_group,
                                               .batched_allocation = KAN_TRUE,
                                               .old_type = old_type,
                                               .new_type = new_type,
                                           });

                node = (struct indexed_storage_record_node_t *) node->list_node.next;
            }

            break;
        }

        case KAN_REFLECTION_MIGRATION_NOT_NEEDED:
            // No migration, therefore don't need to do anything.
            break;

        case KAN_REFLECTION_MIGRATION_REMOVED:
            KAN_ASSERT (indexed_storage_node->scheduled_for_destroy)
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

        const struct kan_reflection_struct_migration_seed_t *seed =
            kan_reflection_migration_seed_get_for_struct (migration_seed, old_type->name);

        // Resetting type for removed storages results in incorrect destruction.
        if (seed->status != KAN_REFLECTION_MIGRATION_REMOVED)
        {
            event_storage_node->type = new_type;
        }

        switch (seed->status)
        {
        case KAN_REFLECTION_MIGRATION_NEEDED:
        {
            struct event_queue_node_t *node = (struct event_queue_node_t *) event_storage_node->event_queue.oldest;
            while (&node->node != event_storage_node->event_queue.next_placeholder)
            {
                KAN_CPU_TASK_LIST_BATCHED (&context->task_list, &context->allocator, execute_migration,
                                           migration_task_section,
                                           {
                                               .record_pointer = &node->event,
                                               .allocation_group = event_storage_node->allocation_group,
                                               .batched_allocation = KAN_TRUE,
                                               .old_type = old_type,
                                               .new_type = new_type,
                                           });

                node = (struct event_queue_node_t *) node->node.next;
            }

            break;
        }

        case KAN_REFLECTION_MIGRATION_NOT_NEEDED:
            // No migration, therefore don't need to do anything.
            break;

        case KAN_REFLECTION_MIGRATION_REMOVED:
            KAN_ASSERT (event_storage_node->scheduled_for_destroy)
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
    struct repository_t *repository = KAN_HANDLE_GET (root_repository);
    KAN_ASSERT (!repository->parent)

    kan_cpu_section_t section = kan_cpu_section_get ("repository_migrate");
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, section);

    kan_cpu_job_t job = kan_cpu_job_create ();
    KAN_ASSERT (KAN_HANDLE_IS_VALID (job))

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
    kan_cpu_section_execution_shutdown (&execution);
}

static struct singleton_storage_node_t *query_singleton_storage_across_hierarchy (struct repository_t *repository,
                                                                                  kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&repository->singleton_storages, KAN_HASH_OBJECT_POINTER (type_name));
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
    struct repository_t *repository_data = KAN_HANDLE_GET (repository);
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
            return KAN_HANDLE_SET_INVALID (kan_repository_singleton_storage_t);
        }

        const kan_allocation_group_t storage_allocation_group = kan_allocation_group_get_child (
            kan_allocation_group_get_child (repository_data->allocation_group, "singletons"), interned_type_name);

        storage = (struct singleton_storage_node_t *) kan_allocate_batched (storage_allocation_group,
                                                                            sizeof (struct singleton_storage_node_t));

        storage->node.hash = KAN_HASH_OBJECT_POINTER (interned_type_name);
        storage->type = singleton_type;
        storage->queries_count = kan_atomic_int_init (0);

        storage->singleton =
            kan_allocate_general (storage_allocation_group, storage->type->size, storage->type->alignment);

        if (storage->type->init)
        {
            kan_allocation_group_stack_push (storage_allocation_group);
            storage->type->init (storage->type->functor_user_data, storage->singleton);
            kan_allocation_group_stack_pop ();
        }

        observation_buffer_definition_init (&storage->observation_buffer);
        storage->observation_buffer_memory = NULL;
        observation_event_triggers_definition_init (&storage->observation_events_triggers);

        storage->allocation_group = storage_allocation_group;
        storage->automation_allocation_group = kan_allocation_group_get_child (storage_allocation_group, "automation");

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        storage->safeguard_access_status = kan_atomic_int_init (0);
#endif

        storage->scheduled_for_destroy = KAN_FALSE;
        kan_hash_storage_update_bucket_count_default (&repository_data->singleton_storages,
                                                      KAN_REPOSITORY_SINGLETON_STORAGE_INITIAL_BUCKETS);
        kan_hash_storage_add (&repository_data->singleton_storages, &storage->node);

        repository_register_events_to_uplift (repository_data, interned_type_name);
    }

    repository_storage_map_access_unlock (repository_data);
    return KAN_HANDLE_SET (kan_repository_singleton_storage_t, storage);
}

void kan_repository_singleton_read_query_init (struct kan_repository_singleton_read_query_t *query,
                                               kan_repository_singleton_storage_t storage)
{
    struct singleton_storage_node_t *storage_data = KAN_HANDLE_GET (storage);
    kan_atomic_int_add (&storage_data->queries_count, 1);
    *(struct singleton_query_t *) query = (struct singleton_query_t) {.storage = storage_data};
}

struct kan_repository_singleton_read_access_t kan_repository_singleton_read_query_execute (
    struct kan_repository_singleton_read_query_t *query)
{
    struct singleton_query_t *query_data = (struct singleton_query_t *) query;
    KAN_ASSERT (query_data->storage)
    struct kan_repository_singleton_read_access_t result = {.implementation_data = NULL};

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    if (!safeguard_singleton_read_access_try_create (query_data->storage))
    {
        return result;
    }
#endif

    result.implementation_data = query_data->storage;
    return result;
}

const void *kan_repository_singleton_read_access_resolve (struct kan_repository_singleton_read_access_t *access)
{
    struct singleton_storage_node_t *storage = access->implementation_data;
    return storage ? storage->singleton : NULL;
}

void kan_repository_singleton_read_access_close (struct kan_repository_singleton_read_access_t *access)
{
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    struct singleton_storage_node_t *storage = access->implementation_data;
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
    struct singleton_storage_node_t *storage_data = KAN_HANDLE_GET (storage);
    kan_atomic_int_add (&storage_data->queries_count, 1);
    *(struct singleton_query_t *) query = (struct singleton_query_t) {.storage = storage_data};
}

struct kan_repository_singleton_write_access_t kan_repository_singleton_write_query_execute (
    struct kan_repository_singleton_write_query_t *query)
{
    struct singleton_query_t *query_data = (struct singleton_query_t *) query;
    KAN_ASSERT (query_data->storage)
    struct kan_repository_singleton_write_access_t result = {.implementation_data = NULL};

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    if (!safeguard_singleton_write_access_try_create (query_data->storage))
    {
        return result;
    }
#endif

    observation_buffer_definition_import (&query_data->storage->observation_buffer,
                                          query_data->storage->observation_buffer_memory,
                                          query_data->storage->singleton);
    result.implementation_data = query_data->storage;
    return result;
}

void *kan_repository_singleton_write_access_resolve (struct kan_repository_singleton_write_access_t *access)
{
    struct singleton_storage_node_t *storage = access->implementation_data;
    return storage ? storage->singleton : NULL;
}

void kan_repository_singleton_write_access_close (struct kan_repository_singleton_write_access_t *access)
{
    struct singleton_storage_node_t *storage = access->implementation_data;
    if (storage)
    {
        const kan_repository_mask_t change_flags = observation_buffer_definition_compare (
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
        kan_hash_storage_query (&repository->indexed_storages, KAN_HASH_OBJECT_POINTER (type_name));
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
                                                                      const char *type_name)
{
    struct repository_t *repository_data = KAN_HANDLE_GET (repository);
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
            return KAN_HANDLE_SET_INVALID (kan_repository_indexed_storage_t);
        }

#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
        if (!validate_indexed_type_is_not_forbidden_due_to_cascade_deletion (repository_data, interned_type_name))
        {
            repository_storage_map_access_unlock (repository_data);
            return KAN_HANDLE_SET_INVALID (kan_repository_indexed_storage_t);
        }
#endif

        const kan_allocation_group_t storage_allocation_group = kan_allocation_group_get_child (
            kan_allocation_group_get_child (repository_data->allocation_group, "indexed"), interned_type_name);

        storage = (struct indexed_storage_node_t *) kan_allocate_batched (storage_allocation_group,
                                                                          sizeof (struct indexed_storage_node_t));

        storage->node.hash = KAN_HASH_OBJECT_POINTER (interned_type_name);
        storage->repository = repository_data;
        storage->type = indexed_type;

        kan_bd_list_init (&storage->records);
        storage->access_status = kan_atomic_int_init (0);
        storage->queries_count = kan_atomic_int_init (0);
        storage->maintenance_lock = kan_atomic_int_init (0);
        storage->dirty_records = NULL;
        kan_stack_group_allocator_init (&storage->temporary_allocator,
                                        kan_allocation_group_get_child (storage_allocation_group, "temporary"),
                                        KAN_REPOSITORY_INDEXED_STORAGE_STACK_INITIAL_SIZE);

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

        storage->scheduled_for_destroy = KAN_FALSE;
        storage->candidate_for_cleanup = KAN_FALSE;

        kan_hash_storage_update_bucket_count_default (&repository_data->indexed_storages,
                                                      KAN_REPOSITORY_INDEXED_STORAGE_INITIAL_BUCKETS);
        kan_hash_storage_add (&repository_data->indexed_storages, &storage->node);

        repository_register_events_to_uplift (repository_data, interned_type_name);
#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
        repository_register_cascade_deleted_types (repository_data, interned_type_name);
#endif
    }

    repository_storage_map_access_unlock (repository_data);
    return KAN_HANDLE_SET (kan_repository_indexed_storage_t, storage);
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
                            const kan_hash_t old_hash =
                                (kan_hash_t) indexed_field_baked_data_extract_unsigned_from_buffer (
                                    &value_index->baked, storage->dirty_records->observation_buffer_memory);
                            value_index_delete_by_hash (value_index, node, old_hash);
                        }

                        value_index_insert_record (value_index, node);
                    }

                    value_index = value_index->next;
                }

                struct signal_index_t *signal_index = storage->first_signal_index;
                while (signal_index)
                {
                    if (signal_index->observation_flags & storage->dirty_records->observation_comparison_flags)
                    {
                        if (signal_index == storage->dirty_records->dirt_source_index)
                        {
                            signal_index_delete_by_node (
                                signal_index,
                                (struct signal_index_node_t *) storage->dirty_records->dirt_source_index_node);
                        }
                        else
                        {
                            const kan_repository_signal_value_t old_value =
                                (kan_repository_signal_value_t) indexed_field_baked_data_extract_unsigned_from_buffer (
                                    &signal_index->baked, storage->dirty_records->observation_buffer_memory);

                            if (old_value == signal_index->signal_value)
                            {
                                signal_index_delete_by_record (signal_index, node);
                            }
                        }

                        signal_index_insert_record (signal_index, node);
                    }

                    signal_index = signal_index->next;
                }

                struct interval_index_t *interval_index = storage->first_interval_index;
                while (interval_index)
                {
                    if (interval_index->observation_flags & storage->dirty_records->observation_comparison_flags)
                    {
                        if (interval_index == storage->dirty_records->dirt_source_index)
                        {
                            interval_index_delete_by_sub_node (
                                interval_index,
                                (struct interval_index_node_t *) storage->dirty_records->dirt_source_index_node,
                                (struct interval_index_sub_node_t *)
                                    storage->dirty_records->dirt_source_index_sub_node);
                        }
                        else
                        {
                            const kan_avl_tree_value_t converted_value = (kan_avl_tree_value_t)
                                indexed_field_baked_data_extract_and_convert_unsigned_from_buffer (
                                    &interval_index->baked, interval_index->baked_archetype,
                                    storage->dirty_records->observation_buffer_memory);
                            interval_index_delete_by_converted_value (interval_index, node, converted_value);
                        }

                        interval_index_insert_record (interval_index, node);
                    }

                    interval_index = interval_index->next;
                }

                struct space_index_t *space_index = storage->first_space_index;
                while (space_index)
                {
                    if (space_index->observation_flags & storage->dirty_records->observation_comparison_flags)
                    {
                        if (space_index == storage->dirty_records->dirt_source_index)
                        {
                            // We don't use sub nodes for indices as sub node pointers are not stable.
                            KAN_ASSERT (!storage->dirty_records->dirt_source_index_sub_node)

                            struct kan_space_tree_node_t *tree_node = storage->dirty_records->dirt_source_index_node;
                            struct space_index_sub_node_t *sub_nodes = tree_node->sub_nodes;

                            // Shouldn't be too slow as we expect nodes to contain manageable amount of sub nodes.
                            for (kan_loop_size_t sub_node_index = 0u; sub_node_index < tree_node->sub_nodes_count;
                                 ++sub_node_index)
                            {
                                if (sub_nodes[sub_node_index].record == storage->dirty_records->source_node)
                                {
                                    space_index_update_with_sub_node (space_index, tree_node,
                                                                      &sub_nodes[sub_node_index],
                                                                      storage->dirty_records->observation_buffer_memory,
                                                                      &storage->temporary_allocator);
                                }
                            }
                        }
                        else
                        {
                            space_index_update (space_index, storage->dirty_records->observation_buffer_memory,
                                                storage->dirty_records->source_node, &storage->temporary_allocator);
                        }
                    }

                    space_index = space_index->next;
                }

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

            struct signal_index_t *signal_index = storage->first_signal_index;
            while (signal_index)
            {
                signal_index_insert_record (signal_index, node);
                signal_index = signal_index->next;
            }

            struct interval_index_t *interval_index = storage->first_interval_index;
            while (interval_index)
            {
                interval_index_insert_record (interval_index, node);
                interval_index = interval_index->next;
            }

            struct space_index_t *space_index = storage->first_space_index;
            while (space_index)
            {
                space_index_insert_record (space_index, node);
                space_index = space_index->next;
            }

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
                // deletion, it would be reported with incorrect asset references (that were never added for this
                // record previously). But if we send change event too, then asset manager would be able to properly
                // process both asset references change and deletion.
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
                    const kan_hash_t old_hash =
                        (kan_hash_t) storage->dirty_records->observation_buffer_memory ?
                            indexed_field_baked_data_extract_unsigned_from_buffer (
                                &value_index->baked, storage->dirty_records->observation_buffer_memory) :
                            indexed_field_baked_data_extract_unsigned_from_record (&value_index->baked, node->record);

                    value_index_delete_by_hash (value_index, node, old_hash);
                }

                value_index = value_index->next;
            }

            struct signal_index_t *signal_index = storage->first_signal_index;
            while (signal_index)
            {
                if (signal_index == storage->dirty_records->dirt_source_index)
                {
                    signal_index_delete_by_node (
                        signal_index, (struct signal_index_node_t *) storage->dirty_records->dirt_source_index_node);
                }
                else
                {
                    const kan_repository_signal_value_t old_value =
                        (kan_repository_signal_value_t) storage->dirty_records->observation_buffer_memory ?
                            indexed_field_baked_data_extract_unsigned_from_buffer (
                                &signal_index->baked, storage->dirty_records->observation_buffer_memory) :
                            indexed_field_baked_data_extract_unsigned_from_record (&signal_index->baked, node->record);

                    if (old_value == signal_index->signal_value)
                    {
                        signal_index_delete_by_record (signal_index, node);
                    }
                }

                signal_index = signal_index->next;
            }

            struct interval_index_t *interval_index = storage->first_interval_index;
            while (interval_index)
            {
                if (interval_index == storage->dirty_records->dirt_source_index)
                {
                    interval_index_delete_by_sub_node (
                        interval_index, (struct interval_index_node_t *) storage->dirty_records->dirt_source_index_node,
                        (struct interval_index_sub_node_t *) storage->dirty_records->dirt_source_index_sub_node);
                }
                else
                {
                    const kan_avl_tree_value_t converted_value =
                        (kan_avl_tree_value_t) storage->dirty_records->observation_buffer_memory ?
                            indexed_field_baked_data_extract_and_convert_unsigned_from_buffer (
                                &interval_index->baked, interval_index->baked_archetype,
                                storage->dirty_records->observation_buffer_memory) :
                            indexed_field_baked_data_extract_and_convert_unsigned_from_record (
                                &interval_index->baked, interval_index->baked_archetype, node->record);

                    interval_index_delete_by_converted_value (interval_index, node, converted_value);
                }

                interval_index = interval_index->next;
            }

            struct space_index_t *space_index = storage->first_space_index;
            while (space_index)
            {
                if (space_index == storage->dirty_records->dirt_source_index)
                {
                    // We don't use sub nodes for indices as sub node pointers are not stable.
                    KAN_ASSERT (!storage->dirty_records->dirt_source_index_sub_node)

                    struct kan_space_tree_node_t *tree_node = storage->dirty_records->dirt_source_index_node;
                    struct space_index_sub_node_t *sub_nodes = tree_node->sub_nodes;

                    // Shouldn't be too slow as we expect nodes to contain manageable amount of sub nodes.
                    for (kan_loop_size_t sub_node_index = 0u; sub_node_index < tree_node->sub_nodes_count;
                         ++sub_node_index)
                    {
                        if (sub_nodes[sub_node_index].record == storage->dirty_records->source_node)
                        {
                            space_index_delete_by_sub_node (space_index, tree_node, &sub_nodes[sub_node_index],
                                                            storage->dirty_records->observation_buffer_memory,
                                                            &storage->temporary_allocator);
                        }
                    }
                }
                else if (storage->dirty_records->observation_buffer_memory)
                {
                    space_index_delete_by_buffer (space_index, storage->dirty_records->observation_buffer_memory,
                                                  storage->dirty_records->source_node, &storage->temporary_allocator);
                }
                else
                {
                    space_index_delete_by_record (space_index, storage->dirty_records->source_node,
                                                  &storage->temporary_allocator);
                }

                space_index = space_index->next;
            }

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
        kan_hash_storage_update_bucket_count_default (&value_index->hash_storage,
                                                      KAN_REPOSITORY_VALUE_INDEX_INITIAL_BUCKETS);
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

    struct indexed_storage_dirty_record_node_t *node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &storage->temporary_allocator, struct indexed_storage_dirty_record_node_t);

    node->next = storage->dirty_records;
    storage->dirty_records = node;

    if (with_observation_buffer_memory && storage->observation_buffer.buffer_size > 0u)
    {
        node->observation_buffer_memory = kan_stack_group_allocator_allocate (
            &storage->temporary_allocator, storage->observation_buffer.buffer_size, OBSERVATION_BUFFER_CHUNK_ALIGNMENT);
    }
    else
    {
        node->observation_buffer_memory = NULL;
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
    struct indexed_storage_node_t *storage_data = KAN_HANDLE_GET (storage);
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
        kan_allocation_group_stack_push (query_data->storage->records_allocation_group);
        query_data->storage->type->init (query_data->storage->type->functor_user_data, package.record);
        kan_allocation_group_stack_pop ();
    }

    return KAN_PUN_TYPE (struct indexed_insertion_package_t, struct kan_repository_indexed_insertion_package_t,
                         package);
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
    indexed_storage_sequence_query_init (KAN_HANDLE_GET (storage), (struct indexed_sequence_query_t *) query);
}

struct kan_repository_indexed_sequence_read_cursor_t kan_repository_indexed_sequence_read_query_execute (
    struct kan_repository_indexed_sequence_read_query_t *query)
{
    struct indexed_sequence_cursor_t cursor =
        indexed_storage_sequence_query_execute ((struct indexed_sequence_query_t *) query);

    return KAN_PUN_TYPE (struct indexed_sequence_cursor_t, struct kan_repository_indexed_sequence_read_cursor_t,
                         cursor);
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

    return KAN_PUN_TYPE (struct indexed_sequence_constant_access_t,
                         struct kan_repository_indexed_sequence_read_access_t, access);
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
    indexed_storage_sequence_query_init (KAN_HANDLE_GET (storage), (struct indexed_sequence_query_t *) query);
}

struct kan_repository_indexed_sequence_update_cursor_t kan_repository_indexed_sequence_update_query_execute (
    struct kan_repository_indexed_sequence_update_query_t *query)
{
    struct indexed_sequence_cursor_t cursor =
        indexed_storage_sequence_query_execute ((struct indexed_sequence_query_t *) query);

    return KAN_PUN_TYPE (struct indexed_sequence_cursor_t, struct kan_repository_indexed_sequence_update_cursor_t,
                         cursor);
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
        struct indexed_storage_record_node_t *old_node = cursor_data->node;
        cursor_data->node = (struct indexed_storage_record_node_t *) cursor_data->node->list_node.next;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_write_access_try_create (access.storage, old_node))
        {
            access.dirty_node = NULL;
        }
        else
#endif
        {
            access.dirty_node =
                indexed_storage_report_mutable_access_begin (cursor_data->storage, old_node, NULL, NULL, NULL);
            indexed_storage_acquire_access (cursor_data->storage);
        }
    }

    return KAN_PUN_TYPE (struct indexed_sequence_mutable_access_t,
                         struct kan_repository_indexed_sequence_update_access_t, access);
}

void *kan_repository_indexed_sequence_update_access_resolve (
    struct kan_repository_indexed_sequence_update_access_t *access)
{
    struct indexed_sequence_mutable_access_t *access_data = (struct indexed_sequence_mutable_access_t *) access;
    return access_data->dirty_node ? access_data->dirty_node->source_node->record : NULL;
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
    indexed_storage_sequence_query_init (KAN_HANDLE_GET (storage), (struct indexed_sequence_query_t *) query);
}

struct kan_repository_indexed_sequence_delete_cursor_t kan_repository_indexed_sequence_delete_query_execute (
    struct kan_repository_indexed_sequence_delete_query_t *query)
{
    struct indexed_sequence_cursor_t cursor =
        indexed_storage_sequence_query_execute ((struct indexed_sequence_query_t *) query);

    return KAN_PUN_TYPE (struct indexed_sequence_cursor_t, struct kan_repository_indexed_sequence_delete_cursor_t,
                         cursor);
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

    return KAN_PUN_TYPE (struct indexed_sequence_constant_access_t,
                         struct kan_repository_indexed_sequence_delete_access_t, access);
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
    indexed_storage_sequence_query_init (KAN_HANDLE_GET (storage), (struct indexed_sequence_query_t *) query);
}

struct kan_repository_indexed_sequence_write_cursor_t kan_repository_indexed_sequence_write_query_execute (
    struct kan_repository_indexed_sequence_write_query_t *query)
{
    struct indexed_sequence_cursor_t cursor =
        indexed_storage_sequence_query_execute ((struct indexed_sequence_query_t *) query);

    return KAN_PUN_TYPE (struct indexed_sequence_cursor_t, struct kan_repository_indexed_sequence_write_cursor_t,
                         cursor);
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
        struct indexed_storage_record_node_t *old_node = cursor_data->node;
        cursor_data->node = (struct indexed_storage_record_node_t *) cursor_data->node->list_node.next;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_write_access_try_create (access.storage, old_node))
        {
            access.dirty_node = NULL;
        }
        else
#endif
        {
            access.dirty_node =
                indexed_storage_report_mutable_access_begin (cursor_data->storage, old_node, NULL, NULL, NULL);
            indexed_storage_acquire_access (cursor_data->storage);
        }
    }

    return KAN_PUN_TYPE (struct indexed_sequence_mutable_access_t,
                         struct kan_repository_indexed_sequence_write_access_t, access);
}

void *kan_repository_indexed_sequence_write_access_resolve (
    struct kan_repository_indexed_sequence_write_access_t *access)
{
    struct indexed_sequence_mutable_access_t *access_data = (struct indexed_sequence_mutable_access_t *) access;
    return access_data->dirty_node ? access_data->dirty_node->source_node->record : NULL;
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

static inline kan_bool_t try_bake_for_single_field_index (kan_reflection_registry_t registry,
                                                          kan_interned_string_t type_name,
                                                          struct kan_repository_field_path_t path,
                                                          kan_allocation_group_t allocation_group,
                                                          struct indexed_field_baked_data_t *baked_output,
                                                          struct interned_field_path_t *interned_path_output,
                                                          enum kan_reflection_archetype_t *archetype_output,
                                                          kan_bool_t allow_not_comparable)
{
    indexed_field_baked_data_init (baked_output);
    *interned_path_output = copy_field_path (path, allocation_group);

    if (!indexed_field_baked_data_bake_from_reflection (baked_output, registry, type_name, *interned_path_output,
                                                        archetype_output, NULL, allow_not_comparable))
    {
        KAN_LOG (repository, KAN_LOG_ERROR, "Unable to create index for path inside struct \"%s\":", type_name)
        for (uint32_t path_index = 0; path_index < path.reflection_path_length; ++path_index)
        {
            KAN_LOG (repository, KAN_LOG_ERROR, "    - %s", path.reflection_path[path_index])
        }

        shutdown_field_path (*interned_path_output, allocation_group);
        return KAN_FALSE;
    }

    return KAN_TRUE;
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

    struct indexed_field_baked_data_t baked;
    struct interned_field_path_t interned_path;

    if (!try_bake_for_single_field_index (storage->repository->registry, storage->type->name, path,
                                          storage->value_index_allocation_group, &baked, &interned_path, NULL,
                                          KAN_TRUE))
    {
        kan_atomic_int_unlock (&storage->maintenance_lock);
        return NULL;
    }

#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
    if (!validation_single_value_index_does_not_intersect_with_multi_value (storage, &baked))
    {
        kan_atomic_int_unlock (&storage->maintenance_lock);
        shutdown_field_path (interned_path, storage->value_index_allocation_group);
        return NULL;
    }
#endif

    index = kan_allocate_batched (storage->value_index_allocation_group, sizeof (struct value_index_t));
    index->next = storage->first_value_index;
    storage->first_value_index = index;

    index->storage = storage;
    index->queries_count = kan_atomic_int_init (0);
    index->baked = baked;

    kan_hash_storage_init (&index->hash_storage, storage->value_index_allocation_group,
                           KAN_REPOSITORY_VALUE_INDEX_INITIAL_BUCKETS);
    index->source_path = interned_path;

    kan_atomic_int_unlock (&storage->maintenance_lock);
    kan_atomic_int_add (&index->storage->queries_count, 1);
    kan_atomic_int_add (&index->queries_count, 1);
    return index;
}

static inline void indexed_storage_value_query_init (struct indexed_value_query_t *query,
                                                     struct indexed_storage_node_t *storage,
                                                     struct kan_repository_field_path_t path)
{
    *query = (struct indexed_value_query_t) {.index = indexed_storage_find_or_create_value_index (storage, path)};
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
    const kan_hash_t hash = indexed_field_baked_data_extract_unsigned_from_pointer (&query_data->index->baked, value);

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
    indexed_storage_value_query_init ((struct indexed_value_query_t *) query, KAN_HANDLE_GET (storage), path);
}

struct kan_repository_indexed_value_read_cursor_t kan_repository_indexed_value_read_query_execute (
    struct kan_repository_indexed_value_read_query_t *query, const void *value)
{
    struct indexed_value_cursor_t cursor =
        indexed_storage_value_query_execute ((struct indexed_value_query_t *) query, value);

    return KAN_PUN_TYPE (struct indexed_value_cursor_t, struct kan_repository_indexed_value_read_cursor_t, cursor);
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

    if (cursor_data->sub_node)
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

    return KAN_PUN_TYPE (struct indexed_value_constant_access_t, struct kan_repository_indexed_value_read_access_t,
                         access);
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
    indexed_storage_value_query_init ((struct indexed_value_query_t *) query, KAN_HANDLE_GET (storage), path);
}

struct kan_repository_indexed_value_update_cursor_t kan_repository_indexed_value_update_query_execute (
    struct kan_repository_indexed_value_update_query_t *query, const void *value)
{
    struct indexed_value_cursor_t cursor =
        indexed_storage_value_query_execute ((struct indexed_value_query_t *) query, value);

    return KAN_PUN_TYPE (struct indexed_value_cursor_t, struct kan_repository_indexed_value_update_cursor_t, cursor);
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

    if (cursor_data->sub_node)
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

    return KAN_PUN_TYPE (struct indexed_value_mutable_access_t, struct kan_repository_indexed_value_update_access_t,
                         access);
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
    indexed_storage_value_query_init ((struct indexed_value_query_t *) query, KAN_HANDLE_GET (storage), path);
}

struct kan_repository_indexed_value_delete_cursor_t kan_repository_indexed_value_delete_query_execute (
    struct kan_repository_indexed_value_delete_query_t *query, const void *value)
{
    struct indexed_value_cursor_t cursor =
        indexed_storage_value_query_execute ((struct indexed_value_query_t *) query, value);

    return KAN_PUN_TYPE (struct indexed_value_cursor_t, struct kan_repository_indexed_value_delete_cursor_t, cursor);
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

    if (cursor_data->sub_node)
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

    return KAN_PUN_TYPE (struct indexed_value_constant_access_t, struct kan_repository_indexed_value_delete_access_t,
                         access);
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
    indexed_storage_value_query_init ((struct indexed_value_query_t *) query, KAN_HANDLE_GET (storage), path);
}

struct kan_repository_indexed_value_write_cursor_t kan_repository_indexed_value_write_query_execute (
    struct kan_repository_indexed_value_write_query_t *query, const void *value)
{
    struct indexed_value_cursor_t cursor =
        indexed_storage_value_query_execute ((struct indexed_value_query_t *) query, value);

    return KAN_PUN_TYPE (struct indexed_value_cursor_t, struct kan_repository_indexed_value_write_cursor_t, cursor);
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

    if (cursor_data->sub_node)
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

    return KAN_PUN_TYPE (struct indexed_value_mutable_access_t, struct kan_repository_indexed_value_write_access_t,
                         access);
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

static struct signal_index_t *indexed_storage_find_or_create_signal_index (struct indexed_storage_node_t *storage,
                                                                           struct kan_repository_field_path_t path,
                                                                           kan_repository_signal_value_t signal_value)
{
    kan_atomic_int_lock (&storage->maintenance_lock);
    struct signal_index_t *index = storage->first_signal_index;

    while (index)
    {
        if (is_field_path_equal (path, index->source_path) && index->signal_value == signal_value)
        {
            kan_atomic_int_unlock (&storage->maintenance_lock);
            kan_atomic_int_add (&index->storage->queries_count, 1);
            kan_atomic_int_add (&index->queries_count, 1);
            return index;
        }

        index = index->next;
    }

    struct indexed_field_baked_data_t baked;
    struct interned_field_path_t interned_path;

    if (!try_bake_for_single_field_index (storage->repository->registry, storage->type->name, path,
                                          storage->signal_index_allocation_group, &baked, &interned_path, NULL,
                                          KAN_TRUE))
    {
        kan_atomic_int_unlock (&storage->maintenance_lock);
        return NULL;
    }

#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
    if (!validation_single_value_index_does_not_intersect_with_multi_value (storage, &baked))
    {
        kan_atomic_int_unlock (&storage->maintenance_lock);
        shutdown_field_path (interned_path, storage->signal_index_allocation_group);
        return NULL;
    }
#endif

    index = kan_allocate_batched (storage->signal_index_allocation_group, sizeof (struct signal_index_t));
    index->next = storage->first_signal_index;
    storage->first_signal_index = index;

    index->storage = storage;
    index->queries_count = kan_atomic_int_init (0);
    index->signal_value = signal_value;
    index->baked = baked;

    index->first_node = NULL;
    index->initial_fill_executed = KAN_FALSE;
    index->source_path = interned_path;

    kan_atomic_int_unlock (&storage->maintenance_lock);
    kan_atomic_int_add (&index->storage->queries_count, 1);
    kan_atomic_int_add (&index->queries_count, 1);
    return index;
}

static inline void indexed_storage_signal_query_init (struct indexed_signal_query_t *query,
                                                      struct indexed_storage_node_t *storage,
                                                      struct kan_repository_field_path_t path,
                                                      kan_repository_signal_value_t signal_value)
{
    *query = (struct indexed_signal_query_t) {
        .index = indexed_storage_find_or_create_signal_index (storage, path, signal_value)};
}

static inline struct indexed_signal_cursor_t indexed_storage_signal_query_execute (struct indexed_signal_query_t *query)
{
    struct indexed_signal_query_t *query_data = (struct indexed_signal_query_t *) query;

    // Handle queries for invalid indices.
    if (!query_data->index)
    {
        return (struct indexed_signal_cursor_t) {
            .index = NULL,
            .node = NULL,
        };
    }

    indexed_storage_acquire_access (query_data->index->storage);
    return (struct indexed_signal_cursor_t) {
        .index = query_data->index,
        .node = query_data->index->first_node,
    };
}

static inline void indexed_storage_signal_cursor_close (struct indexed_signal_cursor_t *cursor)
{
    if (cursor->index)
    {
        indexed_storage_release_access (cursor->index->storage);
    }
}

static inline void indexed_storage_signal_query_shutdown (struct indexed_signal_query_t *query)
{
    if (query->index)
    {
        kan_atomic_int_add (&query->index->queries_count, -1);
        kan_atomic_int_add (&query->index->storage->queries_count, -1);
        query->index = NULL;
    }
}

void kan_repository_indexed_signal_read_query_init (struct kan_repository_indexed_signal_read_query_t *query,
                                                    kan_repository_indexed_storage_t storage,
                                                    struct kan_repository_field_path_t path,
                                                    kan_repository_signal_value_t signal_value)
{
    indexed_storage_signal_query_init ((struct indexed_signal_query_t *) query, KAN_HANDLE_GET (storage), path,
                                       signal_value);
}

struct kan_repository_indexed_signal_read_cursor_t kan_repository_indexed_signal_read_query_execute (
    struct kan_repository_indexed_signal_read_query_t *query)
{
    struct indexed_signal_cursor_t cursor =
        indexed_storage_signal_query_execute ((struct indexed_signal_query_t *) query);

    return KAN_PUN_TYPE (struct indexed_signal_cursor_t, struct kan_repository_indexed_signal_read_cursor_t, cursor);
}

struct kan_repository_indexed_signal_read_access_t kan_repository_indexed_signal_read_cursor_next (
    struct kan_repository_indexed_signal_read_cursor_t *cursor)
{
    struct indexed_signal_cursor_t *cursor_data = (struct indexed_signal_cursor_t *) cursor;
    struct indexed_signal_constant_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->node,
    };

    if (cursor_data->node)
    {
        cursor_data->node = cursor_data->node->next;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_read_access_try_create (access.index->storage, access.node->record))
        {
            access.node = NULL;
        }
        else
#endif
        {
            indexed_storage_acquire_access (cursor_data->index->storage);
        }
    }

    return KAN_PUN_TYPE (struct indexed_signal_constant_access_t, struct kan_repository_indexed_signal_read_access_t,
                         access);
}

const void *kan_repository_indexed_signal_read_access_resolve (
    struct kan_repository_indexed_signal_read_access_t *access)
{
    struct indexed_signal_constant_access_t *access_data = (struct indexed_signal_constant_access_t *) access;
    return access_data->node ? access_data->node->record->record : NULL;
}

void kan_repository_indexed_signal_read_access_close (struct kan_repository_indexed_signal_read_access_t *access)
{
    struct indexed_signal_constant_access_t *access_data = (struct indexed_signal_constant_access_t *) access;
    if (access_data->node)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_indexed_read_access_destroyed (access_data->node->record);
#endif
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_signal_read_cursor_close (struct kan_repository_indexed_signal_read_cursor_t *cursor)
{
    indexed_storage_signal_cursor_close ((struct indexed_signal_cursor_t *) cursor);
}

void kan_repository_indexed_signal_read_query_shutdown (struct kan_repository_indexed_signal_read_query_t *query)
{
    indexed_storage_signal_query_shutdown ((struct indexed_signal_query_t *) query);
}

void kan_repository_indexed_signal_update_query_init (struct kan_repository_indexed_signal_update_query_t *query,
                                                      kan_repository_indexed_storage_t storage,
                                                      struct kan_repository_field_path_t path,
                                                      kan_repository_signal_value_t signal_value)
{
    indexed_storage_signal_query_init ((struct indexed_signal_query_t *) query, KAN_HANDLE_GET (storage), path,
                                       signal_value);
}

struct kan_repository_indexed_signal_update_cursor_t kan_repository_indexed_signal_update_query_execute (
    struct kan_repository_indexed_signal_update_query_t *query)
{
    struct indexed_signal_cursor_t cursor =
        indexed_storage_signal_query_execute ((struct indexed_signal_query_t *) query);

    return KAN_PUN_TYPE (struct indexed_signal_cursor_t, struct kan_repository_indexed_signal_update_cursor_t, cursor);
}

struct kan_repository_indexed_signal_update_access_t kan_repository_indexed_signal_update_cursor_next (
    struct kan_repository_indexed_signal_update_cursor_t *cursor)
{
    struct indexed_signal_cursor_t *cursor_data = (struct indexed_signal_cursor_t *) cursor;
    struct indexed_signal_mutable_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->node,
        .dirty_node = NULL,
    };

    if (cursor_data->node)
    {
        cursor_data->node = cursor_data->node->next;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_write_access_try_create (access.index->storage, access.node->record))
        {
            access.node = NULL;
        }
        else
#endif
        {
            access.dirty_node = indexed_storage_report_mutable_access_begin (access.index->storage, access.node->record,
                                                                             access.index, access.node, NULL);
            indexed_storage_acquire_access (cursor_data->index->storage);
        }
    }

    return KAN_PUN_TYPE (struct indexed_signal_mutable_access_t, struct kan_repository_indexed_signal_update_access_t,
                         access);
}

void *kan_repository_indexed_signal_update_access_resolve (struct kan_repository_indexed_signal_update_access_t *access)
{
    struct indexed_signal_mutable_access_t *access_data = (struct indexed_signal_mutable_access_t *) access;
    return access_data->node ? access_data->node->record->record : NULL;
}

void kan_repository_indexed_signal_update_access_close (struct kan_repository_indexed_signal_update_access_t *access)
{
    struct indexed_signal_mutable_access_t *access_data = (struct indexed_signal_mutable_access_t *) access;
    if (access_data->dirty_node)
    {
        indexed_storage_report_mutable_access_end (access_data->index->storage, access_data->dirty_node);
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_signal_update_cursor_close (struct kan_repository_indexed_signal_update_cursor_t *cursor)
{
    indexed_storage_signal_cursor_close ((struct indexed_signal_cursor_t *) cursor);
}

void kan_repository_indexed_signal_update_query_shutdown (struct kan_repository_indexed_signal_update_query_t *query)
{
    indexed_storage_signal_query_shutdown ((struct indexed_signal_query_t *) query);
}

void kan_repository_indexed_signal_delete_query_init (struct kan_repository_indexed_signal_delete_query_t *query,
                                                      kan_repository_indexed_storage_t storage,
                                                      struct kan_repository_field_path_t path,
                                                      kan_repository_signal_value_t signal_value)
{
    indexed_storage_signal_query_init ((struct indexed_signal_query_t *) query, KAN_HANDLE_GET (storage), path,
                                       signal_value);
}

struct kan_repository_indexed_signal_delete_cursor_t kan_repository_indexed_signal_delete_query_execute (
    struct kan_repository_indexed_signal_delete_query_t *query)
{
    struct indexed_signal_cursor_t cursor =
        indexed_storage_signal_query_execute ((struct indexed_signal_query_t *) query);

    return KAN_PUN_TYPE (struct indexed_signal_cursor_t, struct kan_repository_indexed_signal_delete_cursor_t, cursor);
}

struct kan_repository_indexed_signal_delete_access_t kan_repository_indexed_signal_delete_cursor_next (
    struct kan_repository_indexed_signal_delete_cursor_t *cursor)
{
    struct indexed_signal_cursor_t *cursor_data = (struct indexed_signal_cursor_t *) cursor;
    struct indexed_signal_constant_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->node,
    };

    if (cursor_data->node)
    {
        cursor_data->node = cursor_data->node->next;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_write_access_try_create (access.index->storage, access.node->record))
        {
            access.node = NULL;
        }
        else
#endif
        {
            indexed_storage_acquire_access (cursor_data->index->storage);
        }
    }

    return KAN_PUN_TYPE (struct indexed_signal_constant_access_t, struct kan_repository_indexed_signal_delete_access_t,
                         access);
}

const void *kan_repository_indexed_signal_delete_access_resolve (
    struct kan_repository_indexed_signal_delete_access_t *access)
{
    struct indexed_signal_constant_access_t *access_data = (struct indexed_signal_constant_access_t *) access;
    return access_data->node ? access_data->node->record->record : NULL;
}

void kan_repository_indexed_signal_delete_access_delete (struct kan_repository_indexed_signal_delete_access_t *access)
{
    struct indexed_signal_constant_access_t *access_data = (struct indexed_signal_constant_access_t *) access;
    indexed_storage_report_delete_from_constant_access (access_data->index->storage, access_data->node->record,
                                                        access_data->index, access_data->node, NULL);
    cascade_deleters_definition_fire (&access_data->index->storage->cascade_deleters,
                                      access_data->node->record->record);
    indexed_storage_release_access (access_data->index->storage);
}

void kan_repository_indexed_signal_delete_access_close (struct kan_repository_indexed_signal_delete_access_t *access)
{
    struct indexed_signal_constant_access_t *access_data = (struct indexed_signal_constant_access_t *) access;
    if (access_data->node)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_indexed_write_access_destroyed (access_data->node->record);
#endif
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_signal_delete_cursor_close (struct kan_repository_indexed_signal_delete_cursor_t *cursor)
{
    indexed_storage_signal_cursor_close ((struct indexed_signal_cursor_t *) cursor);
}

void kan_repository_indexed_signal_delete_query_shutdown (struct kan_repository_indexed_signal_delete_query_t *query)
{
    indexed_storage_signal_query_shutdown ((struct indexed_signal_query_t *) query);
}

void kan_repository_indexed_signal_write_query_init (struct kan_repository_indexed_signal_write_query_t *query,
                                                     kan_repository_indexed_storage_t storage,
                                                     struct kan_repository_field_path_t path,
                                                     kan_repository_signal_value_t signal_value)
{
    indexed_storage_signal_query_init ((struct indexed_signal_query_t *) query, KAN_HANDLE_GET (storage), path,
                                       signal_value);
}

struct kan_repository_indexed_signal_write_cursor_t kan_repository_indexed_signal_write_query_execute (
    struct kan_repository_indexed_signal_write_query_t *query)
{
    struct indexed_signal_cursor_t cursor =
        indexed_storage_signal_query_execute ((struct indexed_signal_query_t *) query);

    return KAN_PUN_TYPE (struct indexed_signal_cursor_t, struct kan_repository_indexed_signal_write_cursor_t, cursor);
}

struct kan_repository_indexed_signal_write_access_t kan_repository_indexed_signal_write_cursor_next (
    struct kan_repository_indexed_signal_write_cursor_t *cursor)
{
    struct indexed_signal_cursor_t *cursor_data = (struct indexed_signal_cursor_t *) cursor;
    struct indexed_signal_mutable_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->node,
        .dirty_node = NULL,
    };

    if (cursor_data->node)
    {
        cursor_data->node = cursor_data->node->next;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_write_access_try_create (access.index->storage, access.node->record))
        {
            access.node = NULL;
        }
        else
#endif
        {
            access.dirty_node = indexed_storage_report_mutable_access_begin (access.index->storage, access.node->record,
                                                                             access.index, access.node, NULL);
            indexed_storage_acquire_access (cursor_data->index->storage);
        }
    }

    return KAN_PUN_TYPE (struct indexed_signal_mutable_access_t, struct kan_repository_indexed_signal_write_access_t,
                         access);
}

void *kan_repository_indexed_signal_write_access_resolve (struct kan_repository_indexed_signal_write_access_t *access)
{
    struct indexed_signal_mutable_access_t *access_data = (struct indexed_signal_mutable_access_t *) access;
    return access_data->node ? access_data->node->record->record : NULL;
}

void kan_repository_indexed_signal_write_access_delete (struct kan_repository_indexed_signal_write_access_t *access)
{
    struct indexed_signal_mutable_access_t *access_data = (struct indexed_signal_mutable_access_t *) access;
    KAN_ASSERT (access_data->dirty_node)
    indexed_storage_report_delete_from_mutable_access (access_data->index->storage, access_data->dirty_node);
    cascade_deleters_definition_fire (&access_data->index->storage->cascade_deleters,
                                      access_data->node->record->record);
    indexed_storage_release_access (access_data->index->storage);
}

void kan_repository_indexed_signal_write_access_close (struct kan_repository_indexed_signal_write_access_t *access)
{
    struct indexed_signal_mutable_access_t *access_data = (struct indexed_signal_mutable_access_t *) access;
    if (access_data->dirty_node)
    {
        indexed_storage_report_mutable_access_end (access_data->index->storage, access_data->dirty_node);
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_signal_write_cursor_close (struct kan_repository_indexed_signal_write_cursor_t *cursor)
{
    indexed_storage_signal_cursor_close ((struct indexed_signal_cursor_t *) cursor);
}

void kan_repository_indexed_signal_write_query_shutdown (struct kan_repository_indexed_signal_write_query_t *query)
{
    indexed_storage_signal_query_shutdown ((struct indexed_signal_query_t *) query);
}

static struct interval_index_t *indexed_storage_find_or_create_interval_index (struct indexed_storage_node_t *storage,
                                                                               struct kan_repository_field_path_t path)
{
    kan_atomic_int_lock (&storage->maintenance_lock);
    struct interval_index_t *index = storage->first_interval_index;

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

    struct indexed_field_baked_data_t baked;
    enum kan_reflection_archetype_t baked_archetype;
    struct interned_field_path_t interned_path;

    if (!try_bake_for_single_field_index (storage->repository->registry, storage->type->name, path,
                                          storage->interval_index_allocation_group, &baked, &interned_path,
                                          &baked_archetype, KAN_FALSE))
    {
        kan_atomic_int_unlock (&storage->maintenance_lock);
        return NULL;
    }

#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
    if (!validation_single_value_index_does_not_intersect_with_multi_value (storage, &baked))
    {
        kan_atomic_int_unlock (&storage->maintenance_lock);
        shutdown_field_path (interned_path, storage->interval_index_allocation_group);
        return NULL;
    }
#endif

    index = kan_allocate_batched (storage->interval_index_allocation_group, sizeof (struct interval_index_t));
    index->next = storage->first_interval_index;
    storage->first_interval_index = index;

    index->storage = storage;
    index->queries_count = kan_atomic_int_init (0);
    index->baked = baked;
    index->baked_archetype = baked_archetype;

    kan_avl_tree_init (&index->tree);
    index->source_path = interned_path;

    kan_atomic_int_unlock (&storage->maintenance_lock);
    kan_atomic_int_add (&index->storage->queries_count, 1);
    kan_atomic_int_add (&index->queries_count, 1);
    return index;
}

static inline void indexed_storage_interval_query_init (struct indexed_interval_query_t *query,
                                                        struct indexed_storage_node_t *storage,
                                                        struct kan_repository_field_path_t path)
{
    *query = (struct indexed_interval_query_t) {.index = indexed_storage_find_or_create_interval_index (storage, path)};
}

static inline void indexed_storage_interval_ascending_cursor_fix_floating (struct indexed_interval_cursor_t *cursor);

static inline void indexed_storage_interval_descending_cursor_fix_floating (struct indexed_interval_cursor_t *cursor);

static inline struct indexed_interval_cursor_t indexed_storage_interval_query_execute (
    struct indexed_interval_query_t *query, const void *min, const void *max, kan_bool_t ascending)
{
    struct indexed_interval_query_t *query_data = (struct indexed_interval_query_t *) query;

    // Handle queries for invalid indices.
    if (!query_data->index)
    {
        return (struct indexed_interval_cursor_t) {
            .index = NULL,
            .current_node = NULL,
            .end_node = NULL,
            .sub_node = NULL,
        };
    }

    indexed_storage_acquire_access (query_data->index->storage);
    struct interval_index_node_t *min_node = NULL;
    struct interval_index_node_t *max_node = NULL;

    if (min)
    {
        min_node = (struct interval_index_node_t *) kan_avl_tree_find_lower_bound (
            &query_data->index->tree, indexed_field_baked_data_extract_and_convert_unsigned_from_pointer (
                                          &query_data->index->baked, query_data->index->baked_archetype, min));
    }

    if (max)
    {
        max_node = (struct interval_index_node_t *) kan_avl_tree_find_upper_bound (
            &query_data->index->tree, indexed_field_baked_data_extract_and_convert_unsigned_from_pointer (
                                          &query_data->index->baked, query_data->index->baked_archetype, max));
    }

    struct indexed_interval_cursor_t cursor;
    cursor.index = query_data->index;

    if (ascending)
    {
        cursor.current_node =
            (struct interval_index_node_t *) (min_node ?
                                                  kan_avl_tree_ascending_iteration_next (&min_node->node) :
                                                  kan_avl_tree_ascending_iteration_begin (&query_data->index->tree));
        cursor.end_node = max_node;
    }
    else
    {
        cursor.current_node =
            (struct interval_index_node_t *) (max_node ?
                                                  kan_avl_tree_descending_iteration_next (&max_node->node) :
                                                  kan_avl_tree_descending_iteration_begin (&query_data->index->tree));
        cursor.end_node = min_node;
    }

    if (cursor.current_node && cursor.current_node != cursor.end_node)
    {
        cursor.sub_node = cursor.current_node->first_sub_node;
    }
    else
    {
        cursor.sub_node = NULL;
    }

    if (query_data->index->baked_archetype == KAN_REFLECTION_ARCHETYPE_FLOATING)
    {
        switch (query_data->index->baked.size)
        {
        case 4u:
            cursor.min_floating = min ? *((float *) min) : -INFINITY;
            cursor.max_floating = max ? *((float *) max) : INFINITY;
            break;

        case 8u:
            cursor.min_floating = (kan_repository_indexed_floating_t) (min ? *((double *) min) : -INFINITY);
            cursor.max_floating = (kan_repository_indexed_floating_t) (max ? *((double *) max) : INFINITY);
            break;
        }

        if (ascending)
        {
            indexed_storage_interval_ascending_cursor_fix_floating (&cursor);
        }
        else
        {
            indexed_storage_interval_descending_cursor_fix_floating (&cursor);
        }
    }

    return cursor;
}

#define HELPER_INTERVAL_CURSOR_NEXT(TYPE)                                                                              \
    static inline void indexed_storage_interval_##TYPE##_cursor_next (struct indexed_interval_cursor_t *cursor)        \
    {                                                                                                                  \
        if (cursor->current_node == cursor->end_node)                                                                  \
        {                                                                                                              \
            return;                                                                                                    \
        }                                                                                                              \
                                                                                                                       \
        if (cursor->sub_node)                                                                                          \
        {                                                                                                              \
            cursor->sub_node = cursor->sub_node->next;                                                                 \
        }                                                                                                              \
                                                                                                                       \
        if (!cursor->sub_node)                                                                                         \
        {                                                                                                              \
            cursor->current_node =                                                                                     \
                (struct interval_index_node_t *) kan_avl_tree_##TYPE##_iteration_next (&cursor->current_node->node);   \
                                                                                                                       \
            if (cursor->current_node != cursor->end_node)                                                              \
            {                                                                                                          \
                cursor->sub_node = cursor->current_node->first_sub_node;                                               \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                cursor->sub_node = NULL;                                                                               \
            }                                                                                                          \
        }                                                                                                              \
    }

HELPER_INTERVAL_CURSOR_NEXT (ascending)
HELPER_INTERVAL_CURSOR_NEXT (descending)
#undef HELPER_INTERVAL_CURSOR_NEXT

static inline void indexed_storage_interval_ascending_cursor_fix_floating (struct indexed_interval_cursor_t *cursor)
{
    if (cursor->index->baked_archetype != KAN_REFLECTION_ARCHETYPE_FLOATING)
    {
        return;
    }

    while (cursor->sub_node)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_read_access_try_create (cursor->index->storage, cursor->sub_node->record))
        {
            cursor->sub_node = NULL;
            break;
        }
#endif

        const kan_repository_indexed_floating_t value = indexed_field_baked_data_extract_floating_from_record (
            &cursor->index->baked, cursor->sub_node->record->record);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_indexed_read_access_destroyed (cursor->sub_node->record);
#endif

        if (value >= cursor->min_floating && value <= cursor->max_floating)
        {
            break;
        }

        indexed_storage_interval_ascending_cursor_next (cursor);
    }
}

static inline void indexed_storage_interval_descending_cursor_fix_floating (struct indexed_interval_cursor_t *cursor)
{
    if (cursor->index->baked_archetype != KAN_REFLECTION_ARCHETYPE_FLOATING)
    {
        return;
    }

    while (cursor->sub_node)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_read_access_try_create (cursor->index->storage, cursor->sub_node->record))
        {
            cursor->sub_node = NULL;
            break;
        }
#endif

        const kan_repository_indexed_floating_t value = indexed_field_baked_data_extract_floating_from_record (
            &cursor->index->baked, cursor->sub_node->record->record);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_indexed_read_access_destroyed (cursor->sub_node->record);
#endif

        if (value >= cursor->min_floating && value <= cursor->max_floating)
        {
            break;
        }

        indexed_storage_interval_descending_cursor_next (cursor);
    }
}

static inline void indexed_storage_interval_cursor_close (struct indexed_interval_cursor_t *cursor)
{
    if (cursor->index)
    {
        indexed_storage_release_access (cursor->index->storage);
    }
}

static inline void indexed_storage_interval_query_shutdown (struct indexed_interval_query_t *query)
{
    if (query->index)
    {
        kan_atomic_int_add (&query->index->queries_count, -1);
        kan_atomic_int_add (&query->index->storage->queries_count, -1);
        query->index = NULL;
    }
}

void kan_repository_indexed_interval_read_query_init (struct kan_repository_indexed_interval_read_query_t *query,
                                                      kan_repository_indexed_storage_t storage,
                                                      struct kan_repository_field_path_t path)
{
    indexed_storage_interval_query_init ((struct indexed_interval_query_t *) query, KAN_HANDLE_GET (storage), path);
}

struct kan_repository_indexed_interval_ascending_read_cursor_t
kan_repository_indexed_interval_read_query_execute_ascending (
    struct kan_repository_indexed_interval_read_query_t *query, const void *min, const void *max)
{
    struct indexed_interval_cursor_t cursor =
        indexed_storage_interval_query_execute ((struct indexed_interval_query_t *) query, min, max, KAN_TRUE);

    return KAN_PUN_TYPE (struct indexed_interval_cursor_t,
                         struct kan_repository_indexed_interval_ascending_read_cursor_t, cursor);
}

struct kan_repository_indexed_interval_descending_read_cursor_t
kan_repository_indexed_interval_read_query_execute_descending (
    struct kan_repository_indexed_interval_read_query_t *query, const void *min, const void *max)
{
    struct indexed_interval_cursor_t cursor =
        indexed_storage_interval_query_execute ((struct indexed_interval_query_t *) query, min, max, KAN_FALSE);

    return KAN_PUN_TYPE (struct indexed_interval_cursor_t,
                         struct kan_repository_indexed_interval_descending_read_cursor_t, cursor);
}

struct kan_repository_indexed_interval_read_access_t kan_repository_indexed_interval_ascending_read_cursor_next (
    struct kan_repository_indexed_interval_ascending_read_cursor_t *cursor)
{
    struct indexed_interval_cursor_t *cursor_data = (struct indexed_interval_cursor_t *) cursor;
    struct indexed_interval_constant_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->current_node,
        .sub_node = cursor_data->sub_node,
    };

    if (cursor_data->sub_node)
    {
        indexed_storage_interval_ascending_cursor_next (cursor_data);
        indexed_storage_interval_ascending_cursor_fix_floating (cursor_data);

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

    return KAN_PUN_TYPE (struct indexed_interval_constant_access_t,
                         struct kan_repository_indexed_interval_read_access_t, access);
}

struct kan_repository_indexed_interval_read_access_t kan_repository_indexed_interval_descending_read_cursor_next (
    struct kan_repository_indexed_interval_descending_read_cursor_t *cursor)
{
    struct indexed_interval_cursor_t *cursor_data = (struct indexed_interval_cursor_t *) cursor;
    struct indexed_interval_constant_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->current_node,
        .sub_node = cursor_data->sub_node,
    };

    if (cursor_data->sub_node)
    {
        indexed_storage_interval_descending_cursor_next (cursor_data);
        indexed_storage_interval_descending_cursor_fix_floating (cursor_data);

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

    return KAN_PUN_TYPE (struct indexed_interval_constant_access_t,
                         struct kan_repository_indexed_interval_read_access_t, access);
}

const void *kan_repository_indexed_interval_read_access_resolve (
    struct kan_repository_indexed_interval_read_access_t *access)
{
    struct indexed_interval_constant_access_t *access_data = (struct indexed_interval_constant_access_t *) access;
    return access_data->sub_node ? access_data->sub_node->record->record : NULL;
}

void kan_repository_indexed_interval_read_access_close (struct kan_repository_indexed_interval_read_access_t *access)
{
    struct indexed_interval_constant_access_t *access_data = (struct indexed_interval_constant_access_t *) access;
    if (access_data->sub_node)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_indexed_read_access_destroyed (access_data->sub_node->record);
#endif
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_interval_ascending_read_cursor_close (
    struct kan_repository_indexed_interval_ascending_read_cursor_t *cursor)
{
    indexed_storage_interval_cursor_close ((struct indexed_interval_cursor_t *) cursor);
}

void kan_repository_indexed_interval_descending_read_cursor_close (
    struct kan_repository_indexed_interval_descending_read_cursor_t *cursor)
{
    indexed_storage_interval_cursor_close ((struct indexed_interval_cursor_t *) cursor);
}

void kan_repository_indexed_interval_read_query_shutdown (struct kan_repository_indexed_interval_read_query_t *query)
{
    indexed_storage_interval_query_shutdown ((struct indexed_interval_query_t *) query);
}

void kan_repository_indexed_interval_update_query_init (struct kan_repository_indexed_interval_update_query_t *query,
                                                        kan_repository_indexed_storage_t storage,
                                                        struct kan_repository_field_path_t path)
{
    indexed_storage_interval_query_init ((struct indexed_interval_query_t *) query, KAN_HANDLE_GET (storage), path);
}

struct kan_repository_indexed_interval_ascending_update_cursor_t
kan_repository_indexed_interval_update_query_execute_ascending (
    struct kan_repository_indexed_interval_update_query_t *query, const void *min, const void *max)
{
    struct indexed_interval_cursor_t cursor =
        indexed_storage_interval_query_execute ((struct indexed_interval_query_t *) query, min, max, KAN_TRUE);

    return KAN_PUN_TYPE (struct indexed_interval_cursor_t,
                         struct kan_repository_indexed_interval_ascending_update_cursor_t, cursor);
}

struct kan_repository_indexed_interval_descending_update_cursor_t
kan_repository_indexed_interval_update_query_execute_descending (
    struct kan_repository_indexed_interval_update_query_t *query, const void *min, const void *max)
{
    struct indexed_interval_cursor_t cursor =
        indexed_storage_interval_query_execute ((struct indexed_interval_query_t *) query, min, max, KAN_FALSE);

    return KAN_PUN_TYPE (struct indexed_interval_cursor_t,
                         struct kan_repository_indexed_interval_descending_update_cursor_t, cursor);
}

struct kan_repository_indexed_interval_update_access_t kan_repository_indexed_interval_ascending_update_cursor_next (
    struct kan_repository_indexed_interval_ascending_update_cursor_t *cursor)
{
    struct indexed_interval_cursor_t *cursor_data = (struct indexed_interval_cursor_t *) cursor;
    struct indexed_interval_mutable_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->current_node,
        .sub_node = cursor_data->sub_node,
        .dirty_node = NULL,
    };

    if (cursor_data->sub_node)
    {
        indexed_storage_interval_ascending_cursor_next (cursor_data);
        indexed_storage_interval_ascending_cursor_fix_floating (cursor_data);

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

    return KAN_PUN_TYPE (struct indexed_interval_mutable_access_t,
                         struct kan_repository_indexed_interval_update_access_t, access);
}

struct kan_repository_indexed_interval_update_access_t kan_repository_indexed_interval_descending_update_cursor_next (
    struct kan_repository_indexed_interval_descending_update_cursor_t *cursor)
{
    struct indexed_interval_cursor_t *cursor_data = (struct indexed_interval_cursor_t *) cursor;
    struct indexed_interval_mutable_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->current_node,
        .sub_node = cursor_data->sub_node,
        .dirty_node = NULL,
    };

    if (cursor_data->sub_node)
    {
        indexed_storage_interval_descending_cursor_next (cursor_data);
        indexed_storage_interval_descending_cursor_fix_floating (cursor_data);

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

    return KAN_PUN_TYPE (struct indexed_interval_mutable_access_t,
                         struct kan_repository_indexed_interval_update_access_t, access);
}

void *kan_repository_indexed_interval_update_access_resolve (
    struct kan_repository_indexed_interval_update_access_t *access)
{
    struct indexed_interval_mutable_access_t *access_data = (struct indexed_interval_mutable_access_t *) access;
    return access_data->sub_node ? access_data->sub_node->record->record : NULL;
}

void kan_repository_indexed_interval_update_access_close (
    struct kan_repository_indexed_interval_update_access_t *access)
{
    struct indexed_interval_mutable_access_t *access_data = (struct indexed_interval_mutable_access_t *) access;
    if (access_data->dirty_node)
    {
        indexed_storage_report_mutable_access_end (access_data->index->storage, access_data->dirty_node);
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_interval_ascending_update_cursor_close (
    struct kan_repository_indexed_interval_ascending_update_cursor_t *cursor)
{
    indexed_storage_interval_cursor_close ((struct indexed_interval_cursor_t *) cursor);
}

void kan_repository_indexed_interval_descending_update_cursor_close (
    struct kan_repository_indexed_interval_descending_update_cursor_t *cursor)
{
    indexed_storage_interval_cursor_close ((struct indexed_interval_cursor_t *) cursor);
}

void kan_repository_indexed_interval_update_query_shutdown (
    struct kan_repository_indexed_interval_update_query_t *query)
{
    indexed_storage_interval_query_shutdown ((struct indexed_interval_query_t *) query);
}

void kan_repository_indexed_interval_delete_query_init (struct kan_repository_indexed_interval_delete_query_t *query,
                                                        kan_repository_indexed_storage_t storage,
                                                        struct kan_repository_field_path_t path)
{
    indexed_storage_interval_query_init ((struct indexed_interval_query_t *) query, KAN_HANDLE_GET (storage), path);
}

struct kan_repository_indexed_interval_ascending_delete_cursor_t
kan_repository_indexed_interval_delete_query_execute_ascending (
    struct kan_repository_indexed_interval_delete_query_t *query, const void *min, const void *max)
{
    struct indexed_interval_cursor_t cursor =
        indexed_storage_interval_query_execute ((struct indexed_interval_query_t *) query, min, max, KAN_TRUE);

    return KAN_PUN_TYPE (struct indexed_interval_cursor_t,
                         struct kan_repository_indexed_interval_ascending_delete_cursor_t, cursor);
}

struct kan_repository_indexed_interval_descending_delete_cursor_t
kan_repository_indexed_interval_delete_query_execute_descending (
    struct kan_repository_indexed_interval_delete_query_t *query, const void *min, const void *max)
{
    struct indexed_interval_cursor_t cursor =
        indexed_storage_interval_query_execute ((struct indexed_interval_query_t *) query, min, max, KAN_FALSE);

    return KAN_PUN_TYPE (struct indexed_interval_cursor_t,
                         struct kan_repository_indexed_interval_descending_delete_cursor_t, cursor);
}

struct kan_repository_indexed_interval_delete_access_t kan_repository_indexed_interval_ascending_delete_cursor_next (
    struct kan_repository_indexed_interval_ascending_delete_cursor_t *cursor)
{
    struct indexed_interval_cursor_t *cursor_data = (struct indexed_interval_cursor_t *) cursor;
    struct indexed_interval_constant_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->current_node,
        .sub_node = cursor_data->sub_node,
    };

    if (cursor_data->sub_node)
    {
        indexed_storage_interval_ascending_cursor_next (cursor_data);
        indexed_storage_interval_ascending_cursor_fix_floating (cursor_data);

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

    return KAN_PUN_TYPE (struct indexed_interval_constant_access_t,
                         struct kan_repository_indexed_interval_delete_access_t, access);
}

struct kan_repository_indexed_interval_delete_access_t kan_repository_indexed_interval_descending_delete_cursor_next (
    struct kan_repository_indexed_interval_descending_delete_cursor_t *cursor)
{
    struct indexed_interval_cursor_t *cursor_data = (struct indexed_interval_cursor_t *) cursor;
    struct indexed_interval_constant_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->current_node,
        .sub_node = cursor_data->sub_node,
    };

    if (cursor_data->sub_node)
    {
        indexed_storage_interval_descending_cursor_next (cursor_data);
        indexed_storage_interval_descending_cursor_fix_floating (cursor_data);

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

    return KAN_PUN_TYPE (struct indexed_interval_constant_access_t,
                         struct kan_repository_indexed_interval_delete_access_t, access);
}

const void *kan_repository_indexed_interval_delete_access_resolve (
    struct kan_repository_indexed_interval_delete_access_t *access)
{
    struct indexed_interval_constant_access_t *access_data = (struct indexed_interval_constant_access_t *) access;
    return access_data->sub_node ? access_data->sub_node->record->record : NULL;
}

void kan_repository_indexed_interval_delete_access_delete (
    struct kan_repository_indexed_interval_delete_access_t *access)
{
    struct indexed_interval_constant_access_t *access_data = (struct indexed_interval_constant_access_t *) access;
    indexed_storage_report_delete_from_constant_access (access_data->index->storage, access_data->sub_node->record,
                                                        access_data->index, access_data->node, access_data->sub_node);
    cascade_deleters_definition_fire (&access_data->index->storage->cascade_deleters,
                                      access_data->sub_node->record->record);
    indexed_storage_release_access (access_data->index->storage);
}

void kan_repository_indexed_interval_delete_access_close (
    struct kan_repository_indexed_interval_delete_access_t *access)
{
    struct indexed_interval_constant_access_t *access_data = (struct indexed_interval_constant_access_t *) access;
    if (access_data->sub_node)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_indexed_write_access_destroyed (access_data->sub_node->record);
#endif
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_interval_ascending_delete_cursor_close (
    struct kan_repository_indexed_interval_ascending_delete_cursor_t *cursor)
{
    indexed_storage_interval_cursor_close ((struct indexed_interval_cursor_t *) cursor);
}

void kan_repository_indexed_interval_descending_delete_cursor_close (
    struct kan_repository_indexed_interval_descending_delete_cursor_t *cursor)
{
    indexed_storage_interval_cursor_close ((struct indexed_interval_cursor_t *) cursor);
}

void kan_repository_indexed_interval_delete_query_shutdown (
    struct kan_repository_indexed_interval_delete_query_t *query)
{
    indexed_storage_interval_query_shutdown ((struct indexed_interval_query_t *) query);
}

void kan_repository_indexed_interval_write_query_init (struct kan_repository_indexed_interval_write_query_t *query,
                                                       kan_repository_indexed_storage_t storage,
                                                       struct kan_repository_field_path_t path)
{
    indexed_storage_interval_query_init ((struct indexed_interval_query_t *) query, KAN_HANDLE_GET (storage), path);
}

struct kan_repository_indexed_interval_ascending_write_cursor_t
kan_repository_indexed_interval_write_query_execute_ascending (
    struct kan_repository_indexed_interval_write_query_t *query, const void *min, const void *max)
{
    struct indexed_interval_cursor_t cursor =
        indexed_storage_interval_query_execute ((struct indexed_interval_query_t *) query, min, max, KAN_TRUE);

    return KAN_PUN_TYPE (struct indexed_interval_cursor_t,
                         struct kan_repository_indexed_interval_ascending_write_cursor_t, cursor);
}

struct kan_repository_indexed_interval_descending_write_cursor_t
kan_repository_indexed_interval_write_query_execute_descending (
    struct kan_repository_indexed_interval_write_query_t *query, const void *min, const void *max)
{
    struct indexed_interval_cursor_t cursor =
        indexed_storage_interval_query_execute ((struct indexed_interval_query_t *) query, min, max, KAN_FALSE);

    return KAN_PUN_TYPE (struct indexed_interval_cursor_t,
                         struct kan_repository_indexed_interval_descending_write_cursor_t, cursor);
}

struct kan_repository_indexed_interval_write_access_t kan_repository_indexed_interval_ascending_write_cursor_next (
    struct kan_repository_indexed_interval_ascending_write_cursor_t *cursor)
{
    struct indexed_interval_cursor_t *cursor_data = (struct indexed_interval_cursor_t *) cursor;
    struct indexed_interval_mutable_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->current_node,
        .sub_node = cursor_data->sub_node,
        .dirty_node = NULL,
    };

    if (cursor_data->sub_node)
    {
        indexed_storage_interval_ascending_cursor_next (cursor_data);
        indexed_storage_interval_ascending_cursor_fix_floating (cursor_data);

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

    return KAN_PUN_TYPE (struct indexed_interval_mutable_access_t,
                         struct kan_repository_indexed_interval_write_access_t, access);
}

struct kan_repository_indexed_interval_write_access_t kan_repository_indexed_interval_descending_write_cursor_next (
    struct kan_repository_indexed_interval_descending_write_cursor_t *cursor)
{
    struct indexed_interval_cursor_t *cursor_data = (struct indexed_interval_cursor_t *) cursor;
    struct indexed_interval_mutable_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->current_node,
        .sub_node = cursor_data->sub_node,
        .dirty_node = NULL,
    };

    if (cursor_data->sub_node)
    {
        indexed_storage_interval_descending_cursor_next (cursor_data);
        indexed_storage_interval_descending_cursor_fix_floating (cursor_data);

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

    return KAN_PUN_TYPE (struct indexed_interval_mutable_access_t,
                         struct kan_repository_indexed_interval_write_access_t, access);
}

void *kan_repository_indexed_interval_write_access_resolve (
    struct kan_repository_indexed_interval_write_access_t *access)
{
    struct indexed_interval_mutable_access_t *access_data = (struct indexed_interval_mutable_access_t *) access;
    return access_data->sub_node ? access_data->sub_node->record->record : NULL;
}

void kan_repository_indexed_interval_write_access_delete (struct kan_repository_indexed_interval_write_access_t *access)
{
    struct indexed_interval_mutable_access_t *access_data = (struct indexed_interval_mutable_access_t *) access;
    KAN_ASSERT (access_data->dirty_node)
    indexed_storage_report_delete_from_mutable_access (access_data->index->storage, access_data->dirty_node);
    cascade_deleters_definition_fire (&access_data->index->storage->cascade_deleters,
                                      access_data->sub_node->record->record);
    indexed_storage_release_access (access_data->index->storage);
}

void kan_repository_indexed_interval_write_access_close (struct kan_repository_indexed_interval_write_access_t *access)
{
    struct indexed_interval_mutable_access_t *access_data = (struct indexed_interval_mutable_access_t *) access;
    if (access_data->dirty_node)
    {
        indexed_storage_report_mutable_access_end (access_data->index->storage, access_data->dirty_node);
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_interval_ascending_write_cursor_close (
    struct kan_repository_indexed_interval_ascending_write_cursor_t *cursor)
{
    indexed_storage_interval_cursor_close ((struct indexed_interval_cursor_t *) cursor);
}

void kan_repository_indexed_interval_descending_write_cursor_close (
    struct kan_repository_indexed_interval_descending_write_cursor_t *cursor)
{
    indexed_storage_interval_cursor_close ((struct indexed_interval_cursor_t *) cursor);
}

void kan_repository_indexed_interval_write_query_shutdown (struct kan_repository_indexed_interval_write_query_t *query)
{
    indexed_storage_interval_query_shutdown ((struct indexed_interval_query_t *) query);
}

static struct space_index_t *indexed_storage_find_or_create_space_index (struct indexed_storage_node_t *storage,
                                                                         struct kan_repository_field_path_t min_path,
                                                                         struct kan_repository_field_path_t max_path,
                                                                         kan_repository_indexed_floating_t global_min,
                                                                         kan_repository_indexed_floating_t global_max,
                                                                         kan_repository_indexed_floating_t leaf_size)
{
    kan_atomic_int_lock (&storage->maintenance_lock);
    struct space_index_t *index = storage->first_space_index;

    while (index)
    {
        if (is_field_path_equal (min_path, index->source_path_min) &&
            is_field_path_equal (max_path, index->source_path_max))
        {
            if (index->source_global_min == global_min && index->source_global_max == global_max &&
                index->source_leaf_size == leaf_size)
            {
                kan_atomic_int_unlock (&storage->maintenance_lock);
                kan_atomic_int_add (&index->storage->queries_count, 1);
                kan_atomic_int_add (&index->queries_count, 1);
                return index;
            }
            else
            {
                KAN_LOG (repository, KAN_LOG_ERROR,
                         "Detected request for the space index with the same fields but different global bounds. Path "
                         "to min field:")

                for (kan_loop_size_t path_element_index = 0u; path_element_index < min_path.reflection_path_length;
                     ++path_element_index)
                {
                    KAN_LOG (repository, KAN_LOG_ERROR, "    - \"%s\"", min_path.reflection_path[path_element_index])
                }

                KAN_LOG (repository, KAN_LOG_ERROR, "Path to max field:")
                for (kan_loop_size_t path_element_index = 0u; path_element_index < max_path.reflection_path_length;
                     ++path_element_index)
                {
                    KAN_LOG (repository, KAN_LOG_ERROR, "    - \"%s\"", max_path.reflection_path[path_element_index])
                }
            }
        }

        index = index->next;
    }

    struct indexed_field_baked_data_t baked_min;
    enum kan_reflection_archetype_t baked_min_archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT;
    kan_instance_size_t baked_min_count = 0u;
    struct interned_field_path_t interned_min_path;

    indexed_field_baked_data_init (&baked_min);
    interned_min_path = copy_field_path (min_path, storage->space_index_allocation_group);

    const kan_bool_t bake_min_result = indexed_field_baked_data_bake_from_reflection (
        &baked_min, storage->repository->registry, storage->type->name, interned_min_path, &baked_min_archetype,
        &baked_min_count, KAN_FALSE);

    struct indexed_field_baked_data_t baked_max;
    enum kan_reflection_archetype_t baked_max_archetype = KAN_REFLECTION_ARCHETYPE_SIGNED_INT;
    kan_instance_size_t baked_max_count = 0u;
    struct interned_field_path_t interned_max_path;

    indexed_field_baked_data_init (&baked_max);
    interned_max_path = copy_field_path (max_path, storage->space_index_allocation_group);

    const kan_bool_t bake_max_result = indexed_field_baked_data_bake_from_reflection (
        &baked_max, storage->repository->registry, storage->type->name, interned_max_path, &baked_max_archetype,
        &baked_max_count, KAN_FALSE);

    const kan_bool_t archetypes_match = baked_min_archetype == baked_max_archetype;
    const kan_bool_t counts_match = baked_min_count == baked_max_count;
    const kan_bool_t adequate_count =
        baked_min_count > 0u && baked_min_count <= KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS;

    kan_bool_t min_no_intersection = KAN_TRUE;
    kan_bool_t max_no_intersection = KAN_TRUE;

#if defined(KAN_REPOSITORY_VALIDATION_ENABLED)
    min_no_intersection = validation_multi_value_index_does_not_intersect_with_single_value (storage, &baked_min);
    max_no_intersection = validation_multi_value_index_does_not_intersect_with_single_value (storage, &baked_max);
#endif

    if (!bake_min_result || !bake_max_result || !archetypes_match || !counts_match || !adequate_count ||
        !min_no_intersection || !max_no_intersection)
    {
        KAN_LOG (repository, KAN_LOG_ERROR,
                 "Failed to create space index. Min baked: %s. Max baked: %s. Archetypes match: %s. Counts match: %s. "
                 "Count is not zero and supported by space tree: %s. Min does not intersect with single value indices: "
                 "%s. Max does not intersect with single value indices: %s. Path to min field:",
                 bake_min_result ? "yes" : "no", bake_max_result ? "yes" : "no", archetypes_match ? "yes" : "no",
                 counts_match ? "yes" : "no", adequate_count ? "yes" : "no", min_no_intersection ? "yes" : "no",
                 max_no_intersection ? "yes" : "no")

        for (kan_loop_size_t path_element_index = 0u; path_element_index < min_path.reflection_path_length;
             ++path_element_index)
        {
            KAN_LOG (repository, KAN_LOG_ERROR, "    - \"%s\"", min_path.reflection_path[path_element_index])
        }

        KAN_LOG (repository, KAN_LOG_ERROR, "Path to max field:")
        for (kan_loop_size_t path_element_index = 0u; path_element_index < max_path.reflection_path_length;
             ++path_element_index)
        {
            KAN_LOG (repository, KAN_LOG_ERROR, "    - \"%s\"", max_path.reflection_path[path_element_index])
        }

        shutdown_field_path (interned_min_path, storage->space_index_allocation_group);
        shutdown_field_path (interned_max_path, storage->space_index_allocation_group);
        kan_atomic_int_unlock (&storage->maintenance_lock);
        return NULL;
    }

    index = kan_allocate_batched (storage->space_index_allocation_group, sizeof (struct space_index_t));
    index->next = storage->first_space_index;
    storage->first_space_index = index;

    index->storage = storage;
    index->queries_count = kan_atomic_int_init (0);

    index->baked_min = baked_min;
    index->baked_max = baked_max;
    index->baked_archetype = baked_min_archetype;
    index->baked_dimension_count = baked_min_count;

    kan_space_tree_init (&index->tree, storage->space_index_allocation_group, baked_min_count,
                         sizeof (struct space_index_sub_node_t), _Alignof (struct space_index_sub_node_t), global_min,
                         global_max, leaf_size);

    index->source_path_min = interned_min_path;
    index->source_path_max = interned_max_path;
    index->source_global_min = global_min;
    index->source_global_max = global_max;
    index->source_leaf_size = leaf_size;

    kan_atomic_int_unlock (&storage->maintenance_lock);
    kan_atomic_int_add (&index->storage->queries_count, 1);
    kan_atomic_int_add (&index->queries_count, 1);
    return index;
}

static inline void indexed_storage_space_query_init (struct indexed_space_query_t *query,
                                                     struct indexed_storage_node_t *storage,
                                                     struct kan_repository_field_path_t min_path,
                                                     struct kan_repository_field_path_t max_path,
                                                     kan_repository_indexed_floating_t global_min,
                                                     kan_repository_indexed_floating_t global_max,
                                                     kan_repository_indexed_floating_t leaf_size)
{
    *query = (struct indexed_space_query_t) {.index = indexed_storage_find_or_create_space_index (
                                                 storage, min_path, max_path, global_min, global_max, leaf_size)};
}

static inline void indexed_storage_space_shape_cursor_next (struct indexed_space_shape_cursor_t *cursor);

static inline void indexed_storage_space_shape_cursor_fix (struct indexed_space_shape_cursor_t *cursor);

static inline struct indexed_space_shape_cursor_t indexed_storage_space_query_execute_shape (
    struct indexed_space_query_t *query,
    const kan_repository_indexed_floating_t *min,
    const kan_repository_indexed_floating_t *max)
{
    struct indexed_space_query_t *query_data = (struct indexed_space_query_t *) query;

    // Handle queries for invalid indices.
    if (!query_data->index)
    {
        struct indexed_space_shape_cursor_t blank;
        blank.index = NULL;
        blank.iterator.current_node = NULL;
        blank.current_sub_node_index = 0u;
        return blank;
    }

    struct indexed_space_shape_cursor_t cursor;
    cursor.index = query_data->index;

    switch (cursor.index->baked_dimension_count)
    {
    case 4u:
        cursor.min[3u] = min[3u];
        cursor.max[3u] = max[3u];
    case 3u:
        cursor.min[2u] = min[2u];
        cursor.max[2u] = max[2u];
    case 2u:
        cursor.min[1u] = min[1u];
        cursor.max[1u] = max[1u];
    case 1u:
        cursor.min[0u] = min[0u];
        cursor.max[0u] = max[0u];
    }

    indexed_storage_acquire_access (query_data->index->storage);
    cursor.iterator = kan_space_tree_shape_start (&cursor.index->tree, cursor.min, cursor.max);
    cursor.current_sub_node_index = 0u;

    // Special case: first node has no sub nodes. Go to the next one until we find sub nodes or exhaust iterator.
    if (cursor.iterator.current_node &&
        cursor.current_sub_node_index >= cursor.iterator.current_node->sub_nodes_count &&
        !kan_space_tree_shape_is_finished (&cursor.iterator))
    {
        indexed_storage_space_shape_cursor_next (&cursor);
    }

    indexed_storage_space_shape_cursor_fix (&cursor);
    return cursor;
}

static inline void indexed_storage_space_ray_cursor_next (struct indexed_space_ray_cursor_t *cursor);

static inline void indexed_storage_space_ray_cursor_fix (struct indexed_space_ray_cursor_t *cursor);

static inline struct indexed_space_ray_cursor_t indexed_storage_space_query_execute_ray (
    struct indexed_space_query_t *query,
    const kan_repository_indexed_floating_t *origin,
    const kan_repository_indexed_floating_t *direction,
    kan_repository_indexed_floating_t max_time)
{
    struct indexed_space_query_t *query_data = (struct indexed_space_query_t *) query;

    // Handle queries for invalid indices.
    if (!query_data->index)
    {
        struct indexed_space_ray_cursor_t blank;
        blank.index = NULL;
        blank.iterator.current_node = NULL;
        blank.current_sub_node_index = 0u;
        return blank;
    }

    struct indexed_space_ray_cursor_t cursor;
    cursor.index = query_data->index;

    switch (cursor.index->baked_dimension_count)
    {
    case 4u:
        cursor.origin[3u] = origin[3u];
        cursor.direction[3u] = direction[3u];
    case 3u:
        cursor.origin[2u] = origin[2u];
        cursor.direction[2u] = direction[2u];
    case 2u:
        cursor.origin[1u] = origin[1u];
        cursor.direction[1u] = direction[1u];
    case 1u:
        cursor.origin[0u] = origin[0u];
        cursor.direction[0u] = direction[0u];
    }

    cursor.max_time = max_time;
    indexed_storage_acquire_access (query_data->index->storage);
    cursor.iterator = kan_space_tree_ray_start (&cursor.index->tree, cursor.origin, cursor.direction, cursor.max_time);
    cursor.current_sub_node_index = 0u;

    // Special case: first node has no sub nodes. Go to the next one until we find sub nodes or exhaust iterator.
    if (cursor.iterator.current_node &&
        cursor.current_sub_node_index >= cursor.iterator.current_node->sub_nodes_count &&
        !kan_space_tree_ray_is_finished (&cursor.iterator))
    {
        indexed_storage_space_ray_cursor_next (&cursor);
    }

    indexed_storage_space_ray_cursor_fix (&cursor);
    return cursor;
}

static inline void indexed_storage_space_shape_cursor_next (struct indexed_space_shape_cursor_t *cursor)
{
    if (kan_space_tree_shape_is_finished (&cursor->iterator))
    {
        return;
    }

    if (cursor->current_sub_node_index < cursor->iterator.current_node->sub_nodes_count)
    {
        ++cursor->current_sub_node_index;
    }

    while (cursor->current_sub_node_index >= cursor->iterator.current_node->sub_nodes_count)
    {
        kan_space_tree_shape_move_to_next_node (&cursor->index->tree, &cursor->iterator);
        if (kan_space_tree_shape_is_finished (&cursor->iterator))
        {
            return;
        }

        cursor->current_sub_node_index = 0u;
    }
}

static inline void indexed_storage_space_ray_cursor_next (struct indexed_space_ray_cursor_t *cursor)
{
    if (kan_space_tree_ray_is_finished (&cursor->iterator))
    {
        return;
    }

    if (cursor->current_sub_node_index < cursor->iterator.current_node->sub_nodes_count)
    {
        ++cursor->current_sub_node_index;
    }

    while (cursor->current_sub_node_index >= cursor->iterator.current_node->sub_nodes_count)
    {
        kan_space_tree_ray_move_to_next_node (&cursor->index->tree, &cursor->iterator);
        if (kan_space_tree_ray_is_finished (&cursor->iterator))
        {
            return;
        }

        cursor->current_sub_node_index = 0u;
    }
}

static inline void indexed_storage_space_shape_cursor_fix (struct indexed_space_shape_cursor_t *cursor)
{
    while (cursor->iterator.current_node &&
           cursor->current_sub_node_index < cursor->iterator.current_node->sub_nodes_count)
    {
        struct space_index_sub_node_t *sub_node = &(
            (struct space_index_sub_node_t *) cursor->iterator.current_node->sub_nodes)[cursor->current_sub_node_index];

        if (kan_space_tree_shape_is_first_occurrence (&cursor->index->tree, sub_node->object_min, &cursor->iterator))
        {
            kan_bool_t inside = cursor->iterator.is_inner_node;
            if (!inside)
            {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
                if (!safeguard_indexed_read_access_try_create (cursor->index->storage, sub_node->record))
                {
                    cursor->current_sub_node_index = cursor->iterator.current_node->sub_nodes_count;
                    break;
                }
#endif

                kan_repository_indexed_floating_t record_min[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
                indexed_field_baked_data_extract_and_convert_floating_array_from_record (
                    &cursor->index->baked_min, cursor->index->baked_archetype, cursor->index->baked_dimension_count,
                    sub_node->record->record, record_min);

                kan_repository_indexed_floating_t record_max[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
                indexed_field_baked_data_extract_and_convert_floating_array_from_record (
                    &cursor->index->baked_max, cursor->index->baked_archetype, cursor->index->baked_dimension_count,
                    sub_node->record->record, record_max);

                inside = kan_check_if_bounds_intersect (cursor->index->baked_dimension_count, cursor->min, cursor->max,
                                                        record_min, record_max);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
                safeguard_indexed_read_access_destroyed (sub_node->record);
#endif
            }

            if (inside)
            {
                break;
            }
        }

        indexed_storage_space_shape_cursor_next (cursor);
    }
}

static inline void indexed_storage_space_ray_cursor_fix (struct indexed_space_ray_cursor_t *cursor)
{
    while (cursor->iterator.current_node &&
           cursor->current_sub_node_index < cursor->iterator.current_node->sub_nodes_count)
    {
        struct space_index_sub_node_t *sub_node = &(
            (struct space_index_sub_node_t *) cursor->iterator.current_node->sub_nodes)[cursor->current_sub_node_index];

        if (kan_space_tree_ray_is_first_occurrence (&cursor->index->tree, sub_node->object_min, sub_node->object_max,
                                                    &cursor->iterator))
        {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
            if (!safeguard_indexed_read_access_try_create (cursor->index->storage, sub_node->record))
            {
                cursor->current_sub_node_index = cursor->iterator.current_node->sub_nodes_count;
                break;
            }
#endif

            kan_repository_indexed_floating_t record_min[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
            indexed_field_baked_data_extract_and_convert_floating_array_from_record (
                &cursor->index->baked_min, cursor->index->baked_archetype, cursor->index->baked_dimension_count,
                sub_node->record->record, record_min);

            kan_repository_indexed_floating_t record_max[KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS];
            indexed_field_baked_data_extract_and_convert_floating_array_from_record (
                &cursor->index->baked_max, cursor->index->baked_archetype, cursor->index->baked_dimension_count,
                sub_node->record->record, record_max);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
            safeguard_indexed_read_access_destroyed (sub_node->record);
#endif

            struct kan_ray_intersection_output_t output = kan_check_if_ray_and_bounds_intersect (
                cursor->index->baked_dimension_count, record_min, record_max, cursor->origin, cursor->direction);

            if (output.hit && output.time <= cursor->max_time)
            {
                break;
            }
        }

        indexed_storage_space_ray_cursor_next (cursor);
    }
}

static inline void indexed_storage_space_shape_cursor_close (struct indexed_space_shape_cursor_t *cursor)
{
    if (cursor->index)
    {
        indexed_storage_release_access (cursor->index->storage);
    }
}

static inline void indexed_storage_space_ray_cursor_close (struct indexed_space_ray_cursor_t *cursor)
{
    if (cursor->index)
    {
        indexed_storage_release_access (cursor->index->storage);
    }
}

static inline void indexed_storage_space_query_shutdown (struct indexed_space_query_t *query)
{
    if (query->index)
    {
        kan_atomic_int_add (&query->index->queries_count, -1);
        kan_atomic_int_add (&query->index->storage->queries_count, -1);
        query->index = NULL;
    }
}

void kan_repository_indexed_space_read_query_init (struct kan_repository_indexed_space_read_query_t *query,
                                                   kan_repository_indexed_storage_t storage,
                                                   struct kan_repository_field_path_t min_path,
                                                   struct kan_repository_field_path_t max_path,
                                                   kan_repository_indexed_floating_t global_min,
                                                   kan_repository_indexed_floating_t global_max,
                                                   kan_repository_indexed_floating_t leaf_size)
{
    indexed_storage_space_query_init ((struct indexed_space_query_t *) query, KAN_HANDLE_GET (storage), min_path,
                                      max_path, global_min, global_max, leaf_size);
}

struct kan_repository_indexed_space_shape_read_cursor_t kan_repository_indexed_space_read_query_execute_shape (
    struct kan_repository_indexed_space_read_query_t *query,
    const kan_repository_indexed_floating_t *min,
    const kan_repository_indexed_floating_t *max)
{
    KAN_PUN_TYPE_RECEIVE_AND_RETURN (
        struct indexed_space_shape_cursor_t, struct kan_repository_indexed_space_shape_read_cursor_t,
        indexed_storage_space_query_execute_shape ((struct indexed_space_query_t *) query, min, max));
}

struct kan_repository_indexed_space_ray_read_cursor_t kan_repository_indexed_space_read_query_execute_ray (
    struct kan_repository_indexed_space_read_query_t *query,
    const kan_repository_indexed_floating_t *origin,
    const kan_repository_indexed_floating_t *direction,
    kan_repository_indexed_floating_t max_time)
{
    KAN_PUN_TYPE_RECEIVE_AND_RETURN (
        struct indexed_space_ray_cursor_t, struct kan_repository_indexed_space_ray_read_cursor_t,
        indexed_storage_space_query_execute_ray ((struct indexed_space_query_t *) query, origin, direction, max_time));
}

static inline struct space_index_sub_node_t *space_shape_cursor_get_sub_node_safe (
    struct indexed_space_shape_cursor_t *cursor_data)
{
    return cursor_data->iterator.current_node ?
               &((struct space_index_sub_node_t *)
                     cursor_data->iterator.current_node->sub_nodes)[cursor_data->current_sub_node_index] :
               NULL;
}

struct kan_repository_indexed_space_read_access_t kan_repository_indexed_space_shape_read_cursor_next (
    struct kan_repository_indexed_space_shape_read_cursor_t *cursor)
{
    struct indexed_space_shape_cursor_t *cursor_data = (struct indexed_space_shape_cursor_t *) cursor;
    struct indexed_space_constant_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->iterator.current_node,
        .sub_node = space_shape_cursor_get_sub_node_safe (cursor_data),
    };

    if (cursor_data->iterator.current_node)
    {
        indexed_storage_space_shape_cursor_next (cursor_data);
        indexed_storage_space_shape_cursor_fix (cursor_data);

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

    return KAN_PUN_TYPE (struct indexed_space_constant_access_t, struct kan_repository_indexed_space_read_access_t,
                         access);
}

static inline struct space_index_sub_node_t *space_ray_cursor_get_sub_node_safe (
    struct indexed_space_ray_cursor_t *cursor_data)
{
    return cursor_data->iterator.current_node ?
               &((struct space_index_sub_node_t *)
                     cursor_data->iterator.current_node->sub_nodes)[cursor_data->current_sub_node_index] :
               NULL;
}

struct kan_repository_indexed_space_read_access_t kan_repository_indexed_space_ray_read_cursor_next (
    struct kan_repository_indexed_space_ray_read_cursor_t *cursor)
{
    struct indexed_space_ray_cursor_t *cursor_data = (struct indexed_space_ray_cursor_t *) cursor;
    struct indexed_space_constant_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->iterator.current_node,
        .sub_node = space_ray_cursor_get_sub_node_safe (cursor_data),
    };

    if (cursor_data->iterator.current_node)
    {
        indexed_storage_space_ray_cursor_next (cursor_data);
        indexed_storage_space_ray_cursor_fix (cursor_data);

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

    return KAN_PUN_TYPE (struct indexed_space_constant_access_t, struct kan_repository_indexed_space_read_access_t,
                         access);
}

const void *kan_repository_indexed_space_read_access_resolve (struct kan_repository_indexed_space_read_access_t *access)
{
    struct indexed_space_constant_access_t *access_data = (struct indexed_space_constant_access_t *) access;
    return access_data->sub_node ? access_data->sub_node->record->record : NULL;
}

void kan_repository_indexed_space_read_access_close (struct kan_repository_indexed_space_read_access_t *access)
{
    struct indexed_space_constant_access_t *access_data = (struct indexed_space_constant_access_t *) access;
    if (access_data->sub_node)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_indexed_read_access_destroyed (access_data->sub_node->record);
#endif
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_space_shape_read_cursor_close (
    struct kan_repository_indexed_space_shape_read_cursor_t *cursor)
{
    indexed_storage_space_shape_cursor_close ((struct indexed_space_shape_cursor_t *) cursor);
}

void kan_repository_indexed_space_ray_read_cursor_close (struct kan_repository_indexed_space_ray_read_cursor_t *cursor)
{
    indexed_storage_space_ray_cursor_close ((struct indexed_space_ray_cursor_t *) cursor);
}

void kan_repository_indexed_space_read_query_shutdown (struct kan_repository_indexed_space_read_query_t *query)
{
    indexed_storage_space_query_shutdown ((struct indexed_space_query_t *) query);
}

void kan_repository_indexed_space_update_query_init (struct kan_repository_indexed_space_update_query_t *query,
                                                     kan_repository_indexed_storage_t storage,
                                                     struct kan_repository_field_path_t min_path,
                                                     struct kan_repository_field_path_t max_path,
                                                     kan_repository_indexed_floating_t global_min,
                                                     kan_repository_indexed_floating_t global_max,
                                                     kan_repository_indexed_floating_t leaf_size)
{
    indexed_storage_space_query_init ((struct indexed_space_query_t *) query, KAN_HANDLE_GET (storage), min_path,
                                      max_path, global_min, global_max, leaf_size);
}

struct kan_repository_indexed_space_shape_update_cursor_t kan_repository_indexed_space_update_query_execute_shape (
    struct kan_repository_indexed_space_update_query_t *query,
    const kan_repository_indexed_floating_t *min,
    const kan_repository_indexed_floating_t *max)
{
    KAN_PUN_TYPE_RECEIVE_AND_RETURN (
        struct indexed_space_shape_cursor_t, struct kan_repository_indexed_space_shape_update_cursor_t,
        indexed_storage_space_query_execute_shape ((struct indexed_space_query_t *) query, min, max));
}

struct kan_repository_indexed_space_ray_update_cursor_t kan_repository_indexed_space_update_query_execute_ray (
    struct kan_repository_indexed_space_update_query_t *query,
    const kan_repository_indexed_floating_t *origin,
    const kan_repository_indexed_floating_t *direction,
    kan_repository_indexed_floating_t max_time)
{
    KAN_PUN_TYPE_RECEIVE_AND_RETURN (
        struct indexed_space_ray_cursor_t, struct kan_repository_indexed_space_ray_update_cursor_t,
        indexed_storage_space_query_execute_ray ((struct indexed_space_query_t *) query, origin, direction, max_time));
}

struct kan_repository_indexed_space_update_access_t kan_repository_indexed_space_shape_update_cursor_next (
    struct kan_repository_indexed_space_shape_update_cursor_t *cursor)
{
    struct indexed_space_shape_cursor_t *cursor_data = (struct indexed_space_shape_cursor_t *) cursor;
    struct indexed_space_mutable_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->iterator.current_node,
        .sub_node = space_shape_cursor_get_sub_node_safe (cursor_data),
        .dirty_node = NULL,
    };

    if (cursor_data->iterator.current_node)
    {
        indexed_storage_space_shape_cursor_next (cursor_data);
        indexed_storage_space_shape_cursor_fix (cursor_data);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_write_access_try_create (access.index->storage, access.sub_node->record))
        {
            access.sub_node = NULL;
        }
        else
#endif
        {
            access.dirty_node = indexed_storage_report_mutable_access_begin (
                access.index->storage, access.sub_node->record, access.index, access.node, NULL);
            indexed_storage_acquire_access (cursor_data->index->storage);
        }
    }

    return KAN_PUN_TYPE (struct indexed_space_mutable_access_t, struct kan_repository_indexed_space_update_access_t,
                         access);
}

struct kan_repository_indexed_space_update_access_t kan_repository_indexed_space_ray_update_cursor_next (
    struct kan_repository_indexed_space_ray_update_cursor_t *cursor)
{
    struct indexed_space_ray_cursor_t *cursor_data = (struct indexed_space_ray_cursor_t *) cursor;
    struct indexed_space_mutable_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->iterator.current_node,
        .sub_node = space_ray_cursor_get_sub_node_safe (cursor_data),
        .dirty_node = NULL,
    };

    if (cursor_data->iterator.current_node)
    {
        indexed_storage_space_ray_cursor_next (cursor_data);
        indexed_storage_space_ray_cursor_fix (cursor_data);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_write_access_try_create (access.index->storage, access.sub_node->record))
        {
            access.sub_node = NULL;
        }
        else
#endif
        {
            access.dirty_node = indexed_storage_report_mutable_access_begin (
                access.index->storage, access.sub_node->record, access.index, access.node, NULL);
            indexed_storage_acquire_access (cursor_data->index->storage);
        }
    }

    return KAN_PUN_TYPE (struct indexed_space_mutable_access_t, struct kan_repository_indexed_space_update_access_t,
                         access);
}

void *kan_repository_indexed_space_update_access_resolve (struct kan_repository_indexed_space_update_access_t *access)
{
    struct indexed_space_mutable_access_t *access_data = (struct indexed_space_mutable_access_t *) access;
    return access_data->sub_node ? access_data->sub_node->record->record : NULL;
}

void kan_repository_indexed_space_update_access_close (struct kan_repository_indexed_space_update_access_t *access)
{
    struct indexed_space_mutable_access_t *access_data = (struct indexed_space_mutable_access_t *) access;
    if (access_data->dirty_node)
    {
        indexed_storage_report_mutable_access_end (access_data->index->storage, access_data->dirty_node);
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_space_shape_update_cursor_close (
    struct kan_repository_indexed_space_shape_update_cursor_t *cursor)
{
    indexed_storage_space_shape_cursor_close ((struct indexed_space_shape_cursor_t *) cursor);
}

void kan_repository_indexed_space_ray_update_cursor_close (
    struct kan_repository_indexed_space_ray_update_cursor_t *cursor)
{
    indexed_storage_space_ray_cursor_close ((struct indexed_space_ray_cursor_t *) cursor);
}

void kan_repository_indexed_space_update_query_shutdown (struct kan_repository_indexed_space_update_query_t *query)
{
    indexed_storage_space_query_shutdown ((struct indexed_space_query_t *) query);
}

void kan_repository_indexed_space_delete_query_init (struct kan_repository_indexed_space_delete_query_t *query,
                                                     kan_repository_indexed_storage_t storage,
                                                     struct kan_repository_field_path_t min_path,
                                                     struct kan_repository_field_path_t max_path,
                                                     kan_repository_indexed_floating_t global_min,
                                                     kan_repository_indexed_floating_t global_max,
                                                     kan_repository_indexed_floating_t leaf_size)
{
    indexed_storage_space_query_init ((struct indexed_space_query_t *) query, KAN_HANDLE_GET (storage), min_path,
                                      max_path, global_min, global_max, leaf_size);
}

struct kan_repository_indexed_space_shape_delete_cursor_t kan_repository_indexed_space_delete_query_execute_shape (
    struct kan_repository_indexed_space_delete_query_t *query,
    const kan_repository_indexed_floating_t *min,
    const kan_repository_indexed_floating_t *max)
{
    KAN_PUN_TYPE_RECEIVE_AND_RETURN (
        struct indexed_space_shape_cursor_t, struct kan_repository_indexed_space_shape_delete_cursor_t,
        indexed_storage_space_query_execute_shape ((struct indexed_space_query_t *) query, min, max));
}

struct kan_repository_indexed_space_ray_delete_cursor_t kan_repository_indexed_space_delete_query_execute_ray (
    struct kan_repository_indexed_space_delete_query_t *query,
    const kan_repository_indexed_floating_t *origin,
    const kan_repository_indexed_floating_t *direction,
    kan_repository_indexed_floating_t max_time)
{
    KAN_PUN_TYPE_RECEIVE_AND_RETURN (
        struct indexed_space_ray_cursor_t, struct kan_repository_indexed_space_ray_delete_cursor_t,
        indexed_storage_space_query_execute_ray ((struct indexed_space_query_t *) query, origin, direction, max_time));
}

struct kan_repository_indexed_space_delete_access_t kan_repository_indexed_space_shape_delete_cursor_next (
    struct kan_repository_indexed_space_shape_delete_cursor_t *cursor)
{
    struct indexed_space_shape_cursor_t *cursor_data = (struct indexed_space_shape_cursor_t *) cursor;
    struct indexed_space_constant_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->iterator.current_node,
        .sub_node = space_shape_cursor_get_sub_node_safe (cursor_data),
    };

    if (cursor_data->iterator.current_node)
    {
        indexed_storage_space_shape_cursor_next (cursor_data);
        indexed_storage_space_shape_cursor_fix (cursor_data);

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

    return KAN_PUN_TYPE (struct indexed_space_constant_access_t, struct kan_repository_indexed_space_delete_access_t,
                         access);
}

struct kan_repository_indexed_space_delete_access_t kan_repository_indexed_space_ray_delete_cursor_next (
    struct kan_repository_indexed_space_ray_delete_cursor_t *cursor)
{
    struct indexed_space_ray_cursor_t *cursor_data = (struct indexed_space_ray_cursor_t *) cursor;
    struct indexed_space_constant_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->iterator.current_node,
        .sub_node = space_ray_cursor_get_sub_node_safe (cursor_data),
    };

    if (cursor_data->iterator.current_node)
    {
        indexed_storage_space_ray_cursor_next (cursor_data);
        indexed_storage_space_ray_cursor_fix (cursor_data);

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

    return KAN_PUN_TYPE (struct indexed_space_constant_access_t, struct kan_repository_indexed_space_delete_access_t,
                         access);
}

const void *kan_repository_indexed_space_delete_access_resolve (
    struct kan_repository_indexed_space_delete_access_t *access)
{
    struct indexed_space_constant_access_t *access_data = (struct indexed_space_constant_access_t *) access;
    return access_data->sub_node ? access_data->sub_node->record->record : NULL;
}

void kan_repository_indexed_space_delete_access_delete (struct kan_repository_indexed_space_delete_access_t *access)
{
    struct indexed_space_constant_access_t *access_data = (struct indexed_space_constant_access_t *) access;
    indexed_storage_report_delete_from_constant_access (access_data->index->storage, access_data->sub_node->record,
                                                        access_data->index, access_data->node, NULL);
    cascade_deleters_definition_fire (&access_data->index->storage->cascade_deleters,
                                      access_data->sub_node->record->record);
    indexed_storage_release_access (access_data->index->storage);
}

void kan_repository_indexed_space_delete_access_close (struct kan_repository_indexed_space_delete_access_t *access)
{
    struct indexed_space_constant_access_t *access_data = (struct indexed_space_constant_access_t *) access;
    if (access_data->sub_node)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_indexed_write_access_destroyed (access_data->sub_node->record);
#endif
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_space_shape_delete_cursor_close (
    struct kan_repository_indexed_space_shape_delete_cursor_t *cursor)
{
    indexed_storage_space_shape_cursor_close ((struct indexed_space_shape_cursor_t *) cursor);
}

void kan_repository_indexed_space_ray_delete_cursor_close (
    struct kan_repository_indexed_space_ray_delete_cursor_t *cursor)
{
    indexed_storage_space_ray_cursor_close ((struct indexed_space_ray_cursor_t *) cursor);
}

void kan_repository_indexed_space_delete_query_shutdown (struct kan_repository_indexed_space_delete_query_t *query)
{
    indexed_storage_space_query_shutdown ((struct indexed_space_query_t *) query);
}

void kan_repository_indexed_space_write_query_init (struct kan_repository_indexed_space_write_query_t *query,
                                                    kan_repository_indexed_storage_t storage,
                                                    struct kan_repository_field_path_t min_path,
                                                    struct kan_repository_field_path_t max_path,
                                                    kan_repository_indexed_floating_t global_min,
                                                    kan_repository_indexed_floating_t global_max,
                                                    kan_repository_indexed_floating_t leaf_size)
{
    indexed_storage_space_query_init ((struct indexed_space_query_t *) query, KAN_HANDLE_GET (storage), min_path,
                                      max_path, global_min, global_max, leaf_size);
}

struct kan_repository_indexed_space_shape_write_cursor_t kan_repository_indexed_space_write_query_execute_shape (
    struct kan_repository_indexed_space_write_query_t *query,
    const kan_repository_indexed_floating_t *min,
    const kan_repository_indexed_floating_t *max)
{
    KAN_PUN_TYPE_RECEIVE_AND_RETURN (
        struct indexed_space_shape_cursor_t, struct kan_repository_indexed_space_shape_write_cursor_t,
        indexed_storage_space_query_execute_shape ((struct indexed_space_query_t *) query, min, max));
}

struct kan_repository_indexed_space_ray_write_cursor_t kan_repository_indexed_space_write_query_execute_ray (
    struct kan_repository_indexed_space_write_query_t *query,
    const kan_repository_indexed_floating_t *origin,
    const kan_repository_indexed_floating_t *direction,
    kan_repository_indexed_floating_t max_time)
{
    KAN_PUN_TYPE_RECEIVE_AND_RETURN (
        struct indexed_space_ray_cursor_t, struct kan_repository_indexed_space_ray_write_cursor_t,
        indexed_storage_space_query_execute_ray ((struct indexed_space_query_t *) query, origin, direction, max_time));
}

struct kan_repository_indexed_space_write_access_t kan_repository_indexed_space_shape_write_cursor_next (
    struct kan_repository_indexed_space_shape_write_cursor_t *cursor)
{
    struct indexed_space_shape_cursor_t *cursor_data = (struct indexed_space_shape_cursor_t *) cursor;
    struct indexed_space_mutable_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->iterator.current_node,
        .sub_node = space_shape_cursor_get_sub_node_safe (cursor_data),
        .dirty_node = NULL,
    };

    if (cursor_data->iterator.current_node)
    {
        indexed_storage_space_shape_cursor_next (cursor_data);
        indexed_storage_space_shape_cursor_fix (cursor_data);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_write_access_try_create (access.index->storage, access.sub_node->record))
        {
            access.sub_node = NULL;
        }
        else
#endif
        {
            access.dirty_node = indexed_storage_report_mutable_access_begin (
                access.index->storage, access.sub_node->record, access.index, access.node, NULL);
            indexed_storage_acquire_access (cursor_data->index->storage);
        }
    }

    return KAN_PUN_TYPE (struct indexed_space_mutable_access_t, struct kan_repository_indexed_space_write_access_t,
                         access);
}

struct kan_repository_indexed_space_write_access_t kan_repository_indexed_space_ray_write_cursor_next (
    struct kan_repository_indexed_space_ray_write_cursor_t *cursor)
{
    struct indexed_space_ray_cursor_t *cursor_data = (struct indexed_space_ray_cursor_t *) cursor;
    struct indexed_space_mutable_access_t access = {
        .index = cursor_data->index,
        .node = cursor_data->iterator.current_node,
        .sub_node = space_ray_cursor_get_sub_node_safe (cursor_data),
        .dirty_node = NULL,
    };

    if (cursor_data->iterator.current_node)
    {
        indexed_storage_space_ray_cursor_next (cursor_data);
        indexed_storage_space_ray_cursor_fix (cursor_data);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (!safeguard_indexed_write_access_try_create (access.index->storage, access.sub_node->record))
        {
            access.sub_node = NULL;
        }
        else
#endif
        {
            access.dirty_node = indexed_storage_report_mutable_access_begin (
                access.index->storage, access.sub_node->record, access.index, access.node, NULL);
            indexed_storage_acquire_access (cursor_data->index->storage);
        }
    }

    return KAN_PUN_TYPE (struct indexed_space_mutable_access_t, struct kan_repository_indexed_space_write_access_t,
                         access);
}

void *kan_repository_indexed_space_write_access_resolve (struct kan_repository_indexed_space_write_access_t *access)
{
    struct indexed_space_mutable_access_t *access_data = (struct indexed_space_mutable_access_t *) access;
    return access_data->sub_node ? access_data->sub_node->record->record : NULL;
}

void kan_repository_indexed_space_write_access_delete (struct kan_repository_indexed_space_write_access_t *access)
{
    struct indexed_space_mutable_access_t *access_data = (struct indexed_space_mutable_access_t *) access;
    KAN_ASSERT (access_data->dirty_node)
    indexed_storage_report_delete_from_mutable_access (access_data->index->storage, access_data->dirty_node);
    cascade_deleters_definition_fire (&access_data->index->storage->cascade_deleters,
                                      access_data->sub_node->record->record);
    indexed_storage_release_access (access_data->index->storage);
}

void kan_repository_indexed_space_write_access_close (struct kan_repository_indexed_space_write_access_t *access)
{
    struct indexed_space_mutable_access_t *access_data = (struct indexed_space_mutable_access_t *) access;
    if (access_data->dirty_node)
    {
        indexed_storage_report_mutable_access_end (access_data->index->storage, access_data->dirty_node);
        indexed_storage_release_access (access_data->index->storage);
    }
}

void kan_repository_indexed_space_shape_write_cursor_close (
    struct kan_repository_indexed_space_shape_write_cursor_t *cursor)
{
    indexed_storage_space_shape_cursor_close ((struct indexed_space_shape_cursor_t *) cursor);
}

void kan_repository_indexed_space_ray_write_cursor_close (
    struct kan_repository_indexed_space_ray_write_cursor_t *cursor)
{
    indexed_storage_space_ray_cursor_close ((struct indexed_space_ray_cursor_t *) cursor);
}

void kan_repository_indexed_space_write_query_shutdown (struct kan_repository_indexed_space_write_query_t *query)
{
    indexed_storage_space_query_shutdown ((struct indexed_space_query_t *) query);
}

static struct event_storage_node_t *query_event_storage_across_hierarchy (struct repository_t *repository,
                                                                          kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&repository->event_storages, KAN_HASH_OBJECT_POINTER (type_name));
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

kan_repository_event_storage_t kan_repository_event_storage_open (kan_repository_t repository, const char *type_name)
{
    struct repository_t *repository_data = KAN_HANDLE_GET (repository);
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
            return KAN_HANDLE_SET_INVALID (kan_repository_event_storage_t);
        }

        // Uplift event storage to the highest parent repository that might produce this event.
        struct repository_t *target_event_repository = repository_data;
        struct repository_t *uplift_parent_repository = repository_data->parent;

        while (uplift_parent_repository)
        {
            if (type_name_storage_contains (&uplift_parent_repository->event_types_to_uplift, interned_type_name))
            {
                target_event_repository = uplift_parent_repository;
            }

            uplift_parent_repository = uplift_parent_repository->parent;
        }

        if (target_event_repository != repository_data)
        {
            KAN_LOG (repository, KAN_LOG_DEBUG,
                     "Event type \"%s\" was requested in repository \"%s\", but was instead uplifted to \"%s\" in "
                     "order to make automatic event routine work properly.",
                     interned_type_name, repository_data->name, target_event_repository->name)
        }

        const kan_allocation_group_t storage_allocation_group = kan_allocation_group_get_child (
            kan_allocation_group_get_child (target_event_repository->allocation_group, "events"), interned_type_name);

        storage = (struct event_storage_node_t *) kan_allocate_batched (storage_allocation_group,
                                                                        sizeof (struct event_storage_node_t));

        storage->node.hash = KAN_HASH_OBJECT_POINTER (interned_type_name);
        storage->allocation_group = storage_allocation_group;
        storage->type = event_type;
        storage->single_threaded_operations_lock = kan_atomic_int_init (0);
        storage->queries_count = kan_atomic_int_init (0);
        kan_event_queue_init (&storage->event_queue, &event_queue_node_allocate (storage_allocation_group)->node);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        storage->safeguard_access_status = kan_atomic_int_init (0);
#endif

        storage->scheduled_for_destroy = KAN_FALSE;
        kan_hash_storage_update_bucket_count_default (&target_event_repository->event_storages,
                                                      KAN_REPOSITORY_EVENT_STORAGE_INITIAL_BUCKETS);
        kan_hash_storage_add (&target_event_repository->event_storages, &storage->node);
    }

    repository_storage_map_access_unlock (repository_data);
    return KAN_HANDLE_SET (kan_repository_event_storage_t, storage);
}

void kan_repository_event_insert_query_init (struct kan_repository_event_insert_query_t *query,
                                             kan_repository_event_storage_t storage)
{
    struct event_storage_node_t *storage_data = KAN_HANDLE_GET (storage);
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

        return KAN_PUN_TYPE (struct event_insertion_package_t, struct kan_repository_event_insertion_package_t,
                             package);
    }

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    if (!safeguard_event_insertion_package_try_create (query_data->storage))
    {
        package.event = NULL;

        return KAN_PUN_TYPE (struct event_insertion_package_t, struct kan_repository_event_insertion_package_t,
                             package);
    }
#endif

    package.event = kan_allocate_batched (query_data->storage->allocation_group, query_data->storage->type->size);
    if (query_data->storage->type->init)
    {
        kan_allocation_group_stack_push (query_data->storage->allocation_group);
        query_data->storage->type->init (query_data->storage->type->functor_user_data, package.event);
        kan_allocation_group_stack_pop ();
    }

    return KAN_PUN_TYPE (struct event_insertion_package_t, struct kan_repository_event_insertion_package_t, package);
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

    KAN_ASSERT (node)
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
    struct event_storage_node_t *storage_data = KAN_HANDLE_GET (storage);
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
        access.iterator = KAN_HANDLE_SET_INVALID (kan_event_queue_iterator_t);

        return KAN_PUN_TYPE (struct event_read_access_t, struct kan_repository_event_read_access_t, access);
    }

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    if (!safeguard_event_read_access_try_create (query_data->storage))
    {
        access.iterator = KAN_HANDLE_SET_INVALID (kan_event_queue_iterator_t);

        return KAN_PUN_TYPE (struct event_read_access_t, struct kan_repository_event_read_access_t, access);
    }
#endif

    access.iterator = query_data->iterator;
    query_data->iterator =
        kan_event_queue_iterator_create_next (&query_data->storage->event_queue, query_data->iterator);

    return KAN_PUN_TYPE (struct event_read_access_t, struct kan_repository_event_read_access_t, access);
}

const void *kan_repository_event_read_access_resolve (struct kan_repository_event_read_access_t *access)
{
    struct event_read_access_t *access_data = (struct event_read_access_t *) access;
    KAN_ASSERT (access_data->storage)

    if (KAN_HANDLE_IS_VALID (access_data->iterator))
    {
        struct event_queue_node_t *node = (struct event_queue_node_t *) kan_event_queue_iterator_get (
            &access_data->storage->event_queue, access_data->iterator);

        if (node)
        {
            KAN_ASSERT (node->event)
            return node->event;
        }
    }

    return NULL;
}

void kan_repository_event_read_access_close (struct kan_repository_event_read_access_t *access)
{
    struct event_read_access_t *access_data = (struct event_read_access_t *) access;
    KAN_ASSERT (access_data->storage)

    if (KAN_HANDLE_IS_VALID (access_data->iterator))
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

    while (repository->indexed_storages.items.first)
    {
        indexed_storage_node_shutdown_and_free (
            (struct indexed_storage_node_t *) repository->indexed_storages.items.first, repository);
    }

    while (repository->event_storages.items.first)
    {
        event_storage_node_shutdown_and_free ((struct event_storage_node_t *) repository->event_storages.items.first,
                                              repository);
    }

    kan_hash_storage_shutdown (&repository->singleton_storages);
    kan_hash_storage_shutdown (&repository->indexed_storages);
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

    repository_shutdown_utility_planning_storages (repository);
    kan_free_general (repository->allocation_group, repository, sizeof (struct repository_t));
}

static void repository_finish_scheduled_destroy (struct repository_t *repository)
{
    struct repository_t *child_repository = repository->first;
    while (child_repository)
    {
        struct repository_t *next = child_repository->next;
        repository_finish_scheduled_destroy (child_repository);
        child_repository = next;
    }

    if (repository->scheduled_for_destroy)
    {
        // Only child repositories use destroy scheduling feature.
        KAN_ASSERT (repository->parent)
        repository_destroy_internal (repository);
    }
    else
    {
        struct singleton_storage_node_t *singleton_storage_node =
            (struct singleton_storage_node_t *) repository->singleton_storages.items.first;

        while (singleton_storage_node)
        {
            struct singleton_storage_node_t *next =
                (struct singleton_storage_node_t *) singleton_storage_node->node.list_node.next;

            if (singleton_storage_node->scheduled_for_destroy)
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

            if (indexed_storage_node->scheduled_for_destroy)
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

            if (event_storage_node->scheduled_for_destroy)
            {
                event_storage_node_shutdown_and_free (event_storage_node, repository);
            }

            event_storage_node = next;
        }
    }
}

static kan_bool_t repository_is_indexed_storage_can_be_cleared (struct indexed_storage_node_t *indexed_storage_node)
{
    if (indexed_storage_node->candidate_for_cleanup)
    {
        return KAN_TRUE;
    }

    kan_bool_t has_obstacles = KAN_FALSE;
    indexed_storage_node->candidate_for_cleanup = KAN_TRUE;

    struct kan_reflection_struct_meta_iterator_t iterator = kan_reflection_registry_query_struct_meta (
        indexed_storage_node->repository->registry, indexed_storage_node->type->name,
        meta_automatic_cascade_deletion_name);

    struct kan_repository_meta_automatic_cascade_deletion_t *meta =
        (struct kan_repository_meta_automatic_cascade_deletion_t *) kan_reflection_struct_meta_iterator_get (&iterator);

    while (meta)
    {
        kan_interned_string_t child_type_name = kan_string_intern (meta->child_type_name);
        struct indexed_storage_node_t *child_storage =
            query_indexed_storage_across_hierarchy (indexed_storage_node->repository, child_type_name);

        if (child_storage && (kan_atomic_int_get (&child_storage->queries_count) > 0 ||
                              !repository_is_indexed_storage_can_be_cleared (child_storage)))
        {
            KAN_LOG (repository, KAN_LOG_WARNING,
                     "Failed to remove indexed storage for type \"%s\" that has no associated queries, because removal "
                     "would create cause removal of \"%s\" indexed objects that are being used.",
                     indexed_storage_node->type->name, child_storage->type->name)
            has_obstacles = KAN_TRUE;
        }

        kan_reflection_struct_meta_iterator_next (&iterator);
        meta = (struct kan_repository_meta_automatic_cascade_deletion_t *) kan_reflection_struct_meta_iterator_get (
            &iterator);
    }

    iterator = kan_reflection_registry_query_struct_meta (indexed_storage_node->repository->registry,
                                                          indexed_storage_node->type->name,
                                                          meta_automatic_on_delete_event_name);

    struct kan_repository_meta_automatic_on_delete_event_t *event =
        (struct kan_repository_meta_automatic_on_delete_event_t *) kan_reflection_struct_meta_iterator_get (&iterator);

    while (event)
    {
        const kan_interned_string_t event_type = kan_string_intern (event->event_type);
        struct event_storage_node_t *event_storage =
            query_event_storage_across_hierarchy (indexed_storage_node->repository, event_type);

        if (event_storage && kan_atomic_int_get (&event_storage->queries_count) > 0)
        {
            KAN_LOG (repository, KAN_LOG_WARNING,
                     "Failed to remove indexed storage for type \"%s\" that has no associated queries, because removal "
                     "would create \"%s\" events that are being used.",
                     indexed_storage_node->type->name, event->event_type)
            has_obstacles = KAN_TRUE;
        }

        kan_reflection_struct_meta_iterator_next (&iterator);
        event = (struct kan_repository_meta_automatic_on_delete_event_t *) kan_reflection_struct_meta_iterator_get (
            &iterator);
    }

    indexed_storage_node->candidate_for_cleanup = KAN_FALSE;
    return !has_obstacles;
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

#define HELPER_SHUTDOWN_UNUSED_INDICES(INDEX_TYPE)                                                                     \
    struct INDEX_TYPE##_index_t *INDEX_TYPE##_index = indexed_storage_node->first_##INDEX_TYPE##_index;                \
    struct INDEX_TYPE##_index_t *previous_##INDEX_TYPE##_index = NULL;                                                 \
                                                                                                                       \
    while (INDEX_TYPE##_index)                                                                                         \
    {                                                                                                                  \
        struct INDEX_TYPE##_index_t *next_INDEX_TYPE##_index = INDEX_TYPE##_index->next;                               \
        if (kan_atomic_int_get (&INDEX_TYPE##_index->queries_count) == 0)                                              \
        {                                                                                                              \
            INDEX_TYPE##_index_shutdown_and_free (INDEX_TYPE##_index);                                                 \
            if (previous_##INDEX_TYPE##_index)                                                                         \
            {                                                                                                          \
                previous_##INDEX_TYPE##_index->next = next_INDEX_TYPE##_index;                                         \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                indexed_storage_node->first_##INDEX_TYPE##_index = next_INDEX_TYPE##_index;                            \
            }                                                                                                          \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            previous_##INDEX_TYPE##_index = INDEX_TYPE##_index;                                                        \
        }                                                                                                              \
                                                                                                                       \
        INDEX_TYPE##_index = next_INDEX_TYPE##_index;                                                                  \
    }

        HELPER_SHUTDOWN_UNUSED_INDICES (value)
        HELPER_SHUTDOWN_UNUSED_INDICES (signal)
        HELPER_SHUTDOWN_UNUSED_INDICES (interval)
        HELPER_SHUTDOWN_UNUSED_INDICES (space)

#undef HELPER_SHUTDOWN_UNUSED_INDICES

        if (kan_atomic_int_get (&indexed_storage_node->queries_count) == 0 &&
            repository_is_indexed_storage_can_be_cleared (indexed_storage_node))
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

static void repository_init_cascade_deleters (struct repository_t *repository,
                                              struct kan_stack_group_allocator_t *temporary_allocator)
{
    // Cascade deleters are a special case: they are only needed for a small count of storages and are usually quick
    // to build, therefore it should be okay to build them all at once inside single thread without parallelization.
    // But this decision can be changed later.

    struct indexed_storage_node_t *indexed_storage_node =
        (struct indexed_storage_node_t *) repository->indexed_storages.items.first;

    while (indexed_storage_node)
    {
        cascade_deleters_definition_build (&indexed_storage_node->cascade_deleters, indexed_storage_node->type->name,
                                           repository, temporary_allocator,
                                           indexed_storage_node->automation_allocation_group);
        indexed_storage_node = (struct indexed_storage_node_t *) indexed_storage_node->node.list_node.next;
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
    kan_repository_mask_t *event_flag,
    struct kan_stack_group_allocator_t *temporary_allocator,
    struct observation_buffer_scenario_chunk_list_node_t **first,
    struct observation_buffer_scenario_chunk_list_node_t **last)
{
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
            for (kan_loop_size_t index = 0u; index < event->observed_fields_count; ++index)
            {
                struct kan_repository_field_path_t *path = &event->observed_fields[index];
                kan_instance_size_t absolute_offset;
                kan_instance_size_t size_with_padding;

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

                struct observation_buffer_scenario_chunk_list_node_t *node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                    temporary_allocator, struct observation_buffer_scenario_chunk_list_node_t);

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

static void prepare_singleton_storage (kan_functor_user_data_t user_data)
{
    struct singleton_switch_to_serving_user_data_t *data = (struct singleton_switch_to_serving_user_data_t *) user_data;
    struct kan_stack_group_allocator_t temporary_allocator;

    kan_stack_group_allocator_init (
        &temporary_allocator, kan_allocation_group_get_child (data->storage->allocation_group, "switch_to_serving"),
        KAN_REPOSITORY_SWITCH_TO_SERVING_STACK_INITIAL_SIZE);

    struct observation_buffer_scenario_chunk_list_node_t *first_chunk = NULL;
    struct observation_buffer_scenario_chunk_list_node_t *last_chunk = NULL;

    kan_repository_mask_t extraction_event_flag = 1u;
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
        else
        {
            data->storage->observation_buffer_memory = NULL;
        }
    }

    kan_repository_mask_t building_event_flag = 1u;
    observation_event_triggers_definition_build (
        &data->storage->observation_events_triggers, &building_event_flag, data->repository, data->storage->type,
        &data->storage->observation_buffer, &temporary_allocator, data->storage->automation_allocation_group);
    kan_stack_group_allocator_shutdown (&temporary_allocator);
}

static void extract_observation_chunks_from_indices (struct indexed_storage_node_t *storage,
                                                     kan_repository_mask_t *event_flag,
                                                     struct kan_stack_group_allocator_t *temporary_allocator,
                                                     struct observation_buffer_scenario_chunk_list_node_t **first,
                                                     struct observation_buffer_scenario_chunk_list_node_t **last)
{
#define HELPER_ADD_NODE_FOR(BAKED_GETTER)                                                                              \
    {                                                                                                                  \
        struct observation_buffer_scenario_chunk_list_node_t *node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (        \
            temporary_allocator, struct observation_buffer_scenario_chunk_list_node_t);                                \
                                                                                                                       \
        node->next = 0u;                                                                                               \
        node->source_offset = (BAKED_GETTER).absolute_offset;                                                          \
        node->size = (BAKED_GETTER).size_with_padding;                                                                 \
        node->flags = *event_flag;                                                                                     \
                                                                                                                       \
        if (*last)                                                                                                     \
        {                                                                                                              \
            (*last)->next = node;                                                                                      \
            *last = node;                                                                                              \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            *first = node;                                                                                             \
            *last = node;                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        *event_flag <<= 1u;                                                                                            \
        KAN_ASSERT (*event_flag > 0u)                                                                                  \
    }

    struct value_index_t *value_index = storage->first_value_index;
    while (value_index)
    {
        HELPER_ADD_NODE_FOR (value_index->baked)
        value_index = value_index->next;
    }

    struct signal_index_t *signal_index = storage->first_signal_index;
    while (signal_index)
    {
        HELPER_ADD_NODE_FOR (signal_index->baked)
        signal_index = signal_index->next;
    }

    struct interval_index_t *interval_index = storage->first_interval_index;
    while (interval_index)
    {
        HELPER_ADD_NODE_FOR (interval_index->baked)
        interval_index = interval_index->next;
    }

    struct space_index_t *space_index = storage->first_space_index;
    while (space_index)
    {
        HELPER_ADD_NODE_FOR (space_index->baked_min)
        HELPER_ADD_NODE_FOR (space_index->baked_max)
        space_index = space_index->next;
    }

#undef HELPER_ADD_NODE_FOR
}

static void prepare_indices (struct indexed_storage_node_t *storage, kan_repository_mask_t *event_flag)
{
#define HELPER_FILL_INDEX(INDEX_TYPE)                                                                                  \
    struct indexed_storage_record_node_t *record = (struct indexed_storage_record_node_t *) storage->records.first;    \
                                                                                                                       \
    while (record)                                                                                                     \
    {                                                                                                                  \
        INDEX_TYPE##_index_insert_record (INDEX_TYPE##_index, record);                                                 \
        record = (struct indexed_storage_record_node_t *) record->list_node.next;                                      \
    }

    struct value_index_t *value_index = storage->first_value_index;
    while (value_index)
    {
        KAN_MUTE_UNUSED_WARNINGS_BEGIN
        const kan_bool_t baked_from_buffer =
            indexed_field_baked_data_bake_from_buffer (&value_index->baked, &value_index->storage->observation_buffer);
        KAN_ASSERT (baked_from_buffer)
        KAN_MUTE_UNUSED_WARNINGS_END

        if (value_index->hash_storage.items.size == 0u)
        {
            HELPER_FILL_INDEX (value)
        }

        value_index->observation_flags = *event_flag;
        *event_flag <<= 1u;
        KAN_ASSERT (*event_flag > 0u)
        value_index = value_index->next;
    }

    struct signal_index_t *signal_index = storage->first_signal_index;
    while (signal_index)
    {
        KAN_MUTE_UNUSED_WARNINGS_BEGIN
        const kan_bool_t baked_from_buffer = indexed_field_baked_data_bake_from_buffer (
            &signal_index->baked, &signal_index->storage->observation_buffer);
        KAN_ASSERT (baked_from_buffer)
        KAN_MUTE_UNUSED_WARNINGS_END

        if (!signal_index->initial_fill_executed)
        {
            HELPER_FILL_INDEX (signal)
            signal_index->initial_fill_executed = KAN_TRUE;
        }

        signal_index->observation_flags = *event_flag;
        *event_flag <<= 1u;
        KAN_ASSERT (*event_flag > 0u)
        signal_index = signal_index->next;
    }

    struct interval_index_t *interval_index = storage->first_interval_index;
    while (interval_index)
    {
        KAN_MUTE_UNUSED_WARNINGS_BEGIN
        const kan_bool_t baked_from_buffer = indexed_field_baked_data_bake_from_buffer (
            &interval_index->baked, &interval_index->storage->observation_buffer);
        KAN_ASSERT (baked_from_buffer)
        KAN_MUTE_UNUSED_WARNINGS_END

        if (interval_index->tree.size == 0u)
        {
            HELPER_FILL_INDEX (interval)
        }

        interval_index->observation_flags = *event_flag;
        *event_flag <<= 1u;
        KAN_ASSERT (*event_flag > 0u)
        interval_index = interval_index->next;
    }

    struct space_index_t *space_index = storage->first_space_index;
    while (space_index)
    {
        KAN_MUTE_UNUSED_WARNINGS_BEGIN
        kan_bool_t baked_from_buffer = indexed_field_baked_data_bake_from_buffer (
            &space_index->baked_min, &space_index->storage->observation_buffer);
        KAN_ASSERT (baked_from_buffer)

        baked_from_buffer = indexed_field_baked_data_bake_from_buffer (&space_index->baked_max,
                                                                       &space_index->storage->observation_buffer);
        KAN_ASSERT (baked_from_buffer)
        KAN_MUTE_UNUSED_WARNINGS_END

        if (!space_index->initial_fill_executed)
        {
            HELPER_FILL_INDEX (space)
            space_index->initial_fill_executed = KAN_TRUE;
        }

        space_index->observation_flags = *event_flag;
        *event_flag <<= 1u;
        KAN_ASSERT (*event_flag > 0u)
        space_index = space_index->next;
    }

#undef HELPER_FILL_INDEX
}

static void prepare_indexed_storage (kan_functor_user_data_t user_data)
{
    struct indexed_switch_to_serving_user_data_t *data = (struct indexed_switch_to_serving_user_data_t *) user_data;
    struct kan_stack_group_allocator_t temporary_allocator;

    kan_stack_group_allocator_init (
        &temporary_allocator, kan_allocation_group_get_child (data->storage->allocation_group, "switch_to_serving"),
        KAN_REPOSITORY_SWITCH_TO_SERVING_STACK_INITIAL_SIZE);

    struct observation_buffer_scenario_chunk_list_node_t *first_chunk = NULL;
    struct observation_buffer_scenario_chunk_list_node_t *last_chunk = NULL;

    kan_repository_mask_t extraction_event_flag = 1u;
    extract_observation_chunks_from_on_change_events (data->repository, data->storage->type, &extraction_event_flag,
                                                      &temporary_allocator, &first_chunk, &last_chunk);

    extract_observation_chunks_from_indices (data->storage, &extraction_event_flag, &temporary_allocator, &first_chunk,
                                             &last_chunk);

    if (first_chunk)
    {
        observation_buffer_definition_build (&data->storage->observation_buffer, first_chunk, &temporary_allocator,
                                             data->storage->automation_allocation_group);
    }

    kan_repository_mask_t building_event_flag = 1u;
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
    kan_stack_group_allocator_shutdown (&temporary_allocator);
}

static void repository_prepare_storages (struct repository_t *repository, struct switch_to_serving_context_t *context)
{
    struct singleton_storage_node_t *singleton_storage_node =
        (struct singleton_storage_node_t *) repository->singleton_storages.items.first;

    while (singleton_storage_node)
    {
        KAN_CPU_TASK_LIST_USER_STRUCT (&context->task_list, &context->allocator, prepare_singleton_storage,
                                       switch_to_serving_task_section, struct singleton_switch_to_serving_user_data_t,
                                       {
                                           .storage = singleton_storage_node,
                                           .repository = repository,
                                       })
        singleton_storage_node = (struct singleton_storage_node_t *) singleton_storage_node->node.list_node.next;
    }

    struct indexed_storage_node_t *indexed_storage_node =
        (struct indexed_storage_node_t *) repository->indexed_storages.items.first;

    while (indexed_storage_node)
    {
        KAN_CPU_TASK_LIST_USER_STRUCT (&context->task_list, &context->allocator, prepare_indexed_storage,
                                       switch_to_serving_task_section, struct indexed_switch_to_serving_user_data_t,
                                       {
                                           .storage = indexed_storage_node,
                                           .repository = repository,
                                       })
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
    repository_shutdown_utility_planning_storages (repository);
    repository = repository->first;

    while (repository)
    {
        repository_complete_switch_to_serving (repository);
        repository = repository->next;
    }
}

void kan_repository_enter_serving_mode (kan_repository_t root_repository)
{
    struct repository_t *repository = KAN_HANDLE_GET (root_repository);
    KAN_ASSERT (!repository->parent)
    KAN_ASSERT (repository->mode == REPOSITORY_MODE_PLANNING)

    kan_cpu_section_t section = kan_cpu_section_get ("repository_enter_serving_mode");
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, section);

    repository_finish_scheduled_destroy (repository);
    repository_clean_storages (repository);

    struct switch_to_serving_context_t context;
    kan_stack_group_allocator_init (&context.allocator,
                                    kan_allocation_group_get_child (repository->allocation_group, "switch_to_serving"),
                                    KAN_REPOSITORY_SWITCH_TO_SERVING_STACK_INITIAL_SIZE);

    repository_init_cascade_deleters (repository, &context.allocator);
    kan_cpu_job_t job = kan_cpu_job_create ();
    KAN_ASSERT (KAN_HANDLE_IS_VALID (job))

    context.task_list = NULL;
    repository_prepare_storages (repository, &context);

    kan_cpu_job_dispatch_and_detach_task_list (job, context.task_list);
    kan_cpu_job_release (job);
    kan_cpu_job_wait (job);

    kan_stack_group_allocator_shutdown (&context.allocator);
    repository_complete_switch_to_serving (repository);
    kan_cpu_section_execution_shutdown (&execution);
}

static void repository_schedule_for_destroy (struct repository_t *repository)
{
    KAN_ASSERT (repository->parent)
    KAN_ASSERT (repository->mode == REPOSITORY_MODE_SERVING)
    repository->scheduled_for_destroy = KAN_TRUE;

    struct singleton_storage_node_t *singleton_storage_node =
        (struct singleton_storage_node_t *) repository->singleton_storages.items.first;

    while (singleton_storage_node)
    {
        singleton_storage_node->scheduled_for_destroy = KAN_TRUE;
        singleton_storage_node = (struct singleton_storage_node_t *) singleton_storage_node->node.list_node.next;
    }

    struct indexed_storage_node_t *indexed_storage_node =
        (struct indexed_storage_node_t *) repository->indexed_storages.items.first;

    while (indexed_storage_node)
    {
        indexed_storage_node->scheduled_for_destroy = KAN_TRUE;
        indexed_storage_node = (struct indexed_storage_node_t *) indexed_storage_node->node.list_node.next;
    }

    struct event_storage_node_t *event_storage_node =
        (struct event_storage_node_t *) repository->event_storages.items.first;

    while (event_storage_node)
    {
        event_storage_node->scheduled_for_destroy = KAN_TRUE;
        event_storage_node = (struct event_storage_node_t *) event_storage_node->node.list_node.next;
    }

    repository = repository->first;
    while (repository)
    {
        repository_schedule_for_destroy (repository);
        repository = repository->next;
    }
}

REPOSITORY_API void kan_repository_schedule_child_destroy (kan_repository_t repository)
{
    repository_schedule_for_destroy (KAN_HANDLE_GET (repository));
}

void kan_repository_destroy (kan_repository_t repository)
{
    struct repository_t *repository_data = KAN_HANDLE_GET (repository);
    KAN_ASSERT (!repository_data->parent)
    repository_destroy_internal (repository_data);
}
