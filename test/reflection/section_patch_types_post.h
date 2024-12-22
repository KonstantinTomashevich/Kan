#pragma once

#include <test_reflection_section_patch_types_post_api.h>

#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>
#include <kan/reflection/markup.h>

KAN_C_HEADER_BEGIN

// When including for test root file, we would like to have all the declarations from both pre and post stages
// with appropriate suffixes, so we can test both conveniently.
#if defined(INCLUDING_FOR_TEST_ROOT)
#    undef enum_to_adapt_t
#    undef ENUM_TO_ADAPT_ONE
#    undef ENUM_TO_ADAPT_TWO
#    undef ENUM_TO_ADAPT_THREE

#    undef most_inner_type_t
#    undef type_to_delete_t
#    undef middle_type_t
#    undef root_type_t

#    define enum_to_adapt_t enum_to_adapt_post_t
#    define ENUM_TO_ADAPT_ONE ENUM_TO_ADAPT_ONE_POST
#    define ENUM_TO_ADAPT_TWO ENUM_TO_ADAPT_TWO_POST
#    define ENUM_TO_ADAPT_THREE ENUM_TO_ADAPT_THREE_POST

#    define most_inner_type_t most_inner_type_post_t
#    define middle_type_t middle_type_post_t
#    define root_type_t root_type_post_t
#endif

enum enum_to_adapt_t
{
    ENUM_TO_ADAPT_ONE = 0u,
    ENUM_TO_ADAPT_THREE,
};

KAN_REFLECTION_EXPLICIT_INIT_FUNCTOR (most_inner_type_post_init)
struct most_inner_type_t
{
    kan_instance_size_t x;
    kan_instance_size_t y;
};

TEST_REFLECTION_SECTION_PATCH_TYPES_POST_API void most_inner_type_post_init (struct most_inner_type_t *instance);

KAN_REFLECTION_EXPLICIT_INIT_FUNCTOR (middle_type_post_init)
KAN_REFLECTION_EXPLICIT_SHUTDOWN_FUNCTOR (middle_type_post_shutdown)
struct middle_type_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct most_inner_type_t)
    struct kan_dynamic_array_t inner_structs;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (enum enum_to_adapt_t)
    struct kan_dynamic_array_t enums;
};

TEST_REFLECTION_SECTION_PATCH_TYPES_POST_API void middle_type_post_init (struct middle_type_t *instance);

TEST_REFLECTION_SECTION_PATCH_TYPES_POST_API void middle_type_post_shutdown (struct middle_type_t *instance);

KAN_REFLECTION_EXPLICIT_INIT_FUNCTOR (root_type_post_init)
KAN_REFLECTION_EXPLICIT_SHUTDOWN_FUNCTOR (root_type_post_shutdown)
struct root_type_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct middle_type_t)
    struct kan_dynamic_array_t middle_structs;

    kan_instance_size_t data_after;
};

TEST_REFLECTION_SECTION_PATCH_TYPES_POST_API void root_type_post_init (struct root_type_t *instance);

TEST_REFLECTION_SECTION_PATCH_TYPES_POST_API void root_type_post_shutdown (struct root_type_t *instance);

KAN_C_HEADER_END
