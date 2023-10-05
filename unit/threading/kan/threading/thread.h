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

typedef uint64_t kan_thread_local_storage_t;

/// \brief Indicates that thread local storage handle is invalid.
#define KAN_THREAD_LOCAL_STORAGE_INVALID 0u

typedef void (*kan_thread_local_storage_destructor_t) (void *);

/// \brief Creates new thread local storage slot handle.
/// \details Before using thread local storage slots through `kan_thread_local_storage_set` and
///          `kan_thread_local_storage_get`, appropriate shared handle should be created.
THREADING_API kan_thread_local_storage_t kan_thread_local_storage_create ();

/// \brief Sets value of thread local storage slot for current thread only. Destructor is executed on thread exit.
THREADING_API void kan_thread_local_storage_set (kan_thread_local_storage_t storage,
                                                 void *value,
                                                 kan_thread_local_storage_destructor_t destructor);

/// \brief Returns value of thread local storage slot for current thread.
THREADING_API void *kan_thread_local_storage_get (kan_thread_local_storage_t storage);

KAN_C_HEADER_END
