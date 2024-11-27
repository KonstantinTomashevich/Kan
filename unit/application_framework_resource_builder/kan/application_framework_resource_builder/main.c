#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/application_framework_resource_builder/target_byproduct_state.h>
#include <kan/application_framework_resource_tool/context.h>
#include <kan/application_framework_resource_tool/project.h>
#include <kan/container/hash_storage.h>
#include <kan/context/all_system_names.h>
#include <kan/context/context.h>
#include <kan/context/plugin_system.h>
#include <kan/context/reflection_system.h>
#include <kan/cpu_dispatch/job.h>
#include <kan/cpu_dispatch/task.h>
#include <kan/error/critical.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/path_container.h>
#include <kan/file_system/stream.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/platform/hardware.h>
#include <kan/reflection/generated_reflection.h>
#include <kan/reflection/struct_helpers.h>
#include <kan/resource_index/resource_index.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>
#include <kan/threading/atomic.h>
#include <kan/virtual_file_system/virtual_file_system.h>

#define ERROR_CODE_INCORRECT_ARGUMENTS -1
#define ERROR_CODE_FAILED_TO_READ_PROJECT -2
#define ERROR_CODE_FAILED_TO_LOCK_DIRECTORY -3
#define ERROR_CODE_VFS_SETUP_FAILURE -4
#define ERROR_CODE_FAILED_TO_LOAD_PLATFORM_CONFIGURATION -5
#define ERROR_CODE_FAILED_TO_SETUP_TARGETS -6
#define ERROR_CODE_FAILED_TO_SCAN_TARGETS -7
#define ERROR_CODE_FAILED_TO_COMPILE_RESOURCES -8
#define ERROR_CODE_FAILED_TO_INTERN_STRING -9
#define ERROR_CODE_FAILED_TO_PACK_TARGETS -10

#define VFS_TARGETS_DIRECTORY "targets"
#define VFS_RAW_REFERENCE_CACHE_DIRECTORY "reference_cache"
#define VFS_OUTPUT_DIRECTORY "output"

#define SUB_DIRECTORY_RAW_BYPRODUCT_CACHE "raw_byproduct_cache"
#define SUB_DIRECTORY_COMPILED_CACHE "compiled_cache"
#define SUB_DIRECTORY_COMPILED_REFERENCE_CACHE "compiled_reference_cache"
#define SUB_DIRECTORY_TEMPORARY "temporary"

#define BYPRODUCT_STATE_FILE_SUFFIX ".byproduct_state.bin"

KAN_LOG_DEFINE_CATEGORY (application_framework_resource_builder);

static kan_allocation_group_t reference_type_info_storage_allocation_group;
static kan_allocation_group_t platform_configuration_allocation_group;
static kan_allocation_group_t targets_allocation_group;
static kan_allocation_group_t nodes_allocation_group;
static kan_allocation_group_t loaded_native_entries_allocation_group;
static kan_allocation_group_t loaded_third_party_entries_allocation_group;
static kan_allocation_group_t compilation_state_allocation_group;
static kan_allocation_group_t temporary_allocation_group;

static kan_interned_string_t interned_kan_resource_resource_type_meta_t;
static kan_interned_string_t interned_kan_resource_compilable_meta_t;
static kan_interned_string_t interned_kan_resource_byproduct_type_meta_t;
static kan_interned_string_t interned_kan_resource_platform_configuration_t;
static kan_interned_string_t interned_kan_resource_target_byproduct_state_t;

static struct
{
    struct kan_stack_group_allocator_t temporary_allocator;
    kan_time_size_t newest_loaded_plugin_last_modification_file_time_ns;
    kan_virtual_file_system_volume_t volume;

    kan_reflection_registry_t registry;
    kan_serialization_binary_script_storage_t binary_script_storage;
    struct kan_resource_reference_type_info_storage_t reference_type_info_storage;

    struct kan_application_resource_project_t project;
    struct platform_configuration_t *platform_configurations;

    /// \meta reflection_dynamic_array_type = "struct target_t"
    struct kan_dynamic_array_t targets;

    struct kan_atomic_int_t compilation_queue_lock;
    struct native_entry_node_t *compilation_passive_queue_first;
    struct native_entry_node_t *compilation_passive_queue_last;
    struct native_entry_node_t *compilation_active_queue;

    struct kan_atomic_int_t compilation_resource_management_lock;
    struct native_entry_node_t *resource_management_native_queue;
    struct third_party_entry_node_t *resource_management_third_party_queue;

    struct kan_atomic_int_t errors_count;

} global = {
    .registry = KAN_HANDLE_INITIALIZE_INVALID,
    .platform_configurations = NULL,
    .compilation_passive_queue_first = NULL,
    .compilation_passive_queue_last = NULL,
    .compilation_active_queue = NULL,
    .resource_management_native_queue = NULL,
    .resource_management_third_party_queue = NULL,
};

struct platform_configuration_t
{
    struct platform_configuration_t *next;
    const struct kan_reflection_struct_t *type;
    void *data;
};

struct byproduct_production_source_t
{
    kan_interned_string_t resource_type;
    kan_interned_string_t resource_name;
};

struct byproduct_node_t
{
    struct kan_hash_storage_node_t node;
    struct native_entry_node_t *entry;

    /// \meta reflection_dynamic_array_type = "struct byproduct_production_source_t"
    struct kan_dynamic_array_t produced_from;
};

struct byproduct_production_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t resource_type;
    kan_interned_string_t resource_name;
    kan_interned_string_t produced_type;
    kan_interned_string_t produced_name;
};

struct target_t
{
    kan_interned_string_t name;
    struct kan_application_resource_target_t *source;
    kan_bool_t requested_for_build;

    struct kan_hash_storage_t native;
    struct kan_hash_storage_t third_party;
    kan_serialization_interned_string_registry_t interned_string_registry;

    /// \meta reflection_dynamic_array_type = "struct target_t *"
    struct kan_dynamic_array_t visible_targets;

    struct kan_atomic_int_t byproduct_index_generator;
    struct kan_atomic_int_t byproduct_registration_lock;
    struct kan_hash_storage_t byproducts;

    struct kan_hash_storage_t loaded_byproduct_production;
};

enum compilation_status_t
{
    COMPILATION_STATUS_NOT_YET = 0u,
    COMPILATION_STATUS_REQUESTED,
    COMPILATION_STATUS_LOADING_SOURCE_FOR_REFERENCES,
    COMPILATION_STATUS_WAITING_FOR_COMPILED_DEPENDENCIES,
    COMPILATION_STATUS_COMPILE,
    COMPILATION_STATUS_FINISHED,
    COMPILATION_STATUS_FAILED,
};

enum native_node_compilation_queue_t
{
    NATIVE_NODE_COMPILATION_QUEUE_NONE = 0u,
    NATIVE_NODE_COMPILATION_QUEUE_PASSIVE,
    NATIVE_NODE_COMPILATION_QUEUE_ACTIVE,
};

struct native_entry_node_t
{
    struct kan_hash_storage_node_t node;
    struct target_t *target;

    const struct kan_reflection_struct_t *source_type;
    const struct kan_reflection_struct_t *compiled_type;

    const struct kan_resource_compilable_meta_t *compilable_meta;
    const struct kan_resource_byproduct_type_meta_t *byproduct_meta;

    kan_interned_string_t name;
    char *source_path;
    char *compiled_path;

    void *source_data;
    void *compiled_data;

    struct kan_atomic_int_t source_references;
    struct kan_atomic_int_t compiled_references;

    enum compilation_status_t compilation_status;
    enum compilation_status_t pending_compilation_status;

    kan_bool_t should_be_included_in_pack;
    kan_bool_t loaded_references_from_cache;
    kan_bool_t queued_for_resource_management;

    enum native_node_compilation_queue_t compilation_queue;
    struct native_entry_node_t *next_node_in_compilation_queue;
    struct native_entry_node_t *previous_node_in_compilation_queue;
    struct native_entry_node_t *next_node_in_resource_management_queue;

    struct kan_resource_detected_reference_container_t source_detected_references;
    struct kan_resource_detected_reference_container_t compiled_detected_references;

    struct byproduct_node_t *linked_byproduct;
};

struct third_party_entry_node_t
{
    struct kan_hash_storage_node_t node;
    struct target_t *target;

    kan_interned_string_t name;
    char *path;
    kan_memory_size_t size;
    kan_memory_size_t allocation_size;
    void *data;
    struct kan_atomic_int_t references;

    kan_bool_t should_be_included_in_pack;
    kan_bool_t queued_for_resource_management;
    struct third_party_entry_node_t *next_node_in_resource_management_queue;
};

static inline const void *find_singular_struct_meta (kan_interned_string_t struct_name,
                                                     kan_interned_string_t meta_type_name)
{
    struct kan_reflection_struct_meta_iterator_t iterator =
        kan_reflection_registry_query_struct_meta (global.registry, struct_name, meta_type_name);
    return kan_reflection_struct_meta_iterator_get (&iterator);
}

static struct native_entry_node_t *native_entry_node_create (struct target_t *target,
                                                             kan_interned_string_t source_type_name,
                                                             kan_interned_string_t resource_name,
                                                             const char *source_path)
{
    const struct kan_reflection_struct_t *source_type =
        kan_reflection_registry_query_struct (global.registry, source_type_name);

    if (!source_type)
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                 "Failed to find resource type \"%s\" in registry.", source_type_name)
        kan_atomic_int_add (&global.errors_count, 1);
        return NULL;
    }

    const struct kan_resource_resource_type_meta_t *resource_type_meta =
        find_singular_struct_meta (source_type_name, interned_kan_resource_resource_type_meta_t);

    const struct kan_resource_compilable_meta_t *compilable_meta =
        find_singular_struct_meta (source_type_name, interned_kan_resource_compilable_meta_t);

    const struct kan_resource_byproduct_type_meta_t *byproduct_meta =
        find_singular_struct_meta (source_type_name, interned_kan_resource_byproduct_type_meta_t);

    if (!resource_type_meta && !byproduct_meta)
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                 "Found resource type \"%s\", but it has neither resource type nor byproduct type meta",
                 source_type_name)
        kan_atomic_int_add (&global.errors_count, 1);
        return NULL;
    }

    const struct kan_reflection_struct_t *compiled_type = source_type;
    if (compilable_meta)
    {
        compiled_type = kan_reflection_registry_query_struct (global.registry,
                                                              kan_string_intern (compilable_meta->output_type_name));

        if (!compiled_type)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                     "Resource type \"%s\" should be compiled into \"%s\", but there is no such type in registry.",
                     source_type_name, compilable_meta->output_type_name)
            kan_atomic_int_add (&global.errors_count, 1);
            return NULL;
        }
    }

    struct native_entry_node_t *node =
        kan_allocate_batched (nodes_allocation_group, sizeof (struct native_entry_node_t));

    node->node.hash = (kan_hash_t) resource_name;
    node->target = target;

    node->source_type = source_type;
    node->compiled_type = compiled_type;

    node->compilable_meta = compilable_meta;
    node->byproduct_meta = byproduct_meta;

    node->name = resource_name;
    const kan_instance_size_t source_path_length = (kan_instance_size_t) strlen (source_path);
    node->source_path = kan_allocate_general (nodes_allocation_group, source_path_length + 1u, _Alignof (char));
    memcpy (node->source_path, source_path, source_path_length + 1u);

    struct kan_file_system_path_container_t compiled_path;
    kan_file_system_path_container_copy_string (&compiled_path, VFS_OUTPUT_DIRECTORY "/" SUB_DIRECTORY_COMPILED_CACHE);
    kan_file_system_path_container_append (&compiled_path, node->compiled_type->name);

    kan_virtual_file_system_make_directory (global.volume, compiled_path.path);
    kan_file_system_path_container_append (&compiled_path, node->name);
    kan_file_system_path_container_add_suffix (&compiled_path, ".bin");

    node->compiled_path = kan_allocate_general (nodes_allocation_group, compiled_path.length + 1u, _Alignof (char));
    memcpy (node->compiled_path, compiled_path.path, compiled_path.length + 1u);

    node->source_data = NULL;
    node->compiled_data = NULL;

    node->source_references = kan_atomic_int_init (0);
    node->compiled_references = kan_atomic_int_init (0);

    node->compilation_status = COMPILATION_STATUS_NOT_YET;
    node->pending_compilation_status = COMPILATION_STATUS_NOT_YET;

    node->should_be_included_in_pack = resource_type_meta ? resource_type_meta->root : KAN_FALSE;
    node->loaded_references_from_cache = KAN_FALSE;
    node->queued_for_resource_management = KAN_FALSE;

    node->compilation_queue = NATIVE_NODE_COMPILATION_QUEUE_NONE;
    node->next_node_in_compilation_queue = NULL;
    node->previous_node_in_compilation_queue = NULL;
    node->next_node_in_resource_management_queue = NULL;

    kan_resource_detected_reference_container_init (&node->source_detected_references);
    kan_resource_detected_reference_container_init (&node->compiled_detected_references);

    node->linked_byproduct = NULL;
    return node;
}

static inline void *create_native_instance_for_loading (const struct kan_reflection_struct_t *type)
{
    void *data = kan_allocate_general (loaded_native_entries_allocation_group, type->size, type->alignment);
    if (type->init)
    {
        kan_allocation_group_stack_push (loaded_native_entries_allocation_group);
        type->init (type->functor_user_data, data);
        kan_allocation_group_stack_pop ();
    }

    return data;
}

static inline void destroy_loading_native_instance (void *data, const struct kan_reflection_struct_t *type)
{
    if (type->shutdown)
    {
        kan_allocation_group_stack_push (loaded_native_entries_allocation_group);
        type->shutdown (type->functor_user_data, data);
        kan_allocation_group_stack_pop ();
    }

    kan_free_general (loaded_native_entries_allocation_group, data, type->size);
}

static kan_bool_t load_native_data_internal (const struct kan_reflection_struct_t *type,
                                             const char *path,
                                             kan_serialization_interned_string_registry_t interned_string_registry,
                                             struct kan_stream_t *input_stream,
                                             void *data)
{
    kan_bool_t successful = KAN_TRUE;
    const kan_instance_size_t length = (kan_instance_size_t) strlen (path);

    if (length > 4u && path[length - 4u] == '.' && path[length - 3u] == 'b' && path[length - 2u] == 'i' &&
        path[length - 1u] == 'n')
    {
        kan_interned_string_t type_name;
        if (!kan_serialization_binary_read_type_header (input_stream, &type_name, interned_string_registry))
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Failed to read type header of \"%s\".",
                     path)
            kan_atomic_int_add (&global.errors_count, 1);
            successful = KAN_FALSE;
        }
        else if (type_name != type->name)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                     "Expected type \"%s\" at \"%s\", but got \"%s\".", type->name, path, type_name)
            kan_atomic_int_add (&global.errors_count, 1);
            successful = KAN_FALSE;
        }
        else
        {
            kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
                input_stream, data, type->name, global.binary_script_storage, interned_string_registry,
                loaded_native_entries_allocation_group);

            enum kan_serialization_state_t serialization_state;
            while ((serialization_state = kan_serialization_binary_reader_step (reader)) ==
                   KAN_SERIALIZATION_IN_PROGRESS)
            {
            }

            kan_serialization_binary_reader_destroy (reader);
            if (serialization_state == KAN_SERIALIZATION_FAILED)
            {
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                         "Failed to deserialize resource from \"%s\".", path)
                kan_atomic_int_add (&global.errors_count, 1);
                successful = KAN_FALSE;
            }
            else
            {
                KAN_ASSERT (serialization_state == KAN_SERIALIZATION_FINISHED)
            }
        }
    }
    else if (length > 3u && path[length - 3u] == '.' && path[length - 2u] == 'r' && path[length - 1u] == 'd')
    {
        kan_interned_string_t type_name;
        if (!kan_serialization_rd_read_type_header (input_stream, &type_name))
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Failed to read type header of \"%s\".",
                     path)
            kan_atomic_int_add (&global.errors_count, 1);
            successful = KAN_FALSE;
        }
        else if (type_name != type->name)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                     "Expected type \"%s\" at \"%s\", but got \"%s\".", type->name, path, type_name)
            kan_atomic_int_add (&global.errors_count, 1);
            successful = KAN_FALSE;
        }
        else
        {
            kan_serialization_rd_reader_t reader = kan_serialization_rd_reader_create (
                input_stream, data, type->name, global.registry, loaded_native_entries_allocation_group);

            enum kan_serialization_state_t serialization_state;
            while ((serialization_state = kan_serialization_rd_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
            {
            }

            kan_serialization_rd_reader_destroy (reader);
            if (serialization_state == KAN_SERIALIZATION_FAILED)
            {
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                         "Failed to deserialize resource from \"%s\".", path)
                kan_atomic_int_add (&global.errors_count, 1);
                successful = KAN_FALSE;
            }
            else
            {
                KAN_ASSERT (serialization_state == KAN_SERIALIZATION_FINISHED)
            }
        }
    }
    else
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                 "Failed to detect resource format from path \"%s\".", path)
        kan_atomic_int_add (&global.errors_count, 1);
        successful = KAN_FALSE;
    }

    return successful;
}

