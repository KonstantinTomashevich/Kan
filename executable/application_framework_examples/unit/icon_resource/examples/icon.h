#pragma once

#include <application_framework_examples_icon_resource_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/reflection/markup.h>

KAN_C_HEADER_BEGIN

typedef uint32_t rgba_pixel_t;

struct icon_t
{
    kan_serialized_size_t width;
    kan_serialized_size_t height;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (rgba_pixel_t)
    struct kan_dynamic_array_t pixels;
};

APPLICATION_FRAMEWORK_EXAMPLES_ICON_RESOURCE_API void icon_init (struct icon_t *icon);

APPLICATION_FRAMEWORK_EXAMPLES_ICON_RESOURCE_API void icon_shutdown (struct icon_t *icon);

KAN_C_HEADER_END
