#include <qsort.h>

#include <kan/cpu_dispatch/job.h>
#include <kan/error/critical.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/path_container.h>
#include <kan/file_system/stream.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/platform/hardware.h>
#include <kan/precise_time/precise_time.h>
#include <kan/reflection/struct_helpers.h>
#include <kan/resource_pipeline/build.h>
#include <kan/resource_pipeline/index.h>
#include <kan/resource_pipeline/log.h>
#include <kan/resource_pipeline/platform_configuration.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>
#include <kan/threading/atomic.h>
#include <kan/virtual_file_system/virtual_file_system.h>

static kan_resource_version_t resource_build_version = CUSHION_START_NS_X64;
KAN_LOG_DEFINE_CATEGORY (resource_pipeline_build);

static kan_allocation_group_t main_allocation_group;
static kan_allocation_group_t platform_configuration_allocation_group;
static kan_allocation_group_t targets_allocation_group;
static kan_allocation_group_t build_queue_allocation_group;
static kan_allocation_group_t temporary_allocation_group;

KAN_USE_STATIC_INTERNED_IDS
KAN_USE_STATIC_CPU_SECTIONS
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
        kan_cpu_static_sections_ensure_initialized ();
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

    /// \brief Resource belongs to target that is not being built this run.
    /// \invariant Must be applied during log instantiation, not during build! Must also set available version.
    RESOURCE_STATUS_OUT_OF_SCOPE,
};

enum deployment_step_target_location_t
{
    DEPLOYMENT_STEP_TARGET_LOCATION_DEPLOY = 0u,
    DEPLOYMENT_STEP_TARGET_LOCATION_CACHE,
    DEPLOYMENT_STEP_TARGET_LOCATION_NONE,
};

/// \brief Header contains data that is changed during build,
///        but it not directly connected to build operation execution.
struct resource_entry_header_t
{
    /// \brief Lock for restricting access to the header through atomic read-write lock.
    struct kan_atomic_int_t lock;

    enum resource_status_t status;

    /// \brief Resource version if status is `RESOURCE_STATUS_AVAILABLE`, `RESOURCE_STATUS_PLATFORM_UNSUPPORTED` or
    ///        `RESOURCE_STATUS_OUT_OF_SCOPE`.
    struct kan_resource_log_version_t available_version;

    bool deployment_mark;
    bool cache_mark;

    /// \brief True if resource build was executed.
    /// \details We cannot just compare versions as versions contain timestamp and some OSes like Windows can change
    ///          timestamp when file is just moved around, making it possible for version to change outside of build
    ///          routine execution.
    bool passed_build_routine_mark;
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

    union
    {
        /// \brief Entry that is used as primary input if build rule is executed and it is not an import-rule.
        /// \details Managed internally, therefore not locked under `lock`.
        struct resource_entry_t *internal_primary_input_entry;

        /// \brief Third party that is used as primary input if build rule is executed and it is an import-rule.
        /// \details Managed internally, therefore not locked under `lock`.
        struct raw_third_party_entry_t *internal_primary_input_third_party;

        /// \brief Entry that has produced this entry as secondary output.
        /// \details Managed internally, therefore not locked under `lock`.
        struct resource_entry_t *internal_producer_entry;
    };

    /// \details When secondary reproduction has happened, we need to preserve resulted secondary until there is time
    ///          to properly execute secondary build task to properly acknowledge these results.
    ///          Managed internally, therefore not locked under `lock`.
    void *internal_transient_secondary_output;
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

struct resource_type_container_t
{
    struct kan_hash_storage_node_t node;
    struct target_t *target;
    const struct kan_reflection_struct_t *type;
    struct kan_hash_storage_t entries;
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

    bool marked_for_build;
    const struct kan_resource_log_target_t *initial;

    /// \brief We do no change resource project, so we can just point to it.
    const struct kan_resource_project_target_t *source;

    /// \brief State pointer makes it possible to send target as cpu task user data without additional context.
    struct build_state_t *state;

    bool raw_resource_scan_step_successful;
    bool deployment_step_successful;
    bool pack_step_successful;
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
    instance->header.passed_build_routine_mark = false;

    instance->build.lock = kan_atomic_int_init (0);
    instance->build.loaded_data = NULL;
    instance->build.loaded_data_request_count = 0u;
    instance->build.blocked_other_first = NULL;
    instance->build.internal_next_build_task = RESOURCE_ENTRY_NEXT_BUILD_TASK_NONE;
    instance->build.atomic_next_build_task_block_counter = kan_atomic_int_init (0);
    instance->build.internal_paused_list_item = NULL;
    instance->build.internal_primary_input_entry = NULL;
    instance->build.internal_transient_secondary_output = NULL;

    instance->class = RESOURCE_PRODUCTION_CLASS_RAW;
    instance->current_file_location = NULL;
    instance->initial_log_raw_entry = NULL;

    kan_dynamic_array_init (&instance->new_build_secondary_inputs, 0u, sizeof (struct new_build_secondary_input_t),
                            alignof (struct new_build_secondary_input_t), group);
    kan_dynamic_array_init (&instance->new_references, 0u, sizeof (struct kan_resource_log_reference_t),
                            alignof (struct kan_resource_log_reference_t), group);
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

static void resource_entry_unload_secondary_transient_data (struct resource_entry_t *instance)
{
    if (instance->build.internal_transient_secondary_output)
    {
        if (instance->type->shutdown)
        {
            instance->type->shutdown (instance->type->functor_user_data,
                                      instance->build.internal_transient_secondary_output);
        }

        kan_free_general (instance->allocation_group, instance->build.internal_transient_secondary_output,
                          instance->type->size);
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
    resource_entry_unload_secondary_transient_data (instance);
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
    struct target_t *owner, kan_interned_string_t name, const struct kan_file_system_path_container_t *path)
{
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
    instance->target = owner;
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

    instance->marked_for_build = false;
    instance->initial = NULL;
    instance->source = source;

    instance->state = NULL;
    instance->raw_resource_scan_step_successful = true;
    instance->deployment_step_successful = true;
    instance->pack_step_successful = true;
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

static struct raw_third_party_entry_t *target_search_local_third_party (struct target_t *target,
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

static struct raw_third_party_entry_t *target_search_visible_third_party (struct target_t *from_target,
                                                                          kan_interned_string_t name)
{
    struct raw_third_party_entry_t *found_entry = NULL;
    if ((found_entry = target_search_local_third_party (from_target, name)))
    {
        return found_entry;
    }

    for (kan_loop_size_t index = 0u; index < from_target->visible_targets.size; ++index)
    {
        struct target_t *visible = ((struct target_t **) from_target->visible_targets.data)[index];
        if ((found_entry = target_search_local_third_party (visible, name)))
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
    instance->pack_mode = KAN_RESOURCE_BUILD_PACK_MODE_NONE;
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
        target->state = state;

        for (kan_loop_size_t selection_index = 0u; selection_index < state->setup->targets.size; ++selection_index)
        {
            if (target->name == ((kan_interned_string_t *) state->setup->targets.data)[selection_index])
            {
                target->marked_for_build = true;
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "Marking target \"%s\" for build as it is specified in initial setup.", target->name)
                break;
            }
        }

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

    target = state->targets_first;
    while (target)
    {
        if (target->marked_for_build)
        {
            for (kan_loop_size_t main_index = 0u; main_index < target->visible_targets.size; ++main_index)
            {
                struct target_t *visible = ((struct target_t **) target->visible_targets.data)[main_index];
                if (!visible->marked_for_build)
                {
                    visible->marked_for_build = true;
                    KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                             "Marking target \"%s\" for build as it is visible from target \"%s\" which is already "
                             "marked for build.",
                             visible->name, target->name)
                }
            }
        }

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
    CUSHION_DEFER { kan_file_system_directory_iterator_destroy (iterator); }
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
                // Otherwise entry shutdown will destroy owned patch.
                entry.data = KAN_HANDLE_SET_INVALID (kan_reflection_patch_t);
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

// Resource log instantiation step section.

static void instantiate_log_target (struct build_state_t *state,
                                    struct target_t *target,
                                    const struct kan_resource_log_target_t *log_target)
{
    target->initial = log_target;
    struct kan_file_system_path_container_t path;
    kan_file_system_path_container_copy_string (&path, state->setup->project->workspace_directory);

    for (kan_loop_size_t index = 0u; index < log_target->raw.size; ++index)
    {
        const struct kan_resource_log_raw_entry_t *log_entry =
            &((struct kan_resource_log_raw_entry_t *) log_target->raw.data)[index];

        const struct kan_resource_reflected_data_resource_type_t *reflected_type =
            kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, log_entry->type);

        if (!reflected_type)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Skipping logged resource \"%s\" of type \"%s\" as type is no longer found.",
                     target->name, log_entry->name, log_entry->type);
            continue;
        }

        struct resource_type_container_t *container =
            target_search_resource_type_container_unsafe (target, log_entry->type);

        if (!container)
        {
            container = resource_type_container_create (target, reflected_type->struct_type);
        }

        struct resource_entry_t *entry = resource_entry_create (container, log_entry->name);
        entry->class = RESOURCE_PRODUCTION_CLASS_RAW;
        entry->initial_log_raw_entry = log_entry;

        if (!target->marked_for_build)
        {
            entry->header.status = RESOURCE_STATUS_OUT_OF_SCOPE;
            entry->header.available_version = entry->initial_log_raw_entry->version;
        }
    }

    for (kan_loop_size_t index = 0u; index < log_target->built.size; ++index)
    {
        const struct kan_resource_log_built_entry_t *log_entry =
            &((struct kan_resource_log_built_entry_t *) log_target->built.data)[index];

        const struct kan_resource_reflected_data_resource_type_t *reflected_type =
            kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, log_entry->type);

        if (!reflected_type)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Skipping logged resource \"%s\" of type \"%s\" as type is no longer found.",
                     target->name, log_entry->name, log_entry->type);
            continue;
        }

        struct resource_type_container_t *container =
            target_search_resource_type_container_unsafe (target, log_entry->type);

        if (!container)
        {
            container = resource_type_container_create (target, reflected_type->struct_type);
        }

        struct resource_entry_t *entry = resource_entry_create (container, log_entry->name);
        entry->class = RESOURCE_PRODUCTION_CLASS_PRIMARY;
        entry->initial_log_built_entry = log_entry;

        if (log_entry->saved_directory != KAN_RESOURCE_LOG_SAVED_DIRECTORY_UNSUPPORTED)
        {
            const kan_instance_size_t base_length = path.length;
            switch (log_entry->saved_directory)
            {
            case KAN_RESOURCE_LOG_SAVED_DIRECTORY_DEPLOY:
                kan_resource_build_append_deploy_path_in_workspace (&path, entry->target->name, entry->type->name,
                                                                    entry->name);
                break;

            case KAN_RESOURCE_LOG_SAVED_DIRECTORY_CACHE:
                kan_resource_build_append_cache_path_in_workspace (&path, entry->target->name, entry->type->name,
                                                                   entry->name);
                break;

            case KAN_RESOURCE_LOG_SAVED_DIRECTORY_UNSUPPORTED:
                break;
            }

            KAN_ASSERT (!entry->current_file_location)
            entry->current_file_location =
                kan_allocate_general (entry->allocation_group, path.length + 1u, alignof (char));
            memcpy (entry->current_file_location, path.path, path.length);
            entry->current_file_location[path.length] = '\0';
            kan_file_system_path_container_reset_length (&path, base_length);
        }

        if (!target->marked_for_build)
        {
            entry->header.status = RESOURCE_STATUS_OUT_OF_SCOPE;
            entry->header.available_version = entry->initial_log_raw_entry->version;
        }
    }

    for (kan_loop_size_t index = 0u; index < log_target->secondary.size; ++index)
    {
        const struct kan_resource_log_secondary_entry_t *log_entry =
            &((struct kan_resource_log_secondary_entry_t *) log_target->secondary.data)[index];

        const struct kan_resource_reflected_data_resource_type_t *reflected_type =
            kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, log_entry->type);

        if (!reflected_type)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Skipping logged resource \"%s\" of type \"%s\" as type is no longer found.",
                     target->name, log_entry->name, log_entry->type);
            continue;
        }

        struct resource_type_container_t *container =
            target_search_resource_type_container_unsafe (target, log_entry->type);

        if (!container)
        {
            container = resource_type_container_create (target, reflected_type->struct_type);
        }

        struct resource_entry_t *entry = resource_entry_create (container, log_entry->name);
        entry->class = RESOURCE_PRODUCTION_CLASS_SECONDARY;
        entry->initial_log_secondary_entry = log_entry;

        if (log_entry->saved_directory != KAN_RESOURCE_LOG_SAVED_DIRECTORY_UNSUPPORTED)
        {
            const kan_instance_size_t base_length = path.length;
            switch (log_entry->saved_directory)
            {
            case KAN_RESOURCE_LOG_SAVED_DIRECTORY_DEPLOY:
                kan_resource_build_append_deploy_path_in_workspace (&path, entry->target->name, entry->type->name,
                                                                    entry->name);
                break;

            case KAN_RESOURCE_LOG_SAVED_DIRECTORY_CACHE:
                kan_resource_build_append_cache_path_in_workspace (&path, entry->target->name, entry->type->name,
                                                                   entry->name);
                break;

            case KAN_RESOURCE_LOG_SAVED_DIRECTORY_UNSUPPORTED:
                break;
            }

            KAN_ASSERT (!entry->current_file_location)
            entry->current_file_location =
                kan_allocate_general (entry->allocation_group, path.length + 1u, alignof (char));

            memcpy (entry->current_file_location, path.path, path.length);
            entry->current_file_location[path.length] = '\0';
            kan_file_system_path_container_reset_length (&path, base_length);
        }

        if (!target->marked_for_build)
        {
            entry->header.status = RESOURCE_STATUS_OUT_OF_SCOPE;
            entry->header.available_version = entry->initial_log_raw_entry->version;
        }
    }
}

static enum kan_resource_build_result_t instantiate_initial_resource_log (struct build_state_t *state)
{
    for (kan_loop_size_t index = 0u; index < state->initial_log.targets.size; ++index)
    {
        const struct kan_resource_log_target_t *log_target =
            &((struct kan_resource_log_target_t *) state->initial_log.targets.data)[index];
        struct target_t *target = state->targets_first;

        while (target)
        {
            if (target->name == log_target->name)
            {
                instantiate_log_target (state, target, log_target);
                break;
            }

            target = target->next;
        }

        if (!target)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_INFO,
                     "Skipped target \"%s\" entry in log as there is no such target in project.", log_target->name);
        }
    }

    return KAN_RESOURCE_BUILD_RESULT_SUCCESS;
}

// Raw resource scanning step section.

