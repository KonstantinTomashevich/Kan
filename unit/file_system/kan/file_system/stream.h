#pragma once

#include <file_system_api.h>

#include <kan/api_common/c_header.h>
#include <kan/stream/stream.h>

/// \file
/// \brief Implements stream-based IO abstraction for real file system files.
///
/// \par Direct file stream
/// \parblock
/// Direct file stream has no under-the-hood buffering and directly executes appropriate file IO operations.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Attempts to open file under given path for read and returns stream pointer on success.
FILE_SYSTEM_API struct kan_stream_t *kan_direct_file_stream_open_for_read (const char *path, bool binary);

/// \brief Attempts to open file under given path for write and returns stream pointer on success.
FILE_SYSTEM_API struct kan_stream_t *kan_direct_file_stream_open_for_write (const char *path, bool binary);

KAN_C_HEADER_END
