#include <universe_resource_provider_kan_api.h>

#include <qsort.h>
#include <stddef.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/context/reflection_system.h>
#include <kan/context/virtual_file_system.h>
#include <kan/log/logging.h>
#include <kan/platform/hardware.h>
#include <kan/platform/precise_time.h>
#include <kan/resource_index/resource_index.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>
#include <kan/virtual_file_system/virtual_file_system.h>

KAN_LOG_DEFINE_CATEGORY (universe_resource_provider);

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

    uint64_t container_id_counter;

    /// \meta reflection_dynamic_array_type = "struct scan_item_task_t"
    struct kan_dynamic_array_t scan_item_stack;

    /// \meta reflection_dynamic_array_type = "kan_serialization_interned_string_registry_t"
    struct kan_dynamic_array_t loaded_string_registries;

    kan_virtual_file_system_watcher_t resource_watcher;
    kan_virtual_file_system_watcher_iterator_t resource_watcher_iterator;
};

struct resource_provider_native_entry_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    enum kan_resource_index_native_item_format_t format;
    char *path;
    kan_serialization_interned_string_registry_t string_registry;

    uint64_t request_count;

    /// \warning Needs manual removal as it is not efficient to bind every container type through meta.
    uint64_t loaded_container_id;

    /// \warning Needs manual removal as it is not efficient to bind every container type through meta.
    uint64_t loading_container_id;

    uint64_t reload_after_real_time_ns;
    kan_allocation_group_t my_allocation_group;
};

struct resource_provider_third_party_entry_t
{
    kan_interned_string_t name;
    uint64_t size;
    char *path;

    uint64_t request_count;
    uint8_t *loaded_data;
    uint8_t *loading_data;

    uint64_t reload_after_real_time_ns;
    kan_allocation_group_t my_allocation_group;
};

struct resource_request_on_insert_event_t
{
    uint64_t request_id;
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
    uint64_t request_id;
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
    uint64_t request_id;
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
    uint64_t offset;
    uint64_t size;
    uint8_t *data;
};

struct resource_provider_loading_operation_t
{
    uint64_t priority;
    kan_interned_string_t target_type;
    kan_interned_string_t target_name;
    struct kan_stream_t *stream;

    union
    {
        struct resource_provider_loading_operation_native_data_t native;
        struct resource_provider_loading_operation_third_party_data_t third_party;
    };
};

struct resource_provider_execution_shared_state_t
{
    struct kan_atomic_int_t workers_left;
    struct kan_atomic_int_t concurrency_lock;
    struct kan_repository_indexed_interval_descending_write_cursor_t loading_operation_cursor;
    uint64_t end_time_ns;
};

struct resource_provider_native_container_type_data_t
{
    kan_interned_string_t contained_type_name;
    kan_interned_string_t container_type_name;
    uint64_t contained_type_alignment;

    struct kan_repository_indexed_insert_query_t insert_query;
    struct kan_repository_indexed_value_update_query_t update_by_id_query;
    struct kan_repository_indexed_value_delete_query_t delete_by_id_query;
};

_Static_assert (_Alignof (struct resource_provider_native_container_type_data_t) == _Alignof (uint64_t),
                "Alignment matches.");

struct resource_provider_state_t
{
    kan_allocation_group_t my_allocation_group;
    uint64_t scan_budget_ns;
    uint64_t load_budget_ns;
    uint64_t modify_wait_time_ns;
    kan_bool_t use_load_only_string_registry;
    kan_bool_t observe_file_system;
    char *resource_directory_path;

    kan_reflection_registry_t reflection_registry;
    kan_serialization_binary_script_storage_t shared_script_storage;
    kan_context_system_handle_t virtual_file_system;
    struct kan_stack_group_allocator_t temporary_allocator;

    struct kan_stream_t *serialized_index_stream;
    kan_serialization_binary_reader_t serialized_index_reader;
    struct kan_serialized_resource_index_t serialized_index_read_buffer;

    struct kan_stream_t *string_registry_stream;
    kan_serialization_interned_string_registry_reader_t string_registry_reader;

    kan_interned_string_t interned_kan_serialized_resource_index_t;
    kan_interned_string_t interned_resource_provider_server;

    struct kan_repository_singleton_write_query_t write__kan_resource_provider_singleton;
    struct kan_repository_singleton_write_query_t write__resource_provider_private_singleton;

    struct kan_repository_indexed_insert_query_t insert__resource_provider_native_entry;
    struct kan_repository_indexed_insert_query_t insert__resource_provider_third_party_entry;
    struct kan_repository_indexed_sequence_write_query_t write_sequence__resource_provider_native_entry;
    struct kan_repository_indexed_sequence_write_query_t write_sequence__resource_provider_third_party_entry;

    struct kan_repository_event_fetch_query_t fetch_resource_request_on_insert_event;
    struct kan_repository_event_fetch_query_t fetch_resource_request_on_change_event;
    struct kan_repository_event_fetch_query_t fetch_resource_request_on_delete_event;

    struct kan_repository_indexed_value_read_query_t read_value__kan_resource_request__name;
    struct kan_repository_indexed_value_update_query_t update_value__kan_resource_request__name;

    struct kan_repository_indexed_value_read_query_t read_value__resource_provider_native_entry__name;
    struct kan_repository_indexed_value_update_query_t update_value__resource_provider_native_entry__name;
    struct kan_repository_indexed_value_update_query_t update_value__resource_provider_third_party_entry__name;
    struct kan_repository_indexed_value_write_query_t write_value__resource_provider_native_entry__name;
    struct kan_repository_indexed_value_write_query_t write_value__resource_provider_third_party_entry__name;

    struct kan_repository_indexed_interval_update_query_t
        update_interval__resource_provider_native_entry__reload_after_real_time_ns;
    struct kan_repository_indexed_interval_update_query_t
        update_interval__resource_provider_third_party_entry__reload_after_real_time_ns;

    struct kan_repository_indexed_insert_query_t insert__resource_provider_loading_operation;
    struct kan_repository_indexed_interval_write_query_t write_interval__resource_provider_loading_operation__priority;
    struct kan_repository_indexed_value_delete_query_t delete_value__resource_provider_loading_operation__target_name;

    struct kan_repository_event_insert_query_t insert__kan_resource_request_updated_event;

    /// \meta reflection_ignore_struct_field
    struct resource_provider_execution_shared_state_t execution_shared_state;

    uint64_t native_container_types_count;

    /// \meta reflection_ignore_struct_field
    uint64_t trailing_data[0u];
};

_Static_assert (_Alignof (struct resource_provider_state_t) ==
                    _Alignof (struct resource_provider_native_container_type_data_t),
                "Alignment matches.");

struct generated_container_type_node_t
{
    struct generated_container_type_node_t *next;
    struct kan_reflection_struct_t type;
    const struct kan_reflection_struct_t *source_type;
};

struct kan_reflection_generator_universe_resource_provider_t
{
    uint64_t boostrap_iteration;
    kan_allocation_group_t generated_reflection_group;
    struct generated_container_type_node_t *first_container_type;
    uint64_t container_types_count;

    struct kan_reflection_struct_t mutator_type;
    struct kan_reflection_function_t mutator_deploy_function;
    struct kan_reflection_function_t mutator_execute_function;
    struct kan_reflection_function_t mutator_undeploy_function;

    kan_interned_string_t interned_kan_resource_provider_type_meta_t;
    kan_interned_string_t interned_container_id;
    kan_interned_string_t interned_stored_resource;
};

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_private_singleton_init (
    struct resource_provider_private_singleton_t *data)
{
    data->status = RESOURCE_PROVIDER_STATUS_NOT_INITIALIZED;
    data->container_id_counter = KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE + 1u;

    kan_dynamic_array_init (&data->scan_item_stack, 0u, sizeof (struct scan_item_task_t),
                            _Alignof (struct scan_item_task_t), kan_allocation_group_stack_get ());

    kan_dynamic_array_init (&data->loaded_string_registries, 0u, sizeof (kan_serialization_interned_string_registry_t),
                            _Alignof (kan_serialization_interned_string_registry_t), kan_allocation_group_stack_get ());

    data->resource_watcher = KAN_INVALID_VIRTUAL_FILE_SYSTEM_WATCHER;
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_private_singleton_shutdown (
    struct resource_provider_private_singleton_t *data)
{
    for (uint64_t index = 0u; index < data->scan_item_stack.size; ++index)
    {
        struct scan_item_task_t *task = &((struct scan_item_task_t *) data->scan_item_stack.data)[index];
        kan_free_general (data->scan_item_stack.allocation_group, task->path, strlen (task->path) + 1u);
    }

    for (uint64_t index = 0u; index < data->loaded_string_registries.size; ++index)
    {
        kan_serialization_interned_string_registry_destroy (
            ((kan_serialization_interned_string_registry_t *) data->loaded_string_registries.data)[index]);
    }

    if (data->resource_watcher != KAN_INVALID_VIRTUAL_FILE_SYSTEM_WATCHER)
    {
        kan_virtual_file_system_watcher_iterator_destroy (data->resource_watcher, data->resource_watcher_iterator);
        kan_virtual_file_system_watcher_destroy (data->resource_watcher);
    }

    kan_dynamic_array_shutdown (&data->scan_item_stack);
    kan_dynamic_array_shutdown (&data->loaded_string_registries);
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_native_entry_init (
    struct resource_provider_native_entry_t *instance)
{
    instance->request_count = 0u;
    instance->loaded_container_id = KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE;
    instance->loading_container_id = KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE;
    instance->reload_after_real_time_ns = UINT64_MAX;
    instance->my_allocation_group = kan_allocation_group_stack_get ();
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_native_entry_shutdown (
    struct resource_provider_native_entry_t *instance)
{
    kan_free_general (instance->my_allocation_group, instance->path, strlen (instance->path) + 1u);
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_third_party_entry_init (
    struct resource_provider_third_party_entry_t *instance)
{
    instance->request_count = 0u;
    instance->loaded_data = NULL;
    instance->loading_data = NULL;
    instance->reload_after_real_time_ns = UINT64_MAX;
    instance->my_allocation_group = kan_allocation_group_stack_get ();
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_third_party_entry_shutdown (
    struct resource_provider_third_party_entry_t *instance)
{
    if (instance->loaded_data)
    {
        kan_free_general (instance->my_allocation_group, instance->loaded_data, instance->size);
    }

