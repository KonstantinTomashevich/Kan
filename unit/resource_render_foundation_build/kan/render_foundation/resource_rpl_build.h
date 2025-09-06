#pragma once

#include <resource_render_foundation_build_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/interned_string.h>
#include <kan/context/render_backend_system.h>
#include <kan/error/critical.h>
#include <kan/reflection/markup.h>
#include <kan/render_pipeline_language/parser.h>

/// \file
/// \brief This file stores helper types for parsing render pipeline language sources.
///
/// \par Render pipeline language source resource
/// \parblock
/// `kan_resource_rpl_source_t` is a resource that is produced through import-rule from one render pipeline language
/// source file. This resource is advised everywhere when user needs to parse render pipeline language code, because it
/// makes it possible to avoid parsing the same file several times in several places.
/// \endparblock
///
/// \par Render pipeline language options
/// \parblock
/// `kan_resource_rpl_options_t` is a convenient storage type for storing options for compiling render pipeline language
/// code in other resource types.
/// \endparblock
///
/// \par Render pipeline building
/// \parblock
/// `kan_resource_rpl_pipeline_header_t` provides generic way for building render pipelines with their code and meta for
/// target platform. `kan_resource_rpl_pipeline_header_t` contains all the data needed to compile and arbitrary pipeline
/// and `kan_resource_rpl_pipeline_t` with compiled data is produced through build rule.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Contains target platform configuration for building render passes and materials.
struct kan_resource_render_code_platform_configuration_t
{
    /// \brief Intermediate format to which pipelines should be compiled.
    enum kan_render_code_format_t code_format;

    /// \brief List of render pass tags that can be used to decide whether pass is supported on this platform.
    /// \details Main goal of support tags is to exclude excessive passes like editor-only passes from build, but
    ///          it can be used to customize passes for different platforms too.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t supported_pass_tags;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_render_code_platform_configuration_init (
    struct kan_resource_render_code_platform_configuration_t *instance);

// Forward-declare existence of pass headers as parameters for function.
struct kan_resource_render_pass_header_t;

RESOURCE_RENDER_FOUNDATION_BUILD_API bool kan_resource_render_code_platform_configuration_is_pass_supported (
    const struct kan_resource_render_code_platform_configuration_t *instance,
    const struct kan_resource_render_pass_header_t *pass);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_render_code_platform_configuration_shutdown (
    struct kan_resource_render_code_platform_configuration_t *instance);

/// \brief Contains parsed rpl source, produced from import-rule.
struct kan_resource_rpl_source_t
{
    struct kan_rpl_intermediate_t intermediate;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_rpl_source_init (struct kan_resource_rpl_source_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_rpl_source_shutdown (struct kan_resource_rpl_source_t *instance);

/// \brief Describes flag option and its value for pipeline setup.
struct kan_resource_rpl_flag_option_t
{
    kan_interned_string_t name;
    bool value;
};

/// \brief Describes uint option and its value for pipeline setup.
struct kan_resource_rpl_uint_option_t
{
    kan_interned_string_t name;
    kan_rpl_unsigned_int_literal_t value;
};

/// \brief Describes sint option and its value for pipeline setup.
struct kan_resource_rpl_sint_option_t
{
    kan_interned_string_t name;
    kan_rpl_signed_int_literal_t value;
};

/// \brief Describes float option and its value for pipeline setup.
struct kan_resource_rpl_float_option_t
{
    kan_interned_string_t name;
    kan_rpl_floating_t value;
};

/// \brief Describes enum option and its value for pipeline setup.
struct kan_resource_rpl_enum_option_t
{
    kan_interned_string_t name;
    kan_interned_string_t value;
};

/// \brief Utility structure with option storages for pipelines: materials or passes.
struct kan_resource_rpl_options_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_rpl_flag_option_t)
    struct kan_dynamic_array_t flags;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_rpl_uint_option_t)
    struct kan_dynamic_array_t uints;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_rpl_sint_option_t)
    struct kan_dynamic_array_t sints;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_rpl_float_option_t)
    struct kan_dynamic_array_t floats;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_rpl_enum_option_t)
    struct kan_dynamic_array_t enums;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_rpl_options_init (struct kan_resource_rpl_options_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_rpl_options_init_copy (
    struct kan_resource_rpl_options_t *instance, const struct kan_resource_rpl_options_t *copy_from);

/// \brief Appends options from given container, overriding present values of options with equal names.
RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_rpl_options_append (
    struct kan_resource_rpl_options_t *instance, const struct kan_resource_rpl_options_t *override);

/// \brief Applies all options to given compiler context.
RESOURCE_RENDER_FOUNDATION_BUILD_API bool kan_resource_rpl_options_apply (
    const struct kan_resource_rpl_options_t *options,
    kan_rpl_compiler_context_t compiler_context,
    enum kan_rpl_option_target_scope_t target_scope);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_rpl_options_shutdown (
    struct kan_resource_rpl_options_t *instance);

/// \brief Contains input data needed to build a GPU pipeline code and meta.
struct kan_resource_rpl_pipeline_header_t
{
    enum kan_rpl_pipeline_type_t type;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_entry_point_t)
    struct kan_dynamic_array_t entry_points;

    /// \brief References `kan_resource_rpl_source_t` resources.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t sources;

    /// \brief Options to be applied as global scope options.
    struct kan_resource_rpl_options_t global_options;

    /// \brief Options to be applied as instance scope options.
    struct kan_resource_rpl_options_t instance_options;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_rpl_pipeline_header_init (
    struct kan_resource_rpl_pipeline_header_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_rpl_pipeline_header_shutdown (
    struct kan_resource_rpl_pipeline_header_t *instance);

/// \brief Contains compiled GPU pipeline code and meta.
struct kan_resource_rpl_pipeline_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_rpl_entry_point_t)
    struct kan_dynamic_array_t entry_points;

    struct kan_rpl_meta_t meta;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (uint8_t)
    struct kan_dynamic_array_t code;
};

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_rpl_pipeline_init (struct kan_resource_rpl_pipeline_t *instance);

RESOURCE_RENDER_FOUNDATION_BUILD_API void kan_resource_rpl_pipeline_shutdown (
    struct kan_resource_rpl_pipeline_t *instance);

KAN_C_HEADER_END