static bool scan_file (struct target_t *target, struct kan_file_system_path_container_t *reused_path)
{
    kan_interned_string_t native_type = NULL;
    const char *native_name_end = NULL;

    if (reused_path->length >= 4u && reused_path->path[reused_path->length - 4u] == '.' &&
        reused_path->path[reused_path->length - 3u] == 'b' && reused_path->path[reused_path->length - 2u] == 'i' &&
        reused_path->path[reused_path->length - 1u] == 'n')
    {
        native_name_end = reused_path->path + reused_path->length - 4u;
        struct kan_stream_t *stream = kan_direct_file_stream_open_for_read (reused_path->path, true);

        if (!stream)
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                                 "Unable to open read stream at \"%s\" in order to retrieve entry type while scanning "
                                 "target \"%s\" directories.",
                                 reused_path->path, target->name)
            return false;
        }

        stream = kan_random_access_stream_buffer_open_for_read (stream, KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER);
        CUSHION_DEFER { stream->operations->close (stream); }

        if (!kan_serialization_binary_read_type_header (
                stream, &native_type, KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t)))
        {
            KAN_LOG_WITH_BUFFER (
                KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                "Failed to deserialize type information from \"%s\" while scanning target \"%s\" directories.",
                reused_path->path, target->name)
            return false;
        }
    }
    else if (reused_path->length >= 3u && reused_path->path[reused_path->length - 3u] == '.' &&
             reused_path->path[reused_path->length - 2u] == 'r' && reused_path->path[reused_path->length - 1u] == 'd')
    {
        native_name_end = reused_path->path + reused_path->length - 3u;
        struct kan_stream_t *stream = kan_direct_file_stream_open_for_read (reused_path->path, false);

        if (!stream)
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                                 "Unable to open read stream at \"%s\" in order to retrieve entry type while scanning "
                                 "target \"%s\" directories.",
                                 reused_path->path, target->name)
            return false;
        }

        stream = kan_random_access_stream_buffer_open_for_read (stream, KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER);
        CUSHION_DEFER { stream->operations->close (stream); }

        if (!kan_serialization_rd_read_type_header (stream, &native_type))
        {
            KAN_LOG_WITH_BUFFER (
                KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                "Failed to deserialize type information from \"%s\" while scanning target \"%s\" directories.",
                reused_path->path, target->name)
            return false;
        }
    }
    else
    {
        const char *name_begin = reused_path->path + reused_path->length;
        while (name_begin > reused_path->path && *(name_begin - 1u) != '/' && *(name_begin - 1u) != '\\')
        {
            --name_begin;
        }

        if (!name_begin)
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                                 "Unable to get resource name for third party entry from path \"%s\" while scanning "
                                 "target \"%s\" directories.",
                                 reused_path->path, target->name)
            return false;
        }

        const kan_interned_string_t name = kan_string_intern (name_begin);
        struct raw_third_party_entry_t *entry = target_search_local_third_party (target, name);

        if (entry)
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 3u, resource_pipeline_build, KAN_LOG_ERROR,
                                 "Found third party entry with name \"%s\" at \"%s\" in target \"%s\", while entry "
                                 "with the same name already exists here at \"%s\"",
                                 name, reused_path->path, target->name, entry->file_location)
            return false;
        }

        entry = raw_third_party_entry_create (target, name, reused_path);
        if (!entry)
        {
            return false;
        }

        return true;
    }

    const char *native_name_begin = native_name_end;
    while (native_name_begin > reused_path->path && *(native_name_begin - 1u) != '/' &&
           *(native_name_begin - 1u) != '\\')
    {
        --native_name_begin;
    }

    if (native_name_begin == native_name_end)
    {
        KAN_LOG_WITH_BUFFER (
            KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
            "Unable to get resource name for native entry from path \"%s\" while scanning target \"%s\" directories.",
            reused_path->path, target->name)
        return false;
    }

    const kan_interned_string_t native_name = kan_char_sequence_intern (native_name_begin, native_name_end);
    // Can safely use unsafe here as target is scanned as a whole and targets do not access each other during scan.
    struct resource_entry_t *entry = target_search_local_resource_unsafe (target, native_type, native_name);

    if (entry)
    {
        switch (entry->class)
        {
        case RESOURCE_PRODUCTION_CLASS_RAW:
            if (!entry->current_file_location)
            {
                // Confirmed existence of raw resource, just return true.
                entry->current_file_location =
                    kan_allocate_general (entry->allocation_group, reused_path->length + 1u, alignof (char));

                memcpy (entry->current_file_location, reused_path->path, reused_path->length);
                entry->current_file_location[reused_path->length] = '\0';
                return true;
            }

            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 3u, resource_pipeline_build, KAN_LOG_ERROR,
                                 "Found resource \"%s\" of type \"%s\" at \"%s\" in target \"%s\", while entry "
                                 "with the same name is already found at \"%s\".",
                                 native_name, native_type, target->name, entry->current_file_location)
            return false;

        case RESOURCE_PRODUCTION_CLASS_PRIMARY:
        case RESOURCE_PRODUCTION_CLASS_SECONDARY:
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "Found resource \"%s\" of type \"%s\" at \"%s\" in target \"%s\", while entry "
                     "with the same name is already found in this target.",
                     native_name, native_type, target->name)
            return false;
        }

        KAN_ASSERT (false)
        return false;
    }

    const struct kan_resource_reflected_data_resource_type_t *reflected_type =
        kan_resource_reflected_data_storage_query_resource_type (target->state->setup->reflected_data, native_type);

    if (!reflected_type)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                             "Found resource \"%s\" of type \"%s\" at \"%s\" in target \"%s\" at \"%s\", but there is "
                             "no such resource type!",
                             native_name, native_type, target->name, reused_path->path)
        return false;
    }

    struct resource_type_container_t *container = target_search_resource_type_container_unsafe (target, native_type);
    if (!container)
    {
        container = resource_type_container_create (target, reflected_type->struct_type);
    }

    entry = resource_entry_create (container, native_name);
    entry->class = RESOURCE_PRODUCTION_CLASS_RAW;

    KAN_ASSERT (!entry->current_file_location)
    entry->current_file_location =
        kan_allocate_general (entry->allocation_group, reused_path->length + 1u, alignof (char));

    memcpy (entry->current_file_location, reused_path->path, reused_path->length);
    entry->current_file_location[reused_path->length] = '\0';
    return true;
}

static bool scan_directory (struct target_t *target, struct kan_file_system_path_container_t *reused_path)
{
    kan_file_system_directory_iterator_t iterator = kan_file_system_directory_iterator_create (reused_path->path);
    CUSHION_DEFER { kan_file_system_directory_iterator_destroy (iterator); }
    bool successful = true;
    const char *item_name;

    while ((item_name = kan_file_system_directory_iterator_advance (iterator)))
    {
        if ((item_name[0u] == '.' && item_name[1u] == '\0') ||
            (item_name[0u] == '.' && item_name[1u] == '.' && item_name[2u] == '\0'))
        {
            // Skip special entries.
            continue;
        }

        const kan_instance_size_t base_length = reused_path->length;
        CUSHION_DEFER { kan_file_system_path_container_reset_length (reused_path, base_length); }
        kan_file_system_path_container_append (reused_path, item_name);
        struct kan_file_system_entry_status_t status;

        if (kan_file_system_query_entry (reused_path->path, &status))
        {
            switch (status.type)
            {
            case KAN_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN:
                KAN_LOG_WITH_BUFFER (
                    KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                    "Encountered file entry \"%s\" with unknown type while scanning target \"%s\" directories.",
                    reused_path->path, target->name)
                successful = false;
                break;

            case KAN_FILE_SYSTEM_ENTRY_TYPE_FILE:
                scan_file (target, reused_path);
                break;

            case KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY:
                successful &= scan_directory (target, reused_path);
                break;
            }
        }
        else
        {
            KAN_LOG_WITH_BUFFER (
                KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                "Failed to query status of file entry \"%s\" while scanning target \"%s\" directories.",
                reused_path->path, target->name)
            successful = false;
        }
    }

    return successful;
}

static void execute_raw_resource_scan_for_target (kan_functor_user_data_t user_data)
{
    struct target_t *target = (struct target_t *) user_data;
    target->raw_resource_scan_step_successful = true;
    struct kan_file_system_path_container_t reused_path;

    for (kan_loop_size_t directory_index = 0u; directory_index < target->source->directories.size; ++directory_index)
    {
        const char *directory_path = ((char **) target->source->directories.data)[directory_index];
        kan_file_system_path_container_copy_string (&reused_path, directory_path);
        target->raw_resource_scan_step_successful &= scan_directory (target, &reused_path);
    }
}

static enum kan_resource_build_result_t scan_for_raw_resources (struct build_state_t *state)
{
    struct target_t *target = state->targets_first;
    kan_cpu_job_t job = kan_cpu_job_create ();

    while (target)
    {
        CUSHION_DEFER { target = target->next; }
        if (!target->marked_for_build)
        {
            continue;
        }

        // There is not that many targets, so we can just post tasks one by one instead of using task list.
        kan_cpu_job_dispatch_task (job, (struct kan_cpu_task_t) {
                                            .function = execute_raw_resource_scan_for_target,
                                            .user_data = (kan_functor_user_data_t) target,
                                            .profiler_section = kan_cpu_section_get (target->name),
                                        });
    }

    kan_cpu_job_release (job);
    kan_cpu_job_wait (job);

    bool successful = true;
    target = state->targets_first;

    while (target)
    {
        successful &= target->raw_resource_scan_step_successful;
        target = target->next;
    }

