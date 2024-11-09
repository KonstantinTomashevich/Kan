#pragma once

#include <virtual_file_system_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/file_system/path_container.h>

/// \file
/// \brief Provides API for working with virtual file system.
///
/// \par Description
/// \parblock
/// Virtual file system is an abstraction layer on top of real file system that extends real file system and provides
/// convenient API for mounting data from real file system.
/// \endparblock
///
/// \par Volumes.
/// \parblock
/// Volume is a root object of virtual file system that stores all the information about virtual file system part.
/// Volumes are independent and provide unique independent contexts.
/// \endparblock
///
/// \par Directories
/// \parblock
/// Directories on volumes can be created using 'kan_virtual_file_system_make_directory' and removed using
/// `kan_virtual_file_system_remove_directory_with_content` and `kan_virtual_file_system_remove_empty_directory`.
///
/// If path for directory creation cannot be reparsed as any real path (through real file system mount points), then
/// virtual directories are created. Keep in mind that it is forbidden to create virtual directories inside mounted
/// read only packs (see below). Also, if some intermediate directories do not exist, they will be created too.
///
/// Using `kan_virtual_file_system_remove_directory_with_content` on virtual directory with mount points will not delete
/// any data on mounted paths, it will just remove the mount points.
///
/// `kan_virtual_file_system_remove_empty_directory` on virtual directory with mount points will not delete directory
/// as it won't be considered empty.
///
/// Calling these functions on real directories (that can be reparsed as any real path) results in the same behavior as
/// appropriate function calls inside real file system API.
///
/// Any virtual directory, including volume root (which is technically a virtual directory too) can have mount points
/// as children. Mount point is a special reparse point that only exists in virtual file system memory and does not
/// affect real file system.
/// \endparblock
///
/// \par Real file system mount points
/// \parblock
/// Real file system mount points is the most trivial type of mount point. It commands virtual file system to reparse
/// part of the path to real file system path, allowing seamless access to real file system files under the hood.
/// \endparblock
///
/// \par Read-only packs
/// \parblock
/// Read only packs are special data structures that are optimized to store lots of files in single directory. Most
/// filesystems handle these cases poorly and `fopen` in directories with lots of files might take several milliseconds
/// to complete. Therefore, it is much more convenient to pack all the files into one file and operate on top of this
/// file treating it as our own read only file system. This approach speeds up resource read operations.
///
/// Read only pack can be mounted as mount point, but data under that mount point cannot be modified. Therefore
/// directory creation and file open for write operations will fail.
///
/// Read only packs can be created using `kan_virtual_file_system_read_only_pack_builder_t`. For example:
///
/// ```c
/// // Create new read only pack builder.
/// kan_virtual_file_system_read_only_pack_builder_t builder = kan_virtual_file_system_read_only_pack_builder_create ();
///
/// // Open output stream for resulting read only pack and begin building process.
/// kan_virtual_file_system_read_only_pack_builder_begin (builder, builder_output_stream);
///
/// // Add items to the pack. Provide input streams for reading their data and paths for them inside read only pack.
/// kan_virtual_file_system_read_only_pack_builder_add (builder, item_input_stream, "dir1/dir2/my_item.txt");
///
/// // Finalize pack building. It writes pack registry to output stream.
/// kan_virtual_file_system_read_only_pack_builder_finalize (builder);
/// // After that, you can begin building another pack.
///
/// // After building all the packs, you can destroy the pack builder.
/// kan_virtual_file_system_read_only_pack_builder_destroy (builder);
/// ```
///
/// Keep in mind that neither input streams nor output stream are owned by builder. They must be closed manually.
/// \endparblock
///
/// \par File system watcher
/// \parblock
/// Virtual file system volumes provide file system watcher interface which is similar to `file_system_watcher` unit,
/// but support both virtual directories and reparsed real paths. Virtual file system watcher always reports absolute
/// virtual paths that can be used with virtual file system volume. Mount and unmount operations are treated as addition
/// and removal.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// Every volume provides its separate context, therefore operating on different volumes from different threads is safe.
/// However, volume modification operations are not thread safe. Volumes follow readers-writer pattern: it is safe to
/// call non-modifying operations from any thread unless there is other thread that can modify volume (for example,
/// create directory or mount something).
///
/// Read only pack builder is not thread safe.
///
/// Virtual file system watcher operations are thread safe. The only requirement is that file system watcher should not
/// be destroyed before all its iterators. Also, volume should not be destroyed when file system watchers are still in
/// use.
/// \endparblock

