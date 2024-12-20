#pragma once

#include <application_framework_resource_builder_api.h>

#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/hash/hash.h>
#include <kan/reflection/markup.h>

/// \brief Contains information about one byproduct production event.
struct kan_resource_target_byproduct_production_t
{
    kan_interned_string_t resource_type;
    kan_interned_string_t resource_name;
    kan_interned_string_t byproduct_type;
    kan_interned_string_t byproduct_name;
};

/// \brief Contains cached information about byproduct production to make correct byproduct processing possible when
///        compilation of up to date resources is skipped.
struct kan_resource_target_byproduct_state_t
{
    /// \brief Generator integer value that is used to generate byproduct indices.
    kan_instance_size_t byproduct_index_generator;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_target_byproduct_production_t)
    struct kan_dynamic_array_t production;
};

APPLICATION_FRAMEWORK_RESOURCE_BUILDER_API void kan_resource_target_byproduct_state_init (
    struct kan_resource_target_byproduct_state_t *instance);

APPLICATION_FRAMEWORK_RESOURCE_BUILDER_API void kan_resource_target_byproduct_state_shutdown (
    struct kan_resource_target_byproduct_state_t *instance);
