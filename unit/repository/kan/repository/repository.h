#pragma once

#include <repository_api.h>

#include <kan/api_common/c_header.h>
#include <kan/memory_profiler/allocation_group.h>
#include <kan/reflection/registry.h>
#include <kan/repository/meta.h>

KAN_C_HEADER_BEGIN

typedef uint64_t kan_repository_t;

#define KAN_INVALID_REPOSITORY 0u

typedef uint64_t kan_repository_singleton_storage_t;

typedef uint64_t kan_repository_singleton_read_query_t;

typedef uint64_t kan_repository_singleton_read_access_t;

typedef uint64_t kan_repository_singleton_write_query_t;

typedef uint64_t kan_repository_singleton_write_access_t;

typedef uint64_t kan_repository_indexed_storage_t;

typedef uint64_t kan_repository_indexed_insert_query_t;

typedef uint64_t kan_repository_indexed_insertion_package_t;

typedef uint64_t kan_repository_indexed_sequence_read_query_t;

typedef uint64_t kan_repository_indexed_sequence_read_cursor_t;

typedef uint64_t kan_repository_indexed_sequence_read_access_t;

typedef uint64_t kan_repository_indexed_sequence_write_query_t;

typedef uint64_t kan_repository_indexed_sequence_write_cursor_t;

typedef uint64_t kan_repository_indexed_sequence_write_access_t;

typedef uint64_t kan_repository_indexed_value_read_query_t;

typedef uint64_t kan_repository_indexed_value_read_cursor_t;

typedef uint64_t kan_repository_indexed_value_read_access_t;

typedef uint64_t kan_repository_indexed_value_write_query_t;

typedef uint64_t kan_repository_indexed_value_write_cursor_t;

typedef uint64_t kan_repository_indexed_value_write_access_t;

typedef uint64_t kan_repository_indexed_constant_read_query_t;

typedef uint64_t kan_repository_indexed_constant_read_cursor_t;

typedef uint64_t kan_repository_indexed_constant_read_access_t;

typedef uint64_t kan_repository_indexed_constant_write_query_t;

typedef uint64_t kan_repository_indexed_constant_write_cursor_t;

typedef uint64_t kan_repository_indexed_constant_write_access_t;

typedef uint64_t kan_repository_indexed_interval_read_query_t;

typedef uint64_t kan_repository_indexed_interval_read_cursor_t;

typedef uint64_t kan_repository_indexed_interval_read_access_t;

typedef uint64_t kan_repository_indexed_interval_write_query_t;

typedef uint64_t kan_repository_indexed_interval_write_cursor_t;

typedef uint64_t kan_repository_indexed_interval_write_access_t;

typedef uint64_t kan_repository_indexed_space_shape_read_query_t;

typedef uint64_t kan_repository_indexed_space_shape_read_cursor_t;

typedef uint64_t kan_repository_indexed_space_shape_read_access_t;

typedef uint64_t kan_repository_indexed_space_shape_write_query_t;

typedef uint64_t kan_repository_indexed_space_shape_write_cursor_t;

typedef uint64_t kan_repository_indexed_space_shape_write_access_t;

typedef uint64_t kan_repository_indexed_space_ray_read_query_t;

typedef uint64_t kan_repository_indexed_space_ray_read_cursor_t;

typedef uint64_t kan_repository_indexed_space_ray_read_access_t;

typedef uint64_t kan_repository_indexed_space_ray_write_query_t;

typedef uint64_t kan_repository_indexed_space_ray_write_cursor_t;

typedef uint64_t kan_repository_indexed_space_ray_write_access_t;

typedef uint64_t kan_repository_event_storage_t;

typedef uint64_t kan_repository_event_insert_query_t;

typedef uint64_t kan_repository_event_insertion_package_t;

typedef uint64_t kan_repository_event_fetch_query_t;

typedef uint64_t kan_repository_event_read_access_t;

REPOSITORY_API kan_repository_t kan_repository_create_root (kan_allocation_group_t allocation_group,
                                                            kan_reflection_registry_t registry);

