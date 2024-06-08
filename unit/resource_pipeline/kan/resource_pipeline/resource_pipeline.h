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
/// \par Resource marking
/// \parblock
/// Resource types should be marked with `kan_resource_pipeline_resource_type_meta_t` empty meta to be visible for
/// reflection-based resource logic.
/// \endparblock
///
/// \par Reference scanning
/// \parblock
/// Resource pipeline makes it possible to register fields of structs that reference resources using
/// `kan_resource_pipeline_reference_meta_t` meta. Reflection registry can be scanned for this reference data and output
/// will be stored in `kan_resource_pipeline_reference_type_info_storage_t`, which allows to make reference detection
/// algorithm faster. Then, references can be extracted from every resource type using
/// `kan_resource_pipeline_detect_references` into serializable (and therefore cacheable)
/// `kan_resource_pipeline_detected_reference_container_t`. \endparblock
///
/// \par Compilation
/// \parblock
/// Resources can be marked as compilable using `kan_resource_pipeline_compilable_meta_t`. It means that they must be
/// processed and updated by compilation function that prepares them for release version of the game. Compilation might
/// result in change of resource type if it is requested in meta.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Empty meta for marking types for native resources that should be supported by resource logic.
struct kan_resource_pipeline_resource_type_meta_t
{
    uint64_t stub;
};

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
struct kan_resource_pipeline_reference_meta_t
{
    const char *type;
    enum kan_resource_pipeline_compilation_usage_type_t compilation_usage;
};

/// \brief Stores pointer to field and its `kan_resource_pipeline_reference_meta_t` for
///        `kan_resource_pipeline_reference_type_info_node_t`.
struct kan_resource_pipeline_reference_field_info_t
{
    /// \brief Field with reference if has interned string archetype or
    ///        transitional field (has interned strings with references).
    const struct kan_reflection_field_t *field;

    kan_bool_t is_leaf_field;
    kan_interned_string_t type;
    enum kan_resource_pipeline_compilation_usage_type_t compilation_usage;
};

/// \brief Contains information needed for `kan_resource_pipeline_detect_references` for a particular type.
struct kan_resource_pipeline_reference_type_info_node_t
{
    /// \meta reflection_ignore_struct_field
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
    /// \meta reflection_ignore_struct_field
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

/// \brief Contains information about loaded compilation dependency and its data.
struct kan_resource_pipeline_compilation_dependency_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    void *data;
};

/// \brief Declares signature for resource compilation.
typedef kan_bool_t (*kan_resource_pipeline_compile_functor_t) (
    void *input_instance,
    void *output_instance,
    uint64_t dependencies_count,
    struct kan_resource_pipeline_compilation_dependency_t *dependencies);

/// \brief Adds compilation function to resource type.
struct kan_resource_pipeline_compilable_meta_t
{
    /// \brief Name of the output resource type for compilation.
    const char *result_type_name;

    /// \brief Compilation function.
    kan_resource_pipeline_compile_functor_t compile;
};

KAN_C_HEADER_END