    return successful ? KAN_RESOURCE_BUILD_RESULT_SUCCESS : KAN_RESOURCE_BUILD_RESULT_ERROR_RAW_RESOURCE_SCAN_FAILED;
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

static void add_to_build_queue_new_unsafe (struct build_state_t *state, struct resource_entry_t *entry);

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
                     target->name, entry->name, entry->type->name)

            new_status = RESOURCE_STATUS_UNAVAILABLE;
            break;
        }

        struct kan_file_system_entry_status_t file_status;
        if (!kan_file_system_query_entry (entry->current_file_location, &file_status))
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Marking raw resource \"%s\" of type \"%s\" as unavailable as it wasn't "
                     "possible to query its file status.",
                     target->name, entry->name, entry->type->name)

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
                     target->name, entry->name, entry->type->name)
            new_status = RESOURCE_STATUS_BUILDING;
            break;
        }

        KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                 "[Target \"%s\"] Marking raw resource \"%s\" of type \"%s\" as up to date in current build.",
                 target->name, entry->name, entry->type->name)
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
                     target->name, entry->name, entry->type->name)
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
            target->name, entry->type->name, entry->type->name)

        if (initial->rule_version != reflected_type->build_rule_version)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because of build "
                     "rule version mismatch.",
                     target->name, entry->name, entry->type->name)
            new_status = RESOURCE_STATUS_BUILDING;
            break;
        }

        if (initial->version.type_version != reflected_type->resource_type_meta->version)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because of resource "
                     "type version mismatch.",
                     target->name, entry->name, entry->type->name)
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
                         target->name, entry->name, entry->type->name)
                new_status = RESOURCE_STATUS_UNAVAILABLE;
                break;
            }

            if (platform_configuration->file_time != initial->platform_configuration_time)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because of "
                         "platform configuration time mismatch.",
                         target->name, entry->name, entry->type->name)
                new_status = RESOURCE_STATUS_BUILDING;
                break;
            }
        }

        if (reflected_type->build_rule_primary_input_type)
        {
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
                    KAN_LOG (
                        resource_pipeline_build, KAN_LOG_DEBUG,
                        "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as unavailable because primary "
                        "input is unavailable too.",
                        target->name, entry->name, entry->type->name)
                    new_status = RESOURCE_STATUS_UNAVAILABLE;
                    break;

                case RESOURCE_STATUS_BUILDING:
                    KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                             "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because its "
                             "primary input already has building status.",
                             target->name, entry->name, entry->type->name)
                    new_status = RESOURCE_STATUS_BUILDING;
                    break;

                case RESOURCE_STATUS_AVAILABLE:
                    if (!kan_resource_log_version_is_up_to_date (
                            initial->primary_input_version, primary_input_response.entry->header.available_version))
                    {
                        KAN_LOG (
                            resource_pipeline_build, KAN_LOG_DEBUG,
                            "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because of "
                            "version mismatch with its primary input.",
                            target->name, entry->name, entry->type->name)
                        new_status = RESOURCE_STATUS_BUILDING;
                        break;
                    }

                    break;

                case RESOURCE_STATUS_PLATFORM_UNSUPPORTED:
                    KAN_LOG (
                        resource_pipeline_build, KAN_LOG_DEBUG,
                        "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as platform unsupported because "
                        "primary input is platform unsupported too.",
                        target->name, entry->name, entry->type->name)
                    new_status = RESOURCE_STATUS_PLATFORM_UNSUPPORTED;
                    break;

                case RESOURCE_STATUS_OUT_OF_SCOPE:
                    KAN_ASSERT_FORMATTED (false,
                                          "Internal error, primary input entry has out of scope status, which "
                                          "shouldn't be possible due to target visibility rules.", )
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
                         target->name, entry->name, entry->type->name)
                new_status = RESOURCE_STATUS_UNAVAILABLE;
                break;
            }
        }
        else
        {
            // Import build rule, custom handling.
            struct raw_third_party_entry_t *third_party = target_search_visible_third_party (target, entry->name);

            if (!third_party)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because its "
                         "primary input which is raw third party file cannot be found.",
                         target->name, entry->name, entry->type->name)
                new_status = RESOURCE_STATUS_BUILDING;
                break;
            }

            if (third_party->last_modification_time != initial->primary_input_version.last_modification_time)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because its "
                         "primary input which is raw third party file has been changed.",
                         target->name, entry->name, entry->type->name)
                new_status = RESOURCE_STATUS_BUILDING;
                break;
            }
        }

        for (kan_loop_size_t index = 0u; index < initial->secondary_inputs.size; ++index)
        {
            const struct kan_resource_log_secondary_input_t *secondary =
                &((struct kan_resource_log_secondary_input_t *) initial->secondary_inputs.data)[index];

            if (!secondary->type)
            {
                // Third party dependency, custom handling.
                struct raw_third_party_entry_t *third_party =
                    target_search_visible_third_party (target, secondary->name);

                if (!third_party)
                {
                    KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                             "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because its "
                             "secondary input \"%s\" which is raw third party file cannot be found.",
                             target->name, entry->name, entry->type->name, secondary->name, secondary->type)
                    new_status = RESOURCE_STATUS_BUILDING;
                    break;
                }

                if (third_party->last_modification_time != secondary->version.last_modification_time)
                {
                    KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                             "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because its "
                             "secondary input \"%s\" which is raw third party file has been changed.",
                             target->name, entry->name, entry->type->name, secondary->name, secondary->type)
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
                         target->name, entry->name, entry->type->name, secondary->name, secondary->type)
                new_status = RESOURCE_STATUS_BUILDING;
                break;
            }

            KAN_ATOMIC_INT_SCOPED_LOCK_READ (&secondary_response.entry->header.lock)
            if (secondary_response.entry->header.status != RESOURCE_STATUS_AVAILABLE)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because its "
                         "secondary input \"%s\" of type \"%s\" has other status than available.",
                         target->name, entry->name, entry->type->name, secondary->name, secondary->type)
                new_status = RESOURCE_STATUS_BUILDING;
                break;
            }

            if (!kan_resource_log_version_is_up_to_date (secondary->version,
                                                         secondary_response.entry->header.available_version))
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as out of date because its "
                         "secondary input \"%s\" of type \"%s\" version mismatch.",
                         target->name, entry->name, entry->type->name, secondary->name, secondary->type)
                new_status = RESOURCE_STATUS_BUILDING;
                break;
            }
        }

        if (new_status != RESOURCE_STATUS_AVAILABLE)
        {
            break;
        }

        // If this resource was marked as unsupported during previous build, we should not mark it as available now.
        if (initial->saved_directory == KAN_RESOURCE_LOG_SAVED_DIRECTORY_UNSUPPORTED)
        {
            new_status = RESOURCE_STATUS_PLATFORM_UNSUPPORTED;
        }

        KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                 "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as up to date.", target->name,
                 entry->name, entry->type->name)
        break;
    }

    case RESOURCE_PRODUCTION_CLASS_SECONDARY:
    {
        const struct kan_resource_log_secondary_entry_t *initial = entry->initial_log_secondary_entry;
        // Use initial version value if availability checks succeed.
        available_version = initial->version;

        KAN_ASSERT_FORMATTED (
            initial,
            "[Target %s] Secondary resource \"%s\" of type \"%s\" is in unconfirmed status and no initial log, which "
            "should be impossible as newly created built entries must start in building status right away.",
            target->name, entry->type->name, entry->type->name)

        const struct kan_resource_reflected_data_resource_type_t *reflected_type =
            kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, entry->type->name);

        if (initial->version.type_version != reflected_type->resource_type_meta->version)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Marking secondary built resource \"%s\" of type \"%s\" as out of date because of "
                     "resource type version mismatch.",
                     target->name, entry->name, entry->type->name)
            new_status = RESOURCE_STATUS_BUILDING;
            break;
        }

        struct resource_response_t primary_input_response =
            execute_resource_request_internal (state,
                                               (struct resource_request_t) {
                                                   .from_target = target,
                                                   .type = initial->producer_type,
                                                   .name = initial->producer_name,
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
                         "[Target \"%s\"] Marking secondary built resource \"%s\" of type \"%s\" as unavailable "
                         "because its producer \"%s\" of type \"%s\" is unavailable too.",
                         target->name, entry->name, entry->type->name, initial->producer_name, initial->producer_type)
                new_status = RESOURCE_STATUS_UNAVAILABLE;
                break;

            case RESOURCE_STATUS_BUILDING:
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "[Target \"%s\"] Marking secondary built resource \"%s\" of type \"%s\" as out of date "
                         "because its producer \"%s\" of type \"%s\" already has building status.",
                         target->name, entry->name, entry->type->name, initial->producer_name, initial->producer_type)
                new_status = RESOURCE_STATUS_BUILDING;
                break;

            case RESOURCE_STATUS_AVAILABLE:
                if (!kan_resource_log_version_is_up_to_date (initial->producer_version,
                                                             primary_input_response.entry->header.available_version))
                {
                    KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                             "[Target \"%s\"] Marking secondary built resource \"%s\" of type \"%s\" as unavailable "
                             "because its producer \"%s\" of type \"%s\" has other version and this secondary resource "
                             "is not produced, making it unavailable.",
                             target->name, entry->name, entry->type->name)
                    new_status = RESOURCE_STATUS_UNAVAILABLE;
                    break;
                }

                break;

            case RESOURCE_STATUS_PLATFORM_UNSUPPORTED:
                KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                         "[Target \"%s\"] Marking secondary built resource \"%s\" of type \"%s\" as unavailable "
                         "because its producer \"%s\" of type \"%s\" is marked platform unsupported.",
                         target->name, entry->name, entry->type->name)
                new_status = RESOURCE_STATUS_UNAVAILABLE;
                break;

            case RESOURCE_STATUS_OUT_OF_SCOPE:
                KAN_ASSERT_FORMATTED (false,
                                      "Internal error, producer entry has out of scope status, which shouldn't be "
                                      "possible due to target visibility rules.", )
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
                     "[Target \"%s\"] Marking secondary built resource \"%s\" of type \"%s\" as unavailable "
                     "because its producer \"%s\" of type \"%s\" can no longer be found.",
                     target->name, entry->name, entry->type->name)
            new_status = RESOURCE_STATUS_UNAVAILABLE;
            break;
        }

        if (new_status != RESOURCE_STATUS_AVAILABLE)
        {
            break;
        }

        KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                 "[Target \"%s\"] Marking built resource \"%s\" of type \"%s\" as up to date.", target->name,
                 entry->name, entry->type->name)
        break;
    }
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
    case RESOURCE_STATUS_OUT_OF_SCOPE:
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
        add_to_build_queue_new_unsafe (state, entry);
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
        kan_allocate_batched (blocking_entry->allocation_group, sizeof (struct resource_entry_build_blocked_t));
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
        ++entry->build.loaded_data_request_count;
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
                entry->build.internal_next_build_task = RESOURCE_ENTRY_NEXT_BUILD_TASK_LOAD;

                KAN_ATOMIC_INT_SCOPED_LOCK (&state->build_queue_lock)
                add_to_build_queue_new_unsafe (state, entry);
            }

            block_entry_build_by_entry_unsafe (needed_to_build_entry, entry);
        }

        return true;
    }

    case RESOURCE_STATUS_PLATFORM_UNSUPPORTED:
    {
        // Platform unsupported are treated as valid dependencies unless required flag is set.
        return !required;
    }

    case RESOURCE_STATUS_OUT_OF_SCOPE:
    {
        // Should never be a build dependency if target visibility works as expected.
        KAN_ASSERT (false)
        return false;
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

    if (entry->initial_log_built_entry && kan_resource_log_version_is_up_to_date (
                                              entry->initial_log_built_entry->version, entry->header.available_version))
    {
        switch (entry->class)
        {
        case RESOURCE_PRODUCTION_CLASS_RAW:
            return &entry->initial_log_raw_entry->references;

        case RESOURCE_PRODUCTION_CLASS_PRIMARY:
            return &entry->initial_log_built_entry->references;

        case RESOURCE_PRODUCTION_CLASS_SECONDARY:
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
                    entry->header.status == RESOURCE_STATUS_PLATFORM_UNSUPPORTED ||
                    entry->header.status == RESOURCE_STATUS_OUT_OF_SCOPE)

        if (entry->initial_log_built_entry &&
            kan_resource_log_version_is_up_to_date (entry->initial_log_built_entry->version,
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

    if (reflected_type->build_rule_primary_input_type)
    {
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
        case RESOURCE_STATUS_OUT_OF_SCOPE:
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
        case RESOURCE_STATUS_OUT_OF_SCOPE:
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

    struct resource_request_backtrace_t trace = {
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
        // Not a circular-dependent mode, so we reset our part of backtrace
        // to avoid false positive circular reference failures.
        trace.previous = NULL;
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

            if (!reflected_type->produced_from_build_rule)
            {
                // Not produced by build rule, return default unavailable response.
                return response;
            }

            // Built resource should always be created in the same target as its primary input.
            struct target_t *primary_target = NULL;

            if (reflected_type->build_rule_primary_input_type)
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

                if (!primary_input_response.success)
                {
                    // No primary input to build entry, return default unavailable response.
                    return response;
                }

                primary_target = primary_input_response.entry->target;
            }
            else
            {
                struct raw_third_party_entry_t *entry =
                    target_search_visible_third_party (request.from_target, request.name);

                if (!entry)
                {
                    // No primary input to build entry, return default unavailable response.
                    return response;
                }

                primary_target = entry->target;
            }

            // Resource can be built using a rule, so let's create an entry and try to build it.
            // Lock for write as we're creating new entry.
            KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&state->resource_entries_lock)

            // While we were waiting for write access, somebody else might've already got write access and
            // create this node too. Let's check it.
            response.entry = target_search_local_resource_unsafe (primary_target, request.type, request.name);

            if (!response.entry)
            {
                struct resource_type_container_t *container =
                    target_search_resource_type_container_unsafe (primary_target, request.type);

                if (!container)
                {
                    container = resource_type_container_create (primary_target, reflected_type->struct_type);
                }

                response.entry = resource_entry_create (container, request.name);
                response.entry->class = RESOURCE_PRODUCTION_CLASS_PRIMARY;

                KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&response.entry->header.lock)
                KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&response.entry->build.lock)

                response.entry->header.status = RESOURCE_STATUS_BUILDING;
                response.entry->build.internal_next_build_task = RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_START;

                KAN_ATOMIC_INT_SCOPED_LOCK (&state->build_queue_lock)
                add_to_build_queue_new_unsafe (state, response.entry);
            }
            else if (response.entry->target != primary_target)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                         "[Target \"%s\"] Failed to create \"%s\" of type \"%s\" from build rule as resource with that "
                         "name already exists in target \"%s\" visible from primary input target \"%s\".",
                         request.from_target->name, request.name, request.type, response.entry->target->name,
                         primary_target->name);
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

    struct kan_stream_t *stream = kan_direct_file_stream_open_for_read (entry->current_file_location, true);
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
        kan_interned_string_t read_type_name;
        if (!kan_serialization_binary_read_type_header (
                stream, &read_type_name, KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t)))
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to load \"%s\" of type \"%s\" due to error while reading type header.",
                     entry->target->name, entry->name, entry->type->name);
            serialization_state = KAN_SERIALIZATION_FAILED;
            goto serialization_done;
        }

        if (read_type_name != entry->type->name)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to load \"%s\" of type \"%s\" as its type header specifies unexpected "
                     "type \"%s\".",
                     entry->target->name, entry->name, entry->type->name, read_type_name);
            serialization_state = KAN_SERIALIZATION_FAILED;
            goto serialization_done;
        }

        kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
            stream, loaded_data, entry->type->name, state->binary_script_storage,
            KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t), entry->allocation_group);

        while ((serialization_state = kan_serialization_binary_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
        {
        }

        if (serialization_state == KAN_SERIALIZATION_FAILED)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to load \"%s\" of type \"%s\" due to binary serialization error.",
                     entry->target->name, entry->name, entry->type->name);
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

serialization_done:
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
    enum resource_status_t status;
    struct kan_resource_log_version_t available_version;
    void *loaded_data_to_manage;
};

static struct build_step_output_t execute_build_raw_start (struct build_state_t *state, struct resource_entry_t *entry)
{
    struct build_step_output_t output = {
        .result = BUILD_STEP_RESULT_FAILED,
        .status = RESOURCE_STATUS_UNAVAILABLE,
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
    output.status = RESOURCE_STATUS_AVAILABLE;
    return output;
}

static struct build_step_output_t execute_build_primary_process_primary (struct build_state_t *state,
                                                                         struct resource_entry_t *entry);

static struct build_step_output_t execute_build_execute_build_rule (struct build_state_t *state,
                                                                    struct resource_entry_t *entry);

static struct build_step_output_t execute_build_primary_start (struct build_state_t *state,
                                                               struct resource_entry_t *entry)
{
    struct build_step_output_t output = {
        .result = BUILD_STEP_RESULT_FAILED,
        .status = RESOURCE_STATUS_UNAVAILABLE,
        .available_version = {.type_version = 0u, .last_modification_time = 0u},
        .loaded_data_to_manage = NULL,
    };

    const struct kan_resource_reflected_data_resource_type_t *reflected_type =
        kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, entry->type->name);
    KAN_ASSERT (reflected_type->produced_from_build_rule)

    if (!reflected_type->build_rule_primary_input_type)
    {
        return execute_build_execute_build_rule (state, entry);
    }

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

    return execute_build_primary_process_primary (state, entry);
}

static struct build_step_output_t execute_build_primary_process_primary (struct build_state_t *state,
                                                                         struct resource_entry_t *entry)
{
    struct build_step_output_t output = {
        .result = BUILD_STEP_RESULT_FAILED,
        .status = RESOURCE_STATUS_UNAVAILABLE,
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

            struct resource_response_t response =
                execute_resource_request (state, (struct resource_request_t) {
                                                     .from_target = entry->target,
                                                     .type = reference->type,
                                                     .name = reference->name,
                                                     .mode = (reference->flags & KAN_RESOURCE_REFERENCE_REQUIRED) ?
                                                                 RESOURCE_REQUEST_MODE_BUILD_REQUIRED :
                                                                 RESOURCE_REQUEST_MODE_BUILD_PLATFORM_OPTIONAL,
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
                target_search_visible_third_party (entry->target, reference->name);

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

struct build_rule_interface_data_t
{
    struct build_state_t *state;
    struct resource_entry_t *entry;
};

static bool save_entry_data (struct build_state_t *state, struct resource_entry_t *entry, const void *entry_data)
{
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (entry->current_file_location, true);
    if (!stream)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to save \"%s\" of type \"%s\" as it wasn't possible to open file at path "
                 "\"%s\" for write.",
                 entry->target->name, entry->name, entry->type->name, entry->current_file_location);
        return false;
    }

    stream = kan_random_access_stream_buffer_open_for_write (stream, KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER);
    CUSHION_DEFER { stream->operations->close (stream); }

    if (!kan_serialization_binary_write_type_header (
            stream, entry->type->name, KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t)))
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to save \"%s\" of type \"%s\" due to failure while writing type header.",
                 entry->target->name, entry->name, entry->type->name);
        return false;
    }

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

static void move_secondary_output_data_to_entry (struct build_state_t *state,
                                                 const struct kan_resource_reflected_data_resource_type_t *type_data,
                                                 struct resource_entry_t *entry,
                                                 void *data)
{
    KAN_ASSERT (!entry->build.internal_transient_secondary_output)
    entry->build.internal_transient_secondary_output =
        kan_allocate_general (entry->allocation_group, type_data->struct_type->size, type_data->struct_type->alignment);

    if (type_data->struct_type->init)
    {
        kan_allocation_group_stack_push (entry->allocation_group);
        type_data->struct_type->init (type_data->struct_type->functor_user_data,
                                      entry->build.internal_transient_secondary_output);
    }

    if (type_data->resource_type_meta->move)
    {
        type_data->resource_type_meta->move (entry->build.internal_transient_secondary_output, data);
    }
    else
    {
        kan_reflection_move_struct (state->setup->reflected_data->registry, type_data->struct_type,
                                    entry->build.internal_transient_secondary_output, data);
    }
}

static inline void reset_secondary_output_data (struct build_state_t *state,
                                                const struct kan_resource_reflected_data_resource_type_t *type_data,
                                                void *data)
{
    if (type_data->resource_type_meta->reset)
    {
        type_data->resource_type_meta->reset (data);
    }
    else
    {
        kan_reflection_reset_struct (state->setup->reflected_data->registry, type_data->struct_type, data);
    }
}

enum subroutine_result_t
{
    SUBROUTINE_RESULT_SUCCESSFUL = 0u,
    SUBROUTINE_RESULT_FAILED,
    SUBROUTINE_RESULT_SKIPPED,
};

static void add_to_build_queue_unblocked_unsafe (struct build_state_t *state, struct resource_entry_t *entry);

