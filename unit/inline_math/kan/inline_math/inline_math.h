#pragma once

#include <kan/api_common/core_types.h>
#include <kan/api_common/mute_warnings.h>

KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#define CGLM_CLIPSPACE_INCLUDE_ALL
#define CGLM_FORCE_LEFT_HANDED
#include <cglm/cglm.h>
KAN_MUTE_THIRD_PARTY_WARNINGS_END

#include <kan/api_common/c_header.h>

/// \file
/// \brief Contains common math types and functions.
/// \details Color-related math is also here, because it is also widely used.

KAN_C_HEADER_BEGIN

#define KAN_PI CGLM_PI
#define KAN_PI_2 CGLM_PI_2

/// \brief 2 dimensional unsigned integer vector type.
struct kan_unsigned_integer_vector_2_t
{
    kan_serialized_size_t x;
    kan_serialized_size_t y;
};

/// \brief 3 dimensional unsigned integer vector type.
struct kan_unsigned_integer_vector_3_t
{
    kan_serialized_size_t x;
    kan_serialized_size_t y;
    kan_serialized_size_t z;
};

/// \brief 4 dimensional unsigned integer vector type.
struct kan_unsigned_integer_vector_4_t
{
    kan_serialized_size_t x;
    kan_serialized_size_t y;
    kan_serialized_size_t z;
    kan_serialized_size_t w;
};

/// \brief 2 dimensional integer vector type.
struct kan_integer_vector_2_t
{
    kan_serialized_offset_t x;
    kan_serialized_offset_t y;
};

_Static_assert (sizeof (struct kan_integer_vector_2_t) == sizeof (ivec2), "Size validation.");
_Static_assert (_Alignof (struct kan_integer_vector_2_t) == _Alignof (ivec2), "Alignment validation.");

/// \brief 3 dimensional integer vector type.
struct kan_integer_vector_3_t
{
    kan_serialized_offset_t x;
    kan_serialized_offset_t y;
    kan_serialized_offset_t z;
};

_Static_assert (sizeof (struct kan_integer_vector_3_t) == sizeof (ivec3), "Size validation.");
_Static_assert (_Alignof (struct kan_integer_vector_3_t) == _Alignof (ivec3), "Alignment validation.");

/// \brief 4 dimensional integer vector type.
struct kan_integer_vector_4_t
{
    kan_serialized_offset_t x;
    kan_serialized_offset_t y;
    kan_serialized_offset_t z;
    kan_serialized_offset_t w;
};

_Static_assert (sizeof (struct kan_integer_vector_4_t) == sizeof (ivec4), "Size validation.");
_Static_assert (_Alignof (struct kan_integer_vector_4_t) == _Alignof (ivec4), "Alignment validation.");

/// \brief 2 dimensional floating point vector type.
struct kan_float_vector_2_t
{
    float x;
    float y;
};

_Static_assert (sizeof (struct kan_float_vector_2_t) == sizeof (vec2), "Size validation.");
_Static_assert (_Alignof (struct kan_float_vector_2_t) == _Alignof (vec2), "Alignment validation.");

/// \brief 3 dimensional floating point vector type.
struct kan_float_vector_3_t
{
    float x;
    float y;
    float z;
};

_Static_assert (sizeof (struct kan_float_vector_3_t) == sizeof (vec3), "Size validation.");
_Static_assert (_Alignof (struct kan_float_vector_3_t) == _Alignof (vec3), "Alignment validation.");

/// \brief 4 dimensional floating point vector type.
#if defined(_MSC_VER)
CGLM_ALIGN (16)
#endif
struct kan_float_vector_4_t
{
    float x;
    float y;
    float z;
    float w;
}
#if !defined(_MSC_VER)
CGLM_ALIGN (16)
#endif
    ;

_Static_assert (sizeof (struct kan_float_vector_4_t) == sizeof (vec4), "Size validation.");
_Static_assert (_Alignof (struct kan_float_vector_4_t) == _Alignof (vec4), "Alignment validation.");

/// \brief 3x3 floating point matrix type.
struct kan_float_matrix_3x3_t
{
    struct kan_float_vector_3_t row_0;
    struct kan_float_vector_3_t row_1;
    struct kan_float_vector_3_t row_2;
};

_Static_assert (sizeof (struct kan_float_matrix_3x3_t) == sizeof (mat3), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_3x3_t) == _Alignof (mat3), "Alignment validation.");

/// \brief 3x2 floating point matrix type.
struct kan_float_matrix_3x2_t
{
    struct kan_float_vector_2_t row_0;
    struct kan_float_vector_2_t row_1;
    struct kan_float_vector_2_t row_2;
};

