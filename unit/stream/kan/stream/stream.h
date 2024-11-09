#pragma once

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>

/// \file
/// \brief Describes abstract IO stream structure and its operations.
///
/// \par IO abstraction
/// \parblock
/// It is quite useful to separate logic from input and output channels. For example, serialization and deserialization
/// should work the same way both for files and network. Stream is a common abstraction pattern for this purpose.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Base structure for streams.
/// \details Contains only pointer to operations table.
struct kan_stream_t
{
    struct kan_stream_operations_t *operations;
};

/// \brief Enumerates supported pivots for seek operation.
enum kan_stream_seek_pivot
{
    KAN_STREAM_SEEK_START = 0u,
    KAN_STREAM_SEEK_CURRENT,
    KAN_STREAM_SEEK_END,
};

typedef kan_file_size_t (*kan_stream_operation_read) (struct kan_stream_t *stream,
                                                      kan_file_size_t amount,
                                                      void *output_buffer);
typedef kan_file_size_t (*kan_stream_operation_write) (struct kan_stream_t *stream,
                                                       kan_file_size_t amount,
                                                       const void *input_buffer);
typedef kan_bool_t (*kan_stream_operation_flush) (struct kan_stream_t *stream);
typedef kan_file_size_t (*kan_stream_operation_tell) (struct kan_stream_t *stream);
typedef kan_bool_t (*kan_stream_operation_seek) (struct kan_stream_t *stream,
                                                 enum kan_stream_seek_pivot pivot,
                                                 kan_file_offset_t offset);
typedef void (*kan_stream_operation_close) (struct kan_stream_t *stream);

/// \brief Contains pointers to operations supported by category of streams.
/// \details Operations are optional, for example readonly stream will have `NULL`s in ::write and ::flush as these
///          operations aren't supported. As another example, ::tell and ::seek operations are only supported by
///          random access streams.
struct kan_stream_operations_t
{
    kan_stream_operation_read read;
    kan_stream_operation_write write;
    kan_stream_operation_flush flush;
    kan_stream_operation_tell tell;
    kan_stream_operation_seek seek;
    kan_stream_operation_close close;
};

/// \brief Helper to check whether it is possible to read from stream.
static inline kan_bool_t kan_stream_is_readable (struct kan_stream_t *stream)
{
    return stream->operations->read ? KAN_TRUE : KAN_FALSE;
}

/// \brief Helper to check whether it is possible to write into stream.
static inline kan_bool_t kan_stream_is_writeable (struct kan_stream_t *stream)
{
    return (stream->operations->write && stream->operations->flush) ? KAN_TRUE : KAN_FALSE;
}

/// \brief Helper to check whether stream supports position-changing operations.
static inline kan_bool_t kan_stream_is_random_access (struct kan_stream_t *stream)
{
    return (stream->operations->tell && stream->operations->seek) ? KAN_TRUE : KAN_FALSE;
}

KAN_C_HEADER_END
