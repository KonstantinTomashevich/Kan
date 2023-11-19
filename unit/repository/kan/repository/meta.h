#pragma once

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/container/interned_string.h>

KAN_C_HEADER_BEGIN

struct kan_repository_field_path_t
{
    kan_interned_string_t *reflection_path;
};

struct kan_repository_dimension_t
{
    struct kan_repository_field_path_t min_path;
    struct kan_repository_field_path_t max_path;
    double min;
    double max;
};

struct kan_repository_meta_space_t
{
    kan_interned_string_t name;
    uint64_t dimension_count;
    struct kan_repository_dimension_t *dimensions;
};

struct kan_repository_copy_out_t
{
    struct kan_repository_field_path_t source_path;
    struct kan_repository_field_path_t target_path;
};

struct kan_repository_meta_automatic_on_add_event_t
{
    kan_interned_string_t trigger_type;
    uint64_t copy_outs_count;
    struct kan_repository_copy_out_t *copy_outs;
};

struct kan_repository_meta_automatic_on_change_event_t
{
    kan_interned_string_t trigger_type;
    uint64_t observed_fields_count;
    struct kan_repository_field_path_t *observed_fields;
    uint64_t unchanged_copy_outs_count;
    struct kan_repository_copy_out_t *unchanged_copy_outs;
    uint64_t changed_copy_outs_count;
    struct kan_repository_copy_out_t *changed_copy_outs;
};

struct kan_repository_meta_automatic_on_remove_event_t
{
    kan_interned_string_t trigger_type;
    uint64_t copy_outs_count;
    struct kan_repository_copy_out_t *copy_outs;
};

KAN_C_HEADER_END
