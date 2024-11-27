#include <universe_resource_reference_kan_api.h>

#include <stddef.h>

#include <kan/api_common/alignment.h>
#include <kan/container/hash_storage.h>
#include <kan/context/all_system_names.h>
#include <kan/context/plugin_system.h>
#include <kan/context/reflection_system.h>
#include <kan/context/virtual_file_system.h>
#include <kan/log/logging.h>
#include <kan/platform/hardware.h>
#include <kan/platform/precise_time.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/serialization/binary.h>
#include <kan/stream/random_access_stream_buffer.h>
#include <kan/threading/atomic.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe/reflection_system_generator_helpers.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>
#include <kan/universe_resource_reference/universe_resource_reference.h>
#include <kan/virtual_file_system/virtual_file_system.h>

// \c_interface_scanner_disable
KAN_LOG_DEFINE_CATEGORY (universe_resource_reference);
// \c_interface_scanner_enable

// \meta reflection_function_meta = "kan_universe_mutator_execute_resource_reference_manager"
UNIVERSE_RESOURCE_REFERENCE_KAN_API struct kan_universe_mutator_group_meta_t resource_reference_mutator_group = {
    .group_name = KAN_RESOURCE_REFERENCE_MUTATOR_GROUP,
};

// \meta reflection_struct_meta = "kan_resource_native_entry_t"
UNIVERSE_RESOURCE_REFERENCE_KAN_API struct kan_repository_meta_automatic_cascade_deletion_t
    kan_resource_native_entry_outer_reference_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"attachment_id"}},
        .child_type_name = "kan_resource_native_entry_outer_reference_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"attachment_id"}},
};

struct resource_outer_reference_update_state_t
{
    kan_resource_entry_id_t attachment_id;
    kan_time_size_t last_update_file_time_ns;
};

// \meta reflection_struct_meta = "kan_resource_native_entry_t"
UNIVERSE_RESOURCE_REFERENCE_KAN_API struct kan_repository_meta_automatic_cascade_deletion_t
    resource_outer_reference_update_state_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"attachment_id"}},
        .child_type_name = "resource_outer_reference_update_state_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"attachment_id"}},
};

enum resource_outer_references_operation_state_t
{
    RESOURCE_OUTER_REFERENCES_OPERATION_STATE_REQUESTED = 0u,
    RESOURCE_OUTER_REFERENCES_OPERATION_STATE_WAITING_RESOURCE,
};

/// \details Operation is intentionally not automatically cascade-deleted when its entry is deleted.
///          It is done to avoid eternal-wait situation when something waits for response, but will never receive it.
///          To avoid this situation, we send unsuccessful response to user and manually delete operation.
struct resource_outer_references_operation_t
{
    kan_resource_entry_id_t entry_attachment_id;
    kan_interned_string_t type;
    kan_interned_string_t name;

    enum resource_outer_references_operation_state_t state;
    kan_resource_request_id_t resource_request_id;
};

struct resource_all_references_to_type_operation_t
{
    kan_interned_string_t type;
    kan_bool_t successful;
};

struct resource_outer_references_operation_binding_t
{
    kan_resource_entry_id_t entry_attachment_id;
    kan_interned_string_t all_references_to_type;
};

// \meta reflection_struct_meta = "resource_outer_references_operation_t"
UNIVERSE_RESOURCE_REFERENCE_KAN_API struct kan_repository_meta_automatic_cascade_deletion_t
    resource_outer_references_operation_binding_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"entry_attachment_id"}},
        .child_type_name = "resource_outer_references_operation_binding_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"entry_attachment_id"}},
};

struct resource_reference_manager_native_container_type_data_t
{
    kan_interned_string_t contained_type_name;
    kan_interned_string_t container_type_name;
    kan_memory_size_t contained_type_alignment;
    struct kan_repository_indexed_value_read_query_t read_by_id_query;
};

_Static_assert (_Alignof (struct resource_reference_manager_native_container_type_data_t) ==
                    _Alignof (kan_memory_size_t),
                "Alignment matches.");

struct resource_reference_manager_execution_shared_state_t
{
    /// \meta reflection_ignore_struct_field
    struct kan_atomic_int_t workers_left;

    /// \meta reflection_ignore_struct_field
    struct kan_atomic_int_t concurrency_lock;

    //// \meta reflection_ignore_struct_field
    struct kan_repository_indexed_sequence_write_cursor_t outer_reference_operation_cursor;

    kan_time_size_t end_time_ns;
};

struct resource_reference_manager_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (resource_reference_manager_state)
    KAN_UP_BIND_STATE (resource_reference_manager_state, state)

    struct kan_repository_indexed_sequence_write_query_t write_sequence__resource_outer_references_operation;

    kan_time_offset_t budget_ns;
    kan_interned_string_t workspace_directory_path;

    kan_serialization_binary_script_storage_t binary_script_storage;
    kan_context_system_t plugin_system;
    kan_context_system_t virtual_file_system;

    kan_bool_t need_to_cancel_old_operations;
    kan_allocation_group_t my_allocation_group;

    /// \meta reflection_ignore_struct_field
    struct kan_resource_reference_type_info_storage_t info_storage;

    /// \meta reflection_ignore_struct_field
    struct kan_stack_group_allocator_t temporary_allocator;

    /// \meta reflection_ignore_struct_field
    struct resource_reference_manager_execution_shared_state_t execution_shared_state;

    kan_interned_string_t interned_resource_reference_manager_server;

    kan_instance_size_t trailing_data_count;

    /// \meta reflection_ignore_struct_field
    void *trailing_data[0u];
};

