#include <string.h>

#include <kan/application_framework_tool/application_framework_tool.h>
#include <kan/context/context.h>
#include <kan/context/reflection_system.h>
#include <kan/error/critical.h>
#include <kan/file_system/stream.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>

KAN_LOG_DEFINE_CATEGORY (resource_binarizer);

#define EXIT_CODE_INVALID_ARGUMENTS -1
#define EXIT_CODE_UNABLE_TO_REQUEST_SYSTEMS -2
#define EXIT_CODE_UNABLE_TO_READ_INPUT -3
#define EXIT_CODE_UNABLE_TO_WRITE_OUTPUT -4

static inline void free_instance (const struct kan_reflection_struct_t *type,
                                  kan_allocation_group_t allocation_group,
                                  void *instance)
{
    if (type->shutdown)
    {
        type->shutdown (type->functor_user_data, instance);
    }

    kan_free_general (allocation_group, instance, type->size);
}

static inline int execute (kan_context_handle_t context, const char *input_path, const char *output_path)
{
    KAN_LOG (resource_binarizer, KAN_LOG_INFO, "Given input \"%s\".", input_path)
    KAN_LOG (resource_binarizer, KAN_LOG_INFO, "Given output \"%s\".", output_path)

    kan_context_system_handle_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    if (reflection_system == KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        KAN_LOG (resource_binarizer, KAN_LOG_ERROR, "Failed to request reflection system.")
        return EXIT_CODE_UNABLE_TO_REQUEST_SYSTEMS;
    }

    kan_reflection_registry_t registry = kan_reflection_system_get_registry (reflection_system);
    struct kan_stream_t *input_stream = kan_direct_file_stream_open_for_read (input_path, KAN_TRUE);

    if (!input_stream)
    {
        KAN_LOG (resource_binarizer, KAN_LOG_ERROR, "Failed to open input file.")
        return EXIT_CODE_UNABLE_TO_READ_INPUT;
    }

    input_stream = kan_random_access_stream_buffer_open_for_read (input_stream, KAN_RESOURCE_BINARIZER_IO_BUFFER);
    kan_interned_string_t type_name;

    if (!kan_serialization_rd_read_type_header (input_stream, &type_name))
    {
        KAN_LOG (resource_binarizer, KAN_LOG_ERROR, "Failed to read type header.")
        input_stream->operations->close (input_stream);
        return EXIT_CODE_UNABLE_TO_READ_INPUT;
    }

    const struct kan_reflection_struct_t *type = kan_reflection_registry_query_struct (registry, type_name);
    if (!type)
    {
        KAN_LOG (resource_binarizer, KAN_LOG_ERROR, "Unable to find type \"%s\".", type_name)
        input_stream->operations->close (input_stream);
        return EXIT_CODE_UNABLE_TO_READ_INPUT;
    }

    const kan_allocation_group_t instance_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "resource_binarizer_instance");
    void *instance_data = kan_allocate_general (instance_allocation_group, type->size, type->alignment);

    if (type->init)
    {
        type->init (type->functor_user_data, instance_data);
    }

    kan_serialization_rd_reader_t reader = kan_serialization_rd_reader_create (input_stream, instance_data, type_name,
                                                                               registry, instance_allocation_group);

    enum kan_serialization_state_t state;
    while ((state = kan_serialization_rd_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    if (state == KAN_SERIALIZATION_FAILED)
    {
        KAN_LOG (resource_binarizer, KAN_LOG_ERROR, "Input deserialization failed.")
        kan_serialization_rd_reader_destroy (reader);
        input_stream->operations->close (input_stream);
        free_instance (type, instance_allocation_group, instance_data);
        return EXIT_CODE_UNABLE_TO_READ_INPUT;
    }

    KAN_ASSERT (state == KAN_SERIALIZATION_FINISHED)
    kan_serialization_rd_reader_destroy (reader);
    input_stream->operations->close (input_stream);
    struct kan_stream_t *output_stream = kan_direct_file_stream_open_for_write (output_path, KAN_TRUE);

    if (!output_stream)
    {
        KAN_LOG (resource_binarizer, KAN_LOG_ERROR, "Failed to open output file.")
        return EXIT_CODE_UNABLE_TO_WRITE_OUTPUT;
    }

    output_stream = kan_random_access_stream_buffer_open_for_write (output_stream, KAN_RESOURCE_BINARIZER_IO_BUFFER);
    if (!kan_serialization_binary_write_type_header (output_stream, type_name, KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY))
    {
        KAN_LOG (resource_binarizer, KAN_LOG_ERROR, "Failed to write type header.")
        output_stream->operations->close (output_stream);
        free_instance (type, instance_allocation_group, instance_data);
        return EXIT_CODE_UNABLE_TO_WRITE_OUTPUT;
    }

    kan_serialization_binary_script_storage_t storage = kan_serialization_binary_script_storage_create (registry);
    kan_serialization_binary_writer_t writer = kan_serialization_binary_writer_create (
        output_stream, instance_data, type_name, storage, KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY);

    while ((state = kan_serialization_binary_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    if (state == KAN_SERIALIZATION_FAILED)
    {
        KAN_LOG (resource_binarizer, KAN_LOG_ERROR, "Output serialization failed.")
        kan_serialization_binary_writer_destroy (writer);
        kan_serialization_binary_script_storage_destroy (storage);
        output_stream->operations->close (output_stream);
        free_instance (type, instance_allocation_group, instance_data);
        return EXIT_CODE_UNABLE_TO_WRITE_OUTPUT;
    }

    KAN_ASSERT (state == KAN_SERIALIZATION_FINISHED)
    kan_serialization_binary_writer_destroy (writer);
    kan_serialization_binary_script_storage_destroy (storage);
    output_stream->operations->close (output_stream);
    free_instance (type, instance_allocation_group, instance_data);
    return 0;
}

int main (int arguments_count, char *arguments[])
{
    if (arguments_count != 3u)
    {
        KAN_LOG (resource_binarizer, KAN_LOG_ERROR,
                 "Incorrect arguments. Expected: <path_to_input_file> <path_to_output_file>")
        return EXIT_CODE_INVALID_ARGUMENTS;
    }

    const char *input_path = arguments[1u];
    const uint64_t input_path_length = strlen (input_path);

    if (input_path_length < 3u || input_path[input_path_length - 3u] != '.' ||
        input_path[input_path_length - 2u] != 'r' || input_path[input_path_length - 1u] != 'd')
    {
        KAN_LOG (resource_binarizer, KAN_LOG_ERROR,
                 "Input path must point to file in readable data format (with .rd extension).")
        return EXIT_CODE_INVALID_ARGUMENTS;
    }

    const char *output_path = arguments[2u];
    const uint64_t output_path_length = strlen (output_path);

    if (output_path_length < 4u || output_path[output_path_length - 4u] != '.' ||
        output_path[output_path_length - 3u] != 'b' || output_path[output_path_length - 2u] != 'i' ||
        output_path[output_path_length - 1u] != 'n')
    {
        KAN_LOG (
            resource_binarizer, KAN_LOG_ERROR,
            "Output path must point to file in binary format (with .bin extension). File is not required to exist.")
        return EXIT_CODE_INVALID_ARGUMENTS;
    }

    kan_context_handle_t context = kan_application_framework_tool_create_context ();
    const int result = execute (context, input_path, output_path);
    kan_context_destroy (context);
    return result;
}
