#include <universe_resource_provider_kan_api.h>

#include <qsort.h>
#include <stddef.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/context/all_system_names.h>
#include <kan/context/hot_reload_coordination_system.h>
#include <kan/context/reflection_system.h>
#include <kan/context/virtual_file_system.h>
#include <kan/log/logging.h>
#include <kan/platform/hardware.h>
#include <kan/precise_time/precise_time.h>
#include <kan/resource_index/resource_index.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe/reflection_system_generator_helpers.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>
#include <kan/virtual_file_system/virtual_file_system.h>

// \c_interface_scanner_disable
KAN_LOG_DEFINE_CATEGORY (universe_resource_provider);
// \c_interface_scanner_enable

UNIVERSE_RESOURCE_PROVIDER_KAN_API struct kan_universe_mutator_group_meta_t resource_provider_group_meta = {
    .group_name = KAN_RESOURCE_PROVIDER_MUTATOR_GROUP,
};

enum resource_provider_status_t
{
    RESOURCE_PROVIDER_STATUS_NOT_INITIALIZED = 0u,
    RESOURCE_PROVIDER_STATUS_SCANNING,
    RESOURCE_PROVIDER_STATUS_SERVING,
};

struct scan_item_task_t
{
    char *path;
};

struct resource_provider_private_singleton_t
{
    enum resource_provider_status_t status;

    kan_id_32_t container_id_counter;

    /// \meta reflection_ignore_struct_field
    struct kan_atomic_int_t attachment_id_counter;

    /// \meta reflection_dynamic_array_type = "struct scan_item_task_t"
    struct kan_dynamic_array_t scan_item_stack;

    /// \meta reflection_dynamic_array_type = "kan_serialization_interned_string_registry_t"
    struct kan_dynamic_array_t loaded_string_registries;

    kan_virtual_file_system_watcher_t resource_watcher;
    kan_virtual_file_system_watcher_iterator_t resource_watcher_iterator;
};

struct resource_provider_native_entry_suffix_t
{
    kan_resource_entry_id_t attachment_id;
    kan_instance_size_t request_count;

    enum kan_resource_index_native_item_format_t format;
    kan_serialization_interned_string_registry_t string_registry;

    /// \warning Needs manual removal as it is not efficient to bind every container type through meta.
    kan_resource_container_id_t loaded_container_id;

    /// \warning Needs manual removal as it is not efficient to bind every container type through meta.
    kan_resource_container_id_t loading_container_id;

    kan_packed_timer_t reload_after_real_time_timer;
};

struct resource_provider_third_party_entry_suffix_t
{
    kan_resource_entry_id_t attachment_id;
    kan_instance_size_t request_count;

    uint8_t *loaded_data;
    kan_memory_size_t loaded_data_size;

    uint8_t *loading_data;
    kan_memory_size_t loading_data_size;

    kan_packed_timer_t reload_after_real_time_timer;
    kan_allocation_group_t my_allocation_group;
};

struct resource_request_on_insert_event_t
{
    kan_resource_request_id_t request_id;
    kan_interned_string_t type;
    kan_interned_string_t name;
};

// \meta reflection_struct_meta = "kan_resource_request_t"
UNIVERSE_RESOURCE_PROVIDER_KAN_API struct kan_repository_meta_automatic_on_insert_event_t resource_request_on_insert = {
    .event_type = "resource_request_on_insert_event_t",
    .copy_outs_count = 3u,
    .copy_outs =
        (struct kan_repository_copy_out_t[]) {
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"request_id"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"request_id"}},
            },
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"type"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"type"}},
            },
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
            },
        },
};

struct resource_request_on_change_event_t
{
    kan_resource_request_id_t request_id;
    kan_interned_string_t old_type;
    kan_interned_string_t old_name;
    kan_interned_string_t new_type;
    kan_interned_string_t new_name;
};

// \meta reflection_struct_meta = "kan_resource_request_t"
UNIVERSE_RESOURCE_PROVIDER_KAN_API struct kan_repository_meta_automatic_on_change_event_t resource_request_on_change = {
    .event_type = "resource_request_on_change_event_t",
    .observed_fields_count = 2u,
    .observed_fields =
        (struct kan_repository_field_path_t[]) {
            {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"type"}},
            {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
        },
    .unchanged_copy_outs_count = 2u,
    .unchanged_copy_outs =
        (struct kan_repository_copy_out_t[]) {
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"type"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"old_type"}},
            },
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"old_name"}},
            },
        },
    .changed_copy_outs_count = 3u,
    .changed_copy_outs =
        (struct kan_repository_copy_out_t[]) {
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"request_id"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"request_id"}},
            },
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"type"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"new_type"}},
            },
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"new_name"}},
            },
        },
};

struct resource_request_on_delete_event_t
{
    kan_resource_request_id_t request_id;
    kan_interned_string_t type;
    kan_interned_string_t name;
};

// \meta reflection_struct_meta = "kan_resource_request_t"
UNIVERSE_RESOURCE_PROVIDER_KAN_API struct kan_repository_meta_automatic_on_delete_event_t resource_request_on_delete = {
    .event_type = "resource_request_on_delete_event_t",
    .copy_outs_count = 3u,
    .copy_outs =
        (struct kan_repository_copy_out_t[]) {
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"request_id"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"request_id"}},
            },
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"type"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"type"}},
            },
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
            },
        },
};

struct resource_provider_loading_operation_native_data_t
{
    kan_reflection_registry_t used_registry;
    enum kan_resource_index_native_item_format_t format_cache;

    union
    {
        kan_serialization_binary_reader_t binary_reader;
        kan_serialization_rd_reader_t readable_data_reader;
    };
};

struct resource_provider_loading_operation_third_party_data_t
{
    kan_memory_size_t offset;
    kan_memory_size_t size;
    uint8_t *data;
};

/// \brief Hardcoded priority for handling on request hot swaps.
#define PRIORITY_HOT_SWAP KAN_INT_MAX (kan_instance_size_t)

struct resource_provider_loading_operation_t
{
    kan_instance_size_t priority;
    kan_interned_string_t target_type;
    kan_interned_string_t target_name;
    struct kan_stream_t *stream;

    union
    {
        struct resource_provider_loading_operation_native_data_t native;
        struct resource_provider_loading_operation_third_party_data_t third_party;
    };
};

struct resource_provider_delayed_file_addition_t
{
    char *path;
    kan_packed_timer_t investigate_after_timer;
    kan_interned_string_t name_for_search;
    kan_allocation_group_t my_allocation_group;
};

struct resource_provider_execution_shared_state_t
{
    /// \meta reflection_ignore_struct_field
    struct kan_atomic_int_t workers_left;

    /// \meta reflection_ignore_struct_field
    struct kan_atomic_int_t concurrency_lock;

    //// \meta reflection_ignore_struct_field
    struct kan_repository_indexed_interval_descending_write_cursor_t loading_operation_cursor;

    kan_time_size_t end_time_ns;
};

struct resource_provider_native_container_type_data_t
{
    kan_interned_string_t contained_type_name;
    kan_interned_string_t container_type_name;
    kan_memory_size_t contained_type_alignment;

    struct kan_repository_indexed_insert_query_t insert_query;
    struct kan_repository_indexed_value_update_query_t update_by_id_query;
    struct kan_repository_indexed_value_delete_query_t delete_by_id_query;
};

_Static_assert (_Alignof (struct resource_provider_native_container_type_data_t) == _Alignof (void *),
                "Alignment matches.");

struct resource_provider_state_t
{
    kan_allocation_group_t my_allocation_group;
    kan_time_offset_t scan_budget_ns;
    kan_time_offset_t load_budget_ns;
    kan_bool_t use_load_only_string_registry;
    kan_interned_string_t resource_directory_path;

    kan_reflection_registry_t reflection_registry;
    kan_serialization_binary_script_storage_t shared_script_storage;
    kan_context_system_t virtual_file_system;
    kan_context_system_t hot_reload_system;

    /// \meta reflection_ignore_struct_field
    struct kan_stack_group_allocator_t temporary_allocator;

    /// \meta reflection_ignore_struct_field
    struct kan_stream_t *serialized_index_stream;

    /// \meta reflection_ignore_struct_field
    kan_serialization_binary_reader_t serialized_index_reader;

    /// \meta reflection_ignore_struct_field
    struct kan_resource_index_t serialized_index_read_buffer;

    /// \meta reflection_ignore_struct_field
    struct kan_stream_t *string_registry_stream;

    /// \meta reflection_ignore_struct_field
    kan_serialization_interned_string_registry_reader_t string_registry_reader;

    kan_interned_string_t interned_kan_resource_index_t;
    kan_interned_string_t interned_resource_provider_server;

    KAN_UP_GENERATE_STATE_QUERIES (resource_provider)
    KAN_UP_BIND_STATE (resource_provider, state)

    struct kan_repository_indexed_interval_write_query_t write_interval__resource_provider_loading_operation__priority;

    /// \meta reflection_ignore_struct_field
    struct resource_provider_execution_shared_state_t execution_shared_state;

    kan_instance_size_t trailing_data_count;

    /// \meta reflection_ignore_struct_field
    void *trailing_data[0u];
};

_Static_assert (_Alignof (struct resource_provider_state_t) ==
                    _Alignof (struct resource_provider_native_container_type_data_t),
                "Alignment matches.");

struct universe_resource_provider_generated_container_type_node_t
{
    struct universe_resource_provider_generated_container_type_node_t *next;

    /// \meta reflection_ignore_struct_field
    struct kan_reflection_struct_t type;

    const struct kan_reflection_struct_t *source_type;
};

struct kan_reflection_generator_universe_resource_provider_t
{
    kan_loop_size_t boostrap_iteration;
    kan_allocation_group_t generated_reflection_group;
    struct universe_resource_provider_generated_container_type_node_t *first_container_type;
    kan_instance_size_t container_types_count;

    /// \meta reflection_ignore_struct_field
    struct kan_reflection_struct_t mutator_type;

    /// \meta reflection_ignore_struct_field
    struct kan_reflection_function_t mutator_deploy_function;

    /// \meta reflection_ignore_struct_field
    struct kan_reflection_function_t mutator_execute_function;

    /// \meta reflection_ignore_struct_field
    struct kan_reflection_function_t mutator_undeploy_function;

    kan_interned_string_t interned_kan_resource_resource_type_meta_t;
    kan_interned_string_t interned_container_id;
    kan_interned_string_t interned_stored_resource;
};

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_private_singleton_init (
    struct resource_provider_private_singleton_t *data)
{
    data->status = RESOURCE_PROVIDER_STATUS_NOT_INITIALIZED;
    data->container_id_counter = KAN_TYPED_ID_32_GET (KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t)) + 1u;
    data->attachment_id_counter = kan_atomic_int_init (1);

    kan_dynamic_array_init (&data->scan_item_stack, 0u, sizeof (struct scan_item_task_t),
                            _Alignof (struct scan_item_task_t), kan_allocation_group_stack_get ());

    kan_dynamic_array_init (&data->loaded_string_registries, 0u, sizeof (kan_serialization_interned_string_registry_t),
                            _Alignof (kan_serialization_interned_string_registry_t), kan_allocation_group_stack_get ());

    data->resource_watcher = KAN_HANDLE_SET_INVALID (kan_virtual_file_system_watcher_t);
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_private_singleton_shutdown (
    struct resource_provider_private_singleton_t *data)
{
    for (kan_loop_size_t index = 0u; index < data->scan_item_stack.size; ++index)
    {
        struct scan_item_task_t *task = &((struct scan_item_task_t *) data->scan_item_stack.data)[index];
        kan_free_general (data->scan_item_stack.allocation_group, task->path, strlen (task->path) + 1u);
    }

    for (kan_loop_size_t index = 0u; index < data->loaded_string_registries.size; ++index)
    {
        kan_serialization_interned_string_registry_destroy (
            ((kan_serialization_interned_string_registry_t *) data->loaded_string_registries.data)[index]);
    }

    if (KAN_HANDLE_IS_VALID (data->resource_watcher))
    {
        kan_virtual_file_system_watcher_iterator_destroy (data->resource_watcher, data->resource_watcher_iterator);
        kan_virtual_file_system_watcher_destroy (data->resource_watcher);
    }

