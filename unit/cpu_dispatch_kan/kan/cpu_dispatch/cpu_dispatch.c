#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#include <kan/cpu_dispatch/job.h>
#include <kan/cpu_dispatch/task.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/error/critical.h>
#include <kan/memory/allocation.h>
#include <kan/platform/hardware.h>
#include <kan/threading/atomic.h>
#include <kan/threading/conditional_variable.h>
#include <kan/threading/mutex.h>
#include <kan/threading/thread.h>

#define JOB_STATE_ASSEMBLING 0u
#define JOB_STATE_RELEASED 1u
#define JOB_STATE_DETACHED 2u
#define JOB_STATE_AWAITED 3u
#define JOB_STATE_COMPLETED 4u

#define JOB_STATUS_TASK_COUNT_BITS 24u
#define JOB_STATUS_TASK_COUNT_MASK ((1u << JOB_STATUS_TASK_COUNT_BITS) - 1u)
#define JOB_STATUS_TASK_COUNT_MAX (1u << JOB_STATUS_TASK_COUNT_BITS)
#define JOB_STATUS_INITIAL (JOB_STATE_ASSEMBLING << JOB_STATUS_TASK_COUNT_BITS)

struct job_t
{
    struct kan_atomic_int_t status;
    struct kan_cpu_task_t completion_task;

    kan_conditional_variable_handle_t await_condition;
    kan_mutex_handle_t await_condition_mutex;
};

#define TASK_STATE_QUEUED 0
#define TASK_STATE_RUNNING 1
#define TASK_STATE_FINISHED 2
#define TASK_STATE_QUEUED_DETACHED 3
#define TASK_STATE_RUNNING_DETACHED 4

struct task_node_t
{
    struct task_node_t *next;
    struct job_t *job;
    struct kan_cpu_task_t task;
    struct kan_atomic_int_t state;
};

struct task_dispatcher_t
{
    struct task_node_t *tasks_first;
    struct task_node_t *tasks_last;

    kan_mutex_handle_t task_mutex;
    kan_conditional_variable_handle_t worker_wake_up_condition;

    kan_allocation_group_t allocation_group;
    kan_cpu_section_t execution_section;

    struct kan_atomic_int_t shutting_down;
    uint64_t threads_count;
    kan_thread_handle_t *threads;
};

static kan_bool_t global_task_dispatcher_ready = KAN_FALSE;
static struct kan_atomic_int_t global_task_dispatcher_init_lock = {.value = 0};
static struct task_dispatcher_t global_task_dispatcher;

static void job_report_task_finished (struct job_t *job);

static kan_thread_result_t worker_thread_function (kan_thread_user_data_t user_data)
{
    while (KAN_TRUE)
    {
        if (kan_atomic_int_get (&global_task_dispatcher.shutting_down))
        {
            return 0;
        }

        struct task_node_t *task = NULL;
        kan_mutex_lock (global_task_dispatcher.task_mutex);

        while (KAN_TRUE)
        {
            if (kan_atomic_int_get (&global_task_dispatcher.shutting_down))
            {
                kan_mutex_unlock (global_task_dispatcher.task_mutex);
                return 0;
            }

            if (!global_task_dispatcher.tasks_first)
            {
                kan_conditional_variable_wait (global_task_dispatcher.worker_wake_up_condition,
                                               global_task_dispatcher.task_mutex);
                continue;
            }

            KAN_ASSERT (global_task_dispatcher.tasks_first)
            task = global_task_dispatcher.tasks_first;
            global_task_dispatcher.tasks_first = task->next;

            if (task == global_task_dispatcher.tasks_last)
            {
                global_task_dispatcher.tasks_last = NULL;
            }

            break;
        }

        kan_mutex_unlock (global_task_dispatcher.task_mutex);
        struct kan_cpu_section_execution_t task_type_section_execution;
        kan_cpu_section_execution_init (&task_type_section_execution, global_task_dispatcher.execution_section);

        while (KAN_TRUE)
        {
            const int old_state = kan_atomic_int_get (&task->state);
            KAN_ASSERT (old_state == TASK_STATE_QUEUED || TASK_STATE_QUEUED_DETACHED)
            const int new_state = old_state == TASK_STATE_QUEUED ? TASK_STATE_RUNNING : TASK_STATE_RUNNING_DETACHED;

            if (kan_atomic_int_compare_and_set (&task->state, old_state, new_state))
            {
                break;
            }
        }

        const kan_cpu_section_t task_section = kan_cpu_section_get (task->task.name);
        struct kan_cpu_section_execution_t task_section_execution;

        kan_cpu_section_execution_init (&task_section_execution, task_section);
        task->task.function (task->task.user_data);
        kan_cpu_section_execution_shutdown (&task_section_execution);

        if (task->job)
        {
            job_report_task_finished (task->job);
        }

        kan_bool_t free_task;
        while (KAN_TRUE)
        {
            int old_state = kan_atomic_int_get (&task->state);
            KAN_ASSERT (old_state == TASK_STATE_RUNNING || TASK_STATE_RUNNING_DETACHED)
            free_task = old_state == TASK_STATE_RUNNING_DETACHED;
            const int new_state = TASK_STATE_FINISHED;

            if (kan_atomic_int_compare_and_set (&task->state, old_state, new_state))
            {
                break;
            }
        }

        if (free_task)
        {
            kan_free_batched (global_task_dispatcher.allocation_group, task);
        }

        kan_cpu_section_execution_shutdown (&task_type_section_execution);
    }
}