_Static_assert (_Alignof (struct resource_reference_manager_state_t) ==
                    _Alignof (struct resource_reference_manager_native_container_type_data_t),
                "Alignment matches.");

struct universe_resource_reference_type_node_t
{
    struct universe_resource_reference_type_node_t *next;
    const struct kan_reflection_struct_t *resource_type;
};

struct kan_reflection_generator_universe_resource_reference_t
{
    kan_loop_size_t boostrap_iteration;
    kan_allocation_group_t generated_reflection_group;
    struct universe_resource_reference_type_node_t *first_resource_type;
    kan_instance_size_t resource_types_count;

    /// \meta reflection_ignore_struct_field
    struct kan_reflection_struct_t mutator_type;

    /// \meta reflection_ignore_struct_field
    struct kan_reflection_function_t mutator_deploy_function;

    /// \meta reflection_ignore_struct_field
    struct kan_reflection_function_t mutator_execute_function;

    /// \meta reflection_ignore_struct_field
    struct kan_reflection_function_t mutator_undeploy_function;

    kan_interned_string_t interned_kan_resource_resource_type_meta_t;
};

static inline struct resource_reference_manager_native_container_type_data_t *query_resource_type_data (
    struct resource_reference_manager_state_t *state, kan_interned_string_t type)
{
    KAN_UNIVERSE_REFLECTION_GENERATOR_FIND_GENERATED_STATE (
        struct resource_reference_manager_native_container_type_data_t, contained_type_name, type);
}

UNIVERSE_RESOURCE_REFERENCE_KAN_API void resource_reference_manager_state_init (
    struct resource_reference_manager_state_t *instance)
{
    instance->my_allocation_group = kan_allocation_group_stack_get ();
    instance->interned_resource_reference_manager_server = kan_string_intern ("resource_reference_manager_server");
}

UNIVERSE_RESOURCE_REFERENCE_KAN_API void mutator_template_deploy_resource_reference_manager (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct resource_reference_manager_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_REFERENCE_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RESOURCE_REFERENCE_END_CHECKPOINT);

    const struct kan_resource_reference_configuration_t *configuration =
        kan_universe_world_query_configuration (world, kan_string_intern (KAN_RESOURCE_REFERENCE_CONFIGURATION));
    KAN_ASSERT (configuration)

    state->budget_ns = configuration->budget_ns;
    state->workspace_directory_path = configuration->workspace_directory_path;

    state->binary_script_storage =
        kan_serialization_binary_script_storage_create (kan_universe_get_reflection_registry (universe));

    state->plugin_system = kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_PLUGIN_SYSTEM_NAME);
    state->virtual_file_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_VIRTUAL_FILE_SYSTEM_NAME);
    KAN_ASSERT (KAN_HANDLE_IS_VALID (state->virtual_file_system))
    state->need_to_cancel_old_operations = KAN_TRUE;

    kan_resource_reference_type_info_storage_build (
        &state->info_storage, kan_universe_get_reflection_registry (universe),
        kan_allocation_group_get_child (state->my_allocation_group, "scanned"));

    kan_stack_group_allocator_init (&state->temporary_allocator, state->my_allocation_group,
                                    KAN_UNIVERSE_RESOURCE_REFERENCE_TEMPORARY_STACK);
}

static inline void reset_outer_references_operation (struct resource_reference_manager_state_t *state,
                                                     struct resource_outer_references_operation_t *operation)
{
    if (operation->state == RESOURCE_OUTER_REFERENCES_OPERATION_STATE_WAITING_RESOURCE)
    {
        KAN_UP_VALUE_DELETE (resource_request, kan_resource_request_t, request_id, &operation->resource_request_id)
        {
            KAN_UP_ACCESS_DELETE (resource_request);
        }
    }

    operation->state = RESOURCE_OUTER_REFERENCES_OPERATION_STATE_REQUESTED;
}

static inline void send_outer_references_operation_response (struct resource_reference_manager_state_t *state,
                                                             kan_interned_string_t type,
                                                             kan_interned_string_t name,
                                                             kan_bool_t successful,
                                                             kan_resource_entry_id_t entry_attachment_id)
{
    KAN_UP_EVENT_INSERT (event, kan_resource_update_outer_references_response_event_t)
    {
        event->type = type;
        event->name = name;
        event->successful = successful;
        event->entry_attachment_id = entry_attachment_id;
    }
}

static inline void send_all_references_to_type_operation_response (struct resource_reference_manager_state_t *state,
                                                                   kan_interned_string_t type,
                                                                   kan_bool_t successful)
{
    KAN_UP_EVENT_INSERT (event, kan_resource_update_all_references_to_type_response_event_t)
    {
        event->type = type;
        event->successful = successful;
    }
}

static inline void delete_all_ongoing_operations (struct resource_reference_manager_state_t *state)
{
    KAN_UP_SEQUENCE_WRITE (outer_operation, resource_outer_references_operation_t)
    {
        reset_outer_references_operation (state, outer_operation);
        send_outer_references_operation_response (state, outer_operation->type, outer_operation->name, KAN_FALSE,
                                                  KAN_TYPED_ID_32_SET_INVALID (kan_resource_entry_id_t));
        KAN_UP_ACCESS_DELETE (outer_operation);
    }

    KAN_UP_SEQUENCE_WRITE (all_operation, resource_all_references_to_type_operation_t)
    {
        send_all_references_to_type_operation_response (state, all_operation->type, KAN_FALSE);
        KAN_UP_ACCESS_DELETE (all_operation);
    }
}