static inline void *load_native_data_into_new_allocation (
    const struct kan_reflection_struct_t *type,
    const char *path,
    kan_serialization_interned_string_registry_t interned_string_registry,
    struct kan_stream_t *input_stream)
{
    void *data = create_native_instance_for_loading (type);
    if (!load_native_data_internal (type, path, interned_string_registry, input_stream, data))
    {
        destroy_loading_native_instance (data, type);
        data = NULL;
    }

    return data;
}

static inline void *load_native_data (const struct kan_reflection_struct_t *type,
                                      const char *path,
                                      kan_serialization_interned_string_registry_t interned_string_registry)
{
    struct kan_stream_t *input_stream = kan_virtual_file_stream_open_for_read (global.volume, path);
    if (!input_stream)
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Failed to open resource at path \"%s\".", path)
        kan_atomic_int_add (&global.errors_count, 1);
        return NULL;
    }

    input_stream = kan_random_access_stream_buffer_open_for_read (input_stream, KAN_RESOURCE_BUILDER_IO_BUFFER);
    void *data = load_native_data_into_new_allocation (type, path, interned_string_registry, input_stream);
    input_stream->operations->close (input_stream);
    return data;
}

static inline void *load_native_data_from_real_file_system (
    const struct kan_reflection_struct_t *type,
    const char *path,
    kan_serialization_interned_string_registry_t interned_string_registry)
{
    struct kan_stream_t *input_stream = kan_direct_file_stream_open_for_read (path, KAN_TRUE);
    if (!input_stream)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder,
                             KAN_LOG_ERROR, "Failed to open resource at path \"%s\".", path)
        kan_atomic_int_add (&global.errors_count, 1);
        return NULL;
    }

    input_stream = kan_random_access_stream_buffer_open_for_read (input_stream, KAN_RESOURCE_BUILDER_IO_BUFFER);
    void *data = load_native_data_into_new_allocation (type, path, interned_string_registry, input_stream);
    input_stream->operations->close (input_stream);
    return data;
}

static inline kan_bool_t load_native_data_into_existent_allocation (
    const struct kan_reflection_struct_t *type,
    const char *path,
    kan_serialization_interned_string_registry_t interned_string_registry,
    void *data)
{
    struct kan_stream_t *input_stream = kan_virtual_file_stream_open_for_read (global.volume, path);
    if (!input_stream)
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Failed to open resource at path \"%s\".", path)
        kan_atomic_int_add (&global.errors_count, 1);
        return KAN_FALSE;
    }

    input_stream = kan_random_access_stream_buffer_open_for_read (input_stream, KAN_RESOURCE_BUILDER_IO_BUFFER);
    if (!load_native_data_internal (type, path, interned_string_registry, input_stream, data))
    {
        input_stream->operations->close (input_stream);
        return KAN_FALSE;
    }

    input_stream->operations->close (input_stream);
    return KAN_TRUE;
}

static kan_bool_t save_native_data (void *data,
                                    const char *path,
                                    kan_interned_string_t type_name,
                                    kan_serialization_interned_string_registry_t interned_string_registry,
                                    kan_bool_t with_header)
{
    struct kan_stream_t *stream = kan_virtual_file_stream_open_for_write (global.volume, path);
    if (!stream)
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Failed open \"%s\" for write.", path)
        kan_atomic_int_add (&global.errors_count, 1);
        return KAN_FALSE;
    }

    stream = kan_random_access_stream_buffer_open_for_write (stream, KAN_RESOURCE_BUILDER_IO_BUFFER);
    if (with_header)
    {
        if (!kan_serialization_binary_write_type_header (stream, type_name, interned_string_registry))
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Failed to write type header of \"%s\".",
                     path)
            kan_atomic_int_add (&global.errors_count, 1);
            stream->operations->close (stream);
            return KAN_FALSE;
        }
    }

    kan_serialization_binary_writer_t writer = kan_serialization_binary_writer_create (
        stream, data, type_name, global.binary_script_storage, interned_string_registry);

    enum kan_serialization_state_t serialization_state;
    while ((serialization_state = kan_serialization_binary_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    kan_serialization_binary_writer_destroy (writer);
    stream->operations->close (stream);

    if (serialization_state == KAN_SERIALIZATION_FAILED)
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Failed to serialize \"%s\".", path)
        kan_atomic_int_add (&global.errors_count, 1);
        return KAN_FALSE;
    }

    KAN_ASSERT (serialization_state == KAN_SERIALIZATION_FINISHED)
    return KAN_TRUE;
}

static void native_entry_node_unload_source (struct native_entry_node_t *node)
{
    if (node->source_data)
    {
        if (node->source_type->shutdown)
        {
            kan_allocation_group_stack_push (loaded_native_entries_allocation_group);
            node->source_type->shutdown (node->source_type->functor_user_data, node->source_data);
            kan_allocation_group_stack_pop ();
        }

        kan_free_general (loaded_native_entries_allocation_group, node->source_data, node->source_type->size);
        node->source_data = NULL;
    }
}

static void native_entry_node_unload_compiled (struct native_entry_node_t *node)
{
    if (node->compiled_data)
    {
        if (node->compiled_type->shutdown)
        {
            kan_allocation_group_stack_push (loaded_native_entries_allocation_group);
            node->compiled_type->shutdown (node->compiled_type->functor_user_data, node->compiled_data);
            kan_allocation_group_stack_pop ();
        }

        kan_free_general (loaded_native_entries_allocation_group, node->compiled_data, node->compiled_type->size);
        node->compiled_data = NULL;
    }
}

static void native_entry_node_destroy (struct native_entry_node_t *node)
{
    if (node->source_path)
    {
        kan_free_general (nodes_allocation_group, node->source_path, strlen (node->source_path) + 1u);
    }

    if (node->compiled_path)
    {
        kan_free_general (nodes_allocation_group, node->compiled_path, strlen (node->compiled_path) + 1u);
    }

    native_entry_node_unload_source (node);
    native_entry_node_unload_compiled (node);

    kan_resource_detected_reference_container_shutdown (&node->source_detected_references);
    kan_resource_detected_reference_container_shutdown (&node->compiled_detected_references);
}

static struct third_party_entry_node_t *third_party_entry_node_create (struct target_t *target,
                                                                       kan_interned_string_t resource_name,
                                                                       kan_memory_size_t size,
                                                                       const char *path)
{
    struct third_party_entry_node_t *node =
        kan_allocate_batched (nodes_allocation_group, sizeof (struct third_party_entry_node_t));

    node->node.hash = (kan_hash_t) resource_name;
    node->target = target;

    node->name = resource_name;
    const kan_instance_size_t path_length = (kan_instance_size_t) strlen (path);
    node->path = kan_allocate_general (nodes_allocation_group, path_length + 1u, _Alignof (char));
    memcpy (node->path, path, path_length + 1u);

    node->size = size;
    node->allocation_size = kan_apply_alignment (node->size, _Alignof (kan_memory_size_t));
    node->data = NULL;
    node->references = kan_atomic_int_init (0);

    node->should_be_included_in_pack = KAN_FALSE;
    node->queued_for_resource_management = KAN_FALSE;
    node->next_node_in_resource_management_queue = NULL;
    return node;
}

static void *load_third_party_data (struct third_party_entry_node_t *node)
{
    struct kan_stream_t *input_stream = kan_virtual_file_stream_open_for_read (global.volume, node->path);
    if (!input_stream)
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Unable to open resource file \"%s\".",
                 node->path)
        kan_atomic_int_add (&global.errors_count, 1);
        return NULL;
    }

    void *data = kan_allocate_general (loaded_third_party_entries_allocation_group, node->allocation_size,
                                       _Alignof (kan_memory_size_t));
    const kan_bool_t read = input_stream->operations->read (input_stream, node->size, data) == node->size;
    input_stream->operations->close (input_stream);

    if (!read)
    {
        kan_free_general (loaded_third_party_entries_allocation_group, data, node->size);
        data = NULL;
        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Failed to read data from \"%s\".", node->path)
        kan_atomic_int_add (&global.errors_count, 1);
    }

    return data;
}

static void third_party_entry_node_unload (struct third_party_entry_node_t *node)
{
    if (node->data)
    {
        kan_free_general (loaded_third_party_entries_allocation_group, node->data, node->allocation_size);
        node->data = NULL;
    }
}

static void third_party_entry_node_destroy (struct third_party_entry_node_t *node)
{
    if (node->path)
    {
        kan_free_general (nodes_allocation_group, node->path, strlen (node->path) + 1u);
    }

    third_party_entry_node_unload (node);
}

static void load_platform_configuration (struct kan_file_system_path_container_t *path)
{
    const struct kan_reflection_struct_t *file_type =
        kan_reflection_registry_query_struct (global.registry, interned_kan_resource_platform_configuration_t);
    KAN_ASSERT (file_type)

    struct kan_resource_platform_configuration_t *configuration = load_native_data_from_real_file_system (
        file_type, path->path, KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));

    if (!configuration)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder,
                             KAN_LOG_ERROR, "Failed to read platform configuration at \"%s\".", path->path)
        kan_atomic_int_add (&global.errors_count, 1);
        return;
    }

    if (configuration->parent)
    {
        // Step back to the directory level.
        const char *last_separator = strrchr (path->path, '/');

        if (last_separator)
        {
            kan_file_system_path_container_reset_length (path, (kan_instance_size_t) (last_separator - path->path));
        }
        else
        {
            kan_file_system_path_container_reset_length (path, 0u);
        }

        kan_file_system_path_container_append (path, configuration->parent);
        load_platform_configuration (path);
    }

    for (kan_loop_size_t index = 0u; index < configuration->configuration.size; ++index)
    {
        kan_reflection_patch_t patch = ((kan_reflection_patch_t *) configuration->configuration.data)[index];
        const struct kan_reflection_struct_t *patch_type = kan_reflection_patch_get_type (patch);

        if (!patch_type)
        {
            KAN_LOG_WITH_BUFFER (
                KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder, KAN_LOG_ERROR,
                "Platform configuration at \"%s\" contains patches with unknown or broken types.", path->path)
            kan_atomic_int_add (&global.errors_count, 1);
            break;
        }

        struct platform_configuration_t *instance = global.platform_configurations;
        while (instance)
        {
            if (instance->type == patch_type)
            {
                break;
            }

            instance = instance->next;
        }

        if (!instance)
        {
            instance = kan_allocate_batched (platform_configuration_allocation_group,
                                             sizeof (struct platform_configuration_t));
            instance->next = global.platform_configurations;
            global.platform_configurations = instance;

            instance->type = patch_type;
            instance->data =
                kan_allocate_general (platform_configuration_allocation_group, patch_type->size, patch_type->alignment);

            if (patch_type->init)
            {
                patch_type->init (patch_type->functor_user_data, instance->data);
            }
        }

        kan_reflection_patch_apply (patch, instance->data);
    }

    kan_resource_platform_configuration_shutdown (configuration);
    kan_free_general (loaded_native_entries_allocation_group, configuration,
                      sizeof (struct kan_resource_platform_configuration_t));
}

static void byproduct_node_init (struct byproduct_node_t *instance)
{
    instance->entry = NULL;
    kan_dynamic_array_init (&instance->produced_from, KAN_RESOURCE_BUILDER_BYPRODUCT_PRODUCED_FROM_SIZE,
                            sizeof (struct byproduct_production_source_t),
                            _Alignof (struct byproduct_production_source_t), nodes_allocation_group);
}

static inline void byproduct_node_add_production_unsafe (struct byproduct_node_t *node,
                                                         kan_interned_string_t resource_type,
                                                         kan_interned_string_t resource_name)
{
    struct byproduct_production_source_t *spot = kan_dynamic_array_add_last (&node->produced_from);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (&node->produced_from, node->produced_from.size * 2u);
        spot = kan_dynamic_array_add_last (&node->produced_from);
        KAN_ASSERT (spot)
    }

    spot->resource_type = resource_type;
    spot->resource_name = resource_name;
}

static void byproduct_node_shutdown (struct byproduct_node_t *instance)
{
    kan_dynamic_array_shutdown (&instance->produced_from);
}

static void target_init (struct target_t *instance)
{
    instance->name = NULL;
    instance->source = NULL;
    instance->requested_for_build = KAN_FALSE;

    kan_hash_storage_init (&instance->native, nodes_allocation_group, KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS);
    kan_hash_storage_init (&instance->third_party, nodes_allocation_group, KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS);
    instance->interned_string_registry = KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t);

    kan_dynamic_array_init (&instance->visible_targets, 0u, sizeof (struct target_t *), _Alignof (struct target_t *),
                            targets_allocation_group);

    instance->byproduct_index_generator = kan_atomic_int_init (0);
    instance->byproduct_registration_lock = kan_atomic_int_init (0);
    kan_hash_storage_init (&instance->byproducts, nodes_allocation_group, KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS);

    kan_hash_storage_init (&instance->loaded_byproduct_production, nodes_allocation_group,
                           KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS);
}