    kan_dynamic_array_shutdown (&data->scan_item_stack);
    kan_dynamic_array_shutdown (&data->loaded_string_registries);
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_native_entry_suffix_init (
    struct resource_provider_native_entry_suffix_t *instance)
{
    instance->request_count = 0u;
    instance->loaded_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
    instance->loading_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
    instance->reload_after_real_time_timer = KAN_PACKED_TIMER_NEVER;
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_third_party_entry_suffix_init (
    struct resource_provider_third_party_entry_suffix_t *instance)
{
    instance->request_count = 0u;
    instance->loaded_data = NULL;
    instance->loaded_data_size = 0u;
    instance->loading_data = NULL;
    instance->loading_data_size = 0u;
    instance->reload_after_real_time_timer = KAN_PACKED_TIMER_NEVER;
    instance->my_allocation_group = kan_allocation_group_stack_get ();
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_third_party_entry_suffix_shutdown (
    struct resource_provider_third_party_entry_suffix_t *instance)
{
    if (instance->loaded_data)
    {
        kan_free_general (instance->my_allocation_group, instance->loaded_data, instance->loaded_data_size);
    }

    if (instance->loading_data)
    {
        kan_free_general (instance->my_allocation_group, instance->loading_data, instance->loading_data_size);
    }
}

static inline void resource_provider_loading_operation_destroy_native (
    struct resource_provider_loading_operation_t *instance)
{
    if (instance->target_type)
    {
        switch (instance->native.format_cache)
        {
        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY:
            kan_serialization_binary_reader_destroy (instance->native.binary_reader);
            break;

        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA:
            kan_serialization_rd_reader_destroy (instance->native.readable_data_reader);
            break;
        }
    }
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_loading_operation_shutdown (
    struct resource_provider_loading_operation_t *instance)
{
    if (instance->stream)
    {
        instance->stream->operations->close (instance->stream);
    }

    resource_provider_loading_operation_destroy_native (instance);
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_delayed_file_addition_init (
    struct resource_provider_delayed_file_addition_t *instance)
{
    instance->my_allocation_group = kan_allocation_group_stack_get ();
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_delayed_file_addition_shutdown (
    struct resource_provider_delayed_file_addition_t *instance)
{
    kan_free_general (instance->my_allocation_group, instance->path, strlen (instance->path) + 1u);
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_state_init (struct resource_provider_state_t *data)
{
    data->my_allocation_group = kan_allocation_group_stack_get ();
    data->scan_budget_ns = 0u;
    data->load_budget_ns = 0u;
    data->resource_directory_path = NULL;

    kan_stack_group_allocator_init (&data->temporary_allocator,
                                    kan_allocation_group_get_child (data->my_allocation_group, "temporary"),
                                    KAN_UNIVERSE_RESOURCE_PROVIDER_TEMPORARY_CHUNK_SIZE);

    data->interned_kan_resource_index_t = kan_string_intern ("kan_resource_index_t");
    data->interned_resource_provider_server = kan_string_intern ("resource_provider_server");
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void mutator_template_deploy_resource_provider (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct resource_provider_state_t *state)
{
    struct kan_resource_provider_configuration_t *configuration =
        (struct kan_resource_provider_configuration_t *) kan_universe_world_query_configuration (
            world, kan_string_intern (KAN_RESOURCE_PROVIDER_CONFIGURATION));
    KAN_ASSERT (configuration)

    state->scan_budget_ns = configuration->scan_budget_ns;
    state->load_budget_ns = configuration->load_budget_ns;
    state->use_load_only_string_registry = configuration->use_load_only_string_registry;
    state->resource_directory_path = configuration->resource_directory_path;

    state->reflection_registry = kan_universe_get_reflection_registry (universe);
    state->shared_script_storage = kan_serialization_binary_script_storage_create (state->reflection_registry);

    state->virtual_file_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_VIRTUAL_FILE_SYSTEM_NAME);
    KAN_ASSERT (KAN_HANDLE_IS_VALID (state->virtual_file_system))

    state->hot_reload_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_NAME);

    state->serialized_index_stream = NULL;
    state->serialized_index_reader = KAN_HANDLE_SET_INVALID (kan_serialization_binary_reader_t);
    state->string_registry_stream = NULL;
    state->string_registry_reader = KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_reader_t);

    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
}

static void push_scan_item_task (struct resource_provider_private_singleton_t *private, const char *path)
{
    void *spot = kan_dynamic_array_add_last (&private->scan_item_stack);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (&private->scan_item_stack, private->scan_item_stack.capacity * 2u);
        spot = kan_dynamic_array_add_last (&private->scan_item_stack);
        KAN_ASSERT (spot)
    }

    struct scan_item_task_t *task = (struct scan_item_task_t *) spot;
    const kan_instance_size_t path_length = (kan_instance_size_t) strlen (path);
    task->path = kan_allocate_general (private->scan_item_stack.allocation_group, path_length + 1u, _Alignof (char));
    memcpy (task->path, path, path_length + 1u);
}

static void destroy_scan_item_task (struct resource_provider_private_singleton_t *private,
                                    kan_instance_size_t task_index)
{
    struct scan_item_task_t *task = &((struct scan_item_task_t *) private->scan_item_stack.data)[task_index];
    kan_free_general (private->scan_item_stack.allocation_group, task->path, strlen (task->path) + 1u);
    kan_dynamic_array_remove_swap_at (&private->scan_item_stack, task_index);
}

static void prepare_for_scanning (struct resource_provider_state_t *state,
                                  struct resource_provider_private_singleton_t *private)
{
    KAN_ASSERT (private->scan_item_stack.size == 0u)
    kan_dynamic_array_set_capacity (&private->scan_item_stack, KAN_UNIVERSE_RESOURCE_PROVIDER_SCAN_DIRECTORY_CAPACITY);
    push_scan_item_task (private, state->resource_directory_path);
    kan_dynamic_array_set_capacity (&private->loaded_string_registries,
                                    KAN_UNIVERSE_RESOURCE_PROVIDER_STRING_REGISTRIES);
}

static void add_native_entry (struct resource_provider_state_t *state,
                              struct resource_provider_private_singleton_t *private,
                              kan_interned_string_t type_name,
                              kan_interned_string_t name,
                              enum kan_resource_index_native_item_format_t format,
                              const char *path,
                              kan_serialization_interned_string_registry_t string_registry)
{
    const kan_resource_entry_id_t attachment_id = KAN_TYPED_ID_32_SET (
        kan_resource_entry_id_t, (kan_id_32_t) kan_atomic_int_add (&private->attachment_id_counter, 1));

    KAN_UP_INDEXED_INSERT (entry, kan_resource_native_entry_t)
    {
        entry->attachment_id = attachment_id;
        entry->type = type_name;
        entry->name = name;

        const kan_instance_size_t path_length = (kan_instance_size_t) strlen (path);
        entry->path = kan_allocate_general (entry->my_allocation_group, path_length + 1u, _Alignof (char));
        memcpy (entry->path, path, path_length + 1u);
    }

    KAN_UP_INDEXED_INSERT (suffix, resource_provider_native_entry_suffix_t)
    {
        suffix->attachment_id = attachment_id;
        suffix->format = format;
        suffix->string_registry = string_registry;
    }
}

static void add_third_party_entry (struct resource_provider_state_t *state,
                                   struct resource_provider_private_singleton_t *private,
                                   kan_interned_string_t name,
                                   kan_memory_size_t size,
                                   const char *path)
{
    const kan_resource_entry_id_t attachment_id = KAN_TYPED_ID_32_SET (
        kan_resource_entry_id_t, (kan_id_32_t) kan_atomic_int_add (&private->attachment_id_counter, 1));

    KAN_UP_INDEXED_INSERT (entry, kan_resource_third_party_entry_t)
    {
        entry->attachment_id = attachment_id;
        entry->name = name;
        entry->size = size;

        const kan_instance_size_t path_length = (kan_instance_size_t) strlen (path);
        entry->path = kan_allocate_general (entry->my_allocation_group, path_length + 1u, _Alignof (char));
        memcpy (entry->path, path, path_length + 1u);
    }

    KAN_UP_INDEXED_INSERT (suffix, resource_provider_third_party_entry_suffix_t)
    {
        suffix->attachment_id = attachment_id;
    }
}

static inline void unload_native_entry (struct resource_provider_state_t *state,
                                        struct kan_resource_native_entry_t *entry,
                                        struct resource_provider_native_entry_suffix_t *entry_suffix);

static inline void cancel_native_entry_loading (struct resource_provider_state_t *state,
                                                struct kan_resource_native_entry_t *entry,
                                                struct resource_provider_native_entry_suffix_t *entry_suffix);

static inline void unload_third_party_entry (struct resource_provider_state_t *state,
                                             struct kan_resource_third_party_entry_t *entry,
                                             struct resource_provider_third_party_entry_suffix_t *entry_suffix);

static inline void cancel_third_party_entry_loading (struct resource_provider_state_t *state,
                                                     struct kan_resource_third_party_entry_t *entry,
                                                     struct resource_provider_third_party_entry_suffix_t *entry_suffix);

static void clear_entries (struct resource_provider_state_t *state)
{
    KAN_UP_SEQUENCE_WRITE (native_entry, kan_resource_native_entry_t)
    {
        KAN_UP_VALUE_WRITE (suffix, resource_provider_native_entry_suffix_t, attachment_id,
                            &native_entry->attachment_id)
        {
            unload_native_entry (state, native_entry, suffix);
            cancel_native_entry_loading (state, native_entry, suffix);
            KAN_UP_ACCESS_DELETE (suffix);
        }

        KAN_UP_ACCESS_DELETE (native_entry);
    }

    KAN_UP_SEQUENCE_WRITE (third_party_entry, kan_resource_third_party_entry_t)
    {
        KAN_UP_VALUE_WRITE (suffix, resource_provider_third_party_entry_suffix_t, attachment_id,
                            &third_party_entry->attachment_id)
        {
            unload_third_party_entry (state, third_party_entry, suffix);
            cancel_third_party_entry_loading (state, third_party_entry, suffix);
            KAN_UP_ACCESS_DELETE (suffix);
        }

        KAN_UP_ACCESS_DELETE (third_party_entry);
    }
}

struct file_scan_result_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
};

static struct file_scan_result_t scan_file (struct resource_provider_state_t *state,
                                            struct resource_provider_private_singleton_t *private,
                                            kan_virtual_file_system_volume_t volume,
                                            const char *path,
                                            kan_memory_size_t size)
{
    struct file_scan_result_t result = {NULL, NULL};
    const char *path_end = path;

    while (*path_end)
    {
        ++path_end;
    }

    const kan_instance_size_t path_length = (kan_instance_size_t) (path_end - path);
    if (path_length > 4u && *(path_end - 4u) == '.' && *(path_end - 3u) == 'b' && *(path_end - 2u) == 'i' &&
        *(path_end - 1u) == 'n')
    {
        struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (volume, path);
        if (stream)
        {
            kan_interned_string_t type_name;
            if (kan_serialization_binary_read_type_header (
                    stream, &type_name,
                    // We expect non-indexed files to be encoded without string registries.
                    KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t)))
            {
                const char *last_slash = strrchr (path, '/');
                const char *name_begin = last_slash ? last_slash + 1u : path;
                const char *name_end = path_end - 4u;

                result.type = type_name;
                result.name = kan_char_sequence_intern (name_begin, name_end);

                add_native_entry (state, private, type_name, result.name, KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY,
                                  path, KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));
            }
            else
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                         "Failed to scan entry \"%s\" as its header cannot be read.", path)
            }

            stream->operations->close (stream);
        }
        else
        {
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR, "Failed to scan entry \"%s\" as it cannot be opened.",
                     path)
        }
    }
    else if (path_length > 3u && *(path_end - 3u) == '.' && *(path_end - 2u) == 'r' && *(path_end - 1u) == 'd')
    {
        struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (volume, path);
        if (stream)
        {
            kan_interned_string_t type_name;
            if (kan_serialization_rd_read_type_header (stream, &type_name))
            {
                const char *last_slash = strrchr (path, '/');
                const char *name_begin = last_slash ? last_slash + 1u : path;
                const char *name_end = path_end - 3u;

                result.type = type_name;
                result.name = kan_char_sequence_intern (name_begin, name_end);

                add_native_entry (state, private, type_name, result.name,
                                  KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA, path,
                                  KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));
            }
            else
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                         "Failed to scan entry \"%s\" as its header cannot be read.", path)
            }

            stream->operations->close (stream);
        }
        else
        {
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR, "Failed to scan entry \"%s\" as it cannot be opened.",
                     path)
        }
    }
    else
    {
        const char *last_slash = strrchr (path, '/');
        const char *name_begin = last_slash ? last_slash + 1u : path;
        const char *name_end = path_end;

        result.name = kan_char_sequence_intern (name_begin, name_end);
        add_third_party_entry (state, private, result.name, size, path);
    }

    return result;
}

