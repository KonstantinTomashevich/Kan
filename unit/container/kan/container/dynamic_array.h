#pragma once

#include <container_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/memory_profiler/allocation_group.h>

/// \file
/// \brief Contains implementation of resizable array for trivially copyable and trivially movable items.
///
/// \par Definition
/// \parblock
/// Dynamic array is a wrapper on top of usual fixed size array that is able to change its capacity when requested.
/// \endparblock
///
/// \par Allocation policy
/// \parblock
/// All allocations are done using general allocator inside given allocation group.
/// \endparblock
///
/// \par Usage
/// \parblock
/// Dynamic array can be stored anywhere as `kan_dynamic_array_t` structure. Use `kan_dynamic_array_init` to initialize
/// array and claim resources and `kan_dynamic_array_shutdown` to release resources held by array. Keep in mind that
/// array knows nothing about item destruction, therefore it must be done manually before releasing resources.
///
/// Item management is fully described in documentation for appropriate functions that work with `kan_dynamic_array_t`.
///
/// Capacity is expected to be manually managed by user in order to make capacity management as efficient as possible.
/// It is better to let user decide what to do on overflow than just increase capacity by some modifier.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// This dynamic array implementation is not thread safe.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Contains all the data about dynamic array instance.
struct kan_dynamic_array_t
{
    kan_instance_size_t size;
    kan_instance_size_t capacity;
    uint8_t *data;

    kan_instance_size_t item_size;
    kan_instance_size_t item_alignment;
    kan_allocation_group_t allocation_group;
};

/// \brief Initializes given array instance for usage.
/// \param array Pointer to array instance.
/// \param initial_capacity Initial capacity of the array.
/// \param item_size Size of one item in bytes.
/// \param item_alignment Required alignment for items
/// \param allocation_group Allocation group to log memory usage.
CONTAINER_API void kan_dynamic_array_init (struct kan_dynamic_array_t *array,
                                           kan_instance_size_t initial_capacity,
                                           kan_instance_size_t item_size,
                                           kan_instance_size_t item_alignment,
                                           kan_allocation_group_t allocation_group);

/// \brief Attempts to increase array size, returns pointer to usable memory block for one item on success.
/// \details If array is already full, `NULL` will be returned instead of correct address.
CONTAINER_API void *kan_dynamic_array_add_last (struct kan_dynamic_array_t *array);

/// \brief Attempts to increase array size, returns pointer to memory block for one item at given index on success.
/// \details All the items after given index are moved using memory copying.
///          If array is already full, `NULL` will be returned instead of correct address.
CONTAINER_API void *kan_dynamic_array_add_at (struct kan_dynamic_array_t *array, kan_instance_size_t index);

/// \brief Removes item at given index. All the items after given index are moved using memory copying.
CONTAINER_API void kan_dynamic_array_remove_at (struct kan_dynamic_array_t *array, kan_instance_size_t index);

/// \brief Removes item at given index and copies last item to its place.
CONTAINER_API void kan_dynamic_array_remove_swap_at (struct kan_dynamic_array_t *array, kan_instance_size_t index);

/// \brief Sets new array capacity and moves all items to new data storage.
/// \invariant Capacity should not be zero.
CONTAINER_API void kan_dynamic_array_set_capacity (struct kan_dynamic_array_t *array, kan_instance_size_t new_capacity);

/// \brief Resets array size to zero.
/// \warning Keep in mind that array knows nothing about item destruction,
///          therefore it must be done manually before releasing resources.
CONTAINER_API void kan_dynamic_array_reset (struct kan_dynamic_array_t *array);

/// \brief Deinitializes array and releases all used resources.
/// \warning Keep in mind that array knows nothing about item destruction,
///          therefore it must be done manually before releasing resources.
CONTAINER_API void kan_dynamic_array_shutdown (struct kan_dynamic_array_t *array);

KAN_C_HEADER_END
