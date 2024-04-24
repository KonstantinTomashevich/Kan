#pragma once

#include <universe_resource_provider_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/container/interned_string.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/threading/atomic.h>

KAN_C_HEADER_BEGIN

// TODO: Docs.

#define KAN_RESOURCE_PROVIDER_MUTATOR_GROUP "resource_provider"
#define KAN_RESOURCE_PROVIDER_CONFIGURATION "resource_provider"
#define KAN_RESOURCE_PROVIDER_CONTAINER_TYPE_PREFIX "resource_provider_container_"

#define KAN_RESOURCE_PROVIDER_BEGIN_CHECKPOINT "resource_provider_begin"
#define KAN_RESOURCE_PROVIDER_END_CHECKPOINT "resource_provider_end"

#define KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE 0u

struct kan_resource_third_party_data_t
{
    void *data;
    uint64_t size;
};

struct kan_resource_request_t
{
    uint64_t request_id;

    /// \details Leave NULL for third party resource objects.
    kan_interned_string_t type;

    kan_interned_string_t name;
    uint64_t priority;

    union
    {
        uint64_t provided_container_id;
        struct kan_resource_third_party_data_t provided_third_party;
    };
};

UNIVERSE_RESOURCE_PROVIDER_API void kan_resource_request_init (struct kan_resource_request_t *instance);

/// \meta reflection_ignore_struct
struct kan_resource_container_view_t
{
    uint64_t container_id;
    kan_allocation_group_t my_allocation_group;

    uint8_t data_begin[];
};

struct kan_resource_request_updated_event_t
{
    uint64_t request_id;

    /// \details Providing type name here makes it possible to filter out requests of unsupported types faster.
    kan_interned_string_t type;
};

struct kan_resource_provider_singleton_t
{
    /// \meta reflection_ignore_struct_field
    struct kan_atomic_int_t request_id_counter;

    kan_bool_t request_rescan;
};

UNIVERSE_RESOURCE_PROVIDER_API void kan_resource_provider_singleton_init (
    struct kan_resource_provider_singleton_t *instance);

static inline uint64_t kan_next_resource_request_id (const struct kan_resource_provider_singleton_t *resource_provider)
{
    // Intentionally request const and de-const it to show that it is multithreading-safe function.
    return kan_atomic_int_add ((struct kan_atomic_int_t *) &resource_provider->request_id_counter, 1);
}

struct kan_resource_provider_configuration_t
{
    uint64_t scan_budget_ns;
    uint64_t load_budget_ns;
    uint64_t add_wait_time_ns;
    uint64_t modify_wait_time_ns;
    kan_bool_t use_load_only_string_registry;
    kan_bool_t observe_file_system;
    kan_interned_string_t resource_directory_path;
};

/// \brief Empty meta for marking types that should be supported by resource provider logic.
struct kan_resource_provider_type_meta_t
{
    uint64_t stub;
};

KAN_C_HEADER_END
