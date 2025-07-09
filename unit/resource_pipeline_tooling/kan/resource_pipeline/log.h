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

// TODO: Remark. Log should contain full info about all targets.
//       Targets that were out of scope should just preserve their state.
//       That is also the reason why we don't need "referenced from targets" data in entries:
//       we could gather all references from all targets anyway, no need for duplication.

// TODO: Remark. When error happens, but some compilations were still successful, they should be written to log
//       and log should still be updated. The question is what to do with erred entries: do not write them or
//       mark them with some kind of erred flag. Technically, not writing them might be better for hot reload.

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

    /// \details We need to save last modification time during last access due to possible peculiar race condition
    ///          with file modification times: there is a gap between resource access and produced resource save (for
    ///          example), which means that in case of hot reload user changes might be saved during that gap and never
    ///          be reflected in built data due to this. To combat this race condition we save modification time here,
    ///          which means that if such save happened, it will be seen and processed during next execution.
    kan_time_size_t last_access_modification_time;
};

struct kan_resource_log_raw_entry_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    struct kan_resource_log_version_t version;

    /// \details Raw files might need to be deployed, for example root files. Therefore, we always resave them in binary
    ///          format in cache so we can easily deploy them. This path points to cached or deployed binary, not actual
    ///          raw file.
    const char *saved_virtual_path;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_reference_t)
    struct kan_dynamic_array_t references;
};

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_raw_entry_init (struct kan_resource_log_raw_entry_t *instance);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_raw_entry_shutdown (struct kan_resource_log_raw_entry_t *instance);

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

    kan_time_size_t platform_configuration_time;
    struct kan_resource_log_version_t primary_version;
    kan_resource_version_t rule_version;

    const char *saved_virtual_path;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_reference_t)
    struct kan_dynamic_array_t references;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_secondary_input_t)
    struct kan_dynamic_array_t secondary_inputs;
};

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_built_entry_init (struct kan_resource_log_built_entry_t *instance);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_built_entry_shutdown (
    struct kan_resource_log_built_entry_t *instance);

struct kan_resource_log_secondary_entry_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    struct kan_resource_log_version_t version;

    const char *saved_virtual_path;
    kan_hash_t hash_if_mergeable;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_reference_t)
    struct kan_dynamic_array_t references;
};

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_secondary_entry_init (
    struct kan_resource_log_secondary_entry_t *instance);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_secondary_entry_shutdown (
    struct kan_resource_log_secondary_entry_t *instance);

struct kan_resource_log_target_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_raw_entry_t)
    struct kan_dynamic_array_t raw;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_built_entry_t)
    struct kan_dynamic_array_t built;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_secondary_entry_t)
    struct kan_dynamic_array_t secondary;
};

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_target_init (struct kan_resource_log_target_t *instance);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_target_shutdown (struct kan_resource_log_target_t *instance);

struct kan_resource_log_t
{
    // TODO: Build tool version should be written as first 64-bit unsigned integer before any data is read.
    //       We cannot store build tool version transparently in the structure as trying to read outdated structure
    //       with binary serialization might cause crash.

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_target_t)
    struct kan_dynamic_array_t targets;
};

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_init (struct kan_resource_log_t *instance);

RESOURCE_PIPELINE_TOOLING_API void kan_resource_log_shutdown (struct kan_resource_log_t *instance);

KAN_C_HEADER_END
