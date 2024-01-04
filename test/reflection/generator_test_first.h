#pragma once

#include <test_reflection_api.h>

#include <kan/api_common/c_header.h>

KAN_C_HEADER_BEGIN

struct vector3_t
{
    float x;
    float y;
    float z;
};

struct vector4_t
{
    float x;
    float y;
    float z;
    float w;
};

TEST_REFLECTION_API struct vector3_t vector3_add (const struct vector3_t *first, const struct vector3_t *second);

TEST_REFLECTION_API struct vector4_t vector4_add (const struct vector4_t *first, const struct vector4_t *second);

KAN_C_HEADER_END
