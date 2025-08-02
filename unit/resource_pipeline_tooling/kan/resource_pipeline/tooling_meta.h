#pragma once

#include <resource_pipeline_tooling_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/interned_string.h>
#include <kan/resource_pipeline/common_meta.h>
#include <kan/stream/stream.h>

// TODO: Docs later. Tooling-side meta structure might be changed during implementation,
//       so it is ineffective to document it right now.

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_resource_build_rule_interface_t);

enum kan_resource_build_rule_result_t
{
    KAN_RESOURCE_BUILD_RULE_SUCCESS = 0u,
    KAN_RESOURCE_BUILD_RULE_FAILURE,

    /// \brief Special result that is returned when given resource is not supported on current platform.
    /// \details Not an error when all references to this resource have flag KAN_RESOURCE_REFERENCE_PLATFORM_OPTIONAL,
    ///          otherwise treated as build error.
    KAN_RESOURCE_BUILD_RULE_UNSUPPORTED,
};

struct kan_resource_build_rule_secondary_node_t
{
    struct kan_resource_build_rule_secondary_node_t *next;
    kan_interned_string_t type;
    kan_interned_string_t name;

    union
    {
        /// \brief Loaded secondary resource data if `type` is not NULL.
        const void *data;

        /// \brief Path to third party binary data.
        const char *third_party_path;
    };
};

/// \brief Declares signature for secondary output production and registration. Returns registered name.
/// \details Returns given `name` on success or NULL on failure. This return pattern makes it possible to use convenient
///          if-assign constructions for error validation.
typedef kan_interned_string_t (*kan_resource_build_rule_produce_secondary_output_functor_t) (
    kan_resource_build_rule_interface_t interface, kan_interned_string_t type, kan_interned_string_t name, void *data);

struct kan_resource_build_rule_context_t
{
    kan_interned_string_t primary_name;

    union
    {
        const void *primary_input;

        const char *primary_third_party_path;
    };

    struct kan_resource_build_rule_secondary_node_t *secondary_input_first;

    void *primary_output;
    const void *platform_configuration;

    /// \brief Path to a temporary directory that could be used for temporary outputs from third party tools.
    /// \details Deleted after build execution.
    const char *temporary_workspace;

    kan_resource_build_rule_interface_t interface;
    kan_resource_build_rule_produce_secondary_output_functor_t produce_secondary_output;
};

typedef enum kan_resource_build_rule_result_t (*kan_resource_build_rule_functor_t) (
    struct kan_resource_build_rule_context_t *context);

/// \details Should be attached to struct with primary output resource type, where primary output resource type is a
///          resource that is always produced from this rule on successful execution.
struct kan_resource_build_rule_t
{
    /// \brief Type of the resource from which new output resource is being built.
    /// \details If this field is NULL, then build rule is considered import-rule: it searches for the raw third party
    ///          resource with given name and passes its path as primary input instead.
    const char *primary_input_type;

    const char *platform_configuration_type;

    /// \brief Count of entries in `secondary_types`.
    kan_instance_size_t secondary_types_count;

    /// \brief References in primary input resource with types from this array will be used as
    ///        secondary inputs for that build rule.
    const char **secondary_types;

    kan_resource_build_rule_functor_t functor;
    kan_resource_version_t version;
};

KAN_C_HEADER_END
