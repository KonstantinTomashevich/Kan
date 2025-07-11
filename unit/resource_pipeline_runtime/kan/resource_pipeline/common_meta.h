#pragma once

#include <resource_pipeline_runtime_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/hash/hash.h>

/// \file
/// \brief Contains meta for declaring resource types that are accessible in runtime.

KAN_C_HEADER_BEGIN

/// \brief Enumerates flags that add special behaviors to resource types.
enum kan_resource_type_flags_t
{
    /// \brief Resource should be treated as root while building.
    /// \details Root resources are scanned first and are used as roots for used resource scanning algorithm.
    KAN_RESOURCE_TYPE_ROOT = 1u << 0u,

    /// \brief It is allowed to remove duplicates of this resource if resource build tool is
    ///        able to properly fix references after this procedure.
    /// \details In practice, build tool only guaranties such merging for build rule secondary produced resources.
    /// \invariant Cannot be used with KAN_RESOURCE_TYPE_ROOT.
    KAN_RESOURCE_TYPE_MERGEABLE = 1u << 1u,
};

/// \brief Defines type used for versioning resource types.
/// \details For versioning rather trivial resources that do not take ages to be built, CUSHION_START_NS_X64 is advised
///          as simple and robust approach for automatically bumping version when file that contains meta is recompiled.
///          And if that file has include with resource type structure, it will always be recompiled on structure change
///          and version will be properly updated.
///
///          For resources where it is crucial to avoid excessive rebuilds due to time constraints,
///          conservative method with version-enum is advised.
typedef kan_time_size_t kan_resource_version_t;

/// \brief Declares signature for move function. Used for secondary production.
/// \details When NULL, `kan_reflection_move_struct` will be used instead.
typedef void (*kan_resource_type_move_functor_t) (void *target, void *source);

/// \brief Declares signature for mergeable hash function.
/// \details When NULL, `kan_reflection_hash_struct` will be used instead.
typedef kan_hash_t (*kan_resource_mergeable_hash_functor_t) (void *mergeable);

/// \brief Declares signature for mergeable equality check.
/// \details When NULL, `kan_reflection_are_structs_equal` will be used instead.
typedef bool (*kan_resource_mergeable_is_equal_functor_t) (const void *first, const void *second);

/// \brief Declares signature for mergeable reset function.
/// \details When NULL, `kan_reflection_reset_struct` will be used instead.
typedef void (*kan_resource_mergeable_reset_functor_t) (void *mergeable);

/// \brief Struct meta that marks this struct as resource type.
/// \invariant Cannot be attached several times to one struct.
struct kan_resource_type_meta_t
{
    /// \brief Flags that can alter resource usage behavior including resource build pipeline.
    enum kan_resource_type_flags_t flags;

    /// \brief Resource version is used to detect cases when resource structure has changed while
    ///        raw sources were not changed.
    kan_resource_version_t version;

    kan_resource_type_move_functor_t move;

    kan_resource_mergeable_hash_functor_t mergeable_hash;
    kan_resource_mergeable_is_equal_functor_t mergeable_is_equal;
    kan_resource_mergeable_reset_functor_t mergeable_reset;

#define KAN_RESOURCE_TYPE_META_MERGEABLE_DEFAULT                                                                       \
    .mergeable_hash = NULL, mergeable_is_equal = NULL, .mergeable_reset = NULL
};

/// \brief Enumerates flags that may alter how resource reference is processed.
enum kan_resource_reference_meta_flags_t
{
    /// \brief Informs that resource can be absent on some platforms.
    /// \details This flag is needed for the cases when some resources are not needed on some platforms and therefore
    ///          could be skipped on them. It still results in build time check whether resource can be found and
    ///          built, but does not result in error if resource build returns "unsupported" compilation result.
    KAN_RESOURCE_REFERENCE_META_PLATFORM_OPTIONAL = 1u << 0u,
};

/// \brief Struct field meta that informs that this field is either an interned string with resource name or an array
///        of interned strings with resource names that should be taken into account in resource build pipeline.
struct kan_resource_reference_meta_t
{
    /// \brief Name of a resource type to which reference points.
    const char *type_name;

    enum kan_resource_reference_meta_flags_t flags;
};

KAN_C_HEADER_END
