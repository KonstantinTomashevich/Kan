#pragma once

#include <resource_render_foundation_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/context/render_backend_system.h>
#include <kan/reflection/markup.h>
#include <kan/render_pipeline_language/compiler.h>

/// \file
/// \brief This file stores runtime representation of render material resource.
///
/// \par Overview
/// \parblock
/// Material is a set of GPU pipelines for various supported render passes. Full code and meta for every pipeline
/// for target platform is packed into material resource as in most cases we need to have all of them available at once.
/// Also, it is a good practice to precache all the pipelines during game startup to avoid spikes.
///
/// Keep in mind, that material compilation mostly consists of compiling source code into intermediate platform specific
/// code format, like SPIRV for Vulkan. When creating pipeline in runtime, its code would still need to be compiled from
/// intermediate format to vendor-specific format, which might take some time on CPU during the first load (and should
/// be mitigated by driver pipeline cache for next loads).
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Describes single compiled pipelines for particular variant of particular pass with its specific meta.
/// \details Parts of meta that are stored in pipelines are pipeline-specific.
struct kan_resource_material_pipeline_t
{
    kan_interned_string_t pass_name;
    kan_interned_string_t variant_name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_entry_point_t)
    struct kan_dynamic_array_t entry_points;

    enum kan_render_code_format_t code_format;

    struct kan_rpl_graphics_classic_pipeline_settings_t pipeline_settings;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_meta_color_output_t)
    struct kan_dynamic_array_t color_outputs;

    struct kan_rpl_color_blend_constants_t color_blend_constants;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (uint8_t)
    struct kan_dynamic_array_t code;
};

RESOURCE_RENDER_FOUNDATION_API void kan_resource_material_pipeline_init (
    struct kan_resource_material_pipeline_t *instance);

RESOURCE_RENDER_FOUNDATION_API void kan_resource_material_pipeline_shutdown (
    struct kan_resource_material_pipeline_t *instance);

/// \brief Describes material resource with all its pipelines for the target platform.
/// \details Parts of meta that are stored in material are common for all pipelines.
struct kan_resource_material_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_meta_attribute_source_t)
    struct kan_dynamic_array_t vertex_attribute_sources;

    bool has_instanced_attribute_source;

    KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (has_instanced_attribute_source)
    KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (true)
    struct kan_rpl_meta_attribute_source_t instanced_attribute_source;

    kan_instance_size_t push_constant_size;

    struct kan_rpl_meta_set_bindings_t set_material;
    struct kan_rpl_meta_set_bindings_t set_object;
    struct kan_rpl_meta_set_bindings_t set_shared;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_pipeline_t)
    struct kan_dynamic_array_t pipelines;
};

RESOURCE_RENDER_FOUNDATION_API void kan_resource_material_init (struct kan_resource_material_t *instance);

RESOURCE_RENDER_FOUNDATION_API void kan_resource_material_shutdown (struct kan_resource_material_t *instance);

KAN_C_HEADER_END
