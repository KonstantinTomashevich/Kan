#include <universe_resource_provider_kan_api.h>

#include <qsort.h>
#include <stddef.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/context/all_system_names.h>
#include <kan/context/hot_reload_coordination_system.h>
#include <kan/context/reflection_system.h>
#include <kan/context/resource_pipeline_system.h>
#include <kan/context/virtual_file_system.h>
#include <kan/log/logging.h>
#include <kan/platform/hardware.h>
#include <kan/precise_time/precise_time.h>
#include <kan/reflection/struct_helpers.h>
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

    union
    {
        KAN_REFLECTION_IGNORE
        struct kan_atomic_int_t container_id_counter;

        int container_id_counter_data_for_reflection;
    };

    union
    {
        KAN_REFLECTION_IGNORE
        struct kan_atomic_int_t byproduct_id_counter;

        int byproduct_id_counter_data_for_reflection;
    };

    kan_id_32_t attachment_id_counter;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct scan_item_task_t)
    struct kan_dynamic_array_t scan_item_stack;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_serialization_interned_string_registry_t)
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

enum resource_provider_compilation_state_t
{
    RESOURCE_PROVIDER_COMPILATION_STATE_NOT_PENDING = 0u,
    RESOURCE_PROVIDER_COMPILATION_STATE_PENDING,
    RESOURCE_PROVIDER_COMPILATION_STATE_WAITING_FOR_SOURCE,
    RESOURCE_PROVIDER_COMPILATION_STATE_WAITING_FOR_DEPENDENCIES,
    RESOURCE_PROVIDER_COMPILATION_STATE_COMPILING,
    RESOURCE_PROVIDER_COMPILATION_STATE_DONE,
};

struct resource_provider_compiled_resource_entry_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    kan_interned_string_t source_type;

    /// \details Including internal requests for subsequent compilation of other resources.
    kan_instance_size_t request_count;

    kan_resource_container_id_t compiled_container_id;
    enum resource_provider_compilation_state_t compilation_state;
    kan_resource_container_id_t pending_container_id;

    kan_instance_size_t current_compilation_index;
    kan_instance_size_t pending_compilation_index;

    kan_resource_request_id_t source_resource_request_id;
    kan_resource_container_id_t last_used_source_container_id;
};

struct resource_provider_compilation_dependency_t
{
    kan_interned_string_t compiled_type;
    kan_interned_string_t compiled_name;
    kan_interned_string_t dependency_type;
    kan_interned_string_t dependency_name;
    kan_resource_request_id_t request_id;
};

struct resource_provider_raw_byproduct_entry_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    kan_hash_t hash;

    /// \details Including internal requests for subsequent compilation of other resources and
    ///          references from compiled data of objects that produced these byproducts.
    kan_instance_size_t request_count;

    kan_resource_container_id_t container_id;
};

struct resource_provider_byproduct_production_t
{
    kan_interned_string_t compiled_type;
    kan_interned_string_t compiled_name;
    kan_interned_string_t byproduct_type;
    kan_interned_string_t byproduct_name;
    kan_instance_size_t compilation_index;
};

struct resource_request_on_insert_event_t
{
    kan_resource_request_id_t request_id;
    kan_interned_string_t type;
    kan_interned_string_t name;
};

KAN_REFLECTION_STRUCT_META (kan_resource_request_t)
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
    kan_bool_t was_sleeping;
};

KAN_REFLECTION_STRUCT_META (kan_resource_request_t)
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
    .changed_copy_outs_count = 4u,
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
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"sleeping"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"was_sleeping"}},
            },
        },
};

struct resource_request_on_delete_event_t
{
    kan_resource_request_id_t request_id;
    kan_interned_string_t type;
    kan_interned_string_t name;
    kan_bool_t was_sleeping;
};

KAN_REFLECTION_STRUCT_META (kan_resource_request_t)
UNIVERSE_RESOURCE_PROVIDER_KAN_API struct kan_repository_meta_automatic_on_delete_event_t resource_request_on_delete = {
    .event_type = "resource_request_on_delete_event_t",
    .copy_outs_count = 4u,
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
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"sleeping"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"was_sleeping"}},
            },
        },
};

enum resource_provider_operation_type_t
{
    RESOURCE_PROVIDER_OPERATION_TYPE_LOAD = 0u,
    RESOURCE_PROVIDER_OPERATION_TYPE_COMPILE,
};

struct resource_provider_operation_load_native_data_t
{
    kan_reflection_registry_t used_registry;
    enum kan_resource_index_native_item_format_t format_cache;

    union
    {
        kan_serialization_binary_reader_t binary_reader;
        kan_serialization_rd_reader_t readable_data_reader;
    };
};

struct resource_provider_operation_load_third_party_data_t
{
    kan_memory_size_t offset;
    kan_memory_size_t size;
    uint8_t *data;
};

struct resource_provider_operation_load_t
{
    struct kan_stream_t *stream;
    union
    {
        struct resource_provider_operation_load_native_data_t native;
        struct resource_provider_operation_load_third_party_data_t third_party;
    };
};

struct resource_provider_operation_compile_t
{
    kan_interned_string_t state_type_name;
    kan_resource_container_id_t state_container_id;
};

/// \brief First internal priority.
#define PRIORITY_INTERNAL_BEGIN KAN_RESOURCE_PROVIDER_USER_PRIORITY_MAX + 1u

/// \brief Requests since that priority indicate serve-all-at-once frame which is used for special occasions.
#define PRIORITY_SERVE_ALL_AT_ONCE_SINCE PRIORITY_INTERNAL_BEGIN

/// \brief Priority for hot swap frame resources.
#define PRIORITY_HOT_SWAP PRIORITY_SERVE_ALL_AT_ONCE_SINCE

struct resource_provider_operation_t
{
    kan_instance_size_t priority;
    kan_interned_string_t target_type;
    kan_interned_string_t target_name;
    enum resource_provider_operation_type_t operation_type;

    union
    {
        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (operation_type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (RESOURCE_PROVIDER_OPERATION_TYPE_LOAD)
        struct resource_provider_operation_load_t load;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (operation_type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (RESOURCE_PROVIDER_OPERATION_TYPE_COMPILE)
        struct resource_provider_operation_compile_t compile;
    };
};

enum resource_provider_serve_operation_status_t
{
    RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_IN_PROGRESS,
    RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_DONE,
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
    KAN_REFLECTION_IGNORE
    struct kan_atomic_int_t workers_left;

    KAN_REFLECTION_IGNORE
    struct kan_atomic_int_t concurrency_lock;

    /// \brief Special lock for producing byproducts during runtime compilation.
    KAN_REFLECTION_IGNORE
    struct kan_atomic_int_t byproduct_lock;

    KAN_REFLECTION_IGNORE
    struct kan_repository_indexed_interval_descending_write_cursor_t operation_cursor;

    kan_instance_size_t min_priority;
    kan_time_size_t end_time_ns;
    kan_cpu_job_t job;
};

enum resource_provider_native_container_type_source_t
{
    RESOURCE_PROVIDER_NATIVE_CONTAINER_TYPE_SOURCE_RESOURCE_TYPE,
    RESOURCE_PROVIDER_NATIVE_CONTAINER_TYPE_SOURCE_BYPRODUCT_TYPE,
    RESOURCE_PROVIDER_NATIVE_CONTAINER_TYPE_SOURCE_COMPILATION_STATE,
};

struct resource_provider_native_container_type_data_t
{
    kan_interned_string_t contained_type_name;
    kan_interned_string_t container_type_name;
    kan_memory_size_t contained_type_alignment;

    struct kan_repository_indexed_insert_query_t insert_query;
    struct kan_repository_indexed_value_read_query_t read_by_id_query;
    struct kan_repository_indexed_value_update_query_t update_by_id_query;
    struct kan_repository_indexed_value_delete_query_t delete_by_id_query;

    enum resource_provider_native_container_type_source_t source;
    kan_interned_string_t compiled_from;
};

_Static_assert (_Alignof (struct resource_provider_native_container_type_data_t) == _Alignof (void *),
                "Alignment matches.");

struct resource_provider_state_t
{
    kan_allocation_group_t my_allocation_group;
    kan_time_offset_t scan_budget_ns;
    kan_time_offset_t serve_budget_ns;
    kan_bool_t use_load_only_string_registry;
    kan_bool_t enable_runtime_compilation;
    kan_interned_string_t resource_directory_path;

    kan_reflection_registry_t reflection_registry;
    kan_serialization_binary_script_storage_t shared_script_storage;
    kan_context_system_t hot_reload_system;
    kan_context_system_t resource_pipeline_system;
    kan_resource_pipeline_system_platform_configuration_listener platform_configuration_change_listener;
    kan_context_system_t virtual_file_system;

    KAN_REFLECTION_IGNORE
    struct kan_stack_group_allocator_t temporary_allocator;

    KAN_REFLECTION_IGNORE
    struct kan_stream_t *serialized_index_stream;

    KAN_REFLECTION_IGNORE
    kan_serialization_binary_reader_t serialized_index_reader;

    KAN_REFLECTION_IGNORE
    struct kan_resource_index_t serialized_index_read_buffer;

    KAN_REFLECTION_IGNORE
    struct kan_stream_t *string_registry_stream;

    KAN_REFLECTION_IGNORE
    kan_serialization_interned_string_registry_reader_t string_registry_reader;

    kan_interned_string_t interned_kan_resource_index_t;
    kan_interned_string_t interned_resource_provider_server;
    kan_interned_string_t interned_kan_resource_compilable_meta_t;
    kan_interned_string_t interned_kan_resource_byproduct_type_meta_t;

    KAN_UP_GENERATE_STATE_QUERIES (resource_provider)
    KAN_UP_BIND_STATE (resource_provider, state)

    struct kan_repository_event_fetch_query_t fetch_request_updated_events_for_runtime_compilation;

    struct kan_repository_indexed_interval_read_query_t read_interval__resource_provider_operation__priority;
    struct kan_repository_indexed_interval_write_query_t write_interval__resource_provider_operation__priority;

    KAN_REFLECTION_IGNORE
    struct resource_provider_execution_shared_state_t execution_shared_state;

    kan_time_size_t frame_begin_time_ns;

    /// \details Every time hot reload is happened, we need to restart active runtime compilation as reflection
    ///          including meta could have changed.
    KAN_REFLECTION_IGNORE
    kan_bool_t need_to_restart_runtime_compilation;

    kan_instance_size_t trailing_data_count;

    KAN_REFLECTION_IGNORE
    void *trailing_data[0u];
};

_Static_assert (_Alignof (struct resource_provider_state_t) ==
                    _Alignof (struct resource_provider_native_container_type_data_t),
                "Alignment matches.");

struct universe_resource_provider_generated_container_type_node_t
{
    struct universe_resource_provider_generated_container_type_node_t *next;

    KAN_REFLECTION_IGNORE
    struct kan_reflection_struct_t type;

    const struct kan_reflection_struct_t *source_type;
    enum resource_provider_native_container_type_source_t source;
    const struct kan_resource_compilable_meta_t *compilable;
    kan_interned_string_t compiled_from;
};

struct kan_reflection_generator_universe_resource_provider_t
{
    kan_loop_size_t boostrap_iteration;
    kan_allocation_group_t generated_reflection_group;
    struct universe_resource_provider_generated_container_type_node_t *first_container_type;
    kan_instance_size_t container_types_count;

    KAN_REFLECTION_IGNORE
    struct kan_reflection_struct_t mutator_type;

    KAN_REFLECTION_IGNORE
    struct kan_reflection_function_t mutator_deploy_function;

    KAN_REFLECTION_IGNORE
    struct kan_reflection_function_t mutator_execute_function;

    KAN_REFLECTION_IGNORE
    struct kan_reflection_function_t mutator_undeploy_function;

    kan_interned_string_t interned_kan_resource_resource_type_meta_t;
    kan_interned_string_t interned_kan_resource_compilable_meta_t;
    kan_interned_string_t interned_kan_resource_byproduct_type_meta_t;
    kan_interned_string_t interned_container_id;
    kan_interned_string_t interned_stored_resource;
};

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_private_singleton_init (
    struct resource_provider_private_singleton_t *data)
{
    data->status = RESOURCE_PROVIDER_STATUS_NOT_INITIALIZED;
    data->container_id_counter = kan_atomic_int_init (1);
    data->byproduct_id_counter = kan_atomic_int_init (1);
    data->attachment_id_counter = KAN_TYPED_ID_32_GET (KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t)) + 1u;

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

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_compiled_resource_entry_init (
    struct resource_provider_compiled_resource_entry_t *instance)
{
    instance->request_count = 0u;
    instance->compiled_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
    instance->compilation_state = RESOURCE_PROVIDER_COMPILATION_STATE_NOT_PENDING;
    instance->pending_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
    instance->current_compilation_index = 0u;
    instance->pending_compilation_index = 0u;
    instance->source_resource_request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
    instance->last_used_source_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_raw_byproduct_entry_init (
    struct resource_provider_raw_byproduct_entry_t *instance)
{
    instance->request_count = 0u;
    instance->container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
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
        kan_free_general (instance->my_allocation_group, instance->loaded_data,
                          kan_apply_alignment (instance->loaded_data_size, _Alignof (kan_memory_size_t)));
    }

