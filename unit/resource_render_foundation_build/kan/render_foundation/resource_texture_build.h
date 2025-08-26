#pragma once

#include <resource_render_foundation_build_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/error/critical.h>
#include <kan/reflection/markup.h>
#include <kan/render_foundation/resource_texture.h>

// TODO: Docs once whole render foundation refactor is done? Or rewrite old docs now?

KAN_C_HEADER_BEGIN

/// \brief Contains target platform configuration for building textures.
struct kan_resource_texture_platform_configuration_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (enum kan_resource_texture_format_t)
    struct kan_dynamic_array_t supported_formats;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_texture_platform_configuration_init (
    struct kan_resource_texture_platform_configuration_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_texture_platform_configuration_shutdown (
    struct kan_resource_texture_platform_configuration_t *instance);

enum kan_resource_texture_mip_generation_t
{
    /// \brief Averages value separately for each channel of input RGBA.
    KAN_RESOURCE_TEXTURE_MIP_GENERATION_AVERAGE = 0u,

    /// \brief Selects min value separately for each channel of input RGBA.
    KAN_RESOURCE_TEXTURE_MIP_GENERATION_MIN,

    /// \brief Selects max value separately for each channel of input RGBA.
    KAN_RESOURCE_TEXTURE_MIP_GENERATION_MAX,
};

struct kan_resource_texture_build_preset_t
{
    enum kan_resource_texture_mip_generation_t mip_generation;

    kan_instance_size_t target_mips;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (enum kan_resource_texture_format_t)
    struct kan_dynamic_array_t supported_target_formats;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_texture_build_preset_init (
    struct kan_resource_texture_build_preset_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_texture_build_preset_shutdown (
    struct kan_resource_texture_build_preset_t *instance);

enum kan_resource_texture_image_class_t
{
    KAN_RESOURCE_TEXTURE_IMAGE_CLASS_COLOR_SRGB = 0u,
    KAN_RESOURCE_TEXTURE_IMAGE_CLASS_COLOR_RGB,
    KAN_RESOURCE_TEXTURE_IMAGE_CLASS_DEPTH_GRAYSCALE,
    KAN_RESOURCE_TEXTURE_IMAGE_CLASS_DEPTH_FLOAT_32,
};

struct kan_resource_texture_header_t
{
    kan_interned_string_t preset;
    enum kan_resource_texture_image_class_t image_class;
    kan_interned_string_t image;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_texture_header_init (
    struct kan_resource_texture_header_t *instance);

KAN_C_HEADER_END
