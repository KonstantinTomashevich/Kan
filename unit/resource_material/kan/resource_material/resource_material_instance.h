#pragma once

#include <resource_material_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/context/render_backend_system.h>
#include <kan/error/critical.h>
#include <kan/inline_math/inline_math.h>
#include <kan/reflection/markup.h>
#include <kan/render_pipeline_language/compiler.h>

// TODO: Docs.

KAN_C_HEADER_BEGIN

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

struct kan_resource_material_tail_set_t
{
    kan_interned_string_t tail_name;
    kan_serialized_size_t index;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_parameter_t)
    struct kan_dynamic_array_t parameters;
};

RESOURCE_MATERIAL_API void kan_resource_material_tail_set_init (struct kan_resource_material_tail_set_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_tail_set_shutdown (struct kan_resource_material_tail_set_t *instance);

struct kan_resource_material_tail_append_t
{
    kan_interned_string_t tail_name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_parameter_t)
    struct kan_dynamic_array_t parameters;
};

RESOURCE_MATERIAL_API void kan_resource_material_tail_append_init (
    struct kan_resource_material_tail_append_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_tail_append_shutdown (
    struct kan_resource_material_tail_append_t *instance);

struct kan_resource_material_image_t
{
    kan_interned_string_t name;
    kan_interned_string_t texture;
    struct kan_render_sampler_t sampler;
};

struct kan_resource_material_instance_t
{
    kan_interned_string_t material;
    kan_interned_string_t parent;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_parameter_t)
    struct kan_dynamic_array_t instanced_parameters;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_parameter_t)
    struct kan_dynamic_array_t parameters;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_tail_set_t)
    struct kan_dynamic_array_t tail_set;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_tail_append_t)
    struct kan_dynamic_array_t tail_append;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_image_t)
    struct kan_dynamic_array_t images;
};

RESOURCE_MATERIAL_API void kan_resource_material_instance_init (struct kan_resource_material_instance_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_instance_shutdown (struct kan_resource_material_instance_t *instance);

struct kan_resource_material_instance_static_compiled_t
{
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

struct kan_resource_material_instance_compiled_t
{
    kan_interned_string_t material;
    kan_interned_string_t static_data;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_parameter_t)
    struct kan_dynamic_array_t instanced_parameters;
};

RESOURCE_MATERIAL_API void kan_resource_material_instance_compiled_init (
    struct kan_resource_material_instance_compiled_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_instance_compiled_shutdown (
    struct kan_resource_material_instance_compiled_t *instance);

KAN_C_HEADER_END
