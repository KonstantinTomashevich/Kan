#pragma once

#include <resource_render_foundation_api.h>

#include <kan/api_common/core_types.h>
#include <kan/api_common/mute_warnings.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/context/render_backend_system.h>
#include <kan/reflection/markup.h>
#include <kan/render_pipeline_language/compiler.h>

/// \file
/// \brief This file stores runtime representation of render material instance resource.
///
/// \par Overview
/// \parblock
/// Material instance is a container of parameters for material parameter set and for instanced attributes.
/// In runtime format all parameters are already baked into buffers that can be directly uploaded to GPU,
/// so no additional logic at runtime is needed.
///
/// Instanced attributes are handled through variant system. If material supports instanced attributes, any amount of
/// variants can be added to material instance where variant is a set of attribute values baked into buffer of
/// appropriate size the same way as regular parameters baked into regular buffers.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Buffers should be aligned to 4x4 float matrix rows for easier usage.
#define KAN_RESOURCE_RENDER_FOUNDATION_BUFFER_ALIGNMENT 16u

/// \brief Defines data for material set buffer binding point.
struct kan_resource_buffer_binding_t
{
    kan_rpl_size_t binding;
    enum kan_rpl_buffer_type_t type;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (uint8_t)
    struct kan_dynamic_array_t data;
};

RESOURCE_RENDER_FOUNDATION_API void kan_resource_buffer_binding_init (struct kan_resource_buffer_binding_t *instance);

RESOURCE_RENDER_FOUNDATION_API void kan_resource_buffer_binding_shutdown (
    struct kan_resource_buffer_binding_t *instance);

/// \brief Defines data for material set sampler binding point.
struct kan_resource_sampler_binding_t
{
    kan_rpl_size_t binding;
    struct kan_render_sampler_t sampler;
};

/// \brief Defines data for material set image binding point.
struct kan_resource_image_binding_t
{
    kan_rpl_size_t binding;
    kan_interned_string_t texture;
};

/// \brief Defines data for material instance variant.
struct kan_resource_material_variant_t
{
    kan_interned_string_t name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (uint8_t)
    struct kan_dynamic_array_t instanced_data;
};

RESOURCE_RENDER_FOUNDATION_API void kan_resource_material_variant_init (
    struct kan_resource_material_variant_t *instance);

RESOURCE_RENDER_FOUNDATION_API void kan_resource_material_variant_shutdown (
    struct kan_resource_material_variant_t *instance);

/// \brief Describes runtime material instance resource with all its data.
struct kan_resource_material_instance_t
{
    kan_interned_string_t material;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_buffer_binding_t)
    struct kan_dynamic_array_t buffers;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_sampler_binding_t)
    struct kan_dynamic_array_t samplers;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_image_binding_t)
    struct kan_dynamic_array_t images;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_variant_t)
    struct kan_dynamic_array_t variants;
};

RESOURCE_RENDER_FOUNDATION_API void kan_resource_material_instance_init (
    struct kan_resource_material_instance_t *instance);

RESOURCE_RENDER_FOUNDATION_API void kan_resource_material_instance_shutdown (
    struct kan_resource_material_instance_t *instance);

KAN_C_HEADER_END
