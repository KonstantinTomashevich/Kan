#pragma once

#include <resource_pipeline_tooling_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/hash_storage.h>
#include <kan/container/interned_string.h>
#include <kan/reflection/markup.h>
#include <kan/reflection/registry.h>
#include <kan/resource_pipeline/common_meta.h>
#include <kan/resource_pipeline/log.h>
#include <kan/resource_pipeline/tooling_meta.h>

// TODO: Docs later. Reflected data structure might be changed during implementation,
//       so it is ineffective to document it right now.

KAN_C_HEADER_BEGIN

RESOURCE_PIPELINE_TOOLING_API kan_allocation_group_t kan_resource_reflected_data_get_allocation_group (void);

struct kan_resource_reflected_data_resource_type_t
{
    KAN_REFLECTION_IGNORE
    struct kan_hash_storage_node_t node;

    kan_interned_string_t name;
    const struct kan_reflection_struct_t *source_type;

    /// \brief Resource type meta is preserved as pointer because there is nothing to intern or cache here.
    const struct kan_resource_type_meta_t *resource_type_meta;

    bool produced_from_build_rule;
    kan_interned_string_t build_rule_primary_input_type;
    kan_interned_string_t build_rule_platform_configuration_type;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t build_rule_secondary_types;

    kan_resource_build_rule_functor_t build_rule_functor;
    kan_resource_version_t build_rule_version;
};

RESOURCE_PIPELINE_TOOLING_API void kan_resource_reflected_data_resource_type_init (
    struct kan_resource_reflected_data_resource_type_t *instance);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_reflected_data_resource_type_shutdown (
    struct kan_resource_reflected_data_resource_type_t *instance);

/// \details Either interned string field (including array archetypes) with resource id or struct (including array
///          archetypes) that could contain references itself.
struct kan_resource_reflected_data_referencer_field_t
{
    const struct kan_reflection_field_t *field;
    kan_interned_string_t referenced_type;
    enum kan_resource_reference_meta_flags_t flags;
};

struct kan_resource_reflected_data_referencer_struct_t
{
    KAN_REFLECTION_IGNORE
    struct kan_hash_storage_node_t node;

    kan_interned_string_t name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_reflected_data_referencer_field_t)
    struct kan_dynamic_array_t fields_to_check;
};

RESOURCE_PIPELINE_TOOLING_API void kan_resource_reflected_data_referencer_struct_init (
    struct kan_resource_reflected_data_referencer_struct_t *instance);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_reflected_data_referencer_struct_shutdown (
    struct kan_resource_reflected_data_referencer_struct_t *instance);

struct kan_resource_reflected_data_storage_t
{
    kan_reflection_registry_t registry;

    KAN_REFLECTION_IGNORE
    struct kan_hash_storage_t resource_types;

    KAN_REFLECTION_IGNORE
    struct kan_hash_storage_t referencer_structs;
};

RESOURCE_PIPELINE_TOOLING_API void kan_resource_reflected_data_storage_build (
    struct kan_resource_reflected_data_storage_t *output, kan_reflection_registry_t registry);

RESOURCE_PIPELINE_TOOLING_API const struct kan_resource_reflected_data_resource_type_t *
kan_resource_reflected_data_storage_query_resource_type (const struct kan_resource_reflected_data_storage_t *storage,
                                                         kan_interned_string_t type_name);

RESOURCE_PIPELINE_TOOLING_API const struct kan_resource_reflected_data_referencer_struct_t *
kan_resource_reflected_data_storage_query_referencer_struct (
    const struct kan_resource_reflected_data_storage_t *storage, kan_interned_string_t type_name);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_reflected_data_storage_detect_references (
    const struct kan_resource_reflected_data_storage_t *storage,
    kan_interned_string_t referencer_type_name,
    const void *referencer_data,
    struct kan_dynamic_array_t *output_container);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_reflected_data_storage_shutdown (
    struct kan_resource_reflected_data_storage_t *instance);

KAN_C_HEADER_END
