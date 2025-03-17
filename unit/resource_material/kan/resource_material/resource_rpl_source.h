#pragma once

#include <resource_material_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/interned_string.h>
#include <kan/error/critical.h>
#include <kan/reflection/markup.h>
#include <kan/render_pipeline_language/parser.h>

/// \file
/// \brief This file stored helper types for compiling render pipeline language sources.
///
/// \par Render pipeline language source resource
/// \parblock
/// `kan_resource_rpl_source_t` is a byproduct resource type, that contains only source file name in the raw format
/// and contains parsed intermediate data in compiled format. This resource is advised everywhere when user needs to
/// parse render pipeline language code, because it makes it possible to avoid parsing the same file several times
/// in several places.
/// \endparblock
///
/// \par Render pipeline language options
/// \parblock
/// `kan_resource_rpl_options_t` is a convenient storage type for storing options for compiling render pipeline language
/// code in other resource types.
/// \endparblock

KAN_C_HEADER_BEGIN

struct kan_resource_rpl_source_t
{
    kan_interned_string_t source;
};

struct kan_resource_rpl_source_compiled_t
{
    struct kan_rpl_intermediate_t intermediate;
};

RESOURCE_MATERIAL_API void kan_resource_rpl_source_compiled_init (struct kan_resource_rpl_source_compiled_t *instance);

RESOURCE_MATERIAL_API void kan_resource_rpl_source_compiled_shutdown (
    struct kan_resource_rpl_source_compiled_t *instance);

/// \brief Describes flag option and its value for pipeline setup.
struct kan_resource_rpl_flag_option_t
{
    kan_interned_string_t name;
    kan_bool_t value;
};

/// \brief Describes count option and its value for pipeline setup.
struct kan_resource_rpl_count_option_t
{
    kan_interned_string_t name;
    kan_rpl_unsigned_int_literal_t value;
};

/// \brief Utility structure with option storages for pipelines: materials or passes.
struct kan_resource_rpl_options_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_rpl_flag_option_t)
    struct kan_dynamic_array_t flags;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_rpl_count_option_t)
    struct kan_dynamic_array_t counts;
};

RESOURCE_MATERIAL_API void kan_resource_rpl_options_init (struct kan_resource_rpl_options_t *instance);

RESOURCE_MATERIAL_API void kan_resource_rpl_options_shutdown (struct kan_resource_rpl_options_t *instance);

KAN_C_HEADER_END
