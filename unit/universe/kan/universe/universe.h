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

/// \file
/// \brief Contains API for universe unit -- application state and logic management framework.
///
/// \par Definition
/// \parblock
/// Universe unites repository and workflow capabilities in order to create framework that manages whole application
/// state as tree-like hierarchy of worlds.
/// \endparblock
///
/// \par World
/// \parblock
/// Every world consists of:
/// - Repository instance with world data.
/// - Pipelines -- instances of workflow graphs built from mutators.
/// - Scheduler -- logical object that decides which pipelines to execute during update.
/// - Configuration -- arbitrary configuration, specified during world deployment, that is used to initialize
///   scheduler and pipelines.
///
/// Worlds are organized in tree-like hierarchy: there is a root world for every universe and every non-root world
/// is a child of other world. Repository hierarchy mimics world hierarchy: repositories of child worlds are children
/// of parent world repository.
/// \endparblock
///
/// \par Mutator
/// \parblock
/// Mutator is a logical unit that uses repository queries to update its world state. Usually, there is a lot of
/// mutators that implement separate features and form application logic as a whole. Mutator lifecycle consists
/// of several stages:
///
/// - Initialization. Mutator state is initialized as a structure, it is not yet connected to any world.
/// - Deployment. Mutator is connected to world, its repository and appropriate pipeline.
/// - Execution. Mutator can be executed by world update whenever it is needed.
/// - Undeployment. In this stage mutator should shutdown everything that was attached to world, for example queries.
/// - Shutdown. Mutator state is being shutdown as a structure.
///
/// Initialization and Shutdown stages are reflection-driven and work for mutator states the same way as for any other
/// structs, therefore we won't cover them in detail.
///
/// Deployment stage consists of automatic lifetime query deployment, that will be discussed below, and manual
/// deployment function execution if it is found in reflection registry. Manual deployment function must be named
/// `kan_universe_mutator_deploy_<you_mutator_name>`, return `void` and accept arguments listed in
/// `kan_universe_mutator_deploy_arguments_t` with one exception -- mutator state should have your mutator state type.
/// For example:
///
/// ```c
/// MY_UNIT_API void kan_universe_mutator_deploy_my_mutator_name (
///     kan_universe_t universe,
///     kan_universe_world_t world,
///     kan_repository_t world_repository,
///     kan_workflow_graph_node_t workflow_node,
///     struct my_mutator_state_t *state)
/// {
///     kan_workflow_graph_node_make_dependency_of (workflow_node, "my_other_mutator_name");
/// }
/// ```
///
/// Execution function for workflow node is assigned automatically, therefore deploy function only needs to register
/// node dependencies and resource accesses (unless resourced are accessed through automatic lifetime queries). Keep
/// in mind that deploy function is totally optional and may be omitted.
///
/// Execution stage works the same way as workflow graph execution: mutator execution function is called from available
/// worked thread and receives cpu job and its state. Execution function must be named
/// `kan_universe_mutator_execute_<you_mutator_name>`, return `void` and accept arguments listed in
/// `kan_universe_mutator_execute_arguments_t` with one exception -- mutator state should have your mutator state type.
/// For example:
///
/// ```c
/// MY_UNIT_API void kan_universe_mutator_execute_my_mutator_name (kan_cpu_job_t job, struct my_mutator_state_t *state)
/// {
///     // Do whatever you need here.
///     // Do not forget to release job, this is default requirement for workflow graph nodes.
///     kan_cpu_job_release (job);
/// }
/// ```
///
/// Undeployment stage is similar to deployment: first, automatic lifetime queries are being undeployed, then
/// manual undeployment function is being called if it is found in reflection registry. Manual undeployment function
/// must be named `kan_universe_mutator_undeploy_<you_mutator_name>`, return `void` and accept arguments listed in
/// `kan_universe_mutator_undeploy_arguments_t` with one exception -- mutator state should have your mutator state type.
/// For example:
///
/// ```c
/// MY_UNIT_API void kan_universe_mutator_my_mutator_name (struct my_mutator_state_t *state)
/// {
///     // Do whatever you need here.
/// }
/// ```
///
/// The difference between undeployment and deinitialization is that after undeployment any mutator can be deployed
/// again with the same state, but after deinitialization state is destroyed and therefore mutator does not exist
/// any longer. Also, manual undeployment function is optional.
/// \endparblock
///
/// \par Pipeline
/// \parblock
/// Pipeline is a collection of mutators that form workflow graph and have the same logical purpose, for example there
/// can be game fixed (logical) update pipeline and game normal (visual) update pipeline. Pipelines are executed one
/// by one and whether they'll be executed or not is decided by scheduler.
/// \endparblock
///
/// \par Scheduler
/// \parblock
/// Scheduler is a special world part that runs pipelines during update and decides whether to update child worlds.
/// Scheduler lifetime is similar to mutator lifetime -- it also consists of initialization, deployment, execution,
/// undeployment and deinitialization stages with similar logic except for execution.
///
/// Scheduler deployment manual function is optional and must be named
/// `kan_universe_scheduler_deploy_<you_scheduler_name>`, return `void` and accept listed in
/// `kan_universe_scheduler_deploy_arguments_t` with one exception -- scheduler state should have your scheduler state 
/// type. It solves the same purpose as mutator deployment except it is not attached to any workflow graph.
///
/// Scheduler execution manual function must be named kan_universe_scheduler_execute_<you_scheduler_name>`, return
/// `void` and accept listed in `kan_universe_scheduler_execute_arguments_t` with one exception -- scheduler state 
/// should have your scheduler state type. For example:
///
/// ```c
/// MY_UNIT_API void kan_universe_scheduler_execute_my_scheduler_name (
///     kan_universe_scheduler_interface_t interface, struct my_scheduler_state_t *state)
/// {
///     kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern ("update"));
///     kan_repository_singleton_read_access_close (access);
/// }
/// ```
///
/// Scheduler interface provides scheduler function with the abilities to execute pipeline by name, update single
/// child world or update all child worlds at once.
///
/// Scheduler undeployment manual function is optional and must be named
/// `kan_universe_scheduler_undeploy_<you_scheduler_name>`, return `void` and accept listed in
/// `kan_universe_scheduler_undeploy_arguments_t` with one exception -- scheduler state should have your scheduler 
/// state type. It solves the same purpose as mutator undeployment.
/// \endparblock
///
/// \par Automatic lifetime queries
/// \parblock
/// Automatic lifetime queries are repository queries that are fields of mutators and schedulers and are named according
/// to automatic lifetime query guidelines. Naming guidelines make it possible for universe to automatically initialize
/// and shutdown these queries during deployment and undeployment, which allows programmer to omit writing these calls.
/// Below we describe naming guidelines for every query type:
///
/// - `kan_repository_singleton_read_query_t`: `read__<record_type_name>`, where `_t` suffix for type name might be
///   omitted. For example: `struct kan_repository_singleton_read_query_t read__counters_singleton;`.
///
/// - `kan_repository_singleton_write_query_t`: `write__<record_type_name>`, where `_t` suffix for type name might be
///   omitted. For example: `struct kan_repository_singleton_write_query_t write__counters_singleton;`.
///
/// - `kan_repository_indexed_insert_query_t`: `insert__<record_type_name>`, where `_t` suffix for type name might be
///   omitted. For example: `struct kan_repository_indexed_insert_query_t insert__object_record;`.
///
/// - `kan_repository_indexed_sequence_read_query_t`: `read_sequence__<record_type_name>`, where `_t` suffix for type 
///   name might be omitted. For example: 
///   `struct kan_repository_indexed_sequence_read_query_t read_sequence__status_record;`.
///
/// - `kan_repository_indexed_sequence_update_query_t`: `update_sequence__<record_type_name>`, where `_t` suffix for
///   type name might be omitted. For example: 
///   `struct kan_repository_indexed_sequence_update_query_t update_sequence__status_record;`.
///
/// - `kan_repository_indexed_sequence_delete_query_t`: `delete_sequence__<record_type_name>`, where `_t` suffix for
///   type name might be omitted. For example: 
///   `struct kan_repository_indexed_sequence_delete_query_t delete_sequence__status_record;`.
///
/// - `kan_repository_indexed_sequence_write_query_t`: `write_sequence__<record_type_name>`, where `_t` suffix for
///   type name might be omitted. For example: 
///   `struct kan_repository_indexed_sequence_write_query_t write_sequence__status_record;`.
///
/// - `kan_repository_indexed_value_read_query_t`: `read_value__<record_type_name>__<path_to_field>`, where `_t` suffix
///   for type name might be omitted and field names in path are separated by `__`. For example: 
///   `struct kan_repository_indexed_value_read_query_t read_value__object_record__some_struct__some_child;`.
///
/// - `kan_repository_indexed_value_update_query_t`: `update_value__<record_type_name>__<path_to_field>`, where `_t`
///   suffix for type name might be omitted and field names in path are separated by `__`. For example: 
///   `struct kan_repository_indexed_value_update_query_t update_value__object_record__some_struct__some_child;`.
///
/// - `kan_repository_indexed_value_delete_query_t`: `delete_value__<record_type_name>__<path_to_field>`, where `_t`
///   suffix for type name might be omitted and field names in path are separated by `__`. For example: 
///   `struct kan_repository_indexed_value_delete_query_t delete_value__object_record__some_struct__some_child;`.
///
/// - `kan_repository_indexed_value_write_query_t`: `write_value__<record_type_name>__<path_to_field>`, where `_t`
///   suffix for type name might be omitted and field names in path are separated by `__`. For example: 
///   `struct kan_repository_indexed_value_write_query_t write_value__object_record__some_struct__some_child;`.
///
/// - `kan_repository_indexed_signal_read_query_t`: `read_signal__<record_type_name>__<path_to_field>__<literal>`, 
///   where `_t` suffix for type name might be omitted, field names in path are separated by `__` and literal is 
///   unsigned signal value. For example: 
///   `struct kan_repository_indexed_signal_read_query_t read_signal__object_record__some_field__1;`.
///
/// - `kan_repository_indexed_signal_update_query_t`: `update_signal__<record_type_name>__<path_to_field>__<literal>`,
///   where `_t` suffix for type name might be omitted, field names in path are separated by `__` and literal is 
///   unsigned signal value. For example: 
///   `struct kan_repository_indexed_signal_update_query_t update_signal__object_record__some_field__1;`.
///
/// - `kan_repository_indexed_signal_delete_query_t`: `delete_signal__<record_type_name>__<path_to_field>__<literal>`,
///   where `_t` suffix for type name might be omitted, field names in path are separated by `__` and literal is 
///   unsigned signal value. For example: 
///   `struct kan_repository_indexed_signal_delete_query_t delete_signal__object_record__some_field__1;`.
///
/// - `kan_repository_indexed_signal_write_query_t`: `write_signal__<record_type_name>__<path_to_field>__<literal>`,
///   where `_t` suffix for type name might be omitted, field names in path are separated by `__` and literal is 
///   unsigned signal value. For example: 
///   `struct kan_repository_indexed_signal_write_query_t write_signal__object_record__some_field__1;`.
///
/// - `kan_repository_indexed_interval_read_query_t`: `read_interval__<record_type_name>__<path_to_field>`, where `_t` 
///   suffix for type name might be omitted and field names in path are separated by `__`. For example: 
///   `struct kan_repository_indexed_interval_read_query_t read_interval__object_record__some_struct__some_child;`.
///
/// - `kan_repository_indexed_interval_update_query_t`: `update_interval__<record_type_name>__<path_to_field>`, where 
///   `_t` suffix for type name might be omitted and field names in path are separated by `__`. For example: 
///   `struct kan_repository_indexed_interval_update_query_t update_interval__object_record__some_struct__some_child;`.
///
/// - `kan_repository_indexed_interval_delete_query_t`: `delete_interval__<record_type_name>__<path_to_field>`, where 
///   `_t` suffix for type name might be omitted and field names in path are separated by `__`. For example: 
///   `struct kan_repository_indexed_interval_delete_query_t delete_interval__object_record__some_struct__some_child;`.
///
/// - `kan_repository_indexed_interval_write_query_t`: `write_interval__<record_type_name>__<path_to_field>`, where `_t`
///   suffix for type name might be omitted and field names in path are separated by `__`. For example: 
///   `struct kan_repository_indexed_interval_write_query_t write_interval__object_record__some_struct__some_child;`.
///
/// - `kan_repository_indexed_space_read_query_t`: `read_space__<record_type_name>__<space_config_name>`, where `_t` 
///   suffix for type name might be omitted, query field must have `kan_universe_space_automated_lifetime_query_meta_t`
///   meta with parameters for creating the query and space config name is a name for config of type
///   `kan_universe_space_configuration_t` that will be used to get space borders for the query creation. For example: 
///   `struct kan_repository_indexed_space_read_query_t read_space__object_record__render_2d_space;`.
///
/// - `kan_repository_indexed_space_update_query_t`: `update_space__<record_type_name>__<space_config_name>`, where `_t`
///   suffix for type name might be omitted, query field must have `kan_universe_space_automated_lifetime_query_meta_t`
///   meta with parameters for creating the query and space config name is a name for config of type
///   `kan_universe_space_configuration_t` that will be used to get space borders for the query creation. For example: 
///   `struct kan_repository_indexed_space_update_query_t update_space__object_record__render_2d_space;`.
///
/// - `kan_repository_indexed_space_delete_query_t`: `delete_space__<record_type_name>__<space_config_name>`, where `_t`
///   suffix for type name might be omitted, query field must have `kan_universe_space_automated_lifetime_query_meta_t`
///   with parameters for creating the query and space config name is a name for config of type 
///   `kan_universe_space_configuration_t` that will be used to get space borders for the query creation. For example: 
///   `struct kan_repository_indexed_space_delete_query_t delete_space__object_record__render_2d_space;`.
///
/// - `kan_repository_indexed_space_write_query_t`: `write_space__<record_type_name>__<space_config_name>`, where `_t` 
///   suffix for type name might be omitted, query field must have `kan_universe_space_automated_lifetime_query_meta_t`
///   meta with parameters for creating the query and space config name is a name for config of type
///   `kan_universe_space_configuration_t` that will be used to get space borders for the query creation. For example: 
///   `struct kan_repository_indexed_space_write_query_t write_space__object_record__render_2d_space;`.
///
/// - `kan_repository_event_insert_query_t`: `insert__<record_type_name>`, where `_t` suffix for type name might be
///   omitted. For example: `struct kan_repository_event_insert_query_t insert__manual_event;`.
///
/// - `kan_repository_event_fetch_query_t`: `fetch__<record_type_name>`, where `_t` suffix for type name might be
///   omitted. For example: `struct kan_repository_event_fetch_query_t fetch__manual_event;`.
/// \endparblock
///
/// \par World deployment
/// \parblock
/// Procedure of adding worlds to universe is called world deployment. In order to deploy a world, user needs to
/// construct `struct kan_universe_world_definition_t` with all relevant world information, including pipelines and
/// child worlds if any. Mutators and schedulers will be picked up from reflection by their names.
///
/// When definition is ready, it can be deployed as a root world through `kan_universe_deploy_root` or as child of
/// other world through `kan_universe_deploy_child`.
///
/// Every world, including root world, can be undeployed through `kan_universe_undeploy_world`. Child worlds of this
/// world will be automatically undeployed too.
///
/// In case where world pipelines must be changed without dropping out the repository data, redeploy must be performed
/// through `kan_universe_redeploy`. It will replace pipelines, mutators and scheduler without changing any data.
/// If redeployment definition has children, child worlds with the same names will be recursively redeployed. If there
/// is no child worlds with required names, new worlds will be created.
/// \endparblock
///
/// \par Migration
/// \parblock
/// Universe supports automatic migration of data, mutators and schedulers to new reflection registry. It can be done
/// by calling `kan_universe_migrate`. Keep in mind that migration is a long and heavy operation.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// Universe api follows general thread-safety rule: functions that do not change any data are thread safe unless
/// there is any simultaneous data-changing operation. Universe is designed to be singleton and to manage all
/// multithreading through workflow graphs, therefore it is not particularly designed for being used from multiple
/// threads.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Contains world configuration name and patch with its data.
struct kan_universe_world_configuration_t
{
    kan_interned_string_t name;
    kan_reflection_patch_t data;
};