    if (instance->loading_data)
    {
        kan_free_general (instance->my_allocation_group, instance->loading_data, instance->size);
    }

    kan_free_general (instance->my_allocation_group, instance->path, strlen (instance->path) + 1u);
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

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_state_init (struct resource_provider_state_t *data)
{
    data->my_allocation_group = kan_allocation_group_stack_get ();
    data->scan_budget_ns = 0u;
    data->load_budget_ns = 0u;
    data->resource_directory_path = NULL;

    kan_stack_group_allocator_init (&data->temporary_allocator,
                                    kan_allocation_group_get_child (data->my_allocation_group, "temporary"),
                                    KAN_UNIVERSE_RESOURCE_PROVIDER_TEMPORARY_CHUNK_SIZE);

    data->interned_kan_serialized_resource_index_t = kan_string_intern ("kan_serialized_resource_index_t");
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
    state->modify_wait_time_ns = configuration->modify_wait_time_ns;
    state->use_load_only_string_registry = configuration->use_load_only_string_registry;
    state->observe_file_system = configuration->observe_file_system;

    KAN_ASSERT (configuration->resource_directory_path)
    const uint64_t length = strlen (configuration->resource_directory_path);
    state->resource_directory_path = kan_allocate_general (state->my_allocation_group, length + 1u, _Alignof (char));
    memcpy (state->resource_directory_path, configuration->resource_directory_path, length + 1u);

    state->reflection_registry = kan_universe_get_reflection_registry (universe);
    state->shared_script_storage = kan_serialization_binary_script_storage_create (state->reflection_registry);
    state->virtual_file_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_VIRTUAL_FILE_SYSTEM_NAME);
    KAN_ASSERT (state->virtual_file_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)

    state->serialized_index_reader = KAN_INVALID_SERIALIZATION_BINARY_READER;
}

static void push_scan_item_task (struct resource_provider_state_t *state,
                                 struct resource_provider_private_singleton_t *private,
                                 const char *path)
{
    void *spot = kan_dynamic_array_add_last (&private->scan_item_stack);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (&private->scan_item_stack, private->scan_item_stack.capacity * 2u);
        spot = kan_dynamic_array_add_last (&private->scan_item_stack);
        KAN_ASSERT (spot)
    }

    struct scan_item_task_t *task = (struct scan_item_task_t *) spot;
    const uint64_t path_length = strlen (path);
    task->path = kan_allocate_general (private->scan_item_stack.allocation_group, path_length + 1u, _Alignof (char));
    memcpy (task->path, path, path_length + 1u);
}

static void destroy_scan_item_task (struct resource_provider_private_singleton_t *private, uint64_t task_index)
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
    push_scan_item_task (state, private, state->resource_directory_path);
    kan_dynamic_array_set_capacity (&private->loaded_string_registries,
                                    KAN_UNIVERSE_RESOURCE_PROVIDER_STRING_REGISTRIES);
}

static void add_native_entry (struct resource_provider_state_t *state,
                              kan_interned_string_t type_name,
                              kan_interned_string_t name,
                              enum kan_resource_index_native_item_format_t format,
                              const char *path,
                              kan_serialization_interned_string_registry_t string_registry)
{
    struct kan_repository_indexed_insertion_package_t package =
        kan_repository_indexed_insert_query_execute (&state->insert__resource_provider_native_entry);
    struct resource_provider_native_entry_t *entry =
        (struct resource_provider_native_entry_t *) kan_repository_indexed_insertion_package_get (&package);

    entry->type = type_name;
    entry->name = name;
    entry->format = format;
    entry->string_registry = string_registry;

    const uint64_t path_length = strlen (path);
    entry->path = kan_allocate_general (entry->my_allocation_group, path_length + 1u, _Alignof (char));
    memcpy (entry->path, path, path_length + 1u);

    kan_repository_indexed_insertion_package_submit (&package);
}

static void add_third_party_entry (struct resource_provider_state_t *state,
                                   kan_interned_string_t name,
                                   uint64_t size,
                                   const char *path)
{
    struct kan_repository_indexed_insertion_package_t package =
        kan_repository_indexed_insert_query_execute (&state->insert__resource_provider_third_party_entry);
    struct resource_provider_third_party_entry_t *entry =
        (struct resource_provider_third_party_entry_t *) kan_repository_indexed_insertion_package_get (&package);

    entry->name = name;
    entry->size = size;

    const uint64_t path_length = strlen (path);
    entry->path = kan_allocate_general (entry->my_allocation_group, path_length + 1u, _Alignof (char));
    memcpy (entry->path, path, path_length + 1u);

    kan_repository_indexed_insertion_package_submit (&package);
}

static inline void unload_native_entry (struct resource_provider_state_t *state,
                                        struct resource_provider_native_entry_t *entry);

static inline void unload_third_party_entry (struct resource_provider_state_t *state,
                                             struct resource_provider_third_party_entry_t *entry);

static void clear_entries (struct resource_provider_state_t *state)
{
    struct kan_repository_indexed_sequence_write_cursor_t cursor =
        kan_repository_indexed_sequence_write_query_execute (&state->write_sequence__resource_provider_native_entry);

    while (KAN_TRUE)
    {
        struct kan_repository_indexed_sequence_write_access_t access =
            kan_repository_indexed_sequence_write_cursor_next (&cursor);

        struct resource_provider_native_entry_t *entry =
            (struct resource_provider_native_entry_t *) kan_repository_indexed_sequence_write_access_resolve (&access);

        if (entry)
        {
            unload_native_entry (state, entry);
            kan_repository_indexed_sequence_write_access_delete (&access);
        }
        else
        {
            kan_repository_indexed_sequence_write_cursor_close (&cursor);
            break;
        }
    }

    cursor = kan_repository_indexed_sequence_write_query_execute (
        &state->write_sequence__resource_provider_third_party_entry);

    while (KAN_TRUE)
    {
        struct kan_repository_indexed_sequence_write_access_t access =
            kan_repository_indexed_sequence_write_cursor_next (&cursor);

        struct resource_provider_third_party_entry_t *entry =
            (struct resource_provider_third_party_entry_t *) kan_repository_indexed_sequence_write_access_resolve (
                &access);

        if (entry)
        {
            unload_third_party_entry (state, entry);
            kan_repository_indexed_sequence_write_access_delete (&access);
        }
        else
        {
            kan_repository_indexed_sequence_write_cursor_close (&cursor);
            break;
        }
    }
}

struct file_scan_result_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
};

static struct file_scan_result_t scan_file (struct resource_provider_state_t *state,
                                            kan_virtual_file_system_volume_t volume,
                                            const char *path,
                                            uint64_t size)
{
    struct file_scan_result_t result = {NULL, NULL};
    const char *path_end = path;

    while (*path_end)
    {
        ++path_end;
    }

    const uint64_t path_length = path_end - path;
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
                    KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY))
            {
                const char *last_slash = strrchr (path, '/');
                const char *name_begin = last_slash ? last_slash + 1u : path;
                const char *name_end = path_end - 4u;

                result.type = type_name;
                result.name = kan_char_sequence_intern (name_begin, name_end);

                add_native_entry (state, type_name, result.name, KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY, path,
                                  KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY);
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

                add_native_entry (state, type_name, result.name, KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA,
                                  path, KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY);
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
        add_third_party_entry (state, result.name, size, path);
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
    const uint64_t directory_path_length = path_container.length;

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
        }

        return KAN_FALSE;
    }

    kan_file_system_path_container_reset_length (&path_container, directory_path_length);
    struct kan_virtual_file_system_directory_iterator_t iterator =
        kan_virtual_file_system_directory_iterator_create (volume, path_container.path);
    const char *entry_name;

    while ((entry_name = kan_virtual_file_system_directory_iterator_advance (&iterator)))
    {
        kan_file_system_path_container_append (&path_container, entry_name);
        push_scan_item_task (state, private, path_container.path);
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
            scan_file (state, volume, path, status.size);
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
                                        kan_serialization_interned_string_registry_t string_registry)
{
    for (uint64_t type_index = 0u; type_index < state->serialized_index_read_buffer.native.size; ++type_index)
    {
        struct kan_serialized_resource_index_native_container_t *container =
            &((struct kan_serialized_resource_index_native_container_t *)
                  state->serialized_index_read_buffer.native.data)[type_index];

        for (uint64_t item_index = 0u; item_index < container->items.size; ++item_index)
        {
            struct kan_serialized_resource_index_native_item_t *item =
                &((struct kan_serialized_resource_index_native_item_t *) container->items.data)[item_index];

            add_native_entry (state, container->type, item->name, item->format, item->path, string_registry);
        }
    }

    for (uint64_t item_index = 0u; item_index < state->serialized_index_read_buffer.third_party.size; ++item_index)
    {
        struct kan_serialized_resource_index_third_party_item_t *item =
            &((struct kan_serialized_resource_index_third_party_item_t *)
                  state->serialized_index_read_buffer.third_party.data)[item_index];

        add_third_party_entry (state, item->name, item->size, item->path);
    }
}

static inline const struct resource_provider_native_entry_t *read_native_entry (
    struct resource_provider_state_t *state,
    kan_interned_string_t type,
    kan_interned_string_t name,
    struct kan_repository_indexed_value_read_access_t *access_output)
{
    struct kan_repository_indexed_value_read_cursor_t cursor = kan_repository_indexed_value_read_query_execute (
        &state->read_value__resource_provider_native_entry__name, &name);

