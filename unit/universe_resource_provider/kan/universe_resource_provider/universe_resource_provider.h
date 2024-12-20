#pragma once

#include <universe_resource_provider_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/interned_string.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/threading/atomic.h>

/// \file
/// \brief Provides public API for Resource Provider extension unit for Universe unit.
///
/// \par Definition
/// \parblock
/// Resource provider goal is to provide mutators and types for solving the issue of low-level resource management:
///
/// - It scans resource directory for resources and registers found resources. Resources indices and accompanying
///   string registries are taken into account when found.
/// - If hot reload is enabled, it continues to monitor resource directory for changes and updates internal data
///   according to file system changes. Modified resources are automatically reloaded if it is allowed.
/// - It manages resource loading and unloading through requests system. Requests are automatically updated with the
///   newest possible data.
///
/// In other words, resource provider aims to provide easy and versatile backend API for more elaborate and high-level
/// resource management solutions.
/// \endparblock
///
/// \par Resources
/// \parblock
/// There are two categories of resources:
/// - Native resources can be represented by reflected type and therefore can be deserialized using binary or readable
///   data reader (depending on their format).
/// - Third party resources are any other resources of unknown type, that are loaded into memory as plain binary data.
///
/// To make reflected structure usable for native resources, it must first be registered as supported by resource
/// provider. It is required in order to generate appropriate accompanying reflection types that are used as utility
/// inside resource provider. In order to register type, instance of `kan_resource_resource_type_meta_t` meta
/// must be added to this type. With this meta, type will be automatically found and registered properly.
///
/// Native resource files should have "bin" or "rd" extensions depending on their format. Native resource name is its
/// file name without "bin" or "rd" extension.
///
/// Third party resource name is its file name with its extension.
/// \endparblock
///
/// \par Containers
/// \parblock
/// Loaded native resources are stored in special container types. Container types are automatically generated and are
/// named according to pattern "resource_provider_container_<native_type_name>". Structure of container is represented
/// by `kan_resource_container_view_t`. Keep in mind that if resource type requires alignment that is higher than
/// `offsetof (kan_resource_container_view_t, data_begin)`, its data won't start at `data_begin`, but at
/// `data_begin + (alignment - offsetof (kan_resource_container_view_t, data_begin))` (in order to align it properly).
/// \endparblock
///
/// \par Requests
/// \parblock
/// The main tool for communicating with resource provider is `kan_resource_request_t` instances. When higher level
/// logic needs to access resource data, it should create and fill request instance. Following fields need to be filled:
///
/// - `request_id`: call `kan_next_resource_request_id` to generate unique id for your request.
/// - `type`: name of native resource type or `NULL` for third party resources.
/// - `name`: name of the resource, unique along resources of the same type.
/// - `priority`: priority of the request. If resource is not loaded,
///               highest priority from resources used for loading operation.
///
/// When any resource request is changed, `kan_resource_request_updated_event_t` will be sent. For native types,
/// id of resource container is provided in `provided_container_id`. For third party types, `provided_third_party`
/// is filled with third party data pointer and third data size.
/// \endparblock
///
/// \par Configuration
/// \parblock
/// In order to configure how resource provider operates, its configuration must be added to its universe world under
/// name KAN_RESOURCE_PROVIDER_CONFIGURATION and this configuration should have `kan_resource_provider_configuration_t`
/// type. More about configuration variables in described in its documentation.
/// \endparblock
///
/// \par Entries
/// \parblock
/// Information about resources, visible to resource provider, is stored in globally accessible entries:
/// `kan_resource_native_entry_t` and `kan_resource_third_party_entry_t`. User can read these entries if it is needed
/// to query available resource for some operation.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Group, used to add all the mutators that implement resource provider extension.
#define KAN_RESOURCE_PROVIDER_MUTATOR_GROUP "resource_provider"

/// \brief Name for resource provider configuration object in universe world.
#define KAN_RESOURCE_PROVIDER_CONFIGURATION "resource_provider"

/// \brief Prefix for resource provider container type names.
#define KAN_RESOURCE_PROVIDER_CONTAINER_TYPE_PREFIX "resource_provider_container_"

/// \brief Checkpoint, after which resource provider mutators are executed.
#define KAN_RESOURCE_PROVIDER_BEGIN_CHECKPOINT "resource_provider_begin"

/// \brief Checkpoint, that is hit after all resource provider mutators finished execution.
#define KAN_RESOURCE_PROVIDER_END_CHECKPOINT "resource_provider_end"

KAN_TYPED_ID_32_DEFINE (kan_resource_request_id_t);
KAN_TYPED_ID_32_DEFINE (kan_resource_container_id_t);
KAN_TYPED_ID_32_DEFINE (kan_resource_entry_id_t);

/// \brief Provides data of loaded third party resource.
struct kan_resource_third_party_data_t
{
    /// \warning Might be `NULL` if resource cannot be loaded.
    void *data;

    kan_memory_size_t size;
};

/// \brief Minimum valid priority for resource requests.
/// \details Values outside min-max limits are used internally.
#define KAN_RESOURCE_PROVIDER_USER_PRIORITY_MIN 0u

/// \brief Maximum valid priority for resource requests.
/// \details Values outside min-max limits are used internally.
#define KAN_RESOURCE_PROVIDER_USER_PRIORITY_MAX (KAN_INT_MAX (kan_instance_size_t) / 2u)

