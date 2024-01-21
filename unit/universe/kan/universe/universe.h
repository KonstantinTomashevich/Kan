#pragma once

#include <universe_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/context/context.h>
#include <kan/reflection/migration.h>
#include <kan/reflection/patch.h>
#include <kan/repository/repository.h>
#include <kan/workflow/workflow.h>

KAN_C_HEADER_BEGIN

struct kan_universe_world_configuration_t
{
    kan_interned_string_t name;
    kan_reflection_patch_t data;
};

UNIVERSE_API void kan_universe_world_configuration_init (struct kan_universe_world_configuration_t *data);

UNIVERSE_API void kan_universe_world_configuration_shutdown (struct kan_universe_world_configuration_t *data);

struct kan_universe_world_pipeline_definition_t
{
    kan_interned_string_t name;

    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t mutators;
};

UNIVERSE_API void kan_universe_world_pipeline_definition_init (struct kan_universe_world_pipeline_definition_t *data);

UNIVERSE_API void kan_universe_world_pipeline_definition_shutdown (
    struct kan_universe_world_pipeline_definition_t *data);

struct kan_universe_world_definition_t
{
    kan_interned_string_t world_name;

    /// \meta reflection_dynamic_array_type = "struct kan_universe_world_configuration_t"
    struct kan_dynamic_array_t configuration;

    kan_interned_string_t scheduler_name;

    /// \meta reflection_dynamic_array_type = "struct kan_universe_world_pipeline_definition_t"
    struct kan_dynamic_array_t pipelines;

    /// \meta reflection_dynamic_array_type = "struct kan_universe_world_definition_t"
    struct kan_dynamic_array_t children;
};

UNIVERSE_API void kan_universe_world_definition_init (struct kan_universe_world_definition_t *data);

UNIVERSE_API void kan_universe_world_definition_shutdown (struct kan_universe_world_definition_t *data);

typedef uint64_t kan_universe_world_t;

#define KAN_INVALID_UNIVERSE_WORLD 0u

typedef uint64_t kan_universe_world_iterator_t;

UNIVERSE_API kan_universe_world_t kan_universe_world_get_parent (kan_universe_world_t world);

UNIVERSE_API kan_interned_string_t kan_universe_world_get_name (kan_universe_world_t world);

UNIVERSE_API const void *kan_universe_world_query_configuration (kan_universe_world_t world,
                                                                 kan_interned_string_t name);

UNIVERSE_API kan_universe_world_iterator_t kan_universe_world_iterator_begin_children (kan_universe_world_t world);

UNIVERSE_API kan_universe_world_t kan_universe_world_iterator_get (kan_universe_world_iterator_t iterator,
                                                                   kan_universe_world_t world);

UNIVERSE_API kan_universe_world_iterator_t kan_universe_world_iterator_next (kan_universe_world_iterator_t iterator);

typedef uint64_t kan_universe_t;

#define KAN_INVALID_UNIVERSE 0u

UNIVERSE_API kan_universe_t kan_universe_create (kan_allocation_group_t group,
                                                 kan_reflection_registry_t registry,
                                                 kan_context_handle_t context);

UNIVERSE_API void kan_universe_migrate (kan_universe_t universe,
                                        kan_reflection_registry_t new_registry,
                                        kan_reflection_migration_seed_t migration_seed,
                                        kan_reflection_struct_migrator_t migrator);

UNIVERSE_API kan_context_handle_t kan_universe_get_context (kan_universe_t universe);

UNIVERSE_API kan_universe_world_t kan_universe_get_root_world (kan_universe_t universe);

UNIVERSE_API kan_universe_world_t kan_universe_deploy_root (kan_universe_t universe,
                                                            struct kan_universe_world_definition_t *definition);

UNIVERSE_API kan_universe_world_t kan_universe_deploy_child (kan_universe_t universe,
                                                             kan_universe_world_t parent,
                                                             struct kan_universe_world_definition_t *definition);

UNIVERSE_API kan_universe_world_t kan_universe_redeploy (kan_universe_t universe,
                                                         kan_universe_world_t world,
                                                         struct kan_universe_world_definition_t *definition);

UNIVERSE_API void kan_universe_update (kan_universe_t universe);

UNIVERSE_API void kan_universe_undeploy_world (kan_universe_t universe, kan_universe_world_t world);

UNIVERSE_API void kan_universe_destroy (kan_universe_t universe);

typedef uint64_t kan_universe_scheduler_interface_t;

struct kan_universe_scheduler_deploy_arguments_t
{
    kan_universe_t universe;
    kan_universe_world_t world;
    kan_repository_t world_repository;
    void *scheduler_state;
};

struct kan_universe_scheduler_execute_arguments_t
{
    kan_universe_scheduler_interface_t interface;
    void *scheduler_state;
};

struct kan_universe_scheduler_undeploy_arguments_t
{
    void *scheduler_state;
};

UNIVERSE_API void kan_universe_scheduler_interface_run_pipeline (kan_universe_scheduler_interface_t interface,
                                                                 kan_interned_string_t pipeline_name);

UNIVERSE_API void kan_universe_scheduler_interface_update_all_children (kan_universe_scheduler_interface_t interface);

UNIVERSE_API void kan_universe_scheduler_interface_update_child (kan_universe_scheduler_interface_t interface,
                                                                 kan_universe_world_t child);

struct kan_universe_mutator_deploy_arguments_t
{
    kan_universe_t universe;
    kan_universe_world_t world;
    kan_repository_t world_repository;
    kan_workflow_graph_node_t workflow_node;
    void *mutator_state;
};

struct kan_universe_mutator_execute_arguments_t
{
    kan_cpu_job_t job;
    void *mutator_state;
};

struct kan_universe_mutator_undeploy_arguments_t
{
    void *mutator_state;
};

struct kan_universe_space_automated_lifetime_query_meta_t
{
    struct kan_repository_field_path_t min_path;
    struct kan_repository_field_path_t max_path;
};

struct kan_universe_space_configuration_t
{
    double global_min;
    double global_max;
    double leaf_size;
};

KAN_C_HEADER_END
