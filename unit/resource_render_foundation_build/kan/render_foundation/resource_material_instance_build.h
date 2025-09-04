#pragma once

#include <resource_render_foundation_build_api.h>

#include <kan/api_common/core_types.h>
#include <kan/api_common/mute_warnings.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/context/render_backend_system.h>
#include <kan/error/critical.h>
#include <kan/inline_math/inline_math.h>
#include <kan/reflection/markup.h>
#include <kan/render_foundation/resource_material_instance.h>
#include <kan/render_pipeline_language/compiler.h>

/// \file
/// \brief This file stores data structures for defining material instances to be built for runtime usage.
///
/// \par Overview
/// \parblock
/// To define a new material instance to be built for usage, `kan_resource_material_instance_raw_t` resource should be 
/// used. It describes all the parameters for material instance: both material set parameters and instanced variants
/// with their attributes as parameters. It also supports inheritance: material instance can inherit data from other
/// material instance and apply its values on top of it. Variants are also inherited and updated by their names.
///
/// Buffers with tail parameters from render pipeline language are supported through `kan_resource_material_tail_set_t`
/// and `kan_resource_material_tail_append_t` structures that are used to set data in particular tail of  particular 
/// buffer at some index or append new item to the tail.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Describes one material instance parameter.
KAN_MUTE_STRUCTURE_PADDED_WARNINGS_BEGIN
struct kan_resource_material_parameter_t
{
    kan_interned_string_t name;
    enum kan_rpl_meta_variable_type_t type;

    union
    {
        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_F1)
        float value_f1;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_F2)
        struct kan_float_vector_2_t value_f2;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_F3)
        struct kan_float_vector_3_t value_f3;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_F4)
        struct kan_float_vector_4_t value_f4;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_U1)
        kan_serialized_size_t value_u1;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_U2)
        struct kan_unsigned_integer_vector_2_t value_u2;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_U3)
        struct kan_unsigned_integer_vector_3_t value_u3;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_U4)
        struct kan_unsigned_integer_vector_4_t value_u4;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_S1)
        kan_serialized_offset_t value_s1;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_S2)
        struct kan_integer_vector_2_t value_s2;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_S3)
        struct kan_integer_vector_3_t value_s3;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_S4)
        struct kan_integer_vector_4_t value_s4;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_F3X3)
        struct kan_float_matrix_3x3_t value_f3x3;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_F4X4)
        struct kan_float_matrix_4x4_t value_f4x4;
    };
};
KAN_MUTE_STRUCTURE_PADDED_WARNINGS_END

/// \brief Data structure for setting tail item parameters at particular index.
struct kan_resource_material_tail_set_t
{
    /// \brief Corresponds to `kan_rpl_meta_buffer_t::tail_name`.
    kan_interned_string_t tail_name;

    kan_serialized_size_t index;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_parameter_t)
    struct kan_dynamic_array_t parameters;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_tail_set_init (
    struct kan_resource_material_tail_set_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_tail_set_shutdown (
    struct kan_resource_material_tail_set_t *instance);

/// \brief Data structure for appending new tail item with given parameters.
struct kan_resource_material_tail_append_t
{
    /// \brief Corresponds to `kan_rpl_meta_buffer_t::tail_name`.
    kan_interned_string_t tail_name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_parameter_t)
    struct kan_dynamic_array_t parameters;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_tail_append_init (
    struct kan_resource_material_tail_append_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_tail_append_shutdown (
    struct kan_resource_material_tail_append_t *instance);

/// \brief Data structure for configuring sampler binding in material.
struct kan_resource_material_sampler_t
{
    /// \brief Corresponds to `kan_rpl_meta_sampler_t::name`.
    kan_interned_string_t name;

    /// \brief Sampling configuration.
    struct kan_render_sampler_t sampler;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_sampler_init (
    struct kan_resource_material_sampler_t *instance);

/// \brief Data structure for configuring texture-to-image binding in material.
struct kan_resource_material_image_t
{
    /// \brief Corresponds to `kan_rpl_meta_sampler_t::name`.
    kan_interned_string_t name;

    /// \brief Name of the texture resource.
    kan_interned_string_t texture;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_image_init (
    struct kan_resource_material_image_t *instance);

/// \brief Data structure for configuring named material instance variant with instanced data.
struct kan_resource_material_variant_raw_t
{
    kan_interned_string_t name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_parameter_t)
    struct kan_dynamic_array_t parameters;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_variant_raw_init (
    struct kan_resource_material_variant_raw_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_variant_raw_shutdown (
    struct kan_resource_material_variant_raw_t *instance);

/// \brief Describes material instance resource.
struct kan_resource_material_instance_raw_t
{
    /// \brief Material resource name. If has parent, should be equal to parent material name.
    kan_interned_string_t material;

    /// \brief Name of the parent material instance if any.
    kan_interned_string_t parent;

    /// \brief Array of parameters for material set buffers.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_parameter_t)
    struct kan_dynamic_array_t parameters;

    /// \brief Array of tail item sets (parameter write at particular index).
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_tail_set_t)
    struct kan_dynamic_array_t tail_set;

    /// \brief Array of tail item appends.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_tail_append_t)
    struct kan_dynamic_array_t tail_append;

    /// \brief Array of sampler configurations.
    /// \warning Sampler configuration for particular name fully overrides parent sampler configuration.
    ///          Therefore, material must specify full sampler configuration, not only differences from parent sampler.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_sampler_t)
    struct kan_dynamic_array_t samplers;

    /// \brief Array of texture selections for image slots.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_image_t)
    struct kan_dynamic_array_t images;

    /// \brief Instanced data variants for this material instance if any.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_variant_raw_t)
    struct kan_dynamic_array_t variants;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_instance_raw_init (
    struct kan_resource_material_instance_raw_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_material_instance_raw_shutdown (
    struct kan_resource_material_instance_raw_t *instance);

KAN_C_HEADER_END
