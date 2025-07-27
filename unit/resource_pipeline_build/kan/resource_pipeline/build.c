#include <kan/cpu_dispatch/job.h>
#include <kan/error/critical.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/path_container.h>
#include <kan/file_system/stream.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/platform/hardware.h>
#include <kan/precise_time/precise_time.h>
#include <kan/resource_pipeline/build.h>
#include <kan/resource_pipeline/log.h>
#include <kan/resource_pipeline/platform_configuration.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>
#include <kan/threading/atomic.h>

static kan_resource_version_t resource_build_version = CUSHION_START_NS_X64;
KAN_LOG_DEFINE_CATEGORY (resource_pipeline_build);

static kan_allocation_group_t main_allocation_group;
static kan_allocation_group_t platform_configuration_allocation_group;
static kan_allocation_group_t targets_allocation_group;
static kan_allocation_group_t build_queue_allocation_group;
static kan_allocation_group_t temporary_allocation_group;

KAN_USE_STATIC_INTERNED_IDS
static bool statics_initialized = false;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        main_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "resource_pipeline_build");
        platform_configuration_allocation_group =
            kan_allocation_group_get_child (main_allocation_group, "platform_configuration");
        targets_allocation_group = kan_allocation_group_get_child (main_allocation_group, "targets");
        build_queue_allocation_group = kan_allocation_group_get_child (main_allocation_group, "build_queue");
        temporary_allocation_group = kan_allocation_group_get_child (main_allocation_group, "temporary");
        kan_static_interned_ids_ensure_initialized ();
        statics_initialized = true;
    }
}

kan_allocation_group_t kan_resource_build_get_allocation_group (void)
{
    ensure_statics_initialized ();
    return main_allocation_group;
}

enum resource_production_class_t
{
    RESOURCE_PRODUCTION_CLASS_RAW = 0u,
    RESOURCE_PRODUCTION_CLASS_PRIMARY,
    RESOURCE_PRODUCTION_CLASS_SECONDARY,
    RESOURCE_PRODUCTION_CLASS_SECONDARY_MERGEABLE,
};

enum resource_status_t
{
    /// \brief We didn't check status of this resource yet.
    RESOURCE_STATUS_UNCONFIRMED = 0u,

    /// \brief Either build rule failed or source resource for this chain was removed.
    RESOURCE_STATUS_UNAVAILABLE,

    /// \brief New version of this resource is currently being built.
    RESOURCE_STATUS_BUILDING,

    /// \brief Resource is built, up to date and ready to use (but not necessary loaded right now).
    RESOURCE_STATUS_AVAILABLE,

    /// \brief Resource is not supported on this platform.
    RESOURCE_STATUS_PLATFORM_UNSUPPORTED,
};

/// \brief Header contains data that is changed during build,
///        but it not directly connected to build operation execution.
struct resource_entry_header_t
{
    /// \brief Lock for restricting access to the header through atomic read-write lock.
    struct kan_atomic_int_t lock;

    enum resource_status_t status;

    /// \brief Resource version if status is `RESOURCE_STATUS_AVAILABLE` or `RESOURCE_STATUS_PLATFORM_UNSUPPORTED`.
    struct kan_resource_log_version_t available_version;

    bool deployment_mark;
    bool cache_mark;
};

enum resource_entry_next_build_task_t
{
    /// \brief Default value for when there is nothing to build.
    RESOURCE_ENTRY_NEXT_BUILD_TASK_NONE = 0u,

    /// \brief Starts the build operation
    RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_START,

    RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_PROCESS_PRIMARY,

    RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_EXECUTE_BUILD_RULE,

    RESOURCE_ENTRY_NEXT_BUILD_TASK_LOAD,
};

struct resource_entry_build_blocked_t
{
    struct resource_entry_build_blocked_t *next;
    struct resource_entry_t *blocked_entry;
};

struct resource_entry_build_t
{
    /// \brief Lock for restricting access to the build state through atomic read-write lock.
    /// \invariant Header should never be locked from inside build lock. If we need to lock both, header lock should
    ///            be always acquired first.
    struct kan_atomic_int_t lock;

    void *loaded_data;
    kan_instance_size_t loaded_data_request_count;
    struct resource_entry_build_blocked_t *blocked_other_first;

    /// \details Managed internally, therefore not locked under `lock`.
    enum resource_entry_next_build_task_t internal_next_build_task;

    /// \invariant Should only be incremented from inside of build step execution through resource requests.
    /// \invariant Should only be decremented from `build_task` wrapper that handles task blocks.
    struct kan_atomic_int_t atomic_next_build_task_block_counter;

    /// \details Pointer to paused list item is mostly needed for deadlock detection and logging.
    ///          Managed internally exclusively in `build_task` function under build queue lock.
    struct build_info_list_item_t *internal_paused_list_item;

    /// \brief Entry that is used as primary input if build rule is executed.
    /// \details Managed internally, therefore not locked under `lock`.
    struct resource_entry_t *internal_primary_input_entry;
};

/// \details As on some OSes file movement cna change a last modification timestamp, we cannot just save info about
///          secondary inputs in `kan_resource_log_secondary_input_t` with version as deployment might accidentally
///          change modification time due to this OS feature. If secondary input is native resource, only `entry`
///          field is specified and not null. For third party resources, `entry` is null and `third_party_entry` is
///          not null.
struct new_build_secondary_input_t
{
    struct resource_entry_t *entry;
    struct raw_third_party_entry_t *third_party_entry;
    enum kan_resource_reference_flags_t reference_flags;
};

struct resource_entry_t
{
    struct kan_hash_storage_node_t node;
    struct target_t *target;
    const struct kan_reflection_struct_t *type;
    kan_interned_string_t name;

    struct resource_entry_header_t header;
    struct resource_entry_build_t build;

    enum resource_production_class_t class;
    char *current_file_location;

    union
    {
        const struct kan_resource_log_raw_entry_t *initial_log_raw_entry;
        const struct kan_resource_log_built_entry_t *initial_log_built_entry;
        const struct kan_resource_log_secondary_entry_t *initial_log_secondary_entry;
    };

    /// \details It is only populated for built entries if build has happened already for this resource on this
    ///          execution. This is true when `status` is `RESOURCE_STATUS_AVAILABLE` or
    ///          `RESOURCE_STATUS_PLATFORM_UNSUPPORTED` and
    ///          `!kan_resource_log_version_is_up_to_date (initial_log_built_entry->version, available_version)`.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct new_build_secondary_input_t)
    struct kan_dynamic_array_t new_build_secondary_inputs;

    /// \details It is only populated for entries if build has happened already for this resource on this execution.
    ///          This is true when `status` is `RESOURCE_STATUS_AVAILABLE` and
    ///          `!kan_resource_log_version_is_up_to_date (initial_log_built_entry->version, available_version)`.
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_log_reference_t)
    struct kan_dynamic_array_t new_references;

    kan_allocation_group_t allocation_group;
};

#define RESOURCE_TYPE_CONTAINER_MERGEABLE_MAP_VERSION_NOT_INITIALIZED 0u

struct resource_type_container_t
{
    struct kan_hash_storage_node_t node;
    struct target_t *target;
    const struct kan_reflection_struct_t *type;
    struct kan_hash_storage_t entries;

    /// \brief Lock for restricting access to mergeable map through atomic read-write lock.
    struct kan_atomic_int_t mergeable_map_lock;

    kan_instance_size_t mergeable_map_version;
    // TODO: Other mergeable fields.

    kan_allocation_group_t allocation_group;
};

struct raw_third_party_entry_t
{
    struct kan_hash_storage_node_t node;
    struct target_t *target;
    kan_interned_string_t name;

    kan_time_size_t last_modification_time;
    char *file_location;

    kan_allocation_group_t allocation_group;
};

struct target_t
{
    struct target_t *next;
    kan_interned_string_t name;
    kan_allocation_group_t allocation_group;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct target_t *)
    struct kan_dynamic_array_t visible_targets;

    struct kan_hash_storage_t resource_types;
    struct kan_hash_storage_t raw_third_party;

    /// \brief We do no change resource project, so we can just point to it.
    const struct kan_resource_project_target_t *source;
};

struct platform_configuration_entry_t
{
    struct kan_hash_storage_node_t node;
    const struct kan_reflection_struct_t *type;
    kan_time_size_t file_time;
    void *data;
};

struct build_queue_item_t
{
    struct kan_bd_list_node_t node;
    struct resource_entry_t *entry;

    /// \brief State pointer is needed to make build queue items usable as full build task user data.
    struct build_state_t *state;
};

struct build_info_list_item_t
{
    struct kan_bd_list_node_t node;
    struct resource_entry_t *entry;
};

struct build_state_t
{
    struct kan_resource_build_setup_t *setup;
    struct target_t *targets_first;

    /// \brief Global lock for resource entries operated as atomic read-write lock.
    /// \details Due to the fact that resource queries span across different storages due to target visibility,
    ///          locking storages separately could cause race conditions. Therefore, we use global lock for
    ///          all resource queries with preference to read lock whenever possible.
    struct kan_atomic_int_t resource_entries_lock;

    kan_serialization_binary_script_storage_t binary_script_storage;

    struct kan_hash_storage_t platform_configuration;

    /// \brief Global lock for management of build queues, managed as regular lock, not read-write one.
    /// \details Also used for paused list and failed list.
    struct kan_atomic_int_t build_queue_lock;

    /// \details We limit simultaneous builds in order to control the order and have the ability to limit concurrent
    ///          execution to avoid loading too many resources at once.
    kan_instance_size_t max_simultaneous_build_operations;

    kan_instance_size_t currently_scheduled_build_operations;

    struct kan_bd_list_t build_queue;
    struct kan_bd_list_t paused_list;
    struct kan_bd_list_t failed_list;

    /// \details Initial log is saved if it was parsed as we would need to use data from it and is much easier to just
    ///          save pointers to it from nodes.
    struct kan_resource_log_t initial_log;
};

// Section for common utility functions that are not part of single complex build operation.

static struct resource_entry_t *resource_entry_create (struct resource_type_container_t *owner,
                                                       kan_interned_string_t name)
{
    kan_allocation_group_t group = kan_allocation_group_get_child (owner->allocation_group, name);
    struct resource_entry_t *instance = kan_allocate_batched (group, sizeof (struct resource_entry_t));

    instance->node.hash = KAN_HASH_OBJECT_POINTER (name);
    instance->target = owner->target;
    instance->type = owner->type;
    instance->name = name;

    instance->header.lock = kan_atomic_int_init (0);
    instance->header.status = RESOURCE_STATUS_UNCONFIRMED;
    instance->header.available_version.type_version = 0u;
    instance->header.available_version.last_modification_time = 0u;
    instance->header.deployment_mark = false;
    instance->header.cache_mark = false;

    instance->build.lock = kan_atomic_int_init (0);
    instance->build.loaded_data = NULL;
    instance->build.loaded_data_request_count = 0u;
    instance->build.blocked_other_first = NULL;
    instance->build.internal_next_build_task = RESOURCE_ENTRY_NEXT_BUILD_TASK_NONE;
    instance->build.atomic_next_build_task_block_counter = kan_atomic_int_init (0);
    instance->build.internal_paused_list_item = NULL;
    instance->build.internal_primary_input_entry = NULL;

    instance->class = RESOURCE_PRODUCTION_CLASS_RAW;
    instance->current_file_location = NULL;
    instance->initial_log_raw_entry = NULL;

    kan_dynamic_array_init (&instance->new_build_secondary_inputs, 0u, sizeof (struct new_build_secondary_input_t),
                            alignof (struct new_build_secondary_input_t), instance->allocation_group);
    kan_dynamic_array_init (&instance->new_references, 0u, sizeof (struct kan_resource_log_reference_t),
                            alignof (struct kan_resource_log_reference_t), instance->allocation_group);
    instance->allocation_group = group;

    kan_hash_storage_update_bucket_count_default (&owner->entries, KAN_RESOURCE_PIPELINE_BUILD_RESOURCE_BUCKETS);
    kan_hash_storage_add (&owner->entries, &instance->node);
    return instance;
}

/// \details Expects external synchronization, therefore is unsafe.
static void resource_entry_unload_data_unsafe (struct resource_entry_t *instance)
{
    if (instance->build.loaded_data)
    {
        if (instance->type->shutdown)
        {
            instance->type->shutdown (instance->type->functor_user_data, instance->build.loaded_data);
        }

        kan_free_general (instance->allocation_group, instance->build.loaded_data, instance->type->size);
        instance->build.loaded_data = NULL;
    }
}

static void resource_entry_destroy (struct resource_entry_t *instance)
{
    if (instance->current_file_location)
    {
        kan_free_general (instance->allocation_group, instance->current_file_location,
                          strlen (instance->current_file_location) + 1u);
    }

    resource_entry_unload_data_unsafe (instance);
    struct resource_entry_build_blocked_t *blocked = instance->build.blocked_other_first;

    while (blocked)
    {
        struct resource_entry_build_blocked_t *next = blocked->next;
        kan_free_batched (instance->allocation_group, blocked);
        blocked = next;
    }

    kan_dynamic_array_shutdown (&instance->new_build_secondary_inputs);
    kan_dynamic_array_shutdown (&instance->new_references);
    kan_free_batched (instance->allocation_group, instance);
}

static struct resource_type_container_t *resource_type_container_create (struct target_t *owner,
                                                                         const struct kan_reflection_struct_t *type)
{
    kan_allocation_group_t group = kan_allocation_group_get_child (owner->allocation_group, type->name);
    struct resource_type_container_t *instance =
        kan_allocate_batched (group, sizeof (struct resource_type_container_t));

    instance->node.hash = KAN_HASH_OBJECT_POINTER (type->name);
    instance->target = owner;
    instance->type = type;
    kan_hash_storage_init (&instance->entries, group, KAN_RESOURCE_PIPELINE_BUILD_RESOURCE_BUCKETS);

    instance->mergeable_map_lock = kan_atomic_int_init (0);
    instance->mergeable_map_version = RESOURCE_TYPE_CONTAINER_MERGEABLE_MAP_VERSION_NOT_INITIALIZED;
    instance->allocation_group = group;

    kan_hash_storage_update_bucket_count_default (&owner->resource_types, KAN_RESOURCE_PIPELINE_BUILD_TYPES_BUCKETS);
    kan_hash_storage_add (&owner->resource_types, &instance->node);
    return instance;
}

static void resource_type_container_destroy (struct resource_type_container_t *instance)
{
    struct resource_entry_t *entry = (struct resource_entry_t *) instance->entries.items.first;
    while (entry)
    {
        struct resource_entry_t *next = (struct resource_entry_t *) entry->node.list_node.next;
        resource_entry_destroy (entry);
        entry = next;
    }

    kan_hash_storage_shutdown (&instance->entries);
    kan_free_batched (instance->allocation_group, instance);
}

static struct raw_third_party_entry_t *raw_third_party_entry_create (
    struct target_t *owner, const struct kan_file_system_path_container_t *path)
{
    KAN_ASSERT (path->length > 0u)
    const char *name_end = path->path + path->length;
    const char *name_begin = name_end - 1u;

    while (name_begin > path->path)
    {
        if (*name_begin == '/' || *name_begin == '\\')
        {
            // Found separator, name finished.
            break;
        }

        --name_begin;
    }

    kan_interned_string_t name = kan_char_sequence_intern (name_begin, name_end);
    struct kan_file_system_entry_status_t status;

