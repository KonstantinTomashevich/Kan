#pragma once

#include <serialization_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/reflection/registry.h>
#include <kan/serialization/state.h>
#include <kan/stream/stream.h>

KAN_C_HEADER_BEGIN

typedef uint64_t kan_serialization_rd_reader_t;

SERIALIZATION_API kan_serialization_rd_reader_t
kan_serialization_rd_reader_create (struct kan_stream_t *stream,
                                    void *instance,
                                    kan_interned_string_t type_name,
                                    kan_reflection_registry_t reflection_registry,
                                    kan_allocation_group_t deserialized_string_allocation_group);

SERIALIZATION_API enum kan_serialization_state_t kan_serialization_rd_reader_step (
    kan_serialization_rd_reader_t reader);

SERIALIZATION_API void kan_serialization_rd_reader_destroy (kan_serialization_rd_reader_t reader);

typedef uint64_t kan_serialization_rd_writer_t;

SERIALIZATION_API kan_serialization_rd_writer_t
kan_serialization_rd_writer_create (struct kan_stream_t *stream,
                                    const void *instance,
                                    kan_interned_string_t type_name,
                                    kan_reflection_registry_t reflection_registry);

SERIALIZATION_API enum kan_serialization_state_t kan_serialization_rd_writer_step (
    kan_serialization_rd_writer_t writer);

SERIALIZATION_API void kan_serialization_rd_writer_destroy (kan_serialization_rd_writer_t writer);

SERIALIZATION_API kan_bool_t kan_serialization_rd_read_type_header (struct kan_stream_t *stream,
                                                                    kan_interned_string_t *type_name_output);

SERIALIZATION_API kan_bool_t kan_serialization_rd_write_type_header (struct kan_stream_t *stream,
                                                                     kan_interned_string_t type_name);

KAN_C_HEADER_END
