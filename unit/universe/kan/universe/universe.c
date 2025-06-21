#define _CRT_SECURE_NO_WARNINGS

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <kan/api_common/min_max.h>
#include <kan/container/hash_storage.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/cpu_dispatch/task.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/universe/universe.h>

KAN_LOG_DEFINE_CATEGORY (universe);
KAN_LOG_DEFINE_CATEGORY (universe_api_scan);
KAN_LOG_DEFINE_CATEGORY (universe_migration);
KAN_LOG_DEFINE_CATEGORY (universe_automation);

struct scheduler_api_t
{
    const struct kan_reflection_struct_t *type;
    const struct kan_reflection_function_t *deploy;
    const struct kan_reflection_function_t *execute;
    const struct kan_reflection_function_t *undeploy;
};

struct scheduler_api_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
    struct scheduler_api_t api;
};

struct mutator_api_t
{
    const struct kan_reflection_struct_t *type;
    const struct kan_reflection_function_t *deploy;
    const struct kan_reflection_function_t *execute;
    const struct kan_reflection_function_t *undeploy;
};

struct mutator_api_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
    struct mutator_api_t api;
};

struct mutator_t
{
    kan_interned_string_t name;
    struct mutator_api_t *api;
    kan_bool_t from_group;
    kan_bool_t found_in_groups;
    kan_bool_t added_during_migration;
    void *state;
    kan_allocation_group_t state_allocation_group;
};

struct pipeline_t
{
    kan_interned_string_t name;

    union
    {
        kan_workflow_graph_builder_t graph_builder;
        kan_workflow_graph_t graph;
    };

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct mutator_t)
    struct kan_dynamic_array_t mutators;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t used_groups;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_universe_world_checkpoint_dependency_t)
    struct kan_dynamic_array_t checkpoint_dependencies;
};

struct world_configuration_t
{
    kan_interned_string_t name;
    const struct kan_reflection_struct_t *type;
    void *data;
};

struct world_t
{
    kan_repository_t repository;
    kan_interned_string_t name;
    struct world_t *parent;

    kan_interned_string_t scheduler_name;
    struct scheduler_api_t *scheduler_api;
    void *scheduler_state;
    kan_allocation_group_t scheduler_state_allocation_group;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct pipeline_t)
    struct kan_dynamic_array_t pipelines;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct world_configuration_t)
    struct kan_dynamic_array_t configuration;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct world_t *)
    struct kan_dynamic_array_t children;
};

struct group_multi_map_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t group_name;
    kan_interned_string_t mutator;
};

struct universe_t
{
    kan_reflection_registry_t reflection_registry;
    kan_context_t context;
    struct world_t *root_world;

    kan_allocation_group_t main_allocation_group;
    kan_allocation_group_t worlds_allocation_group;
    kan_allocation_group_t api_allocation_group;
    kan_allocation_group_t configuration_allocation_group;
    kan_allocation_group_t schedulers_allocation_group;
    kan_allocation_group_t mutators_allocation_group;

    struct kan_hash_storage_t scheduler_api_storage;
    struct kan_hash_storage_t mutator_api_storage;
    struct kan_hash_storage_t group_multi_map_storage;

    struct kan_dynamic_array_t environment_tags;
    kan_cpu_section_t update_section;
};

enum query_type_t
{
    QUERY_TYPE_SINGLETON_READ = 0u,
    QUERY_TYPE_SINGLETON_WRITE,
    QUERY_TYPE_INDEXED_INSERT,
    QUERY_TYPE_INDEXED_SEQUENCE_READ,
    QUERY_TYPE_INDEXED_SEQUENCE_UPDATE,
    QUERY_TYPE_INDEXED_SEQUENCE_DELETE,
    QUERY_TYPE_INDEXED_SEQUENCE_WRITE,
    QUERY_TYPE_INDEXED_VALUE_READ,
    QUERY_TYPE_INDEXED_VALUE_UPDATE,
    QUERY_TYPE_INDEXED_VALUE_DELETE,
    QUERY_TYPE_INDEXED_VALUE_WRITE,
    QUERY_TYPE_INDEXED_SIGNAL_READ,
    QUERY_TYPE_INDEXED_SIGNAL_UPDATE,
    QUERY_TYPE_INDEXED_SIGNAL_DELETE,
    QUERY_TYPE_INDEXED_SIGNAL_WRITE,
    QUERY_TYPE_INDEXED_INTERVAL_READ,
    QUERY_TYPE_INDEXED_INTERVAL_UPDATE,
    QUERY_TYPE_INDEXED_INTERVAL_DELETE,
    QUERY_TYPE_INDEXED_INTERVAL_WRITE,
    QUERY_TYPE_INDEXED_SPACE_READ,
    QUERY_TYPE_INDEXED_SPACE_UPDATE,
    QUERY_TYPE_INDEXED_SPACE_DELETE,
    QUERY_TYPE_INDEXED_SPACE_WRITE,
    QUERY_TYPE_EVENT_INSERT,
    QUERY_TYPE_EVENT_FETCH,
};

static kan_bool_t statics_initialized = KAN_FALSE;
static kan_interned_string_t interned_kan_repository_singleton_read_query_t;
static kan_interned_string_t interned_kan_repository_singleton_write_query_t;
static kan_interned_string_t interned_kan_repository_indexed_insert_query_t;
static kan_interned_string_t interned_kan_repository_indexed_sequence_read_query_t;
static kan_interned_string_t interned_kan_repository_indexed_sequence_update_query_t;
static kan_interned_string_t interned_kan_repository_indexed_sequence_delete_query_t;
static kan_interned_string_t interned_kan_repository_indexed_sequence_write_query_t;
static kan_interned_string_t interned_kan_repository_indexed_value_read_query_t;
static kan_interned_string_t interned_kan_repository_indexed_value_update_query_t;
static kan_interned_string_t interned_kan_repository_indexed_value_delete_query_t;
static kan_interned_string_t interned_kan_repository_indexed_value_write_query_t;
static kan_interned_string_t interned_kan_repository_indexed_signal_read_query_t;
static kan_interned_string_t interned_kan_repository_indexed_signal_update_query_t;
static kan_interned_string_t interned_kan_repository_indexed_signal_delete_query_t;
static kan_interned_string_t interned_kan_repository_indexed_signal_write_query_t;
static kan_interned_string_t interned_kan_repository_indexed_interval_read_query_t;
static kan_interned_string_t interned_kan_repository_indexed_interval_update_query_t;
static kan_interned_string_t interned_kan_repository_indexed_interval_delete_query_t;
static kan_interned_string_t interned_kan_repository_indexed_interval_write_query_t;
static kan_interned_string_t interned_kan_repository_indexed_space_read_query_t;
static kan_interned_string_t interned_kan_repository_indexed_space_update_query_t;
static kan_interned_string_t interned_kan_repository_indexed_space_delete_query_t;
static kan_interned_string_t interned_kan_repository_indexed_space_write_query_t;
static kan_interned_string_t interned_kan_repository_event_insert_query_t;
static kan_interned_string_t interned_kan_repository_event_fetch_query_t;
static kan_interned_string_t interned_kan_repository_meta_automatic_cascade_deletion_t;
static kan_interned_string_t interned_kan_universe_space_automated_lifetime_query_meta_t;
static kan_interned_string_t interned_kan_universe_mutator_group_meta_t;
static kan_interned_string_t interned_kan_universe_scheduler_interface_run_pipeline;
static kan_interned_string_t interned_kan_universe_scheduler_interface_update_child;
static kan_interned_string_t interned_kan_universe_scheduler_interface_update_all_children;

static kan_cpu_section_t section_deploy_scheduler;
static kan_cpu_section_t section_deploy_mutator;
static kan_cpu_section_t section_migrate_configuration;
static kan_cpu_section_t section_undeploy_and_migrate_scheduler;
static kan_cpu_section_t section_undeploy_and_migrate_mutator;
static kan_cpu_section_t section_finish_pipeline_deployment;

static void ensure_statics_initialized (void)
{
    if (statics_initialized)
    {
        return;
    }

    interned_kan_repository_singleton_read_query_t = kan_string_intern ("kan_repository_singleton_read_query_t");
    interned_kan_repository_singleton_write_query_t = kan_string_intern ("kan_repository_singleton_write_query_t");
    interned_kan_repository_indexed_insert_query_t = kan_string_intern ("kan_repository_indexed_insert_query_t");

    interned_kan_repository_indexed_sequence_read_query_t =
        kan_string_intern ("kan_repository_indexed_sequence_read_query_t");
    interned_kan_repository_indexed_sequence_update_query_t =
        kan_string_intern ("kan_repository_indexed_sequence_update_query_t");
    interned_kan_repository_indexed_sequence_delete_query_t =
        kan_string_intern ("kan_repository_indexed_sequence_delete_query_t");
    interned_kan_repository_indexed_sequence_write_query_t =
        kan_string_intern ("kan_repository_indexed_sequence_write_query_t");

    interned_kan_repository_indexed_value_read_query_t =
        kan_string_intern ("kan_repository_indexed_value_read_query_t");
    interned_kan_repository_indexed_value_update_query_t =
        kan_string_intern ("kan_repository_indexed_value_update_query_t");
    interned_kan_repository_indexed_value_delete_query_t =
        kan_string_intern ("kan_repository_indexed_value_delete_query_t");
    interned_kan_repository_indexed_value_write_query_t =
        kan_string_intern ("kan_repository_indexed_value_write_query_t");

    interned_kan_repository_indexed_signal_read_query_t =
        kan_string_intern ("kan_repository_indexed_signal_read_query_t");
    interned_kan_repository_indexed_signal_update_query_t =
        kan_string_intern ("kan_repository_indexed_signal_update_query_t");
    interned_kan_repository_indexed_signal_delete_query_t =
        kan_string_intern ("kan_repository_indexed_signal_delete_query_t");
    interned_kan_repository_indexed_signal_write_query_t =
        kan_string_intern ("kan_repository_indexed_signal_write_query_t");

    interned_kan_repository_indexed_interval_read_query_t =
        kan_string_intern ("kan_repository_indexed_interval_read_query_t");
    interned_kan_repository_indexed_interval_update_query_t =
        kan_string_intern ("kan_repository_indexed_interval_update_query_t");
    interned_kan_repository_indexed_interval_delete_query_t =
        kan_string_intern ("kan_repository_indexed_interval_delete_query_t");
    interned_kan_repository_indexed_interval_write_query_t =
        kan_string_intern ("kan_repository_indexed_interval_write_query_t");

    interned_kan_repository_indexed_space_read_query_t =
        kan_string_intern ("kan_repository_indexed_space_read_query_t");
    interned_kan_repository_indexed_space_update_query_t =
        kan_string_intern ("kan_repository_indexed_space_update_query_t");
    interned_kan_repository_indexed_space_delete_query_t =
        kan_string_intern ("kan_repository_indexed_space_delete_query_t");
    interned_kan_repository_indexed_space_write_query_t =
        kan_string_intern ("kan_repository_indexed_space_write_query_t");

    interned_kan_repository_event_insert_query_t = kan_string_intern ("kan_repository_event_insert_query_t");
    interned_kan_repository_event_fetch_query_t = kan_string_intern ("kan_repository_event_fetch_query_t");

    interned_kan_repository_meta_automatic_cascade_deletion_t =
        kan_string_intern ("kan_repository_meta_automatic_cascade_deletion_t");
    interned_kan_universe_space_automated_lifetime_query_meta_t =
        kan_string_intern ("kan_universe_space_automated_lifetime_query_meta_t");
    interned_kan_universe_mutator_group_meta_t = kan_string_intern ("kan_universe_mutator_group_meta_t");

    interned_kan_universe_scheduler_interface_run_pipeline =
        kan_string_intern ("kan_universe_scheduler_interface_run_pipeline");
    interned_kan_universe_scheduler_interface_update_child =
        kan_string_intern ("kan_universe_scheduler_interface_update_child");
    interned_kan_universe_scheduler_interface_update_all_children =
        kan_string_intern ("kan_universe_scheduler_interface_update_all_children");

    section_deploy_scheduler = kan_cpu_section_get ("deploy_scheduler");
    section_deploy_mutator = kan_cpu_section_get ("deploy_mutator");
    section_migrate_configuration = kan_cpu_section_get ("migrate_configuration");
    section_undeploy_and_migrate_scheduler = kan_cpu_section_get ("undeploy_and_migrate_scheduler");
    section_undeploy_and_migrate_mutator = kan_cpu_section_get ("undeploy_and_migrate_mutator");
    section_finish_pipeline_deployment = kan_cpu_section_get ("finish_pipeline_deployment");

    statics_initialized = KAN_TRUE;
}

struct automated_lifetime_query_check_result_t
{
    kan_bool_t is_automated_lifetime_query;
    enum query_type_t query_type;
    const char *body_start;
};

static struct automated_lifetime_query_check_result_t is_automated_lifetime_query_field (
    struct kan_reflection_field_t *field)
{
    if (field->archetype != KAN_REFLECTION_ARCHETYPE_STRUCT)
    {
        return (struct automated_lifetime_query_check_result_t) {KAN_FALSE, QUERY_TYPE_SINGLETON_READ, NULL};
    }

    if (field->archetype_struct.type_name == interned_kan_repository_singleton_read_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "read__", 6u) == 0,
            QUERY_TYPE_SINGLETON_READ,
            field->name + 6u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_singleton_write_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "write__", 7u) == 0,
            QUERY_TYPE_SINGLETON_WRITE,
            field->name + 7u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_insert_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "insert__", 8u) == 0,
            QUERY_TYPE_INDEXED_INSERT,
            field->name + 8u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_sequence_read_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "read_sequence__", 15u) == 0,
            QUERY_TYPE_INDEXED_SEQUENCE_READ,
            field->name + 15u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_sequence_update_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "update_sequence__", 17u) == 0,
            QUERY_TYPE_INDEXED_SEQUENCE_UPDATE,
            field->name + 17u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_sequence_delete_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "delete_sequence__", 17u) == 0,
            QUERY_TYPE_INDEXED_SEQUENCE_DELETE,
            field->name + 17u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_sequence_write_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "write_sequence__", 16u) == 0,
            QUERY_TYPE_INDEXED_SEQUENCE_WRITE,
            field->name + 16u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_value_read_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "read_value__", 12u) == 0,
            QUERY_TYPE_INDEXED_VALUE_READ,
            field->name + 12u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_value_update_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "update_value__", 14u) == 0,
            QUERY_TYPE_INDEXED_VALUE_UPDATE,
            field->name + 14u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_value_delete_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "delete_value__", 14u) == 0,
            QUERY_TYPE_INDEXED_VALUE_DELETE,
            field->name + 14u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_value_write_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "write_value__", 13u) == 0,
            QUERY_TYPE_INDEXED_VALUE_WRITE,
            field->name + 13u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_signal_read_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "read_signal__", 13u) == 0,
            QUERY_TYPE_INDEXED_SIGNAL_READ,
            field->name + 13u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_signal_update_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "update_signal__", 15u) == 0,
            QUERY_TYPE_INDEXED_SIGNAL_UPDATE,
            field->name + 15u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_signal_delete_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "delete_signal__", 15u) == 0,
            QUERY_TYPE_INDEXED_SIGNAL_DELETE,
            field->name + 15u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_signal_write_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "write_signal__", 14u) == 0,
            QUERY_TYPE_INDEXED_SIGNAL_WRITE,
            field->name + 14u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_interval_read_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "read_interval__", 15u) == 0,
            QUERY_TYPE_INDEXED_INTERVAL_READ,
            field->name + 15u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_interval_update_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "update_interval__", 17u) == 0,
            QUERY_TYPE_INDEXED_INTERVAL_UPDATE,
            field->name + 17u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_interval_delete_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "delete_interval__", 17u) == 0,
            QUERY_TYPE_INDEXED_INTERVAL_DELETE,
            field->name + 17u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_interval_write_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "write_interval__", 16u) == 0,
            QUERY_TYPE_INDEXED_INTERVAL_WRITE,
            field->name + 16u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_space_read_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "read_space__", 12u) == 0,
            QUERY_TYPE_INDEXED_SPACE_READ,
            field->name + 12u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_space_update_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "update_space__", 14u) == 0,
            QUERY_TYPE_INDEXED_SPACE_UPDATE,
            field->name + 14u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_space_delete_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "delete_space__", 14u) == 0,
            QUERY_TYPE_INDEXED_SPACE_DELETE,
            field->name + 14u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_indexed_space_write_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "write_space__", 13u) == 0,
            QUERY_TYPE_INDEXED_SPACE_WRITE,
            field->name + 13u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_event_insert_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "insert__", 8u) == 0,
            QUERY_TYPE_EVENT_INSERT,
            field->name + 8u,
        };
    }
    else if (field->archetype_struct.type_name == interned_kan_repository_event_fetch_query_t)
    {
        return (struct automated_lifetime_query_check_result_t) {
            strncmp (field->name, "fetch__", 7u) == 0,
            QUERY_TYPE_EVENT_FETCH,
            field->name + 7u,
        };
    }

    return (struct automated_lifetime_query_check_result_t) {KAN_FALSE, QUERY_TYPE_SINGLETON_READ, NULL};
}

