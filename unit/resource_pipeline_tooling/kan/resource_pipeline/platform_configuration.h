#pragma once

#include <resource_pipeline_tooling_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/reflection/markup.h>
#include <kan/reflection/patch.h>

// TODO: Docs later. Platform configuration structure might be changed during implementation,
//       so it is ineffective to document it right now.

KAN_C_HEADER_BEGIN

RESOURCE_PIPELINE_TOOLING_API kan_allocation_group_t kan_resource_platform_configuration_get_allocation_group (void);

#define KAN_RESOURCE_PLATFORM_CONFIGURATION_SETUP_FILE = "platform_configuration_setup.rd"

struct kan_resource_platform_configuration_setup_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t layers;
};

RESOURCE_PIPELINE_TOOLING_API void kan_resource_platform_configuration_setup_init (
    struct kan_resource_platform_configuration_setup_t *instance);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_platform_configuration_setup_shutdown (
    struct kan_resource_platform_configuration_setup_t *instance);

struct kan_resource_platform_configuration_entry_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t required_tags;

    kan_interned_string_t layer;
    kan_reflection_patch_t data;
};

RESOURCE_PIPELINE_TOOLING_API void kan_resource_platform_configuration_entry_init (
    struct kan_resource_platform_configuration_entry_t *instance);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_platform_configuration_entry_shutdown (
    struct kan_resource_platform_configuration_entry_t *instance);

KAN_C_HEADER_END