static kan_bool_t scan_directory (struct resource_provider_state_t *state,
                                  struct resource_provider_private_singleton_t *private,
                                  kan_virtual_file_system_volume_t volume,
                                  const char *path)
{
    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_string (&path_container, path);
    const kan_instance_size_t directory_path_length = path_container.length;

    kan_file_system_path_container_append (&path_container, KAN_RESOURCE_INDEX_DEFAULT_NAME);
    if (kan_virtual_file_system_check_existence (volume, path_container.path))
    {
        state->serialized_index_stream = kan_virtual_file_stream_open_for_read (volume, path_container.path);
        if (!state->serialized_index_stream)
        {
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                     "Failed to open index \"%s\", directory will be skipped.", path)
            return KAN_TRUE;
        }

        state->serialized_index_stream = kan_random_access_stream_buffer_open_for_read (
            state->serialized_index_stream, KAN_UNIVERSE_RESOURCE_PROVIDER_READ_BUFFER);

        kan_file_system_path_container_reset_length (&path_container, directory_path_length);
        kan_file_system_path_container_append (&path_container,
                                               KAN_RESOURCE_INDEX_ACCOMPANYING_STRING_REGISTRY_DEFAULT_NAME);

        if (kan_virtual_file_system_check_existence (volume, path_container.path))
        {
            state->string_registry_stream = kan_virtual_file_stream_open_for_read (volume, path_container.path);
            if (!state->string_registry_stream)
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                         "Failed to open string registry \"%s\", directory will be skipped.", path)
                state->serialized_index_stream->operations->close (state->serialized_index_stream);
                return KAN_TRUE;
            }

            state->string_registry_stream = kan_random_access_stream_buffer_open_for_read (
                state->string_registry_stream, KAN_UNIVERSE_RESOURCE_PROVIDER_READ_BUFFER);
        }

        return KAN_FALSE;
    }

    kan_file_system_path_container_reset_length (&path_container, directory_path_length);
    struct kan_virtual_file_system_directory_iterator_t iterator =
        kan_virtual_file_system_directory_iterator_create (volume, path_container.path);
    const char *entry_name;

    while ((entry_name = kan_virtual_file_system_directory_iterator_advance (&iterator)))
    {
        if ((entry_name[0u] == '.' && entry_name[1u] == '\0') ||
            (entry_name[0u] == '.' && entry_name[1u] == '.' && entry_name[2u] == '\0'))
        {
            continue;
        }

        kan_file_system_path_container_append (&path_container, entry_name);
        push_scan_item_task (private, path_container.path);
        kan_file_system_path_container_reset_length (&path_container, directory_path_length);
    }

    kan_virtual_file_system_directory_iterator_destroy (&iterator);
    return KAN_TRUE;
}

static kan_bool_t scan_item (struct resource_provider_state_t *state,
                             struct resource_provider_private_singleton_t *private,
                             kan_virtual_file_system_volume_t volume,
                             const char *path)
{
    struct kan_virtual_file_system_entry_status_t status;
    if (kan_virtual_file_system_query_entry (volume, path, &status))
    {
        switch (status.type)
        {
        case KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN:
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR, "Failed to scan entry \"%s\" as it has unknown type.",
                     path)
            return KAN_TRUE;

        case KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE:
            scan_file (state, private, volume, path, status.size);
            return KAN_TRUE;

        case KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY:
            return scan_directory (state, private, volume, path);
        }
    }
    else
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR, "Failed to query status of entry \"%s\".", path)
    }

    return KAN_TRUE;
}

static void instantiate_resource_index (struct resource_provider_state_t *state,
                                        struct resource_provider_private_singleton_t *private,
                                        kan_serialization_interned_string_registry_t string_registry)
{
    struct scan_item_task_t *scan_task =
        &((struct scan_item_task_t *) private->scan_item_stack.data)[private->scan_item_stack.size - 1u];
    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_string (&path_container, scan_task->path);

    for (kan_loop_size_t type_index = 0u; type_index < state->serialized_index_read_buffer.native.size; ++type_index)
    {
        struct kan_resource_index_native_container_t *container =
            &((struct kan_resource_index_native_container_t *)
                  state->serialized_index_read_buffer.native.data)[type_index];

        for (kan_loop_size_t item_index = 0u; item_index < container->items.size; ++item_index)
        {
            struct kan_resource_index_native_item_t *item =
                &((struct kan_resource_index_native_item_t *) container->items.data)[item_index];

            const kan_instance_size_t length_backup = path_container.length;
            kan_file_system_path_container_append (&path_container, item->path);
            add_native_entry (state, private, container->type, item->name, item->format, path_container.path,
                              string_registry);
            kan_file_system_path_container_reset_length (&path_container, length_backup);
        }
    }

    for (kan_loop_size_t item_index = 0u; item_index < state->serialized_index_read_buffer.third_party.size;
         ++item_index)
    {
        struct kan_resource_index_third_party_item_t *item =
            &((struct kan_resource_index_third_party_item_t *)
                  state->serialized_index_read_buffer.third_party.data)[item_index];

        const kan_instance_size_t length_backup = path_container.length;
        kan_file_system_path_container_append (&path_container, item->path);
        add_third_party_entry (state, private, item->name, item->size, path_container.path);
        kan_file_system_path_container_reset_length (&path_container, length_backup);
    }
}

static inline struct resource_provider_native_container_type_data_t *query_container_type_data (
    struct resource_provider_state_t *state, kan_interned_string_t type)
{
    KAN_UNIVERSE_REFLECTION_GENERATOR_FIND_GENERATED_STATE (struct resource_provider_native_container_type_data_t,
                                                            contained_type_name, type);
}

static inline struct kan_resource_container_view_t *native_container_create (
    struct resource_provider_state_t *state,
    struct resource_provider_private_singleton_t *private,
    kan_interned_string_t type,
    struct kan_repository_indexed_insertion_package_t *package_output,
    uint8_t **data_begin_output)
{
    struct resource_provider_native_container_type_data_t *data = query_container_type_data (state, type);
    if (!data)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Unable to find container type for resource type \"%s\". Resources types should have "
                 "\"kan_resource_resource_type_meta_t\" meta attached.",
                 type)
        return NULL;
    }

    *package_output = kan_repository_indexed_insert_query_execute (&data->insert_query);
    struct kan_resource_container_view_t *view =
        (struct kan_resource_container_view_t *) kan_repository_indexed_insertion_package_get (package_output);

    view->container_id =
        KAN_TYPED_ID_32_SET (kan_resource_container_id_t, (kan_id_32_t) private->container_id_counter++);
    *data_begin_output =
        ((uint8_t *) view) + kan_apply_alignment (offsetof (struct kan_resource_container_view_t, data_begin),
                                                  data->contained_type_alignment);

    return view;
}

static inline struct kan_resource_container_view_t *native_container_update (
    struct resource_provider_state_t *state,
    kan_interned_string_t type,
    kan_resource_container_id_t container_id,
    struct kan_repository_indexed_value_update_access_t *access_output,
    uint8_t **data_begin_output)
{
    struct resource_provider_native_container_type_data_t *data = query_container_type_data (state, type);
    if (!data)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Unable to find container type for resource type \"%s\". Resources types should have "
                 "\"kan_resource_resource_type_meta_t\" meta attached.",
                 type)
        return NULL;
    }

    struct kan_repository_indexed_value_update_cursor_t cursor =
        kan_repository_indexed_value_update_query_execute (&data->update_by_id_query, &container_id);
    *access_output = kan_repository_indexed_value_update_cursor_next (&cursor);
    kan_repository_indexed_value_update_cursor_close (&cursor);

    struct kan_resource_container_view_t *view =
        (struct kan_resource_container_view_t *) kan_repository_indexed_value_update_access_resolve (access_output);

    if (view)
    {
        *data_begin_output =
            ((uint8_t *) view) + kan_apply_alignment (offsetof (struct kan_resource_container_view_t, data_begin),
                                                      data->contained_type_alignment);
    }

    return view;
}

static inline void native_container_delete (struct resource_provider_state_t *state,
                                            kan_interned_string_t type,
                                            kan_resource_container_id_t container_id)
{
    struct resource_provider_native_container_type_data_t *data = query_container_type_data (state, type);
    if (!data)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Unable to find container type for resource type \"%s\". Resources types should have "
                 "\"kan_resource_resource_type_meta_t\" meta attached.",
                 type)
        return;
    }

    struct kan_repository_indexed_value_delete_cursor_t cursor =
        kan_repository_indexed_value_delete_query_execute (&data->delete_by_id_query, &container_id);

    struct kan_repository_indexed_value_delete_access_t access =
        kan_repository_indexed_value_delete_cursor_next (&cursor);

    if (kan_repository_indexed_value_delete_access_resolve (&access))
    {
        kan_repository_indexed_value_delete_access_delete (&access);
    }

    kan_repository_indexed_value_delete_cursor_close (&cursor);
}

static inline kan_instance_size_t gather_request_priority (struct resource_provider_state_t *state,
                                                           kan_interned_string_t type,
                                                           kan_interned_string_t name)
{
    kan_instance_size_t priority = 0u;
    KAN_UP_VALUE_READ (request, kan_resource_request_t, name, &name)
    {
        if (request->type == type)
        {
            // Assert that user didn't enter hot swap priority by mistake.
            KAN_ASSERT (priority != PRIORITY_HOT_SWAP)
            priority = KAN_MAX (priority, request->priority);
        }
    }

    return priority;
}

static inline void update_request_provided_data (struct resource_provider_state_t *state,
                                                 struct kan_resource_request_t *request,
                                                 kan_resource_container_id_t container_id,
                                                 void *third_party_data,
                                                 kan_memory_size_t third_party_data_size)
{
    kan_bool_t changed = KAN_FALSE;
    if (request->type)
    {
        if (!KAN_TYPED_ID_32_IS_EQUAL (request->provided_container_id, container_id))
        {
            request->provided_container_id = container_id;
            changed = KAN_TRUE;
        }
    }
    else
    {
        if (request->provided_third_party.data != third_party_data ||
            request->provided_third_party.size != third_party_data_size)
        {
            request->provided_third_party.data = third_party_data;
            request->provided_third_party.size = third_party_data_size;
            changed = KAN_TRUE;
        }
    }

    if (changed)
    {
        KAN_UP_EVENT_INSERT (event, kan_resource_request_updated_event_t)
        {
            event->type = request->type;
            event->request_id = request->request_id;
        }
    }
}

static inline void update_requests (struct resource_provider_state_t *state,
                                    kan_interned_string_t type,
                                    kan_interned_string_t name,
                                    kan_resource_container_id_t container_id,
                                    void *third_party_data,
                                    kan_memory_size_t third_party_data_size)
{
    KAN_UP_VALUE_UPDATE (request, kan_resource_request_t, name, &name)
    {
        if (request->type == type)
        {
            update_request_provided_data (state, request, container_id, third_party_data, third_party_data_size);
        }
    }
}

static inline void loading_operation_cancel (struct resource_provider_state_t *state,
                                             kan_interned_string_t type,
                                             kan_interned_string_t name)
{
    KAN_UP_VALUE_DELETE (operation, resource_provider_loading_operation_t, target_name, &name)
    {
        if (operation->target_type == type)
        {
            KAN_UP_ACCESS_DELETE (operation);
        }
    }
}

static inline kan_bool_t read_type_header (struct kan_stream_t *stream,
                                           const struct kan_resource_native_entry_t *entry,
                                           const struct resource_provider_native_entry_suffix_t *entry_suffix,
                                           kan_interned_string_t *output)
{
    switch (entry_suffix->format)
    {
    case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY:
        if (!kan_serialization_binary_read_type_header (stream, output, entry_suffix->string_registry))
        {
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                     "Failed to read type header for native resource \"%s\" of type \"%s\" at path \"%s\".",
                     entry->name, entry->type, entry->path)
            return KAN_FALSE;
        }

        break;

    case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA:
        if (!kan_serialization_rd_read_type_header (stream, output))
        {
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                     "Failed to read type header for native resource \"%s\" of type \"%s\" at path \"%s\".",
                     entry->name, entry->type, entry->path)
            return KAN_FALSE;
        }

        break;
    }

    return KAN_TRUE;
}

