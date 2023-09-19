#pragma once

#include <threading_api.h>

#include <stdint.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>

/// \file
/// \brief Declares mutual exclusion API for multithreading.

KAN_C_HEADER_BEGIN

typedef uint64_t kan_mutex_handle_t;

#define KAN_INVALID_MUTEX_HANDLE 0u

/// \brief Creates new mutex instance.
THREADING_API kan_mutex_handle_t kan_mutex_create ();

/// \brief Locks given mutex. Pauses current thread if it is already locked.
/// \return Whether locking was successful.
THREADING_API kan_bool_t kan_mutex_lock (kan_mutex_handle_t handle);

/// \brief Attempts to lock given mutex. Returns even if locking wasn't possible.
/// \return Whether locking was successful.
THREADING_API kan_bool_t kan_mutex_try_lock (kan_mutex_handle_t handle);

/// \brief Unlocks given mutex.
/// \return Whether unlocking was successful.
THREADING_API kan_bool_t kan_mutex_unlock (kan_mutex_handle_t handle);

/// \brief Destroys given mutex instance. It should not be locked.
THREADING_API void kan_mutex_destroy (kan_mutex_handle_t handle);

KAN_C_HEADER_END