    while (KAN_TRUE)
    {
        struct kan_repository_indexed_value_read_access_t access =
            kan_repository_indexed_value_read_cursor_next (&cursor);

        const struct resource_provider_native_entry_t *entry =
            kan_repository_indexed_value_read_access_resolve (&access);
        if (!entry)
        {
            kan_repository_indexed_value_read_cursor_close (&cursor);
            return NULL;
        }

        if (entry->type == type)
        {
            kan_repository_indexed_value_read_cursor_close (&cursor);
            *access_output = access;
            return entry;
        }
    }

    return NULL;
}

static inline struct resource_provider_native_entry_t *update_native_entry (
    struct resource_provider_state_t *state,
    kan_interned_string_t type,
    kan_interned_string_t name,
    struct kan_repository_indexed_value_update_access_t *access_output)
{
    struct kan_repository_indexed_value_update_cursor_t cursor = kan_repository_indexed_value_update_query_execute (
        &state->update_value__resource_provider_native_entry__name, &name);

    while (KAN_TRUE)
    {
        struct kan_repository_indexed_value_update_access_t access =
            kan_repository_indexed_value_update_cursor_next (&cursor);

        struct resource_provider_native_entry_t *entry = kan_repository_indexed_value_update_access_resolve (&access);
        if (!entry)
        {
            kan_repository_indexed_value_update_cursor_close (&cursor);
            return NULL;
        }

        if (entry->type == type)
        {
            kan_repository_indexed_value_update_cursor_close (&cursor);
            *access_output = access;
            return entry;
        }
    }

    return NULL;
}

static inline struct resource_provider_native_container_type_data_t *query_container_type_data (
    struct resource_provider_state_t *state, kan_interned_string_t type)
{
    uint64_t left = 0u;
    uint64_t right = state->native_container_types_count;
    struct resource_provider_native_container_type_data_t *types =
        (struct resource_provider_native_container_type_data_t *) state->trailing_data;

    while (left < right)
    {
        uint64_t middle = (left + right) / 2u;

        if (type < types[middle].contained_type_name)
        {
            right = middle;
        }
        else if (type > types[middle].contained_type_name)
        {
            left = middle + 1u;
        }
        else
        {
            return &types[middle];
        }
    }

    return NULL;
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
                 "\"kan_resource_provider_type_meta_t\" meta attached.",
                 type)
        return NULL;
    }

    *package_output = kan_repository_indexed_insert_query_execute (&data->insert_query);
    struct kan_resource_container_view_t *view =
        (struct kan_resource_container_view_t *) kan_repository_indexed_insertion_package_get (package_output);

    *data_begin_output =
        ((uint8_t *) view) + kan_apply_alignment (offsetof (struct kan_resource_container_view_t, data_begin),
                                                  data->contained_type_alignment);

    return view;
}

static inline struct kan_resource_container_view_t *native_container_update (
    struct resource_provider_state_t *state,
    kan_interned_string_t type,
    uint64_t container_id,
    struct kan_repository_indexed_value_update_access_t *access_output,
    uint8_t **data_begin_output)
{
    struct resource_provider_native_container_type_data_t *data = query_container_type_data (state, type);
    if (!data)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Unable to find container type for resource type \"%s\". Resources types should have "
                 "\"kan_resource_provider_type_meta_t\" meta attached.",
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
                                            uint64_t container_id)
{
    struct resource_provider_native_container_type_data_t *data = query_container_type_data (state, type);
    if (!data)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Unable to find container type for resource type \"%s\". Resources types should have "
                 "\"kan_resource_provider_type_meta_t\" meta attached.",
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

static inline uint64_t gather_request_priority (struct resource_provider_state_t *state,
                                                kan_interned_string_t type,
                                                kan_interned_string_t name)
{
    uint64_t priority = 0u;
    struct kan_repository_indexed_value_read_cursor_t request_cursor =
        kan_repository_indexed_value_read_query_execute (&state->read_value__kan_resource_request__name, &name);

    while (KAN_TRUE)
    {
        struct kan_repository_indexed_value_read_access_t access =
            kan_repository_indexed_value_read_cursor_next (&request_cursor);

        struct kan_resource_request_t *request =
            (struct kan_resource_request_t *) kan_repository_indexed_value_read_access_resolve (&access);

        if (request)
        {
            if (request->type == type)
            {
                priority = KAN_MAX (priority, request->priority);
            }

            kan_repository_indexed_value_read_access_close (&access);
        }
        else
        {
            kan_repository_indexed_value_read_cursor_close (&request_cursor);
            break;
        }
    }

    return priority;
}

static inline void update_requests (struct resource_provider_state_t *state,
                                    kan_interned_string_t type,
                                    kan_interned_string_t name,
                                    uint64_t container_id,
                                    void *third_party_data)
{
    struct kan_repository_indexed_value_update_cursor_t request_cursor =
        kan_repository_indexed_value_update_query_execute (&state->update_value__kan_resource_request__name, &name);

    while (KAN_TRUE)
    {
        struct kan_repository_indexed_value_update_access_t access =
            kan_repository_indexed_value_update_cursor_next (&request_cursor);

        struct kan_resource_request_t *request =
            (struct kan_resource_request_t *) kan_repository_indexed_value_update_access_resolve (&access);

        if (request)
        {
            if (request->type == type)
            {
                kan_bool_t changed = KAN_FALSE;
                if (type)
                {
                    if (request->provided_container_id != container_id)
                    {
                        request->provided_container_id = container_id;
                        changed = KAN_TRUE;
                    }
                }
                else
                {
                    if (request->provided_third_party_data != third_party_data)
                    {
                        request->provided_third_party_data = third_party_data;
                        changed = KAN_TRUE;
                    }
                }

                if (changed)
                {
                    struct kan_repository_event_insertion_package_t package =
                        kan_repository_event_insert_query_execute (&state->insert__kan_resource_request_updated_event);

                    struct kan_resource_request_updated_event_t *event =
                        (struct kan_resource_request_updated_event_t *) kan_repository_event_insertion_package_get (
                            &package);

                    event->type = type;
                    event->request_id = request->request_id;
                    kan_repository_event_insertion_package_submit (&package);
                }
            }

            kan_repository_indexed_value_update_access_close (&access);
        }
        else
        {
            kan_repository_indexed_value_update_cursor_close (&request_cursor);
            break;
        }
    }
}

static inline void loading_operation_cancel (struct resource_provider_state_t *state,
                                             kan_interned_string_t type,
                                             kan_interned_string_t name)
{
    struct kan_repository_indexed_value_delete_cursor_t cursor = kan_repository_indexed_value_delete_query_execute (
        &state->delete_value__resource_provider_loading_operation__target_name, &name);

    while (KAN_TRUE)
    {
        struct kan_repository_indexed_value_delete_access_t access =
            kan_repository_indexed_value_delete_cursor_next (&cursor);

        const struct resource_provider_loading_operation_t *operation =
            (const struct resource_provider_loading_operation_t *) kan_repository_indexed_value_delete_access_resolve (
                &access);

        if (operation)
        {
            if (operation->target_type == type)
            {
                kan_repository_indexed_value_delete_access_delete (&access);
            }
            else
            {
                kan_repository_indexed_value_delete_access_close (&access);
            }
        }
        else
        {
            kan_repository_indexed_value_delete_cursor_close (&cursor);
            break;
        }
    }
}

static inline void schedule_native_entry_loading (struct resource_provider_state_t *state,
                                                  struct resource_provider_private_singleton_t *private,
                                                  struct resource_provider_native_entry_t *entry)
{
    if (entry->loading_container_id != KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)
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

    entry->loading_container_id = container_view->container_id;
    struct kan_repository_indexed_insertion_package_t package =
        kan_repository_indexed_insert_query_execute (&state->insert__resource_provider_loading_operation);

    struct resource_provider_loading_operation_t *operation =
        (struct resource_provider_loading_operation_t *) kan_repository_indexed_insertion_package_get (&package);

    operation->priority = gather_request_priority (state, entry->type, entry->name);
    operation->target_type = entry->type;
    operation->target_name = entry->name;
    operation->stream = stream;
    operation->native.format_cache = entry->format;

    switch (entry->format)
    {
    case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY:
        operation->native.binary_reader =
            kan_serialization_binary_reader_create (stream, data_begin, entry->type, state->shared_script_storage,
                                                    entry->string_registry, container_view->my_allocation_group);
        break;

    case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA:
        operation->native.readable_data_reader = kan_serialization_rd_reader_create (
            stream, data_begin, entry->type, state->shared_script_storage, container_view->my_allocation_group);
        break;
    }

    kan_repository_indexed_insertion_package_submit (&package);
    kan_repository_indexed_insertion_package_submit (&container_package);
}

static inline void unload_native_entry (struct resource_provider_state_t *state,
                                        struct resource_provider_native_entry_t *entry)
{
    if (entry->loaded_container_id == KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)
    {
        return;
    }

    update_requests (state, entry->type, entry->name, KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE, NULL);
    native_container_delete (state, entry->type, entry->loaded_container_id);
    entry->loaded_container_id = KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE;
}

static inline void cancel_native_entry_loading (struct resource_provider_state_t *state,
                                                struct resource_provider_native_entry_t *entry)
{
    if (entry->loading_container_id == KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)
    {
        return;
    }

    loading_operation_cancel (state, entry->type, entry->name);
    native_container_delete (state, entry->type, entry->loading_container_id);
    entry->loading_container_id = KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE;
}

static inline struct resource_provider_third_party_entry_t *update_third_party_entry (
    struct resource_provider_state_t *state,
    kan_interned_string_t name,
    struct kan_repository_indexed_value_update_access_t *access_output)
{
    struct kan_repository_indexed_value_update_cursor_t cursor = kan_repository_indexed_value_update_query_execute (
        &state->update_value__resource_provider_third_party_entry__name, &name);
    *access_output = kan_repository_indexed_value_update_cursor_next (&cursor);

#if defined(KAN_WITH_ASSERT)
    // Check that there is no duplicate third party names.
    struct kan_repository_indexed_value_update_access_t next_access =
        kan_repository_indexed_value_update_cursor_next (&cursor);

    if (kan_repository_indexed_value_update_access_resolve (&next_access))
    {
        KAN_ASSERT (KAN_FALSE)
        kan_repository_indexed_value_update_access_close (&next_access);
    }
#endif

    kan_repository_indexed_value_update_cursor_close (&cursor);
    return (struct resource_provider_third_party_entry_t *) kan_repository_indexed_value_update_access_resolve (
        access_output);
}

static inline void schedule_third_party_entry_loading (struct resource_provider_state_t *state,
                                                       struct resource_provider_third_party_entry_t *entry)
{
    if (entry->loading_data)
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

    entry->loading_data = kan_allocate_general (entry->my_allocation_group, entry->size, _Alignof (uint64_t));
    struct kan_repository_indexed_insertion_package_t package =
        kan_repository_indexed_insert_query_execute (&state->insert__resource_provider_loading_operation);

    struct resource_provider_loading_operation_t *operation =
        (struct resource_provider_loading_operation_t *) kan_repository_indexed_insertion_package_get (&package);

    operation->priority = gather_request_priority (state, NULL, entry->name);
    operation->target_type = NULL;
    operation->target_name = entry->name;
    operation->stream = stream;
    operation->third_party.offset = 0u;
    operation->third_party.size = entry->size;
    operation->third_party.data = entry->loading_data;
    kan_repository_indexed_insertion_package_submit (&package);
}

static inline void unload_third_party_entry (struct resource_provider_state_t *state,
                                             struct resource_provider_third_party_entry_t *entry)
{
    if (!entry->loaded_data)
    {
        return;
    }

    update_requests (state, NULL, entry->name, KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE, NULL);
    kan_free_general (entry->my_allocation_group, entry->loaded_data, entry->size);
    entry->loaded_data = NULL;
}

static inline void cancel_third_party_entry_loading (struct resource_provider_state_t *state,
                                                     struct resource_provider_third_party_entry_t *entry)
{
    if (!entry->loading_data)
    {
        return;
    }

    loading_operation_cancel (state, NULL, entry->name);
    kan_free_general (entry->my_allocation_group, entry->loading_data, entry->size);
    entry->loading_data = NULL;
}

static inline void add_native_entry_reference (struct resource_provider_state_t *state,
                                               struct resource_provider_private_singleton_t *private,
                                               kan_interned_string_t type,
                                               kan_interned_string_t name)
{
    struct kan_repository_indexed_value_update_access_t entry_access;
    struct resource_provider_native_entry_t *entry = update_native_entry (state, type, name, &entry_access);

    if (entry)
    {
        ++entry->request_count;
        if (entry->loaded_container_id == KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE &&
            entry->loading_container_id == KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)
        {
            schedule_native_entry_loading (state, private, entry);
        }

        kan_repository_indexed_value_update_access_close (&entry_access);
    }
    else
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Unable to find requested native resource \"%s\" of type \"%s\".", name, type)
    }
}

