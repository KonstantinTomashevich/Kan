#pragma once

#include <resource_material_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/context/render_backend_system.h>
#include <kan/error/critical.h>
#include <kan/reflection/markup.h>
#include <kan/render_pipeline_language/compiler.h>

/// \file
/// \brief This file stores various resource types needed to properly store, compile and use render passes.
///
/// \par Overview
/// \parblock
/// Render pass has the information needed for creation of render backend pass along with pass parameter set layout
/// source file in render pipeline language. Pass parameter set source file is automatically included into pipelines
/// that are compiled for this pass in order to provide proper sets for the pipelines.
///
/// When compiled, pass stored meta about its bindings instead of the link to pass set source. Other data is just copied
/// during compilation.
///
/// Also, tag requirement system is created for passes in order to avoid compiling anything for passes that are not
/// supported by target platform. It is also useful for excluding development-only passes like editor and debug
/// passes, because we don't need them in final build.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Represents data structure of resource that describes render pass.
struct kan_resource_render_pass_t
{
    /// \brief Render pass type.
    enum kan_render_pass_type_t type;

    /// \brief Tags that must present in `kan_resource_material_platform_configuration_t::supported_tags` in order to
    ///        include this render pass into resource compilation.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t required_tags;

    /// \brief Name of the render pipeline language source file that contains pass parameter set layout.
    /// \details If NULL, then pass has no special parameters.
    kan_interned_string_t pass_set_source;

    /// \brief List of render pass attachments and their descriptions.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_pass_attachment_t)
    struct kan_dynamic_array_t attachments;
};

RESOURCE_MATERIAL_API void kan_resource_render_pass_init (struct kan_resource_render_pass_t *instance);

RESOURCE_MATERIAL_API void kan_resource_render_pass_shutdown (struct kan_resource_render_pass_t *instance);

/// \brief Represents compiled data structure of resource that describes render pass.
struct kan_resource_render_pass_compiled_t
{
    /// \brief Whether pass was considered supported at the time of compilation.
    /// \details If pass is not supported, all other data is uninitialized as compilation didn't happen.
    kan_bool_t supported;

    /// \brief Bindings for pass parameter set layout.
    KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (supported)
    KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_TRUE)
    struct kan_rpl_meta_set_bindings_t pass_set_bindings;

    /// \brief Render pass type.
    KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (supported)
    KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_TRUE)
    enum kan_render_pass_type_t type;

    /// \brief List of render pass attachments and their descriptions.
    KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (supported)
    KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_TRUE)
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_render_pass_attachment_t)
    struct kan_dynamic_array_t attachments;
};

RESOURCE_MATERIAL_API void kan_resource_render_pass_compiled_init (
    struct kan_resource_render_pass_compiled_t *instance);

RESOURCE_MATERIAL_API void kan_resource_render_pass_compiled_shutdown (
    struct kan_resource_render_pass_compiled_t *instance);

KAN_C_HEADER_END
