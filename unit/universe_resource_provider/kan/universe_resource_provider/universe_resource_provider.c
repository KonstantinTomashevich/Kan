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
#include <kan/resource_pipeline/common_meta.h>
#include <kan/resource_pipeline/index.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>
#include <kan/universe/macro.h>
#include <kan/universe/reflection_system_generator_helpers.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>
#include <kan/virtual_file_system/virtual_file_system.h>

KAN_LOG_DEFINE_CATEGORY (universe_resource_provider);
KAN_USE_STATIC_INTERNED_IDS
KAN_USE_STATIC_CPU_SECTIONS

UNIVERSE_RESOURCE_PROVIDER_API KAN_UM_MUTATOR_GROUP_META (resource_provider, KAN_RESOURCE_PROVIDER_MUTATOR_GROUP);

struct resource_provider_private_singleton_t
{
    kan_instance_size_t entry_id_counter;
    struct kan_atomic_int_t container_id_counter;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_serialization_interned_string_registry_t)
    struct kan_dynamic_array_t loaded_string_registries;

    kan_virtual_file_system_watcher_t resource_watcher;
    kan_virtual_file_system_watcher_iterator_t resource_watcher_iterator;
};

UNIVERSE_RESOURCE_PROVIDER_API void resource_provider_private_singleton_init (
    struct resource_provider_private_singleton_t *instance)
{
    instance->entry_id_counter = KAN_TYPED_ID_32_INVALID_LITERAL;
    instance->container_id_counter = kan_atomic_int_init (KAN_TYPED_ID_32_INVALID_LITERAL + 1u);

    kan_dynamic_array_init (&instance->loaded_string_registries, 0u,
                            sizeof (kan_serialization_interned_string_registry_t),
                            alignof (kan_serialization_interned_string_registry_t), kan_allocation_group_stack_get ());

    instance->resource_watcher = KAN_HANDLE_SET_INVALID (kan_virtual_file_system_watcher_t);
    instance->resource_watcher_iterator = KAN_HANDLE_SET_INVALID (kan_virtual_file_system_watcher_iterator_t);
}

UNIVERSE_RESOURCE_PROVIDER_API void resource_provider_private_singleton_shutdown (
    struct resource_provider_private_singleton_t *instance)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS (instance->loaded_string_registries,
                                           kan_serialization_interned_string_registry_t)
    {
        kan_serialization_interned_string_registry_destroy (*value);
    }

    if (KAN_HANDLE_IS_VALID (instance->resource_watcher))
    {
        if (KAN_HANDLE_IS_VALID (instance->resource_watcher_iterator))
        {
            kan_virtual_file_system_watcher_iterator_destroy (instance->resource_watcher,
                                                              instance->resource_watcher_iterator);
        }

        kan_virtual_file_system_watcher_destroy (instance->resource_watcher);
    }
}

struct resource_usage_on_insert_event_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
};

KAN_REFLECTION_STRUCT_META (kan_resource_usage_t)
UNIVERSE_RESOURCE_PROVIDER_API struct kan_repository_meta_automatic_on_insert_event_t resource_usage_on_insert = {
    .event_type = "resource_usage_on_insert_event_t",
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

struct resource_usage_on_delete_event_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
};

KAN_REFLECTION_STRUCT_META (kan_resource_usage_t)
UNIVERSE_RESOURCE_PROVIDER_API struct kan_repository_meta_automatic_on_delete_event_t resource_usage_on_delete = {
    .event_type = "resource_usage_on_delete_event_t",
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

struct resource_provider_delayed_file_addition_t
{
    kan_hash_t path_hash;
    char *path;
    kan_packed_timer_t investigate_after_timer;
    kan_allocation_group_t my_allocation_group;
};

UNIVERSE_RESOURCE_PROVIDER_API void resource_provider_delayed_file_addition_init (
    struct resource_provider_delayed_file_addition_t *instance)
{
    instance->path_hash = 0u;
    instance->path = NULL;
    instance->investigate_after_timer = KAN_PACKED_TIMER_NEVER;
    instance->my_allocation_group = kan_allocation_group_stack_get ();
}

UNIVERSE_RESOURCE_PROVIDER_API void resource_provider_delayed_file_addition_shutdown (
    struct resource_provider_delayed_file_addition_t *instance)
{
    kan_free_general (instance->my_allocation_group, instance->path, strlen (instance->path) + 1u);
}

struct resource_provider_operation_t
{
    kan_resource_entry_id_t entry_id;
    kan_instance_size_t priority;

    /// \details We don't need generic entry access and we could go to typed entry from loading function right away.
    ///          Therefore, we need to cache type here to avoid getting type from generic entry.
    kan_interned_string_t type;

    struct kan_stream_t *stream;
    kan_reflection_registry_t used_registry;
    kan_serialization_binary_reader_t binary_reader;
};

UNIVERSE_RESOURCE_PROVIDER_API void resource_provider_operation_init (struct resource_provider_operation_t *instance)
{
    instance->entry_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_entry_id_t);
    instance->priority = 0u;
    instance->type = NULL;

    instance->stream = NULL;
    instance->used_registry = KAN_HANDLE_SET_INVALID (kan_reflection_registry_t);
    instance->binary_reader = KAN_HANDLE_SET_INVALID (kan_serialization_binary_reader_t);
}

UNIVERSE_RESOURCE_PROVIDER_API void resource_provider_operation_shutdown (
    struct resource_provider_operation_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->binary_reader))
    {
        kan_serialization_binary_reader_destroy (instance->binary_reader);
    }

    if (instance->stream)
    {
        instance->stream->operations->close (instance->stream);
    }
}

KAN_REFLECTION_IGNORE
struct universe_resource_provider_generated_node_t
{
    struct universe_resource_provider_generated_node_t *next;
    const struct kan_reflection_struct_t *source_resource_type;
    struct kan_reflection_struct_t typed_entry_type;
    struct kan_reflection_struct_t container_type;
    struct kan_reflection_struct_t registered_event_type;
    struct kan_reflection_struct_t loaded_event_type;
};

struct kan_reflection_generator_universe_resource_provider_t
{
    kan_loop_size_t boostrap_iteration;
    kan_allocation_group_t generated_reflection_group;
    struct universe_resource_provider_generated_node_t *first_node;
    kan_instance_size_t nodes_count;

    KAN_REFLECTION_IGNORE
    struct kan_reflection_struct_t mutator_type;

    KAN_REFLECTION_IGNORE
    struct kan_reflection_function_t mutator_deploy_function;

    KAN_REFLECTION_IGNORE
    struct kan_reflection_function_t mutator_execute_function;

    KAN_REFLECTION_IGNORE
    struct kan_reflection_function_t mutator_undeploy_function;
};

struct resource_provider_resource_type_interface_t
{
    kan_interned_string_t resource_type_name;

    struct kan_repository_indexed_insert_query_t insert_typed_entry;
    struct kan_repository_indexed_value_update_query_t update_typed_entry_by_id;

    struct kan_repository_indexed_insert_query_t insert_container;
    struct kan_repository_indexed_value_update_query_t update_container_by_id;
    struct kan_repository_indexed_value_delete_query_t delete_container_by_id;

    struct kan_repository_event_insert_query_t insert_registered_event;

    struct kan_repository_event_insert_query_t insert_loaded_event;

    struct universe_resource_provider_generated_node_t *source_node;
};

KAN_REFLECTION_IGNORE
struct resource_provider_execution_shared_state_t
{
    struct kan_atomic_int_t workers_left;
    struct kan_atomic_int_t concurrency_lock;
    struct kan_repository_indexed_interval_descending_write_cursor_t operation_cursor;
    kan_time_size_t end_time_ns;

    /// \brief Private and private write access are shared between everyone exclusively for id counter usage.
    struct kan_repository_singleton_write_access_t private_access;

    struct resource_provider_private_singleton_t *private;
    kan_cpu_job_t job;
};

struct resource_provider_state_t
{
    kan_allocation_group_t my_allocation_group;
    kan_time_offset_t serve_budget_ns;
    kan_interned_string_t resource_directory_path;

    kan_reflection_registry_t reflection_registry;
    kan_serialization_binary_script_storage_t shared_script_storage;
    kan_context_system_t hot_reload_system;
    kan_context_system_t virtual_file_system;

    KAN_UM_GENERATE_STATE_QUERIES (resource_provider)
    KAN_UM_BIND_STATE (resource_provider, state)

    struct kan_repository_indexed_interval_write_query_t write_interval__resource_provider_operation__priority;

    KAN_REFLECTION_IGNORE
    struct resource_provider_execution_shared_state_t execution_shared_state;

    KAN_REFLECTION_IGNORE
    struct kan_stack_group_allocator_t temporary_allocator;

    kan_instance_size_t trailing_data_count;

    KAN_REFLECTION_IGNORE
    struct resource_provider_resource_type_interface_t trailing_data[];
};

UNIVERSE_RESOURCE_PROVIDER_API void resource_provider_state_init (struct resource_provider_state_t *instance)
{
    instance->my_allocation_group = kan_allocation_group_stack_get ();
    instance->serve_budget_ns = 0u;
    instance->resource_directory_path = NULL;

    kan_stack_group_allocator_init (&instance->temporary_allocator,
                                    kan_allocation_group_get_child (instance->my_allocation_group, "temporary"),
                                    KAN_UNIVERSE_RESOURCE_PROVIDER_TEMPORARY_CHUNK_SIZE);
}

