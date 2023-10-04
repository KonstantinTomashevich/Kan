#pragma once

#include <threading_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \details Provides thread management API.

KAN_C_HEADER_BEGIN

typedef uint64_t kan_thread_handle_t;

#define KAN_INVALID_THREAD_HANDLE 0u

typedef int kan_thread_result_t;

typedef void *kan_thread_user_data_t;

typedef kan_thread_result_t (*kan_thread_function_t) (kan_thread_user_data_t);

/// \brief Enumerates supported thread execution priorities.
enum kan_thread_priority_t
{
    KAN_THREAD_PRIORITY_LOW = 0,
    KAN_THREAD_PRIORITY_NORMAL,
    KAN_THREAD_PRIORITY_HIGH,
    KAN_THREAD_PRIORITY_TIME_CRITICAL,
    KAN_THREAD_PRIORITY_DEFAULT = KAN_THREAD_PRIORITY_NORMAL,
};

/// \brief Creates new thread with given name that executes given function with given user data.
THREADING_API kan_thread_handle_t kan_thread_create (const char *name, kan_thread_function_t function, void *data);

/// \brief Waits until thread stop its execution and deallocates its resources.
/// \invariant Should be called to shutdown the thread and free resources even if thread has finished executing.
/// \return Result value of thread execution.
THREADING_API kan_thread_result_t kan_thread_wait (kan_thread_handle_t handle);

/// \brief Queries name of the thread with given handle.
THREADING_API const char *kan_thread_get_name (kan_thread_handle_t handle);

/// \brief Queries handle of the invoking thread.
THREADING_API kan_thread_handle_t kan_current_thread ();

/// \brief Sets priority of the invoking thread.
THREADING_API const char *kan_current_thread_set_priority (enum kan_thread_priority_t priority);

KAN_C_HEADER_END