static void shutdown_global_task_dispatcher (void)
{
    kan_mutex_lock (global_task_dispatcher.task_mutex);
    kan_atomic_int_set (&global_task_dispatcher.shutting_down, 1);
    kan_mutex_unlock (global_task_dispatcher.task_mutex);
    kan_conditional_variable_signal_all (global_task_dispatcher.worker_wake_up_condition);

    for (uint64_t index = 0u; index < global_task_dispatcher.threads_count; ++index)
    {
        kan_thread_wait (global_task_dispatcher.threads[index]);
    }
}

static void ensure_global_task_dispatcher_ready (void)
{
    if (!global_task_dispatcher_ready)
    {
        // Initialization clash is a really rare situation, but must be checked any way.
        kan_atomic_int_lock (&global_task_dispatcher_init_lock);

        if (!global_task_dispatcher_ready)
        {
            global_task_dispatcher.tasks_first = NULL;
            global_task_dispatcher.tasks_last = NULL;

            global_task_dispatcher.task_mutex = kan_mutex_create ();
            KAN_ASSERT (global_task_dispatcher.task_mutex != KAN_INVALID_MUTEX_HANDLE)
            global_task_dispatcher.worker_wake_up_condition = kan_conditional_variable_create ();
            KAN_ASSERT (global_task_dispatcher.worker_wake_up_condition != KAN_INVALID_CONDITIONAL_VARIABLE_HANDLE)

            global_task_dispatcher.allocation_group =
                kan_allocation_group_get_child (kan_allocation_group_root (), "global_cpu_dispatcher");
            global_task_dispatcher.execution_section = kan_cpu_section_get ("cpu_dispatch_task");

            global_task_dispatcher.shutting_down = kan_atomic_int_init (0);
            global_task_dispatcher.threads_count = kan_platform_get_cpu_count ();

            global_task_dispatcher.threads = kan_allocate_general (
                global_task_dispatcher.allocation_group,
                sizeof (kan_thread_handle_t) * global_task_dispatcher.threads_count, _Alignof (kan_thread_handle_t));

            for (uint64_t index = 0u; index < global_task_dispatcher.threads_count; ++index)
            {
                global_task_dispatcher.threads[index] =
                    kan_thread_create ("global_cpu_dispatcher_worker", worker_thread_function, 0u);
                KAN_ASSERT (global_task_dispatcher.threads[index] != KAN_INVALID_THREAD_HANDLE)
            }

            atexit (shutdown_global_task_dispatcher);
            global_task_dispatcher_ready = KAN_TRUE;
        }

        kan_atomic_int_unlock (&global_task_dispatcher_init_lock);
    }
}

static struct task_node_t *dispatch_task (struct job_t *job, struct kan_cpu_task_t task)
{
    ensure_global_task_dispatcher_ready ();
    struct task_node_t *task_node = (struct task_node_t *) kan_allocate_batched (
        global_task_dispatcher.allocation_group, sizeof (struct task_node_t));
    task_node->job = job;
    task_node->task = task;
    task_node->state = kan_atomic_int_init (TASK_STATE_QUEUED);

    if (job)
    {
        KAN_ASSERT ((((unsigned int) kan_atomic_int_get (&job->status)) & JOB_STATUS_TASK_COUNT_MASK) + 1u <
                    JOB_STATUS_TASK_COUNT_MAX)
        kan_atomic_int_add (&job->status, 1);
    }

    kan_mutex_lock (global_task_dispatcher.task_mutex);
    task_node->next = NULL;

