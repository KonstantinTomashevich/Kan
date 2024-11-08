#pragma once

#include <serialization_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/reflection/registry.h>
#include <kan/serialization/state.h>
#include <kan/stream/stream.h>

/// \file
/// \brief Provides API for deserializing data from and to readable data format using reflection.
///
/// \par Reading
/// \parblock
/// To read specific type from readable data stream, use `kan_serialization_rd_reader_t`. It can be created using
/// `kan_serialization_rd_reader_create` function. Reading is step-based: instead of reading everything through one
/// blocking function, user can pause and resume by just stopping calling `kan_serialization_rd_reader_step` function.
/// See information about step return state in comments to `kan_serialization_state_t`. To destroy
/// `kan_serialization_rd_reader_t` after reading use `kan_serialization_rd_reader_destroy` function.
/// \endparblock
///
/// \par Writing
/// \parblock
/// To write specific type to readable data stream, use `kan_serialization_rd_writer_t`. It can be created using
/// `kan_serialization_rd_writer_create` function. Writing is step-based: instead of writing everything through one
/// blocking function, user can pause and resume by just stopping calling `kan_serialization_rd_writer_step` function.
/// See information about step return state in comments to `kan_serialization_state_t`. To destroy
/// `kan_serialization_rd_writer_t` after writing use `kan_serialization_rd_writer_destroy` function.
/// \endparblock
///
/// \par Type headers
/// \parblock
/// In some cases data type is unknown, therefore type header in the beginning of the stream is needed to read type
/// name. For reading and writing type headers use `kan_serialization_rd_read_type_header` and
/// `kan_serialization_rd_write_type_header`.
/// \endparblock

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_serialization_rd_reader_t);

/// \brief Creates new instance of reader in order to read into given instance.
/// \param stream Stream that contains data in readable data format.
/// \param instance Initialized instance of structure that is used as output.
/// \param type_name Type name of the structure.
/// \param reflection_registry Registry with reflection data for reading.
/// \param deserialized_string_allocation_group Allocation group for KAN_REFLECTION_ARCHETYPE_STRING_POINTER
///                                             allocations if any.
SERIALIZATION_API kan_serialization_rd_reader_t
kan_serialization_rd_reader_create (struct kan_stream_t *stream,
                                    void *instance,
                                    kan_interned_string_t type_name,
                                    kan_reflection_registry_t reflection_registry,
                                    kan_allocation_group_t deserialized_string_allocation_group);

/// \brief Advances reading process and returns current reader state.
SERIALIZATION_API enum kan_serialization_state_t kan_serialization_rd_reader_step (
    kan_serialization_rd_reader_t reader);

/// \brief Destroys given reader instance.
SERIALIZATION_API void kan_serialization_rd_reader_destroy (kan_serialization_rd_reader_t reader);

KAN_HANDLE_DEFINE (kan_serialization_rd_writer_t);

/// \brief Creates new instance of writer in order to write given instance into given stream.
/// \param stream Stream that serves as output for writer.
/// \param instance Instance of structure to be written.
/// \param type_name Type name of the structure.
/// \param reflection_registry Registry with reflection data for writing.
SERIALIZATION_API kan_serialization_rd_writer_t
kan_serialization_rd_writer_create (struct kan_stream_t *stream,
                                    const void *instance,
                                    kan_interned_string_t type_name,
                                    kan_reflection_registry_t reflection_registry);

/// \brief Advances writing process and returns current writer state.
SERIALIZATION_API enum kan_serialization_state_t kan_serialization_rd_writer_step (
    kan_serialization_rd_writer_t writer);

/// \brief Destroys given writer instance.
SERIALIZATION_API void kan_serialization_rd_writer_destroy (kan_serialization_rd_writer_t writer);

/// \brief Reads type header in readable data format from given stream.
SERIALIZATION_API kan_bool_t kan_serialization_rd_read_type_header (struct kan_stream_t *stream,
                                                                    kan_interned_string_t *type_name_output);

/// \brief Writes type header in readable data format to given stream.
SERIALIZATION_API kan_bool_t kan_serialization_rd_write_type_header (struct kan_stream_t *stream,
                                                                     kan_interned_string_t type_name);

KAN_C_HEADER_END