static inline void add_outer_references_operation_for_entry (struct resource_reference_manager_state_t *state,
                                                             const struct kan_resource_native_entry_t *entry,
                                                             kan_interned_string_t all_references_to_type)
{
    {
        KAN_UP_VALUE_UPDATE (operation, resource_outer_references_operation_t, entry_attachment_id,
                             &entry->attachment_id)
        {
            reset_outer_references_operation (state, operation);
            if (all_references_to_type)
            {
                kan_bool_t binding_found = KAN_FALSE;
                KAN_UP_VALUE_READ (binding, resource_outer_references_operation_binding_t, entry_attachment_id,
                                   &entry->attachment_id)
                {
                    if (binding->all_references_to_type == all_references_to_type)
                    {
                        binding_found = KAN_TRUE;
                        KAN_UP_QUERY_BREAK;
                    }
                }

                if (!binding_found)
                {
                    KAN_UP_INDEXED_INSERT (new_binding, resource_outer_references_operation_binding_t)
                    {
                        new_binding->entry_attachment_id = entry->attachment_id;
                        new_binding->all_references_to_type = all_references_to_type;
                    }
                }
            }

            KAN_UP_QUERY_RETURN_VOID;
        }
    }

    KAN_UP_INDEXED_INSERT (operation, resource_outer_references_operation_t)
    {
        operation->entry_attachment_id = entry->attachment_id;
        operation->type = entry->type;
        operation->name = entry->name;
        operation->state = RESOURCE_OUTER_REFERENCES_OPERATION_STATE_REQUESTED;
    }

    if (all_references_to_type)
    {
        KAN_UP_INDEXED_INSERT (binding, resource_outer_references_operation_binding_t)
        {
            binding->entry_attachment_id = entry->attachment_id;
            binding->all_references_to_type = all_references_to_type;
        }
    }
}

static inline void add_all_references_to_type (struct resource_reference_manager_state_t *state,
                                               kan_interned_string_t type)
{
    kan_bool_t found = KAN_FALSE;
    KAN_UP_SEQUENCE_UPDATE (operation, resource_all_references_to_type_operation_t)
    {
        if (operation->type == type)
        {
            found = KAN_TRUE;
            // Reset successful flag.
            operation->successful = KAN_TRUE;
            KAN_UP_QUERY_BREAK;
        }
    }

    if (!found)
    {
        KAN_UP_INDEXED_INSERT (new_operation, resource_all_references_to_type_operation_t)
        {
            new_operation->type = type;
            new_operation->successful = KAN_TRUE;
        }
    }

    if (type)
    {
        const struct kan_resource_reference_type_info_node_t *type_node =
            kan_resource_reference_type_info_storage_query (&state->info_storage, type);

        if (type_node)
        {
            for (kan_loop_size_t index = 0u; index < type_node->referencer_types.size; ++index)
            {
                kan_interned_string_t referencer_type =
                    ((kan_interned_string_t *) type_node->referencer_types.data)[index];
                KAN_UP_VALUE_READ (entry, kan_resource_native_entry_t, type, &referencer_type)
                {
                    add_outer_references_operation_for_entry (state, entry, type);
                }
            }
        }
    }
    else
    {
        for (kan_loop_size_t index = 0u; index < state->info_storage.third_party_referencers.size; ++index)
        {
            kan_interned_string_t referencer_type =
                ((kan_interned_string_t *) state->info_storage.third_party_referencers.data)[index];
            KAN_UP_VALUE_READ (entry, kan_resource_native_entry_t, type, &referencer_type)
            {
                add_outer_references_operation_for_entry (state, entry, type);
            }
        }
    }
}

static inline void fail_all_references_to_type_operation (struct resource_reference_manager_state_t *state,
                                                          kan_resource_entry_id_t failed_outer_references_operation_id)
{
    KAN_UP_VALUE_READ (binding, resource_outer_references_operation_binding_t, entry_attachment_id,
                       &failed_outer_references_operation_id)
    {
        KAN_UP_SEQUENCE_UPDATE (operation, resource_all_references_to_type_operation_t)
        {
            if (operation->type == binding->all_references_to_type)
            {
                operation->successful = KAN_FALSE;
                KAN_UP_QUERY_BREAK;
            }
        }
    }
}

static inline kan_time_size_t get_last_outer_reference_update_file_time_ns (
    struct resource_reference_manager_state_t *state, const struct resource_outer_references_operation_t *operation)
{
    KAN_UP_VALUE_READ (update_state, resource_outer_reference_update_state_t, attachment_id,
                       &operation->entry_attachment_id)
    {
        KAN_UP_QUERY_RETURN_VALUE (kan_time_size_t, update_state->last_update_file_time_ns);
    }

    return 0u;
}

static inline void construct_cache_file_directory (struct resource_reference_manager_state_t *state,
                                                   const struct resource_outer_references_operation_t *operation,
                                                   struct kan_file_system_path_container_t *output)
{
    kan_file_system_path_container_copy_string (output, state->workspace_directory_path);
    kan_file_system_path_container_append (output, operation->type);
}

static inline void construct_cache_file_path (struct resource_reference_manager_state_t *state,
                                              const struct resource_outer_references_operation_t *operation,
                                              struct kan_file_system_path_container_t *output)
{
    kan_file_system_path_container_copy_string (output, state->workspace_directory_path);
    kan_file_system_path_container_append (output, operation->type);
    kan_file_system_path_container_append (output, operation->name);
}

