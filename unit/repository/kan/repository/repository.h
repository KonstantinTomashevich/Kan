#pragma once

#include <repository_api.h>

#include <kan/api_common/c_header.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/reflection/migration.h>
#include <kan/reflection/registry.h>
#include <kan/repository/meta.h>

/// \file
/// \brief Contains full API for repository unit -- data management library.
///
/// \par Definition
/// \parblock
/// The goal of the repository is to be the central storage and manager for application logical state. Therefore,
/// repository provides tools to:
///
/// - Store, access and delete any data defined by reflection.
/// - Efficiently search for data through prepared queries that uses indices as backend.
/// - Observe record addition, deleting and modification through automatic events ecosystem.
/// - Delete logically connected data automatically using cascade deleters.
/// \endparblock
///
/// \par Event records
/// \parblock
/// Events are lightweight readonly records that are stored in event queues and used to preserve some data until all
/// event readers process required events. They can only be inserted and then fetched in insertion order. Primary use
/// case for event is reporting about what happened during data processed: record addition, record deletion, important
/// data changes and custom user events if they're needed.
/// \endparblock
///
/// \par Automatic events
/// \parblock
/// Automatic events are inserted by repository itself (in contrast to manual custom events) and are configured through
/// reflection meta. Their main goal is to make data processing easier and more convenient for the developer.
/// \endparblock
///
/// \par Singletons
/// \parblock
/// Singleton records have only one instance per repository and therefore are easy to access and use. They are used to
/// share some common global data, like time or physics world configuration. Technically, singletons always exist,
/// therefore there is no automatic events for singleton addition and deletion, but there are automatic events for
/// singleton data changes.
/// \endparblock
///
/// \par Indexed records
/// \parblock
/// Indexed records describe application state and are the main way to store information. User can create any amount
/// of indexed record instances and access them through prepared queries. These records are designed to be the main
/// storage for application long-term data. Keep in mind that indexed records performance depends on how much indexing
/// through prepared queries is used. All types of automatic events are supported by indexed records.
/// \endparblock
///
/// \par Repository hierarchy
/// \parblock
/// Repositories might be organized in a tree-like hierarchy. Every repository has access to its parent (parent of
/// parent and so on) data, but does not know anything about data of its children. This covers two fundamental cases:
///
/// - Some of the data needs to be preserved while other data needs to be deleted. For example, we're going from game
///   session to main menu: we'd like to preserve assets and some metagame data, but clear everything else. In this
///   case, global shared information is stored inside parent repository and scene-local information is stored inside
///   child repository. When scene changes, child repository is destroyed, but parent is preserved with all the data.
///
/// - In some cases it is important to isolate the data. It is primarily needed for applications like editors, where
///   user can open several documents and every document has its own independent state. In this case, parent repository
///   stores common application data and there are multiple child repositories that store document-local data.
/// \endparblock
///
/// \par Repository modes
/// \parblock
/// Whole repository hierarchy can be in one of two modes:
///
/// - Planning mode. This is when user plans how repository would be used by creating new prepared queries and
///   shutting down old queries that are not longer needed. Also, during planning mode repository data can be migrated
///   to new reflection registry: it will only affect internal data and prepared query instances won't be changed.
///   Keep in mind, that queries cannot be executed in this mode.
///
/// - Serving mode. This is when user code operates repository by inserting, modifying and deleting data from
///   repository by executing queries and working with returned cursors and accesses. When switching to serving mode,
///   all unused data will be automatically destroyed. Keep in mind, that it is forbidden to switch back to planning
///   until all cursors and accesses are closed.
/// \endparblock
///
/// \par Cursor lifetime
/// \parblock
/// If query execution returns cursor, it should be closed after being used even if it pointed to the empty sequence.
/// \endparblock
///
/// \par Access lifetime
/// \parblock
/// If query or cursor returns access, it should only be closed when it was pointing to the actual data. If resolving
/// access results in null pointer, then access shouldn't be closed. Special case is indexed record accesses, that also
/// provide delete operation: this operation also closes the access automatically. Keep in mind that cursors and
/// accesses do not depend on each other: user can safely close cursor but store opened accesses to use them later.
/// \endparblock
///
/// \par Insertion package lifetime
/// \parblock
/// If query returns insertion package and its value is not null (might happen if insertion is forbidden), insertion
/// package should be either submitted (which confirms insertion) or undone (which cancels insertion).
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// Repository aims to be easy to use in multithreaded environments and therefore provides its own thread safety rules.
///
/// During planning mode, query init and shutdown operations are thread safe and can be called from any thread. But all
/// other planning mode operations -- migration, repository creation and destruction -- are not thread safe as they
/// deeply impact repository structure.
///
/// During serving mode, query execution operations are thread safe. However, cursor operations are not thread safe --
/// only one thread should own the cursor and execute operations on cursor. Accesses are also not thread safe and they
/// should only be owned by one thread, but they can be safely moved between threads. Therefore, common approach is to
/// gather all required accesses from cursor on one thread and then send them to other threads (for example as tasks).
///
/// While executing queries, remember about access patterns: opening write or delete accesses on different records of
/// the same type is allowed, but opening write or delete accesses on the same records results in race condition. Also,
/// opening read access on record that is already accessed through write or delete access results in race condition too.
/// \endparblock

KAN_C_HEADER_BEGIN

typedef uint64_t kan_repository_t;

#define KAN_INVALID_REPOSITORY_SINGLETON_STORAGE 0u

typedef uint64_t kan_repository_singleton_storage_t;

/// \meta reflection_ignore_init_shutdown
struct kan_repository_singleton_read_query_t
{
    void *implementation_data;
};

typedef uint64_t kan_repository_singleton_read_access_t;

