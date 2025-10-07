#pragma once

#include <resource_pipeline_api.h>

#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/reflection/markup.h>

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

/// \brief Default name for resource index file.
#define KAN_RESOURCE_INDEX_DEFAULT_NAME ".resource_index"

/// \brief Default name for interned string registry that might be attached to resource index.
#define KAN_RESOURCE_INDEX_ACCOMPANYING_STRING_REGISTRY_DEFAULT_NAME ".resource_index_string_registry"

/// \brief Returns allocation group that is used for allocating everything connected to indices.
RESOURCE_PIPELINE_API kan_allocation_group_t kan_resource_index_get_allocation_group (void);

/// \brief Describes one resource in index.
struct kan_resource_index_item_t
{
    kan_interned_string_t name;
    char *path;
};

RESOURCE_PIPELINE_API void kan_resource_index_item_init (struct kan_resource_index_item_t *item);

RESOURCE_PIPELINE_API void kan_resource_index_item_shutdown (struct kan_resource_index_item_t *item);

/// \brief Describes all resources of particular type in index.
struct kan_resource_index_container_t
{
    kan_interned_string_t type;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_index_item_t)
    struct kan_dynamic_array_t items;
};

RESOURCE_PIPELINE_API void kan_resource_index_container_init (struct kan_resource_index_container_t *container);

RESOURCE_PIPELINE_API void kan_resource_index_container_shutdown (struct kan_resource_index_container_t *container);

/// \brief Data structure that contains full resource index.
struct kan_resource_index_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_index_container_t)
    struct kan_dynamic_array_t containers;

    /// \brief Items for deployed resources in third party formats.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_index_item_t)
    struct kan_dynamic_array_t third_party_items;
};

RESOURCE_PIPELINE_API void kan_resource_index_init (struct kan_resource_index_t *index);

RESOURCE_PIPELINE_API void kan_resource_index_shutdown (struct kan_resource_index_t *index);

KAN_C_HEADER_END