static enum subroutine_result_t interface_produce_secondary_output_check_reproduction (
    struct build_state_t *state,
    struct resource_entry_t *parent_entry,
    const struct kan_resource_reflected_data_resource_type_t *type_data,
    kan_interned_string_t name,
    void *data)
{
    KAN_ATOMIC_INT_SCOPED_LOCK_READ (&state->resource_entries_lock)
    struct resource_entry_t *reproduced =
        target_search_local_resource_unsafe (parent_entry->target, type_data->struct_type->name, name);

    if (!reproduced)
    {
        return SUBROUTINE_RESULT_SKIPPED;
    }

    if (reproduced->class != RESOURCE_PRODUCTION_CLASS_SECONDARY || !reproduced->initial_log_secondary_entry ||
        reproduced->initial_log_secondary_entry->producer_type != parent_entry->type->name ||
        reproduced->initial_log_secondary_entry->producer_name != parent_entry->name)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to produce secondary \"%s\" of type \"%s\" from \"%s\" of type \"%s\" as "
                 "resource with produced name and type already exists and does not acknowledge the same producer.",
                 parent_entry->target->name, reproduced->name, reproduced->type->name, parent_entry->type->name,
                 parent_entry->name);
        return SUBROUTINE_RESULT_FAILED;
    }

    KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&reproduced->header.lock)
    KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&reproduced->build.lock)

    switch (reproduced->header.status)
    {
    case RESOURCE_STATUS_UNCONFIRMED:
    {
        KAN_ASSERT (!reproduced->build.internal_transient_secondary_output)
        move_secondary_output_data_to_entry (state, type_data, reproduced, data);

        reproduced->header.status = RESOURCE_STATUS_BUILDING;
        reproduced->build.internal_next_build_task = RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_START;

        KAN_ATOMIC_INT_SCOPED_LOCK (&state->build_queue_lock)
        // Use unblocked order as we'd like to save and unload reproduced secondary as soon as possible.
        add_to_build_queue_unblocked_unsafe (state, reproduced);
        return SUBROUTINE_RESULT_SUCCESSFUL;
    }

    case RESOURCE_STATUS_UNAVAILABLE:
    case RESOURCE_STATUS_AVAILABLE:
    case RESOURCE_STATUS_PLATFORM_UNSUPPORTED:
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to produce secondary \"%s\" of type \"%s\" from \"%s\" of type \"%s\" as "
                 "target reproduced entry is already in unavailable/available/platform_unsupported state. Most likely "
                 "an internal error.",
                 parent_entry->target->name, reproduced->name, reproduced->type->name, parent_entry->type->name,
                 parent_entry->name);
        return SUBROUTINE_RESULT_FAILED;
    }

    case RESOURCE_STATUS_BUILDING:
    {
        if (reproduced->build.internal_transient_secondary_output)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to produce secondary \"%s\" of type \"%s\" from \"%s\" of type \"%s\" as "
                     "it was already produced from the same producer during current execution.",
                     parent_entry->target->name, reproduced->name, reproduced->type->name, parent_entry->type->name,
                     parent_entry->name);
            return SUBROUTINE_RESULT_FAILED;
        }

        move_secondary_output_data_to_entry (state, type_data, reproduced, data);
        return SUBROUTINE_RESULT_SUCCESSFUL;
    }

    case RESOURCE_STATUS_OUT_OF_SCOPE:
        // Should never be possible.
        KAN_ASSERT (false)
        break;
    }

    KAN_ASSERT (false)
    return SUBROUTINE_RESULT_FAILED;
}

static kan_interned_string_t interface_produce_secondary_output (kan_resource_build_rule_interface_t interface,
                                                                 kan_interned_string_t type,
                                                                 kan_interned_string_t name,
                                                                 void *data)
{
    struct build_rule_interface_data_t *interface_data = KAN_HANDLE_GET (interface);
    struct build_state_t *state = interface_data->state;
    struct resource_entry_t *parent_entry = interface_data->entry;

    const struct kan_resource_reflected_data_resource_type_t *type_data =
        kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, type);

    KAN_ASSERT_FORMATTED (
        type_data, "Received secondary production of type \"%s\", but unable to query that resource type reflection.",
        type)

    switch (interface_produce_secondary_output_check_reproduction (state, parent_entry, type_data, name, data))
    {
    case SUBROUTINE_RESULT_SUCCESSFUL:
        return name;

    case SUBROUTINE_RESULT_FAILED:
        reset_secondary_output_data (state, type_data, data);
        return NULL;

    case SUBROUTINE_RESULT_SKIPPED:
        break;
    }

    KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&state->resource_entries_lock)
    struct resource_entry_t *entry =
        target_search_local_resource_unsafe (parent_entry->target, type_data->struct_type->name, name);

    if (entry)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to produce secondary \"%s\" of type \"%s\" from \"%s\" of type \"%s\" as it "
                 "is already produced from some other producer.",
                 parent_entry->target->name, entry->name, entry->type->name, parent_entry->type->name,
                 parent_entry->name);

        reset_secondary_output_data (state, type_data, data);
        return NULL;
    }

    struct resource_type_container_t *container =
        target_search_resource_type_container_unsafe (parent_entry->target, type_data->name);

    if (!container)
    {
        container = resource_type_container_create (parent_entry->target, type_data->struct_type);
    }

    entry = resource_entry_create (container, name);
    entry->class = RESOURCE_PRODUCTION_CLASS_SECONDARY;

    KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&entry->header.lock)
    KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&entry->build.lock)

    entry->header.status = RESOURCE_STATUS_BUILDING;
    entry->build.internal_next_build_task = RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_START;
    entry->build.internal_producer_entry = parent_entry;
    move_secondary_output_data_to_entry (state, type_data, entry, data);

    KAN_ATOMIC_INT_SCOPED_LOCK (&state->build_queue_lock)
    // Use unblocked order as we'd like to save and unload reproduced secondary as soon as possible.
    add_to_build_queue_unblocked_unsafe (state, entry);
    return name;
}

static void remove_entry_loaded_data_usage (struct resource_entry_t *entry)
{
    KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&entry->build.lock)
    --entry->build.loaded_data_request_count;

    if (entry->build.loaded_data_request_count == 0u)
    {
        resource_entry_unload_data_unsafe (entry);
    }
}

static void cleanup_build_rule_context (struct kan_resource_build_rule_context_t *build_context,
                                        struct resource_entry_t *entry,
                                        struct resource_entry_t *primary)
{
    if (primary)
    {
        remove_entry_loaded_data_usage (primary);
    }

    for (kan_loop_size_t index = 0u; index < entry->new_build_secondary_inputs.size; ++index)
    {
        struct new_build_secondary_input_t *input =
            &((struct new_build_secondary_input_t *) entry->new_build_secondary_inputs.data)[index];

        if (input->entry)
        {
            remove_entry_loaded_data_usage (input->entry);
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

static void replace_entry_current_file_location (struct resource_entry_t *entry,
                                                 const struct kan_file_system_path_container_t *path)
{
    if (entry->current_file_location)
    {
        kan_free_general (entry->allocation_group, entry->current_file_location,
                          strlen (entry->current_file_location) + 1u);
    }

    entry->current_file_location = kan_allocate_general (entry->allocation_group, path->length + 1u, alignof (char));
    memcpy (entry->current_file_location, path->path, path->length);
    entry->current_file_location[path->length] = '\0';
}

/// \details In case of import build rules, can be executed right away from start task call.
static struct build_step_output_t execute_build_execute_build_rule (struct build_state_t *state,
                                                                    struct resource_entry_t *entry)
{
    struct build_step_output_t output = {
        .result = BUILD_STEP_RESULT_FAILED,
        .status = RESOURCE_STATUS_UNAVAILABLE,
        .available_version = {.type_version = 0u, .last_modification_time = 0u},
        .loaded_data_to_manage = NULL,
    };

    const struct kan_resource_reflected_data_resource_type_t *reflected_type =
        kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, entry->type->name);
    KAN_ASSERT (reflected_type->produced_from_build_rule)

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

    struct resource_entry_t *primary = NULL;
    struct raw_third_party_entry_t *primary_third_party = NULL;
    CUSHION_DEFER { cleanup_build_rule_context (&build_context, entry, primary); }

    if (reflected_type->build_rule_primary_input_type)
    {
        primary = entry->build.internal_primary_input_entry;
        KAN_ATOMIC_INT_SCOPED_LOCK_READ (&primary->header.lock)
        KAN_ATOMIC_INT_SCOPED_LOCK_READ (&primary->build.lock)

        // Shouldn't be able to enter that function if it is not the case.
        KAN_ASSERT (primary->header.status == RESOURCE_STATUS_AVAILABLE)

        build_context.primary_input = primary->build.loaded_data;
        KAN_ASSERT (build_context.primary_input)
    }
    else
    {
        primary_third_party = target_search_visible_third_party (entry->target, entry->name);
        if (!primary_third_party)
        {
            KAN_LOG (
                resource_pipeline_build, KAN_LOG_ERROR,
                "[Target \"%s\"] Failed to build \"%s\" of type \"%s\" as its primary third party input is not found.",
                entry->target->name, entry->name, entry->type->name);
            return output;
        }

        entry->build.internal_primary_input_third_party = primary_third_party;
        build_context.primary_third_party_path = primary_third_party->file_location;
    }

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
                KAN_ASSERT (node->data)

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

            case RESOURCE_STATUS_OUT_OF_SCOPE:
                KAN_ASSERT_FORMATTED (false,
                                      "Internal error, secondary input entry has out of scope status, which shouldn't "
                                      "be possible due to target visibility rules.", )
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
        output.status = RESOURCE_STATUS_PLATFORM_UNSUPPORTED;
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
    replace_entry_current_file_location (entry, &temporary_workspace);

    CUSHION_DEFER
    {
        if (output.result != BUILD_STEP_RESULT_SUCCESSFUL)
        {
            if (entry->type->shutdown)
            {
                entry->type->shutdown (entry->type->functor_user_data, loaded_data);
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
    output.status = RESOURCE_STATUS_AVAILABLE;
    output.available_version.type_version = reflected_type->resource_type_meta->version;
    output.available_version.last_modification_time = status.last_modification_time_ns;
    output.loaded_data_to_manage = loaded_data;
    return output;
}

static struct build_step_output_t execute_build_secondary_process_primary (struct build_state_t *state,
                                                                           struct resource_entry_t *entry);

static struct build_step_output_t execute_build_secondary_start (struct build_state_t *state,
                                                                 struct resource_entry_t *entry)
{
    struct build_step_output_t output = {
        .result = BUILD_STEP_RESULT_FAILED,
        .status = RESOURCE_STATUS_UNAVAILABLE,
        .available_version = {.type_version = 0u, .last_modification_time = 0u},
        .loaded_data_to_manage = NULL,
    };

    struct resource_request_t request;
    if (entry->build.internal_producer_entry)
    {
        // Produced during this build, create request in order to wait for primary build.
        request = (struct resource_request_t) {
            .from_target = entry->target,
            .type = entry->build.internal_producer_entry->type->name,
            .name = entry->build.internal_producer_entry->name,
            .mode = RESOURCE_REQUEST_MODE_BUILD_REQUIRED,
            .needed_to_build_entry = entry,
        };
    }
    else
    {
        // Must've been produced earlier, check if producer is up-to-date.
        KAN_ASSERT (entry->initial_log_secondary_entry)
        request = (struct resource_request_t) {
            .from_target = entry->target,
            .type = entry->initial_log_secondary_entry->producer_type,
            .name = entry->initial_log_secondary_entry->producer_name,
            .mode = RESOURCE_REQUEST_MODE_BUILD_REQUIRED,
            .needed_to_build_entry = entry,
        };
    }

    struct resource_response_t response = execute_resource_request (state, request);
    if (!response.success)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to process build start for \"%s\" of type \"%s\" as it wasn't possible to "
                 "request producer resource \"%s\" of type \"%s\".",
                 entry->target->name, entry->name, entry->type->name, entry->initial_log_secondary_entry->producer_type,
                 entry->initial_log_secondary_entry->producer_name);
        return output;
    }

    entry->build.internal_producer_entry = response.entry;
    if (kan_atomic_int_get (&entry->build.atomic_next_build_task_block_counter) > 0)
    {
        entry->build.internal_next_build_task = RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_PROCESS_PRIMARY;
        output.result = BUILD_STEP_RESULT_PAUSED;
        return output;
    }

    return execute_build_secondary_process_primary (state, entry);
}

static void cleanup_secondary_process_context (struct build_state_t *state, struct resource_entry_t *entry)
{
    if (entry->build.internal_producer_entry)
    {
        remove_entry_loaded_data_usage (entry->build.internal_producer_entry);
    }

    // It is only not NULL on cleanup if processing has failed.
    resource_entry_unload_secondary_transient_data (entry);
}

static struct build_step_output_t execute_build_secondary_process_primary (struct build_state_t *state,
                                                                           struct resource_entry_t *entry)
{
    struct build_step_output_t output = {
        .result = BUILD_STEP_RESULT_FAILED,
        .status = RESOURCE_STATUS_UNAVAILABLE,
        .available_version = {.type_version = 0u, .last_modification_time = 0u},
        .loaded_data_to_manage = NULL,
    };

    const struct kan_resource_reflected_data_resource_type_t *reflected_type =
        kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data, entry->type->name);
    CUSHION_DEFER { cleanup_secondary_process_context (state, entry); }

    {
        struct resource_entry_t *producer = entry->build.internal_producer_entry;
        KAN_ATOMIC_INT_SCOPED_LOCK_READ (&producer->header.lock)

        switch (producer->header.status)
        {
        case RESOURCE_STATUS_UNCONFIRMED:
        case RESOURCE_STATUS_BUILDING:
        case RESOURCE_STATUS_OUT_OF_SCOPE:
            KAN_ASSERT (false)
            break;

        case RESOURCE_STATUS_UNAVAILABLE:
        case RESOURCE_STATUS_PLATFORM_UNSUPPORTED:
        {
            // Technically not a failure, it is just that this secondary resource is no longer produced.
            // If it is still needed somewhere it will be a failure there, but it is a different story.
            output.result = BUILD_STEP_RESULT_SUCCESSFUL;
            output.status = RESOURCE_STATUS_UNAVAILABLE;

            KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                     "[Target \"%s\"] Marking secondary \"%s\" of type \"%s\" as unavailable, because its producer "
                     "\"%s\" of type \"%s\" is %s.",
                     entry->target->name, entry->name, entry->type->name, producer->name, producer->type->name,
                     producer->header.status == RESOURCE_STATUS_UNAVAILABLE ? "no longer available" :
                                                                              "not supported on this platform");

            return output;
        }

        case RESOURCE_STATUS_AVAILABLE:
            // Further processing is made below as we don't need locks for that.
            break;
        }
    }

    if (!entry->build.internal_transient_secondary_output)
    {
        output.result = BUILD_STEP_RESULT_SUCCESSFUL;
        output.status = RESOURCE_STATUS_UNAVAILABLE;

        KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                 "[Target \"%s\"] Marking secondary \"%s\" of type \"%s\" as unavailable, because its producer "
                 "\"%s\" of type \"%s\" didn't reproduce it during its build time.");
        return output;
    }

    // Should not be able to start build twice for one resource.
    KAN_ASSERT (entry->new_references.size == 0u)
    kan_resource_reflected_data_storage_detect_references (state->setup->reflected_data, entry->type->name,
                                                           entry->build.internal_transient_secondary_output,
                                                           &entry->new_references);

    struct kan_file_system_path_container_t temporary_save_path;
    kan_file_system_path_container_copy_string (&temporary_save_path, state->setup->project->workspace_directory);
    kan_file_system_path_container_append (&temporary_save_path, KAN_RESOURCE_PROJECT_WORKSPACE_TEMPORARY_DIRECTORY);

    kan_file_system_path_container_append (&temporary_save_path, entry->target->name);
    kan_file_system_make_directory (temporary_save_path.path);

    kan_file_system_path_container_append (&temporary_save_path, entry->type->name);
    kan_file_system_make_directory (temporary_save_path.path);

    kan_file_system_path_container_append (&temporary_save_path, entry->name);
    kan_file_system_path_container_add_suffix (&temporary_save_path, ".bin");
    replace_entry_current_file_location (entry, &temporary_save_path);

    const bool saved = save_entry_data (state, entry, entry->build.internal_transient_secondary_output);
    struct kan_file_system_entry_status_t status;
    const bool proper_version = saved && kan_file_system_query_entry (entry->current_file_location, &status);

    if (!proper_version)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to properly save \"%s\" of type \"%s\" as it wasn't possible to query "
                 "file status at \"%s\" after the save operation.",
                 entry->target->name, entry->name, entry->type->name, entry->current_file_location);
        return output;
    }

    output.result = BUILD_STEP_RESULT_SUCCESSFUL;
    output.status = RESOURCE_STATUS_AVAILABLE;
    output.available_version.type_version = reflected_type->resource_type_meta->version;
    output.available_version.last_modification_time = status.last_modification_time_ns;
    output.loaded_data_to_manage = entry->build.internal_transient_secondary_output;
    entry->build.internal_transient_secondary_output = NULL;
    return output;
}