UNIVERSE_API void kan_universe_world_configuration_init (struct kan_universe_world_configuration_t *data);

UNIVERSE_API void kan_universe_world_configuration_shutdown (struct kan_universe_world_configuration_t *data);

/// \brief Contains pipeline name and names of mutators inside this pipeline.
struct kan_universe_world_pipeline_definition_t
{
    kan_interned_string_t name;

    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t mutators;
};

UNIVERSE_API void kan_universe_world_pipeline_definition_init (struct kan_universe_world_pipeline_definition_t *data);

UNIVERSE_API void kan_universe_world_pipeline_definition_shutdown (
    struct kan_universe_world_pipeline_definition_t *data);

/// \brief Contains whole world definition with possible child worlds.
struct kan_universe_world_definition_t
{
    /// \brief World name. Cannot be changed later.
    kan_interned_string_t world_name;

    /// \brief Array of configuration for this world deployment.
    /// \meta reflection_dynamic_array_type = "struct kan_universe_world_configuration_t"
    struct kan_dynamic_array_t configuration;

    /// \brief Name of scheduler to be used with this world.
    kan_interned_string_t scheduler_name;

    /// \brief Array of pipelines for this world.
    /// \meta reflection_dynamic_array_type = "struct kan_universe_world_pipeline_definition_t"
    struct kan_dynamic_array_t pipelines;

