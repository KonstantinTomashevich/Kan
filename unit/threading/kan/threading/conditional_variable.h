#pragma once

#include <threading_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/threading/mutex.h>

/// \file
/// \brief Declares multithreading API for conditional variable primitive.

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_conditional_variable_t);

/// \brief Creates new conditional variable instance.
THREADING_API kan_conditional_variable_t kan_conditional_variable_create (void);

/// \brief Waits until given conditional variable is signaled, manages given mutex status.
THREADING_API kan_bool_t kan_conditional_variable_wait (kan_conditional_variable_t handle,
                                                        kan_mutex_t associated_mutex);

/// \brief Awakes one of the threads waiting for variable to signal.
THREADING_API kan_bool_t kan_conditional_variable_signal_one (kan_conditional_variable_t handle);

/// \brief Awakes all the threads waiting for variable to signal.
THREADING_API kan_bool_t kan_conditional_variable_signal_all (kan_conditional_variable_t handle);

/// \brief Destroys given conditional variable.
THREADING_API void kan_conditional_variable_destroy (kan_conditional_variable_t handle);

KAN_C_HEADER_END
