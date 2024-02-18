#pragma once

#include <serialization_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/reflection/registry.h>
#include <kan/serialization/state.h>
#include <kan/stream/stream.h>

KAN_C_HEADER_BEGIN

typedef uint64_t kan_serialization_binary_script_storage_t;

SERIALIZATION_API kan_serialization_binary_script_storage_t
kan_serialization_binary_script_storage_create (kan_reflection_registry_t registry);

SERIALIZATION_API void kan_serialization_binary_script_storage_destroy (
    kan_serialization_binary_script_storage_t storage);

typedef uint64_t kan_serialization_interned_string_registry_t;

#define KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY 0u

SERIALIZATION_API kan_serialization_interned_string_registry_t
kan_serialization_interned_string_registry_create_empty (void);

SERIALIZATION_API void kan_serialization_interned_string_registry_destroy (
    kan_serialization_interned_string_registry_t registry);

typedef uint64_t kan_serialization_interned_string_registry_reader_t;

SERIALIZATION_API kan_serialization_interned_string_registry_reader_t
kan_serialization_interned_string_registry_reader_create (struct kan_stream_t *stream, kan_bool_t load_only_registry);

SERIALIZATION_API enum kan_serialization_state_t kan_serialization_interned_string_registry_reader_step (
    kan_serialization_interned_string_registry_reader_t reader);

SERIALIZATION_API kan_serialization_interned_string_registry_t
kan_serialization_interned_string_registry_reader_get (kan_serialization_interned_string_registry_reader_t reader);

SERIALIZATION_API void kan_serialization_interned_string_registry_reader_destroy (
    kan_serialization_interned_string_registry_reader_t reader);

typedef uint64_t kan_serialization_interned_string_registry_writer_t;

SERIALIZATION_API kan_serialization_interned_string_registry_writer_t
kan_serialization_interned_string_registry_writer_create (struct kan_stream_t *stream,
                                                          kan_serialization_interned_string_registry_t registry);

SERIALIZATION_API enum kan_serialization_state_t kan_serialization_interned_string_registry_writer_step (
    kan_serialization_interned_string_registry_writer_t writer);

SERIALIZATION_API void kan_serialization_interned_string_registry_writer_destroy (
    kan_serialization_interned_string_registry_writer_t writer);

typedef uint64_t kan_serialization_binary_reader_t;

SERIALIZATION_API kan_serialization_binary_reader_t
kan_serialization_binary_reader_create (struct kan_stream_t *stream,
                                        void *instance,
                                        kan_interned_string_t type_name,
                                        kan_serialization_binary_script_storage_t script_storage,
                                        kan_serialization_interned_string_registry_t interned_string_registry,
                                        kan_allocation_group_t deserialized_string_allocation_group);

SERIALIZATION_API enum kan_serialization_state_t kan_serialization_binary_reader_step (
    kan_serialization_binary_reader_t reader);

SERIALIZATION_API void kan_serialization_binary_reader_destroy (kan_serialization_binary_reader_t reader);

typedef uint64_t kan_serialization_binary_writer_t;

SERIALIZATION_API kan_serialization_binary_writer_t
kan_serialization_binary_writer_create (struct kan_stream_t *stream,
                                        const void *instance,
                                        kan_interned_string_t type_name,
                                        kan_serialization_binary_script_storage_t script_storage,
                                        kan_serialization_interned_string_registry_t interned_string_registry);

SERIALIZATION_API enum kan_serialization_state_t kan_serialization_binary_writer_step (
    kan_serialization_binary_writer_t writer);

SERIALIZATION_API void kan_serialization_binary_writer_destroy (kan_serialization_binary_writer_t writer);

KAN_C_HEADER_END