static inline void add_third_party_entry_reference (struct resource_provider_state_t *state, kan_interned_string_t name)
{
    struct kan_repository_indexed_value_update_access_t entry_access;
    struct resource_provider_third_party_entry_t *entry = update_third_party_entry (state, name, &entry_access);

    if (entry)
    {
        ++entry->request_count;
        if (entry->loaded_data == KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE &&
            entry->loading_data == KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)
        {
            schedule_third_party_entry_loading (state, entry);
        }

        kan_repository_indexed_value_update_access_close (&entry_access);
    }
    else
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR, "Unable to find requested third party resource \"%s\".",
                 name)
    }
}

static inline void remove_native_entry_reference (struct resource_provider_state_t *state,
                                                  kan_interned_string_t type,
                                                  kan_interned_string_t name)
{
    struct kan_repository_indexed_value_update_access_t entry_access;
    struct resource_provider_native_entry_t *entry = update_native_entry (state, type, name, &entry_access);

    if (entry)
    {
        KAN_ASSERT (entry->request_count > 0u)
        --entry->request_count;

        if (entry->request_count == 0u)
        {
            unload_native_entry (state, entry);
            cancel_native_entry_loading (state, entry);
        }

        kan_repository_indexed_value_update_access_close (&entry_access);
    }
    else
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Unable to find requested native resource \"%s\" of type \"%s\".", name, type)
    }
}

static inline void remove_third_party_entry_reference (struct resource_provider_state_t *state,
                                                       kan_interned_string_t name)
{
    struct kan_repository_indexed_value_update_access_t entry_access;
    struct resource_provider_third_party_entry_t *entry = update_third_party_entry (state, name, &entry_access);

    if (entry)
    {
        KAN_ASSERT (entry->request_count > 0u)
        --entry->request_count;

        if (entry->request_count == 0u)
        {
            unload_third_party_entry (state, entry);
            cancel_third_party_entry_loading (state, entry);
        }

        kan_repository_indexed_value_update_access_close (&entry_access);
    }
    else
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR, "Unable to find requested third party resource \"%s\".",
                 name)
    }
}

static inline void on_file_added (struct resource_provider_state_t *state,
                                  struct resource_provider_private_singleton_t *private,
                                  kan_virtual_file_system_volume_t volume,
                                  const char *path)
{
    struct kan_virtual_file_system_entry_status_t status;
    if (!kan_virtual_file_system_query_entry (volume, path, &status))
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Failed to react to \"%s\" addition, unable to query status.", path)
        return;
    }

    KAN_ASSERT (status.type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
    struct file_scan_result_t scan_result = scan_file (state, volume, path, status.size);

    if (!scan_result.name)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR, "Failed to react to \"%s\" addition, scan failed.", path)
        return;
    }

    uint64_t request_count = 0u;
    struct kan_repository_indexed_value_read_cursor_t cursor = kan_repository_indexed_value_read_query_execute (
        &state->read_value__kan_resource_request__name, &scan_result.name);

    while (KAN_TRUE)
    {
        struct kan_repository_indexed_value_read_access_t access =
            kan_repository_indexed_value_read_cursor_next (&cursor);

        struct kan_resource_request_t *request =
            (struct kan_resource_request_t *) kan_repository_indexed_value_read_access_resolve (&access);

        if (request)
        {
            if (request->type == scan_result.type)
            {
                ++request_count;
            }

            kan_repository_indexed_value_read_access_close (&access);
        }
        else
        {
            kan_repository_indexed_value_read_cursor_close (&cursor);
            break;
        }
    }

    if (request_count > 0u)
    {
        if (scan_result.type)
        {
            struct kan_repository_indexed_value_update_access_t entry_access;
            struct resource_provider_native_entry_t *entry =
                update_native_entry (state, scan_result.type, scan_result.name, &entry_access);
            KAN_ASSERT (entry)

            entry->request_count = request_count;
            KAN_ASSERT (entry->loaded_container_id == KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE &&
                        entry->loading_container_id == KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)

            schedule_native_entry_loading (state, private, entry);
            kan_repository_indexed_value_update_access_close (&entry_access);
        }
        else
        {
            struct kan_repository_indexed_value_update_access_t entry_access;
            struct resource_provider_third_party_entry_t *entry =
                update_third_party_entry (state, scan_result.name, &entry_access);
            KAN_ASSERT (entry)

            entry->request_count = request_count;
            KAN_ASSERT (entry->loaded_data == KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE &&
                        entry->loading_data == KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)

            schedule_third_party_entry_loading (state, entry);
            kan_repository_indexed_value_update_access_close (&entry_access);
        }
    }
}

static inline kan_interned_string_t extract_name_from_path (const char *path)
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

    const uint64_t path_length = path_end - path;
    if (path_length > 4u && *(path_end - 4u) == '.' && *(path_end - 3u) == 'b' && *(path_end - 2u) == 'i' &&
        *(path_end - 1u) == 'n')
    {
        name_end = path_end - 4u;
    }
    else if (path_length > 3u && *(path_end - 3u) == '.' && *(path_end - 2u) == 'r' && *(path_end - 1u) == 'd')
    {
        name_end = path_end - 3u;
    }

    return kan_char_sequence_intern (name_begin, name_end);
}

static inline struct resource_provider_native_entry_t *write_native_entry_by_path (
    struct resource_provider_state_t *state,
    kan_interned_string_t name,
    const char *path,
    struct kan_repository_indexed_value_write_access_t *access_output)
{
    struct kan_repository_indexed_value_write_cursor_t cursor = kan_repository_indexed_value_write_query_execute (
        &state->write_value__resource_provider_native_entry__name, &name);

    while (KAN_TRUE)
    {
        struct kan_repository_indexed_value_write_access_t access =
            kan_repository_indexed_value_write_cursor_next (&cursor);

        struct resource_provider_native_entry_t *entry = kan_repository_indexed_value_write_access_resolve (&access);
        if (!entry)
        {
            kan_repository_indexed_value_write_cursor_close (&cursor);
            return NULL;
        }

        if (strcmp (entry->path, path) == 0)
        {
            kan_repository_indexed_value_write_cursor_close (&cursor);
            *access_output = access;
            return entry;
        }
    }

    return NULL;
}

static inline struct resource_provider_third_party_entry_t *write_third_party_entry_by_path (
    struct resource_provider_state_t *state,
    kan_interned_string_t name,
    const char *path,
    struct kan_repository_indexed_value_write_access_t *access_output)
{
    struct kan_repository_indexed_value_write_cursor_t cursor = kan_repository_indexed_value_write_query_execute (
        &state->write_value__resource_provider_third_party_entry__name, &name);

    while (KAN_TRUE)
    {
        struct kan_repository_indexed_value_write_access_t access =
            kan_repository_indexed_value_write_cursor_next (&cursor);

        struct resource_provider_third_party_entry_t *entry =
            kan_repository_indexed_value_write_access_resolve (&access);
        if (!entry)
        {
            kan_repository_indexed_value_write_cursor_close (&cursor);
            return NULL;
        }

        if (strcmp (entry->path, path) == 0)
        {
            kan_repository_indexed_value_write_cursor_close (&cursor);
            *access_output = access;
            return entry;
        }
    }

    return NULL;
}

