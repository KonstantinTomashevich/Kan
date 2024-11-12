#pragma once

#include <test_reflection_section_patch_types_post_api.h>

#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>

KAN_C_HEADER_BEGIN

// Don't care what other include might've defined.
#undef enum_to_adapt_t
#undef ENUM_TO_ADAPT_ONE
#undef ENUM_TO_ADAPT_TWO
#undef ENUM_TO_ADAPT_THREE

#undef most_inner_type_t
#undef most_inner_type_init

#undef type_to_delete_t

#undef middle_type_t
#undef middle_type_init
#undef middle_type_shutdown

#undef root_type_t
#undef root_type_init
#undef root_type_shutdown

// Defines to trick c_interface_scanner while keeping both files includeable.
#define enum_to_adapt_t enum_to_adapt_post_t
#define ENUM_TO_ADAPT_ONE ENUM_TO_ADAPT_ONE_POST
#define ENUM_TO_ADAPT_TWO ENUM_TO_ADAPT_TWO_POST
#define ENUM_TO_ADAPT_THREE ENUM_TO_ADAPT_THREE_POST

#define most_inner_type_t most_inner_type_post_t
#define most_inner_type_init most_inner_type_post_init

#define middle_type_t middle_type_post_t
#define middle_type_init middle_type_post_init
#define middle_type_shutdown middle_type_post_shutdown

#define root_type_t root_type_post_t
#define root_type_init root_type_post_init
#define root_type_shutdown root_type_post_shutdown

enum enum_to_adapt_t
{
    ENUM_TO_ADAPT_ONE = 0u,
    ENUM_TO_ADAPT_THREE,
};

struct most_inner_type_t
{
    kan_instance_size_t x;
    kan_instance_size_t y;
};

TEST_REFLECTION_SECTION_PATCH_TYPES_POST_API void most_inner_type_init (struct most_inner_type_t *instance);

struct middle_type_t
{
    /// \meta reflection_dynamic_array_type = "struct most_inner_type_t"
    struct kan_dynamic_array_t inner_structs;

    /// \meta reflection_dynamic_array_type = "enum enum_to_adapt_t"
    struct kan_dynamic_array_t enums;
};

TEST_REFLECTION_SECTION_PATCH_TYPES_POST_API void middle_type_init (struct middle_type_t *instance);

TEST_REFLECTION_SECTION_PATCH_TYPES_POST_API void middle_type_shutdown (struct middle_type_t *instance);

struct root_type_t
{
    /// \meta reflection_dynamic_array_type = "struct middle_type_t"
    struct kan_dynamic_array_t middle_structs;

    kan_instance_size_t data_after;
};

TEST_REFLECTION_SECTION_PATCH_TYPES_POST_API void root_type_init (struct root_type_t *instance);

TEST_REFLECTION_SECTION_PATCH_TYPES_POST_API void root_type_shutdown (struct root_type_t *instance);

KAN_C_HEADER_END
