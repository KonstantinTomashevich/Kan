#pragma once

#include <memory_profiler_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>

/// \file
/// \brief Contains functions for reporting memory usage.
///
/// \par Allocation group
/// \parblock
/// Every profiled allocation is connected to allocation group -- item of tree-like structure that makes it possible
/// to represent memory usage as grouped flame graph. That is quite useful as it makes it possible to both observe
/// memory usage from general point of view by looking at the top nodes and to see memory usage details individually.
/// \endparblock
///
/// \par Allocation group stack
/// \parblock
/// There are thread local allocation group stacks that make it easy to inject allocation group subtrees one into
/// another. Instead of requesting allocation group in every function call, it is advised to push selected parent
/// group onto stack and let used module decide how to use it. Allocation group stack is totally optional
/// convenience-oriented feature.
/// \endparblock

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_allocation_group_t);

/// \brief Value of `kan_allocation_group_t` that actually means that allocation should not be reported.
#define KAN_ALLOCATION_GROUP_IGNORE KAN_HANDLE_SET_INVALID (kan_allocation_group_t)

/// \brief Identifier of allocation group tree root.
MEMORY_PROFILER_API kan_allocation_group_t kan_allocation_group_root (void);

/// \brief Gets or creates child of given allocation group with given name.
/// \details Can be relatively slow, therefore it is advised to get identifier once and cache it somewhere.
MEMORY_PROFILER_API kan_allocation_group_t kan_allocation_group_get_child (kan_allocation_group_t parent,
                                                                           const char *name);

/// \brief Reports allocation of given amount of memory to given group.
MEMORY_PROFILER_API void kan_allocation_group_allocate (kan_allocation_group_t group, kan_memory_size_t amount);

/// \brief Reports deallocation of given amount of memory from given group.
MEMORY_PROFILER_API void kan_allocation_group_free (kan_allocation_group_t group, kan_memory_size_t amount);

/// \brief Adds human readable marker to allocation group history (if it is being observed).
/// \details Given marker name is copied inside on invocation.
MEMORY_PROFILER_API void kan_allocation_group_marker (kan_allocation_group_t group, const char *name);

/// \brief Allocation group on top of the stack or root group if stack is empty.
MEMORY_PROFILER_API kan_allocation_group_t kan_allocation_group_stack_get (void);

/// \brief Pushes given group to the top.
MEMORY_PROFILER_API void kan_allocation_group_stack_push (kan_allocation_group_t group);

/// \brief Pops the top group from the stack.
MEMORY_PROFILER_API void kan_allocation_group_stack_pop (void);

KAN_C_HEADER_END
