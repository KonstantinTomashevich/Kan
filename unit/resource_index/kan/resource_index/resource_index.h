#pragma once

#include <resource_index_api.h>

#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>

/// \file
/// \brief Contains serializable data structure that works as resource index.
///
/// \par Definition
/// \parblock
/// Resource index is a data structure that contains information about resources: their names, types and locations.
/// Resource indices might be accompanied by string registries, that should be used to deserialize resources.
/// If accompanying string registry exists, then resource index is also encoded using this string registry.
/// \endparblock

KAN_C_HEADER_BEGIN

#define KAN_RESOURCE_INDEX_DEFAULT_NAME ".resource_index"
#define KAN_RESOURCE_INDEX_ACCOMPANYING_STRING_REGISTRY_DEFAULT_NAME ".resource_index_string_registry"

enum kan_resource_index_native_item_format_t
{
    KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY = 0u,
    KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA,
};

/// \brief Contains path, name and format for native "typed" resource.
struct kan_resource_index_native_item_t
{
    kan_interned_string_t name;
    enum kan_resource_index_native_item_format_t format;
    char *path;
};

RESOURCE_INDEX_API void kan_resource_index_native_item_init (struct kan_resource_index_native_item_t *item);

RESOURCE_INDEX_API void kan_resource_index_native_item_shutdown (struct kan_resource_index_native_item_t *item);

/// \brief Contains array of native "typed" resources of the same type.
struct kan_resource_index_native_container_t
{
    kan_interned_string_t type;

    /// \meta reflection_dynamic_array_type = "struct kan_resource_index_native_item_t"
    struct kan_dynamic_array_t items;
};

RESOURCE_INDEX_API void kan_resource_index_native_container_init (
    struct kan_resource_index_native_container_t *container);

RESOURCE_INDEX_API void kan_resource_index_native_container_shutdown (
    struct kan_resource_index_native_container_t *container);

/// \brief Contains path, size and name for third party "typeless" resource.
struct kan_resource_index_third_party_item_t
{
    kan_interned_string_t name;
    char *path;
    uint64_t size;
};

RESOURCE_INDEX_API void kan_resource_index_third_party_item_init (struct kan_resource_index_third_party_item_t *item);

RESOURCE_INDEX_API void kan_resource_index_third_party_item_shutdown (
    struct kan_resource_index_third_party_item_t *item);

/// \brief Serializable structure for serializing and deserializing resource indices.
struct kan_resource_index_t
{
    /// \meta reflection_dynamic_array_type = "struct kan_resource_index_native_container_t"
    struct kan_dynamic_array_t native;

    /// \meta reflection_dynamic_array_type = "struct kan_resource_index_third_party_item_t"
    struct kan_dynamic_array_t third_party;
};

RESOURCE_INDEX_API void kan_resource_index_init (struct kan_resource_index_t *index);

RESOURCE_INDEX_API void kan_resource_index_shutdown (struct kan_resource_index_t *index);

/// \brief Returns allocation group for allocating serialized resource index strings during deserialization.
RESOURCE_INDEX_API kan_allocation_group_t kan_resource_index_get_string_allocation_group (void);

/// \brief Helper for adding native entry to resource index data structure.
RESOURCE_INDEX_API void kan_resource_index_add_native_entry (struct kan_resource_index_t *index,
                                                             kan_interned_string_t type,
                                                             kan_interned_string_t name,
                                                             enum kan_resource_index_native_item_format_t format,
                                                             const char *path);

/// \brief Helper for adding third party entry to resource index data structure.
RESOURCE_INDEX_API void kan_resource_index_add_third_party_entry (struct kan_resource_index_t *index,
                                                                  kan_interned_string_t name,
                                                                  const char *path,
                                                                  uint64_t size);

KAN_C_HEADER_END