    if (!kan_file_system_query_entry (path->path, &status))
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                             "Unable to query status of raw third party file status at \"%s\".", path->path);
        return NULL;
    }

    kan_allocation_group_t group = kan_allocation_group_get_child (owner->allocation_group, name);
    struct raw_third_party_entry_t *instance = kan_allocate_batched (group, sizeof (struct raw_third_party_entry_t));

    instance->node.hash = KAN_HASH_OBJECT_POINTER (name);
    instance->name = name;
    instance->last_modification_time = status.last_modification_time_ns;
    instance->file_location = kan_allocate_general (group, path->length + 1u, alignof (char));
    memcpy (instance->file_location, path->path, path->length);
    instance->file_location[path->length] = '\0';
    instance->allocation_group = group;

    kan_hash_storage_update_bucket_count_default (&owner->raw_third_party, KAN_RESOURCE_PIPELINE_BUILD_TYPES_BUCKETS);
    kan_hash_storage_add (&owner->raw_third_party, &instance->node);
    return instance;
}

static void raw_third_party_entry_destroy (struct raw_third_party_entry_t *instance)
{
    if (instance->file_location)
    {
        kan_free_general (instance->allocation_group, instance->file_location, strlen (instance->file_location) + 1u);
    }

    kan_free_batched (instance->allocation_group, instance);
}

static void target_init (struct target_t *instance, const struct kan_resource_project_target_t *source)
{
    instance->next = NULL;
    instance->name = source->name;
    instance->allocation_group = kan_allocation_group_get_child (targets_allocation_group, source->name);

    kan_dynamic_array_init (&instance->visible_targets, 0u, sizeof (struct target_t *), alignof (struct target_t *),
                            instance->allocation_group);

    kan_hash_storage_init (&instance->resource_types, instance->allocation_group,
                           KAN_RESOURCE_PIPELINE_BUILD_TYPES_BUCKETS);

    kan_hash_storage_init (&instance->raw_third_party, instance->allocation_group,
                           KAN_RESOURCE_PIPELINE_BUILD_THIRD_PARTY_BUCKETS);
    instance->source = source;
}

/// \details Has no inbuilt locking, must be externally synchronized (therefore _unsafe suffix).
static struct resource_type_container_t *target_search_resource_type_container_unsafe (struct target_t *target,
                                                                                       kan_interned_string_t type)
{
    const struct kan_hash_storage_bucket_t *type_bucket =
        kan_hash_storage_query (&target->resource_types, KAN_HASH_OBJECT_POINTER (type));
    struct resource_type_container_t *type_node = (struct resource_type_container_t *) type_bucket->first;
    const struct resource_type_container_t *type_node_end =
        (struct resource_type_container_t *) (type_bucket->last ? type_bucket->last->next : NULL);

    while (type_node != type_node_end)
    {
        if (type_node->type->name == type)
        {
            return type_node;
        }

        type_node = (struct resource_type_container_t *) type_node->node.list_node.next;
    }

    return NULL;
}

/// \details Has no inbuilt locking, must be externally synchronized (therefore _unsafe suffix).
static struct resource_entry_t *target_search_local_resource_unsafe (struct target_t *target,
                                                                     kan_interned_string_t type,
                                                                     kan_interned_string_t name)
{
    struct resource_type_container_t *type_node = target_search_resource_type_container_unsafe (target, type);
    if (!type_node)
    {
        return NULL;
    }

    const struct kan_hash_storage_bucket_t *entry_bucket =
        kan_hash_storage_query (&type_node->entries, KAN_HASH_OBJECT_POINTER (name));
    struct resource_entry_t *entry_node = (struct resource_entry_t *) entry_bucket->first;
    const struct resource_entry_t *entry_node_end =
        (struct resource_entry_t *) (entry_bucket->last ? entry_bucket->last->next : NULL);

    while (entry_node != entry_node_end)
    {
        if (entry_node->name == name)
        {
            return entry_node;
        }

        entry_node = (struct resource_entry_t *) entry_node->node.list_node.next;
    }

    return NULL;
}

/// \details Has no inbuilt locking, must be externally synchronized (therefore _unsafe suffix).
static inline struct resource_entry_t *target_search_visible_resource_unsafe (struct target_t *from_target,
                                                                              kan_interned_string_t type,
                                                                              kan_interned_string_t name)
{
    struct resource_entry_t *found_entry = NULL;
    if ((found_entry = target_search_local_resource_unsafe (from_target, type, name)))
    {
        return found_entry;
    }

    for (kan_loop_size_t index = 0u; index < from_target->visible_targets.size; ++index)
    {
        struct target_t *visible = ((struct target_t **) from_target->visible_targets.data)[index];
        if ((found_entry = target_search_local_resource_unsafe (visible, type, name)))
        {
            return found_entry;
        }
    }

    return NULL;
}

/// \details Has no inbuilt locking, must be externally synchronized (therefore _unsafe suffix).
///          Technically, third party entries are not changed during build and do not need synchronization in that
///          phase. However, name uniqueness must be checked during raw resource scan phase and that is where
///          calls to this function must be synchronized as third party storages are being changed during the scan.
static struct raw_third_party_entry_t *target_search_local_third_party_unsafe (struct target_t *target,
                                                                               kan_interned_string_t name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&target->raw_third_party, KAN_HASH_OBJECT_POINTER (name));
    struct raw_third_party_entry_t *node = (struct raw_third_party_entry_t *) bucket->first;
    const struct raw_third_party_entry_t *node_end =
        (struct raw_third_party_entry_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == name)
        {
            return node;
        }

        node = (struct raw_third_party_entry_t *) node->node.list_node.next;
    }

    return NULL;
}

/// \details Has no inbuilt locking, must be externally synchronized (therefore _unsafe suffix).
///          Technically, third party entries are not changed during build and do not need synchronization in that
///          phase. However, name uniqueness must be checked during raw resource scan phase and that is where
///          calls to this function must be synchronized as third party storages are being changed during the scan.
static struct raw_third_party_entry_t *target_search_visible_third_party_unsafe (struct target_t *from_target,
                                                                                 kan_interned_string_t name)
{
    struct raw_third_party_entry_t *found_entry = NULL;
    if ((found_entry = target_search_local_third_party_unsafe (from_target, name)))
    {
        return found_entry;
    }

    for (kan_loop_size_t index = 0u; index < from_target->visible_targets.size; ++index)
    {
        struct target_t *visible = ((struct target_t **) from_target->visible_targets.data)[index];
        if ((found_entry = target_search_local_third_party_unsafe (visible, name)))
        {
            return found_entry;
        }
    }

    return NULL;
}

static void target_shutdown (struct target_t *instance)
{
    kan_dynamic_array_shutdown (&instance->visible_targets);
    struct resource_type_container_t *container =
        (struct resource_type_container_t *) instance->resource_types.items.first;

    while (container)
    {
        struct resource_type_container_t *next = (struct resource_type_container_t *) container->node.list_node.next;
        resource_type_container_destroy (container);
        container = next;
    }

    struct raw_third_party_entry_t *third_party =
        (struct raw_third_party_entry_t *) instance->raw_third_party.items.first;

    while (third_party)
    {
        struct raw_third_party_entry_t *next = (struct raw_third_party_entry_t *) third_party->node.list_node.next;
        raw_third_party_entry_destroy (third_party);
        third_party = next;
    }

    kan_hash_storage_shutdown (&instance->resource_types);
    kan_hash_storage_shutdown (&instance->raw_third_party);
}

static void build_state_init (struct build_state_t *instance, struct kan_resource_build_setup_t *setup)
{
    instance->setup = setup;
    instance->targets_first = NULL;
    instance->resource_entries_lock = kan_atomic_int_init (0);
    instance->binary_script_storage = kan_serialization_binary_script_storage_create (setup->reflected_data->registry);

    kan_hash_storage_init (&instance->platform_configuration, platform_configuration_allocation_group,
                           KAN_RESOURCE_PIPELINE_BUILD_PC_BUCKETS);

    instance->build_queue_lock = kan_atomic_int_init (0);
    // Right now just limit to core count, might add separate setting for that later.
    instance->max_simultaneous_build_operations = kan_platform_get_cpu_logical_core_count ();
    instance->currently_scheduled_build_operations = 0u;

    kan_bd_list_init (&instance->build_queue);
    kan_bd_list_init (&instance->paused_list);
    kan_bd_list_init (&instance->failed_list);
    kan_resource_log_init (&instance->initial_log);
}

static struct platform_configuration_entry_t *build_state_find_platform_configuration (struct build_state_t *instance,
                                                                                       kan_interned_string_t type)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&instance->platform_configuration, KAN_HASH_OBJECT_POINTER (type));
    struct platform_configuration_entry_t *node = (struct platform_configuration_entry_t *) bucket->first;
    const struct platform_configuration_entry_t *node_end =
        (struct platform_configuration_entry_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->type->name == type)
        {
            return node;
        }

        node = (struct platform_configuration_entry_t *) node->node.list_node.next;
    }

    return NULL;
}

static struct platform_configuration_entry_t *build_state_new_platform_configuration (
    struct build_state_t *instance, const struct kan_reflection_struct_t *type)
{
    struct platform_configuration_entry_t *entry =
        kan_allocate_batched (platform_configuration_allocation_group, sizeof (struct platform_configuration_entry_t));

    entry->node.hash = KAN_HASH_OBJECT_POINTER (type->name);
    entry->type = type;
    entry->file_time = 0u;
    entry->data = kan_allocate_general (platform_configuration_allocation_group, type->size, type->alignment);

    if (type->init)
    {
        kan_allocation_group_stack_push (platform_configuration_allocation_group);
        type->init (type->functor_user_data, entry->data);
        kan_allocation_group_stack_pop ();
    }

    kan_hash_storage_update_bucket_count_default (&instance->platform_configuration,
                                                  KAN_RESOURCE_PIPELINE_BUILD_PC_BUCKETS);
    kan_hash_storage_add (&instance->platform_configuration, &entry->node);
    return entry;
}

static void build_state_shutdown (struct build_state_t *instance)
{
    struct target_t *target = instance->targets_first;
    while (target)
    {
        struct target_t *next = target->next;
        target_shutdown (target);
        kan_free_batched (targets_allocation_group, target);
        target = next;
    }

    kan_serialization_binary_script_storage_destroy (instance->binary_script_storage);
    struct platform_configuration_entry_t *platform_configuration =
        (struct platform_configuration_entry_t *) instance->platform_configuration.items.first;

    while (platform_configuration)
    {
        struct platform_configuration_entry_t *next =
            (struct platform_configuration_entry_t *) platform_configuration->node.list_node.next;

        KAN_ASSERT (platform_configuration->type)
        KAN_ASSERT (platform_configuration->data)

        if (platform_configuration->type->shutdown)
        {
            platform_configuration->type->shutdown (platform_configuration->type->functor_user_data,
                                                    platform_configuration->data);
        }

        kan_free_general (platform_configuration_allocation_group, platform_configuration->data,
                          platform_configuration->type->size);

        kan_free_batched (platform_configuration_allocation_group, platform_configuration);
        platform_configuration = next;
    }

    kan_hash_storage_shutdown (&instance->platform_configuration);
    struct build_queue_item_t *build_queue_item = (struct build_queue_item_t *) instance->build_queue.first;

    while (build_queue_item)
    {
        struct build_queue_item_t *next = (struct build_queue_item_t *) build_queue_item->node.next;
        kan_free_batched (build_queue_allocation_group, build_queue_item);
        build_queue_item = next;
    }

    struct build_info_list_item_t *paused_list_item = (struct build_info_list_item_t *) instance->paused_list.first;
    while (paused_list_item)
    {
        struct build_info_list_item_t *next = (struct build_info_list_item_t *) paused_list_item->node.next;
        kan_free_batched (build_queue_allocation_group, paused_list_item);
        paused_list_item = next;
    }

    struct build_info_list_item_t *failed_list_item = (struct build_info_list_item_t *) instance->failed_list.first;
    while (failed_list_item)
    {
        struct build_info_list_item_t *next = (struct build_info_list_item_t *) failed_list_item->node.next;
        kan_free_batched (build_queue_allocation_group, failed_list_item);
        failed_list_item = next;
    }

    kan_resource_log_shutdown (&instance->initial_log);
}

void kan_resource_build_setup_init (struct kan_resource_build_setup_t *instance)
{
    ensure_statics_initialized ();
    instance->project = NULL;
    instance->reflected_data = NULL;
    instance->pack = false;
    instance->log_verbosity = KAN_LOG_INFO;
    kan_dynamic_array_init (&instance->targets, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            main_allocation_group);
}

void kan_resource_build_setup_shutdown (struct kan_resource_build_setup_t *instance)
{
    kan_dynamic_array_shutdown (&instance->targets);
}

// Target setup steps section.

static enum kan_resource_build_result_t create_targets (struct build_state_t *state)
{
    for (kan_loop_size_t index = 0u; index < state->setup->project->targets.size; ++index)
    {
        const struct kan_resource_project_target_t *source =
            &((struct kan_resource_project_target_t *) state->setup->project->targets.data)[index];
        struct target_t *target = state->targets_first;

        while (target)
        {
            if (target->name == source->name)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                         "There are several targets with name \"%s\" in resource project.", source->name)
                return KAN_RESOURCE_BUILD_RESULT_ERROR_PROJECT_DUPLICATE_TARGETS;
            }

            target = target->next;
        }

        target = kan_allocate_batched (targets_allocation_group, sizeof (struct target_t));
        target_init (target, source);
        target->next = state->targets_first;
        state->targets_first = target;
    }

    return KAN_RESOURCE_BUILD_RESULT_SUCCESS;
}

static enum kan_resource_build_result_t link_visible_targets (struct build_state_t *state)
{
    struct target_t *main_target = state->targets_first;
    while (main_target)
    {
        kan_dynamic_array_set_capacity (&main_target->visible_targets, main_target->source->visible_targets.size);
        for (kan_loop_size_t index = 0u; index < main_target->source->visible_targets.size; ++index)
        {
            kan_interned_string_t name = ((kan_interned_string_t *) main_target->source->visible_targets.data)[index];
            struct target_t *visible_target = state->targets_first;

            while (visible_target)
            {
                if (visible_target->name == name)
                {
                    *(struct target_t **) kan_dynamic_array_add_last (&main_target->visible_targets) = visible_target;
                    break;
                }

                visible_target = visible_target->next;
            }

            if (!visible_target)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                         "Unable to find target \"%s\" which is listed as visible for \"%s\" in resource project.",
                         name, main_target->next)
                return KAN_RESOURCE_BUILD_RESULT_ERROR_PROJECT_VISIBLE_TARGET_NOT_FOUND;
            }
        }

        main_target = main_target->next;
    }

    return KAN_RESOURCE_BUILD_RESULT_SUCCESS;
}

static enum kan_resource_build_result_t linearize_visible_targets (struct build_state_t *state)
{
    struct target_t *target = state->targets_first;
    while (target)
    {
        // The idea is simple: if B is visible from A and C is visible from B, then C should be visible from A and added
        // to the end of the array if it is not here already, so it will be scanned in the end too. This addition to the
        // end actually creates recursion and results in full linearization.

        for (kan_loop_size_t main_index = 0u; main_index < target->visible_targets.size; ++main_index)
        {
            struct target_t *main_visible = ((struct target_t **) target->visible_targets.data)[main_index];
            for (kan_loop_size_t child_index = 0u; child_index < main_visible->visible_targets.size; ++child_index)
            {
                struct target_t *child_visible = ((struct target_t **) main_visible->visible_targets.data)[child_index];
                bool already_here = child_visible == target;

                for (kan_loop_size_t existent_index = 0u;
                     !already_here && existent_index < target->visible_targets.size; ++existent_index)
                {
                    already_here = child_visible == ((struct target_t **) target->visible_targets.data)[existent_index];
                }

                if (!already_here)
                {
                    struct target_t **spot = kan_dynamic_array_add_last (&target->visible_targets);
                    if (!spot)
                    {
                        kan_dynamic_array_set_capacity (&target->visible_targets, target->visible_targets.size * 2u);
                        spot = kan_dynamic_array_add_last (&target->visible_targets);
                        KAN_ASSERT (spot)
                    }

                    *spot = child_visible;
                }
            }
        }

        kan_dynamic_array_set_capacity (&target->visible_targets, target->visible_targets.size);
        target = target->next;
    }

