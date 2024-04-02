#pragma once

#include <resource_index_api.h>

#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>

/// \file
/// \brief Contains resource index functionality and serializable data structure.
///
/// \par Definition
/// \parblock
/// Resource index is a data structure that contains information about resources: their names, types and locations.
/// This information is organized in order to make querying location by type and name faster and easier.
///
/// Third party "typeless" resources are queried by their names only, because these resources do not have associated
/// type in Kan.
/// \endparblock
///
/// \par Serialization
/// \parblock
/// To make resource index easily serializable, serialization structure `kan_serialized_resource_index_t` is provided.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// Resource index follows readers-writers pattern: it is safe to query data from resource index when it is guaranteed
/// that it is not concurrently modified. Modification is not thread safe.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Contains path and name for native "typed" resource.
struct kan_serialized_resource_index_native_item_t
{
    kan_interned_string_t name;
    char *path;
};

RESOURCE_INDEX_API void kan_serialized_resource_index_native_item_init (
    struct kan_serialized_resource_index_native_item_t *item);

RESOURCE_INDEX_API void kan_serialized_resource_index_native_item_shutdown (
    struct kan_serialized_resource_index_native_item_t *item);

/// \brief Contains array of native "typed" resources of the same type.
struct kan_serialized_resource_index_native_container_t
{
    kan_interned_string_t type;

    /// \meta reflection_dynamic_array_type = "struct kan_serialized_resource_index_native_item_t"
    struct kan_dynamic_array_t items;
};

RESOURCE_INDEX_API void kan_serialized_resource_index_native_container_init (
    struct kan_serialized_resource_index_native_container_t *container);

RESOURCE_INDEX_API void kan_serialized_resource_index_native_container_shutdown (
    struct kan_serialized_resource_index_native_container_t *container);

/// \brief Contains path, size and name for third party "typeless" resource.
struct kan_serialized_resource_index_third_party_item_t
{
    kan_interned_string_t name;
    char *path;
    uint64_t size;
};

RESOURCE_INDEX_API void kan_serialized_resource_index_third_party_item_init (
    struct kan_serialized_resource_index_third_party_item_t *item);

RESOURCE_INDEX_API void kan_serialized_resource_index_third_party_item_shutdown (
    struct kan_serialized_resource_index_third_party_item_t *item);

/// \brief Serializable structure for serializing and deserializing resource indices.
struct kan_serialized_resource_index_t
{
    /// \meta reflection_dynamic_array_type = "struct kan_serialized_resource_index_native_container_t"
    struct kan_dynamic_array_t native;

    /// \meta reflection_dynamic_array_type = "struct kan_serialized_resource_index_third_party_item_t"
    struct kan_dynamic_array_t third_party;
};

RESOURCE_INDEX_API void kan_serialized_resource_index_init (struct kan_serialized_resource_index_t *index);

RESOURCE_INDEX_API void kan_serialized_resource_index_shutdown (struct kan_serialized_resource_index_t *index);

/// \brief Returns allocation group for allocating serialized resource index strings during deserialization.
RESOURCE_INDEX_API kan_allocation_group_t kan_serialized_resource_index_get_string_allocation_group (void);

typedef uint64_t kan_resource_index_t;

/// \brief Creates new resource index.
RESOURCE_INDEX_API kan_resource_index_t kan_resource_index_create (void);

/// \brief Loads resource index data from given serialized structure.
RESOURCE_INDEX_API void kan_resource_index_load (kan_resource_index_t index,
                                                 const struct kan_serialized_resource_index_t *serialized);

/// \brief Saves resource index data to given serialized structure.
RESOURCE_INDEX_API void kan_resource_index_save (kan_resource_index_t index,
                                                 struct kan_serialized_resource_index_t *serialized);

/// \brief Queries native resource path by its type and name pair.
RESOURCE_INDEX_API const char *kan_resource_index_get_native (kan_resource_index_t index,
                                                              kan_interned_string_t type,
                                                              kan_interned_string_t name);

/// \brief Queries third party resource path and size by its name.
RESOURCE_INDEX_API const char *kan_resource_index_get_third_party (kan_resource_index_t index,
                                                                   kan_interned_string_t name,
                                                                   uint64_t *size_output);

/// \brief Adds native resource with given information to the index.
RESOURCE_INDEX_API kan_bool_t kan_resource_index_add_native (kan_resource_index_t index,
                                                             kan_interned_string_t type,
                                                             kan_interned_string_t name,
                                                             const char *path);

/// \brief Adds third party resource with given information to the index.
RESOURCE_INDEX_API kan_bool_t kan_resource_index_add_third_party (kan_resource_index_t index,
                                                                  kan_interned_string_t name,
                                                                  const char *path,
                                                                  uint64_t size);

/// \brief Destroys given resources index.
RESOURCE_INDEX_API void kan_resource_index_destroy (kan_resource_index_t index);

KAN_C_HEADER_END