/// \brief Instance of resource request, used to communicate with resource provider.
struct kan_resource_request_t
{
    /// \brief Unique id of this request.
    kan_resource_request_id_t request_id;

    /// \brief Type of native resource.
    /// \details Set to NULL for third party resource objects.
    kan_interned_string_t type;

    /// \brief Name of the requested resource.
    kan_interned_string_t name;

    /// \brief Priority for resource loading if it is not already loaded.
    kan_instance_size_t priority;

    union
    {
        /// \brief Id of container with loaded resource if any.
        kan_resource_container_id_t provided_container_id;

        /// \brief Loaded third party data if any.
        struct kan_resource_third_party_data_t provided_third_party;
    };
};

UNIVERSE_RESOURCE_PROVIDER_API void kan_resource_request_init (struct kan_resource_request_t *instance);

/// \brief Struct that mimics data layout of native resource containers.
/// \meta reflection_ignore_struct
struct kan_resource_container_view_t
{
    kan_resource_container_id_t container_id;
    kan_allocation_group_t my_allocation_group;

    uint8_t data_begin[];
};

/// \brief Event that is send when resource request is updated.
struct kan_resource_request_updated_event_t
{
    /// \brief Id of a request that is being updated.
    kan_resource_request_id_t request_id;

    /// \details Providing type name here makes it possible to filter out requests of unsupported types faster.
    kan_interned_string_t type;
};

/// \brief Singleton instance for assigning request ids and for requesting common operations.
struct kan_resource_provider_singleton_t
{
    /// \brief Atomic counter for assigning request ids. Safe to be modified from different threads.
    /// \meta reflection_ignore_struct_field
    struct kan_atomic_int_t request_id_counter;

    /// \brief When set to true, resource provider will stop serving, unload all the data and start scan from scratch.
    /// \details Automatically reset to false when scanning is started.
    kan_bool_t request_reset;

    /// \brief Whether resource provider finished scanning for resources and is able to provide full list of entries.
    kan_bool_t scan_done;
};

UNIVERSE_RESOURCE_PROVIDER_API void kan_resource_provider_singleton_init (
    struct kan_resource_provider_singleton_t *instance);

/// \brief Inline helper for generation resource request ids.
static inline kan_resource_request_id_t kan_next_resource_request_id (
    const struct kan_resource_provider_singleton_t *resource_provider)
{
    // Intentionally request const and de-const it to show that it is multithreading-safe function.
    return KAN_TYPED_ID_32_SET (
        kan_resource_request_id_t,
        (kan_id_32_t) kan_atomic_int_add ((struct kan_atomic_int_t *) &resource_provider->request_id_counter, 1));
}

/// \brief Structure that contains configuration for resource provider.
struct kan_resource_provider_configuration_t
{
    /// \brief How much time in nanoseconds should be spent scanning for resources during update.
    kan_time_offset_t scan_budget_ns;

    /// \brief How much time in nanoseconds should be spent loading resources during update.
    /// \details Or compiling if runtime compilation is enabled.
    kan_time_offset_t serve_budget_ns;

    /// \brief Whether string registries should be loaded in load-only mode.
    /// \details Generally, should always be true as resource provider does not save assets at the moment.
    kan_bool_t use_load_only_string_registry;

    /// \brief Path to virtual directory with resources, that is used as resource root directory.
    kan_interned_string_t resource_directory_path;
};

/// \brief Provides information about native resource entry visible to resource provider.
struct kan_resource_native_entry_t
{
    /// \brief Id for attaching additional data to the entry.
    kan_resource_entry_id_t attachment_id;

    kan_interned_string_t type;
    kan_interned_string_t name;
    char *path;
    kan_allocation_group_t my_allocation_group;
};

UNIVERSE_RESOURCE_PROVIDER_API void kan_resource_native_entry_init (struct kan_resource_native_entry_t *instance);

UNIVERSE_RESOURCE_PROVIDER_API void kan_resource_native_entry_shutdown (struct kan_resource_native_entry_t *instance);

/// \brief Provides information about third party resource entry visible to resource provider.
struct kan_resource_third_party_entry_t
{
    /// \brief Id for attaching additional data to the entry.
    kan_resource_entry_id_t attachment_id;

    kan_interned_string_t name;
    kan_memory_size_t size;
    char *path;
    kan_allocation_group_t my_allocation_group;
};

UNIVERSE_RESOURCE_PROVIDER_API void kan_resource_third_party_entry_init (
    struct kan_resource_third_party_entry_t *instance);

UNIVERSE_RESOURCE_PROVIDER_API void kan_resource_third_party_entry_shutdown (
    struct kan_resource_third_party_entry_t *instance);

/// \brief Event that is send when known resource entry is changed or removed.
/// \details Resource entry can have no requests, but this event will still be sent if resource entry is known to
///          provider. When runtime compilation is enabled, it is also sent for compiled resources, that were produced
///          at least once and therefore are known to the resource provider.
/// \warning This event does not mean that data in requests is updated. Its goal is to inform about change of
///          potentially outdated resources. For request updates, use `kan_resource_request_updated_event_t`.
struct kan_resource_entry_changed_event_t
{
    /// \details NULL for third party entries.
    kan_interned_string_t type;

    kan_interned_string_t name;
};

KAN_C_HEADER_END
