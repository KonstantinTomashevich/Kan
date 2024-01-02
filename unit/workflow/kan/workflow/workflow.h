#pragma once

#include <workflow_api.h>

#include <stdint.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/cpu_dispatch/job.h>
#include <kan/memory_profiler/allocation_group.h>

KAN_C_HEADER_BEGIN

typedef uint64_t kan_workflow_graph_t;

#define KAN_INVALID_WORKFLOW_GRAPH 0u

typedef uint64_t kan_workflow_graph_builder_t;

typedef uint64_t kan_workflow_graph_node_t;

typedef uint64_t kan_workflow_user_data_t;

typedef void (*kan_workflow_function_t) (kan_cpu_job_t job, kan_workflow_user_data_t user_data);

WORKFLOW_API kan_workflow_graph_builder_t kan_workflow_graph_builder_create (kan_allocation_group_t group);

WORKFLOW_API kan_bool_t kan_workflow_graph_builder_register_checkpoint_dependency (kan_workflow_graph_builder_t builder,
                                                                                   const char *dependency_checkpoint,
                                                                                   const char *dependant_checkpoint);

WORKFLOW_API kan_workflow_graph_t kan_workflow_graph_builder_finalize (kan_workflow_graph_builder_t builder);

WORKFLOW_API void kan_workflow_graph_builder_destroy (kan_workflow_graph_builder_t builder);

WORKFLOW_API kan_workflow_graph_node_t kan_workflow_graph_node_create (kan_workflow_graph_builder_t builder,
                                                                       const char *name);

WORKFLOW_API void kan_workflow_graph_node_set_function (kan_workflow_graph_node_t node,
                                                        kan_workflow_function_t function,
                                                        kan_workflow_user_data_t user_data);

WORKFLOW_API void kan_workflow_graph_node_insert_resource (kan_workflow_graph_node_t node, const char *resource_name);

WORKFLOW_API void kan_workflow_graph_node_write_resource (kan_workflow_graph_node_t node, const char *resource_name);

WORKFLOW_API void kan_workflow_graph_node_read_resource (kan_workflow_graph_node_t node, const char *resource_name);

WORKFLOW_API void kan_workflow_graph_node_depend_on (kan_workflow_graph_node_t node, const char *name);

WORKFLOW_API void kan_workflow_graph_node_make_dependency_of (kan_workflow_graph_node_t node, const char *name);

WORKFLOW_API kan_bool_t kan_workflow_graph_node_submit (kan_workflow_graph_node_t node);

WORKFLOW_API void kan_workflow_graph_node_destroy (kan_workflow_graph_node_t node);

WORKFLOW_API void kan_workflow_graph_execute (kan_workflow_graph_t graph);

WORKFLOW_API void kan_workflow_graph_destroy (kan_workflow_graph_t graph);

KAN_C_HEADER_END
