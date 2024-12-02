#pragma once

#include <context_update_system_api.h>

#include <kan/api_common/c_header.h>
#include <kan/context/context.h>

/// \file
/// \brief Contains API for context update system -- lightweight update cycle with dependencies implementation.
///
/// \par Definition
/// \parblock
/// Goal of the update system is to register all update callbacks and sort them in topological order using provided
/// dependency handles. Systems provided through dependency routine are allowed to never be connected. In that case,
/// these dependencies will be skipped.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// Like other top-level context systems, update system is not thread safe.
/// \endparblock

KAN_C_HEADER_BEGIN

typedef void (*kan_context_update_run_t) (kan_context_system_t system);

/// \brief Connect other system as update delegate.
/// \details System handles in dependencies and dependency of arrays are allowed to be invalid for code simplification.
CONTEXT_UPDATE_SYSTEM_API void kan_update_system_connect_on_run (kan_context_system_t update_system,
                                                                 kan_context_system_t other_system,
                                                                 kan_context_update_run_t functor,
                                                                 kan_instance_size_t dependencies_count,
                                                                 kan_context_system_t *dependencies,
                                                                 kan_instance_size_t dependency_of_count,
                                                                 kan_context_system_t *dependency_of);

/// \brief Disconnect other system from update delegates.
CONTEXT_UPDATE_SYSTEM_API void kan_update_system_disconnect_on_run (kan_context_system_t update_system,
                                                                    kan_context_system_t other_system);

/// \brief Runs all update delegates in appropriate order if possible.
CONTEXT_UPDATE_SYSTEM_API void kan_update_system_run (kan_context_system_t update_system);

KAN_C_HEADER_END
