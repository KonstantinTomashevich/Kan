#pragma once

#include <test_reflection_section_patch_types_pre_api.h>

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

#    define enum_to_adapt_t enum_to_adapt_pre_t
#    define ENUM_TO_ADAPT_ONE ENUM_TO_ADAPT_ONE_PRE
#    define ENUM_TO_ADAPT_TWO ENUM_TO_ADAPT_TWO_PRE
#    define ENUM_TO_ADAPT_THREE ENUM_TO_ADAPT_THREE_PRE

#    define most_inner_type_t most_inner_type_pre_t
#    define type_to_delete_t type_to_delete_pre_t
#    define middle_type_t middle_type_pre_t
#    define root_type_t root_type_pre_t
#endif

enum enum_to_adapt_t
{
    ENUM_TO_ADAPT_ONE = 0u,
    ENUM_TO_ADAPT_TWO,
    ENUM_TO_ADAPT_THREE,
};

KAN_REFLECTION_EXPLICIT_INIT_FUNCTOR (most_inner_type_pre_init)
struct most_inner_type_t
{
    kan_instance_size_t x;
    kan_instance_size_t y;
    kan_instance_size_t z;
};

TEST_REFLECTION_SECTION_PATCH_TYPES_PRE_API void most_inner_type_pre_init (struct most_inner_type_t *instance);

struct type_to_delete_t
{
    kan_instance_size_t some_data;
    kan_instance_size_t some_other_data;
};

KAN_REFLECTION_EXPLICIT_INIT_FUNCTOR (middle_type_pre_init)
KAN_REFLECTION_EXPLICIT_SHUTDOWN_FUNCTOR (middle_type_pre_shutdown)
struct middle_type_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct most_inner_type_t)
    struct kan_dynamic_array_t inner_structs;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct type_to_delete_t)
    struct kan_dynamic_array_t structs_to_delete;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (enum enum_to_adapt_t)
    struct kan_dynamic_array_t enums;
};

TEST_REFLECTION_SECTION_PATCH_TYPES_PRE_API void middle_type_pre_init (struct middle_type_t *instance);

TEST_REFLECTION_SECTION_PATCH_TYPES_PRE_API void middle_type_pre_shutdown (struct middle_type_t *instance);

KAN_REFLECTION_EXPLICIT_INIT_FUNCTOR (root_type_pre_init)
KAN_REFLECTION_EXPLICIT_SHUTDOWN_FUNCTOR (root_type_pre_shutdown)
struct root_type_t
{
    kan_instance_size_t data_before;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct middle_type_t)
    struct kan_dynamic_array_t middle_structs;

    kan_instance_size_t data_after;
};

TEST_REFLECTION_SECTION_PATCH_TYPES_PRE_API void root_type_pre_init (struct root_type_t *instance);

TEST_REFLECTION_SECTION_PATCH_TYPES_PRE_API void root_type_pre_shutdown (struct root_type_t *instance);

KAN_C_HEADER_END
