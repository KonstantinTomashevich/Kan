#include <string.h>

#include <kan/application_framework_tool/application_framework_tool.h>
#include <kan/container/hash_storage.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/context/context.h>
#include <kan/context/reflection_system.h>
#include <kan/cpu_dispatch/job.h>
#include <kan/error/critical.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/path_container.h>
#include <kan/file_system/stream.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/resource_index/resource_index.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>
#include <kan/threading/atomic.h>
#include <kan/virtual_file_system/virtual_file_system.h>

KAN_LOG_DEFINE_CATEGORY (packer);

#define EXIT_CODE_INVALID_ARGUMENTS -1
#define EXIT_CODE_UNABLE_TO_REQUEST_SYSTEMS -2
#define EXIT_CODE_INPUT_LIST_FAILURE -3
#define EXIT_CODE_STRING_INTERNING_FAILURE -4
#define EXIT_CODE_RESOURCE_INDEX_FAILURE -4
#define EXIT_CODE_STRING_REGISTRY_FAILURE -5
#define EXIT_CODE_PACK_BUILDING_FAILURE -6

#define PACKER_TEMPORARY_DIRECTORY_NAME "application_framework_packer_temporary"

static kan_allocation_group_t packer_allocation_group;
static kan_allocation_group_t resources_allocation_group;
static kan_allocation_group_t interning_allocation_group;
static struct kan_hash_storage_t resources_storage;
static kan_serialization_interned_string_registry_t accompanying_string_registry =
    KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY;

static char path_in_pack_buffer[KAN_PACKER_PACK_PATH_MAX];

struct
{
    char *input_list_path;
    char *output_pack_path;
    kan_bool_t intern_strings;
} parsed_arguments;

struct
{
    kan_serialization_binary_script_storage_t script_storage;
    kan_serialization_interned_string_registry_t string_registry;
    kan_reflection_registry_t reflection_registry;

    struct kan_atomic_int_t tasks_processed;
    uint64_t tasks_total;
    struct kan_atomic_int_t successful;
} interning_shared_context;

struct resource_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
    kan_interned_string_t type_name;
    enum kan_resource_index_native_item_format_t native_format;
    uint64_t third_party_size;
    char *source_path;
};

static inline void init_statics (void)
{
    packer_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "application_framework_packer");
    resources_allocation_group = kan_allocation_group_get_child (packer_allocation_group, "resources");
    interning_allocation_group = kan_allocation_group_get_child (packer_allocation_group, "interning");
    kan_hash_storage_init (&resources_storage, resources_allocation_group, KAN_PACKER_RESOURCE_STORAGE_BUCKETS);

    kan_file_system_remove_directory_with_content (PACKER_TEMPORARY_DIRECTORY_NAME);
    kan_file_system_make_directory (PACKER_TEMPORARY_DIRECTORY_NAME);
}

static inline void shutdown_statics (void)
{
    struct resource_node_t *node = (struct resource_node_t *) resources_storage.items.first;
    while (node)
    {
        struct resource_node_t *next = (struct resource_node_t *) node->node.list_node.next;
        kan_free_general (resources_allocation_group, node->source_path, strlen (node->source_path) + 1u);
        kan_free_batched (resources_allocation_group, node);
        node = next;
    }

    kan_hash_storage_shutdown (&resources_storage);
    if (accompanying_string_registry != KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY)
    {
        kan_serialization_interned_string_registry_destroy (accompanying_string_registry);
    }

    kan_file_system_remove_directory_with_content (PACKER_TEMPORARY_DIRECTORY_NAME);
}

static inline const char *fill_path_in_pack_buffer (struct resource_node_t *node)
{
    if (node->type_name)
    {
        const char *extension = "extension_error";
        switch (node->native_format)
        {
        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY:
            extension = "bin";
            break;

        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA:
            extension = "rd";
            break;
        }

        snprintf (path_in_pack_buffer, KAN_PACKER_PACK_PATH_MAX, "%s/%s.%s", node->type_name, node->name, extension);
    }
    else
    {
        snprintf (path_in_pack_buffer, KAN_PACKER_PACK_PATH_MAX, "third_party/%s", node->name);
    }

    return path_in_pack_buffer;
}

