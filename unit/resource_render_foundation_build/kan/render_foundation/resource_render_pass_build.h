#pragma once

#include <resource_render_foundation_build_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/render_foundation/resource_render_pass.h>
#include <kan/render_foundation/resource_rpl_build.h>

/// \file
/// \brief This file stores data structures for defining render passes to be built for runtime usage.
///
/// \par Overview
/// \parblock
/// To define a new render pass to be built for usage, `kan_resource_render_pass_header_t` resource should be used.
/// It configures render pass and defines pass variants. Also, render passes can be marked as unsupported on this
/// platform if tag requirements are not met.
///
/// Render pass code should not alter any attributes or any parameter sets except for pass parameter set. Otherwise, it
/// would break "common input interface" rule for material pipelines.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Describes how to build render pass variant with its unique sources and instance options.
struct kan_resource_render_pass_variant_header_t
{
    kan_interned_string_t name;

    /// \brief List of render pipeline language source file names for that variant.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t sources;

    /// \invariant Must only contain KAN_RPL_OPTION_SCOPE_INSTANCE options, otherwise compilation will fail.
    struct kan_resource_rpl_options_t instance_options;

    /// \brief List of stages that are explicitly disabled in this variant for optimization.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (enum kan_rpl_pipeline_stage_t)
    struct kan_dynamic_array_t disabled_stages;

    /// \brief List of tags that must be present in `kan_resource_material_pass_header_t::tags`
    ///        in order for this variant to be enabled.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t required_tags;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_render_pass_variant_header_init (
    struct kan_resource_render_pass_variant_header_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_render_pass_variant_header_shutdown (
    struct kan_resource_render_pass_variant_header_t *instance);

/// \brief Describes how to build a render pass.
struct kan_resource_render_pass_header_t
{
    /// \brief Render pass type.
    enum kan_render_pass_type_t type;

    /// \brief Tags that must present in `kan_resource_render_code_platform_configuration_t::supported_tags` in order to
    ///        include this render pass into resource compilation. Always include if this array is empty.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t required_tags;

    /// \brief List of render pass attachments and their descriptions.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_pass_attachment_t)
    struct kan_dynamic_array_t attachments;

    /// \brief List of variants for render pass.
    /// \details Can be left empty if render pass has only one variant without custom sources or options.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_render_pass_variant_header_t)
    struct kan_dynamic_array_t variants;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_render_pass_header_init (
    struct kan_resource_render_pass_header_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_render_pass_header_shutdown (
    struct kan_resource_render_pass_header_t *instance);

KAN_C_HEADER_END
