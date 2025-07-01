#pragma once

#include <resource_material_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/context/render_backend_system.h>
#include <kan/error/critical.h>
#include <kan/reflection/markup.h>
#include <kan/render_pipeline_language/compiler.h>
#include <kan/resource_material/resource_rpl_source.h>

/// \file
/// \brief This file stores various resource types needed to properly store, compile and use render passes.
///
/// \par Overview
/// \parblock
/// Render pass has the information needed for creation of render backend pass along with pass variants. Pass variant
/// is a combination of specific additional sources and compilation options with KAN_RPL_OPTION_SCOPE_INSTANCE scope,
/// that need to be inserted into material code while compiling this particular pass variant. One pass can have multiple
/// variants as passes might need to draw the same geometry several times with different pipelines, for example to
/// implement outline or some other complex effect. As a result, every pass variant has its unique layout, which is
/// described in compiled variant resource.
///
/// Also, tag requirement system is created for passes in order to avoid compiling anything for passes that are not
/// supported by target platform. It is also useful for excluding development-only passes like editor and debug
/// passes, because we don't need them in final build.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Describes one render pass variant with its unique sources and instance options.
struct kan_resource_render_pass_variant_description_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t sources;

    /// \invariant Must only contain KAN_RPL_OPTION_SCOPE_INSTANCE options, otherwise compilation will fail.
    struct kan_resource_rpl_options_t instance_options;

    /// \brief List of stages that are explicitly disabled in this variant for optimization.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (enum kan_rpl_pipeline_stage_t)
    struct kan_dynamic_array_t disabled_stages;

    /// \brief List of tags that must be present in `kan_resource_material_pass_t::tags`
    ///        in order for this variant to be enabled.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t required_tags;
};

RESOURCE_MATERIAL_API void kan_resource_render_pass_variant_description_init (
    struct kan_resource_render_pass_variant_description_t *instance);

RESOURCE_MATERIAL_API void kan_resource_render_pass_variant_description_shutdown (
    struct kan_resource_render_pass_variant_description_t *instance);

/// \brief Represents data structure of resource that describes render pass.
struct kan_resource_render_pass_t
{
    /// \brief Render pass type.
    enum kan_render_pass_type_t type;

    /// \brief Tags that must present in `kan_resource_material_platform_configuration_t::supported_tags` in order to
    ///        include this render pass into resource compilation.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t required_tags;

    /// \brief List of render pass attachments and their descriptions.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_pass_attachment_t)
    struct kan_dynamic_array_t attachments;

    /// \brief List of variants for render pass.
    /// \details Can be left empty if render pass has only one variant without custom sources or options.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_render_pass_variant_description_t)
    struct kan_dynamic_array_t variants;
};

RESOURCE_MATERIAL_API void kan_resource_render_pass_init (struct kan_resource_render_pass_t *instance);

RESOURCE_MATERIAL_API void kan_resource_render_pass_shutdown (struct kan_resource_render_pass_t *instance);

/// \brief Compiled byproduct resource that contains information about bindings for particular render pass variant.
struct kan_resource_render_pass_variant_compiled_t
{
    struct kan_rpl_meta_set_bindings_t pass_set_bindings;
};

RESOURCE_MATERIAL_API void kan_resource_render_pass_variant_compiled_init (
    struct kan_resource_render_pass_variant_compiled_t *instance);

RESOURCE_MATERIAL_API void kan_resource_render_pass_variant_compiled_shutdown (
    struct kan_resource_render_pass_variant_compiled_t *instance);

/// \brief Represents compiled data structure of resource that describes render pass.
struct kan_resource_render_pass_compiled_t
{
    /// \brief Whether pass was considered supported at the time of compilation.
    /// \details If pass is not supported, all other data is uninitialized as compilation didn't happen.
    bool supported;

    /// \brief Render pass type.
    KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (supported)
    KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (true)
    enum kan_render_pass_type_t type;

    /// \brief List of render pass attachments and their descriptions.
    KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (supported)
    KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (true)
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_pass_attachment_t)
    struct kan_dynamic_array_t attachments;

    /// \brief List of render pass variant resource names if pass has customized variants.
    KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (supported)
    KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (true)
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t variants;
};

RESOURCE_MATERIAL_API void kan_resource_render_pass_compiled_init (
    struct kan_resource_render_pass_compiled_t *instance);

RESOURCE_MATERIAL_API void kan_resource_render_pass_compiled_shutdown (
    struct kan_resource_render_pass_compiled_t *instance);

KAN_C_HEADER_END