static inline kan_bool_t register_resource (char *source_path, kan_serialization_interned_string_registry_t registry)
{
    struct kan_resource_index_info_from_path_t info_from_path;
    kan_resource_index_extract_info_from_path (source_path, &info_from_path);
    kan_interned_string_t type_name = NULL;
    uint64_t third_party_size = 0u;

    if (info_from_path.native)
    {
        struct kan_stream_t *stream = kan_direct_file_stream_open_for_read (source_path, KAN_TRUE);
        if (!stream)
        {
            KAN_LOG (packer, KAN_LOG_ERROR, "Failed to open input file \"%s\" to scan its type.", source_path)
            return KAN_FALSE;
        }

        stream = kan_random_access_stream_buffer_open_for_read (stream, KAN_PACKER_IO_BUFFER_SIZE);
        switch (info_from_path.native_format)
        {
        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY:
            if (!kan_serialization_binary_read_type_header (stream, &type_name,
                                                            KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY))
            {
                KAN_LOG (packer, KAN_LOG_ERROR, "Failed to read type header from \"%s\".", source_path)
                stream->operations->close (stream);
                return KAN_FALSE;
            }

            break;

        case KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA:
            if (!kan_serialization_rd_read_type_header (stream, &type_name))
            {
                KAN_LOG (packer, KAN_LOG_ERROR, "Failed to read type header from \"%s\".", source_path)
                stream->operations->close (stream);
                return KAN_FALSE;
            }

            break;
        }

        stream->operations->close (stream);
        if (!kan_reflection_registry_query_struct (registry, type_name))
        {
            KAN_LOG (packer, KAN_LOG_ERROR, "Failed to find type for type name \"%s\" (from resource \"%s\").",
                     type_name, source_path)
            return KAN_FALSE;
        }
    }
    else
    {
        struct kan_file_system_entry_status_t status;
        if (!kan_file_system_query_entry (source_path, &status))
        {
            KAN_LOG (packer, KAN_LOG_ERROR, "Failed to query size of \"%s\".", source_path)
            return KAN_FALSE;
        }

        third_party_size = status.size;
    }

    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&resources_storage, (uint64_t) info_from_path.name);
    struct resource_node_t *node = (struct resource_node_t *) bucket->first;
    const struct resource_node_t *node_end = (struct resource_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == info_from_path.name && node->type_name == type_name)
        {
            KAN_LOG (packer, KAN_LOG_ERROR,
                     "Found resource collision. Resource with name \"%s\" and type \"%s\" already exists at \"%s\", "
                     "but was also added from \"%s\".",
                     info_from_path.name, type_name, node->source_path, source_path)
            return KAN_FALSE;
        }

        node = (struct resource_node_t *) node->node.list_node.next;
    }

    node = kan_allocate_batched (resources_allocation_group, sizeof (struct resource_node_t));
    node->node.hash = (uint64_t) info_from_path.name;
    node->name = info_from_path.name;
    node->type_name = type_name;
    node->native_format = info_from_path.native_format;
    node->third_party_size = third_party_size;

    const uint64_t path_length = strlen (source_path);
    node->source_path = kan_allocate_general (resources_allocation_group, path_length + 1u, _Alignof (char));
    memcpy (node->source_path, source_path, path_length + 1u);

    if (resources_storage.items.size >= resources_storage.bucket_count * KAN_PACKER_RESOURCE_STORAGE_LOAD_FACTOR)
    {
        kan_hash_storage_set_bucket_count (&resources_storage, resources_storage.bucket_count * 2u);
    }

    kan_hash_storage_add (&resources_storage, &node->node);
    return KAN_TRUE;
}