    return KAN_RESOURCE_BUILD_RESULT_SUCCESS;
}

// Platform configuration setup step section.

struct transient_platform_configuration_entry_t
{
    kan_reflection_patch_t data;
    kan_time_size_t file_time;
};

static void transient_platform_configuration_entry_shutdown (struct transient_platform_configuration_entry_t *instance)
{
    kan_reflection_patch_destroy (instance->data);
}

struct transient_platform_configuration_layer_t
{
    kan_interned_string_t name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct transient_platform_configuration_entry_t)
    struct kan_dynamic_array_t entries;
};

static void transient_platform_configuration_layer_shutdown (struct transient_platform_configuration_layer_t *instance)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->entries, transient_platform_configuration_entry)
}

struct transient_platform_configuration_hierarchy_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct transient_platform_configuration_layer_t)
    struct kan_dynamic_array_t layers;
};

static void transient_platform_configuration_hierarchy_shutdown (
    struct transient_platform_configuration_hierarchy_t *instance)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->layers, transient_platform_configuration_layer)
}

static enum kan_resource_build_result_t load_platform_configuration_entries_recursive (
    struct build_state_t *state,
    struct transient_platform_configuration_hierarchy_t *hierarchy,
    struct kan_file_system_path_container_t *path_container,
    bool root_call)
{
    kan_file_system_directory_iterator_t iterator = kan_file_system_directory_iterator_create (path_container->path);
    const char *item_name;

    while ((item_name = kan_file_system_directory_iterator_advance (iterator)))
    {
        if ((item_name[0u] == '.' && item_name[1u] == '\0') ||
            (item_name[0u] == '.' && item_name[1u] == '.' && item_name[2u] == '\0'))
        {
            // Skip special entries.
            continue;
        }

        if (strcmp (item_name, KAN_RESOURCE_PLATFORM_CONFIGURATION_SETUP_FILE) == 0)
        {
            if (root_call)
            {
                continue;
            }
            else
            {
                KAN_LOG_WITH_BUFFER (
                    KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                    "Encountered platform configuration setup file not in root directory while searching for "
                    "platform configuration entries (search directory is \"%s\").",
                    path_container->path);
                return KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_UNKNOWN_ENTRY_FILE;
            }
        }

        const kan_instance_size_t preserved_length = path_container->length;
        kan_file_system_path_container_append (path_container, item_name);
        CUSHION_DEFER { kan_file_system_path_container_reset_length (path_container, preserved_length); }
        struct kan_file_system_entry_status_t status;

        if (kan_file_system_query_entry (path_container->path, &status))
        {
            switch (status.type)
            {
            case KAN_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN:
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                                     "File \"%s\" has unknown type, found while platform configuration entries.",
                                     path_container->path);
                return KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_UNKNOWN_ENTRY_FILE;

            case KAN_FILE_SYSTEM_ENTRY_TYPE_FILE:
            {
                if (path_container->length < 3u || path_container->path[path_container->length - 3u] != '.' ||
                    path_container->path[path_container->length - 2u] != 'r' ||
                    path_container->path[path_container->length - 1u] != 'd')
                {
                    KAN_LOG_WITH_BUFFER (
                        KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                        "File \"%s\" is not a readable data file, found while platform configuration entries.",
                        path_container->path);
                    return KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_UNKNOWN_ENTRY_FILE;
                }

                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_DEBUG,
                                     "Loading platform configuration entry from \"%s\".", path_container->path);

                struct kan_stream_t *stream = kan_direct_file_stream_open_for_read (path_container->path, false);
                if (!stream)
                {
                    KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                                         "Unable to open platform configuration entry file for read at path \"%s\".",
                                         path_container->path);
                    return KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_IO_ERROR;
                }

                stream = kan_random_access_stream_buffer_open_for_read (stream, KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER);
                CUSHION_DEFER { stream->operations->close (stream); }

                struct kan_resource_platform_configuration_entry_t entry;
                kan_resource_platform_configuration_entry_init (&entry);
                CUSHION_DEFER { kan_resource_platform_configuration_entry_shutdown (&entry); }

                kan_serialization_rd_reader_t reader = kan_serialization_rd_reader_create (
                    stream, &entry, KAN_STATIC_INTERNED_ID_GET (kan_resource_platform_configuration_entry_t),
                    state->setup->reflected_data->registry,
                    kan_resource_platform_configuration_get_allocation_group ());

                CUSHION_DEFER { kan_serialization_rd_reader_destroy (reader); }
                enum kan_serialization_state_t serialization_state;

                while ((serialization_state = kan_serialization_rd_reader_step (reader)) ==
                       KAN_SERIALIZATION_IN_PROGRESS)
                {
                }

                if (serialization_state == KAN_SERIALIZATION_FAILED)
                {
                    KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                                         "Failed to deserialize platform configuration entry from \"%s\".",
                                         path_container->path);
                    return KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_IO_ERROR;
                }

                bool all_tags_found = true;
                for (kan_loop_size_t required_index = 0u; required_index < entry.required_tags.size; ++required_index)
                {
                    bool found = false;
                    for (kan_loop_size_t existent_index = 0u;
                         existent_index < state->setup->project->platform_configuration_tags.size; ++existent_index)
                    {
                        if (((kan_interned_string_t *) entry.required_tags.data)[required_index] ==
                            ((kan_interned_string_t *)
                                 state->setup->project->platform_configuration_tags.data)[existent_index])
                        {
                            found = true;
                            break;
                        }
                    }

                    all_tags_found &= found;
                }

                if (!all_tags_found)
                {
                    KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_DEBUG,
                                         "Disabled platform configuration entry from \"%s\" due to missing tags.",
                                         path_container->path);
                    break;
                }

                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_DEBUG,
                                     "Disabled platform configuration entry from \"%s\" as all tags are found.",
                                     path_container->path);

                struct transient_platform_configuration_layer_t *found_layer = NULL;
                for (kan_loop_size_t index = 0u; index < hierarchy->layers.size; ++index)
                {
                    struct transient_platform_configuration_layer_t *layer =
                        &((struct transient_platform_configuration_layer_t *) hierarchy->layers.data)[index];

                    if (layer->name == entry.layer)
                    {
                        found_layer = layer;
                        break;
                    }
                }

                if (!found_layer)
                {
                    KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                                         "Platform configuration entry from \"%s\" is bound to unknown layer \"%s\".",
                                         path_container->path, entry.layer);
                    return KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_UNKNOWN_LAYER;
                }

                for (kan_loop_size_t index = 0u; index < found_layer->entries.size; ++index)
                {
                    struct transient_platform_configuration_entry_t *found_entry =
                        &((struct transient_platform_configuration_entry_t *) found_layer->entries.data)[index];

                    if (kan_reflection_patch_get_type (found_entry->data) == kan_reflection_patch_get_type (entry.data))
                    {
                        KAN_LOG_WITH_BUFFER (
                            KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                            "Platform configuration entry from \"%s\" has type \"%s\" and there is already other entry "
                            "with that type on that layer which is enabled with current tag setup.",
                            path_container->path, kan_reflection_patch_get_type (found_entry->data)->name);
                        return KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_DUPLICATE_TYPE;
                    }
                }

                struct transient_platform_configuration_entry_t *spot =
                    kan_dynamic_array_add_last (&found_layer->entries);

                if (!spot)
                {
                    kan_dynamic_array_set_capacity (&found_layer->entries, found_layer->entries.size * 2u);
                    spot = kan_dynamic_array_add_last (&found_layer->entries);
                    KAN_ASSERT (spot)
                }

                spot->data = entry.data;
                spot->file_time = status.last_modification_time_ns;
                break;
            }

            case KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY:
                load_platform_configuration_entries_recursive (state, hierarchy, path_container, false);
                break;
            }
        }
        else
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                                 "Failed to query file \"%s\" status while platform configuration entries.",
                                 path_container->path);
            return KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_UNKNOWN_ENTRY_FILE;
        }
    };

    return KAN_RESOURCE_BUILD_RESULT_SUCCESS;
}

static enum kan_resource_build_result_t load_platform_configuration (struct build_state_t *state)
{
    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_string (&path_container,
                                                state->setup->project->platform_configuration_directory);
    kan_file_system_path_container_append (&path_container, KAN_RESOURCE_PLATFORM_CONFIGURATION_SETUP_FILE);

    if (!kan_file_system_check_existence (path_container.path))
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_INFO,
                             "Platform configuration setup not found at expected path \"%s\".", path_container.path);
        return KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_NOT_FOUND;
    }

    struct kan_stream_t *setup_stream = kan_direct_file_stream_open_for_read (path_container.path, false);
    if (!setup_stream)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                             "Unable to open platform configuration file for read at path \"%s\".",
                             path_container.path);
        return KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_IO_ERROR;
    }

    setup_stream = kan_random_access_stream_buffer_open_for_read (setup_stream, KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER);
    CUSHION_DEFER { setup_stream->operations->close (setup_stream); }

    struct kan_resource_platform_configuration_setup_t configuration_setup;
    kan_resource_platform_configuration_setup_init (&configuration_setup);
    CUSHION_DEFER { kan_resource_platform_configuration_setup_shutdown (&configuration_setup); }

    kan_serialization_rd_reader_t setup_reader = kan_serialization_rd_reader_create (
        setup_stream, &configuration_setup, KAN_STATIC_INTERNED_ID_GET (kan_resource_platform_configuration_setup_t),
        state->setup->reflected_data->registry, kan_resource_platform_configuration_get_allocation_group ());

    CUSHION_DEFER { kan_serialization_rd_reader_destroy (setup_reader); }
    enum kan_serialization_state_t serialization_state;

    while ((serialization_state = kan_serialization_rd_reader_step (setup_reader)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    if (serialization_state == KAN_SERIALIZATION_FAILED)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                             "Failed to deserialize platform configuration setup from \"%s\".", path_container.path);
        return KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_IO_ERROR;
    }

    KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_DEBUG,
                         "Loaded platform configuration setup from \"%s\".", path_container.path);

    struct transient_platform_configuration_hierarchy_t transient_hierarchy;
    kan_dynamic_array_init (&transient_hierarchy.layers, configuration_setup.layers.size,
                            sizeof (struct transient_platform_configuration_layer_t),
                            alignof (struct transient_platform_configuration_layer_t),
                            kan_resource_platform_configuration_get_allocation_group ());
    CUSHION_DEFER { transient_platform_configuration_hierarchy_shutdown (&transient_hierarchy); }

    for (kan_loop_size_t index = 0u; index < configuration_setup.layers.size; ++index)
    {
        kan_interned_string_t name = ((kan_interned_string_t *) configuration_setup.layers.data)[index];
        for (kan_loop_size_t existent_index = 0u; existent_index < index; ++existent_index)
        {
            if (name == ((kan_interned_string_t *) configuration_setup.layers.data)[existent_index])
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                         "Layer \"%s\" is specified twice in platform configuration setup.", name);
                return KAN_RESOURCE_BUILD_RESULT_ERROR_PLATFORM_CONFIGURATION_DUPLICATE_LAYER;
            }
        }

        struct transient_platform_configuration_layer_t *layer =
            kan_dynamic_array_add_last (&transient_hierarchy.layers);
        KAN_ASSERT (layer)

        layer->name = name;
        kan_dynamic_array_init (&layer->entries, KAN_RESOURCE_PIPELINE_BUILD_TPC_ENTRIES_PER_LAYER,
                                sizeof (struct transient_platform_configuration_entry_t),
                                alignof (struct transient_platform_configuration_entry_t),
                                kan_resource_platform_configuration_get_allocation_group ());
    }

    kan_file_system_path_container_copy_string (&path_container,
                                                state->setup->project->platform_configuration_directory);

    enum kan_resource_build_result_t result =
        load_platform_configuration_entries_recursive (state, &transient_hierarchy, &path_container, true);

    if (result != KAN_RESOURCE_BUILD_RESULT_SUCCESS)
    {
        return result;
    }

    for (kan_loop_size_t layer_index = 0u; layer_index < configuration_setup.layers.size; ++layer_index)
    {
        struct transient_platform_configuration_layer_t *layer =
            &((struct transient_platform_configuration_layer_t *) transient_hierarchy.layers.data)[layer_index];

        for (kan_loop_size_t entry_index = 0u; entry_index < layer->entries.size; ++entry_index)
        {
            struct transient_platform_configuration_entry_t *transient_entry =
                &((struct transient_platform_configuration_entry_t *) layer->entries.data)[entry_index];

            if (!kan_reflection_patch_get_type (transient_entry->data))
            {
                continue;
            }

            struct platform_configuration_entry_t *built_entry = build_state_find_platform_configuration (
                state, kan_reflection_patch_get_type (transient_entry->data)->name);

            if (!built_entry)
            {
                built_entry = build_state_new_platform_configuration (
                    state, kan_reflection_patch_get_type (transient_entry->data));
            }

            built_entry->file_time = KAN_MAX (built_entry->file_time, transient_entry->file_time);
            kan_reflection_patch_apply (transient_entry->data, built_entry->data);
        }
    }

    return KAN_RESOURCE_BUILD_RESULT_SUCCESS;
}

// Resource log loading step section.

static enum kan_resource_build_result_t cleanup_workspace (struct build_state_t *state)
{
    if (kan_file_system_check_existence (state->setup->project->workspace_directory))
    {
        if (!kan_file_system_remove_directory_with_content (state->setup->project->workspace_directory))
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR, "Failed to cleanup project workspace at \"%s\".",
                     state->setup->project->workspace_directory);
            return KAN_RESOURCE_BUILD_RESULT_ERROR_WORKSPACE_CLEANUP_FAILED;
        }
    }

    if (!kan_file_system_make_directory (state->setup->project->workspace_directory))
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR, "Failed to make fresh project workspace directory at \"%s\".",
                 state->setup->project->workspace_directory);
        return KAN_RESOURCE_BUILD_RESULT_ERROR_WORKSPACE_CANNOT_MAKE_DIRECTORY;
    }

    return KAN_RESOURCE_BUILD_RESULT_SUCCESS;
}

static enum kan_resource_build_result_t load_resource_log_if_exists (struct build_state_t *state)
{
    struct kan_file_system_path_container_t resource_log_path;
    kan_file_system_path_container_copy_string (&resource_log_path, state->setup->project->workspace_directory);
    kan_file_system_path_container_append (&resource_log_path, KAN_RESOURCE_LOG_DEFAULT_NAME);

    if (!kan_file_system_check_existence (resource_log_path.path))
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_INFO,
                 "Resource log not found. Workspace will be cleaned and full rebuild will be triggered.");
        return cleanup_workspace (state);
    }

    struct kan_stream_t *log_stream = kan_direct_file_stream_open_for_read (resource_log_path.path, true);
    if (!log_stream)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "Unable to open resource log file for read. Workspace directory might be corrupted and needs manual "
                 "check at path \"%s\".",
                 state->setup->project->workspace_directory);
        return KAN_RESOURCE_BUILD_RESULT_ERROR_LOG_CANNOT_BE_OPENED;
    }

    log_stream = kan_random_access_stream_buffer_open_for_read (log_stream, KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER);
    CUSHION_DEFER { log_stream->operations->close (log_stream); }

    kan_resource_version_t saved_log_version = 0u;
    if (log_stream->operations->read (log_stream, sizeof (saved_log_version), &saved_log_version) !=
        sizeof (saved_log_version))
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                             "Failed to read saved resource log build version from \"%s\".", resource_log_path.path);
        return KAN_RESOURCE_BUILD_RESULT_ERROR_LOG_IO_ERROR;
    }

    if (saved_log_version != resource_build_version)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_INFO,
                 "Resource log is saved for another resource build version. Workspace will be cleaned and full rebuild "
                 "will be triggered.");
        return cleanup_workspace (state);
    }

    kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
        log_stream, &state->initial_log, KAN_STATIC_INTERNED_ID_GET (kan_resource_log_t), state->binary_script_storage,
        KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t),
        kan_resource_log_get_allocation_group ());

    CUSHION_DEFER { kan_serialization_binary_reader_destroy (reader); }
    enum kan_serialization_state_t serialization_state;

    while ((serialization_state = kan_serialization_binary_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    if (serialization_state == KAN_SERIALIZATION_FAILED)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                             "Failed to deserialize saved resource log from \"%s\".", resource_log_path.path);
        return KAN_RESOURCE_BUILD_RESULT_ERROR_LOG_IO_ERROR;
    }

    KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_DEBUG,
                         "Resource log successfully loaded from \"%s\".", resource_log_path.path);
    return KAN_RESOURCE_BUILD_RESULT_SUCCESS;
}