_Static_assert (sizeof (struct kan_float_matrix_3x2_t) == sizeof (mat3x2), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_3x2_t) == _Alignof (mat3x2), "Alignment validation.");

/// \brief 3x4 floating point matrix type.
struct kan_float_matrix_3x4_t
{
    struct kan_float_vector_4_t row_0;
    struct kan_float_vector_4_t row_1;
    struct kan_float_vector_4_t row_2;
};

_Static_assert (sizeof (struct kan_float_matrix_3x4_t) == sizeof (mat3x4), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_3x4_t) == _Alignof (mat3x4), "Alignment validation.");

/// \brief 2x2 floating point matrix type.
#if defined(_MSC_VER)
CGLM_ALIGN (16)
#endif
struct kan_float_matrix_2x2_t
{
    struct kan_float_vector_2_t row_0;
    struct kan_float_vector_2_t row_1;
}
#if !defined(_MSC_VER)
CGLM_ALIGN (16)
#endif
    ;

_Static_assert (sizeof (struct kan_float_matrix_2x2_t) == sizeof (mat2), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_2x2_t) == _Alignof (mat2), "Alignment validation.");

/// \brief 2x3 floating point matrix type.
struct kan_float_matrix_2x3_t
{
    struct kan_float_vector_3_t row_0;
    struct kan_float_vector_3_t row_1;
};

_Static_assert (sizeof (struct kan_float_matrix_2x3_t) == sizeof (mat2x3), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_2x3_t) == _Alignof (mat2x3), "Alignment validation.");

/// \brief 2x4 floating point matrix type.
struct kan_float_matrix_2x4_t
{
    struct kan_float_vector_4_t row_0;
    struct kan_float_vector_4_t row_1;
};

_Static_assert (sizeof (struct kan_float_matrix_2x4_t) == sizeof (mat2x4), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_2x4_t) == _Alignof (mat2x4), "Alignment validation.");

/// \brief 4x4 floating point matrix type.
#if defined(_MSC_VER)
CGLM_ALIGN_MAT
#endif
struct kan_float_matrix_4x4_t
{
    struct kan_float_vector_4_t row_0;
    struct kan_float_vector_4_t row_1;
    struct kan_float_vector_4_t row_2;
    struct kan_float_vector_4_t row_3;
}
#if !defined(_MSC_VER)
CGLM_ALIGN (16)
#endif
    ;

_Static_assert (sizeof (struct kan_float_matrix_4x4_t) == sizeof (mat4), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_4x4_t) == _Alignof (mat4), "Alignment validation.");

/// \brief 4x2 floating point matrix type.
struct kan_float_matrix_4x2_t
{
    struct kan_float_vector_2_t row_0;
    struct kan_float_vector_2_t row_1;
    struct kan_float_vector_2_t row_2;
    struct kan_float_vector_2_t row_3;
};

_Static_assert (sizeof (struct kan_float_matrix_4x2_t) == sizeof (mat4x2), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_4x2_t) == _Alignof (mat4x2), "Alignment validation.");

/// \brief 4x3 floating point matrix type.
struct kan_float_matrix_4x3_t
{
    struct kan_float_vector_3_t row_0;
    struct kan_float_vector_3_t row_1;
    struct kan_float_vector_3_t row_2;
    struct kan_float_vector_3_t row_3;
};

_Static_assert (sizeof (struct kan_float_matrix_4x3_t) == sizeof (mat4x3), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_4x3_t) == _Alignof (mat4x3), "Alignment validation.");

/// \brief Structure for storing transform in 2d.
struct kan_transform_2_t
{
    struct kan_float_vector_2_t location;
    float rotation;
    struct kan_float_vector_2_t scale;
};

/// \brief Structure for storing transform in 3d.
struct kan_transform_3_t
{
    struct kan_float_vector_4_t rotation;
    struct kan_float_vector_3_t location;
    struct kan_float_vector_3_t scale;
};

/// \brief Convenience constructor function for kan_integer_vector_2_t.
static inline struct kan_integer_vector_2_t kan_make_integer_vector_2_t (kan_serialized_offset_t x,
                                                                         kan_serialized_offset_t y)
{
    return (struct kan_integer_vector_2_t) {.x = x, .y = y};
}

/// \brief Convenience constructor function for kan_integer_vector_3_t.
static inline struct kan_integer_vector_3_t kan_make_integer_vector_3_t (kan_serialized_offset_t x,
                                                                         kan_serialized_offset_t y,
                                                                         kan_serialized_offset_t z)
{
    return (struct kan_integer_vector_3_t) {.x = x, .y = y, .z = z};
}