static struct build_step_output_t execute_build_step (struct build_state_t *state, struct resource_entry_t *entry)
{
    struct build_step_output_t output = {
        .result = BUILD_STEP_RESULT_FAILED,
        .status = RESOURCE_STATUS_UNAVAILABLE,
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
            output = execute_build_raw_start (state, entry);
            break;

        case RESOURCE_PRODUCTION_CLASS_PRIMARY:
            output = execute_build_primary_start (state, entry);
            break;

        case RESOURCE_PRODUCTION_CLASS_SECONDARY:
            output = execute_build_secondary_start (state, entry);
            break;
        }

        break;

    case RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_PROCESS_PRIMARY:

        switch (entry->class)
        {
        case RESOURCE_PRODUCTION_CLASS_RAW:
            KAN_ASSERT_FORMATTED (false,
                                  "[Target \"%s\"] Got process primary input task for resource \"%s\" of type \"%s\" "
                                  "which is raw and therefore cannot get this task. It is an internal error.",
                                  entry->target->name, entry->name, entry->type->name)
            break;

        case RESOURCE_PRODUCTION_CLASS_PRIMARY:
            output = execute_build_primary_process_primary (state, entry);
            break;

        case RESOURCE_PRODUCTION_CLASS_SECONDARY:
            output = execute_build_secondary_process_primary (state, entry);
            break;
        }

        break;

    case RESOURCE_ENTRY_NEXT_BUILD_TASK_BUILD_EXECUTE_BUILD_RULE:
        output = execute_build_execute_build_rule (state, entry);
        break;

    case RESOURCE_ENTRY_NEXT_BUILD_TASK_LOAD:
        if ((output.loaded_data_to_manage = load_resource_entry_data (state, entry)))
        {
            output.result = BUILD_STEP_RESULT_SUCCESSFUL;
            KAN_ATOMIC_INT_SCOPED_LOCK_READ (&entry->header.lock)
            output.status = entry->header.status;
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
static void add_to_build_queue_new_unsafe (struct build_state_t *state, struct resource_entry_t *entry)
{
    struct build_queue_item_t *item =
        kan_allocate_batched (build_queue_allocation_group, sizeof (struct build_queue_item_t));

    item->entry = entry;
    item->state = state;
    kan_bd_list_add (&state->build_queue, NULL, &item->node);
}

/// \details Has no inbuilt locking, must be externally synchronized (therefore _unsafe suffix).
static void add_to_build_queue_unblocked_unsafe (struct build_state_t *state, struct resource_entry_t *entry)
{
    struct build_queue_item_t *item =
        kan_allocate_batched (build_queue_allocation_group, sizeof (struct build_queue_item_t));

    item->entry = entry;
    item->state = state;
    kan_bd_list_add (&state->build_queue, state->build_queue.first, &item->node);
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
            add_to_build_queue_unblocked_unsafe (state, blocked->blocked_entry);

            kan_bd_list_remove (&state->paused_list, &blocked->blocked_entry->build.internal_paused_list_item->node);
            kan_free_batched (build_queue_allocation_group, blocked->blocked_entry->build.internal_paused_list_item);
            blocked->blocked_entry->build.internal_paused_list_item = NULL;
        }

        kan_free_batched (entry->allocation_group, blocked);
        blocked = next;
    }

    entry->build.blocked_other_first = NULL;
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

    struct build_step_output_t output = execute_build_step (item->state, item->entry);
    switch (output.result)
    {
    case BUILD_STEP_RESULT_SUCCESSFUL:
    {
        const bool repeated_task = is_resource_entry_next_build_task_repeated (task_to_execute);
        bool should_propagate_deployment = false;
        bool should_propagate_cache = false;

        {
            KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&item->entry->header.lock)
            KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&item->entry->build.lock)

            // Make sure that repeated tasks will not override build state.
            KAN_ASSERT (!repeated_task || item->entry->header.status == output.status)
            KAN_ASSERT (!repeated_task ||
                        (item->entry->header.available_version.type_version == output.available_version.type_version &&
                         item->entry->header.available_version.last_modification_time ==
                             output.available_version.last_modification_time))

            item->entry->header.status = output.status;
            item->entry->header.available_version = output.available_version;
            item->entry->header.passed_build_routine_mark |= !repeated_task;

            // Sanity check. We cannot end up with available entry and no resource after successful build.
            KAN_ASSERT (output.loaded_data_to_manage || output.status != RESOURCE_STATUS_AVAILABLE)

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

        if (!repeated_task && output.status != RESOURCE_STATUS_UNAVAILABLE)
        {
            if (should_propagate_deployment)
            {
                mark_resource_references_for_deployment (item->state, item->entry, NULL);
            }

            if (should_propagate_deployment || should_propagate_cache)
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
        KAN_ASSERT (!output.loaded_data_to_manage)
        KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&item->entry->header.lock)
        KAN_ATOMIC_INT_SCOPED_LOCK_WRITE (&item->entry->build.lock)

        item->entry->header.status = RESOURCE_STATUS_UNAVAILABLE;
        item->entry->header.passed_build_routine_mark = true;

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
        KAN_ASSERT (!output.loaded_data_to_manage)
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
            add_to_build_queue_new_unsafe (item->state, item->entry);
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

static bool mark_root_for_deployment (struct build_state_t *state)
{
    // Deployment mark application also results in status confirmation checks for most of the build tree, which could
    // take significant time. However, just executing every deployment mark request in separate thread would not change
    // a lot: in most cases there are very little count of root resources that reference everything else, so marking
    // one of that resources would still result in checking most of the build tree.
    //
    // It is a good thing to change in the future, but we should prioritize overall implementation before trying to
    // optimize this part.

    KAN_LOG (resource_pipeline_build, KAN_LOG_INFO, "Marking root resources for deployment.")
    const kan_time_size_t start = kan_precise_time_get_elapsed_nanoseconds ();
    struct target_t *target = state->targets_first;

    struct kan_dynamic_array_t resources_to_mark;
    kan_dynamic_array_init (&resources_to_mark, KAN_RESOURCE_PIPELINE_BUILD_RRDM_INITIAL,
                            sizeof (struct resource_entry_t *), alignof (struct resource_entry_t *),
                            temporary_allocation_group);

    while (target)
    {
        // We actually need to mark deployment from out of scope targets to make sure that we will not lose resources
        // that are only referenced for deployment from out of scope targets. Therefore, `!target->marked_for_build`
        // check is not needed here.

        for (kan_loop_size_t index = 0u; index < state->setup->reflected_data->root_resource_type_names.size; ++index)
        {
            kan_interned_string_t type_name =
                ((kan_interned_string_t *) state->setup->reflected_data->root_resource_type_names.data)[index];

            struct resource_type_container_t *container =
                target_search_resource_type_container_unsafe (target, type_name);

            if (!container)
            {
                continue;
            }

            if (resources_to_mark.size + container->entries.items.size < resources_to_mark.capacity)
            {
                kan_dynamic_array_set_capacity (
                    &resources_to_mark,
                    KAN_MAX (resources_to_mark.size * 2u, resources_to_mark.size + container->entries.items.size));
            }

            struct resource_entry_t *entry = (struct resource_entry_t *) container->entries.items.first;
            while (entry)
            {
                *(struct resource_entry_t **) kan_dynamic_array_add_last (&resources_to_mark) = entry;
                entry = (struct resource_entry_t *) entry->node.list_node.next;
            }
        }

        target = target->next;
    }

    KAN_LOG (resource_pipeline_build, KAN_LOG_INFO, "Gathered %u root resources to mark for deployment.",
             (unsigned int) resources_to_mark.size)
    bool successful = true;

    for (kan_loop_size_t index = 0u; index < resources_to_mark.size; ++index)
    {
        struct resource_entry_t *entry = ((struct resource_entry_t **) resources_to_mark.data)[index];
        struct resource_response_t response =
            execute_resource_request (state, (struct resource_request_t) {
                                                 .from_target = entry->target,
                                                 .type = entry->type->name,
                                                 .name = entry->name,
                                                 .mode = RESOURCE_REQUEST_MODE_MARK_DEPLOYMENT,
                                                 .needed_to_build_entry = NULL,
                                             });

        if (!response.success)
        {
            successful = false;
            KAN_LOG (resource_pipeline_build, KAN_LOG_INFO,
                     "Failed to mark root resource \"%s\" of type \"%s\" in target \"%s\" for deployment.", entry->name,
                     entry->type->name, entry->target->name)
        }
    }

    kan_dynamic_array_shutdown (&resources_to_mark);
    kan_atomic_int_lock (&state->build_queue_lock);
    dispatch_new_tasks_from_queue_unsafe (state);
    kan_atomic_int_unlock (&state->build_queue_lock);

    const kan_time_size_t end = kan_precise_time_get_elapsed_nanoseconds ();
    KAN_LOG (resource_pipeline_build, KAN_LOG_INFO, "Done marking root resources for deployment in %.3f ms.",
             1e-6f * (float) (end - start))
    return successful;
}

static void append_entry_target_location_to_path_container (struct resource_entry_t *entry,
                                                            enum deployment_step_target_location_t location,
                                                            struct kan_file_system_path_container_t *path)
{
    switch (location)
    {
    case DEPLOYMENT_STEP_TARGET_LOCATION_DEPLOY:
        kan_resource_build_append_deploy_path_in_workspace (path, entry->target->name, entry->type->name, entry->name);
        break;

    case DEPLOYMENT_STEP_TARGET_LOCATION_CACHE:
        kan_resource_build_append_cache_path_in_workspace (path, entry->target->name, entry->type->name, entry->name);
        break;

    case DEPLOYMENT_STEP_TARGET_LOCATION_NONE:
        KAN_ASSERT (false)
        break;
    }
}

static bool move_unchanged_resource_for_deployment (struct resource_entry_t *entry,
                                                    enum deployment_step_target_location_t location,
                                                    struct kan_file_system_path_container_t *reused_path)
{
    const kan_instance_size_t base_path_length = reused_path->length;
    CUSHION_DEFER { kan_file_system_path_container_reset_length (reused_path, base_path_length); }
    append_entry_target_location_to_path_container (entry, location, reused_path);

    if (!kan_file_system_move_file (entry->current_file_location, reused_path->path))
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                             "[Target \"%s\"] Unable to move file for resource \"%s\" of type \"%s\" to path \"%s\" "
                             "during deployment/caching.",
                             entry->target->name, entry->name, entry->type->name, reused_path->path);
        return false;
    }

    replace_entry_current_file_location (entry, reused_path);
    // We must update version as some OSes will change file modification timestamp when moving file.
    struct kan_file_system_entry_status_t status;

    if (!kan_file_system_query_entry (reused_path->path, &status))
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                             "[Target \"%s\"] Unable to query file status for resource \"%s\" of type \"%s\" at path "
                             "\"%s\" during deployment/caching.",
                             entry->target->name, entry->name, entry->type->name, reused_path->path);
        return false;
    }

    entry->header.available_version.last_modification_time = status.last_modification_time_ns;
    KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
             "[Target \"%s\"] Done moving deployed/cached file for \"%s\" of type \"%s\".", entry->target->name,
             entry->name, entry->type->name);
    return true;
}

static bool remove_unchanged_resource_from_deployment_or_cache (struct resource_entry_t *entry,
                                                                struct kan_file_system_path_container_t *reused_path)
{
    if (!kan_file_system_remove_file (entry->current_file_location))
    {
        KAN_LOG (
            resource_pipeline_build, KAN_LOG_ERROR,
            "[Target \"%s\"] Failed to remove file for \"%s\" of type \"%s\" that is no longer deployed nor cached.",
            entry->target->name, entry->name, entry->type->name);
        return false;
    }

    KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
             "[Target \"%s\"] Done removing file for \"%s\" of type \"%s\" that is no longer deployed nor cached.",
             entry->target->name, entry->name, entry->type->name);
    return true;
}

static bool remove_changed_resource_from_old_location (struct resource_entry_t *entry,
                                                       enum deployment_step_target_location_t old_location,
                                                       struct kan_file_system_path_container_t *reused_path)
{
    if (old_location == DEPLOYMENT_STEP_TARGET_LOCATION_NONE)
    {
        return true;
    }

    const kan_instance_size_t base_path_length = reused_path->length;
    CUSHION_DEFER { kan_file_system_path_container_reset_length (reused_path, base_path_length); }
    append_entry_target_location_to_path_container (entry, old_location, reused_path);

    if (!kan_file_system_remove_file (reused_path->path))
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to remove old deployed/cached file for \"%s\" of type \"%s\" after resource "
                 "is changed.",
                 entry->target->name, entry->name, entry->type->name);
        return false;
    }

    KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
             "[Target \"%s\"] Done removing old deployed/cached file for \"%s\" of type \"%s\" (resource is changed).",
             entry->target->name, entry->name, entry->type->name);
    return true;
}

static inline bool deploy_raw_resource (struct build_state_t *state,
                                        struct resource_entry_t *entry,
                                        struct kan_file_system_path_container_t *reused_path)
{
    void *loaded_data = load_resource_entry_data (state, entry);
    if (!entry)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to deploy raw resource \"%s\" of type \"%s\" as it wasn't possible to load "
                 "its data.",
                 entry->target->name, entry->name, entry->type->name);
        return false;
    }

    CUSHION_DEFER
    {
        if (entry->type->shutdown)
        {
            entry->type->shutdown (entry->type->functor_user_data, loaded_data);
        }

        kan_free_general (entry->allocation_group, loaded_data, entry->type->size);
    }

    const kan_instance_size_t base_path_length = reused_path->length;
    CUSHION_DEFER { kan_file_system_path_container_reset_length (reused_path, base_path_length); }
    append_entry_target_location_to_path_container (entry, DEPLOYMENT_STEP_TARGET_LOCATION_DEPLOY, reused_path);

    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (reused_path->path, true);
    if (!stream)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to deploy raw resource \"%s\" of type \"%s\" as it wasn't possible to open "
                 "file write stream.",
                 entry->target->name, entry->name, entry->type->name);
        return false;
    }

    stream = kan_random_access_stream_buffer_open_for_write (stream, KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER);
    CUSHION_DEFER { stream->operations->close (stream); }

    if (!kan_serialization_binary_write_type_header (
            stream, entry->type->name, KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t)))
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to deploy raw resource \"%s\" of type \"%s\" due to failure while writing "
                 "type header.",
                 entry->target->name, entry->name, entry->type->name);
        return false;
    }

    kan_serialization_binary_writer_t writer =
        kan_serialization_binary_writer_create (stream, loaded_data, entry->type->name, state->binary_script_storage,
                                                KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));
    CUSHION_DEFER { kan_serialization_binary_writer_destroy (writer); }

    enum kan_serialization_state_t serialization_state;
    while ((serialization_state = kan_serialization_binary_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    if (serialization_state == KAN_SERIALIZATION_FAILED)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Failed to deploy raw resource \"%s\" of type \"%s\" due to serialization error.",
                 entry->target->name, entry->name, entry->type->name);
        return false;
    }

    // We do not update version as for deployed raw resources version of raw file is used instead of deployed file.
    KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
             "[Target \"%s\"] Done deploying raw resource file for \"%s\" of type \"%s\".", entry->target->name,
             entry->name, entry->type->name);
    return true;
}

static inline bool move_produced_file_for_cache_or_deployment (struct resource_entry_t *entry,
                                                               enum deployment_step_target_location_t location,
                                                               struct kan_file_system_path_container_t *reused_path)
{
    const kan_instance_size_t base_path_length = reused_path->length;
    CUSHION_DEFER { kan_file_system_path_container_reset_length (reused_path, base_path_length); }
    append_entry_target_location_to_path_container (entry, location, reused_path);