static enum kan_resource_build_result_t instantiate_initial_resource_log (struct build_state_t *state)
{
    // TODO: Implement. Instantiate data from resource log in targets. If log is empty, it will just mean not data.
    //       It should be implemented after the compilation routine when entry data structure is designed already.
    return KAN_RESOURCE_BUILD_RESULT_SUCCESS;
}

// Resource request feature section which is a significant part of resource build routine.

enum resource_request_mode_t
{
    /// \brief Dependency for build operation execution that must always be available.
    /// \details Causes resource to be loaded. Pauses build operation execution until available.
    RESOURCE_REQUEST_MODE_BUILD_REQUIRED = 0u,

    /// \brief Dependency for build operation execution that is allowed to be unavailable on some platforms.
    /// \details Causes resource to be loaded. Pauses build operation execution until available if can be
    ///          available at all.
    RESOURCE_REQUEST_MODE_BUILD_PLATFORM_OPTIONAL,

    /// \brief Used for resource status recursive hierarchical confirmation and for internal utility requests..
    RESOURCE_REQUEST_MODE_STATUS_CONFIRMATION,

    /// \brief Marks this resource as requested for deployment. Triggers resource build if necessary.
    RESOURCE_REQUEST_MODE_MARK_DEPLOYMENT,

    /// \brief Marks this resource as requested for deployment. Triggers resource build if necessary.
    /// \details Unlike `RESOURCE_REQUEST_MODE_MARK_DEPLOYMENT`, does not result in an error if resource has
    ///          platform unsupported status.
    RESOURCE_REQUEST_MODE_MARK_DEPLOYMENT_PLATFORM_OPTIONAL,

    /// \brief Marks this resource as needed for the build cache.
    /// \details Produces an error if this resource does not exist or is not available yet. Should only be used to mark
    ///          resources to be cached after the build operation is done.
    RESOURCE_REQUEST_MODE_MARK_CACHE,
};

struct resource_request_t
{
    struct target_t *from_target;
    kan_interned_string_t type;
    kan_interned_string_t name;
    enum resource_request_mode_t mode;

    /// \brief Entry that needs this resource for build operation if `RESOURCE_REQUEST_MODE_BUILD_REQUIRED` or
    ///        `RESOURCE_REQUEST_MODE_BUILD_PLATFORM_OPTIONAL` is used.
    struct resource_entry_t *needed_to_build_entry;
};

struct resource_response_t
{
    bool success;
    struct resource_entry_t *entry;
};

struct resource_request_backtrace_t
{
    const struct resource_request_backtrace_t *previous;
    kan_interned_string_t type;
    kan_interned_string_t name;
};

static struct resource_response_t execute_resource_request_internal (
    struct build_state_t *state,
    struct resource_request_t request,
    const struct resource_request_backtrace_t *backtrace);

static inline struct resource_response_t execute_resource_request (struct build_state_t *state,
                                                                   struct resource_request_t request)
{
    return execute_resource_request_internal (state, request, NULL);
}

static void add_to_build_queue_unsafe (struct build_state_t *state, struct resource_entry_t *entry);

static inline void confirm_resource_status (struct build_state_t *state,
                                            struct resource_entry_t *entry,
                                            const struct resource_request_backtrace_t *backtrace)
{
    {
        KAN_ATOMIC_INT_SCOPED_LOCK_READ (&entry->header.lock)
        if (entry->header.status != RESOURCE_STATUS_UNCONFIRMED)
        {
            return;
        }
    }

    // We do resource status confirmation without locking as calculated status should always be the same and chance of
    // calculating status twice should be quite rare.

    struct target_t *target = entry->target;
    enum resource_status_t new_status = RESOURCE_STATUS_AVAILABLE;
    struct kan_resource_log_version_t available_version = {.type_version = 0u, .last_modification_time = 0u};

    switch (entry->class)
    {
    case RESOURCE_PRODUCTION_CLASS_RAW:
    {
        if (!entry->current_file_location)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Marking raw resource \"%s\" of type \"%s\" as unavailable as it wasn't detected "
                     "during raw resource scan phase.",
                     target->next, entry->name, entry->type->name)

            new_status = RESOURCE_STATUS_UNAVAILABLE;
            break;
        }

        struct kan_file_system_entry_status_t file_status;
        if (!kan_file_system_query_entry (entry->current_file_location, &file_status))
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Marking raw resource \"%s\" of type \"%s\" as unavailable as it wasn't "
                     "possible to query its file status.",
                     target->next, entry->name, entry->type->name)

            new_status = RESOURCE_STATUS_UNAVAILABLE;
            break;
        }

        const struct kan_resource_reflected_data_resource_type_t *reflected_type =
            kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, entry->type->name);

        available_version.type_version = reflected_type->resource_type_meta->version;
        available_version.last_modification_time = file_status.last_modification_time_ns;

        if (!entry->initial_log_raw_entry ||
            !kan_resource_log_version_is_up_to_date (entry->initial_log_raw_entry->version, available_version))
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Marking raw resource \"%s\" of type \"%s\" as out of date in current build.",
                     target->next, entry->name, entry->type->name)
            new_status = RESOURCE_STATUS_BUILDING;
            break;
        }

        KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                 "[Target \"%s\"] Marking raw resource \"%s\" of type \"%s\" as up to date in current build.",
                 target->next, entry->name, entry->type->name)
        break;
    }

    case RESOURCE_PRODUCTION_CLASS_PRIMARY:
    {
        const struct kan_resource_reflected_data_resource_type_t *reflected_type =
            kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, entry->type->name);

        if (!reflected_type->produced_from_build_rule)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as unavailable because build rule "
                     "is no longer exists.",
                     target->next, entry->name, entry->type->name)
            new_status = RESOURCE_STATUS_UNAVAILABLE;
            break;
        }

        const struct kan_resource_log_built_entry_t *initial = entry->initial_log_built_entry;
        // Use initial version value if availability checks succeed.
        available_version = initial->version;

        KAN_ASSERT_FORMATTED (
            initial,
            "[Target %s] Built resource \"%s\" of type \"%s\" is in unconfirmed status and no initial log, which "
            "should be impossible as newly created built entries must start in building status right away.",
            target->name, entry->name, entry->type->name)

        if (initial->rule_version != reflected_type->build_rule_version)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because of build "
                     "rule version mismatch.",
                     target->next, entry->name, entry->type->name)
            new_status = RESOURCE_STATUS_BUILDING;
            break;
        }

        if (initial->version.type_version != reflected_type->resource_type_meta->version)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because of resource "
                     "type version mismatch.",
                     target->next, entry->name, entry->type->name)
            new_status = RESOURCE_STATUS_BUILDING;
            break;
        }

        if (reflected_type->build_rule_platform_configuration_type)
        {
            const struct platform_configuration_entry_t *platform_configuration =
                build_state_find_platform_configuration (state, reflected_type->build_rule_platform_configuration_type);

            if (!platform_configuration)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as unavailable because platform "
                         "configuration entry is absent",
                         target->next, entry->name, entry->type->name)
                new_status = RESOURCE_STATUS_UNAVAILABLE;
                break;
            }

            if (platform_configuration->file_time != initial->platform_configuration_time)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because of "
                         "platform configuration time mismatch.",
                         target->next, entry->name, entry->type->name)
                new_status = RESOURCE_STATUS_BUILDING;
                break;
            }
        }

        struct resource_response_t primary_input_response =
            execute_resource_request_internal (state,
                                               (struct resource_request_t) {
                                                   .from_target = target,
                                                   .type = reflected_type->build_rule_primary_input_type,
                                                   .name = entry->name,
                                                   .mode = RESOURCE_REQUEST_MODE_STATUS_CONFIRMATION,
                                                   .needed_to_build_entry = NULL,
                                               },
                                               backtrace);

        if (primary_input_response.success)
        {
            KAN_ATOMIC_INT_SCOPED_LOCK_READ (&primary_input_response.entry->header.lock)
            switch (primary_input_response.entry->header.status)
            {
            case RESOURCE_STATUS_UNCONFIRMED:
                KAN_ASSERT_FORMATTED (
                    false, "Internal error, entry still has unconfirmed even after status confirmation request.", )
                break;

            case RESOURCE_STATUS_UNAVAILABLE:
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as unavailable because primary "
                         "input is unavailable too.",
                         target->next, entry->name, entry->type->name)
                new_status = RESOURCE_STATUS_UNAVAILABLE;
                break;

            case RESOURCE_STATUS_BUILDING:
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because its "
                         "primary input already has building status.",
                         target->next, entry->name, entry->type->name)
                new_status = RESOURCE_STATUS_BUILDING;
                break;

            case RESOURCE_STATUS_AVAILABLE:
                if (!kan_resource_log_version_is_up_to_date (initial->primary_input_version,
                                                             primary_input_response.entry->header.available_version))
                {
                    KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                             "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because of "
                             "version mismatch with its primary input.",
                             target->next, entry->name, entry->type->name)
                    new_status = RESOURCE_STATUS_BUILDING;
                    break;
                }

                break;

            case RESOURCE_STATUS_PLATFORM_UNSUPPORTED:
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as platform unsupported because "
                         "primary input is platform unsupported too.",
                         target->next, entry->name, entry->type->name)
                new_status = RESOURCE_STATUS_PLATFORM_UNSUPPORTED;
                break;
            }

            if (new_status != RESOURCE_STATUS_AVAILABLE)
            {
                // Changed the status inside switch, break from the check.
                break;
            }
        }
        else
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as unavailable because primary "
                     "input can no longer be found.",
                     target->next, entry->name, entry->type->name)
            new_status = RESOURCE_STATUS_UNAVAILABLE;
            break;
        }

        for (kan_loop_size_t index = 0u; index < initial->secondary_inputs.size; ++index)
        {
            const struct kan_resource_log_secondary_input_t *secondary =
                &((struct kan_resource_log_secondary_input_t *) initial->secondary_inputs.data)[index];

            if (!secondary->type)
            {
                // Third party dependency, custom handling.
                struct raw_third_party_entry_t *third_party =
                    target_search_visible_third_party_unsafe (target, secondary->name);

                if (!third_party)
                {
                    KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                             "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because its "
                             "secondary input \"%s\" which is raw third party file cannot be found.",
                             target->next, entry->name, entry->type->name, secondary->name, secondary->type)
                    new_status = RESOURCE_STATUS_BUILDING;
                    break;
                }

                if (third_party->last_modification_time != secondary->version.last_modification_time)
                {
                    KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                             "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because its "
                             "secondary input \"%s\" which is raw third party file has been changed.",
                             target->next, entry->name, entry->type->name, secondary->name, secondary->type)
                    new_status = RESOURCE_STATUS_BUILDING;
                    break;
                }

                continue;
            }

            struct resource_response_t secondary_response =
                execute_resource_request_internal (state,
                                                   (struct resource_request_t) {
                                                       .from_target = target,
                                                       .type = secondary->type,
                                                       .name = secondary->name,
                                                       .mode = RESOURCE_REQUEST_MODE_STATUS_CONFIRMATION,
                                                       .needed_to_build_entry = NULL,
                                                   },
                                                   backtrace);

            if (!secondary_response.success)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because its "
                         "secondary input \"%s\" of type \"%s\" cannot be found.",
                         target->next, entry->name, entry->type->name, secondary->name, secondary->type)
                new_status = RESOURCE_STATUS_BUILDING;
                break;
            }

            KAN_ATOMIC_INT_SCOPED_LOCK_READ (&secondary_response.entry->header.lock)
            if (secondary_response.entry->header.status != RESOURCE_STATUS_AVAILABLE)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because its "
                         "secondary input \"%s\" of type \"%s\" has other status than available.",
                         target->next, entry->name, entry->type->name, secondary->name, secondary->type)
                new_status = RESOURCE_STATUS_BUILDING;
                break;
            }

            if (!kan_resource_log_version_is_up_to_date (secondary->version,
                                                         secondary_response.entry->header.available_version))
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because its "
                         "secondary input \"%s\" of type \"%s\" version mismatch.",
                         target->next, entry->name, entry->type->name, secondary->name, secondary->type)
                new_status = RESOURCE_STATUS_BUILDING;
                break;
            }
        }

        if (new_status != RESOURCE_STATUS_AVAILABLE)
        {
            break;
        }

        KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                 "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as up to date.", target->next,
                 entry->name, entry->type->name)
        break;
    }

    case RESOURCE_PRODUCTION_CLASS_SECONDARY:
    case RESOURCE_PRODUCTION_CLASS_SECONDARY_MERGEABLE:
        // We don't do any status checks for secondary products as resource that uses other resource secondary products
        // must also depend on producer resource. And if any old reference persists after that we can detect it through
        // secondary resource source resource version mismatch while caching or deploying.
        available_version = entry->initial_log_secondary_entry->version;
        break;
    }

    KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&entry->header.lock)
    if (entry->header.status != RESOURCE_STATUS_UNCONFIRMED)
    {
        // We've calculated status for this entry two times.
        // It is possible due to absence of locking and should be fine as it won't change the result of the execution.
        return;
    }

    entry->header.status = new_status;
    switch (entry->header.status)
    {
    case RESOURCE_STATUS_UNCONFIRMED:
        KAN_ASSERT (false)
        break;

    case RESOURCE_STATUS_UNAVAILABLE:
        // Nothing special to do.
        break;

    case RESOURCE_STATUS_BUILDING:
    {
        KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&entry->build.lock)
        entry->build.internal_next_build_task = RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_START;

        KAN_ATOMIC_INT_SCOPED_LOCK (&state->build_queue_lock)
        add_to_build_queue_unsafe (state, entry);
        break;
    }

    case RESOURCE_STATUS_AVAILABLE:
    case RESOURCE_STATUS_PLATFORM_UNSUPPORTED:
        entry->header.available_version = available_version;
        break;
    }
}

static void print_circular_reference_backtrace (const struct resource_request_backtrace_t *backtrace)
{
    char chain_buffer[KAN_RESOURCE_PIPELINE_BUILD_CR_MESSAGE_BUFFER];
    kan_instance_size_t chain_size = 0u;

    while (backtrace)
    {
#define ADD_TO_CHAIN(STRING)                                                                                           \
    {                                                                                                                  \
        const char *cursor = STRING;                                                                                   \
        while (*cursor && chain_size < KAN_RESOURCE_PIPELINE_BUILD_CR_MESSAGE_BUFFER - 1u)                             \
        {                                                                                                              \
            chain_buffer[chain_size] = *cursor;                                                                        \
            ++cursor;                                                                                                  \
            ++chain_size;                                                                                              \
        }                                                                                                              \
    }

        ADD_TO_CHAIN (backtrace->type)
        ADD_TO_CHAIN ("::")
        ADD_TO_CHAIN (backtrace->name)

        if (backtrace->previous)
        {
            ADD_TO_CHAIN (" <= ")
        }

#undef ADD_TO_CHAIN
        backtrace = backtrace->previous;
    }

    chain_buffer[chain_size] = '\0';
    KAN_LOG_WITH_BUFFER (KAN_RESOURCE_PIPELINE_BUILD_CR_MESSAGE_BUFFER * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                         "Caught circular reference while executing resource request with build operation or status "
                         "confirmation mode. It usually means that there is a circular dependency between build rules. "
                         "Printing reference chain (left is the deepest): %s.",
                         chain_buffer)
}

