#include <stddef.h>
#include <stdlib.h>

#include <kan/cpu_dispatch/job.h>
#include <kan/cpu_dispatch/task.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/error/critical.h>
#include <kan/memory/allocation.h>
#include <kan/platform/hardware.h>
#include <kan/precise_time/precise_time.h>
#include <kan/threading/atomic.h>
#include <kan/threading/thread.h>

// TODO: Temporary
#include <stdio.h>

KAN_USE_STATIC_CPU_SECTIONS

#define JOB_STATE_ASSEMBLING 0u
#define JOB_STATE_RELEASED 1u
#define JOB_STATE_DETACHED 2u

/// \brief We cannot switch to completed right away, because detach or wait function can deallocate job when it has
///        switched to completed, but before it was able to do on-complete activities. Therefore, we use finishing
///        when job thinks that it has completed, and then switch to completed when we're fully done.
#define JOB_STATE_FINISHING 3u
#define JOB_STATE_COMPLETED 4u

#define JOB_STATUS_TASK_COUNT_BITS 24u
#define JOB_STATUS_TASK_COUNT_MASK ((1u << JOB_STATUS_TASK_COUNT_BITS) - 1u)
#define JOB_STATUS_TASK_COUNT_MAX (1u << JOB_STATUS_TASK_COUNT_BITS)
#define JOB_STATUS_INITIAL (JOB_STATE_ASSEMBLING << JOB_STATUS_TASK_COUNT_BITS)

struct job_t
{
    struct kan_atomic_int_t status;
    struct kan_cpu_task_t completion_task;
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
    struct kan_atomic_int_t task_lock;
    kan_allocation_group_t allocation_group;

    struct kan_atomic_int_t shutting_down;
    kan_instance_size_t threads_count;
    kan_thread_t *threads;
    struct kan_atomic_int_t dispatched_tasks_counter;
};

static bool global_task_dispatcher_ready = false;
static struct kan_atomic_int_t global_task_dispatcher_init_lock = {.value = 0};
static struct task_dispatcher_t global_task_dispatcher;

static void job_report_task_finished (struct job_t *job);

static kan_thread_result_t worker_thread_function (kan_thread_user_data_t user_data)
{
    while (true)
    {
        if (kan_atomic_int_get (&global_task_dispatcher.shutting_down))
        {
            return 0;
        }

        struct task_node_t *task = NULL;
        while (true)
        {
            if (kan_atomic_int_get (&global_task_dispatcher.shutting_down))
            {
                kan_atomic_int_unlock (&global_task_dispatcher.task_lock);
                return 0;
            }

            fprintf (stderr, "Worker trying to take task.\n");

            {
                KAN_ATOMIC_INT_SCOPED_LOCK (&global_task_dispatcher.task_lock)
                if (global_task_dispatcher.tasks_first)
                {
                    task = global_task_dispatcher.tasks_first;
                    global_task_dispatcher.tasks_first = task->next;
                    break;
                }
            }

            fprintf (stderr, "Worker going to sleep.\n");
            kan_precise_time_sleep (KAN_CPU_DISPATCHER_NO_TASK_SLEEP_NS);
            fprintf (stderr, "Worker slept and has awakened now.\n");
        }

        KAN_CPU_SCOPED_STATIC_SECTION (cpu_dispatch_task)
        KAN_ATOMIC_INT_COMPARE_AND_SET (&task->state)
        {
            KAN_ASSERT (old_value == TASK_STATE_QUEUED || old_value == TASK_STATE_QUEUED_DETACHED)
            new_value = old_value == TASK_STATE_QUEUED ? TASK_STATE_RUNNING : TASK_STATE_RUNNING_DETACHED;
        }

        struct kan_cpu_section_execution_t task_section_execution;
        kan_cpu_section_execution_init (&task_section_execution, task->task.profiler_section);
        task->task.function (task->task.user_data);
        kan_cpu_section_execution_shutdown (&task_section_execution);

        if (task->job)
        {
            job_report_task_finished (task->job);
        }

        bool free_task;
        KAN_ATOMIC_INT_COMPARE_AND_SET (&task->state)
        {
            KAN_ASSERT (old_value == TASK_STATE_RUNNING || old_value == TASK_STATE_RUNNING_DETACHED)
            free_task = old_value == TASK_STATE_RUNNING_DETACHED;
            new_value = TASK_STATE_FINISHED;
        }

        if (free_task)
        {
            kan_free_batched (global_task_dispatcher.allocation_group, task);
        }
    }
}

