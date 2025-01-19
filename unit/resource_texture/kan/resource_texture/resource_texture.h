#pragma once

#include <resource_texture_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/error/critical.h>
#include <kan/memory/allocation.h>
#include <kan/reflection/markup.h>

// TODO: Docs.

KAN_C_HEADER_BEGIN

enum kan_resource_texture_raw_format_t
{
    KAN_RESOURCE_TEXTURE_RAW_FORMAT_R8 = 0u,
    KAN_RESOURCE_TEXTURE_RAW_FORMAT_RG16,
    KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGB24,
    KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGBA32,

    /// \brief Depth values stored as 16-bit unsigned normalized integers.
    KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH16,

    /// \brief Depth values stored as 32 bit floats.
    KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH32,
};

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

struct kan_resource_texture_compilation_preset_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (enum kan_resource_texture_compiled_format_t)
    struct kan_dynamic_array_t supported_compiled_formats;
};

RESOURCE_TEXTURE_API void kan_resource_texture_compilation_preset_init (
    struct kan_resource_texture_compilation_preset_t *instance);

RESOURCE_TEXTURE_API void kan_resource_texture_compilation_preset_shutdown (
    struct kan_resource_texture_compilation_preset_t *instance);

struct kan_resource_texture_t
{
    kan_interned_string_t raw_data;
    kan_interned_string_t compilation_preset;
    kan_instance_size_t mips;
};

RESOURCE_TEXTURE_API void kan_resource_texture_init (struct kan_resource_texture_t *instance);

struct kan_resource_texture_platform_configuration_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (enum kan_resource_texture_compiled_format_t)
    struct kan_dynamic_array_t supported_compiled_formats;
};

RESOURCE_TEXTURE_API void kan_resource_texture_platform_configuration_init (
    struct kan_resource_texture_platform_configuration_t *instance);

RESOURCE_TEXTURE_API void kan_resource_texture_platform_configuration_shutdown (
    struct kan_resource_texture_platform_configuration_t *instance);

struct kan_resource_texture_compiled_data_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (uint8_t)
    struct kan_dynamic_array_t data;
};

RESOURCE_TEXTURE_API void kan_resource_texture_compiled_data_init (
    struct kan_resource_texture_compiled_data_t *instance);

RESOURCE_TEXTURE_API void kan_resource_texture_compiled_data_shutdown (
    struct kan_resource_texture_compiled_data_t *instance);

struct kan_resource_texture_compiled_format_item_t
{
    enum kan_resource_texture_compiled_format_t format;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t compiled_data_per_mip;
};

RESOURCE_TEXTURE_API void kan_resource_texture_compiled_format_item_init (
    struct kan_resource_texture_compiled_format_item_t *instance);

RESOURCE_TEXTURE_API void kan_resource_texture_compiled_format_item_shutdown (
    struct kan_resource_texture_compiled_format_item_t *instance);

struct kan_resource_texture_compiled_t
{
    kan_instance_size_t width;
    kan_instance_size_t height;
    kan_instance_size_t depth;
    kan_instance_size_t mips;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_texture_compiled_format_item_t)
    struct kan_dynamic_array_t compiled_formats;
};

RESOURCE_TEXTURE_API void kan_resource_texture_compiled_init (struct kan_resource_texture_compiled_t *instance);

RESOURCE_TEXTURE_API void kan_resource_texture_compiled_shutdown (struct kan_resource_texture_compiled_t *instance);

KAN_C_HEADER_END
