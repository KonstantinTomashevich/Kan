#pragma once

#include <resource_render_foundation_build_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/interned_string.h>
#include <kan/context/render_backend_system.h>
#include <kan/error/critical.h>
#include <kan/reflection/markup.h>
#include <kan/render_foundation/resource_material.h>
#include <kan/render_foundation/resource_rpl_build.h>

/// \file
/// \brief This file stores data structures for defining materials to be built for runtime usage.
///
/// \par Overview
/// \parblock
/// To define a new material to be built for usage, `kan_resource_material_header_t` resource should be used.
/// It configures material common configuration and configuration for every supported pass.
///
/// There are also resource types with `transient` in their name which means that they are used as intermediate
/// resources during build and should not be created directly by user.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Describes single pass configuration for the material.
struct kan_resource_material_pass_header_t
{
    /// \brief Pass name as resource.
    kan_interned_string_t name;

    /// \brief Entry points for pass stages.
    /// \details Entry points are allowed to be different for different passes.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_entry_point_t)
    struct kan_dynamic_array_t entry_points;

    /// \brief Instance options for this pass pipeline.
    /// \invariant Only KAN_RPL_OPTION_SCOPE_INSTANCE options are allowed!
    struct kan_resource_rpl_options_t options;

    /// \brief Tags for this material in this pass, used to enable optional pass variants.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t tags;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_pass_header_init (
    struct kan_resource_material_pass_header_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_pass_header_shutdown (
    struct kan_resource_material_pass_header_t *instance);

/// \brief Describes material resource with its sources, options and attached passes.
struct kan_resource_material_header_t
{
    /// \brief List of render pipeline language sources.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t sources;

    /// \brief Global options for all the passes.
    /// \invariant Only KAN_RPL_OPTION_SCOPE_GLOBAL options are allowed!
    struct kan_resource_rpl_options_t global_options;

    /// \brief List of passes in which this material could be used.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_pass_header_t)
    struct kan_dynamic_array_t passes;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_header_init (
    struct kan_resource_material_header_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_header_shutdown (
    struct kan_resource_material_header_t *instance);

/// \brief Intermediate structure to reference pipeline to be built.
struct kan_resource_material_pipeline_transient_t
{
    kan_interned_string_t pass_name;
    kan_interned_string_t variant_name;
    kan_interned_string_t pipeline_name;
};

/// \brief Intermediate structure used when pipeline compilation is ordered and we're waiting for it to finish.
struct kan_resource_material_transient_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t sources;

    struct kan_resource_rpl_options_t global_options;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_pipeline_transient_t)
    struct kan_dynamic_array_t pipelines;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_transient_init (
    struct kan_resource_material_transient_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_transient_shutdown (
    struct kan_resource_material_transient_t *instance);

KAN_C_HEADER_END