UNIVERSE_RESOURCE_PROVIDER_API void resource_provider_state_shutdown (struct resource_provider_state_t *instance)
{
    kan_stack_group_allocator_shutdown (&instance->temporary_allocator);
}

static inline struct resource_provider_resource_type_interface_t *query_resource_type_interface (
    struct resource_provider_state_t *state, kan_interned_string_t type)
{
    KAN_UNIVERSE_REFLECTION_GENERATOR_FIND_GENERATED_STATE (struct resource_provider_resource_type_interface_t,
                                                            resource_type_name, type);
}

static inline struct kan_repository_indexed_value_update_access_t update_typed_resource_entry (
    struct resource_provider_resource_type_interface_t *interface, kan_resource_entry_id_t entry_id)
{
    struct kan_repository_indexed_value_update_cursor_t cursor =
        kan_repository_indexed_value_update_query_execute (&interface->update_typed_entry_by_id, &entry_id);
    CUSHION_DEFER { kan_repository_indexed_value_update_cursor_close (&cursor); }
    return kan_repository_indexed_value_update_cursor_next (&cursor);
}

static inline struct kan_repository_indexed_value_update_access_t update_container_by_id (
    struct resource_provider_resource_type_interface_t *interface, kan_resource_container_id_t container_id)
{
    struct kan_repository_indexed_value_update_cursor_t cursor =
        kan_repository_indexed_value_update_query_execute (&interface->update_container_by_id, &container_id);
    CUSHION_DEFER { kan_repository_indexed_value_update_cursor_close (&cursor); }
    return kan_repository_indexed_value_update_cursor_next (&cursor);
}

static void delete_container_by_id (struct resource_provider_resource_type_interface_t *interface,
                                    kan_resource_container_id_t container_id)
{
    struct kan_repository_indexed_value_delete_cursor_t cursor =
        kan_repository_indexed_value_delete_query_execute (&interface->delete_container_by_id, &container_id);
    CUSHION_DEFER { kan_repository_indexed_value_delete_cursor_close (&cursor); }

    struct kan_repository_indexed_value_delete_access_t access =
        kan_repository_indexed_value_delete_cursor_next (&cursor);

    if (kan_repository_indexed_value_delete_access_resolve (&access))
    {
        kan_repository_indexed_value_delete_access_delete (&access);
    }
}

UNIVERSE_RESOURCE_PROVIDER_API KAN_UM_MUTATOR_DEPLOY_SIGNATURE (mutator_template_deploy_resource_provider,
                                                                resource_provider_state_t)
{
    kan_static_interned_ids_ensure_initialized ();
    kan_cpu_static_sections_ensure_initialized ();

    const struct kan_resource_provider_configuration_t *configuration =
        kan_universe_world_query_configuration (world, kan_string_intern (KAN_RESOURCE_PROVIDER_CONFIGURATION));
    KAN_ASSERT (configuration)

    state->serve_budget_ns = configuration->serve_budget_ns;
    state->resource_directory_path = configuration->resource_directory_path;

    state->reflection_registry = kan_universe_get_reflection_registry (universe);
    state->shared_script_storage = kan_serialization_binary_script_storage_create (state->reflection_registry);

    state->hot_reload_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_NAME);

    state->virtual_file_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_VIRTUAL_FILE_SYSTEM_NAME);
    KAN_ASSERT (KAN_HANDLE_IS_VALID (state->virtual_file_system))

    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
}

static kan_resource_entry_id_t register_new_entry (struct resource_provider_state_t *state,
                                                   struct resource_provider_private_singleton_t *private,
                                                   kan_interned_string_t type,
                                                   kan_interned_string_t name,
                                                   const char *path,
                                                   kan_serialization_interned_string_registry_t string_registry)
{
    struct resource_provider_resource_type_interface_t *interface = query_resource_type_interface (state, type);
    if (!interface)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Failed to insert entry \"%s\" of type \"%s\" from path \"%s\": given type is not a known resource "
                 "type, check meta.",
                 name, type, path)
        return KAN_TYPED_ID_32_SET_INVALID (kan_resource_entry_id_t);
    }

    kan_resource_entry_id_t entry_id = KAN_TYPED_ID_32_SET (kan_resource_entry_id_t, ++private->entry_id_counter);
    KAN_UMO_INDEXED_INSERT (generic, kan_resource_generic_entry_t)
    {
        generic->entry_id = entry_id;
        generic->type = type;
        generic->name = name;

        const kan_instance_size_t path_length = (kan_instance_size_t) strlen (path);
        generic->path = kan_allocate_general (generic->my_allocation_group, path_length + 1u, alignof (char));
        memcpy (generic->path, path, path_length + 1u);
        generic->path_hash = kan_string_hash (generic->path);
    }

    struct kan_repository_indexed_insertion_package_t insert_typed =
        kan_repository_indexed_insert_query_execute (&interface->insert_typed_entry);
    CUSHION_DEFER { kan_repository_indexed_insertion_package_submit (&insert_typed); }

    struct kan_resource_typed_entry_view_t *typed = kan_repository_indexed_insertion_package_get (&insert_typed);
    typed->entry_id = entry_id;
    typed->name = name;

    typed->loaded_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
    typed->loading_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
    typed->bound_to_string_registry = string_registry;

    struct kan_repository_event_insertion_package_t insert_event =
        kan_repository_event_insert_query_execute (&interface->insert_registered_event);
    struct kan_resource_registered_event_view_t *event = kan_repository_event_insertion_package_get (&insert_event);

    if (event)
    {
        event->entry_id = entry_id;
        event->name = name;
        kan_repository_event_insertion_package_submit (&insert_event);
    }

    return entry_id;
}

static void register_new_entry_with_duplication_check (struct resource_provider_state_t *state,
                                                       struct resource_provider_private_singleton_t *private,
                                                       kan_interned_string_t type,
                                                       kan_interned_string_t name,
                                                       const char *path,
                                                       kan_serialization_interned_string_registry_t string_registry)
{
    KAN_UML_VALUE_READ (potential_duplicate, kan_resource_generic_entry_t, name, &name)
    {
        if (potential_duplicate->type == type)
        {
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                     "Failed to insert entry \"%s\" of type \"%s\" from path \"%s\" due to name collision.", name, type,
                     path)
            return;
        }
    }

    register_new_entry (state, private, type, name, path, string_registry);
}

static bool load_directory_resource_index_if_any (struct resource_provider_state_t *state,
                                                  struct resource_provider_private_singleton_t *private,
                                                  kan_virtual_file_system_volume_t volume,
                                                  struct kan_file_system_path_container_t *path_container)
{
    const kan_instance_size_t base_length = path_container->length;
    CUSHION_DEFER { kan_file_system_path_container_reset_length (path_container, base_length); }
    kan_file_system_path_container_append (path_container, KAN_RESOURCE_INDEX_DEFAULT_NAME);

    if (!kan_virtual_file_system_check_existence (volume, path_container->path))
    {
        return false;
    }

    kan_serialization_interned_string_registry_t string_registry = KAN_HANDLE_INITIALIZE_INVALID;
    kan_file_system_path_container_reset_length (path_container, base_length);
    kan_file_system_path_container_append (path_container,
                                           KAN_RESOURCE_INDEX_ACCOMPANYING_STRING_REGISTRY_DEFAULT_NAME);

    if (kan_virtual_file_system_check_existence (volume, path_container->path))
    {
        struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (volume, path_container->path);
        if (!stream)
        {
            KAN_LOG (
                universe_resource_provider, KAN_LOG_ERROR,
                "Failed to read index accompanying string registry at virtual path \"%s\": unable to open read stream.",
                path_container->path)

            // We still have index, but failed to decode it.
            return true;
        }

        stream = kan_random_access_stream_buffer_open_for_read (stream, KAN_UNIVERSE_RESOURCE_PROVIDER_IO_BUFFER);
        CUSHION_DEFER { stream->operations->close (stream); }

        kan_serialization_interned_string_registry_reader_t reader =
            kan_serialization_interned_string_registry_reader_create (stream, true);
        CUSHION_DEFER { kan_serialization_interned_string_registry_reader_destroy (reader); }
        enum kan_serialization_state_t serialization_state;

        while ((serialization_state = kan_serialization_interned_string_registry_reader_step (reader)) ==
               KAN_SERIALIZATION_IN_PROGRESS)
        {
        }

        string_registry = kan_serialization_interned_string_registry_reader_get (reader);
        if (serialization_state == KAN_SERIALIZATION_FAILED)
        {
            kan_serialization_interned_string_registry_destroy (string_registry);
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                     "Failed to read index accompanying string registry at virtual path \"%s\": serialization error.",
                     path_container->path)

            // We still have index, but failed to decode it.
            return true;
        }

        kan_serialization_interned_string_registry_t *spot =
            kan_dynamic_array_add_last (&private->loaded_string_registries);

        if (!spot)
        {
            kan_dynamic_array_set_capacity (&private->loaded_string_registries,
                                            KAN_MAX (1u, private->loaded_string_registries.size * 2u));
            spot = kan_dynamic_array_add_last (&private->loaded_string_registries);
        }