/// \details Needs external write access to build state of blocking entry, therefore marked unsafe.
static inline void block_entry_build_by_entry_unsafe (struct resource_entry_t *entry_to_build,
                                                      struct resource_entry_t *blocking_entry)
{
    kan_atomic_int_add (&entry_to_build->build.atomic_next_build_task_block_counter, 1);
    struct resource_entry_build_blocked_t *blocked =
        kan_allocate_batched (build_queue_allocation_group, sizeof (struct resource_entry_build_blocked_t));
    blocked->blocked_entry = entry_to_build;

    blocked->next = blocking_entry->build.blocked_other_first;
    blocking_entry->build.blocked_other_first = blocked;
}

static void *load_resource_entry_data (struct build_state_t *state, struct resource_entry_t *entry);

static bool process_resource_as_build_dependency (struct build_state_t *state,
                                                  struct resource_entry_t *entry,
                                                  bool required,
                                                  struct resource_entry_t *needed_to_build_entry)
{
    KAN_ATOMIC_INT_SCOPED_LOCK_READ (&entry->header.lock)
    switch (entry->header.status)
    {
    case RESOURCE_STATUS_UNCONFIRMED:
    {
        // If other logic works as expected, it can never happen.
        KAN_ASSERT (false)
        return false;
    }

    case RESOURCE_STATUS_UNAVAILABLE:
    {
        // Not available, there is not a valid build dependency.
        return false;
    }

    case RESOURCE_STATUS_BUILDING:
    {
        KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&entry->build.lock)
        block_entry_build_by_entry_unsafe (needed_to_build_entry, entry);
        return true;
    }

    case RESOURCE_STATUS_AVAILABLE:
    {
        // Can trigger loading, therefore a write lock.
        KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&entry->build.lock)
        ++entry->build.loaded_data_request_count;

        if (!entry->build.loaded_data)
        {
            if (entry->build.loaded_data_request_count == 1u)
            {
                // First request, we need to properly start the build.
                switch (entry->class)
                {
                case RESOURCE_PRODUCTION_CLASS_RAW:
                case RESOURCE_PRODUCTION_CLASS_PRIMARY:
                case RESOURCE_PRODUCTION_CLASS_SECONDARY:
                {
                    block_entry_build_by_entry_unsafe (needed_to_build_entry, entry);
                    entry->build.internal_next_build_task = RESOURCE_ENTRY_NEXT_BUILD_TASK_LOAD;

                    KAN_ATOMIC_INT_SCOPED_LOCK (&state->build_queue_lock)
                    add_to_build_queue_unsafe (state, entry);
                    break;
                }

                case RESOURCE_PRODUCTION_CLASS_SECONDARY_MERGEABLE:
                {
                    // For simplicity of operating with mergeables, we always load them right away under the lock.
                    // Mergeables must be relatively small (as there is no sense for huge ones),
                    // therefore it should not be an issue to do so.
                    entry->build.loaded_data = load_resource_entry_data (state, entry);

                    if (!entry->build.loaded_data)
                    {
                        return false;
                    }

                    break;
                }
                }
            }
        }

        return true;
    }

    case RESOURCE_STATUS_PLATFORM_UNSUPPORTED:
    {
        // Platform unsupported are treated as valid dependencies unless required flag is set.
        return !required;
    }
    }

    return false;
}

static inline const struct kan_dynamic_array_t *get_references_from_resource_entry_unsafe (
    struct resource_entry_t *entry)
{
    // If assert fails, then caller didn't ensure that build is finished prior to accessing references.
    KAN_ASSERT (entry->header.status == RESOURCE_STATUS_AVAILABLE ||
                entry->header.status == RESOURCE_STATUS_PLATFORM_UNSUPPORTED)

    if (kan_resource_log_version_is_up_to_date (entry->initial_log_built_entry->version,
                                                entry->header.available_version))
    {
        switch (entry->class)
        {
        case RESOURCE_PRODUCTION_CLASS_RAW:
            return &entry->initial_log_raw_entry->references;

        case RESOURCE_PRODUCTION_CLASS_PRIMARY:
            return &entry->initial_log_built_entry->references;

        case RESOURCE_PRODUCTION_CLASS_SECONDARY:
        case RESOURCE_PRODUCTION_CLASS_SECONDARY_MERGEABLE:
            return &entry->initial_log_secondary_entry->references;
        }
    }

    return &entry->new_references;
}

static bool mark_resource_references_for_deployment (struct build_state_t *state,
                                                     struct resource_entry_t *entry,
                                                     const struct resource_request_backtrace_t *backtrace)
{
    const struct kan_dynamic_array_t *source_array;
    kan_atomic_int_lock_read (&entry->header.lock);
    source_array = get_references_from_resource_entry_unsafe (entry);
    kan_atomic_int_unlock_read (&entry->header.lock);

    // We don't need full locking here as `new_references` should not be changed after build and function
    // caller should ensure that this function is only being called after the build.

    for (kan_loop_size_t index = 0u; index < source_array->size; ++index)
    {
        const struct kan_resource_log_reference_t *reference =
            &((struct kan_resource_log_reference_t *) source_array->data)[index];

        if (!reference->type)
        {
            KAN_LOG (
                resource_pipeline_build, KAN_LOG_ERROR,
                "[Target \"%s\"] Resource \"%s\" of type \"%s\" contains reference to raw third party file \"%s\" "
                "while being marked for deployment. Deployed resources must not contain any third party references.",
                entry->target->name, entry->name, entry->type->name, reference->name);
            return false;
        }

        struct resource_request_t reference_request = {
            .from_target = entry->target,
            .type = reference->type,
            .name = reference->name,
            .mode = (reference->flags & KAN_RESOURCE_REFERENCE_REQUIRED) ?
                        RESOURCE_REQUEST_MODE_MARK_DEPLOYMENT :
                        RESOURCE_REQUEST_MODE_MARK_DEPLOYMENT_PLATFORM_OPTIONAL,
            .needed_to_build_entry = NULL,
        };

        struct resource_response_t reference_response =
            execute_resource_request_internal (state, reference_request, backtrace);

        if (!reference_response.success)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to mark \"%s\" of type \"%s\" for deployment (it needs to be deployed as "
                     "it is referenced from \"%s\" of type \"%s\" which is deployed).",
                     entry->target->name, reference->name, reference->type, entry->name, entry->type->name);
            return false;
        }
    }

    return true;
}

static bool mark_resource_build_dependencies_for_cache (struct build_state_t *state,
                                                        struct resource_entry_t *entry,
                                                        const struct resource_request_backtrace_t *backtrace)
{
    if (entry->class != RESOURCE_PRODUCTION_CLASS_PRIMARY)
    {
        // No dependencies to mark.
        return true;
    }

    const struct kan_dynamic_array_t *source_array;
    bool new_build;

    {
        KAN_ATOMIC_INT_SCOPED_LOCK_READ (&entry->header.lock)
        // If assert fails, then caller didn't ensure that build is finished prior to marking dependencies for cache.
        KAN_ASSERT (entry->header.status == RESOURCE_STATUS_AVAILABLE ||
                    entry->header.status == RESOURCE_STATUS_PLATFORM_UNSUPPORTED)

        if (kan_resource_log_version_is_up_to_date (entry->initial_log_built_entry->version,
                                                    entry->header.available_version))
        {
            source_array = &entry->initial_log_built_entry->secondary_inputs;
            new_build = false;
        }
        else
        {
            source_array = &entry->new_build_secondary_inputs;
            new_build = true;
        }
    }

    // We don't need full locking here as `new_build_secondary_inputs` should not be changed after build and function
    // caller should ensure that this function is only being called after the build.

    const struct kan_resource_reflected_data_resource_type_t *reflected_type =
        kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, entry->type->name);

    struct resource_request_t primary_input_request = {
        .from_target = entry->target,
        .type = reflected_type->build_rule_primary_input_type,
        .name = entry->name,
        .mode = RESOURCE_REQUEST_MODE_MARK_CACHE,
        .needed_to_build_entry = NULL,
    };

    struct resource_response_t primary_input_response =
        execute_resource_request_internal (state, primary_input_request, backtrace);

    if (!primary_input_response.success)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to mark \"%s\" of type \"%s\" for cache (it needs to be cached as build "
                 "dependency of \"%s\" of type \"%s\").",
                 entry->target->name, entry->name, reflected_type->build_rule_primary_input_type, entry->name,
                 entry->type->name);
        return false;
    }

    for (kan_loop_size_t index = 0u; index < source_array->size; ++index)
    {
        struct resource_request_t secondary_input_request = {
            .from_target = entry->target,
            .type = NULL,
            .name = NULL,
            .mode = RESOURCE_REQUEST_MODE_MARK_CACHE,
            .needed_to_build_entry = NULL,
        };

        if (new_build)
        {
            struct new_build_secondary_input_t *secondary =
                &((struct new_build_secondary_input_t *) source_array->data)[index];

            if (!secondary->entry)
            {
                // Third party resources cannot be cached and cannot be marked for cache.
                continue;
            }

            secondary_input_request.type = secondary->entry->type->name;
            secondary_input_request.name = secondary->entry->name;
        }
        else
        {
            const struct kan_resource_log_secondary_input_t *secondary =
                &((struct kan_resource_log_secondary_input_t *) source_array->data)[index];

            if (!secondary->type)
            {
                // Third party resources cannot be cached and cannot be marked for cache.
                continue;
            }

            secondary_input_request.type = secondary->type;
            secondary_input_request.name = secondary->name;
        }

        struct resource_response_t secondary_input_response =
            execute_resource_request_internal (state, secondary_input_request, backtrace);

        if (!secondary_input_response.success)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to mark \"%s\" of type \"%s\" for cache (it needs to be cached as build "
                     "dependency of \"%s\" of type \"%s\").",
                     entry->target->name, secondary_input_request.name, secondary_input_request.type, entry->name,
                     entry->type->name);
            return false;
        }
    }

    return true;
}

static bool mark_resource_for_deployment (struct build_state_t *state,
                                          struct resource_entry_t *entry,
                                          bool required,
                                          const struct resource_request_backtrace_t *backtrace)
{
    {
        KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&entry->header.lock)
        if (entry->header.deployment_mark)
        {
            // Already marked, no need to mark referenced resources for deployment and dependencies for the cache.
            return true;
        }

        entry->header.deployment_mark = true;
        switch (entry->header.status)
        {
        case RESOURCE_STATUS_UNCONFIRMED:
        case RESOURCE_STATUS_UNAVAILABLE:
        case RESOURCE_STATUS_BUILDING:
            // Neither references nor dependency list are ready, so there is nothing more to mark right now.
            // It also means that these marks will be applied in the end of build operation as deployment mark will be
            // visible as already applied to the build operation when it ends.
            return true;

        case RESOURCE_STATUS_AVAILABLE:
            // Cannot return right now, we also need to mark references and dependencies properly.
            break;

        case RESOURCE_STATUS_PLATFORM_UNSUPPORTED:
            if (!required)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                         "[Target \"%s\"] Failed to mark \"%s\" of type \"%s\" for deployment as it is unsupported on "
                         "this platform, but reference field meta does not allow platform unsupported resources.",
                         entry->target->name, entry->name, entry->type->name);
                return false;
            }

            break;
        }
    }

    return mark_resource_references_for_deployment (state, entry, backtrace) &&
           mark_resource_build_dependencies_for_cache (state, entry, backtrace);
}

static bool mark_resource_for_cache (struct build_state_t *state,
                                     struct resource_entry_t *entry,
                                     const struct resource_request_backtrace_t *backtrace)
{
    {
        KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&entry->header.lock)
        switch (entry->header.status)
        {
        case RESOURCE_STATUS_UNCONFIRMED:
        case RESOURCE_STATUS_UNAVAILABLE:
        case RESOURCE_STATUS_BUILDING:
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to mark \"%s\" of type \"%s\" for cache as it is neither available "
                     "nor platform unsupported.",
                     entry->target->name, entry->name, entry->type->name);
            return false;

        case RESOURCE_STATUS_AVAILABLE:
        case RESOURCE_STATUS_PLATFORM_UNSUPPORTED:
            // It is okay to have that status while marking for the cache.
            break;
        }

        if (entry->header.cache_mark)
        {
            // Already marked, no need to mark dependencies for the cache.
            return true;
        }

        entry->header.cache_mark = true;
    }

    return mark_resource_build_dependencies_for_cache (state, entry, backtrace);
}