static struct native_entry_node_t *target_query_local_native_by_source_type (struct target_t *target,
                                                                             kan_interned_string_t type,
                                                                             kan_interned_string_t name)
{
    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (&target->native, (kan_hash_t) name);
    struct native_entry_node_t *node = (struct native_entry_node_t *) bucket->first;
    const struct native_entry_node_t *node_end =
        (struct native_entry_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == name && node->source_type->name == type)
        {
            return node;
        }

        node = (struct native_entry_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static struct native_entry_node_t *target_query_local_native_by_compiled_type (struct target_t *target,
                                                                               kan_interned_string_t type,
                                                                               kan_interned_string_t name)
{
    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (&target->native, (kan_hash_t) name);
    struct native_entry_node_t *node = (struct native_entry_node_t *) bucket->first;
    const struct native_entry_node_t *node_end =
        (struct native_entry_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == name && node->compiled_type->name == type)
        {
            return node;
        }

        node = (struct native_entry_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static struct native_entry_node_t *target_query_global_native_by_source_type (struct target_t *target,
                                                                              kan_interned_string_t type,
                                                                              kan_interned_string_t name)
{
    struct native_entry_node_t *node = target_query_local_native_by_source_type (target, type, name);
    if (node)
    {
        return node;
    }

    for (kan_loop_size_t visible_target_index = 0u; visible_target_index < target->visible_targets.size;
         ++visible_target_index)
    {
        node = target_query_local_native_by_source_type (
            ((struct target_t **) target->visible_targets.data)[visible_target_index], type, name);

        if (node)
        {
            return node;
        }
    }

    return NULL;
}

static struct native_entry_node_t *target_query_global_native_by_compiled_type (struct target_t *target,
                                                                                kan_interned_string_t type,
                                                                                kan_interned_string_t name)
{
    struct native_entry_node_t *node = target_query_local_native_by_compiled_type (target, type, name);
    if (node)
    {
        return node;
    }

    for (kan_loop_size_t visible_target_index = 0u; visible_target_index < target->visible_targets.size;
         ++visible_target_index)
    {
        node = target_query_local_native_by_compiled_type (
            ((struct target_t **) target->visible_targets.data)[visible_target_index], type, name);

        if (node)
        {
            return node;
        }
    }

    return NULL;
}

static struct third_party_entry_node_t *target_query_local_third_party (struct target_t *target,
                                                                        kan_interned_string_t name)
{
    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (&target->third_party, (kan_hash_t) name);
    struct third_party_entry_node_t *node = (struct third_party_entry_node_t *) bucket->first;
    const struct third_party_entry_node_t *node_end =
        (struct third_party_entry_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == name)
        {
            return node;
        }

        node = (struct third_party_entry_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static struct third_party_entry_node_t *target_query_global_third_party (struct target_t *target,
                                                                         kan_interned_string_t name)
{
    struct third_party_entry_node_t *node = target_query_local_third_party (target, name);
    if (node)
    {
        return node;
    }

    for (kan_loop_size_t visible_target_index = 0u; visible_target_index < target->visible_targets.size;
         ++visible_target_index)
    {
        node = target_query_local_third_party (
            ((struct target_t **) target->visible_targets.data)[visible_target_index], name);

        if (node)
        {
            return node;
        }
    }

    return NULL;
}

static void target_shutdown (struct target_t *instance)
{
    struct native_entry_node_t *native_node = (struct native_entry_node_t *) instance->native.items.first;
    while (native_node)
    {
        struct native_entry_node_t *next = (struct native_entry_node_t *) native_node->node.list_node.next;
        native_entry_node_destroy (native_node);
        native_node = next;
    }

    struct third_party_entry_node_t *third_party_node =
        (struct third_party_entry_node_t *) instance->third_party.items.first;

    while (third_party_node)
    {
        struct third_party_entry_node_t *next =
            (struct third_party_entry_node_t *) third_party_node->node.list_node.next;
        third_party_entry_node_destroy (third_party_node);
        third_party_node = next;
    }

    kan_hash_storage_shutdown (&instance->native);
    kan_hash_storage_shutdown (&instance->third_party);

    if (KAN_HANDLE_IS_VALID (instance->interned_string_registry))
    {
        kan_serialization_interned_string_registry_destroy (instance->interned_string_registry);
    }

    kan_dynamic_array_shutdown (&instance->visible_targets);
    struct byproduct_node_t *byproduct_node = (struct byproduct_node_t *) instance->byproducts.items.first;

    while (byproduct_node)
    {
        struct byproduct_node_t *next = (struct byproduct_node_t *) byproduct_node->node.list_node.next;
        byproduct_node_shutdown (byproduct_node);
        kan_free_batched (nodes_allocation_group, byproduct_node);
        byproduct_node = next;
    }

    struct byproduct_production_node_t *production_node =
        (struct byproduct_production_node_t *) instance->loaded_byproduct_production.items.first;

    while (production_node)
    {
        struct byproduct_production_node_t *next =
            (struct byproduct_production_node_t *) production_node->node.list_node.next;
        kan_free_batched (nodes_allocation_group, production_node);
        production_node = next;
    }

    kan_hash_storage_shutdown (&instance->byproducts);
    kan_hash_storage_shutdown (&instance->loaded_byproduct_production);
}

static void target_collect_references (kan_instance_size_t build_target_index, kan_instance_size_t project_target_index)
{
    struct target_t *build_target = &((struct target_t *) global.targets.data)[build_target_index];
    struct kan_application_resource_target_t *project_target =
        &((struct kan_application_resource_target_t *) global.project.targets.data)[project_target_index];

    for (kan_loop_size_t visible_target_index = 0u; visible_target_index < build_target->visible_targets.size;
         ++visible_target_index)
    {
        struct target_t *visible_target =
            &((struct target_t *) build_target->visible_targets.data)[visible_target_index];
        if (visible_target->name == project_target->name)
        {
            // Already added, skip.
            return;
        }
    }

    // Don't add self (happens during recursion root call).
    if (build_target_index != project_target_index)
    {
        void *spot = kan_dynamic_array_add_last (&build_target->visible_targets);
        if (!spot)
        {
            kan_dynamic_array_set_capacity (&build_target->visible_targets,
                                            KAN_MAX (1u, build_target->visible_targets.size * 2u));
            spot = kan_dynamic_array_add_last (&build_target->visible_targets);
            KAN_ASSERT (spot)
        }

        struct target_t *new_visible_target = &((struct target_t *) global.targets.data)[project_target_index];
        *(struct target_t **) spot = new_visible_target;

        if (build_target->requested_for_build && !new_visible_target->requested_for_build)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO,
                     "Adding target \"%s\" to build as it is visible to other build targets.", new_visible_target->name)
            new_visible_target->requested_for_build = KAN_TRUE;
        }
    }

    for (kan_loop_size_t visible_name_index = 0u; visible_name_index < project_target->visible_targets.size;
         ++visible_name_index)
    {
        kan_interned_string_t visible_name =
            ((kan_interned_string_t *) project_target->visible_targets.data)[visible_name_index];
        kan_instance_size_t found_index = KAN_INT_MAX (kan_instance_size_t);

        for (kan_loop_size_t target_index = 0u; target_index < global.project.targets.size; ++target_index)
        {
            if (((struct kan_application_resource_target_t *) global.project.targets.data)[target_index].name ==
                visible_name)
            {
                found_index = target_index;
                break;
            }
        }

        if (found_index != KAN_INT_MAX (kan_instance_size_t))
        {
            // Ignore cycles.
            if (found_index != build_target_index)
            {
                target_collect_references (build_target_index, found_index);
            }
        }
        else
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                     "Failed to find target \"%s\" in project, it is declared visible by target \"%s\".", visible_name,
                     project_target->name)
            kan_atomic_int_add (&global.errors_count, 1);
        }
    }
}

static void scan_file (struct target_t *target, struct kan_file_system_path_container_t *path_container)
{
    struct kan_resource_index_info_from_path_t info;
    kan_resource_index_extract_info_from_path (path_container->path, &info);

    if (info.native)
    {
        kan_interned_string_t type_name = NULL;
        struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (global.volume, path_container->path);

        if (!stream)
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder,
                                 KAN_LOG_ERROR, "Failed to open native entry at \"%s\" for read.", path_container->path)
            kan_atomic_int_add (&global.errors_count, 1);
            return;
        }

        stream = kan_random_access_stream_buffer_open_for_read (stream, KAN_RESOURCE_BUILDER_IO_BUFFER);
        switch (info.native_format)
        {
        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY:
            if (!kan_serialization_binary_read_type_header (
                    stream, &type_name, KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t)))
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder,
                                     KAN_LOG_ERROR, "Failed to read type header from native entry at \"%s\".",
                                     path_container->path)
                kan_atomic_int_add (&global.errors_count, 1);
                stream->operations->close (stream);
                return;
            }

            break;

        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA:
            if (!kan_serialization_rd_read_type_header (stream, &type_name))
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder,
                                     KAN_LOG_ERROR, "Failed to read type header from native entry at \"%s\".",
                                     path_container->path)
                kan_atomic_int_add (&global.errors_count, 1);
                stream->operations->close (stream);
                return;
            }

            break;
        }

        stream->operations->close (stream);
        struct native_entry_node_t *collision = target_query_local_native_by_source_type (target, type_name, info.name);

        if (collision)
        {
            KAN_LOG_WITH_BUFFER (
                KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder, KAN_LOG_ERROR,
                "Found collision: two native resources with the same type and name at \"%s\" and at \"%s\".",
                path_container->path, collision->source_path)
            kan_atomic_int_add (&global.errors_count, 1);
            return;
        }

        struct native_entry_node_t *node =
            native_entry_node_create (target, type_name, info.name, path_container->path);

        if (!node)
        {
            return;
        }

        kan_hash_storage_update_bucket_count_default (&target->native, KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS);
        kan_hash_storage_add (&target->native, &node->node);

        // Register byproduct too if it is a byproduct.
        if (node->byproduct_meta)
        {
            // Byproduct should always be loaded.
            node->source_references = kan_atomic_int_init (1);

            node->source_data =
                load_native_data (node->source_type, node->source_path,
                                  KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));

            if (!node->source_data)
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder,
                                     KAN_LOG_ERROR, "Failed to byproduct native entry at \"%s\".", path_container->path)
                kan_atomic_int_add (&global.errors_count, 1);
                return;
            }

            struct byproduct_node_t *byproduct_node =
                kan_allocate_batched (nodes_allocation_group, sizeof (struct byproduct_node_t));
            byproduct_node_init (byproduct_node);

            byproduct_node->node.hash =
                node->byproduct_meta->hash ?
                    node->byproduct_meta->hash (node->source_data) :
                    kan_reflection_hash_struct (global.registry, node->source_type, node->source_data);
            byproduct_node->entry = node;
            node->linked_byproduct = byproduct_node;

            kan_hash_storage_update_bucket_count_default (&target->byproducts,
                                                          KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS);
            kan_hash_storage_add (&target->byproducts, &byproduct_node->node);
        }
    }
    else
    {
        if (strcmp (info.name, KAN_RESOURCE_INDEX_DEFAULT_NAME) == 0)
        {
            KAN_LOG_WITH_BUFFER (
                KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder, KAN_LOG_ERROR,
                "We do not expect indices in raw resources, they're shipping only. But we found one at \"%s\".",
                path_container->path)
            kan_atomic_int_add (&global.errors_count, 1);
            return;
        }

        if (strcmp (info.name, KAN_RESOURCE_INDEX_ACCOMPANYING_STRING_REGISTRY_DEFAULT_NAME) == 0)
        {
            KAN_LOG_WITH_BUFFER (
                KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder, KAN_LOG_ERROR,
                "We do not expect accompanying string registries in raw resources, they're shipping only. But we "
                "found one at \"%s\".",
                path_container->path)
            kan_atomic_int_add (&global.errors_count, 1);
            return;
        }

        struct kan_virtual_file_system_entry_status_t status;
        if (!kan_virtual_file_system_query_entry (global.volume, path_container->path, &status))
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder,
                                 KAN_LOG_ERROR, "Unable to query status of \"%s\".", path_container->path)
            kan_atomic_int_add (&global.errors_count, 1);
            return;
        }

        struct third_party_entry_node_t *collision = target_query_local_third_party (target, info.name);
        if (collision)
        {
            KAN_LOG_WITH_BUFFER (
                KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder, KAN_LOG_ERROR,
                "Found collision: two third party resources with the same name at \"%s\" and at \"%s\".",
                path_container->path, collision->path)
            kan_atomic_int_add (&global.errors_count, 1);
            return;
        }

        struct third_party_entry_node_t *node =
            third_party_entry_node_create (target, info.name, status.size, path_container->path);

        if (!node)
        {
            return;
        }

        kan_hash_storage_update_bucket_count_default (&target->third_party, KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS);
        kan_hash_storage_add (&target->third_party, &node->node);
    }
}

static void scan_directory (struct target_t *target, struct kan_file_system_path_container_t *path_container)
{
    struct kan_virtual_file_system_directory_iterator_t iterator =
        kan_virtual_file_system_directory_iterator_create (global.volume, path_container->path);
    const char *item_name;

    while ((item_name = kan_virtual_file_system_directory_iterator_advance (&iterator)))
    {
        if ((item_name[0u] == '.' && item_name[1u] == '\0') ||
            (item_name[0u] == '.' && item_name[1u] == '.' && item_name[2u] == '\0'))
        {
            // Skip special entries.
            continue;
        }

        const kan_instance_size_t old_length = path_container->length;
        kan_file_system_path_container_append (path_container, item_name);
        struct kan_virtual_file_system_entry_status_t status;

        if (kan_virtual_file_system_query_entry (global.volume, path_container->path, &status))
        {
            switch (status.type)
            {
            case KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN:
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder,
                                     KAN_LOG_ERROR, "Entry \"%s\" has unknown type.", path_container->path)
                kan_atomic_int_add (&global.errors_count, 1);
                break;

            case KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE:
                scan_file (target, path_container);
                break;

            case KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY:
                scan_directory (target, path_container);
                break;
            }
        }
        else
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder,
                                 KAN_LOG_ERROR, "Failed to query status of entry \"%s\".", path_container->path)
            kan_atomic_int_add (&global.errors_count, 1);
        }

        kan_file_system_path_container_reset_length (path_container, old_length);
    }

    kan_virtual_file_system_directory_iterator_destroy (&iterator);
}

static void scan_target_for_resources (kan_functor_user_data_t user_data)
{
    struct target_t *target = (struct target_t *) user_data;
    struct kan_file_system_path_container_t path_container;

    kan_file_system_path_container_copy_string (&path_container, VFS_TARGETS_DIRECTORY);
    kan_file_system_path_container_append (&path_container, target->name);
    scan_directory (target, &path_container);

    KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "[Target \"%s\"] Done scanning for resources.",
             target->name)

    kan_file_system_path_container_copy_string (&path_container,
                                                VFS_OUTPUT_DIRECTORY "/" SUB_DIRECTORY_RAW_BYPRODUCT_CACHE);
    kan_file_system_path_container_append (&path_container, target->name);

    if (kan_virtual_file_system_check_existence (global.volume, path_container.path))
    {
        scan_directory (target, &path_container);
    }

    KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "[Target \"%s\"] Done scanning for byproducts.",
             target->name)

    kan_file_system_path_container_copy_string (&path_container,
                                                VFS_OUTPUT_DIRECTORY "/" SUB_DIRECTORY_RAW_BYPRODUCT_CACHE);
    kan_file_system_path_container_append (&path_container, target->name);
    kan_file_system_path_container_add_suffix (&path_container, BYPRODUCT_STATE_FILE_SUFFIX);

    if (kan_virtual_file_system_check_existence (global.volume, path_container.path))
    {
        struct kan_resource_target_byproduct_state_t loaded_byproduct_state;
        kan_resource_target_byproduct_state_init (&loaded_byproduct_state);

        if (load_native_data_into_existent_allocation (
                kan_reflection_registry_query_struct (global.registry, interned_kan_resource_target_byproduct_state_t),
                path_container.path, KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t),
                &loaded_byproduct_state))
        {
            target->byproduct_index_generator =
                kan_atomic_int_init ((int) loaded_byproduct_state.byproduct_index_generator);

            // Prepare generation map for correctly skipping up to date generation.
            for (kan_loop_size_t index = 0u; index < loaded_byproduct_state.production.size; ++index)
            {
                struct kan_resource_target_byproduct_production_t *production =
                    &((struct kan_resource_target_byproduct_production_t *)
                          loaded_byproduct_state.production.data)[index];

                struct byproduct_production_node_t *new_node =
                    kan_allocate_batched (nodes_allocation_group, sizeof (struct byproduct_production_node_t));

                new_node->node.hash =
                    kan_hash_combine ((kan_hash_t) production->resource_type, (kan_hash_t) production->resource_name);

                new_node->resource_type = production->resource_type;
                new_node->resource_name = production->resource_name;
                new_node->produced_type = production->byproduct_type;
                new_node->produced_name = production->byproduct_name;

                kan_hash_storage_update_bucket_count_default (&target->loaded_byproduct_production,
                                                              KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS);
                kan_hash_storage_add (&target->loaded_byproduct_production, &new_node->node);
            }
        }
        else
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO,
                     "[Target \"%s\"] Failed to restore byproduct state.", target->name)
            kan_atomic_int_add (&global.errors_count, 1);
        }

        kan_resource_target_byproduct_state_shutdown (&loaded_byproduct_state);
    }

    KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "[Target \"%s\"] Done restoring byproduct state.",
             target->name)
}

static void scan_native_for_name_collisions (kan_functor_user_data_t user_data)
{
    struct native_entry_node_t *node = (struct native_entry_node_t *) user_data;
    for (kan_loop_size_t visible_target_index = 0u; visible_target_index < node->target->visible_targets.size;
         ++visible_target_index)
    {
        struct target_t *other_target = ((struct target_t **) node->target->visible_targets.data)[visible_target_index];
        struct native_entry_node_t *other_node =
            target_query_local_native_by_source_type (other_target, node->source_type->name, node->name);

        if (other_node)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                     "Found collision: two native resources have same type and name at \"%s\" (target \"%s\") and at "
                     "\"%s\" (target \"%s\").",
                     node->source_path, node->target->name, other_node->source_path, other_target->name)
            kan_atomic_int_add (&global.errors_count, 1);
            return;
        }
    }

    KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO,
             "[Target \"%s\"] No collisions for native resource \"%s\" of type \"%s\".", node->target->name, node->name,
             node->source_type->name)
}

static void scan_third_party_for_name_collisions (kan_functor_user_data_t user_data)
{
    struct third_party_entry_node_t *node = (struct third_party_entry_node_t *) user_data;
    for (kan_loop_size_t visible_target_index = 0u; visible_target_index < node->target->visible_targets.size;
         ++visible_target_index)
    {
        struct target_t *other_target = ((struct target_t **) node->target->visible_targets.data)[visible_target_index];
        struct third_party_entry_node_t *other_node = target_query_local_third_party (other_target, node->name);

        if (other_node)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                     "Found collision: two third party resources have same name at \"%s\" (target \"%s\") and at "
                     "\"%s\" (target \"%s\").",
                     node->path, node->target->name, other_node->path, other_target->name)
            kan_atomic_int_add (&global.errors_count, 1);
            return;
        }
    }

    KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO,
             "[Target \"%s\"] No collisions for third party resource \"%s\".", node->target->name, node->name)
}

