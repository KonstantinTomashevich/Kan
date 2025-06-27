#pragma once

#include <cpu_dispatch_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/api_common/min_max.h>
#include <kan/container/interned_string.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/platform/hardware.h>
#include <kan/reflection/markup.h>

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
    kan_cpu_task_function_t function;
    kan_functor_user_data_t user_data;
    kan_cpu_section_t profiler_section;
};

KAN_HANDLE_DEFINE (kan_cpu_task_t);

/// \brief Dispatches single task.
CPU_DISPATCH_API kan_cpu_task_t kan_cpu_task_dispatch (struct kan_cpu_task_t task);

/// \brief Checks whether task is finished.
CPU_DISPATCH_API bool kan_cpu_task_is_finished (kan_cpu_task_t task);

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

/// \brief Returns count of tasks dispatched since startup or last kan_cpu_reset_task_dispatch_counter call.
/// \details Dispatching tasks has its cost and game should not dispatch too many tasks per frame.
///          This counter makes it possible to measure amount of dispatched tasks per frame.
CPU_DISPATCH_API kan_instance_size_t kan_cpu_get_task_dispatch_counter (void);

/// \brief Resets counter of the dispatched tasks to zero.
CPU_DISPATCH_API void kan_cpu_reset_task_dispatch_counter (void);

/// \brief Syntax sugar for allocating cpu task with big user data (more than 64 bits) from temporary allocator and
///        adding it to the list of cpu tasks.
/// \param LIST_HEAD Pointer to pointer to list head (struct kan_cpu_task_list_node_t **).
/// \param TEMPORARY_ALLOCATOR Pointer to stack group allocator used for temporary allocation of user data and cpu task.
/// \param FUNCTION Task function to be executed.
/// \param SECTION Profiler section for proper reporting of task execution.
/// \param USER_TYPE User structure name with `struct` prefix.
/// \param ... User data designated initializer.
#define KAN_CPU_TASK_LIST_USER_STRUCT(LIST_HEAD, TEMPORARY_ALLOCATOR, FUNCTION, SECTION, USER_TYPE, ...)               \
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
            .function = FUNCTION,                                                                                      \
            .user_data = (kan_functor_user_data_t) user_data,                                                          \
            .profiler_section = SECTION,                                                                               \
        };                                                                                                             \
                                                                                                                       \
        new_node->next = *LIST_HEAD;                                                                                   \
        *LIST_HEAD = new_node;                                                                                         \
    }

/// \brief Syntax sugar for allocating cpu task with small user data (can be packed into 64 bits) from temporary
///        allocator and adding it to the list of cpu tasks.
/// \param LIST_HEAD Pointer to pointer to list head (struct kan_cpu_task_list_node_t **).
/// \param TEMPORARY_ALLOCATOR Pointer to stack group allocator used for temporary allocation of cpu task.
/// \param FUNCTION Task function to be executed.
/// \param SECTION Profiler section for proper reporting of task execution.
/// \param USER_VALUE User value that can be converted to `kan_functor_user_data_t`.
#define KAN_CPU_TASK_LIST_USER_VALUE(LIST_HEAD, TEMPORARY_ALLOCATOR, FUNCTION, SECTION, USER_VALUE)                    \
    {                                                                                                                  \
        _Static_assert (sizeof (USER_VALUE) <= sizeof (kan_functor_user_data_t),                                       \
                        "Do not use this for user data that cannot fit in pointer.");                                  \
                                                                                                                       \
        struct kan_cpu_task_list_node_t *new_node =                                                                    \
            KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (TEMPORARY_ALLOCATOR, struct kan_cpu_task_list_node_t);           \
                                                                                                                       \
        new_node->task = (struct kan_cpu_task_t) {                                                                     \
            .function = FUNCTION,                                                                                      \
            .user_data = (kan_functor_user_data_t) USER_VALUE,                                                         \
            .profiler_section = SECTION,                                                                               \
        };                                                                                                             \
                                                                                                                       \
        new_node->next = *LIST_HEAD;                                                                                   \
        *LIST_HEAD = new_node;                                                                                         \
    }

/// \brief Begins declaration of header data structure for cpu task batched registration.
/// \details Header data structure is a data structure that contains common parameters for all work units in a batch.
#define KAN_CPU_TASK_BATCHED_HEADER(TASK_NAME) KAN_REFLECTION_IGNORE struct TASK_NAME##_batched_task_header_t

/// \brief Begins declaration of body data structure for cpu task batched registration.
/// \details Body data structure is a data structure that contains parameters for one particular work unit.
#define KAN_CPU_TASK_BATCHED_BODY(TASK_NAME) KAN_REFLECTION_IGNORE struct TASK_NAME##_batched_task_body_t

