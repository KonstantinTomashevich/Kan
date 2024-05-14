#pragma once

#include <kan/api_common/mute_third_party_warnings.h>

KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#include <cglm/vec2.h>
KAN_MUTE_THIRD_PARTY_WARNINGS_END

#include <kan/api_common/c_header.h>

/// \file
/// \brief Contains common math types and functions.

KAN_C_HEADER_BEGIN

// \c_interface_scanner_disable

/// \brief Stub for forcing alignment.
struct CGLM_ALIGN (8) kan_math_align_as_8
{
    float stub;
};

/// \brief Stub for forcing alignment.
struct CGLM_ALIGN (16) kan_math_align_as_16
{
    float stub;
};

/// \brief Stub for forcing alignment.
struct CGLM_ALIGN_MAT kan_math_align_as_cglm_mat
{
    float stub;
};

// \c_interface_scanner_enable

/// \brief 2 dimensional integer vector type.
struct kan_integer_vector_2
{
    int x;
    int y;
};

_Static_assert (sizeof (struct kan_integer_vector_2) == sizeof (ivec2), "Size validation.");
_Static_assert (_Alignof (struct kan_integer_vector_2) == _Alignof (ivec2), "Alignment validation.");

/// \brief 3 dimensional integer vector type.
struct kan_integer_vector_3
{
    int x;
    int y;
    int z;
};

_Static_assert (sizeof (struct kan_integer_vector_3) == sizeof (ivec3), "Size validation.");
_Static_assert (_Alignof (struct kan_integer_vector_3) == _Alignof (ivec3), "Alignment validation.");

/// \brief 4 dimensional integer vector type.
struct kan_integer_vector_4
{
    int x;
    int y;
    int z;
    int w;
};

_Static_assert (sizeof (struct kan_integer_vector_4) == sizeof (ivec4), "Size validation.");
_Static_assert (_Alignof (struct kan_integer_vector_4) == _Alignof (ivec4), "Alignment validation.");

/// \brief 2 dimensional floating point vector type.
struct kan_float_vector_2
{
    float x;
    float y;
};

_Static_assert (sizeof (struct kan_float_vector_2) == sizeof (vec2), "Size validation.");
_Static_assert (_Alignof (struct kan_float_vector_2) == _Alignof (vec2), "Alignment validation.");

/// \brief 3 dimensional floating point vector type.
struct kan_float_vector_3
{
    float x;
    float y;
    float z;
};

_Static_assert (sizeof (struct kan_float_vector_3) == sizeof (vec3), "Size validation.");
_Static_assert (_Alignof (struct kan_float_vector_3) == _Alignof (vec3), "Alignment validation.");

/// \brief 4 dimensional floating point vector type.
struct kan_float_vector_4
{
    float x;
    float y;
    float z;
    float w;

    // \c_interface_scanner_disable
    KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
    struct kan_math_align_as_16 align_stub[0u];
    KAN_MUTE_THIRD_PARTY_WARNINGS_END
    // \c_interface_scanner_enable
};

_Static_assert (sizeof (struct kan_float_vector_4) == sizeof (vec4), "Size validation.");
_Static_assert (_Alignof (struct kan_float_vector_4) == _Alignof (vec4), "Alignment validation.");

/// \brief 3x3 floating point matrix type.
struct kan_float_matrix_3x3
{
    struct kan_float_vector_3 row_0;
    struct kan_float_vector_3 row_1;
    struct kan_float_vector_3 row_2;
};

_Static_assert (sizeof (struct kan_float_matrix_3x3) == sizeof (mat3), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_3x3) == _Alignof (mat3), "Alignment validation.");

/// \brief 3x2 floating point matrix type.
struct kan_float_matrix_3x2
{
    struct kan_float_vector_2 row_0;
    struct kan_float_vector_2 row_1;
    struct kan_float_vector_2 row_2;
};

_Static_assert (sizeof (struct kan_float_matrix_3x2) == sizeof (mat3x2), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_3x2) == _Alignof (mat3x2), "Alignment validation.");

/// \brief 3x4 floating point matrix type.
struct kan_float_matrix_3x4
{
    struct kan_float_vector_4 row_0;
    struct kan_float_vector_4 row_1;
    struct kan_float_vector_4 row_2;
};

_Static_assert (sizeof (struct kan_float_matrix_3x4) == sizeof (mat3x4), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_3x4) == _Alignof (mat3x4), "Alignment validation.");

/// \brief 2x2 floating point matrix type.
struct kan_float_matrix_2x2
{
    struct kan_float_vector_2 row_0;
    struct kan_float_vector_2 row_1;

    // \c_interface_scanner_disable
    KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
    struct kan_math_align_as_16 align_stub[0u];
    KAN_MUTE_THIRD_PARTY_WARNINGS_END
    // \c_interface_scanner_enable
};

_Static_assert (sizeof (struct kan_float_matrix_2x2) == sizeof (mat2), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_2x2) == _Alignof (mat2), "Alignment validation.");

/// \brief 2x3 floating point matrix type.
struct kan_float_matrix_2x3
{
    struct kan_float_vector_3 row_0;
    struct kan_float_vector_3 row_1;
};

_Static_assert (sizeof (struct kan_float_matrix_2x3) == sizeof (mat2x3), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_2x3) == _Alignof (mat2x3), "Alignment validation.");

/// \brief 2x4 floating point matrix type.
struct kan_float_matrix_2x4
{
    struct kan_float_vector_4 row_0;
    struct kan_float_vector_4 row_1;
};

_Static_assert (sizeof (struct kan_float_matrix_2x4) == sizeof (mat2x4), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_2x4) == _Alignof (mat2x4), "Alignment validation.");

/// \brief 4x4 floating point matrix type.
struct kan_float_matrix_4x4
{
    struct kan_float_vector_4 row_0;
    struct kan_float_vector_4 row_1;
    struct kan_float_vector_4 row_2;
    struct kan_float_vector_4 row_3;

    // \c_interface_scanner_disable
    KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
    struct kan_math_align_as_cglm_mat align_stub[0u];
    KAN_MUTE_THIRD_PARTY_WARNINGS_END
    // \c_interface_scanner_enable
};

_Static_assert (sizeof (struct kan_float_matrix_4x4) == sizeof (mat4), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_4x4) == _Alignof (mat4), "Alignment validation.");

/// \brief 4x2 floating point matrix type.
struct kan_float_matrix_4x2
{
    struct kan_float_vector_2 row_0;
    struct kan_float_vector_2 row_1;
    struct kan_float_vector_2 row_2;
    struct kan_float_vector_2 row_3;
};

_Static_assert (sizeof (struct kan_float_matrix_4x2) == sizeof (mat4x2), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_4x2) == _Alignof (mat4x2), "Alignment validation.");

/// \brief 4x3 floating point matrix type.
struct kan_float_matrix_4x3
{
    struct kan_float_vector_3 row_0;
    struct kan_float_vector_3 row_1;
    struct kan_float_vector_3 row_2;
    struct kan_float_vector_3 row_3;
};

_Static_assert (sizeof (struct kan_float_matrix_4x3) == sizeof (mat4x3), "Size validation.");
_Static_assert (_Alignof (struct kan_float_matrix_4x3) == _Alignof (mat4x3), "Alignment validation.");

// TODO: Below we add used functions from cglm. We're planning to add them on-demand.

KAN_C_HEADER_END