// TODO: We need real mount point overlay system for modding support, like it is done for Hugo and Jekyll site
//       generators. For example, we would have raw_resources_unit_1 mount point and there is 2 installed mods:
//       mods/mod1 and mods/mod2 with resources/raw_resources_unit_1 subdirectories. Then we should mount everything
//       and prefer files from overlays over files from base directory if they exist.
//       This needs to be supported in a way, that it is usable both for the game, for baking pipeline (to bake game
//       resources with mods) and for game editor (to edit mods). Additional caution should be applied in editor case:
//       base directory should be treated as read only and every edition should be made to mod directory, including
//       creation of new files and file modifications (when saving modified file, save it to mod overlay directory
//       instead). Also, caution should be taken with file system watchers: if file is created in overlay and it exists
//       in base, it should be reported as modification, also if file was deleted in overlay and exists in base, it
//       should be reported as modification too.
//       Also, we need some way to mark files as deleted in overlay to hide base directory files.
//       Also, implementing directory iterators in this case would be non-trivial.

KAN_C_HEADER_BEGIN

KAN_HANDLE_DEFINE (kan_virtual_file_system_volume_t);

/// \brief Holds data for directory iterator implementation.
struct kan_virtual_file_system_directory_iterator_t
{
    void *implementation_data[5u];
};

/// \brief Enumerates virtual file system entry public types.
/// \details End user should not care about private types (mount point, virtual directory, etc), therefore they're not
///          exposed.
enum kan_virtual_file_system_entry_type_t
{
    KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN,
    KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE,
    KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY,
};

/// \brief Contains status report for `kan_virtual_file_system_query_entry`.
struct kan_virtual_file_system_entry_status_t
{
    enum kan_virtual_file_system_entry_type_t type;

    /// \warning Used for KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE.
    kan_file_size_t size;

    /// \warning Used for KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE.
    kan_time_size_t last_modification_time_ns;

    /// \warning Used for KAN_VIRTUAL_FILE_SYSTEM_ENTRY_TYPE_FILE.
    kan_bool_t read_only;
};

KAN_HANDLE_DEFINE (kan_virtual_file_system_watcher_t);
KAN_HANDLE_DEFINE (kan_virtual_file_system_watcher_iterator_t);

/// \brief Enumerates supported event types for virtual file system watcher.
enum kan_virtual_file_system_watcher_event_type_t
{
    KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_ADDED = 0u,
    KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_MODIFIED,
    KAN_VIRTUAL_FILE_SYSTEM_EVENT_TYPE_REMOVED,
};

/// \brief Structure of virtual file system watcher event.
struct kan_virtual_file_system_watcher_event_t
{
    enum kan_virtual_file_system_watcher_event_type_t event_type;
    enum kan_virtual_file_system_entry_type_t entry_type;
    struct kan_file_system_path_container_t path_container;
};

KAN_HANDLE_DEFINE (kan_virtual_file_system_read_only_pack_builder_t);

/// \brief Creates new virtual file system volume.
VIRTUAL_FILE_SYSTEM_API kan_virtual_file_system_volume_t kan_virtual_file_system_volume_create (void);

/// \brief Mounts real file system path into given mount point path.
VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_volume_mount_real (kan_virtual_file_system_volume_t volume,
                                                                              const char *mount_path,
                                                                              const char *real_file_system_path);

/// \brief Mounts read only pack from real file system path into given mount point path.
VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_volume_mount_read_only_pack (
    kan_virtual_file_system_volume_t volume, const char *mount_path, const char *pack_real_path);

/// \brief Unmounts mount point at given path.
VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_volume_unmount (kan_virtual_file_system_volume_t volume,
                                                                           const char *mount_path);

/// \brief Destroys given virtual file system volume.
/// \invariant It is safe to destroy all virtual file system watchers if any.
VIRTUAL_FILE_SYSTEM_API void kan_virtual_file_system_volume_destroy (kan_virtual_file_system_volume_t volume);

/// \brief Creates new instance of virtual file system directory iterator.
VIRTUAL_FILE_SYSTEM_API struct kan_virtual_file_system_directory_iterator_t
kan_virtual_file_system_directory_iterator_create (kan_virtual_file_system_volume_t volume, const char *directory_path);

/// \brief Advances virtual file system directory iterator and returns next entry name or NULL if there is no more
///        entries.
VIRTUAL_FILE_SYSTEM_API const char *kan_virtual_file_system_directory_iterator_advance (
    struct kan_virtual_file_system_directory_iterator_t *iterator);

/// \brief Destroys given virtual file system directory iterator.
VIRTUAL_FILE_SYSTEM_API void kan_virtual_file_system_directory_iterator_destroy (
    struct kan_virtual_file_system_directory_iterator_t *iterator);

/// \brief Queries information about entry at given virtual path.
VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_query_entry (
    kan_virtual_file_system_volume_t volume, const char *path, struct kan_virtual_file_system_entry_status_t *status);