static struct resource_response_t execute_resource_request_internal (
    struct build_state_t *state,
    struct resource_request_t request,
    const struct resource_request_backtrace_t *backtrace)
{
    KAN_ASSERT_FORMATTED (request.type,
                          "Caught attempt to use execute_resource_request to get third party resource, which is not "
                          "supported, it is an internal error.", )

    struct resource_response_t response = {
        .success = false,
        .entry = NULL,
    };

    const struct resource_request_backtrace_t trace = {
        .previous = backtrace,
        .type = request.type,
        .name = request.name,
    };

    switch (request.mode)
    {
    case RESOURCE_REQUEST_MODE_BUILD_REQUIRED:
    case RESOURCE_REQUEST_MODE_BUILD_PLATFORM_OPTIONAL:
    case RESOURCE_REQUEST_MODE_STATUS_CONFIRMATION:
    {
        // Check for circular request dependency, so we don't end up in a deadlock.
        const struct resource_request_backtrace_t *circular_check = backtrace;

        while (circular_check)
        {
            if (circular_check->type == request.type && circular_check->name == request.name)
            {
                print_circular_reference_backtrace (&trace);
                return response;
            }

            circular_check = circular_check->previous;
        }

        break;
    }

    case RESOURCE_REQUEST_MODE_MARK_DEPLOYMENT:
    case RESOURCE_REQUEST_MODE_MARK_DEPLOYMENT_PLATFORM_OPTIONAL:
    case RESOURCE_REQUEST_MODE_MARK_CACHE:
        // No preprocess step.
        break;
    }

    // First pass: try to find among the ones that already exist in shared read-only mode.
    kan_atomic_int_lock_read (&state->resource_entries_lock);
    response.entry = target_search_visible_resource_unsafe (request.from_target, request.type, request.name);
    kan_atomic_int_unlock_read (&state->resource_entries_lock);

    if (!response.entry)
    {
        // Resource is nowhere to be found. Decide what could be done next depending on the mode.
        switch (request.mode)
        {
        case RESOURCE_REQUEST_MODE_BUILD_REQUIRED:
        case RESOURCE_REQUEST_MODE_BUILD_PLATFORM_OPTIONAL:
        case RESOURCE_REQUEST_MODE_STATUS_CONFIRMATION:
        case RESOURCE_REQUEST_MODE_MARK_DEPLOYMENT:
        case RESOURCE_REQUEST_MODE_MARK_DEPLOYMENT_PLATFORM_OPTIONAL:
        {
            // Resource can be produced by build rule. Let's check if it exists.
            const struct kan_resource_reflected_data_resource_type_t *reflected_type =
                kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, request.type);

            if (reflected_type->produced_from_build_rule)
            {
                struct resource_response_t primary_input_response =
                    execute_resource_request_internal (state,
                                                       (struct resource_request_t) {
                                                           .from_target = request.from_target,
                                                           .type = reflected_type->build_rule_primary_input_type,
                                                           .name = request.name,
                                                           .mode = RESOURCE_REQUEST_MODE_STATUS_CONFIRMATION,
                                                           .needed_to_build_entry = NULL,
                                                       },
                                                       &trace);

                if (primary_input_response.success)
                {
                    // Resource can be built using a rule, so let's create an entry and try to build it.
                    // Lock for write as we're creating new entry.
                    KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&state->resource_entries_lock)

                    // Built resource should always be created in the same target as its primary input.
                    struct target_t *primary_target = primary_input_response.entry->target;

                    // While we were waiting for write access, somebody else might've already got write access and
                    // create this node too. Let's check it.
                    response.entry = target_search_local_resource_unsafe (primary_target, request.type, request.name);

                    if (!response.entry)
                    {
                        struct resource_type_container_t *container =
                            target_search_resource_type_container_unsafe (primary_target, request.type);

                        if (!container)
                        {
                            container = resource_type_container_create (primary_target, reflected_type->source_type);
                        }

                        // TODO: What about name collisions?

                        response.entry = resource_entry_create (container, request.name);
                        response.entry->class = RESOURCE_PRODUCTION_CLASS_PRIMARY;

                        KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&response.entry->header.lock)
                        KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&response.entry->build.lock)

                        response.entry->header.status = RESOURCE_STATUS_BUILDING;
                        response.entry->build.internal_next_build_task = RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_START;

                        KAN_ATOMIC_INT_SCOPED_LOCK (&state->build_queue_lock)
                        add_to_build_queue_unsafe (state, response.entry);
                    }
                }
            }

            if (!response.entry)
            {
                // Entry just does not exist, return default unavailable response.
                return response;
            }

            break;
        }

        case RESOURCE_REQUEST_MODE_MARK_CACHE:
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to mark \"%s\" of type \"%s\" for cache as it does not exist.",
                     request.from_target->name, request.name, request.type);
            return response;
        }
    }

    KAN_ASSERT (response.entry)
    confirm_resource_status (state, response.entry, &trace);

    switch (request.mode)
    {
    case RESOURCE_REQUEST_MODE_BUILD_REQUIRED:
        response.success =
            process_resource_as_build_dependency (state, response.entry, true, request.needed_to_build_entry);
        break;

    case RESOURCE_REQUEST_MODE_BUILD_PLATFORM_OPTIONAL:
        response.success =
            process_resource_as_build_dependency (state, response.entry, false, request.needed_to_build_entry);
        break;

    case RESOURCE_REQUEST_MODE_STATUS_CONFIRMATION:
        // No additional tasks, confirmation is executed, so we can just mark response as successful and that's all.
        response.success = true;
        break;

    case RESOURCE_REQUEST_MODE_MARK_DEPLOYMENT:
        response.success = mark_resource_for_deployment (state, response.entry, true, &trace);
        break;

    case RESOURCE_REQUEST_MODE_MARK_DEPLOYMENT_PLATFORM_OPTIONAL:
        response.success = mark_resource_for_deployment (state, response.entry, false, &trace);
        break;

    case RESOURCE_REQUEST_MODE_MARK_CACHE:
        response.success = mark_resource_for_cache (state, response.entry, &trace);
        break;
    }

    return response;
}

// Build task feature section which implements core routine of building resources.

static const char *get_resource_entry_next_build_task_name (enum resource_entry_next_build_task_t task)
{
    switch (task)
    {
    case RESOURCE_ENTRY_NEXT_BUILD_TASK_NONE:
        return "<none>";

    case RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_START:
        return "build: start";

    case RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_PROCESS_PRIMARY:
        return "build: process primary input";

    case RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_EXECUTE_BUILD_RULE:
        return "build: execute build rule";

    case RESOURCE_ENTRY_NEXT_BUILD_TASK_LOAD:
        return "load";
    }

    return "<unknown>";
}

/// \brief Tasks that can be executed after main build operation for helper causes (like loading built entry that is
///        already unloaded) are called repeated tasks.
static bool is_resource_entry_next_build_task_repeated (enum resource_entry_next_build_task_t task)
{
    switch (task)
    {
    case RESOURCE_ENTRY_NEXT_BUILD_TASK_NONE:
    case RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_START:
    case RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_PROCESS_PRIMARY:
    case RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_EXECUTE_BUILD_RULE:
        return false;

    case RESOURCE_ENTRY_NEXT_BUILD_TASK_LOAD:
        return true;
    }

    return false;
}

enum build_step_result_t
{
    BUILD_STEP_RESULT_SUCCESSFUL = 0u,
    BUILD_STEP_RESULT_FAILED,
    BUILD_STEP_RESULT_PAUSED,
};

static void *load_resource_entry_data (struct build_state_t *state, struct resource_entry_t *entry)
{
    if (!entry->current_file_location)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to load \"%s\" of type \"%s\" as there is no path provided for it due to "
                 "internal error.",
                 entry->target->name, entry->name, entry->type->name);
        return NULL;
    }

    struct kan_stream_t *stream = kan_direct_file_stream_open_for_read (entry->current_file_location, false);
    if (!stream)
    {
        KAN_LOG (
            resource_pipeline_build, KAN_LOG_ERROR,
            "[Target \"%s\"] Failed to load \"%s\" of type \"%s\" as it wasn't possible to open file at path \"%s\".",
            entry->target->name, entry->name, entry->type->name, entry->current_file_location);
        return NULL;
    }

    stream = kan_random_access_stream_buffer_open_for_read (stream, KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER);
    CUSHION_DEFER { stream->operations->close (stream); }

    void *loaded_data = kan_allocate_general (entry->allocation_group, entry->type->size, entry->type->alignment);
    if (entry->type->init)
    {
        kan_allocation_group_stack_push (entry->allocation_group);
        entry->type->init (entry->type->functor_user_data, loaded_data);
        kan_allocation_group_stack_pop ();
    }

    kan_instance_size_t location_length = (kan_instance_size_t) strlen (entry->current_file_location);
    enum kan_serialization_state_t serialization_state = KAN_SERIALIZATION_FAILED;

    if (location_length >= 4u && entry->current_file_location[location_length - 4u] == '.' &&
        entry->current_file_location[location_length - 3u] == 'b' &&
        entry->current_file_location[location_length - 2u] == 'i' &&
        entry->current_file_location[location_length - 1u] == 'n')
    {
        kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
            stream, loaded_data, entry->type->name, state->binary_script_storage,
            KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t), entry->allocation_group);

        while ((serialization_state = kan_serialization_binary_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
        {
        }

        if (serialization_state == KAN_SERIALIZATION_FAILED)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to load \"%s\" of type \"%s\" as due to binary serialization error.",
                     entry->target->name, entry->name, entry->type->name, entry->current_file_location);
        }

        kan_serialization_binary_reader_destroy (reader);
    }
    else if (location_length >= 3u && entry->current_file_location[location_length - 3u] == '.' &&
             entry->current_file_location[location_length - 2u] == 'r' &&
             entry->current_file_location[location_length - 1u] == 'd')
    {
        kan_serialization_rd_reader_t reader = kan_serialization_rd_reader_create (
            stream, loaded_data, entry->type->name, state->setup->reflected_data->registry, entry->allocation_group);

        while ((serialization_state = kan_serialization_rd_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
        {
        }

        if (serialization_state == KAN_SERIALIZATION_FAILED)
        {
            KAN_LOG (
                resource_pipeline_build, KAN_LOG_ERROR,
                "[Target \"%s\"] Failed to load \"%s\" of type \"%s\" as due to readable data serialization error.",
                entry->target->name, entry->name, entry->type->name, entry->current_file_location);
        }

        kan_serialization_rd_reader_destroy (reader);
    }
    else
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to load \"%s\" of type \"%s\" as it wasn't possible to guess the serialized "
                 "format from path \"%s\".",
                 entry->target->name, entry->name, entry->type->name, entry->current_file_location);
    }

    if (serialization_state != KAN_SERIALIZATION_FINISHED)
    {
        if (entry->type->shutdown)
        {
            entry->type->shutdown (entry->type->functor_user_data, loaded_data);
        }

        kan_free_general (entry->allocation_group, loaded_data, entry->type->size);
        loaded_data = NULL;
    }

    return loaded_data;
}

struct build_step_output_t
{
    enum build_step_result_t result;
    struct kan_resource_log_version_t available_version;
    void *loaded_data_to_manage;
};

static struct build_step_output_t execute_build_start_raw (struct build_state_t *state, struct resource_entry_t *entry)
{
    struct build_step_output_t output = {
        .result = BUILD_STEP_RESULT_FAILED,
        .available_version = {.type_version = 0u, .last_modification_time = 0u},
        .loaded_data_to_manage = NULL,
    };

    if (!entry->current_file_location)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to process build start for \"%s\" of type \"%s\" as there is no path provided "
                 "for it due to internal error.",
                 entry->target->name, entry->name, entry->type->name);
        return output;
    }

    struct kan_file_system_entry_status_t status;
    if (!kan_file_system_query_entry (entry->current_file_location, &status))
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to process build start for \"%s\" of type \"%s\" as it wasn't possible to "
                 "query file status at \"%s\".",
                 entry->target->name, entry->name, entry->type->name, entry->current_file_location);
        return output;
    }

    const struct kan_resource_reflected_data_resource_type_t *reflected_type =
        kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, entry->type->name);

    output.available_version.type_version = reflected_type->resource_type_meta->version;
    output.available_version.last_modification_time = status.last_modification_time_ns;

    if (!(output.loaded_data_to_manage = load_resource_entry_data (state, entry)))
    {
        return output;
    }

    // Should not be able to start build twice for one resource.
    KAN_ASSERT (entry->new_references.size == 0u)
    kan_resource_reflected_data_storage_detect_references (state->setup->reflected_data, entry->type->name,
                                                           output.loaded_data_to_manage, &entry->new_references);

    entry->build.internal_next_build_task = RESOURCE_ENTRY_NEXT_BUILD_TASK_NONE;
    output.result = BUILD_STEP_RESULT_SUCCESSFUL;
    return output;
}

static struct build_step_output_t execute_build_process_primary (struct build_state_t *state,
                                                                 struct resource_entry_t *entry);

static struct build_step_output_t execute_build_start_primary (struct build_state_t *state,
                                                               struct resource_entry_t *entry)
{
    struct build_step_output_t output = {
        .result = BUILD_STEP_RESULT_FAILED,
        .available_version = {.type_version = 0u, .last_modification_time = 0u},
        .loaded_data_to_manage = NULL,
    };

    const struct kan_resource_reflected_data_resource_type_t *reflected_type =
        kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, entry->type->name);
    KAN_ASSERT (reflected_type->produced_from_build_rule)

    struct resource_response_t response =
        execute_resource_request (state, (struct resource_request_t) {
                                             .from_target = entry->target,
                                             .type = reflected_type->build_rule_primary_input_type,
                                             .name = entry->name,
                                             .mode = RESOURCE_REQUEST_MODE_BUILD_REQUIRED,
                                             .needed_to_build_entry = entry,
                                         });

    if (!response.success)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to process build start for \"%s\" of type \"%s\" as it wasn't possible to "
                 "request build rule primary input of type \"%s\".",
                 entry->target->name, entry->name, entry->type->name, reflected_type->build_rule_primary_input_type);
        return output;
    }

    entry->build.internal_primary_input_entry = response.entry;
    if (kan_atomic_int_get (&entry->build.atomic_next_build_task_block_counter) > 0)
    {
        entry->build.internal_next_build_task = RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_PROCESS_PRIMARY;
        output.result = BUILD_STEP_RESULT_PAUSED;
        return output;
    }

    return execute_build_process_primary (state, entry);
}

static struct build_step_output_t execute_build_execute_build_rule (struct build_state_t *state,
                                                                    struct resource_entry_t *entry);

static struct build_step_output_t execute_build_process_primary (struct build_state_t *state,
                                                                 struct resource_entry_t *entry)
{
    struct build_step_output_t output = {
        .result = BUILD_STEP_RESULT_FAILED,
        .available_version = {.type_version = 0u, .last_modification_time = 0u},
        .loaded_data_to_manage = NULL,
    };

    const struct kan_dynamic_array_t *primary_reference_array = NULL;
    {
        struct resource_entry_t *primary = entry->build.internal_primary_input_entry;
        KAN_ATOMIC_INT_SCOPED_LOCK_READ (&primary->header.lock)

        if (primary->header.status != RESOURCE_STATUS_AVAILABLE)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to build \"%s\" of type \"%s\" as build rule primary input of type \"%s\" "
                     "is not available.",
                     entry->target->name, entry->name, entry->type->name, primary->type->name);
            return output;
        }

        primary_reference_array = get_references_from_resource_entry_unsafe (primary);
    }

    const struct kan_resource_reflected_data_resource_type_t *reflected_type =
        kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, entry->type->name);
    KAN_ASSERT (reflected_type->produced_from_build_rule)

    kan_dynamic_array_set_capacity (&entry->new_build_secondary_inputs, primary_reference_array->size);
    bool has_failed_secondary_inputs = false;

    for (kan_loop_size_t reference_index = 0u; reference_index < primary_reference_array->size; ++reference_index)
    {
        struct kan_resource_log_reference_t *reference =
            &((struct kan_resource_log_reference_t *) primary_reference_array->data)[reference_index];

        if (reference->type)
        {
            bool used_for_build = false;
            for (kan_loop_size_t type_index = 0u; type_index < reflected_type->build_rule_secondary_types.size;
                 ++type_index)
            {
                if (reference->type ==
                    ((kan_interned_string_t *) reflected_type->build_rule_secondary_types.data)[type_index])
                {
                    used_for_build = true;
                    break;
                }
            }

            if (!used_for_build)
            {
                // Not used for build, skip.
                continue;
            }

            struct resource_response_t response = execute_resource_request (
                state, (struct resource_request_t) {
                           .from_target = entry->target,
                           .type = reference->type,
                           .name = reference->name,
                           .mode = (reference->flags & KAN_RESOURCE_REFERENCE_REQUIRED) ?
                                       RESOURCE_REQUEST_MODE_BUILD_REQUIRED :
                                       RESOURCE_REQUEST_MODE_MARK_DEPLOYMENT_PLATFORM_OPTIONAL,
                           .needed_to_build_entry = entry,
                       });

            if (!response.success)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                         "[Target \"%s\"] Failed to find secondary input \"%s\" of type \"%s\" to build \"%s\" of type "
                         "\"%s\".",
                         entry->target->name, reference->name, reference->type, entry->name, entry->type->name);

                has_failed_secondary_inputs = true;
                continue;
            }

            struct new_build_secondary_input_t *new_input =
                kan_dynamic_array_add_last (&entry->new_build_secondary_inputs);
            new_input->entry = response.entry;
            new_input->third_party_entry = NULL;
            new_input->reference_flags = reference->flags;
        }
        else
        {
            struct raw_third_party_entry_t *third_party_entry =
                target_search_visible_third_party_unsafe (entry->target, reference->type);

            if (!third_party_entry)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                         "[Target \"%s\"] Failed to find third party input \"%s\" to build \"%s\" of type \"%s\".",
                         entry->target->name, reference->name, entry->name, entry->type->name);

                has_failed_secondary_inputs = true;
                continue;
            }

            struct new_build_secondary_input_t *new_input =
                kan_dynamic_array_add_last (&entry->new_build_secondary_inputs);
            new_input->entry = NULL;
            new_input->third_party_entry = third_party_entry;
            new_input->reference_flags = reference->flags;
        }
    }

    kan_dynamic_array_set_capacity (&entry->new_build_secondary_inputs, entry->new_build_secondary_inputs.size);
    if (has_failed_secondary_inputs)
    {
        KAN_LOG (
            resource_pipeline_build, KAN_LOG_ERROR,
            "[Target \"%s\"] Failed to build \"%s\" of type \"%s\" as it wasn't possible to request secondary inputs.",
            entry->target->name, entry->name, entry->type->name);
        return output;
    }

    if (kan_atomic_int_get (&entry->build.atomic_next_build_task_block_counter) > 0)
    {
        entry->build.internal_next_build_task = RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_EXECUTE_BUILD_RULE;
        output.result = BUILD_STEP_RESULT_PAUSED;
        return output;
    }

    return execute_build_execute_build_rule (state, entry);
}