REPOSITORY_API kan_repository_t kan_repository_create_child (kan_repository_t parent, kan_interned_string_t name);

REPOSITORY_API void kan_repository_enter_planning_mode (kan_repository_t root_repository);

REPOSITORY_API void kan_repository_migrate (kan_repository_t root_repository, kan_reflection_registry_t new_registry);

REPOSITORY_API kan_repository_singleton_storage_t
kan_repository_singleton_storage_open (kan_repository_t repository, kan_interned_string_t type_name);

REPOSITORY_API kan_repository_singleton_read_query_t
kan_repository_singleton_read_query_create (kan_repository_singleton_storage_t storage);

REPOSITORY_API kan_repository_singleton_read_access_t
kan_repository_singleton_read_query_execute (kan_repository_singleton_read_query_t query);

REPOSITORY_API const void *kan_repository_singleton_read_access_resolve (kan_repository_singleton_read_access_t access);

REPOSITORY_API void kan_repository_singleton_read_access_close (kan_repository_singleton_read_access_t access);

REPOSITORY_API void kan_repository_singleton_read_query_destroy (kan_repository_singleton_read_query_t handle);

REPOSITORY_API kan_repository_singleton_write_query_t
kan_repository_singleton_write_query_create (kan_repository_singleton_storage_t storage);

REPOSITORY_API kan_repository_singleton_write_access_t
kan_repository_singleton_write_query_execute (kan_repository_singleton_write_query_t query);

REPOSITORY_API void *kan_repository_singleton_write_access_resolve (kan_repository_singleton_write_access_t access);

REPOSITORY_API void kan_repository_singleton_write_access_close (kan_repository_singleton_write_access_t access);

REPOSITORY_API void kan_repository_singleton_write_query_destroy (kan_repository_singleton_write_query_t handle);

REPOSITORY_API void kan_repository_singleton_storage_close (kan_repository_singleton_storage_t storage);

REPOSITORY_API kan_repository_indexed_storage_t kan_repository_indexed_storage_open (kan_repository_t repository,
                                                                                     kan_interned_string_t type_name);

REPOSITORY_API kan_repository_indexed_insert_query_t
kan_repository_indexed_insert_query_create (kan_repository_indexed_storage_t storage);

REPOSITORY_API kan_repository_indexed_insertion_package_t
kan_repository_indexed_insert_query_execute (kan_repository_indexed_insert_query_t query);

REPOSITORY_API void *kan_repository_indexed_insertion_package_get (kan_repository_indexed_insertion_package_t package);

REPOSITORY_API void kan_repository_indexed_insertion_package_undo (kan_repository_indexed_insertion_package_t package);

REPOSITORY_API void kan_repository_indexed_insertion_package_submit (
    kan_repository_indexed_insertion_package_t package);

REPOSITORY_API void kan_repository_indexed_insert_query_destroy (kan_repository_indexed_insert_query_t query);

REPOSITORY_API kan_repository_indexed_sequence_read_query_t
kan_repository_indexed_sequence_read_query_create (kan_repository_indexed_storage_t storage);

REPOSITORY_API kan_repository_indexed_sequence_read_cursor_t
kan_repository_indexed_sequence_read_query_execute (kan_repository_indexed_sequence_read_query_t query);

REPOSITORY_API kan_repository_indexed_sequence_read_access_t
kan_repository_indexed_sequence_read_cursor_next (kan_repository_indexed_sequence_read_cursor_t cursor);

REPOSITORY_API const void *kan_repository_indexed_sequence_read_access_resolve (
    kan_repository_indexed_sequence_read_access_t access);

REPOSITORY_API void kan_repository_indexed_sequence_read_access_close (
    kan_repository_indexed_sequence_read_access_t access);

REPOSITORY_API void kan_repository_indexed_sequence_read_cursor_close (
    kan_repository_indexed_sequence_read_cursor_t cursor);

