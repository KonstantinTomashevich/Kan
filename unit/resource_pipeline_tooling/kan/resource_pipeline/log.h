#pragma once

#include <resource_pipeline_tooling_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/reflection/markup.h>
#include <kan/resource_pipeline/tooling_meta.h>

// TODO: Docs later. Log structure might change a lot during implementation,
//       so it is ineffective to document it right now.

KAN_C_HEADER_BEGIN

RESOURCE_PIPELINE_TOOLING_API kan_allocation_group_t kan_resource_log_get_allocation_group (void);

/// \brief Flags for detected resource reference.
/// \details We use different flags for meta and log as meta flags should provide the most obvious defaults for the
///          user and log flags should be mergeable through bitwise or in a logical way, which results in logical
///          conflicts in some cases.
enum kan_resource_reference_flags_t
{
    /// \brief Enabled if none of the actual references has KAN_RESOURCE_REFERENCE_META_PLATFORM_OPTIONAL flag.
    KAN_RESOURCE_REFERENCE_REQUIRED = 1u << 0u,
};

struct kan_resource_log_reference_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    enum kan_resource_reference_flags_t flags;
};

struct kan_resource_log_version_t
{
    kan_resource_version_t type_version;
    kan_time_size_t last_modification_time;
};

static inline bool kan_resource_log_version_is_up_to_date (struct kan_resource_log_version_t logged,
                                                           struct kan_resource_log_version_t detected)
{
    return logged.type_version == detected.type_version &&
           logged.last_modification_time == detected.last_modification_time;
}

struct kan_resource_log_raw_entry_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    struct kan_resource_log_version_t version;
    bool deployed;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_reference_t)
    struct kan_dynamic_array_t references;
};

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_raw_entry_init (struct kan_resource_log_raw_entry_t *instance);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_raw_entry_init_copy (
    struct kan_resource_log_raw_entry_t *instance, const struct kan_resource_log_raw_entry_t *copy_from);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_raw_entry_shutdown (struct kan_resource_log_raw_entry_t *instance);

enum kan_resource_log_saved_directory_t
{
    KAN_RESOURCE_LOG_SAVED_DIRECTORY_DEPLOY = 0u,
    KAN_RESOURCE_LOG_SAVED_DIRECTORY_CACHE,

    /// \brief Special value for platform unsupported resources that we still need to record in the log file.
    KAN_RESOURCE_LOG_SAVED_DIRECTORY_UNSUPPORTED,
};

/// \details Can point to any type of entry (raw, built, secondary) and even to third party binaries. Third party
///          binaries are not de-facto entries as we never deploy them or process in any way except for build rules.
///          Which means that having only data that is present here is enough for processing third party inputs.
struct kan_resource_log_secondary_input_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    struct kan_resource_log_version_t version;
};

struct kan_resource_log_built_entry_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    struct kan_resource_log_version_t version;

    kan_time_size_t platform_configuration_time;
    kan_resource_version_t rule_version;
    struct kan_resource_log_version_t primary_input_version;
    enum kan_resource_log_saved_directory_t saved_directory;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_reference_t)
    struct kan_dynamic_array_t references;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_secondary_input_t)
    struct kan_dynamic_array_t secondary_inputs;
};

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_built_entry_init (struct kan_resource_log_built_entry_t *instance);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_built_entry_init_copy (
    struct kan_resource_log_built_entry_t *instance, const struct kan_resource_log_built_entry_t *copy_from);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_built_entry_shutdown (
    struct kan_resource_log_built_entry_t *instance);

struct kan_resource_log_secondary_entry_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    struct kan_resource_log_version_t version;
    enum kan_resource_log_saved_directory_t saved_directory;

    kan_interned_string_t producer_type;
    kan_interned_string_t producer_name;
    struct kan_resource_log_version_t producer_version;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_reference_t)
    struct kan_dynamic_array_t references;
};

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_secondary_entry_init (
    struct kan_resource_log_secondary_entry_t *instance);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_secondary_entry_init_copy (
    struct kan_resource_log_secondary_entry_t *instance, const struct kan_resource_log_secondary_entry_t *copy_from);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_secondary_entry_shutdown (
    struct kan_resource_log_secondary_entry_t *instance);

struct kan_resource_log_target_t
{
    kan_interned_string_t name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_raw_entry_t)
    struct kan_dynamic_array_t raw;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_built_entry_t)
    struct kan_dynamic_array_t built;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_secondary_entry_t)
    struct kan_dynamic_array_t secondary;
};

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_target_init (struct kan_resource_log_target_t *instance);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_target_init_copy (
    struct kan_resource_log_target_t *instance, const struct kan_resource_log_target_t *copy_from);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_target_shutdown (struct kan_resource_log_target_t *instance);

/// \brief Default name for resource log file.
#define KAN_RESOURCE_LOG_DEFAULT_NAME ".resource_log"

struct kan_resource_log_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_target_t)
    struct kan_dynamic_array_t targets;
};

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_init (struct kan_resource_log_t *instance);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_shutdown (struct kan_resource_log_t *instance);

KAN_C_HEADER_END
