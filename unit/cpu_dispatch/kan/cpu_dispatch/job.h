#pragma once

#include <cpu_dispatch_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/cpu_dispatch/task.h>
#include <kan/error/critical.h>

/// \file
/// \brief Describes job -- a high-level activity, that can be executed by multiple threads in parallel.
///
/// \par Job
/// \parblock
/// Technically, job is just aa group of tasks, that are executed in parallel and do not depend on each other, but
/// describe some high-level activity as a group. For example, game system or algorithm that migrates lots of objects.
/// In addition, job can automatically dispatch completion tasks once all other tasks are finished and you can also
/// wait for the job, putting calling thread to sleep until all tasks in a job are finished (excluding completion task).
/// \endparblock
///
/// \par Job lifecycle
/// \parblock
/// After job is created, it is in assembly state. It means that it waits for all required tasks to be dispatched in
/// job context using kan_cpu_job_dispatch_task or kan_cpu_job_dispatch_task_list. All tasks, dispatched using these
/// functions, will be registered as dependencies of this job. Keep in mind, that tasks, dispatched that way, are
/// added to the execution queue like any other tasks and might be executed right away. But job cannot be completed in
/// assembly state, therefore it is safe. To report that all tasks are added and job can be completed, you need to
/// release job using kan_cpu_job_release. Only after this point completion task might be dispatched, even if all other
/// tasks were finished during assembly state. After transitioning to release state, you can either wait for the job or
/// detach from it. Either way, once kan_cpu_job_detach or kan_cpu_job_wait exits, job handle is no longer valid.
/// Detaching does not stop the job, it only allows implementation to free its resources on completion. Also, cancelled
/// tasks do not block their job from completing, they'll just be ignored.
/// \endparblock

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_cpu_job_t);

/// \brief Creates new job instance.
CPU_DISPATCH_API kan_cpu_job_t kan_cpu_job_create (void);

/// \brief Sets task to be dispatched on job completion.
CPU_DISPATCH_API void kan_cpu_job_set_completion_task (kan_cpu_job_t job, struct kan_cpu_task_t completion_task);

/// \brief Dispatches task inside job context.
/// \invariant Job should either be in assembly state or dispatch should be called from inside of task that is
///            dispatched from this job. Otherwise, race conditions and crashes are possible.
CPU_DISPATCH_API kan_cpu_task_t kan_cpu_job_dispatch_task (kan_cpu_job_t job, struct kan_cpu_task_t task);

/// \brief Dispatches task list inside job context.
/// \invariant Job should either be in assembly state or dispatch should be called from inside of task that is
///            dispatched from this job. Otherwise, race conditions and crashes are possible.
CPU_DISPATCH_API void kan_cpu_job_dispatch_task_list (kan_cpu_job_t job, struct kan_cpu_task_list_node_t *list);

/// \brief Transfers job to release mode, making it possible to complete the job.
CPU_DISPATCH_API void kan_cpu_job_release (kan_cpu_job_t job);

/// \brief Invalidates job handle and allows implementation to automatically free the resources once job is completed.
CPU_DISPATCH_API void kan_cpu_job_detach (kan_cpu_job_t job);

/// \brief Puts current threads to sleep until job is completed.
/// \invariant This function should not be called inside tasks as it essentially bricks task dispatch logic.
CPU_DISPATCH_API void kan_cpu_job_wait (kan_cpu_job_t job);

/// \brief Inline utility function that calls ::kan_cpu_job_dispatch_task_list and then detaches all tasks.
/// \details Dispatch and immediate detach of job tasks is a common pattern, therefore we're adding syntax sugar for it.
static inline void kan_cpu_job_dispatch_and_detach_task_list (kan_cpu_job_t job, struct kan_cpu_task_list_node_t *list)
{
    if (list)
    {
        kan_cpu_job_dispatch_task_list (job, list);
        while (list)
        {
            KAN_ASSERT (KAN_HANDLE_IS_VALID (list->dispatch_handle))
            kan_cpu_task_detach (list->dispatch_handle);
            list = list->next;
        }
    }
}

KAN_C_HEADER_END
