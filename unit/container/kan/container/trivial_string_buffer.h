#pragma once

#include <container_api.h>

#include <memory.h>
#include <stdio.h>
#include <string.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/min_max.h>
#include <kan/error/critical.h>
#include <kan/memory/allocation.h>

/// \file
/// \brief Contains inline implementation for trivial string buffer with append operations.

KAN_C_HEADER_BEGIN

/// \brief Trivial string buffer structure.
struct kan_trivial_string_buffer_t
{
    kan_instance_size_t size;
    kan_instance_size_t capacity;
    kan_allocation_group_t allocation_group;
    char *buffer;
};

/// \brief Initializes trivial string buffer with initial non-zero capacity.
static inline void kan_trivial_string_buffer_init (struct kan_trivial_string_buffer_t *instance,
                                                   kan_allocation_group_t allocation_group,
                                                   kan_instance_size_t capacity)
{
    KAN_ASSERT (capacity > 0u)
    instance->size = 0u;
    instance->capacity = capacity;
    instance->allocation_group = allocation_group;
    instance->buffer = kan_allocate_general (allocation_group, capacity, _Alignof (char));
}

/// \brief Appends given amount of character from given input string to string buffer.
static inline void kan_trivial_string_buffer_append_char_sequence (struct kan_trivial_string_buffer_t *instance,
                                                                   const char *begin,
                                                                   kan_instance_size_t length)
{
    if (length > 0u)
    {
        if (instance->size + length >= instance->capacity)
        {
            const kan_instance_size_t new_capacity = KAN_MAX (instance->size + length + 1u, instance->capacity * 2u);
            char *new_buffer = kan_allocate_general (instance->allocation_group, new_capacity, _Alignof (char));

            memcpy (new_buffer, instance->buffer, instance->size);
            kan_free_general (instance->allocation_group, instance->buffer, instance->capacity);

            instance->capacity = new_capacity;
            instance->buffer = new_buffer;
        }

        memcpy (instance->buffer + instance->size, begin, length);
        instance->size += length;
        instance->buffer[instance->size] = '\0';
    }
}

/// \brief Appends given null terminated string to string buffer.
static inline void kan_trivial_string_buffer_append_string (struct kan_trivial_string_buffer_t *instance,
                                                            const char *string)
{
    kan_trivial_string_buffer_append_char_sequence (instance, string, (kan_instance_size_t) strlen (string));
}

/// \brief Formats and appends given unsigned long to string buffer.
static inline void kan_trivial_string_buffer_append_unsigned_long (struct kan_trivial_string_buffer_t *instance,
                                                                   unsigned long value)
{
    char buffer[64u];
    snprintf (buffer, 63u, "%lu", value);
    buffer[63u] = '\0';
    kan_trivial_string_buffer_append_string (instance, buffer);
}

/// \brief Formats and appends given signed long to string buffer.
static inline void kan_trivial_string_buffer_append_signed_long (struct kan_trivial_string_buffer_t *instance,
                                                                 long value)
{
    char buffer[64u];
    snprintf (buffer, 63u, "%ld", value);
    buffer[63u] = '\0';
    kan_trivial_string_buffer_append_string (instance, buffer);
}

/// \brief Sets buffer size to given size which is smaller than current, forgetting excess characters.
static inline void kan_trivial_string_buffer_reset (struct kan_trivial_string_buffer_t *instance,
                                                    kan_instance_size_t new_size)
{
    KAN_ASSERT (new_size <= instance->size)
    instance->size = new_size;
}

/// \brief Shuts down string buffer and frees its resources.
static inline void kan_trivial_string_buffer_shutdown (struct kan_trivial_string_buffer_t *instance)
{
    kan_free_general (instance->allocation_group, instance->buffer, instance->capacity);
}

KAN_C_HEADER_END
