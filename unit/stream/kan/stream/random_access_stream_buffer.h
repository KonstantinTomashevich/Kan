#pragma once

#include <stream_api.h>

#include <kan/api_common/c_header.h>
#include <kan/stream/stream.h>

/// \file
/// \brief Provides API for buffer creation for random access IO streams.
///
/// \par Random access stream buffer
/// \parblock
/// Random access stream buffers are wrappers for random access read and write streams that use buffering to reduce
/// count of low level IO operations.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Wraps given source read stream into buffer with given size. Returns buffered proxy stream.
STREAM_API struct kan_stream_t *kan_random_access_stream_buffer_open_for_read (struct kan_stream_t *source_stream,
                                                                               uint64_t buffer_size);

/// \brief Wraps given source write stream into buffer with given size. Returns buffered proxy stream.
STREAM_API struct kan_stream_t *kan_random_access_stream_buffer_open_for_write (struct kan_stream_t *source_stream,
                                                                                uint64_t buffer_size);

/// \brief Closes buffered proxy stream. Does not close underlying source stream.
STREAM_API void kan_random_access_stream_buffer_close (struct kan_stream_t *stream);

KAN_C_HEADER_END
