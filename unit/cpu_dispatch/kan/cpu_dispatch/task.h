#pragma once

#include <cpu_dispatch_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/container/interned_string.h>
#include <kan/container/stack_group_allocator.h>

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
/// you to interact with dispatched task. For example, query whether it is finished using kan_cpu_task_is_finished.
/// When you don't need handle anymore, you should detach it using kan_cpu_task_detach to avoid memory leak as task info
/// is stored until handle is detached. Detaching does not cancel or stop task execution, therefore you can detach right
/// after dispatch.
/// \endparblock

KAN_C_HEADER_BEGIN

typedef void (*kan_cpu_task_function_t) (kan_functor_user_data_t);

/// \brief Describes a task to be dispatched.
struct kan_cpu_task_t
{
    kan_interned_string_t name;
    kan_cpu_task_function_t function;
    kan_functor_user_data_t user_data;
};

KAN_HANDLE_DEFINE (kan_cpu_task_t);

/// \brief Dispatches single task.
CPU_DISPATCH_API kan_cpu_task_t kan_cpu_task_dispatch (struct kan_cpu_task_t task);

/// \brief Checks whether task is finished.
CPU_DISPATCH_API kan_bool_t kan_cpu_task_is_finished (kan_cpu_task_t task);

/// \brief Detaches task handle and allows dispatcher to free resources when needed. Handle is no longer usable.
CPU_DISPATCH_API void kan_cpu_task_detach (kan_cpu_task_t task);

/// \brief Describes list node for submitting multiple tasks at once.
struct kan_cpu_task_list_node_t
{
    struct kan_cpu_task_list_node_t *next;
    struct kan_cpu_task_t task;

    /// \brief Field for storing dispatched task handle after dispatch.
    kan_cpu_task_t dispatch_handle;
};

/// \brief Dispatches list of tasks. Advised when you have multiple tasks to be dispatched.
CPU_DISPATCH_API void kan_cpu_task_dispatch_list (struct kan_cpu_task_list_node_t *list);

/// \brief Syntax sugar for allocating cpu task with big user data (more than 64 bits) from temporary allocator and
///        adding it to the list of cpu tasks.
/// \param LIST_HEAD Pointer to pointer to list head (struct kan_cpu_task_list_node_t **).
/// \param TEMPORARY_ALLOCATOR Pointer to stack group allocator used for temporary allocation of user data and cpu task.
/// \param NAME Name of the task. Interned string.
/// \param FUNCTION Task function to be executed.
/// \param USER_TYPE User structure name with `struct` prefix.
/// \param ... User data designated initializer
#define KAN_CPU_TASK_LIST_USER_STRUCT(LIST_HEAD, TEMPORARY_ALLOCATOR, NAME, FUNCTION, USER_TYPE, ...)                  \
    {                                                                                                                  \
        _Static_assert (sizeof (USER_TYPE) > sizeof (kan_functor_user_data_t),                                         \
                        "Do not use this for user data that can fit in pointer.");                                     \
                                                                                                                       \
        USER_TYPE *user_data = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (TEMPORARY_ALLOCATOR, USER_TYPE);              \
        *user_data = (USER_TYPE) __VA_ARGS__;                                                                          \
                                                                                                                       \
        struct kan_cpu_task_list_node_t *new_node =                                                                    \
            KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (TEMPORARY_ALLOCATOR, struct kan_cpu_task_list_node_t);           \
                                                                                                                       \
        new_node->task = (struct kan_cpu_task_t) {                                                                     \
            .name = NAME,                                                                                              \
            .function = FUNCTION,                                                                                      \
            .user_data = (kan_functor_user_data_t) user_data,                                                          \
        };                                                                                                             \
                                                                                                                       \
        new_node->next = *LIST_HEAD;                                                                                   \
        *LIST_HEAD = new_node;                                                                                         \
    }

/// \brief Syntax sugar for allocating cpu task with small user data (can be packed into 64 bits) from temporary
///        allocator and adding it to the list of cpu tasks.
/// \param LIST_HEAD Pointer to pointer to list head (struct kan_cpu_task_list_node_t **).
/// \param TEMPORARY_ALLOCATOR Pointer to stack group allocator used for temporary allocation of cpu task.
/// \param NAME Name of the task. Interned string.
/// \param FUNCTION Task function to be executed.
/// \param USER_VALUE User value that can be converted to `kan_functor_user_data_t`.
#define KAN_CPU_TASK_LIST_USER_VALUE(LIST_HEAD, TEMPORARY_ALLOCATOR, NAME, FUNCTION, USER_VALUE)                       \
    {                                                                                                                  \
        _Static_assert (sizeof (USER_VALUE) <= sizeof (kan_functor_user_data_t),                                       \
                        "Do not use this for user data that cannot fit in pointer.");                                  \
                                                                                                                       \
        struct kan_cpu_task_list_node_t *new_node =                                                                    \
            KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (TEMPORARY_ALLOCATOR, struct kan_cpu_task_list_node_t);           \
                                                                                                                       \
        new_node->task = (struct kan_cpu_task_t) {                                                                     \
            .name = NAME,                                                                                              \
            .function = FUNCTION,                                                                                      \
            .user_data = (kan_functor_user_data_t) USER_VALUE,                                                         \
        };                                                                                                             \
                                                                                                                       \
        new_node->next = *LIST_HEAD;                                                                                   \
        *LIST_HEAD = new_node;                                                                                         \
    }

KAN_C_HEADER_END