static struct resource_type_container_t *find_or_create_container_for_secondary (
    struct build_state_t *state, struct target_t *target, const struct kan_reflection_struct_t *type)
{
    struct resource_type_container_t *container;
    {
        KAN_ATOMIC_INT_SCOPED_LOCK_READ (&state->resource_entries_lock)
        container = target_search_resource_type_container_unsafe (target, type->name);

        if (container)
        {
            return container;
        }
    }

    KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&state->resource_entries_lock)
    container = target_search_resource_type_container_unsafe (target, type->name);

    if (!container)
    {
        container = resource_type_container_create (target, type);
    }

    return container;
}

struct build_rule_interface_data_t
{
    struct build_state_t *state;
    struct resource_entry_t *entry;
};

static bool save_entry_data (struct build_state_t *state, struct resource_entry_t *entry, const void *entry_data)
{
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (entry->current_file_location, false);
    if (!stream)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to save \"%s\" of type \"%s\" as it wasn't possible to open file at path "
                 "\"%s\" for write.",
                 entry->target->name, entry->name, entry->type->name, entry->current_file_location);
        return false;
    }

    stream = kan_random_access_stream_buffer_open_for_read (stream, KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER);
    CUSHION_DEFER { stream->operations->close (stream); }

    kan_serialization_binary_writer_t writer =
        kan_serialization_binary_writer_create (stream, entry_data, entry->type->name, state->binary_script_storage,
                                                KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));
    CUSHION_DEFER { kan_serialization_binary_writer_destroy (writer); }

    enum kan_serialization_state_t serialization_state;
    while ((serialization_state = kan_serialization_binary_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    if (serialization_state == KAN_SERIALIZATION_FAILED)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to save \"%s\" of type \"%s\" due to serialization error.",
                 entry->target->name, entry->name, entry->type->name, entry->current_file_location);
        return false;
    }

    return true;
}

static kan_interned_string_t interface_produce_secondary_output (kan_resource_build_rule_interface_t interface,
                                                                 kan_interned_string_t type,
                                                                 kan_interned_string_t name,
                                                                 void *data)
{
    // TODO: We cannot just produce secondaries like that, because we have no way to prevent usage of old secondary
    //       RIGHT NOW IN BUILD RULE FUNCTOR while we're replacing that old secondary from here.
    //       Main way to prevent that is to use tasks for secondaries that will make secondaries dependant on primary
    //       producer builds. Which is cool from build hierarchy uniformity view.
    //       But then it breaks mergeables, as we need to be able to load mergeable right of the bat and ... with
    //       uniform build hierarchy we wouldn't be able to do so in some cases due to the need to confirm and execute
    //       base producer rule.
    //       I'm starting to think more and more that mergeables are a mistake. They had two goals:
    //       1. To reduce amount of graphics pipelines. However, if you really care about pipelines, then you would
    //          already ensure maximum uniformity and reusability of materials, which means that you would gain almost
    //          nothing from merging here. Merging would only help if situation is already pretty fucked up and we
    //          do not have that (and we should never fuck it up that hard).
    //       2. To reuse parsed third party data, for example to parse shader code file into intermediate AST and
    //          then reuse it for multiple shaders. I think it would be much more efficient to provide other solution
    //          to that. For example, we can say that if build rule has NULL instead of input type, it uses third party
    //          with the same name to produce parsed native resource.
    //       It would be good to sit and reevaluate all the mergeables stuff as they are pretty complex for the
    //       implementation and we must be sure that we actually have enough benefits from them.

    struct build_rule_interface_data_t *interface_data = KAN_HANDLE_GET (interface);
    struct build_state_t *state = interface_data->state;
    struct resource_entry_t *parent_entry = interface_data->entry;

    const struct kan_resource_reflected_data_resource_type_t *type_data =
        kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, type);

    KAN_ASSERT_FORMATTED (
        type_data, "Received secondary production of type \"%s\", but unable to query that resource type reflection.",
        type)

    struct resource_type_container_t *container =
        find_or_create_container_for_secondary (state, parent_entry->target, type_data->source_type);
    const bool mergeable = type_data->resource_type_meta->flags & KAN_RESOURCE_TYPE_MERGEABLE;

    while (true)
    {
        kan_instance_size_t mergeable_scan_map_version = RESOURCE_TYPE_CONTAINER_MERGEABLE_MAP_VERSION_NOT_INITIALIZED;
        if (mergeable)
        {
            KAN_ATOMIC_INT_SCOPED_LOCK_READ (&container->mergeable_map_lock)
            mergeable_scan_map_version = container->mergeable_map_version;

            // TODO: Search for merge target in current container. If found, confirm merge and return.
        }

        // Either not mergeable or there is no candidate for merging. Create new entry.
        struct resource_entry_t *entry;

        {
            KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&state->resource_entries_lock)
            if (mergeable)
            {
                kan_atomic_int_lock_write (&container->mergeable_map_lock);
                if (mergeable_scan_map_version != container->mergeable_map_version)
                {
                    // New insertions have happened, retry.
                    kan_atomic_int_unlock_write (&container->mergeable_map_lock);
                    continue;
                }
            }

            // TODO: What about name collisions? We also need to properly handle the reproduction case.

            entry = resource_entry_create (container, name);
            entry->class =
                mergeable ? RESOURCE_PRODUCTION_CLASS_SECONDARY_MERGEABLE : RESOURCE_PRODUCTION_CLASS_SECONDARY;

            // We do manual locking here intentionally, because we need to save new data, but we don't need to lock
            // entire entry addition context for the time of save operation. We can lock entry and save it after
            // global entry write lock is cleared.
            kan_atomic_int_lock_write (&entry->header.lock);
            kan_atomic_int_lock_write (&entry->build.lock);
            entry->header.status = RESOURCE_STATUS_AVAILABLE;

            if (mergeable)
            {
                if (container->mergeable_map_version == RESOURCE_TYPE_CONTAINER_MERGEABLE_MAP_VERSION_NOT_INITIALIZED)
                {
                    // TODO: Initialize map.
                }

                // TODO: Register entry with mergeable map.

                ++container->mergeable_map_version;
                kan_atomic_int_unlock_write (&container->mergeable_map_lock);
            }
            else
            {
            }
        }

        struct kan_file_system_path_container_t temporary_workspace;
        kan_file_system_path_container_copy_string (&temporary_workspace, state->setup->project->workspace_directory);
        kan_file_system_path_container_append (&temporary_workspace,
                                               KAN_RESOURCE_PROJECT_WORKSPACE_TEMPORARY_DIRECTORY);

        kan_file_system_path_container_append (&temporary_workspace, entry->target->name);
        kan_file_system_make_directory (temporary_workspace.path);

        kan_file_system_path_container_append (&temporary_workspace, entry->type->name);
        kan_file_system_make_directory (temporary_workspace.path);

        kan_file_system_path_container_append (&temporary_workspace, entry->name);
        kan_file_system_path_container_add_suffix (&temporary_workspace, ".bin");

        entry->current_file_location =
            kan_allocate_general (entry->allocation_group, temporary_workspace.length + 1u, alignof (char));
        memcpy (entry->current_file_location, temporary_workspace.path, temporary_workspace.length);
        entry->current_file_location[temporary_workspace.length] = '\0';

        const bool saved = save_entry_data (state, entry, data);
        struct kan_file_system_entry_status_t status;
        const bool proper_version = saved && kan_file_system_query_entry (entry->current_file_location, &status);

        if (!proper_version)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to properly save \"%s\" of type \"%s\" as it wasn't possible to query "
                     "file status at \"%s\" after the save operation.",
                     entry->target->name, entry->name, entry->type->name, entry->current_file_location);
        }

        entry->header.available_version.type_version = type_data->resource_type_meta->version;
        entry->header.available_version.last_modification_time = status.last_modification_time_ns;

        if (mergeable)
        {
            // TODO: Allocate new memory and move data there, save it to loaded data of build entry category.
        }
        else
        {
            // TODO: Just reset, because we saved data and do not need it anymore?
        }

        kan_atomic_int_unlock_write (&entry->header.lock);
        kan_atomic_int_unlock_write (&entry->build.lock);
        return saved && proper_version ? name : NULL;
    }

    // We should never exit that way.
    KAN_ASSERT (false)
    return NULL;
}

static void cleanup_build_rule_context (struct kan_resource_build_rule_context_t *build_context,
                                        struct resource_entry_t *entry,
                                        struct resource_entry_t *primary)
{
    {
        KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&primary->build.lock)
        --primary->build.loaded_data_request_count;

        if (primary->build.loaded_data_request_count == 0u &&
            // Secondary mergeables should be very small and are never unloaded for simplicity.
            primary->class != RESOURCE_PRODUCTION_CLASS_SECONDARY_MERGEABLE)
        {
            resource_entry_unload_data_unsafe (primary);
        }
    }

    for (kan_loop_size_t index = 0u; index < entry->new_build_secondary_inputs.size; ++index)
    {
        struct new_build_secondary_input_t *input =
            &((struct new_build_secondary_input_t *) entry->new_build_secondary_inputs.data)[index];

        if (input->entry)
        {
            KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&input->entry->build.lock)
            --input->entry->build.loaded_data_request_count;

            if (input->entry->build.loaded_data_request_count == 0u &&
                // Secondary mergeables should be very small and are never unloaded for simplicity.
                input->entry->class != RESOURCE_PRODUCTION_CLASS_SECONDARY_MERGEABLE)
            {
                resource_entry_unload_data_unsafe (input->entry);
            }
        }
    }

    struct kan_resource_build_rule_secondary_node_t *node_to_cleanup = build_context->secondary_input_first;
    while (node_to_cleanup)
    {
        struct kan_resource_build_rule_secondary_node_t *next = node_to_cleanup->next;
        kan_free_batched (temporary_allocation_group, node_to_cleanup);
        node_to_cleanup = next;
    }

    build_context->secondary_input_first = NULL;
}

static struct build_step_output_t execute_build_execute_build_rule (struct build_state_t *state,
                                                                    struct resource_entry_t *entry)
{
    struct build_step_output_t output = {
        .result = BUILD_STEP_RESULT_FAILED,
        .available_version = {.type_version = 0u, .last_modification_time = 0u},
        .loaded_data_to_manage = NULL,
    };

    struct build_rule_interface_data_t interface_data = {
        .state = state,
        .entry = entry,
    };

    struct kan_resource_build_rule_context_t build_context = {
        .primary_name = entry->name,
        .primary_input = NULL,
        .secondary_input_first = NULL,
        .primary_output = NULL,
        .platform_configuration = NULL,
        .temporary_workspace = NULL,
        .interface = KAN_HANDLE_SET (kan_resource_build_rule_interface_t, &interface_data),
        .produce_secondary_output = interface_produce_secondary_output,
    };

    struct resource_entry_t *primary = entry->build.internal_primary_input_entry;
    CUSHION_DEFER { cleanup_build_rule_context (&build_context, entry, primary); }

    {
        KAN_ATOMIC_INT_SCOPED_LOCK_READ (&primary->header.lock)
        KAN_ATOMIC_INT_SCOPED_LOCK_READ (&primary->build.lock)

        // Shouldn't be able to enter that function if it is not the case.
        KAN_ASSERT (primary->header.status == RESOURCE_STATUS_AVAILABLE)

        build_context.primary_input = primary->build.loaded_data;
        KAN_ASSERT (build_context.primary_input)
    }

