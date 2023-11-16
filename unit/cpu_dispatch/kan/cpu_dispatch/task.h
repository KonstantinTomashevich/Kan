#pragma once

#include <cpu_dispatch_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/container/interned_string.h>

/// \file
/// \brief Describes tasks -- minimal multithreading units that can be run on different CPU threads.
///
/// \par Task
/// \parblock
/// Task as a minimal multithreading unit that is described as a tuple of function, 64-bit user data parameter and name
/// for debugging and profiling. Tasks are used to defer some work to other threads with minimal overhead.
/// \endparblock
///
/// \par Task dispatch
/// \parblock
/// Act of submitting task for execution is called task dispatch. You can dispatch single task through
/// kan_cpu_task_dispatch or list of tasks using kan_cpu_task_dispatch_list. It is advised to dispatch tasks through
/// lists when you have more than one task, because it is results in less locking overhead.
/// \endparblock
///
/// \par Task lifecycle
/// \parblock
/// After you've dispatched task, it is registered inside internal task dispatcher and you receive handle that allows
/// you to interact with dispatched task. For example, query whether it is finished using kan_cpu_task_is_finished or
/// cancel it using kan_cpu_task_cancel. When you don't need handle anymore, you should detach it using
/// kan_cpu_task_detach to avoid memory leak as task info is stored until handle is detached. Detaching does not cancel
/// or stop task execution, therefore you can detach right after dispatch.
/// \endparblock
///
/// \par Task queues
/// \parblock
/// There are two queues for tasks: foreground and background. Foreground queue is optimized for relatively small
/// tasks that keep application running and responsive. Background queue is optimized for relatively big tasks that
/// need more time to be processed and do not actively block application. Other than that, implementations are allowed
/// to handle queues in a way that fits implementation better.
/// \endparblock

KAN_C_HEADER_BEGIN

typedef uint64_t kan_cpu_task_user_data_t;

typedef void (*kan_cpu_task_function_t) (kan_cpu_task_user_data_t);

/// \brief Describes a task to be dispatched.
struct kan_cpu_task_t
{
    kan_interned_string_t name;
    kan_cpu_task_function_t function;
    kan_cpu_task_user_data_t user_data;
};

/// \brief Enumerates task queues.
enum kan_cpu_dispatch_queue_t
{
    KAN_CPU_DISPATCH_QUEUE_FOREGROUND = 0u,
    KAN_CPU_DISPATCH_QUEUE_BACKGROUND,
};

typedef uint64_t kan_cpu_task_handle_t;

#define KAN_INVALID_CPU_TASK_HANDLE 0u

/// \brief Dispatches single task and adds it to appropriate queue.
CPU_DISPATCH_API kan_cpu_task_handle_t kan_cpu_task_dispatch (struct kan_cpu_task_t task,
                                                              enum kan_cpu_dispatch_queue_t queue);

/// \brief Checks whether task is finished.
CPU_DISPATCH_API kan_bool_t kan_cpu_task_is_finished (kan_cpu_task_handle_t task);

/// \brief Cancels task unless it is already executing. Returns whether task was cancelled.
CPU_DISPATCH_API kan_bool_t kan_cpu_task_cancel (kan_cpu_task_handle_t task);

/// \brief Detaches task handle and allows dispatcher to free resources when needed. Handle is no longer usable.
CPU_DISPATCH_API void kan_cpu_task_detach (kan_cpu_task_handle_t task);

/// \brief Describes list node for submitting multiple tasks at once.
struct kan_cpu_task_list_node_t
{
    struct kan_cpu_task_list_node_t *next;
    struct kan_cpu_task_t task;
    enum kan_cpu_dispatch_queue_t queue;

    /// \brief Field for storing dispatched task handle after dispatch.
    kan_cpu_task_handle_t dispatch_handle;
};

/// \brief Dispatches list of tasks. Advised when you have multiple tasks to be dispatched.
CPU_DISPATCH_API void kan_cpu_task_dispatch_list (struct kan_cpu_task_list_node_t *list);

KAN_C_HEADER_END
