#pragma once

#include <kan/api_common/mute_third_party_warnings.h>

KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#include <cglm/cglm.h>
KAN_MUTE_THIRD_PARTY_WARNINGS_END

#include <kan/api_common/c_header.h>

/// \file
/// \brief Contains common math types and functions.

KAN_C_HEADER_BEGIN

// \c_interface_scanner_disable

/// \brief Stub for forcing alignment.
struct CGLM_ALIGN (8) kan_math_align_as_8_t
{
    float stub;
};

/// \brief Stub for forcing alignment.
struct CGLM_ALIGN (16) kan_math_align_as_16_t
{
    float stub;
};

/// \brief Stub for forcing alignment.
struct CGLM_ALIGN_MAT kan_math_align_as_cglm_mat_t
{
    float stub;
};

// \c_interface_scanner_enable

#define KAN_PI CGLM_PI

/// \brief 2 dimensional integer vector type.
struct kan_integer_vector_2_t
{
    int x;
    int y;
};

_Static_assert (sizeof (struct kan_integer_vector_2_t) == sizeof (ivec2), "Size validation.");
_Static_assert (_Alignof (struct kan_integer_vector_2_t) == _Alignof (ivec2), "Alignment validation.");

/// \brief 3 dimensional integer vector type.
struct kan_integer_vector_3_t
{
    int x;
    int y;
    int z;
};

_Static_assert (sizeof (struct kan_integer_vector_3_t) == sizeof (ivec3), "Size validation.");
_Static_assert (_Alignof (struct kan_integer_vector_3_t) == _Alignof (ivec3), "Alignment validation.");

/// \brief 4 dimensional integer vector type.
struct kan_integer_vector_4_t
{
    int x;
    int y;
    int z;
    int w;
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
struct kan_float_vector_4_t
{
    float x;
    float y;
    float z;
    float w;

    // \c_interface_scanner_disable
    KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
    struct kan_math_align_as_16_t align_stub[0u];
    KAN_MUTE_THIRD_PARTY_WARNINGS_END
    // \c_interface_scanner_enable
};

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
struct kan_float_matrix_2x2_t
{
    struct kan_float_vector_2_t row_0;
    struct kan_float_vector_2_t row_1;

    // \c_interface_scanner_disable
    KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
    struct kan_math_align_as_16_t align_stub[0u];
    KAN_MUTE_THIRD_PARTY_WARNINGS_END
    // \c_interface_scanner_enable
};

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
struct kan_float_matrix_4x4_t
{
    struct kan_float_vector_4_t row_0;
    struct kan_float_vector_4_t row_1;
    struct kan_float_vector_4_t row_2;
    struct kan_float_vector_4_t row_3;