    if (instance->loading_data)
    {
        kan_free_general (instance->my_allocation_group, instance->loading_data,
                          kan_apply_alignment (instance->loading_data_size, _Alignof (kan_memory_size_t)));
    }
}

static inline void resource_provider_operation_destroy_native (struct resource_provider_operation_t *instance)
{
    KAN_ASSERT (instance->operation_type == RESOURCE_PROVIDER_OPERATION_TYPE_LOAD)
    if (instance->target_type)
    {
        switch (instance->load.native.format_cache)
        {
        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY:
            kan_serialization_binary_reader_destroy (instance->load.native.binary_reader);
            break;

        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA:
            kan_serialization_rd_reader_destroy (instance->load.native.readable_data_reader);
            break;
        }
    }
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void resource_provider_operation_shutdown (
    struct resource_provider_operation_t *instance)
{
    switch (instance->operation_type)
    {
    case RESOURCE_PROVIDER_OPERATION_TYPE_LOAD:
        if (instance->load.stream)
        {
            instance->load.stream->operations->close (instance->load.stream);
        }

        resource_provider_operation_destroy_native (instance);
        break;

    case RESOURCE_PROVIDER_OPERATION_TYPE_COMPILE:
        break;
    }
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
    data->serve_budget_ns = 0u;
    data->resource_directory_path = NULL;

    kan_stack_group_allocator_init (&data->temporary_allocator,
                                    kan_allocation_group_get_child (data->my_allocation_group, "temporary"),
                                    KAN_UNIVERSE_RESOURCE_PROVIDER_TEMPORARY_CHUNK_SIZE);

    data->interned_kan_resource_index_t = kan_string_intern ("kan_resource_index_t");
    data->interned_resource_provider_server = kan_string_intern ("resource_provider_server");
    data->interned_kan_resource_compilable_meta_t = kan_string_intern ("kan_resource_compilable_meta_t");
    data->interned_kan_resource_byproduct_type_meta_t = kan_string_intern ("kan_resource_byproduct_type_meta_t");
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void mutator_template_deploy_resource_provider (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct resource_provider_state_t *state)
{
    const struct kan_resource_provider_configuration_t *configuration =
        kan_universe_world_query_configuration (world, kan_string_intern (KAN_RESOURCE_PROVIDER_CONFIGURATION));
    KAN_ASSERT (configuration)

    state->scan_budget_ns = configuration->scan_budget_ns;
    state->serve_budget_ns = configuration->serve_budget_ns;
    state->use_load_only_string_registry = configuration->use_load_only_string_registry;
    state->enable_runtime_compilation = KAN_FALSE;
    state->resource_directory_path = configuration->resource_directory_path;

    state->reflection_registry = kan_universe_get_reflection_registry (universe);
    state->shared_script_storage = kan_serialization_binary_script_storage_create (state->reflection_registry);

    state->hot_reload_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_NAME);

    state->resource_pipeline_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_RESOURCE_PIPELINE_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (state->resource_pipeline_system) &&
        kan_resource_pipeline_system_is_runtime_compilation_enabled (state->resource_pipeline_system))
    {
        state->enable_runtime_compilation = KAN_TRUE;
    }

    state->virtual_file_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_VIRTUAL_FILE_SYSTEM_NAME);
    KAN_ASSERT (KAN_HANDLE_IS_VALID (state->virtual_file_system))

    state->serialized_index_stream = NULL;
    state->serialized_index_reader = KAN_HANDLE_SET_INVALID (kan_serialization_binary_reader_t);
    state->string_registry_stream = NULL;
    state->string_registry_reader = KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_reader_t);

    if (state->enable_runtime_compilation)
    {
        kan_repository_event_storage_t request_event_storage =
            kan_repository_event_storage_open (world_repository, "kan_resource_request_updated_event_t");

        kan_repository_event_fetch_query_init (&state->fetch_request_updated_events_for_runtime_compilation,
                                               request_event_storage);

        state->platform_configuration_change_listener =
            kan_resource_pipeline_system_add_platform_configuration_change_listener (state->resource_pipeline_system);
    }

    state->need_to_restart_runtime_compilation = KAN_TRUE;
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
    const kan_resource_entry_id_t attachment_id =
        KAN_TYPED_ID_32_SET (kan_resource_entry_id_t, (kan_id_32_t) private->attachment_id_counter++);

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
    const kan_resource_entry_id_t attachment_id =
        KAN_TYPED_ID_32_SET (kan_resource_entry_id_t, (kan_id_32_t) private->attachment_id_counter++);

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

    view->container_id = KAN_TYPED_ID_32_SET (kan_resource_container_id_t,
                                              (kan_id_32_t) kan_atomic_int_add (&private->container_id_counter, 1));
    *data_begin_output =
        ((uint8_t *) view) + kan_apply_alignment (offsetof (struct kan_resource_container_view_t, data_begin),
                                                  data->contained_type_alignment);

    return view;
}

static inline const struct kan_resource_container_view_t *native_container_read (
    struct resource_provider_state_t *state,
    kan_interned_string_t type,
    kan_resource_container_id_t container_id,
    struct kan_repository_indexed_value_read_access_t *access_output,
    const uint8_t **data_begin_output)
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

    struct kan_repository_indexed_value_read_cursor_t cursor =
        kan_repository_indexed_value_read_query_execute (&data->read_by_id_query, &container_id);
    *access_output = kan_repository_indexed_value_read_cursor_next (&cursor);
    kan_repository_indexed_value_read_cursor_close (&cursor);

    const struct kan_resource_container_view_t *view =
        (struct kan_resource_container_view_t *) kan_repository_indexed_value_read_access_resolve (access_output);

    if (view)
    {
        *data_begin_output =
            ((uint8_t *) view) + kan_apply_alignment (offsetof (struct kan_resource_container_view_t, data_begin),
                                                      data->contained_type_alignment);
    }

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
#if defined(KAN_WITH_ASSERT)
    const kan_bool_t expected_new_data = request->expecting_new_data;
#endif
    request->expecting_new_data = KAN_FALSE;

    if (request->sleeping)
    {
        return;
    }

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

#if defined(KAN_WITH_ASSERT)
    if (expected_new_data)
    {
        // Otherwise, expectation from old to new data would go unnoticed and
        // high level systems would be stuck in waiting state.
        KAN_ASSERT (changed)
    }
#endif

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

static inline void resource_provider_operation_cancel (struct resource_provider_state_t *state,
                                                       kan_interned_string_t type,
                                                       kan_interned_string_t name)
{
    KAN_UP_VALUE_DELETE (operation, resource_provider_operation_t, target_name, &name)
    {
        if (operation->target_type == type)
        {
            switch (operation->operation_type)
            {
            case RESOURCE_PROVIDER_OPERATION_TYPE_LOAD:
                break;

            case RESOURCE_PROVIDER_OPERATION_TYPE_COMPILE:
                if (KAN_TYPED_ID_32_IS_VALID (operation->compile.state_container_id))
                {
                    native_container_delete (state, operation->compile.state_type_name,
                                             operation->compile.state_container_id);
                }

                break;
            }

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

static inline void inform_requests_about_new_data (struct resource_provider_state_t *state,
                                                   kan_interned_string_t type,
                                                   kan_interned_string_t name)
{
    if (KAN_HANDLE_IS_VALID (state->hot_reload_system) &&
        kan_hot_reload_coordination_system_get_current_mode (state->hot_reload_system) != KAN_HOT_RELOAD_MODE_DISABLED)
    {
        KAN_UP_VALUE_UPDATE (request, kan_resource_request_t, name, &name)
        {
            if (request->type == type)
            {
                request->expecting_new_data = KAN_TRUE;
            }
        }
    }
}

static inline void schedule_native_entry_loading (struct resource_provider_state_t *state,
                                                  struct resource_provider_private_singleton_t *private,
                                                  struct kan_resource_native_entry_t *entry,
                                                  struct resource_provider_native_entry_suffix_t *entry_suffix,
                                                  kan_instance_size_t min_priority)
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

    inform_requests_about_new_data (state, entry->type, entry->name);
    entry_suffix->loading_container_id = container_view->container_id;

    KAN_UP_INDEXED_INSERT (operation, resource_provider_operation_t)
    {
        const kan_instance_size_t natural_priority = gather_request_priority (state, NULL, entry->name);
        operation->priority = KAN_MAX (min_priority, natural_priority);

        operation->target_type = entry->type;
        operation->target_name = entry->name;
        operation->operation_type = RESOURCE_PROVIDER_OPERATION_TYPE_LOAD;
        operation->load.stream = stream;
        operation->load.native.format_cache = entry_suffix->format;
        operation->load.native.used_registry = state->reflection_registry;

        switch (entry_suffix->format)
        {
        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY:
            operation->load.native.binary_reader = kan_serialization_binary_reader_create (
                stream, data_begin, entry->type, state->shared_script_storage, entry_suffix->string_registry,
                container_view->my_allocation_group);
            break;

        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA:
            operation->load.native.readable_data_reader = kan_serialization_rd_reader_create (
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

    resource_provider_operation_cancel (state, entry->type, entry->name);
    native_container_delete (state, entry->type, entry_suffix->loading_container_id);
    entry_suffix->loading_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
}

static inline void schedule_third_party_entry_loading (
    struct resource_provider_state_t *state,
    struct kan_resource_third_party_entry_t *entry,
    struct resource_provider_third_party_entry_suffix_t *entry_suffix,
    kan_instance_size_t min_priority)
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

    inform_requests_about_new_data (state, NULL, entry->name);
    entry_suffix->loading_data = kan_allocate_general (entry_suffix->my_allocation_group,
                                                       kan_apply_alignment (entry->size, _Alignof (kan_memory_size_t)),
                                                       _Alignof (kan_memory_size_t));
    entry_suffix->loading_data_size = entry->size;

    KAN_UP_INDEXED_INSERT (operation, resource_provider_operation_t)
    {
        const kan_instance_size_t natural_priority = gather_request_priority (state, NULL, entry->name);
        operation->priority = KAN_MAX (min_priority, natural_priority);

        operation->target_type = NULL;
        operation->target_name = entry->name;
        operation->operation_type = RESOURCE_PROVIDER_OPERATION_TYPE_LOAD;
        operation->load.stream = stream;
        operation->load.third_party.offset = 0u;
        operation->load.third_party.size = entry->size;
        operation->load.third_party.data = entry_suffix->loading_data;
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

    resource_provider_operation_cancel (state, NULL, entry->name);
    kan_free_general (entry_suffix->my_allocation_group, entry_suffix->loading_data, entry_suffix->loading_data_size);
    entry_suffix->loading_data = NULL;
    entry_suffix->loading_data_size = 0u;
}

static inline void bootstrap_pending_compilation (struct resource_provider_state_t *state,
                                                  const struct kan_resource_provider_singleton_t *public,
                                                  struct resource_provider_compiled_resource_entry_t *entry,
                                                  kan_instance_size_t min_priority)
{
    KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
    {
        request->request_id = kan_next_resource_request_id (public);
        request->type = entry->source_type;
        request->name = entry->name;
        request->priority = gather_request_priority (state, entry->type, entry->name);
        request->priority = KAN_MAX (request->priority, min_priority);
        entry->source_resource_request_id = request->request_id;
    }

    inform_requests_about_new_data (state, entry->type, entry->name);
    entry->compilation_state = RESOURCE_PROVIDER_COMPILATION_STATE_WAITING_FOR_SOURCE;
    ++entry->pending_compilation_index;
}

static inline void compiled_entry_remove_dependencies (struct resource_provider_state_t *state,
                                                       struct resource_provider_compiled_resource_entry_t *entry)
{
    KAN_UP_VALUE_DELETE (old_dependency, resource_provider_compilation_dependency_t, compiled_name, &entry->name)
    {
        if (old_dependency->compiled_type == entry->type)
        {
            if (KAN_TYPED_ID_32_IS_VALID (old_dependency->request_id))
            {
                KAN_UP_VALUE_DELETE (old_request, kan_resource_request_t, request_id, &old_dependency->request_id)
                {
                    KAN_UP_ACCESS_DELETE (old_request);
                }
            }

            KAN_UP_ACCESS_DELETE (old_dependency);
        }
    }
}

static inline void add_compilation_dependency (struct resource_provider_state_t *state,
                                               const struct kan_resource_provider_singleton_t *public,
                                               kan_interned_string_t compiled_type,
                                               kan_interned_string_t compiled_name,
                                               kan_interned_string_t dependency_type,
                                               kan_interned_string_t dependency_name,
                                               kan_instance_size_t priority)
{
    KAN_UP_INDEXED_INSERT (dependency, resource_provider_compilation_dependency_t)
    {
        KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (public);
            request->type = dependency_type;
            request->name = dependency_name;
            request->priority = priority;
            dependency->request_id = request->request_id;
        }

        dependency->compiled_type = compiled_type;
        dependency->compiled_name = compiled_name;
        dependency->dependency_type = dependency_type;
        dependency->dependency_name = dependency_name;
    }
}

static inline void schedule_compilation (struct resource_provider_state_t *state,
                                         struct resource_provider_private_singleton_t *private,
                                         struct resource_provider_compiled_resource_entry_t *entry,
                                         kan_instance_size_t min_priority)
{
    KAN_ASSERT (entry->compilation_state == RESOURCE_PROVIDER_COMPILATION_STATE_WAITING_FOR_DEPENDENCIES)
    KAN_ASSERT (!KAN_TYPED_ID_32_IS_VALID (entry->pending_container_id))

    struct kan_repository_indexed_insertion_package_t container_package;
    uint8_t *data_begin;
    struct kan_resource_container_view_t *container_view =
        native_container_create (state, private, entry->type, &container_package, &data_begin);

    if (!container_view)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Failed to create container for compiled resource \"%s\" of type \"%s\".", entry->name, entry->type)
        return;
    }

    struct kan_reflection_struct_meta_iterator_t meta_iterator = kan_reflection_registry_query_struct_meta (
        state->reflection_registry, entry->source_type, state->interned_kan_resource_compilable_meta_t);

    const struct kan_resource_compilable_meta_t *meta = kan_reflection_struct_meta_iterator_get (&meta_iterator);
    // If we somehow ended up here, then resource must be compilable.
    KAN_ASSERT (meta)

    kan_resource_container_id_t state_container_id = KAN_TYPED_ID_32_INITIALIZE_INVALID;
    kan_interned_string_t state_type_name = kan_string_intern (meta->state_type_name);

    if (meta->state_type_name)
    {
        struct kan_repository_indexed_insertion_package_t state_container_package;
        uint8_t *state_data_begin;
        struct kan_resource_container_view_t *state_container_view =
            native_container_create (state, private, state_type_name, &state_container_package, &state_data_begin);

        if (!container_view)
        {
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                     "Failed to create state container for compiled resource \"%s\" of type \"%s\" with compilation "
                     "state type \"%s\".",
                     entry->name, entry->type, state_type_name)

            kan_repository_indexed_insertion_package_undo (&container_package);
            return;
        }

        state_container_id = state_container_view->container_id;
        kan_repository_indexed_insertion_package_submit (&state_container_package);
    }

    entry->pending_container_id = container_view->container_id;
    ++entry->pending_compilation_index;
    entry->compilation_state = RESOURCE_PROVIDER_COMPILATION_STATE_COMPILING;

    KAN_UP_INDEXED_INSERT (operation, resource_provider_operation_t)
    {
        if (min_priority < PRIORITY_INTERNAL_BEGIN)
        {
            const kan_instance_size_t request_priority = gather_request_priority (state, entry->type, entry->name);
            operation->priority = KAN_MAX (min_priority, request_priority);
        }
        else
        {
            operation->priority = min_priority;
        }

        operation->target_type = entry->type;
        operation->target_name = entry->name;
        operation->operation_type = RESOURCE_PROVIDER_OPERATION_TYPE_COMPILE;
        operation->compile.state_container_id = state_container_id;
        operation->compile.state_type_name = state_type_name;
    }

    kan_repository_indexed_insertion_package_submit (&container_package);
}

static inline void cancel_runtime_compilation (struct resource_provider_state_t *state,
                                               struct resource_provider_compiled_resource_entry_t *entry);

static inline void transition_compiled_entry_state (struct resource_provider_state_t *state,
                                                    const struct kan_resource_provider_singleton_t *public,
                                                    struct resource_provider_private_singleton_t *private,
                                                    struct resource_provider_compiled_resource_entry_t *entry,
                                                    kan_instance_size_t min_priority)
{
    switch (entry->compilation_state)
    {
    case RESOURCE_PROVIDER_COMPILATION_STATE_NOT_PENDING:
    case RESOURCE_PROVIDER_COMPILATION_STATE_DONE:
        // These states can not be changed from here.
        break;

    case RESOURCE_PROVIDER_COMPILATION_STATE_PENDING:
        bootstrap_pending_compilation (state, public, entry, min_priority);
        break;

    case RESOURCE_PROVIDER_COMPILATION_STATE_WAITING_FOR_SOURCE:
    {
        entry->last_used_source_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
        kan_bool_t expecting_new_data = KAN_FALSE;

        KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &entry->source_resource_request_id)
        {
            entry->last_used_source_container_id = request->provided_container_id;
            expecting_new_data = request->expecting_new_data;
        }

        if (!KAN_TYPED_ID_32_IS_VALID (entry->last_used_source_container_id) || expecting_new_data)
        {
            break;
        }

        compiled_entry_remove_dependencies (state, entry);
        struct kan_repository_indexed_value_read_access_t access;
        const uint8_t *data = NULL;

        if (!native_container_read (state, entry->source_type, entry->last_used_source_container_id, &access, &data))
        {
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                     "Internal error, failed to retrieve data of loaded container for \"%s\" of type \"%s\".",
                     entry->name, entry->source_type)
            break;
        }

        kan_instance_size_t priority = gather_request_priority (state, entry->type, entry->name);
        priority = KAN_MAX (priority, min_priority);

        struct kan_resource_detected_reference_container_t reference_container;
        kan_resource_detected_reference_container_init (&reference_container);

        struct kan_resource_reference_type_info_storage_t *info_storage =
            kan_resource_pipeline_system_get_reference_type_info_storage (state->resource_pipeline_system);
        KAN_ASSERT (info_storage)

        kan_resource_detect_references (info_storage, entry->source_type, data, &reference_container);
        kan_repository_indexed_value_read_access_close (&access);
        kan_bool_t no_dependencies = KAN_TRUE;

        for (kan_loop_size_t index = 0u; index < reference_container.detected_references.size; ++index)
        {
            struct kan_resource_detected_reference_t *reference =
                &((struct kan_resource_detected_reference_t *) reference_container.detected_references.data)[index];

            switch (reference->compilation_usage)
            {
            case KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED:
                break;

            case KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW:
                no_dependencies = KAN_FALSE;
                add_compilation_dependency (state, public, entry->type, entry->name, reference->type, reference->name,
                                            priority);
                break;

            case KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED:
            {
                no_dependencies = KAN_FALSE;
                struct kan_reflection_struct_meta_iterator_t meta_iterator = kan_reflection_registry_query_struct_meta (
                    state->reflection_registry, reference->type, state->interned_kan_resource_compilable_meta_t);

                const struct kan_resource_compilable_meta_t *meta =
                    kan_reflection_struct_meta_iterator_get (&meta_iterator);

                if (meta)
                {
                    add_compilation_dependency (state, public, entry->type, entry->name,
                                                kan_string_intern (meta->output_type_name), reference->name, priority);
                }
                else
                {
                    add_compilation_dependency (state, public, entry->type, entry->name, reference->type,
                                                reference->name, priority);
                }

                break;
            }
            }
        }

        kan_resource_detected_reference_container_shutdown (&reference_container);
        entry->compilation_state = RESOURCE_PROVIDER_COMPILATION_STATE_WAITING_FOR_DEPENDENCIES;

        if (no_dependencies)
        {
            // If there were no dependencies, we need to transition right away.
            transition_compiled_entry_state (state, public, private, entry, min_priority);
        }

        break;
    }

    case RESOURCE_PROVIDER_COMPILATION_STATE_WAITING_FOR_DEPENDENCIES:
    {
        kan_resource_container_id_t source_container_id = entry->last_used_source_container_id;
        KAN_UP_VALUE_READ (source_request, kan_resource_request_t, request_id, &entry->source_resource_request_id)
        {
            source_container_id = source_request->provided_container_id;
        }

        if (!KAN_TYPED_ID_32_IS_EQUAL (source_container_id, entry->last_used_source_container_id))
        {
            // Something has changed compilation source, therefore we need to restart compilation.
            cancel_runtime_compilation (state, entry);
            bootstrap_pending_compilation (state, public, entry, min_priority);
            break;
        }

        kan_bool_t is_all_dependencies_ready = KAN_TRUE;
        KAN_UP_VALUE_READ (dependency, resource_provider_compilation_dependency_t, compiled_name, &entry->name)
        {
            if (dependency->compiled_type == entry->type)
            {
                KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &dependency->request_id)
                {
                    if (dependency->dependency_type)
                    {
                        is_all_dependencies_ready =
                            KAN_TYPED_ID_32_IS_VALID (request->provided_container_id) && !request->expecting_new_data;
                    }
                    else
                    {
                        is_all_dependencies_ready =
                            request->provided_third_party.data != NULL && !request->expecting_new_data;
                    }
                }

                if (!is_all_dependencies_ready)
                {
                    KAN_UP_QUERY_BREAK;
                }
            }
        }

        if (!is_all_dependencies_ready)
        {
            break;
        }

        schedule_compilation (state, private, entry, min_priority);
        entry->compilation_state = RESOURCE_PROVIDER_COMPILATION_STATE_COMPILING;
        break;
    }

    case RESOURCE_PROVIDER_COMPILATION_STATE_COMPILING:
        cancel_runtime_compilation (state, entry);
        bootstrap_pending_compilation (state, public, entry, min_priority);
        break;
    }
}