    if (global_task_dispatcher.tasks_last)
    {
        global_task_dispatcher.tasks_last->next = task_node;
        global_task_dispatcher.tasks_last = task_node;
    }
    else
    {
        global_task_dispatcher.tasks_first = task_node;
        global_task_dispatcher.tasks_last = task_node;
    }

    kan_mutex_unlock (global_task_dispatcher.task_mutex);
    kan_conditional_variable_signal_one (global_task_dispatcher.worker_wake_up_condition);
    return task_node;
}

static void dispatch_task_list (struct job_t *job, struct kan_cpu_task_list_node_t *tasks)
{
    ensure_global_task_dispatcher_ready ();
    KAN_ASSERT (tasks)

    uint64_t count = 0u;
    struct task_node_t *begin = NULL;
    struct task_node_t *end = NULL;

    while (tasks)
    {
        struct task_node_t *task_node = (struct task_node_t *) kan_allocate_batched (
            global_task_dispatcher.allocation_group, sizeof (struct task_node_t));
        task_node->job = job;
        task_node->task = tasks->task;
        task_node->state = kan_atomic_int_init (TASK_STATE_QUEUED);
        tasks->dispatch_handle = (kan_cpu_task_handle_t) task_node;
        task_node->next = NULL;

        if (end)
        {
            end->next = task_node;
            end = task_node;
        }
        else
        {
            begin = task_node;
            end = task_node;
        }

        ++count;
        tasks = tasks->next;
    }

    if (job)
    {
        KAN_ASSERT ((((unsigned int) kan_atomic_int_get (&job->status)) & JOB_STATUS_TASK_COUNT_MASK) + count <
                    JOB_STATUS_TASK_COUNT_MAX)
        kan_atomic_int_add (&job->status, (int) count);
    }

    kan_mutex_lock (global_task_dispatcher.task_mutex);
    if (begin)
    {
        if (global_task_dispatcher.tasks_last)
        {
            global_task_dispatcher.tasks_last->next = begin;
        }
        else
        {
            global_task_dispatcher.tasks_first = begin;
        }

        global_task_dispatcher.tasks_last = end;
    }

    kan_mutex_unlock (global_task_dispatcher.task_mutex);
    kan_conditional_variable_signal_all (global_task_dispatcher.worker_wake_up_condition);
}

kan_cpu_task_handle_t kan_cpu_task_dispatch (struct kan_cpu_task_t task)
{
    return (kan_cpu_task_handle_t) dispatch_task (NULL, task);
}

kan_bool_t kan_cpu_task_is_finished (kan_cpu_task_handle_t task)
{
    struct task_node_t *task_node = (struct task_node_t *) task;
    return kan_atomic_int_get (&task_node->state) == TASK_STATE_FINISHED;
}

void kan_cpu_task_detach (kan_cpu_task_handle_t task)
{
    struct task_node_t *task_node = (struct task_node_t *) task;
    while (KAN_TRUE)
    {
        int old_state = kan_atomic_int_get (&task_node->state);
        KAN_ASSERT (old_state == TASK_STATE_QUEUED || TASK_STATE_RUNNING || TASK_STATE_FINISHED)

        if (old_state == TASK_STATE_FINISHED)
        {
            kan_free_batched (global_task_dispatcher.allocation_group, task_node);
            return;
        }

        int new_state = old_state;
        if (old_state == TASK_STATE_QUEUED)
        {
            new_state = TASK_STATE_QUEUED_DETACHED;
        }
        else if (old_state == TASK_STATE_RUNNING)
        {
            new_state = TASK_STATE_RUNNING_DETACHED;
        }

        if (kan_atomic_int_compare_and_set (&task_node->state, old_state, new_state))
        {
            break;
        }
    }
}

void kan_cpu_task_dispatch_list (struct kan_cpu_task_list_node_t *list)
{
    dispatch_task_list (NULL, list);
}

static kan_bool_t job_allocation_group_ready = KAN_FALSE;
static kan_allocation_group_t job_allocation_group;

