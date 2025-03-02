#pragma once

#include <resource_material_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/context/render_backend_system.h>
#include <kan/error/critical.h>
#include <kan/reflection/markup.h>
#include <kan/render_pipeline_language/compiler.h>
#include <kan/resource_material/resource_render_pass.h>

/// \file
/// \brief This file stores various resource types needed to properly store, compile and use materials.
///
/// \par Overview
/// \parblock
/// Material is a set of render pipeline language sources and their configurations for different render passes.
/// It specifies KAN_RPL_OPTION_SCOPE_GLOBAL options in `kan_resource_material_t::global_options` and
/// KAN_RPL_OPTION_SCOPE_INSTANCE can be specified in every pass configuration in
/// `kan_resource_material_pass_t::options`. This split is done to make sure from the architecture point of view that
/// all parameter sets for every pipeline created from material are identical except from pass parameter set.
/// Pass parameter set layout is specified `kan_resource_render_pass_t::pass_set_source` which is injected automatically
/// during compilation for every pass.
///
/// When compiled, material produces family and pipeline byproducts. Family defines common input interface for all
/// the pipelines (except for pass set) by storing information about attributes, material, object and unstable sets.
/// Pipelines are created and compiled for each pass separately (if that pass is considered supported for the platform).
/// Their meta is stripped of input information by design as it is the same for all the families.
///
/// Materials with the same source list and global parameters can share families -- it would be handled by byproduct
/// routine. In the same way, if materials share family, then if some of their pipelines have the same instance options
/// and passes, these pipelines would be shared too.
///
/// Keep in mind, that material compilation mostly consists of compiling source code into intermediate code format,
/// like SPIRV for Vulkan. When creating pipeline in runtime, its code would still need to be compiled from intermediate
/// format to vendor-specific format, which might take some time.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Describes flag option and its value for material setup.
struct kan_resource_material_flag_option_t
{
    kan_interned_string_t name;
    kan_bool_t value;
};

/// \brief Describes count option and its value for material setup.
struct kan_resource_material_count_option_t
{
    kan_interned_string_t name;
    kan_rpl_unsigned_int_literal_t value;
};

/// \brief Utility structure with option storages for material or pass.
struct kan_resource_material_options_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_flag_option_t)
    struct kan_dynamic_array_t flags;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_count_option_t)
    struct kan_dynamic_array_t counts;
};

RESOURCE_MATERIAL_API void kan_resource_material_options_init (struct kan_resource_material_options_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_options_shutdown (struct kan_resource_material_options_t *instance);

/// \brief Describes single pass configuration for the material.
struct kan_resource_material_pass_t
{
    /// \brief Pass name as resource.
    kan_interned_string_t name;

    /// \details Entry points are allowed to be different for different passes.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_entry_point_t)
    struct kan_dynamic_array_t entry_points;

    /// \brief Instance options for this pass pipeline.
    /// \invariant Only KAN_RPL_OPTION_SCOPE_INSTANCE options are allowed!
    struct kan_resource_material_options_t options;
};

RESOURCE_MATERIAL_API void kan_resource_material_pass_init (struct kan_resource_material_pass_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_pass_shutdown (struct kan_resource_material_pass_t *instance);

/// \brief Describes material resource with its sources, options and attached passes.
struct kan_resource_material_t
{
    /// \brief List of render pipeline language sources.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t sources;

    /// \brief Global options for all the passes.
    /// \invariant Only KAN_RPL_OPTION_SCOPE_GLOBAL options are allowed!
    struct kan_resource_material_options_t global_options;

    /// \brief List of passes in which this material could be used.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_pass_t)
    struct kan_dynamic_array_t passes;
};

RESOURCE_MATERIAL_API void kan_resource_material_init (struct kan_resource_material_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_shutdown (struct kan_resource_material_t *instance);

/// \brief Contains target platform configuration for compiling materials.
/// \details Must be present in order to make compilation possible.
struct kan_resource_material_platform_configuration_t
{
    /// \brief Intermediate format to which pipelines should be compiled.
    enum kan_render_code_format_t code_format;

    /// \brief List of render pass tags that can be used to decide whether pass is supported on this platform.
    /// \details Main goal of support tags is to exclude excessive passes like editor-only passes from build, but
    ///          it can be used to customize passes for different platforms too.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t supported_pass_tags;
};

RESOURCE_MATERIAL_API void kan_resource_material_platform_configuration_init (
    struct kan_resource_material_platform_configuration_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_platform_configuration_shutdown (
    struct kan_resource_material_platform_configuration_t *instance);

/// \brief Returns whether given pass resource is supported under given platform configuration.
RESOURCE_MATERIAL_API kan_bool_t kan_resource_material_platform_configuration_is_pass_supported (
    const struct kan_resource_material_platform_configuration_t *configuration,
    const struct kan_resource_render_pass_t *pass);

/// \brief Contains compiled data of pipeline family -- meta that describes common input pattern for pipelines.
struct kan_resource_material_pipeline_family_compiled_t
{
    struct kan_rpl_meta_t meta;
};

RESOURCE_MATERIAL_API void kan_resource_material_pipeline_family_compiled_init (
    struct kan_resource_material_pipeline_family_compiled_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_pipeline_family_compiled_shutdown (
    struct kan_resource_material_pipeline_family_compiled_t *instance);

/// \brief Contains compiled data for one pipeline -- its entry points, meta and code.
struct kan_resource_material_pipeline_compiled_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_entry_point_t)
    struct kan_dynamic_array_t entry_points;

    enum kan_render_code_format_t code_format;

    /// \details Attribute buffers and pipeline parameter sets are excluded as
    ///          they should be the same for the whole material.
    struct kan_rpl_meta_t meta;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (uint8_t)
    struct kan_dynamic_array_t code;
};

RESOURCE_MATERIAL_API void kan_resource_material_pipeline_compiled_init (
    struct kan_resource_material_pipeline_compiled_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_pipeline_compiled_shutdown (
    struct kan_resource_material_pipeline_compiled_t *instance);

/// \brief Entry of pipeline-for-pass array in compiled material.
struct kan_resource_material_pass_compiled_t
{
    kan_interned_string_t name;
    kan_interned_string_t pipeline;
};

/// \brief Compiled material resource, that references its pipeline family and
///        lists supported passes with their pipelines.
struct kan_resource_material_compiled_t
{
    kan_interned_string_t pipeline_family;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_pass_compiled_t)
    struct kan_dynamic_array_t passes;
};

RESOURCE_MATERIAL_API void kan_resource_material_compiled_init (struct kan_resource_material_compiled_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_compiled_shutdown (struct kan_resource_material_compiled_t *instance);

KAN_C_HEADER_END