        *spot = string_registry;
    }

    kan_file_system_path_container_reset_length (path_container, base_length);
    kan_file_system_path_container_append (path_container, KAN_RESOURCE_INDEX_DEFAULT_NAME);

    struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (volume, path_container->path);
    if (!stream)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Failed to read resource index at virtual path \"%s\": unable to open read stream.",
                 path_container->path)

        // We still have index, but failed to decode it.
        return true;
    }

    stream = kan_random_access_stream_buffer_open_for_read (stream, KAN_UNIVERSE_RESOURCE_PROVIDER_IO_BUFFER);
    CUSHION_DEFER { stream->operations->close (stream); }

    struct kan_resource_index_t resource_index;
    kan_resource_index_init (&resource_index);
    CUSHION_DEFER { kan_resource_index_shutdown (&resource_index); }

    kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
        stream, &resource_index, KAN_STATIC_INTERNED_ID_GET (kan_resource_index_t), state->shared_script_storage,
        string_registry, kan_resource_index_get_allocation_group ());
    enum kan_serialization_state_t serialization_state;

    while ((serialization_state = kan_serialization_binary_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    kan_serialization_binary_reader_destroy (reader);
    if (serialization_state == KAN_SERIALIZATION_FAILED)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Failed to read resource index at virtual path \"%s\": serialization error.", path_container->path)

        // We still have index, but failed to decode it.
        return true;
    }

    for (kan_loop_size_t container_index = 0u; container_index < resource_index.containers.size; ++container_index)
    {
        const struct kan_resource_index_container_t *container =
            &((struct kan_resource_index_container_t *) resource_index.containers.data)[container_index];

        for (kan_loop_size_t item_index = 0u; item_index < container->items.size; ++item_index)
        {
            const struct kan_resource_index_item_t *item =
                &((struct kan_resource_index_item_t *) container->items.data)[item_index];

            kan_file_system_path_container_reset_length (path_container, base_length);
            kan_file_system_path_container_append (path_container, item->path);
            register_new_entry_with_duplication_check (state, private, container->type, item->name,
                                                       path_container->path, string_registry);
        }
    }

    return true;
}

struct scan_file_internal_result_t
{
    bool successful;
    kan_interned_string_t type;
    kan_interned_string_t name;
};

static struct scan_file_internal_result_t scan_file_internal (struct resource_provider_state_t *state,
                                                              struct resource_provider_private_singleton_t *private,
                                                              kan_virtual_file_system_volume_t volume,
                                                              kan_instance_size_t path_length,
                                                              const char *path)
{
    struct scan_file_internal_result_t result = {
        .successful = false,
        .type = NULL,
        .name = NULL,
    };

    const char *path_end = path + path_length;
    if (path_length <= 4u || *(path_end - 4u) != '.' || *(path_end - 3u) != 'b' || *(path_end - 2u) != 'i' ||
        *(path_end - 1u) != 'n')
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Failed to scan entry at virtual path \"%s\": it does not look like a resource in binary (.bin) "
                 "format, only binaries are supported as of now.",
                 path)
        return result;
    }

    const char *name_begin = path_end;
    while (name_begin > path && *(name_begin - 1u) != '/' && *(name_begin - 1u) != '\\')
    {
        --name_begin;
    }

    result.name = kan_char_sequence_intern (name_begin, path_end - 4u);
    struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (volume, path);

    if (!stream)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Failed to scan entry at virtual path \"%s\": unable to open read stream to read type header.", path)
        return result;
    }

    stream = kan_random_access_stream_buffer_open_for_read (stream, KAN_UNIVERSE_RESOURCE_PROVIDER_TYPE_HEADER_BUFFER);
    CUSHION_DEFER { stream->operations->close (stream); }

    if (!kan_serialization_binary_read_type_header (
            stream, &result.type,
            // We expect non-indexed files to be encoded without string registries.
            KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t)))
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Failed to scan entry at virtual path \"%s\": unable to read type header.", path)
        return result;
    }

    result.successful = true;
    return result;
}

static void scan_file (struct resource_provider_state_t *state,
                       struct resource_provider_private_singleton_t *private,
                       kan_virtual_file_system_volume_t volume,
                       struct kan_file_system_path_container_t *container)
{
    struct scan_file_internal_result_t scan_result =
        scan_file_internal (state, private, volume, container->length, container->path);

    if (scan_result.successful)
    {
        register_new_entry_with_duplication_check (
            state, private, scan_result.type, scan_result.name, container->path,
            KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));
    }
}

static void scan_directory (struct resource_provider_state_t *state,
                            struct resource_provider_private_singleton_t *private,
                            kan_virtual_file_system_volume_t volume,
                            struct kan_file_system_path_container_t *container)
{
    if (load_directory_resource_index_if_any (state, private, volume, container))
    {
        return;
    }

    struct kan_virtual_file_system_directory_iterator_t iterator =
        kan_virtual_file_system_directory_iterator_create (volume, container->path);
    CUSHION_DEFER { kan_virtual_file_system_directory_iterator_destroy (&iterator); }
    const char *entry_name;

    while ((entry_name = kan_virtual_file_system_directory_iterator_advance (&iterator)))
    {
        if ((entry_name[0u] == '.' && entry_name[1u] == '\0') ||
            (entry_name[0u] == '.' && entry_name[1u] == '.' && entry_name[2u] == '\0'))
        {
            continue;
        }

        const kan_instance_size_t base_length = container->length;
        CUSHION_DEFER { kan_file_system_path_container_reset_length (container, base_length); }
        kan_file_system_path_container_append (container, entry_name);
        struct kan_virtual_file_system_entry_status_t status;

        if (!kan_virtual_file_system_query_entry (volume, container->path, &status))
        {
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                     "Failed to scan entry at virtual path \"%s\": unable to query status.", container->path)
            continue;
        }

        switch (status.type)
        {
        case KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN:
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                     "Failed to scan entry at virtual path \"%s\": type is unknown.", container->path)
            break;

        case KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE:
            scan_file (state, private, volume, container);
            break;

        case KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY:
            scan_directory (state, private, volume, container);
            break;
        }
    }
}

static kan_instance_size_t calculate_usage_priority (struct resource_provider_state_t *state,
                                                     kan_interned_string_t type,
                                                     kan_interned_string_t name)
{
    kan_instance_size_t priority = 0u;
    KAN_UML_VALUE_READ (usage, kan_resource_usage_t, name, &name)
    {
        if (usage->type == type)
        {
            priority = KAN_MAX (priority, usage->priority);
        }
    }

    return priority;
}

static void process_usage_insert (struct resource_provider_state_t *state,
                                  kan_interned_string_t type,
                                  kan_interned_string_t name)
{
    KAN_UML_VALUE_UPDATE (generic, kan_resource_generic_entry_t, name, &name)
    {
        if (generic->type == type)
        {
            ++generic->usage_counter;
            if (generic->usage_counter == 1u)
            {
                if (generic->removal_mark)
                {
                    KAN_LOG (universe_resource_provider, KAN_LOG_WARNING,
                             "Added usage to \"%s\" of type \"%s\", but it is removed in actual file system due to hot "
                             "reload.",
                             name, type)
                }
                else if (generic->reload_after_timer != KAN_PACKED_TIMER_NEVER)
                {
                    KAN_LOG (universe_resource_provider, KAN_LOG_DEBUG,
                             "Added usage to \"%s\" of type \"%s\", but loading is delayed as entry has a hot reload "
                             "timer on it.",
                             name, type)
                }
                else
                {
                    KAN_UMO_INDEXED_INSERT (operation, resource_provider_operation_t)
                    {
                        operation->entry_id = generic->entry_id;
                        operation->type = generic->type;
                        operation->priority = calculate_usage_priority (state, type, name);
                    }
                }
            }
            else
            {
                KAN_UMI_VALUE_UPDATE_OPTIONAL (operation, resource_provider_operation_t, entry_id, &generic->entry_id)
                if (operation)
                {
                    operation->priority = calculate_usage_priority (state, type, name);
                }
            }

            return;
        }
    }

    KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
             "Failed to add usage for \"%s\" of type \"%s\": entry is not found.", name, type)
}

static void process_usage_delete (struct resource_provider_state_t *state,
                                  kan_interned_string_t type,
                                  kan_interned_string_t name)
{
    KAN_UML_VALUE_UPDATE (generic, kan_resource_generic_entry_t, name, &name)
    {
        if (generic->type == type)
        {
            KAN_ASSERT (generic->usage_counter > 0u)
            --generic->usage_counter;

            if (generic->usage_counter > 0u)
            {
                KAN_UMI_VALUE_UPDATE_OPTIONAL (operation, resource_provider_operation_t, entry_id, &generic->entry_id)
                if (operation)
                {
                    operation->priority = calculate_usage_priority (state, type, name);
                }
            }
            else
            {
                KAN_UMI_VALUE_DELETE_OPTIONAL (operation, resource_provider_operation_t, entry_id, &generic->entry_id)
                if (operation)
                {
                    KAN_UM_ACCESS_DELETE (operation);
                }

                struct resource_provider_resource_type_interface_t *interface =
                    query_resource_type_interface (state, type);
                KAN_ASSERT (interface)

                struct kan_repository_indexed_value_update_access_t access =
                    update_typed_resource_entry (interface, generic->entry_id);

                struct kan_resource_typed_entry_view_t *typed =
                    kan_repository_indexed_value_update_access_resolve (&access);
                KAN_ASSERT (typed)

                if (KAN_TYPED_ID_32_IS_VALID (typed->loaded_container_id))
                {
                    delete_container_by_id (interface, typed->loaded_container_id);
                    typed->loaded_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
                }

                if (KAN_TYPED_ID_32_IS_VALID (typed->loading_container_id))
                {
                    delete_container_by_id (interface, typed->loading_container_id);
                    typed->loading_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
                }

                kan_repository_indexed_value_update_access_close (&access);
            }

            return;
        }
    }
}