static inline kan_bool_t skip_type_header (struct kan_stream_t *stream,
                                           const struct kan_resource_native_entry_t *entry,
                                           const struct resource_provider_native_entry_suffix_t *entry_suffix)
{
    kan_interned_string_t type_from_header;
    if (!read_type_header (stream, entry, entry_suffix, &type_from_header))
    {
        return KAN_FALSE;
    }

    if (type_from_header != entry->type)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Failed to read type header for native resource \"%s\" of type \"%s\" at path \"%s\" due to type "
                 "mismatch: type \"%s\" is specified in header.",
                 entry->name, entry->type, entry->path, type_from_header)
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static inline void schedule_native_entry_loading (struct resource_provider_state_t *state,
                                                  struct resource_provider_private_singleton_t *private,
                                                  struct kan_resource_native_entry_t *entry,
                                                  struct resource_provider_native_entry_suffix_t *entry_suffix,
                                                  kan_bool_t from_hot_reload)
{
    if (KAN_TYPED_ID_32_IS_VALID (entry_suffix->loading_container_id))
    {
        return;
    }

    kan_virtual_file_system_volume_t volume =
        kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);

    struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (volume, entry->path);
    kan_virtual_file_system_close_context_read_access (state->virtual_file_system);

    if (!stream)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Failed to open input stream for native resource \"%s\" of type \"%s\" at path \"%s\".", entry->name,
                 entry->type, entry->path)
        return;
    }

    stream = kan_random_access_stream_buffer_open_for_read (stream, KAN_UNIVERSE_RESOURCE_PROVIDER_READ_BUFFER);
    if (!skip_type_header (stream, entry, entry_suffix))
    {
        stream->operations->close (stream);
        return;
    }

    struct kan_repository_indexed_insertion_package_t container_package;
    uint8_t *data_begin;
    struct kan_resource_container_view_t *container_view =
        native_container_create (state, private, entry->type, &container_package, &data_begin);

    if (!container_view)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Failed to create container for native resource \"%s\" of type \"%s\" at path \"%s\".", entry->name,
                 entry->type, entry->path)

        stream->operations->close (stream);
        return;
    }

    entry_suffix->loading_container_id = container_view->container_id;
    KAN_UP_INDEXED_INSERT (operation, resource_provider_loading_operation_t)
    {
        operation->priority =
            from_hot_reload && kan_hot_reload_coordination_system_is_hot_swap (state->hot_reload_system) ?
                PRIORITY_HOT_SWAP :
                gather_request_priority (state, entry->type, entry->name);

        operation->target_type = entry->type;
        operation->target_name = entry->name;
        operation->stream = stream;
        operation->native.format_cache = entry_suffix->format;
        operation->native.used_registry = state->reflection_registry;

        switch (entry_suffix->format)
        {
        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY:
            operation->native.binary_reader = kan_serialization_binary_reader_create (
                stream, data_begin, entry->type, state->shared_script_storage, entry_suffix->string_registry,
                container_view->my_allocation_group);
            break;

        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA:
            operation->native.readable_data_reader = kan_serialization_rd_reader_create (
                stream, data_begin, entry->type, state->reflection_registry, container_view->my_allocation_group);
            break;
        }
    }

    kan_repository_indexed_insertion_package_submit (&container_package);
}

static inline void unload_native_entry (struct resource_provider_state_t *state,
                                        struct kan_resource_native_entry_t *entry,
                                        struct resource_provider_native_entry_suffix_t *entry_suffix)
{
    if (!KAN_TYPED_ID_32_IS_VALID (entry_suffix->loaded_container_id))
    {
        return;
    }

    update_requests (state, entry->type, entry->name, KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t), NULL,
                     0u);
    native_container_delete (state, entry->type, entry_suffix->loaded_container_id);
    entry_suffix->loaded_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
}

static inline void cancel_native_entry_loading (struct resource_provider_state_t *state,
                                                struct kan_resource_native_entry_t *entry,
                                                struct resource_provider_native_entry_suffix_t *entry_suffix)
{
    if (!KAN_TYPED_ID_32_IS_VALID (entry_suffix->loading_container_id))
    {
        return;
    }

    loading_operation_cancel (state, entry->type, entry->name);
    native_container_delete (state, entry->type, entry_suffix->loading_container_id);
    entry_suffix->loading_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
}

static inline void schedule_third_party_entry_loading (
    struct resource_provider_state_t *state,
    struct kan_resource_third_party_entry_t *entry,
    struct resource_provider_third_party_entry_suffix_t *entry_suffix,
    kan_bool_t from_hot_reload)
{
    if (entry_suffix->loading_data)
    {
        return;
    }

    kan_virtual_file_system_volume_t volume =
        kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);

    struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (volume, entry->path);
    kan_virtual_file_system_close_context_read_access (state->virtual_file_system);

    if (!stream)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Failed to open input stream for third party resource \"%s\" at path \"%s\".", entry->name,
                 entry->path)
        return;
    }

    entry_suffix->loading_data =
        kan_allocate_general (entry_suffix->my_allocation_group, entry->size, _Alignof (kan_memory_size_t));
    entry_suffix->loading_data_size = entry->size;

    KAN_UP_INDEXED_INSERT (operation, resource_provider_loading_operation_t)
    {
        operation->priority =
            from_hot_reload && kan_hot_reload_coordination_system_is_hot_swap (state->hot_reload_system) ?
                PRIORITY_HOT_SWAP :
                gather_request_priority (state, NULL, entry->name);

        operation->target_type = NULL;
        operation->target_name = entry->name;
        operation->stream = stream;
        operation->third_party.offset = 0u;
        operation->third_party.size = entry->size;
        operation->third_party.data = entry_suffix->loading_data;
    }
}

static inline void unload_third_party_entry (struct resource_provider_state_t *state,
                                             struct kan_resource_third_party_entry_t *entry,
                                             struct resource_provider_third_party_entry_suffix_t *entry_suffix)
{
    if (!entry_suffix->loaded_data)
    {
        return;
    }

    update_requests (state, NULL, entry->name, KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t), NULL, 0u);
    kan_free_general (entry_suffix->my_allocation_group, entry_suffix->loaded_data, entry_suffix->loaded_data_size);
    entry_suffix->loaded_data = NULL;
}

static inline void cancel_third_party_entry_loading (struct resource_provider_state_t *state,
                                                     struct kan_resource_third_party_entry_t *entry,
                                                     struct resource_provider_third_party_entry_suffix_t *entry_suffix)
{
    if (!entry_suffix->loading_data)
    {
        return;
    }

    loading_operation_cancel (state, NULL, entry->name);
    kan_free_general (entry_suffix->my_allocation_group, entry_suffix->loading_data, entry_suffix->loading_data_size);
    entry_suffix->loading_data = NULL;
    entry_suffix->loading_data_size = 0u;
}

static inline void add_native_entry_reference (struct resource_provider_state_t *state,
                                               struct resource_provider_private_singleton_t *private,
                                               kan_resource_request_id_t request_id,
                                               kan_interned_string_t type,
                                               kan_interned_string_t name)
{
    KAN_UP_VALUE_UPDATE (entry, kan_resource_native_entry_t, name, &name)
    {
        if (entry->type == type)
        {
            KAN_UP_VALUE_UPDATE (suffix, resource_provider_native_entry_suffix_t, attachment_id, &entry->attachment_id)
            {
                ++suffix->request_count;
                if (KAN_TYPED_ID_32_IS_VALID (suffix->loaded_container_id))
                {
                    KAN_UP_VALUE_UPDATE (request, kan_resource_request_t, request_id, &request_id)
                    {
                        update_request_provided_data (state, request, suffix->loaded_container_id, NULL, 0u);
                    }
                }
                else if (!KAN_TYPED_ID_32_IS_VALID (suffix->loading_container_id))
                {
                    schedule_native_entry_loading (state, private, entry, suffix, KAN_FALSE);
                }

                KAN_UP_QUERY_RETURN_VOID;
            }

            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                     "Unable to find suffix of requested native resource \"%s\" of type \"%s\".", name, type)
            KAN_UP_QUERY_RETURN_VOID;
        }
    }

    KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
             "Unable to find requested native resource \"%s\" of type \"%s\".", name, type)
}

static inline void add_third_party_entry_reference (struct resource_provider_state_t *state,
                                                    kan_resource_request_id_t request_id,
                                                    kan_interned_string_t name)
{
    KAN_UP_VALUE_UPDATE (entry, kan_resource_third_party_entry_t, name, &name)
    {
        KAN_UP_VALUE_UPDATE (suffix, resource_provider_third_party_entry_suffix_t, attachment_id, &entry->attachment_id)
        {
            ++suffix->request_count;
            if (suffix->loaded_data)
            {
                KAN_UP_VALUE_UPDATE (request, kan_resource_request_t, request_id, &request_id)
                {
                    update_request_provided_data (state, request,
                                                  KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t),
                                                  suffix->loaded_data, suffix->loaded_data_size);
                }
            }
            else if (!suffix->loading_data)
            {
                schedule_third_party_entry_loading (state, entry, suffix, KAN_FALSE);
            }

            KAN_UP_QUERY_RETURN_VOID;
        }

        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Unable to find suffix of requested third party resource \"%s\".", name)
        KAN_UP_QUERY_RETURN_VOID;
    }

    KAN_LOG (universe_resource_provider, KAN_LOG_ERROR, "Unable to find requested third party resource \"%s\".", name)
}

static inline void remove_native_entry_reference (struct resource_provider_state_t *state,
                                                  kan_interned_string_t type,
                                                  kan_interned_string_t name)
{
    KAN_UP_VALUE_UPDATE (entry, kan_resource_native_entry_t, name, &name)
    {
        if (entry->type == type)
        {
            KAN_UP_VALUE_UPDATE (suffix, resource_provider_native_entry_suffix_t, attachment_id, &entry->attachment_id)
            {
                KAN_ASSERT (suffix->request_count > 0u)
                --suffix->request_count;

                if (suffix->request_count == 0u)
                {
                    unload_native_entry (state, entry, suffix);
                    cancel_native_entry_loading (state, entry, suffix);
                }

                KAN_UP_QUERY_RETURN_VOID;
            }

            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                     "Unable to find suffix of requested native resource \"%s\" of type \"%s\".", name, type)
            KAN_UP_QUERY_RETURN_VOID;
        }
    }

    KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
             "Unable to find requested native resource \"%s\" of type \"%s\".", name, type)
}

static inline void remove_third_party_entry_reference (struct resource_provider_state_t *state,
                                                       kan_interned_string_t name)
{
    KAN_UP_VALUE_UPDATE (entry, kan_resource_third_party_entry_t, name, &name)
    {
        KAN_UP_VALUE_UPDATE (suffix, resource_provider_third_party_entry_suffix_t, attachment_id, &entry->attachment_id)
        {
            KAN_ASSERT (suffix->request_count > 0u)
            --suffix->request_count;

            if (suffix->request_count == 0u)
            {
                unload_third_party_entry (state, entry, suffix);
                cancel_third_party_entry_loading (state, entry, suffix);
            }

            KAN_UP_QUERY_RETURN_VOID;
        }

        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Unable to find suffix of requested third party resource \"%s\".", name)
        KAN_UP_QUERY_RETURN_VOID;
    }

    KAN_LOG (universe_resource_provider, KAN_LOG_ERROR, "Unable to find requested third party resource \"%s\".", name)
}

static inline void on_file_added (struct resource_provider_state_t *state,
                                  struct resource_provider_private_singleton_t *private,
                                  const char *path)
{
    // Skip first "/" in order to have same path format for scanned and observed files.
    if (path && path[0u] == '/')
    {
        ++path;
    }

    struct kan_hot_reload_automatic_config_t *automatic_config =
        kan_hot_reload_coordination_system_get_automatic_config (state->hot_reload_system);

    KAN_UP_INDEXED_INSERT (addition, resource_provider_delayed_file_addition_t)
    {
        const kan_instance_size_t path_length = (kan_instance_size_t) strlen (path);
        addition->path = kan_allocate_general (addition->my_allocation_group, path_length + 1u, _Alignof (char));
        memcpy (addition->path, path, path_length + 1u);

        struct kan_resource_index_info_from_path_t info_from_path;
        kan_resource_index_extract_info_from_path (path, &info_from_path);

        addition->name_for_search = info_from_path.name;
        const kan_time_size_t investigate_after_ns =
            kan_precise_time_get_elapsed_nanoseconds () + automatic_config->change_wait_time_ns;
        KAN_ASSERT (KAN_PACKED_TIMER_IS_SAFE_TO_SET (investigate_after_ns))
        addition->investigate_after_timer = KAN_PACKED_TIMER_SET (investigate_after_ns);
    }
}