REPOSITORY_API void kan_repository_indexed_sequence_read_query_destroy (
    kan_repository_indexed_sequence_read_query_t handle);

REPOSITORY_API kan_repository_indexed_sequence_write_query_t
kan_repository_indexed_sequence_write_query_create (kan_repository_indexed_storage_t storage);

REPOSITORY_API kan_repository_indexed_sequence_write_cursor_t
kan_repository_indexed_sequence_write_query_execute (kan_repository_indexed_sequence_write_query_t query);

REPOSITORY_API kan_repository_indexed_sequence_write_access_t
kan_repository_indexed_sequence_write_cursor_next (kan_repository_indexed_sequence_write_cursor_t cursor);

REPOSITORY_API void *kan_repository_indexed_sequence_write_access_resolve (
    kan_repository_indexed_sequence_write_access_t access);

REPOSITORY_API void kan_repository_indexed_sequence_write_access_delete (
    kan_repository_indexed_sequence_write_access_t access);

REPOSITORY_API void kan_repository_indexed_sequence_write_access_close (
    kan_repository_indexed_sequence_write_access_t access);

REPOSITORY_API void kan_repository_indexed_sequence_write_cursor_close (
    kan_repository_indexed_sequence_write_cursor_t cursor);

REPOSITORY_API void kan_repository_indexed_sequence_write_query_destroy (
    kan_repository_indexed_sequence_write_query_t handle);

REPOSITORY_API kan_repository_indexed_value_read_query_t kan_repository_indexed_value_read_query_create (
    kan_repository_indexed_storage_t storage, struct kan_repository_field_path_t field_path);

REPOSITORY_API kan_repository_indexed_value_read_cursor_t
kan_repository_indexed_value_read_query_execute (kan_repository_indexed_value_read_query_t query, void *value_bytes);

REPOSITORY_API kan_repository_indexed_value_read_access_t
kan_repository_indexed_value_read_cursor_next (kan_repository_indexed_value_read_cursor_t cursor);

REPOSITORY_API const void *kan_repository_indexed_value_read_access_resolve (
    kan_repository_indexed_value_read_access_t access);

REPOSITORY_API void kan_repository_indexed_value_read_access_close (kan_repository_indexed_value_read_access_t access);

REPOSITORY_API void kan_repository_indexed_value_read_cursor_close (kan_repository_indexed_value_read_cursor_t cursor);

REPOSITORY_API void kan_repository_indexed_value_read_query_destroy (kan_repository_indexed_value_read_query_t handle);

REPOSITORY_API kan_repository_indexed_value_write_query_t kan_repository_indexed_value_write_query_create (
    kan_repository_indexed_storage_t storage, struct kan_repository_field_path_t field_path);

REPOSITORY_API kan_repository_indexed_value_write_cursor_t
kan_repository_indexed_value_write_query_execute (kan_repository_indexed_value_write_query_t query, void *value_bytes);

REPOSITORY_API kan_repository_indexed_value_write_access_t
kan_repository_indexed_value_write_cursor_next (kan_repository_indexed_value_write_cursor_t cursor);

REPOSITORY_API void *kan_repository_indexed_value_write_access_resolve (
    kan_repository_indexed_value_write_access_t access);

REPOSITORY_API void kan_repository_indexed_value_write_access_delete (
    kan_repository_indexed_value_write_access_t access);

REPOSITORY_API void kan_repository_indexed_value_write_access_close (
    kan_repository_indexed_value_write_access_t access);

REPOSITORY_API void kan_repository_indexed_value_write_cursor_close (
    kan_repository_indexed_value_write_cursor_t cursor);

REPOSITORY_API void kan_repository_indexed_value_write_query_destroy (
    kan_repository_indexed_value_write_query_t handle);

REPOSITORY_API kan_repository_indexed_constant_read_query_t kan_repository_indexed_constant_read_query_create (
    kan_repository_indexed_storage_t storage, struct kan_repository_field_path_t field_path, void *constant_bytes);

