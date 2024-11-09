#include <stddef.h>
#include <string.h>

#include <kan/memory/allocation.h>
#include <kan/platform/precise_time.h>
#include <kan/testing/testing.h>
#include <kan/threading/atomic.h>
#include <kan/workflow/workflow.h>

struct node_t
{
    const char *name;
    kan_workflow_function_t function;

    kan_instance_size_t depends_on_count;
    const char **depends_on;
    kan_instance_size_t dependency_of_count;
    const char **dependency_of;
};

struct checkpoint_dependency_t
{
    const char *dependency;
    const char *dependant;
};

#define MAX_NODES 100u

static const char *finish_nodes[MAX_NODES];
static struct kan_atomic_int_t finish_index;

static inline void finish_node (kan_functor_user_data_t user_data)
{
    int index = kan_atomic_int_add (&finish_index, 1u);
    finish_nodes[index] = (const char *) user_data;
}

static inline kan_loop_size_t find_finished_node_index (const char *node_name)
{
    kan_loop_size_t node_finish_index = 0u;
    while (node_finish_index < (kan_loop_size_t) kan_atomic_int_get (&finish_index) &&
           strcmp (finish_nodes[node_finish_index], node_name) != 0)
    {
        ++node_finish_index;
    }

    return node_finish_index;
}

static void verify_checkpoint_dependency_up (kan_instance_size_t nodes_count,
                                             struct node_t *nodes,
                                             kan_instance_size_t checkpoint_dependencies_count,
                                             struct checkpoint_dependency_t *checkpoint_dependencies,
                                             kan_loop_size_t root_node_finish_index,
                                             const char *checkpoint_name)
{
    for (kan_loop_size_t node_index = 0u; node_index < nodes_count; ++node_index)
    {
        struct node_t *node = &nodes[node_index];
        for (kan_loop_size_t depends_on_index = 0u; depends_on_index < node->depends_on_count; ++depends_on_index)
        {
            if (strcmp (node->depends_on[depends_on_index], checkpoint_name) == 0)
            {
                const kan_loop_size_t found_index = find_finished_node_index (node->name);
                KAN_TEST_ASSERT (found_index > root_node_finish_index)
                break;
            }
        }
    }

    for (kan_loop_size_t index = 0u; index < checkpoint_dependencies_count; ++index)
    {
        if (strcmp (checkpoint_dependencies[index].dependency, checkpoint_name) == 0)
        {
            verify_checkpoint_dependency_up (nodes_count, nodes, checkpoint_dependencies_count, checkpoint_dependencies,
                                             root_node_finish_index, checkpoint_dependencies[index].dependant);
        }
    }
}

static void verify_checkpoint_dependency_down (kan_instance_size_t nodes_count,
                                               struct node_t *nodes,
                                               kan_instance_size_t checkpoint_dependencies_count,
                                               struct checkpoint_dependency_t *checkpoint_dependencies,
                                               kan_loop_size_t root_node_finish_index,
                                               const char *checkpoint_name)
{
    for (kan_loop_size_t node_index = 0u; node_index < nodes_count; ++node_index)
    {
        struct node_t *node = &nodes[node_index];
        for (kan_loop_size_t dependency_of_index = 0u; dependency_of_index < node->dependency_of_count;
             ++dependency_of_index)
        {
            if (strcmp (node->dependency_of[dependency_of_index], checkpoint_name) == 0)
            {
                const kan_loop_size_t found_index = find_finished_node_index (node->name);
                KAN_TEST_ASSERT (found_index < root_node_finish_index)
                break;
            }
        }
    }

    for (kan_loop_size_t index = 0u; index < checkpoint_dependencies_count; ++index)
    {
        if (strcmp (checkpoint_dependencies[index].dependant, checkpoint_name) == 0)
        {
            verify_checkpoint_dependency_down (nodes_count, nodes, checkpoint_dependencies_count,
                                               checkpoint_dependencies, root_node_finish_index,
                                               checkpoint_dependencies[index].dependency);
        }
    }
}

static void execute_and_check (kan_instance_size_t nodes_count,
                               struct node_t *nodes,
                               kan_instance_size_t checkpoint_dependencies_count,
                               struct checkpoint_dependency_t *checkpoint_dependencies)
{
    KAN_TEST_ASSERT (nodes_count <= MAX_NODES)
    kan_workflow_graph_builder_t builder = kan_workflow_graph_builder_create (KAN_ALLOCATION_GROUP_IGNORE);

