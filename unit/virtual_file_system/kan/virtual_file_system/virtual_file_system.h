#pragma once

#include <virtual_file_system_api.h>

#include <kan/api_common/bool.h>
#include <kan/api_common/c_header.h>
#include <kan/file_system/path_container.h>

// TODO: Docs.
// TODO: Do not forget thread safety notes. They're important and non-trivial here.

KAN_C_HEADER_BEGIN

typedef uint64_t kan_virtual_file_system_volume_t;

struct kan_virtual_file_system_directory_iterator_t
{
    uint64_t implementation_data[5u];
};

enum kan_virtual_file_system_entry_type_t
{
    KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN,
    KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE,
    KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY,
};

struct kan_virtual_file_system_entry_status_t
{
    enum kan_virtual_file_system_entry_type_t type;

    /// \warning Used for KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE.
    uint64_t size;

    /// \warning Used for KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE.
    uint64_t last_modification_time_ns;

    /// \warning Used for KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE.
    kan_bool_t read_only;
};

typedef uint64_t kan_virtual_file_system_watcher_t;

#define KAN_INVALID_VIRTUAL_FILE_SYSTEM_WATCHER 0u

typedef uint64_t kan_virtual_file_system_watcher_iterator_t;

enum kan_virtual_file_system_watcher_event_type_t
{
    KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_ADDED = 0u,
    KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_MODIFIED,
    KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_REMOVED,
};

struct kan_virtual_file_system_watcher_event_t
{
    enum kan_virtual_file_system_watcher_event_type_t event_type;
    enum kan_virtual_file_system_entry_type_t entry_type;
    struct kan_file_system_path_container_t path_container;
};

typedef uint64_t kan_virtual_file_system_read_only_pack_builder_t;

VIRTUAL_FILE_SYSTEM_API kan_virtual_file_system_volume_t kan_virtual_file_system_volume_create (void);

VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_volume_mount_real (kan_virtual_file_system_volume_t volume,
                                                                              const char *mount_path,
                                                                              const char *real_file_system_path);

VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_volume_mount_read_only_pack (
    kan_virtual_file_system_volume_t volume, const char *mount_path, const char *pack_real_path);

VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_volume_unmount (kan_virtual_file_system_volume_t volume,
                                                                           const char *mount_path);

VIRTUAL_FILE_SYSTEM_API void kan_virtual_file_system_volume_destroy (kan_virtual_file_system_volume_t volume);

VIRTUAL_FILE_SYSTEM_API struct kan_virtual_file_system_directory_iterator_t
kan_virtual_file_system_directory_iterator_create (kan_virtual_file_system_volume_t volume, const char *directory_path);

VIRTUAL_FILE_SYSTEM_API const char *kan_virtual_file_system_directory_iterator_advance (
    struct kan_virtual_file_system_directory_iterator_t *iterator);

VIRTUAL_FILE_SYSTEM_API void kan_virtual_file_system_directory_iterator_destroy (
    struct kan_virtual_file_system_directory_iterator_t *iterator);

VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_query_entry (
    kan_virtual_file_system_volume_t volume, const char *path, struct kan_virtual_file_system_entry_status_t *status);

VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_check_existence (kan_virtual_file_system_volume_t volume,
                                                                            const char *path);

VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_remove_file (kan_virtual_file_system_volume_t volume,
                                                                        const char *path);

VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_make_directory (kan_virtual_file_system_volume_t volume,
                                                                           const char *path);

VIRTUAL_FILE_SYSTEM_API kan_bool_t
kan_virtual_file_system_remove_directory_with_content (kan_virtual_file_system_volume_t volume, const char *path);

VIRTUAL_FILE_SYSTEM_API kan_bool_t
kan_virtual_file_system_remove_empty_directory (kan_virtual_file_system_volume_t volume, const char *path);

VIRTUAL_FILE_SYSTEM_API struct kan_stream_t *kan_virtual_file_stream_open_for_read (
    kan_virtual_file_system_volume_t volume, const char *path);

VIRTUAL_FILE_SYSTEM_API struct kan_stream_t *kan_virtual_file_stream_open_for_write (
    kan_virtual_file_system_volume_t volume, const char *path);

VIRTUAL_FILE_SYSTEM_API kan_virtual_file_system_read_only_pack_builder_t
kan_virtual_file_system_read_only_pack_builder_create (void);

VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_read_only_pack_builder_begin (
    kan_virtual_file_system_read_only_pack_builder_t builder, struct kan_stream_t *output_stream);

VIRTUAL_FILE_SYSTEM_API kan_bool_t
kan_virtual_file_system_read_only_pack_builder_add (kan_virtual_file_system_read_only_pack_builder_t builder,
                                                    struct kan_stream_t *input_stream,
                                                    const char *path_in_pack);

VIRTUAL_FILE_SYSTEM_API kan_bool_t
kan_virtual_file_system_read_only_pack_builder_finalize (kan_virtual_file_system_read_only_pack_builder_t builder);

VIRTUAL_FILE_SYSTEM_API void kan_virtual_file_system_read_only_pack_builder_destroy (
    kan_virtual_file_system_read_only_pack_builder_t builder);

VIRTUAL_FILE_SYSTEM_API kan_virtual_file_system_watcher_t
kan_virtual_file_system_watcher_create (kan_virtual_file_system_volume_t volume, const char *directory_path);

VIRTUAL_FILE_SYSTEM_API void kan_virtual_file_system_watcher_destroy (kan_virtual_file_system_watcher_t watcher);

VIRTUAL_FILE_SYSTEM_API kan_virtual_file_system_watcher_iterator_t
kan_virtual_file_system_watcher_iterator_create (kan_virtual_file_system_watcher_t watcher);

VIRTUAL_FILE_SYSTEM_API const struct kan_virtual_file_system_watcher_event_t *
kan_virtual_file_system_watcher_iterator_get (kan_virtual_file_system_watcher_t watcher,
                                              kan_virtual_file_system_watcher_iterator_t iterator);

VIRTUAL_FILE_SYSTEM_API kan_virtual_file_system_watcher_iterator_t kan_virtual_file_system_watcher_iterator_advance (
    kan_virtual_file_system_watcher_t watcher, kan_virtual_file_system_watcher_iterator_t iterator);

VIRTUAL_FILE_SYSTEM_API void kan_virtual_file_system_watcher_iterator_destroy (
    kan_virtual_file_system_watcher_t watcher, kan_virtual_file_system_watcher_iterator_t iterator);

KAN_C_HEADER_END
