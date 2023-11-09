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

KAN_C_HEADER_END