static void job_report_task_finished (struct job_t *job)
{
    struct job_t *job_data = (struct job_t *) job;
    while (KAN_TRUE)
    {
        const int old_status = kan_atomic_int_get (&job_data->status);
        const unsigned int old_status_bits = (unsigned int) old_status;
        const unsigned int old_status_state = old_status_bits >> JOB_STATUS_TASK_COUNT_BITS;
        KAN_ASSERT (old_status_state != JOB_STATE_COMPLETED)

        KAN_ASSERT ((old_status_bits & JOB_STATUS_TASK_COUNT_MASK) > 0u)
        unsigned int new_status_bits = old_status_bits - 1u;

        if (old_status_state != JOB_STATE_ASSEMBLING && (new_status_bits & JOB_STATUS_TASK_COUNT_MASK) == 0u)
        {
            new_status_bits = JOB_STATE_COMPLETED << JOB_STATUS_TASK_COUNT_BITS;
        }

        const int new_status = (int) new_status_bits;
        if (kan_atomic_int_compare_and_set (&job_data->status, old_status, new_status))
        {
            if (new_status_bits == (JOB_STATE_COMPLETED << JOB_STATUS_TASK_COUNT_BITS))
            {
                // Job is now completed fully. Check old state to choose what to do.
                KAN_ASSERT (old_status_state == JOB_STATE_RELEASED || old_status_state == JOB_STATE_DETACHED ||
                            old_status_state == JOB_STATE_AWAITED)

                if (job->completion_task.function)
                {
                    kan_cpu_task_detach ((kan_cpu_task_handle_t) dispatch_task (NULL, job->completion_task));
                }

                if (old_status_state == JOB_STATE_DETACHED)
                {
                    kan_free_batched (job_allocation_group, job);
                }
                else if (old_status_state == JOB_STATE_AWAITED)
                {
                    KAN_ASSERT (job->await_condition != KAN_INVALID_CONDITIONAL_VARIABLE_HANDLE)
                    kan_conditional_variable_signal_one (job->await_condition);
                }

                // Nothing more to do if JOB_STATE_RELEASED.
            }

            break;
        }
    }
}

kan_cpu_job_t kan_cpu_job_create (void)
{
    if (!job_allocation_group_ready)
    {
        job_allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "cpu_job");
        job_allocation_group_ready = KAN_TRUE;
    }

    struct job_t *job = (struct job_t *) kan_allocate_batched (job_allocation_group, sizeof (struct job_t));
    job->status = kan_atomic_int_init (JOB_STATUS_INITIAL);
    job->completion_task = (struct kan_cpu_task_t) {.name = NULL, .function = NULL, .user_data = 0u};

    // Intentionally create only if someone wait for job to be completed.
    job->await_condition = KAN_INVALID_CONDITIONAL_VARIABLE_HANDLE;
    job->await_condition_mutex = KAN_INVALID_MUTEX_HANDLE;
    return (kan_cpu_job_t) job;
}

void kan_cpu_job_set_completion_task (kan_cpu_job_t job, struct kan_cpu_task_t completion_task)
{
    struct job_t *job_data = (struct job_t *) job;
    KAN_ASSERT ((((unsigned int) kan_atomic_int_get (&job_data->status)) >> JOB_STATUS_TASK_COUNT_BITS) ==
                JOB_STATE_ASSEMBLING)
    job_data->completion_task = completion_task;
}

kan_cpu_task_handle_t kan_cpu_job_dispatch_task (kan_cpu_job_t job, struct kan_cpu_task_t task)
{
    struct job_t *job_data = (struct job_t *) job;
    KAN_ASSERT ((((unsigned int) kan_atomic_int_get (&job_data->status)) >> JOB_STATUS_TASK_COUNT_BITS) ==
                JOB_STATE_ASSEMBLING)
    return (kan_cpu_task_handle_t) dispatch_task (job_data, task);
}

void kan_cpu_job_dispatch_task_list (kan_cpu_job_t job, struct kan_cpu_task_list_node_t *list)
{
    struct job_t *job_data = (struct job_t *) job;
    KAN_ASSERT ((((unsigned int) kan_atomic_int_get (&job_data->status)) >> JOB_STATUS_TASK_COUNT_BITS) ==
                JOB_STATE_ASSEMBLING)
    dispatch_task_list (job_data, list);
}

