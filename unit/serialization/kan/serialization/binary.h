#pragma once

#include <serialization_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/reflection/registry.h>
#include <kan/serialization/state.h>
#include <kan/stream/stream.h>

/// \file
/// \brief Provides API for serializing data from and to binary format using reflection.
///
/// \par Script storage
/// \parblock
/// Binary serialization uses internal script system to make serialization and deserialization faster. Instead of
/// directly using reflection in reader and writer, binary serialization generates internal scripts that are then used
/// by readers and writers. Scripts merge fields into blocks whenever possible, including recursively merging fields
/// from different substructures if possible. It greatly reduces count of IO operations and optimizes serialization
/// speed, but has a side effect of serializing and deserializing padding bytes.
/// \endparblock
///
/// \par Interned string registry
/// \parblock
/// Interned strings usually have common values in lots of different structure. In runtime it is optimized by string
/// interning -- there is always only one instance of common string in memory. But due to unstable nature of string
/// interning, it cannot be used for serialization. Usual solution is to write interned strings as common strings and
/// intern them during deserialization, but it results in lots of duplication in serialized data.
///
/// To combat this duplication we use interned string registry: special registry for interned strings that are used
/// in serialization. If interned string registry is used, interned strings are serialized as 32-bit integers which
/// greatly reduces their impact on serialized data size. Interned string registry is automatically populated during
/// serialization and can be then saved afterwards. To deserialize data, interned string registry must be deserialized
/// first and passed to appropriate readers.
/// \endparblock
///
/// \par Reading
/// \parblock
/// To read specific type from binary stream, use `kan_serialization_binary_reader_t`. It can be created using
/// `kan_serialization_binary_reader_create` function. Reading is step-based: instead of reading everything through one
/// blocking function, user can pause and resume by just stopping calling `kan_serialization_binary_reader_step`
/// function. See information about step return state in comments to `kan_serialization_state_t`. To destroy
/// `kan_serialization_binary_reader_t` after reading use `kan_serialization_binary_reader_destroy` function.
/// \endparblock
///
/// \par Writing
/// \parblock
/// To write specific type to binary stream, use `kan_serialization_binary_writer_t`. It can be created using
/// `kan_serialization_binary_writer_create` function. Writing is step-based: instead of writing everything through one
/// blocking function, user can pause and resume by just stopping calling `kan_serialization_binary_writer_step`
/// function. See information about step return state in comments to `kan_serialization_state_t`. To destroy
/// `kan_serialization_binary_writer_t` after writing use `kan_serialization_binary_writer_destroy` function.
/// \endparblock
///
/// \par Type headers
/// \parblock
/// In some cases data type is unknown, therefore type header in the beginning of the stream is needed to read type
/// name. For reading and writing type headers use `kan_serialization_binary_read_type_header` and
/// `kan_serialization_binary_write_type_header`. Type headers are allowed to use interned string registries like
/// other serialization logic.
/// \endparblock
///
/// \par Binary compatibility
/// \parblock
/// Right now, binary written on one platform under one base type preset is not compatible with game executable built
/// for other platform with different base type preset. In the future, we plan to fix that by providing recoding layer
/// that will be used to encode output files with proper endianness, sizes and offsets for patches.
/// \endparblock

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_serialization_binary_script_storage_t);

/// \brief Creates new script storage instance associated with given reflection registry.
SERIALIZATION_API kan_serialization_binary_script_storage_t
kan_serialization_binary_script_storage_create (kan_reflection_registry_t registry);

/// \brief Destroys given script storage and frees all the scripts.
SERIALIZATION_API void kan_serialization_binary_script_storage_destroy (
    kan_serialization_binary_script_storage_t storage);

KAN_HANDLE_DEFINE (kan_serialization_interned_string_registry_t);

/// \brief Creates new empty interned string registry, that will be later populated by writers.
SERIALIZATION_API kan_serialization_interned_string_registry_t
kan_serialization_interned_string_registry_create_empty (void);

/// \brief Destroys given interned string registry.
SERIALIZATION_API void kan_serialization_interned_string_registry_destroy (
    kan_serialization_interned_string_registry_t registry);

#define KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY_READER 0u

KAN_HANDLE_DEFINE (kan_serialization_interned_string_registry_reader_t);

/// \brief Creates reader for interned string registry stream.
/// \param stream Stream with interned string registry data.
/// \param load_only_registry If true, only deserialization-related data will be initialized.
///                           This flag is used to optimize memory usage and registry reading speed.
SERIALIZATION_API kan_serialization_interned_string_registry_reader_t
kan_serialization_interned_string_registry_reader_create (struct kan_stream_t *stream, bool load_only_registry);

/// \brief Advances interned string registry reader and returns its state.
SERIALIZATION_API enum kan_serialization_state_t kan_serialization_interned_string_registry_reader_step (
    kan_serialization_interned_string_registry_reader_t reader);

