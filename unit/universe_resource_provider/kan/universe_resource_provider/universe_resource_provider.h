#pragma once

#include <universe_resource_provider_api.h>

#include <kan/api_common/alignment.h>
#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/interned_string.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/reflection/markup.h>
#include <kan/serialization/binary.h>
#include <kan/threading/atomic.h>
#include <kan/universe/macro.h>

/// \file
/// \brief Provides public API for resource provider extension unit for universe unit.
///
/// \par Definition
/// \parblock
/// Resource provider is designed as a common solution for working with resources that are built using resource pipeline
/// build tool. It can be used to load resources, inspect available resources and get notifications about resource
/// changes as part of hot reload routine. It automatically reloads changed resources if hot reload is allowed. It also
/// takes care of string interning if it is used.
/// \endparblock
///
/// \par Configuration
/// \parblock
/// World-level configuration of type `kan_resource_provider_configuration_t` and with name
/// `KAN_RESOURCE_PROVIDER_CONFIGURATION` is used to provide high-level global configuration for resource provider.
/// \endparblock
///
/// \par Entries
/// \parblock
/// Entries are used to represent visible resources. For every visible resource, separate generic entry and typed
/// entry are created with shared `kan_resource_entry_id_t`.
///
/// `kan_resource_generic_entry_t` is used to store common information about the resource, primarily its type and name,
/// but also usage counter, path and hot reload information.
///
/// For every resource type, typed entry type with name that follows `KAN_RESOURCE_PROVIDER_MAKE_TYPED_ENTRY_TYPE`
/// pattern is created that follows pattern defined by view type `kan_resource_typed_entry_view_t`. Primary reason for
/// having typed entries is that it can make loaded resource querying more convenient for the end user: when typed entry
/// exists, this entry and its contents can be queried by single name field, but when everything is in generic entry,
/// loop with type check would be needed. Therefore, loading-stage data including loaded and loading containers ids is
/// moved to typed entries to make access easier.
///
/// The advised way to query potentially loaded resource is to use `KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED` helper macro,
/// that will populate variable with given name with non-NULL value if resource is actually loaded.
/// \endparblock
///
/// \par Containers
/// \parblock
/// For every resource type, special container record type is generated with name that follows
/// `KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE` pattern where loaded or loading entry can be stored. All container
/// entries follow `kan_resource_container_view_t` pattern, however actual data might start not from `data_begin` field,
/// but further down (only if it is required by alignment rules). It is advised to use macro
/// `KAN_RESOURCE_PROVIDER_CONTAINER_GET` if you need to manually extract data from a container record.
/// \endparblock
///
/// \par Usages
/// \parblock
/// Usages are records of `kan_resource_usage_t` type that are used to inform resource provider that we'd like to see
/// specified resource among loaded resources. Resource is only loaded if at least one usage references it. If there
/// are no usages left, resource will be unloaded.
///
/// Usages are intended to be never changed after their insertion, therefore provider will not observe them for changes.
/// If usage needs to be changed, it should be deleted and new usage should be inserted. The reason for that is because
/// in real use case multiple different mutator groups would like to insert their usages and delete their usages without
/// introducing dependencies between each other. It is possible to do safely as long as all groups declare that they
/// only use insert and detach queries.
/// \endparblock
///
/// \par Events
/// \parblock
/// For every resource type, registered event type is created with name that follows
/// `KAN_RESOURCE_PROVIDER_REGISTERED_EVENT_TYPE_FORMAT` and content that follows
/// `kan_resource_registered_event_view_t`. These events are fired when new resource entry is created during scan or
/// when hot reload is detected. Separate event type is created for every resource type as in most cases users only need
/// this events for very specific resource types. Macro `KAN_UML_RESOURCE_REGISTERED_EVENT_FETCH` is advised for
/// fetching these events.
///
/// When resource change due to hot reload is detected, `kan_resource_updated_event_t` is sent. However, it only informs
/// that change was detected. It would be reloaded later if it is loaded right now and regular loaded event will be sent
/// when it is done.
///
/// For every resource type, loaded event type is created with name that follows
/// `KAN_RESOURCE_PROVIDER_LOADED_EVENT_TYPE_FORMAT` and content that follows `kan_resource_loaded_event_view_t`.
/// These events are fired when used resource entry is loaded or reloaded due to hot reload. Separate event type is
/// created for every resource type as users might need to use type-based ordering while processing these events.
/// Macro `KAN_UML_RESOURCE_LOADED_EVENT_FETCH` is advised for fetching these events.
/// \endparblock
///
/// \par Hot reload
/// \parblock
/// When `kan_hot_reload_coordination_system_is_possible` is `true` and hot reload coordination system is present,
/// hot reload will be enabled and resource watcher will be configured to watch mounted resources for changes. Changes
/// might not be applied right away, especially due to the fact that addition/modification events do not work as
/// transaction end events, therefore we must wait to make sure that new or updated resources are not being written
/// to right now.
///
/// Also, even when resource is removed from file system, its entry is not deleted as its loaded data can still be
/// used somewhere. However, when that data is no longer used and unloaded, trying to load it again by adding new
/// usages will fail as loading would not be possible anymore.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Group, used to add all the mutators that implement resource provider extension.
#define KAN_RESOURCE_PROVIDER_MUTATOR_GROUP "resource_provider"