static inline void on_file_modified (struct resource_provider_state_t *state,
                                     kan_virtual_file_system_volume_t volume,
                                     const char *path)
{
    // Skip first "/" in order to have same path format for scanned and observed files.
    if (path && path[0u] == '/')
    {
        ++path;
    }

    struct kan_resource_index_info_from_path_t info_from_path;
    kan_resource_index_extract_info_from_path (path, &info_from_path);

    KAN_UP_VALUE_WRITE (native_entry, kan_resource_native_entry_t, name, &info_from_path.name)
    {
        if (strcmp (path, native_entry->path) == 0)
        {
            KAN_UP_VALUE_UPDATE (suffix, resource_provider_native_entry_suffix_t, attachment_id,
                                 &native_entry->attachment_id)
            {
                if (suffix->request_count > 0u)
                {
                    struct kan_hot_reload_automatic_config_t *automatic_config =
                        kan_hot_reload_coordination_system_get_automatic_config (state->hot_reload_system);

                    const kan_time_size_t reload_after_ns =
                        kan_precise_time_get_elapsed_nanoseconds () + automatic_config->change_wait_time_ns;
                    KAN_ASSERT (KAN_PACKED_TIMER_IS_SAFE_TO_SET (reload_after_ns))
                    suffix->reload_after_real_time_timer = KAN_PACKED_TIMER_SET (reload_after_ns);
                }

                KAN_UP_QUERY_RETURN_VOID;
            }
        }
    }

    KAN_UP_VALUE_WRITE (third_party_entry, kan_resource_third_party_entry_t, name, &info_from_path.name)
    {
        if (strcmp (path, third_party_entry->path) == 0)
        {
            KAN_UP_VALUE_UPDATE (suffix, resource_provider_third_party_entry_suffix_t, attachment_id,
                                 &third_party_entry->attachment_id)
            {
                if (suffix->request_count > 0u)
                {
                    struct kan_hot_reload_automatic_config_t *automatic_config =
                        kan_hot_reload_coordination_system_get_automatic_config (state->hot_reload_system);

                    const kan_time_size_t reload_after_ns =
                        kan_precise_time_get_elapsed_nanoseconds () + automatic_config->change_wait_time_ns;
                    KAN_ASSERT (KAN_PACKED_TIMER_IS_SAFE_TO_SET (reload_after_ns))
                    suffix->reload_after_real_time_timer = KAN_PACKED_TIMER_SET (reload_after_ns);
                }

                KAN_UP_QUERY_RETURN_VOID;
            }
        }
    }

    // If it is file waiting for addition, we need to shift investigate timer.
    KAN_UP_VALUE_WRITE (addition, resource_provider_delayed_file_addition_t, name_for_search, &info_from_path.name)
    {
        if (strcmp (path, addition->path) == 0)
        {
            struct kan_hot_reload_automatic_config_t *automatic_config =
                kan_hot_reload_coordination_system_get_automatic_config (state->hot_reload_system);

            const kan_time_size_t investigate_after_ns =
                kan_precise_time_get_elapsed_nanoseconds () + automatic_config->change_wait_time_ns;
            KAN_ASSERT (KAN_PACKED_TIMER_IS_SAFE_TO_SET (investigate_after_ns))
            addition->investigate_after_timer = KAN_PACKED_TIMER_SET (investigate_after_ns);
            KAN_UP_QUERY_RETURN_VOID;
        }
    }
}

static inline void on_file_removed (struct resource_provider_state_t *state,
                                    kan_virtual_file_system_volume_t volume,
                                    const char *path)
{
    // Skip first "/" in order to have same path format for scanned and observed files.
    if (path && path[0u] == '/')
    {
        ++path;
    }

    struct kan_resource_index_info_from_path_t info_from_path;
    kan_resource_index_extract_info_from_path (path, &info_from_path);

    KAN_UP_VALUE_WRITE (native_entry, kan_resource_native_entry_t, name, &info_from_path.name)
    {
        if (strcmp (path, native_entry->path) == 0)
        {
            KAN_UP_VALUE_WRITE (suffix, resource_provider_native_entry_suffix_t, attachment_id,
                                &native_entry->attachment_id)
            {
                unload_native_entry (state, native_entry, suffix);
                cancel_native_entry_loading (state, native_entry, suffix);
                KAN_UP_ACCESS_DELETE (suffix);
            }

            KAN_UP_ACCESS_DELETE (native_entry);
            KAN_UP_QUERY_RETURN_VOID;
        }
    }

    KAN_UP_VALUE_WRITE (third_party_entry, kan_resource_third_party_entry_t, name, &info_from_path.name)
    {
        if (strcmp (path, third_party_entry->path) == 0)
        {
            KAN_UP_VALUE_WRITE (suffix, resource_provider_third_party_entry_suffix_t, attachment_id,
                                &third_party_entry->attachment_id)
            {
                unload_third_party_entry (state, third_party_entry, suffix);
                cancel_third_party_entry_loading (state, third_party_entry, suffix);
                KAN_UP_ACCESS_DELETE (suffix);
            }

            KAN_UP_ACCESS_DELETE (third_party_entry);
            KAN_UP_QUERY_RETURN_VOID;
        }
    }

    // If it is file waiting for addition, we need to cancel investigation.
    KAN_UP_VALUE_WRITE (addition, resource_provider_delayed_file_addition_t, name_for_search, &info_from_path.name)
    {
        if (strcmp (path, addition->path) == 0)
        {
            KAN_UP_ACCESS_DELETE (addition);
            KAN_UP_QUERY_RETURN_VOID;
        }
    }
}

static inline void process_request_on_insert (struct resource_provider_state_t *state,
                                              struct resource_provider_private_singleton_t *private)
{
    KAN_UP_EVENT_FETCH (event, resource_request_on_insert_event_t)
    {
        if (event->type)
        {
            add_native_entry_reference (state, private, event->request_id, event->type, event->name);
        }
        else
        {
            add_third_party_entry_reference (state, event->request_id, event->name);
        }
    }
}

static inline void process_request_on_change (struct resource_provider_state_t *state,
                                              struct resource_provider_private_singleton_t *private)
{
    KAN_UP_EVENT_FETCH (event, resource_request_on_change_event_t)
    {
        if (event->old_type)
        {
            remove_native_entry_reference (state, event->old_type, event->old_name);
        }
        else
        {
            remove_third_party_entry_reference (state, event->old_name);
        }

        if (event->new_type)
        {
            add_native_entry_reference (state, private, event->request_id, event->new_type, event->new_name);
        }
        else
        {
            add_third_party_entry_reference (state, event->request_id, event->new_name);
        }
    }
}

static inline void process_request_on_delete (struct resource_provider_state_t *state)
{
    KAN_UP_EVENT_FETCH (event, resource_request_on_delete_event_t)
    {
        if (event->type)
        {
            remove_native_entry_reference (state, event->type, event->name);
        }
        else
        {
            remove_third_party_entry_reference (state, event->name);
        }
    }
}

static inline void process_file_addition (struct resource_provider_state_t *state,
                                          struct resource_provider_private_singleton_t *private,
                                          const char *path)
{
    kan_virtual_file_system_volume_t volume =
        kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);
    struct kan_virtual_file_system_entry_status_t status;

    if (!kan_virtual_file_system_query_entry (volume, path, &status))
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Failed to react to \"%s\" addition, unable to query status.", path)
        kan_virtual_file_system_close_context_read_access (state->virtual_file_system);
        return;
    }

    KAN_ASSERT (status.type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
    struct file_scan_result_t scan_result = scan_file (state, private, volume, path, status.size);
    kan_virtual_file_system_close_context_read_access (state->virtual_file_system);

    if (!scan_result.name)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR, "Failed to react to \"%s\" addition, scan failed.", path)
        return;
    }

    kan_loop_size_t request_count = 0u;
    KAN_UP_VALUE_READ (request, kan_resource_request_t, name, &scan_result.name)
    {
        if (request->type == scan_result.type)
        {
            ++request_count;
        }
    }

    if (request_count > 0u)
    {
        if (scan_result.type)
        {
            KAN_UP_VALUE_UPDATE (entry, kan_resource_native_entry_t, name, &scan_result.name)
            {
                if (entry->type == scan_result.type)
                {
                    KAN_UP_VALUE_UPDATE (suffix, resource_provider_native_entry_suffix_t, attachment_id,
                                         &entry->attachment_id)
                    {
                        suffix->request_count = request_count;
                        KAN_ASSERT (!KAN_TYPED_ID_32_IS_VALID (suffix->loaded_container_id) &&
                                    !KAN_TYPED_ID_32_IS_VALID (suffix->loading_container_id))

                        schedule_native_entry_loading (state, private, entry, suffix, KAN_TRUE);
                        KAN_UP_QUERY_RETURN_VOID;
                    }
                }
            }
        }
        else
        {
            KAN_UP_VALUE_UPDATE (entry, kan_resource_third_party_entry_t, name, &scan_result.name)
            {
                KAN_UP_VALUE_UPDATE (suffix, resource_provider_third_party_entry_suffix_t, attachment_id,
                                     &entry->attachment_id)
                {
                    suffix->request_count = request_count;
                    KAN_ASSERT (suffix->loaded_data == NULL && suffix->loading_data == NULL)

                    schedule_third_party_entry_loading (state, entry, suffix, KAN_TRUE);
                    KAN_UP_QUERY_RETURN_VOID;
                }
            }
        }
    }
}

static inline void process_delayed_addition (struct resource_provider_state_t *state,
                                             struct resource_provider_private_singleton_t *private)
{
    if (kan_hot_reload_coordination_system_get_current_mode (state->hot_reload_system) !=
            KAN_HOT_RELOAD_MODE_AUTOMATIC_INDEPENDENT &&
        !kan_hot_reload_coordination_system_is_hot_swap (state->hot_reload_system))
    {
        return;
    }

    KAN_ASSERT (KAN_PACKED_TIMER_IS_SAFE_TO_SET (kan_precise_time_get_elapsed_nanoseconds ()))
    const kan_packed_timer_t current_timer = kan_hot_reload_coordination_system_is_hot_swap (state->hot_reload_system) ?
                                                 KAN_PACKED_TIMER_MAX :
                                                 KAN_PACKED_TIMER_SET (kan_precise_time_get_elapsed_nanoseconds ());

    KAN_UP_INTERVAL_ASCENDING_WRITE (delayed_addition, resource_provider_delayed_file_addition_t,
                                     investigate_after_timer, NULL, &current_timer)
    {
        process_file_addition (state, private, delayed_addition->path);
        KAN_UP_ACCESS_DELETE (delayed_addition);
    }
}

static inline void process_delayed_reload (struct resource_provider_state_t *state,
                                           struct resource_provider_private_singleton_t *private)
{
    if (kan_hot_reload_coordination_system_get_current_mode (state->hot_reload_system) !=
            KAN_HOT_RELOAD_MODE_AUTOMATIC_INDEPENDENT &&
        !kan_hot_reload_coordination_system_is_hot_swap (state->hot_reload_system))
    {
        return;
    }

    KAN_ASSERT (KAN_PACKED_TIMER_IS_SAFE_TO_SET (kan_precise_time_get_elapsed_nanoseconds ()))
    const kan_packed_timer_t current_timer = kan_hot_reload_coordination_system_is_hot_swap (state->hot_reload_system) ?
                                                 KAN_PACKED_TIMER_MAX :
                                                 KAN_PACKED_TIMER_SET (kan_precise_time_get_elapsed_nanoseconds ());

    KAN_UP_INTERVAL_ASCENDING_UPDATE (native_suffix, resource_provider_native_entry_suffix_t,
                                      reload_after_real_time_timer, NULL, &current_timer)
    {
        KAN_UP_VALUE_UPDATE (entry, kan_resource_native_entry_t, attachment_id, &native_suffix->attachment_id)
        {
            if (native_suffix->request_count > 0u)
            {
                cancel_native_entry_loading (state, entry, native_suffix);

                // Read type header in case if type was modified.
                kan_virtual_file_system_volume_t volume =
                    kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);

                struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (volume, entry->path);
                kan_virtual_file_system_close_context_read_access (state->virtual_file_system);

                if (stream)
                {
                    kan_interned_string_t new_type;
                    if (read_type_header (stream, entry, native_suffix, &new_type))
                    {
                        if (entry->type != new_type)
                        {
                            unload_native_entry (state, entry, native_suffix);
                            entry->type = new_type;
                        }
                    }

                    stream->operations->close (stream);
                }
                else
                {
                    KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                             "Failed to open input stream for native resource \"%s\" of type \"%s\" at path \"%s\".",
                             entry->name, entry->type, entry->path)
                }

                schedule_native_entry_loading (state, private, entry, native_suffix, KAN_TRUE);
            }

