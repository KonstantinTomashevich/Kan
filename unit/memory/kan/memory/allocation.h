#pragma once

#include <memory_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/memory_profiler/allocation_group.h>

/// \file
/// \brief Contains implementations for different memory allocation strategies.
///
/// \par General allocation
/// \parblock
/// General allocation can be used everywhere, but not as performant as optimized allocation strategies.
/// It should only be used in cases where there is no optimized allocation strategy that suits better than general
/// allocation, because general allocation is usually slower and might result in memory fragmentation.
/// \endparblock
///
/// \par Batched allocation
/// \parblock
/// Batched allocation is optimized for repeated allocation of small to medium size objects. It is both more performant
/// and less prone to memory fragmentation in this case. But it should never be used to allocate objects of rare sizes,
/// like singletons, of very big objects (they just aren't supported by this type of allocator).
/// \endparblock
///
/// \par Stack allocation
/// \parblock
/// Stack allocation is helpful when some algorithm allocates lots of arbitrary data that can be just "forgotten"
/// after algorithm execution. It is usually a fixed size stack to which items are being pushed. After algorithm
/// execution stack pointer is just returned back to zero and stack can be used again. It makes allocation
/// lighting-fast, but also is quite limited compared to other allocators.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// - General allocation is always thread safe.
/// - Batched allocation is always thread safe.
/// - Stack allocation is thread safe only when different threads use different instances.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Allocates given amount of memory with given alignment without registering it in profiler.
MEMORY_API void *kan_allocate_general_no_profiling (kan_memory_size_t amount, kan_memory_size_t alignment);

/// \brief Frees memory allocated using kan_allocate_general_no_profiling.
MEMORY_API void kan_free_general_no_profiling (void *memory);

/// \brief Allocates given amount of memory with given alignment and reports it to given profiling group.
MEMORY_API void *kan_allocate_general (kan_allocation_group_t group,
                                       kan_memory_size_t amount,
                                       kan_memory_size_t alignment);

/// \brief Frees memory allocated using kan_allocate_general and reports it to given profiling group.
MEMORY_API void kan_free_general (kan_allocation_group_t group, void *memory, kan_memory_size_t amount);

/// \brief Maximum size of an object in bytes supported by batched allocator implementation.
MEMORY_API kan_memory_size_t kan_get_batched_allocation_max_size (void);

/// \brief Allocates given amount of memory with alignment equal to its size using batched allocator.
MEMORY_API void *kan_allocate_batched (kan_allocation_group_t group, kan_memory_size_t item_size);

/// \brief Frees memory allocated using kan_allocate_batched.
MEMORY_API void kan_free_batched (kan_allocation_group_t group, void *memory);

KAN_HANDLE_DEFINE (kan_stack_allocator_t);

/// \brief Creates new instance of stack allocator with given fixed size.
/// \details All allocations will be done inside given group.
MEMORY_API kan_stack_allocator_t kan_stack_allocator_create (kan_allocation_group_t group, kan_memory_size_t amount);

/// \brief Allocates memory on top of given stack. Returns `NULL` on stack overflow.
MEMORY_API void *kan_stack_allocator_allocate (kan_stack_allocator_t allocator,
                                               kan_memory_size_t amount,
                                               kan_memory_size_t alignment);

/// \brief Returns given stack pointer back to zero.
MEMORY_API void kan_stack_allocator_reset (kan_stack_allocator_t allocator);

/// \brief Saves pointer to stack top that can be later used to partially reset stack.
MEMORY_API void *kan_stack_allocator_save_top (kan_stack_allocator_t allocator);

/// \brief Sets stack top to given value which usually causes partial stack reset.
MEMORY_API void kan_stack_allocator_load_top (kan_stack_allocator_t allocator, void *top);

/// \brief Returns total size of a stack allocator, specified during creation.
MEMORY_API kan_memory_size_t kan_stack_allocator_get_size (kan_stack_allocator_t allocator);

/// \brief Returns amount of bytes that can be allocated from stack allocator.
MEMORY_API kan_memory_size_t kan_stack_allocator_get_available (kan_stack_allocator_t allocator);

/// \brief Destroys given stack instance and frees its memory.
MEMORY_API void kan_stack_allocator_destroy (kan_stack_allocator_t allocator);

KAN_C_HEADER_END
