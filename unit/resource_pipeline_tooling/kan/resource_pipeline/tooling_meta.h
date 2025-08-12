#pragma once

#include <resource_pipeline_tooling_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/interned_string.h>
#include <kan/resource_pipeline/common_meta.h>
#include <kan/stream/stream.h>

/// \file
/// \brief Contains meta for resource types that does not affect runtime, but is used for tooling purposes.
///
/// \par Build rules
/// \parblock
/// Build rules are used to produce resources of new type from resources of old type using user-defined logic. They are
/// primarily used to convert data from edition-friendly format to engine-friendly format. They can also be used to
/// split big resource into sub resources using secondary resource production, for example to separate image mips into
/// different image data resources.
///
/// Build rule always produces resource of built type from resource of primary input type and built resource always
/// has the same name as primary input resource. Resource references from primary input will be considered secondary
/// inputs and loaded if their types are listed in `kan_resource_build_rule_t::secondary_types`. Third party resource
/// references are always considered secondary inputs. Build rule can also produce secondary outputs of any resource
/// type through `kan_resource_build_rule_context_t::produce_secondary_output` function.
///
/// Special kind of rules -- import-rules -- are used exclusively for parsing third party files as they treat third
/// party resource file as their primary input. It is used as a neat trick to avoid parsing one third party resource
/// several times, which could've happened if it was just passed as secondary input.
/// \endparblock

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_resource_build_rule_interface_t);

/// \brief Enumerates result values that can be returned from build rule functor.
enum kan_resource_build_rule_result_t
{
    /// \brief Build rule was successfully executed.
    KAN_RESOURCE_BUILD_RULE_SUCCESS = 0u,

    /// \brief Build rule execution has failed.
    KAN_RESOURCE_BUILD_RULE_FAILURE,

    /// \brief Special result that is returned when given resource is not supported on current platform.
    /// \details Not an error when all references to this resource have flag KAN_RESOURCE_REFERENCE_PLATFORM_OPTIONAL,
    ///          otherwise treated as build error.
    KAN_RESOURCE_BUILD_RULE_UNSUPPORTED,
};

/// \brief Contains information about secondary input for the build rule.
struct kan_resource_build_rule_secondary_node_t
{
    /// \brief Pointer to the next secondary input if any.
    struct kan_resource_build_rule_secondary_node_t *next;

    kan_interned_string_t type;
    kan_interned_string_t name;

    union
    {
        /// \brief Loaded secondary resource data if `type` is not NULL.
        const void *data;

        /// \brief Path to third party binary data if `type` is NULL.
        const char *third_party_path;
    };
};

/// \brief Declares signature for secondary output production and registration. Returns registered name.
/// \details Returns given `name` on success or NULL on failure. This return pattern makes it possible to use convenient
///          if-assign constructions for error validation. `data` is allowed to point to stack as move and reset
///          functors from `kan_resource_type_meta_t` are used according to that meta docs.
typedef kan_interned_string_t (*kan_resource_build_rule_produce_secondary_output_functor_t) (
    kan_resource_build_rule_interface_t interface, kan_interned_string_t type, kan_interned_string_t name, void *data);

/// \brief Context that is provided to build rule execution functor.
struct kan_resource_build_rule_context_t
{
    /// \brief Name of the primary input and output resources.
    kan_interned_string_t primary_name;

    union
    {
        /// \brief Pointer to primary input resource data if rule is not an import-rule.
        const void *primary_input;

        /// \brief Path to third party resource file if rule is an import-rule.
        const char *primary_third_party_path;
    };

    /// \brief First node of secondary input list if any.
    struct kan_resource_build_rule_secondary_node_t *secondary_input_first;

    /// \brief Pointer to primary output resource data.
    void *primary_output;

    /// \brief Pointer to platform configuration if build rule requested any.
    const void *platform_configuration;

    /// \brief Path to a temporary directory that could be used for temporary outputs from third party tools.
    /// \details Deleted after build execution.
    const char *temporary_workspace;

    /// \brief Opaque interface data that is used to pass information to additional capability functors.
    kan_resource_build_rule_interface_t interface;

    /// \brief Functor that is used to produce secondary outputs.
    kan_resource_build_rule_produce_secondary_output_functor_t produce_secondary_output;
};

/// \brief Functor for build rule implementation logic.
typedef enum kan_resource_build_rule_result_t (*kan_resource_build_rule_functor_t) (
    struct kan_resource_build_rule_context_t *context);

/// \brief Declares a build rule for producing built resources.
/// \details Should be attached to struct with primary output resource type, where primary output resource type is a
///          resource that is always produced from this rule on successful execution.
struct kan_resource_build_rule_t
{
    /// \brief Type of the resource from which new primary output resource is being built.
    /// \details If this field is NULL, then build rule is considered import-rule: it searches for the raw third party
    ///          resource with required name and passes its path as primary input instead.
    const char *primary_input_type;

    /// \brief Name of platform configuration type for build rule or NULL if platform configuration is not used.
    const char *platform_configuration_type;

    /// \brief Count of entries in `secondary_types`.
    kan_instance_size_t secondary_types_count;

    /// \brief References in primary input resource with types from this array will be used as
    ///        secondary inputs for that build rule.
    const char **secondary_types;

    /// \brief Functor that implements build rule.
    kan_resource_build_rule_functor_t functor;

    /// \brief Build rule version for resource build routine results versioning.
    kan_resource_version_t version;
};

KAN_C_HEADER_END
