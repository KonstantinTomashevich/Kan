#pragma once

#include <resource_material_api.h>

#include <kan/api_common/core_types.h>
#include <kan/api_common/mute_warnings.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/context/render_backend_system.h>
#include <kan/error/critical.h>
#include <kan/inline_math/inline_math.h>
#include <kan/reflection/markup.h>
#include <kan/render_pipeline_language/compiler.h>

/// \file
/// \brief This file stores various resource types needed to properly store, compile and use material instance.
///
/// \par Overview
/// \parblock
/// Material instance is a container of parameters for material parameter set and for instanced attributes.
/// Also, material instances support inheritance.
///
/// When compiled, material instance removes all inheritance by merging all the data. Also, everything except for
/// instanced attributes is separated into static data, which can be shared between several instances. The rule is that
/// if material instance only has instanced attributes, it just reuses static data from parent (if any). It makes it
/// easier to avoid allocating unneeded data for material instances in runtime: parameter set and buffers only need
/// to be allocated once per static data, not per material instance.
/// \endparblock
///
/// \par Tails
/// \parblock
/// Buffer tails from render pipeline language is a useful tool for instancing: if we have several subsets of parameters
/// for material instance, they can be stored as tail parameters and then instanced parameter with tail index would be
/// used to select appropriate subset. If this approach is used, we would still have one material instance parameter set
/// and therefore we would be able to batch lots of draws.
///
/// Keep in mind, that every buffer except for attribute buffers is allowed to have tail, therefore it is possible to
/// have several tails inside one material and to select each one independently through independent index.
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
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_I1)
        kan_serialized_offset_t value_i1;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_I2)
        struct kan_integer_vector_2_t value_i2;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_I3)
        struct kan_integer_vector_3_t value_i3;

        KAN_REFLECTION_VISIBILITY_CONDITION_FIELD (type)
        KAN_REFLECTION_VISIBILITY_CONDITION_VALUE (KAN_RPL_META_VARIABLE_TYPE_I4)
        struct kan_integer_vector_4_t value_i4;

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

RESOURCE_MATERIAL_API void kan_resource_material_tail_set_init (struct kan_resource_material_tail_set_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_tail_set_shutdown (struct kan_resource_material_tail_set_t *instance);

/// \brief Data structure for appending new tail item with given parameters.
struct kan_resource_material_tail_append_t
{
    /// \brief Corresponds to `kan_rpl_meta_buffer_t::tail_name`.
    kan_interned_string_t tail_name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_parameter_t)
    struct kan_dynamic_array_t parameters;
};

RESOURCE_MATERIAL_API void kan_resource_material_tail_append_init (
    struct kan_resource_material_tail_append_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_tail_append_shutdown (
    struct kan_resource_material_tail_append_t *instance);

/// \brief Data structure for configuring texture-to-image binding in material.
struct kan_resource_material_image_t
{
    /// \brief Corresponds to `kan_rpl_meta_sampler_t::name`.
    kan_interned_string_t name;

    /// \brief Name of the texture resource.
    kan_interned_string_t texture;

    /// \brief Sampling configuration.
    struct kan_render_sampler_t sampler;
};

RESOURCE_MATERIAL_API void kan_resource_material_image_init (struct kan_resource_material_image_t *instance);

/// \brief Describes material instance resource.
struct kan_resource_material_instance_t
{
    /// \brief Material resource name. If has parent, material resource names should be equal here and in parent.
    kan_interned_string_t material;

    /// \brief Name of the parent material instance if any.
    kan_interned_string_t parent;

    /// \brief Array of parameters for instance attribute buffers.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_parameter_t)
    struct kan_dynamic_array_t instanced_parameters;

    /// \brief Array of parameters for material set buffers.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_parameter_t)
    struct kan_dynamic_array_t parameters;

    /// \brief Array of tail item sets (parameter write at particular index).
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_tail_set_t)
    struct kan_dynamic_array_t tail_set;

    /// \brief Array of tail item appends.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_tail_append_t)
    struct kan_dynamic_array_t tail_append;

    /// \brief Array of texture selections for image slots.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_image_t)
    struct kan_dynamic_array_t images;
};

RESOURCE_MATERIAL_API void kan_resource_material_instance_init (struct kan_resource_material_instance_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_instance_shutdown (struct kan_resource_material_instance_t *instance);

/// \brief Contains compiled material data that can be shared between multiple materials
///        if only difference between these materials is instanced parameters.
struct kan_resource_material_instance_static_compiled_t
{
    kan_interned_string_t material;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_parameter_t)
    struct kan_dynamic_array_t parameters;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_tail_set_t)
    struct kan_dynamic_array_t tail_set;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_tail_append_t)
    struct kan_dynamic_array_t tail_append;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_image_t)
    struct kan_dynamic_array_t images;
};

RESOURCE_MATERIAL_API void kan_resource_material_instance_static_compiled_init (
    struct kan_resource_material_instance_static_compiled_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_instance_static_compiled_shutdown (
    struct kan_resource_material_instance_static_compiled_t *instance);

/// \brief Contains compiled material instance data.
struct kan_resource_material_instance_compiled_t
{
    kan_interned_string_t static_data;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_parameter_t)
    struct kan_dynamic_array_t instanced_parameters;
};

RESOURCE_MATERIAL_API void kan_resource_material_instance_compiled_init (
    struct kan_resource_material_instance_compiled_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_instance_compiled_shutdown (
    struct kan_resource_material_instance_compiled_t *instance);

KAN_C_HEADER_END
