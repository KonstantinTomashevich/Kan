#define _CRT_SECURE_NO_WARNINGS  __CUSHION_PRESERVE__

#include <string.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/min_max.h>
#include <kan/error/critical.h>
#include <kan/memory/allocation.h>
#include <kan/stream/random_access_stream_buffer.h>

static kan_bool_t allocation_group_ready = KAN_FALSE;
static kan_allocation_group_t allocation_group;

static kan_allocation_group_t get_allocation_group (void)
{
    if (!allocation_group_ready)
    {
        allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "random_access_stream_buffer");
        allocation_group_ready = KAN_TRUE;
    }

    return allocation_group;
}

struct random_access_stream_buffer_t
{
    struct kan_stream_t as_stream;
    struct kan_stream_t *source_stream;
    kan_file_size_t stream_position;
    kan_file_size_t stream_size;
    kan_file_size_t buffer_position;
    kan_file_size_t buffer_size;
    uint8_t buffer[];
};

static inline kan_bool_t fill_buffer (struct random_access_stream_buffer_t *data)
{
    data->buffer_position = data->stream_position;
    data->source_stream->operations->seek (data->source_stream, KAN_STREAM_SEEK_START, data->stream_position);
    const kan_file_size_t to_read_into_buffer = KAN_MIN (data->stream_size - data->stream_position, data->buffer_size);
    return data->source_stream->operations->read (data->source_stream, to_read_into_buffer, data->buffer) ==
           to_read_into_buffer;
}

static kan_file_size_t buffered_read (struct kan_stream_t *stream, kan_file_size_t amount, void *output_buffer)
{
    struct random_access_stream_buffer_t *data = (struct random_access_stream_buffer_t *) stream;
    const kan_file_size_t buffer_begin = data->buffer_position;
    const kan_file_size_t buffer_end = buffer_begin + data->buffer_size;

    kan_file_size_t request_begin = data->stream_position;
    const kan_file_size_t request_end = request_begin + amount;
    uint8_t *output = (uint8_t *) output_buffer;
    kan_file_size_t total_read = 0u;

    if (request_begin >= (kan_file_size_t) buffer_begin && request_begin < (kan_file_size_t) buffer_end)
    {
        const kan_file_size_t begin_buffer_offset = request_begin - buffer_begin;
        if (request_end < (kan_file_size_t) buffer_end)
        {
            const kan_file_size_t buffered_amount = KAN_MIN (data->stream_size, request_end) - request_begin;
            memcpy (output, data->buffer + begin_buffer_offset, buffered_amount);
            data->stream_position += buffered_amount;
            // Short exit, avoid doing unnecessary math when everything is buffered.
            return (kan_file_size_t) buffered_amount;
        }
        else
        {
            const kan_file_size_t buffered_amount = KAN_MIN (data->stream_size, buffer_end) - request_begin;
            memcpy (output, data->buffer + begin_buffer_offset, buffered_amount);
            output += buffered_amount;
            request_begin += buffered_amount;
            total_read += (kan_file_size_t) buffered_amount;
            data->stream_position += buffered_amount;
        }
    }
    else if (request_begin < buffer_begin && request_end >= buffer_begin)
    {
        const kan_file_size_t to_read_unbuffered = buffer_begin - request_begin;
        data->source_stream->operations->seek (data->source_stream, KAN_STREAM_SEEK_START, data->stream_position);

        const kan_file_size_t read =
            (kan_file_size_t) data->source_stream->operations->read (data->source_stream, to_read_unbuffered, output);
        total_read += read;
        output += read;
        data->stream_position += read;

        if (request_end <= buffer_end)
        {
            const kan_file_size_t buffered_amount = KAN_MIN (data->stream_size, request_end) - buffer_begin;
            memcpy (output, data->buffer, buffered_amount);
            total_read += buffered_amount;
            data->stream_position += total_read;
            // Short exit, avoid doing unnecessary math.
            return (kan_file_size_t) total_read;
        }
        else
        {
            const kan_file_size_t buffered_amount =
                KAN_MIN (data->stream_size - data->buffer_position, data->buffer_size);
            memcpy (output, data->buffer, buffered_amount);
            total_read += (kan_file_size_t) buffered_amount;
            output += buffered_amount;
            request_begin = buffer_begin + buffered_amount;
            data->stream_position += buffered_amount;
        }
    }

    if (request_end - request_begin > data->buffer_size)
    {
        // Very big request, no sense to invalidate buffer.
        data->source_stream->operations->seek (data->source_stream, KAN_STREAM_SEEK_START, data->stream_position);
        const kan_file_size_t read = (kan_file_size_t) data->source_stream->operations->read (
            data->source_stream, request_end - request_begin, output);
        total_read += read;
        data->stream_position += read;
    }
    else
    {
        // Small request, fill buffer.
        if (!fill_buffer (data))
        {
            // Unable to fill buffer, exiting.
            return 0u;
        }

        const kan_file_size_t buffered_amount = KAN_MIN (data->stream_size, request_end) - request_begin;
        memcpy (output, data->buffer, buffered_amount);
        total_read += buffered_amount;
        data->stream_position += buffered_amount;
    }

    return total_read;
}