static inline int read_input_list (kan_serialization_interned_string_registry_t registry)
{
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_read (parsed_arguments.input_list_path, KAN_TRUE);
    if (!stream)
    {
        KAN_LOG (packer, KAN_LOG_ERROR, "Failed to open input list file.")
        return EXIT_CODE_INPUT_LIST_FAILURE;
    }

    stream = kan_random_access_stream_buffer_open_for_read (stream, KAN_PACKER_IO_BUFFER_SIZE);
    char path_buffer[KAN_FILE_SYSTEM_MAX_PATH_LENGTH];
    kan_bool_t everything_registered = KAN_TRUE;

    while (KAN_TRUE)
    {
        char *path_end = path_buffer;
        while (KAN_TRUE)
        {
            if (path_end >= path_buffer + KAN_FILE_SYSTEM_MAX_PATH_LENGTH - 1u)
            {
                KAN_LOG (packer, KAN_LOG_ERROR, "Failed to parse input list: encountered line longer that max path.")
                stream->operations->close (stream);
                return EXIT_CODE_INPUT_LIST_FAILURE;
            }

            if (stream->operations->read (stream, 1u, path_end) != 1u)
            {
                // We should've encountered end of file.
                *path_end = '\0';
                break;
            }

            if (*path_end == '\r')
            {
                // Ignore carriage returns: it is a first symbol of windows line ending.
                break;
            }

            if (*path_end == '\n')
            {
                // We've encountered end of line.
                *path_end = '\0';
                break;
            }

            ++path_end;
        }

        if (path_end == path_buffer)
        {
            // Encountered end of list.
            break;
        }

        if (!register_resource (path_buffer, registry))
        {
            everything_registered = KAN_FALSE;
        }
    }

    stream->operations->close (stream);
    return everything_registered ? 0 : EXIT_CODE_INPUT_LIST_FAILURE;
}

static inline void intern_strings_task_exit_successfully (struct resource_node_t *node, char *new_path)
{
    int index = kan_atomic_int_add (&interning_shared_context.tasks_processed, 1);
    KAN_LOG (packer, KAN_LOG_INFO, "[%llu/%llu] Successful. %s", (unsigned long long) (index + 1),
             (unsigned long long) (interning_shared_context.tasks_total), node->source_path)

    kan_free_general (resources_allocation_group, node->source_path, strlen (node->source_path) + 1u);
    const uint64_t path_length = strlen (new_path);
    node->source_path = kan_allocate_general (resources_allocation_group, path_length + 1u, _Alignof (char));
    memcpy (node->source_path, new_path, path_length + 1u);
}

static inline void intern_strings_task_exit_with_error (struct resource_node_t *node, const char *error)
{
    int index = kan_atomic_int_add (&interning_shared_context.tasks_processed, 1);
    KAN_LOG (packer, KAN_LOG_INFO, "[%llu/%llu] %s. %s", (unsigned long long) (index + 1),
             (unsigned long long) (interning_shared_context.tasks_total), error, node->source_path)
    kan_atomic_int_set (&interning_shared_context.successful, 0);
}

static inline void free_instance (const struct kan_reflection_struct_t *type, void *instance)
{
    if (type->shutdown)
    {
        kan_allocation_group_stack_push (interning_allocation_group);
        type->shutdown (type->functor_user_data, instance);
        kan_allocation_group_stack_pop ();
    }

    kan_free_general (interning_allocation_group, instance, type->size);
}