/// \brief Name for resource provider configuration object in universe world.
#define KAN_RESOURCE_PROVIDER_CONFIGURATION "resource_provider"

/// \brief Checkpoint, after which resource provider mutators are executed.
#define KAN_RESOURCE_PROVIDER_BEGIN_CHECKPOINT "resource_provider_begin"

/// \brief Checkpoint, that is hit after all resource provider mutators finished execution.
#define KAN_RESOURCE_PROVIDER_END_CHECKPOINT "resource_provider_end"

/// \brief Convenience macro for making resource typed entry types from their resource types.
#define KAN_RESOURCE_PROVIDER_MAKE_TYPED_ENTRY_TYPE(RESOURCE_TYPE) resource_provider_typed_entry_##RESOURCE_TYPE

/// \brief Macro that provides formatting string used to create resource provider typed entry type names.
#define KAN_RESOURCE_PROVIDER_TYPED_ENTRY_TYPE_FORMAT "resource_provider_typed_entry_%s"

/// \brief Convenience macro for making resource container types from their resource types.
#define KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE(RESOURCE_TYPE) resource_provider_container_##RESOURCE_TYPE

/// \brief Macro that provides formatting string used to create resource provider container type names.
#define KAN_RESOURCE_PROVIDER_CONTAINER_TYPE_FORMAT "resource_provider_container_%s"

/// \brief Convenience macro for making resource typed registered event types from their resource types.
#define KAN_RESOURCE_PROVIDER_MAKE_REGISTERED_EVENT_TYPE(RESOURCE_TYPE)                                                \
    resource_provider_registered_event_##RESOURCE_TYPE

/// \brief Macro that provides formatting string used to create resource provider typed registered event type names.
#define KAN_RESOURCE_PROVIDER_REGISTERED_EVENT_TYPE_FORMAT "resource_provider_registered_event_%s"

/// \brief Convenience macro for making resource typed loaded event types from their resource types.
#define KAN_RESOURCE_PROVIDER_MAKE_LOADED_EVENT_TYPE(RESOURCE_TYPE) resource_provider_loaded_event_##RESOURCE_TYPE

/// \brief Macro that provides formatting string used to create resource provider typed loaded event type names.
#define KAN_RESOURCE_PROVIDER_LOADED_EVENT_TYPE_FORMAT "resource_provider_loaded_event_%s"

KAN_TYPED_ID_32_DEFINE (kan_resource_entry_id_t);
KAN_TYPED_ID_32_DEFINE (kan_resource_container_id_t);
KAN_TYPED_ID_32_DEFINE (kan_resource_usage_id_t);

/// \brief Structure that contains configuration for resource provider.
struct kan_resource_provider_configuration_t
{
    /// \brief How much time in nanoseconds should be spent loading resources during update.
    kan_time_offset_t serve_budget_ns;

    /// \brief Path to virtual directory with resources, that is used as resource root directory.
    kan_interned_string_t resource_directory_path;
};

UNIVERSE_RESOURCE_PROVIDER_API void kan_resource_provider_configuration_init (
    struct kan_resource_provider_configuration_t *instance);

