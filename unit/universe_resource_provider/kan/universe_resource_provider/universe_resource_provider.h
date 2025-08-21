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

// TODO: Docs.

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

KAN_REFLECTION_IGNORE
struct kan_resource_typed_entry_view_t
{
    kan_resource_entry_id_t entry_id;
    kan_interned_string_t name;
    kan_resource_container_id_t loaded_container_id;
    kan_resource_container_id_t loading_container_id;
    kan_serialization_interned_string_registry_t bound_to_string_registry;
};

KAN_REFLECTION_IGNORE
struct kan_resource_container_view_t
{
    kan_resource_container_id_t container_id;
    kan_allocation_group_t my_allocation_group;
    uint8_t data_begin[];
};

/// \brief Helper macro for extracting container data with proper alignment.
#define KAN_RESOURCE_PROVIDER_CONTAINER_GET(TYPE_NAME, CONTAINER)                                                      \
    ((const struct TYPE_NAME *) kan_apply_alignment ((kan_memory_size_t) container_view->data_begin,                   \
                                                     alignof (struct TYPE_NAME)))

/// \details Usages are intended to be never changed after their insertion, therefore provider will not observe them for
///          changes. If usage needs to be changed, it should be deleted and new usage should be inserted. The reason
///          for that is because in real use case multiple different mutator groups would like to insert their usages
///          and delete their usages without introducing dependencies between each other. It is possible to do safely
///          as long as all groups declare that they only use insert and detach queries
struct kan_resource_usage_t
{
    kan_resource_usage_id_t usage_id;
    kan_interned_string_t type;
    kan_interned_string_t name;
    kan_instance_size_t priority;
};

UNIVERSE_RESOURCE_PROVIDER_API void kan_resource_usage_init (struct kan_resource_usage_t *instance);

/// \brief Fired when new resource is registered, including initial resource registration.
/// \details Separate event type is created for every resource type as in most cases users only need this events for
///          very specific resource types.
KAN_REFLECTION_IGNORE
struct kan_resource_registered_event_view_t
{
    kan_resource_entry_id_t entry_id;
    kan_interned_string_t name;
};

/// \brief Fired when resource file update was detected.
/// \warning Does not mean that resource state loaded in memory is changed!
struct kan_resource_updated_event_t
{
    kan_resource_entry_id_t entry_id;
    kan_interned_string_t type;
    kan_interned_string_t name;
};

/// \brief Fired when resource was fully loaded in memory, including reload due to resource update.
/// \details Separate event type is created for every resource type as users might need to use type-based ordering
///          while processing these events.
KAN_REFLECTION_IGNORE
struct kan_resource_loaded_event_view_t
{
    kan_resource_entry_id_t entry_id;
    kan_interned_string_t name;
};

// TODO: Convenience universe query macro wrappers later.

KAN_C_HEADER_END
