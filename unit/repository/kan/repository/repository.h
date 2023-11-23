#pragma once

#include <repository_api.h>

#include <kan/api_common/c_header.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/reflection/migration.h>
#include <kan/reflection/registry.h>
#include <kan/repository/meta.h>

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

#define KAN_INVALID_REPOSITORY_EVENT_STORAGE 0u

typedef uint64_t kan_repository_event_storage_t;

/// \meta reflection_ignore_init_shutdown
struct kan_repository_event_insert_query_t
{
    // TODO: Why void*? Because it will make migrators auto-zero queries and it'll be easy to migrate systems.
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

REPOSITORY_API kan_repository_t kan_repository_create_root (kan_allocation_group_t allocation_group,
                                                            kan_reflection_registry_t registry);

REPOSITORY_API kan_repository_t kan_repository_create_child (kan_repository_t parent, kan_interned_string_t name);

REPOSITORY_API void kan_repository_enter_planning_mode (kan_repository_t root_repository);

REPOSITORY_API void kan_repository_migrate (kan_repository_t root_repository,
                                            kan_reflection_registry_t new_registry,
                                            kan_reflection_migration_seed_t migration_seed,
                                            kan_reflection_struct_migrator_t migrator);

REPOSITORY_API kan_repository_singleton_storage_t
kan_repository_singleton_storage_open (kan_repository_t repository, kan_interned_string_t type_name);

REPOSITORY_API void kan_repository_singleton_read_query_init (struct kan_repository_singleton_read_query_t *query,
                                                              kan_repository_singleton_storage_t storage);

REPOSITORY_API kan_repository_singleton_read_access_t
kan_repository_singleton_read_query_execute (struct kan_repository_singleton_read_query_t *query);

REPOSITORY_API const void *kan_repository_singleton_read_access_resolve (kan_repository_singleton_read_access_t access);

REPOSITORY_API void kan_repository_singleton_read_access_close (kan_repository_singleton_read_access_t access);

REPOSITORY_API void kan_repository_singleton_read_query_shutdown (struct kan_repository_singleton_read_query_t *query);

REPOSITORY_API void kan_repository_singleton_write_query_init (struct kan_repository_singleton_write_query_t *query,
                                                               kan_repository_singleton_storage_t storage);

REPOSITORY_API kan_repository_singleton_write_access_t
kan_repository_singleton_write_query_execute (struct kan_repository_singleton_write_query_t *query);

REPOSITORY_API void *kan_repository_singleton_write_access_resolve (kan_repository_singleton_write_access_t access);

REPOSITORY_API void kan_repository_singleton_write_access_close (kan_repository_singleton_write_access_t access);

REPOSITORY_API void kan_repository_singleton_write_query_shutdown (
    struct kan_repository_singleton_write_query_t *query);

REPOSITORY_API kan_repository_indexed_storage_t kan_repository_indexed_storage_open (kan_repository_t repository,
                                                                                     kan_interned_string_t type_name);

REPOSITORY_API void kan_repository_indexed_insert_query_init (struct kan_repository_indexed_insert_query_t *query,
                                                              kan_repository_indexed_storage_t storage);

REPOSITORY_API struct kan_repository_indexed_insertion_package_t kan_repository_indexed_insert_query_execute (
    struct kan_repository_indexed_insert_query_t *query);

REPOSITORY_API void *kan_repository_indexed_insertion_package_get (
    struct kan_repository_indexed_insertion_package_t *package);

REPOSITORY_API void kan_repository_indexed_insertion_package_undo (
    struct kan_repository_indexed_insertion_package_t *package);

REPOSITORY_API void kan_repository_indexed_insertion_package_submit (
    struct kan_repository_indexed_insertion_package_t *package);

REPOSITORY_API void kan_repository_indexed_insert_query_shutdown (struct kan_repository_indexed_insert_query_t *query);

REPOSITORY_API void kan_repository_indexed_sequence_read_query_init (
    struct kan_repository_indexed_sequence_read_query_t *query, kan_repository_indexed_storage_t storage);

REPOSITORY_API struct kan_repository_indexed_sequence_read_cursor_t kan_repository_indexed_sequence_read_query_execute (
    struct kan_repository_indexed_sequence_read_query_t *query);

REPOSITORY_API struct kan_repository_indexed_sequence_read_access_t kan_repository_indexed_sequence_read_cursor_next (
    struct kan_repository_indexed_sequence_read_cursor_t *cursor);

REPOSITORY_API const void *kan_repository_indexed_sequence_read_access_resolve (
    struct kan_repository_indexed_sequence_read_access_t *access);

REPOSITORY_API void kan_repository_indexed_sequence_read_access_close (
    struct kan_repository_indexed_sequence_read_access_t *access);

REPOSITORY_API void kan_repository_indexed_sequence_read_cursor_close (
    struct kan_repository_indexed_sequence_read_cursor_t *cursor);

REPOSITORY_API void kan_repository_indexed_sequence_read_query_shutdown (
    struct kan_repository_indexed_sequence_read_query_t *query);

REPOSITORY_API void kan_repository_indexed_sequence_update_query_init (
    struct kan_repository_indexed_sequence_update_query_t *query, kan_repository_indexed_storage_t storage);

REPOSITORY_API struct kan_repository_indexed_sequence_update_cursor_t
kan_repository_indexed_sequence_update_query_execute (struct kan_repository_indexed_sequence_update_query_t *query);

REPOSITORY_API struct kan_repository_indexed_sequence_update_access_t
kan_repository_indexed_sequence_update_cursor_next (struct kan_repository_indexed_sequence_update_cursor_t *cursor);

REPOSITORY_API void *kan_repository_indexed_sequence_update_access_resolve (
    struct kan_repository_indexed_sequence_update_access_t *access);

REPOSITORY_API void kan_repository_indexed_sequence_update_access_close (
    struct kan_repository_indexed_sequence_update_access_t *access);

REPOSITORY_API void kan_repository_indexed_sequence_update_cursor_close (
    struct kan_repository_indexed_sequence_update_cursor_t *cursor);

REPOSITORY_API void kan_repository_indexed_sequence_update_query_shutdown (
    struct kan_repository_indexed_sequence_update_query_t *query);

REPOSITORY_API void kan_repository_indexed_sequence_delete_query_init (
    struct kan_repository_indexed_sequence_delete_query_t *query, kan_repository_indexed_storage_t storage);

REPOSITORY_API struct kan_repository_indexed_sequence_delete_cursor_t
kan_repository_indexed_sequence_delete_query_execute (struct kan_repository_indexed_sequence_delete_query_t *query);

REPOSITORY_API struct kan_repository_indexed_sequence_delete_access_t
kan_repository_indexed_sequence_delete_cursor_next (struct kan_repository_indexed_sequence_delete_cursor_t *cursor);

REPOSITORY_API const void *kan_repository_indexed_sequence_delete_access_resolve (
    struct kan_repository_indexed_sequence_delete_access_t *access);

REPOSITORY_API void kan_repository_indexed_sequence_delete_access_delete (
    struct kan_repository_indexed_sequence_delete_access_t *access);

REPOSITORY_API void kan_repository_indexed_sequence_delete_access_close (
    struct kan_repository_indexed_sequence_delete_access_t *access);

REPOSITORY_API void kan_repository_indexed_sequence_delete_cursor_close (
    struct kan_repository_indexed_sequence_delete_cursor_t *cursor);

REPOSITORY_API void kan_repository_indexed_sequence_delete_query_shutdown (
    struct kan_repository_indexed_sequence_delete_query_t *query);

REPOSITORY_API void kan_repository_indexed_sequence_write_query_init (
    struct kan_repository_indexed_sequence_write_query_t *query, kan_repository_indexed_storage_t storage);

REPOSITORY_API struct kan_repository_indexed_sequence_write_cursor_t
kan_repository_indexed_sequence_write_query_execute (struct kan_repository_indexed_sequence_write_query_t *query);

REPOSITORY_API struct kan_repository_indexed_sequence_write_access_t kan_repository_indexed_sequence_write_cursor_next (
    struct kan_repository_indexed_sequence_write_cursor_t *cursor);

REPOSITORY_API void *kan_repository_indexed_sequence_write_access_resolve (
    struct kan_repository_indexed_sequence_write_access_t *access);

REPOSITORY_API void kan_repository_indexed_sequence_write_access_delete (
    struct kan_repository_indexed_sequence_write_access_t *access);

REPOSITORY_API void kan_repository_indexed_sequence_write_access_close (
    struct kan_repository_indexed_sequence_write_access_t *access);

REPOSITORY_API void kan_repository_indexed_sequence_write_cursor_close (
    struct kan_repository_indexed_sequence_write_cursor_t *cursor);

REPOSITORY_API void kan_repository_indexed_sequence_write_query_shutdown (
    struct kan_repository_indexed_sequence_write_query_t *query);

REPOSITORY_API kan_repository_event_storage_t kan_repository_event_storage_open (kan_repository_t repository,
                                                                                 kan_interned_string_t type_name);

REPOSITORY_API void kan_repository_event_insert_query_init (struct kan_repository_event_insert_query_t *query,
                                                            kan_repository_event_storage_t storage);

REPOSITORY_API struct kan_repository_event_insertion_package_t kan_repository_event_insert_query_execute (
    struct kan_repository_event_insert_query_t *query);

REPOSITORY_API void *kan_repository_event_insertion_package_get (
    struct kan_repository_event_insertion_package_t *package);

REPOSITORY_API void kan_repository_event_insertion_package_undo (
    struct kan_repository_event_insertion_package_t *package);

REPOSITORY_API void kan_repository_event_insertion_package_submit (
    struct kan_repository_event_insertion_package_t *package);

REPOSITORY_API void kan_repository_event_insert_query_shutdown (struct kan_repository_event_insert_query_t *query);

REPOSITORY_API void kan_repository_event_fetch_query_init (struct kan_repository_event_fetch_query_t *query,
                                                           kan_repository_event_storage_t storage);

REPOSITORY_API struct kan_repository_event_read_access_t kan_repository_event_fetch_query_next (
    struct kan_repository_event_fetch_query_t *query);

REPOSITORY_API const void *kan_repository_event_read_access_resolve (struct kan_repository_event_read_access_t *access);

REPOSITORY_API void kan_repository_event_read_access_close (struct kan_repository_event_read_access_t *access);

REPOSITORY_API void kan_repository_event_fetch_query_shutdown (struct kan_repository_event_fetch_query_t *query);

REPOSITORY_API void kan_repository_enter_serving_mode (kan_repository_t root_repository);

REPOSITORY_API void kan_repository_destroy (kan_repository_t repository);

KAN_C_HEADER_END