static inline kan_bool_t resource_provider_raw_byproduct_entry_has_producers (struct resource_provider_state_t *state,
                                                                              kan_interned_string_t byproduct_type,
                                                                              kan_interned_string_t byproduct_name)
{
    KAN_UP_VALUE_READ (production, resource_provider_byproduct_production_t, byproduct_name, &byproduct_name)
    {
        if (production->byproduct_type == byproduct_type)
        {
            KAN_UP_QUERY_RETURN_VALUE (kan_bool_t, KAN_TRUE);
        }
    }

    return KAN_FALSE;
}

static inline void unload_compiled_entry (struct resource_provider_state_t *state,
                                          struct resource_provider_compiled_resource_entry_t *entry);

static inline kan_bool_t resource_provider_raw_byproduct_entry_update_reference_status (
    struct resource_provider_state_t *state,
    kan_interned_string_t type,
    kan_interned_string_t name,
    kan_bool_t remove_reference)
{
    kan_bool_t byproduct_deleted = KAN_FALSE;
    kan_bool_t found = KAN_FALSE;

    KAN_UP_VALUE_WRITE (entry, resource_provider_raw_byproduct_entry_t, name, &name)
    {
        if (entry->type == type)
        {
            if (remove_reference)
            {
                KAN_ASSERT (entry->request_count > 0u)
                --entry->request_count;
            }

            if (entry->request_count == 0u && !resource_provider_raw_byproduct_entry_has_producers (state, type, name))
            {
                native_container_delete (state, type, entry->container_id);
                KAN_UP_ACCESS_DELETE (entry);
                byproduct_deleted = KAN_TRUE;
            }

            found = KAN_TRUE;
            KAN_UP_QUERY_BREAK;
        }
    }

    if (byproduct_deleted)
    {
        // Delete compiled entry as well if it is not referenced anymore.
        KAN_UP_VALUE_WRITE (compiled_entry, resource_provider_compiled_resource_entry_t, name, &name)
        {
            if (compiled_entry->source_type == type)
            {
                if (compiled_entry->request_count == 0u)
                {
                    unload_compiled_entry (state, compiled_entry);
                    cancel_runtime_compilation (state, compiled_entry);
                    KAN_UP_ACCESS_DELETE (compiled_entry);
                }

                KAN_UP_QUERY_BREAK;
            }
        }
    }

    return found;
}

struct resource_provider_type_name_pair_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
};

static inline void dynamic_array_add_resource_provider_type_name_pair (struct kan_dynamic_array_t *array,
                                                                       kan_interned_string_t type,
                                                                       kan_interned_string_t name)
{
    struct resource_provider_type_name_pair_t *pair = kan_dynamic_array_add_last (array);
    if (!pair)
    {
        kan_dynamic_array_set_capacity (array, array->size * 2u);
        pair = kan_dynamic_array_add_last (array);
    }

    pair->type = type;
    pair->name = name;
}

static inline void dynamic_array_add_resource_provider_type_name_pair_unique (struct kan_dynamic_array_t *array,
                                                                              kan_interned_string_t type,
                                                                              kan_interned_string_t name)
{
    for (kan_loop_size_t index = 0u; index < array->size; ++index)
    {
        struct resource_provider_type_name_pair_t *existent =
            &((struct resource_provider_type_name_pair_t *) array->data)[index];

        if (type == existent->type && name == existent->name)
        {
            return;
        }
    }

    dynamic_array_add_resource_provider_type_name_pair (array, type, name);
}

static inline void remove_references_to_byproducts (struct resource_provider_state_t *state,
                                                    kan_interned_string_t compiled_type,
                                                    kan_interned_string_t compiled_name,
                                                    kan_instance_size_t at_compilation_index)
{
    KAN_ASSERT (state->enable_runtime_compilation)
    struct kan_dynamic_array_t byproducts_to_update_references;
    kan_dynamic_array_init (&byproducts_to_update_references, KAN_UNIVERSE_RESOURCE_PROVIDER_RC_INITIAL_SIZE,
                            sizeof (struct resource_provider_type_name_pair_t),
                            _Alignof (struct resource_provider_type_name_pair_t), state->my_allocation_group);

    KAN_UP_VALUE_DELETE (production, resource_provider_byproduct_production_t, compiled_name, &compiled_name)
    {
        if (production->compiled_type == compiled_type && production->compilation_index == at_compilation_index)
        {
            dynamic_array_add_resource_provider_type_name_pair_unique (
                &byproducts_to_update_references, production->byproduct_type, production->byproduct_name);
            KAN_UP_ACCESS_DELETE (production);
        }
    }

    for (kan_loop_size_t index = 0u; index < byproducts_to_update_references.size; ++index)
    {
        struct resource_provider_type_name_pair_t *to_update =
            &((struct resource_provider_type_name_pair_t *) byproducts_to_update_references.data)[index];
        resource_provider_raw_byproduct_entry_update_reference_status (state, to_update->type, to_update->name,
                                                                       KAN_FALSE);
    }

    kan_dynamic_array_shutdown (&byproducts_to_update_references);
}

static inline void unload_compiled_entry (struct resource_provider_state_t *state,
                                          struct resource_provider_compiled_resource_entry_t *entry)
{
    if (!KAN_TYPED_ID_32_IS_VALID (entry->compiled_container_id))
    {
        return;
    }

    update_requests (state, entry->type, entry->name, KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t), NULL,
                     0u);
    remove_references_to_byproducts (state, entry->type, entry->name, entry->current_compilation_index);
    native_container_delete (state, entry->type, entry->compiled_container_id);
    entry->compiled_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
}

static inline void remove_native_entry_reference (struct resource_provider_state_t *state,
                                                  kan_interned_string_t type,
                                                  kan_interned_string_t name);

static inline void remove_third_party_entry_reference (struct resource_provider_state_t *state,
                                                       kan_interned_string_t name);

static inline void runtime_compilation_delete_dependency_requests (
    struct resource_provider_state_t *state, struct resource_provider_compiled_resource_entry_t *entry)
{
    if (KAN_TYPED_ID_32_IS_VALID (entry->source_resource_request_id))
    {
        KAN_UP_VALUE_DELETE (old_request, kan_resource_request_t, request_id, &entry->source_resource_request_id)
        {
            KAN_UP_ACCESS_DELETE (old_request);
        }

        entry->source_resource_request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
    }

    KAN_UP_VALUE_UPDATE (dependency, resource_provider_compilation_dependency_t, compiled_name, &entry->name)
    {
        if (dependency->compiled_type == entry->type)
        {
            if (KAN_TYPED_ID_32_IS_VALID (dependency->request_id))
            {
                KAN_UP_VALUE_DELETE (old_request, kan_resource_request_t, request_id, &dependency->request_id)
                {
                    KAN_UP_ACCESS_DELETE (old_request);
                }

                dependency->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
            }
        }
    }
}