    for (kan_loop_size_t index = 0u; index < checkpoint_dependencies_count; ++index)
    {
        kan_workflow_graph_builder_register_checkpoint_dependency (builder, checkpoint_dependencies[index].dependency,
                                                                   checkpoint_dependencies[index].dependant);
    }

    for (kan_loop_size_t index = 0u; index < nodes_count; ++index)
    {
        struct node_t *node = &nodes[index];
        kan_workflow_graph_node_t graph_node = kan_workflow_graph_node_create (builder, node->name);
        kan_workflow_graph_node_set_function (graph_node, node->function, (kan_functor_user_data_t) node->name);

        for (kan_loop_size_t depends_on_index = 0u; depends_on_index < node->depends_on_count; ++depends_on_index)
        {
            kan_workflow_graph_node_depend_on (graph_node, node->depends_on[depends_on_index]);
        }

        for (kan_loop_size_t dependency_of_index = 0u; dependency_of_index < node->dependency_of_count;
             ++dependency_of_index)
        {
            kan_workflow_graph_node_make_dependency_of (graph_node, node->dependency_of[dependency_of_index]);
        }

        KAN_TEST_CHECK (kan_workflow_graph_node_submit (graph_node))
    }

    kan_workflow_graph_t graph = kan_workflow_graph_builder_finalize (builder);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (graph))
    kan_workflow_graph_builder_destroy (builder);

    // Execute and test multiple times to check that graph is reusable.
    for (kan_loop_size_t execution_index = 0u; execution_index < 5u; ++execution_index)
    {
        finish_index = kan_atomic_int_init (0);
        kan_workflow_graph_execute (graph);

        for (kan_loop_size_t node_index = 0u; node_index < nodes_count; ++node_index)
        {
            struct node_t *node = &nodes[node_index];
            const kan_loop_size_t node_finish_index = find_finished_node_index (node->name);

            if (node_finish_index == (kan_loop_size_t) kan_atomic_int_get (&finish_index))
            {
                // Node has not finished executing for some reason.
                KAN_TEST_CHECK (KAN_FALSE)
                continue;
            }

            for (kan_loop_size_t depends_on_index = 0u; depends_on_index < node->depends_on_count; ++depends_on_index)
            {
                const kan_loop_size_t found_index = find_finished_node_index (node->depends_on[depends_on_index]);
                if (found_index != (kan_loop_size_t) kan_atomic_int_get (&finish_index))
                {
                    KAN_TEST_CHECK (node_finish_index > found_index)
                }
                else
                {
                    verify_checkpoint_dependency_down (nodes_count, nodes, checkpoint_dependencies_count,
                                                       checkpoint_dependencies, node_finish_index,
                                                       node->depends_on[depends_on_index]);
                }
            }

            for (kan_loop_size_t dependency_of_index = 0u; dependency_of_index < node->dependency_of_count;
                 ++dependency_of_index)
            {
                const kan_loop_size_t found_index = find_finished_node_index (node->dependency_of[dependency_of_index]);
                if (found_index != (kan_loop_size_t) kan_atomic_int_get (&finish_index))
                {
                    KAN_TEST_CHECK (node_finish_index < found_index)
                }
                else
                {
                    verify_checkpoint_dependency_up (nodes_count, nodes, checkpoint_dependencies_count,
                                                     checkpoint_dependencies, node_finish_index,
                                                     node->dependency_of[dependency_of_index]);
                }
            }
        }
    }

    kan_workflow_graph_destroy (graph);
}

static void single_threaded_node_function (kan_cpu_job_t job, kan_functor_user_data_t user_data)
{
    // Release job right away as a test -- it should not be completed until this cpu task completes.
    kan_cpu_job_release (job);

    // Spend some time by sleeping.
    kan_platform_sleep (1000000u);

    finish_node (user_data);
}

struct multi_threaded_node_state_t
{
    struct kan_atomic_int_t executions_left;
    kan_functor_user_data_t user_data;
};

#define MULTI_THREAD_NODE_SUB_TASKS 10u

static void multi_threaded_sub_task (kan_functor_user_data_t user_data)
{
    struct multi_threaded_node_state_t *state = (struct multi_threaded_node_state_t *) user_data;

    // Spend some time by sleeping.
    kan_platform_sleep (1000000u);

    if (kan_atomic_int_add (&state->executions_left, -1) == 1)
    {
        const kan_functor_user_data_t node_user_data = state->user_data;
        kan_free_batched (KAN_ALLOCATION_GROUP_IGNORE, state);
        finish_node (node_user_data);
    }
}