static inline kan_file_size_t write_to_buffer (struct random_access_stream_buffer_t *data,
                                               kan_file_size_t amount,
                                               const void *input_buffer)
{
    KAN_ASSERT (data->buffer_position + data->buffer_size - data->stream_position >= amount)
    memcpy (data->buffer + (data->stream_position - data->buffer_position), input_buffer, amount);
    data->stream_position += amount;
    return amount;
}

static kan_file_size_t buffered_write (struct kan_stream_t *stream, kan_file_size_t amount, const void *input_buffer)
{
    struct random_access_stream_buffer_t *data = (struct random_access_stream_buffer_t *) stream;
    KAN_ASSERT (data->buffer_position <= data->stream_position)

    kan_file_size_t buffer_end = data->buffer_position + data->buffer_size;
    kan_file_size_t available = buffer_end - data->stream_position;
    kan_file_size_t written;

    if (available < amount)
    {
        kan_file_size_t to_write = data->stream_position - data->buffer_position;
        KAN_ASSERT (to_write > 0u)

        if (!data->source_stream->operations->seek (data->source_stream, KAN_STREAM_SEEK_START, data->buffer_position))
        {
            // Failed to seek to buffer output, exiting.
            return 0u;
        }

        if (data->source_stream->operations->write (data->source_stream, to_write, data->buffer) != to_write)
        {
            // Unable to flush buffer, exiting.
            return 0u;
        }

        data->buffer_position = data->stream_position;
        buffer_end = data->buffer_position + data->buffer_size;
        available = buffer_end - data->stream_position;

        if (available < amount)
        {
            // Too big to fit into buffer, write directly.
            written = data->source_stream->operations->write (data->source_stream, amount, input_buffer);
            data->stream_position += written;
            data->buffer_position = data->stream_position;
        }
        else
        {
            written = write_to_buffer (data, amount, input_buffer);
        }
    }
    else
    {
        written = write_to_buffer (data, amount, input_buffer);
    }

    if (data->stream_position > data->stream_size)
    {
        data->stream_size = data->stream_position;
    }

    return written;
}

static kan_bool_t buffered_flush (struct kan_stream_t *stream)
{
    struct random_access_stream_buffer_t *data = (struct random_access_stream_buffer_t *) stream;
    KAN_ASSERT (data->buffer_position <= data->stream_position)
    kan_file_size_t to_write = data->stream_position - data->buffer_position;

    if (to_write > 0u)
    {
        if (!data->source_stream->operations->seek (data->source_stream, KAN_STREAM_SEEK_START, data->buffer_position))
        {
            // Failed to seek to buffer output, exiting.
            return KAN_FALSE;
        }

        const kan_file_size_t written =
            data->source_stream->operations->write (data->source_stream, to_write, data->buffer);
        data->buffer_position = data->stream_position;
        return written == to_write;
    }

    return KAN_TRUE;
}

static kan_file_size_t buffered_tell (struct kan_stream_t *stream)
{
    struct random_access_stream_buffer_t *data = (struct random_access_stream_buffer_t *) stream;
    return data->stream_position;
}

