#pragma once

#if !defined(_CRT_SECURE_NO_WARNINGS)
#    define _CRT_SECURE_NO_WARNINGS
#endif

#include <string.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/api_common/min_max.h>

/// \file
/// \brief Contains inline implementation for file system entry path container and operations on it.
///
/// \par Path container
/// \parblock
/// Path container is a buffer of maximum path length that is used to store path to file system entry. Its main goal
/// is to provide operations that make it possible to avoid unnecessary allocations during file system lookup.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Contains buffer for file system entry path.
struct kan_file_system_path_container_t
{
    kan_instance_size_t length;
    char path[KAN_FILE_SYSTEM_MAX_PATH_LENGTH];
};

/// \brief Resets path container length to given value and places null terminator at appropriate position.
static inline void kan_file_system_path_container_reset_length (struct kan_file_system_path_container_t *container,
                                                                kan_instance_size_t new_length)
{
    container->length = KAN_MIN (KAN_FILE_SYSTEM_MAX_PATH_LENGTH - 1u, new_length);
    container->path[container->length] = '\0';
}

/// \brief Initializes path container by copying path from given char sequence.
static inline void kan_file_system_path_container_copy_char_sequence (
    struct kan_file_system_path_container_t *target_container,
    const char *arbitrary_path_begin,
    const char *arbitrary_path_end)
{
    const kan_instance_size_t arbitrary_path_length = (kan_instance_size_t) (arbitrary_path_end - arbitrary_path_begin);
    kan_file_system_path_container_reset_length (target_container, arbitrary_path_length);

    if (target_container->length > 0u)
    {
        memcpy (target_container->path, arbitrary_path_begin, target_container->length);
    }
}

/// \brief Initializes path container by copying path from given string.
static inline void kan_file_system_path_container_copy_string (
    struct kan_file_system_path_container_t *target_container, const char *arbitrary_path)
{
    const kan_instance_size_t arbitrary_path_length = (kan_instance_size_t) strlen (arbitrary_path);
    kan_file_system_path_container_copy_char_sequence (target_container, arbitrary_path,
                                                       arbitrary_path + arbitrary_path_length);
}

/// \brief Initializes path container by copying data from another path container.
static inline void kan_file_system_path_container_copy (struct kan_file_system_path_container_t *target_container,
                                                        const struct kan_file_system_path_container_t *source_container)
{
    target_container->length = source_container->length;
    memcpy (target_container->path, source_container->path, target_container->length + 1u);
}

/// \brief Appends given sub path to path container, automatically adding separator between base path and sub path.
static inline void kan_file_system_path_container_append_char_sequence (
    struct kan_file_system_path_container_t *target_container, const char *sub_path_begin, const char *sub_path_end)
{
    const kan_instance_size_t sub_path_length = (kan_instance_size_t) (sub_path_end - sub_path_begin);
    const kan_instance_size_t new_length = target_container->length + 1u + sub_path_length;
    const kan_instance_size_t slash_position = target_container->length;
    const kan_instance_size_t sub_path_position = slash_position + 1u;
    kan_file_system_path_container_reset_length (target_container, new_length);

    if (slash_position < target_container->length)
    {
        target_container->path[slash_position] = '/';
    }

    if (sub_path_position < target_container->length)
    {
        const kan_instance_size_t to_copy = target_container->length - sub_path_position;
        memcpy (&target_container->path[sub_path_position], sub_path_begin, to_copy);
    }
}

/// \brief Appends given sub path to path container, automatically adding separator between base path and sub path.
static inline void kan_file_system_path_container_append (struct kan_file_system_path_container_t *target_container,
                                                          const char *sub_path)
{
    const kan_instance_size_t sub_path_length = (kan_instance_size_t) strlen (sub_path);
    kan_file_system_path_container_append_char_sequence (target_container, sub_path, sub_path + sub_path_length);
}

/// \brief Appends given raw string to path container.
static inline void kan_file_system_path_container_add_suffix (struct kan_file_system_path_container_t *target_container,
                                                              const char *suffix)
{
    const kan_instance_size_t suffix_length = (kan_instance_size_t) strlen (suffix);
    const kan_instance_size_t new_length = target_container->length + suffix_length;
    const kan_instance_size_t suffix_position = target_container->length;
    kan_file_system_path_container_reset_length (target_container, new_length);

    if (suffix_position < target_container->length)
    {
        const kan_instance_size_t to_copy = target_container->length - suffix_position;
        memcpy (&target_container->path[suffix_position], suffix, to_copy);
    }
}

KAN_C_HEADER_END