static void process_file_added (struct resource_provider_state_t *state, const char *path)
{
    KAN_UMO_INDEXED_INSERT (delayed, resource_provider_delayed_file_addition_t)
    {
        delayed->path_hash = kan_string_hash (path);
        const kan_instance_size_t path_length = (kan_instance_size_t) strlen (path);
        delayed->path = kan_allocate_general (delayed->my_allocation_group, path_length + 1u, alignof (char));
        memcpy (delayed->path, path, path_length + 1u);

        const kan_time_size_t investigate_after_ns =
            kan_precise_time_get_elapsed_nanoseconds () +
            kan_hot_reload_coordination_system_get_change_wait_time_ns (state->hot_reload_system);

        KAN_ASSERT (KAN_PACKED_TIMER_IS_SAFE_TO_SET (investigate_after_ns))
        delayed->investigate_after_timer = KAN_PACKED_TIMER_SET (investigate_after_ns);
    }
}

static void process_file_modified (struct resource_provider_state_t *state, const char *path)
{
    const kan_hash_t path_hash = kan_string_hash (path);
    KAN_UML_VALUE_UPDATE (generic, kan_resource_generic_entry_t, path_hash, &path_hash)
    {
        if (strcmp (generic->path, path) == 0)
        {
            KAN_ASSERT (!generic->removal_mark)
            const kan_time_size_t reload_after_ns =
                kan_precise_time_get_elapsed_nanoseconds () +
                kan_hot_reload_coordination_system_get_change_wait_time_ns (state->hot_reload_system);

            KAN_ASSERT (KAN_PACKED_TIMER_IS_SAFE_TO_SET (reload_after_ns))
            generic->reload_after_timer = KAN_PACKED_TIMER_SET (reload_after_ns);

            KAN_UMO_EVENT_INSERT (event, kan_resource_updated_event_t)
            {
                event->entry_id = generic->entry_id;
                event->type = generic->type;
                event->name = generic->name;
            }

            return;
        }
    }

    KAN_UML_VALUE_UPDATE (delayed, resource_provider_delayed_file_addition_t, path_hash, &path_hash)
    {
        if (strcmp (delayed->path, path) == 0)
        {
            const kan_time_size_t investigate_after_ns =
                kan_precise_time_get_elapsed_nanoseconds () +
                kan_hot_reload_coordination_system_get_change_wait_time_ns (state->hot_reload_system);

            KAN_ASSERT (KAN_PACKED_TIMER_IS_SAFE_TO_SET (investigate_after_ns))
            delayed->investigate_after_timer = KAN_PACKED_TIMER_SET (investigate_after_ns);
            return;
        }
    }
}

static void process_file_removed (struct resource_provider_state_t *state, const char *path)
{
    const kan_hash_t path_hash = kan_string_hash (path);
    KAN_UML_VALUE_UPDATE (generic, kan_resource_generic_entry_t, path_hash, &path_hash)
    {
        if (strcmp (generic->path, path) == 0)
        {
            // Found entry, just add removal mark to it.
            generic->removal_mark = true;
            return;
        }
    }

    KAN_UML_VALUE_DELETE (delayed, resource_provider_delayed_file_addition_t, path_hash, &path_hash)
    {
        if (strcmp (delayed->path, path) == 0)
        {
            KAN_UM_ACCESS_DELETE (delayed);
            return;
        }
    }
}

static void reload_entry (struct resource_provider_state_t *state, struct kan_resource_generic_entry_t *generic)
{
    if (generic->usage_counter == 0u)
    {
        return;
    }

    // Firstly, clear current loading operation if it is somehow being executed.
    {
        KAN_UMI_VALUE_DELETE_OPTIONAL (operation, resource_provider_operation_t, entry_id, &generic->entry_id)
        if (operation)
        {
            KAN_UM_ACCESS_DELETE (operation);
        }

        struct resource_provider_resource_type_interface_t *interface =
            query_resource_type_interface (state, generic->type);
        KAN_ASSERT (interface)

        struct kan_repository_indexed_value_update_access_t access =
            update_typed_resource_entry (interface, generic->entry_id);

        struct kan_resource_typed_entry_view_t *typed = kan_repository_indexed_value_update_access_resolve (&access);
        KAN_ASSERT (typed)

        if (KAN_TYPED_ID_32_IS_VALID (typed->loading_container_id))
        {
            delete_container_by_id (interface, typed->loading_container_id);
            typed->loading_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
        }

        kan_repository_indexed_value_update_access_close (&access);
    }

    // Then start new loading operation in order to do the reload.
    KAN_UMO_INDEXED_INSERT (operation, resource_provider_operation_t)
    {
        operation->entry_id = generic->entry_id;
        operation->type = generic->type;
        operation->priority = calculate_usage_priority (state, generic->type, generic->name);
    }
}

static void process_delayed_addition (struct resource_provider_state_t *state,
                                      struct resource_provider_private_singleton_t *private)
{
    KAN_CPU_SCOPED_STATIC_SECTION (process_delayed_addition)
    const kan_time_size_t current_time_ns = kan_precise_time_get_elapsed_nanoseconds ();
    KAN_ASSERT (KAN_PACKED_TIMER_IS_SAFE_TO_SET (current_time_ns))
    const kan_packed_timer_t current_timer = KAN_PACKED_TIMER_SET (current_time_ns);

    KAN_UML_INTERVAL_ASCENDING_DELETE (delayed, resource_provider_delayed_file_addition_t, investigate_after_timer,
                                       NULL, &current_timer)
    {
        const kan_instance_size_t path_length = (kan_instance_size_t) strlen (delayed->path);
        kan_virtual_file_system_volume_t volume =
            kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);

        struct scan_file_internal_result_t scan_result =
            scan_file_internal (state, private, volume, path_length, delayed->path);
        kan_virtual_file_system_close_context_read_access (state->virtual_file_system);

        if (!scan_result.successful)
        {
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                     "Failed to process addition at virtual path \"%s\" due to scan failure.", delayed->path)
            KAN_UM_ACCESS_DELETE (delayed);
            continue;
        }

        bool invalid_addition = false;
        kan_resource_entry_id_t entry_id = KAN_TYPED_ID_32_INITIALIZE_INVALID;
        bool new_entry = false;

        KAN_UML_VALUE_UPDATE (existing_generic, kan_resource_generic_entry_t, name, &scan_result.name)
        {
            if (existing_generic->type == scan_result.name)
            {
                entry_id = existing_generic->entry_id;
                if (!existing_generic->removal_mark)
                {
                    KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                             "Failed to process addition at virtual path \"%s\" as entry \"%s\" of type \"%s\" already "
                             "exists at \"%s\".",
                             delayed->path, existing_generic->name, existing_generic->type, existing_generic->path)
                    invalid_addition = true;
                    break;
                }

                if (strcmp (existing_generic->path, delayed->path) != 0)
                {
                    kan_free_general (existing_generic->my_allocation_group, existing_generic->path,
                                      (kan_instance_size_t) strlen (existing_generic->path) + 1u);

                    existing_generic->path =
                        kan_allocate_general (existing_generic->my_allocation_group, path_length + 1u, alignof (char));
                    memcpy (existing_generic->path, delayed->path, path_length + 1u);
                    existing_generic->path_hash = kan_string_hash (existing_generic->path);
                }

                existing_generic->reload_after_timer = KAN_PACKED_TIMER_NEVER;
                existing_generic->removal_mark = false;
                break;
            }
        }

        if (invalid_addition)
        {
            KAN_UM_ACCESS_DELETE (delayed);
            continue;
        }

        if (!KAN_TYPED_ID_32_IS_VALID (entry_id))
        {
            new_entry = true;
            entry_id = register_new_entry (state, private, scan_result.type, scan_result.name, delayed->path,
                                           KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));

            if (!KAN_TYPED_ID_32_IS_VALID (entry_id))
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                         "Failed to process addition at virtual path \"%s\" due to entry registration failure.",
                         delayed->path)
                KAN_UM_ACCESS_DELETE (delayed);
                continue;
            }
        }

        // Technically, we could've preserved access to existing generic entry if any, but this is kind of a rare case,
        // so it doesn't seem to be worth to complicate code for that.

        KAN_UML_VALUE_UPDATE (generic, kan_resource_generic_entry_t, entry_id, &entry_id)
        {
            if (new_entry)
            {
                KAN_UML_VALUE_READ (usage, kan_resource_usage_t, name, &generic->name)
                {
                    if (usage->type == generic->type)
                    {
                        ++generic->usage_counter;
                    }
                }
            }

            reload_entry (state, generic);
        }

        KAN_UM_ACCESS_DELETE (delayed);
    }
}