static inline kan_time_size_t get_last_outer_reference_cache_file_time_ns (
    struct resource_reference_manager_state_t *state, const char *cache_path, kan_virtual_file_system_volume_t volume)
{
    if (kan_virtual_file_system_check_existence (volume, cache_path))
    {
        struct kan_virtual_file_system_entry_status_t status;
        if (kan_virtual_file_system_query_entry (volume, cache_path, &status))
        {
            return status.last_modification_time_ns;
        }
        else
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_resource_reference, KAN_LOG_ERROR,
                                 "Failed to get last modification time of cache file \"%s\".", cache_path)
        }
    }

    return 0u;
}

static inline kan_time_size_t get_last_outer_reference_source_file_time_ns (
    struct resource_reference_manager_state_t *state,
    const struct kan_resource_native_entry_t *entry,
    kan_virtual_file_system_volume_t volume)
{
    struct kan_virtual_file_system_entry_status_t status;
    if (kan_virtual_file_system_query_entry (volume, entry->path, &status))
    {
        return status.last_modification_time_ns;
    }
    else
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_resource_reference, KAN_LOG_ERROR,
                             "Failed to get last modification time of resource file \"%s\".", entry->path)
    }

    return 0u;
}

static inline void publish_references (struct resource_reference_manager_state_t *state,
                                       const struct kan_resource_native_entry_t *entry,
                                       const struct kan_resource_detected_reference_container_t *container,
                                       kan_time_size_t file_time_ns)
{
    kan_loop_size_t reference_output_index = 0u;
    KAN_UP_VALUE_WRITE (old_reference, kan_resource_native_entry_outer_reference_t, attachment_id,
                        &entry->attachment_id)
    {
        if (reference_output_index < container->detected_references.size)
        {
            struct kan_resource_detected_reference_t *detected =
                &((struct kan_resource_detected_reference_t *)
                      container->detected_references.data)[reference_output_index];

            old_reference->reference_type = detected->type;
            old_reference->reference_name = detected->name;
            ++reference_output_index;
        }
        else
        {
            KAN_UP_ACCESS_DELETE (old_reference);
        }
    }

    while (reference_output_index < container->detected_references.size)
    {
        struct kan_resource_detected_reference_t *detected =
            &((struct kan_resource_detected_reference_t *) container->detected_references.data)[reference_output_index];

        KAN_UP_INDEXED_INSERT (new_reference, kan_resource_native_entry_outer_reference_t)
        {
            new_reference->attachment_id = entry->attachment_id;
            new_reference->reference_type = detected->type;
            new_reference->reference_name = detected->name;
        }

        ++reference_output_index;
    }

    KAN_UP_VALUE_UPDATE (update_state, resource_outer_reference_update_state_t, attachment_id, &entry->attachment_id)
    {
        update_state->last_update_file_time_ns = file_time_ns;
        KAN_UP_QUERY_RETURN_VOID;
    }

    KAN_UP_INDEXED_INSERT (new_update_state, resource_outer_reference_update_state_t)
    {
        new_update_state->attachment_id = entry->attachment_id;
        new_update_state->last_update_file_time_ns = file_time_ns;
    }
}

static inline kan_bool_t update_references_from_cache (struct resource_reference_manager_state_t *state,
                                                       const struct kan_resource_native_entry_t *entry,
                                                       const char *cache_path)
{
    kan_virtual_file_system_volume_t volume =
        kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);
    struct kan_stream_t *input_stream = kan_virtual_file_stream_open_for_read (volume, cache_path);
    kan_bool_t successful = KAN_TRUE;

    if (input_stream)
    {
        input_stream =
            kan_random_access_stream_buffer_open_for_read (input_stream, KAN_UNIVERSE_RESOURCE_REFERENCE_IO_BUFFER);
        struct kan_resource_detected_reference_container_t container;
        kan_resource_detected_reference_container_init (&container);

        kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
            input_stream, &container, kan_string_intern ("kan_resource_detected_reference_container_t"),
            state->binary_script_storage, KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t),
            container.detected_references.allocation_group);

        enum kan_serialization_state_t serialization_state;
        while ((serialization_state = kan_serialization_binary_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
        {
        }

        kan_serialization_binary_reader_destroy (reader);
        input_stream->operations->close (input_stream);

        if (serialization_state == KAN_SERIALIZATION_FINISHED)
        {
            struct kan_virtual_file_system_entry_status_t status;
            if (kan_virtual_file_system_query_entry (volume, cache_path, &status))
            {
                publish_references (state, entry, &container, status.last_modification_time_ns);
            }
            else
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_resource_reference, KAN_LOG_ERROR,
                                     "Failed to get status of cache file \"%s\".", cache_path)
                successful = KAN_FALSE;
            }
        }
        else
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_resource_reference, KAN_LOG_ERROR,
                                 "Failed to deserialize cache file \"%s\".", cache_path)
            successful = KAN_FALSE;
        }

        kan_resource_detected_reference_container_shutdown (&container);
    }
    else
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_resource_reference, KAN_LOG_ERROR,
                             "Failed to open cache file \"%s\" for read.", cache_path)
        successful = KAN_FALSE;
    }

    kan_virtual_file_system_close_context_read_access (state->virtual_file_system);
    return successful;
}