static inline void on_file_modified (struct resource_provider_state_t *state,
                                     kan_virtual_file_system_volume_t volume,
                                     const char *path)
{
    kan_interned_string_t name = extract_name_from_path (path);
    {
        struct kan_repository_indexed_value_write_access_t access;
        struct resource_provider_native_entry_t *entry = write_native_entry_by_path (state, name, path, &access);

        if (entry)
        {
            if (entry->request_count > 0u)
            {
                entry->reload_after_real_time_ns = kan_platform_get_elapsed_nanoseconds () + state->modify_wait_time_ns;
            }

            kan_repository_indexed_value_write_access_close (&access);
            return;
        }
    }

    {
        struct kan_repository_indexed_value_write_access_t access;
        struct resource_provider_third_party_entry_t *entry =
            write_third_party_entry_by_path (state, name, path, &access);

        if (entry)
        {
            if (entry->request_count > 0u)
            {
                entry->reload_after_real_time_ns = kan_platform_get_elapsed_nanoseconds () + state->modify_wait_time_ns;
            }

            kan_repository_indexed_value_write_access_close (&access);
            return;
        }
    }
}

static inline void on_file_removed (struct resource_provider_state_t *state,
                                    kan_virtual_file_system_volume_t volume,
                                    const char *path)
{
    kan_interned_string_t name = extract_name_from_path (path);
    {
        struct kan_repository_indexed_value_write_access_t access;
        struct resource_provider_native_entry_t *entry = write_native_entry_by_path (state, name, path, &access);

        if (entry)
        {
            unload_native_entry (state, entry);
            cancel_native_entry_loading (state, entry);
            kan_repository_indexed_value_write_access_delete (&access);
            return;
        }
    }

    {
        struct kan_repository_indexed_value_write_access_t access;
        struct resource_provider_third_party_entry_t *entry =
            write_third_party_entry_by_path (state, name, path, &access);

        if (entry)
        {
            unload_third_party_entry (state, entry);
            cancel_third_party_entry_loading (state, entry);
            kan_repository_indexed_value_write_access_delete (&access);
            return;
        }
    }
}

static inline void process_request_on_insert (struct resource_provider_state_t *state,
                                              struct resource_provider_private_singleton_t *private)
{
    while (KAN_TRUE)
    {
        struct kan_repository_event_read_access_t access =
            kan_repository_event_fetch_query_next (&state->fetch_resource_request_on_insert_event);

        struct resource_request_on_insert_event_t *event =
            (struct resource_request_on_insert_event_t *) kan_repository_event_read_access_resolve (&access);

        if (event)
        {
            if (event->type)
            {
                add_native_entry_reference (state, private, event->type, event->name);
            }
            else
            {
                add_third_party_entry_reference (state, event->name);
            }

            kan_repository_event_read_access_close (&access);
        }
        else
        {
            break;
        }
    }
}

static inline void process_request_on_change (struct resource_provider_state_t *state,
                                              struct resource_provider_private_singleton_t *private)
{
    while (KAN_TRUE)
    {
        struct kan_repository_event_read_access_t access =
            kan_repository_event_fetch_query_next (&state->fetch_resource_request_on_change_event);

        struct resource_request_on_change_event_t *event =
            (struct resource_request_on_change_event_t *) kan_repository_event_read_access_resolve (&access);

        if (event)
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
                add_native_entry_reference (state, private, event->new_type, event->new_name);
            }
            else
            {
                add_third_party_entry_reference (state, event->new_name);
            }

            kan_repository_event_read_access_close (&access);
        }
        else
        {
            break;
        }
    }
}

static inline void process_request_on_delete (struct resource_provider_state_t *state)
{
    while (KAN_TRUE)
    {
        struct kan_repository_event_read_access_t access =
            kan_repository_event_fetch_query_next (&state->fetch_resource_request_on_delete_event);

        struct resource_request_on_delete_event_t *event =
            (struct resource_request_on_delete_event_t *) kan_repository_event_read_access_resolve (&access);

        if (event)
        {
            if (event->type)
            {
                remove_native_entry_reference (state, event->type, event->name);
            }
            else
            {
                remove_third_party_entry_reference (state, event->name);
            }

            kan_repository_event_read_access_close (&access);
        }
        else
        {
            break;
        }
    }
}

static inline void process_delayed_reload (struct resource_provider_state_t *state,
                                           struct resource_provider_private_singleton_t *private)
{
    const uint64_t current_real_time = kan_platform_get_elapsed_nanoseconds ();
    struct kan_repository_indexed_interval_ascending_update_cursor_t cursor =
        kan_repository_indexed_interval_update_query_execute_ascending (
            &state->update_interval__resource_provider_native_entry__reload_after_real_time_ns, NULL,
            &current_real_time);

    while (KAN_TRUE)
    {
        struct kan_repository_indexed_interval_update_access_t access =
            kan_repository_indexed_interval_ascending_update_cursor_next (&cursor);

        struct resource_provider_native_entry_t *entry =
            kan_repository_indexed_interval_update_access_resolve (&access);

        if (entry)
        {
            if (entry->request_count > 0u)
            {
                cancel_native_entry_loading (state, entry);
                schedule_native_entry_loading (state, private, entry);
            }

            entry->reload_after_real_time_ns = UINT64_MAX;
            kan_repository_indexed_interval_update_access_close (&access);
        }
        else
        {
            kan_repository_indexed_interval_ascending_update_cursor_close (&cursor);
            break;
        }
    }

    cursor = kan_repository_indexed_interval_update_query_execute_ascending (
        &state->update_interval__resource_provider_third_party_entry__reload_after_real_time_ns, NULL,
        &current_real_time);

    while (KAN_TRUE)
    {
        struct kan_repository_indexed_interval_update_access_t access =
            kan_repository_indexed_interval_ascending_update_cursor_next (&cursor);

        struct resource_provider_third_party_entry_t *entry =
            kan_repository_indexed_interval_update_access_resolve (&access);

        if (entry)
        {
            if (entry->request_count > 0u)
            {
                cancel_third_party_entry_loading (state, entry);
                schedule_third_party_entry_loading (state, entry);
            }

            entry->reload_after_real_time_ns = UINT64_MAX;
            kan_repository_indexed_interval_update_access_close (&access);
        }
        else
        {
            kan_repository_indexed_interval_ascending_update_cursor_close (&cursor);
            break;
        }
    }
}