REPOSITORY_API kan_repository_indexed_constant_read_cursor_t
kan_repository_indexed_constant_read_query_execute (kan_repository_indexed_constant_read_query_t query);

REPOSITORY_API kan_repository_indexed_constant_read_access_t
kan_repository_indexed_constant_read_cursor_next (kan_repository_indexed_constant_read_cursor_t cursor);

REPOSITORY_API const void *kan_repository_indexed_constant_read_access_resolve (
    kan_repository_indexed_constant_read_access_t access);

REPOSITORY_API void kan_repository_indexed_constant_read_access_close (
    kan_repository_indexed_constant_read_access_t access);

REPOSITORY_API void kan_repository_indexed_constant_read_cursor_close (
    kan_repository_indexed_constant_read_cursor_t cursor);

REPOSITORY_API void kan_repository_indexed_constant_read_query_destroy (
    kan_repository_indexed_constant_read_query_t handle);

REPOSITORY_API kan_repository_indexed_constant_write_query_t kan_repository_indexed_constant_write_query_create (
    kan_repository_indexed_storage_t storage, struct kan_repository_field_path_t field_path, void *constant_bytes);

REPOSITORY_API kan_repository_indexed_constant_write_cursor_t
kan_repository_indexed_constant_write_query_execute (kan_repository_indexed_constant_write_query_t query);

REPOSITORY_API kan_repository_indexed_constant_write_access_t
kan_repository_indexed_constant_write_cursor_next (kan_repository_indexed_constant_write_cursor_t cursor);

REPOSITORY_API void *kan_repository_indexed_constant_write_access_resolve (
    kan_repository_indexed_constant_write_access_t access);

REPOSITORY_API void kan_repository_indexed_constant_write_access_delete (
    kan_repository_indexed_constant_write_access_t access);

REPOSITORY_API void kan_repository_indexed_constant_write_access_close (
    kan_repository_indexed_constant_write_access_t access);

REPOSITORY_API void kan_repository_indexed_constant_write_cursor_close (
    kan_repository_indexed_constant_write_cursor_t cursor);

REPOSITORY_API void kan_repository_indexed_constant_write_query_destroy (
    kan_repository_indexed_constant_write_query_t handle);

REPOSITORY_API kan_repository_indexed_interval_read_query_t kan_repository_indexed_interval_read_query_create (
    kan_repository_indexed_storage_t storage, struct kan_repository_field_path_t field_path);

REPOSITORY_API kan_repository_indexed_interval_read_cursor_t kan_repository_indexed_interval_read_query_execute (
    kan_repository_indexed_interval_read_query_t query, void *min_bytes, void *max_bytes);

REPOSITORY_API kan_repository_indexed_interval_read_access_t
kan_repository_indexed_interval_read_cursor_next (kan_repository_indexed_interval_read_cursor_t cursor);

REPOSITORY_API const void *kan_repository_indexed_interval_read_access_resolve (
    kan_repository_indexed_interval_read_access_t access);

REPOSITORY_API void kan_repository_indexed_interval_read_access_close (
    kan_repository_indexed_interval_read_access_t access);

REPOSITORY_API void kan_repository_indexed_interval_read_cursor_close (
    kan_repository_indexed_interval_read_cursor_t cursor);

REPOSITORY_API void kan_repository_indexed_interval_read_query_destroy (
    kan_repository_indexed_interval_read_query_t handle);

REPOSITORY_API kan_repository_indexed_interval_write_query_t kan_repository_indexed_interval_write_query_create (
    kan_repository_indexed_storage_t storage, struct kan_repository_field_path_t field_path);

REPOSITORY_API kan_repository_indexed_interval_write_cursor_t kan_repository_indexed_interval_write_query_execute (
    kan_repository_indexed_interval_write_query_t query, void *min_bytes, void *max_bytes);

REPOSITORY_API kan_repository_indexed_interval_write_access_t
kan_repository_indexed_interval_write_cursor_next (kan_repository_indexed_interval_write_cursor_t cursor);