/// \brief Contains counter for usage ids and flag that tells whether resource scan is done.
struct kan_resource_provider_singleton_t
{
    /// \brief Atomic counter for assigning usage ids. Safe to be modified from different threads.
    struct kan_atomic_int_t usage_id_counter;

    /// \brief Whether resource provider finished scanning for resources and is able to provide full list of entries.
    bool scan_done;
};

UNIVERSE_RESOURCE_PROVIDER_API void kan_resource_provider_singleton_init (
    struct kan_resource_provider_singleton_t *instance);

/// \brief Inline helper for generation of resource usage ids.
static inline kan_resource_usage_id_t kan_next_resource_usage_id (
    const struct kan_resource_provider_singleton_t *resource_provider)
{
    // Intentionally usage const and de-const it to show that it is multithreading-safe function.
    return KAN_TYPED_ID_32_SET (
        kan_resource_usage_id_t,
        (kan_id_32_t) kan_atomic_int_add ((struct kan_atomic_int_t *) &resource_provider->usage_id_counter, 1));
}

/// \brief Contains common information about resource entry.
struct kan_resource_generic_entry_t
{
    kan_resource_entry_id_t entry_id;
    kan_interned_string_t type;
    kan_interned_string_t name;
    kan_instance_size_t usage_counter;

    /// \brief Hot reload timer after which entry content will be reloaded if it is loaded.
    kan_packed_timer_t reload_after_timer;

    /// \brief If true, entry was removed from file system during hot reload.
    /// \details Will be loaded back if reappears and has non zero usages.
    bool removal_mark;

    kan_hash_t path_hash;
    char *path;
    kan_allocation_group_t my_allocation_group;
};

UNIVERSE_RESOURCE_PROVIDER_API void kan_resource_generic_entry_init (struct kan_resource_generic_entry_t *instance);

UNIVERSE_RESOURCE_PROVIDER_API void kan_resource_generic_entry_shutdown (struct kan_resource_generic_entry_t *instance);

/// \brief Describes information that is stored in typed resource entries.
/// \details Typed resource entries store information that should be easily accessible by resource name.
KAN_REFLECTION_IGNORE
struct kan_resource_typed_entry_view_t
{
    kan_resource_entry_id_t entry_id;
    kan_interned_string_t name;
    kan_resource_container_id_t loaded_container_id;
    kan_resource_container_id_t loading_container_id;
    kan_serialization_interned_string_registry_t bound_to_string_registry;
};

/// \brief Describes layout of record that store loaded resource data.
KAN_REFLECTION_IGNORE
struct kan_resource_container_view_t
{
    kan_resource_container_id_t container_id;
    kan_allocation_group_t my_allocation_group;
    uint8_t data_begin[];
};

/// \brief Helper macro for extracting container data with proper alignment.
#define KAN_RESOURCE_PROVIDER_CONTAINER_GET(TYPE_NAME, CONTAINER)                                                      \
    ((const struct TYPE_NAME *) kan_apply_alignment (                                                                  \
        (kan_memory_size_t) ((struct kan_resource_container_view_t *) CONTAINER)->data_begin,                          \
        alignof (struct TYPE_NAME)))

/// \brief Record that informs resource provider that resource with given type and name is expected to be loaded.
struct kan_resource_usage_t
{
    kan_resource_usage_id_t usage_id;
    kan_interned_string_t type;
    kan_interned_string_t name;
    kan_instance_size_t priority;
};

UNIVERSE_RESOURCE_PROVIDER_API void kan_resource_usage_init (struct kan_resource_usage_t *instance);

/// \brief Describes layout of typed event that is fired when new resource is registered,
///        including initial resource registration.
KAN_REFLECTION_IGNORE
struct kan_resource_registered_event_view_t
{
    kan_resource_entry_id_t entry_id;
    kan_interned_string_t name;
};

/// \brief Fired when resource file update was detected, for example due to new resource build tool execution.
/// \warning Does not mean that resource state loaded in memory is changed!
struct kan_resource_updated_event_t
{
    kan_resource_entry_id_t entry_id;
    kan_interned_string_t type;
    kan_interned_string_t name;
};

