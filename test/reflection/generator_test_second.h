#pragma once

#include <test_reflection_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/reflection/patch.h>

#include <generator_test_first.h>

KAN_C_HEADER_BEGIN

struct first_component_t
{
    struct vector3_t position;
    struct vector4_t rotation;
};

TEST_REFLECTION_API void first_component_init (struct first_component_t *instance);

TEST_REFLECTION_API void first_component_shutdown (struct first_component_t *instance);

struct second_component_t
{
    struct vector3_t velocity;
    struct vector3_t acceleration;
};

TEST_REFLECTION_API void second_component_init (struct second_component_t *instance);

TEST_REFLECTION_API void second_component_shutdown (struct second_component_t *instance);

// \meta reflection_ignore_struct
struct ignored_t
{
    struct vector3_t velocity;
    struct vector3_t acceleration;
};

// \meta reflection_ignore_enum
enum ignored_enum_t
{
    IGNORED_ENUM_VALUE_1 = 0,
    IGNORED_ENUM_VALUE_2,
};

enum some_enum_t
{
    SOME_ENUM_VALUE_1 = 0,

    // \meta reflection_ignore_enum_value
    SOME_ENUM_HIDDEN,

    SOME_ENUM_VALUE_2,

    // \meta reflection_ignore_enum_value
    SOME_ENUM_COUNT,
};

// \meta reflection_flags
enum some_flags_t
{
    FLAG_1 = 1u << 0u,
    FLAG_2 = 1u << 1u,
};

struct a_bit_of_everything_t
{
    enum some_enum_t some_enum;
    enum some_flags_t some_flags;

    uint32_t uint32;
    int32_t int32;

    uint64_t count;

    // \meta reflection_size_field = count
    int8_t bytes[128u];

    uint64_t selector;

    union
    {
        // \meta reflection_visibility_condition_field = selector
        // \meta reflection_visibility_condition_values = "0, 2"
        uint32_t first_selection[8u];

        // \meta reflection_visibility_condition_field = selector
        // \meta reflection_visibility_condition_values = 1
        int64_t second_selection[4u];
    };

    char *owned_string;
    kan_interned_string_t interned_string;
    kan_reflection_patch_t patch;

    void *first_external_pointer;
    struct first_component_t *struct_pointer;

    /// \meta reflection_external_pointer
    struct first_component_t *second_external_pointer;

    /// \meta reflection_ignore_struct_field
    struct kan_dynamic_array_t ignored_dynamic_array;

    /// \meta reflection_dynamic_array_type = "struct second_component_t *"
    struct kan_dynamic_array_t dynamic_array;
};

KAN_C_HEADER_END