/// \brief Convenience constructor function for kan_integer_vector_4_t.
static inline struct kan_integer_vector_4_t kan_make_integer_vector_4_t (kan_serialized_offset_t x,
                                                                         kan_serialized_offset_t y,
                                                                         kan_serialized_offset_t z,
                                                                         kan_serialized_offset_t w)
{
    return (struct kan_integer_vector_4_t) {.x = x, .y = y, .z = z, .w = w};
}

/// \brief Convenience constructor function for kan_float_vector_2_t.
static inline struct kan_float_vector_2_t kan_make_float_vector_2_t (float x, float y)
{
    return (struct kan_float_vector_2_t) {.x = x, .y = y};
}

/// \brief Convenience constructor function for kan_float_vector_3_t.
static inline struct kan_float_vector_3_t kan_make_float_vector_3_t (float x, float y, float z)
{
    return (struct kan_float_vector_3_t) {.x = x, .y = y, .z = z};
}

/// \brief Convenience constructor function for kan_float_vector_4_t.
static inline struct kan_float_vector_4_t kan_make_float_vector_4_t (float x, float y, float z, float w)
{
    struct kan_float_vector_4_t vector;
    vector.x = x;
    vector.y = y;
    vector.z = z;
    vector.w = w;
    return vector;
}

/// \brief Convenience constructor function for making kan_float_vector_4_t from kan_float_vector_3_t.
static inline struct kan_float_vector_4_t kan_extend_float_vector_3_t (const struct kan_float_vector_3_t vector,
                                                                       float w)
{
    return (struct kan_float_vector_4_t) {.x = vector.x, .y = vector.y, .z = vector.z, .w = w};
}