/// \brief After reader completed reading, deserialized interned string registry can be acquired through this function.
SERIALIZATION_API kan_serialization_interned_string_registry_t
kan_serialization_interned_string_registry_reader_get (kan_serialization_interned_string_registry_reader_t reader);

/// \brief Destroys given interned string registry reader.
SERIALIZATION_API void kan_serialization_interned_string_registry_reader_destroy (
    kan_serialization_interned_string_registry_reader_t reader);

#define KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY_WRITER 0u

KAN_HANDLE_DEFINE (kan_serialization_interned_string_registry_writer_t);

/// \brief Creates new writer for given interned string registry.
SERIALIZATION_API kan_serialization_interned_string_registry_writer_t
kan_serialization_interned_string_registry_writer_create (struct kan_stream_t *stream,
                                                          kan_serialization_interned_string_registry_t registry);

/// \brief Advances interned string registry writer and returns its state.
SERIALIZATION_API enum kan_serialization_state_t kan_serialization_interned_string_registry_writer_step (
    kan_serialization_interned_string_registry_writer_t writer);

/// \brief Destroys given interned string registry writer.
SERIALIZATION_API void kan_serialization_interned_string_registry_writer_destroy (
    kan_serialization_interned_string_registry_writer_t writer);

#define KAN_INVALID_SERIALIZATION_BINARY_READER 0u

KAN_HANDLE_DEFINE (kan_serialization_binary_reader_t);

/// \brief Creates new binary data reader.
/// \param stream Binary data stream that is used as input.
/// \param instance Initialized instance of structure to be used as output.
/// \param type_name Type name of the structure.
/// \param script_storage Script storage used for deserialization. Script storage is multithreading-friendly and can
///                       be safely used for multiple readers from multiple threads.
/// \param interned_string_registry Interned string registry for reading or
///                                 KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY if interned strings are expected
///                                 to be encoded like usual strings. Interned string storage is multithreading-friendly
///                                 and can be safely used for multiple readers from multiple threads.
/// \param deserialized_string_allocation_group Allocation group for KAN_REFLECTION_ARCHETYPE_STRING_POINTER
///                                             allocations if any.
SERIALIZATION_API kan_serialization_binary_reader_t
kan_serialization_binary_reader_create (struct kan_stream_t *stream,
                                        void *instance,
                                        kan_interned_string_t type_name,
                                        kan_serialization_binary_script_storage_t script_storage,
                                        kan_serialization_interned_string_registry_t interned_string_registry,
                                        kan_allocation_group_t deserialized_string_allocation_group);

/// \brief Advances reading process and returns current reader state.
SERIALIZATION_API enum kan_serialization_state_t kan_serialization_binary_reader_step (
    kan_serialization_binary_reader_t reader);

/// \brief Destroys given reader instance.
SERIALIZATION_API void kan_serialization_binary_reader_destroy (kan_serialization_binary_reader_t reader);

#define KAN_INVALID_SERIALIZATION_BINARY_WRITER 0u

KAN_HANDLE_DEFINE (kan_serialization_binary_writer_t);

/// \brief Creates new binary data writer.
/// \param stream Binary data stream that is used as output.
/// \param instance Instance of structure to be written.
/// \param type_name Type name of the structure.
/// \param script_storage Script storage used for serialization. Script storage is multithreading-friendly and can
///                       be safely used for multiple writers from multiple threads.
/// \param interned_string_registry Interned string registry for writing or
///                                 KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY if interned strings are expected
///                                 to be encoded like usual strings. Interned string storage is multithreading-friendly
///                                 and can be safely used for multiple readers from multiple threads.
/// \return
SERIALIZATION_API kan_serialization_binary_writer_t
kan_serialization_binary_writer_create (struct kan_stream_t *stream,
                                        const void *instance,
                                        kan_interned_string_t type_name,
                                        kan_serialization_binary_script_storage_t script_storage,
                                        kan_serialization_interned_string_registry_t interned_string_registry);

/// \brief Advances writing process and returns current writer state.
SERIALIZATION_API enum kan_serialization_state_t kan_serialization_binary_writer_step (
    kan_serialization_binary_writer_t writer);

/// \brief Destroys given writer instance.
SERIALIZATION_API void kan_serialization_binary_writer_destroy (kan_serialization_binary_writer_t writer);

/// \brief Reads type header in binary format from given stream. Can optionally use interned string registry.
SERIALIZATION_API bool kan_serialization_binary_read_type_header (
    struct kan_stream_t *stream,
    kan_interned_string_t *type_name_output,
    kan_serialization_interned_string_registry_t interned_string_registry);

/// \brief Writes type header in binary format to given stream. Can optionally use interned string registry.
SERIALIZATION_API bool kan_serialization_binary_write_type_header (
    struct kan_stream_t *stream,
    kan_interned_string_t type_name,
    kan_serialization_interned_string_registry_t interned_string_registry);

KAN_C_HEADER_END