/// \brief Describes layout of typed event that is fired when resource is fully loaded in memory,
///        including reload due to resource update from hot reload.
KAN_REFLECTION_IGNORE
struct kan_resource_loaded_event_view_t
{
    kan_resource_entry_id_t entry_id;
    kan_interned_string_t name;
};

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED(NAME, RESOURCE_TYPE, RESOURCE_NAME_POINTER)                            \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct RESOURCE_TYPE *NAME = NULL;                                                                       \
        /* Add this useless pointer so IDE highlight would never consider argument unused. */                          \
        const void *argument_pointer_for_highlight_##NAME = RESOURCE_NAME_POINTER;
#else
#    define KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED(NAME, RESOURCE_TYPE, RESOURCE_NAME_POINTER)                            \
        KAN_UM_INTERNAL_VALUE_OPTIONAL (resource_typed_entry_##NAME,                                                   \
                                        KAN_RESOURCE_PROVIDER_MAKE_TYPED_ENTRY_TYPE (RESOURCE_TYPE), name,             \
                                        RESOURCE_NAME_POINTER, read, read, const)                                      \
                                                                                                                       \
        const struct RESOURCE_TYPE *NAME = NULL;                                                                       \
        struct kan_repository_indexed_value_read_access_t container_access_if_escaped_##NAME;                          \
                                                                                                                       \
        CUSHION_DEFER                                                                                                  \
        {                                                                                                              \
            if (NAME)                                                                                                  \
            {                                                                                                          \
                kan_repository_indexed_value_read_access_close (&container_access_if_escaped_##NAME);                  \
            }                                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        const struct kan_resource_typed_entry_view_t *resource_typed_entry_view_##NAME =                               \
            (const struct kan_resource_typed_entry_view_t *) resource_typed_entry_##NAME;                              \
                                                                                                                       \
        if (resource_typed_entry_view_##NAME &&                                                                        \
            KAN_TYPED_ID_32_IS_VALID (resource_typed_entry_view_##NAME->loaded_container_id))                          \
        {                                                                                                              \
            KAN_UM_INTERNAL_VALUE_OPTIONAL (resource_container_##NAME,                                                 \
                                            KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (RESOURCE_TYPE), container_id,   \
                                            &resource_typed_entry_view_##NAME->loaded_container_id, read, read, const) \
                                                                                                                       \
            if (resource_container_##NAME)                                                                             \
            {                                                                                                          \
                NAME = KAN_RESOURCE_PROVIDER_CONTAINER_GET (RESOURCE_TYPE, resource_container_##NAME);                 \
                KAN_UM_ACCESS_ESCAPE (container_access_if_escaped_##NAME, resource_container_##NAME)                   \
            }                                                                                                          \
        }
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_RESOURCE_REGISTERED_EVENT_FETCH(NAME, RESOURCE_TYPE)                                               \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct kan_resource_registered_event_view_t *NAME = NULL;                                                \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_RESOURCE_REGISTERED_EVENT_FETCH(NAME, RESOURCE_TYPE)                                               \
        KAN_UM_INTERNAL_EVENT_FETCH (NAME, KAN_RESOURCE_PROVIDER_MAKE_REGISTERED_EVENT_TYPE (RESOURCE_TYPE),           \
                                     kan_resource_registered_event_view_t)
#endif

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_UML_RESOURCE_LOADED_EVENT_FETCH(NAME, RESOURCE_TYPE)                                                   \
        /* Highlight-autocomplete replacement. */                                                                      \
        const struct kan_resource_loaded_event_view_t *NAME = NULL;                                                    \
        for (kan_loop_size_t fake_index_##NAME = 0u; fake_index_##NAME < 1u; ++fake_index_##NAME)
#else
#    define KAN_UML_RESOURCE_LOADED_EVENT_FETCH(NAME, RESOURCE_TYPE)                                                   \
        KAN_UM_INTERNAL_EVENT_FETCH (NAME, KAN_RESOURCE_PROVIDER_MAKE_LOADED_EVENT_TYPE (RESOURCE_TYPE),               \
                                     kan_resource_loaded_event_view_t)
#endif

KAN_C_HEADER_END
