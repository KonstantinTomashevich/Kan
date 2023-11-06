#include <string.h>

#include <kan/c_interface/file.h>
#include <kan/error/critical.h>
#include <kan/memory/allocation.h>

void kan_c_interface_file_init (struct kan_c_interface_file_t *file)
{
    file->interface = NULL;
    file->source_file_path = NULL;
    file->optional_includable_object = NULL;
}

kan_bool_t kan_c_interface_file_should_have_includable_object (struct kan_c_interface_file_t *file)
{
    uint64_t length = strlen (file->source_file_path);
    return length >= 2u && (file->source_file_path[length - 2u] != '.' || file->source_file_path[length - 1u] != 'h');
}

C_INTERFACE_API kan_bool_t kan_c_interface_file_serialize (struct kan_c_interface_file_t *file,
                                                           struct kan_stream_t *stream)
{
    KAN_ASSERT (kan_stream_is_writeable (stream))
    if (!kan_c_interface_serialize (file->interface, stream))
    {
        return KAN_FALSE;
    }

    const uint16_t path_length = (uint16_t) strlen (file->source_file_path);
    if (stream->operations->write (stream, sizeof (uint16_t), &path_length) != sizeof (uint16_t))
    {
        return KAN_FALSE;
    }

    if (stream->operations->write (stream, path_length, file->source_file_path) != path_length)
    {
        return KAN_FALSE;
    }

    if (kan_c_interface_file_should_have_includable_object (file))
    {
        KAN_ASSERT (file->optional_includable_object)
        const uint32_t object_length = strlen (file->optional_includable_object);

        if (stream->operations->write (stream, sizeof (uint32_t), &object_length) != sizeof (uint32_t))
        {
            return KAN_FALSE;
        }

        if (stream->operations->write (stream, object_length, file->optional_includable_object) != object_length)
        {
            return KAN_FALSE;
        }
    }

    return KAN_TRUE;
}

C_INTERFACE_API kan_bool_t kan_c_interface_file_deserialize (struct kan_c_interface_file_t *file,
                                                             struct kan_stream_t *stream)
{
    KAN_ASSERT (kan_stream_is_readable (stream))
    if (!(file->interface = kan_c_interface_deserialize (stream)))
    {
        return KAN_FALSE;
    }

    uint16_t path_length;
    if (stream->operations->read (stream, sizeof (uint16_t), &path_length) != sizeof (uint16_t))
    {
        return KAN_FALSE;
    }

    file->source_file_path =
        kan_allocate_general (kan_c_interface_allocation_group (), path_length + 1u, _Alignof (char));
    if (stream->operations->read (stream, path_length, file->source_file_path) != path_length)
    {
        return KAN_FALSE;
    }

    file->source_file_path[path_length] = '\0';
    if (kan_c_interface_file_should_have_includable_object (file))
    {
        uint32_t object_length;
        if (stream->operations->read (stream, sizeof (uint32_t), &object_length) != sizeof (uint32_t))
        {
            return KAN_FALSE;
        }

        file->optional_includable_object =
            kan_allocate_general (kan_c_interface_allocation_group (), object_length + 1u, _Alignof (char));

        if (stream->operations->read (stream, object_length, file->optional_includable_object) != object_length)
        {
            return KAN_FALSE;
        }

        file->optional_includable_object[object_length] = '\0';
    }

    return KAN_TRUE;
}

void kan_c_interface_file_shutdown (struct kan_c_interface_file_t *file)
{
    kan_c_interface_destroy (file->interface);
    kan_free_general (kan_c_interface_allocation_group (), file->source_file_path,
                      strlen (file->source_file_path) + 1u);

    if (file->optional_includable_object)
    {
        kan_free_general (kan_c_interface_allocation_group (), file->optional_includable_object,
                          strlen (file->optional_includable_object) + 1u);
    }
}