REPOSITORY_API void *kan_repository_indexed_interval_write_access_resolve (
    kan_repository_indexed_interval_write_access_t access);

REPOSITORY_API void kan_repository_indexed_interval_write_access_delete (
    kan_repository_indexed_interval_write_access_t access);

REPOSITORY_API void kan_repository_indexed_interval_write_access_close (
    kan_repository_indexed_interval_write_access_t access);

REPOSITORY_API void kan_repository_indexed_interval_write_cursor_close (
    kan_repository_indexed_interval_write_cursor_t cursor);

REPOSITORY_API void kan_repository_indexed_interval_write_query_destroy (
    kan_repository_indexed_interval_write_query_t handle);

REPOSITORY_API kan_repository_indexed_space_shape_read_query_t kan_repository_indexed_space_shape_read_query_create (
    kan_repository_indexed_storage_t storage, kan_interned_string_t space_name);

REPOSITORY_API kan_repository_indexed_space_shape_read_cursor_t kan_repository_indexed_space_shape_read_query_execute (
    kan_repository_indexed_space_shape_read_query_t query, void *min_bytes, void *max_bytes);

REPOSITORY_API kan_repository_indexed_space_shape_read_access_t
kan_repository_indexed_space_shape_read_cursor_next (kan_repository_indexed_space_shape_read_cursor_t cursor);

REPOSITORY_API const void *kan_repository_indexed_space_shape_read_access_resolve (
    kan_repository_indexed_space_shape_read_access_t access);

REPOSITORY_API void kan_repository_indexed_space_shape_read_access_close (
    kan_repository_indexed_space_shape_read_access_t access);

REPOSITORY_API void kan_repository_indexed_space_shape_read_cursor_close (
    kan_repository_indexed_space_shape_read_cursor_t cursor);

REPOSITORY_API void kan_repository_indexed_space_shape_read_query_destroy (
    kan_repository_indexed_space_shape_read_query_t handle);

REPOSITORY_API kan_repository_indexed_space_shape_write_query_t kan_repository_indexed_space_shape_write_query_create (
    kan_repository_indexed_storage_t storage, kan_interned_string_t space_name);

REPOSITORY_API kan_repository_indexed_space_shape_write_cursor_t
kan_repository_indexed_space_shape_write_query_execute (kan_repository_indexed_space_shape_write_query_t query,
                                                        void *min_bytes,
                                                        void *max_bytes);

REPOSITORY_API kan_repository_indexed_space_shape_write_access_t
kan_repository_indexed_space_shape_write_cursor_next (kan_repository_indexed_space_shape_write_cursor_t cursor);

REPOSITORY_API void *kan_repository_indexed_space_shape_write_access_resolve (
    kan_repository_indexed_space_shape_write_access_t access);

REPOSITORY_API void kan_repository_indexed_space_shape_write_access_delete (
    kan_repository_indexed_space_shape_write_access_t access);

REPOSITORY_API void kan_repository_indexed_space_shape_write_access_close (
    kan_repository_indexed_space_shape_write_access_t access);

REPOSITORY_API void kan_repository_indexed_space_shape_write_cursor_close (
    kan_repository_indexed_space_shape_write_cursor_t cursor);

REPOSITORY_API void kan_repository_indexed_space_shape_write_query_destroy (
    kan_repository_indexed_space_shape_write_query_t handle);

REPOSITORY_API kan_repository_indexed_space_ray_read_query_t kan_repository_indexed_space_ray_read_query_create (
    kan_repository_indexed_storage_t storage, kan_interned_string_t space_name);

REPOSITORY_API kan_repository_indexed_space_ray_read_cursor_t kan_repository_indexed_space_ray_read_query_execute (
    kan_repository_indexed_space_ray_read_query_t query, void *origin_bytes, void *direction_bytes);

REPOSITORY_API kan_repository_indexed_space_ray_read_access_t
kan_repository_indexed_space_ray_read_cursor_next (kan_repository_indexed_space_ray_read_cursor_t cursor);