static void execute_shared_serve (uint64_t user_data)
{
    struct resource_provider_state_t *state = (struct resource_provider_state_t *) user_data;
    while (KAN_TRUE)
    {
        if (kan_platform_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
        {
            // Shutdown: no more time.
            if (kan_atomic_int_add (&state->execution_shared_state.workers_left, -1) == 1)
            {
                kan_repository_indexed_interval_descending_write_cursor_close (
                    &state->execution_shared_state.loading_operation_cursor);
            }

            return;
        }

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

        // Restart loading for native type if reflection has changed.
        if (loading_operation->target_type && loading_operation->native.used_registry != state->reflection_registry)
        {
            resource_provider_loading_operation_destroy_native (loading_operation);
            loading_operation->stream->operations->close (loading_operation->stream);

            struct kan_repository_indexed_value_read_access_t entry_access;
            const struct resource_provider_native_entry_t *entry = read_native_entry (
                state, loading_operation->target_type, loading_operation->target_name, &entry_access);

            if (!entry)
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                         "Unable to restart loading of \"%s\" of type \"%s\" as native entry no longer exists. "
                         "Internal handling error?",
                         loading_operation->target_name, loading_operation->target_type)

                kan_repository_indexed_interval_write_access_delete (&loading_operation_access);
                continue;
            }

            kan_virtual_file_system_volume_t volume =
                kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);

            loading_operation->stream = kan_virtual_file_stream_open_for_read (volume, entry->path);
            kan_virtual_file_system_close_context_read_access (state->virtual_file_system);

            if (!loading_operation->stream)
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                         "Unable to restart loading of \"%s\" of type \"%s\" as stream can no longer be opened. "
                         "Internal handling error?",
                         loading_operation->target_name, loading_operation->target_type)

                kan_repository_indexed_value_read_access_close (&entry_access);
                kan_repository_indexed_interval_write_access_delete (&loading_operation_access);
                continue;
            }

            struct kan_repository_indexed_value_update_access_t container_view_access;
            uint8_t *container_data_begin;
            struct kan_resource_container_view_t *container_view = native_container_update (
                state, entry->type, entry->loading_container_id, &container_view_access, &container_data_begin);

            if (!container_view)
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                         "Unable to restart loading of \"%s\" of type \"%s\" as loading container is absent. "
                         "Internal handling error?",
                         loading_operation->target_name, loading_operation->target_type)

                kan_repository_indexed_value_read_access_close (&entry_access);
                kan_repository_indexed_interval_write_access_delete (&loading_operation_access);
                continue;
            }

            switch (entry->format)
            {
            case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY:
                loading_operation->native.binary_reader = kan_serialization_binary_reader_create (
                    loading_operation->stream, container_data_begin, entry->type, state->shared_script_storage,
                    entry->string_registry, container_view->my_allocation_group);
                break;

            case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA:
                loading_operation->native.readable_data_reader = kan_serialization_rd_reader_create (
                    loading_operation->stream, container_data_begin, entry->type, state->shared_script_storage,
                    container_view->my_allocation_group);
                break;
            }

            kan_repository_indexed_value_update_access_close (&container_view_access);
            kan_repository_indexed_value_read_access_close (&entry_access);
        }

        // Process serialization.
        enum kan_serialization_state_t serialization_state;

        if (loading_operation->target_type)
        {
            switch (loading_operation->native.format_cache)
            {
            case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY:
            {
                while ((serialization_state = kan_serialization_binary_reader_step (
                            loading_operation->native.binary_reader)) == KAN_SERIALIZATION_IN_PROGRESS)
                {
                    if (kan_platform_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
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
                    if (kan_platform_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
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
                uint64_t to_read =
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

                if (kan_platform_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
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

                struct kan_repository_indexed_value_update_access_t entry_access;
                struct resource_provider_native_entry_t *entry = update_native_entry (
                    state, loading_operation->target_type, loading_operation->target_name, &entry_access);

                if (entry)
                {
                    native_container_delete (state, entry->type, entry->loaded_container_id);
                    entry->loaded_container_id = entry->loading_container_id;
                    entry->loading_container_id = KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE;
                    update_requests (state, entry->type, entry->name, entry->loaded_container_id, NULL);
                    kan_repository_indexed_value_update_access_close (&entry_access);
                }
            }
            else
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_DEBUG, "Loaded third party resource \"%s\".",
                         loading_operation->target_name)

                struct kan_repository_indexed_value_update_access_t entry_access;
                struct resource_provider_third_party_entry_t *entry =
                    update_third_party_entry (state, loading_operation->target_name, &entry_access);

                if (entry)
                {
                    kan_free_general (entry->my_allocation_group, entry->loaded_data, entry->size);
                    entry->loaded_data = entry->loading_data;
                    entry->loading_data = NULL;
                    update_requests (state, NULL, entry->name, KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE,
                                     entry->loaded_data);
                    kan_repository_indexed_value_update_access_close (&entry_access);
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

                struct kan_repository_indexed_value_update_access_t entry_access;
                struct resource_provider_native_entry_t *entry = update_native_entry (
                    state, loading_operation->target_type, loading_operation->target_name, &entry_access);

                if (entry)
                {
                    native_container_delete (state, entry->type, entry->loading_container_id);
                    entry->loading_container_id = KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE;
                    kan_repository_indexed_value_update_access_close (&entry_access);
                }
            }
            else
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                         "Failed to load third party resource \"%s\": serialization error.",
                         loading_operation->target_name)

                struct kan_repository_indexed_value_update_access_t entry_access;
                struct resource_provider_third_party_entry_t *entry =
                    update_third_party_entry (state, loading_operation->target_name, &entry_access);

                if (entry)
                {
                    kan_free_general (entry->my_allocation_group, entry->loading_data, entry->size);
                    entry->loading_data = NULL;
                    kan_repository_indexed_value_update_access_close (&entry_access);
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
    const uint64_t begin_time = kan_platform_get_elapsed_nanoseconds ();

    kan_repository_singleton_write_access_t public_access =
        kan_repository_singleton_write_query_execute (&state->write__kan_resource_provider_singleton);
    struct kan_resource_provider_singleton_t *public =
        (struct kan_resource_provider_singleton_t *) kan_repository_singleton_write_access_resolve (public_access);

    kan_repository_singleton_write_access_t private_access =
        kan_repository_singleton_write_query_execute (&state->write__resource_provider_private_singleton);
    struct resource_provider_private_singleton_t *private =
        (struct resource_provider_private_singleton_t *) kan_repository_singleton_write_access_resolve (private_access);

    if (private->status == RESOURCE_PROVIDER_STATUS_NOT_INITIALIZED)
    {
        prepare_for_scanning (state, private);
        private->status = RESOURCE_PROVIDER_STATUS_SCANNING;
    }

    if (private->status == RESOURCE_PROVIDER_STATUS_SERVING && public->request_rescan)
    {
        clear_entries (state);
        for (uint64_t index = 0u; index < private->loaded_string_registries.size; ++index)
        {
            kan_serialization_interned_string_registry_destroy (
                ((kan_serialization_interned_string_registry_t *) private->loaded_string_registries.data)[index]);
        }

        kan_dynamic_array_reset (&private->loaded_string_registries);
        if (private->resource_watcher != KAN_INVALID_VIRTUAL_FILE_SYSTEM_WATCHER)
        {
            kan_virtual_file_system_watcher_iterator_destroy (private->resource_watcher,
                                                              private->resource_watcher_iterator);
            kan_virtual_file_system_watcher_destroy (private->resource_watcher);
        }

        prepare_for_scanning (state, private);
        private->status = RESOURCE_PROVIDER_STATUS_SCANNING;
        public->request_rescan = KAN_FALSE;
    }

    if (private->status == RESOURCE_PROVIDER_STATUS_SCANNING &&
        begin_time + state->scan_budget_ns < kan_platform_get_elapsed_nanoseconds ())
    {
        kan_virtual_file_system_volume_t volume =
            kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);

        while (private->scan_item_stack.size > 0u &&
               begin_time + state->scan_budget_ns < kan_platform_get_elapsed_nanoseconds ())
        {
            if (state->string_registry_stream)
            {
                if (state->string_registry_reader == KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY_READER)
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
                    state->string_registry_reader = KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY_READER;

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
                if (state->serialized_index_reader == KAN_INVALID_SERIALIZATION_BINARY_READER)
                {
                    kan_serialization_interned_string_registry_t registry =
                        state->string_registry_reader == KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY_READER ?
                            KAN_INVALID_REFLECTION_REGISTRY :
                            kan_serialization_interned_string_registry_reader_get (state->string_registry_reader);

                    kan_serialized_resource_index_init (&state->serialized_index_read_buffer);
                    state->serialized_index_reader = kan_serialization_binary_reader_create (
                        state->serialized_index_stream, &state->serialized_index_read_buffer,
                        state->interned_kan_serialized_resource_index_t, state->shared_script_storage, registry,
                        kan_serialized_resource_index_get_string_allocation_group ());
                }

                switch (kan_serialization_binary_reader_step (state->serialized_index_reader))
                {
                case KAN_SERIALIZATION_IN_PROGRESS:
                    break;

                case KAN_SERIALIZATION_FINISHED:
                {
                    kan_serialization_interned_string_registry_t string_registry =
                        KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY;

                    if (state->string_registry_reader != KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY_READER)
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
                        state->string_registry_reader = KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY_READER;
                    }

                    instantiate_resource_index (state, string_registry);
                    kan_serialized_resource_index_shutdown (&state->serialized_index_read_buffer);
                    kan_serialization_binary_reader_destroy (state->serialized_index_reader);
                    state->serialized_index_reader = KAN_INVALID_SERIALIZATION_BINARY_READER;

                    state->serialized_index_stream->operations->close (state->serialized_index_stream);
                    state->serialized_index_stream = NULL;

                    // Destroy last scan task that issued this read procedure.
                    destroy_scan_item_task (private, private->scan_item_stack.size - 1u);
                    break;
                }

                case KAN_SERIALIZATION_FAILED:
                    KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                             "Failed to deserialize string registry, its directory will be skipped.")

                    if (state->string_registry_reader != KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY_READER)
                    {
                        kan_serialization_interned_string_registry_destroy (
                            kan_serialization_interned_string_registry_reader_get (state->string_registry_reader));
                        kan_serialization_interned_string_registry_reader_destroy (state->string_registry_reader);
                        state->string_registry_reader = KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY_READER;
                    }

                    kan_serialized_resource_index_shutdown (&state->serialized_index_read_buffer);
                    kan_serialization_binary_reader_destroy (state->serialized_index_reader);
                    state->serialized_index_reader = KAN_INVALID_SERIALIZATION_BINARY_READER;

                    state->serialized_index_stream->operations->close (state->serialized_index_stream);
                    state->serialized_index_stream = NULL;

                    // Destroy last scan task that issued this read procedure.
                    destroy_scan_item_task (private, private->scan_item_stack.size - 1u);
                    break;
                }

                continue;
            }

            uint64_t task_index = private->scan_item_stack.size - 1u;
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
            kan_dynamic_array_set_capacity (&private->loaded_string_registries, private->loaded_string_registries.size);

            if (state->observe_file_system)
            {
                volume = kan_virtual_file_system_get_context_volume_for_write (state->virtual_file_system);
                private->resource_watcher =
                    kan_virtual_file_system_watcher_create (volume, state->resource_directory_path);
                private->resource_watcher_iterator =
                    kan_virtual_file_system_watcher_iterator_create (private->resource_watcher);
                kan_virtual_file_system_close_context_write_access (state->virtual_file_system);
            }

            private->status = RESOURCE_PROVIDER_STATUS_SERVING;
        }
    }

    if (private->status == RESOURCE_PROVIDER_STATUS_SERVING &&
        begin_time + state->load_budget_ns < kan_platform_get_elapsed_nanoseconds ())
    {
        // Events need to be always processed in order to keep everything up to date.
        // Therefore, load budget does not affect event processing.

        if (private->resource_watcher != KAN_INVALID_VIRTUAL_FILE_SYSTEM_WATCHER)
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
                        on_file_added (state, private, volume, event->path_container.path);
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
        process_delayed_reload (state, private);

        kan_stack_group_allocator_reset (&state->temporary_allocator);
        const uint64_t cpu_count = kan_platform_get_cpu_count ();
        struct kan_cpu_task_list_node_t *task_list_node = NULL;

        state->execution_shared_state.workers_left = kan_atomic_int_init ((int) cpu_count);
        state->execution_shared_state.concurrency_lock = kan_atomic_int_init (0);
        state->execution_shared_state.loading_operation_cursor =
            kan_repository_indexed_interval_write_query_execute_descending (
                &state->write_interval__resource_provider_loading_operation__priority, NULL, NULL);
        state->execution_shared_state.end_time_ns = begin_time + state->load_budget_ns;

        for (uint64_t worker_index = 0u; worker_index < cpu_count; ++worker_index)
        {
            KAN_CPU_TASK_LIST_USER_VALUE (&task_list_node, &state->temporary_allocator,
                                          state->interned_resource_provider_server, execute_shared_serve, FOREGROUND,
                                          state)
        }

        kan_cpu_job_dispatch_and_detach_task_list (job, task_list_node);
    }

    kan_repository_singleton_write_access_close (public_access);
    kan_repository_singleton_write_access_close (private_access);
    kan_cpu_job_release (job);
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void mutator_template_undeploy_resource_provider (
    struct resource_provider_state_t *state)
{
    if (state->resource_directory_path)
    {
        kan_free_general (state->my_allocation_group, state->resource_directory_path,
                          strlen (state->resource_directory_path) + 1u);
    }

    kan_serialization_binary_script_storage_destroy (state->shared_script_storage);
    if (state->serialized_index_stream)
    {
        if (state->serialized_index_reader != KAN_INVALID_SERIALIZATION_BINARY_READER)
        {
            kan_serialized_resource_index_shutdown (&state->serialized_index_read_buffer);
            state->serialized_index_reader = KAN_INVALID_SERIALIZATION_BINARY_READER;
        }

        state->serialized_index_stream->operations->close (state->serialized_index_stream);
        state->serialized_index_stream = NULL;
    }

    if (state->string_registry_stream)
    {
        if (state->string_registry_reader != KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY_READER)
        {
            kan_serialization_interned_string_registry_reader_destroy (state->string_registry_reader);
            state->string_registry_reader = KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY_READER;
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

    instance->interned_kan_resource_provider_type_meta_t = kan_string_intern ("kan_resource_provider_type_meta_t");
    instance->interned_container_id = kan_string_intern ("container_id");
    instance->interned_stored_resource = kan_string_intern ("stored_resource");
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void kan_reflection_generator_universe_resource_provider_boostrap (
    struct kan_reflection_generator_universe_resource_provider_t *instance, uint64_t bootstrap_iteration)
{
    instance->boostrap_iteration = bootstrap_iteration;
}

static void generated_container_init (uint64_t function_user_data, void *data)
{
    ((struct kan_resource_container_view_t *) data)->container_id = KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE;
    ((struct kan_resource_container_view_t *) data)->my_allocation_group = kan_allocation_group_stack_get ();
    const struct kan_reflection_struct_t *boxed_type = (const struct kan_reflection_struct_t *) function_user_data;

    if (boxed_type->init)
    {
        const uint64_t offset =
            kan_apply_alignment (offsetof (struct kan_resource_container_view_t, data_begin), boxed_type->alignment);
        uint8_t *address = (uint8_t *) data + offset;
        boxed_type->init (boxed_type->functor_user_data, address);
    }
}

static void generated_container_shutdown (uint64_t function_user_data, void *data)
{
    const struct kan_reflection_struct_t *boxed_type = (const struct kan_reflection_struct_t *) function_user_data;
    if (boxed_type->shutdown)
    {
        const uint64_t offset =
            kan_apply_alignment (offsetof (struct kan_resource_container_view_t, data_begin), boxed_type->alignment);
        uint8_t *address = (uint8_t *) data + offset;
        boxed_type->shutdown (boxed_type->functor_user_data, address);
    }
}

static void reflection_generation_iteration_check_type (
    struct kan_reflection_generator_universe_resource_provider_t *instance,
    kan_reflection_registry_t registry,
    const struct kan_reflection_struct_t *type,
    kan_reflection_system_generation_iterator_t generation_iterator)
{
    struct kan_reflection_struct_meta_iterator_t meta_iterator = kan_reflection_registry_query_struct_meta (
        registry, type->name, instance->interned_kan_resource_provider_type_meta_t);

    if (!kan_reflection_struct_meta_iterator_get (&meta_iterator))
    {
        // Not marked as resource type.
        return;
    }

    struct generated_container_type_node_t *node = (struct generated_container_type_node_t *) kan_allocate_batched (
        instance->generated_reflection_group, sizeof (struct generated_container_type_node_t));

    node->source_type = type;
    node->next = instance->first_container_type;
    instance->first_container_type = node;
    ++instance->container_types_count;

#define MAX_GENERATED_NAME_LENGTH 256u
    char buffer[MAX_GENERATED_NAME_LENGTH];
    snprintf (buffer, MAX_GENERATED_NAME_LENGTH, KAN_RESOURCE_PROVIDER_CONTAINER_TYPE_PREFIX "%s", type->name);
    node->type.name = kan_string_intern (buffer);
#undef MAX_GENERATED_NAME_LENGTH

    const uint64_t container_alignment = KAN_MAX (_Alignof (uint64_t), type->alignment);
    const uint64_t data_offset = kan_apply_alignment (sizeof (uint64_t), container_alignment);
    node->type.alignment = container_alignment;
    node->type.size = kan_apply_alignment (data_offset + type->size, container_alignment);

    node->type.functor_user_data = (uint64_t) type;
    node->type.init = generated_container_init;
    node->type.shutdown = generated_container_shutdown;

    node->type.fields_count = 2u;
    node->type.fields =
        kan_allocate_general (instance->generated_reflection_group, sizeof (struct kan_reflection_field_t) * 2u,
                              _Alignof (struct kan_reflection_field_t));

    node->type.fields[0u].name = instance->interned_container_id;
    node->type.fields[0u].offset = 0u;
    node->type.fields[0u].size = sizeof (uint64_t);
    node->type.fields[0u].archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT;
    node->type.fields[0u].visibility_condition_field = NULL;
    node->type.fields[0u].visibility_condition_values_count = 0u;
    node->type.fields[0u].visibility_condition_values = NULL;

    node->type.fields[1u].name = instance->interned_stored_resource;
    node->type.fields[1u].offset = data_offset;
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
    uint64_t iteration_index)
{
    if (iteration_index == instance->boostrap_iteration)
    {
        kan_reflection_registry_struct_iterator_t struct_iterator =
            kan_reflection_registry_struct_iterator_create (registry);
        const struct kan_reflection_struct_t *type;

        while ((type = kan_reflection_registry_struct_iterator_get (struct_iterator)))
        {
            reflection_generation_iteration_check_type (instance, registry, type, iterator);
            struct_iterator = kan_reflection_registry_struct_iterator_next (struct_iterator);
        }
    }
    else
    {
        kan_interned_string_t type_name;
        while ((type_name = kan_reflection_system_generation_iterator_next_added_struct (iterator)))
        {
            const struct kan_reflection_struct_t *type = kan_reflection_registry_query_struct (registry, type_name);
            if (type)
            {
                reflection_generation_iteration_check_type (instance, registry, type, iterator);
            }
        }

        struct kan_reflection_system_added_struct_meta_t added_meta;
        while ((added_meta = kan_reflection_system_generation_iterator_next_added_struct_meta (iterator)).struct_name)
        {
            if (added_meta.meta_type_name == instance->interned_kan_resource_provider_type_meta_t)
            {
                kan_bool_t already_added = KAN_FALSE;
                // Not the most effective search, but should be okay enough here.
                // We can use hash storage for faster search, but it seems like an overkill,
                // because it is only needed here.
                struct generated_container_type_node_t *node = instance->first_container_type;

                while (node)
                {
                    if (node->source_type->name == added_meta.struct_name)
                    {
                        already_added = KAN_TRUE;
                        break;
                    }

                    node = node->next;
                }

                if (!already_added)
                {
                    const struct kan_reflection_struct_t *type =
                        kan_reflection_registry_query_struct (registry, added_meta.struct_name);

                    if (type)
                    {
                        reflection_generation_iteration_check_type (instance, registry, type, iterator);
                    }
                }
            }
        }
    }
}

static void generated_mutator_init (uint64_t function_user_data, void *data)
{
    struct kan_reflection_generator_universe_resource_provider_t *instance =
        (struct kan_reflection_generator_universe_resource_provider_t *) function_user_data;

    struct resource_provider_state_t *base_state = (struct resource_provider_state_t *) data;
    resource_provider_state_init (base_state);
    base_state->native_container_types_count = instance->container_types_count;

    struct generated_container_type_node_t *node = instance->first_container_type;
    struct resource_provider_native_container_type_data_t *mutator_node =
        (struct resource_provider_native_container_type_data_t *) base_state->trailing_data;

    while (node)
    {
        mutator_node->contained_type_name = node->source_type->name;
        mutator_node->container_type_name = node->type.name;
        mutator_node->contained_type_alignment = node->source_type->alignment;

        node = node->next;
        ++mutator_node;
    }
}

static void generated_mutator_deploy (kan_reflection_functor_user_data_t user_data,
                                      void *return_address,
                                      void *arguments_address)
{
    struct
    {
        kan_universe_t universe;
        kan_universe_world_t world;
        kan_repository_t world_repository;
        kan_workflow_graph_node_t workflow_node;
        struct resource_provider_state_t *state;
    } *arguments = arguments_address;

    mutator_template_deploy_resource_provider (arguments->universe, arguments->world, arguments->world_repository,
                                               arguments->workflow_node, arguments->state);
    struct resource_provider_native_container_type_data_t *mutator_nodes =
        (struct resource_provider_native_container_type_data_t *) arguments->state->trailing_data;

    for (uint64_t index = 0u; index < arguments->state->native_container_types_count; ++index)
    {
        struct resource_provider_native_container_type_data_t *node = &mutator_nodes[index];
        kan_repository_indexed_storage_t storage =
            kan_repository_indexed_storage_open (arguments->world_repository, node->container_type_name);

        const char *container_id_name = "container_id";
        struct kan_repository_field_path_t container_id_path = {
            .reflection_path_length = 1u,
            &container_id_name,
        };

        kan_repository_indexed_insert_query_init (&node->insert_query, storage);
        kan_repository_indexed_value_update_query_init (&node->update_by_id_query, storage, container_id_path);
        kan_repository_indexed_value_delete_query_init (&node->delete_by_id_query, storage, container_id_path);
    }
}

static void generated_mutator_execute (kan_reflection_functor_user_data_t user_data,
                                       void *return_address,
                                       void *arguments_address)
{
    struct
    {
        kan_cpu_job_t job;
        struct resource_provider_state_t *state;
    } *arguments = arguments_address;

    mutator_template_execute_resource_provider (arguments->job, arguments->state);
}

static void generated_mutator_undeploy (kan_reflection_functor_user_data_t user_data,
                                        void *return_address,
                                        void *arguments_address)
{
    struct
    {
        struct resource_provider_state_t *state;
    } *arguments = arguments_address;

    mutator_template_undeploy_resource_provider (arguments->state);
    struct resource_provider_native_container_type_data_t *mutator_nodes =
        (struct resource_provider_native_container_type_data_t *) arguments->state->trailing_data;

    for (uint64_t index = 0u; index < arguments->state->native_container_types_count; ++index)
    {
        kan_repository_indexed_insert_query_shutdown (&mutator_nodes[index].insert_query);
        kan_repository_indexed_value_update_query_shutdown (&mutator_nodes[index].update_by_id_query);
        kan_repository_indexed_value_delete_query_shutdown (&mutator_nodes[index].delete_by_id_query);
    }
}

static void generated_mutator_shutdown (uint64_t function_user_data, void *data)
{
    struct resource_provider_state_t *base_state = (struct resource_provider_state_t *) data;
    resource_provider_state_shutdown (base_state);
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void kan_reflection_generator_universe_resource_provider_finalize (
    struct kan_reflection_generator_universe_resource_provider_t *instance, kan_reflection_registry_t registry)
{
    if (instance->container_types_count > 0u)
    {
        struct generated_container_type_node_t **nodes_array_to_sort = kan_allocate_general (
            instance->generated_reflection_group, sizeof (void *) * instance->container_types_count, _Alignof (void *));

        struct generated_container_type_node_t *node = instance->first_container_type;
        uint64_t output_index = 0u;

        while (node)
        {
            nodes_array_to_sort[output_index] = node;
            ++output_index;
            node = node->next;
        }

        {
#define LESS(first_index, second_index)                                                                                \
    (nodes_array_to_sort[first_index]->source_type->name < nodes_array_to_sort[second_index]->source_type->name)

#define SWAP(first_index, second_index)                                                                                \
    node = nodes_array_to_sort[first_index], nodes_array_to_sort[first_index] = nodes_array_to_sort[second_index],     \
    nodes_array_to_sort[second_index] = node

            QSORT ((unsigned long) instance->container_types_count, LESS, SWAP);
#undef LESS
#undef SWAP
        }

        for (uint64_t node_index = 0u; node_index < instance->container_types_count; ++node_index)
        {
            if (node_index + 1u < instance->container_types_count)
            {
                nodes_array_to_sort[node_index]->next = nodes_array_to_sort[node_index + 1u];
            }
            else
            {
                nodes_array_to_sort[node_index]->next = NULL;
            }
        }

        instance->first_container_type = nodes_array_to_sort[0u];
        kan_free_general (instance->generated_reflection_group, nodes_array_to_sort,
                          sizeof (void *) * instance->container_types_count);
    }

    instance->mutator_type.name = kan_string_intern ("generated_resource_provider_state_t");
    instance->mutator_type.alignment = _Alignof (struct resource_provider_state_t);
    instance->mutator_type.size =
        sizeof (struct resource_provider_state_t) +
        sizeof (struct resource_provider_native_container_type_data_t) * instance->container_types_count;

    instance->mutator_type.functor_user_data = (uint64_t) instance;
    instance->mutator_type.init = generated_mutator_init;
    instance->mutator_type.shutdown = generated_mutator_shutdown;

    instance->mutator_type.fields_count = 2u;
    instance->mutator_type.fields =
        kan_allocate_general (instance->generated_reflection_group, sizeof (struct kan_reflection_field_t) * 2u,
                              _Alignof (struct kan_reflection_field_t));

    instance->mutator_type.fields[0u].name = kan_string_intern ("base_mutator_state");
    instance->mutator_type.fields[0u].offset = 0u;
    instance->mutator_type.fields[0u].size = sizeof (struct resource_provider_state_t);
    instance->mutator_type.fields[0u].archetype = KAN_REFLECTION_ARCHETYPE_STRUCT;
    instance->mutator_type.fields[0u].archetype_struct.type_name = kan_string_intern ("resource_provider_state_t");
    instance->mutator_type.fields[0u].visibility_condition_field = NULL;
    instance->mutator_type.fields[0u].visibility_condition_values_count = 0u;
    instance->mutator_type.fields[0u].visibility_condition_values = NULL;

    instance->mutator_type.fields[1u].name = kan_string_intern ("container_types");
    instance->mutator_type.fields[1u].offset = sizeof (struct resource_provider_state_t);
    instance->mutator_type.fields[1u].size =
        sizeof (struct resource_provider_native_container_type_data_t) * instance->container_types_count;
    instance->mutator_type.fields[1u].archetype = KAN_REFLECTION_ARCHETYPE_INLINE_ARRAY;
    instance->mutator_type.fields[1u].archetype_inline_array.item_count = instance->container_types_count;
    instance->mutator_type.fields[1u].archetype_inline_array.item_size =
        sizeof (struct resource_provider_native_container_type_data_t);
    instance->mutator_type.fields[1u].archetype_inline_array.size_field = NULL;
    instance->mutator_type.fields[1u].archetype_inline_array.item_archetype = KAN_REFLECTION_ARCHETYPE_STRUCT;
    instance->mutator_type.fields[1u].archetype_inline_array.item_archetype_struct.type_name =
        kan_string_intern ("resource_provider_native_container_type_data_t");
    instance->mutator_type.fields[1u].visibility_condition_field = NULL;
    instance->mutator_type.fields[1u].visibility_condition_values_count = 0u;
    instance->mutator_type.fields[1u].visibility_condition_values = NULL;

    instance->mutator_deploy_function.name =
        kan_string_intern ("kan_universe_mutator_deploy_generated_resource_provider");
    instance->mutator_deploy_function.call = generated_mutator_deploy;
    instance->mutator_deploy_function.call_user_data = 0u;

    instance->mutator_deploy_function.return_type.size = 0u;
    instance->mutator_deploy_function.return_type.archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT;

    instance->mutator_deploy_function.arguments_count = 5u;
    instance->mutator_deploy_function.arguments =
        kan_allocate_general (instance->generated_reflection_group, sizeof (struct kan_reflection_argument_t) * 5u,
                              _Alignof (struct kan_reflection_argument_t));

    instance->mutator_deploy_function.arguments[0u].name = kan_string_intern ("universe");
    instance->mutator_deploy_function.arguments[0u].size = sizeof (kan_universe_t);
    instance->mutator_deploy_function.arguments[0u].archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT;

    instance->mutator_deploy_function.arguments[1u].name = kan_string_intern ("world");
    instance->mutator_deploy_function.arguments[1u].size = sizeof (kan_universe_world_t);
    instance->mutator_deploy_function.arguments[1u].archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT;

    instance->mutator_deploy_function.arguments[2u].name = kan_string_intern ("world_repository");
    instance->mutator_deploy_function.arguments[2u].size = sizeof (kan_repository_t);
    instance->mutator_deploy_function.arguments[2u].archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT;

    instance->mutator_deploy_function.arguments[3u].name = kan_string_intern ("workflow_node");
    instance->mutator_deploy_function.arguments[3u].size = sizeof (kan_workflow_graph_node_t);
    instance->mutator_deploy_function.arguments[3u].archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT;

    instance->mutator_deploy_function.arguments[4u].name = kan_string_intern ("state");
    instance->mutator_deploy_function.arguments[4u].size = sizeof (void *);
    instance->mutator_deploy_function.arguments[4u].archetype = KAN_REFLECTION_ARCHETYPE_STRING_POINTER;
    instance->mutator_deploy_function.arguments[4u].archetype_struct_pointer.type_name = instance->mutator_type.name;

    instance->mutator_execute_function.name =
        kan_string_intern ("kan_universe_mutator_execute_generated_resource_provider");
    instance->mutator_execute_function.call = generated_mutator_execute;
    instance->mutator_execute_function.call_user_data = 0u;

    instance->mutator_execute_function.return_type.size = 0u;
    instance->mutator_execute_function.return_type.archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT;

    instance->mutator_execute_function.arguments_count = 2u;
    instance->mutator_execute_function.arguments =
        kan_allocate_general (instance->generated_reflection_group, sizeof (struct kan_reflection_argument_t) * 2u,
                              _Alignof (struct kan_reflection_argument_t));

    instance->mutator_execute_function.arguments[0u].name = kan_string_intern ("job");
    instance->mutator_execute_function.arguments[0u].size = sizeof (kan_cpu_job_t);
    instance->mutator_execute_function.arguments[0u].archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT;

    instance->mutator_execute_function.arguments[1u].name = kan_string_intern ("state");
    instance->mutator_execute_function.arguments[1u].size = sizeof (void *);
    instance->mutator_execute_function.arguments[1u].archetype = KAN_REFLECTION_ARCHETYPE_STRING_POINTER;
    instance->mutator_execute_function.arguments[1u].archetype_struct_pointer.type_name = instance->mutator_type.name;

    instance->mutator_undeploy_function.name =
        kan_string_intern ("kan_universe_mutator_undeploy_generated_resource_provider");
    instance->mutator_undeploy_function.call = generated_mutator_undeploy;
    instance->mutator_undeploy_function.call_user_data = 0u;

    instance->mutator_undeploy_function.return_type.size = 0u;
    instance->mutator_undeploy_function.return_type.archetype = KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT;

    instance->mutator_undeploy_function.arguments_count = 1u;
    instance->mutator_undeploy_function.arguments =
        kan_allocate_general (instance->generated_reflection_group, sizeof (struct kan_reflection_argument_t) * 1u,
                              _Alignof (struct kan_reflection_argument_t));

    instance->mutator_undeploy_function.arguments[0u].name = kan_string_intern ("state");
    instance->mutator_undeploy_function.arguments[0u].size = sizeof (void *);
    instance->mutator_undeploy_function.arguments[0u].archetype = KAN_REFLECTION_ARCHETYPE_STRING_POINTER;
    instance->mutator_undeploy_function.arguments[0u].archetype_struct_pointer.type_name = instance->mutator_type.name;

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
    struct generated_container_type_node_t *node = instance->first_container_type;
    while (node)
    {
        struct generated_container_type_node_t *next = node->next;

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
    instance->provided_container_id = KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE;
}

void kan_resource_provider_singleton_init (struct kan_resource_provider_singleton_t *instance)
{
    instance->request_id_counter = kan_atomic_int_init (0);
    instance->request_rescan = KAN_FALSE;
}