            native_suffix->reload_after_real_time_timer = KAN_PACKED_TIMER_NEVER;
            KAN_UP_QUERY_BREAK;
        }
    }

    KAN_UP_INTERVAL_ASCENDING_UPDATE (third_party_suffix, resource_provider_third_party_entry_suffix_t,
                                      reload_after_real_time_timer, NULL, &current_timer)
    {
        KAN_UP_VALUE_UPDATE (entry, kan_resource_third_party_entry_t, attachment_id, &third_party_suffix->attachment_id)
        {
            if (third_party_suffix->request_count > 0u)
            {
                cancel_third_party_entry_loading (state, entry, third_party_suffix);

                // Update third party size.
                struct kan_virtual_file_system_entry_status_t status;
                kan_virtual_file_system_volume_t volume =
                    kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);

                if (kan_virtual_file_system_query_entry (volume, entry->path, &status))
                {
                    entry->size = status.size;
                }
                else
                {
                    KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                             "Failed to query third party resource entry \"%s\" at path \"%s\".", entry->name,
                             entry->path)
                }

                kan_virtual_file_system_close_context_read_access (state->virtual_file_system);
                schedule_third_party_entry_loading (state, entry, third_party_suffix, KAN_TRUE);
            }

            third_party_suffix->reload_after_real_time_timer = KAN_PACKED_TIMER_NEVER;
            KAN_UP_QUERY_BREAK;
        }
    }
}