REPOSITORY_API const void *kan_repository_indexed_space_ray_read_access_resolve (
    kan_repository_indexed_space_ray_read_access_t access);

REPOSITORY_API void kan_repository_indexed_space_ray_read_access_close (
    kan_repository_indexed_space_ray_read_access_t access);

REPOSITORY_API void kan_repository_indexed_space_ray_read_cursor_close (
    kan_repository_indexed_space_ray_read_cursor_t cursor);

REPOSITORY_API void kan_repository_indexed_space_ray_read_query_destroy (
    kan_repository_indexed_space_ray_read_query_t handle);

REPOSITORY_API kan_repository_indexed_space_ray_write_query_t kan_repository_indexed_space_ray_write_query_create (
    kan_repository_indexed_storage_t storage, kan_interned_string_t space_name);

REPOSITORY_API kan_repository_indexed_space_ray_write_cursor_t kan_repository_indexed_space_ray_write_query_execute (
    kan_repository_indexed_space_ray_write_query_t query, void *origin_bytes, void *direction_bytes);

REPOSITORY_API kan_repository_indexed_space_ray_write_access_t
kan_repository_indexed_space_ray_write_cursor_next (kan_repository_indexed_space_ray_write_cursor_t cursor);

REPOSITORY_API void *kan_repository_indexed_space_ray_write_access_resolve (
    kan_repository_indexed_space_ray_write_access_t access);

REPOSITORY_API void kan_repository_indexed_space_ray_write_access_delete (
    kan_repository_indexed_space_ray_write_access_t access);

REPOSITORY_API void kan_repository_indexed_space_ray_write_access_close (
    kan_repository_indexed_space_ray_write_access_t access);

REPOSITORY_API void kan_repository_indexed_space_ray_write_cursor_close (
    kan_repository_indexed_space_ray_write_cursor_t cursor);

REPOSITORY_API void kan_repository_indexed_space_ray_write_query_destroy (
    kan_repository_indexed_space_ray_write_query_t handle);

REPOSITORY_API void kan_repository_indexed_storage_close (kan_repository_indexed_storage_t storage);

REPOSITORY_API kan_repository_event_storage_t kan_repository_event_storage_open (kan_repository_t repository,
                                                                                 kan_interned_string_t type_name);

REPOSITORY_API kan_repository_event_insert_query_t
kan_repository_event_insert_query_create (kan_repository_event_storage_t storage);

REPOSITORY_API kan_repository_event_insertion_package_t
kan_repository_event_insert_query_execute (kan_repository_event_insert_query_t query);

REPOSITORY_API void *kan_repository_event_insertion_package_get (kan_repository_event_insertion_package_t package);

REPOSITORY_API void kan_repository_event_insertion_package_undo (kan_repository_event_insertion_package_t package);

REPOSITORY_API void kan_repository_event_insertion_package_submit (kan_repository_event_insertion_package_t package);

REPOSITORY_API void kan_repository_event_insert_query_destroy (kan_repository_event_insert_query_t query);

REPOSITORY_API kan_repository_event_fetch_query_t
kan_repository_event_fetch_query_create (kan_repository_event_storage_t storage);

REPOSITORY_API kan_repository_event_read_access_t
kan_repository_event_fetch_query_next (kan_repository_event_fetch_query_t query);

REPOSITORY_API const void *kan_repository_event_read_access_resolve (kan_repository_event_read_access_t access);

REPOSITORY_API void kan_repository_event_read_access_close (kan_repository_event_read_access_t access);

REPOSITORY_API void kan_repository_event_fetch_query_destroy (kan_repository_event_fetch_query_t query);

REPOSITORY_API void kan_repository_event_storage_close (kan_repository_event_storage_t storage);

REPOSITORY_API void kan_repository_enter_serving_mode (kan_repository_t root_repository);

REPOSITORY_API void kan_repository_destroy (kan_repository_t repository);

KAN_C_HEADER_END