static void shutdown_global_task_dispatcher (void)
{
    kan_atomic_int_set (&global_task_dispatcher.shutting_down, 1);
    for (kan_loop_size_t index = 0u; index < global_task_dispatcher.threads_count; ++index)
    {
        kan_thread_wait (global_task_dispatcher.threads[index]);
    }
}

static void ensure_global_task_dispatcher_ready (void)
{
    if (!global_task_dispatcher_ready)
    {
        // Initialization clash is a really rare situation, but must be checked any way.
        KAN_ATOMIC_INT_SCOPED_LOCK (&global_task_dispatcher_init_lock)

        if (!global_task_dispatcher_ready)
        {
            kan_cpu_static_sections_ensure_initialized ();
            global_task_dispatcher.tasks_first = NULL;
            global_task_dispatcher.task_lock = kan_atomic_int_init (0);

            global_task_dispatcher.allocation_group =
                kan_allocation_group_get_child (kan_allocation_group_root (), "global_cpu_dispatcher");

            global_task_dispatcher.shutting_down = kan_atomic_int_init (0);
            global_task_dispatcher.threads_count = kan_platform_get_cpu_logical_core_count ();

            global_task_dispatcher.threads = kan_allocate_general (
                global_task_dispatcher.allocation_group, sizeof (kan_thread_t) * global_task_dispatcher.threads_count,
                alignof (kan_thread_t));

            for (kan_loop_size_t index = 0u; index < global_task_dispatcher.threads_count; ++index)
            {
                global_task_dispatcher.threads[index] =
                    kan_thread_create ("global_cpu_dispatcher_worker", worker_thread_function, 0u);
                KAN_ASSERT (KAN_HANDLE_IS_VALID (global_task_dispatcher.threads[index]))
            }

            global_task_dispatcher.dispatched_tasks_counter = kan_atomic_int_init (0);
            atexit (shutdown_global_task_dispatcher);
            global_task_dispatcher_ready = true;
        }
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

    kan_atomic_int_lock (&global_task_dispatcher.task_lock);
    task_node->next = global_task_dispatcher.tasks_first;
    global_task_dispatcher.tasks_first = task_node;
    kan_atomic_int_unlock (&global_task_dispatcher.task_lock);

    kan_atomic_int_add (&global_task_dispatcher.dispatched_tasks_counter, 1);
    return task_node;
}

static void dispatch_task_list (struct job_t *job, struct kan_cpu_task_list_node_t *tasks)
{
    ensure_global_task_dispatcher_ready ();
    KAN_ASSERT (tasks)

    kan_instance_size_t count = 0u;
    struct task_node_t *begin = NULL;
    struct task_node_t *end = NULL;

    while (tasks)
    {
        struct task_node_t *task_node = (struct task_node_t *) kan_allocate_batched (
            global_task_dispatcher.allocation_group, sizeof (struct task_node_t));
        task_node->job = job;
        task_node->task = tasks->task;
        task_node->state = kan_atomic_int_init (TASK_STATE_QUEUED);
        tasks->dispatch_handle = KAN_HANDLE_SET (kan_cpu_task_t, task_node);
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

    if (begin)
    {
        KAN_ATOMIC_INT_SCOPED_LOCK (&global_task_dispatcher.task_lock)
        end->next = global_task_dispatcher.tasks_first;
        global_task_dispatcher.tasks_first = begin;
    }

    kan_atomic_int_add (&global_task_dispatcher.dispatched_tasks_counter, (int) count);
}

kan_cpu_task_t kan_cpu_task_dispatch (struct kan_cpu_task_t task)
{
    return KAN_HANDLE_SET (kan_cpu_task_t, dispatch_task (NULL, task));
}

bool kan_cpu_task_is_finished (kan_cpu_task_t task)
{
    struct task_node_t *task_node = KAN_HANDLE_GET (task);
    return kan_atomic_int_get (&task_node->state) == TASK_STATE_FINISHED;
}

void kan_cpu_task_detach (kan_cpu_task_t task)
{
    struct task_node_t *task_node = KAN_HANDLE_GET (task);
    KAN_ATOMIC_INT_COMPARE_AND_SET (&task_node->state)
    {
        KAN_ASSERT (old_value == TASK_STATE_QUEUED || old_value == TASK_STATE_RUNNING ||
                    old_value == TASK_STATE_FINISHED)

        if (old_value == TASK_STATE_FINISHED)
        {
            kan_free_batched (global_task_dispatcher.allocation_group, task_node);
            return;
        }

        new_value = old_value;
        if (old_value == TASK_STATE_QUEUED)
        {
            new_value = TASK_STATE_QUEUED_DETACHED;
        }
        else if (old_value == TASK_STATE_RUNNING)
        {
            new_value = TASK_STATE_RUNNING_DETACHED;
        }
    }
}

void kan_cpu_task_dispatch_list (struct kan_cpu_task_list_node_t *list) { dispatch_task_list (NULL, list); }

kan_instance_size_t kan_cpu_get_task_dispatch_counter (void)
{
    return (kan_instance_size_t) kan_atomic_int_get (&global_task_dispatcher.dispatched_tasks_counter);
}

void kan_cpu_reset_task_dispatch_counter (void)
{
    kan_atomic_int_set (&global_task_dispatcher.dispatched_tasks_counter, 0);
}

static bool job_allocation_group_ready = false;
static kan_allocation_group_t job_allocation_group;

static void job_report_task_finished (struct job_t *job)
{
    unsigned int new_status_bits;
    unsigned int old_status_state;

    KAN_ATOMIC_INT_COMPARE_AND_SET (&job->status)
    {
        const unsigned int old_status_bits = (unsigned int) old_value;
        old_status_state = old_status_bits >> JOB_STATUS_TASK_COUNT_BITS;
        KAN_ASSERT (old_status_state != JOB_STATE_COMPLETED)

        KAN_ASSERT ((old_status_bits & JOB_STATUS_TASK_COUNT_MASK) > 0u)
        new_status_bits = old_status_bits - 1u;

        if (old_status_state != JOB_STATE_ASSEMBLING && (new_status_bits & JOB_STATUS_TASK_COUNT_MASK) == 0u)
        {
            new_status_bits = JOB_STATE_FINISHING << JOB_STATUS_TASK_COUNT_BITS;
        }

        new_value = (int) new_status_bits;
    }

    if (new_status_bits == (JOB_STATE_FINISHING << JOB_STATUS_TASK_COUNT_BITS))
    {
        // Job is now completed fully. Check old state to choose what to do.
        KAN_ASSERT (old_status_state == JOB_STATE_RELEASED || old_status_state == JOB_STATE_DETACHED)

        if (job->completion_task.function)
        {
            kan_cpu_task_detach (KAN_HANDLE_SET (kan_cpu_task_t, dispatch_task (NULL, job->completion_task)));
        }

        if (old_status_state == JOB_STATE_DETACHED)
        {
            kan_free_batched (job_allocation_group, job);
        }
        else
        {
#if defined(KAN_WITH_ASSERT)
            // We've already switched to finishing state, nothing should be able to bother us. Check it.
            KAN_ASSERT (kan_atomic_int_compare_and_set (&job->status, new_status_bits,
                                                        JOB_STATE_COMPLETED << JOB_STATUS_TASK_COUNT_BITS))
#else
            kan_atomic_int_set (&job->status, JOB_STATE_COMPLETED << JOB_STATUS_TASK_COUNT_BITS);
#endif
        }
    }
}

kan_cpu_job_t kan_cpu_job_create (void)
{
    if (!job_allocation_group_ready)
    {
        job_allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "cpu_job");
        job_allocation_group_ready = true;
    }

    struct job_t *job = (struct job_t *) kan_allocate_batched (job_allocation_group, sizeof (struct job_t));
    job->status = kan_atomic_int_init (JOB_STATUS_INITIAL);
    job->completion_task =
        (struct kan_cpu_task_t) {.profiler_section = KAN_HANDLE_INITIALIZE_INVALID, .function = NULL, .user_data = 0u};
    return KAN_HANDLE_SET (kan_cpu_job_t, job);
}

void kan_cpu_job_set_completion_task (kan_cpu_job_t job, struct kan_cpu_task_t completion_task)
{
    struct job_t *job_data = KAN_HANDLE_GET (job);
    KAN_ASSERT ((((unsigned int) kan_atomic_int_get (&job_data->status)) >> JOB_STATUS_TASK_COUNT_BITS) ==
                JOB_STATE_ASSEMBLING)
    job_data->completion_task = completion_task;
}

kan_cpu_task_t kan_cpu_job_dispatch_task (kan_cpu_job_t job, struct kan_cpu_task_t task)
{
    struct job_t *job_data = KAN_HANDLE_GET (job);
#if defined(KAN_WITH_ASSERT)
    unsigned int current_job_state =
        ((unsigned int) kan_atomic_int_get (&job_data->status)) >> JOB_STATUS_TASK_COUNT_BITS;
    KAN_ASSERT (current_job_state != JOB_STATE_FINISHING && current_job_state != JOB_STATE_COMPLETED)
#endif
    return KAN_HANDLE_SET (kan_cpu_task_t, dispatch_task (job_data, task));
}

void kan_cpu_job_dispatch_task_list (kan_cpu_job_t job, struct kan_cpu_task_list_node_t *list)
{
    struct job_t *job_data = KAN_HANDLE_GET (job);
#if defined(KAN_WITH_ASSERT)
    unsigned int current_job_state =
        ((unsigned int) kan_atomic_int_get (&job_data->status)) >> JOB_STATUS_TASK_COUNT_BITS;
    KAN_ASSERT (current_job_state != JOB_STATE_FINISHING && current_job_state != JOB_STATE_COMPLETED)
#endif
    dispatch_task_list (job_data, list);
}

void kan_cpu_job_release (kan_cpu_job_t job)
{
    struct job_t *job_data = KAN_HANDLE_GET (job);
    KAN_ASSERT ((((unsigned int) kan_atomic_int_get (&job_data->status)) >> JOB_STATUS_TASK_COUNT_BITS) ==
                JOB_STATE_ASSEMBLING)
    unsigned int new_status_bits;

    KAN_ATOMIC_INT_COMPARE_AND_SET (&job_data->status)
    {
        const unsigned int old_status_bits = (unsigned int) old_value;
        const unsigned int old_status_tasks = old_status_bits & JOB_STATUS_TASK_COUNT_MASK;

        if (old_status_tasks == 0u)
        {
            new_status_bits = JOB_STATE_COMPLETED << JOB_STATUS_TASK_COUNT_BITS;
        }
        else
        {
            new_status_bits = (JOB_STATE_RELEASED << JOB_STATUS_TASK_COUNT_BITS) | old_status_tasks;
        }

        new_value = (int) new_status_bits;
    }

    if (new_status_bits == (JOB_STATE_COMPLETED << JOB_STATUS_TASK_COUNT_BITS) && job_data->completion_task.function)
    {
        kan_cpu_task_detach (KAN_HANDLE_SET (kan_cpu_task_t, dispatch_task (NULL, job_data->completion_task)));
    }
}

void kan_cpu_job_detach (kan_cpu_job_t job)
{
    struct job_t *job_data = KAN_HANDLE_GET (job);
    KAN_ATOMIC_INT_COMPARE_AND_SET (&job_data->status)
    {
        const unsigned int old_status_bits = (unsigned int) old_value;
        const unsigned int old_status_state = old_status_bits >> JOB_STATUS_TASK_COUNT_BITS;
        KAN_ASSERT (old_status_state == JOB_STATE_RELEASED || old_status_state == JOB_STATE_FINISHING ||
                    old_status_state == JOB_STATE_COMPLETED)

        if (old_status_state == JOB_STATE_COMPLETED)
        {
            kan_free_batched (job_allocation_group, job_data);
            return;
        }
        else if (old_status_state == JOB_STATE_FINISHING)
        {
            continue;
        }

        const unsigned int old_status_tasks = old_status_bits & JOB_STATUS_TASK_COUNT_MASK;
        const unsigned int new_status_bits = (JOB_STATE_DETACHED << JOB_STATUS_TASK_COUNT_BITS) | old_status_tasks;
        new_value = (int) new_status_bits;
    }
}

void kan_cpu_job_wait (kan_cpu_job_t job)
{
    struct job_t *job_data = KAN_HANDLE_GET (job);
    if ((((unsigned int) kan_atomic_int_get (&job_data->status)) >> JOB_STATUS_TASK_COUNT_BITS) == JOB_STATE_COMPLETED)
    {
        return;
    }

    while (true)
    {
        const int old_status = kan_atomic_int_get (&job_data->status);
        const unsigned int old_status_bits = (unsigned int) old_status;
        const unsigned int old_status_state = old_status_bits >> JOB_STATUS_TASK_COUNT_BITS;
        KAN_ASSERT (old_status_state == JOB_STATE_RELEASED || old_status_state == JOB_STATE_FINISHING ||
                    old_status_state == JOB_STATE_COMPLETED)

        // Some time passed from fast-forward check above, so we might be completed as well.
        if (old_status_state == JOB_STATE_COMPLETED)
        {
            kan_free_batched (job_allocation_group, job_data);
            return;
        }
        else if (old_status_state == JOB_STATE_FINISHING)
        {
            // No need to wait, we're almost here.
            continue;
        }

        // From purist point of view, waiting like that is pretty bad. But there is a chain of thoughts for that:
        // - To wait properly, we need mutex with conditional variable.
        // - Also, thread status should only be changed under that mutex.
        // - Therefore, mutex and variable should always be created. Almost, task submission will be slower.
        // - But for the most jobs we don't need the await routine.
        // - And the jobs that use it can safely wait a little bit more.
        // Therefore, correct await routine would only harm here.
        kan_precise_time_sleep (KAN_CPU_DISPATCHER_WAIT_CHECK_DELAY_NS);
    }
}
