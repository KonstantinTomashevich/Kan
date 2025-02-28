#define _CRT_SECURE_NO_WARNINGS

#include <time.h>

#include <kan/cpu_dispatch/job.h>
#include <kan/cpu_dispatch/task.h>
#include <kan/memory/allocation.h>
#include <kan/precise_time/precise_time.h>
#include <kan/testing/testing.h>
#include <kan/threading/atomic.h>

struct test_task_user_data_t
{
    struct kan_atomic_int_t work_done;
};

static void test_task_function (kan_functor_user_data_t user_data)
{
    // Simulate some work.
    const kan_time_size_t start = kan_precise_time_get_elapsed_nanoseconds ();

    while (kan_precise_time_get_elapsed_nanoseconds () - start < 1000000u)
    {
        kan_memory_size_t stub[1000u];
        stub[0u] = 1u;
        stub[1u] = 1u;

        for (kan_loop_size_t index = 2u; index < 1000u; ++index)
        {
            stub[index] = stub[index - 1u] + stub[index - 2u];
        }
    }

    kan_atomic_int_set (&((struct test_task_user_data_t *) user_data)->work_done, 1);
}

static void dispatch_separately (kan_cpu_job_t job,
                                 kan_cpu_task_t *handles_output,
                                 struct test_task_user_data_t *user_data_output,
                                 kan_instance_size_t count)
{
    const kan_cpu_section_t test_task_section = kan_cpu_section_get ("test_task");
    for (kan_loop_size_t index = 0u; index < count; ++index)
    {
        user_data_output[index].work_done = kan_atomic_int_init (0);
        struct kan_cpu_task_t task = {
            .function = test_task_function,
            .user_data = (kan_functor_user_data_t) &user_data_output[index],
            .profiler_section = test_task_section,
        };

        if (!KAN_HANDLE_IS_VALID (job))
        {
            handles_output[index] = kan_cpu_task_dispatch (task);
        }
        else
        {
            handles_output[index] = kan_cpu_job_dispatch_task (job, task);
        }
    }
}

static void dispatch_as_list (kan_cpu_job_t job,
                              kan_cpu_task_t *handles_output,
                              struct test_task_user_data_t *user_data_output,
                              kan_instance_size_t count)
{
    const kan_cpu_section_t test_task_section = kan_cpu_section_get ("test_task");
    struct kan_cpu_task_list_node_t *nodes =
        kan_allocate_general (KAN_ALLOCATION_GROUP_IGNORE, sizeof (struct kan_cpu_task_list_node_t) * count,
                              _Alignof (struct kan_cpu_task_list_node_t));

    for (kan_loop_size_t index = 0u; index < count; ++index)
    {
        user_data_output[index].work_done = kan_atomic_int_init (0);
        nodes[index].next = index + 1u == count ? NULL : &nodes[index + 1u];

        nodes[index].task = (struct kan_cpu_task_t) {
            .function = test_task_function,
            .user_data = (kan_functor_user_data_t) &user_data_output[index],
            .profiler_section = test_task_section,
        };
    }

    if (!KAN_HANDLE_IS_VALID (job))
    {
        kan_cpu_task_dispatch_list (nodes);
    }
    else
    {
        kan_cpu_job_dispatch_task_list (job, nodes);
    }

    for (kan_loop_size_t index = 0u; index < count; ++index)
    {
        handles_output[index] = nodes[index].dispatch_handle;
    }

    kan_free_general (KAN_ALLOCATION_GROUP_IGNORE, nodes, sizeof (struct kan_cpu_task_list_node_t) * count);
}

static void wait_until_all_finished (kan_cpu_task_t *handles, kan_instance_size_t count)
{
    while (KAN_TRUE)
    {
        kan_bool_t all_finished = KAN_TRUE;
        for (kan_loop_size_t index = 0u; index < count; ++index)
        {
            if (!kan_cpu_task_is_finished (handles[index]))
            {
                all_finished = KAN_FALSE;
            }
        }

        if (all_finished)
        {
            break;
        }
    }
}

KAN_TEST_CASE (execute_1000_tasks_separate_dispatch)
{
    kan_cpu_task_t handles[1000u];
    struct test_task_user_data_t user_data[1000u];
    dispatch_separately (KAN_HANDLE_SET_INVALID (kan_cpu_job_t), handles, user_data, 1000u);
    wait_until_all_finished (handles, 1000u);

    for (kan_loop_size_t index = 0u; index < 1000u; ++index)
    {
        kan_cpu_task_detach (handles[index]);
    }
}

