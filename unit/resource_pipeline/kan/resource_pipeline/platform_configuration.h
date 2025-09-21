#pragma once

#include <resource_pipeline_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/reflection/markup.h>
#include <kan/reflection/patch.h>

/// \file
/// \brief Contains data structures that are used to define platform specific configuration for resource build routine.
///
/// \par Overview
/// \parblock
/// Platform configuration is used to provide platform-specific information for resource build routine, for example
/// target language for shaders or target formats for textures.
///
/// Platform configuration consists of setup file and entry files. Setup file must have
/// `kan_resource_platform_configuration_setup_t` type and list configuration layers in order in which they are applied.
///
/// Then every file in the same directory hierarchy as setup file is treated as platform configuration entry and must
/// have `kan_resource_platform_configuration_entry_t` type, which describes data for arbitrary platform configuration
/// type, layer on which this entry is applied and tags that must exist in
/// `kan_resource_project_t::platform_configuration_tags` in order for this entry to be considered enabled.
///
/// This approach makes it possible to store all the configurations for all the platforms in one directory and filter
/// them by tags, while also having patch hierarchy through layers and separate timestamps for every configuration to
/// avoid unnecessary full rebuilds in resource build routine.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Returns allocation group that is used for allocating everything connected to platform configuration.
RESOURCE_PIPELINE_API kan_allocation_group_t kan_resource_platform_configuration_get_allocation_group (void);

/// \brief Expected name for platform configuration setup file.
#define KAN_RESOURCE_PLATFORM_CONFIGURATION_SETUP_FILE "platform_configuration_setup.rd"

/// \brief Describes platform configuration setup data structure.
struct kan_resource_platform_configuration_setup_t
{
    /// \brief List of known layers in order in which they should be applied.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t layers;
};

RESOURCE_PIPELINE_API void kan_resource_platform_configuration_setup_init (
    struct kan_resource_platform_configuration_setup_t *instance);

RESOURCE_PIPELINE_API void kan_resource_platform_configuration_setup_shutdown (
    struct kan_resource_platform_configuration_setup_t *instance);

/// \brief Describes single platform configuration entry of particular type on particular layer.
struct kan_resource_platform_configuration_entry_t
{
    /// \brief If these tags are not present in `kan_resource_project_t::platform_configuration_tags`,
    ///        then this entry is ignored.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t required_tags;

    kan_interned_string_t layer;
    kan_reflection_patch_t data;
};

RESOURCE_PIPELINE_API void kan_resource_platform_configuration_entry_init (
    struct kan_resource_platform_configuration_entry_t *instance);

RESOURCE_PIPELINE_API void kan_resource_platform_configuration_entry_shutdown (
    struct kan_resource_platform_configuration_entry_t *instance);

KAN_C_HEADER_END