static void multi_threaded_node_function (kan_cpu_job_t job, kan_functor_user_data_t user_data)
{
    struct multi_threaded_node_state_t *state =
        kan_allocate_batched (KAN_ALLOCATION_GROUP_IGNORE, sizeof (struct multi_threaded_node_state_t));

    state->executions_left = kan_atomic_int_init (MULTI_THREAD_NODE_SUB_TASKS);
    state->user_data = user_data;

    for (kan_loop_size_t index = 0u; index < MULTI_THREAD_NODE_SUB_TASKS; ++index)
    {
        kan_cpu_job_dispatch_task (job, (struct kan_cpu_task_t) {
                                            .name = kan_string_intern ("multithreaded_task"),
                                            .function = multi_threaded_sub_task,
                                            .user_data = (kan_functor_user_data_t) state,
                                        });

        // Spend some time by sleeping.
        kan_platform_sleep (1000u);
    }

    kan_cpu_job_release (job);
}

KAN_TEST_CASE (sequential_nodes)
{
    execute_and_check (3u,
                       (struct node_t[]) {
                           {"0", single_threaded_node_function, 0u, NULL, 1u, (const char *[]) {"1"}},
                           {"1", single_threaded_node_function, 0u, NULL, 0u, NULL},
                           {"2", single_threaded_node_function, 1u, (const char *[]) {"1"}, 0u, NULL},
                       },
                       0u, NULL);
}

KAN_TEST_CASE (automatic_checkpoint)
{
    execute_and_check (2u,
                       (struct node_t[]) {
                           {"A", single_threaded_node_function, 0u, NULL, 1u, (const char *[]) {"checkpoint"}},
                           {"B", single_threaded_node_function, 1u, (const char *[]) {"checkpoint"}, 0u, NULL},
                       },
                       0u, NULL);
}

KAN_TEST_CASE (checkpoint_dependency)
{
    execute_and_check (2u,
                       (struct node_t[]) {
                           {"A", single_threaded_node_function, 0u, NULL, 1u, (const char *[]) {"checkpoint_1"}},
                           {"B", single_threaded_node_function, 1u, (const char *[]) {"checkpoint_2"}, 0u, NULL},
                       },
                       1u,
                       (struct checkpoint_dependency_t[]) {
                           {"checkpoint_1", "checkpoint_2"},
                       });
}

KAN_TEST_CASE (parallel_nodes)
{
    execute_and_check (3u,
                       (struct node_t[]) {
                           {"0", single_threaded_node_function, 0u, NULL, 0u, NULL},
                           {"1", single_threaded_node_function, 0u, NULL, 0u, NULL},
                           {"2", single_threaded_node_function, 0u, NULL, 0u, NULL},
                       },
                       0u, NULL);
}

KAN_TEST_CASE (multi_threaded_nodes)
{
    execute_and_check (3u,
                       (struct node_t[]) {
                           {"0", multi_threaded_node_function, 0u, NULL, 1u, (const char *[]) {"1"}},
                           {"1", multi_threaded_node_function, 0u, NULL, 0u, NULL},
                           {"2", multi_threaded_node_function, 1u, (const char *[]) {"1"}, 0u, NULL},
                       },
                       0u, NULL);
}

KAN_TEST_CASE (fork_and_join)
{
    execute_and_check (
        8u,
        (struct node_t[]) {
            {"A", multi_threaded_node_function, 0u, NULL, 0u, NULL},
            {"B1", multi_threaded_node_function, 1u, (const char *[]) {"A"}, 1u, (const char *[]) {"C1"}},
            {"B2", multi_threaded_node_function, 1u, (const char *[]) {"A"}, 1u, (const char *[]) {"C2"}},
            {"B3", multi_threaded_node_function, 1u, (const char *[]) {"A"}, 1u, (const char *[]) {"C3"}},
            {"C1", multi_threaded_node_function, 0u, NULL, 0u, NULL},
            {"C2", multi_threaded_node_function, 0u, NULL, 0u, NULL},
            {"C3", multi_threaded_node_function, 0u, NULL, 0u, NULL},
            {"D", multi_threaded_node_function, 3u, (const char *[]) {"C1", "C2", "C3"}, 0u, NULL},
        },
        0u, NULL);
}