    /// \brief Array of child worlds definitions if any.
    /// \meta reflection_dynamic_array_type = "struct kan_universe_world_definition_t"
    struct kan_dynamic_array_t children;
};

UNIVERSE_API void kan_universe_world_definition_init (struct kan_universe_world_definition_t *data);

UNIVERSE_API void kan_universe_world_definition_shutdown (struct kan_universe_world_definition_t *data);

typedef uint64_t kan_universe_world_t;

#define KAN_INVALID_UNIVERSE_WORLD 0u

typedef uint64_t kan_universe_world_iterator_t;

/// \brief Returns parent world of given world if any.
UNIVERSE_API kan_universe_world_t kan_universe_world_get_parent (kan_universe_world_t world);

/// \brief Returns name of given world.
UNIVERSE_API kan_interned_string_t kan_universe_world_get_name (kan_universe_world_t world);

/// \brief Queries configuration with given name inside given world configuration and its parents configurations.
UNIVERSE_API const void *kan_universe_world_query_configuration (kan_universe_world_t world,
                                                                 kan_interned_string_t name);

/// \brief Returns iteration to the first child world of given world.
UNIVERSE_API kan_universe_world_iterator_t kan_universe_world_iterator_begin_children (kan_universe_world_t world);

/// \brief Returns child world to which this iterator points or
///        `KAN_INVALID_UNIVERSE_WORLD` if there is no more children.
UNIVERSE_API kan_universe_world_t kan_universe_world_iterator_get (kan_universe_world_iterator_t iterator,
                                                                   kan_universe_world_t world);

