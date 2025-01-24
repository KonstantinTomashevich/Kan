#pragma once

#include <resource_material_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/context/render_backend_system.h>
#include <kan/error/critical.h>
#include <kan/reflection/markup.h>
#include <kan/render_pipeline_language/compiler.h>

// TODO: Docs.

KAN_C_HEADER_BEGIN

struct kan_resource_material_flag_option_t
{
    kan_interned_string_t name;
    kan_bool_t value;
};

struct kan_resource_material_count_option_t
{
    kan_interned_string_t name;
    kan_rpl_unsigned_int_literal_t value;
};

struct kan_resource_material_options_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_flag_option_t)
    struct kan_dynamic_array_t flags;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_count_option_t)
    struct kan_dynamic_array_t counts;
};

RESOURCE_MATERIAL_API void kan_resource_material_options_init (struct kan_resource_material_options_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_options_shutdown (struct kan_resource_material_options_t *instance);

struct kan_resource_material_pass_t
{
    kan_interned_string_t name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_entry_point_t)
    struct kan_dynamic_array_t entry_points;

    struct kan_resource_material_options_t options;
};

RESOURCE_MATERIAL_API void kan_resource_material_pass_init (struct kan_resource_material_pass_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_pass_shutdown (struct kan_resource_material_pass_t *instance);

struct kan_resource_material_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t sources;

    struct kan_resource_material_options_t global_options;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_pass_t)
    struct kan_dynamic_array_t passes;
};

RESOURCE_MATERIAL_API void kan_resource_material_init (struct kan_resource_material_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_shutdown (struct kan_resource_material_t *instance);

struct kan_resource_material_platform_configuration_t
{
    enum kan_render_code_format_t code_format;
};

RESOURCE_MATERIAL_API void kan_resource_material_platform_configuration_init (
    struct kan_resource_material_platform_configuration_t *instance);

struct kan_resource_material_meta_compiled_t
{
    struct kan_rpl_meta_t meta;
};

RESOURCE_MATERIAL_API void kan_resource_material_meta_compiled_init (
    struct kan_resource_material_meta_compiled_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_meta_compiled_shutdown (
    struct kan_resource_material_meta_compiled_t *instance);

struct kan_resource_material_pipeline_compiled_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_entry_point_t)
    struct kan_dynamic_array_t entry_points;

    enum kan_render_code_format_t code_format;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (uint8_t)
    struct kan_dynamic_array_t code;
};

RESOURCE_MATERIAL_API void kan_resource_material_pipeline_compiled_init (
    struct kan_resource_material_pipeline_compiled_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_pipeline_compiled_shutdown (
    struct kan_resource_material_pipeline_compiled_t *instance);

struct kan_resource_material_pass_compiled_t
{
    kan_interned_string_t name;
    kan_interned_string_t pipeline;
};

struct kan_resource_material_compiled_t
{
    kan_interned_string_t meta;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_pass_compiled_t)
    struct kan_dynamic_array_t passes;
};

RESOURCE_MATERIAL_API void kan_resource_material_compiled_init (struct kan_resource_material_compiled_t *instance);

RESOURCE_MATERIAL_API void kan_resource_material_compiled_shutdown (struct kan_resource_material_compiled_t *instance);

KAN_C_HEADER_END