static void print_node_in_dead_lock_queue (struct native_entry_node_t *node)
{
    // There shouldn't be other way to get into deadlock.
    KAN_ASSERT (node->compilation_status == COMPILATION_STATUS_WAITING_FOR_COMPILED_DEPENDENCIES)
    KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
             "    [Target \"%s\"] Native resource \"%s\" of type \"%s\". Waits for compiled:", node->target->name,
             node->name, node->source_type->name)

    for (kan_loop_size_t index = 0u; index < node->source_detected_references.detected_references.size; ++index)
    {
        struct kan_resource_detected_reference_t *reference =
            &((struct kan_resource_detected_reference_t *)
                  node->source_detected_references.detected_references.data)[index];

        if (reference->compilation_usage == KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                     "        Native resource \"%s\" of type \"%s\".", reference->name, reference->type)
        }
    }
}

static inline void add_to_passive_queue (struct native_entry_node_t *node)
{
    node->compilation_queue = NATIVE_NODE_COMPILATION_QUEUE_PASSIVE;
    node->next_node_in_compilation_queue = NULL;

    if (global.compilation_passive_queue_last)
    {
        node->previous_node_in_compilation_queue = global.compilation_passive_queue_last;
        global.compilation_passive_queue_last->next_node_in_compilation_queue = node;
        global.compilation_passive_queue_last = node;
    }
    else
    {
        node->previous_node_in_compilation_queue = NULL;
        global.compilation_passive_queue_first = node;
        global.compilation_passive_queue_last = node;
    }
}

static void schedule_new_compilation (struct native_entry_node_t *node)
{
    KAN_ASSERT (node->compilation_status == COMPILATION_STATUS_NOT_YET)
    kan_atomic_int_lock (&global.compilation_queue_lock);

    if (node->compilation_queue != NATIVE_NODE_COMPILATION_QUEUE_PASSIVE)
    {
        node->pending_compilation_status = COMPILATION_STATUS_REQUESTED;
        add_to_passive_queue (node);
    }

    kan_atomic_int_unlock (&global.compilation_queue_lock);
}

static inline void remove_from_active_queue (struct native_entry_node_t *node)
{
    node->compilation_queue = NATIVE_NODE_COMPILATION_QUEUE_NONE;
    if (node->previous_node_in_compilation_queue)
    {
        node->previous_node_in_compilation_queue->next_node_in_compilation_queue = node->next_node_in_compilation_queue;
    }
    else
    {
        global.compilation_active_queue = node->next_node_in_compilation_queue;
    }

    if (node->next_node_in_compilation_queue)
    {
        node->next_node_in_compilation_queue->previous_node_in_compilation_queue =
            node->previous_node_in_compilation_queue;
    }
}

static void wait_in_passive_queue (struct native_entry_node_t *node)
{
    kan_atomic_int_lock (&global.compilation_queue_lock);
    if (node->compilation_queue == NATIVE_NODE_COMPILATION_QUEUE_ACTIVE)
    {
        remove_from_active_queue (node);
    }

    if (node->compilation_queue != NATIVE_NODE_COMPILATION_QUEUE_PASSIVE)
    {
        add_to_passive_queue (node);
    }

    kan_atomic_int_unlock (&global.compilation_queue_lock);
}

static void recursively_add_to_pack (struct native_entry_node_t *node)
{
    for (kan_loop_size_t index = 0u; index < node->compiled_detected_references.detected_references.size; ++index)
    {
        struct kan_resource_detected_reference_t *reference =
            &((struct kan_resource_detected_reference_t *)
                  node->compiled_detected_references.detected_references.data)[index];

        if (reference->type)
        {
            struct native_entry_node_t *referenced =
                target_query_global_native_by_compiled_type (node->target, reference->type, reference->name);

            if (!referenced)
            {
                referenced = target_query_global_native_by_source_type (node->target, reference->type, reference->name);
            }

            if (referenced)
            {
                if (!referenced->should_be_included_in_pack)
                {
                    referenced->should_be_included_in_pack = KAN_TRUE;
                    recursively_add_to_pack (referenced);

                    if (referenced->compilation_status == COMPILATION_STATUS_NOT_YET)
                    {
                        schedule_new_compilation (referenced);
                    }
                }
            }
            else
            {
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                         "[Target \"%s\"] Compiled native resource \"%s\" of type \"%s\" references native \"%s\" "
                         "of type \"%s\" which does not exist.",
                         node->target->name, node->name, node->compiled_type->name, reference->name, reference->type)
                kan_atomic_int_add (&global.errors_count, 1);
            }
        }
        else
        {
            struct third_party_entry_node_t *referenced =
                target_query_global_third_party (node->target, reference->name);

            if (referenced)
            {
                referenced->should_be_included_in_pack = KAN_TRUE;
            }
            else
            {
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                         "[Target \"%s\"] Compiled native resource \"%s\" of type \"%s\" references third party \"%s\" "
                         "which does not exist.",
                         node->target->name, node->name, node->compiled_type->name, reference->name)
                kan_atomic_int_add (&global.errors_count, 1);
            }
        }
    }
}

static void add_native_to_resource_management_pass (struct native_entry_node_t *node)
{
    kan_atomic_int_lock (&global.compilation_resource_management_lock);
    if (!node->queued_for_resource_management)
    {
        node->queued_for_resource_management = KAN_TRUE;
        node->next_node_in_resource_management_queue = global.resource_management_native_queue;
        global.resource_management_native_queue = node;
    }

    kan_atomic_int_unlock (&global.compilation_resource_management_lock);
}

static void add_native_source_reference (struct native_entry_node_t *node)
{
    if (kan_atomic_int_add (&node->source_references, 1) == 0)
    {
        add_native_to_resource_management_pass (node);
    }
}

static void remove_native_source_reference (struct native_entry_node_t *node)
{
    if (kan_atomic_int_add (&node->source_references, -1) == 1)
    {
        add_native_to_resource_management_pass (node);
    }
}

static void add_native_compiled_reference (struct native_entry_node_t *node)
{
    if (kan_atomic_int_add (&node->compiled_references, 1) == 0)
    {
        add_native_to_resource_management_pass (node);
    }
}

static void remove_native_compiled_reference (struct native_entry_node_t *node)
{
    if (kan_atomic_int_add (&node->compiled_references, -1) == 1)
    {
        add_native_to_resource_management_pass (node);
    }
}

static void add_third_party_to_resource_management_pass (struct third_party_entry_node_t *node)
{
    kan_atomic_int_lock (&global.compilation_resource_management_lock);
    if (!node->queued_for_resource_management)
    {
        node->queued_for_resource_management = KAN_TRUE;
        node->next_node_in_resource_management_queue = global.resource_management_third_party_queue;
        global.resource_management_third_party_queue = node;
    }

    kan_atomic_int_unlock (&global.compilation_resource_management_lock);
}

static void add_third_party_reference (struct third_party_entry_node_t *node)
{
    if (kan_atomic_int_add (&node->references, 1) == 0)
    {
        add_third_party_to_resource_management_pass (node);
    }
}

static void remove_third_party_reference (struct third_party_entry_node_t *node)
{
    if (kan_atomic_int_add (&node->references, -1) == 1)
    {
        add_third_party_to_resource_management_pass (node);
    }
}

static void manage_resources_native (kan_functor_user_data_t user_data)
{
    struct native_entry_node_t *node = (struct native_entry_node_t *) user_data;
    int source_references = kan_atomic_int_get (&node->source_references);
    int compiled_references = kan_atomic_int_get (&node->compiled_references);

    if (node->source_data && source_references == 0)
    {
        native_entry_node_unload_source (node);
    }
    else if (!node->source_data && source_references > 0)
    {
        node->source_data = load_native_data (node->source_type, node->source_path,
                                              KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));
    }

    if (node->compilation_status == COMPILATION_STATUS_FINISHED)
    {
        if (node->compiled_data && compiled_references == 0)
        {
            native_entry_node_unload_compiled (node);
        }
        else if (!node->compiled_data && compiled_references > 0)
        {
            node->compiled_data =
                load_native_data (node->compiled_type, node->compiled_path,
                                  KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));
        }
    }

    node->queued_for_resource_management = KAN_FALSE;
    node->next_node_in_resource_management_queue = NULL;
}

static void manage_resources_third_party (kan_functor_user_data_t user_data)
{
    struct third_party_entry_node_t *node = (struct third_party_entry_node_t *) user_data;
    int references = kan_atomic_int_get (&node->references);

    if (node->data && references == 0)
    {
        third_party_entry_node_unload (node);
    }
    else if (!node->data && references > 0)
    {
        node->data = load_third_party_data (node);
    }

    node->queued_for_resource_management = KAN_FALSE;
    node->next_node_in_resource_management_queue = NULL;
}

static inline void form_references_cache_directory_path (struct native_entry_node_t *node,
                                                         struct kan_file_system_path_container_t *output)
{
    kan_file_system_path_container_copy_string (output, VFS_RAW_REFERENCE_CACHE_DIRECTORY);
    kan_file_system_path_container_append (output, node->source_type->name);
}

static inline void form_references_cache_item_path (struct native_entry_node_t *node,
                                                    struct kan_file_system_path_container_t *output)
{
    form_references_cache_directory_path (node, output);
    kan_file_system_path_container_append (output, node->name);
}

static inline void form_compiled_references_cache_directory_path (struct native_entry_node_t *node,
                                                                  struct kan_file_system_path_container_t *output)
{
    kan_file_system_path_container_copy_string (output,
                                                VFS_OUTPUT_DIRECTORY "/" SUB_DIRECTORY_COMPILED_REFERENCE_CACHE);
    kan_file_system_path_container_append (output, node->source_type->name);
}

static inline void form_compiled_references_cache_item_path (struct native_entry_node_t *node,
                                                             struct kan_file_system_path_container_t *output)
{
    form_compiled_references_cache_directory_path (node, output);
    kan_file_system_path_container_append (output, node->name);
}

static inline kan_time_size_t get_file_last_modification_time_ns (const char *path)
{
    struct kan_virtual_file_system_entry_status_t status;
    if (kan_virtual_file_system_query_entry (global.volume, path, &status))
    {
        return status.last_modification_time_ns;
    }

    // If file became absent for whatever reason, it is better to treat it as newly modified and
    // invalidate things that depend on this file.
    return KAN_INT_MAX (kan_time_size_t);
}

static inline kan_bool_t read_detected_references_cache (struct native_entry_node_t *node,
                                                         struct kan_resource_detected_reference_container_t *container,
                                                         const char *path)
{
    struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (global.volume, path);
    if (!stream)
    {
        // Reference cache failures are warnings, because we can always load resource itself.
        KAN_LOG (application_framework_resource_builder, KAN_LOG_WARNING,
                 "[Target \"%s\"] Failed to open reference cache for native resource \"%s\" of type \"%s\".",
                 node->target->name, node->name, node->source_type->name)
        return KAN_FALSE;
    }

    stream = kan_random_access_stream_buffer_open_for_read (stream, KAN_RESOURCE_BUILDER_IO_BUFFER);
    kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
        stream, container, kan_string_intern ("kan_resource_detected_reference_container_t"),
        global.binary_script_storage, KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t),
        container->detected_references.allocation_group);

    enum kan_serialization_state_t serialization_state;
    while ((serialization_state = kan_serialization_binary_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    kan_serialization_binary_reader_destroy (reader);
    stream->operations->close (stream);

    if (serialization_state == KAN_SERIALIZATION_FAILED)
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_WARNING,
                 "[Target \"%s\"] Failed to deserialize reference cache for native resource \"%s\" of type \"%s\".",
                 node->target->name, node->name, node->source_type->name)
        return KAN_FALSE;
    }

    KAN_ASSERT (serialization_state == KAN_SERIALIZATION_FINISHED);
    return KAN_TRUE;
}

static inline void remove_requests_for_compiled_dependencies (struct native_entry_node_t *node)
{
    for (kan_loop_size_t index = 0u; index < node->source_detected_references.detected_references.size; ++index)
    {
        struct kan_resource_detected_reference_t *reference =
            &((struct kan_resource_detected_reference_t *)
                  node->source_detected_references.detected_references.data)[index];

        if (reference->compilation_usage == KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED)
        {
            struct native_entry_node_t *dependency =
                target_query_global_native_by_source_type (node->target, reference->type, reference->name);

            if (dependency)
            {
                remove_native_compiled_reference (dependency);
            }
        }
    }
}

static inline kan_bool_t request_compiled_dependencies (struct native_entry_node_t *node)
{
    kan_bool_t successful = KAN_TRUE;
    for (kan_loop_size_t index = 0u; index < node->source_detected_references.detected_references.size; ++index)
    {
        struct kan_resource_detected_reference_t *reference =
            &((struct kan_resource_detected_reference_t *)
                  node->source_detected_references.detected_references.data)[index];

        if (reference->compilation_usage == KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED)
        {
            struct native_entry_node_t *dependency =
                target_query_global_native_by_source_type (node->target, reference->type, reference->name);

            if (dependency)
            {
                add_native_compiled_reference (dependency);
                if (dependency->compilation_status == COMPILATION_STATUS_NOT_YET)
                {
                    schedule_new_compilation (dependency);
                }
            }
            else
            {
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                         "[Target \"%s\"] Failed to compile native resource \"%s\" of type \"%s\" as its "
                         "native compile dependency \"%s\" of type \"%s\" is not found.",
                         node->target->name, node->name, node->source_type->name, reference->type, reference->name)
                successful = KAN_FALSE;
            }
        }
    }

    if (!successful)
    {
        remove_requests_for_compiled_dependencies (node);
    }

    return successful;
}

static inline void remove_requests_for_raw_dependencies (struct native_entry_node_t *node)
{
    for (kan_loop_size_t index = 0u; index < node->source_detected_references.detected_references.size; ++index)
    {
        struct kan_resource_detected_reference_t *reference =
            &((struct kan_resource_detected_reference_t *)
                  node->source_detected_references.detected_references.data)[index];

        if (reference->compilation_usage == KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW)
        {
            if (reference->type)
            {
                struct native_entry_node_t *dependency =
                    target_query_global_native_by_source_type (node->target, reference->type, reference->name);

                if (dependency)
                {
                    remove_native_source_reference (dependency);
                }
            }
            else
            {
                struct third_party_entry_node_t *dependency =
                    target_query_global_third_party (node->target, reference->name);

                if (dependency)
                {
                    remove_third_party_reference (dependency);
                }
            }
        }
    }
}

static inline kan_bool_t request_raw_dependencies (struct native_entry_node_t *node)
{
    kan_bool_t successful = KAN_TRUE;
    for (kan_loop_size_t index = 0u; index < node->source_detected_references.detected_references.size; ++index)
    {
        struct kan_resource_detected_reference_t *reference =
            &((struct kan_resource_detected_reference_t *)
                  node->source_detected_references.detected_references.data)[index];

        if (reference->compilation_usage == KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW)
        {
            if (reference->type)
            {
                struct native_entry_node_t *dependency =
                    target_query_global_native_by_source_type (node->target, reference->type, reference->name);

                if (dependency)
                {
                    add_native_source_reference (dependency);
                }
                else
                {
                    KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                             "[Target \"%s\"] Failed to compile native resource \"%s\" of type \"%s\" as its "
                             "native compile dependency \"%s\" of type \"%s\" is not found.",
                             node->target->name, node->name, node->source_type->name, reference->type, reference->name)
                    successful = KAN_FALSE;
                }
            }
            else
            {
                struct third_party_entry_node_t *dependency =
                    target_query_global_third_party (node->target, reference->name);

                if (dependency)
                {
                    add_third_party_reference (dependency);
                }
                else
                {
                    KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                             "[Target \"%s\"] Failed to compile native resource \"%s\" of type \"%s\" as its "
                             "third party compile dependency \"%s\" is not found.",
                             node->target->name, node->name, node->source_type->name, reference->name)
                    successful = KAN_FALSE;
                }
            }
        }
    }

    if (!successful)
    {
        remove_requests_for_raw_dependencies (node);
    }

    return successful;
}