void kan_cpu_job_release (kan_cpu_job_t job)
{
    struct job_t *job_data = (struct job_t *) job;
    KAN_ASSERT ((((unsigned int) kan_atomic_int_get (&job_data->status)) >> JOB_STATUS_TASK_COUNT_BITS) ==
                JOB_STATE_ASSEMBLING)

    while (KAN_TRUE)
    {
        const int old_status = kan_atomic_int_get (&job_data->status);
        const unsigned int old_status_bits = (unsigned int) old_status;
        const unsigned int old_status_tasks = old_status_bits & JOB_STATUS_TASK_COUNT_MASK;

        unsigned int new_status_bits;
        if (old_status_tasks == 0u)
        {
            new_status_bits = JOB_STATE_COMPLETED << JOB_STATUS_TASK_COUNT_BITS;
        }
        else
        {
            new_status_bits = (JOB_STATE_RELEASED << JOB_STATUS_TASK_COUNT_BITS) | old_status_tasks;
        }

        const int new_status = (int) new_status_bits;
        if (kan_atomic_int_compare_and_set (&job_data->status, old_status, new_status))
        {
            if (new_status_bits == (JOB_STATE_COMPLETED << JOB_STATUS_TASK_COUNT_BITS) &&
                job_data->completion_task.function)
            {
                kan_cpu_task_detach ((kan_cpu_task_handle_t) dispatch_task (NULL, job_data->completion_task));
            }

            break;
        }
    }
}

void kan_cpu_job_detach (kan_cpu_job_t job)
{
    struct job_t *job_data = (struct job_t *) job;
    while (KAN_TRUE)
    {
        const int old_status = kan_atomic_int_get (&job_data->status);
        const unsigned int old_status_bits = (unsigned int) old_status;
        const unsigned int old_status_state = old_status_bits >> JOB_STATUS_TASK_COUNT_BITS;
        KAN_ASSERT (old_status_state == JOB_STATE_RELEASED || old_status_state == JOB_STATE_COMPLETED)

        if (old_status_state == JOB_STATE_COMPLETED)
        {
            kan_free_batched (job_allocation_group, job_data);
            return;
        }

        const unsigned int old_status_tasks = old_status_bits & JOB_STATUS_TASK_COUNT_MASK;
        unsigned int new_status_bits = (JOB_STATE_DETACHED << JOB_STATUS_TASK_COUNT_BITS) | old_status_tasks;
        const int new_status = (int) new_status_bits;

        if (kan_atomic_int_compare_and_set (&job_data->status, old_status, new_status))
        {
            break;
        }
    }
}

void kan_cpu_job_wait (kan_cpu_job_t job)
{
    struct job_t *job_data = (struct job_t *) job;
    if ((((unsigned int) kan_atomic_int_get (&job_data->status)) >> JOB_STATUS_TASK_COUNT_BITS) == JOB_STATE_COMPLETED)
    {
        return;
    }

    job_data->await_condition = kan_conditional_variable_create ();
    KAN_ASSERT (job_data->await_condition != KAN_INVALID_CONDITIONAL_VARIABLE_HANDLE)
    job_data->await_condition_mutex = kan_mutex_create ();
    KAN_ASSERT (job_data->await_condition_mutex != KAN_INVALID_MUTEX_HANDLE)

    while (KAN_TRUE)
    {
        const int old_status = kan_atomic_int_get (&job_data->status);
        const unsigned int old_status_bits = (unsigned int) old_status;
        const unsigned int old_status_state = old_status_bits >> JOB_STATUS_TASK_COUNT_BITS;
        KAN_ASSERT (old_status_state == JOB_STATE_RELEASED || old_status_state == JOB_STATE_COMPLETED)

        // Some time passed from fast-forward check above, so we might be completed as well.
        if (old_status_state == JOB_STATE_COMPLETED)
        {
            kan_conditional_variable_destroy (job_data->await_condition);
            kan_mutex_destroy (job_data->await_condition_mutex);
            kan_free_batched (job_allocation_group, job_data);
            return;
        }

        const unsigned int old_status_tasks = old_status_bits & JOB_STATUS_TASK_COUNT_MASK;
        unsigned int new_status_bits = (JOB_STATE_AWAITED << JOB_STATUS_TASK_COUNT_BITS) | old_status_tasks;
        const int new_status = (int) new_status_bits;

        if (kan_atomic_int_compare_and_set (&job_data->status, old_status, new_status))
        {
            break;
        }
    }

    kan_mutex_lock (job_data->await_condition_mutex);
    while ((((unsigned int) kan_atomic_int_get (&job_data->status)) >> JOB_STATUS_TASK_COUNT_BITS) !=
           JOB_STATE_COMPLETED)
    {
        kan_conditional_variable_wait (job_data->await_condition, job_data->await_condition_mutex);
    }

    kan_mutex_unlock (job_data->await_condition_mutex);
    kan_conditional_variable_destroy (job_data->await_condition);
    kan_mutex_destroy (job_data->await_condition_mutex);
    kan_free_batched (job_allocation_group, job_data);
}