/// \meta reflection_ignore_init_shutdown
struct kan_repository_singleton_write_query_t
{
    void *implementation_data;
};

typedef uint64_t kan_repository_singleton_write_access_t;

#define KAN_INVALID_REPOSITORY_INDEXED_STORAGE 0u

typedef uint64_t kan_repository_indexed_storage_t;

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_insert_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_insertion_package_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_sequence_read_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_sequence_read_cursor_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_sequence_read_access_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_sequence_update_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_sequence_update_cursor_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_sequence_update_access_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_sequence_delete_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_sequence_delete_cursor_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_sequence_delete_access_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_sequence_write_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_sequence_write_cursor_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_sequence_write_access_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_value_read_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_value_read_cursor_t
{
    void *implementation_data[3u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_value_read_access_t
{
    void *implementation_data[3u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_value_update_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_value_update_cursor_t
{
    void *implementation_data[3u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_value_update_access_t
{
    void *implementation_data[4u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_value_delete_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_value_delete_cursor_t
{
    void *implementation_data[3u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_value_delete_access_t
{
    void *implementation_data[3u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_value_write_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_value_write_cursor_t
{
    void *implementation_data[3u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_value_write_access_t
{
    void *implementation_data[4u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_signal_read_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_signal_read_cursor_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_signal_read_access_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_signal_update_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_signal_update_cursor_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_signal_update_access_t
{
    void *implementation_data[3u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_signal_delete_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_signal_delete_cursor_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_signal_delete_access_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_signal_write_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_signal_write_cursor_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_signal_write_access_t
{
    void *implementation_data[3u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_read_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_ascending_read_cursor_t
{
    void *implementation_data[4u];
    uint64_t implementation_data_64[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_descending_read_cursor_t
{
    void *implementation_data[4u];
    uint64_t implementation_data_64[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_read_access_t
{
    void *implementation_data[3u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_update_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_ascending_update_cursor_t
{
    void *implementation_data[4u];
    uint64_t implementation_data_64[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_descending_update_cursor_t
{
    void *implementation_data[4u];
    uint64_t implementation_data_64[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_update_access_t
{
    void *implementation_data[4u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_delete_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_ascending_delete_cursor_t
{
    void *implementation_data[4u];
    uint64_t implementation_data_64[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_descending_delete_cursor_t
{
    void *implementation_data[4u];
    uint64_t implementation_data_64[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_delete_access_t
{
    void *implementation_data[3u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_write_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_ascending_write_cursor_t
{
    void *implementation_data[4u];
    uint64_t implementation_data_64[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_descending_write_cursor_t
{
    void *implementation_data[4u];
    uint64_t implementation_data_64[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_interval_write_access_t
{
    void *implementation_data[4u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_read_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_shape_read_cursor_t
{
    uint64_t implementation_data_64[22u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_ray_read_cursor_t
{
    uint64_t implementation_data_64[32u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_read_access_t
{
    void *implementation_data[3u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_update_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_shape_update_cursor_t
{
    uint64_t implementation_data_64[22u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_ray_update_cursor_t
{
    uint64_t implementation_data_64[32u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_update_access_t
{
    void *implementation_data[4u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_delete_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_shape_delete_cursor_t
{
    uint64_t implementation_data_64[22u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_ray_delete_cursor_t
{
    uint64_t implementation_data_64[32u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_delete_access_t
{
    void *implementation_data[3u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_write_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_shape_write_cursor_t
{
    uint64_t implementation_data_64[22u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_ray_write_cursor_t
{
    uint64_t implementation_data_64[32u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_indexed_space_write_access_t
{
    void *implementation_data[4u];
};

#define KAN_INVALID_REPOSITORY_EVENT_STORAGE 0u

typedef uint64_t kan_repository_event_storage_t;

/// \meta reflection_ignore_init_shutdown
struct kan_repository_event_insert_query_t
{
    void *implementation_data;
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_event_insertion_package_t
{
    uint64_t implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_event_fetch_query_t
{
    void *implementation_data[2u];
};

/// \meta reflection_ignore_init_shutdown
struct kan_repository_event_read_access_t
{
    uint64_t implementation_data[2u];
};

/// \brief Creates new root repository with given reflection registry.
REPOSITORY_API kan_repository_t kan_repository_create_root (kan_allocation_group_t allocation_group,
                                                            kan_reflection_registry_t registry);

/// \brief Creates named child repository for given repository.
REPOSITORY_API kan_repository_t kan_repository_create_child (kan_repository_t parent, const char *name);

/// \brief Switches repository hierarchy to planning mode.
/// \invariant Should be called on root repository.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_enter_planning_mode (kan_repository_t root_repository);

/// \brief Migrates repository hierarchy data to new registry using given seed and migrator.
/// \invariant Should be called on root repository.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_migrate (kan_repository_t root_repository,
                                            kan_reflection_registry_t new_registry,
                                            kan_reflection_migration_seed_t migration_seed,
                                            kan_reflection_struct_migrator_t migrator);

/// \brief Queries for storage for singleton with given type name in visible part of repository hierarchy.
/// \details If there is no visible storage in hierarchy, new one will be created in given repository.
/// \invariant Should be called in planning mode.
REPOSITORY_API kan_repository_singleton_storage_t kan_repository_singleton_storage_open (kan_repository_t repository,
                                                                                         const char *type_name);

/// \brief Initializes read-only query for accessing singleton from given storage.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_singleton_read_query_init (struct kan_repository_singleton_read_query_t *query,
                                                              kan_repository_singleton_storage_t storage);

/// \brief Executes query to get read access to underlying singleton.
/// \invariant Should be called in serving mode.
REPOSITORY_API kan_repository_singleton_read_access_t
kan_repository_singleton_read_query_execute (struct kan_repository_singleton_read_query_t *query);

/// \brief Resolves read access to get pointer to underlying singleton instance.
/// \invariant Should be called in serving mode.
REPOSITORY_API const void *kan_repository_singleton_read_access_resolve (kan_repository_singleton_read_access_t access);

/// \brief Closes read access and ends its lifetime.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_singleton_read_access_close (kan_repository_singleton_read_access_t access);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_singleton_read_query_shutdown (struct kan_repository_singleton_read_query_t *query);

/// \brief Initializes read-write query for accessing singleton from given storage.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_singleton_write_query_init (struct kan_repository_singleton_write_query_t *query,
                                                               kan_repository_singleton_storage_t storage);

/// \brief Executes query to get read-write access to underlying singleton.
/// \invariant Should be called in serving mode.
REPOSITORY_API kan_repository_singleton_write_access_t
kan_repository_singleton_write_query_execute (struct kan_repository_singleton_write_query_t *query);

/// \brief Resolves read-write access to get pointer to underlying singleton instance.
/// \invariant Should be called in serving mode.
REPOSITORY_API void *kan_repository_singleton_write_access_resolve (kan_repository_singleton_write_access_t access);

/// \brief Closes read-write access and ends its lifetime.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_singleton_write_access_close (kan_repository_singleton_write_access_t access);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_singleton_write_query_shutdown (
    struct kan_repository_singleton_write_query_t *query);

/// \brief Queries for storage for index records with given type name in visible part of repository hierarchy.
/// \details If there is no visible storage in hierarchy, new one will be created in given repository.
/// \invariant Should be called in planning mode.
REPOSITORY_API kan_repository_indexed_storage_t kan_repository_indexed_storage_open (kan_repository_t repository,
                                                                                     const char *type_name);

/// \brief Initializes query for inserting new indexed records to given storage.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_insert_query_init (struct kan_repository_indexed_insert_query_t *query,
                                                              kan_repository_indexed_storage_t storage);

/// \brief Executes query to create new insertion package for inserting new indexed record.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_insertion_package_t kan_repository_indexed_insert_query_execute (
    struct kan_repository_indexed_insert_query_t *query);

/// \brief Gets pointer to record that will be inserted. Initializer from reflection is automatically called if present.
/// \invariant Should be called in serving mode.
REPOSITORY_API void *kan_repository_indexed_insertion_package_get (
    struct kan_repository_indexed_insertion_package_t *package);

/// \brief Cancels insertion operation and frees underlying record.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_insertion_package_undo (
    struct kan_repository_indexed_insertion_package_t *package);

/// \brief Inserts underlying record into indexed records storage.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_insertion_package_submit (
    struct kan_repository_indexed_insertion_package_t *package);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_insert_query_shutdown (struct kan_repository_indexed_insert_query_t *query);

/// \brief Initializes read-only query for accessing indexed records from given storage as unordered sequence.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_sequence_read_query_init (
    struct kan_repository_indexed_sequence_read_query_t *query, kan_repository_indexed_storage_t storage);

/// \brief Returns read-only cursor that iterates all indexed records in unspecified order.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_sequence_read_cursor_t kan_repository_indexed_sequence_read_query_execute (
    struct kan_repository_indexed_sequence_read_query_t *query);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_sequence_read_access_t kan_repository_indexed_sequence_read_cursor_next (
    struct kan_repository_indexed_sequence_read_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API const void *kan_repository_indexed_sequence_read_access_resolve (
    struct kan_repository_indexed_sequence_read_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_sequence_read_access_close (
    struct kan_repository_indexed_sequence_read_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_sequence_read_cursor_close (
    struct kan_repository_indexed_sequence_read_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_sequence_read_query_shutdown (
    struct kan_repository_indexed_sequence_read_query_t *query);

/// \brief Initializes read-write query for accessing indexed records from given storage as unordered sequence.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_sequence_update_query_init (
    struct kan_repository_indexed_sequence_update_query_t *query, kan_repository_indexed_storage_t storage);

/// \brief Returns read-write cursor that iterates all indexed records in unspecified order.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_sequence_update_cursor_t
kan_repository_indexed_sequence_update_query_execute (struct kan_repository_indexed_sequence_update_query_t *query);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_sequence_update_access_t
kan_repository_indexed_sequence_update_cursor_next (struct kan_repository_indexed_sequence_update_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API void *kan_repository_indexed_sequence_update_access_resolve (
    struct kan_repository_indexed_sequence_update_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_sequence_update_access_close (
    struct kan_repository_indexed_sequence_update_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_sequence_update_cursor_close (
    struct kan_repository_indexed_sequence_update_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_sequence_update_query_shutdown (
    struct kan_repository_indexed_sequence_update_query_t *query);

/// \brief Initializes read-delete query for accessing indexed records from given storage as unordered sequence.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_sequence_delete_query_init (
    struct kan_repository_indexed_sequence_delete_query_t *query, kan_repository_indexed_storage_t storage);

/// \brief Returns read-delete cursor that iterates all indexed records in unspecified order.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_sequence_delete_cursor_t
kan_repository_indexed_sequence_delete_query_execute (struct kan_repository_indexed_sequence_delete_query_t *query);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_sequence_delete_access_t
kan_repository_indexed_sequence_delete_cursor_next (struct kan_repository_indexed_sequence_delete_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API const void *kan_repository_indexed_sequence_delete_access_resolve (
    struct kan_repository_indexed_sequence_delete_access_t *access);

/// \brief Deletes record to which access points. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_sequence_delete_access_delete (
    struct kan_repository_indexed_sequence_delete_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_sequence_delete_access_close (
    struct kan_repository_indexed_sequence_delete_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_sequence_delete_cursor_close (
    struct kan_repository_indexed_sequence_delete_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_sequence_delete_query_shutdown (
    struct kan_repository_indexed_sequence_delete_query_t *query);

/// \brief Initializes read-write-delete query for accessing indexed records from given storage as unordered sequence.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_sequence_write_query_init (
    struct kan_repository_indexed_sequence_write_query_t *query, kan_repository_indexed_storage_t storage);

/// \brief Returns read-write-delete cursor that iterates all indexed records in unspecified order.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_sequence_write_cursor_t
kan_repository_indexed_sequence_write_query_execute (struct kan_repository_indexed_sequence_write_query_t *query);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_sequence_write_access_t kan_repository_indexed_sequence_write_cursor_next (
    struct kan_repository_indexed_sequence_write_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API void *kan_repository_indexed_sequence_write_access_resolve (
    struct kan_repository_indexed_sequence_write_access_t *access);

/// \brief Deletes record to which access points. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_sequence_write_access_delete (
    struct kan_repository_indexed_sequence_write_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_sequence_write_access_close (
    struct kan_repository_indexed_sequence_write_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_sequence_write_cursor_close (
    struct kan_repository_indexed_sequence_write_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_sequence_write_query_shutdown (
    struct kan_repository_indexed_sequence_write_query_t *query);

/// \brief Initializes read-only query for accessing indexed records from given storage through value equality query.
/// \details Indexes field at given reflection path. Field type must be integer, floating, enum or interned string.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_value_read_query_init (
    struct kan_repository_indexed_value_read_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t path);

/// \brief Returns read-only cursor that iterates over records with value equal to value at given pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_value_read_cursor_t kan_repository_indexed_value_read_query_execute (
    struct kan_repository_indexed_value_read_query_t *query, const void *value);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_value_read_access_t kan_repository_indexed_value_read_cursor_next (
    struct kan_repository_indexed_value_read_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API const void *kan_repository_indexed_value_read_access_resolve (
    struct kan_repository_indexed_value_read_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_value_read_access_close (
    struct kan_repository_indexed_value_read_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_value_read_cursor_close (
    struct kan_repository_indexed_value_read_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_value_read_query_shutdown (
    struct kan_repository_indexed_value_read_query_t *query);

/// \brief Initializes read-write query for accessing indexed records from given storage through value equality query.
/// \details Indexes field at given reflection path. Field type must be integer, floating, enum or interned string.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_value_update_query_init (
    struct kan_repository_indexed_value_update_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t path);

/// \brief Returns read-write cursor that iterates over records with value equal to value at given pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_value_update_cursor_t kan_repository_indexed_value_update_query_execute (
    struct kan_repository_indexed_value_update_query_t *query, const void *value);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_value_update_access_t kan_repository_indexed_value_update_cursor_next (
    struct kan_repository_indexed_value_update_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API void *kan_repository_indexed_value_update_access_resolve (
    struct kan_repository_indexed_value_update_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_value_update_access_close (
    struct kan_repository_indexed_value_update_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_value_update_cursor_close (
    struct kan_repository_indexed_value_update_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_value_update_query_shutdown (
    struct kan_repository_indexed_value_update_query_t *query);

/// \brief Initializes read-delete query for accessing indexed records from given storage through value equality query.
/// \details Indexes field at given reflection path. Field type must be integer, floating, enum or interned string.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_value_delete_query_init (
    struct kan_repository_indexed_value_delete_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t path);

/// \brief Returns read-delete cursor that iterates over records with value equal to value at given pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_value_delete_cursor_t kan_repository_indexed_value_delete_query_execute (
    struct kan_repository_indexed_value_delete_query_t *query, const void *value);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_value_delete_access_t kan_repository_indexed_value_delete_cursor_next (
    struct kan_repository_indexed_value_delete_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API const void *kan_repository_indexed_value_delete_access_resolve (
    struct kan_repository_indexed_value_delete_access_t *access);

/// \brief Deletes record to which access points. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_value_delete_access_delete (
    struct kan_repository_indexed_value_delete_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_value_delete_access_close (
    struct kan_repository_indexed_value_delete_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_value_delete_cursor_close (
    struct kan_repository_indexed_value_delete_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_value_delete_query_shutdown (
    struct kan_repository_indexed_value_delete_query_t *query);

/// \brief Initializes read-write-delete query for accessing indexed records from given storage through value equality
///        query.
/// \details Indexes field at given reflection path. Field type must be integer, floating, enum or interned string.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_value_write_query_init (
    struct kan_repository_indexed_value_write_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t path);

/// \brief Returns read-write-delete cursor that iterates over records with value equal to value at given pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_value_write_cursor_t kan_repository_indexed_value_write_query_execute (
    struct kan_repository_indexed_value_write_query_t *query, const void *value);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_value_write_access_t kan_repository_indexed_value_write_cursor_next (
    struct kan_repository_indexed_value_write_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API void *kan_repository_indexed_value_write_access_resolve (
    struct kan_repository_indexed_value_write_access_t *access);

/// \brief Deletes record to which access points. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_value_write_access_delete (
    struct kan_repository_indexed_value_write_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_value_write_access_close (
    struct kan_repository_indexed_value_write_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_value_write_cursor_close (
    struct kan_repository_indexed_value_write_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_value_write_query_shutdown (
    struct kan_repository_indexed_value_write_query_t *query);

/// \brief Initializes read-only query for accessing indexed records from given storage that have specified value.
/// \details Indexes field at given reflection path. Field type must be integer, floating, enum or interned string.
///          Signal value is a result of reintepret cast from value of actual type.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_signal_read_query_init (
    struct kan_repository_indexed_signal_read_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t path,
    uint64_t signal_value);

/// \brief Returns read-only cursor that iterates over records with predefined value.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_signal_read_cursor_t kan_repository_indexed_signal_read_query_execute (
    struct kan_repository_indexed_signal_read_query_t *query);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_signal_read_access_t kan_repository_indexed_signal_read_cursor_next (
    struct kan_repository_indexed_signal_read_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API const void *kan_repository_indexed_signal_read_access_resolve (
    struct kan_repository_indexed_signal_read_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_signal_read_access_close (
    struct kan_repository_indexed_signal_read_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_signal_read_cursor_close (
    struct kan_repository_indexed_signal_read_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_signal_read_query_shutdown (
    struct kan_repository_indexed_signal_read_query_t *query);

/// \brief Initializes read-write query for accessing indexed records from given storage that have specified value.
/// \details Indexes field at given reflection path. Field type must be integer, floating, enum or interned string.
///          Signal value is a result of reintepret cast from value of actual type.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_signal_update_query_init (
    struct kan_repository_indexed_signal_update_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t path,
    uint64_t signal_value);

/// \brief Returns read-owrite cursor that iterates over records with predefined value.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_signal_update_cursor_t kan_repository_indexed_signal_update_query_execute (
    struct kan_repository_indexed_signal_update_query_t *query);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_signal_update_access_t kan_repository_indexed_signal_update_cursor_next (
    struct kan_repository_indexed_signal_update_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API void *kan_repository_indexed_signal_update_access_resolve (
    struct kan_repository_indexed_signal_update_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_signal_update_access_close (
    struct kan_repository_indexed_signal_update_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_signal_update_cursor_close (
    struct kan_repository_indexed_signal_update_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_signal_update_query_shutdown (
    struct kan_repository_indexed_signal_update_query_t *query);

/// \brief Initializes read-delete query for accessing indexed records from given storage that have specified value.
/// \details Indexes field at given reflection path. Field type must be integer, floating, enum or interned string.
///          Signal value is a result of reintepret cast from value of actual type.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_signal_delete_query_init (
    struct kan_repository_indexed_signal_delete_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t path,
    uint64_t signal_value);

/// \brief Returns read-delete cursor that iterates over records with predefined value.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_signal_delete_cursor_t kan_repository_indexed_signal_delete_query_execute (
    struct kan_repository_indexed_signal_delete_query_t *query);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_signal_delete_access_t kan_repository_indexed_signal_delete_cursor_next (
    struct kan_repository_indexed_signal_delete_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API const void *kan_repository_indexed_signal_delete_access_resolve (
    struct kan_repository_indexed_signal_delete_access_t *access);

/// \brief Deletes record to which access points. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_signal_delete_access_delete (
    struct kan_repository_indexed_signal_delete_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_signal_delete_access_close (
    struct kan_repository_indexed_signal_delete_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_signal_delete_cursor_close (
    struct kan_repository_indexed_signal_delete_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_signal_delete_query_shutdown (
    struct kan_repository_indexed_signal_delete_query_t *query);

/// \brief Initializes read-write-delete query for accessing indexed records from given storage that have specified
///        value.
/// \details Indexes field at given reflection path. Field type must be integer, floating, enum or interned string.
///          Signal value is a result of reintepret cast from value of actual type.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_signal_write_query_init (
    struct kan_repository_indexed_signal_write_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t path,
    uint64_t signal_value);

/// \brief Returns read-write-delete cursor that iterates over records with predefined value.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_signal_write_cursor_t kan_repository_indexed_signal_write_query_execute (
    struct kan_repository_indexed_signal_write_query_t *query);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_signal_write_access_t kan_repository_indexed_signal_write_cursor_next (
    struct kan_repository_indexed_signal_write_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API void *kan_repository_indexed_signal_write_access_resolve (
    struct kan_repository_indexed_signal_write_access_t *access);

/// \brief Deletes record to which access points. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_signal_write_access_delete (
    struct kan_repository_indexed_signal_write_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_signal_write_access_close (
    struct kan_repository_indexed_signal_write_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_signal_write_cursor_close (
    struct kan_repository_indexed_signal_write_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_signal_write_query_shutdown (
    struct kan_repository_indexed_signal_write_query_t *query);

/// \brief Initializes read-only query for accessing indexed records from given storage through interval search query.
/// \details Indexes field at given reflection path. Field type must be integer or floating.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_interval_read_query_init (
    struct kan_repository_indexed_interval_read_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t path);

/// \brief Returns read-only cursor that iterates over records inside given interval in ascending order.
/// \details Null pointer as parameter is treated as infinity (minus for min and plus for max).
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_ascending_read_cursor_t
kan_repository_indexed_interval_read_query_execute_ascending (
    struct kan_repository_indexed_interval_read_query_t *query, const void *min, const void *max);

/// \brief Returns read-only cursor that iterates over records inside given interval in descending order.
/// \details Null pointer as parameter is treated as infinity (minus for min and plus for max).
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_descending_read_cursor_t
kan_repository_indexed_interval_read_query_execute_descending (
    struct kan_repository_indexed_interval_read_query_t *query, const void *min, const void *max);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_read_access_t
kan_repository_indexed_interval_ascending_read_cursor_next (
    struct kan_repository_indexed_interval_ascending_read_cursor_t *cursor);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_read_access_t
kan_repository_indexed_interval_descending_read_cursor_next (
    struct kan_repository_indexed_interval_descending_read_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API const void *kan_repository_indexed_interval_read_access_resolve (
    struct kan_repository_indexed_interval_read_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_interval_read_access_close (
    struct kan_repository_indexed_interval_read_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_interval_ascending_read_cursor_close (
    struct kan_repository_indexed_interval_ascending_read_cursor_t *cursor);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_interval_descending_read_cursor_close (
    struct kan_repository_indexed_interval_descending_read_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_interval_read_query_shutdown (
    struct kan_repository_indexed_interval_read_query_t *query);

/// \brief Initializes read-write query for accessing indexed records from given storage through interval search query.
/// \details Indexes field at given reflection path. Field type must be integer or floating.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_interval_update_query_init (
    struct kan_repository_indexed_interval_update_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t path);

/// \brief Returns read-write cursor that iterates over records inside given interval in ascending order.
/// \details Null pointer as parameter is treated as infinity (minus for min and plus for max).
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_ascending_update_cursor_t
kan_repository_indexed_interval_update_query_execute_ascending (
    struct kan_repository_indexed_interval_update_query_t *query, const void *min, const void *max);

/// \brief Returns read-write cursor that iterates over records inside given interval in descending order.
/// \details Null pointer as parameter is treated as infinity (minus for min and plus for max).
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_descending_update_cursor_t
kan_repository_indexed_interval_update_query_execute_descending (
    struct kan_repository_indexed_interval_update_query_t *query, const void *min, const void *max);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_update_access_t
kan_repository_indexed_interval_ascending_update_cursor_next (
    struct kan_repository_indexed_interval_ascending_update_cursor_t *cursor);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_update_access_t
kan_repository_indexed_interval_descending_update_cursor_next (
    struct kan_repository_indexed_interval_descending_update_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API void *kan_repository_indexed_interval_update_access_resolve (
    struct kan_repository_indexed_interval_update_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_interval_update_access_close (
    struct kan_repository_indexed_interval_update_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_interval_ascending_update_cursor_close (
    struct kan_repository_indexed_interval_ascending_update_cursor_t *cursor);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_interval_descending_update_cursor_close (
    struct kan_repository_indexed_interval_descending_update_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_interval_update_query_shutdown (
    struct kan_repository_indexed_interval_update_query_t *query);

/// \brief Initializes read-delete query for accessing indexed records from given storage through interval search query.
/// \details Indexes field at given reflection path. Field type must be integer or floating.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_interval_delete_query_init (
    struct kan_repository_indexed_interval_delete_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t path);

/// \brief Returns read-delete cursor that iterates over records inside given interval in ascending order.
/// \details Null pointer as parameter is treated as infinity (minus for min and plus for max).
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_ascending_delete_cursor_t
kan_repository_indexed_interval_delete_query_execute_ascending (
    struct kan_repository_indexed_interval_delete_query_t *query, const void *min, const void *max);

/// \brief Returns read-delete cursor that iterates over records inside given interval in descending order.
/// \details Null pointer as parameter is treated as infinity (minus for min and plus for max).
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_descending_delete_cursor_t
kan_repository_indexed_interval_delete_query_execute_descending (
    struct kan_repository_indexed_interval_delete_query_t *query, const void *min, const void *max);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_delete_access_t
kan_repository_indexed_interval_ascending_delete_cursor_next (
    struct kan_repository_indexed_interval_ascending_delete_cursor_t *cursor);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_delete_access_t
kan_repository_indexed_interval_descending_delete_cursor_next (
    struct kan_repository_indexed_interval_descending_delete_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API const void *kan_repository_indexed_interval_delete_access_resolve (
    struct kan_repository_indexed_interval_delete_access_t *access);

/// \brief Deletes record to which access points. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_interval_delete_access_delete (
    struct kan_repository_indexed_interval_delete_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_interval_delete_access_close (
    struct kan_repository_indexed_interval_delete_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_interval_ascending_delete_cursor_close (
    struct kan_repository_indexed_interval_ascending_delete_cursor_t *cursor);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_interval_descending_delete_cursor_close (
    struct kan_repository_indexed_interval_descending_delete_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_interval_delete_query_shutdown (
    struct kan_repository_indexed_interval_delete_query_t *query);

/// \brief Initializes read-write-delete query for accessing indexed records from given storage through interval search
///        query.
/// \details Indexes field at given reflection path. Field type must be integer or floating.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_interval_write_query_init (
    struct kan_repository_indexed_interval_write_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t path);

/// \brief Returns read-write-delete cursor that iterates over records inside given interval in ascending order.
/// \details Null pointer as parameter is treated as infinity (minus for min and plus for max).
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_ascending_write_cursor_t
kan_repository_indexed_interval_write_query_execute_ascending (
    struct kan_repository_indexed_interval_write_query_t *query, const void *min, const void *max);

/// \brief Returns read-write-delete cursor that iterates over records inside given interval in descending order.
/// \details Null pointer as parameter is treated as infinity (minus for min and plus for max).
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_descending_write_cursor_t
kan_repository_indexed_interval_write_query_execute_descending (
    struct kan_repository_indexed_interval_write_query_t *query, const void *min, const void *max);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_write_access_t
kan_repository_indexed_interval_ascending_write_cursor_next (
    struct kan_repository_indexed_interval_ascending_write_cursor_t *cursor);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_interval_write_access_t
kan_repository_indexed_interval_descending_write_cursor_next (
    struct kan_repository_indexed_interval_descending_write_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API void *kan_repository_indexed_interval_write_access_resolve (
    struct kan_repository_indexed_interval_write_access_t *access);

/// \brief Deletes record to which access points. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_interval_write_access_delete (
    struct kan_repository_indexed_interval_write_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_interval_write_access_close (
    struct kan_repository_indexed_interval_write_access_t *access);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_interval_ascending_write_cursor_close (
    struct kan_repository_indexed_interval_ascending_write_cursor_t *cursor);

/// \brief Closes underlying cursor. Returned accesses are not affected by this operation.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_interval_descending_write_cursor_close (
    struct kan_repository_indexed_interval_descending_write_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_interval_write_query_shutdown (
    struct kan_repository_indexed_interval_write_query_t *query);

/// \brief Initializes read-only query for accessing indexed records from given storage through axis aligned bounding
///        shape and ray queries.
/// \details Indexes min and max fields at given reflection path. Field type must be inline arrays of integer or
///          floating values. Min and max arrays must have the same length, that is equal to space dimension count.
///          All dimensions share given global min and max, also leaf size is used to adjust space tree height.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_space_read_query_init (
    struct kan_repository_indexed_space_read_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t min_path,
    struct kan_repository_field_path_t max_path,
    double global_min,
    double global_max,
    double leaf_size);

/// \brief Returns read-only cursor that iterates over records which shapes intersect with given axis aligned bounding
///        shape.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_shape_read_cursor_t
kan_repository_indexed_space_read_query_execute_shape (struct kan_repository_indexed_space_read_query_t *query,
                                                       const double *min,
                                                       const double *max);

/// \brief Returns read-only cursor that iterates over records which shapes intersect with given ray.
/// \details Direction is not required to be normalized, that's why we use time instead of distance here.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_ray_read_cursor_t
kan_repository_indexed_space_read_query_execute_ray (struct kan_repository_indexed_space_read_query_t *query,
                                                     const double *origin,
                                                     const double *direction,
                                                     double max_time);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_read_access_t kan_repository_indexed_space_shape_read_cursor_next (
    struct kan_repository_indexed_space_shape_read_cursor_t *cursor);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_read_access_t kan_repository_indexed_space_ray_read_cursor_next (
    struct kan_repository_indexed_space_ray_read_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API const void *kan_repository_indexed_space_read_access_resolve (
    struct kan_repository_indexed_space_read_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_space_read_access_close (
    struct kan_repository_indexed_space_read_access_t *access);

/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_space_shape_read_cursor_close (
    struct kan_repository_indexed_space_shape_read_cursor_t *cursor);

/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_space_ray_read_cursor_close (
    struct kan_repository_indexed_space_ray_read_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_space_read_query_shutdown (
    struct kan_repository_indexed_space_read_query_t *query);

/// \brief Initializes read-write query for accessing indexed records from given storage through axis aligned bounding
///        shape and ray queries.
/// \details Indexes min and max fields at given reflection path. Field type must be inline arrays of integer or
///          floating values. Min and max arrays must have the same length, that is equal to space dimension count.
///          All dimensions share given global min and max, also leaf size is used to adjust space tree height.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_space_update_query_init (
    struct kan_repository_indexed_space_update_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t min_path,
    struct kan_repository_field_path_t max_path,
    double global_min,
    double global_max,
    double leaf_size);

/// \brief Returns read-write cursor that iterates over records which shapes intersect with given axis aligned bounding
///        shape.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_shape_update_cursor_t
kan_repository_indexed_space_update_query_execute_shape (struct kan_repository_indexed_space_update_query_t *query,
                                                         const double *min,
                                                         const double *max);

/// \brief Returns read-write cursor that iterates over records which shapes intersect with given ray.
/// \details Direction is not required to be normalized, that's why we use time instead of distance here.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_ray_update_cursor_t
kan_repository_indexed_space_update_query_execute_ray (struct kan_repository_indexed_space_update_query_t *query,
                                                       const double *origin,
                                                       const double *direction,
                                                       double max_time);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_update_access_t
kan_repository_indexed_space_shape_update_cursor_next (
    struct kan_repository_indexed_space_shape_update_cursor_t *cursor);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_update_access_t kan_repository_indexed_space_ray_update_cursor_next (
    struct kan_repository_indexed_space_ray_update_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API void *kan_repository_indexed_space_update_access_resolve (
    struct kan_repository_indexed_space_update_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_space_update_access_close (
    struct kan_repository_indexed_space_update_access_t *access);

/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_space_shape_update_cursor_close (
    struct kan_repository_indexed_space_shape_update_cursor_t *cursor);

/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_space_ray_update_cursor_close (
    struct kan_repository_indexed_space_ray_update_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_space_update_query_shutdown (
    struct kan_repository_indexed_space_update_query_t *query);

/// \brief Initializes read-delete query for accessing indexed records from given storage through axis aligned bounding
///        shape and ray queries.
/// \details Indexes min and max fields at given reflection path. Field type must be inline arrays of integer or
///          floating values. Min and max arrays must have the same length, that is equal to space dimension count.
///          All dimensions share given global min and max, also leaf size is used to adjust space tree height.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_space_delete_query_init (
    struct kan_repository_indexed_space_delete_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t min_path,
    struct kan_repository_field_path_t max_path,
    double global_min,
    double global_max,
    double leaf_size);

/// \brief Returns read-delete cursor that iterates over records which shapes intersect with given axis aligned bounding
///        shape.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_shape_delete_cursor_t
kan_repository_indexed_space_delete_query_execute_shape (struct kan_repository_indexed_space_delete_query_t *query,
                                                         const double *min,
                                                         const double *max);

/// \brief Returns read-delete cursor that iterates over records which shapes intersect with given ray.
/// \details Direction is not required to be normalized, that's why we use time instead of distance here.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_ray_delete_cursor_t
kan_repository_indexed_space_delete_query_execute_ray (struct kan_repository_indexed_space_delete_query_t *query,
                                                       const double *origin,
                                                       const double *direction,
                                                       double max_time);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_delete_access_t
kan_repository_indexed_space_shape_delete_cursor_next (
    struct kan_repository_indexed_space_shape_delete_cursor_t *cursor);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_delete_access_t kan_repository_indexed_space_ray_delete_cursor_next (
    struct kan_repository_indexed_space_ray_delete_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API const void *kan_repository_indexed_space_delete_access_resolve (
    struct kan_repository_indexed_space_delete_access_t *access);

/// \brief Deletes record to which access points. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_space_delete_access_delete (
    struct kan_repository_indexed_space_delete_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_space_delete_access_close (
    struct kan_repository_indexed_space_delete_access_t *access);

/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_space_shape_delete_cursor_close (
    struct kan_repository_indexed_space_shape_delete_cursor_t *cursor);

/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_space_ray_delete_cursor_close (
    struct kan_repository_indexed_space_ray_delete_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_space_delete_query_shutdown (
    struct kan_repository_indexed_space_delete_query_t *query);

/// \brief Initializes read-write-delete query for accessing indexed records from given storage through axis aligned
///        bounding shape and ray queries.
/// \details Indexes min and max fields at given reflection path. Field type must be inline arrays of integer or
///          floating values. Min and max arrays must have the same length, that is equal to space dimension count.
///          All dimensions share given global min and max, also leaf size is used to adjust space tree height.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_space_write_query_init (
    struct kan_repository_indexed_space_write_query_t *query,
    kan_repository_indexed_storage_t storage,
    struct kan_repository_field_path_t min_path,
    struct kan_repository_field_path_t max_path,
    double global_min,
    double global_max,
    double leaf_size);

/// \brief Returns read-write-delete cursor that iterates over records which shapes intersect with given axis aligned
///        bounding shape.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_shape_write_cursor_t
kan_repository_indexed_space_write_query_execute_shape (struct kan_repository_indexed_space_write_query_t *query,
                                                        const double *min,
                                                        const double *max);

/// \brief Returns read-write-delete cursor that iterates over records which shapes intersect with given ray.
/// \details Direction is not required to be normalized, that's why we use time instead of distance here.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_ray_write_cursor_t
kan_repository_indexed_space_write_query_execute_ray (struct kan_repository_indexed_space_write_query_t *query,
                                                      const double *origin,
                                                      const double *direction,
                                                      double max_time);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_write_access_t kan_repository_indexed_space_shape_write_cursor_next (
    struct kan_repository_indexed_space_shape_write_cursor_t *cursor);

/// \brief Returns access to the current record and moves cursor to the next one.
/// \details If there are no more records in query result, returns access to null pointer.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_indexed_space_write_access_t kan_repository_indexed_space_ray_write_cursor_next (
    struct kan_repository_indexed_space_ray_write_cursor_t *cursor);

/// \brief Resolves given access to indexed record pointer or null if it doesn't point anywhere.
/// \invariant Should be called in serving mode.
REPOSITORY_API void *kan_repository_indexed_space_write_access_resolve (
    struct kan_repository_indexed_space_write_access_t *access);

/// \brief Deletes record to which access points. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_space_write_access_delete (
    struct kan_repository_indexed_space_write_access_t *access);

/// \brief Closes underlying read access. Should not be called for null accesses.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_space_write_access_close (
    struct kan_repository_indexed_space_write_access_t *access);

/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_space_shape_write_cursor_close (
    struct kan_repository_indexed_space_shape_write_cursor_t *cursor);

/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_indexed_space_ray_write_cursor_close (
    struct kan_repository_indexed_space_ray_write_cursor_t *cursor);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_indexed_space_write_query_shutdown (
    struct kan_repository_indexed_space_write_query_t *query);

/// \brief Queries for storage for events with given type name in visible part of repository hierarchy.
/// \details If there is no visible storage in hierarchy, new one will be created in given repository.
/// \invariant Should be called in planning mode.
REPOSITORY_API kan_repository_event_storage_t kan_repository_event_storage_open (kan_repository_t repository,
                                                                                 const char *type_name);

/// \brief Initializes query for inserting events into given storage.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_event_insert_query_init (struct kan_repository_event_insert_query_t *query,
                                                            kan_repository_event_storage_t storage);

/// \brief Executes query to create new insertion package for inserting new event.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_event_insertion_package_t kan_repository_event_insert_query_execute (
    struct kan_repository_event_insert_query_t *query);

/// \brief Gets pointer to event that will be inserted. Initializer from reflection is automatically called if present.
/// \details If there is no readers for events of this type, event will not be allocated and null will be returned.
/// \invariant Should be called in serving mode.
REPOSITORY_API void *kan_repository_event_insertion_package_get (
    struct kan_repository_event_insertion_package_t *package);

/// \brief Cancels insertion operation and frees underlying event.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_event_insertion_package_undo (
    struct kan_repository_event_insertion_package_t *package);

/// \brief Inserts underlying event into events storage.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_event_insertion_package_submit (
    struct kan_repository_event_insertion_package_t *package);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_event_insert_query_shutdown (struct kan_repository_event_insert_query_t *query);

/// \brief Initializes query for fetching events from given storage. Every query has its own next event pointer.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_event_fetch_query_init (struct kan_repository_event_fetch_query_t *query,
                                                           kan_repository_event_storage_t storage);

/// \brief Fetches read access to next unread event and moves query pointer further.
/// \details Returns access to null event if there is no unread events.
/// \invariant Should be called in serving mode.
REPOSITORY_API struct kan_repository_event_read_access_t kan_repository_event_fetch_query_next (
    struct kan_repository_event_fetch_query_t *query);

/// \brief Resolves read access and returns pointer to the underlying event if any.
/// \invariant Should be called in serving mode.
REPOSITORY_API const void *kan_repository_event_read_access_resolve (struct kan_repository_event_read_access_t *access);

/// \brief Closes read access and free underlying event if cannot be read by anyone else.
/// \invariant Should be called in serving mode.
REPOSITORY_API void kan_repository_event_read_access_close (struct kan_repository_event_read_access_t *access);

/// \brief Marks query as unused so pointed resources might be freed if possible later.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_event_fetch_query_shutdown (struct kan_repository_event_fetch_query_t *query);

/// \invariant Should be called on root repository.
/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_enter_serving_mode (kan_repository_t root_repository);

/// \invariant Should be called in planning mode.
REPOSITORY_API void kan_repository_destroy (kan_repository_t repository);

KAN_C_HEADER_END