/// \brief Moves child world iterator to the next child world.
UNIVERSE_API kan_universe_world_iterator_t kan_universe_world_iterator_next (kan_universe_world_iterator_t iterator);

typedef uint64_t kan_universe_t;

#define KAN_INVALID_UNIVERSE 0u

/// \brief Creates new universe bound to given context and given registry.
UNIVERSE_API kan_universe_t kan_universe_create (kan_allocation_group_t group,
                                                 kan_reflection_registry_t registry,
                                                 kan_context_handle_t context);

/// \brief Migrates universe to new reflection registry along with all data, all mutators and all schedulers.
UNIVERSE_API void kan_universe_migrate (kan_universe_t universe,
                                        kan_reflection_registry_t new_registry,
                                        kan_reflection_migration_seed_t migration_seed,
                                        kan_reflection_struct_migrator_t migrator);

/// \brief Returns context to which this universe is bound.
UNIVERSE_API kan_context_handle_t kan_universe_get_context (kan_universe_t universe);

/// \brief Returns root world of the universe if any.
UNIVERSE_API kan_universe_world_t kan_universe_get_root_world (kan_universe_t universe);

/// \brief Deploys given definition as root world.
/// \invariant There is no root world in this universe.
UNIVERSE_API kan_universe_world_t kan_universe_deploy_root (kan_universe_t universe,
                                                            struct kan_universe_world_definition_t *definition);