static kan_bool_t is_compiled_data_newer_than_dependencies (struct native_entry_node_t *node)
{
    const kan_time_size_t compiled_time = get_file_last_modification_time_ns (node->compiled_path);
    if (compiled_time < global.newest_loaded_plugin_last_modification_file_time_ns)
    {
        return KAN_FALSE;
    }

    for (kan_loop_size_t index = 0u; index < node->source_detected_references.detected_references.size; ++index)
    {
        struct kan_resource_detected_reference_t *reference =
            &((struct kan_resource_detected_reference_t *)
                  node->source_detected_references.detected_references.data)[index];

        if (reference->compilation_usage == KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW)
        {
            if (reference->type)
            {
                struct native_entry_node_t *dependency =
                    target_query_global_native_by_source_type (node->target, reference->type, reference->name);

                if (!dependency || compiled_time < get_file_last_modification_time_ns (dependency->source_path))
                {
                    return KAN_FALSE;
                }
            }
            else
            {
                struct third_party_entry_node_t *dependency =
                    target_query_global_third_party (node->target, reference->name);

                if (!dependency || compiled_time < get_file_last_modification_time_ns (dependency->path))
                {
                    return KAN_FALSE;
                }
            }
        }
        else if (reference->compilation_usage == KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED)
        {
            struct native_entry_node_t *dependency =
                target_query_global_native_by_source_type (node->target, reference->type, reference->name);

            if (!dependency || compiled_time < get_file_last_modification_time_ns (dependency->compiled_path))
            {
                return KAN_FALSE;
            }
        }
    }

    return KAN_TRUE;
}

static inline void confirm_loaded_byproduct_production (struct target_t *target,
                                                        kan_interned_string_t resource_type,
                                                        kan_interned_string_t resource_name)
{
    const kan_hash_t resource_hash = kan_hash_combine ((kan_hash_t) resource_type, (kan_hash_t) resource_name);
    kan_atomic_int_lock (&target->byproduct_registration_lock);

    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&target->loaded_byproduct_production, (kan_hash_t) resource_hash);
    struct byproduct_production_node_t *node = (struct byproduct_production_node_t *) bucket->first;
    const struct byproduct_production_node_t *node_end =
        (struct byproduct_production_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->resource_type == resource_type && node->resource_name == resource_name)
        {
            struct native_entry_node_t *entry =
                target_query_local_native_by_source_type (target, node->produced_type, node->produced_name);

            if (entry && entry->linked_byproduct)
            {
                byproduct_node_add_production_unsafe (entry->linked_byproduct, resource_type, resource_name);
            }
            else
            {
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                         "[Target \"%s\"] Failed to confirm byproduct \"%s\" of type \"%s\" production from \"%s\" of "
                         "type \"%s\" as byproduct is missing. Broken cache?",
                         target->name, node->produced_name, node->produced_type, resource_name, resource_type)
                kan_atomic_int_add (&global.errors_count, 1);
            }
        }

        node = (struct byproduct_production_node_t *) node->node.list_node.next;
    }

    kan_atomic_int_unlock (&target->byproduct_registration_lock);
}

static kan_interned_string_t interface_register_byproduct (kan_functor_user_data_t interface_user_data,
                                                           kan_interned_string_t byproduct_type_name,
                                                           void *byproduct_data)
{
    struct native_entry_node_t *source_node = (struct native_entry_node_t *) interface_user_data;
    const struct kan_reflection_struct_t *byproduct_type =
        kan_reflection_registry_query_struct (global.registry, byproduct_type_name);

    if (!byproduct_type)
    {
        KAN_LOG (
            application_framework_resource_builder, KAN_LOG_ERROR,
            "[Target \"%s\"] Failed to register byproduct of unknown type \"%s\" from native resource \"%s\" of type "
            "\"%s\".",
            source_node->target->name, byproduct_type_name, source_node->name, source_node->source_type->name)
        kan_atomic_int_add (&global.errors_count, 1);
        return NULL;
    }

    const struct kan_resource_byproduct_type_meta_t *meta =
        find_singular_struct_meta (byproduct_type_name, interned_kan_resource_byproduct_type_meta_t);

    if (!meta)
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to register byproduct of type \"%s\" from native resource \"%s\" of type "
                 "\"%s\" as it has no \"kan_resource_byproduct_type_meta_t\" meta.",
                 source_node->target->name, byproduct_type_name, source_node->name, source_node->source_type->name)
        kan_atomic_int_add (&global.errors_count, 1);
        return NULL;
    }

    const kan_hash_t byproduct_hash = meta->hash ?
                                          meta->hash (byproduct_data) :
                                          kan_reflection_hash_struct (global.registry, byproduct_type, byproduct_data);

    kan_atomic_int_lock (&source_node->target->byproduct_registration_lock);
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&source_node->target->byproducts, (kan_hash_t) byproduct_hash);
    struct byproduct_node_t *node = (struct byproduct_node_t *) bucket->first;
    const struct byproduct_node_t *node_end = (struct byproduct_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        KAN_ASSERT (node->entry->source_data)
        if (node->node.hash == byproduct_hash && node->entry->source_type == byproduct_type &&
            (meta->is_equal ? meta->is_equal (node->entry->source_data, byproduct_data) :
                              kan_reflection_are_structs_equal (global.registry, byproduct_type,
                                                                node->entry->source_data, byproduct_data)))
        {
            break;
        }

        node = (struct byproduct_node_t *) node->node.list_node.next;
    }

    const kan_bool_t needs_new_node = node == node_end;
    if (needs_new_node)
    {
        node = kan_allocate_batched (nodes_allocation_group, sizeof (struct byproduct_node_t));
        byproduct_node_init (node);
        node->node.hash = byproduct_hash;

        char name_buffer[KAN_RESOURCE_BUILDER_BYPRODUCT_NAME_BUFFER];
        snprintf (name_buffer, KAN_RESOURCE_BUILDER_BYPRODUCT_NAME_BUFFER, "%s_byproduct_%lu",
                  source_node->target->name,
                  (unsigned long) kan_atomic_int_add (&source_node->target->byproduct_index_generator, 1));

        struct kan_file_system_path_container_t path_container;
        kan_file_system_path_container_copy_string (&path_container,
                                                    VFS_OUTPUT_DIRECTORY "/" SUB_DIRECTORY_RAW_BYPRODUCT_CACHE);

        kan_file_system_path_container_append (&path_container, source_node->target->name);
        kan_virtual_file_system_make_directory (global.volume, path_container.path);

        kan_file_system_path_container_append (&path_container, byproduct_type->name);
        kan_virtual_file_system_make_directory (global.volume, path_container.path);

        kan_file_system_path_container_append (&path_container, name_buffer);
        kan_file_system_path_container_add_suffix (&path_container, ".bin");

        node->entry = native_entry_node_create (source_node->target, byproduct_type_name,
                                                kan_string_intern (name_buffer), path_container.path);

        if (!node->entry)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to register byproduct of type \"%s\" from native resource \"%s\" of type "
                     "\"%s\" as its native entry creation has failed.",
                     source_node->target->name, byproduct_type_name, source_node->name, source_node->source_type->name)
            kan_atomic_int_add (&global.errors_count, 1);
            kan_atomic_int_unlock (&source_node->target->byproduct_registration_lock);
            return NULL;
        }

        node->entry->linked_byproduct = node;
        node->entry->source_data = kan_allocate_general (loaded_native_entries_allocation_group, byproduct_type->size,
                                                         byproduct_type->alignment);

        // Always referenced as needed for byproduct reduction.
        node->entry->source_references = kan_atomic_int_init (1);

        if (byproduct_type->init)
        {
            kan_allocation_group_stack_push (loaded_native_entries_allocation_group);
            byproduct_type->init (byproduct_type->functor_user_data, node->entry->source_data);
            kan_allocation_group_stack_pop ();
        }

        if (meta->move)
        {
            meta->move (node->entry->source_data, byproduct_data);
        }
        else
        {
            kan_reflection_move_struct (global.registry, byproduct_type, node->entry->source_data, byproduct_data);
        }

        kan_hash_storage_update_bucket_count_default (&source_node->target->native,
                                                      KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS);
        kan_hash_storage_add (&source_node->target->native, &node->entry->node);

        kan_hash_storage_update_bucket_count_default (&source_node->target->byproducts,
                                                      KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS);
        kan_hash_storage_add (&source_node->target->byproducts, &node->node);
    }

    byproduct_node_add_production_unsafe (node, source_node->source_type->name, source_node->name);
    kan_atomic_int_unlock (&source_node->target->byproduct_registration_lock);

    if (needs_new_node)
    {
        // New byproduct, save it to cache.
        if (!save_native_data (node->entry->source_data, node->entry->source_path, node->entry->source_type->name,
                               KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t), KAN_TRUE))
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to save byproduct of type \"%s\" from native resource \"%s\" of type "
                     "\"%s\" into byproduct cache.",
                     source_node->target->name, byproduct_type_name, source_node->name, source_node->source_type->name)
            kan_atomic_int_add (&global.errors_count, 1);
        }
    }
    else
    {
        // Byproduct already exist, reset user data.
        if (meta->reset)
        {
            meta->reset (byproduct_data);
        }
        else
        {
            kan_reflection_reset_struct (global.registry, byproduct_type, byproduct_data);
        }
    }

    return node->entry->name;
}

static void save_references_to_cache (struct native_entry_node_t *node, kan_bool_t compiled)
{
    struct kan_file_system_path_container_t path_container;
    if (compiled)
    {
        form_compiled_references_cache_directory_path (node, &path_container);
        kan_virtual_file_system_make_directory (global.volume, path_container.path);
        form_compiled_references_cache_item_path (node, &path_container);
    }
    else
    {
        form_references_cache_directory_path (node, &path_container);
        kan_virtual_file_system_make_directory (global.volume, path_container.path);
        form_references_cache_item_path (node, &path_container);
    }

    struct kan_stream_t *stream = kan_virtual_file_stream_open_for_write (global.volume, path_container.path);
    if (!stream)
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to save compiled native resource \"%s\" of type \"%s\" reference cache.",
                 node->target->name, node->name, node->source_type->name)
        return;
    }

    stream = kan_random_access_stream_buffer_open_for_write (stream, KAN_RESOURCE_BUILDER_IO_BUFFER);
    kan_serialization_binary_writer_t writer = kan_serialization_binary_writer_create (
        stream, compiled ? &node->compiled_detected_references : &node->source_detected_references,
        kan_string_intern ("kan_resource_detected_reference_container_t"), global.binary_script_storage,
        KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));

    enum kan_serialization_state_t serialization_state;
    while ((serialization_state = kan_serialization_binary_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    kan_serialization_binary_writer_destroy (writer);
    stream->operations->close (stream);

    if (serialization_state == KAN_SERIALIZATION_FAILED)
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to save %s native resource \"%s\" of type \"%s\" reference cache.",
                 node->target->name, compiled ? "compiled" : "source", node->name, node->source_type->name)
    }

    KAN_ASSERT (serialization_state == KAN_SERIALIZATION_FINISHED)
}