KAN_TEST_CASE (execute_1000_tasks_list_dispatch)
{
    kan_cpu_task_t handles[1000u];
    struct test_task_user_data_t user_data[1000u];
    dispatch_as_list (KAN_HANDLE_SET_INVALID (kan_cpu_job_t), handles, user_data, 1000u);
    wait_until_all_finished (handles, 1000u);

    for (kan_loop_size_t index = 0u; index < 1000u; ++index)
    {
        kan_cpu_task_detach (handles[index]);
    }
}

// Full name is too much to bear for the Windows without long file paths.
// execute_1000_tasks_separate_dispatch_and_detach_right_away
KAN_TEST_CASE (execute_1000_tasks_sd_dra)
{
    kan_cpu_task_t handles[1000u];
    struct test_task_user_data_t user_data[1000u];
    dispatch_separately (KAN_HANDLE_SET_INVALID (kan_cpu_job_t), handles, user_data, 1000u);

    for (kan_loop_size_t index = 0u; index < 1000u; ++index)
    {
        kan_cpu_task_detach (handles[index]);
    }

    while (KAN_TRUE)
    {
        kan_bool_t all_finished = KAN_TRUE;
        for (kan_loop_size_t index = 0u; index < 1000u; ++index)
        {
            if (!kan_atomic_int_get (&user_data[index].work_done))
            {
                all_finished = KAN_FALSE;
            }
        }

        if (all_finished)
        {
            break;
        }
    }
}

KAN_TEST_CASE (job_1000_tasks_separate_dispatch)
{
    kan_cpu_task_t handles[1000u];
    struct test_task_user_data_t user_data[1000u];
    const kan_cpu_job_t job = kan_cpu_job_create ();

    dispatch_separately (job, handles, user_data, 1000u);
    for (kan_loop_size_t index = 0u; index < 1000u; ++index)
    {
        kan_cpu_task_detach (handles[index]);
    }

    kan_cpu_job_release (job);
    kan_cpu_job_wait (job);
}

KAN_TEST_CASE (job_1000_tasks_list_dispatch)
{
    kan_cpu_task_t handles[1000u];
    struct test_task_user_data_t user_data[1000u];
    const kan_cpu_job_t job = kan_cpu_job_create ();

    dispatch_as_list (job, handles, user_data, 1000u);
    for (kan_loop_size_t index = 0u; index < 1000u; ++index)
    {
        kan_cpu_task_detach (handles[index]);
    }

    kan_cpu_job_release (job);
    kan_cpu_job_wait (job);
}

KAN_TEST_CASE (job_1000_tasks_detach)
{
    kan_cpu_task_t handles[1000u];
    struct test_task_user_data_t user_data[1000u];
    const kan_cpu_job_t job = kan_cpu_job_create ();
    dispatch_as_list (job, handles, user_data, 1000u);

    for (kan_loop_size_t index = 0u; index < 1000u; ++index)
    {
        kan_cpu_task_detach (handles[index]);
    }

    kan_cpu_job_release (job);
    kan_cpu_job_detach (job);

    while (KAN_TRUE)
    {
        kan_bool_t all_finished = KAN_TRUE;
        for (kan_loop_size_t index = 0u; index < 1000u; ++index)
        {
            if (!kan_atomic_int_get (&user_data[index].work_done))
            {
                all_finished = KAN_FALSE;
            }
        }

        if (all_finished)
        {
            break;
        }
    }
}

KAN_TEST_CASE (job_1000_completion_task)
{
    kan_cpu_task_t handles[1000u];
    struct test_task_user_data_t user_data[1000u];
    struct test_task_user_data_t completion_task_user_data;
    completion_task_user_data.work_done = kan_atomic_int_init (0);
    const kan_cpu_job_t job = kan_cpu_job_create ();

    kan_cpu_job_set_completion_task (job, (struct kan_cpu_task_t) {
                                              .function = test_task_function,
                                              .user_data = (kan_functor_user_data_t) &completion_task_user_data,
                                              .profiler_section = kan_cpu_section_get ("completion_task"),
                                          });

    dispatch_as_list (job, handles, user_data, 1000u);
    for (kan_loop_size_t index = 0u; index < 1000u; ++index)
    {
        kan_cpu_task_detach (handles[index]);
    }

    kan_cpu_job_release (job);
    kan_cpu_job_detach (job);

    while (KAN_TRUE)
    {
        if (kan_atomic_int_get (&completion_task_user_data.work_done))
        {
            break;
        }
    }
}
