#pragma once

#include <application_framework_examples_platform_configuration_api.h>

#include <kan/api_common/c_header.h>

KAN_C_HEADER_BEGIN

// Pipeline instance compilation configuration for compilation byproduct test.
// Part of the common because everything that is in the platform configuration must be common.
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

APPLICATION_FRAMEWORK_EXAMPLES_PLATFORM_CONFIGURATION_API void pipeline_instance_platform_configuration_init (
    struct pipeline_instance_platform_configuration_t *instance);

KAN_C_HEADER_END