/// \brief Deploys given definition as child of given parent world.
UNIVERSE_API kan_universe_world_t kan_universe_deploy_child (kan_universe_t universe,
                                                             kan_universe_world_t parent,
                                                             struct kan_universe_world_definition_t *definition);

/// \brief Redeploys given definition inside given world by updating mutators and schedulers without changing data.
/// \invariant Definition world name is equal to given world name.
UNIVERSE_API kan_universe_world_t kan_universe_redeploy (kan_universe_t universe,
                                                         kan_universe_world_t world,
                                                         struct kan_universe_world_definition_t *definition);

/// \brief Runs scheduler of root world if it exists in order to update worlds data.
UNIVERSE_API void kan_universe_update (kan_universe_t universe);

/// \brief Undeploys given world and removes it from world hierarchy along with all its children.
UNIVERSE_API void kan_universe_undeploy_world (kan_universe_t universe, kan_universe_world_t world);

/// \brief Destroys given universe and all its worlds.
UNIVERSE_API void kan_universe_destroy (kan_universe_t universe);

typedef uint64_t kan_universe_scheduler_interface_t;

/// \brief Contains list of arguments for scheduler deploy function.
struct kan_universe_scheduler_deploy_arguments_t
{
    kan_universe_t universe;
    kan_universe_world_t world;
    kan_repository_t world_repository;
    void *scheduler_state;
};

