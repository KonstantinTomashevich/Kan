#pragma once

#include <checksum_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>

/// \file
/// \brief Provides simple API for streamed checksum calculation.

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_checksum_state_t);

/// \brief Creates new checksum calculation state.
CHECKSUM_API kan_checksum_state_t kan_checksum_create (void);

/// \brief Appends given data of given size to checksum calculation state.
CHECKSUM_API void kan_checksum_append (kan_checksum_state_t state, kan_file_size_t size, void *data);

/// \brief Finalizes checksum calculates and destroys checksum state. Returns checksum.
CHECKSUM_API kan_file_size_t kan_checksum_finalize (kan_checksum_state_t state);

KAN_C_HEADER_END