    KAN_ASSERT (entry->current_file_location)
    if (!kan_file_system_move_file (entry->current_file_location, reused_path->path))
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Unable to deploy/cache \"%s\" of type \"%s\" as file move operation failed.",
                 entry->target->name, entry->name, entry->type->name);
        return false;
    }

    replace_entry_current_file_location (entry, reused_path);
    // We must update version as some OSes will change file modification timestamp when moving file.
    struct kan_file_system_entry_status_t status;

    if (!kan_file_system_query_entry (reused_path->path, &status))
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "[Target \"%s\"] Unable to deploy/cache \"%s\" of type \"%s\" as file status query after move failed.",
                 entry->target->name, entry->name, entry->type->name);
        return false;
    }

    entry->header.available_version.last_modification_time = status.last_modification_time_ns;
    KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
             "[Target \"%s\"] Done deploying/caching file for \"%s\" of type \"%s\".", entry->target->name, entry->name,
             entry->type->name);
    return true;
}

static bool execute_deployment_caching_step_for_entry (struct build_state_t *state,
                                                       struct resource_entry_t *entry,
                                                       struct kan_file_system_path_container_t *reused_path)
{
    enum deployment_step_target_location_t old_location = DEPLOYMENT_STEP_TARGET_LOCATION_NONE;
    switch (entry->class)
    {
    case RESOURCE_PRODUCTION_CLASS_RAW:
        if (entry->initial_log_raw_entry && entry->initial_log_raw_entry->deployed)
        {
            old_location = DEPLOYMENT_STEP_TARGET_LOCATION_DEPLOY;
        }

        break;

    case RESOURCE_PRODUCTION_CLASS_PRIMARY:
        if (entry->initial_log_built_entry)
        {
            switch (entry->initial_log_built_entry->saved_directory)
            {
            case KAN_RESOURCE_LOG_SAVED_DIRECTORY_DEPLOY:
                old_location = DEPLOYMENT_STEP_TARGET_LOCATION_DEPLOY;
                break;

            case KAN_RESOURCE_LOG_SAVED_DIRECTORY_CACHE:
                old_location = DEPLOYMENT_STEP_TARGET_LOCATION_CACHE;
                break;

            case KAN_RESOURCE_LOG_SAVED_DIRECTORY_UNSUPPORTED:
                break;
            }
        }

        break;

    case RESOURCE_PRODUCTION_CLASS_SECONDARY:
        if (entry->initial_log_secondary_entry)
        {
            switch (entry->initial_log_secondary_entry->saved_directory)
            {
            case KAN_RESOURCE_LOG_SAVED_DIRECTORY_DEPLOY:
                old_location = DEPLOYMENT_STEP_TARGET_LOCATION_DEPLOY;
                break;

            case KAN_RESOURCE_LOG_SAVED_DIRECTORY_CACHE:
                old_location = DEPLOYMENT_STEP_TARGET_LOCATION_CACHE;
                break;

            case KAN_RESOURCE_LOG_SAVED_DIRECTORY_UNSUPPORTED:
                break;
            }
        }

        break;
    }

    enum deployment_step_target_location_t new_location = DEPLOYMENT_STEP_TARGET_LOCATION_NONE;
    if (entry->header.deployment_mark)
    {
        new_location = DEPLOYMENT_STEP_TARGET_LOCATION_DEPLOY;
    }
    else if (entry->header.cache_mark &&
             // Platform unsupported resources have no output and therefore there ios no physical file to cache.
             entry->header.status != RESOURCE_STATUS_PLATFORM_UNSUPPORTED &&
             // Raw files are preserved as raw files, so there is no sense to physically cache them.
             // Cache mark is only needed on them to check whether they were used in build at all.
             entry->class != RESOURCE_PRODUCTION_CLASS_RAW)
    {
        new_location = DEPLOYMENT_STEP_TARGET_LOCATION_CACHE;
    }

    if (!entry->header.passed_build_routine_mark)
    {
        if (old_location != new_location)
        {
            switch (old_location)
            {
            case DEPLOYMENT_STEP_TARGET_LOCATION_DEPLOY:
            case DEPLOYMENT_STEP_TARGET_LOCATION_CACHE:
                switch (new_location)
                {
                case DEPLOYMENT_STEP_TARGET_LOCATION_DEPLOY:
                case DEPLOYMENT_STEP_TARGET_LOCATION_CACHE:
                    return move_unchanged_resource_for_deployment (entry, new_location, reused_path);

                case DEPLOYMENT_STEP_TARGET_LOCATION_NONE:
                    return remove_unchanged_resource_from_deployment_or_cache (entry, reused_path);
                }

                break;

            case DEPLOYMENT_STEP_TARGET_LOCATION_NONE:
                switch (new_location)
                {
                case DEPLOYMENT_STEP_TARGET_LOCATION_DEPLOY:
                case DEPLOYMENT_STEP_TARGET_LOCATION_CACHE:
                    KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                             "[Target \"%s\"] Unable to deploy/cache \"%s\" of type \"%s\" as its version was not "
                             "changed, but there is no previous deployed/cached version.",
                             entry->target->name, entry->name, entry->type->name);
                    return false;

                case DEPLOYMENT_STEP_TARGET_LOCATION_NONE:
                    break;
                }

                break;
            }
        }

        return true;
    }

    remove_changed_resource_from_old_location (entry, old_location, reused_path);
    switch (new_location)
    {
    case DEPLOYMENT_STEP_TARGET_LOCATION_DEPLOY:
    case DEPLOYMENT_STEP_TARGET_LOCATION_CACHE:
        switch (entry->header.status)
        {
        case RESOURCE_STATUS_UNCONFIRMED:
            // Should not be able to get the mark and still be unconfirmed.
            KAN_ASSERT (false)
            break;

        case RESOURCE_STATUS_UNAVAILABLE:
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Unable to deploy/cache \"%s\" of type \"%s\" as it is unavailable due to build "
                     "failure.",
                     entry->target->name, entry->name, entry->type->name);
            return false;

        case RESOURCE_STATUS_BUILDING:
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Unable to deploy/cache \"%s\" of type \"%s\" as it is somehow in building "
                     "status, usually it happens due to deadlock.",
                     entry->target->name, entry->name, entry->type->name);
            return false;

        case RESOURCE_STATUS_AVAILABLE:
            switch (entry->class)
            {
            case RESOURCE_PRODUCTION_CLASS_RAW:
                return deploy_raw_resource (state, entry, reused_path);

            case RESOURCE_PRODUCTION_CLASS_PRIMARY:
            case RESOURCE_PRODUCTION_CLASS_SECONDARY:
                return move_produced_file_for_cache_or_deployment (entry, new_location, reused_path);
            }

            break;

        case RESOURCE_STATUS_PLATFORM_UNSUPPORTED:
        case RESOURCE_STATUS_OUT_OF_SCOPE:
            // Should never get not-none build location.
            KAN_ASSERT (false)
            break;
        }

        break;

    case DEPLOYMENT_STEP_TARGET_LOCATION_NONE:
        // Nothing to do, we've just cleared old deployment and that is all we need to do.
        break;
    }

    return true;
}

static void execute_deployment_caching_step_for_target (kan_functor_user_data_t user_data)
{
    struct target_t *target = (struct target_t *) user_data;
    struct build_state_t *state = target->state;
    target->deployment_step_successful = true;

    struct kan_file_system_path_container_t reused_path;
    kan_file_system_path_container_copy_string (&reused_path, state->setup->project->workspace_directory);
    const kan_instance_size_t reused_path_base_length = reused_path.length;

    // Ensure deploy directory for this target is created.
    kan_file_system_path_container_append (&reused_path, KAN_RESOURCE_PROJECT_WORKSPACE_DEPLOY_DIRECTORY);
    kan_file_system_make_directory (reused_path.path);

    kan_file_system_path_container_append (&reused_path, target->name);
    kan_file_system_make_directory (reused_path.path);
    kan_file_system_path_container_reset_length (&reused_path, reused_path_base_length);

    // Ensure cache directory for this target is created.
    kan_file_system_path_container_append (&reused_path, KAN_RESOURCE_PROJECT_WORKSPACE_CACHE_DIRECTORY);
    kan_file_system_make_directory (reused_path.path);

    kan_file_system_path_container_append (&reused_path, target->name);
    kan_file_system_make_directory (reused_path.path);
    kan_file_system_path_container_reset_length (&reused_path, reused_path_base_length);

    struct resource_type_container_t *container =
        (struct resource_type_container_t *) target->resource_types.items.first;

    while (container)
    {
        // Ensure deploy directory for this resource type is created.
        kan_file_system_path_container_append (&reused_path, KAN_RESOURCE_PROJECT_WORKSPACE_DEPLOY_DIRECTORY);
        kan_file_system_path_container_append (&reused_path, target->name);
        kan_file_system_path_container_append (&reused_path, container->type->name);
        kan_file_system_make_directory (reused_path.path);
        kan_file_system_path_container_reset_length (&reused_path, reused_path_base_length);

        // Ensure cache directory for this resource type is created.
        kan_file_system_path_container_append (&reused_path, KAN_RESOURCE_PROJECT_WORKSPACE_CACHE_DIRECTORY);
        kan_file_system_path_container_append (&reused_path, target->name);
        kan_file_system_path_container_append (&reused_path, container->type->name);
        kan_file_system_make_directory (reused_path.path);
        kan_file_system_path_container_reset_length (&reused_path, reused_path_base_length);

        struct resource_entry_t *entry = (struct resource_entry_t *) container->entries.items.first;
        while (entry)
        {
            target->deployment_step_successful &=
                execute_deployment_caching_step_for_entry (state, entry, &reused_path);
            kan_file_system_path_container_reset_length (&reused_path, reused_path_base_length);
            entry = (struct resource_entry_t *) entry->node.list_node.next;
        }

        container = (struct resource_type_container_t *) container->node.list_node.next;
    }
}

static bool execute_deployment_caching_step (struct build_state_t *state)
{
    KAN_LOG (resource_pipeline_build, KAN_LOG_INFO, "Executing resource deployment and caching passes.")
    KAN_CPU_SCOPED_STATIC_SECTION (execute_deployment_and_caching)
    const kan_time_size_t start = kan_precise_time_get_elapsed_nanoseconds ();

    struct target_t *target = state->targets_first;
    kan_cpu_job_t job = kan_cpu_job_create ();

    while (target)
    {
        CUSHION_DEFER { target = target->next; }
        if (!target->marked_for_build)
        {
            continue;
        }

        // There is not that many targets, so we can just post tasks one by one instead of using task list.
        kan_cpu_job_dispatch_task (job, (struct kan_cpu_task_t) {
                                            .function = execute_deployment_caching_step_for_target,
                                            .user_data = (kan_functor_user_data_t) target,
                                            .profiler_section = kan_cpu_section_get (target->name),
                                        });
    }

    kan_cpu_job_release (job);
    kan_cpu_job_wait (job);

    bool successful = true;
    target = state->targets_first;

    while (target)
    {
        successful &= target->deployment_step_successful;
        target = target->next;
    }

    const kan_time_size_t end = kan_precise_time_get_elapsed_nanoseconds ();
    KAN_LOG (resource_pipeline_build, KAN_LOG_INFO, "Done moving resource files for deployment and caching in %.3f ms.",
             1e-6f * (float) (end - start))
    return successful;
}

static void add_entry_to_build_log (struct build_state_t *state,
                                    struct kan_resource_log_target_t *log_target,
                                    struct resource_entry_t *entry)
{
    // Unless entry is marked as available or platform unsupported, it is not added to log.
    switch (entry->header.status)
    {
    case RESOURCE_STATUS_UNCONFIRMED:
    case RESOURCE_STATUS_UNAVAILABLE:
    case RESOURCE_STATUS_BUILDING:
        return;

    case RESOURCE_STATUS_AVAILABLE:
    case RESOURCE_STATUS_PLATFORM_UNSUPPORTED:
        break;

    case RESOURCE_STATUS_OUT_OF_SCOPE:
        // Must be processed separately on target log generation level.
        KAN_ASSERT (false)
        return;
    }

    // Unless entry is either deployed or cached, it is not added to log.
    if (!entry->header.deployment_mark && !entry->header.cache_mark)
    {
        return;
    }

