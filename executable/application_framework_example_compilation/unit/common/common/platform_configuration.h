#pragma once

#include <application_framework_example_compilation_common_api.h>

#include <kan/api_common/c_header.h>

KAN_C_HEADER_BEGIN

// Pipeline instance compilation configuration for byproduct test.
// As it is only example stub, we just use simple enum.

enum pipeline_instance_platform_format_t
{
    PIPELINE_INSTANCE_PLATFORM_FORMAT_UNKNOWN = 0u,
    PIPELINE_INSTANCE_PLATFORM_FORMAT_SPIRV,
};

struct pipeline_instance_platform_configuration_t
{
    enum pipeline_instance_platform_format_t format;
};

APPLICATION_FRAMEWORK_EXAMPLE_COMPILATION_COMMON_API void pipeline_instance_platform_configuration_init (
    struct pipeline_instance_platform_configuration_t *instance);

KAN_C_HEADER_END