static inline kan_time_size_t write_references_to_cache (
    struct resource_reference_manager_state_t *state,
    const struct resource_outer_references_operation_t *operation,
    const struct kan_resource_detected_reference_container_t *container)
{
    struct kan_file_system_path_container_t cache_path;

    kan_virtual_file_system_volume_t volume =
        kan_virtual_file_system_get_context_volume_for_write (state->virtual_file_system);

    construct_cache_file_directory (state, operation, &cache_path);
    kan_virtual_file_system_make_directory (volume, cache_path.path);
    construct_cache_file_path (state, operation, &cache_path);
    struct kan_stream_t *stream = kan_virtual_file_stream_open_for_write (volume, cache_path.path);

    if (stream)
    {
        stream = kan_random_access_stream_buffer_open_for_write (stream, KAN_UNIVERSE_RESOURCE_REFERENCE_IO_BUFFER);
        kan_serialization_binary_writer_t writer = kan_serialization_binary_writer_create (
            stream, container, kan_string_intern ("kan_resource_detected_reference_container_t"),
            state->binary_script_storage, KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));

        enum kan_serialization_state_t serialization_state;
        while ((serialization_state = kan_serialization_binary_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
        {
        }

        kan_serialization_binary_writer_destroy (writer);
        stream->operations->close (stream);

        if (serialization_state == KAN_SERIALIZATION_FAILED)
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_resource_reference, KAN_LOG_ERROR,
                                 "Failed to write cache file \"%s\".", cache_path.path)
            kan_virtual_file_system_remove_file (volume, cache_path.path);
        }
    }
    else
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_resource_reference, KAN_LOG_ERROR,
                             "Failed to open cache file \"%s\" for write.", cache_path.path)
    }

    struct kan_virtual_file_system_entry_status_t status;
    if (kan_virtual_file_system_query_entry (volume, cache_path.path, &status))
    {
        kan_virtual_file_system_close_context_write_access (state->virtual_file_system);
        return status.last_modification_time_ns;
    }
    else
    {
        kan_virtual_file_system_close_context_write_access (state->virtual_file_system);
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, universe_resource_reference, KAN_LOG_ERROR,
                             "Failed to get status of cache file \"%s\".", cache_path.path)
        return 0u;
    }
}

static void process_outer_reference_operation_in_requested_state (
    struct resource_reference_manager_state_t *state,
    struct resource_outer_references_operation_t *operation,
    struct kan_repository_indexed_sequence_write_access_t *operation_access,
    const struct kan_resource_native_entry_t *entry)
{
    struct kan_file_system_path_container_t cache_path;
    construct_cache_file_path (state, operation, &cache_path);

    const kan_time_size_t transient_update_time_ns = get_last_outer_reference_update_file_time_ns (state, operation);
    const kan_time_size_t plugin_update_time_ns =
        KAN_HANDLE_IS_VALID (state->plugin_system) ?
            kan_plugin_system_get_newest_loaded_plugin_last_modification_file_time_ns (state->plugin_system) :
            0u;

    kan_virtual_file_system_volume_t volume =
        kan_virtual_file_system_get_context_volume_for_read (state->virtual_file_system);
    const kan_time_size_t cache_update_time_ns =
        get_last_outer_reference_cache_file_time_ns (state, cache_path.path, volume);
    const kan_time_size_t source_update_time_ns = get_last_outer_reference_source_file_time_ns (state, entry, volume);
    kan_virtual_file_system_close_context_read_access (state->virtual_file_system);

    const kan_bool_t cache_is_up_to_date =
        cache_update_time_ns > source_update_time_ns && cache_update_time_ns > plugin_update_time_ns;

    const kan_bool_t update_not_needed = transient_update_time_ns > cache_update_time_ns && cache_is_up_to_date;

    if (update_not_needed)
    {
        send_outer_references_operation_response (state, operation->type, operation->name, KAN_TRUE,
                                                  entry->attachment_id);
        reset_outer_references_operation (state, operation);
        kan_repository_indexed_sequence_write_access_delete (operation_access);
        return;
    }

    if (cache_is_up_to_date)
    {
        kan_bool_t successful = update_references_from_cache (state, entry, cache_path.path);
        send_outer_references_operation_response (state, operation->type, operation->name, successful,
                                                  entry->attachment_id);

        if (!successful)
        {
            fail_all_references_to_type_operation (state, operation->entry_attachment_id);
        }

        reset_outer_references_operation (state, operation);
        kan_repository_indexed_sequence_write_access_delete (operation_access);
        return;
    }

    KAN_UP_SINGLETON_READ (provider, kan_resource_provider_singleton_t)
    {
        KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (provider);
            request->type = entry->type;
            request->name = entry->name;
            // For now, we don't have special priorities for resource loading for reference scan.
            request->priority = 0u;

            operation->state = RESOURCE_OUTER_REFERENCES_OPERATION_STATE_WAITING_RESOURCE;
            operation->resource_request_id = request->request_id;
        }
    }

    kan_repository_indexed_sequence_write_access_close (operation_access);
}