static void process_delayed_modification (struct resource_provider_state_t *state)
{
    KAN_CPU_SCOPED_STATIC_SECTION (process_delayed_modification)
    const kan_time_size_t current_time_ns = kan_precise_time_get_elapsed_nanoseconds ();
    KAN_ASSERT (KAN_PACKED_TIMER_IS_SAFE_TO_SET (current_time_ns))
    const kan_packed_timer_t current_timer = KAN_PACKED_TIMER_SET (current_time_ns);

    KAN_UML_INTERVAL_ASCENDING_UPDATE (generic, kan_resource_generic_entry_t, reload_after_timer, NULL, &current_timer)
    {
        generic->reload_after_timer = KAN_PACKED_TIMER_NEVER;
        if (generic->removal_mark)
        {
            continue;
        }

        {
            // Read type header in case if type was modified.
            kan_virtual_file_system_volume_t volume =
                kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);

            struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (volume, generic->path);
            kan_virtual_file_system_close_context_read_access (state->virtual_file_system);

            if (!stream)
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                         "Failed to process modification of entry \"%s\" of type \"%s\": unable to open stream to read "
                         "type header.",
                         generic->name, generic->type)
                continue;
            }

            stream = kan_random_access_stream_buffer_open_for_read (stream,
                                                                    KAN_UNIVERSE_RESOURCE_PROVIDER_TYPE_HEADER_BUFFER);
            CUSHION_DEFER { stream->operations->close (stream); }
            kan_interned_string_t type;

            if (!kan_serialization_binary_read_type_header (
                    stream, &type,
                    // We expect non-indexed files to be encoded without string registries.
                    KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t)))
            {
                KAN_LOG (
                    universe_resource_provider, KAN_LOG_ERROR,
                    "Failed to process modification of entry \"%s\" of type \"%s\": failed to deserialize type header.",
                    generic->name, generic->type)
                continue;
            }

            if (type != generic->type)
            {
                KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                         "Failed to process modification of entry \"%s\" of type \"%s\": type header has type \"%s\" "
                         "and we do not expect such things to happen as resource builder is expected to deploy files "
                         "into type-based directories.",
                         generic->name, generic->type, type)
                continue;
            }
        }

        reload_entry (state, generic);
    }
}

enum resource_provider_serve_operation_status_t
{
    RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_IN_PROGRESS,
    RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_FINISHED,
    RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_FAILED,
};

static inline enum resource_provider_serve_operation_status_t execute_shared_serve_load (
    struct resource_provider_state_t *state,
    struct resource_provider_resource_type_interface_t *interface,
    struct resource_provider_operation_t *operation,
    struct kan_resource_typed_entry_view_t *typed)
{
    if (!KAN_HANDLE_IS_EQUAL (operation->used_registry, state->reflection_registry))
    {
        // Registry has changed, reset everything.
        // Technically, we could reset state more efficiently by avoiding stream and container recreation, but it
        // would make code more difficult and there is no performance requirements that enforce efficiency for this
        // particular development-only occurrence.

        if (KAN_HANDLE_IS_VALID (operation->binary_reader))
        {
            kan_serialization_binary_reader_destroy (operation->binary_reader);
            operation->binary_reader = KAN_HANDLE_SET_INVALID (kan_serialization_binary_reader_t);
        }

        if (KAN_TYPED_ID_32_IS_VALID (typed->loading_container_id))
        {
            delete_container_by_id (interface, typed->loading_container_id);
            typed->loading_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
        }

        if (operation->stream)
        {
            operation->stream->operations->close (operation->stream);
            operation->stream = NULL;
        }
    }

    if (!operation->stream)
    {
        KAN_ASSERT (!KAN_HANDLE_IS_VALID (operation->binary_reader))
        KAN_UMI_VALUE_READ_REQUIRED (generic, kan_resource_generic_entry_t, entry_id, &operation->entry_id)

        kan_virtual_file_system_volume_t volume =
            kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);
        operation->stream = kan_virtual_file_stream_open_for_read (volume, generic->path);
        kan_virtual_file_system_close_context_read_access (state->virtual_file_system);

        if (!operation->stream)
        {
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                     "Failed to open file at virtual path \"%s\" to read entry \"%s\" of type \"%s\".", generic->path,
                     typed->name, operation->type)
            return RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_FAILED;
        }

        operation->stream =
            kan_random_access_stream_buffer_open_for_read (operation->stream, KAN_UNIVERSE_RESOURCE_PROVIDER_IO_BUFFER);
        kan_interned_string_t type;

        if (!kan_serialization_binary_read_type_header (operation->stream, &type, typed->bound_to_string_registry))
        {
            KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                     "Failed to check type header while loading entry \"%s\" of type \"%s\": serialization error.",
                     typed->name, operation->type)
            return RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_FAILED;
        }

        if (type != operation->type)
        {
            KAN_LOG (
                universe_resource_provider, KAN_LOG_ERROR,
                "Failed to check type header while loading entry \"%s\" of type \"%s\": type header has type \"%s\".",
                typed->name, operation->type, type)
            return RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_FAILED;
        }
    }

    // We cannot just query the container after inserting it as it is not guaranteed that repository maintenance has
    // happened. And if it didn't happen yet, query would've returned NULL as inserted container is not there yet.
    bool existent_container = false;
    struct kan_repository_indexed_value_update_access_t existent_container_access;
    struct kan_repository_indexed_insertion_package_t inserted_container_package;
    struct kan_resource_container_view_t *container_view = NULL;

    CUSHION_DEFER
    {
        if (existent_container)
        {
            kan_repository_indexed_value_update_access_close (&existent_container_access);
        }
        else
        {
            kan_repository_indexed_insertion_package_submit (&inserted_container_package);
        }
    }

    if ((existent_container = KAN_TYPED_ID_32_IS_VALID (typed->loading_container_id)))
    {
        existent_container_access = update_container_by_id (interface, typed->loading_container_id);
        container_view = kan_repository_indexed_value_update_access_resolve (&existent_container_access);
    }
    else
    {
        KAN_ASSERT (!KAN_HANDLE_IS_VALID (operation->binary_reader))
        inserted_container_package = kan_repository_indexed_insert_query_execute (&interface->insert_container);

        container_view = kan_repository_indexed_insertion_package_get (&inserted_container_package);
        container_view->container_id =
            KAN_TYPED_ID_32_SET (kan_resource_container_id_t,
                                 kan_atomic_int_add (&state->execution_shared_state.private->container_id_counter, 1));

        typed->loading_container_id = container_view->container_id;
    }

    KAN_ASSERT (container_view)
    void *contained_data = (void *) kan_apply_alignment ((kan_memory_size_t) container_view->data_begin,
                                                         interface->source_node->source_resource_type->alignment);

    if (!KAN_HANDLE_IS_VALID (operation->binary_reader))
    {
        operation->binary_reader = kan_serialization_binary_reader_create (
            operation->stream, contained_data, operation->type, state->shared_script_storage,
            typed->bound_to_string_registry, container_view->my_allocation_group);
    }

    enum kan_serialization_state_t serialization_state;
    while ((serialization_state = kan_serialization_binary_reader_step (operation->binary_reader)) ==
           KAN_SERIALIZATION_IN_PROGRESS)
    {
        if (kan_precise_time_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
        {
            return RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_IN_PROGRESS;
        }
    }

    if (serialization_state != KAN_SERIALIZATION_FINISHED)
    {
        KAN_LOG (universe_resource_provider, KAN_LOG_ERROR,
                 "Failed to load entry \"%s\" of type \"%s\": serialization error.", typed->name, operation->type)
        return RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_FAILED;
    }

    if (KAN_TYPED_ID_32_IS_VALID (typed->loaded_container_id))
    {
        delete_container_by_id (interface, typed->loaded_container_id);
    }

    typed->loaded_container_id = typed->loading_container_id;
    typed->loading_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);

    struct kan_repository_event_insertion_package_t insert_event =
        kan_repository_event_insert_query_execute (&interface->insert_loaded_event);
    struct kan_resource_loaded_event_view_t *event = kan_repository_event_insertion_package_get (&insert_event);

    if (event)
    {
        event->entry_id = typed->entry_id;
        event->name = typed->name;
        kan_repository_event_insertion_package_submit (&insert_event);
    }

    return RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_FINISHED;
}