    if (entry->header.passed_build_routine_mark)
    {
        switch (entry->class)
        {
        case RESOURCE_PRODUCTION_CLASS_RAW:
        {
            struct kan_resource_log_raw_entry_t *log_entry = kan_dynamic_array_add_last (&log_target->raw);
            if (!log_entry)
            {
                kan_dynamic_array_set_capacity (&log_target->raw, log_target->raw.size * 2u);
                log_entry = kan_dynamic_array_add_last (&log_target->raw);
                KAN_ASSERT (entry)
            }

            kan_resource_log_raw_entry_init (log_entry);
            log_entry->type = entry->type->name;
            log_entry->name = entry->name;
            log_entry->version = entry->header.available_version;
            log_entry->deployed = entry->header.deployment_mark;

            kan_dynamic_array_set_capacity (&log_entry->references, entry->new_references.size);
            log_entry->references.size = entry->new_references.size;
            memcpy (log_entry->references.data, entry->new_references.data,
                    sizeof (struct kan_resource_log_reference_t) * log_entry->references.size);
            break;
        }

        case RESOURCE_PRODUCTION_CLASS_PRIMARY:
        {
            struct kan_resource_log_built_entry_t *log_entry = kan_dynamic_array_add_last (&log_target->built);
            if (!log_entry)
            {
                kan_dynamic_array_set_capacity (&log_target->built, log_target->built.size * 2u);
                log_entry = kan_dynamic_array_add_last (&log_target->built);
                KAN_ASSERT (entry)
            }

            kan_resource_log_built_entry_init (log_entry);
            log_entry->type = entry->type->name;
            log_entry->name = entry->name;
            log_entry->version = entry->header.available_version;

            const struct kan_resource_reflected_data_resource_type_t *reflected_type =
                kan_resource_reflected_data_storage_query_resource_type (state->setup->reflected_data,
                                                                         entry->type->name);

            if (reflected_type->build_rule_platform_configuration_type)
            {
                const struct platform_configuration_entry_t *platform_configuration =
                    build_state_find_platform_configuration (state,
                                                             reflected_type->build_rule_platform_configuration_type);

                // If we've successfully built this entry, then configuration is here.
                KAN_ASSERT (platform_configuration)
                log_entry->platform_configuration_time = platform_configuration->file_time;
            }
            else
            {
                log_entry->platform_configuration_time = 0u;
            }

            log_entry->rule_version = reflected_type->build_rule_version;
            if (reflected_type->build_rule_primary_input_type)
            {
                // If we've successfully built this entry, then primary input is here.
                KAN_ASSERT (entry->build.internal_primary_input_entry)
                log_entry->primary_input_version = entry->build.internal_primary_input_entry->header.available_version;
            }
            else
            {
                // If we've successfully built this entry, then primary input is here.
                KAN_ASSERT (entry->build.internal_primary_input_third_party)
                log_entry->primary_input_version.type_version = 0u;
                log_entry->primary_input_version.last_modification_time =
                    entry->build.internal_primary_input_third_party->last_modification_time;
            }

            if (entry->header.status == RESOURCE_STATUS_PLATFORM_UNSUPPORTED)
            {
                log_entry->saved_directory = KAN_RESOURCE_LOG_SAVED_DIRECTORY_UNSUPPORTED;
            }
            else if (entry->header.deployment_mark)
            {
                log_entry->saved_directory = KAN_RESOURCE_LOG_SAVED_DIRECTORY_DEPLOY;
            }
            else if (entry->header.cache_mark)
            {
                log_entry->saved_directory = KAN_RESOURCE_LOG_SAVED_DIRECTORY_CACHE;
            }

            kan_dynamic_array_set_capacity (&log_entry->references, entry->new_references.size);
            log_entry->references.size = entry->new_references.size;

            memcpy (log_entry->references.data, entry->new_references.data,
                    sizeof (struct kan_resource_log_reference_t) * log_entry->references.size);
            kan_dynamic_array_set_capacity (&log_entry->secondary_inputs, entry->new_build_secondary_inputs.size);

            for (kan_loop_size_t index = 0u; index < entry->new_build_secondary_inputs.size; ++index)
            {
                struct new_build_secondary_input_t *input =
                    &((struct new_build_secondary_input_t *) entry->new_build_secondary_inputs.data)[index];

                struct kan_resource_log_secondary_input_t *output =
                    kan_dynamic_array_add_last (&log_entry->secondary_inputs);
                KAN_ASSERT (output)

                if (input->entry)
                {
                    output->type = input->entry->type->name;
                    output->name = input->entry->name;
                    output->version = input->entry->header.available_version;
                }
                else
                {
                    output->type = NULL;
                    output->name = input->third_party_entry->name;
                    output->version.type_version = 0u;
                    output->version.last_modification_time = input->third_party_entry->last_modification_time;
                }
            }

            break;
        }

        case RESOURCE_PRODUCTION_CLASS_SECONDARY:
        {
            struct kan_resource_log_secondary_entry_t *log_entry = kan_dynamic_array_add_last (&log_target->secondary);
            if (!log_entry)
            {
                kan_dynamic_array_set_capacity (&log_target->secondary, log_target->secondary.size * 2u);
                log_entry = kan_dynamic_array_add_last (&log_target->secondary);
                KAN_ASSERT (entry)
            }

            kan_resource_log_secondary_entry_init (log_entry);
            log_entry->type = entry->type->name;
            log_entry->name = entry->name;
            log_entry->version = entry->header.available_version;

            KAN_ASSERT (entry->header.status != RESOURCE_STATUS_PLATFORM_UNSUPPORTED)
            if (entry->header.deployment_mark)
            {
                log_entry->saved_directory = KAN_RESOURCE_LOG_SAVED_DIRECTORY_DEPLOY;
            }
            else if (entry->header.cache_mark)
            {
                log_entry->saved_directory = KAN_RESOURCE_LOG_SAVED_DIRECTORY_CACHE;
            }

            // If we've successfully built this entry, then producer is here.
            KAN_ASSERT (entry->build.internal_producer_entry)
            log_entry->producer_type = entry->build.internal_producer_entry->type->name;
            log_entry->producer_name = entry->build.internal_producer_entry->name;
            log_entry->producer_version = entry->build.internal_producer_entry->header.available_version;

            kan_dynamic_array_set_capacity (&log_entry->references, entry->new_references.size);
            log_entry->references.size = entry->new_references.size;
            memcpy (log_entry->references.data, entry->new_references.data,
                    sizeof (struct kan_resource_log_reference_t) * log_entry->references.size);
            break;
        }
        }
    }
    else
    {
        switch (entry->class)
        {
        case RESOURCE_PRODUCTION_CLASS_RAW:
        {
            struct kan_resource_log_raw_entry_t *log_entry = kan_dynamic_array_add_last (&log_target->raw);
            if (!log_entry)
            {
                kan_dynamic_array_set_capacity (&log_target->raw, log_target->raw.size * 2u);
                log_entry = kan_dynamic_array_add_last (&log_target->raw);
                KAN_ASSERT (entry)
            }

            // Should never get that status, only primary resources can get it.
            KAN_ASSERT (entry->header.status != RESOURCE_STATUS_PLATFORM_UNSUPPORTED)
            KAN_ASSERT (entry->initial_log_raw_entry)
            kan_resource_log_raw_entry_init_copy (log_entry, entry->initial_log_raw_entry);

            log_entry->version = entry->header.available_version;
            log_entry->deployed = entry->header.deployment_mark;
            break;
        }

        case RESOURCE_PRODUCTION_CLASS_PRIMARY:
        {
            struct kan_resource_log_built_entry_t *log_entry = kan_dynamic_array_add_last (&log_target->built);
            if (!log_entry)
            {
                kan_dynamic_array_set_capacity (&log_target->built, log_target->built.size * 2u);
                log_entry = kan_dynamic_array_add_last (&log_target->built);
                KAN_ASSERT (entry)
            }

            KAN_ASSERT (entry->initial_log_built_entry)
            kan_resource_log_built_entry_init_copy (log_entry, entry->initial_log_built_entry);
            log_entry->version = entry->header.available_version;

            if (entry->header.status == RESOURCE_STATUS_PLATFORM_UNSUPPORTED)
            {
                log_entry->saved_directory = KAN_RESOURCE_LOG_SAVED_DIRECTORY_UNSUPPORTED;
            }
            else if (entry->header.deployment_mark)
            {
                log_entry->saved_directory = KAN_RESOURCE_LOG_SAVED_DIRECTORY_DEPLOY;
            }
            else if (entry->header.cache_mark)
            {
                log_entry->saved_directory = KAN_RESOURCE_LOG_SAVED_DIRECTORY_CACHE;
            }

            break;
        }

        case RESOURCE_PRODUCTION_CLASS_SECONDARY:
        {
            struct kan_resource_log_secondary_entry_t *log_entry = kan_dynamic_array_add_last (&log_target->secondary);
            if (!log_entry)
            {
                kan_dynamic_array_set_capacity (&log_target->secondary, log_target->secondary.size * 2u);
                log_entry = kan_dynamic_array_add_last (&log_target->secondary);
                KAN_ASSERT (entry)
            }

            // Should never get that status, only primary resources can get it.
            KAN_ASSERT (entry->header.status != RESOURCE_STATUS_PLATFORM_UNSUPPORTED)
            KAN_ASSERT (entry->initial_log_secondary_entry)

            kan_resource_log_secondary_entry_init_copy (log_entry, entry->initial_log_secondary_entry);
            log_entry->version = entry->header.available_version;

            if (entry->header.deployment_mark)
            {
                log_entry->saved_directory = KAN_RESOURCE_LOG_SAVED_DIRECTORY_DEPLOY;
            }
            else if (entry->header.cache_mark)
            {
                log_entry->saved_directory = KAN_RESOURCE_LOG_SAVED_DIRECTORY_CACHE;
            }

            break;
        }
        }
    }
}

static bool generate_and_save_build_log (struct build_state_t *state)
{
    KAN_LOG (resource_pipeline_build, KAN_LOG_INFO, "Generating and saving build log.")
    KAN_CPU_SCOPED_STATIC_SECTION (generate_and_save_build_log)

    const kan_time_size_t start = kan_precise_time_get_elapsed_nanoseconds ();
    CUSHION_DEFER
    {
        const kan_time_size_t end = kan_precise_time_get_elapsed_nanoseconds ();
        KAN_LOG (resource_pipeline_build, KAN_LOG_INFO, "Finished build log generation and saving step in %.3f ms.",
                 1e-6f * (float) (end - start))
    }

    struct kan_resource_log_t new_log;
    kan_resource_log_init (&new_log);
    CUSHION_DEFER { kan_resource_log_shutdown (&new_log); }

    kan_dynamic_array_set_capacity (&new_log.targets, state->setup->project->targets.size);
    struct target_t *target = state->targets_first;

    while (target)
    {
        CUSHION_DEFER { target = target->next; }
        if (!target->marked_for_build)
        {
            if (target->initial)
            {
                struct kan_resource_log_target_t *log_target = kan_dynamic_array_add_last (&new_log.targets);
                KAN_ASSERT (log_target)
                kan_resource_log_target_init_copy (log_target, target->initial);
            }

            continue;
        }

        struct kan_resource_log_target_t *log_target = kan_dynamic_array_add_last (&new_log.targets);
        KAN_ASSERT (log_target)

        kan_resource_log_target_init (log_target);
        log_target->name = target->name;

        kan_dynamic_array_set_capacity (&log_target->raw, KAN_RESOURCE_PIPELINE_BUILD_LOG_ENTRIES_CAPACITY);
        kan_dynamic_array_set_capacity (&log_target->built, KAN_RESOURCE_PIPELINE_BUILD_LOG_ENTRIES_CAPACITY);
        kan_dynamic_array_set_capacity (&log_target->secondary, KAN_RESOURCE_PIPELINE_BUILD_LOG_ENTRIES_CAPACITY);

        struct resource_type_container_t *container =
            (struct resource_type_container_t *) target->resource_types.items.first;

        while (container)
        {
            struct resource_entry_t *entry = (struct resource_entry_t *) container->entries.items.first;
            while (entry)
            {
                add_entry_to_build_log (state, log_target, entry);
                entry = (struct resource_entry_t *) entry->node.list_node.next;
            }

            container = (struct resource_type_container_t *) container->node.list_node.next;
        }
    }

    struct kan_file_system_path_container_t resource_log_path;
    kan_file_system_path_container_copy_string (&resource_log_path, state->setup->project->workspace_directory);
    kan_file_system_path_container_append (&resource_log_path, KAN_RESOURCE_LOG_DEFAULT_NAME);
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (resource_log_path.path, true);

    if (!stream)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                             "Failed to save build log at \"%s\": unable to open write stream.",
                             resource_log_path.path);
        return false;
    }

    stream = kan_random_access_stream_buffer_open_for_write (stream, KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER);
    CUSHION_DEFER { stream->operations->close (stream); }

    if (stream->operations->write (stream, sizeof (resource_build_version), &resource_build_version) !=
        sizeof (resource_build_version))
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                             "Failed to write resource build version into \"%s\".", resource_log_path.path);
        return false;
    }

    kan_serialization_binary_writer_t writer = kan_serialization_binary_writer_create (
        stream, &new_log, KAN_STATIC_INTERNED_ID_GET (kan_resource_log_t), state->binary_script_storage,
        KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));
    CUSHION_DEFER { kan_serialization_binary_writer_destroy (writer); }

    enum kan_serialization_state_t serialization_state;
    while ((serialization_state = kan_serialization_binary_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    if (serialization_state == KAN_SERIALIZATION_FAILED)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                             "Failed to save build log at \"%s\": serialization error encountered.",
                             resource_log_path.path);
        return false;
    }

    return true;
}

static enum kan_resource_build_result_t execute_build (struct build_state_t *state)
{
    struct kan_file_system_path_container_t temporary_workspace;
    kan_file_system_path_container_copy_string (&temporary_workspace, state->setup->project->workspace_directory);
    kan_file_system_path_container_append (&temporary_workspace, KAN_RESOURCE_PROJECT_WORKSPACE_TEMPORARY_DIRECTORY);

    KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_DEBUG,
                         "Cleaning temporary directory \"%s\".", temporary_workspace.path);

    if (kan_file_system_check_existence (temporary_workspace.path))
    {
        if (!kan_file_system_remove_directory_with_content (temporary_workspace.path))
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                                 "Failed to clean temporary directory \"%s\".", temporary_workspace.path);
            return KAN_RESOURCE_BUILD_RESULT_ERROR_BUILD_FAILED;
        }
    }

    if (!kan_file_system_make_directory (temporary_workspace.path))
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                             "Failed to create temporary directory \"%s\".", temporary_workspace.path);
        return KAN_RESOURCE_BUILD_RESULT_ERROR_BUILD_FAILED;
    }

    const bool marked_root_for_deployment = mark_root_for_deployment (state);
    if (!marked_root_for_deployment)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "Failed to mark some root resources for deployment, build result will be incomplete.")
    }

    while (true)
    {
        {
            KAN_ATOMIC_INT_SCOPED_LOCK (&state->build_queue_lock)
            if (state->build_queue.size == 0u && state->currently_scheduled_build_operations == 0u)
            {
                // Finished building, check the results.
                break;
            }
        }

        kan_precise_time_sleep (KAN_RESOURCE_PIPELINE_BUILD_WORKING_CHECK_DELAY_NS);
    }

    const bool not_in_deadlock = state->paused_list.size == 0u;
    if (!not_in_deadlock)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                 "Build finished in deadlock state. Printing locked resources and their locks.")

        struct build_info_list_item_t *paused_list_item = (struct build_info_list_item_t *) state->paused_list.first;
        while (paused_list_item)
        {
            struct resource_entry_t *entry = paused_list_item->entry;
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "Entry \"%s\" of type \"%s\" from target \"%s\" is inside deadlock list.", entry->name,
                     entry->type->name, entry->target->name)

            struct resource_entry_build_blocked_t *blocked = entry->build.blocked_other_first;
            while (blocked)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                         "Entry \"%s\" of type \"%s\" from target \"%s\" blocks building of entry \"%s\" of type "
                         "\"%s\" from target \"%s\".",
                         entry->name, entry->type->name, entry->target->name, blocked->blocked_entry->name,
                         blocked->blocked_entry->type->name, blocked->blocked_entry->target->name)
                blocked = blocked->next;
            }

            paused_list_item = (struct build_info_list_item_t *) paused_list_item->node.next;
        }
    }

    const bool has_no_failed_tasks = state->failed_list.size == 0u;
    if (!has_no_failed_tasks)
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR, "Build finished with failed resource build tasks.")
        struct build_info_list_item_t *failed_list_item = (struct build_info_list_item_t *) state->failed_list.first;

        while (failed_list_item)
        {
            struct resource_entry_t *entry = failed_list_item->entry;
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "Entry \"%s\" of type \"%s\" from target \"%s\" build task has failed.", entry->name,
                     entry->type->name, entry->target->name)
            failed_list_item = (struct build_info_list_item_t *) failed_list_item->node.next;
        }
    }

    const bool deployment_successful = execute_deployment_caching_step (state);
    const bool post_clean_done = kan_file_system_remove_directory_with_content (temporary_workspace.path);

    if (!post_clean_done)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                             "Failed to clean temporary directory \"%s\".", temporary_workspace.path);
    }

    const bool log_update_successful = generate_and_save_build_log (state);
    return marked_root_for_deployment && not_in_deadlock && has_no_failed_tasks && deployment_successful &&
                   log_update_successful ?
               KAN_RESOURCE_BUILD_RESULT_SUCCESS :
               KAN_RESOURCE_BUILD_RESULT_ERROR_BUILD_FAILED;
}

// Pack step implementation section.

static bool pack_entry_sort_comparator (struct resource_entry_t *left, struct resource_entry_t *right)
{
    if (left->type == right->type)
    {
        return strcmp (left->name, right->name) < 0;
    }

    return strcmp (left->type->name, right->type->name) < 0;
}

static inline void pack_fill_internal_path (struct resource_entry_t *entry,
                                            struct kan_file_system_path_container_t *output)
{
    kan_file_system_path_container_copy_string (output, entry->type->name);
    kan_file_system_path_container_append (output, entry->name);
    kan_file_system_path_container_add_suffix (output, ".bin");
}

