#include <memory.h>

#include <kan/container/dynamic_array.h>
#include <kan/error/critical.h>
#include <kan/memory/allocation.h>

void kan_dynamic_array_init (struct kan_dynamic_array_t *array,
                             kan_instance_size_t initial_capacity,
                             kan_instance_size_t item_size,
                             kan_instance_size_t item_alignment,
                             kan_allocation_group_t allocation_group)
{
    array->size = 0;
    array->capacity = initial_capacity;

    if (initial_capacity > 0u)
    {
        array->data = (uint8_t *) kan_allocate_general (allocation_group, initial_capacity * item_size, item_alignment);
    }
    else
    {
        array->data = NULL;
    }

    // Custom case for byte-sized array with special alignment requirements.
    KAN_ASSERT (item_size % item_alignment == 0u || item_size == 1u)
    array->item_size = item_size;
    array->item_alignment = item_alignment;
    array->allocation_group = allocation_group;
}

void kan_dynamic_array_init_move (struct kan_dynamic_array_t *array, struct kan_dynamic_array_t *move_from_array)
{
    *array = *move_from_array;
    move_from_array->size = 0u;
    move_from_array->capacity = 0u;
    move_from_array->data = NULL;
}

void *kan_dynamic_array_add_last (struct kan_dynamic_array_t *array)
{
    if (array->size == array->capacity)
    {
        return NULL;
    }

    ++array->size;
    return array->data + (array->size - 1u) * array->item_size;
}

void *kan_dynamic_array_add_at (struct kan_dynamic_array_t *array, kan_instance_size_t index)
{
    if (array->size == array->capacity)
    {
        return NULL;
    }

    KAN_ASSERT (index < array->size)
    for (kan_loop_size_t output_index = array->size; output_index > index; --output_index)
    {
        memcpy (array->data + output_index * array->item_size, array->data + (output_index - 1u) * array->item_size,
                array->item_size);
    }

    ++array->size;
    return array->data + index * array->item_size;
}

void kan_dynamic_array_remove_at (struct kan_dynamic_array_t *array, kan_instance_size_t index)
{
    KAN_ASSERT (index < array->size)
    for (kan_loop_size_t output_index = index; output_index < array->size - 1u; ++output_index)
    {
        memcpy (array->data + output_index * array->item_size, array->data + (output_index + 1u) * array->item_size,
                array->item_size);
    }

    --array->size;
}

void kan_dynamic_array_remove_swap_at (struct kan_dynamic_array_t *array, kan_instance_size_t index)
{
    KAN_ASSERT (index < array->size)
    if (index != array->size - 1u)
    {
        memcpy (array->data + index * array->item_size, array->data + (array->size - 1u) * array->item_size,
                array->item_size);
    }

    --array->size;
}

void kan_dynamic_array_set_capacity (struct kan_dynamic_array_t *array, kan_instance_size_t new_capacity)
{
    if (new_capacity == array->capacity)
    {
        return;
    }

    KAN_ASSERT (new_capacity >= array->size)
    uint8_t *new_data = NULL;

    if (new_capacity > 0u)
    {
        new_data = (uint8_t *) kan_allocate_general (array->allocation_group, new_capacity * array->item_size,
                                                     array->item_alignment);

        if (array->data && array->size > 0u)
        {
            memcpy (new_data, array->data, array->size * array->item_size);
        }
    }

    if (array->data)
    {
        kan_free_general (array->allocation_group, array->data, array->capacity * array->item_size);
    }

    array->data = new_data;
    array->capacity = new_capacity;
}

void kan_dynamic_array_reset (struct kan_dynamic_array_t *array) { array->size = 0u; }

void kan_dynamic_array_shutdown (struct kan_dynamic_array_t *array)
{
    if (array->data)
    {
        kan_free_general (array->allocation_group, array->data, array->capacity * array->item_size);
    }
}