static void execute_shared_serve (kan_functor_user_data_t user_data)
{
    struct resource_provider_state_t *state = (struct resource_provider_state_t *) user_data;
    const bool hot_reload_scheduled = KAN_HANDLE_IS_VALID (state->hot_reload_system) &&
                                      kan_hot_reload_coordination_system_is_scheduled (state->hot_reload_system);
    bool done_any_work = false;

    while (true)
    {
        if (done_any_work && kan_precise_time_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
        {
            // Exit: no more time.
            if (hot_reload_scheduled)
            {
                kan_hot_reload_coordination_system_delay (state->hot_reload_system);
            }

            break;
        }

        done_any_work = true;
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
        {
            struct resource_provider_resource_type_interface_t *interface =
                query_resource_type_interface (state, operation->type);
            KAN_ASSERT (interface)

            struct kan_repository_indexed_value_update_access_t access =
                update_typed_resource_entry (interface, operation->entry_id);

            struct kan_resource_typed_entry_view_t *typed =
                kan_repository_indexed_value_update_access_resolve (&access);

            KAN_ASSERT (typed)
            status = execute_shared_serve_load (state, interface, operation, typed);

            // Ensure that if loading is not in progress, loading container is cleaned up.
            if (status != RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_IN_PROGRESS &&
                KAN_TYPED_ID_32_IS_VALID (typed->loading_container_id))
            {
                delete_container_by_id (interface, typed->loading_container_id);
                typed->loading_container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
            }

            kan_repository_indexed_value_update_access_close (&access);
        }

        switch (status)
        {
        case RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_IN_PROGRESS:
            if (hot_reload_scheduled)
            {
                kan_hot_reload_coordination_system_delay (state->hot_reload_system);
            }

            kan_repository_indexed_interval_write_access_close (&operation_access);
            break;

        case RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_FINISHED:
        case RESOURCE_PROVIDER_SERVE_OPERATION_STATUS_FAILED:
            // No matter the result, operation execution is done, therefore it should be deleted.
            kan_repository_indexed_interval_write_access_delete (&operation_access);
            break;
        }
    }

    if (kan_atomic_int_add (&state->execution_shared_state.workers_left, -1) == 1)
    {
        kan_repository_indexed_interval_descending_write_cursor_close (&state->execution_shared_state.operation_cursor);
        kan_repository_singleton_write_access_close (&state->execution_shared_state.private_access);
    }
}

UNIVERSE_RESOURCE_PROVIDER_API KAN_UM_MUTATOR_EXECUTE_SIGNATURE (mutator_template_execute_resource_provider,
                                                                 resource_provider_state_t)
{
    KAN_UMI_SINGLETON_WRITE (public, kan_resource_provider_singleton_t)
    KAN_UMI_SINGLETON_WRITE (private, resource_provider_private_singleton_t)

    if (!public->scan_done)
    {
        KAN_CPU_SCOPED_STATIC_SECTION (scan)
        {
            kan_virtual_file_system_volume_t volume =
                kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);
            CUSHION_DEFER { kan_virtual_file_system_close_context_read_access (state->virtual_file_system); };

            struct kan_file_system_path_container_t container;
            kan_file_system_path_container_copy_string (&container, state->resource_directory_path);
            scan_directory (state, private, volume, &container);
            kan_dynamic_array_set_capacity (&private->loaded_string_registries, private->loaded_string_registries.size);
        }

        if (kan_hot_reload_coordination_system_is_possible () && KAN_HANDLE_IS_VALID (state->hot_reload_system))
        {
            kan_virtual_file_system_volume_t volume =
                kan_virtual_file_system_get_context_volume_for_write (state->virtual_file_system);
            CUSHION_DEFER { kan_virtual_file_system_close_context_write_access (state->virtual_file_system); }

            private->resource_watcher = kan_virtual_file_system_watcher_create (volume, state->resource_directory_path);
            KAN_ASSERT (KAN_HANDLE_IS_VALID (private->resource_watcher))
            private->resource_watcher_iterator =
                kan_virtual_file_system_watcher_iterator_create (private->resource_watcher);
        }

        public->scan_done = true;
    }

    // We do not record frame begin until scan is done, because first frame with scan is kind of a special case in the
    // beginning of application execution.
    const kan_time_size_t frame_begin_time_ns = kan_precise_time_get_elapsed_nanoseconds ();

    // If we're watching resources for changes, process events from resource watcher.
    if (KAN_HANDLE_IS_VALID (private->resource_watcher))
    {
        {
            KAN_CPU_SCOPED_STATIC_SECTION (resource_watcher)
            kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);
            const struct kan_virtual_file_system_watcher_event_t *event;

            while ((event = kan_virtual_file_system_watcher_iterator_get (private->resource_watcher,
                                                                          private->resource_watcher_iterator)))
            {
                if (event->entry_type == KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE)
                {
                    const char *path = event->path_container.path;

                    // Skip first "/" in order to have same path format for scanned and observed files.
                    if (path && path[0u] == '/')
                    {
                        ++path;
                    }

                    switch (event->event_type)
                    {
                    case KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_ADDED:
                        process_file_added (state, path);
                        break;

                    case KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_MODIFIED:
                        process_file_modified (state, path);
                        break;

                    case KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_REMOVED:
                        process_file_removed (state, path);
                        break;
                    }
                }

                private->resource_watcher_iterator = kan_virtual_file_system_watcher_iterator_advance (
                    private->resource_watcher, private->resource_watcher_iterator);
            }

            kan_virtual_file_system_close_context_read_access (state->virtual_file_system);
        }

        if (kan_hot_reload_coordination_system_is_reload_allowed (state->hot_reload_system))
        {
            process_delayed_addition (state, private);
            process_delayed_modification (state);
        }
    }

    {
        KAN_CPU_SCOPED_STATIC_SECTION (events)
        KAN_UML_EVENT_FETCH (insert_event, resource_usage_on_insert_event_t)
        {
            process_usage_insert (state, insert_event->type, insert_event->name);
        }

        KAN_UML_EVENT_FETCH (delete_event, resource_usage_on_delete_event_t)
        {
            process_usage_delete (state, delete_event->type, delete_event->name);
        }
    }

    if (KAN_HANDLE_IS_VALID (state->hot_reload_system) &&
        kan_hot_reload_coordination_system_is_executing (state->hot_reload_system))
    {
        // Hot reload is going on, do not start any operation until it is done. All operations prior to hot reload
        // execution would delay it, so we cannot have any ongoing operation if it is already executing.
        return;
    }

    kan_stack_group_allocator_reset (&state->temporary_allocator);
    const kan_instance_size_t cpu_count = kan_platform_get_cpu_logical_core_count ();
    struct kan_cpu_task_list_node_t *task_list_node = NULL;

    state->execution_shared_state.job = job;
    state->execution_shared_state.end_time_ns = frame_begin_time_ns + state->serve_budget_ns;

    state->execution_shared_state.workers_left = kan_atomic_int_init ((int) cpu_count);
    state->execution_shared_state.concurrency_lock = kan_atomic_int_init (0);

    state->execution_shared_state.private = private;
    KAN_UM_ACCESS_ESCAPE (state->execution_shared_state.private_access, private)

    state->execution_shared_state.operation_cursor = kan_repository_indexed_interval_write_query_execute_descending (
        &state->write_interval__resource_provider_operation__priority, NULL, NULL);

    for (kan_loop_size_t worker_index = 0u; worker_index < cpu_count; ++worker_index)
    {
        KAN_CPU_TASK_LIST_USER_VALUE (&task_list_node, &state->temporary_allocator, execute_shared_serve,
                                      KAN_CPU_STATIC_SECTION_GET (resource_provider_server), state)
    }

    kan_cpu_job_dispatch_and_detach_task_list (state->execution_shared_state.job, task_list_node);
}

UNIVERSE_RESOURCE_PROVIDER_API KAN_UM_MUTATOR_UNDEPLOY_SIGNATURE (mutator_template_undeploy_resource_provider,
                                                                  resource_provider_state_t)
{
    kan_serialization_binary_script_storage_destroy (state->shared_script_storage);
    kan_stack_group_allocator_reset (&state->temporary_allocator);
}

static void generated_container_init (kan_functor_user_data_t function_user_data, void *data)
{
    struct kan_resource_container_view_t *instance = data;
    instance->container_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_container_id_t);
    instance->my_allocation_group = kan_allocation_group_stack_get ();
    const struct kan_reflection_struct_t *boxed_type = (const struct kan_reflection_struct_t *) function_user_data;

    if (boxed_type->init)
    {
        void *contained_data =
            (void *) kan_apply_alignment ((kan_memory_size_t) instance->data_begin, boxed_type->alignment);
        boxed_type->init (boxed_type->functor_user_data, contained_data);
    }
}

static void generated_container_shutdown (kan_functor_user_data_t function_user_data, void *data)
{
    struct kan_resource_container_view_t *instance = data;
    const struct kan_reflection_struct_t *boxed_type = (const struct kan_reflection_struct_t *) function_user_data;

    if (boxed_type->shutdown)
    {
        void *contained_data =
            (void *) kan_apply_alignment ((kan_memory_size_t) instance->data_begin, boxed_type->alignment);

        kan_allocation_group_stack_push (((struct kan_resource_container_view_t *) data)->my_allocation_group);
        boxed_type->shutdown (boxed_type->functor_user_data, contained_data);
        kan_allocation_group_stack_pop ();
    }
}

static void generated_mutator_init (kan_functor_user_data_t function_user_data, void *data)
{
    struct kan_reflection_generator_universe_resource_provider_t *generator =
        (struct kan_reflection_generator_universe_resource_provider_t *) function_user_data;

    struct resource_provider_state_t *instance = data;
    instance->trailing_data_count = generator->nodes_count;
    resource_provider_state_init (instance);

    struct universe_resource_provider_generated_node_t *source = generator->first_node;
    struct resource_provider_resource_type_interface_t *target = instance->trailing_data;

    while (source)
    {
        target->resource_type_name = source->source_resource_type->name;
        target->source_node = source;
        source = source->next;
        ++target;
    }
}

static void generated_mutator_shutdown (kan_functor_user_data_t function_user_data, void *data)
{
    struct resource_provider_state_t *instance = data;
    resource_provider_state_shutdown (instance);
}

