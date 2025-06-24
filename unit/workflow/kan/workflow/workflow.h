#pragma once

#include <workflow_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/cpu_dispatch/job.h>
#include <kan/memory_profiler/allocation_group.h>

/// \file
/// \brief Contains API for workflow unit -- data processing graph library.
///
/// \par Definition
/// \parblock
/// Repeated operations on data, for example game normal and fixed updates, can be represented as directional graphs
/// of executable nodes where each edge describes dependency -- source node is a dependency of the target node.
/// Workflow unit provides API to build and execute such graphs along with optional verification that checks for
/// cycles and data races inside graph.
/// \endparblock
///
/// \par Nodes
/// \parblock
/// Node is a primary concept of workflow -- it is a graph node with attached name, function, resources access mask and
/// dependencies. Every node name must be unique inside its graph as it is used to specify dependencies between nodes.
/// Function is described in separate paragraph.
///
/// Resources are specified as string names and are used to validate graph for data races during building process.
/// It is advised to use structure names as resource names to avoid spelling mistakes.
///
/// Dependencies are specified as node names. If node with given name is found when graph is finalized, dependency
/// with that node will be created. Otherwise, special build-time-only checkpoint "ghost node" will be created.
/// Checkpoints are used to make dependency specification easier and more flexible by introducing universal names
/// like "render_begin" and "render_end" instead of specific node names. Also, dependencies between checkpoints can
/// be specified through `kan_workflow_graph_builder_register_checkpoint_dependency`. Checkpoints are optimized out
/// during graph finalization, therefore there is no execution cost for checkpoints in result graph. Also, node
/// dependencies can be specified as "depends on" and "dependency of", making it possible to inject dependencies
/// to other nodes too.
/// \endparblock
///
/// \par Node function
/// \parblock
/// Node function is a pointer to function that accepts two arguments: cpu job handle and 64-bit user data. Job is
/// always in assembly state, therefore new tasks can be attached to it. It makes it possible to implement multithreaded
/// nodes: for example to process every player movement as separate cpu task. After all required tasks were added to
/// job, it must be released. Release should be done even if there were no cpu tasks. Also, release can be safely called
/// from any spawned cpu task. The only requirement is that job must be released to finish node execution: job
/// completion task is used to continue graph execution.
/// \endparblock
///
/// \par Access classes
/// \parblock
/// There are 3 types of resources access classes for verification:
/// - Population access. Node either inserts resources of given type or uses so-called detach operation:
///   deletes instance of resource that is exclusively referenced by other resource to which this node
///   has modification access.
/// - View access. Node reads data from arbitrary instances of resources of given type, but does not modifies them.
/// - Modification access. Node can read, write and delete arbitrary instances of resources of given type.
///
/// One node can declare several access classes to resource type and it will not be considered an error.
///
/// Using accesses, workflow ensures rules that prevent race conditions from happening. If any of the rules below is
/// broken in graph, error is raised and user should manually add dependencies to nodes to solve race condition
/// properly and in deterministic way.
///
/// - If node has population access to resource type, it cannot be executed in parallel with nodes that have
///   view or modification access type to this resource type.
///
/// - If node has view access to resource type, it cannot be executed in parallel with nodes that have
///   population or modification access type to this resource type.
///
/// - If node has modification access to resource type, it cannot be executed in parallel with nodes that have
///   any access to this resource type.
///
/// Explanation:
///
/// - Several nodes with population access cannot have race condition as their access patterns to that resource can
///   never overlap. Other accesses might introduce overlaps (for example, reading all resources of type while this
///   node is inserting them) and therefore are not safe.
///
/// - Several nodes with view access cannot have race condition as they do not modify this resource type. However,
///   they cannot be executed in parallel with population nodes as it will result in race condition as population
///   inserts or deletes can affect view context.
///
/// - Modification can never be safely executed in parallel as different nodes have no tools to properly synchronize
///   their modifications.
///
/// There is also one interesting case: custom synchronization primitives on resource instance level. If modification
/// is only executed on data that is synchronized through this primitives, view access should be used instead of
/// modification access, because no unguarded modification happens (only guarded modification). However, it is advised
/// to avoid introducing this pattern unless it is absolutely required for performance or architecture, because it
/// introduces more space for user errors.
/// \endparblock
///
/// \par Graph building
/// \parblock
/// To build graph, you need to create graph builder using `kan_workflow_graph_builder_create`. Then you can create
/// nodes using `kan_workflow_graph_node_create`. After node configuration, it must be either submitted using
/// `kan_workflow_graph_node_submit` or destroyed using `kan_workflow_graph_node_destroy` if it shouldn't be added
/// to graph for some reason. Keep in mind that every node must specify its function through
/// `kan_workflow_graph_node_set_function`.
///
/// Creation, configuration and submission of nodes is thread safe: multiple different nodes can be safely created
/// from different threads and submitted to graph builder from that threads.
///
/// Dependencies between checkpoints can be specified through
/// `kan_workflow_graph_builder_register_checkpoint_dependency`, but it is not thread safe.
///
/// After submitting all the nodes, you need to finalize build process in order to receive ready-to-use workflow
/// graph. To do it, call `kan_workflow_graph_builder_finalize`. If graph contains errors, `KAN_INVALID_WORKFLOW_GRAPH`
/// will be returned. This function will also clean everything in graph builder, therefore builder can be used again
/// after that or destroyed using `kan_workflow_graph_builder_destroy`.
/// \endparblock
///
/// \par Graph execution
/// \parblock
/// After you've received valid graph from `kan_workflow_graph_builder_finalize`, you can execute it using
/// `kan_workflow_graph_execute`. Graph is reusable, therefore you can execute it as many times as you need. But it is
/// not thread safe: graph execution should only be called from one thread at a time. Also, it is advised not to
/// execute multiple graphs at once, as every graph uses cpu dispatch for scheduling node functions as foreground tasks,
/// therefore multiple graph execution will overflow cpu with work. When graph is no longer needed, it should be
/// destroyed using `kan_workflow_graph_destroy`.
/// \endparblock

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_workflow_graph_t);
KAN_HANDLE_DEFINE (kan_workflow_graph_builder_t);
KAN_HANDLE_DEFINE (kan_workflow_graph_node_t);