static void process_native_node_compilation (kan_functor_user_data_t user_data)
{
    struct native_entry_node_t *node = (struct native_entry_node_t *) user_data;
    switch (node->compilation_status)
    {
    case COMPILATION_STATUS_NOT_YET:
    case COMPILATION_STATUS_FINISHED:
    case COMPILATION_STATUS_FAILED:
        KAN_ASSERT (KAN_FALSE)
        break;

    case COMPILATION_STATUS_REQUESTED:
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO,
                 "[Target \"%s\"] Processing request to compile native resource \"%s\" of type \"%s\".",
                 node->target->name, node->name, node->source_type->name)

        struct kan_file_system_path_container_t path_container;
        form_references_cache_item_path (node, &path_container);

        const kan_time_size_t source_time_ns = get_file_last_modification_time_ns (node->source_path);
        const kan_time_size_t reference_cache_time_ns = get_file_last_modification_time_ns (path_container.path);
        node->loaded_references_from_cache = KAN_FALSE;

        const kan_bool_t cache_is_up_to_date =
            reference_cache_time_ns > source_time_ns &&
            reference_cache_time_ns > global.newest_loaded_plugin_last_modification_file_time_ns;

        if (cache_is_up_to_date)
        {
            if ((node->loaded_references_from_cache =
                     read_detected_references_cache (node, &node->source_detected_references, path_container.path)))
            {
                if (request_compiled_dependencies (node))
                {
                    node->pending_compilation_status = COMPILATION_STATUS_WAITING_FOR_COMPILED_DEPENDENCIES;
                }
                else
                {
                    kan_atomic_int_add (&global.errors_count, 1);
                    node->pending_compilation_status = COMPILATION_STATUS_FAILED;
                }

                return;
            }
        }

        add_native_source_reference (node);
        node->pending_compilation_status = COMPILATION_STATUS_LOADING_SOURCE_FOR_REFERENCES;
        return;
    }

    case COMPILATION_STATUS_LOADING_SOURCE_FOR_REFERENCES:
    {
        if (!node->source_data)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to compile native resource \"%s\" of type \"%s\" due to failure to load "
                     "source data.",
                     node->target->name, node->name, node->source_type->name)
            kan_atomic_int_add (&global.errors_count, 1);
            node->pending_compilation_status = COMPILATION_STATUS_FAILED;
            return;
        }

        KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO,
                 "[Target \"%s\"] Detecting reference of native resource \"%s\" of type \"%s\".", node->target->name,
                 node->name, node->source_type->name)

        kan_resource_detect_references (&global.reference_type_info_storage, node->source_type->name, node->source_data,
                                        &node->source_detected_references);
        save_references_to_cache (node, KAN_FALSE);

        if (request_compiled_dependencies (node))
        {
            node->pending_compilation_status = COMPILATION_STATUS_WAITING_FOR_COMPILED_DEPENDENCIES;
        }
        else
        {
            kan_atomic_int_add (&global.errors_count, 1);
            node->pending_compilation_status = COMPILATION_STATUS_FAILED;
        }

        return;
    }

    case COMPILATION_STATUS_WAITING_FOR_COMPILED_DEPENDENCIES:
    {
        kan_bool_t all_compiled_dependencies_ready = KAN_TRUE;
        for (kan_loop_size_t index = 0u; index < node->source_detected_references.detected_references.size; ++index)
        {
            struct kan_resource_detected_reference_t *reference =
                &((struct kan_resource_detected_reference_t *)
                      node->source_detected_references.detected_references.data)[index];

            if (reference->compilation_usage == KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED)
            {
                struct native_entry_node_t *dependency =
                    target_query_global_native_by_source_type (node->target, reference->type, reference->name);

                // We've already found this earlier, it can't be null.
                KAN_ASSERT (dependency)

                if (dependency->compilation_status == COMPILATION_STATUS_FAILED)
                {
                    KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                             "[Target \"%s\"] Failed to compile native resource \"%s\" of type \"%s\" as its "
                             "compile dependency \"%s\" of type \"%s\" failed to compile",
                             node->target->name, node->name, node->source_type->name, reference->type, reference->name)

                    kan_atomic_int_add (&global.errors_count, 1);
                    node->pending_compilation_status = COMPILATION_STATUS_FAILED;
                    remove_requests_for_compiled_dependencies (node);

                    if (!node->loaded_references_from_cache)
                    {
                        remove_native_source_reference (node);
                    }

                    return;
                }
                else if (dependency->compilation_status != COMPILATION_STATUS_FINISHED)
                {
                    all_compiled_dependencies_ready = KAN_FALSE;
                }
            }
        }

        if (all_compiled_dependencies_ready)
        {
            if (kan_virtual_file_system_check_existence (global.volume, node->compiled_path) &&
                is_compiled_data_newer_than_dependencies (node))
            {
                struct kan_file_system_path_container_t path_container;
                form_compiled_references_cache_item_path (node, &path_container);

                if (read_detected_references_cache (node, &node->compiled_detected_references, path_container.path))
                {
                    KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO,
                             "[Target \"%s\"] Native resource \"%s\" of type \"%s\" is up to date.", node->target->name,
                             node->name, node->source_type->name)
                    node->pending_compilation_status = COMPILATION_STATUS_FINISHED;
                    remove_requests_for_compiled_dependencies (node);

                    if (!node->loaded_references_from_cache)
                    {
                        remove_native_source_reference (node);
                    }

                    confirm_loaded_byproduct_production (node->target, node->source_type->name, node->name);
                    return;
                }
            }

            if (!request_raw_dependencies (node))
            {
                kan_atomic_int_add (&global.errors_count, 1);
                node->pending_compilation_status = COMPILATION_STATUS_FAILED;
                remove_requests_for_compiled_dependencies (node);

                if (!node->loaded_references_from_cache)
                {
                    remove_native_source_reference (node);
                }

                return;
            }

            if (node->loaded_references_from_cache)
            {
                add_native_source_reference (node);
            }

            node->pending_compilation_status = COMPILATION_STATUS_COMPILE;
        }
        else
        {
            wait_in_passive_queue (node);
        }

        return;
    }

    case COMPILATION_STATUS_COMPILE:
    {
        kan_bool_t everything_ready_for_compilation = KAN_TRUE;
        if (!node->source_data)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to compile native resource \"%s\" of type \"%s\" due to failure to load "
                     "source data.",
                     node->target->name, node->name, node->source_type->name)
            everything_ready_for_compilation = KAN_FALSE;
        }

        // Won't be needed after compilation.
        remove_native_source_reference (node);

        KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO,
                 "[Target \"%s\"] Compiling native resource \"%s\" of type \"%s\".", node->target->name, node->name,
                 node->source_type->name)
        kan_instance_size_t dependencies_count = 0u;

        for (kan_loop_size_t index = 0u; index < node->source_detected_references.detected_references.size; ++index)
        {
            struct kan_resource_detected_reference_t *reference =
                &((struct kan_resource_detected_reference_t *)
                      node->source_detected_references.detected_references.data)[index];

            if (reference->compilation_usage == KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW)
            {
                ++dependencies_count;
                if (reference->type)
                {
                    struct native_entry_node_t *dependency =
                        target_query_global_native_by_source_type (node->target, reference->type, reference->name);
                    KAN_ASSERT (dependency)

                    if (!dependency->source_data)
                    {
                        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                                 "[Target \"%s\"] Failed to compile native resource \"%s\" of type \"%s\" as "
                                 "native dependency \"%s\" of type \"%s\" failed to load.",
                                 node->target->name, node->name, node->source_type->name, reference->name,
                                 reference->type)
                        everything_ready_for_compilation = KAN_FALSE;
                    }

                    // Won't be needed after compilation.
                    remove_native_source_reference (dependency);
                }
                else
                {
                    struct third_party_entry_node_t *dependency =
                        target_query_global_third_party (node->target, reference->name);
                    KAN_ASSERT (dependency)

                    if (!dependency->data)
                    {
                        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                                 "[Target \"%s\"] Failed to compile native resource \"%s\" of type \"%s\" as "
                                 "third party dependency \"%s\" failed to load.",
                                 node->target->name, node->name, node->source_type->name, reference->name)
                        everything_ready_for_compilation = KAN_FALSE;
                    }

                    // Won't be needed after compilation.
                    remove_third_party_reference (dependency);
                }
            }
            else if (reference->compilation_usage == KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED)
            {
                ++dependencies_count;
                struct native_entry_node_t *dependency =
                    target_query_global_native_by_source_type (node->target, reference->type, reference->name);
                KAN_ASSERT (dependency)

                if (!dependency->compiled_data)
                {
                    KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                             "[Target \"%s\"] Failed to compile native resource \"%s\" of type \"%s\" as "
                             "native dependency \"%s\" of type \"%s\" failed to load.",
                             node->target->name, node->name, node->source_type->name, reference->name, reference->type)
                    everything_ready_for_compilation = KAN_FALSE;
                }

                // Won't be needed after compilation.
                remove_native_compiled_reference (dependency);
            }
        }

        if (!everything_ready_for_compilation)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to compile native resource \"%s\" of type \"%s\" as not all dependencies "
                     "are ready.",
                     node->target->name, node->name, node->source_type->name)

            kan_atomic_int_add (&global.errors_count, 1);
            node->pending_compilation_status = COMPILATION_STATUS_FAILED;
            return;
        }

        struct kan_resource_compilation_dependency_t *dependency_array = NULL;
        if (dependencies_count > 0u)
        {
            dependency_array = kan_allocate_general (
                temporary_allocation_group, sizeof (struct kan_resource_compilation_dependency_t) * dependencies_count,
                _Alignof (struct kan_resource_compilation_dependency_t));
            kan_instance_size_t dependency_index = 0u;

            for (kan_loop_size_t index = 0u; index < node->source_detected_references.detected_references.size; ++index)
            {
                struct kan_resource_detected_reference_t *reference =
                    &((struct kan_resource_detected_reference_t *)
                          node->source_detected_references.detected_references.data)[index];

                if (reference->compilation_usage == KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW)
                {
                    dependency_array[dependency_index].type = reference->type;
                    dependency_array[dependency_index].name = reference->name;

                    if (reference->type)
                    {
                        struct native_entry_node_t *dependency =
                            target_query_global_native_by_source_type (node->target, reference->type, reference->name);
                        KAN_ASSERT (dependency)
                        KAN_ASSERT (dependency->source_data)
                        dependency_array[dependency_index].data = dependency->source_data;
                    }
                    else
                    {
                        struct third_party_entry_node_t *dependency =
                            target_query_global_third_party (node->target, reference->name);
                        KAN_ASSERT (dependency)
                        KAN_ASSERT (dependency->data)
                        dependency_array[dependency_index].data = dependency->data;
                    }

                    ++dependency_index;
                }
                else if (reference->compilation_usage == KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED)
                {
                    struct native_entry_node_t *dependency =
                        target_query_global_native_by_source_type (node->target, reference->type, reference->name);
                    KAN_ASSERT (dependency)
                    KAN_ASSERT (dependency->compiled_data)

                    dependency_array[dependency_index].type = dependency->compiled_type->name;
                    dependency_array[dependency_index].name = reference->name;
                    dependency_array[dependency_index].data = dependency->compiled_data;
                    ++dependency_index;
                }
            }
        }

        KAN_ASSERT (!node->compiled_data)
        if (node->compilable_meta)
        {
            node->compiled_data = kan_allocate_general (loaded_native_entries_allocation_group,
                                                        node->compiled_type->size, node->compiled_type->alignment);

            if (node->compiled_type->init)
            {
                kan_allocation_group_stack_push (loaded_native_entries_allocation_group);
                node->compiled_type->init (node->compiled_type->functor_user_data, node->compiled_data);
                kan_allocation_group_stack_pop ();
            }

            struct kan_resource_compile_state_t state = {
                .input_instance = node->source_data,
                .output_instance = node->compiled_data,
                .platform_configuration = NULL,
                .deadline = KAN_INT_MAX (kan_time_size_t),
                .user_state = NULL,
                .runtime_compilation = KAN_FALSE,
                .dependencies_count = dependencies_count,
                .dependencies = dependency_array,
                .interface_user_data = (kan_functor_user_data_t) node,
                .register_byproduct = interface_register_byproduct,
            };

            enum kan_resource_compile_result_t compile_result = KAN_RESOURCE_PIPELINE_COMPILE_IN_PROGRESS;
            if (node->compilable_meta->configuration_type_name)
            {
                struct platform_configuration_t *configuration = global.platform_configurations;
                while (configuration)
                {
                    if (strcmp (configuration->type->name, node->compilable_meta->configuration_type_name) == 0)
                    {
                        state.platform_configuration = configuration->data;
                        break;
                    }
                }

                if (!configuration)
                {
                    KAN_LOG (
                        application_framework_resource_builder, KAN_LOG_ERROR,
                        "[Target \"%s\"] Failed to retrieve platform configuration for native resource \"%s\" of type "
                        "\"%s\" as there is no platform configuration with type \"%s\".",
                        node->target->name, node->name, node->source_type->name,
                        node->compilable_meta->configuration_type_name)
                    kan_atomic_int_add (&global.errors_count, 1);
                    compile_result = KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
                }
            }

            const struct kan_reflection_struct_t *compilation_state_type = NULL;
            if (compile_result == KAN_RESOURCE_PIPELINE_COMPILE_IN_PROGRESS && node->compilable_meta->state_type_name)
            {
                compilation_state_type = kan_reflection_registry_query_struct (
                    global.registry, kan_string_intern (node->compilable_meta->state_type_name));

                if (compilation_state_type)
                {
                    state.user_state =
                        kan_allocate_general (compilation_state_allocation_group, compilation_state_type->size,
                                              compilation_state_type->alignment);

                    if (compilation_state_type->init)
                    {
                        kan_allocation_group_stack_push (compilation_state_allocation_group);
                        compilation_state_type->init (compilation_state_type->functor_user_data, state.user_state);
                        kan_allocation_group_stack_pop ();
                    }
                }
                else
                {
                    KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                             "[Target \"%s\"] Failed to allocate compilation state for native resource \"%s\" of type "
                             "\"%s\" as there is no type \"%s\".",
                             node->target->name, node->name, node->source_type->name,
                             node->compilable_meta->output_type_name)
                    kan_atomic_int_add (&global.errors_count, 1);
                    compile_result = KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
                }
            }

            if (compile_result == KAN_RESOURCE_PIPELINE_COMPILE_IN_PROGRESS)
            {
                while ((compile_result = node->compilable_meta->functor (&state)) ==
                       KAN_RESOURCE_PIPELINE_COMPILE_IN_PROGRESS)
                {
                }
            }

            if (state.user_state)
            {
                if (compilation_state_type->shutdown)
                {
                    kan_allocation_group_stack_push (compilation_state_allocation_group);
                    compilation_state_type->shutdown (compilation_state_type->functor_user_data, state.user_state);
                    kan_allocation_group_stack_pop ();
                }

                kan_free_general (compilation_state_allocation_group, state.user_state, compilation_state_type->size);
            }

            if (dependency_array)
            {
                kan_free_general (temporary_allocation_group, dependency_array,
                                  sizeof (struct kan_resource_compilation_dependency_t) * dependencies_count);
            }

            if (compile_result == KAN_RESOURCE_PIPELINE_COMPILE_FINISHED)
            {
                if (!save_native_data (node->compiled_data, node->compiled_path, node->compiled_type->name,
                                       KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t), KAN_TRUE))
                {
                    native_entry_node_unload_compiled (node);
                    KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                             "[Target \"%s\"] Failed to compile native resource \"%s\" of type \"%s\" due to failure "
                             "during saving.",
                             node->target->name, node->name, node->source_type->name)
                    kan_atomic_int_add (&global.errors_count, 1);
                    node->pending_compilation_status = COMPILATION_STATUS_FAILED;
                    return;
                }

                kan_resource_detect_references (&global.reference_type_info_storage, node->compiled_type->name,
                                                node->compiled_data, &node->compiled_detected_references);

                save_references_to_cache (node, KAN_TRUE);
                node->pending_compilation_status = COMPILATION_STATUS_FINISHED;
            }
            else
            {
                KAN_ASSERT (compile_result == KAN_RESOURCE_PIPELINE_COMPILE_FAILED)
                native_entry_node_unload_compiled (node);
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                         "[Target \"%s\"] Failed to compile native resource \"%s\" of type \"%s\".", node->target->name,
                         node->name, node->source_type->name)
                kan_atomic_int_add (&global.errors_count, 1);
                node->pending_compilation_status = COMPILATION_STATUS_FAILED;
                return;
            }
        }
        else
        {
            KAN_ASSERT (node->source_type == node->compiled_type)
            if (!save_native_data (node->source_data, node->compiled_path, node->source_type->name,
                                   KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t), KAN_TRUE))
            {
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                         "[Target \"%s\"] Failed to compile native resource \"%s\" of type \"%s\" due to failure "
                         "during saving.",
                         node->target->name, node->name, node->source_type->name)
                kan_atomic_int_add (&global.errors_count, 1);
                node->pending_compilation_status = COMPILATION_STATUS_FAILED;
                return;
            }

            kan_resource_detect_references (&global.reference_type_info_storage, node->source_type->name,
                                            node->source_data, &node->compiled_detected_references);

            save_references_to_cache (node, KAN_TRUE);
            node->pending_compilation_status = COMPILATION_STATUS_FINISHED;
        }

        return;
    }
    }
}

static void compilation_loop (void)
{
    // We cannot risk compiling everything at once, because we can run out of memory.
    const kan_instance_size_t max_in_active_queue = kan_platform_get_cpu_logical_core_count ();

    const kan_interned_string_t interned_manage_resources_native = kan_string_intern ("manage_resources_native");
    const kan_interned_string_t interned_manage_resources_third_party =
        kan_string_intern ("manage_resources_third_party");
    const kan_interned_string_t interned_process_native_node_compilation =
        kan_string_intern ("process_native_node_compilation");

    while (global.compilation_active_queue || global.compilation_passive_queue_first)
    {
        struct native_entry_node_t *native_node = global.compilation_active_queue;
        kan_instance_size_t actual_active_nodes = 0u;
        kan_bool_t changed_statuses = KAN_FALSE;
        const kan_bool_t had_empty_active_queue = global.compilation_active_queue == NULL;

        while (native_node)
        {
            KAN_ASSERT (native_node->compilation_queue == NATIVE_NODE_COMPILATION_QUEUE_ACTIVE)
            struct native_entry_node_t *next = native_node->next_node_in_compilation_queue;

            if (native_node->compilation_status != native_node->pending_compilation_status)
            {
                native_node->compilation_status = native_node->pending_compilation_status;
                changed_statuses = KAN_TRUE;
            }

            switch (native_node->compilation_status)
            {
            case COMPILATION_STATUS_NOT_YET:
            case COMPILATION_STATUS_REQUESTED:
            case COMPILATION_STATUS_LOADING_SOURCE_FOR_REFERENCES:
            case COMPILATION_STATUS_WAITING_FOR_COMPILED_DEPENDENCIES:
            case COMPILATION_STATUS_COMPILE:
                ++actual_active_nodes;
                break;

            case COMPILATION_STATUS_FINISHED:
                if (native_node->should_be_included_in_pack)
                {
                    recursively_add_to_pack (native_node);
                }

                // Update resources usage after status is changed to finished as management is only enabled for
                // finished compiled data.
                add_native_to_resource_management_pass (native_node);

                remove_from_active_queue (native_node);
                break;

            case COMPILATION_STATUS_FAILED:
                remove_from_active_queue (native_node);
                break;
            }

            native_node = next;
        }

        while (global.compilation_passive_queue_first && actual_active_nodes < max_in_active_queue)
        {
            native_node = global.compilation_passive_queue_first;
            KAN_ASSERT (native_node->compilation_queue == NATIVE_NODE_COMPILATION_QUEUE_PASSIVE)
            global.compilation_passive_queue_first = native_node->next_node_in_compilation_queue;

            if (native_node->next_node_in_compilation_queue)
            {
                native_node->next_node_in_compilation_queue->previous_node_in_compilation_queue = NULL;
            }

            if (native_node == global.compilation_passive_queue_last)
            {
                global.compilation_passive_queue_last = NULL;
            }

            native_node->compilation_queue = NATIVE_NODE_COMPILATION_QUEUE_ACTIVE;
            native_node->next_node_in_compilation_queue = global.compilation_active_queue;

            if (native_node->next_node_in_compilation_queue)
            {
                native_node->next_node_in_compilation_queue->previous_node_in_compilation_queue = native_node;
            }

            global.compilation_active_queue = native_node;
            ++actual_active_nodes;

            if (native_node->compilation_status != native_node->pending_compilation_status)
            {
                native_node->compilation_status = native_node->pending_compilation_status;
                // Cannot finish or fail while being in passive mode.
                KAN_ASSERT (native_node->compilation_status != COMPILATION_STATUS_FINISHED &&
                            native_node->compilation_status != COMPILATION_STATUS_FAILED)
                changed_statuses = KAN_TRUE;
            }
        }

        if (had_empty_active_queue && !changed_statuses)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                     "Exiting compilation loop due to detected deadlock. Printing nodes in queues.")

            native_node = global.compilation_active_queue;
            while (native_node)
            {
                print_node_in_dead_lock_queue (native_node);
                native_node = native_node->next_node_in_compilation_queue;
            }

            native_node = global.compilation_passive_queue_first;
            while (native_node)
            {
                print_node_in_dead_lock_queue (native_node);
                native_node = native_node->next_node_in_compilation_queue;
            }

            kan_atomic_int_add (&global.errors_count, 1);
            return;
        }

        struct kan_cpu_task_list_node_t *task_list = NULL;
        native_node = global.resource_management_native_queue;

        while (native_node)
        {
            KAN_ASSERT (native_node->queued_for_resource_management)
            KAN_CPU_TASK_LIST_USER_VALUE (&task_list, &global.temporary_allocator, interned_manage_resources_native,
                                          manage_resources_native, native_node)
            native_node = native_node->next_node_in_resource_management_queue;
        }

        struct third_party_entry_node_t *third_party_node = global.resource_management_third_party_queue;
        while (third_party_node)
        {
            KAN_ASSERT (third_party_node->queued_for_resource_management)
            KAN_CPU_TASK_LIST_USER_VALUE (&task_list, &global.temporary_allocator,
                                          interned_manage_resources_third_party, manage_resources_third_party,
                                          third_party_node)
            third_party_node = third_party_node->next_node_in_resource_management_queue;
        }

        if (task_list)
        {
            kan_cpu_job_t manage_job = kan_cpu_job_create ();
            kan_cpu_job_dispatch_and_detach_task_list (manage_job, task_list);
            kan_cpu_job_release (manage_job);
            kan_cpu_job_wait (manage_job);

            global.resource_management_native_queue = NULL;
            global.resource_management_third_party_queue = NULL;
            kan_stack_group_allocator_reset (&global.temporary_allocator);
        }

        task_list = NULL;
        native_node = global.compilation_active_queue;

        while (native_node)
        {
            KAN_ASSERT (native_node->compilation_queue == NATIVE_NODE_COMPILATION_QUEUE_ACTIVE)
            KAN_CPU_TASK_LIST_USER_VALUE (&task_list, &global.temporary_allocator,
                                          interned_process_native_node_compilation, process_native_node_compilation,
                                          native_node)
            native_node = native_node->next_node_in_compilation_queue;
        }

        if (task_list)
        {
            kan_cpu_job_t run_compilation_job = kan_cpu_job_create ();
            kan_cpu_job_dispatch_and_detach_task_list (run_compilation_job, task_list);
            kan_cpu_job_release (run_compilation_job);
            kan_cpu_job_wait (run_compilation_job);
            kan_stack_group_allocator_reset (&global.temporary_allocator);
        }
    }
}