static void execute_pack_for_target (kan_functor_user_data_t user_data)
{
    struct target_t *target = (struct target_t *) user_data;
    struct build_state_t *state = target->state;
    target->pack_step_successful = true;

    struct kan_dynamic_array_t entries_to_pack;
    kan_dynamic_array_init (&entries_to_pack, KAN_RESOURCE_PIPELINE_BUILD_PACK_ENTRIES_CAPACITY,
                            sizeof (struct resource_entry_t *), alignof (struct resource_entry_t *),
                            temporary_allocation_group);
    CUSHION_DEFER { kan_dynamic_array_shutdown (&entries_to_pack); }

    struct resource_type_container_t *container =
        (struct resource_type_container_t *) target->resource_types.items.first;
    kan_loop_size_t entry_types_count = 0u;

    while (container)
    {
        bool any_added = false;
        struct resource_entry_t *entry = (struct resource_entry_t *) container->entries.items.first;

        while (entry)
        {
            if (entry->header.deployment_mark)
            {
                struct resource_entry_t **spot = kan_dynamic_array_add_last (&entries_to_pack);
                if (!spot)
                {
                    kan_dynamic_array_set_capacity (&entries_to_pack, entries_to_pack.size * 2u);
                    spot = kan_dynamic_array_add_last (&entries_to_pack);
                }

                *spot = entry;
                any_added = true;
            }

            entry = (struct resource_entry_t *) entry->node.list_node.next;
        }

        if (any_added)
        {
            ++entry_types_count;
        }

        container = (struct resource_type_container_t *) container->node.list_node.next;
    }

    {
        struct resource_entry_t *temporary;

#define AT_INDEX(INDEX) (((struct resource_entry_t **) entries_to_pack.data)[INDEX])
#define LESS(first_index, second_index)                                                                                \
    __CUSHION_PRESERVE__ pack_entry_sort_comparator (AT_INDEX (first_index), AT_INDEX (second_index))
#define SWAP(first_index, second_index)                                                                                \
    __CUSHION_PRESERVE__                                                                                               \
    temporary = AT_INDEX (first_index), AT_INDEX (first_index) = AT_INDEX (second_index),                              \
    AT_INDEX (second_index) = temporary

        QSORT (entries_to_pack.size, LESS, SWAP);
#undef LESS
#undef SWAP
#undef AT_INDEX
    }

    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_string (&path_container, state->setup->project->workspace_directory);
    kan_resource_build_append_pack_path_in_workspace (&path_container, target->name);
    struct kan_stream_t *pack_output_stream = kan_direct_file_stream_open_for_write (path_container.path, true);

    if (!pack_output_stream)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, resource_pipeline_build, KAN_LOG_ERROR,
                             "[Target \"%s\"] Failed to open pack file at \"%s\" for write.", target->name,
                             path_container.path);
        target->pack_step_successful = false;
        return;
    }

    pack_output_stream =
        kan_random_access_stream_buffer_open_for_write (pack_output_stream, KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER);
    CUSHION_DEFER { pack_output_stream->operations->close (pack_output_stream); }

    kan_virtual_file_system_read_only_pack_builder_t pack_builder =
        kan_virtual_file_system_read_only_pack_builder_create ();
    CUSHION_DEFER { kan_virtual_file_system_read_only_pack_builder_destroy (pack_builder); }

    if (!kan_virtual_file_system_read_only_pack_builder_begin (pack_builder, pack_output_stream))
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR, "[Target \"%s\"] Pack builder start failure.", target->name);
        target->pack_step_successful = false;
        return;
    }

    kan_serialization_interned_string_registry_t interned_string_registry = KAN_HANDLE_INITIALIZE_INVALID;
    CUSHION_DEFER
    {
        if (KAN_HANDLE_IS_VALID (interned_string_registry))
        {
            kan_serialization_interned_string_registry_destroy (interned_string_registry);
        }
    }

    switch (state->setup->pack_mode)
    {
    case KAN_RESOURCE_BUILD_PACK_MODE_NONE:
        KAN_ASSERT (false)
        break;

    case KAN_RESOURCE_BUILD_PACK_MODE_REGULAR:
        // No need for additional setup.
        break;

    case KAN_RESOURCE_BUILD_PACK_MODE_INTERNED:
        interned_string_registry = kan_serialization_interned_string_registry_create_empty ();
        break;
    }

    KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG, "[Target \"%s\"] Going to pack %lu resources.", target->name,
             (unsigned long) entries_to_pack.size)

    for (kan_loop_size_t index = 0u; index < entries_to_pack.size; ++index)
    {
        struct resource_entry_t *entry = ((struct resource_entry_t **) entries_to_pack.data)[index];
        KAN_LOG (resource_pipeline_build, KAN_LOG_DEBUG,
                 "[Target \"%s\"] (%lu/%lu) Adding entry \"%s\" of type \"%s\" to pack.", target->name,
                 (unsigned long) (index + 1u), (unsigned long) entries_to_pack.size, entry->name, entry->type->name)

        if (KAN_HANDLE_IS_VALID (interned_string_registry))
        {
            void *loaded_data = load_resource_entry_data (state, entry);
            if (!entry)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                         "[Target \"%s\"] Failed to load resource \"%s\" of type \"%s\" in order to do string "
                         "interning and pack it.",
                         target->name, entry->name, entry->type->name)

                target->pack_step_successful = false;
                return;
            }

            CUSHION_DEFER
            {
                if (entry->type->shutdown)
                {
                    entry->type->shutdown (entry->type->functor_user_data, loaded_data);
                }

                kan_free_general (entry->allocation_group, loaded_data, entry->type->size);
            }

            pack_fill_internal_path (entry, &path_container);
            struct kan_stream_t *entry_stream =
                kan_virtual_file_system_read_only_pack_builder_add_streamed (pack_builder, path_container.path);

            if (!entry_stream)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                         "[Target \"%s\"] Failed to add resource \"%s\" of type \"%s\" to pack.", target->name,
                         entry->name, entry->type->name)

                target->pack_step_successful = false;
                return;
            }

            CUSHION_DEFER { entry_stream->operations->close (entry_stream); }
            if (!kan_serialization_binary_write_type_header (entry_stream, entry->type->name, interned_string_registry))
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                         "[Target \"%s\"] Failed to add resource \"%s\" of type \"%s\" due to failure while writing "
                         "type header.",
                         target->name, entry->name, entry->type->name)

                target->pack_step_successful = false;
                return;
            }

            kan_serialization_binary_writer_t writer = kan_serialization_binary_writer_create (
                entry_stream, loaded_data, entry->type->name, state->binary_script_storage, interned_string_registry);
            CUSHION_DEFER { kan_serialization_binary_writer_destroy (writer); }

            enum kan_serialization_state_t serialization_state;
            while ((serialization_state = kan_serialization_binary_writer_step (writer)) ==
                   KAN_SERIALIZATION_IN_PROGRESS)
            {
            }

            if (serialization_state == KAN_SERIALIZATION_FAILED)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                         "[Target \"%s\"] Failed to add resource \"%s\" of type \"%s\" due to serialization error "
                         "while resaving with string registry.",
                         target->name, entry->name, entry->type->name)

                target->pack_step_successful = false;
                return;
            }
        }
        else
        {
            struct kan_stream_t *entry_stream = NULL;
            switch (entry->class)
            {
            case RESOURCE_PRODUCTION_CLASS_RAW:
                append_entry_target_location_to_path_container (entry, DEPLOYMENT_STEP_TARGET_LOCATION_DEPLOY,
                                                                &path_container);
                entry_stream = kan_direct_file_stream_open_for_read (path_container.path, true);
                break;

            case RESOURCE_PRODUCTION_CLASS_PRIMARY:
            case RESOURCE_PRODUCTION_CLASS_SECONDARY:
                entry_stream = kan_direct_file_stream_open_for_read (entry->current_file_location, true);
                break;
            }

            if (!entry_stream)
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                         "[Target \"%s\"] Failed to open input stream to resource \"%s\" of type \"%s\" in order to "
                         "pack it.",
                         target->name, entry->name, entry->type->name)

                target->pack_step_successful = false;
                return;
            }

            pack_fill_internal_path (entry, &path_container);
            if (!kan_virtual_file_system_read_only_pack_builder_add (pack_builder, entry_stream, path_container.path))
            {
                KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                         "[Target \"%s\"] Failed to add resource \"%s\" of type \"%s\" to pack.", target->name,
                         entry->name, entry->type->name)
                target->pack_step_successful = false;
                return;
            }
        }
    }

    struct kan_resource_index_t resource_index;
    kan_resource_index_init (&resource_index);
    kan_dynamic_array_set_capacity (&resource_index.containers, entry_types_count);
    CUSHION_DEFER { kan_resource_index_shutdown (&resource_index); }

    const struct kan_reflection_struct_t *last_addition_type = NULL;
    struct kan_resource_index_container_t *last_addition_container = NULL;

    for (kan_loop_size_t index = 0u; index < entries_to_pack.size; ++index)
    {
        struct resource_entry_t *entry = ((struct resource_entry_t **) entries_to_pack.data)[index];

        // Entries must be sorted by types first, so we do not need to search anything.
        if (last_addition_type != entry->type)
        {
            last_addition_type = entry->type;
            last_addition_container = kan_dynamic_array_add_last (&resource_index.containers);

            kan_resource_index_container_init (last_addition_container);
            last_addition_container->type = entry->type->name;
            kan_dynamic_array_set_capacity (&last_addition_container->items,
                                            KAN_RESOURCE_PIPELINE_BUILD_PACK_INDEX_ITEM_CAPACITY);
        }

        struct kan_resource_index_item_t *item = kan_dynamic_array_add_last (&last_addition_container->items);
        if (!item)
        {
            kan_dynamic_array_set_capacity (&last_addition_container->items, last_addition_container->items.size * 2u);
            item = kan_dynamic_array_add_last (&last_addition_container->items);
        }

        kan_resource_index_item_init (item);
        item->name = entry->name;

        pack_fill_internal_path (entry, &path_container);
        item->path = kan_allocate_general (kan_resource_index_get_allocation_group (), path_container.length + 1u,
                                           alignof (char));
        memcpy (item->path, path_container.path, path_container.length + 1u);
    }

    // In scope to always close the addition stream properly.
    {
        struct kan_stream_t *index_stream =
            kan_virtual_file_system_read_only_pack_builder_add_streamed (pack_builder, KAN_RESOURCE_INDEX_DEFAULT_NAME);

        if (!index_stream)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR, "[Target \"%s\"] Failed to add resource index to pack.",
                     target->name)
            target->pack_step_successful = false;
            return;
        }

        CUSHION_DEFER { index_stream->operations->close (index_stream); }
        kan_serialization_binary_writer_t writer = kan_serialization_binary_writer_create (
            index_stream, &resource_index, KAN_STATIC_INTERNED_ID_GET (kan_resource_index_t),
            state->binary_script_storage, interned_string_registry);
        CUSHION_DEFER { kan_serialization_binary_writer_destroy (writer); }

        enum kan_serialization_state_t serialization_state;
        while ((serialization_state = kan_serialization_binary_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
        {
        }

        if (serialization_state == KAN_SERIALIZATION_FAILED)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to add resource index to pack due to serialization error.", target->name)
            target->pack_step_successful = false;
            return;
        }
    }

    // If registry exists, it must be packed after the index as index might add strings to it.
    if (KAN_HANDLE_IS_VALID (interned_string_registry))
    {
        struct kan_stream_t *registry_stream = kan_virtual_file_system_read_only_pack_builder_add_streamed (
            pack_builder, KAN_RESOURCE_INDEX_ACCOMPANYING_STRING_REGISTRY_DEFAULT_NAME);

        if (!registry_stream)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to add interned string registry to pack.", target->name)

            target->pack_step_successful = false;
            return;
        }

        CUSHION_DEFER { registry_stream->operations->close (registry_stream); }
        kan_serialization_interned_string_registry_writer_t writer =
            kan_serialization_interned_string_registry_writer_create (registry_stream, interned_string_registry);
        CUSHION_DEFER { kan_serialization_interned_string_registry_writer_destroy (writer); }

        enum kan_serialization_state_t serialization_state;
        while ((serialization_state = kan_serialization_interned_string_registry_writer_step (writer)) ==
               KAN_SERIALIZATION_IN_PROGRESS)
        {
        }

        if (serialization_state == KAN_SERIALIZATION_FAILED)
        {
            KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR,
                     "[Target \"%s\"] Failed to add interned string registry to pack due to serialization error.",
                     target->name)

            target->pack_step_successful = false;
            return;
        }
    }

    if (!kan_virtual_file_system_read_only_pack_builder_finalize (pack_builder))
    {
        KAN_LOG (resource_pipeline_build, KAN_LOG_ERROR, "[Target \"%s\"] Failed to finalize pack building procedure.",
                 target->name)
        target->pack_step_successful = false;
    }
}

static enum kan_resource_build_result_t execute_pack (struct build_state_t *state)
{
    struct target_t *target = state->targets_first;
    kan_cpu_job_t job = kan_cpu_job_create ();

    while (target)
    {
        CUSHION_DEFER { target = target->next; }
        if (!target->marked_for_build)
        {
            continue;
        }

        // There is not that many targets, so we can just post tasks one by one instead of using task list.
        kan_cpu_job_dispatch_task (job, (struct kan_cpu_task_t) {
                                            .function = execute_pack_for_target,
                                            .user_data = (kan_functor_user_data_t) target,
                                            .profiler_section = kan_cpu_section_get (target->name),
                                        });
    }

    kan_cpu_job_release (job);
    kan_cpu_job_wait (job);

    bool successful = true;
    target = state->targets_first;

    while (target)
    {
        successful &= target->pack_step_successful;
        target = target->next;
    }

    return successful ? KAN_RESOURCE_BUILD_RESULT_SUCCESS : KAN_RESOURCE_BUILD_RESULT_ERROR_PACK_FAILED;
}

// Build procedure main implementation section.

enum kan_resource_build_result_t kan_resource_build (struct kan_resource_build_setup_t *setup)
{
    ensure_statics_initialized ();
    KAN_CPU_SCOPED_STATIC_SECTION (kan_resource_build)

    enum kan_resource_build_result_t result = KAN_RESOURCE_BUILD_RESULT_SUCCESS;
    kan_log_category_set_verbosity (kan_log_category_get ("resource_pipeline_build"), setup->log_verbosity);

    struct build_state_t state;
    build_state_init (&state, setup);
    CUSHION_DEFER { build_state_shutdown (&state); }

#define CHECKED_STEP(NAME)                                                                                             \
    {                                                                                                                  \
        KAN_CPU_SCOPED_STATIC_SECTION (NAME)                                                                           \
        const kan_time_size_t start = kan_precise_time_get_elapsed_nanoseconds ();                                     \
                                                                                                                       \
        CUSHION_DEFER                                                                                                  \
        {                                                                                                              \
            const kan_time_size_t end = kan_precise_time_get_elapsed_nanoseconds ();                                   \
            KAN_LOG (resource_pipeline_build, KAN_LOG_INFO, "Step \"%s\" done in %.3f ms.", #NAME,                     \
                     1e-6f * (float) (end - start))                                                                    \
        }                                                                                                              \
                                                                                                                       \
        result = NAME (&state);                                                                                        \
        if (result != KAN_RESOURCE_BUILD_RESULT_SUCCESS)                                                               \
        {                                                                                                              \
            return result;                                                                                             \
        }                                                                                                              \
    }

    CHECKED_STEP (create_targets)
    CHECKED_STEP (link_visible_targets)
    CHECKED_STEP (linearize_visible_targets)
    CHECKED_STEP (load_platform_configuration)
    CHECKED_STEP (load_resource_log_if_exists)
    CHECKED_STEP (instantiate_initial_resource_log)
    CHECKED_STEP (scan_for_raw_resources)
    CHECKED_STEP (execute_build)

    if (setup->pack_mode != KAN_RESOURCE_BUILD_PACK_MODE_NONE)
    {
        CHECKED_STEP (execute_pack)
    }

#undef CHECKED_STEP
    return result;
}
