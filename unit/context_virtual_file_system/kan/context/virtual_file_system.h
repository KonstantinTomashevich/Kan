#pragma once

#include <context_virtual_file_system_api.h>

#include <kan/api_common/c_header.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/interned_string.h>
#include <kan/context/context.h>
#include <kan/virtual_file_system/virtual_file_system.h>

/// \file
/// \brief Provides system that integrates virtual file system volume into context.
///
/// \par Definition
/// \parblock
/// This system integrates virtual file system unit by creating volume and mounting everything from given config.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// Virtual file system provides access to virtual file system volume through read-write lock system. If user needs
/// to only fetch data, `kan_virtual_file_system_get_context_volume_for_read` should be used and followed by
/// `kan_virtual_file_system_close_context_read_access` when read access is no longer needed. The same way, when
/// volume modification is required, `kan_virtual_file_system_get_context_volume_for_write` should be used and followed
/// by `kan_virtual_file_system_close_context_write_access`.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Configuration item for mounting real path.
struct kan_virtual_file_system_config_mount_real_t
{
    kan_interned_string_t mount_path;
    kan_interned_string_t real_path;
};

/// \brief Configuration item for mounting read only pack.
struct kan_virtual_file_system_config_mount_read_only_pack_t
{
    kan_interned_string_t mount_path;
    kan_interned_string_t pack_real_path;
};

/// \brief Configuration type for context virtual file system.
struct kan_virtual_file_system_config_t
{
    /// \meta reflection_dynamic_array_type = "struct kan_virtual_file_system_config_mount_real_t"
    struct kan_dynamic_array_t mount_real;

    /// \meta reflection_dynamic_array_type = "struct kan_virtual_file_system_config_mount_read_only_pack_t"
    struct kan_dynamic_array_t mount_read_only_pack;
};

CONTEXT_VIRTUAL_FILE_SYSTEM_API void kan_virtual_file_system_config_init (
    struct kan_virtual_file_system_config_t *instance);

CONTEXT_VIRTUAL_FILE_SYSTEM_API void kan_virtual_file_system_config_shutdown (
    struct kan_virtual_file_system_config_t *instance);

/// \brief Acquires read access and returns volume. Blocking.
CONTEXT_VIRTUAL_FILE_SYSTEM_API kan_virtual_file_system_volume_t
kan_virtual_file_system_get_context_volume_for_read (kan_context_system_t virtual_file_system);

/// \brief Releases read access to volume.
CONTEXT_VIRTUAL_FILE_SYSTEM_API void kan_virtual_file_system_close_context_read_access (
    kan_context_system_t virtual_file_system);

/// \brief Acquires write access and returns volume. Blocking.
CONTEXT_VIRTUAL_FILE_SYSTEM_API kan_virtual_file_system_volume_t
kan_virtual_file_system_get_context_volume_for_write (kan_context_system_t virtual_file_system);

/// \brief Releases write access to volume.
CONTEXT_VIRTUAL_FILE_SYSTEM_API void kan_virtual_file_system_close_context_write_access (
    kan_context_system_t virtual_file_system);

KAN_C_HEADER_END