/// \brief Defines internal user data type and executor for cpu task batching, then starts user executor definition.
/// \details User executor is a function that is called inside a cycle for every work unit in task user data.
#define KAN_CPU_TASK_BATCHED_DEFINE(TASK_NAME)                                                                         \
    KAN_REFLECTION_IGNORE struct TASK_NAME##_batched_task_user_data_t                                                  \
    {                                                                                                                  \
        struct TASK_NAME##_batched_task_header_t header;                                                               \
        kan_instance_size_t size;                                                                                      \
        struct TASK_NAME##_batched_task_body_t body[];                                                                 \
    };                                                                                                                 \
                                                                                                                       \
    static void TASK_NAME (struct TASK_NAME##_batched_task_header_t *header,                                           \
                           struct TASK_NAME##_batched_task_body_t *body);                                              \
                                                                                                                       \
    static void TASK_NAME##_batched_task_runner (kan_functor_user_data_t user_data)                                    \
    {                                                                                                                  \
        struct TASK_NAME##_batched_task_user_data_t *data = (struct TASK_NAME##_batched_task_user_data_t *) user_data; \
        for (kan_loop_size_t index = 0u; index < data->size; ++index)                                                  \
        {                                                                                                              \
            TASK_NAME (&data->header, &data->body[index]);                                                             \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    static void TASK_NAME (struct TASK_NAME##_batched_task_header_t *header,                                           \
                           struct TASK_NAME##_batched_task_body_t *body)

/// \brief Contains preparation logic for scheduling cpu tasks with batched work units.
/// \details Calculates task batch size and initializes task batch header data.
/// \param TASK_NAME Name of the cpu task with batching, used to declare functions and data structures.
/// \param MIN Minimum amount of work units inside one batch.
/// \param AVERAGE_TOTAL_EXPECTATION How many work units we expect on average, so we can calculate more optimal batch
///                                  size. Batch size is calculated in a way that if all cores are free and amount of
///                                  work units is equal to an average, then work is evenly split between all cores.
/// \param ... Header data designated initializer.
#define KAN_CPU_TASK_LIST_BATCHED_PREPARE(TASK_NAME, MIN, AVERAGE_TOTAL_EXPECTATION, ...)                              \
    const kan_instance_size_t TASK_NAME##_task_batch_size =                                                            \
        KAN_MAX (MIN, AVERAGE_TOTAL_EXPECTATION / kan_platform_get_cpu_logical_core_count ());                         \
    const struct TASK_NAME##_batched_task_header_t TASK_NAME##_batched_task_header = __VA_ARGS__

/// \brief Appends new work unit to cpu task list with batching.
/// \details New cpu task nodes in list are created on per need basis.
///          Work unit batching inside CPU tasks is needed because tasks are relatively costly and we cannot create new
///          tasks for every small work unit. We need to batch work units into larger tasks in order to be effective.
/// \param LIST_HEAD Pointer to pointer to list head (struct kan_cpu_task_list_node_t **).
/// \param TEMPORARY_ALLOCATOR Pointer to stack group allocator used for temporary allocation of user data and tasks.
/// \param TASK_NAME Name of the cpu task with batching, used to declare functions and data structures.
/// \param SECTION Profiler section for proper reporting of task execution.
/// \param ... Work unit body designated initializer.
#define KAN_CPU_TASK_LIST_BATCHED(LIST_HEAD, TEMPORARY_ALLOCATOR, TASK_NAME, SECTION, ...)                             \
    if (!*LIST_HEAD || ((struct TASK_NAME##_batched_task_user_data_t *) (*LIST_HEAD)->task.user_data)->size >=         \
                           TASK_NAME##_task_batch_size)                                                                \
    {                                                                                                                  \
        {                                                                                                              \
            struct TASK_NAME##_batched_task_user_data_t *user_data = kan_stack_group_allocator_allocate (              \
                TEMPORARY_ALLOCATOR,                                                                                   \
                sizeof (struct TASK_NAME##_batched_task_user_data_t) +                                                 \
                    sizeof (struct TASK_NAME##_batched_task_body_t) * TASK_NAME##_task_batch_size,                     \
                _Alignof (struct TASK_NAME##_batched_task_user_data_t));                                               \
                                                                                                                       \
            user_data->size = 0u;                                                                                      \
            user_data->header = TASK_NAME##_batched_task_header;                                                       \
                                                                                                                       \
            struct kan_cpu_task_list_node_t *new_node =                                                                \
                KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (TEMPORARY_ALLOCATOR, struct kan_cpu_task_list_node_t);       \
                                                                                                                       \
            new_node->task = (struct kan_cpu_task_t) {                                                                 \
                .function = TASK_NAME##_batched_task_runner,                                                           \
                .user_data = (kan_functor_user_data_t) user_data,                                                      \
                .profiler_section = SECTION,                                                                           \
            };                                                                                                         \
                                                                                                                       \
            new_node->next = *LIST_HEAD;                                                                               \
            *LIST_HEAD = new_node;                                                                                     \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    struct TASK_NAME##_batched_task_user_data_t *TASK_NAME##_batched_user_data =                                       \
        (struct TASK_NAME##_batched_task_user_data_t *) (*LIST_HEAD)->task.user_data;                                  \
                                                                                                                       \
    TASK_NAME##_batched_user_data->body[TASK_NAME##_batched_user_data->size] =                                         \
        (struct TASK_NAME##_batched_task_body_t) __VA_ARGS__;                                                          \
    ++TASK_NAME##_batched_user_data->size

KAN_C_HEADER_END
