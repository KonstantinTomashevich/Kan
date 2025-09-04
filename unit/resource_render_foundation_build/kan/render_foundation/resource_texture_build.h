#pragma once

#include <resource_render_foundation_build_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/error/critical.h>
#include <kan/reflection/markup.h>
#include <kan/render_foundation/resource_texture.h>

/// \file
/// \brief This file stores data structures for defining textures to be built for runtime usage.
///
/// \par Overview
/// \parblock
/// To define a new texture to be built for usage, `kan_resource_texture_header_t` resource should be used. It defines
/// texture type, build preset and actual texture data third party resource name, for example name of png file with
/// texture data.
///
/// `kan_resource_texture_build_preset_t` is used to specify common options for building different textures as usually
/// textures are split into categories that share build options like mip generation and target formats.
///
/// `kan_resource_texture_platform_configuration_t` is platform configuration container for texture building.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Contains target platform configuration for building textures.
struct kan_resource_texture_platform_configuration_t
{
    /// \brief Lists all formats that are supported on this platform.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (enum kan_resource_texture_format_t)
    struct kan_dynamic_array_t supported_formats;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_texture_platform_configuration_init (
    struct kan_resource_texture_platform_configuration_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_texture_platform_configuration_shutdown (
    struct kan_resource_texture_platform_configuration_t *instance);

/// \brief Enumerates mip generation strategies.
enum kan_resource_texture_mip_generation_t
{
    /// \brief Averages value separately for each channel of input data.
    KAN_RESOURCE_TEXTURE_MIP_GENERATION_AVERAGE = 0u,

    /// \brief Selects min value separately for each channel of input data.
    KAN_RESOURCE_TEXTURE_MIP_GENERATION_MIN,

    /// \brief Selects max value separately for each channel of input data.
    KAN_RESOURCE_TEXTURE_MIP_GENERATION_MAX,
};

/// \brief Describes common configuration for building a category of textures.
struct kan_resource_texture_build_preset_t
{
    enum kan_resource_texture_mip_generation_t mip_generation;
    
    /// \brief Advised count of mips. There will be less mips if texture is not big enough.
    kan_instance_size_t target_mips;

    /// \brief Formats in which this texture can be built and stored.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (enum kan_resource_texture_format_t)
    struct kan_dynamic_array_t supported_target_formats;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_texture_build_preset_init (
    struct kan_resource_texture_build_preset_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_texture_build_preset_shutdown (
    struct kan_resource_texture_build_preset_t *instance);

/// \brief Enumerates image classes that define how image is parsed.
enum kan_resource_texture_image_class_t
{
    /// \brief Non-linear RGBA data with 4 channels and 8 bits per channel.
    KAN_RESOURCE_TEXTURE_IMAGE_CLASS_COLOR_SRGB = 0u,
    
    /// \brief Linear RGBA data with 4 channels and 8 bits per channel.
    KAN_RESOURCE_TEXTURE_IMAGE_CLASS_COLOR_RGB,
    
    /// \brief Depth in grayscale format: image loaded as linear RGBA, but only first 8 bit channel of 4 channels is 
    ///        used for normalized depth calculation.
    KAN_RESOURCE_TEXTURE_IMAGE_CLASS_DEPTH_GRAYSCALE,
    
    /// \brief Depth in plain 32-bit float format: every 4 byte pixel is treated as raw bits for 32 bit float value 
    ///        with 1 channel for depth.
    KAN_RESOURCE_TEXTURE_IMAGE_CLASS_DEPTH_FLOAT_32,
};

/// \brief Describes how to build a runtime texture resource.
struct kan_resource_texture_header_t
{
    kan_interned_string_t preset;
    enum kan_resource_texture_image_class_t image_class;
    kan_interned_string_t image;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_texture_header_init (
    struct kan_resource_texture_header_t *instance);

KAN_C_HEADER_END