static void generated_mutator_deploy (kan_functor_user_data_t user_data, void *return_address, void *arguments_address)
{
    KAN_UNIVERSE_EXTRACT_DEPLOY_ARGUMENTS (arguments, arguments_address, struct resource_provider_state_t);
    mutator_template_deploy_resource_provider (arguments->universe, arguments->world, arguments->world_repository,
                                               arguments->workflow_node, arguments->state);
    kan_reflection_registry_t registry = kan_repository_get_reflection_registry (arguments->world_repository);

    const char *entry_id_name = "entry_id";
    struct kan_repository_field_path_t entry_id_path = {
        .reflection_path_length = 1u,
        &entry_id_name,
    };

    const char *container_id_name = "container_id";
    struct kan_repository_field_path_t container_id_path = {
        .reflection_path_length = 1u,
        &container_id_name,
    };

    for (kan_loop_size_t index = 0u; index < arguments->state->trailing_data_count; ++index)
    {
        struct resource_provider_resource_type_interface_t *interface = &arguments->state->trailing_data[index];
        kan_repository_indexed_storage_t typed_entry_storage = kan_repository_indexed_storage_open (
            arguments->world_repository, interface->source_node->typed_entry_type.name);

        kan_repository_indexed_insert_query_init (&interface->insert_typed_entry, typed_entry_storage);
        kan_universe_register_indexed_insert_from_mutator (registry, arguments->workflow_node,
                                                           interface->source_node->typed_entry_type.name);

        kan_repository_indexed_value_update_query_init (&interface->update_typed_entry_by_id, typed_entry_storage,
                                                        entry_id_path);
        kan_universe_register_indexed_update_from_mutator (registry, arguments->workflow_node,
                                                           interface->source_node->typed_entry_type.name);

        kan_repository_indexed_storage_t container_storage = kan_repository_indexed_storage_open (
            arguments->world_repository, interface->source_node->container_type.name);

        kan_repository_indexed_insert_query_init (&interface->insert_container, container_storage);
        kan_universe_register_indexed_insert_from_mutator (registry, arguments->workflow_node,
                                                           interface->source_node->container_type.name);

        kan_repository_indexed_value_update_query_init (&interface->update_container_by_id, container_storage,
                                                        container_id_path);
        kan_universe_register_indexed_update_from_mutator (registry, arguments->workflow_node,
                                                           interface->source_node->container_type.name);

        kan_repository_indexed_value_delete_query_init (&interface->delete_container_by_id, container_storage,
                                                        container_id_path);
        kan_universe_register_indexed_delete_from_mutator (registry, arguments->workflow_node,
                                                           interface->source_node->container_type.name);

        kan_repository_event_storage_t registered_storage = kan_repository_event_storage_open (
            arguments->world_repository, interface->source_node->registered_event_type.name);

        kan_repository_event_insert_query_init (&interface->insert_registered_event, registered_storage);
        kan_universe_register_event_insert_from_mutator (registry, arguments->workflow_node,
                                                         interface->source_node->registered_event_type.name);

        kan_repository_event_storage_t loaded_storage = kan_repository_event_storage_open (
            arguments->world_repository, interface->source_node->loaded_event_type.name);

        kan_repository_event_insert_query_init (&interface->insert_loaded_event, loaded_storage);
        kan_universe_register_event_insert_from_mutator (registry, arguments->workflow_node,
                                                         interface->source_node->loaded_event_type.name);
    }
}

static void generated_mutator_execute (kan_functor_user_data_t user_data, void *return_address, void *arguments_address)
{
    KAN_UNIVERSE_EXTRACT_EXECUTE_ARGUMENTS (arguments, arguments_address, struct resource_provider_state_t);
    mutator_template_execute_resource_provider (arguments->job, arguments->state);
}

static void generated_mutator_undeploy (kan_functor_user_data_t user_data,
                                        void *return_address,
                                        void *arguments_address)
{
    KAN_UNIVERSE_EXTRACT_UNDEPLOY_ARGUMENTS (arguments, arguments_address, struct resource_provider_state_t);
    mutator_template_undeploy_resource_provider (arguments->state);

    for (kan_loop_size_t index = 0u; index < arguments->state->trailing_data_count; ++index)
    {
        struct resource_provider_resource_type_interface_t *interface = &arguments->state->trailing_data[index];
        kan_repository_indexed_insert_query_shutdown (&interface->insert_typed_entry);
        kan_repository_indexed_value_update_query_shutdown (&interface->update_typed_entry_by_id);

        kan_repository_indexed_insert_query_shutdown (&interface->insert_container);
        kan_repository_indexed_value_update_query_shutdown (&interface->update_container_by_id);
        kan_repository_indexed_value_delete_query_shutdown (&interface->delete_container_by_id);

        kan_repository_event_insert_query_shutdown (&interface->insert_registered_event);

        kan_repository_event_insert_query_shutdown (&interface->insert_loaded_event);
    }
}

UNIVERSE_RESOURCE_PROVIDER_API void kan_reflection_generator_universe_resource_provider_init (
    struct kan_reflection_generator_universe_resource_provider_t *instance)
{
    kan_static_interned_ids_ensure_initialized ();
    instance->generated_reflection_group =
        kan_allocation_group_get_child (kan_allocation_group_stack_get (), "generated_reflection");
    instance->first_node = NULL;
    instance->nodes_count = 0u;

    kan_static_interned_ids_ensure_initialized ();
    kan_cpu_static_sections_ensure_initialized ();
}