static void process_outer_reference_operation_in_waiting_resource_state (
    struct resource_reference_manager_state_t *state,
    struct resource_outer_references_operation_t *operation,
    struct kan_repository_indexed_sequence_write_access_t *operation_access,
    const struct kan_resource_native_entry_t *entry)
{
    KAN_UP_VALUE_DELETE (request, kan_resource_request_t, request_id, &operation->resource_request_id)
    {
        if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
        {
            struct resource_reference_manager_native_container_type_data_t *type_data =
                query_resource_type_data (state, operation->type);

            if (!type_data)
            {
                KAN_LOG (universe_resource_reference, KAN_LOG_ERROR,
                         "Failed to process outer references request for entry \"%s\" of type \"%s\": its type is not "
                         "registered among accessible resource types due to internal error.",
                         operation->name, operation->type)

                kan_repository_indexed_sequence_write_access_delete (operation_access);
                KAN_UP_QUERY_RETURN_VOID;
            }

            struct kan_repository_indexed_value_read_cursor_t container_cursor =
                kan_repository_indexed_value_read_query_execute (&type_data->read_by_id_query,
                                                                 &request->provided_container_id);

            struct kan_repository_indexed_value_read_access_t container_access =
                kan_repository_indexed_value_read_cursor_next (&container_cursor);
            kan_repository_indexed_value_read_cursor_close (&container_cursor);

            const uint8_t *container_data = kan_repository_indexed_value_read_access_resolve (&container_access);
            KAN_ASSERT (container_data)

            const kan_memory_size_t instance_offset = kan_apply_alignment (
                offsetof (struct kan_resource_container_view_t, data_begin), type_data->contained_type_alignment);
            const void *instance_data = container_data + instance_offset;

            struct kan_resource_detected_reference_container_t reference_container;
            kan_resource_detected_reference_container_init (&reference_container);
            kan_resource_detect_references (&state->info_storage, operation->type, instance_data, &reference_container);

            const kan_time_size_t cache_file_time = write_references_to_cache (state, operation, &reference_container);
            publish_references (state, entry, &reference_container, cache_file_time);
            kan_resource_detected_reference_container_shutdown (&reference_container);

            send_outer_references_operation_response (state, operation->type, operation->name, KAN_TRUE,
                                                      entry->attachment_id);
            kan_repository_indexed_value_read_access_close (&container_access);
            kan_repository_indexed_sequence_write_access_delete (operation_access);
        }
        else
        {
            kan_repository_indexed_sequence_write_access_close (operation_access);
        }

        KAN_UP_QUERY_RETURN_VOID;
    }

    KAN_LOG (universe_resource_reference, KAN_LOG_ERROR,
             "Failed to process outer references request for entry \"%s\" of type \"%s\": lost resource "
             "request due to internal error.",
             operation->name, operation->type)

    send_outer_references_operation_response (state, operation->type, operation->name, KAN_FALSE, entry->attachment_id);
    fail_all_references_to_type_operation (state, operation->entry_attachment_id);
    reset_outer_references_operation (state, operation);
    kan_repository_indexed_sequence_write_access_delete (operation_access);
}

static void execute_shared_serve (kan_functor_user_data_t user_data)
{
    struct resource_reference_manager_state_t *state = (struct resource_reference_manager_state_t *) user_data;
    while (KAN_TRUE)
    {
        // Currently operation execution is done in synchronous manner, because this logic is editor-only and
        // 10-20ms hitches can be okay in most cases when searching references in editor (at least it is considered
        // okay in UE5). Therefore, I've decided to simplify implementation and avoid asynchronous IO here.

        if (kan_platform_get_elapsed_nanoseconds () > state->execution_shared_state.end_time_ns)
        {
            break;
        }

        kan_atomic_int_lock (&state->execution_shared_state.concurrency_lock);
        struct kan_repository_indexed_sequence_write_access_t operation_access =
            kan_repository_indexed_sequence_write_cursor_next (
                &state->execution_shared_state.outer_reference_operation_cursor);
        kan_atomic_int_unlock (&state->execution_shared_state.concurrency_lock);

        struct resource_outer_references_operation_t *operation =
            kan_repository_indexed_sequence_write_access_resolve (&operation_access);

        if (!operation)
        {
            break;
        }

        // Check entry existence and delete request if it is outdated.
        kan_bool_t outdated = KAN_TRUE;

        KAN_UP_VALUE_READ (entry, kan_resource_native_entry_t, attachment_id, &operation->entry_attachment_id)
        {
            outdated = KAN_FALSE;
            switch (operation->state)
            {
            case RESOURCE_OUTER_REFERENCES_OPERATION_STATE_REQUESTED:
            {
                process_outer_reference_operation_in_requested_state (state, operation, &operation_access, entry);
                break;
            }

            case RESOURCE_OUTER_REFERENCES_OPERATION_STATE_WAITING_RESOURCE:
            {
                process_outer_reference_operation_in_waiting_resource_state (state, operation, &operation_access,
                                                                             entry);
                break;
            }
            }

            KAN_UP_QUERY_BREAK;
        }

        if (outdated)
        {
            KAN_LOG (universe_resource_reference, KAN_LOG_ERROR,
                     "Failed to process outer references request as its entry \"%s\" of type \"%s\" does not exist.",
                     operation->name, operation->type)

            send_outer_references_operation_response (state, operation->type, operation->name, KAN_FALSE,
                                                      KAN_TYPED_ID_32_SET_INVALID (kan_resource_entry_id_t));
            fail_all_references_to_type_operation (state, operation->entry_attachment_id);
            reset_outer_references_operation (state, operation);
            kan_repository_indexed_sequence_write_access_delete (&operation_access);
            continue;
        }
    }

    if (kan_atomic_int_add (&state->execution_shared_state.workers_left, -1) == 1)
    {
        kan_repository_indexed_sequence_write_cursor_close (
            &state->execution_shared_state.outer_reference_operation_cursor);
    }
}