static void save_target_byproduct_state (kan_functor_user_data_t user_data)
{
    struct target_t *target = (struct target_t *) user_data;
    struct kan_resource_target_byproduct_state_t state;
    kan_resource_target_byproduct_state_init (&state);

    state.byproduct_index_generator = (kan_instance_size_t) kan_atomic_int_get (&target->byproduct_index_generator);
    // Production will not be smaller than produced byproducts any way.
    kan_dynamic_array_set_capacity (&state.production, target->byproducts.items.size);
    struct byproduct_node_t *node = (struct byproduct_node_t *) target->byproducts.items.first;

    while (node)
    {
        for (kan_loop_size_t index = 0u; index < node->produced_from.size; ++index)
        {
            struct byproduct_production_source_t *input =
                &((struct byproduct_production_source_t *) node->produced_from.data)[index];

            struct kan_resource_target_byproduct_production_t *output = kan_dynamic_array_add_last (&state.production);
            if (!output)
            {
                kan_dynamic_array_set_capacity (&state.production, state.production.size * 2u);
                output = kan_dynamic_array_add_last (&state.production);
                KAN_ASSERT (output)
            }

            output->resource_type = input->resource_type;
            output->resource_name = input->resource_name;
            output->byproduct_type = node->entry->source_type->name;
            output->byproduct_name = node->entry->name;
        }

        if (node->produced_from.size == 0u)
        {
            // Node weren't actually produced and only exists due to cache from previous iteration. Delete it.
            kan_virtual_file_system_remove_file (global.volume, node->entry->source_path);
        }

        node = (struct byproduct_node_t *) node->node.list_node.next;
    }

    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_string (&path_container,
                                                VFS_OUTPUT_DIRECTORY "/" SUB_DIRECTORY_RAW_BYPRODUCT_CACHE);
    kan_file_system_path_container_append (&path_container, target->name);
    kan_file_system_path_container_add_suffix (&path_container, BYPRODUCT_STATE_FILE_SUFFIX);

    if (!save_native_data (&state, path_container.path, interned_kan_resource_target_byproduct_state_t,
                           KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t), KAN_TRUE))
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder, KAN_LOG_INFO,
                             "[Target \"%s\"] Failed to save byproduct state to \"%s\".", target->name,
                             path_container.path)
        kan_atomic_int_add (&global.errors_count, 1);
    }

    kan_resource_target_byproduct_state_shutdown (&state);
    KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "[Target \"%s\"] Done saving byproduct state.",
             target->name)
}

static void intern_strings_in_native (kan_functor_user_data_t user_data)
{
    struct native_entry_node_t *node = (struct native_entry_node_t *) user_data;
    KAN_ASSERT (node->compilation_status == COMPILATION_STATUS_FINISHED)
    // Everything should be unloaded after compilation.
    KAN_ASSERT (!node->compiled_data)

    node->compiled_data = load_native_data (node->compiled_type, node->compiled_path,
                                            KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));

    if (!node->compiled_data)
    {
        return;
    }

    kan_free_general (nodes_allocation_group, node->compiled_path, strlen (node->compiled_path) + 1u);
    struct kan_file_system_path_container_t new_path_container;
    kan_file_system_path_container_copy_string (&new_path_container, VFS_OUTPUT_DIRECTORY "/" SUB_DIRECTORY_TEMPORARY);
    kan_file_system_path_container_append (&new_path_container, node->compiled_type->name);
    kan_virtual_file_system_make_directory (global.volume, new_path_container.path);
    kan_file_system_path_container_append (&new_path_container, node->name);
    kan_file_system_path_container_add_suffix (&new_path_container, ".bin");

    node->compiled_path =
        kan_allocate_general (nodes_allocation_group, new_path_container.length + 1u, _Alignof (char));
    memcpy (node->compiled_path, new_path_container.path, new_path_container.length + 1u);

    const kan_bool_t successful = save_native_data (node->compiled_data, node->compiled_path, node->compiled_type->name,
                                                    node->target->interned_string_registry, KAN_TRUE);
    native_entry_node_unload_compiled (node);

    if (successful)
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO,
                 "[Target \"%s\"] Finished interning strings in native resource \"%s\" of type \"%s\".",
                 node->target->name, node->name, node->compiled_type->name)
    }
}

static inline void form_native_node_path_in_pack (struct native_entry_node_t *native_node,
                                                  struct kan_file_system_path_container_t *output)
{
    kan_file_system_path_container_copy_string (output, native_node->compiled_type->name);
    kan_file_system_path_container_append (output, native_node->name);
    kan_file_system_path_container_add_suffix (output, ".bin");
}

static inline void form_third_party_node_path_in_pack (struct third_party_entry_node_t *third_party_node,
                                                       struct kan_file_system_path_container_t *output)
{
    kan_file_system_path_container_copy_string (output, "third_party");
    kan_file_system_path_container_append (output, third_party_node->name);
    kan_file_system_path_container_add_suffix (output, ".bin");
}

static inline void form_temporary_index_path (struct target_t *target, struct kan_file_system_path_container_t *output)
{
    kan_file_system_path_container_copy_string (output, VFS_OUTPUT_DIRECTORY "/" SUB_DIRECTORY_TEMPORARY);
    kan_file_system_path_container_append (output, target->name);
    kan_file_system_path_container_add_suffix (output, KAN_RESOURCE_INDEX_DEFAULT_NAME);
}

static inline void form_temporary_string_registry_path (struct target_t *target,
                                                        struct kan_file_system_path_container_t *output)
{
    kan_file_system_path_container_copy_string (output, VFS_OUTPUT_DIRECTORY "/" SUB_DIRECTORY_TEMPORARY);
    kan_file_system_path_container_append (output, target->name);
    kan_file_system_path_container_add_suffix (output, KAN_RESOURCE_INDEX_ACCOMPANYING_STRING_REGISTRY_DEFAULT_NAME);
}

static inline kan_bool_t add_to_pack (kan_virtual_file_system_read_only_pack_builder_t builder,
                                      const char *path,
                                      const char *path_in_pack)
{
    struct kan_stream_t *resource_stream = kan_virtual_file_stream_open_for_read (global.volume, path);
    if (!resource_stream)
    {
        // Need to finalize before destruction from outside.
        kan_virtual_file_system_read_only_pack_builder_finalize (builder);
        kan_virtual_file_system_read_only_pack_builder_destroy (builder);
        return KAN_FALSE;
    }

    resource_stream = kan_random_access_stream_buffer_open_for_read (resource_stream, KAN_RESOURCE_BUILDER_IO_BUFFER);
    const kan_bool_t added =
        kan_virtual_file_system_read_only_pack_builder_add (builder, resource_stream, path_in_pack);

    resource_stream->operations->close (resource_stream);

    if (!added)
    {
        kan_virtual_file_system_read_only_pack_builder_destroy (builder);
        return KAN_FALSE;
    }

    return KAN_TRUE;
}

static void pack_target (kan_functor_user_data_t user_data)
{
    struct target_t *target = (struct target_t *) user_data;
    struct kan_file_system_path_container_t path_container;

    KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "[Target \"%s\"] Building index...", target->name)
    struct kan_resource_index_t resource_index;
    kan_resource_index_init (&resource_index);
    struct native_entry_node_t *native_node = (struct native_entry_node_t *) target->native.items.first;

    while (native_node)
    {
        if (native_node->should_be_included_in_pack)
        {
            form_native_node_path_in_pack (native_node, &path_container);
            kan_resource_index_add_native_entry (&resource_index, native_node->compiled_type->name, native_node->name,
                                                 KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY, path_container.path);
        }

        native_node = (struct native_entry_node_t *) native_node->node.list_node.next;
    }

    struct third_party_entry_node_t *third_party_node =
        (struct third_party_entry_node_t *) target->third_party.items.first;

    while (third_party_node)
    {
        if (third_party_node->should_be_included_in_pack)
        {
            form_third_party_node_path_in_pack (third_party_node, &path_container);
            kan_resource_index_add_third_party_entry (&resource_index, third_party_node->name, path_container.path,
                                                      third_party_node->size);
        }

        third_party_node = (struct third_party_entry_node_t *) third_party_node->node.list_node.next;
    }

    form_temporary_index_path (target, &path_container);
    const kan_bool_t saved =
        save_native_data (&resource_index, path_container.path, kan_string_intern ("kan_resource_index_t"),
                          target->interned_string_registry, KAN_FALSE);
    kan_resource_index_shutdown (&resource_index);

    if (!saved)
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to save resource index.", target->name)
        kan_atomic_int_add (&global.errors_count, 1);
        return;
    }

    if (global.project.use_string_interning)
    {
        KAN_ASSERT (KAN_HANDLE_IS_VALID (target->interned_string_registry))
        KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "[Target \"%s\"] Saving string registry...",
                 target->name)

        form_temporary_string_registry_path (target, &path_container);
        struct kan_stream_t *stream = kan_virtual_file_stream_open_for_write (global.volume, path_container.path);

        if (!stream)
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder,
                                 KAN_LOG_ERROR, "[Target \"%s\"] Failed to open \"%s\" for write.", target->name,
                                 path_container.path)
            kan_atomic_int_add (&global.errors_count, 1);
            return;
        }

        stream = kan_random_access_stream_buffer_open_for_write (stream, KAN_RESOURCE_BUILDER_IO_BUFFER);
        kan_serialization_interned_string_registry_writer_t writer =
            kan_serialization_interned_string_registry_writer_create (stream, target->interned_string_registry);

        enum kan_serialization_state_t serialization_state;
        while ((serialization_state = kan_serialization_interned_string_registry_writer_step (writer)) ==
               KAN_SERIALIZATION_IN_PROGRESS)
        {
        }

        kan_serialization_interned_string_registry_writer_destroy (writer);
        stream->operations->close (stream);

        if (serialization_state == KAN_SERIALIZATION_FAILED)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to save string registry.", target->name)
            kan_atomic_int_add (&global.errors_count, 1);
            return;
        }

        KAN_ASSERT (serialization_state == KAN_SERIALIZATION_FINISHED)
    }

    KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "[Target \"%s\"] Packing...", target->name)

    kan_file_system_path_container_copy_string (&path_container, VFS_OUTPUT_DIRECTORY);
    kan_file_system_path_container_append (&path_container, target->name);
    kan_file_system_path_container_add_suffix (&path_container, ".pack");
    struct kan_stream_t *stream = kan_virtual_file_stream_open_for_write (global.volume, path_container.path);

    if (!stream)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder,
                             KAN_LOG_ERROR, "[Target \"%s\"] Failed to open \"%s\" for write.", target->name,
                             path_container.path)
        kan_atomic_int_add (&global.errors_count, 1);
        return;
    }

    stream = kan_random_access_stream_buffer_open_for_write (stream, KAN_RESOURCE_BUILDER_IO_BUFFER);
    kan_virtual_file_system_read_only_pack_builder_t builder = kan_virtual_file_system_read_only_pack_builder_create ();
    kan_virtual_file_system_read_only_pack_builder_begin (builder, stream);

    if (global.project.use_string_interning)
    {
        form_temporary_string_registry_path (target, &path_container);
        if (!add_to_pack (builder, path_container.path, KAN_RESOURCE_INDEX_ACCOMPANYING_STRING_REGISTRY_DEFAULT_NAME))
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder,
                                 KAN_LOG_ERROR, "[Target \"%s\"] Failed to add \"%s\" to pack at \"%s\".", target->name,
                                 path_container.path, KAN_RESOURCE_INDEX_ACCOMPANYING_STRING_REGISTRY_DEFAULT_NAME)
            kan_atomic_int_add (&global.errors_count, 1);
            stream->operations->close (stream);
            return;
        }
    }

    form_temporary_index_path (target, &path_container);
    if (!add_to_pack (builder, path_container.path, KAN_RESOURCE_INDEX_DEFAULT_NAME))
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder,
                             KAN_LOG_ERROR, "[Target \"%s\"] Failed to add \"%s\" to pack at \"%s\".", target->name,
                             path_container.path, KAN_RESOURCE_INDEX_DEFAULT_NAME)
        kan_atomic_int_add (&global.errors_count, 1);
        stream->operations->close (stream);
        return;
    }

    native_node = (struct native_entry_node_t *) target->native.items.first;
    while (native_node)
    {
        if (native_node->should_be_included_in_pack)
        {
            form_native_node_path_in_pack (native_node, &path_container);
            if (!add_to_pack (builder, native_node->compiled_path, path_container.path))
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder,
                                     KAN_LOG_ERROR, "[Target \"%s\"] Failed to add \"%s\" to pack at \"%s\".",
                                     target->name, native_node->compiled_path, path_container.path)
                kan_atomic_int_add (&global.errors_count, 1);
                stream->operations->close (stream);
                return;
            }
        }

        native_node = (struct native_entry_node_t *) native_node->node.list_node.next;
    }

    third_party_node = (struct third_party_entry_node_t *) target->third_party.items.first;
    while (third_party_node)
    {
        if (third_party_node->should_be_included_in_pack)
        {
            form_third_party_node_path_in_pack (third_party_node, &path_container);
            if (!add_to_pack (builder, third_party_node->path, path_container.path))
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_builder,
                                     KAN_LOG_ERROR, "[Target \"%s\"] Failed to add \"%s\" to pack at \"%s\".",
                                     target->name, third_party_node->path, path_container.path)
                kan_atomic_int_add (&global.errors_count, 1);
                stream->operations->close (stream);
                return;
            }
        }

        third_party_node = (struct third_party_entry_node_t *) third_party_node->node.list_node.next;
    }

    if (!kan_virtual_file_system_read_only_pack_builder_finalize (builder))
    {
        kan_virtual_file_system_read_only_pack_builder_destroy (builder);
        KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO,
                 "[Target \"%s\"] Failed to finalize building pack.", target->name)
        kan_atomic_int_add (&global.errors_count, 1);
        stream->operations->close (stream);
        return;
    }

    stream->operations->close (stream);
    KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "[Target \"%s\"] Done packing.", target->name)
}

KAN_REFLECTION_EXPECT_UNIT_REGISTRAR_LOCAL (application_framework_resource_builder);