/// \brief Checks if entry at given virtual path exists.
VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_check_existence (kan_virtual_file_system_volume_t volume,
                                                                            const char *path);

/// \brief Attempts to remove file at given virtual path.
VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_remove_file (kan_virtual_file_system_volume_t volume,
                                                                        const char *path);

/// \brief Attempts to make directories at given virtual path.
VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_make_directory (kan_virtual_file_system_volume_t volume,
                                                                           const char *path);

/// \brief Attempts to remove directory and all its children at given virtual path.
VIRTUAL_FILE_SYSTEM_API kan_bool_t
kan_virtual_file_system_remove_directory_with_content (kan_virtual_file_system_volume_t volume, const char *path);

/// \brief Attempts to remove empty directory and all its children at given virtual path.
VIRTUAL_FILE_SYSTEM_API kan_bool_t
kan_virtual_file_system_remove_empty_directory (kan_virtual_file_system_volume_t volume, const char *path);

/// \brief Attempts to open file at given virtual path for reading.
VIRTUAL_FILE_SYSTEM_API struct kan_stream_t *kan_virtual_file_stream_open_for_read (
    kan_virtual_file_system_volume_t volume, const char *path);

/// \brief Attempts to open file at given virtual path for writing.
VIRTUAL_FILE_SYSTEM_API struct kan_stream_t *kan_virtual_file_stream_open_for_write (
    kan_virtual_file_system_volume_t volume, const char *path);

/// \brief Creates new read only pack builder instance.
VIRTUAL_FILE_SYSTEM_API kan_virtual_file_system_read_only_pack_builder_t
kan_virtual_file_system_read_only_pack_builder_create (void);

/// \brief Begins read only pack building routine and uses given stream as output.
VIRTUAL_FILE_SYSTEM_API kan_bool_t kan_virtual_file_system_read_only_pack_builder_begin (
    kan_virtual_file_system_read_only_pack_builder_t builder, struct kan_stream_t *output_stream);

/// \brief Adds new entry to the pack. Entry data is taken from given stream.
VIRTUAL_FILE_SYSTEM_API kan_bool_t
kan_virtual_file_system_read_only_pack_builder_add (kan_virtual_file_system_read_only_pack_builder_t builder,
                                                    struct kan_stream_t *input_stream,
                                                    const char *path_in_pack);

/// \brief Finalizes read only pack building routine by writing read only pack registry.
VIRTUAL_FILE_SYSTEM_API kan_bool_t
kan_virtual_file_system_read_only_pack_builder_finalize (kan_virtual_file_system_read_only_pack_builder_t builder);

/// \brief Destroys given read only pack builder. It should not be inside building routine.
VIRTUAL_FILE_SYSTEM_API void kan_virtual_file_system_read_only_pack_builder_destroy (
    kan_virtual_file_system_read_only_pack_builder_t builder);

/// \brief Creates virtual file system watcher instance for directory at given path.
VIRTUAL_FILE_SYSTEM_API kan_virtual_file_system_watcher_t
kan_virtual_file_system_watcher_create (kan_virtual_file_system_volume_t volume, const char *directory_path);

/// \brief Destroys given virtual file system watcher instance.
VIRTUAL_FILE_SYSTEM_API void kan_virtual_file_system_watcher_destroy (kan_virtual_file_system_watcher_t watcher);

/// \brief Creates iterator for observing virtual file system events from given watcher.
VIRTUAL_FILE_SYSTEM_API kan_virtual_file_system_watcher_iterator_t
kan_virtual_file_system_watcher_iterator_create (kan_virtual_file_system_watcher_t watcher);

/// \brief Extracts current event or NULL if there is no more events using given iterator for given watcher.
VIRTUAL_FILE_SYSTEM_API const struct kan_virtual_file_system_watcher_event_t *
kan_virtual_file_system_watcher_iterator_get (kan_virtual_file_system_watcher_t watcher,
                                              kan_virtual_file_system_watcher_iterator_t iterator);

/// \brief Advances given iterator for given watcher and returns new iterator value.
VIRTUAL_FILE_SYSTEM_API kan_virtual_file_system_watcher_iterator_t kan_virtual_file_system_watcher_iterator_advance (
    kan_virtual_file_system_watcher_t watcher, kan_virtual_file_system_watcher_iterator_t iterator);

/// \brief Destroys given iterator for given watcher.
VIRTUAL_FILE_SYSTEM_API void kan_virtual_file_system_watcher_iterator_destroy (
    kan_virtual_file_system_watcher_t watcher, kan_virtual_file_system_watcher_iterator_t iterator);

KAN_C_HEADER_END