    const struct kan_resource_reflected_data_resource_type_t *reflected_type =
        kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, entry->type->name);
    KAN_ASSERT (reflected_type->produced_from_build_rule)

    if (reflected_type->build_rule_platform_configuration_type)
    {
        struct platform_configuration_entry_t *configuration_entry =
            build_state_find_platform_configuration (state, reflected_type->build_rule_platform_configuration_type);

        if (!configuration_entry)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to build \"%s\" of type \"%s\" as platform configuration entry \"%s\" is "
                     "not found.",
                     entry->target->name, entry->name, entry->type->name,
                     reflected_type->build_rule_platform_configuration_type);
            return output;
        }

        build_context.platform_configuration = configuration_entry->data;
    }

    bool secondary_inputs_ready = true;
    for (kan_loop_size_t index = 0u; index < entry->new_build_secondary_inputs.size; ++index)
    {
        struct new_build_secondary_input_t *input =
            &((struct new_build_secondary_input_t *) entry->new_build_secondary_inputs.data)[index];

        if (input->entry)
        {
            KAN_ATOMIC_INT_SCOPED_LOCK_READ (&input->entry->header.lock)
            KAN_ATOMIC_INT_SCOPED_LOCK_READ (&input->entry->build.lock)

            switch (input->entry->header.status)
            {
            case RESOURCE_STATUS_UNCONFIRMED:
            case RESOURCE_STATUS_BUILDING:
                // Must never happen if requests work properly.
                KAN_ASSERT (false)
                break;

            case RESOURCE_STATUS_UNAVAILABLE:
                KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                         "[Target \"%s\"] Failed to build \"%s\" of type \"%s\" as build rule secondary input \"%s\" "
                         "of type \"%s\" is not available.",
                         entry->target->name, entry->name, entry->type->name, input->entry->name,
                         input->entry->type->name);
                secondary_inputs_ready = false;
                break;

            case RESOURCE_STATUS_AVAILABLE:
            {
                struct kan_resource_build_rule_secondary_node_t *node = kan_allocate_batched (
                    temporary_allocation_group, sizeof (struct kan_resource_build_rule_secondary_node_t));

                node->type = input->entry->type->name;
                node->name = input->entry->name;
                node->data = input->entry->build.loaded_data;

                node->next = build_context.secondary_input_first;
                build_context.secondary_input_first = node;
                break;
            }

            case RESOURCE_STATUS_PLATFORM_UNSUPPORTED:
                if (input->reference_flags & KAN_RESOURCE_REFERENCE_REQUIRED)
                {
                    KAN_LOG (
                        resource_pipeline_build, KAN_LOG_ERROR,
                        "[Target \"%s\"] Failed to build \"%s\" of type \"%s\" as build rule secondary input \"%s\" "
                        "of type \"%s\" is unsupported on this platform, but reference is configured as required.",
                        entry->target->name, entry->name, entry->type->name, input->entry->name,
                        input->entry->type->name);
                    secondary_inputs_ready = false;
                }

                break;
            }
        }
        else
        {
            struct kan_resource_build_rule_secondary_node_t *node = kan_allocate_batched (
                temporary_allocation_group, sizeof (struct kan_resource_build_rule_secondary_node_t));

            node->type = NULL;
            node->name = input->third_party_entry->name;
            node->third_party_path = input->third_party_entry->file_location;

            node->next = build_context.secondary_input_first;
            build_context.secondary_input_first = node;
        }
    }

    if (!secondary_inputs_ready)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to build \"%s\" of type \"%s\" as inputs are not available.",
                 entry->target->name, entry->name, entry->type->name);
        return output;
    }

    struct kan_file_system_path_container_t temporary_workspace;
    kan_file_system_path_container_copy_string (&temporary_workspace, state->setup->project->workspace_directory);
    kan_file_system_path_container_append (&temporary_workspace, KAN_RESOURCE_PROJECT_WORKSPACE_TEMPORARY_DIRECTORY);

    kan_file_system_path_container_append (&temporary_workspace, entry->target->name);
    kan_file_system_make_directory (temporary_workspace.path);

    kan_file_system_path_container_append (&temporary_workspace, entry->type->name);
    kan_file_system_make_directory (temporary_workspace.path);

    kan_file_system_path_container_append (&temporary_workspace, entry->name);
    if (!kan_file_system_make_directory (temporary_workspace.path))
    {
        KAN_LOG (
            resource_pipeline_build, KAN_LOG_ERROR,
            "[Target \"%s\"] Failed to build \"%s\" of type \"%s\" due to failure during temporary workspace creation.",
            entry->target->name, entry->name, entry->type->name);
        return output;
    }

    build_context.temporary_workspace = temporary_workspace.path;
    void *loaded_data = kan_allocate_general (entry->allocation_group, entry->type->size, entry->type->alignment);
    build_context.primary_output = loaded_data;

    if (entry->type->init)
    {
        kan_allocation_group_stack_push (entry->allocation_group);
        entry->type->init (entry->type->functor_user_data, loaded_data);
        kan_allocation_group_stack_pop ();
    }

    const enum kan_resource_build_rule_result_t result = reflected_type->build_rule_functor (&build_context);
    switch (result)
    {
    case KAN_RESOURCE_BUILD_RULE_SUCCESS:
        // Processed below.
        break;

    case KAN_RESOURCE_BUILD_RULE_FAILURE:
    case KAN_RESOURCE_BUILD_RULE_UNSUPPORTED:
        if (entry->type->shutdown)
        {
            entry->type->shutdown (entry->type->functor_user_data, output.loaded_data_to_manage);
        }

        kan_free_general (entry->allocation_group, loaded_data, entry->type->size);
        loaded_data = NULL;
        break;
    }

    switch (result)
    {
    case KAN_RESOURCE_BUILD_RULE_SUCCESS:
        // Processed below.
        break;

    case KAN_RESOURCE_BUILD_RULE_FAILURE:
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to build \"%s\" of type \"%s\" due to build rule failure.",
                 entry->target->name, entry->name, entry->type->name);
        return output;
    }

    case KAN_RESOURCE_BUILD_RULE_UNSUPPORTED:
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                 "[Target \"%s\"] Resource \"%s\" of type \"%s\" is marked as platform unsupported by build rule.",
                 entry->target->name, entry->name, entry->type->name);

        output.result = BUILD_STEP_RESULT_SUCCESSFUL;
        output.available_version.type_version = reflected_type->resource_type_meta->version;

        // As result is unsupported, we do not have a file for the timestamp, so we just use current time.
        output.available_version.last_modification_time = kan_precise_time_get_epoch_nanoseconds_utc ();

        // Unsupported, so now loaded data.
        output.loaded_data_to_manage = NULL;
        return output;
    }
    }

    // Should not be able to start build twice for one resource.
    KAN_ASSERT (entry->new_references.size == 0u)
    kan_resource_reflected_data_storage_detect_references (state->setup->reflected_data, entry->type->name, loaded_data,
                                                           &entry->new_references);

    // We no longer use temporary workspace path container, so may as well use it for saving.
    kan_file_system_path_container_append (&temporary_workspace, entry->name);
    kan_file_system_path_container_add_suffix (&temporary_workspace, ".bin");

    entry->current_file_location =
        kan_allocate_general (entry->allocation_group, temporary_workspace.length + 1u, alignof (char));
    memcpy (entry->current_file_location, temporary_workspace.path, temporary_workspace.length);
    entry->current_file_location[temporary_workspace.length] = '\0';

    CUSHION_DEFER
    {
        if (output.result == BUILD_STEP_RESULT_SUCCESSFUL)
        {
            output.loaded_data_to_manage = loaded_data;
        }
        else
        {
            if (entry->type->shutdown)
            {
                entry->type->shutdown (entry->type->functor_user_data, output.loaded_data_to_manage);
            }

            kan_free_general (entry->allocation_group, loaded_data, entry->type->size);
            loaded_data = NULL;
        }
    }

    if (!save_entry_data (state, entry, loaded_data))
    {
        return output;
    }

    struct kan_file_system_entry_status_t status;
    if (!kan_file_system_query_entry (entry->current_file_location, &status))
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to properly save \"%s\" of type \"%s\" as it wasn't possible to query file "
                 "status at \"%s\" after the save operation.",
                 entry->target->name, entry->name, entry->type->name, entry->current_file_location);
        return output;
    }

    output.result = BUILD_STEP_RESULT_SUCCESSFUL;
    output.available_version.type_version = reflected_type->resource_type_meta->version;
    output.available_version.last_modification_time = status.last_modification_time_ns;
    return output;
}

static struct build_step_output_t execute_build_step (struct build_state_t *state, struct resource_entry_t *entry)
{
    struct build_step_output_t output = {
        .result = BUILD_STEP_RESULT_FAILED,
        .available_version = {.type_version = 0u, .last_modification_time = 0u},
        .loaded_data_to_manage = NULL,
    };

    switch (entry->build.internal_next_build_task)
    {
    case RESOURCE_ENTRY_NEXT_BUILD_TASK_NONE:
        break;

    case RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_START:
        switch (entry->class)
        {
        case RESOURCE_PRODUCTION_CLASS_RAW:
            output = execute_build_start_raw (state, entry);
            break;

        case RESOURCE_PRODUCTION_CLASS_PRIMARY:
            output = execute_build_start_primary (state, entry);
            break;

        case RESOURCE_PRODUCTION_CLASS_SECONDARY:
        case RESOURCE_PRODUCTION_CLASS_SECONDARY_MERGEABLE:
            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Got build start task for resource \"%s\" of type \"%s\" which is secondary and "
                     "therefore cannot be built in a traditional way. Most likely an internal error.",
                     entry->target->name, entry->name, entry->type->name)
            break;
        }

        break;

    case RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_PROCESS_PRIMARY:
        output = execute_build_process_primary (state, entry);
        break;

    case RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_EXECUTE_BUILD_RULE:
        output = execute_build_execute_build_rule (state, entry);
        break;

    case RESOURCE_ENTRY_NEXT_BUILD_TASK_LOAD:
        if ((output.loaded_data_to_manage = load_resource_entry_data (state, entry)))
        {
            output.result = BUILD_STEP_RESULT_SUCCESSFUL;
            KAN_ATOMIC_INT_SCOPED_LOCK_READ (&entry->header.lock)
            output.available_version = entry->header.available_version;
        }
        else
        {
            output.result = BUILD_STEP_RESULT_FAILED;
        }

        break;
    }

    return output;
}

/// \details Has no inbuilt locking, must be externally synchronized (therefore _unsafe suffix).
static void add_to_build_queue_unsafe (struct build_state_t *state, struct resource_entry_t *entry)
{
    struct build_queue_item_t *item =
        kan_allocate_batched (build_queue_allocation_group, sizeof (struct build_queue_item_t));

    item->entry = entry;
    item->state = state;
    kan_bd_list_add (&state->build_queue, NULL, &item->node);
}

static void unblock_dependant_entries (struct build_state_t *state, struct resource_entry_t *entry)
{
    KAN_ATOMIC_INT_SCOPED_LOCK (&state->build_queue_lock)
    struct resource_entry_build_blocked_t *blocked = entry->build.blocked_other_first;

    while (blocked)
    {
        struct resource_entry_build_blocked_t *next = blocked->next;
        if (kan_atomic_int_add (&blocked->blocked_entry->build.atomic_next_build_task_block_counter, -1) == 1 &&
            blocked->blocked_entry->build.internal_paused_list_item)
        {
            // Last block, push to a build queue.
            add_to_build_queue_unsafe (state, blocked->blocked_entry);

            kan_bd_list_remove (&state->paused_list, &blocked->blocked_entry->build.internal_paused_list_item->node);
            kan_free_batched (build_queue_allocation_group, blocked->blocked_entry->build.internal_paused_list_item);
            blocked->blocked_entry->build.internal_paused_list_item = NULL;
        }

        kan_free_batched (entry->allocation_group, blocked);
        blocked = next;
    }
}

static void build_task (kan_functor_user_data_t user_data);

static void dispatch_new_tasks_from_queue_unsafe (struct build_state_t *state)
{
    while (state->currently_scheduled_build_operations < state->max_simultaneous_build_operations &&
           state->build_queue.first)
    {
        struct build_queue_item_t *item = (struct build_queue_item_t *) state->build_queue.first;
        kan_bd_list_remove (&state->build_queue, &item->node);
        ++state->currently_scheduled_build_operations;

        // There is no need to improve performance with task lists and precached sections here,
        // because total amount of tasks should not be that big.
        kan_cpu_task_dispatch ((struct kan_cpu_task_t) {
            .function = build_task,
            .user_data = (kan_functor_user_data_t) item,
            .profiler_section = kan_cpu_section_get (item->entry->name),
        });
    }
}

static void build_task (kan_functor_user_data_t user_data)
{
    struct build_queue_item_t *item = (struct build_queue_item_t *) user_data;
    enum resource_entry_next_build_task_t task_to_execute;
    task_to_execute = item->entry->build.internal_next_build_task;

    KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
             "[Target \"%s\"] Start build task \"%s\" execution for resource \"%s\" of type \"%s\".",
             item->entry->target->name, get_resource_entry_next_build_task_name (task_to_execute), item->entry->name,
             item->entry->type->name)

    // We should never ever try to build secondary mergeables, they have a separate routine.
    KAN_ASSERT (item->entry->class != RESOURCE_PRODUCTION_CLASS_SECONDARY_MERGEABLE)

    struct build_step_output_t output = execute_build_step (item->state, item->entry);
    switch (output.result)
    {
    case BUILD_STEP_RESULT_SUCCESSFUL:
    {
        bool should_propagate_deployment = false;
        bool should_propagate_cache = false;

        {
            KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&item->entry->header.lock)
            KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&item->entry->build.lock)

            item->entry->header.status =
                output.loaded_data_to_manage ? RESOURCE_STATUS_AVAILABLE : RESOURCE_STATUS_PLATFORM_UNSUPPORTED;
            item->entry->header.available_version = output.available_version;

            if (item->entry->build.loaded_data_request_count > 0u)
            {
                item->entry->build.loaded_data = output.loaded_data_to_manage;
            }
            else if (output.loaded_data_to_manage)
            {
                if (item->entry->type->shutdown)
                {
                    item->entry->type->shutdown (item->entry->type->functor_user_data, output.loaded_data_to_manage);
                }

                kan_free_general (item->entry->allocation_group, output.loaded_data_to_manage, item->entry->type->size);
            }

            unblock_dependant_entries (item->state, item->entry);
            should_propagate_deployment = item->entry->header.deployment_mark;
            should_propagate_cache = item->entry->header.cache_mark;
        }

        if (!is_resource_entry_next_build_task_repeated (task_to_execute))
        {
            if (should_propagate_deployment)
            {
                mark_resource_references_for_deployment (item->state, item->entry, NULL);
            }

            if (should_propagate_cache)
            {
                mark_resource_build_dependencies_for_cache (item->state, item->entry, NULL);
            }
        }

        KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                 "[Target \"%s\"] Finished build task \"%s\" execution for resource \"%s\" of type \"%s\" with "
                 "successful build exit.",
                 item->entry->target->name, get_resource_entry_next_build_task_name (task_to_execute),
                 item->entry->name, item->entry->type->name)
        break;
    }

    case BUILD_STEP_RESULT_FAILED:
    {
        KAN_ASSERT (!output.result)
        KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&item->entry->header.lock)
        KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&item->entry->build.lock)

        item->entry->header.status = RESOURCE_STATUS_UNAVAILABLE;
        struct build_info_list_item_t *info =
            kan_allocate_batched (build_queue_allocation_group, sizeof (struct build_info_list_item_t));

        info->entry = item->entry;
        kan_bd_list_add (&item->state->failed_list, NULL, &info->node);
        unblock_dependant_entries (item->state, item->entry);

        KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                 "[Target \"%s\"] Finished build task \"%s\" execution for resource \"%s\" of type \"%s\" with "
                 "failed build exit.",
                 item->entry->target->name, get_resource_entry_next_build_task_name (task_to_execute),
                 item->entry->name, item->entry->type->name)

        break;
    }

    case BUILD_STEP_RESULT_PAUSED:
    {
        KAN_ASSERT (!output.result)
        // Externally visible state should not be changed,
        // therefore we do not need to lock anything other than build queue.

        KAN_ATOMIC_INT_SCOPED_LOCK (&item->state->build_queue_lock)
        if (kan_atomic_int_get (&item->entry->build.atomic_next_build_task_block_counter) > 0)
        {
            struct build_info_list_item_t *info =
                kan_allocate_batched (build_queue_allocation_group, sizeof (struct build_info_list_item_t));

            info->entry = item->entry;
            kan_bd_list_add (&item->state->paused_list, NULL, &info->node);
            item->entry->build.internal_paused_list_item = info;
        }
        else
        {
            // Already got unblocked, recycle it into build queue again.
            add_to_build_queue_unsafe (item->state, item->entry);
        }

        break;
    }
    }

    {
        KAN_ATOMIC_INT_SCOPED_LOCK (&item->state->build_queue_lock)
        --item->state->currently_scheduled_build_operations;
        dispatch_new_tasks_from_queue_unsafe (item->state);
    }

    kan_free_batched (build_queue_allocation_group, item);
}

// Build step implementation section.

static enum kan_resource_build_result_t execute_build (struct build_state_t *state)
{
    // TODO: Clear temp directory.
    // TODO: For all root resources everywhere, mark them as deployed and it will trigger all the builds recursively.
    // TODO: Wait until all the builds are complete. Can just sleep and check under the lock.
    // TODO: Execute deployment/caching routines.
    //       It seems that on Windows move changes modification time. Need to update it in versions.
    // TODO: Clear temp directory again just in case.
    // TODO: Create updated log and save it.
    // TODO: Return success only if everything succeeded.
    return KAN_RESOURCE_BUILD_RESULT_SUCCESS;
}

// Build procedure main implementation section.

enum kan_resource_build_result_t kan_resource_build (struct kan_resource_build_setup_t *setup)
{
    ensure_statics_initialized ();
    enum kan_resource_build_result_t result = KAN_RESOURCE_BUILD_RESULT_SUCCESS;
    kan_log_category_set_verbosity (kan_log_category_get ("resource_pipeline_build"), setup->log_verbosity);

    struct build_state_t state;
    build_state_init (&state, setup);
    CUSHION_DEFER { build_state_shutdown (&state); }

#define CHECKED_STEP(NAME, ...)                                                                                        \
    {                                                                                                                  \
        kan_time_size_t start = kan_precise_time_get_elapsed_nanoseconds ();                                           \
        CUSHION_DEFER                                                                                                  \
        {                                                                                                              \
            kan_time_size_t end = kan_precise_time_get_elapsed_nanoseconds ();                                         \
            KAN_LOG (resource_pipeline_build, KAN_LOG_INFO, "Step \"%s\" done in %.3f ms.", #NAME,                     \
                     1e-6f * (float) (end - start));                                                                   \
        }                                                                                                              \
                                                                                                                       \
        result = NAME (&state __VA_OPT__ (, ) __VA_ARGS__);                                                            \
        if (result != KAN_RESOURCE_BUILD_RESULT_SUCCESS)                                                               \
        {                                                                                                              \
            return result;                                                                                             \
        }                                                                                                              \
    }

    CHECKED_STEP (create_targets, )
    CHECKED_STEP (link_visible_targets, )
    CHECKED_STEP (linearize_visible_targets, )
    CHECKED_STEP (load_platform_configuration, )
    CHECKED_STEP (load_resource_log_if_exists, )
    CHECKED_STEP (instantiate_initial_resource_log, )
    // TODO: Scan for changes in raw resources.
    CHECKED_STEP (execute_build, )
    // TODO: Pack step if needed.

#undef CHECKED_STEP
    return result;
}
