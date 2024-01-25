#pragma once

#include <workflow_api.h>

#include <stdint.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
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
/// \par Access types
/// \parblock
/// There are 3 types of resources accesses for verification:
/// - Insert access. Node inserts resources of given type.
/// - Write access. Node reads, writes and/or deletes resources of given type.
/// - Read access. Node reads resources of given type.
///
/// The rules around accesses are:
/// - Insert access is thread safe, so multiple nodes can insert resource simultaneously. But insert access blocks write
///   and read accesses: read or write access during insertion is considered data access race.
/// - Write access is considered safe from only one node at a time. Any other access of any type to that resource is
///   considered data access race if it can happen simultaneously with write access.
/// - Read access is thread safe, so multiple nodes can read resource simultaneously. However, any access of other type
///   to that resource is considered a data race if it can happen simultaneously with any read access.
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

typedef uint64_t kan_workflow_graph_t;

#define KAN_INVALID_WORKFLOW_GRAPH 0u

typedef uint64_t kan_workflow_graph_builder_t;

#define KAN_INVALID_WORKFLOW_GRAPH_NODE 0u

typedef uint64_t kan_workflow_graph_node_t;

typedef uint64_t kan_workflow_user_data_t;

typedef void (*kan_workflow_function_t) (kan_cpu_job_t job, kan_workflow_user_data_t user_data);

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
                                                        kan_workflow_user_data_t user_data);

/// \brief Informs that given graph node has insert access to resource with given name.
WORKFLOW_API void kan_workflow_graph_node_insert_resource (kan_workflow_graph_node_t node, const char *resource_name);

/// \brief Informs that given graph node has write access to resource with given name.
WORKFLOW_API void kan_workflow_graph_node_write_resource (kan_workflow_graph_node_t node, const char *resource_name);

/// \brief Informs that given graph node has read access to resource with given name.
WORKFLOW_API void kan_workflow_graph_node_read_resource (kan_workflow_graph_node_t node, const char *resource_name);

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