static inline void cancel_runtime_compilation (struct resource_provider_state_t *state,
                                               struct resource_provider_compiled_resource_entry_t *entry)
{
    if (entry->compilation_state == RESOURCE_PROVIDER_COMPILATION_STATE_NOT_PENDING ||
        entry->compilation_state == RESOURCE_PROVIDER_COMPILATION_STATE_DONE)
    {
        return;
    }

    resource_provider_operation_cancel (state, entry->type, entry->name);
    runtime_compilation_delete_dependency_requests (state, entry);
    entry->compilation_state = RESOURCE_PROVIDER_COMPILATION_STATE_NOT_PENDING;

    if (KAN_TYPED_ID_32_IS_VALID (entry->pending_container_id))
    {
        remove_references_to_byproducts (state, entry->type, entry->name, entry->pending_compilation_index);
        native_container_delete (state, entry->type, entry->pending_container_id);
        entry->pending_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
    }
}

static inline void add_compiled_entry_and_start_compilation (struct resource_provider_state_t *state,
                                                             const struct kan_resource_provider_singleton_t *public,
                                                             struct resource_provider_private_singleton_t *private,
                                                             kan_interned_string_t type,
                                                             kan_interned_string_t name,
                                                             kan_interned_string_t source_type,
                                                             kan_instance_size_t request_count)
{
    KAN_UP_INDEXED_INSERT (new_entry, resource_provider_compiled_resource_entry_t)
    {
        new_entry->type = type;
        new_entry->name = name;
        new_entry->source_type = source_type;
        new_entry->request_count = request_count;
        new_entry->compilation_state = RESOURCE_PROVIDER_COMPILATION_STATE_PENDING;
    }
}

static inline void add_native_entry_reference (struct resource_provider_state_t *state,
                                               const struct kan_resource_provider_singleton_t *public,
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
                    if (KAN_TYPED_ID_32_IS_VALID (request_id) &&
                        // Special case for hot reload: there is no need to rush and update request with old data when
                        // we're already loading new data.
                        !KAN_TYPED_ID_32_IS_VALID (suffix->loading_container_id))
                    {
                        KAN_UP_VALUE_UPDATE (request, kan_resource_request_t, request_id, &request_id)
                        {
                            update_request_provided_data (state, request, suffix->loaded_container_id, NULL, 0u);
                        }
                    }
                }
                else if (!KAN_TYPED_ID_32_IS_VALID (suffix->loading_container_id))
                {
                    schedule_native_entry_loading (state, private, entry, suffix, 0u);
                }

                KAN_UP_QUERY_RETURN_VOID;
            }

            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                     "Unable to find suffix of requested native resource \"%s\" of type \"%s\".", name, type)
            KAN_UP_QUERY_RETURN_VOID;
        }
    }

    if (state->enable_runtime_compilation)
    {
        KAN_UP_VALUE_UPDATE (compiled_entry, resource_provider_compiled_resource_entry_t, name, &name)
        {
            if (compiled_entry->type == type)
            {
                ++compiled_entry->request_count;
                if (KAN_TYPED_ID_32_IS_VALID (compiled_entry->compiled_container_id))
                {
                    if (KAN_TYPED_ID_32_IS_VALID (request_id) &&
                        // Special case for hot reload: there is no need to rush and update request with old data when
                        // we're already loading new data.
                        compiled_entry->compilation_state == RESOURCE_PROVIDER_COMPILATION_STATE_DONE)
                    {
                        KAN_UP_VALUE_UPDATE (request, kan_resource_request_t, request_id, &request_id)
                        {
                            update_request_provided_data (state, request, compiled_entry->compiled_container_id, NULL,
                                                          0u);
                        }
                    }
                }

                if (compiled_entry->compilation_state == RESOURCE_PROVIDER_COMPILATION_STATE_NOT_PENDING)
                {
                    compiled_entry->compilation_state = RESOURCE_PROVIDER_COMPILATION_STATE_PENDING;
                }

                KAN_UP_QUERY_RETURN_VOID;
            }
        }

        struct resource_provider_native_container_type_data_t *type_data = query_container_type_data (state, type);
        if (type_data && type_data->compiled_from)
        {
            kan_bool_t has_compilation_source = KAN_FALSE;
            KAN_UP_VALUE_UPDATE (source_entry, kan_resource_native_entry_t, name, &name)
            {
                if (source_entry->type == type_data->compiled_from)
                {
                    has_compilation_source = KAN_TRUE;
                    KAN_UP_QUERY_BREAK;
                }
            }

            if (!has_compilation_source)
            {
                KAN_UP_VALUE_READ (byproduct, resource_provider_raw_byproduct_entry_t, name, &name)
                {
                    if (byproduct->type == type_data->compiled_from)
                    {
                        has_compilation_source = KAN_TRUE;
                        KAN_UP_QUERY_BREAK;
                    }
                }
            }

            if (has_compilation_source)
            {
                // This must be first request that mentions this entry and following requests will be processed as
                // events too, therefore we have no need to count them here.
                add_compiled_entry_and_start_compilation (state, public, private, type, name, type_data->compiled_from,
                                                          1u);

                KAN_UP_QUERY_RETURN_VOID;
            }
        }

        KAN_UP_VALUE_UPDATE (byproduct, resource_provider_raw_byproduct_entry_t, name, &name)
        {
            if (byproduct->type == type)
            {
                // Existent byproducts are always loaded, therefore there is nothing more to do with them.
                ++byproduct->request_count;

                if (KAN_TYPED_ID_32_IS_VALID (request_id))
                {
                    KAN_UP_VALUE_UPDATE (request, kan_resource_request_t, request_id, &request_id)
                    {
                        update_request_provided_data (state, request, byproduct->container_id, NULL, 0u);
                    }
                }

                KAN_UP_QUERY_RETURN_VOID;
            }
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
                if (KAN_TYPED_ID_32_IS_VALID (request_id) &&
                    // Special case for hot reload: there is no need to rush and update request with old data when
                    // we're already loading new data.
                    !suffix->loading_data)
                {
                    KAN_UP_VALUE_UPDATE (request, kan_resource_request_t, request_id, &request_id)
                    {
                        update_request_provided_data (state, request,
                                                      KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t),
                                                      suffix->loaded_data, suffix->loaded_data_size);
                    }
                }
            }
            else if (!suffix->loading_data)
            {
                schedule_third_party_entry_loading (state, entry, suffix, 0u);
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

    if (state->enable_runtime_compilation)
    {
        KAN_UP_VALUE_UPDATE (compiled_entry, resource_provider_compiled_resource_entry_t, name, &name)
        {
            if (compiled_entry->type == type)
            {
                KAN_ASSERT (compiled_entry->request_count > 0u)
                --compiled_entry->request_count;

                if (compiled_entry->request_count == 0u)
                {
                    // Cancel and unload everything, but keep the entry.
                    // TODO: Currently breaks byproducts as it may destroy byproduct that is referenced by other
                    //       resources. For example, child material instance may reference static data produced
                    //       from parent material instance, but parent material instance might not be referenced
                    //       by anything except for child material instance compilation. Therefore, right after child
                    //       material compilation, when user is still unable to reference child material instance,
                    //       parent material instance would be deleted along with its byproduct static data as static
                    //       data is not yet referenced by anything.
                    // unload_compiled_entry (state, compiled_entry);
                    // cancel_runtime_compilation (state, compiled_entry);
                    // compiled_entry->compilation_state = RESOURCE_PROVIDER_COMPILATION_STATE_NOT_PENDING;
                }

                KAN_UP_QUERY_RETURN_VOID;
            }
        }

        if (resource_provider_raw_byproduct_entry_update_reference_status (state, type, name, KAN_TRUE))
        {
            return;
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
                struct kan_hot_reload_automatic_config_t *automatic_config =
                    kan_hot_reload_coordination_system_get_automatic_config (state->hot_reload_system);

                const kan_time_size_t reload_after_ns =
                    kan_precise_time_get_elapsed_nanoseconds () + automatic_config->change_wait_time_ns;
                KAN_ASSERT (KAN_PACKED_TIMER_IS_SAFE_TO_SET (reload_after_ns))
                suffix->reload_after_real_time_timer = KAN_PACKED_TIMER_SET (reload_after_ns);
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
                struct kan_hot_reload_automatic_config_t *automatic_config =
                    kan_hot_reload_coordination_system_get_automatic_config (state->hot_reload_system);

                const kan_time_size_t reload_after_ns =
                    kan_precise_time_get_elapsed_nanoseconds () + automatic_config->change_wait_time_ns;
                KAN_ASSERT (KAN_PACKED_TIMER_IS_SAFE_TO_SET (reload_after_ns))
                suffix->reload_after_real_time_timer = KAN_PACKED_TIMER_SET (reload_after_ns);
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

            if (state->enable_runtime_compilation)
            {
                KAN_UP_VALUE_WRITE (compiled_entry, resource_provider_compiled_resource_entry_t, name,
                                    &info_from_path.name)
                {
                    if (compiled_entry->source_type == native_entry->type)
                    {
                        unload_compiled_entry (state, compiled_entry);
                        cancel_runtime_compilation (state, compiled_entry);
                        compiled_entry_remove_dependencies (state, compiled_entry);
                        KAN_UP_ACCESS_DELETE (compiled_entry);
                    }
                }

                // We do not actually need to manually cancel compilation: it will either be canceled due to request
                // updates or will just stay there indefinitely (or until appropriate resource is added again).
                // Either scenario is fine and will make file addition processing easier to support.
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

            // We do not actually need to manually cancel compilation: it will either be canceled due to request
            // updates or will just stay there indefinitely (or until appropriate resource is added again).
            // Either scenario is fine and will make file addition processing easier to support.

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
                                              const struct kan_resource_provider_singleton_t *public,
                                              struct resource_provider_private_singleton_t *private)
{
    KAN_UP_EVENT_FETCH (event, resource_request_on_insert_event_t)
    {
        if (event->type)
        {
            add_native_entry_reference (state, public, private, event->request_id, event->type, event->name);
        }
        else
        {
            add_third_party_entry_reference (state, event->request_id, event->name);
        }
    }
}

static inline void process_request_on_change (struct resource_provider_state_t *state,
                                              const struct kan_resource_provider_singleton_t *public,
                                              struct resource_provider_private_singleton_t *private)
{
    KAN_UP_EVENT_FETCH (event, resource_request_on_change_event_t)
    {
        if (!event->was_sleeping)
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
                add_native_entry_reference (state, public, private, event->request_id, event->new_type,
                                            event->new_name);
            }
            else
            {
                add_third_party_entry_reference (state, event->request_id, event->new_name);
            }
        }
    }
}

static inline void process_request_on_delete (struct resource_provider_state_t *state)
{
    KAN_UP_EVENT_FETCH (event, resource_request_on_delete_event_t)
    {
        if (!event->was_sleeping)
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
}

static kan_instance_size_t recursively_awake_requests (struct resource_provider_state_t *state,
                                                       const struct kan_resource_provider_singleton_t *public,
                                                       struct resource_provider_private_singleton_t *private,
                                                       kan_interned_string_t type,
                                                       kan_interned_string_t name)
{
    kan_instance_size_t direct_request_count = 0u;
    KAN_UP_VALUE_UPDATE (request, kan_resource_request_t, name, &name)
    {
        if (request->type == type)
        {
            request->sleeping = KAN_FALSE;
            ++direct_request_count;
        }
    }

    KAN_UP_VALUE_UPDATE (compiled_entry, resource_provider_compiled_resource_entry_t, name, &name)
    {
        if (compiled_entry->type == type)
        {
            KAN_ASSERT (direct_request_count >= compiled_entry->request_count)
            compiled_entry->request_count = direct_request_count;

            // Changes should be able to trigger recompilation, therefore we need to update its state properly.
            if (compiled_entry->compilation_state == RESOURCE_PROVIDER_COMPILATION_STATE_DONE ||
                compiled_entry->compilation_state == RESOURCE_PROVIDER_COMPILATION_STATE_NOT_PENDING)
            {
                compiled_entry->compilation_state = direct_request_count > 0u ?
                                                        RESOURCE_PROVIDER_COMPILATION_STATE_PENDING :
                                                        RESOURCE_PROVIDER_COMPILATION_STATE_NOT_PENDING;
            }

            transition_compiled_entry_state (state, public, private, compiled_entry,
                                             state->execution_shared_state.min_priority);
            KAN_UP_QUERY_BREAK;
        }
    }

    kan_interned_string_t compiled_type_name = NULL;
    KAN_UP_VALUE_UPDATE (dependant_compiled_entry, resource_provider_compiled_resource_entry_t, name, &name)
    {
        if (dependant_compiled_entry->source_type == type)
        {
            compiled_type_name = dependant_compiled_entry->type;
            KAN_UP_QUERY_BREAK;
        }
    }

    if (compiled_type_name)
    {
        recursively_awake_requests (state, public, private, compiled_type_name, name);
    }

    KAN_UP_VALUE_READ (dependency, resource_provider_compilation_dependency_t, dependency_name, &name)
    {
        if (dependency->dependency_type == type)
        {
            recursively_awake_requests (state, public, private, dependency->compiled_type, dependency->compiled_name);
        }
    }

    return direct_request_count;
}

static inline void process_file_addition (struct resource_provider_state_t *state,
                                          const struct kan_resource_provider_singleton_t *public,
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

    if (state->enable_runtime_compilation && scan_result.type)
    {
        // Check if new entry is compilable and if there are already requests to compile it.
        struct kan_reflection_struct_meta_iterator_t meta_iterator = kan_reflection_registry_query_struct_meta (
            state->reflection_registry, scan_result.type, state->interned_kan_resource_compilable_meta_t);
        const struct kan_resource_compilable_meta_t *meta = kan_reflection_struct_meta_iterator_get (&meta_iterator);

        if (meta)
        {
            kan_interned_string_t output_type_name = kan_string_intern (meta->output_type_name);
            kan_loop_size_t compiled_request_count = 0u;

            KAN_UP_VALUE_READ (request, kan_resource_request_t, name, &scan_result.name)
            {
                // We do not care about sleeping status here as requests will be awakened down the road.
                if (request->type == output_type_name)
                {
                    ++compiled_request_count;
                }
            }

            if (compiled_request_count > 0u)
            {
                add_compiled_entry_and_start_compilation (state, public, private, output_type_name, scan_result.name,
                                                          scan_result.type, compiled_request_count);
            }
        }
    }

    const kan_instance_size_t request_count =
        recursively_awake_requests (state, public, private, scan_result.type, scan_result.name);

    if (request_count > 0u)
    {
        const kan_instance_size_t min_priority =
            kan_hot_reload_coordination_system_is_hot_swap (state->hot_reload_system) ? PRIORITY_HOT_SWAP : 0u;

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

                        schedule_native_entry_loading (state, private, entry, suffix, min_priority);
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

                    schedule_third_party_entry_loading (state, entry, suffix, min_priority);
                    KAN_UP_QUERY_RETURN_VOID;
                }
            }
        }
    }
}

