#pragma once

#include <stdint.h>

#include <kan/api_common/c_header.h>

KAN_C_HEADER_BEGIN

struct kan_repository_field_path_t
{
    uint64_t reflection_path_length;
    const char **reflection_path;
};

struct kan_repository_copy_out_t
{
    struct kan_repository_field_path_t source_path;
    struct kan_repository_field_path_t target_path;
};

struct kan_repository_meta_automatic_on_insert_event_t
{
    const char *event_type;
    uint64_t copy_outs_count;
    struct kan_repository_copy_out_t *copy_outs;
};

struct kan_repository_meta_automatic_on_change_event_t
{
    const char *event_type;
    uint64_t observed_fields_count;
    struct kan_repository_field_path_t *observed_fields;
    uint64_t unchanged_copy_outs_count;
    struct kan_repository_copy_out_t *unchanged_copy_outs;
    uint64_t changed_copy_outs_count;
    struct kan_repository_copy_out_t *changed_copy_outs;
};

struct kan_repository_meta_automatic_on_delete_event_t
{
    const char *event_type;
    uint64_t copy_outs_count;
    struct kan_repository_copy_out_t *copy_outs;
};

struct kan_repository_meta_automatic_cascade_deletion_t
{
    struct kan_repository_field_path_t parent_key_path;
    const char *child_type_name;
    struct kan_repository_field_path_t child_key_path;
};

KAN_C_HEADER_END