UNIVERSE_RESOURCE_REFERENCE_KAN_API void mutator_template_execute_resource_reference_manager (
    kan_cpu_job_t job, struct resource_reference_manager_state_t *state)
{
    const kan_time_size_t begin_time = kan_platform_get_elapsed_nanoseconds ();
    if (state->need_to_cancel_old_operations)
    {
        delete_all_ongoing_operations (state);
        state->need_to_cancel_old_operations = KAN_FALSE;
    }

    KAN_UP_SINGLETON_READ (provider, kan_resource_provider_singleton_t)
    {
        if (!provider->scan_done)
        {
            // If provider scan is not done, we cannot do anything.
            // In case of rescan (in the editor due to remounts, for example), we should cancel all operations.
            delete_all_ongoing_operations (state);
            KAN_UP_MUTATOR_RETURN;
        }
    }

    KAN_UP_EVENT_FETCH (outer_references_request, kan_resource_update_outer_references_request_event_t)
    {
        kan_bool_t found = KAN_FALSE;
        KAN_UP_VALUE_READ (entry, kan_resource_native_entry_t, name, &outer_references_request->name)
        {
            if (entry->type == outer_references_request->type)
            {
                found = KAN_TRUE;
                add_outer_references_operation_for_entry (state, entry, NULL);
                KAN_UP_QUERY_BREAK;
            }
        }

        if (!found)
        {
            KAN_LOG (universe_resource_reference, KAN_LOG_ERROR,
                     "Unable to find native resource \"%s\" of type \"%s\" in order to collect its outer references.",
                     outer_references_request->name, outer_references_request->type)
            send_outer_references_operation_response (state, outer_references_request->type,
                                                      outer_references_request->name, KAN_FALSE,
                                                      KAN_TYPED_ID_32_SET_INVALID (kan_resource_entry_id_t));
        }
    }

    KAN_UP_EVENT_FETCH (all_references_request, kan_resource_update_all_references_to_type_request_event_t)
    {
        add_all_references_to_type (state, all_references_request->type);
    }

    // Mark all reference to type requests as ready of they have no attached operations.
    KAN_UP_SEQUENCE_WRITE (operation, resource_all_references_to_type_operation_t)
    {
        kan_bool_t any_binding = KAN_FALSE;
        KAN_UP_SEQUENCE_READ (binding, resource_outer_references_operation_binding_t)
        {
            if (binding->all_references_to_type == operation->type)
            {
                any_binding = KAN_TRUE;
                KAN_UP_QUERY_BREAK;
            }
        }

        if (!any_binding)
        {
            send_all_references_to_type_operation_response (state, operation->type, operation->successful);
            KAN_UP_ACCESS_DELETE (operation);
        }
    }

    // Process operations.

    kan_stack_group_allocator_reset (&state->temporary_allocator);
    const kan_instance_size_t cpu_count = kan_platform_get_cpu_logical_core_count ();
    struct kan_cpu_task_list_node_t *task_list_node = NULL;

    state->execution_shared_state.workers_left = kan_atomic_int_init ((int) cpu_count);
    state->execution_shared_state.concurrency_lock = kan_atomic_int_init (0);
    state->execution_shared_state.outer_reference_operation_cursor =
        kan_repository_indexed_sequence_write_query_execute (
            &state->write_sequence__resource_outer_references_operation);
    state->execution_shared_state.end_time_ns = begin_time + state->budget_ns;

    for (kan_loop_size_t worker_index = 0u; worker_index < cpu_count; ++worker_index)
    {
        KAN_CPU_TASK_LIST_USER_VALUE (&task_list_node, &state->temporary_allocator,
                                      state->interned_resource_reference_manager_server, execute_shared_serve, state)
    }

    kan_cpu_job_dispatch_and_detach_task_list (job, task_list_node);
    KAN_UP_MUTATOR_RETURN;
}

UNIVERSE_RESOURCE_REFERENCE_KAN_API void mutator_template_undeploy_resource_reference_manager (
    struct resource_reference_manager_state_t *state)
{
    kan_serialization_binary_script_storage_destroy (state->binary_script_storage);
    kan_resource_reference_type_info_storage_shutdown (&state->info_storage);
    kan_stack_group_allocator_shutdown (&state->temporary_allocator);
}

UNIVERSE_RESOURCE_REFERENCE_KAN_API void resource_reference_manager_state_shutdown (
    struct resource_reference_manager_state_t *instance)
{
}

UNIVERSE_RESOURCE_REFERENCE_KAN_API void kan_reflection_generator_universe_resource_reference_init (
    struct kan_reflection_generator_universe_resource_reference_t *instance)
{
    instance->generated_reflection_group =
        kan_allocation_group_get_child (kan_allocation_group_stack_get (), "generated_reflection");
    instance->first_resource_type = NULL;
    instance->resource_types_count = 0u;
    instance->interned_kan_resource_resource_type_meta_t = kan_string_intern ("kan_resource_resource_type_meta_t");
}

UNIVERSE_RESOURCE_REFERENCE_KAN_API void kan_reflection_generator_universe_resource_reference_bootstrap (
    struct kan_reflection_generator_universe_resource_reference_t *instance, kan_loop_size_t bootstrap_iteration)
{
    instance->boostrap_iteration = bootstrap_iteration;
}