static void execute_shared_serve (kan_functor_user_data_t user_data)
{
    struct resource_provider_state_t *state = (struct resource_provider_state_t *) user_data;
    while (KAN_TRUE)
    {
        // Retrieve loading operation.
        kan_atomic_int_lock (&state->execution_shared_state.concurrency_lock);
        struct kan_repository_indexed_interval_write_access_t loading_operation_access =
            kan_repository_indexed_interval_descending_write_cursor_next (
                &state->execution_shared_state.loading_operation_cursor);
        kan_atomic_int_unlock (&state->execution_shared_state.concurrency_lock);

        struct resource_provider_loading_operation_t *loading_operation =
            (struct resource_provider_loading_operation_t *) kan_repository_indexed_interval_write_access_resolve (
                &loading_operation_access);

        if (!loading_operation)
        {
            // Shutdown: no loading operations.
            if (kan_atomic_int_add (&state->execution_shared_state.workers_left, -1) == 1)
            {
                kan_repository_indexed_interval_descending_write_cursor_close (
                    &state->execution_shared_state.loading_operation_cursor);
            }

            return;
        }

        if (loading_operation->priority != PRIORITY_HOT_SWAP &&
            kan_precise_time_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
        {
            // Shutdown: no more time.
            kan_repository_indexed_interval_write_access_close (&loading_operation_access);

            if (kan_atomic_int_add (&state->execution_shared_state.workers_left, -1) == 1)
            {
                kan_repository_indexed_interval_descending_write_cursor_close (
                    &state->execution_shared_state.loading_operation_cursor);
            }

            return;
        }

        // Restart loading for native type if reflection has changed.
        if (loading_operation->target_type &&
            !KAN_HANDLE_IS_EQUAL (loading_operation->native.used_registry, state->reflection_registry))
        {
            resource_provider_loading_operation_destroy_native (loading_operation);
            loading_operation->stream->operations->close (loading_operation->stream);

            kan_bool_t entry_found = KAN_FALSE;
            kan_bool_t other_error = KAN_FALSE;

            KAN_UP_VALUE_READ (entry, kan_resource_native_entry_t, name, &loading_operation->target_name)
            {
                if (entry->type == loading_operation->target_type)
                {
                    entry_found = KAN_TRUE;
                    kan_virtual_file_system_volume_t volume =
                        kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);

                    loading_operation->stream = kan_virtual_file_stream_open_for_read (volume, entry->path);
                    kan_virtual_file_system_close_context_read_access (state->virtual_file_system);

                    if (!loading_operation->stream)
                    {
                        KAN_LOG (
                            universe_resource_provider, KAN_LOG_ERROR,
                            "Unable to restart loading of \"%s\" of type \"%s\" as stream can no longer be opened. "
                            "Internal handling error?",
                            loading_operation->target_name, loading_operation->target_type)

                        other_error = KAN_TRUE;
                        KAN_UP_QUERY_BREAK;
                    }

                    loading_operation->stream = kan_random_access_stream_buffer_open_for_read (
                        loading_operation->stream, KAN_UNIVERSE_RESOURCE_PROVIDER_READ_BUFFER);

                    KAN_UP_VALUE_UPDATE (suffix, resource_provider_native_entry_suffix_t, attachment_id,
                                         &entry->attachment_id)
                    {
                        if (!skip_type_header (loading_operation->stream, entry, suffix))
                        {
                            other_error = KAN_TRUE;
                            KAN_UP_QUERY_BREAK;
                        }

                        struct kan_repository_indexed_value_update_access_t container_view_access;
                        uint8_t *container_data_begin;
                        struct kan_resource_container_view_t *container_view =
                            native_container_update (state, entry->type, suffix->loading_container_id,
                                                     &container_view_access, &container_data_begin);

                        if (!container_view)
                        {
                            KAN_LOG (
                                universe_resource_provider, KAN_LOG_ERROR,
                                "Unable to restart loading of \"%s\" of type \"%s\" as loading container is absent. "
                                "Internal handling error?",
                                loading_operation->target_name, loading_operation->target_type)

                            other_error = KAN_TRUE;
                            KAN_UP_QUERY_BREAK;
                        }

                        switch (suffix->format)
                        {
                        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY:
                            loading_operation->native.binary_reader = kan_serialization_binary_reader_create (
                                loading_operation->stream, container_data_begin, entry->type,
                                state->shared_script_storage, suffix->string_registry,
                                container_view->my_allocation_group);
                            break;

                        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA:
                            loading_operation->native.readable_data_reader = kan_serialization_rd_reader_create (
                                loading_operation->stream, container_data_begin, entry->type,
                                state->reflection_registry, container_view->my_allocation_group);
                            break;
                        }
                    }

                    KAN_UP_QUERY_BREAK;
                }
            }

            if (!entry_found)
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                         "Unable to restart loading of \"%s\" of type \"%s\" as native entry no longer exists. "
                         "Internal handling error?",
                         loading_operation->target_name, loading_operation->target_type)
            }

            if (!entry_found || other_error)
            {
                kan_repository_indexed_interval_write_access_delete (&loading_operation_access);
                continue;
            }
        }

        // Process serialization.
        enum kan_serialization_state_t serialization_state = KAN_SERIALIZATION_FINISHED;

        if (loading_operation->target_type)
        {
            switch (loading_operation->native.format_cache)
            {
            case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY:
            {
                while ((serialization_state = kan_serialization_binary_reader_step (
                            loading_operation->native.binary_reader)) == KAN_SERIALIZATION_IN_PROGRESS)
                {
                    if (loading_operation->priority != PRIORITY_HOT_SWAP &&
                        kan_precise_time_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
                    {
                        // Shutdown: no more time.
                        kan_repository_indexed_interval_write_access_close (&loading_operation_access);

                        if (kan_atomic_int_add (&state->execution_shared_state.workers_left, -1) == 1)
                        {
                            kan_repository_indexed_interval_descending_write_cursor_close (
                                &state->execution_shared_state.loading_operation_cursor);
                        }

                        return;
                    }
                }

                break;
            }

            case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA:
                while ((serialization_state = kan_serialization_rd_reader_step (
                            loading_operation->native.readable_data_reader)) == KAN_SERIALIZATION_IN_PROGRESS)
                {
                    if (loading_operation->priority != PRIORITY_HOT_SWAP &&
                        kan_precise_time_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
                    {
                        // Shutdown: no more time.
                        kan_repository_indexed_interval_write_access_close (&loading_operation_access);

                        if (kan_atomic_int_add (&state->execution_shared_state.workers_left, -1) == 1)
                        {
                            kan_repository_indexed_interval_descending_write_cursor_close (
                                &state->execution_shared_state.loading_operation_cursor);
                        }

                        return;
                    }
                }

                break;
            }
        }
        else
        {
            while (KAN_TRUE)
            {
                kan_memory_size_t to_read =
                    KAN_MIN (KAN_UNIVERSE_RESOURCE_PROVIDER_TPL_CHUNK,
                             loading_operation->third_party.size - loading_operation->third_party.offset);

                if (to_read > 0u)
                {
                    if (loading_operation->stream->operations->read (
                            loading_operation->stream, to_read,
                            loading_operation->third_party.data + loading_operation->third_party.offset) == to_read)
                    {
                        loading_operation->third_party.offset += to_read;
                        serialization_state =
                            loading_operation->third_party.offset == loading_operation->third_party.size ?
                                KAN_SERIALIZATION_FINISHED :
                                KAN_SERIALIZATION_IN_PROGRESS;
                    }
                    else
                    {
                        serialization_state = KAN_SERIALIZATION_FAILED;
                    }
                }
                else
                {
                    serialization_state = KAN_SERIALIZATION_FINISHED;
                }

                if (loading_operation->priority != PRIORITY_HOT_SWAP &&
                    kan_precise_time_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
                {
                    // Shutdown: no more time.
                    kan_repository_indexed_interval_write_access_close (&loading_operation_access);

                    if (kan_atomic_int_add (&state->execution_shared_state.workers_left, -1) == 1)
                    {
                        kan_repository_indexed_interval_descending_write_cursor_close (
                            &state->execution_shared_state.loading_operation_cursor);
                    }

                    return;
                }

                if (serialization_state != KAN_SERIALIZATION_IN_PROGRESS)
                {
                    break;
                }
            }
        }

        kan_atomic_int_lock (&state->execution_shared_state.concurrency_lock);
        switch (serialization_state)
        {
        case KAN_SERIALIZATION_IN_PROGRESS:
            KAN_ASSERT (KAN_FALSE)
            break;

        case KAN_SERIALIZATION_FINISHED:
            if (loading_operation->target_type)
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_DEBUG, "Loaded native resource \"%s\" of type \"%s\".",
                         loading_operation->target_name, loading_operation->target_type)

                KAN_UP_VALUE_READ (entry, kan_resource_native_entry_t, name, &loading_operation->target_name)
                {
                    if (entry->type == loading_operation->target_type)
                    {
                        KAN_UP_VALUE_UPDATE (suffix, resource_provider_native_entry_suffix_t, attachment_id,
                                             &entry->attachment_id)
                        {
                            native_container_delete (state, entry->type, suffix->loaded_container_id);
                            suffix->loaded_container_id = suffix->loading_container_id;
                            suffix->loading_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
                            update_requests (state, entry->type, entry->name, suffix->loaded_container_id, NULL, 0u);
                        }

                        KAN_UP_QUERY_BREAK;
                    }
                }
            }
            else
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_DEBUG, "Loaded third party resource \"%s\".",
                         loading_operation->target_name)

                KAN_UP_VALUE_READ (entry, kan_resource_third_party_entry_t, name, &loading_operation->target_name)
                {
                    KAN_UP_VALUE_UPDATE (suffix, resource_provider_third_party_entry_suffix_t, attachment_id,
                                         &entry->attachment_id)
                    {
                        if (suffix->loaded_data)
                        {
                            kan_free_general (suffix->my_allocation_group, suffix->loaded_data,
                                              suffix->loaded_data_size);
                        }

                        suffix->loaded_data = suffix->loading_data;
                        suffix->loaded_data_size = suffix->loading_data_size;
                        suffix->loading_data = NULL;
                        suffix->loading_data_size = 0u;

                        update_requests (state, NULL, entry->name,
                                         KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t), suffix->loaded_data,
                                         suffix->loaded_data_size);
                    }

                    KAN_UP_QUERY_BREAK;
                }
            }

            kan_repository_indexed_interval_write_access_delete (&loading_operation_access);
            break;

        case KAN_SERIALIZATION_FAILED:
        {
            if (loading_operation->target_type)
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                         "Failed to load native resource \"%s\" of type \"%s\": serialization error.",
                         loading_operation->target_name, loading_operation->target_type)

                KAN_UP_VALUE_READ (entry, kan_resource_native_entry_t, name, &loading_operation->target_name)
                {
                    if (entry->type == loading_operation->target_type)
                    {
                        KAN_UP_VALUE_UPDATE (suffix, resource_provider_native_entry_suffix_t, attachment_id,
                                             &entry->attachment_id)
                        {
                            native_container_delete (state, entry->type, suffix->loading_container_id);
                            suffix->loading_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
                        }

                        KAN_UP_QUERY_BREAK;
                    }
                }
            }
            else
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                         "Failed to load third party resource \"%s\": serialization error.",
                         loading_operation->target_name)

                KAN_UP_VALUE_READ (entry, kan_resource_third_party_entry_t, name, &loading_operation->target_name)
                {
                    KAN_UP_VALUE_UPDATE (suffix, resource_provider_third_party_entry_suffix_t, attachment_id,
                                         &entry->attachment_id)
                    {
                        if (suffix->loading_data)
                        {
                            kan_free_general (suffix->my_allocation_group, suffix->loading_data,
                                              suffix->loading_data_size);
                        }

                        suffix->loading_data = NULL;
                        suffix->loading_data_size = 0u;
                    }

                    KAN_UP_QUERY_BREAK;
                }
            }

            kan_repository_indexed_interval_write_access_delete (&loading_operation_access);
            break;
        }
        }

        kan_atomic_int_unlock (&state->execution_shared_state.concurrency_lock);
    }
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void mutator_template_execute_resource_provider (
    kan_cpu_job_t job, struct resource_provider_state_t *state)
{
    const kan_time_size_t begin_time = kan_precise_time_get_elapsed_nanoseconds ();
    KAN_UP_SINGLETON_WRITE (public, kan_resource_provider_singleton_t)
    KAN_UP_SINGLETON_WRITE (private, resource_provider_private_singleton_t)
    {
        if (private->status == RESOURCE_PROVIDER_STATUS_NOT_INITIALIZED)
        {
            prepare_for_scanning (state, private);
            private->status = RESOURCE_PROVIDER_STATUS_SCANNING;
        }

        if (private->status == RESOURCE_PROVIDER_STATUS_SERVING && public->request_rescan)
        {
            clear_entries (state);
            for (kan_loop_size_t index = 0u; index < private->loaded_string_registries.size; ++index)
            {
                kan_serialization_interned_string_registry_destroy (
                    ((kan_serialization_interned_string_registry_t *) private->loaded_string_registries.data)[index]);
            }

            kan_dynamic_array_reset (&private->loaded_string_registries);
            if (KAN_HANDLE_IS_VALID (private->resource_watcher))
            {
                kan_virtual_file_system_watcher_iterator_destroy (private->resource_watcher,
                                                                  private->resource_watcher_iterator);
                kan_virtual_file_system_watcher_destroy (private->resource_watcher);
            }

            prepare_for_scanning (state, private);
            private->status = RESOURCE_PROVIDER_STATUS_SCANNING;
            public->request_rescan = KAN_FALSE;
            public->scan_done = KAN_FALSE;
        }

        if (private->status == RESOURCE_PROVIDER_STATUS_SCANNING &&
            begin_time + state->scan_budget_ns > kan_precise_time_get_elapsed_nanoseconds ())
        {
            kan_virtual_file_system_volume_t volume =
                kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);

            while (private->scan_item_stack.size > 0u &&
                   begin_time + state->scan_budget_ns > kan_precise_time_get_elapsed_nanoseconds ())
            {
                if (state->string_registry_stream)
                {
                    if (!KAN_HANDLE_IS_VALID (state->string_registry_reader))
                    {
                        state->string_registry_reader = kan_serialization_interned_string_registry_reader_create (
                            state->string_registry_stream, state->use_load_only_string_registry);
                    }

                    switch (kan_serialization_interned_string_registry_reader_step (state->string_registry_reader))
                    {
                    case KAN_SERIALIZATION_IN_PROGRESS:
                        break;

                    case KAN_SERIALIZATION_FINISHED:
                        state->string_registry_stream->operations->close (state->string_registry_stream);
                        state->string_registry_stream = NULL;
                        break;

                    case KAN_SERIALIZATION_FAILED:
                        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                                 "Failed to deserialize string registry, its directory will be skipped.")

                        kan_serialization_interned_string_registry_reader_destroy (state->string_registry_reader);
                        state->string_registry_reader =
                            KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_reader_t);

                        state->string_registry_stream->operations->close (state->string_registry_stream);
                        state->string_registry_stream = NULL;

                        state->serialized_index_stream->operations->close (state->serialized_index_stream);
                        state->serialized_index_stream = NULL;

                        // Destroy last scan task that issued this read procedure.
                        destroy_scan_item_task (private, private->scan_item_stack.size - 1u);
                        break;
                    }

                    continue;
                }

                if (state->serialized_index_stream)
                {
                    if (!KAN_HANDLE_IS_VALID (state->serialized_index_reader))
                    {
                        kan_serialization_interned_string_registry_t registry =
                            !KAN_HANDLE_IS_VALID (state->string_registry_reader) ?
                                KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t) :
                                kan_serialization_interned_string_registry_reader_get (state->string_registry_reader);

                        kan_resource_index_init (&state->serialized_index_read_buffer);
                        state->serialized_index_reader = kan_serialization_binary_reader_create (
                            state->serialized_index_stream, &state->serialized_index_read_buffer,
                            state->interned_kan_resource_index_t, state->shared_script_storage, registry,
                            kan_resource_index_get_string_allocation_group ());
                    }

                    switch (kan_serialization_binary_reader_step (state->serialized_index_reader))
                    {
                    case KAN_SERIALIZATION_IN_PROGRESS:
                        break;

                    case KAN_SERIALIZATION_FINISHED:
                    {
                        kan_serialization_interned_string_registry_t string_registry =
                            KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t);

                        if (KAN_HANDLE_IS_VALID (state->string_registry_reader))
                        {
                            string_registry =
                                kan_serialization_interned_string_registry_reader_get (state->string_registry_reader);

                            void *spot = kan_dynamic_array_add_last (&private->loaded_string_registries);
                            if (!spot)
                            {
                                kan_dynamic_array_set_capacity (&private->loaded_string_registries,
                                                                private->loaded_string_registries.capacity * 2u);
                                spot = kan_dynamic_array_add_last (&private->loaded_string_registries);
                                KAN_ASSERT (spot)
                            }

                            *(kan_serialization_interned_string_registry_t *) spot = string_registry;
                            kan_serialization_interned_string_registry_reader_destroy (state->string_registry_reader);
                            state->string_registry_reader =
                                KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_reader_t);
                        }

                        instantiate_resource_index (state, private, string_registry);
                        kan_resource_index_shutdown (&state->serialized_index_read_buffer);
                        kan_serialization_binary_reader_destroy (state->serialized_index_reader);
                        state->serialized_index_reader = KAN_HANDLE_SET_INVALID (kan_serialization_binary_reader_t);

                        state->serialized_index_stream->operations->close (state->serialized_index_stream);
                        state->serialized_index_stream = NULL;

                        // Destroy last scan task that issued this read procedure.
                        destroy_scan_item_task (private, private->scan_item_stack.size - 1u);
                        break;
                    }

                    case KAN_SERIALIZATION_FAILED:
                        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                                 "Failed to deserialize resource index, its directory will be skipped.")

                        if (KAN_HANDLE_IS_VALID (state->string_registry_reader))
                        {
                            kan_serialization_interned_string_registry_destroy (
                                kan_serialization_interned_string_registry_reader_get (state->string_registry_reader));
                            kan_serialization_interned_string_registry_reader_destroy (state->string_registry_reader);
                            state->string_registry_reader =
                                KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_reader_t);
                        }

                        kan_resource_index_shutdown (&state->serialized_index_read_buffer);
                        kan_serialization_binary_reader_destroy (state->serialized_index_reader);
                        state->serialized_index_reader = KAN_HANDLE_SET_INVALID (kan_serialization_binary_reader_t);

                        state->serialized_index_stream->operations->close (state->serialized_index_stream);
                        state->serialized_index_stream = NULL;

                        // Destroy last scan task that issued this read procedure.
                        destroy_scan_item_task (private, private->scan_item_stack.size - 1u);
                        break;
                    }

                    continue;
                }

                kan_instance_size_t task_index = private->scan_item_stack.size - 1u;
                if (scan_item (state, private, volume,
                               ((struct scan_item_task_t *) private->scan_item_stack.data)[task_index].path))
                {
                    destroy_scan_item_task (private, task_index);
                }
            }

            kan_virtual_file_system_close_context_read_access (state->virtual_file_system);
            if (private->scan_item_stack.size == 0u)
            {
                kan_dynamic_array_set_capacity (&private->scan_item_stack, 0u);
                kan_dynamic_array_set_capacity (&private->loaded_string_registries,
                                                private->loaded_string_registries.size);

                if (KAN_HANDLE_IS_VALID (state->hot_reload_system) &&
                    kan_hot_reload_coordination_system_get_current_mode (state->hot_reload_system) !=
                        KAN_HOT_RELOAD_MODE_DISABLED)
                {
                    volume = kan_virtual_file_system_get_context_volume_for_write (state->virtual_file_system);
                    private->resource_watcher =
                        kan_virtual_file_system_watcher_create (volume, state->resource_directory_path);
                    private->resource_watcher_iterator =
                        kan_virtual_file_system_watcher_iterator_create (private->resource_watcher);
                    kan_virtual_file_system_close_context_write_access (state->virtual_file_system);
                }

                public->scan_done = KAN_TRUE;
                private->status = RESOURCE_PROVIDER_STATUS_SERVING;
            }
        }

        if (private->status == RESOURCE_PROVIDER_STATUS_SERVING &&
            begin_time + state->load_budget_ns > kan_precise_time_get_elapsed_nanoseconds ())
        {
            // Events need to be always processed in order to keep everything up to date.
            // Therefore, load budget does not affect event processing.

            if (KAN_HANDLE_IS_VALID (private->resource_watcher))
            {
                kan_virtual_file_system_volume_t volume =
                    kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);
                const struct kan_virtual_file_system_watcher_event_t *event;

                while ((event = kan_virtual_file_system_watcher_iterator_get (private->resource_watcher,
                                                                              private->resource_watcher_iterator)))
                {
                    if (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
                    {
                        switch (event->event_type)
                        {
                        case KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_ADDED:
                            on_file_added (state, private, event->path_container.path);
                            break;

                        case KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_MODIFIED:
                            on_file_modified (state, volume, event->path_container.path);
                            break;

                        case KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_REMOVED:
                            on_file_removed (state, volume, event->path_container.path);
                            break;
                        }
                    }

                    private->resource_watcher_iterator = kan_virtual_file_system_watcher_iterator_advance (
                        private->resource_watcher, private->resource_watcher_iterator);
                }

                kan_virtual_file_system_close_context_read_access (state->virtual_file_system);
            }

            process_request_on_insert (state, private);
            process_request_on_change (state, private);
            process_request_on_delete (state);

            if (KAN_HANDLE_IS_VALID (private->resource_watcher))
            {
                process_delayed_addition (state, private);
                process_delayed_reload (state, private);
            }

            kan_stack_group_allocator_reset (&state->temporary_allocator);
            const kan_instance_size_t cpu_count = kan_platform_get_cpu_logical_core_count ();
            struct kan_cpu_task_list_node_t *task_list_node = NULL;

            state->execution_shared_state.workers_left = kan_atomic_int_init ((int) cpu_count);
            state->execution_shared_state.concurrency_lock = kan_atomic_int_init (0);
            state->execution_shared_state.loading_operation_cursor =
                kan_repository_indexed_interval_write_query_execute_descending (
                    &state->write_interval__resource_provider_loading_operation__priority, NULL, NULL);
            state->execution_shared_state.end_time_ns = begin_time + state->load_budget_ns;

            for (kan_loop_size_t worker_index = 0u; worker_index < cpu_count; ++worker_index)
            {
                KAN_CPU_TASK_LIST_USER_VALUE (&task_list_node, &state->temporary_allocator,
                                              state->interned_resource_provider_server, execute_shared_serve, state)
            }

            kan_cpu_job_dispatch_and_detach_task_list (job, task_list_node);
        }
    }

    KAN_UP_MUTATOR_RETURN;
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void mutator_template_undeploy_resource_provider (
    struct resource_provider_state_t *state)
{
    kan_serialization_binary_script_storage_destroy (state->shared_script_storage);
    if (state->serialized_index_stream)
    {
        if (KAN_HANDLE_IS_VALID (state->serialized_index_reader))
        {
            kan_resource_index_shutdown (&state->serialized_index_read_buffer);
            state->serialized_index_reader = KAN_HANDLE_SET_INVALID (kan_serialization_binary_reader_t);
        }

        state->serialized_index_stream->operations->close (state->serialized_index_stream);
        state->serialized_index_stream = NULL;
    }

    if (state->string_registry_stream)
    {
        if (KAN_HANDLE_IS_VALID (state->string_registry_reader))
        {
            kan_serialization_interned_string_registry_reader_destroy (state->string_registry_reader);
            state->string_registry_reader =
                KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_reader_t);
        }