typedef void (*kan_workflow_function_t) (kan_cpu_job_t job, kan_functor_user_data_t user_data);

/// \brief Enumerates access classes for resources accessed from workflow nodes.
enum kan_workflow_resource_access_class_t
{
    KAN_WORKFLOW_RESOURCE_ACCESS_CLASS_POPULATION = 0u,
    KAN_WORKFLOW_RESOURCE_ACCESS_CLASS_VIEW,
    KAN_WORKFLOW_RESOURCE_ACCESS_CLASS_MODIFICATION,
};

/// \brief Creates new graph builder instance that uses given allocation group to allocate graph.
WORKFLOW_API kan_workflow_graph_builder_t kan_workflow_graph_builder_create (kan_allocation_group_t group);

/// \brief Adds dependency between two given checkpoints inside graph builder.
WORKFLOW_API kan_bool_t kan_workflow_graph_builder_register_checkpoint_dependency (kan_workflow_graph_builder_t builder,
                                                                                   const char *dependency_checkpoint,
                                                                                   const char *dependant_checkpoint);

/// \brief Finalizes graph building process and attempts to build graph from submitted data.
/// \warning This function clears all submitted data after execution.
WORKFLOW_API kan_workflow_graph_t kan_workflow_graph_builder_finalize (kan_workflow_graph_builder_t builder);

/// \brief Destroys graph builder instance. Does not affect finalized graphs.
WORKFLOW_API void kan_workflow_graph_builder_destroy (kan_workflow_graph_builder_t builder);

/// \brief Creates new graph node with given name inside given graph builder context.
WORKFLOW_API kan_workflow_graph_node_t kan_workflow_graph_node_create (kan_workflow_graph_builder_t builder,
                                                                       const char *name);

/// \brief Sets given graph node function and user data.
WORKFLOW_API void kan_workflow_graph_node_set_function (kan_workflow_graph_node_t node,
                                                        kan_workflow_function_t function,
                                                        kan_functor_user_data_t user_data);

/// \brief Informs that given graph node has access of given class to resource with given name.
WORKFLOW_API void kan_workflow_graph_node_register_access (kan_workflow_graph_node_t node,
                                                           const char *resource_name,
                                                           enum kan_workflow_resource_access_class_t access_class);

/// \brief Informs that given node depends on other node or checkpoint with given name.
WORKFLOW_API void kan_workflow_graph_node_depend_on (kan_workflow_graph_node_t node, const char *name);

/// \brief Informs that given node is a dependency of other node or checkpoint with given name.
WORKFLOW_API void kan_workflow_graph_node_make_dependency_of (kan_workflow_graph_node_t node, const char *name);

/// \brief Submits given graph node to its graph builder.
WORKFLOW_API kan_bool_t kan_workflow_graph_node_submit (kan_workflow_graph_node_t node);

/// \brief Destroys given graph node and discards it from graph builder.
WORKFLOW_API void kan_workflow_graph_node_destroy (kan_workflow_graph_node_t node);

/// \brief Executes given graph instance. Returns when all nodes have finished execution.
WORKFLOW_API void kan_workflow_graph_execute (kan_workflow_graph_t graph);

/// \brief Destroys given graph instance.
WORKFLOW_API void kan_workflow_graph_destroy (kan_workflow_graph_t graph);

KAN_C_HEADER_END