static inline void process_delayed_addition (struct resource_provider_state_t *state,
                                             const struct kan_resource_provider_singleton_t *public,
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
                                     investigate_after_timer, KAN_UP_NOTHING, &current_timer)
    {
        process_file_addition (state, public, private, delayed_addition->path);
        KAN_UP_ACCESS_DELETE (delayed_addition);
    }
}

static inline void process_delayed_reload (struct resource_provider_state_t *state,
                                           const struct kan_resource_provider_singleton_t *public,
                                           struct resource_provider_private_singleton_t *private)
{
    if (kan_hot_reload_coordination_system_get_current_mode (state->hot_reload_system) !=
            KAN_HOT_RELOAD_MODE_AUTOMATIC_INDEPENDENT &&
        !kan_hot_reload_coordination_system_is_hot_swap (state->hot_reload_system))
    {
        return;
    }

    const kan_instance_size_t min_priority =
        kan_hot_reload_coordination_system_is_hot_swap (state->hot_reload_system) ? PRIORITY_HOT_SWAP : 0u;

    KAN_ASSERT (KAN_PACKED_TIMER_IS_SAFE_TO_SET (kan_precise_time_get_elapsed_nanoseconds ()))
    const kan_packed_timer_t current_timer = kan_hot_reload_coordination_system_is_hot_swap (state->hot_reload_system) ?
                                                 KAN_PACKED_TIMER_MAX :
                                                 KAN_PACKED_TIMER_SET (kan_precise_time_get_elapsed_nanoseconds ());

    KAN_UP_INTERVAL_ASCENDING_UPDATE (native_suffix, resource_provider_native_entry_suffix_t,
                                      reload_after_real_time_timer, KAN_UP_NOTHING, &current_timer)
    {
        KAN_UP_VALUE_UPDATE (entry, kan_resource_native_entry_t, attachment_id, &native_suffix->attachment_id)
        {
            native_suffix->request_count =
                recursively_awake_requests (state, public, private, entry->type, entry->name);

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

                schedule_native_entry_loading (state, private, entry, native_suffix, min_priority);
            }

            native_suffix->reload_after_real_time_timer = KAN_PACKED_TIMER_NEVER;
            KAN_UP_QUERY_BREAK;
        }
    }

    KAN_UP_INTERVAL_ASCENDING_UPDATE (third_party_suffix, resource_provider_third_party_entry_suffix_t,
                                      reload_after_real_time_timer, KAN_UP_NOTHING, &current_timer)
    {
        KAN_UP_VALUE_UPDATE (entry, kan_resource_third_party_entry_t, attachment_id, &third_party_suffix->attachment_id)
        {
            third_party_suffix->request_count = recursively_awake_requests (state, public, private, NULL, entry->name);

            // Update third party size.
            kan_virtual_file_system_volume_t volume =
                kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);
            struct kan_virtual_file_system_entry_status_t status;

            if (kan_virtual_file_system_query_entry (volume, entry->path, &status))
            {
                entry->size = status.size;
            }
            else
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                         "Failed to query third party resource entry \"%s\" at path \"%s\".", entry->name, entry->path)
            }

            kan_virtual_file_system_close_context_read_access (state->virtual_file_system);
            if (third_party_suffix->request_count > 0u)
            {
                cancel_third_party_entry_loading (state, entry, third_party_suffix);
                schedule_third_party_entry_loading (state, entry, third_party_suffix, min_priority);
            }

            third_party_suffix->reload_after_real_time_timer = KAN_PACKED_TIMER_NEVER;
            KAN_UP_QUERY_BREAK;
        }
    }
}

static inline void reset_struct_instance (kan_reflection_registry_t registry,
                                          kan_interned_string_t type,
                                          void *instance)
{
    const struct kan_reflection_struct_t *struct_type = kan_reflection_registry_query_struct (registry, type);
    KAN_ASSERT (struct_type)

    if (struct_type->shutdown)
    {
        struct_type->shutdown (struct_type->functor_user_data, instance);
    }

    if (struct_type->init)
    {
        struct_type->init (struct_type->functor_user_data, instance);
    }
}