        state->string_registry_stream->operations->close (state->string_registry_stream);
        state->string_registry_stream = NULL;
    }

    kan_stack_group_allocator_reset (&state->temporary_allocator);
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_state_shutdown (struct resource_provider_state_t *state)
{
    kan_stack_group_allocator_shutdown (&state->temporary_allocator);
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void kan_reflection_generator_universe_resource_provider_init (
    struct kan_reflection_generator_universe_resource_provider_t *instance)
{
    instance->generated_reflection_group =
        kan_allocation_group_get_child (kan_allocation_group_stack_get (), "generated_reflection");
    instance->first_container_type = NULL;
    instance->container_types_count = 0u;

    instance->interned_kan_resource_resource_type_meta_t = kan_string_intern ("kan_resource_resource_type_meta_t");
    instance->interned_container_id = kan_string_intern ("container_id");
    instance->interned_stored_resource = kan_string_intern ("stored_resource");
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void kan_reflection_generator_universe_resource_provider_bootstrap (
    struct kan_reflection_generator_universe_resource_provider_t *instance, kan_loop_size_t bootstrap_iteration)
{
    instance->boostrap_iteration = bootstrap_iteration;
}

static void generated_container_init (kan_functor_user_data_t function_user_data, void *data)
{
    ((struct kan_resource_container_view_t *) data)->container_id =
        KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
    ((struct kan_resource_container_view_t *) data)->my_allocation_group = kan_allocation_group_stack_get ();
    const struct kan_reflection_struct_t *boxed_type = (const struct kan_reflection_struct_t *) function_user_data;

    if (boxed_type->init)
    {
        const kan_memory_size_t offset =
            kan_apply_alignment (offsetof (struct kan_resource_container_view_t, data_begin), boxed_type->alignment);
        uint8_t *address = (uint8_t *) data + offset;
        boxed_type->init (boxed_type->functor_user_data, address);
    }
}

static void generated_container_shutdown (kan_functor_user_data_t function_user_data, void *data)
{
    const struct kan_reflection_struct_t *boxed_type = (const struct kan_reflection_struct_t *) function_user_data;
    if (boxed_type->shutdown)
    {
        const kan_memory_size_t offset =
            kan_apply_alignment (offsetof (struct kan_resource_container_view_t, data_begin), boxed_type->alignment);
        uint8_t *address = (uint8_t *) data + offset;

        kan_allocation_group_stack_push (((struct kan_resource_container_view_t *) data)->my_allocation_group);
        boxed_type->shutdown (boxed_type->functor_user_data, address);
        kan_allocation_group_stack_pop ();
    }
}

static inline void reflection_generation_iteration_check_type (
    struct kan_reflection_generator_universe_resource_provider_t *instance,
    kan_reflection_registry_t registry,
    const struct kan_reflection_struct_t *type,
    const struct kan_resource_resource_type_meta_t *meta,
    kan_reflection_system_generation_iterator_t generation_iterator)
{
    struct universe_resource_provider_generated_container_type_node_t *node =
        (struct universe_resource_provider_generated_container_type_node_t *) kan_allocate_batched (
            instance->generated_reflection_group,
            sizeof (struct universe_resource_provider_generated_container_type_node_t));

    node->source_type = type;
    node->next = instance->first_container_type;
    instance->first_container_type = node;
    ++instance->container_types_count;

#define MAX_GENERATED_NAME_LENGTH 256u
    char buffer[MAX_GENERATED_NAME_LENGTH];
    snprintf (buffer, MAX_GENERATED_NAME_LENGTH, KAN_RESOURCE_PROVIDER_CONTAINER_TYPE_PREFIX "%s", type->name);
    node->type.name = kan_string_intern (buffer);
#undef MAX_GENERATED_NAME_LENGTH

    const kan_memory_size_t container_alignment =
        KAN_MAX (_Alignof (struct kan_resource_container_view_t), type->alignment);
    const kan_memory_size_t data_offset =
        kan_apply_alignment (offsetof (struct kan_resource_container_view_t, data_begin), container_alignment);

    node->type.alignment = (kan_instance_size_t) container_alignment;
    node->type.size = (kan_instance_size_t) kan_apply_alignment (data_offset + type->size, container_alignment);

    node->type.functor_user_data = (kan_functor_user_data_t) type;
    node->type.init = generated_container_init;
    node->type.shutdown = generated_container_shutdown;

    node->type.fields_count = 2u;
    node->type.fields =
        kan_allocate_general (instance->generated_reflection_group, sizeof (struct kan_reflection_field_t) * 2u,
                              _Alignof (struct kan_reflection_field_t));

    node->type.fields[0u].name = instance->interned_container_id;
    node->type.fields[0u].offset = 0u;
    node->type.fields[0u].size = sizeof (kan_id_32_t);
    node->type.fields[0u].archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT;
    node->type.fields[0u].visibility_condition_field = NULL;
    node->type.fields[0u].visibility_condition_values_count = 0u;
    node->type.fields[0u].visibility_condition_values = NULL;

    node->type.fields[1u].name = instance->interned_stored_resource;
    node->type.fields[1u].offset = (kan_instance_size_t) data_offset;
    node->type.fields[1u].size = type->size;
    node->type.fields[1u].archetype = KAN_REFLECTION_ARCHETYPE_STRUCT;
    node->type.fields[1u].archetype_struct.type_name = type->name;
    node->type.fields[1u].visibility_condition_field = NULL;
    node->type.fields[1u].visibility_condition_values_count = 0u;
    node->type.fields[1u].visibility_condition_values = NULL;

    kan_reflection_system_generation_iterator_add_struct (generation_iterator, &node->type);
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void kan_reflection_generator_universe_resource_provider_iterate (
    struct kan_reflection_generator_universe_resource_provider_t *instance,
    kan_reflection_registry_t registry,
    kan_reflection_system_generation_iterator_t iterator,
    kan_loop_size_t iteration_index)
{
    KAN_UNIVERSE_REFLECTION_GENERATOR_ITERATE_TYPES_WITH_META (
        struct kan_resource_resource_type_meta_t, instance->interned_kan_resource_resource_type_meta_t,
        reflection_generation_iteration_check_type, struct universe_resource_provider_generated_container_type_node_t,
        first_container_type, source_type)
}

static inline void generated_mutator_init_node (
    struct resource_provider_native_container_type_data_t *mutator_node,
    struct universe_resource_provider_generated_container_type_node_t *generated_node)
{
    mutator_node->contained_type_name = generated_node->source_type->name;
    mutator_node->container_type_name = generated_node->type.name;
    mutator_node->contained_type_alignment = generated_node->source_type->alignment;
}

static inline void generated_mutator_deploy_node (kan_repository_t world_repository,
                                                  struct resource_provider_native_container_type_data_t *node)
{
    kan_repository_indexed_storage_t storage =
        kan_repository_indexed_storage_open (world_repository, node->container_type_name);

    const char *container_id_name = "container_id";
    struct kan_repository_field_path_t container_id_path = {
        .reflection_path_length = 1u,
        &container_id_name,
    };

    kan_repository_indexed_insert_query_init (&node->insert_query, storage);
    kan_repository_indexed_value_update_query_init (&node->update_by_id_query, storage, container_id_path);
    kan_repository_indexed_value_delete_query_init (&node->delete_by_id_query, storage, container_id_path);
}

static inline void generated_mutator_undeploy_node (struct resource_provider_native_container_type_data_t *node)
{
    kan_repository_indexed_insert_query_shutdown (&node->insert_query);
    kan_repository_indexed_value_update_query_shutdown (&node->update_by_id_query);
    kan_repository_indexed_value_delete_query_shutdown (&node->delete_by_id_query);
}

static inline void generated_mutator_shutdown_node (struct resource_provider_native_container_type_data_t *node)
{
}

// \c_interface_scanner_disable
KAN_UNIVERSE_REFLECTION_GENERATOR_MUTATOR_FUNCTIONS (generated_mutator,
                                                     struct kan_reflection_generator_universe_resource_provider_t,
                                                     struct universe_resource_provider_generated_container_type_node_t,
                                                     instance->container_types_count,
                                                     instance->first_container_type,
                                                     struct resource_provider_state_t,
                                                     struct resource_provider_native_container_type_data_t,
                                                     resource_provider_state_init,
                                                     mutator_template_deploy_resource_provider,
                                                     mutator_template_execute_resource_provider,
                                                     mutator_template_undeploy_resource_provider,
                                                     resource_provider_state_shutdown)
// \c_interface_scanner_enable

UNIVERSE_RESOURCE_PROVIDER_KAN_API void kan_reflection_generator_universe_resource_provider_finalize (
    struct kan_reflection_generator_universe_resource_provider_t *instance, kan_reflection_registry_t registry)
{
    if (instance->container_types_count > 0u)
    {
#define KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_LESS(first_index, second_index)                              \
    (KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY[first_index]->source_type->name <                         \
     KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY[second_index]->source_type->name)

        KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES (
            instance->container_types_count, struct universe_resource_provider_generated_container_type_node_t,
            instance->first_container_type, instance->generated_reflection_group);
#undef KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_LESS
    }

    KAN_UNIVERSE_REFLECTION_GENERATOR_FILL_MUTATOR (
        instance->mutator, "generated_resource_provider_state_t", resource_provider_state_t,
        resource_provider_native_container_type_data_t, instance->container_types_count, generated_resource_provider,
        generated_mutator);

    kan_reflection_registry_add_struct (registry, &instance->mutator_type);
    kan_reflection_registry_add_function (registry, &instance->mutator_deploy_function);
    kan_reflection_registry_add_function (registry, &instance->mutator_execute_function);
    kan_reflection_registry_add_function_meta (registry, instance->mutator_execute_function.name,
                                               kan_string_intern ("kan_universe_mutator_group_meta_t"),
                                               &resource_provider_group_meta);
    kan_reflection_registry_add_function (registry, &instance->mutator_undeploy_function);
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void kan_reflection_generator_universe_resource_provider_shutdown (
    struct kan_reflection_generator_universe_resource_provider_t *instance)
{
    struct universe_resource_provider_generated_container_type_node_t *node = instance->first_container_type;
    while (node)
    {
        struct universe_resource_provider_generated_container_type_node_t *next = node->next;

        // We do not generate visibility data for containers, therefore we can just deallocate fields.
        kan_free_general (instance->generated_reflection_group, node->type.fields,
                          sizeof (struct kan_reflection_field_t) * node->type.fields_count);

        kan_free_batched (instance->generated_reflection_group, node);
        node = next;
    }

    // We do not generate visibility data for containers, therefore we can just deallocate fields.
    kan_free_general (instance->generated_reflection_group, instance->mutator_type.fields,
                      sizeof (struct kan_reflection_field_t) * instance->mutator_type.fields_count);

    kan_free_general (instance->generated_reflection_group, instance->mutator_deploy_function.arguments,
                      sizeof (struct kan_reflection_argument_t) * instance->mutator_deploy_function.arguments_count);

    kan_free_general (instance->generated_reflection_group, instance->mutator_execute_function.arguments,
                      sizeof (struct kan_reflection_argument_t) * instance->mutator_execute_function.arguments_count);

    kan_free_general (instance->generated_reflection_group, instance->mutator_undeploy_function.arguments,
                      sizeof (struct kan_reflection_argument_t) * instance->mutator_undeploy_function.arguments_count);
}

void kan_resource_request_init (struct kan_resource_request_t *instance)
{
    instance->provided_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
    instance->provided_third_party.data = NULL;
    instance->provided_third_party.size = 0u;
}

void kan_resource_provider_singleton_init (struct kan_resource_provider_singleton_t *instance)
{
    instance->request_id_counter = kan_atomic_int_init (0);
    instance->request_rescan = KAN_FALSE;
    instance->scan_done = KAN_FALSE;
}

void kan_resource_native_entry_init (struct kan_resource_native_entry_t *instance)
{
    instance->my_allocation_group = kan_allocation_group_stack_get ();
}

void kan_resource_native_entry_shutdown (struct kan_resource_native_entry_t *instance)
{
    kan_free_general (instance->my_allocation_group, instance->path, strlen (instance->path) + 1u);
}

void kan_resource_third_party_entry_init (struct kan_resource_third_party_entry_t *instance)
{
    instance->my_allocation_group = kan_allocation_group_stack_get ();
}

void kan_resource_third_party_entry_shutdown (struct kan_resource_third_party_entry_t *instance)
{
    kan_free_general (instance->my_allocation_group, instance->path, strlen (instance->path) + 1u);
}
