#pragma once

#include <resource_render_foundation_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/context/render_backend_system.h>
#include <kan/reflection/markup.h>
#include <kan/render_pipeline_language/compiler.h>

/// \file
/// \brief This file stores runtime representation of render pass resource.
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

/// \brief Contains information about bindings for particular render pass variant.
struct kan_resource_render_pass_variant_t
{
    kan_interned_string_t name;
    struct kan_rpl_meta_set_bindings_t pass_set_bindings;
};

RESOURCE_RENDER_FOUNDATION_API void kan_resource_render_pass_variant_init (
    struct kan_resource_render_pass_variant_t *instance);

RESOURCE_RENDER_FOUNDATION_API void kan_resource_render_pass_variant_shutdown (
    struct kan_resource_render_pass_variant_t *instance);

/// \brief Resource that contains information about render pass and its variants.
struct kan_resource_render_pass_t
{
    /// \brief Render pass type.
    enum kan_render_pass_type_t type;

    /// \brief List of render pass attachments and their descriptions.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_pass_attachment_t)
    struct kan_dynamic_array_t attachments;

    /// \brief List of render pass variants if pass has customized variants.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_render_pass_variant_t)
    struct kan_dynamic_array_t variants;
};

RESOURCE_RENDER_FOUNDATION_API void kan_resource_render_pass_init (struct kan_resource_render_pass_t *instance);

RESOURCE_RENDER_FOUNDATION_API void kan_resource_render_pass_shutdown (struct kan_resource_render_pass_t *instance);

KAN_C_HEADER_END