struct char_sequence_t
{
    const char *begin;
    const char *end;
};

static kan_loop_size_t split_automated_query_name (const char *body_start, struct char_sequence_t *output)
{
    kan_loop_size_t count = 0u;
    while (*body_start != '\0')
    {
        output->begin = body_start;
        while (*body_start != '\0')
        {
            if (*body_start == '_' && *(body_start + 1u) == '_')
            {
                break;
            }

            ++body_start;
        }

        output->end = body_start;
        if (*body_start != '\0')
        {
            body_start += 2u;
        }

        if (output->begin != output->end)
        {
            ++output;
            ++count;
        }

        if (count >= KAN_UNIVERSE_MAX_AUTOMATED_QUERY_NAME_PARTS)
        {
            break;
        }
    }

    return count;
}

static void register_cascade_deletion (kan_reflection_registry_t registry,
                                       kan_workflow_graph_node_t workflow_node,
                                       kan_interned_string_t type_name)
{
    struct kan_reflection_struct_meta_iterator_t iterator = kan_reflection_registry_query_struct_meta (
        registry, type_name, interned_kan_repository_meta_automatic_cascade_deletion_t);

    while (KAN_TRUE)
    {
        const struct kan_repository_meta_automatic_cascade_deletion_t *meta =
            (const struct kan_repository_meta_automatic_cascade_deletion_t *) kan_reflection_struct_meta_iterator_get (
                &iterator);

        if (!meta)
        {
            break;
        }

        kan_workflow_graph_node_write_resource (workflow_node, kan_string_intern (meta->child_type_name));
        kan_reflection_struct_meta_iterator_next (&iterator);
    }
}

