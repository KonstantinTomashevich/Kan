#pragma once

#include <resource_render_foundation_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/error/critical.h>
#include <kan/reflection/markup.h>

/// \file
/// \brief This file stores runtime representation of texture and texture data resources.
///
/// \par Overview
/// \parblock
/// While in runtime, texture is represented by primary resource `kan_resource_texture_t` and data resources
/// `kan_resource_texture_data_t`. Primary resource stores meta information about texture, primarily dimensions
/// and built formats with their mips. Every data resource stores texture data for one particular mip in one particular
/// format. It makes it possible to load texture only in required mips and required formats.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Enumerates known formats that can be used as texture build target formats.
/// \details There is no built 24 bit uncompressed formats as
///          they are usually not natively supported by GPUs for sampling.
enum kan_resource_texture_format_t
{
    KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_R8_SRGB = 0u,
    KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RG16_SRGB,
    KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RGBA32_SRGB,

    KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_R8_UNORM,
    KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RG16_UNORM,
    KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RGBA32_UNORM,

    KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_D16,
    KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_D32,

    // TODO: Compressed formats like BCn, ETC2 and ASTC will be added in the future on demand.
};

/// \brief Contains data for particular mip in particular format for some texture.
/// \details Has only texture data, because format and other parameters are determined by texture resource.
struct kan_resource_texture_data_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (uint8_t)
    struct kan_dynamic_array_t data;
};

RESOURCE_RENDER_FOUNDATION_API void kan_resource_texture_data_init (struct kan_resource_texture_data_t *instance);

RESOURCE_RENDER_FOUNDATION_API void kan_resource_texture_data_shutdown (struct kan_resource_texture_data_t *instance);

/// \brief Contains references to per mip texture data for particular format.
struct kan_resource_texture_format_item_t
{
    /// \brief Format in which texture data is built.
    enum kan_resource_texture_format_t format;

    /// \brief Array of data resource names for every mip in ascending mip order.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t data_per_mip;
};

RESOURCE_RENDER_FOUNDATION_API void kan_resource_texture_format_item_init (
    struct kan_resource_texture_format_item_t *instance);

RESOURCE_RENDER_FOUNDATION_API void kan_resource_texture_format_item_shutdown (
    struct kan_resource_texture_format_item_t *instance);

/// \brief Contains built texture information excluding actual texture data.
/// \details Texture data is separated in order to make per-mip loading possible.
struct kan_resource_texture_t
{
    kan_instance_size_t width;
    kan_instance_size_t height;
    kan_instance_size_t depth;
    kan_instance_size_t mips;

    /// \brief Contains information about data for every built format.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_texture_format_item_t)
    struct kan_dynamic_array_t formats;
};

RESOURCE_RENDER_FOUNDATION_API void kan_resource_texture_init (struct kan_resource_texture_t *instance);

RESOURCE_RENDER_FOUNDATION_API void kan_resource_texture_shutdown (struct kan_resource_texture_t *instance);

KAN_C_HEADER_END
