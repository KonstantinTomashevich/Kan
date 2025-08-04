#pragma once

#include <resource_pipeline_build_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/reflection/markup.h>
#include <kan/resource_pipeline/project.h>
#include <kan/resource_pipeline/reflected_data.h>
#include <kan/resource_pipeline/tooling_meta.h>

KAN_C_HEADER_BEGIN

RESOURCE_PIPELINE_BUILD_API kan_allocation_group_t kan_resource_build_get_allocation_group (void);

enum kan_resource_build_result_t
{
    KAN_RESOURCE_BUILD_RESULT_SUCCESS = 0u,
    KAN_RESOURCE_BUILD_RESULT_ERROR_PROJECT_DUPLICATE_TARGETS,
    KAN_RESOURCE_BUILD_RESULT_ERROR_PROJECT_VISIBLE_TARGET_NOT_FOUND,
    KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_NOT_FOUND,
    KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_IO_ERROR,
    KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_DUPLICATE_LAYER,
    KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_UNKNOWN_ENTRY_FILE,
    KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_UNKNOWN_LAYER,
    KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_DUPLICATE_TYPE,
    KAN_RESOURCE_BUILD_RESULT_ERROR_WORKSPACE_CLEANUP_FAILED,
    KAN_RESOURCE_BUILD_RESULT_ERROR_WORKSPACE_CANNOT_MAKE_DIRECTORY,
    KAN_RESOURCE_BUILD_RESULT_ERROR_LOG_CANNOT_BE_OPENED,
    KAN_RESOURCE_BUILD_RESULT_ERROR_LOG_IO_ERROR,
    KAN_RESOURCE_BUILD_RESULT_ERROR_RAW_RESOURCE_SCAN_FAILED,
    KAN_RESOURCE_BUILD_RESULT_ERROR_BUILD_FAILED,
};

struct kan_resource_build_setup_t
{
    const struct kan_resource_project_t *project;
    const struct kan_resource_reflected_data_storage_t *reflected_data;

    bool pack;
    enum kan_log_verbosity_t log_verbosity;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t targets;
};

RESOURCE_PIPELINE_BUILD_API void kan_resource_build_setup_init (struct kan_resource_build_setup_t *instance);

RESOURCE_PIPELINE_BUILD_API void kan_resource_build_setup_shutdown (struct kan_resource_build_setup_t *instance);

RESOURCE_PIPELINE_BUILD_API enum kan_resource_build_result_t kan_resource_build (
    struct kan_resource_build_setup_t *setup);

KAN_C_HEADER_END
