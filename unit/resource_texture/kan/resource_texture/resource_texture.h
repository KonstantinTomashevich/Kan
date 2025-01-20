#pragma once

#include <resource_texture_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/error/critical.h>
#include <kan/memory/allocation.h>
#include <kan/reflection/markup.h>

/// \file
/// \brief This file stores various resource types needed to properly store, compile and use textures.
///
/// \par Overview
/// \parblock
/// When stored raw, texture is represented by several resources:
/// - `kan_resource_texture_t` contains resource configuration, including names of raw data resource and compilation
///   preset. It is expected to be user created, not produced from import.
/// - `kan_resource_texture_raw_data_t` contains texture properties that are usually produced as a result of image
///   import operation and texture data in a form of bitmap. Texture raw data is stored in format from
///   `kan_resource_texture_raw_format_t`.
/// - `kan_resource_texture_compilation_preset_t` contains settings for texture compilation and is usually shared
///   by a family of textures.
///
/// In a compiled format aimed for packaged game, texture is represented by:
/// - `kan_resource_texture_compiled_t` is a result of compiling `kan_resource_texture_t` and contains common
///   information about the texture. Texture can be compiled into several formats and mips will be automatically
///   generated.
/// - `kan_resource_texture_compiled_data_t` contains texture data in particular format for particular mip. Separating
///   texture data blocks makes it possible to load mips independently.
/// \endparblock
///
/// \par Compilation
/// \parblock
/// Textures need to be compiled for several reasons:
/// - We might want to compress texture into more effective format.
/// - We might want to generate mips offline.
///
/// `kan_resource_texture_compilation_preset_t` resource lists all the format supported by this texture as target
/// compilation formats, including compressed and uncompressed ones. And `kan_resource_texture_platform_configuration_t`
/// is a resource platform configuration entry that lists target compilation formats that are considered supported by
/// target platform. Texture is compiled into target format only if this format is present in both
/// `kan_resource_texture_compilation_preset_t` and `kan_resource_texture_platform_configuration_t`.
///
/// Keep in mind, that it is allowed to compile texture into several formats and choose supported format at runtime,
/// which might be useful for platforms when format support is ambiguous, but will also consume more space.
///
/// When working with texture resources in development environment, it is advised to avoid runtime compilation as it
/// would be costly and unnecessary for textures.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Enumerates known formats for texture raw data bitmap.
enum kan_resource_texture_raw_format_t
{
    /// \brief 1-channel 8-bit color values.
    KAN_RESOURCE_TEXTURE_RAW_FORMAT_R8 = 0u,

    /// \brief 2-channel 8-bit color values.
    KAN_RESOURCE_TEXTURE_RAW_FORMAT_RG16,

    /// \brief 3-channel 8-bit color values.
    KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGB24,

    /// \brief 4-channel 8-bit color values.
    KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGBA32,

    /// \brief Depth values stored as 16-bit unsigned normalized integers.
    KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH16,

    /// \brief Depth values stored as 32 bit floats.
    KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH32,
};

/// \brief Resource that contains texture raw data. Expected to be produced by resource import.
struct kan_resource_texture_raw_data_t
{
    kan_instance_size_t width;
    kan_instance_size_t height;
    kan_instance_size_t depth;
    enum kan_resource_texture_raw_format_t format;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (uint8_t)
    struct kan_dynamic_array_t data;
};

RESOURCE_TEXTURE_API void kan_resource_texture_raw_data_init (struct kan_resource_texture_raw_data_t *instance);

RESOURCE_TEXTURE_API void kan_resource_texture_raw_data_shutdown (struct kan_resource_texture_raw_data_t *instance);

/// \brief Enumerates known formats that can be used as texture compilation target formats.
enum kan_resource_texture_compiled_format_t
{
    KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_R8 = 0u,
    KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_RG16,
    KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_RGB24,
    KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_RGBA32,

    KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_D16,
    KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_D32,

    // TODO: Compressed formats like BCn, ETC2 and ASTC will be added in the future on demand.
};

/// \brief Contains shared configuration for compiling textures.
struct kan_resource_texture_compilation_preset_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (enum kan_resource_texture_compiled_format_t)
    struct kan_dynamic_array_t supported_compiled_formats;
};

RESOURCE_TEXTURE_API void kan_resource_texture_compilation_preset_init (
    struct kan_resource_texture_compilation_preset_t *instance);

RESOURCE_TEXTURE_API void kan_resource_texture_compilation_preset_shutdown (
    struct kan_resource_texture_compilation_preset_t *instance);

/// \brief Contains top level information about the texture. Expected to be created manually, not imported.
struct kan_resource_texture_t
{
    /// \brief Name of the `kan_resource_texture_raw_data_t` resource with actual data.
    kan_interned_string_t raw_data;

    /// \brief Name of the `kan_resource_texture_compilation_preset_t` resource for compilation.
    kan_interned_string_t compilation_preset;

    /// \brief Count of mips to generate for this texture.
    kan_instance_size_t mips;
};

RESOURCE_TEXTURE_API void kan_resource_texture_init (struct kan_resource_texture_t *instance);

/// \brief Contains target platform configuration for compiling texture.
/// \details Must be present in order to make compilation possible.
struct kan_resource_texture_platform_configuration_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (enum kan_resource_texture_compiled_format_t)
    struct kan_dynamic_array_t supported_compiled_formats;
};

RESOURCE_TEXTURE_API void kan_resource_texture_platform_configuration_init (
    struct kan_resource_texture_platform_configuration_t *instance);

RESOURCE_TEXTURE_API void kan_resource_texture_platform_configuration_shutdown (
    struct kan_resource_texture_platform_configuration_t *instance);

/// \brief Contains compiled data for compiled texture.
/// \details Has only texture data, because format and other parameters are determined by how compiled texture
///          references compiled data.
struct kan_resource_texture_compiled_data_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (uint8_t)
    struct kan_dynamic_array_t data;
};

RESOURCE_TEXTURE_API void kan_resource_texture_compiled_data_init (
    struct kan_resource_texture_compiled_data_t *instance);

RESOURCE_TEXTURE_API void kan_resource_texture_compiled_data_shutdown (
    struct kan_resource_texture_compiled_data_t *instance);

/// \brief Contains references to compiled data for particular compiled formats.
struct kan_resource_texture_compiled_format_item_t
{
    /// \brief Format to which texture is compiled.
    enum kan_resource_texture_compiled_format_t format;

    /// \brief Array of compiled data resource names for every mip in ascending mip order.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t compiled_data_per_mip;
};

RESOURCE_TEXTURE_API void kan_resource_texture_compiled_format_item_init (
    struct kan_resource_texture_compiled_format_item_t *instance);

RESOURCE_TEXTURE_API void kan_resource_texture_compiled_format_item_shutdown (
    struct kan_resource_texture_compiled_format_item_t *instance);

/// \brief Contains compiled texture information excluding actual texture data.
struct kan_resource_texture_compiled_t
{
    kan_instance_size_t width;
    kan_instance_size_t height;
    kan_instance_size_t depth;
    kan_instance_size_t mips;

    /// \brief Contains information about compiled data for every compiled format.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_texture_compiled_format_item_t)
    struct kan_dynamic_array_t compiled_formats;
};

RESOURCE_TEXTURE_API void kan_resource_texture_compiled_init (struct kan_resource_texture_compiled_t *instance);

RESOURCE_TEXTURE_API void kan_resource_texture_compiled_shutdown (struct kan_resource_texture_compiled_t *instance);

KAN_C_HEADER_END