/// \brief Contains list of arguments for scheduler execute function.
struct kan_universe_scheduler_execute_arguments_t
{
    kan_universe_scheduler_interface_t interface;
    void *scheduler_state;
};

/// \brief Contains list of arguments for scheduler undeploy function.
struct kan_universe_scheduler_undeploy_arguments_t
{
    void *scheduler_state;
};

/// \brief Commands scheduler interface to execute pipeline with given name.
UNIVERSE_API void kan_universe_scheduler_interface_run_pipeline (kan_universe_scheduler_interface_t interface,
                                                                 kan_interned_string_t pipeline_name);

/// \brief Commands scheduler interface to execute schedulers of all child worlds.
UNIVERSE_API void kan_universe_scheduler_interface_update_all_children (kan_universe_scheduler_interface_t interface);

/// \brief Commands scheduler interface to execute scheduler of specific child world.
UNIVERSE_API void kan_universe_scheduler_interface_update_child (kan_universe_scheduler_interface_t interface,
                                                                 kan_universe_world_t child);

/// \brief Contains list of arguments for mutator deploy function.
struct kan_universe_mutator_deploy_arguments_t
{
    kan_universe_t universe;
    kan_universe_world_t world;
    kan_repository_t world_repository;
    kan_workflow_graph_node_t workflow_node;
    void *mutator_state;
};

/// \brief Contains list of arguments for mutator execute function.
struct kan_universe_mutator_execute_arguments_t
{
    kan_cpu_job_t job;
    void *mutator_state;
};

/// \brief Contains list of arguments for mutator undeploy function.
struct kan_universe_mutator_undeploy_arguments_t
{
    void *mutator_state;
};

/// \brief Meta for automatic lifetime space queries with paths to their fields.
struct kan_universe_space_automated_lifetime_query_meta_t
{
    struct kan_repository_field_path_t min_path;
    struct kan_repository_field_path_t max_path;
};

/// \brief Configuration for automatic lifetime space queries with their bounds.
struct kan_universe_space_configuration_t
{
    double global_min;
    double global_max;
    double leaf_size;
};

KAN_C_HEADER_END