static void deploy_automated_lifetime_queries (kan_reflection_registry_t registry,
                                               struct world_t *world,
                                               kan_workflow_graph_node_t workflow_node,
                                               kan_interned_string_t type_name,
                                               void *fragment)
{
    struct kan_reflection_struct_t *struct_data =
        (struct kan_reflection_struct_t *) kan_reflection_registry_query_struct (registry, type_name);
    KAN_ASSERT (struct_data)
    kan_repository_t repository = world->repository;

    for (kan_loop_size_t field_index = 0u; field_index < struct_data->fields_count; ++field_index)
    {
        struct kan_reflection_field_t *field = &struct_data->fields[field_index];
        uint8_t *position = ((uint8_t *) fragment) + field->offset;
        struct automated_lifetime_query_check_result_t check_result = is_automated_lifetime_query_field (field);

        if (check_result.is_automated_lifetime_query)
        {
            if (0u != *(uintptr_t *) position)
            {
                // Query beginning is not zeroed, therefore it should've been initialized by migration.
                // We use zero-filling to detect such migrated or initialized through other way queries.
                continue;
            }

            struct char_sequence_t name_parts[KAN_UNIVERSE_MAX_AUTOMATED_QUERY_NAME_PARTS];
            const kan_loop_size_t name_parts_count = split_automated_query_name (check_result.body_start, name_parts);

            if (name_parts_count == 0u)
            {
                KAN_LOG (universe_automation, KAN_LOG_ERROR,
                         "Tried to auto-deploy query from field \"%s\", but it has no type info in name.", field->name)
                continue;
            }

            char queried_type_name_mutable[KAN_UNIVERSE_MAX_AUTOMATED_QUERY_TYPE_LENGTH];
            KAN_ASSERT (name_parts[0u].end - name_parts[0u].begin + 2u < KAN_UNIVERSE_MAX_AUTOMATED_QUERY_TYPE_LENGTH)
            strncpy (queried_type_name_mutable, name_parts[0u].begin, name_parts[0u].end - name_parts[0u].begin);
            const kan_instance_size_t length = (kan_instance_size_t) (name_parts[0u].end - name_parts[0u].begin);
            queried_type_name_mutable[length] = '\0';

            kan_interned_string_t queried_type_name = kan_string_intern (queried_type_name_mutable);
            if (!kan_reflection_registry_query_struct (registry, queried_type_name))
            {
                queried_type_name_mutable[length] = '_';
                queried_type_name_mutable[length + 1u] = 't';
                queried_type_name_mutable[length + 2u] = '\0';
                queried_type_name = kan_string_intern (queried_type_name_mutable);

                if (!kan_reflection_registry_query_struct (registry, queried_type_name))
                {
                    KAN_LOG (universe_automation, KAN_LOG_ERROR,
                             "Tried to auto-deploy query from field \"%s\", but its queried type name does not exist "
                             "neither as given neither with \"_t\" suffix.",
                             field->name)
                    continue;
                }
            }

#define SKIP_IF_MORE_THAN_ONE_PART                                                                                     \
    if (name_parts_count != 1u)                                                                                        \
    {                                                                                                                  \
        KAN_LOG (universe_automation, KAN_LOG_ERROR,                                                                   \
                 "Tried to auto-deploy query from field \"%s\", but its name has additional parts after "              \
                 "queried type name, which is not expected for its query type.",                                       \
                 field->name)                                                                                          \
        break;                                                                                                         \
    }

            switch (check_result.query_type)
            {
            case QUERY_TYPE_SINGLETON_READ:
            {
                SKIP_IF_MORE_THAN_ONE_PART
                kan_repository_singleton_storage_t storage =
                    kan_repository_singleton_storage_open (repository, queried_type_name);

                kan_repository_singleton_read_query_init ((struct kan_repository_singleton_read_query_t *) position,
                                                          storage);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_read_resource (workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_SINGLETON_WRITE:
            {
                SKIP_IF_MORE_THAN_ONE_PART
                kan_repository_singleton_storage_t storage =
                    kan_repository_singleton_storage_open (repository, queried_type_name);

                kan_repository_singleton_write_query_init ((struct kan_repository_singleton_write_query_t *) position,
                                                           storage);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_INSERT:
            {
                SKIP_IF_MORE_THAN_ONE_PART
                kan_repository_indexed_storage_t storage =
                    kan_repository_indexed_storage_open (repository, queried_type_name);

                kan_repository_indexed_insert_query_init ((struct kan_repository_indexed_insert_query_t *) position,
                                                          storage);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_insert_resource (workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_SEQUENCE_READ:
            {
                SKIP_IF_MORE_THAN_ONE_PART
                kan_repository_indexed_storage_t storage =
                    kan_repository_indexed_storage_open (repository, queried_type_name);

                kan_repository_indexed_sequence_read_query_init (
                    (struct kan_repository_indexed_sequence_read_query_t *) position, storage);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_read_resource (workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_SEQUENCE_UPDATE:
            {
                SKIP_IF_MORE_THAN_ONE_PART
                kan_repository_indexed_storage_t storage =
                    kan_repository_indexed_storage_open (repository, queried_type_name);

                kan_repository_indexed_sequence_update_query_init (
                    (struct kan_repository_indexed_sequence_update_query_t *) position, storage);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_SEQUENCE_DELETE:
            {
                SKIP_IF_MORE_THAN_ONE_PART
                kan_repository_indexed_storage_t storage =
                    kan_repository_indexed_storage_open (repository, queried_type_name);

                kan_repository_indexed_sequence_delete_query_init (
                    (struct kan_repository_indexed_sequence_delete_query_t *) position, storage);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                    register_cascade_deletion (registry, workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_SEQUENCE_WRITE:
            {
                SKIP_IF_MORE_THAN_ONE_PART
                kan_repository_indexed_storage_t storage =
                    kan_repository_indexed_storage_open (repository, queried_type_name);

                kan_repository_indexed_sequence_write_query_init (
                    (struct kan_repository_indexed_sequence_write_query_t *) position, storage);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                    register_cascade_deletion (registry, workflow_node, queried_type_name);
                }

                break;
            }

#define DEPLOY_INDEXED_VALUE(TYPE)                                                                                     \
    if (name_parts_count == 1u)                                                                                        \
    {                                                                                                                  \
        KAN_LOG (universe_automation, KAN_LOG_ERROR,                                                                   \
                 "Tried to auto-deploy query from field \"%s\", but its name has no indexed value field "              \
                 "path which is required.",                                                                            \
                 field->name)                                                                                          \
        break;                                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    kan_interned_string_t path[KAN_UNIVERSE_MAX_AUTOMATED_QUERY_NAME_PARTS - 1u];                                      \
    for (kan_loop_size_t index = 1u; index < name_parts_count; ++index)                                                \
    {                                                                                                                  \
        path[index - 1u] = kan_char_sequence_intern (name_parts[index].begin, name_parts[index].end);                  \
    }                                                                                                                  \
                                                                                                                       \
    struct kan_repository_field_path_t reflected_path;                                                                 \
    reflected_path.reflection_path_length = name_parts_count - 1u;                                                     \
    reflected_path.reflection_path = path;                                                                             \
                                                                                                                       \
    kan_repository_indexed_storage_t storage = kan_repository_indexed_storage_open (repository, queried_type_name);    \
                                                                                                                       \
    kan_repository_indexed_value_##TYPE##_query_init (                                                                 \
        (struct kan_repository_indexed_value_##TYPE##_query_t *) position, storage, reflected_path)

            case QUERY_TYPE_INDEXED_VALUE_READ:
            {
                DEPLOY_INDEXED_VALUE (read);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_read_resource (workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_VALUE_UPDATE:
            {
                DEPLOY_INDEXED_VALUE (update);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_VALUE_DELETE:
            {
                DEPLOY_INDEXED_VALUE (delete);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                    register_cascade_deletion (registry, workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_VALUE_WRITE:
            {
                DEPLOY_INDEXED_VALUE (write);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                    register_cascade_deletion (registry, workflow_node, queried_type_name);
                }

                break;
            }

#undef DEPLOY_INDEXED_VALUE

#define DEPLOY_INDEXED_SIGNAL(TYPE)                                                                                    \
    if (name_parts_count <= 2u)                                                                                        \
    {                                                                                                                  \
        KAN_LOG (universe_automation, KAN_LOG_ERROR,                                                                   \
                 "Tried to auto-deploy query from field \"%s\", but its name has no indexed value field "              \
                 "path and signal value suffix that are required.",                                                    \
                 field->name)                                                                                          \
        break;                                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    KAN_ASSERT (*name_parts[name_parts_count - 1u].end == '\0')                                                        \
    char *parse_end = NULL;                                                                                            \
    kan_repository_signal_value_t signal_value =                                                                       \
        (kan_repository_signal_value_t) strtoull (name_parts[name_parts_count - 1u].begin, &parse_end, 10);            \
                                                                                                                       \
    if (parse_end != name_parts[name_parts_count - 1u].end &&                                                          \
                                                                                                                       \
        /* Allow `u` suffix as it is common for unsigned values inside macro definitions. */                           \
        (parse_end != name_parts[name_parts_count - 1u].end - 1u || *parse_end != 'u'))                                \
    {                                                                                                                  \
        KAN_LOG (universe_automation, KAN_LOG_ERROR,                                                                   \
                 "Tried to auto-deploy query from field \"%s\", but failed to parse signal value from \"%s\".",        \
                 field->name, name_parts[name_parts_count - 1u].begin)                                                 \
        break;                                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    kan_interned_string_t path[KAN_UNIVERSE_MAX_AUTOMATED_QUERY_NAME_PARTS - 2u];                                      \
    for (kan_loop_size_t index = 1u; index < name_parts_count - 1u; ++index)                                           \
    {                                                                                                                  \
        path[index - 1u] = kan_char_sequence_intern (name_parts[index].begin, name_parts[index].end);                  \
    }                                                                                                                  \
                                                                                                                       \
    struct kan_repository_field_path_t reflected_path;                                                                 \
    reflected_path.reflection_path_length = name_parts_count - 2u;                                                     \
    reflected_path.reflection_path = path;                                                                             \
                                                                                                                       \
    kan_repository_indexed_storage_t storage = kan_repository_indexed_storage_open (repository, queried_type_name);    \
                                                                                                                       \
    kan_repository_indexed_signal_##TYPE##_query_init (                                                                \
        (struct kan_repository_indexed_signal_##TYPE##_query_t *) position, storage, reflected_path, signal_value)

            case QUERY_TYPE_INDEXED_SIGNAL_READ:
            {
                // TODO: Currently automated queries only work with unsigned integer signals which covers most cases.
                //       If we need it in future, we might add interned string support and signed integer support.
                DEPLOY_INDEXED_SIGNAL (read);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_read_resource (workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_SIGNAL_UPDATE:
            {
                DEPLOY_INDEXED_SIGNAL (update);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_SIGNAL_DELETE:
            {
                DEPLOY_INDEXED_SIGNAL (delete);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                    register_cascade_deletion (registry, workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_SIGNAL_WRITE:
            {
                DEPLOY_INDEXED_SIGNAL (write);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                    register_cascade_deletion (registry, workflow_node, queried_type_name);
                }

                break;
            }

#undef DEPLOY_INDEXED_SIGNAL

#define DEPLOY_INDEXED_INTERVAL(TYPE)                                                                                  \
    if (name_parts_count == 1u)                                                                                        \
    {                                                                                                                  \
        KAN_LOG (universe_automation, KAN_LOG_ERROR,                                                                   \
                 "Tried to auto-deploy query from field \"%s\", but its name has no indexed value field "              \
                 "path which is required.",                                                                            \
                 field->name)                                                                                          \
        break;                                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    kan_interned_string_t path[KAN_UNIVERSE_MAX_AUTOMATED_QUERY_NAME_PARTS - 1u];                                      \
    for (kan_loop_size_t index = 1u; index < name_parts_count; ++index)                                                \
    {                                                                                                                  \
        path[index - 1u] = kan_char_sequence_intern (name_parts[index].begin, name_parts[index].end);                  \
    }                                                                                                                  \
                                                                                                                       \
    struct kan_repository_field_path_t reflected_path;                                                                 \
    reflected_path.reflection_path_length = name_parts_count - 1u;                                                     \
    reflected_path.reflection_path = path;                                                                             \
                                                                                                                       \
    kan_repository_indexed_storage_t storage = kan_repository_indexed_storage_open (repository, queried_type_name);    \
                                                                                                                       \
    kan_repository_indexed_interval_##TYPE##_query_init (                                                              \
        (struct kan_repository_indexed_interval_##TYPE##_query_t *) position, storage, reflected_path)

            case QUERY_TYPE_INDEXED_INTERVAL_READ:
            {
                DEPLOY_INDEXED_INTERVAL (read);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_read_resource (workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_INTERVAL_UPDATE:
            {
                DEPLOY_INDEXED_INTERVAL (update);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_INTERVAL_DELETE:
            {
                DEPLOY_INDEXED_INTERVAL (delete);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                    register_cascade_deletion (registry, workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_INTERVAL_WRITE:
            {
                DEPLOY_INDEXED_INTERVAL (write);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                    register_cascade_deletion (registry, workflow_node, queried_type_name);
                }

                break;
            }

#undef DEPLOY_INDEXED_INTERVAL

#define DEPLOY_INDEXED_SPACE(TYPE)                                                                                     \
    if (name_parts_count != 2u)                                                                                        \
    {                                                                                                                  \
        KAN_LOG (universe_automation, KAN_LOG_ERROR,                                                                   \
                 "Tried to auto-deploy query from field \"%s\", but its name has no space borders "                    \
                 "configuration name as suffix which is required.",                                                    \
                 field->name)                                                                                          \
        break;                                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    struct kan_reflection_struct_field_meta_iterator_t meta_iterator =                                                 \
        kan_reflection_registry_query_struct_field_meta (registry, type_name, field->name,                             \
                                                         interned_kan_universe_space_automated_lifetime_query_meta_t); \
                                                                                                                       \
    const struct kan_universe_space_automated_lifetime_query_meta_t *meta =                                            \
        (const struct kan_universe_space_automated_lifetime_query_meta_t *)                                            \
            kan_reflection_struct_field_meta_iterator_get (&meta_iterator);                                            \
                                                                                                                       \
    if (!meta)                                                                                                         \
    {                                                                                                                  \
        KAN_LOG (universe_automation, KAN_LOG_ERROR,                                                                   \
                 "Tried to auto-deploy query from field \"%s\", but it has no meta for space min-max "                 \
                 "selection (kan_universe_space_automated_lifetime_query_meta_t).",                                    \
                 field->name)                                                                                          \
        break;                                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    const kan_interned_string_t configuration_name =                                                                   \
        kan_char_sequence_intern (name_parts[1u].begin, name_parts[1u].end);                                           \
                                                                                                                       \
    const struct kan_universe_space_configuration_t *configuration =                                                   \
        (const struct kan_universe_space_configuration_t *) kan_universe_world_query_configuration (                   \
            KAN_HANDLE_SET (kan_universe_world_t, world), configuration_name);                                         \
                                                                                                                       \
    if (!configuration)                                                                                                \
    {                                                                                                                  \
        KAN_LOG (universe_automation, KAN_LOG_ERROR,                                                                   \
                 "Tried to auto-deploy query from field \"%s\", but unable to find its space configuration \"%s\".",   \
                 field->name, configuration_name)                                                                      \
        break;                                                                                                         \
    }                                                                                                                  \
                                                                                                                       \
    kan_repository_indexed_storage_t storage = kan_repository_indexed_storage_open (repository, queried_type_name);    \
                                                                                                                       \
    kan_repository_indexed_space_##TYPE##_query_init (                                                                 \
        (struct kan_repository_indexed_space_##TYPE##_query_t *) position, storage, meta->min_path, meta->max_path,    \
        configuration->global_min, configuration->global_max, configuration->leaf_size)

            case QUERY_TYPE_INDEXED_SPACE_READ:
            {
                DEPLOY_INDEXED_SPACE (read);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_read_resource (workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_SPACE_UPDATE:
            {
                DEPLOY_INDEXED_SPACE (update);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_SPACE_DELETE:
            {
                DEPLOY_INDEXED_SPACE (delete);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                    register_cascade_deletion (registry, workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_INDEXED_SPACE_WRITE:
            {
                DEPLOY_INDEXED_SPACE (write);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_write_resource (workflow_node, queried_type_name);
                    register_cascade_deletion (registry, workflow_node, queried_type_name);
                }

                break;
            }

#undef DEPLOY_INDEXED_SPACE

            case QUERY_TYPE_EVENT_INSERT:
            {
                SKIP_IF_MORE_THAN_ONE_PART
                kan_repository_event_storage_t storage =
                    kan_repository_event_storage_open (repository, queried_type_name);

                kan_repository_event_insert_query_init ((struct kan_repository_event_insert_query_t *) position,
                                                        storage);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_insert_resource (workflow_node, queried_type_name);
                }

                break;
            }

            case QUERY_TYPE_EVENT_FETCH:
            {
                SKIP_IF_MORE_THAN_ONE_PART
                kan_repository_event_storage_t storage =
                    kan_repository_event_storage_open (repository, queried_type_name);

                kan_repository_event_fetch_query_init ((struct kan_repository_event_fetch_query_t *) position, storage);

                if (KAN_HANDLE_IS_VALID (workflow_node))
                {
                    kan_workflow_graph_node_read_resource (workflow_node, queried_type_name);
                }

                break;
            }
            }

#undef SKIP_IF_MORE_THAN_ONE_PART
        }
        else if (field->archetype == KAN_REFLECTION_ARCHETYPE_STRUCT)
        {
            deploy_automated_lifetime_queries (registry, world, workflow_node, field->archetype_struct.type_name,
                                               position);
        }
    }
}

static void undeploy_automated_lifetime_queries (kan_reflection_registry_t registry,
                                                 kan_interned_string_t type_name,
                                                 void *fragment)
{
    struct kan_reflection_struct_t *struct_data =
        (struct kan_reflection_struct_t *) kan_reflection_registry_query_struct (registry, type_name);
    KAN_ASSERT (struct_data)

    for (kan_loop_size_t field_index = 0u; field_index < struct_data->fields_count; ++field_index)
    {
        struct kan_reflection_field_t *field = &struct_data->fields[field_index];
        uint8_t *position = ((uint8_t *) fragment) + field->offset;
        struct automated_lifetime_query_check_result_t check_result = is_automated_lifetime_query_field (field);

        if (check_result.is_automated_lifetime_query)
        {
            switch (check_result.query_type)
            {
            case QUERY_TYPE_SINGLETON_READ:
                kan_repository_singleton_read_query_shutdown (
                    (struct kan_repository_singleton_read_query_t *) position);
                break;

            case QUERY_TYPE_SINGLETON_WRITE:
                kan_repository_singleton_write_query_shutdown (
                    (struct kan_repository_singleton_write_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_INSERT:
                kan_repository_indexed_insert_query_shutdown (
                    (struct kan_repository_indexed_insert_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_SEQUENCE_READ:
                kan_repository_indexed_sequence_read_query_shutdown (
                    (struct kan_repository_indexed_sequence_read_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_SEQUENCE_UPDATE:
                kan_repository_indexed_sequence_update_query_shutdown (
                    (struct kan_repository_indexed_sequence_update_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_SEQUENCE_DELETE:
                kan_repository_indexed_sequence_delete_query_shutdown (
                    (struct kan_repository_indexed_sequence_delete_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_SEQUENCE_WRITE:
                kan_repository_indexed_sequence_write_query_shutdown (
                    (struct kan_repository_indexed_sequence_write_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_VALUE_READ:
                kan_repository_indexed_value_read_query_shutdown (
                    (struct kan_repository_indexed_value_read_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_VALUE_UPDATE:
                kan_repository_indexed_value_update_query_shutdown (
                    (struct kan_repository_indexed_value_update_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_VALUE_DELETE:
                kan_repository_indexed_value_delete_query_shutdown (
                    (struct kan_repository_indexed_value_delete_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_VALUE_WRITE:
                kan_repository_indexed_value_write_query_shutdown (
                    (struct kan_repository_indexed_value_write_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_SIGNAL_READ:
                kan_repository_indexed_signal_read_query_shutdown (
                    (struct kan_repository_indexed_signal_read_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_SIGNAL_UPDATE:
                kan_repository_indexed_signal_update_query_shutdown (
                    (struct kan_repository_indexed_signal_update_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_SIGNAL_DELETE:
                kan_repository_indexed_signal_delete_query_shutdown (
                    (struct kan_repository_indexed_signal_delete_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_SIGNAL_WRITE:
                kan_repository_indexed_signal_write_query_shutdown (
                    (struct kan_repository_indexed_signal_write_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_INTERVAL_READ:
                kan_repository_indexed_interval_read_query_shutdown (
                    (struct kan_repository_indexed_interval_read_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_INTERVAL_UPDATE:
                kan_repository_indexed_interval_update_query_shutdown (
                    (struct kan_repository_indexed_interval_update_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_INTERVAL_DELETE:
                kan_repository_indexed_interval_delete_query_shutdown (
                    (struct kan_repository_indexed_interval_delete_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_INTERVAL_WRITE:
                kan_repository_indexed_interval_write_query_shutdown (
                    (struct kan_repository_indexed_interval_write_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_SPACE_READ:
                kan_repository_indexed_space_read_query_shutdown (
                    (struct kan_repository_indexed_space_read_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_SPACE_UPDATE:
                kan_repository_indexed_space_update_query_shutdown (
                    (struct kan_repository_indexed_space_update_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_SPACE_DELETE:
                kan_repository_indexed_space_delete_query_shutdown (
                    (struct kan_repository_indexed_space_delete_query_t *) position);
                break;

            case QUERY_TYPE_INDEXED_SPACE_WRITE:
                kan_repository_indexed_space_write_query_shutdown (
                    (struct kan_repository_indexed_space_write_query_t *) position);
                break;

            case QUERY_TYPE_EVENT_INSERT:
                kan_repository_event_insert_query_shutdown ((struct kan_repository_event_insert_query_t *) position);
                break;

            case QUERY_TYPE_EVENT_FETCH:
                kan_repository_event_fetch_query_shutdown ((struct kan_repository_event_fetch_query_t *) position);
                break;
            }
        }
        else if (field->archetype == KAN_REFLECTION_ARCHETYPE_STRUCT)
        {
            undeploy_automated_lifetime_queries (registry, field->archetype_struct.type_name, position);
        }
    }
}

static struct scheduler_api_node_t *universe_get_scheduler_api (struct universe_t *universe, kan_interned_string_t name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&universe->scheduler_api_storage, KAN_HASH_OBJECT_POINTER (name));
    struct scheduler_api_node_t *node = (struct scheduler_api_node_t *) bucket->first;
    const struct scheduler_api_node_t *node_end =
        (struct scheduler_api_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == name)
        {
            return node;
        }

        node = (struct scheduler_api_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static struct scheduler_api_node_t *universe_get_or_create_scheduler_api (struct universe_t *universe,
                                                                          kan_interned_string_t name)
{
    struct scheduler_api_node_t *node = universe_get_scheduler_api (universe, name);
    if (!node)
    {
        node = (struct scheduler_api_node_t *) kan_allocate_batched (universe->api_allocation_group,
                                                                     sizeof (struct scheduler_api_node_t));
        node->node.hash = KAN_HASH_OBJECT_POINTER (name);
        node->name = name;

        node->api.type = NULL;
        node->api.deploy = NULL;
        node->api.execute = NULL;
        node->api.undeploy = NULL;

        kan_hash_storage_update_bucket_count_default (&universe->scheduler_api_storage,
                                                      KAN_UNIVERSE_SCHEDULER_INITIAL_BUCKETS);
        kan_hash_storage_add (&universe->scheduler_api_storage, &node->node);
    }

    return node;
}

static struct mutator_api_node_t *universe_get_mutator_api (struct universe_t *universe, kan_interned_string_t name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&universe->mutator_api_storage, KAN_HASH_OBJECT_POINTER (name));
    struct mutator_api_node_t *node = (struct mutator_api_node_t *) bucket->first;
    const struct mutator_api_node_t *node_end =
        (struct mutator_api_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == name)
        {
            return node;
        }

        node = (struct mutator_api_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static struct mutator_api_node_t *universe_get_or_create_mutator_api (struct universe_t *universe,
                                                                      kan_interned_string_t name)
{
    struct mutator_api_node_t *node = universe_get_mutator_api (universe, name);
    if (!node)
    {
        node = (struct mutator_api_node_t *) kan_allocate_batched (universe->api_allocation_group,
                                                                   sizeof (struct mutator_api_node_t));
        node->node.hash = KAN_HASH_OBJECT_POINTER (name);
        node->name = name;

        node->api.type = NULL;
        node->api.deploy = NULL;
        node->api.execute = NULL;
        node->api.undeploy = NULL;

        kan_hash_storage_update_bucket_count_default (&universe->mutator_api_storage,
                                                      KAN_UNIVERSE_MUTATOR_INITIAL_BUCKETS);
        kan_hash_storage_add (&universe->mutator_api_storage, &node->node);
    }

    return node;
}

static void add_mutator_to_groups (struct universe_t *universe,
                                   kan_interned_string_t function_name,
                                   kan_interned_string_t mutator_name)
{
    struct kan_reflection_function_meta_iterator_t iterator = kan_reflection_registry_query_function_meta (
        universe->reflection_registry, function_name, interned_kan_universe_mutator_group_meta_t);

    const struct kan_universe_mutator_group_meta_t *meta;
    while ((meta = (const struct kan_universe_mutator_group_meta_t *) kan_reflection_function_meta_iterator_get (
                &iterator)))
    {
        kan_interned_string_t group_name = kan_string_intern (meta->group_name);
        const struct kan_hash_storage_bucket_t *bucket =
            kan_hash_storage_query (&universe->group_multi_map_storage, KAN_HASH_OBJECT_POINTER (group_name));
        struct group_multi_map_node_t *node = (struct group_multi_map_node_t *) bucket->first;
        const struct group_multi_map_node_t *node_end =
            (struct group_multi_map_node_t *) (bucket->last ? bucket->last->next : NULL);
        kan_bool_t found = KAN_FALSE;

        while (node != node_end)
        {
            if (node->group_name == group_name && node->mutator == mutator_name)
            {
                found = KAN_TRUE;
                break;
            }

            node = (struct group_multi_map_node_t *) node->node.list_node.next;
        }

        if (!found)
        {
            struct group_multi_map_node_t *new_node = (struct group_multi_map_node_t *) kan_allocate_batched (
                universe->api_allocation_group, sizeof (struct group_multi_map_node_t));
            new_node->node.hash = KAN_HASH_OBJECT_POINTER (group_name);
            new_node->group_name = group_name;
            new_node->mutator = mutator_name;

            kan_hash_storage_update_bucket_count_default (&universe->group_multi_map_storage,
                                                          KAN_UNIVERSE_GROUP_INITIAL_BUCKETS);
            kan_hash_storage_add (&universe->group_multi_map_storage, &new_node->node);
        }

        kan_reflection_function_meta_iterator_next (&iterator);
    }
}

static void universe_fill_api_storages (struct universe_t *universe)
{
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, kan_cpu_section_get ("universe_fill_api_storages"));

    kan_reflection_registry_function_iterator_t function_iterator =
        kan_reflection_registry_function_iterator_create (universe->reflection_registry);

    while (KAN_TRUE)
    {
        const struct kan_reflection_function_t *function =
            kan_reflection_registry_function_iterator_get (function_iterator);

        if (!function)
        {
            break;
        }

        if (strncmp (function->name, "kan_universe_scheduler_", 23u) == 0 &&
            function->name != interned_kan_universe_scheduler_interface_run_pipeline &&
            function->name != interned_kan_universe_scheduler_interface_update_child &&
            function->name != interned_kan_universe_scheduler_interface_update_all_children)
        {
            if (strncmp (function->name + 23u, "deploy_", 7u) == 0)
            {
                kan_interned_string_t name = kan_string_intern (function->name + 30u);
                struct scheduler_api_node_t *node = universe_get_or_create_scheduler_api (universe, name);

                if (node->api.deploy)
                {
                    KAN_LOG (universe_api_scan, KAN_LOG_ERROR, "Found several deploys for scheduler \"%s\".", name)
                }
                else
                {
                    node->api.deploy = function;
                }
            }
            else if (strncmp (function->name + 23u, "execute_", 8u) == 0)
            {
                kan_interned_string_t name = kan_string_intern (function->name + 31u);
                struct scheduler_api_node_t *node = universe_get_or_create_scheduler_api (universe, name);

                if (node->api.execute)
                {
                    KAN_LOG (universe_api_scan, KAN_LOG_ERROR, "Found several executes for scheduler \"%s\".", name)
                }
                else
                {
                    node->api.execute = function;
                }
            }
            else if (strncmp (function->name + 23u, "undeploy_", 9u) == 0)
            {
                kan_interned_string_t name = kan_string_intern (function->name + 32u);
                struct scheduler_api_node_t *node = universe_get_or_create_scheduler_api (universe, name);

                if (node->api.undeploy)
                {
                    KAN_LOG (universe_api_scan, KAN_LOG_ERROR, "Found several undeploys for scheduler \"%s\".", name)
                }
                else
                {
                    node->api.undeploy = function;
                }
            }
            else
            {
                KAN_LOG (universe_api_scan, KAN_LOG_ERROR,
                         "Unable to detect scheduler function type for function \"%s\".", function->name)
            }
        }
        else if (strncmp (function->name, "kan_universe_mutator_", 21u) == 0)
        {
            if (strncmp (function->name + 21u, "deploy_", 7u) == 0)
            {
                kan_interned_string_t name = kan_string_intern (function->name + 28u);
                struct mutator_api_node_t *node = universe_get_or_create_mutator_api (universe, name);

                if (node->api.deploy)
                {
                    KAN_LOG (universe_api_scan, KAN_LOG_ERROR, "Found several deploys for mutator \"%s\".", name)
                }
                else
                {
                    node->api.deploy = function;
                }

                add_mutator_to_groups (universe, function->name, name);
            }
            else if (strncmp (function->name + 21u, "execute_", 8u) == 0)
            {
                kan_interned_string_t name = kan_string_intern (function->name + 29u);
                struct mutator_api_node_t *node = universe_get_or_create_mutator_api (universe, name);

                if (node->api.execute)
                {
                    KAN_LOG (universe_api_scan, KAN_LOG_ERROR, "Found several executes for mutator \"%s\".", name)
                }
                else
                {
                    node->api.execute = function;
                }

                add_mutator_to_groups (universe, function->name, name);
            }
            else if (strncmp (function->name + 21u, "undeploy_", 9u) == 0)
            {
                kan_interned_string_t name = kan_string_intern (function->name + 30u);
                struct mutator_api_node_t *node = universe_get_or_create_mutator_api (universe, name);

                if (node->api.undeploy)
                {
                    KAN_LOG (universe_api_scan, KAN_LOG_ERROR, "Found several undeploys for mutator \"%s\".", name)
                }
                else
                {
                    node->api.undeploy = function;
                }

                add_mutator_to_groups (universe, function->name, name);
            }
            else
            {
                KAN_LOG (universe_api_scan, KAN_LOG_ERROR,
                         "Unable to detect mutator function type for function \"%s\".", function->name)
            }
        }

        function_iterator = kan_reflection_registry_function_iterator_next (function_iterator);
    }

    struct scheduler_api_node_t *scheduler_node =
        (struct scheduler_api_node_t *) universe->scheduler_api_storage.items.first;

    while (scheduler_node)
    {
        struct scheduler_api_node_t *next = (struct scheduler_api_node_t *) scheduler_node->node.list_node.next;
        kan_bool_t passing = KAN_TRUE;

        kan_interned_string_t state_struct_name = NULL;
        if (scheduler_node->api.deploy)
        {
            kan_bool_t passing_signature = KAN_TRUE;
            if (scheduler_node->api.deploy->arguments_count == 4u)
            {
                passing_signature &=
                    scheduler_node->api.deploy->arguments[0u].archetype == KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL &&
                    scheduler_node->api.deploy->arguments[0u].size == sizeof (kan_universe_t);

                passing_signature &=
                    scheduler_node->api.deploy->arguments[1u].archetype == KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL &&
                    scheduler_node->api.deploy->arguments[1u].size == sizeof (kan_universe_world_t);

                passing_signature &=
                    scheduler_node->api.deploy->arguments[2u].archetype == KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL &&
                    scheduler_node->api.deploy->arguments[2u].size == sizeof (kan_repository_t);

                passing_signature &=
                    scheduler_node->api.deploy->arguments[3u].archetype == KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER;

                state_struct_name = scheduler_node->api.deploy->arguments[3u].archetype_struct_pointer.type_name;
            }
            else
            {
                passing_signature = KAN_FALSE;
            }

            if (!passing_signature)
            {
                KAN_LOG (universe_api_scan, KAN_LOG_ERROR,
                         "Scheduler deploy function \"%s\" has incorrect signature! Expected arguments: "
                         "(kan_universe_t universe, kan_universe_world_t world, kan_repository_t world_repository, "
                         "struct your_scheduler_state_t *scheduler_state).",
                         scheduler_node->api.deploy->name)
                passing = KAN_FALSE;
            }
        }

        if (scheduler_node->api.execute)
        {
            kan_bool_t passing_signature = KAN_TRUE;
            if (scheduler_node->api.execute->arguments_count == 2u)
            {
                passing_signature &=
                    scheduler_node->api.execute->arguments[0u].archetype == KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL &&
                    scheduler_node->api.execute->arguments[0u].size == sizeof (kan_universe_scheduler_interface_t);

                passing_signature &=
                    scheduler_node->api.execute->arguments[1u].archetype == KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER;

                if (!state_struct_name)
                {
                    state_struct_name = scheduler_node->api.execute->arguments[1u].archetype_struct_pointer.type_name;
                }
                else if (state_struct_name !=
                         scheduler_node->api.execute->arguments[1u].archetype_struct_pointer.type_name)
                {
                    KAN_LOG (universe_api_scan, KAN_LOG_ERROR,
                             "Scheduler execute function has other state type than previous scheduler function: \"%s\" "
                             "!= \"%s\".",
                             state_struct_name,
                             scheduler_node->api.execute->arguments[1u].archetype_struct_pointer.type_name)
                    passing = KAN_FALSE;
                }
            }
            else
            {
                passing_signature = KAN_FALSE;
            }

            if (!passing_signature)
            {
                KAN_LOG (
                    universe_api_scan, KAN_LOG_ERROR,
                    "Scheduler execute function \"%s\" has incorrect signature! Expected arguments: "
                    "(kan_universe_scheduler_interface_t interface, struct your_scheduler_state_t *scheduler_state).",
                    scheduler_node->api.execute->name)
                passing = KAN_FALSE;
            }
        }
        else
        {
            KAN_LOG (universe_api_scan, KAN_LOG_ERROR, "Scheduler \"%s\" has no execute function!",
                     scheduler_node->name)
            passing = KAN_FALSE;
        }

        if (scheduler_node->api.undeploy)
        {
            kan_bool_t passing_signature = KAN_TRUE;
            if (scheduler_node->api.undeploy->arguments_count == 1u)
            {
                passing_signature &=
                    scheduler_node->api.undeploy->arguments[0u].archetype == KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER;

                if (!state_struct_name)
                {
                    state_struct_name = scheduler_node->api.undeploy->arguments[0u].archetype_struct_pointer.type_name;
                }
                else if (state_struct_name !=
                         scheduler_node->api.undeploy->arguments[0u].archetype_struct_pointer.type_name)
                {
                    KAN_LOG (
                        universe_api_scan, KAN_LOG_ERROR,
                        "Scheduler undeploy function has other state type than previous scheduler function: \"%s\" "
                        "!= \"%s\".",
                        state_struct_name,
                        scheduler_node->api.undeploy->arguments[0u].archetype_struct_pointer.type_name)
                    passing = KAN_FALSE;
                }
            }
            else
            {
                passing_signature = KAN_FALSE;
            }

            if (!passing_signature)
            {
                KAN_LOG (universe_api_scan, KAN_LOG_ERROR,
                         "Scheduler undeploy function \"%s\" has incorrect signature! Expected arguments: (struct "
                         "your_scheduler_state_t *scheduler_state).",
                         scheduler_node->api.undeploy->name)
                passing = KAN_FALSE;
            }
        }

        KAN_ASSERT (!passing || state_struct_name)
        if (state_struct_name)
        {
            scheduler_node->api.type =
                kan_reflection_registry_query_struct (universe->reflection_registry, state_struct_name);

            if (!scheduler_node->api.type)
            {
                KAN_LOG (universe_api_scan, KAN_LOG_ERROR, "Unable to find state struct \"%s\" for scheduler \"%s\".",
                         state_struct_name, scheduler_node->name)
                passing = KAN_FALSE;
            }
        }

        if (!passing)
        {
            kan_hash_storage_remove (&universe->scheduler_api_storage, &scheduler_node->node);
            kan_free_batched (universe->api_allocation_group, scheduler_node);
        }

        scheduler_node = next;
    }

    struct mutator_api_node_t *mutator_node = (struct mutator_api_node_t *) universe->mutator_api_storage.items.first;
    while (mutator_node)
    {
        struct mutator_api_node_t *next = (struct mutator_api_node_t *) mutator_node->node.list_node.next;
        kan_bool_t passing = KAN_TRUE;

        kan_interned_string_t state_struct_name = NULL;
        if (mutator_node->api.deploy)
        {
            kan_bool_t passing_signature = KAN_TRUE;
            if (mutator_node->api.deploy->arguments_count == 5u)
            {
                passing_signature &=
                    mutator_node->api.deploy->arguments[0u].archetype == KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL &&
                    mutator_node->api.deploy->arguments[0u].size == sizeof (kan_universe_t);

                passing_signature &=
                    mutator_node->api.deploy->arguments[1u].archetype == KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL &&
                    mutator_node->api.deploy->arguments[1u].size == sizeof (kan_universe_world_t);

                passing_signature &=
                    mutator_node->api.deploy->arguments[2u].archetype == KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL &&
                    mutator_node->api.deploy->arguments[2u].size == sizeof (kan_repository_t);

                passing_signature &=
                    mutator_node->api.deploy->arguments[3u].archetype == KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL &&
                    mutator_node->api.deploy->arguments[3u].size == sizeof (kan_workflow_graph_node_t);

                passing_signature &=
                    mutator_node->api.deploy->arguments[4u].archetype == KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER;

                state_struct_name = mutator_node->api.deploy->arguments[4u].archetype_struct_pointer.type_name;
            }
            else
            {
                passing_signature = KAN_FALSE;
            }

            if (!passing_signature)
            {
                KAN_LOG (universe_api_scan, KAN_LOG_ERROR,
                         "Mutator deploy function \"%s\" has incorrect signature! Expected arguments: "
                         "(kan_universe_t universe, kan_universe_world_t world, kan_repository_t world_repository, "
                         "kan_workflow_graph_node_t workflow_node, struct your_mutator_state_t *mutator_state).",
                         mutator_node->api.deploy->name)
                passing = KAN_FALSE;
            }
        }

        if (mutator_node->api.execute)
        {
            kan_bool_t passing_signature = KAN_TRUE;
            if (mutator_node->api.execute->arguments_count == 2u)
            {
                passing_signature &=
                    mutator_node->api.execute->arguments[0u].archetype == KAN_REFLECTION_ARCHETYPE_PACKED_ELEMENTAL &&
                    mutator_node->api.execute->arguments[0u].size == sizeof (kan_cpu_job_t);

                passing_signature &=
                    mutator_node->api.execute->arguments[1u].archetype == KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER;

                if (!state_struct_name)
                {
                    state_struct_name = mutator_node->api.execute->arguments[1u].archetype_struct_pointer.type_name;
                }
                else if (state_struct_name !=
                         mutator_node->api.execute->arguments[1u].archetype_struct_pointer.type_name)
                {
                    KAN_LOG (universe_api_scan, KAN_LOG_ERROR,
                             "Mutator execute function has other state type than previous mutator function: \"%s\" "
                             "!= \"%s\".",
                             state_struct_name,
                             mutator_node->api.execute->arguments[1u].archetype_struct_pointer.type_name)
                    passing = KAN_FALSE;
                }
            }
            else
            {
                passing_signature = KAN_FALSE;
            }

            if (!passing_signature)
            {
                KAN_LOG (universe_api_scan, KAN_LOG_ERROR,
                         "Mutator execute function \"%s\" has incorrect signature! Expected arguments: "
                         "(kan_cpu_job_t job, struct your_mutator_state_t *mutator_state).",
                         mutator_node->api.execute->name)
                passing = KAN_FALSE;
            }
        }
        else
        {
            KAN_LOG (universe_api_scan, KAN_LOG_ERROR, "Mutator \"%s\" has no execute function!", mutator_node->name)
            passing = KAN_FALSE;
        }

        if (mutator_node->api.undeploy)
        {
            kan_bool_t passing_signature = KAN_TRUE;
            if (mutator_node->api.undeploy->arguments_count == 1u)
            {
                passing_signature &=
                    mutator_node->api.undeploy->arguments[0u].archetype == KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER;

                if (!state_struct_name)
                {
                    state_struct_name = mutator_node->api.undeploy->arguments[0u].archetype_struct_pointer.type_name;
                }
                else if (state_struct_name !=
                         mutator_node->api.undeploy->arguments[0u].archetype_struct_pointer.type_name)
                {
                    KAN_LOG (universe_api_scan, KAN_LOG_ERROR,
                             "Mutator undeploy function has other state type than previous mutator function: \"%s\" "
                             "!= \"%s\".",
                             state_struct_name,
                             mutator_node->api.undeploy->arguments[0u].archetype_struct_pointer.type_name)
                    passing = KAN_FALSE;
                }
            }
            else
            {
                passing_signature = KAN_FALSE;
            }

            if (!passing_signature)
            {
                KAN_LOG (universe_api_scan, KAN_LOG_ERROR,
                         "Mutator undeploy function \"%s\" has incorrect signature! Expected arguments: (struct "
                         "your_mutator_state_t *mutator_state).",
                         mutator_node->api.undeploy->name)
                passing = KAN_FALSE;
            }
        }

        KAN_ASSERT (!passing || state_struct_name)
        if (state_struct_name)
        {
            mutator_node->api.type =
                kan_reflection_registry_query_struct (universe->reflection_registry, state_struct_name);

            if (!mutator_node->api.type)
            {
                KAN_LOG (universe_api_scan, KAN_LOG_ERROR, "Unable to find state struct \"%s\" for mutator \"%s\".",
                         state_struct_name, mutator_node->name)
                passing = KAN_FALSE;
            }
        }

        if (!passing)
        {
            kan_hash_storage_remove (&universe->mutator_api_storage, &mutator_node->node);
            kan_free_batched (universe->api_allocation_group, mutator_node);
        }

        mutator_node = next;
    }

    kan_cpu_section_execution_shutdown (&execution);
}

static struct world_t *world_create (struct universe_t *universe, struct world_t *parent, kan_interned_string_t name)
{
    struct world_t *world = kan_allocate_batched (universe->worlds_allocation_group, sizeof (struct world_t));
    if (parent)
    {
        void *spot = kan_dynamic_array_add_last (&parent->children);
        if (!spot)
        {
            kan_dynamic_array_set_capacity (&parent->children, KAN_MAX (1u, parent->children.capacity * 2u));
            spot = kan_dynamic_array_add_last (&parent->children);
            KAN_ASSERT (spot)
        }

        *(struct world_t **) spot = world;
        world->repository = kan_repository_create_child (parent->repository, name);
    }
    else
    {
        world->repository =
            kan_repository_create_root (kan_allocation_group_get_child (universe->main_allocation_group, "repository"),
                                        universe->reflection_registry);
    }

    world->name = name;
    world->parent = parent;

    world->scheduler_name = NULL;
    world->scheduler_api = NULL;
    world->scheduler_state = NULL;

    kan_dynamic_array_init (&world->pipelines, 0u, sizeof (struct pipeline_t), _Alignof (struct pipeline_t),
                            universe->worlds_allocation_group);

    kan_dynamic_array_init (&world->configuration, 0u, sizeof (struct world_configuration_t),
                            _Alignof (struct world_configuration_t), universe->worlds_allocation_group);

    kan_dynamic_array_init (&world->children, 0u, sizeof (struct world_t), _Alignof (struct world_t),
                            universe->worlds_allocation_group);
    return world;
}

static void world_scheduler_init (struct universe_t *universe, struct world_t *world)
{
    world->scheduler_state_allocation_group =
        kan_allocation_group_get_child (universe->schedulers_allocation_group, world->scheduler_api->type->name);

    world->scheduler_state =
        kan_allocate_general (world->scheduler_state_allocation_group, world->scheduler_api->type->size,
                              world->scheduler_api->type->alignment);

    // We use zeroes to check which automated queries weren't initialized yet.
    memset (world->scheduler_state, 0u, world->scheduler_api->type->size);

    if (world->scheduler_api->type->init)
    {
        kan_allocation_group_stack_push (world->scheduler_state_allocation_group);
        world->scheduler_api->type->init (world->scheduler_api->type->functor_user_data, world->scheduler_state);
        kan_allocation_group_stack_pop ();
    }
}

static void world_scheduler_undeploy (struct world_t *world, kan_reflection_registry_t actual_registry)
{
    KAN_ASSERT (world->scheduler_api)
    KAN_ASSERT (world->scheduler_state)
    undeploy_automated_lifetime_queries (actual_registry, world->scheduler_api->type->name, world->scheduler_state);

    if (world->scheduler_api->undeploy)
    {
        struct kan_universe_scheduler_undeploy_arguments_t arguments = {.scheduler_state = world->scheduler_state};
        world->scheduler_api->undeploy->call (world->scheduler_api->undeploy->call_user_data, NULL, &arguments);
    }
}

static void world_scheduler_remove (struct universe_t *universe,
                                    struct world_t *world,
                                    kan_reflection_registry_t scheduler_reflection_registry)
{
    // Undeploy is usually much faster than deploy, therefore we're not separating it to task.
    world_scheduler_undeploy (world, scheduler_reflection_registry);

    if (world->scheduler_api->type->shutdown)
    {
        kan_allocation_group_stack_push (world->scheduler_state_allocation_group);
        world->scheduler_api->type->shutdown (world->scheduler_api->type->functor_user_data, world->scheduler_state);
        kan_allocation_group_stack_pop ();
    }

    kan_free_general (world->scheduler_state_allocation_group, world->scheduler_state,
                      world->scheduler_api->type->size);
    world->scheduler_name = NULL;
    world->scheduler_api = NULL;
    world->scheduler_state = NULL;
}

static void mutator_clean (struct universe_t *universe,
                           struct mutator_t *mutator,
                           kan_reflection_registry_t mutator_reflection_registry);

static void world_clean_self_preserving_repository (struct universe_t *universe, struct world_t *world)
{
    if (world->scheduler_state)
    {
        world_scheduler_remove (universe, world, universe->reflection_registry);
    }

    for (kan_loop_size_t index = 0u; index < world->pipelines.size; ++index)
    {
        struct pipeline_t *pipeline = &((struct pipeline_t *) world->pipelines.data)[index];
        for (kan_loop_size_t mutator_index = 0u; mutator_index < pipeline->mutators.size; ++mutator_index)
        {
            struct mutator_t *mutator = &((struct mutator_t *) pipeline->mutators.data)[mutator_index];
            mutator_clean (universe, mutator, universe->reflection_registry);
        }

        if (KAN_HANDLE_IS_VALID (pipeline->graph))
        {
            kan_workflow_graph_destroy (pipeline->graph);
        }

        kan_dynamic_array_shutdown (&pipeline->mutators);
        kan_dynamic_array_shutdown (&pipeline->used_groups);
        kan_dynamic_array_shutdown (&pipeline->checkpoint_dependencies);
    }

    kan_dynamic_array_reset (&world->pipelines);
    kan_dynamic_array_set_capacity (&world->pipelines, 0u);

    for (kan_loop_size_t index = 0u; index < world->configuration.size; ++index)
    {
        struct world_configuration_t *configuration =
            &((struct world_configuration_t *) world->configuration.data)[index];

        if (configuration->type->shutdown)
        {
            configuration->type->shutdown (configuration->type->functor_user_data, configuration->data);
        }

        kan_free_batched (universe->configuration_allocation_group, configuration->data);
    }

    kan_dynamic_array_reset (&world->configuration);
    kan_dynamic_array_set_capacity (&world->configuration, 0u);
}

struct deploy_scheduler_user_data_t
{
    struct universe_t *universe;
    struct world_t *world;
};

static void deploy_scheduler_execute (kan_functor_user_data_t user_data)
{
    struct deploy_scheduler_user_data_t *data = (struct deploy_scheduler_user_data_t *) user_data;
    KAN_ASSERT (data->world->scheduler_state)
    KAN_ASSERT (data->world->scheduler_api)

    deploy_automated_lifetime_queries (data->universe->reflection_registry, data->world,
                                       KAN_HANDLE_SET_INVALID (kan_workflow_graph_node_t),
                                       data->world->scheduler_api->type->name, data->world->scheduler_state);

    if (data->world->scheduler_api->deploy)
    {
        struct kan_universe_scheduler_deploy_arguments_t arguments = {
            .universe = KAN_HANDLE_SET (kan_universe_t, data->universe),
            .world = KAN_HANDLE_SET (kan_universe_world_t, data->world),
            .world_repository = data->world->repository,
            .scheduler_state = data->world->scheduler_state,
        };

        data->world->scheduler_api->deploy->call (data->world->scheduler_api->deploy->call_user_data, NULL, &arguments);
    }
}

struct deploy_mutator_user_data_t
{
    struct universe_t *universe;
    struct world_t *world;
    struct mutator_t *mutator;
    kan_workflow_graph_node_t workflow_node;
};

static void execute_mutator (kan_cpu_job_t job, kan_functor_user_data_t user_data)
{
    struct mutator_t *mutator = (struct mutator_t *) user_data;
    KAN_ASSERT (mutator->api)
    KAN_ASSERT (mutator->api->execute)

    struct kan_universe_mutator_execute_arguments_t arguments = {
        .job = job,
        .mutator_state = mutator->state,
    };

    mutator->api->execute->call (mutator->api->execute->call_user_data, NULL, &arguments);
}

static void deploy_mutator_execute (kan_functor_user_data_t user_data)
{
    struct deploy_mutator_user_data_t *data = (struct deploy_mutator_user_data_t *) user_data;
    KAN_ASSERT (data->mutator->state)
    KAN_ASSERT (data->mutator->api)

    deploy_automated_lifetime_queries (data->universe->reflection_registry, data->world, data->workflow_node,
                                       data->mutator->api->type->name, data->mutator->state);

    if (data->mutator->api->deploy)
    {
        struct kan_universe_mutator_deploy_arguments_t arguments = {
            .universe = KAN_HANDLE_SET (kan_universe_t, data->universe),
            .world = KAN_HANDLE_SET (kan_universe_world_t, data->world),
            .world_repository = data->world->repository,
            .workflow_node = data->workflow_node,
            .mutator_state = data->mutator->state,
        };

        data->mutator->api->deploy->call (data->mutator->api->deploy->call_user_data, NULL, &arguments);
    }

    kan_workflow_graph_node_set_function (data->workflow_node, execute_mutator,
                                          (kan_functor_user_data_t) data->mutator);
    kan_workflow_graph_node_submit (data->workflow_node);
}

static void world_collect_deployment_tasks (struct universe_t *universe,
                                            struct world_t *world,
                                            struct kan_cpu_task_list_node_t **list_node,
                                            struct kan_stack_group_allocator_t *temporary_allocator)
{
    KAN_CPU_TASK_LIST_USER_STRUCT (list_node, temporary_allocator, deploy_scheduler_execute, section_deploy_scheduler,
                                   struct deploy_scheduler_user_data_t,
                                   {
                                       .universe = universe,
                                       .world = world,
                                   })

    for (kan_loop_size_t index = 0u; index < world->pipelines.size; ++index)
    {
        struct pipeline_t *pipeline = &((struct pipeline_t *) world->pipelines.data)[index];
        if (KAN_HANDLE_IS_VALID (pipeline->graph))
        {
            kan_workflow_graph_destroy (pipeline->graph);
        }

        pipeline->graph_builder = kan_workflow_graph_builder_create (temporary_allocator->group);
        for (kan_loop_size_t mutator_index = 0u; mutator_index < pipeline->mutators.size; ++mutator_index)
        {
            struct mutator_t *mutator = &((struct mutator_t *) pipeline->mutators.data)[mutator_index];
            KAN_CPU_TASK_LIST_USER_STRUCT (
                list_node, temporary_allocator, deploy_mutator_execute, section_deploy_mutator,
                struct deploy_mutator_user_data_t,
                {
                    .universe = universe,
                    .world = world,
                    .mutator = mutator,
                    .workflow_node = kan_workflow_graph_node_create (pipeline->graph_builder, mutator->name),
                })
        }
    }
}

static void finish_pipeline_deployment_execute (kan_functor_user_data_t user_data)
{
    struct pipeline_t *pipeline = (struct pipeline_t *) user_data;
    for (kan_loop_size_t dependency_index = 0u; dependency_index < pipeline->checkpoint_dependencies.size;
         ++dependency_index)
    {
        struct kan_universe_world_checkpoint_dependency_t *dependency =
            &((struct kan_universe_world_checkpoint_dependency_t *)
                  pipeline->checkpoint_dependencies.data)[dependency_index];

        kan_workflow_graph_builder_register_checkpoint_dependency (
            pipeline->graph_builder, dependency->dependency_checkpoint, dependency->dependendant_checkpoint);
    }

    kan_workflow_graph_t graph = kan_workflow_graph_builder_finalize (pipeline->graph_builder);

    if (!KAN_HANDLE_IS_VALID (graph))
    {
        KAN_LOG (universe, KAN_LOG_ERROR, "Failed to build pipeline \"%s\", see workflow errors for details.",
                 pipeline->name)
    }

    kan_workflow_graph_builder_destroy (pipeline->graph_builder);
    pipeline->graph = graph;
}

static void world_finish_deployment (struct universe_t *universe,
                                     struct world_t *world,
                                     struct kan_cpu_task_list_node_t **list_node,
                                     struct kan_stack_group_allocator_t *temporary_allocator)
{
    for (kan_loop_size_t index = 0u; index < world->pipelines.size; ++index)
    {
        struct pipeline_t *pipeline = &((struct pipeline_t *) world->pipelines.data)[index];
        KAN_CPU_TASK_LIST_USER_VALUE (list_node, temporary_allocator, finish_pipeline_deployment_execute,
                                      section_finish_pipeline_deployment, pipeline)
    }

    for (kan_loop_size_t index = 0u; index < world->children.size; ++index)
    {
        world_finish_deployment (universe, ((struct world_t **) world->children.data)[index], list_node,
                                 temporary_allocator);
    }
}

static void world_hierarchy_deploy_one_by_one (struct universe_t *universe,
                                               struct world_t *world,
                                               struct kan_stack_group_allocator_t *temporary_allocator)
{
    kan_cpu_job_t job = kan_cpu_job_create ();
    struct kan_cpu_task_list_node_t *list_node = NULL;

    world_collect_deployment_tasks (universe, world, &list_node, temporary_allocator);
    kan_cpu_job_dispatch_and_detach_task_list (job, list_node);
    kan_cpu_job_release (job);
    kan_cpu_job_wait (job);

    for (kan_loop_size_t index = 0u; index < world->children.size; ++index)
    {
        world_hierarchy_deploy_one_by_one (universe, ((struct world_t **) world->children.data)[index],
                                           temporary_allocator);
    }
}

static void world_deploy_hierarchy (struct universe_t *universe, struct world_t *world)
{
    struct kan_stack_group_allocator_t temporary_allocator;
    kan_stack_group_allocator_init (&temporary_allocator,
                                    kan_allocation_group_get_child (universe->main_allocation_group, "deploy"),
                                    KAN_UNIVERSE_DEPLOY_INITIAL_STACK);

    // We cannot deploy all worlds at once, because this may result in incorrect repository storage creation:
    // storages that need to be in higher repositories might end up in lower ones due to race condition.
    // Therefore, we start by initializing higher worlds and then initialize lower ones.
    world_hierarchy_deploy_one_by_one (universe, world, &temporary_allocator);

    kan_stack_group_allocator_reset (&temporary_allocator);
    kan_cpu_job_t job = kan_cpu_job_create ();
    struct kan_cpu_task_list_node_t *list_node = NULL;

    world_finish_deployment (universe, world, &list_node, &temporary_allocator);
    kan_cpu_job_dispatch_and_detach_task_list (job, list_node);
    kan_cpu_job_release (job);
    kan_cpu_job_wait (job);
    kan_stack_group_allocator_shutdown (&temporary_allocator);
}

static void world_prepare_repository_for_destroy (struct universe_t *universe, struct world_t *world)
{
    while (world->children.size > 0u)
    {
        struct world_t *child = ((struct world_t **) world->children.data)[0u];
        world_prepare_repository_for_destroy (universe, child);
    }

    kan_repository_schedule_child_destroy (world->repository);
}

static void world_destroy (struct universe_t *universe, struct world_t *world)
{
    world_clean_self_preserving_repository (universe, world);
    while (world->children.size > 0u)
    {
        struct world_t *child = ((struct world_t **) world->children.data)[0u];
        world_destroy (universe, child);
    }

    kan_dynamic_array_shutdown (&world->pipelines);
    kan_dynamic_array_shutdown (&world->configuration);
    kan_dynamic_array_shutdown (&world->children);

    if (world == universe->root_world)
    {
        kan_repository_destroy (world->repository);
    }

    if (world->parent)
    {
        for (kan_loop_size_t index = 0u; index < world->parent->children.size; ++index)
        {
            struct world_t *other_world = ((struct world_t **) world->parent->children.data)[index];
            if (other_world == world)
            {
                kan_dynamic_array_remove_swap_at (&world->parent->children, index);
                break;
            }
        }
    }
    else
    {
        KAN_ASSERT (universe->root_world == world)
        universe->root_world = NULL;
    }
}

static void mutator_init (struct universe_t *universe, struct mutator_t *mutator)
{
    mutator->state_allocation_group =
        kan_allocation_group_get_child (universe->mutators_allocation_group, mutator->api->type->name);
    mutator->state =
        kan_allocate_general (mutator->state_allocation_group, mutator->api->type->size, mutator->api->type->alignment);
    // We use zeroes to check which automated queries weren't initialized yet.
    memset (mutator->state, 0u, mutator->api->type->size);
    mutator->from_group = KAN_FALSE;
    mutator->found_in_groups = KAN_FALSE;
    mutator->added_during_migration = KAN_FALSE;

    if (mutator->api->type->init)
    {
        kan_allocation_group_stack_push (mutator->state_allocation_group);
        mutator->api->type->init (mutator->api->type->functor_user_data, mutator->state);
        kan_allocation_group_stack_pop ();
    }
}

static void mutator_undeploy (struct mutator_t *mutator, kan_reflection_registry_t actual_registry)
{
    KAN_ASSERT (mutator->api)
    KAN_ASSERT (mutator->state)
    undeploy_automated_lifetime_queries (actual_registry, mutator->api->type->name, mutator->state);

    if (mutator->api->undeploy)
    {
        struct kan_universe_mutator_undeploy_arguments_t arguments = {.mutator_state = mutator->state};
        mutator->api->undeploy->call (mutator->api->undeploy->call_user_data, NULL, &arguments);
    }
}

static void mutator_clean (struct universe_t *universe,
                           struct mutator_t *mutator,
                           kan_reflection_registry_t mutator_reflection_registry)
{
    // Undeploy is usually much faster than deploy, therefore we're not separating it to task.
    mutator_undeploy (mutator, mutator_reflection_registry);

    if (mutator->api->type->shutdown)
    {
        kan_allocation_group_stack_push (mutator->state_allocation_group);
        mutator->api->type->shutdown (mutator->api->type->functor_user_data, mutator->state);
        kan_allocation_group_stack_pop ();
    }

    kan_free_general (mutator->state_allocation_group, mutator->state, mutator->api->type->size);
}

void kan_universe_world_configuration_layer_init (struct kan_universe_world_configuration_layer_t *data)
{
    kan_dynamic_array_init (&data->required_tags, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    data->data = KAN_HANDLE_SET_INVALID (kan_reflection_patch_t);
}

void kan_universe_world_configuration_layer_shutdown (struct kan_universe_world_configuration_layer_t *data)
{
    kan_dynamic_array_shutdown (&data->required_tags);
    if (KAN_HANDLE_IS_VALID (data->data))
    {
        kan_reflection_patch_destroy (data->data);
    }
}

void kan_universe_world_configuration_init (struct kan_universe_world_configuration_t *data)
{
    data->name = NULL;
    kan_dynamic_array_init (&data->layers, 0u, sizeof (struct kan_universe_world_configuration_layer_t),
                            _Alignof (struct kan_universe_world_configuration_layer_t),
                            kan_allocation_group_stack_get ());
}

void kan_universe_world_configuration_shutdown (struct kan_universe_world_configuration_t *data)
{
    for (kan_loop_size_t index = 0u; index < data->layers.size; ++index)
    {
        kan_universe_world_configuration_layer_shutdown (
            &((struct kan_universe_world_configuration_layer_t *) data->layers.data)[index]);
    }

    kan_dynamic_array_shutdown (&data->layers);
}

void kan_universe_world_pipeline_definition_init (struct kan_universe_world_pipeline_definition_t *data)
{
    data->name = NULL;
    kan_dynamic_array_init (&data->mutators, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&data->mutator_groups, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_dynamic_array_init (
        &data->checkpoint_dependencies, 0u, sizeof (struct kan_universe_world_checkpoint_dependency_t),
        _Alignof (struct kan_universe_world_checkpoint_dependency_t), kan_allocation_group_stack_get ());
}

void kan_universe_world_pipeline_definition_shutdown (struct kan_universe_world_pipeline_definition_t *data)
{
    kan_dynamic_array_shutdown (&data->mutators);
    kan_dynamic_array_shutdown (&data->mutator_groups);
    kan_dynamic_array_shutdown (&data->checkpoint_dependencies);
}

UNIVERSE_API void kan_universe_world_definition_init (struct kan_universe_world_definition_t *data)
{
    data->world_name = NULL;
    kan_dynamic_array_init (&data->configuration, 0u, sizeof (struct kan_universe_world_configuration_t),
                            _Alignof (struct kan_universe_world_configuration_t), kan_allocation_group_stack_get ());

    data->scheduler_name = NULL;
    kan_dynamic_array_init (&data->pipelines, 0u, sizeof (struct kan_universe_world_pipeline_definition_t),
                            _Alignof (struct kan_universe_world_pipeline_definition_t),
                            kan_allocation_group_stack_get ());

    kan_dynamic_array_init (&data->children, 0u, sizeof (struct kan_universe_world_definition_t),
                            _Alignof (struct kan_universe_world_definition_t), kan_allocation_group_stack_get ());
}

UNIVERSE_API void kan_universe_world_definition_shutdown (struct kan_universe_world_definition_t *data)
{
    for (kan_loop_size_t index = 0u; index < data->configuration.size; ++index)
    {
        kan_universe_world_configuration_shutdown (
            &((struct kan_universe_world_configuration_t *) data->configuration.data)[index]);
    }

    kan_dynamic_array_shutdown (&data->configuration);
    for (kan_loop_size_t index = 0u; index < data->pipelines.size; ++index)
    {
        kan_universe_world_pipeline_definition_shutdown (
            &((struct kan_universe_world_pipeline_definition_t *) data->pipelines.data)[index]);
    }

    kan_dynamic_array_shutdown (&data->pipelines);
    for (kan_loop_size_t index = 0u; index < data->children.size; ++index)
    {
        kan_universe_world_definition_shutdown (
            &((struct kan_universe_world_definition_t *) data->children.data)[index]);
    }

    kan_dynamic_array_shutdown (&data->children);
}

kan_universe_world_t kan_universe_world_get_parent (kan_universe_world_t world)
{
    struct world_t *world_data = KAN_HANDLE_GET (world);
    return KAN_HANDLE_SET (kan_universe_world_t, world_data->parent);
}

kan_interned_string_t kan_universe_world_get_name (kan_universe_world_t world)
{
    struct world_t *world_data = KAN_HANDLE_GET (world);
    return world_data->name;
}

const void *kan_universe_world_query_configuration (kan_universe_world_t world, kan_interned_string_t name)
{
    struct world_t *world_data = KAN_HANDLE_GET (world);
    for (kan_loop_size_t index = 0u; index < world_data->configuration.size; ++index)
    {
        struct world_configuration_t *configuration =
            &((struct world_configuration_t *) world_data->configuration.data)[index];

        if (configuration->name == name)
        {
            return configuration->data;
        }
    }

    if (world_data->parent)
    {
        return kan_universe_world_query_configuration (KAN_HANDLE_SET (kan_universe_world_t, world_data->parent), name);
    }

    return NULL;
}

kan_universe_world_iterator_t kan_universe_world_iterator_begin_children (kan_universe_world_t world)
{
    return KAN_TYPED_ID_32_SET (kan_universe_world_iterator_t, 0u);
}

kan_universe_world_t kan_universe_world_iterator_get (kan_universe_world_iterator_t iterator,
                                                      kan_universe_world_t world)
{
    struct world_t *world_data = KAN_HANDLE_GET (world);
    if (KAN_TYPED_ID_32_GET (iterator) < world_data->children.size)
    {
        return KAN_HANDLE_SET (kan_universe_world_t,
                               ((struct world_t **) world_data->children.data)[KAN_TYPED_ID_32_GET (iterator)]);
    }

    return KAN_HANDLE_SET_INVALID (kan_universe_world_t);
}

kan_universe_world_iterator_t kan_universe_world_iterator_next (kan_universe_world_iterator_t iterator)
{
    return KAN_TYPED_ID_32_SET (kan_universe_world_iterator_t, KAN_TYPED_ID_32_GET (iterator) + 1u);
}

kan_universe_t kan_universe_create (kan_allocation_group_t group,
                                    kan_reflection_registry_t registry,
                                    kan_context_t context)
{
    ensure_statics_initialized ();
    struct universe_t *universe =
        (struct universe_t *) kan_allocate_general (group, sizeof (struct universe_t), _Alignof (struct universe_t));

    universe->reflection_registry = registry;
    universe->context = context;
    universe->root_world = NULL;

    universe->main_allocation_group = group;
    universe->worlds_allocation_group = kan_allocation_group_get_child (group, "worlds");
    universe->api_allocation_group = kan_allocation_group_get_child (group, "api");
    universe->configuration_allocation_group = kan_allocation_group_get_child (group, "configuration");
    universe->schedulers_allocation_group = kan_allocation_group_get_child (group, "schedulers");
    universe->mutators_allocation_group = kan_allocation_group_get_child (group, "mutators");

    kan_hash_storage_init (&universe->scheduler_api_storage, universe->api_allocation_group,
                           KAN_UNIVERSE_SCHEDULER_INITIAL_BUCKETS);
    kan_hash_storage_init (&universe->mutator_api_storage, universe->api_allocation_group,
                           KAN_UNIVERSE_MUTATOR_INITIAL_BUCKETS);
    kan_hash_storage_init (&universe->group_multi_map_storage, universe->api_allocation_group,
                           KAN_UNIVERSE_GROUP_INITIAL_BUCKETS);

    kan_dynamic_array_init (&universe->environment_tags, 0u, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), universe->main_allocation_group);
    universe->update_section = kan_cpu_section_get ("universe_update");
    universe_fill_api_storages (universe);
    return KAN_HANDLE_SET (kan_universe_t, universe);
}

struct undeploy_and_migrate_scheduler_user_data_t
{
    struct universe_t *universe;
    struct world_t *world;
    kan_reflection_registry_t old_registry;
    struct scheduler_api_t *old_api;
    kan_reflection_struct_migrator_t migrator;
};

static void undeploy_and_migrate_scheduler_execute (kan_functor_user_data_t user_data)
{
    struct undeploy_and_migrate_scheduler_user_data_t *data =
        (struct undeploy_and_migrate_scheduler_user_data_t *) user_data;

    // Undeploy non-automated queries first, so they won't be migrated and won't cause unexpected errors.
    if (data->old_api->undeploy)
    {
        struct kan_universe_scheduler_undeploy_arguments_t arguments = {.scheduler_state =
                                                                            data->world->scheduler_state};
        data->old_api->undeploy->call (data->old_api->undeploy->call_user_data, NULL, &arguments);
    }

    void *old_scheduler_state = data->world->scheduler_state;
    const kan_instance_size_t old_scheduler_size = data->world->scheduler_api->type->size;
    kan_allocation_group_t old_scheduler_state_allocation_group = data->world->scheduler_state_allocation_group;
    world_scheduler_init (data->universe, data->world);

    kan_reflection_struct_migrator_migrate_instance (data->migrator, data->world->scheduler_api->type->name,
                                                     old_scheduler_state, data->world->scheduler_state);

    // Undeploy everything that wasn't migrated.
    undeploy_automated_lifetime_queries (data->old_registry, data->old_api->type->name, old_scheduler_state);

    if (data->old_api->type->shutdown)
    {
        data->old_api->type->shutdown (data->old_api->type->functor_user_data, old_scheduler_state);
    }

    kan_free_general (old_scheduler_state_allocation_group, old_scheduler_state, old_scheduler_size);
}

struct undeploy_and_migrate_mutator_user_data_t
{
    struct universe_t *universe;
    struct mutator_t *mutator;
    kan_reflection_registry_t old_registry;
    struct mutator_api_t *old_api;
    kan_reflection_struct_migrator_t migrator;
};

static void undeploy_and_migrate_mutator_execute (kan_functor_user_data_t user_data)
{
    struct undeploy_and_migrate_mutator_user_data_t *data =
        (struct undeploy_and_migrate_mutator_user_data_t *) user_data;

    // Undeploy non-automated queries first, so they won't be migrated and won't cause unexpected errors.
    if (data->old_api->undeploy)
    {
        struct kan_universe_mutator_undeploy_arguments_t arguments = {.mutator_state = data->mutator->state};
        data->old_api->undeploy->call (data->old_api->undeploy->call_user_data, NULL, &arguments);
    }

    void *old_mutator_state = data->mutator->state;
    const kan_instance_size_t old_mutator_size = data->old_api->type->size;
    kan_allocation_group_t old_mutator_allocation_group = data->mutator->state_allocation_group;
    mutator_init (data->universe, data->mutator);

    kan_reflection_struct_migrator_migrate_instance (data->migrator, data->mutator->api->type->name, old_mutator_state,
                                                     data->mutator->state);

    // Undeploy everything that wasn't migrated.
    undeploy_automated_lifetime_queries (data->old_registry, data->old_api->type->name, old_mutator_state);

    if (data->old_api->type->shutdown)
    {
        data->old_api->type->shutdown (data->old_api->type->functor_user_data, old_mutator_state);
    }

    kan_free_general (old_mutator_allocation_group, old_mutator_state, old_mutator_size);
}

static void world_migration_schedulers_mutators_migrate (struct universe_t *universe,
                                                         struct world_t *world,
                                                         kan_reflection_registry_t old_reflection_registry,
                                                         kan_reflection_migration_seed_t migration_seed,
                                                         kan_reflection_struct_migrator_t migrator,
                                                         struct kan_cpu_task_list_node_t **first_task_node,
                                                         struct kan_stack_group_allocator_t *temporary_allocator)
{
    struct scheduler_api_node_t *new_scheduler_api_node = universe_get_scheduler_api (universe, world->scheduler_name);
    if (new_scheduler_api_node)
    {
        if (new_scheduler_api_node->api.type->name == world->scheduler_api->type->name)
        {
            const struct kan_reflection_struct_migration_seed_t *seed =
                kan_reflection_migration_seed_get_for_struct (migration_seed, new_scheduler_api_node->api.type->name);

            switch (seed->status)
            {
            case KAN_REFLECTION_MIGRATION_NEEDED:
            {
                struct scheduler_api_t *old_api = world->scheduler_api;
                world->scheduler_api = &new_scheduler_api_node->api;

                KAN_CPU_TASK_LIST_USER_STRUCT (
                    first_task_node, temporary_allocator, undeploy_and_migrate_scheduler_execute,
                    section_undeploy_and_migrate_scheduler, struct undeploy_and_migrate_scheduler_user_data_t,
                    {
                        .universe = universe,
                        .world = world,
                        .old_registry = old_reflection_registry,
                        .old_api = old_api,
                        .migrator = migrator,
                    })

                break;
            }

            case KAN_REFLECTION_MIGRATION_NOT_NEEDED:
            {
                world_scheduler_undeploy (world, old_reflection_registry);
                world->scheduler_api = &new_scheduler_api_node->api;
                break;
            }

            case KAN_REFLECTION_MIGRATION_REMOVED:
                // In case of removal we should end up in other branches.
                KAN_ASSERT (KAN_FALSE)
                world_scheduler_remove (universe, world, old_reflection_registry);
                break;
            }
        }
        else
        {
            world_scheduler_remove (universe, world, old_reflection_registry);
            world->scheduler_api = &new_scheduler_api_node->api;
            world_scheduler_init (universe, world);
        }
    }
    else if (world->scheduler_api)
    {
        KAN_LOG (universe_migration, KAN_LOG_WARNING,
                 "World \"%s\" lost its scheduler \"%s\" as a result of migration.", world->name, world->scheduler_name)

        world_scheduler_remove (universe, world, old_reflection_registry);
    }

    for (kan_loop_size_t pipeline_index = 0u; pipeline_index < world->pipelines.size; ++pipeline_index)
    {
        struct pipeline_t *pipeline = &((struct pipeline_t *) world->pipelines.data)[pipeline_index];

        // Groups might've changed due to migration.
        // We need to remove mutators that we're added from groups, but no longer belong to them.
        // Also, we need to add mutators that were added to groups due to migration.
        for (kan_loop_size_t mutator_index = 0u; mutator_index < pipeline->mutators.size; ++mutator_index)
        {
            struct mutator_t *mutator = &((struct mutator_t *) pipeline->mutators.data)[mutator_index];
            mutator->found_in_groups = KAN_FALSE;
        }

        for (kan_loop_size_t group_index = 0u; group_index < pipeline->used_groups.size; ++group_index)
        {
            kan_interned_string_t group_name = ((kan_interned_string_t *) pipeline->used_groups.data)[group_index];
            const struct kan_hash_storage_bucket_t *bucket =
                kan_hash_storage_query (&universe->group_multi_map_storage, KAN_HASH_OBJECT_POINTER (group_name));
            struct group_multi_map_node_t *node = (struct group_multi_map_node_t *) bucket->first;
            const struct group_multi_map_node_t *node_end =
                (struct group_multi_map_node_t *) (bucket->last ? bucket->last->next : NULL);

            while (node != node_end)
            {
                if (node->group_name == group_name)
                {
                    // Find mutator and mark it as found.
                    kan_bool_t found = KAN_FALSE;

                    for (kan_loop_size_t mutator_index = 0u; mutator_index < pipeline->mutators.size; ++mutator_index)
                    {
                        struct mutator_t *mutator = &((struct mutator_t *) pipeline->mutators.data)[mutator_index];
                        if (mutator->name == node->mutator)
                        {
                            mutator->found_in_groups = KAN_TRUE;
                            found = KAN_TRUE;
                            break;
                        }
                    }

                    if (!found)
                    {
                        struct mutator_api_node_t *mutator_node = universe_get_mutator_api (universe, node->mutator);
                        if (mutator_node)
                        {
                            struct mutator_t *mutator =
                                (struct mutator_t *) kan_dynamic_array_add_last (&pipeline->mutators);
                            if (!mutator)
                            {
                                kan_dynamic_array_set_capacity (&pipeline->mutators, pipeline->mutators.size * 2u);
                                mutator = (struct mutator_t *) kan_dynamic_array_add_last (&pipeline->mutators);
                            }

                            KAN_ASSERT (mutator)
                            mutator->name = node->mutator;
                            mutator->api = &mutator_node->api;
                            mutator_init (universe, mutator);
                            mutator->from_group = KAN_TRUE;
                            mutator->added_during_migration = KAN_TRUE;
                        }
                        else
                        {
                            KAN_LOG (universe, KAN_LOG_ERROR,
                                     "Unable to find mutator \"%s\" requested from group \"%s\".", node->mutator,
                                     group_name)
                        }
                    }
                }

                node = (struct group_multi_map_node_t *) node->node.list_node.next;
            }
        }

        for (kan_loop_size_t mutator_index = 0u; mutator_index < pipeline->mutators.size;)
        {
            struct mutator_t *mutator = &((struct mutator_t *) pipeline->mutators.data)[mutator_index];
            if (mutator->added_during_migration)
            {
                mutator->added_during_migration = KAN_FALSE;
                ++mutator_index;
                continue;
            }

            struct mutator_api_node_t *new_mutator_api_node = universe_get_mutator_api (universe, mutator->name);
            if ((!mutator->from_group || mutator->found_in_groups) && new_mutator_api_node)
            {
                if (new_mutator_api_node->api.type->name == mutator->api->type->name)
                {
                    const struct kan_reflection_struct_migration_seed_t *seed =
                        kan_reflection_migration_seed_get_for_struct (migration_seed,
                                                                      new_mutator_api_node->api.type->name);

                    switch (seed->status)
                    {
                    case KAN_REFLECTION_MIGRATION_NEEDED:
                    {
                        struct mutator_api_t *old_api = mutator->api;
                        mutator->api = &new_mutator_api_node->api;

                        KAN_CPU_TASK_LIST_USER_STRUCT (
                            first_task_node, temporary_allocator, undeploy_and_migrate_mutator_execute,
                            section_undeploy_and_migrate_mutator, struct undeploy_and_migrate_mutator_user_data_t,
                            {
                                .universe = universe,
                                .mutator = mutator,
                                .old_registry = old_reflection_registry,
                                .old_api = old_api,
                                .migrator = migrator,
                            })

                        ++mutator_index;
                        break;
                    }

                    case KAN_REFLECTION_MIGRATION_NOT_NEEDED:
                    {
                        mutator_undeploy (mutator, old_reflection_registry);
                        mutator->api = &new_mutator_api_node->api;
                        ++mutator_index;
                        break;
                    }

                    case KAN_REFLECTION_MIGRATION_REMOVED:
                        // In case of removal we should end up in other branches.
                        KAN_ASSERT (KAN_FALSE)
                        mutator_clean (universe, mutator, old_reflection_registry);
                        kan_dynamic_array_remove_swap_at (&pipeline->mutators, mutator_index);
                        break;
                    }
                }
                else
                {
                    mutator_clean (universe, mutator, old_reflection_registry);
                    mutator->api = &new_mutator_api_node->api;
                    mutator_init (universe, mutator);
                    ++mutator_index;
                }
            }
            else
            {
                KAN_LOG (universe_migration, KAN_LOG_WARNING, "Mutator \"%s\" removed as a result of migration.",
                         mutator->name)

                mutator_clean (universe, mutator, old_reflection_registry);
                kan_dynamic_array_remove_swap_at (&pipeline->mutators, mutator_index);
            }
        }
    }

    for (kan_loop_size_t child_index = 0u; child_index < world->children.size; ++child_index)
    {
        world_migration_schedulers_mutators_migrate (universe, ((struct world_t **) world->children.data)[child_index],
                                                     old_reflection_registry, migration_seed, migrator, first_task_node,
                                                     temporary_allocator);
    }
}

struct migrate_configuration_user_data_t
{
    struct world_configuration_t *configuration;
    const struct kan_reflection_struct_t *new_type;
    struct universe_t *universe;
    kan_reflection_struct_migrator_t migrator;
};

static void migrate_configuration_execute (kan_functor_user_data_t user_data)
{
    struct migrate_configuration_user_data_t *data = (struct migrate_configuration_user_data_t *) user_data;
    void *old_data = data->configuration->data;
    data->configuration->data =
        kan_allocate_batched (data->universe->configuration_allocation_group, data->new_type->size);

    if (data->new_type->init)
    {
        data->new_type->init (data->new_type->functor_user_data, data->configuration->data);
    }

    kan_reflection_struct_migrator_migrate_instance (data->migrator, data->new_type->name, old_data,
                                                     data->configuration->data);

    data->configuration->type->shutdown (data->configuration->type->functor_user_data, old_data);
    kan_free_batched (data->universe->configuration_allocation_group, old_data);
    data->configuration->type = data->new_type;
}

static void world_migrate_configuration (struct universe_t *universe,
                                         struct world_t *world,
                                         kan_reflection_migration_seed_t migration_seed,
                                         kan_reflection_struct_migrator_t migrator,
                                         struct kan_cpu_task_list_node_t **first_task_node,
                                         struct kan_stack_group_allocator_t *temporary_allocator)
{
    for (kan_loop_size_t configuration_index = 0u; configuration_index < world->configuration.size;)
    {
        struct world_configuration_t *configuration =
            &((struct world_configuration_t *) world->configuration.data)[configuration_index];

        const struct kan_reflection_struct_migration_seed_t *configuration_seed =
            kan_reflection_migration_seed_get_for_struct (migration_seed, configuration->type->name);

        switch (configuration_seed->status)
        {
        case KAN_REFLECTION_MIGRATION_NEEDED:
        {
            const struct kan_reflection_struct_t *new_type =
                kan_reflection_registry_query_struct (universe->reflection_registry, configuration->type->name);
            KAN_ASSERT (new_type)

            KAN_CPU_TASK_LIST_USER_STRUCT (first_task_node, temporary_allocator, migrate_configuration_execute,
                                           section_migrate_configuration, struct migrate_configuration_user_data_t,
                                           {
                                               .configuration = configuration,
                                               .new_type = new_type,
                                               .universe = universe,
                                               .migrator = migrator,
                                           })

            ++configuration_index;
            break;
        }

        case KAN_REFLECTION_MIGRATION_NOT_NEEDED:
            configuration->type =
                kan_reflection_registry_query_struct (universe->reflection_registry, configuration->type->name);
            ++configuration_index;
            break;

        case KAN_REFLECTION_MIGRATION_REMOVED:
            configuration->type->shutdown (configuration->type->functor_user_data, configuration->data);
            kan_free_batched (universe->configuration_allocation_group, configuration->data);
            kan_dynamic_array_remove_swap_at (&world->configuration, configuration_index);
            break;
        }
    }

    for (kan_loop_size_t child_index = 0u; child_index < world->children.size; ++child_index)
    {
        world_migrate_configuration (universe, ((struct world_t **) world->children.data)[child_index], migration_seed,
                                     migrator, first_task_node, temporary_allocator);
    }
}

void kan_universe_migrate (kan_universe_t universe,
                           kan_reflection_registry_t new_registry,
                           kan_reflection_migration_seed_t migration_seed,
                           kan_reflection_struct_migrator_t migrator)
{
    struct universe_t *universe_data = KAN_HANDLE_GET (universe);
    struct kan_cpu_section_execution_t migrate_execution;
    kan_cpu_section_execution_init (&migrate_execution, kan_cpu_section_get ("universe_migrate"));

    struct kan_hash_storage_t old_scheduler_api_storage = universe_data->scheduler_api_storage;
    struct kan_hash_storage_t old_mutator_api_storage = universe_data->mutator_api_storage;

    kan_hash_storage_init (&universe_data->scheduler_api_storage, universe_data->api_allocation_group,
                           KAN_UNIVERSE_SCHEDULER_INITIAL_BUCKETS);
    kan_hash_storage_init (&universe_data->mutator_api_storage, universe_data->api_allocation_group,
                           KAN_UNIVERSE_MUTATOR_INITIAL_BUCKETS);

    kan_reflection_registry_t old_registry = universe_data->reflection_registry;
    universe_data->reflection_registry = new_registry;
    universe_fill_api_storages (universe_data);

    if (universe_data->root_world)
    {
        kan_repository_prepare_for_migration (universe_data->root_world->repository, migration_seed);
        kan_repository_enter_planning_mode (universe_data->root_world->repository);

        struct kan_stack_group_allocator_t temporary_allocator;
        kan_stack_group_allocator_init (
            &temporary_allocator, kan_allocation_group_get_child (universe_data->main_allocation_group, "migration"),
            KAN_UNIVERSE_MIGRATION_INITIAL_STACK);

        struct kan_cpu_section_execution_t mutators_schedulers_execution;
        kan_cpu_section_execution_init (&mutators_schedulers_execution,
                                        kan_cpu_section_get ("mutators_and_schedulers"));

        kan_cpu_job_t job = kan_cpu_job_create ();
        struct kan_cpu_task_list_node_t *task_list = NULL;
        world_migration_schedulers_mutators_migrate (universe_data, universe_data->root_world, old_registry,
                                                     migration_seed, migrator, &task_list, &temporary_allocator);

        kan_cpu_job_dispatch_and_detach_task_list (job, task_list);
        kan_cpu_job_release (job);
        kan_cpu_job_wait (job);
        kan_stack_group_allocator_reset (&temporary_allocator);

        kan_cpu_section_execution_shutdown (&mutators_schedulers_execution);
        struct kan_cpu_section_execution_t configuration_execution;
        kan_cpu_section_execution_init (&configuration_execution, kan_cpu_section_get ("configuration"));

        job = kan_cpu_job_create ();
        task_list = NULL;
        world_migrate_configuration (universe_data, universe_data->root_world, migration_seed, migrator, &task_list,
                                     &temporary_allocator);

        kan_cpu_job_dispatch_and_detach_task_list (job, task_list);
        kan_cpu_job_release (job);
        kan_cpu_job_wait (job);
        kan_stack_group_allocator_reset (&temporary_allocator);
        kan_cpu_section_execution_shutdown (&configuration_execution);

        kan_repository_migrate (universe_data->root_world->repository, new_registry, migration_seed, migrator);
        world_deploy_hierarchy (universe_data, universe_data->root_world);

        kan_stack_group_allocator_shutdown (&temporary_allocator);
        kan_repository_enter_serving_mode (universe_data->root_world->repository);
    }

    while (old_scheduler_api_storage.items.first)
    {
        struct scheduler_api_node_t *node = (struct scheduler_api_node_t *) old_scheduler_api_storage.items.first;
        kan_hash_storage_remove (&old_scheduler_api_storage, &node->node);
        kan_free_batched (universe_data->api_allocation_group, node);
    }

    while (old_mutator_api_storage.items.first)
    {
        struct mutator_api_node_t *node = (struct mutator_api_node_t *) old_mutator_api_storage.items.first;
        kan_hash_storage_remove (&old_mutator_api_storage, &node->node);
        kan_free_batched (universe_data->api_allocation_group, node);
    }

    kan_hash_storage_shutdown (&old_scheduler_api_storage);
    kan_hash_storage_shutdown (&old_mutator_api_storage);
    kan_cpu_section_execution_shutdown (&migrate_execution);
}

kan_reflection_registry_t kan_universe_get_reflection_registry (kan_universe_t universe)
{
    struct universe_t *universe_data = KAN_HANDLE_GET (universe);
    return universe_data->reflection_registry;
}

kan_context_t kan_universe_get_context (kan_universe_t universe)
{
    struct universe_t *universe_data = KAN_HANDLE_GET (universe);
    return universe_data->context;
}

kan_universe_world_t kan_universe_get_root_world (kan_universe_t universe)
{
    struct universe_t *universe_data = KAN_HANDLE_GET (universe);
    return KAN_HANDLE_SET (kan_universe_world_t, universe_data->root_world);
}

void kan_universe_add_environment_tag (kan_universe_t universe, kan_interned_string_t environment_tag)
{
    struct universe_t *universe_data = KAN_HANDLE_GET (universe);
    kan_interned_string_t *spot = kan_dynamic_array_add_last (&universe_data->environment_tags);

    if (!spot)
    {
        kan_dynamic_array_set_capacity (&universe_data->environment_tags,
                                        KAN_MAX (1u, universe_data->environment_tags.capacity * 2u));
        spot = kan_dynamic_array_add_last (&universe_data->environment_tags);
        KAN_ASSERT (spot)
    }

    *spot = environment_tag;
}

static void fill_world_from_definition (struct universe_t *universe,
                                        struct world_t *world,
                                        const struct kan_universe_world_definition_t *definition)
{
    KAN_ASSERT (world->name == definition->world_name)
    KAN_ASSERT (world->scheduler_name == NULL)
    KAN_ASSERT (world->scheduler_api == NULL)
    KAN_ASSERT (world->scheduler_state == NULL)

    KAN_ASSERT (world->pipelines.size == 0u)
    KAN_ASSERT (world->configuration.size == 0u)

    kan_dynamic_array_set_capacity (&world->configuration, definition->configuration.size);
    for (kan_loop_size_t index = 0u; index < definition->configuration.size; ++index)
    {
        struct kan_universe_world_configuration_t *input =
            &((struct kan_universe_world_configuration_t *) definition->configuration.data)[index];
        struct world_configuration_t *output = NULL;

        for (kan_loop_size_t variant_index = 0u; variant_index < input->layers.size; ++variant_index)
        {
            struct kan_universe_world_configuration_layer_t *layer =
                &((struct kan_universe_world_configuration_layer_t *) input->layers.data)[variant_index];

            if (!KAN_HANDLE_IS_VALID (layer->data) || !kan_reflection_patch_get_type (layer->data))
            {
                continue;
            }

            kan_bool_t requirement_met = KAN_TRUE;
            for (kan_loop_size_t requirement_index = 0u; requirement_index < layer->required_tags.size;
                 ++requirement_index)
            {
                kan_interned_string_t requirement =
                    ((kan_interned_string_t *) layer->required_tags.data)[requirement_index];
                kan_bool_t found = KAN_FALSE;

                for (kan_loop_size_t tag_index = 0u; tag_index < universe->environment_tags.size; ++tag_index)
                {
                    kan_interned_string_t tag = ((kan_interned_string_t *) universe->environment_tags.data)[tag_index];

                    if (tag == requirement)
                    {
                        found = KAN_TRUE;
                        break;
                    }
                }

                if (!found)
                {
                    requirement_met = KAN_FALSE;
                    break;
                }
            }

            if (requirement_met)
            {
                if (!output)
                {
                    output = (struct world_configuration_t *) kan_dynamic_array_add_last (&world->configuration);
                    KAN_ASSERT (output)

                    output->name = input->name;
                    output->type = kan_reflection_patch_get_type (layer->data);
                    output->data = kan_allocate_batched (universe->configuration_allocation_group, output->type->size);

                    if (output->type->init)
                    {
                        kan_allocation_group_stack_push (universe->configuration_allocation_group);
                        output->type->init (output->type->functor_user_data, output->data);
                        kan_allocation_group_stack_pop ();
                    }
                }

                if (output->type == kan_reflection_patch_get_type (layer->data))
                {
                    kan_reflection_patch_apply (layer->data, output->data);
                }
                else
                {
                    KAN_LOG (
                        universe, KAN_LOG_ERROR,
                        "Configuration \"%s\": encountered layer that has type \"%s\", but type \"%s\" is expected.",
                        input->name, kan_reflection_patch_get_type (layer->data)->name, output->type->name)
                }

                break;
            }
        }
    }

    kan_dynamic_array_set_capacity (&world->configuration, world->configuration.size);
    world->scheduler_name = definition->scheduler_name;
    struct scheduler_api_node_t *scheduler_node = universe_get_scheduler_api (universe, definition->scheduler_name);

    if (scheduler_node)
    {
        world->scheduler_api = &scheduler_node->api;
        world_scheduler_init (universe, world);
    }
    else
    {
        KAN_LOG (universe, KAN_LOG_ERROR, "Unable to find requested scheduler \"%s\".", definition->scheduler_name)
        world->scheduler_api = NULL;
        world->scheduler_state = NULL;
    }

    kan_dynamic_array_set_capacity (&world->pipelines, definition->pipelines.size);
    for (kan_loop_size_t index = 0u; index < definition->pipelines.size; ++index)
    {
        struct kan_universe_world_pipeline_definition_t *input =
            &((struct kan_universe_world_pipeline_definition_t *) definition->pipelines.data)[index];

        struct pipeline_t *output = (struct pipeline_t *) kan_dynamic_array_add_last (&world->pipelines);
        KAN_ASSERT (output)

        output->name = input->name;
        output->graph = KAN_HANDLE_SET_INVALID (kan_workflow_graph_t);

        kan_dynamic_array_init (&output->used_groups, input->mutator_groups.size, sizeof (kan_interned_string_t),
                                _Alignof (kan_interned_string_t), world->pipelines.allocation_group);

        kan_dynamic_array_init (&output->mutators, input->mutator_groups.size, sizeof (struct mutator_t),
                                _Alignof (struct mutator_t), world->pipelines.allocation_group);

        kan_dynamic_array_init (&output->checkpoint_dependencies, input->checkpoint_dependencies.size,
                                sizeof (struct kan_universe_world_checkpoint_dependency_t),
                                _Alignof (struct kan_universe_world_checkpoint_dependency_t),
                                world->pipelines.allocation_group);

        for (kan_loop_size_t group_index = 0u; group_index < input->mutator_groups.size; ++group_index)
        {
            kan_interned_string_t group_name = ((kan_interned_string_t *) input->mutator_groups.data)[group_index];
            kan_interned_string_t *group_name_output =
                (kan_interned_string_t *) kan_dynamic_array_add_last (&output->used_groups);

            if (!group_name_output)
            {
                kan_dynamic_array_set_capacity (&output->used_groups, output->used_groups.size * 2u);
                group_name_output = (kan_interned_string_t *) kan_dynamic_array_add_last (&output->used_groups);
            }

            KAN_ASSERT (group_name_output)
            *group_name_output = group_name;
            kan_bool_t group_found = KAN_FALSE;

            const struct kan_hash_storage_bucket_t *bucket =
                kan_hash_storage_query (&universe->group_multi_map_storage, KAN_HASH_OBJECT_POINTER (group_name));
            struct group_multi_map_node_t *node = (struct group_multi_map_node_t *) bucket->first;
            const struct group_multi_map_node_t *node_end =
                (struct group_multi_map_node_t *) (bucket->last ? bucket->last->next : NULL);

            while (node != node_end)
            {
                if (node->group_name == group_name)
                {
                    group_found = KAN_TRUE;
                    struct mutator_api_node_t *mutator_node = universe_get_mutator_api (universe, node->mutator);

                    if (mutator_node)
                    {
                        struct mutator_t *mutator = (struct mutator_t *) kan_dynamic_array_add_last (&output->mutators);
                        if (!mutator)
                        {
                            kan_dynamic_array_set_capacity (&output->mutators, output->mutators.size * 2u);
                            mutator = (struct mutator_t *) kan_dynamic_array_add_last (&output->mutators);
                        }

                        KAN_ASSERT (mutator)
                        mutator->name = node->mutator;
                        mutator->api = &mutator_node->api;
                        mutator_init (universe, mutator);
                        mutator->from_group = KAN_TRUE;
                    }
                    else
                    {
                        KAN_LOG (universe, KAN_LOG_ERROR, "Unable to find mutator \"%s\" requested from group \"%s\".",
                                 node->mutator, group_name)
                    }
                }

                node = (struct group_multi_map_node_t *) node->node.list_node.next;
            }

            if (!group_found)
            {
                KAN_LOG (universe, KAN_LOG_ERROR, "Unable to find requested group \"%s\".", group_name)
            }
        }

        kan_dynamic_array_set_capacity (&output->mutators, output->mutators.size + input->mutators.size);
        for (kan_loop_size_t mutator_index = 0u; mutator_index < input->mutators.size; ++mutator_index)
        {
            kan_interned_string_t requested_name = ((kan_interned_string_t *) input->mutators.data)[mutator_index];
            struct mutator_api_node_t *mutator_node = universe_get_mutator_api (universe, requested_name);

            if (mutator_node)
            {
                struct mutator_t *mutator = (struct mutator_t *) kan_dynamic_array_add_last (&output->mutators);
                KAN_ASSERT (mutator)

                mutator->name = requested_name;
                mutator->api = &mutator_node->api;
                mutator_init (universe, mutator);
            }
            else
            {
                KAN_LOG (universe, KAN_LOG_ERROR, "Unable to find requested mutator \"%s\".", requested_name)
            }
        }

        for (kan_loop_size_t dependency_index = 0u; dependency_index < input->checkpoint_dependencies.size;
             ++dependency_index)
        {
            struct kan_universe_world_checkpoint_dependency_t *dependency_input =
                &((struct kan_universe_world_checkpoint_dependency_t *)
                      input->checkpoint_dependencies.data)[dependency_index];

            struct kan_universe_world_checkpoint_dependency_t *dependency_output =
                kan_dynamic_array_add_last (&output->checkpoint_dependencies);

            KAN_ASSERT (dependency_output)
            *dependency_output = *dependency_input;
        }
    }
}

static void update_world_hierarchy_with_overlaps (struct universe_t *universe,
                                                  struct world_t *world_to_update,
                                                  const struct kan_universe_world_definition_t *new_definition)
{
    world_clean_self_preserving_repository (universe, world_to_update);
    fill_world_from_definition (universe, world_to_update, new_definition);

    for (kan_loop_size_t definition_child_index = 0u; definition_child_index < new_definition->children.size;
         ++definition_child_index)
    {
        struct kan_universe_world_definition_t *definition_child =
            &((struct kan_universe_world_definition_t *) new_definition->children.data)[definition_child_index];
        kan_bool_t found_child = KAN_FALSE;

        for (kan_loop_size_t world_child_index = 0u; world_child_index < world_to_update->children.size;
             ++world_child_index)
        {
            struct world_t *world_child = ((struct world_t **) world_to_update->children.data)[world_child_index];
            if (world_child->name == definition_child->world_name)
            {
                found_child = KAN_TRUE;
                update_world_hierarchy_with_overlaps (universe, world_child, definition_child);
                break;
            }
        }

        if (!found_child)
        {
            struct world_t *world_child = world_create (universe, world_to_update, definition_child->world_name);
            update_world_hierarchy_with_overlaps (universe, world_child, definition_child);
        }
    }
}

kan_universe_world_t kan_universe_deploy_root (kan_universe_t universe,
                                               const struct kan_universe_world_definition_t *definition)
{
    struct universe_t *universe_data = KAN_HANDLE_GET (universe);
    KAN_ASSERT (!universe_data->root_world)
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, kan_cpu_section_get ("universe_deploy_root"));

    universe_data->root_world = world_create (universe_data, NULL, definition->world_name);
    update_world_hierarchy_with_overlaps (universe_data, universe_data->root_world, definition);
    world_deploy_hierarchy (universe_data, universe_data->root_world);
    kan_repository_enter_serving_mode (universe_data->root_world->repository);

    kan_cpu_section_execution_shutdown (&execution);
    return KAN_HANDLE_SET (kan_universe_world_t, universe_data->root_world);
}

kan_universe_world_t kan_universe_deploy_child (kan_universe_t universe,
                                                kan_universe_world_t parent,
                                                const struct kan_universe_world_definition_t *definition)
{
    struct universe_t *universe_data = KAN_HANDLE_GET (universe);
    KAN_ASSERT (universe_data->root_world)
    KAN_ASSERT (KAN_HANDLE_IS_VALID (parent))
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, kan_cpu_section_get ("universe_deploy_child"));

    kan_repository_enter_planning_mode (universe_data->root_world->repository);
    struct world_t *deploy_root = world_create (universe_data, KAN_HANDLE_GET (parent), definition->world_name);
    update_world_hierarchy_with_overlaps (universe_data, deploy_root, definition);
    world_deploy_hierarchy (universe_data, deploy_root);

    kan_repository_enter_serving_mode (universe_data->root_world->repository);
    kan_cpu_section_execution_shutdown (&execution);
    return KAN_HANDLE_SET (kan_universe_world_t, deploy_root);
}

kan_universe_world_t kan_universe_redeploy (kan_universe_t universe,
                                            kan_universe_world_t world,
                                            const struct kan_universe_world_definition_t *definition)
{
    struct universe_t *universe_data = KAN_HANDLE_GET (universe);
    KAN_ASSERT (universe_data->root_world)
    KAN_ASSERT (KAN_HANDLE_IS_VALID (world))
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, kan_cpu_section_get ("universe_redeploy"));

    kan_repository_enter_planning_mode (universe_data->root_world->repository);
    struct world_t *deploy_root = KAN_HANDLE_GET (world);
    update_world_hierarchy_with_overlaps (universe_data, deploy_root, definition);
    world_deploy_hierarchy (universe_data, deploy_root);

    kan_repository_enter_serving_mode (universe_data->root_world->repository);
    kan_cpu_section_execution_shutdown (&execution);
    return KAN_HANDLE_SET (kan_universe_world_t, deploy_root);
}

static void update_world (struct world_t *world)
{
    if (!world->scheduler_state)
    {
        return;
    }

    KAN_ASSERT (world->scheduler_api->execute)
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, kan_cpu_section_get (world->name));

    struct kan_universe_scheduler_execute_arguments_t arguments = {
        .interface = KAN_HANDLE_SET (kan_universe_scheduler_interface_t, world),
        .scheduler_state = world->scheduler_state,
    };

    world->scheduler_api->execute->call (world->scheduler_api->execute->call_user_data, NULL, &arguments);
    kan_cpu_section_execution_shutdown (&execution);
}

void kan_universe_update (kan_universe_t universe)
{
    struct universe_t *universe_data = KAN_HANDLE_GET (universe);
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, universe_data->update_section);

    if (universe_data->root_world)
    {
        update_world (universe_data->root_world);
    }

    kan_cpu_section_execution_shutdown (&execution);
}

void kan_universe_undeploy_world (kan_universe_t universe, kan_universe_world_t world)
{
    struct universe_t *universe_data = KAN_HANDLE_GET (universe);
    KAN_ASSERT (universe_data->root_world)
    KAN_ASSERT (KAN_HANDLE_IS_VALID (world))
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, kan_cpu_section_get ("universe_undeploy"));

    world_prepare_repository_for_destroy (universe_data, KAN_HANDLE_GET (world));
    kan_repository_enter_planning_mode (universe_data->root_world->repository);
    world_destroy (universe_data, KAN_HANDLE_GET (world));
    kan_repository_enter_serving_mode (universe_data->root_world->repository);
    kan_cpu_section_execution_shutdown (&execution);
}

void kan_universe_destroy (kan_universe_t universe)
{
    struct universe_t *universe_data = KAN_HANDLE_GET (universe);
    if (universe_data->root_world)
    {
        // We do not need to prepare repository for destroy here as we destroy root repository anyway.
        kan_repository_enter_planning_mode (universe_data->root_world->repository);
        world_destroy (universe_data, universe_data->root_world);
    }

    while (universe_data->scheduler_api_storage.items.first)
    {
        struct scheduler_api_node_t *node =
            (struct scheduler_api_node_t *) universe_data->scheduler_api_storage.items.first;
        kan_hash_storage_remove (&universe_data->scheduler_api_storage, &node->node);
        kan_free_batched (universe_data->api_allocation_group, node);
    }

    while (universe_data->mutator_api_storage.items.first)
    {
        struct mutator_api_node_t *node = (struct mutator_api_node_t *) universe_data->mutator_api_storage.items.first;
        kan_hash_storage_remove (&universe_data->mutator_api_storage, &node->node);
        kan_free_batched (universe_data->api_allocation_group, node);
    }

    kan_hash_storage_shutdown (&universe_data->scheduler_api_storage);
    kan_hash_storage_shutdown (&universe_data->mutator_api_storage);
    kan_hash_storage_shutdown (&universe_data->group_multi_map_storage);
    kan_dynamic_array_shutdown (&universe_data->environment_tags);
    kan_free_general (universe_data->main_allocation_group, universe_data, sizeof (struct universe_t));
}

void kan_universe_scheduler_interface_run_pipeline (kan_universe_scheduler_interface_t interface,
                                                    kan_interned_string_t pipeline_name)
{
    struct world_t *world = KAN_HANDLE_GET (interface);
    struct kan_cpu_section_execution_t execution;
    kan_cpu_section_execution_init (&execution, kan_cpu_section_get (pipeline_name));

    for (kan_loop_size_t pipeline_index = 0u; pipeline_index < world->pipelines.size; ++pipeline_index)
    {
        struct pipeline_t *pipeline = &((struct pipeline_t *) world->pipelines.data)[pipeline_index];
        if (pipeline->name == pipeline_name)
        {
            if (KAN_HANDLE_IS_VALID (pipeline->graph))
            {
                kan_workflow_graph_execute (pipeline->graph);
            }

            break;
        }
    }

    kan_cpu_section_execution_shutdown (&execution);
}

void kan_universe_scheduler_interface_update_all_children (kan_universe_scheduler_interface_t interface)
{
    struct world_t *world = KAN_HANDLE_GET (interface);
    for (kan_loop_size_t child_index = 0u; child_index < world->children.size; ++child_index)
    {
        update_world (((struct world_t **) world->children.data)[child_index]);
    }
}

void kan_universe_scheduler_interface_update_child (kan_universe_scheduler_interface_t interface,
                                                    kan_universe_world_t child)
{
    struct world_t *child_world = KAN_HANDLE_GET (child);
    update_world (child_world);
}