static kan_bool_t buffered_seek (struct kan_stream_t *stream,
                                 enum kan_stream_seek_pivot pivot,
                                 kan_file_offset_t offset)
{
    struct random_access_stream_buffer_t *data = (struct random_access_stream_buffer_t *) stream;
    if (data->as_stream.operations->write)
    {
        buffered_flush (stream);
    }

    switch (pivot)
    {
    case KAN_STREAM_SEEK_START:
        KAN_ASSERT (offset >= 0)
        data->stream_position = (kan_file_size_t) offset;

        if (data->stream_position > data->stream_size)
        {
            data->stream_position = data->stream_size;
        }

        break;

    case KAN_STREAM_SEEK_CURRENT:
    {
        kan_file_offset_t position_signed = (kan_file_offset_t) data->stream_position;
        position_signed += offset;

        if (position_signed < 0)
        {
            position_signed = 0;
        }
        else if (position_signed > (kan_file_offset_t) data->stream_size)
        {
            position_signed = (kan_file_offset_t) data->stream_size;
        }

        data->stream_position = (kan_file_size_t) position_signed;
        break;
    }

    case KAN_STREAM_SEEK_END:
    {
        kan_file_offset_t position_signed = (kan_file_offset_t) data->stream_size;
        position_signed += offset;

        if (position_signed < 0)
        {
            position_signed = 0;
        }
        else if (position_signed > (kan_file_offset_t) data->stream_size)
        {
            position_signed = (kan_file_offset_t) data->stream_size;
        }

        data->stream_position = (kan_file_size_t) position_signed;
        break;
    }
    }

    if (data->as_stream.operations->write)
    {
        data->buffer_position = data->stream_position;
    }

    return KAN_TRUE;
}

static void buffered_close (struct kan_stream_t *stream)
{
    struct random_access_stream_buffer_t *data = (struct random_access_stream_buffer_t *) stream;
    if (data->as_stream.operations->write)
    {
        buffered_flush (stream);
    }

    data->source_stream->operations->close (data->source_stream);
    kan_free_general (get_allocation_group (), data, sizeof (struct random_access_stream_buffer_t) + data->buffer_size);
}

static struct kan_stream_operations_t random_access_stream_buffer_read_operations = {
    .read = buffered_read,
    .write = NULL,
    .flush = NULL,
    .tell = buffered_tell,
    .seek = buffered_seek,
    .close = buffered_close,
};

static struct kan_stream_operations_t random_access_stream_buffer_write_operations = {
    .read = NULL,
    .write = buffered_write,
    .flush = buffered_flush,
    .tell = buffered_tell,
    .seek = buffered_seek,
    .close = buffered_close,
};

struct kan_stream_t *kan_random_access_stream_buffer_open_for_read (struct kan_stream_t *source_stream,
                                                                    kan_file_size_t buffer_size)
{
    KAN_ASSERT (kan_stream_is_readable (source_stream))
    KAN_ASSERT (kan_stream_is_random_access (source_stream))

    struct random_access_stream_buffer_t *stream =
        kan_allocate_general (get_allocation_group (),
                              kan_apply_alignment (sizeof (struct random_access_stream_buffer_t) + buffer_size,
                                                   _Alignof (struct random_access_stream_buffer_t)),
                              _Alignof (struct random_access_stream_buffer_t));

    stream->as_stream.operations = &random_access_stream_buffer_read_operations;
    stream->source_stream = source_stream;
    stream->stream_position = 0u;
    stream->buffer_position = 0u;
    stream->buffer_size = buffer_size;

    stream->source_stream->operations->seek (stream->source_stream, KAN_STREAM_SEEK_END, 0u);
    stream->stream_size = stream->source_stream->operations->tell (stream->source_stream);
    stream->source_stream->operations->seek (stream->source_stream, KAN_STREAM_SEEK_START, 0u);

    fill_buffer (stream);
    return (struct kan_stream_t *) stream;
}

struct kan_stream_t *kan_random_access_stream_buffer_open_for_write (struct kan_stream_t *source_stream,
                                                                     kan_file_size_t buffer_size)
{
    KAN_ASSERT (kan_stream_is_writeable (source_stream))
    KAN_ASSERT (kan_stream_is_random_access (source_stream))

    struct random_access_stream_buffer_t *stream =
        kan_allocate_general (get_allocation_group (),
                              kan_apply_alignment (sizeof (struct random_access_stream_buffer_t) + buffer_size,
                                                   _Alignof (struct random_access_stream_buffer_t)),
                              _Alignof (struct random_access_stream_buffer_t));

    stream->as_stream.operations = &random_access_stream_buffer_write_operations;
    stream->source_stream = source_stream;
    stream->stream_position = 0u;
    stream->buffer_position = 0u;
    stream->buffer_size = buffer_size;

    stream->source_stream->operations->seek (stream->source_stream, KAN_STREAM_SEEK_END, 0u);
    stream->stream_size = stream->source_stream->operations->tell (stream->source_stream);
    stream->source_stream->operations->seek (stream->source_stream, KAN_STREAM_SEEK_START, 0u);
    return (struct kan_stream_t *) stream;
}