static void intern_strings_task (uint64_t user_data)
{
    struct resource_node_t *node = (struct resource_node_t *) user_data;
    KAN_ASSERT (node->type_name)
    KAN_ASSERT (node->native_format == KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY)

    const struct kan_reflection_struct_t *type =
        kan_reflection_registry_query_struct (interning_shared_context.reflection_registry, node->type_name);
    // Should've been checked earlier.
    KAN_ASSERT (type)

    struct kan_stream_t *stream = kan_direct_file_stream_open_for_read (node->source_path, KAN_TRUE);
    kan_interned_string_t read_type_name;

    if (!stream)
    {
        intern_strings_task_exit_with_error (node, "Failed to open source file.");
        return;
    }

    stream = kan_random_access_stream_buffer_open_for_read (stream, KAN_PACKER_IO_BUFFER_SIZE);
    if (!kan_serialization_binary_read_type_header (stream, &read_type_name,
                                                    KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY))
    {
        stream->operations->close (stream);
        intern_strings_task_exit_with_error (node, "Failed to read type header.");
        return;
    }

    void *instance_data = kan_allocate_general (interning_allocation_group, type->size, type->alignment);
    if (type->init)
    {
        kan_allocation_group_stack_push (interning_allocation_group);
        type->init (type->functor_user_data, instance_data);
        kan_allocation_group_stack_pop ();
    }

    kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
        stream, instance_data, node->type_name, interning_shared_context.script_storage,
        KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY, interning_allocation_group);

    enum kan_serialization_state_t serialization_state;
    while ((serialization_state = kan_serialization_binary_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    kan_serialization_binary_reader_destroy (reader);
    stream->operations->close (stream);

    if (serialization_state == KAN_SERIALIZATION_FAILED)
    {
        free_instance (type, instance_data);
        intern_strings_task_exit_with_error (node, "Failed to deserialize file.");
        return;
    }

    KAN_ASSERT (serialization_state == KAN_SERIALIZATION_FINISHED)
    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_string (&path_container, PACKER_TEMPORARY_DIRECTORY_NAME);
    kan_file_system_path_container_append (&path_container, node->type_name);
    kan_file_system_make_directory (path_container.path);
    kan_file_system_path_container_append (&path_container, node->name);
    kan_file_system_path_container_add_suffix (&path_container, ".bin");

    stream = kan_direct_file_stream_open_for_write (path_container.path, KAN_TRUE);
    if (!stream)
    {
        free_instance (type, instance_data);
        intern_strings_task_exit_with_error (node, "Failed to open output file.");
        return;
    }

    stream = kan_random_access_stream_buffer_open_for_write (stream, KAN_PACKER_IO_BUFFER_SIZE);
    kan_serialization_binary_writer_t writer = kan_serialization_binary_writer_create (
        stream, instance_data, node->type_name, interning_shared_context.script_storage,
        interning_shared_context.string_registry);

    while ((serialization_state = kan_serialization_binary_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    kan_serialization_binary_writer_destroy (writer);
    stream->operations->close (stream);
    free_instance (type, instance_data);

    if (serialization_state == KAN_SERIALIZATION_FAILED)
    {
        intern_strings_task_exit_with_error (node, "Failed to serialize output file.");
        return;
    }

    KAN_ASSERT (serialization_state == KAN_SERIALIZATION_FINISHED)
    intern_strings_task_exit_successfully (node, path_container.path);
}

static inline int perform_string_interning (kan_reflection_registry_t registry)
{
    interning_shared_context.script_storage = kan_serialization_binary_script_storage_create (registry);
    interning_shared_context.string_registry = kan_serialization_interned_string_registry_create_empty ();
    interning_shared_context.reflection_registry = registry;

    interning_shared_context.tasks_processed = kan_atomic_int_init (0);
    interning_shared_context.tasks_total = 0u;
    interning_shared_context.successful = kan_atomic_int_init (1);

    KAN_LOG (packer, KAN_LOG_INFO, "Starting string interning procedure...")
    struct kan_stack_group_allocator_t temporary_allocator;
    kan_stack_group_allocator_init (&temporary_allocator, interning_allocation_group,
                                    KAN_PACKER_TEMPORARY_ALLOCATOR_STACK_SIZE);

    struct kan_cpu_task_list_node_t *first_task_node = NULL;
    struct resource_node_t *resource_node = (struct resource_node_t *) resources_storage.items.first;
    const kan_interned_string_t task_name = kan_string_intern ("intern_resource_strings");

    while (resource_node)
    {
        if (resource_node->type_name && resource_node->native_format == KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY)
        {
            KAN_CPU_TASK_LIST_USER_VALUE (&first_task_node, &temporary_allocator, task_name, intern_strings_task,
                                          FOREGROUND, resource_node)
        }

        resource_node = (struct resource_node_t *) resource_node->node.list_node.next;
    }

    if (first_task_node)
    {
        kan_cpu_job_t job = kan_cpu_job_create ();
        kan_cpu_job_dispatch_and_detach_task_list (job, first_task_node);
        kan_cpu_job_release (job);
        kan_cpu_job_wait (job);
    }

    kan_stack_group_allocator_shutdown (&temporary_allocator);
    KAN_LOG (packer, KAN_LOG_INFO, "Finished string interning procedure...")
    kan_serialization_binary_script_storage_destroy (interning_shared_context.script_storage);
    accompanying_string_registry = interning_shared_context.string_registry;
    return kan_atomic_int_get (&interning_shared_context.successful) ? 0 : EXIT_CODE_STRING_INTERNING_FAILURE;
}

static inline int build_resource_index (kan_reflection_registry_t registry)
{
    struct kan_resource_index_t resource_index;
    kan_resource_index_init (&resource_index);
    struct resource_node_t *resource_node = (struct resource_node_t *) resources_storage.items.first;

    while (resource_node)
    {
        if (resource_node->type_name)
        {
            kan_resource_index_add_native_entry (&resource_index, resource_node->type_name, resource_node->name,
                                                 resource_node->native_format,
                                                 fill_path_in_pack_buffer (resource_node));
        }
        else
        {
            kan_resource_index_add_third_party_entry (&resource_index, resource_node->name,
                                                      fill_path_in_pack_buffer (resource_node),
                                                      resource_node->third_party_size);
        }

        resource_node = (struct resource_node_t *) resource_node->node.list_node.next;
    }

    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_string (&path_container, PACKER_TEMPORARY_DIRECTORY_NAME);
    kan_file_system_path_container_append (&path_container, KAN_RESOURCE_INDEX_DEFAULT_NAME);
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (path_container.path, KAN_TRUE);

    if (!stream)
    {
        kan_resource_index_shutdown (&resource_index);
        KAN_LOG (packer, KAN_LOG_ERROR, "Failed to open output stream for resource index.")
        return EXIT_CODE_RESOURCE_INDEX_FAILURE;
    }

    stream = kan_random_access_stream_buffer_open_for_write (stream, KAN_PACKER_IO_BUFFER_SIZE);
    kan_serialization_binary_script_storage_t script_storage =
        kan_serialization_binary_script_storage_create (registry);

    kan_serialization_binary_writer_t writer =
        kan_serialization_binary_writer_create (stream, &resource_index, kan_string_intern ("kan_resource_index_t"),
                                                script_storage, accompanying_string_registry);

    enum kan_serialization_state_t serialization_state;
    while ((serialization_state = kan_serialization_binary_writer_step (writer)) != KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    kan_resource_index_shutdown (&resource_index);
    kan_serialization_binary_writer_destroy (writer);
    kan_serialization_binary_script_storage_destroy (script_storage);
    stream->operations->close (stream);

    if (serialization_state == KAN_SERIALIZATION_FAILED)
    {
        KAN_LOG (packer, KAN_LOG_ERROR, "Failed to serialize resource index.")
        return EXIT_CODE_RESOURCE_INDEX_FAILURE;
    }

    return 0;
}

static inline int save_accompanying_string_registry (void)
{
    KAN_ASSERT (accompanying_string_registry != KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY)
    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_string (&path_container, PACKER_TEMPORARY_DIRECTORY_NAME);
    kan_file_system_path_container_append (&path_container,
                                           KAN_RESOURCE_INDEX_ACCOMPANYING_STRING_REGISTRY_DEFAULT_NAME);
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (path_container.path, KAN_TRUE);

    if (!stream)
    {
        KAN_LOG (packer, KAN_LOG_ERROR, "Failed to open output stream for resource index accompanying string registry.")
        return EXIT_CODE_STRING_REGISTRY_FAILURE;
    }

    stream = kan_random_access_stream_buffer_open_for_write (stream, KAN_PACKER_IO_BUFFER_SIZE);
    kan_serialization_interned_string_registry_writer_t writer =
        kan_serialization_interned_string_registry_writer_create (stream, accompanying_string_registry);

    enum kan_serialization_state_t serialization_state;
    while ((serialization_state = kan_serialization_interned_string_registry_writer_step (writer)))
    {
    }

    kan_serialization_interned_string_registry_writer_destroy (writer);
    stream->operations->close (stream);

    if (serialization_state == KAN_SERIALIZATION_FAILED)
    {
        KAN_LOG (packer, KAN_LOG_ERROR, "Failed to serialize resource index accompanying string registry.")
        return EXIT_CODE_STRING_REGISTRY_FAILURE;
    }

    return 0;
}

static inline int pack_resources (void)
{
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (parsed_arguments.output_pack_path, KAN_TRUE);
    if (!stream)
    {
        KAN_LOG (packer, KAN_LOG_ERROR, "Failed to open pack output stream.")
        return EXIT_CODE_PACK_BUILDING_FAILURE;
    }

    stream = kan_random_access_stream_buffer_open_for_write (stream, KAN_PACKER_IO_BUFFER_SIZE);
    kan_virtual_file_system_read_only_pack_builder_t builder = kan_virtual_file_system_read_only_pack_builder_create ();

    if (!kan_virtual_file_system_read_only_pack_builder_begin (builder, stream))
    {
        stream->operations->close (stream);
        kan_virtual_file_system_read_only_pack_builder_destroy (builder);
        KAN_LOG (packer, KAN_LOG_ERROR, "Failed to begin building pack.")
        return EXIT_CODE_PACK_BUILDING_FAILURE;
    }

    struct resource_node_t *resource_node = (struct resource_node_t *) resources_storage.items.first;
    while (resource_node)
    {
        struct kan_stream_t *resource_stream =
            kan_direct_file_stream_open_for_read (resource_node->source_path, KAN_TRUE);

        if (!resource_stream)
        {
            stream->operations->close (stream);
            // Need to finalize before destruction from outside.
            kan_virtual_file_system_read_only_pack_builder_finalize (builder);
            kan_virtual_file_system_read_only_pack_builder_destroy (builder);
            KAN_LOG (packer, KAN_LOG_ERROR, "Failed to open resource stream for \"%s\".", resource_node->source_path)
            return EXIT_CODE_PACK_BUILDING_FAILURE;
        }

        resource_stream = kan_random_access_stream_buffer_open_for_read (resource_stream, KAN_PACKER_IO_BUFFER_SIZE);
        const kan_bool_t added = kan_virtual_file_system_read_only_pack_builder_add (
            builder, resource_stream, fill_path_in_pack_buffer (resource_node));
        resource_stream->operations->close (resource_stream);

        if (!added)
        {
            stream->operations->close (stream);
            kan_virtual_file_system_read_only_pack_builder_destroy (builder);
            KAN_LOG (packer, KAN_LOG_ERROR, "Failed to add resource from \"%s\" to pack.", resource_node->source_path)
            return EXIT_CODE_PACK_BUILDING_FAILURE;
        }

        resource_node = (struct resource_node_t *) resource_node->node.list_node.next;
    }

    if (!kan_virtual_file_system_read_only_pack_builder_finalize (builder))
    {
        stream->operations->close (stream);
        kan_virtual_file_system_read_only_pack_builder_destroy (builder);
        KAN_LOG (packer, KAN_LOG_ERROR, "Failed to finalize building pack.")
        return EXIT_CODE_PACK_BUILDING_FAILURE;
    }

    stream->operations->close (stream);
    kan_virtual_file_system_read_only_pack_builder_destroy (builder);
    return 0;
}

static inline int execute (kan_context_handle_t context)
{
    KAN_LOG (packer, KAN_LOG_INFO, "Input list: \"%s\".", parsed_arguments.input_list_path)
    KAN_LOG (packer, KAN_LOG_INFO, "Output pack: \"%s\".", parsed_arguments.output_pack_path)
    KAN_LOG (packer, KAN_LOG_INFO, "Interning strings: \"%s\".", parsed_arguments.intern_strings ? "yes" : "false")

    kan_context_system_handle_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    if (reflection_system == KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        KAN_LOG (packer, KAN_LOG_ERROR, "Failed to request reflection system.")
        return EXIT_CODE_UNABLE_TO_REQUEST_SYSTEMS;
    }

    kan_reflection_registry_t registry = kan_reflection_system_get_registry (reflection_system);
    init_statics ();
    int result = read_input_list (registry);

    if (result == 0 && parsed_arguments.intern_strings)
    {
        result = perform_string_interning (registry);
    }

    if (result == 0)
    {
        result = build_resource_index (registry);
    }

    if (result == 0 && parsed_arguments.intern_strings)
    {
        result = save_accompanying_string_registry ();
    }

    if (result == 0)
    {
        result = pack_resources ();
    }

    shutdown_statics ();
    return result;
}

int main (int arguments_count, char *arguments[])
{
    kan_bool_t correct_arguments = KAN_FALSE;
    parsed_arguments.intern_strings = KAN_FALSE;

    if (arguments_count == 3u)
    {
        correct_arguments = KAN_TRUE;
    }
    else if (arguments_count == 4u && strcmp (arguments[3u], "--intern-strings") == 0)
    {
        correct_arguments = KAN_TRUE;
        parsed_arguments.intern_strings = KAN_TRUE;
    }

    if (!correct_arguments)
    {
        KAN_LOG (packer, KAN_LOG_ERROR,
                 "Incorrect arguments. Expected: <path_to_input_file> <path_to_output_file> [--intern-strings]")
        return EXIT_CODE_INVALID_ARGUMENTS;
    }

    parsed_arguments.input_list_path = arguments[1u];
    parsed_arguments.output_pack_path = arguments[2u];

    kan_context_handle_t context = kan_application_framework_tool_create_context ();
    const int result = execute (context);
    kan_context_destroy (context);
    return result;
}