static inline void reflection_generation_iteration_check_type (
    struct kan_reflection_generator_universe_resource_reference_t *instance,
    kan_reflection_registry_t registry,
    const struct kan_reflection_struct_t *type,
    const struct kan_resource_resource_type_meta_t *meta,
    kan_reflection_system_generation_iterator_t generation_iterator)
{
    struct universe_resource_reference_type_node_t *node =
        (struct universe_resource_reference_type_node_t *) kan_allocate_batched (
            instance->generated_reflection_group, sizeof (struct universe_resource_reference_type_node_t));

    node->resource_type = type;
    node->next = instance->first_resource_type;
    instance->first_resource_type = node;
    ++instance->resource_types_count;
}

UNIVERSE_RESOURCE_REFERENCE_KAN_API void kan_reflection_generator_universe_resource_reference_iterate (
    struct kan_reflection_generator_universe_resource_reference_t *instance,
    kan_reflection_registry_t registry,
    kan_reflection_system_generation_iterator_t iterator,
    kan_loop_size_t iteration_index)
{
    KAN_UNIVERSE_REFLECTION_GENERATOR_ITERATE_TYPES_WITH_META (
        struct kan_resource_resource_type_meta_t, instance->interned_kan_resource_resource_type_meta_t,
        reflection_generation_iteration_check_type, struct universe_resource_reference_type_node_t, first_resource_type,
        resource_type)
}

static inline void generated_mutator_init_node (
    struct resource_reference_manager_native_container_type_data_t *mutator_node,
    struct universe_resource_reference_type_node_t *generated_node)
{
    char type_name_buffer[256u];
    snprintf (type_name_buffer, 256u, "%s%s", KAN_RESOURCE_PROVIDER_CONTAINER_TYPE_PREFIX,
              generated_node->resource_type->name);
    mutator_node->contained_type_name = generated_node->resource_type->name;
    mutator_node->container_type_name = kan_string_intern (type_name_buffer);
    mutator_node->contained_type_alignment = generated_node->resource_type->alignment;
}

static inline void generated_mutator_deploy_node (kan_repository_t world_repository,
                                                  struct resource_reference_manager_native_container_type_data_t *node)
{
    kan_repository_indexed_storage_t storage =
        kan_repository_indexed_storage_open (world_repository, node->container_type_name);

    const char *container_id_name = "container_id";
    struct kan_repository_field_path_t container_id_path = {
        .reflection_path_length = 1u,
        &container_id_name,
    };

    kan_repository_indexed_value_read_query_init (&node->read_by_id_query, storage, container_id_path);
}

static inline void generated_mutator_undeploy_node (
    struct resource_reference_manager_native_container_type_data_t *node)
{
    kan_repository_indexed_value_read_query_shutdown (&node->read_by_id_query);
}

static inline void generated_mutator_shutdown_node (
    struct resource_reference_manager_native_container_type_data_t *node)
{
}

// \c_interface_scanner_disable
KAN_UNIVERSE_REFLECTION_GENERATOR_MUTATOR_FUNCTIONS (generated_mutator,
                                                     struct kan_reflection_generator_universe_resource_reference_t,
                                                     struct universe_resource_reference_type_node_t,
                                                     instance->resource_types_count,
                                                     instance->first_resource_type,
                                                     struct resource_reference_manager_state_t,
                                                     struct resource_reference_manager_native_container_type_data_t,
                                                     resource_reference_manager_state_init,
                                                     mutator_template_deploy_resource_reference_manager,
                                                     mutator_template_execute_resource_reference_manager,
                                                     mutator_template_undeploy_resource_reference_manager,
                                                     resource_reference_manager_state_shutdown)
// \c_interface_scanner_enable

UNIVERSE_RESOURCE_REFERENCE_KAN_API void kan_reflection_generator_universe_resource_reference_finalize (
    struct kan_reflection_generator_universe_resource_reference_t *instance, kan_reflection_registry_t registry)
{
    if (instance->resource_types_count > 0u)
    {
#define KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_LESS(first_index, second_index)                              \
    (KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY[first_index]->resource_type->name <                       \
     KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_ARRAY[second_index]->resource_type->name)

        KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES (
            instance->resource_types_count, struct universe_resource_reference_type_node_t,
            instance->first_resource_type, instance->generated_reflection_group);
#undef KAN_UNIVERSE_REFLECTION_GENERATOR_SORT_TYPE_NODES_LESS
    }

    KAN_UNIVERSE_REFLECTION_GENERATOR_FILL_MUTATOR (
        instance->mutator, "generated_resource_reference_manager_state_t", resource_reference_manager_state_t,
        resource_reference_manager_native_container_type_data_t, instance->resource_types_count,
        generated_resource_reference_manager, generated_mutator);

    kan_reflection_registry_add_struct (registry, &instance->mutator_type);
    kan_reflection_registry_add_function (registry, &instance->mutator_deploy_function);
    kan_reflection_registry_add_function (registry, &instance->mutator_execute_function);
    kan_reflection_registry_add_function_meta (registry, instance->mutator_execute_function.name,
                                               kan_string_intern ("kan_universe_mutator_group_meta_t"),
                                               &resource_reference_mutator_group);
    kan_reflection_registry_add_function (registry, &instance->mutator_undeploy_function);
}

UNIVERSE_RESOURCE_REFERENCE_KAN_API void kan_reflection_generator_universe_resource_reference_shutdown (
    struct kan_reflection_generator_universe_resource_reference_t *instance)
{
    struct universe_resource_reference_type_node_t *node = instance->first_resource_type;
    while (node)
    {
        struct universe_resource_reference_type_node_t *next = node->next;
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