UNIVERSE_RESOURCE_PROVIDER_API void kan_reflection_generator_universe_resource_provider_shutdown (
    struct kan_reflection_generator_universe_resource_provider_t *instance)
{
    struct universe_resource_provider_generated_node_t *node = instance->first_node;
    while (node)
    {
        struct universe_resource_provider_generated_node_t *next = node->next;

        // We do not generate visibility data for containers, therefore we can just deallocate fields.

        kan_free_general (instance->generated_reflection_group, node->typed_entry_type.fields,
                          sizeof (struct kan_reflection_field_t) * node->typed_entry_type.fields_count);

        kan_free_general (instance->generated_reflection_group, node->container_type.fields,
                          sizeof (struct kan_reflection_field_t) * node->container_type.fields_count);

        kan_free_general (instance->generated_reflection_group, node->registered_event_type.fields,
                          sizeof (struct kan_reflection_field_t) * node->registered_event_type.fields_count);

        kan_free_general (instance->generated_reflection_group, node->loaded_event_type.fields,
                          sizeof (struct kan_reflection_field_t) * node->loaded_event_type.fields_count);

        kan_free_general (instance->generated_reflection_group, node, sizeof (node));
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

UNIVERSE_RESOURCE_PROVIDER_API void kan_reflection_generator_universe_resource_provider_bootstrap (
    struct kan_reflection_generator_universe_resource_provider_t *instance, kan_loop_size_t bootstrap_iteration)
{
    instance->boostrap_iteration = bootstrap_iteration;
}

static inline bool is_resource_type_already_registered (
    struct kan_reflection_generator_universe_resource_provider_t *instance, kan_interned_string_t type_name)
{
    struct universe_resource_provider_generated_node_t *node = instance->first_node;
    while (node)
    {
        if (node->source_resource_type->name == type_name)
        {
            return true;
        }

        node = node->next;
    }

    return false;
}

static inline void register_resource_type (struct kan_reflection_generator_universe_resource_provider_t *instance,
                                           const struct kan_reflection_struct_t *type,
                                           kan_reflection_system_generation_iterator_t generation_iterator)
{
    struct universe_resource_provider_generated_node_t *node = kan_allocate_general (
        instance->generated_reflection_group, sizeof (struct universe_resource_provider_generated_node_t),
        alignof (struct universe_resource_provider_generated_node_t));

    node->source_resource_type = type;
    node->next = instance->first_node;
    instance->first_node = node;
    ++instance->nodes_count;

    char buffer[256u];

    // Generated typed entry struct.

    snprintf (buffer, sizeof (buffer), KAN_RESOURCE_PROVIDER_TYPED_ENTRY_TYPE_FORMAT, type->name);
    node->typed_entry_type.name = kan_string_intern (buffer);

    node->typed_entry_type.alignment = alignof (struct kan_resource_typed_entry_view_t);
    node->typed_entry_type.size = sizeof (struct kan_resource_typed_entry_view_t);

    node->typed_entry_type.functor_user_data = 0u;
    node->typed_entry_type.init = NULL;
    node->typed_entry_type.shutdown = NULL;

    node->typed_entry_type.fields_count = 5u;
    node->typed_entry_type.fields =
        kan_allocate_general (instance->generated_reflection_group,
                              sizeof (struct kan_reflection_field_t) * node->typed_entry_type.fields_count,
                              alignof (struct kan_reflection_field_t));

#define ADD_FIELD(TARGET, BASE, TYPE, NAME, ARCHETYPE)                                                                 \
    (TARGET).name = KAN_STATIC_INTERNED_ID_GET (NAME);                                                                 \
    (TARGET).offset = offsetof (struct BASE, NAME);                                                                    \
    (TARGET).size = sizeof (TYPE);                                                                                     \
    (TARGET).archetype = ARCHETYPE;                                                                                    \
    (TARGET).visibility_condition_field = NULL;                                                                        \
    (TARGET).visibility_condition_values_count = 0u;                                                                   \
    (TARGET).visibility_condition_values = NULL

    ADD_FIELD (node->typed_entry_type.fields[0u], kan_resource_typed_entry_view_t, kan_resource_entry_id_t, entry_id,
               KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL);
    ADD_FIELD (node->typed_entry_type.fields[1u], kan_resource_typed_entry_view_t, kan_interned_string_t, name,
               KAN_REFLECTION_ARCHETYPE_INTERNED_STRING);
    ADD_FIELD (node->typed_entry_type.fields[2u], kan_resource_typed_entry_view_t, kan_resource_container_id_t,
               loaded_container_id, KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL);
    ADD_FIELD (node->typed_entry_type.fields[3u], kan_resource_typed_entry_view_t, kan_resource_container_id_t,
               loading_container_id, KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL);
    ADD_FIELD (node->typed_entry_type.fields[4u], kan_resource_typed_entry_view_t,
               kan_serialization_interned_string_registry_t, bound_to_string_registry,
               KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL);
    kan_reflection_system_generation_iterator_add_struct (generation_iterator, &node->typed_entry_type);

    // Generate container type.

    snprintf (buffer, sizeof (buffer), KAN_RESOURCE_PROVIDER_CONTAINER_TYPE_FORMAT, type->name);
    node->container_type.name = kan_string_intern (buffer);

    const kan_memory_size_t container_alignment =
        KAN_MAX (alignof (struct kan_resource_container_view_t), type->alignment);
    const kan_memory_size_t data_offset =
        kan_apply_alignment (offsetof (struct kan_resource_container_view_t, data_begin), container_alignment);

    node->container_type.alignment = container_alignment;
    node->container_type.size =
        (kan_instance_size_t) kan_apply_alignment (data_offset + type->size, container_alignment);

    node->container_type.functor_user_data = (kan_functor_user_data_t) type;
    node->container_type.init = generated_container_init;
    node->container_type.shutdown = generated_container_shutdown;

    node->container_type.fields_count = 2u;
    node->container_type.fields =
        kan_allocate_general (instance->generated_reflection_group,
                              sizeof (struct kan_reflection_field_t) * node->container_type.fields_count,
                              alignof (struct kan_reflection_field_t));

    ADD_FIELD (node->container_type.fields[0u], kan_resource_container_view_t, kan_resource_container_id_t,
               container_id, KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL);

    node->container_type.fields[1u].name = KAN_STATIC_INTERNED_ID_GET (stored_resource);
    node->container_type.fields[1u].offset = (kan_instance_size_t) data_offset;
    node->container_type.fields[1u].size = type->size;
    node->container_type.fields[1u].archetype = KAN_REFLECTION_ARCHETYPE_STRUCT;
    node->container_type.fields[1u].archetype_struct.type_name = type->name;
    node->container_type.fields[1u].visibility_condition_field = NULL;
    node->container_type.fields[1u].visibility_condition_values_count = 0u;
    node->container_type.fields[1u].visibility_condition_values = NULL;
    kan_reflection_system_generation_iterator_add_struct (generation_iterator, &node->container_type);

    // Generate registered event type.

    snprintf (buffer, sizeof (buffer), KAN_RESOURCE_PROVIDER_REGISTERED_EVENT_TYPE_FORMAT, type->name);
    node->registered_event_type.name = kan_string_intern (buffer);

    node->registered_event_type.alignment = alignof (struct kan_resource_registered_event_view_t);
    node->registered_event_type.size = sizeof (struct kan_resource_registered_event_view_t);

    node->registered_event_type.functor_user_data = 0u;
    node->registered_event_type.init = NULL;
    node->registered_event_type.shutdown = NULL;

    node->registered_event_type.fields_count = 2u;
    node->registered_event_type.fields =
        kan_allocate_general (instance->generated_reflection_group,
                              sizeof (struct kan_reflection_field_t) * node->registered_event_type.fields_count,
                              alignof (struct kan_reflection_field_t));

    ADD_FIELD (node->registered_event_type.fields[0u], kan_resource_registered_event_view_t, kan_resource_entry_id_t,
               entry_id, KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL);
    ADD_FIELD (node->registered_event_type.fields[1u], kan_resource_registered_event_view_t, kan_interned_string_t,
               name, KAN_REFLECTION_ARCHETYPE_INTERNED_STRING);
    kan_reflection_system_generation_iterator_add_struct (generation_iterator, &node->registered_event_type);

    // Generate loaded event type.

    snprintf (buffer, sizeof (buffer), KAN_RESOURCE_PROVIDER_LOADED_EVENT_TYPE_FORMAT, type->name);
    node->loaded_event_type.name = kan_string_intern (buffer);

    node->loaded_event_type.alignment = alignof (struct kan_resource_loaded_event_view_t);
    node->loaded_event_type.size = sizeof (struct kan_resource_loaded_event_view_t);

    node->loaded_event_type.functor_user_data = 0u;
    node->loaded_event_type.init = NULL;
    node->loaded_event_type.shutdown = NULL;

    node->loaded_event_type.fields_count = 2u;
    node->loaded_event_type.fields =
        kan_allocate_general (instance->generated_reflection_group,
                              sizeof (struct kan_reflection_field_t) * node->loaded_event_type.fields_count,
                              alignof (struct kan_reflection_field_t));

    ADD_FIELD (node->loaded_event_type.fields[0u], kan_resource_loaded_event_view_t, kan_resource_entry_id_t, entry_id,
               KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL);
    ADD_FIELD (node->loaded_event_type.fields[1u], kan_resource_loaded_event_view_t, kan_interned_string_t, name,
               KAN_REFLECTION_ARCHETYPE_INTERNED_STRING);
    kan_reflection_system_generation_iterator_add_struct (generation_iterator, &node->loaded_event_type);

#undef ADD_FIELD
}

UNIVERSE_RESOURCE_PROVIDER_API void kan_reflection_generator_universe_resource_provider_iterate (
    struct kan_reflection_generator_universe_resource_provider_t *instance,
    kan_reflection_registry_t registry,
    kan_reflection_system_generation_iterator_t iterator,
    kan_loop_size_t iteration_index)
{
    // Cannot use inside macro below due to pushes, need to forward declare like that.
    const kan_interned_string_t meta_name = KAN_STATIC_INTERNED_ID_GET (kan_resource_type_meta_t);
    KAN_UNIVERSE_REFLECTION_GENERATOR_STRUCT_META_SCANNER_CORE (universe_resource_provider);

    KAN_UNIVERSE_REFLECTION_GENERATOR_ON_STRUCT_META_SCANNED (kan_resource_type_meta_t, meta_name)
    {
        if (!is_resource_type_already_registered (instance, type->name))
        {
            register_resource_type (instance, type, iterator);
        }
    }
}

UNIVERSE_RESOURCE_PROVIDER_API void kan_reflection_generator_universe_resource_provider_finalize (
    struct kan_reflection_generator_universe_resource_provider_t *instance, kan_reflection_registry_t registry)
{
    if (instance->first_node)
    {
        KAN_UNIVERSE_REFLECTION_GENERATOR_NODE_LIST_TO_TEMPORARY_ARRAY (
            struct universe_resource_provider_generated_node_t, nodes_array, instance->first_node,
            instance->nodes_count, instance->generated_reflection_group)

        {
            struct universe_resource_provider_generated_node_t *temporary;

#define LESS(first_index, second_index)                                                                                \
    __CUSHION_PRESERVE__ strcmp (nodes_array[first_index]->source_resource_type->name,                                 \
                                 nodes_array[second_index]->source_resource_type->name) < 0

#define SWAP(first_index, second_index)                                                                                \
    __CUSHION_PRESERVE__                                                                                               \
    temporary = nodes_array[first_index], nodes_array[first_index] = nodes_array[second_index],                        \
    nodes_array[second_index] = temporary

            QSORT (instance->nodes_count, LESS, SWAP);
#undef LESS
#undef SWAP
#undef AT_INDEX
        }

        instance->first_node = nodes_array[0u];
        KAN_UNIVERSE_REFLECTION_GENERATOR_NODE_REORDER_FROM_ARRAY (nodes_array, instance->nodes_count)
    }

    KAN_UNIVERSE_REFLECTION_GENERATOR_MUTATOR_TYPE (
        instance->mutator_type, instance, generated_resource_provider_state_t, resource_provider_state_t,
        resource_provider_resource_type_interface_t, instance->nodes_count, generated_mutator_init,
        generated_mutator_shutdown, instance->generated_reflection_group);
    kan_reflection_registry_add_struct (registry, &instance->mutator_type);

    KAN_UNIVERSE_REFLECTION_GENERATOR_DEPLOY_FUNCTION (
        instance->mutator_deploy_function, generated_resource_provider_state, generated_resource_provider_state_t,
        generated_mutator_deploy, instance->generated_reflection_group);
    kan_reflection_registry_add_function (registry, &instance->mutator_deploy_function);

    KAN_UNIVERSE_REFLECTION_GENERATOR_EXECUTE_FUNCTION (
        instance->mutator_execute_function, generated_resource_provider_state, generated_resource_provider_state_t,
        generated_mutator_execute, instance->generated_reflection_group);
    kan_reflection_registry_add_function (registry, &instance->mutator_execute_function);
    kan_reflection_registry_add_function_meta (registry, instance->mutator_execute_function.name,
                                               KAN_STATIC_INTERNED_ID_GET (kan_universe_mutator_group_meta_t),
                                               &universe_mutator_group_meta_resource_provider);

    KAN_UNIVERSE_REFLECTION_GENERATOR_UNDEPLOY_FUNCTION (
        instance->mutator_undeploy_function, generated_resource_provider_state, generated_resource_provider_state_t,
        generated_mutator_undeploy, instance->generated_reflection_group);
    kan_reflection_registry_add_function (registry, &instance->mutator_undeploy_function);
}

void kan_resource_provider_configuration_init (struct kan_resource_provider_configuration_t *instance)
{
    instance->serve_budget_ns = 2000000u;
    instance->resource_directory_path = kan_string_intern ("resources");
}

void kan_resource_provider_singleton_init (struct kan_resource_provider_singleton_t *instance)
{
    instance->usage_id_counter = kan_atomic_int_init (1);
    instance->scan_done = false;
}

void kan_resource_generic_entry_init (struct kan_resource_generic_entry_t *instance)
{
    instance->entry_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_entry_id_t);
    instance->type = NULL;
    instance->name = NULL;
    instance->usage_counter = 0u;

    instance->reload_after_timer = KAN_PACKED_TIMER_NEVER;
    instance->removal_mark = false;

    instance->path_hash = 0u;
    instance->path = NULL;
    instance->my_allocation_group = kan_allocation_group_stack_get ();
}

void kan_resource_generic_entry_shutdown (struct kan_resource_generic_entry_t *instance)
{
    if (instance->path)
    {
        kan_free_general (instance->my_allocation_group, instance->path,
                          (kan_instance_size_t) strlen (instance->path) + 1u);
    }
}

void kan_resource_usage_init (struct kan_resource_usage_t *instance)
{
    instance->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);
    instance->type = NULL;
    instance->name = NULL;
    instance->priority = 0u;
}