    // \c_interface_scanner_disable
    KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
    struct kan_math_align_as_cglm_mat_t align_stub[0u];
    KAN_MUTE_THIRD_PARTY_WARNINGS_END
    // \c_interface_scanner_enable
};

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
static inline struct kan_integer_vector_2_t kan_make_integer_vector_2_t (int x, int y)
{
    return (struct kan_integer_vector_2_t) {.x = x, .y = y};
}

/// \brief Convenience constructor function for kan_integer_vector_3_t.
static inline struct kan_integer_vector_3_t kan_make_integer_vector_3_t (int x, int y, int z)
{
    return (struct kan_integer_vector_3_t) {.x = x, .y = y, .z = z};
}

/// \brief Convenience constructor function for kan_integer_vector_4_t.
static inline struct kan_integer_vector_4_t kan_make_integer_vector_4_t (int x, int y, int z, int w)
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

/// \brief Constructor for creating quaternions from euler angles.
static inline struct kan_float_vector_4_t kan_make_quaternion_from_euler (float x, float y, float z)
{
    struct kan_float_vector_4_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    vec3 angles = {x, y, z};
    glm_euler_xyz_quat (&angles, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Linear interpolation for single floating point number.
static inline float kan_float_lerp (float left, float right, float alpha)
{
    return glm_lerp (left, right, alpha);
}

/// \brief Linear interpolation for kan_float_vector_2_t.
static inline struct kan_float_vector_2_t kan_float_vector_2_lerp (struct kan_float_vector_2_t *left,
                                                                   struct kan_float_vector_2_t *right,
                                                                   float alpha)
{
    struct kan_float_vector_2_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_vec2_lerp (left, right, alpha, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Linear interpolation for kan_float_vector_3_t.
static inline struct kan_float_vector_3_t kan_float_vector_3_lerp (struct kan_float_vector_3_t *left,
                                                                   struct kan_float_vector_3_t *right,
                                                                   float alpha)
{
    struct kan_float_vector_3_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_vec3_lerp (left, right, alpha, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Linear interpolation for kan_float_vector_4_t.
static inline struct kan_float_vector_4_t kan_float_vector_4_lerp (struct kan_float_vector_4_t *left,
                                                                   struct kan_float_vector_4_t *right,
                                                                   float alpha)
{
    struct kan_float_vector_4_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_vec4_lerp (left, right, alpha, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Spherical linear interpolation for quaternions.
static inline struct kan_float_vector_4_t kan_float_vector_4_slerp (struct kan_float_vector_4_t *left,
                                                                    struct kan_float_vector_4_t *right,
                                                                    float alpha)
{
    struct kan_float_vector_4_t result;
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_quat_slerp (left, right, alpha, &result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
    return result;
}

/// \brief Multiplies two 3x3 matrices and stores result in output.
static inline void kan_float_matrix_3x3_multiply (const struct kan_float_matrix_3x3_t *left,
                                                  const struct kan_float_matrix_3x3_t *right,
                                                  struct kan_float_matrix_3x3_t *result)
{
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_mat3_mul (left, right, result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
}

/// \brief Inverses given 3x3 matrix and stores result in output.
static inline void kan_float_matrix_3x3_inverse (const struct kan_float_matrix_3x3_t *matrix,
                                                 struct kan_float_matrix_3x3_t *result)
{
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_mat3_inv (matrix, result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
}

/// \brief Multiplies two 4x4 matrices and stores result in output.
static inline void kan_float_matrix_4x4_multiply (const struct kan_float_matrix_4x4_t *left,
                                                  const struct kan_float_matrix_4x4_t *right,
                                                  struct kan_float_matrix_4x4_t *result)
{
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_mat4_mul (left, right, result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
}

/// \brief Multiplies two 4x4 affine transform matrices and stores result in output.
static inline void kan_float_matrix_4x4_multiply_for_transform (const struct kan_float_matrix_4x4_t *left,
                                                                const struct kan_float_matrix_4x4_t *right,
                                                                struct kan_float_matrix_4x4_t *result)
{
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_mul (left, right, result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
}

/// \brief Inverses given 4x4 matrix and stores result in output.
static inline void kan_float_matrix_4x4_inverse (const struct kan_float_matrix_4x4_t *matrix,
                                                 struct kan_float_matrix_4x4_t *result)
{
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_mat4_inv (matrix, result);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
}

/// \brief Converts 2d transform to 3x3 matrix.
static inline void kan_transform_2_to_float_matrix_3x3 (const struct kan_transform_2_t *transform,
                                                        struct kan_float_matrix_3x3_t *matrix)
{
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    glm_translate2d_make (matrix, &transform->location);
    glm_rotate2d (matrix, transform->rotation);
    glm_scale2d (matrix, &transform->scale);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
}

/// \brief Converts 3x3 matrix to 2d transform.
static inline void kan_float_matrix_3x3_to_transform_2 (const struct kan_float_matrix_3x3_t *matrix,
                                                        struct kan_transform_2_t *transform)
{
    transform->location.x = matrix->row_2.x;
    transform->location.y = matrix->row_2.y;
    transform->rotation = atan2f (matrix->row_0.y, matrix->row_0.x);
    transform->scale.x = sqrtf (matrix->row_0.x * matrix->row_0.x + matrix->row_0.y * matrix->row_0.y);
    transform->scale.y = sqrtf (matrix->row_1.x * matrix->row_1.x + matrix->row_1.y * matrix->row_1.y);
}

/// \brief Converts 3d transform to 4x4 matrix.
static inline void kan_transform_3_to_float_matrix_4x4 (const struct kan_transform_3_t *transform,
                                                        struct kan_float_matrix_4x4_t *matrix)
{
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    mat4 intermediate;
    glm_translate_make (intermediate, &transform->location);
    glm_quat_rotate (intermediate, &transform->rotation, matrix);
    glm_scale (matrix, &transform->scale);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
}

/// \brief Converts 4x4 matrix to 3d transform.
static inline void kan_float_matrix_4x4_to_transform_3 (const struct kan_float_matrix_4x4_t *matrix,
                                                        struct kan_transform_3_t *transform)
{
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_BEGIN
    mat4 rotation_matrix;
    glm_decompose (matrix, &transform->location, rotation_matrix, &transform->scale);
    glm_mat4_quat (rotation_matrix, &transform->rotation);
    KAN_MUTE_POINTER_CONVERSION_WARNINGS_END
}

// TODO: Below we add used functions from cglm. We're planning to add them on-demand.

KAN_C_HEADER_END