int main (int argument_count, char **argument_values)
{
    if (argument_count < 3)
    {
        KAN_LOG (
            application_framework_resource_builder, KAN_LOG_ERROR,
            "Incorrect number of arguments. Expected arguments: <path_to_resource_project_file> targets_to_build...")
        return ERROR_CODE_INCORRECT_ARGUMENTS;
    }

    reference_type_info_storage_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "reference_type_info_storage");
    platform_configuration_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "platform_configuration");
    targets_allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "targets");
    nodes_allocation_group = kan_allocation_group_get_child (targets_allocation_group, "nodes");
    loaded_native_entries_allocation_group = kan_allocation_group_get_child (nodes_allocation_group, "loaded_native");
    loaded_third_party_entries_allocation_group =
        kan_allocation_group_get_child (nodes_allocation_group, "loaded_third_party");
    compilation_state_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "compilation_state");
    temporary_allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "temporary");

    interned_kan_resource_resource_type_meta_t = kan_string_intern ("kan_resource_resource_type_meta_t");
    interned_kan_resource_compilable_meta_t = kan_string_intern ("kan_resource_compilable_meta_t");
    interned_kan_resource_byproduct_type_meta_t = kan_string_intern ("kan_resource_byproduct_type_meta_t");
    interned_kan_resource_platform_configuration_t = kan_string_intern ("kan_resource_platform_configuration_t");
    interned_kan_resource_target_byproduct_state_t = kan_string_intern ("kan_resource_target_byproduct_state_t");

    KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "Reading project...")
    kan_application_resource_project_init (&global.project);
    int result = 0;

    if (!kan_application_resource_project_read (argument_values[1u], &global.project))
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Failed to read project from \"%s\".",
                 argument_values[1u])
        result = ERROR_CODE_FAILED_TO_READ_PROJECT;
    }

    kan_bool_t locked_directory = KAN_FALSE;
    if (result == 0)
    {
        if (kan_file_system_lock_file_create (global.project.output_directory, KAN_TRUE))
        {
            locked_directory = KAN_TRUE;
        }
        else
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Failed to lock output directory \"%s\".",
                     global.project.output_directory)
            result = ERROR_CODE_FAILED_TO_LOCK_DIRECTORY;
        }
    }

    global.volume = kan_virtual_file_system_volume_create ();
    if (result == 0)
    {
        KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "Preparing virtual file system...")
        if (!kan_virtual_file_system_volume_mount_real (global.volume, VFS_OUTPUT_DIRECTORY,
                                                        global.project.output_directory))
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Unable to mount output directory \"%s\".",
                     global.project.output_directory)
            result = ERROR_CODE_VFS_SETUP_FAILURE;
        }

        if (!kan_virtual_file_system_volume_mount_real (global.volume, VFS_RAW_REFERENCE_CACHE_DIRECTORY,
                                                        global.project.reference_cache_directory))
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                     "Unable to mount reference cache directory \"%s\".", global.project.reference_cache_directory)
            result = ERROR_CODE_VFS_SETUP_FAILURE;
        }

        kan_virtual_file_system_make_directory (global.volume, VFS_TARGETS_DIRECTORY);
        kan_virtual_file_system_make_directory (global.volume,
                                                VFS_OUTPUT_DIRECTORY "/" SUB_DIRECTORY_RAW_BYPRODUCT_CACHE);
        kan_virtual_file_system_make_directory (global.volume, VFS_OUTPUT_DIRECTORY "/" SUB_DIRECTORY_COMPILED_CACHE);
        kan_virtual_file_system_make_directory (global.volume,
                                                VFS_OUTPUT_DIRECTORY "/" SUB_DIRECTORY_COMPILED_REFERENCE_CACHE);
        kan_virtual_file_system_make_directory (global.volume, VFS_OUTPUT_DIRECTORY "/" SUB_DIRECTORY_TEMPORARY);
    }

    if (result == 0)
    {
        kan_context_t context = kan_application_create_resource_tool_context (&global.project, argument_values[0u]);
        kan_stack_group_allocator_init (&global.temporary_allocator, temporary_allocation_group,
                                        KAN_RESOURCE_BUILDER_TEMPORARY_STACK);

        global.compilation_queue_lock = kan_atomic_int_init (0);
        global.compilation_resource_management_lock = kan_atomic_int_init (0);
        global.errors_count = kan_atomic_int_init (0);

        kan_context_system_t plugin_system = kan_context_query (context, KAN_CONTEXT_PLUGIN_SYSTEM_NAME);
        KAN_ASSERT (KAN_HANDLE_IS_VALID (plugin_system))
        global.newest_loaded_plugin_last_modification_file_time_ns =
            kan_plugin_system_get_newest_loaded_plugin_last_modification_file_time_ns (plugin_system);

        kan_context_system_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
        KAN_ASSERT (KAN_HANDLE_IS_VALID (reflection_system))
        global.registry = kan_reflection_system_get_registry (reflection_system);

        // Not the best way to inject reflection, but totally safe in tools context as there is no hot reload here.
        KAN_REFLECTION_UNIT_REGISTRAR_NAME (application_framework_resource_builder) (global.registry);

        global.binary_script_storage = kan_serialization_binary_script_storage_create (global.registry);
        kan_resource_reference_type_info_storage_build (&global.reference_type_info_storage, global.registry,
                                                        reference_type_info_storage_allocation_group);

        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "Loading platform configuration...")
            struct kan_file_system_path_container_t configuration_path;
            kan_file_system_path_container_copy_string (&configuration_path, global.project.platform_configuration);
            load_platform_configuration (&configuration_path);

            if (kan_atomic_int_get (&global.errors_count) > 0)
            {
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                         "Failed to load platform configuration.")
                result = ERROR_CODE_FAILED_TO_LOAD_PLATFORM_CONFIGURATION;
            }
        }

        KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "Setting up target structure...")
        kan_dynamic_array_init (&global.targets, global.project.targets.size, sizeof (struct target_t),
                                _Alignof (struct target_t), targets_allocation_group);

        if (result == 0)
        {
            for (int argument_index = 2; argument_index < argument_count; ++argument_index)
            {
                kan_bool_t found = KAN_FALSE;
                for (kan_loop_size_t target_index = 0u; target_index < global.project.targets.size; ++target_index)
                {
                    struct kan_application_resource_target_t *project_target =
                        &((struct kan_application_resource_target_t *) global.project.targets.data)[target_index];

                    if (strcmp (project_target->name, argument_values[argument_index]) == 0)
                    {
                        found = KAN_TRUE;
                        break;
                    }
                }

                if (!found)
                {
                    KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                             "Unable to find requested target \"%s\".", argument_values[argument_index])
                    kan_atomic_int_add (&global.errors_count, 1);
                }
            }

            for (kan_loop_size_t target_index = 0u; target_index < global.project.targets.size; ++target_index)
            {
                struct kan_application_resource_target_t *project_target =
                    &((struct kan_application_resource_target_t *) global.project.targets.data)[target_index];

                struct target_t *build_target = kan_dynamic_array_add_last (&global.targets);
                KAN_ASSERT (build_target)
                target_init (build_target);

                for (int argument_index = 2; argument_index < argument_count; ++argument_index)
                {
                    if (strcmp (project_target->name, argument_values[argument_index]) == 0)
                    {
                        build_target->requested_for_build = KAN_TRUE;
                        break;
                    }
                }

                build_target->name = project_target->name;
                build_target->source = project_target;

                struct kan_file_system_path_container_t target_directory;
                kan_file_system_path_container_copy_string (&target_directory, VFS_TARGETS_DIRECTORY);
                kan_file_system_path_container_append (&target_directory, project_target->name);
                kan_virtual_file_system_make_directory (global.volume, target_directory.path);

                for (kan_loop_size_t directory_index = 0u; directory_index < project_target->directories.size;
                     ++directory_index)
                {
                    const char *directory = ((const char **) project_target->directories.data)[directory_index];
                    const kan_instance_size_t target_length = target_directory.length;
                    char index_string[32u];
                    snprintf (index_string, 32u, "%lu", (unsigned long) directory_index);
                    kan_file_system_path_container_append (&target_directory, index_string);

                    if (!kan_virtual_file_system_volume_mount_real (global.volume, target_directory.path, directory))
                    {
                        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u,
                                             application_framework_resource_builder, KAN_LOG_ERROR,
                                             "Unable to mount \"%s\" at \"%s\".", directory, target_directory.path)
                        kan_atomic_int_add (&global.errors_count, 1);
                    }

                    kan_file_system_path_container_reset_length (&target_directory, target_length);
                }

                if (global.project.use_string_interning)
                {
                    build_target->interned_string_registry = kan_serialization_interned_string_registry_create_empty ();
                }
            }

            for (kan_loop_size_t target_index = 0u; target_index < global.project.targets.size; ++target_index)
            {
                target_collect_references (target_index, target_index);
            }

            if (kan_atomic_int_get (&global.errors_count) > 0)
            {
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                         "Failed to setup targets due to errors above.")
                result = ERROR_CODE_FAILED_TO_SETUP_TARGETS;
            }
        }

        if (result == 0)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "Scanning targets for resources...")
            struct kan_cpu_task_list_node_t *task_list = NULL;
            const kan_interned_string_t task_name = kan_string_intern ("scan_target_for_resources");

            for (kan_loop_size_t target_index = 0u; target_index < global.targets.size; ++target_index)
            {
                struct target_t *target = &((struct target_t *) global.targets.data)[target_index];
                if (target->requested_for_build)
                {
                    KAN_CPU_TASK_LIST_USER_VALUE (&task_list, &global.temporary_allocator, task_name,
                                                  scan_target_for_resources, target)
                }
            }

            if (task_list)
            {
                kan_cpu_job_t job = kan_cpu_job_create ();
                kan_cpu_job_dispatch_and_detach_task_list (job, task_list);
                kan_cpu_job_release (job);
                kan_cpu_job_wait (job);
            }

            if (kan_atomic_int_get (&global.errors_count) > 0)
            {
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Failed to scan targets for resources.")
                result = ERROR_CODE_FAILED_TO_SCAN_TARGETS;
            }

            kan_stack_group_allocator_reset (&global.temporary_allocator);
        }

        if (result == 0)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "Checking resources for collisions...")
            struct kan_cpu_task_list_node_t *task_list = NULL;
            const kan_interned_string_t task_name = kan_string_intern ("check_for_name_collisions");

            for (kan_loop_size_t target_index = 0u; target_index < global.targets.size; ++target_index)
            {
                struct target_t *target = &((struct target_t *) global.targets.data)[target_index];
                struct native_entry_node_t *native_node = (struct native_entry_node_t *) target->native.items.first;

                while (native_node)
                {
                    KAN_CPU_TASK_LIST_USER_VALUE (&task_list, &global.temporary_allocator, task_name,
                                                  scan_native_for_name_collisions, native_node)
                    native_node = (struct native_entry_node_t *) native_node->node.list_node.next;
                }

                struct third_party_entry_node_t *third_party_node =
                    (struct third_party_entry_node_t *) target->third_party.items.first;

                while (third_party_node)
                {
                    KAN_CPU_TASK_LIST_USER_VALUE (&task_list, &global.temporary_allocator, task_name,
                                                  scan_third_party_for_name_collisions, third_party_node)
                    third_party_node = (struct third_party_entry_node_t *) third_party_node->node.list_node.next;
                }
            }

            if (task_list)
            {
                kan_cpu_job_t job = kan_cpu_job_create ();
                kan_cpu_job_dispatch_and_detach_task_list (job, task_list);
                kan_cpu_job_release (job);
                kan_cpu_job_wait (job);
            }

            if (kan_atomic_int_get (&global.errors_count) > 0)
            {
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                         "Failed to ensure absence of collisions.")
                result = ERROR_CODE_FAILED_TO_SCAN_TARGETS;
            }

            kan_stack_group_allocator_reset (&global.temporary_allocator);
        }

        if (result == 0)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "Compiling resources...")

            for (kan_loop_size_t target_index = 0u; target_index < global.targets.size; ++target_index)
            {
                struct target_t *target = &((struct target_t *) global.targets.data)[target_index];
                struct native_entry_node_t *native_node = (struct native_entry_node_t *) target->native.items.first;

                while (native_node)
                {
                    // Start by compiling nodes that are already scheduled for packing as roots.
                    if (native_node->should_be_included_in_pack)
                    {
                        schedule_new_compilation (native_node);
                    }

                    native_node = (struct native_entry_node_t *) native_node->node.list_node.next;
                }
            }

            compilation_loop ();
            if (kan_atomic_int_get (&global.errors_count) > 0)
            {
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Failed to compile resources.")
                result = ERROR_CODE_FAILED_TO_COMPILE_RESOURCES;
            }

            kan_stack_group_allocator_reset (&global.temporary_allocator);
        }

        if (result == 0)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "Saving targets byproduct states...")
            struct kan_cpu_task_list_node_t *task_list = NULL;
            const kan_interned_string_t task_name = kan_string_intern ("save_target_byproduct_state");

            for (kan_loop_size_t target_index = 0u; target_index < global.targets.size; ++target_index)
            {
                struct target_t *target = &((struct target_t *) global.targets.data)[target_index];
                if (target->requested_for_build)
                {
                    KAN_CPU_TASK_LIST_USER_VALUE (&task_list, &global.temporary_allocator, task_name,
                                                  save_target_byproduct_state, target)
                }
            }

            if (task_list)
            {
                kan_cpu_job_t job = kan_cpu_job_create ();
                kan_cpu_job_dispatch_and_detach_task_list (job, task_list);
                kan_cpu_job_release (job);
                kan_cpu_job_wait (job);
            }

            if (kan_atomic_int_get (&global.errors_count) > 0)
            {
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR,
                         "Failed to save targets byproducts states.")
                result = ERROR_CODE_FAILED_TO_SCAN_TARGETS;
            }

            kan_stack_group_allocator_reset (&global.temporary_allocator);
        }

        if (result == 0 && global.project.use_string_interning)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "Interning strings...")
            struct kan_cpu_task_list_node_t *task_list = NULL;
            const kan_interned_string_t task_name = kan_string_intern ("intern_strings");

            for (kan_loop_size_t target_index = 0u; target_index < global.targets.size; ++target_index)
            {
                struct target_t *target = &((struct target_t *) global.targets.data)[target_index];
                struct native_entry_node_t *native_node = (struct native_entry_node_t *) target->native.items.first;
                while (native_node)
                {
                    if (native_node->should_be_included_in_pack)
                    {
                        KAN_CPU_TASK_LIST_USER_VALUE (&task_list, &global.temporary_allocator, task_name,
                                                      intern_strings_in_native, native_node)
                    }

                    native_node = (struct native_entry_node_t *) native_node->node.list_node.next;
                }
            }

            if (task_list)
            {
                kan_cpu_job_t job = kan_cpu_job_create ();
                kan_cpu_job_dispatch_and_detach_task_list (job, task_list);
                kan_cpu_job_release (job);
                kan_cpu_job_wait (job);
            }

            if (kan_atomic_int_get (&global.errors_count) > 0)
            {
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Failed to perform string interning.")
                result = ERROR_CODE_FAILED_TO_INTERN_STRING;
            }

            kan_stack_group_allocator_reset (&global.temporary_allocator);
        }

        if (result == 0)
        {
            KAN_LOG (application_framework_resource_builder, KAN_LOG_INFO, "Building packs...")
            struct kan_cpu_task_list_node_t *task_list = NULL;
            const kan_interned_string_t task_name = kan_string_intern ("pack_target");

            for (kan_loop_size_t target_index = 0u; target_index < global.targets.size; ++target_index)
            {
                struct target_t *target = &((struct target_t *) global.targets.data)[target_index];
                if (target->requested_for_build)
                {
                    KAN_CPU_TASK_LIST_USER_VALUE (&task_list, &global.temporary_allocator, task_name, pack_target,
                                                  target)
                }
            }

            if (task_list)
            {
                kan_cpu_job_t job = kan_cpu_job_create ();
                kan_cpu_job_dispatch_and_detach_task_list (job, task_list);
                kan_cpu_job_release (job);
                kan_cpu_job_wait (job);
            }

            if (kan_atomic_int_get (&global.errors_count) > 0)
            {
                KAN_LOG (application_framework_resource_builder, KAN_LOG_ERROR, "Failed to pack targets.")
                result = ERROR_CODE_FAILED_TO_PACK_TARGETS;
            }

            kan_stack_group_allocator_reset (&global.temporary_allocator);
        }

        struct platform_configuration_t *platform_configuration = global.platform_configurations;
        global.platform_configurations = NULL;

        while (platform_configuration)
        {
            struct platform_configuration_t *next = platform_configuration->next;
            if (platform_configuration->type->shutdown)
            {
                platform_configuration->type->shutdown (platform_configuration->type->functor_user_data,
                                                        platform_configuration->data);
            }

            kan_free_general (platform_configuration_allocation_group, platform_configuration->data,
                              platform_configuration->type->size);
            kan_free_batched (platform_configuration_allocation_group, platform_configuration);
            platform_configuration = next;
        }

        for (kan_loop_size_t target_index = 0u; target_index < global.project.targets.size; ++target_index)
        {
            target_shutdown (&((struct target_t *) global.targets.data)[target_index]);
        }

        kan_dynamic_array_shutdown (&global.targets);
        kan_stack_group_allocator_shutdown (&global.temporary_allocator);
        kan_serialization_binary_script_storage_destroy (global.binary_script_storage);
        kan_resource_reference_type_info_storage_shutdown (&global.reference_type_info_storage);
        kan_context_destroy (context);
    }

    kan_virtual_file_system_remove_directory_with_content (global.volume,
                                                           VFS_OUTPUT_DIRECTORY "/" SUB_DIRECTORY_TEMPORARY);
    kan_virtual_file_system_volume_destroy (global.volume);

    if (locked_directory)
    {
        kan_file_system_lock_file_destroy (global.project.output_directory);
    }

    kan_application_resource_project_shutdown (&global.project);
    return result;
}