/// \brief Constructor for creating quaternions from euler angles passed as a vector.
static inline struct kan_float_vector_4_t kan_make_quaternion_from_euler_vector (struct kan_float_vector_3_t euler)
{
    struct kan_float_vector_4_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_euler_yxz_quat (&euler, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Constructor for creating quaternions from euler angles.
static inline struct kan_float_vector_4_t kan_make_quaternion_from_euler (float x, float y, float z)
{
    return kan_make_quaternion_from_euler_vector ((struct kan_float_vector_3_t) {x, y, z});
}

/// \brief Constructor for creating quaternion that represents difference from one vector to another.
static inline struct kan_float_vector_4_t kan_make_quaternion_from_vector_difference (
    const struct kan_float_vector_3_t *from, const struct kan_float_vector_3_t *to)
{
    struct kan_float_vector_4_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_quat_from_vecs (from, to, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}
/// \brief Linear interpolation for single floating point number.
static inline float kan_float_lerp (float left, float right, float alpha)
{
    return glm_lerp (left, right, alpha);
}

/// \brief Linear interpolation for kan_float_vector_2_t.
static inline struct kan_float_vector_2_t kan_float_vector_2_lerp (const struct kan_float_vector_2_t left,
                                                                   const struct kan_float_vector_2_t right,
                                                                   float alpha)
{
    struct kan_float_vector_2_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_vec2_lerp (&left, &right, alpha, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Linear interpolation for kan_float_vector_3_t.
static inline struct kan_float_vector_3_t kan_float_vector_3_lerp (const struct kan_float_vector_3_t left,
                                                                   const struct kan_float_vector_3_t right,
                                                                   float alpha)
{
    struct kan_float_vector_3_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_vec3_lerp (&left, &right, alpha, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Linear interpolation for kan_float_vector_4_t.
static inline struct kan_float_vector_4_t kan_float_vector_4_lerp (const struct kan_float_vector_4_t left,
                                                                   const struct kan_float_vector_4_t right,
                                                                   float alpha)
{
    struct kan_float_vector_4_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_vec4_lerp (&left, &right, alpha, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Returns normalized version of given non-zero-length vector.
static inline struct kan_float_vector_2_t kan_float_vector_2_normalized (const struct kan_float_vector_2_t vector)
{
    struct kan_float_vector_2_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_vec2_normalize_to (&vector, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Returns normalized version of given non-zero-length vector.
static inline struct kan_float_vector_3_t kan_float_vector_3_normalized (const struct kan_float_vector_3_t vector)
{
    struct kan_float_vector_3_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_vec3_normalize_to (&vector, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Returns normalized version of given non-zero-length vector.
static inline struct kan_float_vector_4_t kan_float_vector_4_normalized (const struct kan_float_vector_4_t vector)
{
    struct kan_float_vector_4_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_vec4_normalize_to (&vector, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Spherical linear interpolation for quaternions.
static inline struct kan_float_vector_4_t kan_float_vector_4_slerp (const struct kan_float_vector_4_t left,
                                                                    const struct kan_float_vector_4_t right,
                                                                    float alpha)
{
    struct kan_float_vector_4_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_quat_slerp (&left, &right, alpha, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Rotates given 3d vector using given quaternion.
static inline struct kan_float_vector_3_t kan_float_vector_3_rotate (const struct kan_float_vector_3_t vector,
                                                                     const struct kan_float_vector_4_t quaternion)
{
    struct kan_float_vector_3_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_quat_rotatev (&quaternion, &vector, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Creates identity 3x3 matrix.
static inline struct kan_float_matrix_3x3_t kan_float_matrix_3x3_get_identity (void)
{
    struct kan_float_matrix_3x3_t matrix;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_mat3_identity (&matrix);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return matrix;
}

/// \brief Multiplies two 3x3 matrices and stores result in output.
static inline struct kan_float_matrix_3x3_t kan_float_matrix_3x3_multiply (const struct kan_float_matrix_3x3_t *left,
                                                                           const struct kan_float_matrix_3x3_t *right)
{
    struct kan_float_matrix_3x3_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_mat3_mul (left, right, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Multiplies 3x3 matrix and vector.
static inline struct kan_float_vector_3_t kan_float_matrix_3x3_multiply_vector (
    const struct kan_float_matrix_3x3_t *left, const struct kan_float_vector_3_t right)
{
    struct kan_float_vector_3_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_mat3_mulv (left, &right, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Inverses given 3x3 matrix and stores result in output.
static inline struct kan_float_matrix_3x3_t kan_float_matrix_3x3_inverse (const struct kan_float_matrix_3x3_t *matrix)
{
    struct kan_float_matrix_3x3_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_mat3_inv (matrix, &result);
    return result;
}

/// \brief Creates identity 4x4 matrix.
static inline struct kan_float_matrix_4x4_t kan_float_matrix_4x4_get_identity (void)
{
    struct kan_float_matrix_4x4_t matrix;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_mat4_identity (&matrix);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return matrix;
}

/// \brief Multiplies two 4x4 matrices and stores result in output.
static inline struct kan_float_matrix_4x4_t kan_float_matrix_4x4_multiply (const struct kan_float_matrix_4x4_t *left,
                                                                           const struct kan_float_matrix_4x4_t *right)
{
    struct kan_float_matrix_4x4_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_mat4_mul (left, right, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Multiplies two 4x4 affine transform matrices and stores result in output.
static inline struct kan_float_matrix_4x4_t kan_float_matrix_4x4_multiply_for_transform (
    const struct kan_float_matrix_4x4_t *left, const struct kan_float_matrix_4x4_t *right)
{
    struct kan_float_matrix_4x4_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_mul (left, right, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Multiplies 4x4 matrix and vector;
static inline void kan_float_matrix_4x4_multiply_vector (const struct kan_float_matrix_4x4_t *left,
                                                         const struct kan_float_vector_4_t *right,
                                                         struct kan_float_vector_4_t *result)
{
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_mat4_mulv (left, right, result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
}

/// \brief Inverses given 4x4 matrix and stores result in output.
static inline struct kan_float_matrix_4x4_t kan_float_matrix_4x4_inverse (const struct kan_float_matrix_4x4_t *matrix)
{
    struct kan_float_matrix_4x4_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_mat4_inv (matrix, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Creates identity 2d transformation.
static inline struct kan_transform_2_t kan_transform_2_get_identity (void)
{
    return (struct kan_transform_2_t) {
        .location = {0.0f, 0.0f},
        .rotation = 0.0f,
        .scale = {1.0f, 1.0f},
    };
}

/// \brief Converts 2d transform to 3x3 matrix.
static inline struct kan_float_matrix_3x3_t kan_transform_2_to_float_matrix_3x3 (
    const struct kan_transform_2_t *transform)
{
    struct kan_float_matrix_3x3_t matrix;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_translate2d_make (&matrix, &transform->location);
    glm_rotate2d (&matrix, transform->rotation);
    glm_scale2d (&matrix, &transform->scale);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return matrix;
}

/// \brief Converts 3x3 matrix to 2d transform.
static inline struct kan_transform_2_t kan_float_matrix_3x3_to_transform_2 (const struct kan_float_matrix_3x3_t *matrix)
{
    return (struct kan_transform_2_t) {
        .location =
            {
                .x = matrix->row_2.x,
                .y = matrix->row_2.y,
            },
        .rotation = atan2f (matrix->row_0.y, matrix->row_0.x),
        .scale =
            {
                .x = sqrtf (matrix->row_0.x * matrix->row_0.x + matrix->row_0.y * matrix->row_0.y),
                .y = sqrtf (matrix->row_1.x * matrix->row_1.x + matrix->row_1.y * matrix->row_1.y),
            },
    };
}

/// \brief Creates identity 3d transformation.
static inline struct kan_transform_3_t kan_transform_3_get_identity (void)
{
    return (struct kan_transform_3_t) {
        .location = kan_make_float_vector_3_t (0.0f, 0.0f, 0.0f),
        .rotation = kan_make_float_vector_4_t (0.0f, 0.0f, 0.0f, 1.0f),
        .scale = kan_make_float_vector_3_t (1.0f, 1.0f, 1.0f),
    };
}

/// \brief Converts 3d transform to 4x4 matrix.
static inline struct kan_float_matrix_4x4_t kan_transform_3_to_float_matrix_4x4 (
    const struct kan_transform_3_t *transform)
{
    struct kan_float_matrix_4x4_t matrix;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    mat4 intermediate;
    glm_translate_make (intermediate, &transform->location);
    glm_quat_rotate (intermediate, &transform->rotation, &matrix);
    glm_scale (&matrix, &transform->scale);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return matrix;
}

/// \brief Converts 4x4 matrix to 3d transform.
static inline struct kan_transform_3_t kan_float_matrix_4x4_to_transform_3 (const struct kan_float_matrix_4x4_t *matrix)
{
    struct kan_transform_3_t transform;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    mat4 rotation_matrix;
    vec4 location_4;
    glm_decompose (matrix, location_4, rotation_matrix, &transform.scale);
    transform.location.x = location_4[0u];
    transform.location.y = location_4[1u];
    transform.location.z = location_4[2u];
    glm_mat4_quat (rotation_matrix, &transform.rotation);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return transform;
}

/// \brief Generation projection matrix for left-handed zero-one depth orthographic projection.
static inline struct kan_float_matrix_4x4_t kan_orthographic_projection (
    float left, float right, float bottom, float top, float near, float far)
{
    struct kan_float_matrix_4x4_t matrix;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_ortho_lh_zo (left, right, bottom, top, near, far, &matrix);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return matrix;
}

/// \brief Generation projection matrix for left-handed zero-one depth perspective projection.
static inline struct kan_float_matrix_4x4_t kan_perspective_projection (float field_of_view_y,
                                                                        float aspect_ratio,
                                                                        float near,
                                                                        float far)
{
    struct kan_float_matrix_4x4_t matrix;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_perspective_lh_zo (field_of_view_y, aspect_ratio, near, far, &matrix);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return matrix;
}

// TODO: Below we add used functions from cglm. We're planning to add them on-demand.

/// \brief Converts one channel value in [0, 1] interval from SRGB to RGB format by Vulkan specification.
static inline float kan_color_transfer_srgb_to_rgb (float value)
{
    return value <= 0.04045f ? value / 12.92f : powf ((value + 0.055f) / 1.055f, 2.4f);
}

/// \brief Converts one channel value in [0, 1] interval from RGB to SRGB format by Vulkan specification.
static inline float kan_color_transfer_rgb_to_srgb (float value)
{
    return value <= 0.0031308f ? value * 12.92f : 1.055f * powf (value, 1.0f / 2.4f) - 0.055f;
}

/// \brief Converts one channel value in [0, 1] interval from SRGB to RGB format with faster approximation.
static inline float kan_color_transfer_srgb_to_rgb_approximate (float value)
{
    return powf (value, 1.0f / 2.2f);
}

/// \brief Converts one channel value in [0, 1] interval from RGB to SRGB format with faster approximation.
static inline float kan_color_transfer_rgb_to_srgb_approximate (float value)
{
    return powf (value, 2.2f);
}

/// \brief Checks if sum of per-component differences of given 32-bit colors is greater than given tolerance.
static inline kan_bool_t kan_are_colors_different (uint32_t first, uint32_t second, uint32_t tolerance)
{
    int difference = 0;
#define CHECK(OFFSET)                                                                                                  \
    difference += abs ((int) ((first & (0xFF << OFFSET)) >> OFFSET) - (int) ((second & (0xFF << OFFSET)) >> OFFSET))
    CHECK (0u);
    CHECK (8u);
    CHECK (16u);
    CHECK (24u);
#undef CHECK
    return tolerance < (uint32_t) difference;
}

KAN_C_HEADER_END