static enum resource_provider_serve_operation_status_t execute_shared_serve_load (
    struct resource_provider_state_t *state, struct resource_provider_operation_t *loading_operation)
{
    KAN_ASSERT (loading_operation->priority >= state->execution_shared_state.min_priority)
    if (kan_precise_time_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
    {
        return RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_IN_PROGRESS;
    }

    // Restart loading for native type if reflection has changed.
    if (loading_operation->target_type &&
        !KAN_HANDLE_IS_EQUAL (loading_operation->load.native.used_registry, state->reflection_registry))
    {
        loading_operation->load.native.used_registry = state->reflection_registry;
        resource_provider_operation_destroy_native (loading_operation);
        loading_operation->load.stream->operations->close (loading_operation->load.stream);

        kan_bool_t entry_found = KAN_FALSE;
        kan_bool_t other_error = KAN_FALSE;

        KAN_UP_VALUE_READ (entry, kan_resource_native_entry_t, name, &loading_operation->target_name)
        {
            if (entry->type == loading_operation->target_type)
            {
                entry_found = KAN_TRUE;
                kan_virtual_file_system_volume_t volume =
                    kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);

                loading_operation->load.stream = kan_virtual_file_stream_open_for_read (volume, entry->path);
                kan_virtual_file_system_close_context_read_access (state->virtual_file_system);

                if (!loading_operation->load.stream)
                {
                    KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                             "Unable to restart loading of \"%s\" of type \"%s\" as stream can no longer be opened. "
                             "Internal handling error?",
                             loading_operation->target_name, loading_operation->target_type)

                    other_error = KAN_TRUE;
                    KAN_UP_QUERY_BREAK;
                }

                loading_operation->load.stream = kan_random_access_stream_buffer_open_for_read (
                    loading_operation->load.stream, KAN_UNIVERSE_RESOURCE_PROVIDER_READ_BUFFER);

                KAN_UP_VALUE_UPDATE (suffix, resource_provider_native_entry_suffix_t, attachment_id,
                                     &entry->attachment_id)
                {
                    if (!skip_type_header (loading_operation->load.stream, entry, suffix))
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
                        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                                 "Unable to restart loading of \"%s\" of type \"%s\" as loading container is absent. "
                                 "Internal handling error?",
                                 loading_operation->target_name, loading_operation->target_type)

                        other_error = KAN_TRUE;
                        KAN_UP_QUERY_BREAK;
                    }

                    reset_struct_instance (state->reflection_registry, entry->type, container_data_begin);
                    switch (suffix->format)
                    {
                    case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY:
                        loading_operation->load.native.binary_reader = kan_serialization_binary_reader_create (
                            loading_operation->load.stream, container_data_begin, entry->type,
                            state->shared_script_storage, suffix->string_registry, container_view->my_allocation_group);
                        break;

                    case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA:
                        loading_operation->load.native.readable_data_reader = kan_serialization_rd_reader_create (
                            loading_operation->load.stream, container_data_begin, entry->type,
                            state->reflection_registry, container_view->my_allocation_group);
                        break;
                    }

                    kan_repository_indexed_value_update_access_close (&container_view_access);
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
            return RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_DONE;
        }
    }

    // Process serialization.
    enum kan_serialization_state_t serialization_state = KAN_SERIALIZATION_FINISHED;

    if (loading_operation->target_type)
    {
        switch (loading_operation->load.native.format_cache)
        {
        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY:
        {
            while ((serialization_state = kan_serialization_binary_reader_step (
                        loading_operation->load.native.binary_reader)) == KAN_SERIALIZATION_IN_PROGRESS)
            {
                if (kan_precise_time_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
                {
                    return RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_IN_PROGRESS;
                }
            }

            break;
        }

        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA:
            while ((serialization_state = kan_serialization_rd_reader_step (
                        loading_operation->load.native.readable_data_reader)) == KAN_SERIALIZATION_IN_PROGRESS)
            {
                if (kan_precise_time_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
                {
                    return RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_IN_PROGRESS;
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
                         loading_operation->load.third_party.size - loading_operation->load.third_party.offset);

            if (to_read > 0u)
            {
                if (loading_operation->load.stream->operations->read (
                        loading_operation->load.stream, to_read,
                        loading_operation->load.third_party.data + loading_operation->load.third_party.offset) ==
                    to_read)
                {
                    loading_operation->load.third_party.offset += to_read;
                    serialization_state =
                        loading_operation->load.third_party.offset == loading_operation->load.third_party.size ?
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

            if (kan_precise_time_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
            {
                return RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_IN_PROGRESS;
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
                        kan_free_general (suffix->my_allocation_group, suffix->loaded_data, suffix->loaded_data_size);
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
                     "Failed to load third party resource \"%s\": serialization error.", loading_operation->target_name)

            KAN_UP_VALUE_READ (entry, kan_resource_third_party_entry_t, name, &loading_operation->target_name)
            {
                KAN_UP_VALUE_UPDATE (suffix, resource_provider_third_party_entry_suffix_t, attachment_id,
                                     &entry->attachment_id)
                {
                    if (suffix->loading_data)
                    {
                        kan_free_general (suffix->my_allocation_group, suffix->loading_data, suffix->loading_data_size);
                    }

                    suffix->loading_data = NULL;
                    suffix->loading_data_size = 0u;
                }

                KAN_UP_QUERY_BREAK;
            }
        }

        break;
    }
    }

    kan_atomic_int_unlock (&state->execution_shared_state.concurrency_lock);
    return RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_DONE;
}

struct resource_provider_compilation_interface_user_data_t
{
    struct resource_provider_state_t *state;
    kan_interned_string_t compiled_entry_type;
    kan_interned_string_t compiled_entry_name;
    kan_instance_size_t pending_compilation_index;
};

static inline kan_interned_string_t register_byproduct_internal (kan_functor_user_data_t interface_user_data,
                                                                 kan_interned_string_t byproduct_type_name,
                                                                 kan_interned_string_t byproduct_name,
                                                                 void *byproduct_data)
{
    struct resource_provider_compilation_interface_user_data_t *data =
        (struct resource_provider_compilation_interface_user_data_t *) interface_user_data;
    struct resource_provider_state_t *state = data->state;

    const struct kan_reflection_struct_t *byproduct_type =
        kan_reflection_registry_query_struct (state->reflection_registry, byproduct_type_name);
    KAN_ASSERT (byproduct_type)

    struct kan_reflection_struct_meta_iterator_t meta_iterator = kan_reflection_registry_query_struct_meta (
        state->reflection_registry, byproduct_type_name, state->interned_kan_resource_byproduct_type_meta_t);

    const struct kan_resource_byproduct_type_meta_t *meta = kan_reflection_struct_meta_iterator_get (&meta_iterator);
    // Byproducts are required to have byproduct meta.
    KAN_ASSERT (meta)

#define BYPRODUCT_UNIQUE_HASH KAN_INT_MAX (kan_hash_t)
    kan_hash_t byproduct_hash = BYPRODUCT_UNIQUE_HASH;

    // Byproducts need to be registered under lock in order to avoid excessive byproduct creation.
    kan_atomic_int_lock (&state->execution_shared_state.byproduct_lock);

    if (!byproduct_name)
    {
        // Non unique byproduct, search for available replacements.
        byproduct_hash = meta->hash ?
                             meta->hash (byproduct_data) :
                             kan_reflection_hash_struct (state->reflection_registry, byproduct_type, byproduct_data);

        if (byproduct_hash == BYPRODUCT_UNIQUE_HASH)
        {
            --byproduct_hash;
        }

        KAN_UP_VALUE_READ (byproduct, resource_provider_raw_byproduct_entry_t, hash, &byproduct_hash)
        {
            if (byproduct->hash == byproduct_hash && byproduct->type == byproduct_type_name)
            {
                struct kan_repository_indexed_value_read_access_t container_access;
                const uint8_t *container_data;

                if (native_container_read (state, byproduct_type_name, byproduct->container_id, &container_access,
                                           &container_data))
                {
                    kan_bool_t equal =
                        (meta->is_equal ? meta->is_equal (container_data, byproduct_data) :
                                          kan_reflection_are_structs_equal (state->reflection_registry, byproduct_type,
                                                                            container_data, byproduct_data));
                    kan_repository_indexed_value_read_access_close (&container_access);

                    if (equal)
                    {
                        if (meta->reset)
                        {
                            meta->reset (byproduct_data);
                        }
                        else
                        {
                            kan_reflection_reset_struct (state->reflection_registry, byproduct_type, byproduct_data);
                        }

                        KAN_UP_INDEXED_INSERT (production, resource_provider_byproduct_production_t)
                        {
                            production->compiled_type = data->compiled_entry_type;
                            production->compiled_name = data->compiled_entry_name;
                            production->byproduct_type = byproduct_type_name;
                            production->byproduct_name = byproduct->name;
                            production->compilation_index = data->pending_compilation_index;
                        }

                        byproduct_name = byproduct->name;
                        KAN_UP_QUERY_BREAK;
                    }
                }
            }
        }
    }

    if (byproduct_name && byproduct_hash != BYPRODUCT_UNIQUE_HASH)
    {
        // Replaced by the same byproduct, no need for insertion.
        kan_atomic_int_unlock (&state->execution_shared_state.byproduct_lock);
        return byproduct_name;
    }
#undef BYPRODUCT_UNIQUE_HASH

    KAN_UP_INDEXED_INSERT (new_byproduct, resource_provider_raw_byproduct_entry_t)
    {
        KAN_UP_SINGLETON_READ (private, resource_provider_private_singleton_t)
        {
            int new_byproduct_name_id =
                kan_atomic_int_add ((struct kan_atomic_int_t *) &private->byproduct_id_counter, 1);
            char name_buffer[KAN_UNIVERSE_RESOURCE_PROVIDER_RB_MAX_NAME_LENGTH];

            if (!byproduct_name)
            {
                snprintf (name_buffer, KAN_UNIVERSE_RESOURCE_PROVIDER_RB_MAX_NAME_LENGTH, "byproduct_%s_%lu",
                          byproduct_type_name, (unsigned long) new_byproduct_name_id);
                byproduct_name = kan_string_intern (name_buffer);
            }
            else
            {
                // Unique byproducts are always re-produced as new byproducts with name suffix.
                // It is unavoidable because during hot reload we need to properly handle both old byproduct,
                // that should not override anything until compilation is done, and new byproduct with the new data.
                snprintf (name_buffer, KAN_UNIVERSE_RESOURCE_PROVIDER_RB_MAX_NAME_LENGTH, "%s_%lu",
                          byproduct_name, (unsigned long) new_byproduct_name_id);
                byproduct_name = kan_string_intern (name_buffer);
            }

            new_byproduct->type = byproduct_type_name;
            new_byproduct->name = byproduct_name;
            new_byproduct->hash = byproduct_hash;
            new_byproduct->request_count = 0u;

            struct kan_repository_indexed_insertion_package_t container_package;
            uint8_t *container_data;

            struct kan_resource_container_view_t *container_view =
                native_container_create (state,
                                         // As we are sure that private singleton is only used for atomic id generation,
                                         // we can cast out const like that.
                                         (struct resource_provider_private_singleton_t *) private, byproduct_type_name,
                                         &container_package, &container_data);

            // It would hard to recover from insert error here, therefore we just use assert.
            KAN_ASSERT (container_view)

            new_byproduct->container_id = container_view->container_id;
            if (meta->move)
            {
                meta->move (container_data, byproduct_data);
            }
            else
            {
                kan_reflection_move_struct (state->reflection_registry, byproduct_type, container_data, byproduct_data);
            }

            kan_repository_indexed_insertion_package_submit (&container_package);
            KAN_UP_INDEXED_INSERT (production, resource_provider_byproduct_production_t)
            {
                production->compiled_type = data->compiled_entry_type;
                production->compiled_name = data->compiled_entry_name;
                production->byproduct_type = byproduct_type_name;
                production->byproduct_name = new_byproduct->name;
                production->compilation_index = data->pending_compilation_index;
            }
        }
    }

    kan_atomic_int_unlock (&state->execution_shared_state.byproduct_lock);
    return byproduct_name;
}

static kan_interned_string_t compilation_interface_register_byproduct (kan_functor_user_data_t interface_user_data,
                                                                       kan_interned_string_t byproduct_type_name,
                                                                       void *byproduct_data)
{
    return register_byproduct_internal (interface_user_data, byproduct_type_name, NULL, byproduct_data);
}

static kan_interned_string_t compilation_interface_register_unique_byproduct (kan_functor_user_data_t interface_user_data,
                                                                   kan_interned_string_t byproduct_type_name,
                                                                   kan_interned_string_t byproduct_name,
                                                                   void *byproduct_data)
{
    return register_byproduct_internal (interface_user_data, byproduct_type_name, byproduct_name, byproduct_data);
}

static enum resource_provider_serve_operation_status_t execute_shared_serve_compile (
    struct resource_provider_state_t *state, struct resource_provider_operation_t *compile_operation)
{
    KAN_ASSERT (compile_operation->priority >= state->execution_shared_state.min_priority)
    if (kan_precise_time_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
    {
        return RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_IN_PROGRESS;
    }

    kan_interned_string_t source_type = NULL;
    kan_resource_container_id_t pending_container_id = KAN_TYPED_ID_32_INITIALIZE_INVALID;
    kan_instance_size_t pending_compilation_index = 0u;
    kan_resource_request_id_t source_resource_request_id;

    // Below, we still need locks for request access, even despite the fact that access is done by unique id.
    // The reason is that otherwise access to request storage may be prevented from going to maintenance due to open
    // read accesses and it might potentially break request update logic if other compilation is finished right now.

    kan_atomic_int_lock (&state->execution_shared_state.concurrency_lock);
    KAN_UP_VALUE_READ (compiled_entry, resource_provider_compiled_resource_entry_t, name,
                       &compile_operation->target_name)
    {
        if (compiled_entry->type == compile_operation->target_type)
        {
            source_type = compiled_entry->source_type;
            pending_container_id = compiled_entry->pending_container_id;
            pending_compilation_index = compiled_entry->pending_compilation_index;
            source_resource_request_id = compiled_entry->source_resource_request_id;
            KAN_UP_QUERY_BREAK;
        }
    }

    if (!source_type)
    {
        kan_atomic_int_unlock (&state->execution_shared_state.concurrency_lock);
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Unable to find compiled entry for compile request \"%s\" of type \"%s\": internal error.",
                 compile_operation->target_name, compile_operation->target_type)
        return RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_DONE;
    }

    kan_resource_container_id_t input_container_id = KAN_TYPED_ID_32_INITIALIZE_INVALID;
    KAN_UP_VALUE_READ (input_request, kan_resource_request_t, request_id, &source_resource_request_id)
    {
        input_container_id = input_request->provided_container_id;
    }

    kan_atomic_int_unlock (&state->execution_shared_state.concurrency_lock);
    struct kan_repository_indexed_value_read_access_t input_access;
    const uint8_t *input_data = NULL;

    if (KAN_TYPED_ID_32_IS_VALID (input_container_id))
    {
        native_container_read (state, source_type, input_container_id, &input_access, &input_data);
    }

    struct kan_repository_indexed_value_update_access_t output_access;
    uint8_t *output_data = NULL;
    native_container_update (state, compile_operation->target_type, pending_container_id, &output_access, &output_data);

    struct kan_repository_indexed_value_update_access_t state_access;
    uint8_t *state_data = NULL;

    if (KAN_TYPED_ID_32_IS_VALID (compile_operation->compile.state_container_id))
    {
        native_container_update (state, compile_operation->compile.state_type_name,
                                 compile_operation->compile.state_container_id, &state_access, &state_data);
    }

    struct kan_dynamic_array_t dependencies;
    kan_dynamic_array_init (&dependencies, KAN_UNIVERSE_RESOURCE_PROVIDER_RC_INITIAL_SIZE,
                            sizeof (struct kan_resource_compilation_dependency_t),
                            _Alignof (struct kan_resource_compilation_dependency_t), state->my_allocation_group);

    struct kan_dynamic_array_t dependencies_accesses;
    kan_dynamic_array_init (&dependencies_accesses, KAN_UNIVERSE_RESOURCE_PROVIDER_RC_INITIAL_SIZE,
                            sizeof (struct kan_repository_indexed_value_read_access_t),
                            _Alignof (struct kan_repository_indexed_value_read_access_t), state->my_allocation_group);

    kan_atomic_int_lock (&state->execution_shared_state.concurrency_lock);
    KAN_UP_VALUE_READ (compilation_dependency, resource_provider_compilation_dependency_t, compiled_name,
                       &compile_operation->target_name)
    {
        if (compilation_dependency->compiled_type == compile_operation->target_type)
        {
            struct kan_resource_compilation_dependency_t *dependency = kan_dynamic_array_add_last (&dependencies);

            if (!dependency)
            {
                kan_dynamic_array_set_capacity (&dependencies, dependencies.size * 2u);
                dependency = kan_dynamic_array_add_last (&dependencies);
            }

            struct kan_repository_indexed_value_read_access_t *access =
                kan_dynamic_array_add_last (&dependencies_accesses);

            if (!access)
            {
                kan_dynamic_array_set_capacity (&dependencies_accesses, dependencies_accesses.size * 2u);
                access = kan_dynamic_array_add_last (&dependencies_accesses);
            }

            dependency->type = compilation_dependency->dependency_type;
            dependency->name = compilation_dependency->dependency_name;
            dependency->data = NULL;

            KAN_UP_VALUE_READ (dependency_request, kan_resource_request_t, request_id,
                               &compilation_dependency->request_id)
            {
                if (dependency_request->type)
                {
                    const uint8_t *data = NULL;
                    native_container_read (state, compilation_dependency->dependency_type,
                                           dependency_request->provided_container_id, access, &data);
                    dependency->data = data;
                }
                else
                {
                    dependency->data = dependency_request->provided_third_party.data;
                    dependency->data_size_if_third_party = dependency_request->provided_third_party.size;
                }
            }
        }
    }

    kan_atomic_int_unlock (&state->execution_shared_state.concurrency_lock);
    struct kan_reflection_struct_meta_iterator_t meta_iterator = kan_reflection_registry_query_struct_meta (
        state->reflection_registry, source_type, state->interned_kan_resource_compilable_meta_t);

    const struct kan_resource_compilable_meta_t *meta = kan_reflection_struct_meta_iterator_get (&meta_iterator);
    KAN_ASSERT (meta)
    const void *platform_configuration = NULL;

    if (meta->configuration_type_name)
    {
        platform_configuration = kan_resource_pipeline_system_query_platform_configuration (
            state->resource_pipeline_system, kan_string_intern (meta->configuration_type_name));

        if (!platform_configuration)
        {
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR, "Unable to find platform configuration of type \"%s\".",
                     meta->configuration_type_name)
        }
    }

    if (!input_data || !output_data ||
        (KAN_TYPED_ID_32_IS_VALID (compile_operation->compile.state_container_id) && !state_data) ||
        (meta->configuration_type_name && !platform_configuration))
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Internal error, failed to retrieve data for compilation state to compile \"%s\" of type \"%s\".",
                 compile_operation->target_name, compile_operation->target_type)

        if (input_data)
        {
            kan_repository_indexed_value_read_access_close (&input_access);
        }

        if (output_data)
        {
            kan_repository_indexed_value_update_access_close (&output_access);
        }

        if (state_data)
        {
            kan_repository_indexed_value_update_access_close (&state_access);
        }

        for (kan_loop_size_t index = 0u; index < dependencies.size; ++index)
        {
            struct kan_resource_compilation_dependency_t *dependency =
                &((struct kan_resource_compilation_dependency_t *) dependencies.data)[index];

            if (dependency->type && dependency->data)
            {
                struct kan_repository_indexed_value_read_access_t *access =
                    &((struct kan_repository_indexed_value_read_access_t *) dependencies_accesses.data)[index];
                kan_repository_indexed_value_read_access_close (access);
            }
        }

        kan_dynamic_array_shutdown (&dependencies);
        kan_dynamic_array_shutdown (&dependencies_accesses);

        KAN_UP_QUERY_RETURN_VALUE (enum resource_provider_serve_operation_status_t,
                                   RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_DONE);
    }

    struct resource_provider_compilation_interface_user_data_t interface_user_data = {
        .state = state,
        .compiled_entry_type = compile_operation->target_type,
        .compiled_entry_name = compile_operation->target_name,
        .pending_compilation_index = pending_compilation_index,
    };

    struct kan_resource_compile_state_t compilation_state = {
        .input_instance = input_data,
        .output_instance = output_data,
        .platform_configuration = platform_configuration,
        .deadline = state->execution_shared_state.end_time_ns,
        .user_state = state_data,
        .runtime_compilation = KAN_TRUE,
        .dependencies_count = dependencies.size,
        .dependencies = (struct kan_resource_compilation_dependency_t *) dependencies.data,
        .interface_user_data = (kan_functor_user_data_t) &interface_user_data,
        .register_byproduct = compilation_interface_register_byproduct,
        .register_unique_byproduct = compilation_interface_register_unique_byproduct,
        .name = compile_operation->target_name,
    };

    enum kan_resource_compile_result_t compile_result = meta->functor (&compilation_state);
    kan_repository_indexed_value_read_access_close (&input_access);
    kan_repository_indexed_value_update_access_close (&output_access);

    if (state_data)
    {
        kan_repository_indexed_value_update_access_close (&state_access);
    }

    for (kan_loop_size_t index = 0u; index < dependencies.size; ++index)
    {
        struct kan_resource_compilation_dependency_t *dependency =
            &((struct kan_resource_compilation_dependency_t *) dependencies.data)[index];

        if (dependency->type)
        {
            struct kan_repository_indexed_value_read_access_t *access =
                &((struct kan_repository_indexed_value_read_access_t *) dependencies_accesses.data)[index];
            kan_repository_indexed_value_read_access_close (access);
        }
    }

    kan_dynamic_array_shutdown (&dependencies);
    kan_dynamic_array_shutdown (&dependencies_accesses);
    enum resource_provider_serve_operation_status_t status = RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_IN_PROGRESS;

    switch (compile_result)
    {
    case KAN_RESOURCE_PIPELINE_COMPILE_IN_PROGRESS:
        status = RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_IN_PROGRESS;
        break;

    case KAN_RESOURCE_PIPELINE_COMPILE_FAILED:
    {
        kan_atomic_int_lock (&state->execution_shared_state.concurrency_lock);
        KAN_UP_VALUE_UPDATE (entry, resource_provider_compiled_resource_entry_t, name, &compile_operation->target_name)
        {
            if (entry->type == compile_operation->target_type)
            {
                runtime_compilation_delete_dependency_requests (state, entry);
                entry->compilation_state = RESOURCE_PROVIDER_COMPILATION_STATE_NOT_PENDING;
                remove_references_to_byproducts (state, entry->type, entry->name, entry->pending_compilation_index);
                native_container_delete (state, entry->type, entry->pending_container_id);
                entry->pending_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
                KAN_UP_QUERY_BREAK;
            }
        }

        kan_atomic_int_unlock (&state->execution_shared_state.concurrency_lock);
        status = RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_DONE;
        break;
    }

    case KAN_RESOURCE_PIPELINE_COMPILE_FINISHED:
    {
        kan_atomic_int_lock (&state->execution_shared_state.concurrency_lock);
        KAN_UP_VALUE_UPDATE (entry, resource_provider_compiled_resource_entry_t, name, &compile_operation->target_name)
        {
            if (entry->type == compile_operation->target_type)
            {
                runtime_compilation_delete_dependency_requests (state, entry);
                if (KAN_TYPED_ID_32_IS_VALID (entry->compiled_container_id))
                {
                    remove_references_to_byproducts (state, entry->type, entry->name, entry->current_compilation_index);
                    native_container_delete (state, entry->type, entry->compiled_container_id);
                }

                entry->compilation_state = RESOURCE_PROVIDER_COMPILATION_STATE_DONE;
                entry->compiled_container_id = entry->pending_container_id;
                entry->current_compilation_index = entry->pending_compilation_index;
                entry->pending_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
                update_requests (state, entry->type, entry->name, entry->compiled_container_id, NULL, 0u);
                KAN_UP_QUERY_BREAK;
            }
        }

        kan_atomic_int_unlock (&state->execution_shared_state.concurrency_lock);
        status = RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_DONE;
        break;
    }
    }

    KAN_UP_QUERY_RETURN_VALUE (enum resource_provider_serve_operation_status_t, status);
}

static inline void update_runtime_compilation_states_on_request_events (
    struct resource_provider_state_t *state,
    const struct kan_resource_provider_singleton_t *public,
    struct resource_provider_private_singleton_t *private)
{
    KAN_ASSERT (state->enable_runtime_compilation)
    kan_bool_t any_updates;

    struct kan_dynamic_array_t compiled_entries_to_update;
    kan_dynamic_array_init (&compiled_entries_to_update, KAN_UNIVERSE_RESOURCE_PROVIDER_RC_INITIAL_SIZE,
                            sizeof (struct resource_provider_type_name_pair_t),
                            _Alignof (struct resource_provider_type_name_pair_t), state->my_allocation_group);

    do
    {
        // Process events about updated requests in order to safely update compilation status for other entries.
        any_updates = KAN_FALSE;

        while (KAN_TRUE)
        {
            // We use plain query instead of autogenerated one because otherwise we would need to fetch events
            // even when runtime compilation is disabled.
            struct kan_repository_event_read_access_t access =
                kan_repository_event_fetch_query_next (&state->fetch_request_updated_events_for_runtime_compilation);

            const struct kan_resource_request_updated_event_t *event =
                kan_repository_event_read_access_resolve (&access);

            if (!event)
            {
                break;
            }

            KAN_UP_VALUE_READ (compiled_entry, resource_provider_compiled_resource_entry_t, source_resource_request_id,
                               &event->request_id)
            {
                dynamic_array_add_resource_provider_type_name_pair_unique (&compiled_entries_to_update,
                                                                           compiled_entry->type, compiled_entry->name);
            }

            KAN_UP_VALUE_READ (dependency, resource_provider_compilation_dependency_t, request_id, &event->request_id)
            {
                dynamic_array_add_resource_provider_type_name_pair_unique (
                    &compiled_entries_to_update, dependency->compiled_type, dependency->compiled_name);
            }

            kan_repository_event_read_access_close (&access);
        }

        for (kan_loop_size_t index = 0u; index < compiled_entries_to_update.size; ++index)
        {
            struct resource_provider_type_name_pair_t *pair =
                &((struct resource_provider_type_name_pair_t *) compiled_entries_to_update.data)[index];

            KAN_UP_VALUE_UPDATE (compiled_entry, resource_provider_compiled_resource_entry_t, name, &pair->name)
            {
                if (compiled_entry->type == pair->type)
                {
                    // Changes should be able to trigger recompilation.
                    if (compiled_entry->compilation_state == RESOURCE_PROVIDER_COMPILATION_STATE_DONE)
                    {
                        compiled_entry->compilation_state = RESOURCE_PROVIDER_COMPILATION_STATE_PENDING;
                    }

                    transition_compiled_entry_state (state, public, private, compiled_entry,
                                                     state->execution_shared_state.min_priority);

                    any_updates = KAN_TRUE;
                }
            }
        }

        _Static_assert (RESOURCE_PROVIDER_COMPILATION_STATE_PENDING == 1,
                        "Compilation state has expected signal value.");

        KAN_UP_SIGNAL_UPDATE (compiled_entry, resource_provider_compiled_resource_entry_t, compilation_state, 1)
        {
            transition_compiled_entry_state (state, public, private, compiled_entry,
                                             state->execution_shared_state.min_priority);
        }

        // Update requests after possible compiled entry state changed so we might get new transitions.
        process_request_on_insert (state, public, private);
        process_request_on_change (state, public, private);
        process_request_on_delete (state);

        compiled_entries_to_update.size = 0u;
    } while (any_updates);
    kan_dynamic_array_shutdown (&compiled_entries_to_update);
}

static void dispatch_shared_serve (struct resource_provider_state_t *state);

static void execute_shared_serve (kan_functor_user_data_t user_data)
{
    struct resource_provider_state_t *state = (struct resource_provider_state_t *) user_data;
    while (KAN_TRUE)
    {
        if (kan_precise_time_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
        {
            // Exit: no more time.
            break;
        }

        // Retrieve loading operation.
        kan_atomic_int_lock (&state->execution_shared_state.concurrency_lock);
        struct kan_repository_indexed_interval_write_access_t operation_access =
            kan_repository_indexed_interval_descending_write_cursor_next (
                &state->execution_shared_state.operation_cursor);
        kan_atomic_int_unlock (&state->execution_shared_state.concurrency_lock);

        struct resource_provider_operation_t *operation =
            kan_repository_indexed_interval_write_access_resolve (&operation_access);

        if (!operation)
        {
            // Exit: No more items.
            break;
        }

        enum resource_provider_serve_operation_status_t status = RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_IN_PROGRESS;
        switch (operation->operation_type)
        {
        case RESOURCE_PROVIDER_OPERATION_TYPE_LOAD:
            status = execute_shared_serve_load (state, operation);
            break;

        case RESOURCE_PROVIDER_OPERATION_TYPE_COMPILE:
            status = execute_shared_serve_compile (state, operation);
            break;
        }

        switch (status)
        {
        case RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_IN_PROGRESS:
            kan_repository_indexed_interval_write_access_close (&operation_access);
            break;

        case RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_DONE:
            switch (operation->operation_type)
            {
            case RESOURCE_PROVIDER_OPERATION_TYPE_LOAD:
                break;

            case RESOURCE_PROVIDER_OPERATION_TYPE_COMPILE:
                if (KAN_TYPED_ID_32_IS_VALID (operation->compile.state_container_id))
                {
                    native_container_delete (state, operation->compile.state_type_name,
                                             operation->compile.state_container_id);
                }

                break;
            }

            kan_repository_indexed_interval_write_access_delete (&operation_access);
            break;
        }
    }

    if (kan_atomic_int_add (&state->execution_shared_state.workers_left, -1) == 1)
    {
        kan_repository_indexed_interval_descending_write_cursor_close (&state->execution_shared_state.operation_cursor);

        if (state->enable_runtime_compilation)
        {
            KAN_UP_SINGLETON_READ (public, kan_resource_provider_singleton_t)
            KAN_UP_SINGLETON_WRITE (private, resource_provider_private_singleton_t)
            {
                update_runtime_compilation_states_on_request_events (state, public, private);
            }
        }

        // New operations might've been spawned and if we're using serve all at once,
        // then some of these operations might be interesting for us.
        // When using serve all at once and there are some new operations, we need to process them recursively too.
        if (state->execution_shared_state.min_priority >= PRIORITY_SERVE_ALL_AT_ONCE_SINCE)
        {
            KAN_ASSERT (state->execution_shared_state.end_time_ns == KAN_INT_MAX (kan_time_size_t))

            kan_bool_t should_run_again = KAN_FALSE;
            struct kan_repository_indexed_interval_descending_read_cursor_t check_cursor =
                kan_repository_indexed_interval_read_query_execute_descending (
                    &state->read_interval__resource_provider_operation__priority,
                    &state->execution_shared_state.min_priority, NULL);

            struct kan_repository_indexed_interval_read_access_t check_access =
                kan_repository_indexed_interval_descending_read_cursor_next (&check_cursor);

            if (kan_repository_indexed_interval_read_access_resolve (&check_access))
            {
                should_run_again = KAN_TRUE;
                kan_repository_indexed_interval_read_access_close (&check_access);
            }

            kan_repository_indexed_interval_descending_read_cursor_close (&check_cursor);
            if (should_run_again)
            {
                dispatch_shared_serve (state);
            }
        }
    }
}

static void dispatch_shared_serve (struct resource_provider_state_t *state)
{
    kan_stack_group_allocator_reset (&state->temporary_allocator);
    const kan_instance_size_t cpu_count = kan_platform_get_cpu_logical_core_count ();
    struct kan_cpu_task_list_node_t *task_list_node = NULL;

    state->execution_shared_state.workers_left = kan_atomic_int_init ((int) cpu_count);
    state->execution_shared_state.concurrency_lock = kan_atomic_int_init (0);
    state->execution_shared_state.byproduct_lock = kan_atomic_int_init (0);

    state->execution_shared_state.operation_cursor = kan_repository_indexed_interval_write_query_execute_descending (
        &state->write_interval__resource_provider_operation__priority, &state->execution_shared_state.min_priority,
        NULL);

    for (kan_loop_size_t worker_index = 0u; worker_index < cpu_count; ++worker_index)
    {
        KAN_CPU_TASK_LIST_USER_VALUE (&task_list_node, &state->temporary_allocator,
                                      state->interned_resource_provider_server, execute_shared_serve, state)
    }

    kan_cpu_job_dispatch_and_detach_task_list (state->execution_shared_state.job, task_list_node);
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void mutator_template_execute_resource_provider (
    kan_cpu_job_t job, struct resource_provider_state_t *state)
{
    state->frame_begin_time_ns = kan_precise_time_get_elapsed_nanoseconds ();
    kan_bool_t execute_dispatch_shared_serve = KAN_FALSE;

    KAN_UP_SINGLETON_WRITE (public, kan_resource_provider_singleton_t)
    KAN_UP_SINGLETON_WRITE (private, resource_provider_private_singleton_t)
    {
        if (private->status == RESOURCE_PROVIDER_STATUS_NOT_INITIALIZED)
        {
            prepare_for_scanning (state, private);
            private->status = RESOURCE_PROVIDER_STATUS_SCANNING;
        }

        if (private->status == RESOURCE_PROVIDER_STATUS_SCANNING &&
            state->frame_begin_time_ns + state->scan_budget_ns > kan_precise_time_get_elapsed_nanoseconds ())
        {
            kan_virtual_file_system_volume_t volume =
                kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);

            while (private->scan_item_stack.size > 0u &&
                   state->frame_begin_time_ns + state->scan_budget_ns > kan_precise_time_get_elapsed_nanoseconds ())
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
            state->frame_begin_time_ns + state->serve_budget_ns > kan_precise_time_get_elapsed_nanoseconds ())
        {
            if (state->enable_runtime_compilation)
            {
                const kan_bool_t platform_configuration_changed =
                    kan_resource_pipeline_system_platform_configuration_listener_consume (
                        state->platform_configuration_change_listener);

                if (state->need_to_restart_runtime_compilation || platform_configuration_changed)
                {
                    // Reset every active compilation on reflection frame. We have to do it as reflection might've been
                    // changed and therefore dependencies and compilation functors might've been changed too.

                    const kan_instance_size_t min_priority =
                        KAN_HANDLE_IS_VALID (state->hot_reload_system) &&
                                kan_hot_reload_coordination_system_is_hot_swap (state->hot_reload_system) ?
                            PRIORITY_HOT_SWAP :
                            0u;

                    KAN_UP_SEQUENCE_UPDATE (compiled_entry, resource_provider_compiled_resource_entry_t)
                    {
                        switch (compiled_entry->compilation_state)
                        {
                        case RESOURCE_PROVIDER_COMPILATION_STATE_NOT_PENDING:
                        case RESOURCE_PROVIDER_COMPILATION_STATE_PENDING:
                        case RESOURCE_PROVIDER_COMPILATION_STATE_WAITING_FOR_SOURCE:
                            break;

                        case RESOURCE_PROVIDER_COMPILATION_STATE_WAITING_FOR_DEPENDENCIES:
                        case RESOURCE_PROVIDER_COMPILATION_STATE_COMPILING:
                            cancel_runtime_compilation (state, compiled_entry);
                            bootstrap_pending_compilation (state, public, compiled_entry, min_priority);
                            break;

                        case RESOURCE_PROVIDER_COMPILATION_STATE_DONE:
                            if (platform_configuration_changed)
                            {
                                bootstrap_pending_compilation (state, public, compiled_entry, min_priority);
                            }

                            break;
                        }
                    }

                    state->need_to_restart_runtime_compilation = KAN_FALSE;
                }
            }

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

            KAN_UP_EVENT_FETCH (sleep_event, kan_resource_request_defer_sleep_event_t)
            {
                if (KAN_HANDLE_IS_VALID (state->hot_reload_system) &&
                    kan_hot_reload_coordination_system_get_current_mode (state->hot_reload_system) !=
                        KAN_HOT_RELOAD_MODE_DISABLED)
                {
                    kan_interned_string_t type_to_remove_request = NULL;
                    kan_interned_string_t name_to_remove_request = NULL;

                    KAN_UP_VALUE_UPDATE (request, kan_resource_request_t, request_id, &sleep_event->request_id)
                    {
                        // Sleeping only makes sense when there is no pending operation already.
                        if (!request->expecting_new_data)
                        {
                            request->sleeping = KAN_TRUE;
                            // Cannot do it inside request update access as reference removal accesses requests.
                            type_to_remove_request = request->type;
                            name_to_remove_request = request->name;
                        }
                    }

                    if (name_to_remove_request)
                    {
                        if (type_to_remove_request)
                        {
                            remove_native_entry_reference (state, type_to_remove_request, name_to_remove_request);
                        }
                        else
                        {
                            remove_third_party_entry_reference (state, name_to_remove_request);
                        }
                    }
                }
                else
                {
                    // No need to implement sleep when there is no hot reload, just delete the sleeping event.
                    KAN_UP_VALUE_DELETE (request, kan_resource_request_t, request_id, &sleep_event->request_id)
                    {
                        KAN_UP_ACCESS_DELETE (request);
                    }
                }
            }

            KAN_UP_EVENT_FETCH (delete_event, kan_resource_request_defer_delete_event_t)
            {
                KAN_UP_VALUE_DELETE (request, kan_resource_request_t, request_id, &delete_event->request_id)
                {
                    KAN_UP_ACCESS_DELETE (request);
                }
            }

            state->execution_shared_state.job = job;
            if (KAN_HANDLE_IS_VALID (state->hot_reload_system) &&
                kan_hot_reload_coordination_system_is_hot_swap (state->hot_reload_system))
            {
                state->execution_shared_state.min_priority = PRIORITY_HOT_SWAP;
                state->execution_shared_state.end_time_ns = KAN_INT_MAX (kan_time_size_t);
            }
            else
            {
                state->execution_shared_state.min_priority = 0u;
                state->execution_shared_state.end_time_ns = state->frame_begin_time_ns + state->serve_budget_ns;
            }

            process_request_on_insert (state, public, private);
            process_request_on_change (state, public, private);
            process_request_on_delete (state);

            if (KAN_HANDLE_IS_VALID (private->resource_watcher))
            {
                process_delayed_addition (state, public, private);
                process_delayed_reload (state, public, private);
            }

            if (state->enable_runtime_compilation)
            {
                update_runtime_compilation_states_on_request_events (state, public, private);
            }

            execute_dispatch_shared_serve = KAN_TRUE;
        }
    }

    // We need to call dispatch outside of singleton access scope to prevent race conditions in singleton access.
    if (execute_dispatch_shared_serve)
    {
        dispatch_shared_serve (state);
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

    if (state->enable_runtime_compilation)
    {
        kan_repository_event_fetch_query_shutdown (&state->fetch_request_updated_events_for_runtime_compilation);
        kan_resource_pipeline_system_remove_platform_configuration_change_listener (
            state->resource_pipeline_system, state->platform_configuration_change_listener);
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
    instance->interned_kan_resource_compilable_meta_t = kan_string_intern ("kan_resource_compilable_meta_t");
    instance->interned_kan_resource_byproduct_type_meta_t = kan_string_intern ("kan_resource_byproduct_type_meta_t");
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

static inline void reflection_generation_iteration_add_container_for_type (
    struct kan_reflection_generator_universe_resource_provider_t *instance,
    kan_reflection_registry_t registry,
    const struct kan_reflection_struct_t *type,
    enum resource_provider_native_container_type_source_t source,
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
    snprintf (buffer, MAX_GENERATED_NAME_LENGTH, KAN_RESOURCE_PROVIDER_CONTAINER_TYPE_FORMAT, type->name);
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

    node->source = source;
    node->compilable = NULL;
    node->compiled_from = NULL;

    switch (node->source)
    {
    case RESOURCE_PROVIDER_NATIVE_CONTAINER_TYPE_SOURCE_RESOURCE_TYPE:
    case RESOURCE_PROVIDER_NATIVE_CONTAINER_TYPE_SOURCE_BYPRODUCT_TYPE:
    {
        struct kan_reflection_struct_meta_iterator_t meta_iterator = kan_reflection_registry_query_struct_meta (
            registry, type->name, instance->interned_kan_resource_compilable_meta_t);
        node->compilable = kan_reflection_struct_meta_iterator_get (&meta_iterator);
        break;
    }

    case RESOURCE_PROVIDER_NATIVE_CONTAINER_TYPE_SOURCE_COMPILATION_STATE:
        break;
    }

    kan_reflection_system_generation_iterator_add_struct (generation_iterator, &node->type);
    if (node->compilable && node->compilable->state_type_name)
    {
        const struct kan_reflection_struct_t *state_type =
            kan_reflection_registry_query_struct (registry, kan_string_intern (node->compilable->state_type_name));

        if (state_type)
        {
            reflection_generation_iteration_add_container_for_type (
                instance, registry, state_type, RESOURCE_PROVIDER_NATIVE_CONTAINER_TYPE_SOURCE_COMPILATION_STATE,
                generation_iterator);
        }
    }
}

static inline kan_bool_t kan_reflection_generator_universe_resource_provider_check_is_type_already_added (
    struct kan_reflection_generator_universe_resource_provider_t *instance, kan_interned_string_t type_name)
{
    struct universe_resource_provider_generated_container_type_node_t *node = instance->first_container_type;
    while (node)
    {
        if (node->source_type->name == type_name)
        {
            return KAN_TRUE;
        }

        node = node->next;
    }

    return KAN_FALSE;
}

UNIVERSE_RESOURCE_PROVIDER_KAN_API void kan_reflection_generator_universe_resource_provider_iterate (
    struct kan_reflection_generator_universe_resource_provider_t *instance,
    kan_reflection_registry_t registry,
    kan_reflection_system_generation_iterator_t iterator,
    kan_loop_size_t iteration_index)
{
    if (iteration_index == instance->boostrap_iteration)
    {
        kan_reflection_registry_struct_iterator_t struct_iterator =
            kan_reflection_registry_struct_iterator_create (registry);
        const struct kan_reflection_struct_t *type;

        while ((type = kan_reflection_registry_struct_iterator_get (struct_iterator)))
        {
            struct kan_reflection_struct_meta_iterator_t meta_iterator = kan_reflection_registry_query_struct_meta (
                registry, type->name, instance->interned_kan_resource_resource_type_meta_t);

            if (kan_reflection_struct_meta_iterator_get (&meta_iterator))
            {
                reflection_generation_iteration_add_container_for_type (
                    instance, registry, type, RESOURCE_PROVIDER_NATIVE_CONTAINER_TYPE_SOURCE_RESOURCE_TYPE, iterator);
            }

            meta_iterator = kan_reflection_registry_query_struct_meta (
                registry, type->name, instance->interned_kan_resource_byproduct_type_meta_t);

            if (kan_reflection_struct_meta_iterator_get (&meta_iterator))
            {
                reflection_generation_iteration_add_container_for_type (
                    instance, registry, type, RESOURCE_PROVIDER_NATIVE_CONTAINER_TYPE_SOURCE_RESOURCE_TYPE, iterator);
            }

            struct_iterator = kan_reflection_registry_struct_iterator_next (struct_iterator);
        }

        return;
    }

    kan_interned_string_t type_name;
    while ((type_name = kan_reflection_system_generation_iterator_next_added_struct (iterator)))
    {
        const struct kan_reflection_struct_t *type = kan_reflection_registry_query_struct (registry, type_name);
        if (type)
        {
            struct kan_reflection_struct_meta_iterator_t meta_iterator = kan_reflection_registry_query_struct_meta (
                registry, type->name, instance->interned_kan_resource_resource_type_meta_t);

            if (kan_reflection_struct_meta_iterator_get (&meta_iterator))
            {
                reflection_generation_iteration_add_container_for_type (
                    instance, registry, type, RESOURCE_PROVIDER_NATIVE_CONTAINER_TYPE_SOURCE_RESOURCE_TYPE, iterator);
            }

            meta_iterator = kan_reflection_registry_query_struct_meta (
                registry, type->name, instance->interned_kan_resource_byproduct_type_meta_t);

            if (kan_reflection_struct_meta_iterator_get (&meta_iterator))
            {
                reflection_generation_iteration_add_container_for_type (
                    instance, registry, type, RESOURCE_PROVIDER_NATIVE_CONTAINER_TYPE_SOURCE_RESOURCE_TYPE, iterator);
            }
        }
    }

    struct kan_reflection_system_added_struct_meta_t added_meta;
    while ((added_meta = kan_reflection_system_generation_iterator_next_added_struct_meta (iterator)).struct_name)
    {
        if ((added_meta.meta_type_name == instance->interned_kan_resource_resource_type_meta_t ||
             added_meta.meta_type_name == instance->interned_kan_resource_byproduct_type_meta_t) &&
            !kan_reflection_generator_universe_resource_provider_check_is_type_already_added (instance,
                                                                                              added_meta.struct_name))
        {
            const struct kan_reflection_struct_t *type =
                kan_reflection_registry_query_struct (registry, added_meta.struct_name);

            if (type)
            {
                reflection_generation_iteration_add_container_for_type (
                    instance, registry, type,
                    added_meta.meta_type_name == instance->interned_kan_resource_resource_type_meta_t ?
                        RESOURCE_PROVIDER_NATIVE_CONTAINER_TYPE_SOURCE_RESOURCE_TYPE :
                        RESOURCE_PROVIDER_NATIVE_CONTAINER_TYPE_SOURCE_BYPRODUCT_TYPE,
                    iterator);
            }
        }
        else if (added_meta.meta_type_name == instance->interned_kan_resource_compilable_meta_t)
        {
            struct kan_reflection_struct_meta_iterator_t meta_iterator = kan_reflection_registry_query_struct_meta (
                registry, added_meta.struct_name, instance->interned_kan_resource_compilable_meta_t);
            const struct kan_resource_compilable_meta_t *compilable =
                kan_reflection_struct_meta_iterator_get (&meta_iterator);

            if (compilable)
            {
                const struct kan_reflection_struct_t *type =
                    kan_reflection_registry_query_struct (registry, kan_string_intern (compilable->state_type_name));

                if (type && !kan_reflection_generator_universe_resource_provider_check_is_type_already_added (
                                instance, type->name))
                {
                    reflection_generation_iteration_add_container_for_type (
                        instance, registry, type, RESOURCE_PROVIDER_NATIVE_CONTAINER_TYPE_SOURCE_COMPILATION_STATE,
                        iterator);
                }
            }
        }
    }
}

static inline void generated_mutator_init_node (
    struct resource_provider_native_container_type_data_t *mutator_node,
    struct universe_resource_provider_generated_container_type_node_t *generated_node)
{
    mutator_node->contained_type_name = generated_node->source_type->name;
    mutator_node->container_type_name = generated_node->type.name;
    mutator_node->contained_type_alignment = generated_node->source_type->alignment;

    mutator_node->source = generated_node->source;
    mutator_node->compiled_from = generated_node->compiled_from;
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
    kan_repository_indexed_value_read_query_init (&node->read_by_id_query, storage, container_id_path);
    kan_repository_indexed_value_update_query_init (&node->update_by_id_query, storage, container_id_path);
    kan_repository_indexed_value_delete_query_init (&node->delete_by_id_query, storage, container_id_path);
}

static inline void generated_mutator_undeploy_node (struct resource_provider_native_container_type_data_t *node)
{
    kan_repository_indexed_insert_query_shutdown (&node->insert_query);
    kan_repository_indexed_value_read_query_shutdown (&node->read_by_id_query);
    kan_repository_indexed_value_update_query_shutdown (&node->update_by_id_query);
    kan_repository_indexed_value_delete_query_shutdown (&node->delete_by_id_query);
}

static inline void generated_mutator_shutdown_node (struct resource_provider_native_container_type_data_t *node)
{
}

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

    struct universe_resource_provider_generated_container_type_node_t *node = instance->first_container_type;
    while (node)
    {
        if (node->compilable)
        {
            struct universe_resource_provider_generated_container_type_node_t *other_node =
                instance->first_container_type;
            while (other_node)
            {
                if (strcmp (other_node->source_type->name, node->compilable->output_type_name) == 0)
                {
                    other_node->compiled_from = node->source_type->name;
                    break;
                }

                other_node = other_node->next;
            }
        }

        node = node->next;
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
    instance->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
    instance->type = NULL;
    instance->name = NULL;
    instance->priority = KAN_RESOURCE_PROVIDER_USER_PRIORITY_MIN;
    instance->expecting_new_data = KAN_FALSE;
    instance->sleeping = KAN_FALSE;
    instance->provided_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
    instance->provided_third_party.data = NULL;
    instance->provided_third_party.size = 0u;
}

void kan_resource_provider_singleton_init (struct kan_resource_provider_singleton_t *instance)
{
    instance->request_id_counter = kan_atomic_int_init (1);
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

KAN_REFLECTION_STRUCT_META (kan_resource_native_entry_t)
UNIVERSE_RESOURCE_PROVIDER_KAN_API struct kan_repository_meta_automatic_on_insert_event_t
    kan_resource_native_entry_on_insert = {
        .event_type = "kan_resource_native_entry_on_insert_event_t",
        .copy_outs_count = 2u,
        .copy_outs =
            (struct kan_repository_copy_out_t[]) {
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

KAN_REFLECTION_STRUCT_META (kan_resource_native_entry_t)
UNIVERSE_RESOURCE_PROVIDER_KAN_API struct kan_repository_meta_automatic_on_delete_event_t
    kan_resource_native_entry_on_delete = {
        .event_type = "kan_resource_native_entry_on_delete_event_t",
        .copy_outs_count = 2u,
        .copy_outs =
            (struct kan_repository_copy_out_t[]) {
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

KAN_REFLECTION_STRUCT_META (kan_resource_third_party_entry_t)
UNIVERSE_RESOURCE_PROVIDER_KAN_API struct kan_repository_meta_automatic_on_insert_event_t
    kan_resource_third_party_entry_on_insert = {
        .event_type = "kan_resource_third_party_entry_on_insert_event_t",
        .copy_outs_count = 1u,
        .copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                    .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                },
            },
};

KAN_REFLECTION_STRUCT_META (kan_resource_third_party_entry_t)
UNIVERSE_RESOURCE_PROVIDER_KAN_API struct kan_repository_meta_automatic_on_delete_event_t
    kan_resource_third_party_entry_on_delete = {
        .event_type = "kan_resource_third_party_entry_on_delete_event_t",
        .copy_outs_count = 1u,
        .copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                    .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                },
            },
};
