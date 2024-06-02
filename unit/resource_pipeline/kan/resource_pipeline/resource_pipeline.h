#pragma once

#include <resource_reference_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/hash_storage.h>
#include <kan/container/interned_string.h>
#include <kan/reflection/registry.h>

/// \file
/// \brief Provides common utilities for building resource pipeline and interacting with it.
///
/// \par Reference scanning
/// \parblock
/// Resource pipeline makes it possible to register fields of structs that reference resources using
/// `kan_resource_pipeline_meta_t` meta. Reflection registry can be scanned for this reference data and output will be
/// stored in `kan_resource_pipeline_reference_type_info_storage_t`, which allows to make reference detection algorithm
/// faster. Then, references can be extracted from every resource type using `kan_resource_pipeline_detect_references`
/// into serializable (and therefore cacheable) `kan_resource_pipeline_detected_reference_container_t`.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Describes whether and how reference is used in resource compilation routine if any.
enum kan_resource_pipeline_compilation_usage_type_t
{
    /// \brief Referenced object is not needed for resource compilation.
    KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED = 0u,

    /// \brief Referenced object raw data is needed for resource compilation.
    KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW,

    /// \brief Compiled version of referenced object is needed for resource compilation.
    KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED,
};

/// \brief Meta for marking fields of native resources that point to other resources by their name.
/// \invariant Field type is 'kan_interned_string_t'.
struct kan_resource_pipeline_meta_t
{
    kan_interned_string_t type;
    enum kan_resource_pipeline_compilation_usage_type_t compilation_usage;
};

/// \brief Stores pointer to field and its `kan_resource_pipeline_meta_t` for
///        `kan_resource_pipeline_reference_type_info_node_t`.
struct kan_resource_pipeline_reference_field_info_t
{
    /// \brief Field with reference if has interned string archetype or
    ///        transitional field (has interned strings with references).
    const struct kan_reflection_field_t *field;

    /// \warning Null for transitional fields.
    const struct kan_resource_pipeline_meta_t *reference_meta;
};

/// \brief Contains information needed for `kan_resource_pipeline_detect_references` for a particular type.
struct kan_resource_pipeline_reference_type_info_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t type_name;

    /// \brief Lists fields that may contain references (including structs that may contain references in their fields).
    /// \meta reflection_dynamic_array_type = "struct kan_resource_pipeline_reference_field_info_t"
    struct kan_dynamic_array_t fields_to_check;

    /// \brief List of resource types that are able to reference resources of this type.
    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t referencer_types;

    kan_bool_t is_resource_type;
    kan_bool_t contains_patches;
};

/// \brief Contains processed reflection data for reference detection.
struct kan_resource_pipeline_reference_type_info_storage_t
{
    struct kan_hash_storage_t scanned_types;
    kan_allocation_group_t scanned_allocation_group;
};

/// \brief Builds reference type info storage from given registry, allocating it in given allocation group.
RESOURCE_REFERENCE_API void kan_resource_pipeline_reference_type_info_storage_build (
    struct kan_resource_pipeline_reference_type_info_storage_t *storage,
    kan_allocation_group_t allocation_group,
    kan_reflection_registry_t registry);

/// \brief Queries resource type info node for particular type.
RESOURCE_REFERENCE_API const struct kan_resource_pipeline_reference_type_info_node_t *
kan_resource_pipeline_reference_type_info_storage_query (
    struct kan_resource_pipeline_reference_type_info_storage_t *storage, kan_interned_string_t type_name);

/// \brief Frees resource type info storage and its resources.
RESOURCE_REFERENCE_API void kan_resource_pipeline_reference_type_info_storage_shutdown (
    struct kan_resource_pipeline_reference_type_info_storage_t *storage);

/// \brief Describes reference that was detected during scan for references.
struct kan_resource_pipeline_detected_reference_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    enum kan_resource_pipeline_compilation_usage_type_t compilation_usage;
};

/// \brief Container for detected references.
struct kan_resource_pipeline_detected_reference_container_t
{
    /// \meta reflection_dynamic_array_type = "struct kan_resource_pipeline_detected_reference_t"
    struct kan_dynamic_array_t detected_references;
};

RESOURCE_REFERENCE_API void kan_resource_pipeline_detected_reference_container_init (
    struct kan_resource_pipeline_detected_reference_container_t *instance);

RESOURCE_REFERENCE_API void kan_resource_pipeline_detected_reference_container_shutdown (
    struct kan_resource_pipeline_detected_reference_container_t *instance);

/// \brief Detects all references in given referencer instance and outputs them to given container.
RESOURCE_REFERENCE_API void kan_resource_pipeline_detect_references (
    struct kan_resource_pipeline_reference_type_info_storage_t *storage,
    kan_interned_string_t referencer_type_name,
    const void *referencer_data,
    struct kan_resource_pipeline_detected_reference_container_t *output_container);

KAN_C_HEADER_END
