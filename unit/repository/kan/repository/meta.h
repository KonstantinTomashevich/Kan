#pragma once

#include <stdint.h>

#include <kan/api_common/c_header.h>

KAN_C_HEADER_BEGIN

/// \brief Describes path to a field of reflected structure.
struct kan_repository_field_path_t
{
    uint64_t reflection_path_length;
    const char **reflection_path;
};

/// \brief Describes pair of source and target field for copy out operation.
struct kan_repository_copy_out_t
{
    struct kan_repository_field_path_t source_path;
    struct kan_repository_field_path_t target_path;
};

/// \brief Describes automatic event that should be fired on indexed records insertion.
/// \details Copy outs are used to copy data from inserted record to the event.
///          Should be attached to indexed record type.
struct kan_repository_meta_automatic_on_insert_event_t
{
    const char *event_type;
    uint64_t copy_outs_count;
    struct kan_repository_copy_out_t *copy_outs;
};

/// \brief Describes automatic event that should be fired when any observed field has changed.
/// \details Copy outs are used to copy data from changed record to the event.
///          There are both unchanged and changed copy outs, but unchanged copy outs only support observed fields.
///          Should be attached to indexed or singleton record type.
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

/// \brief Describes automatic event that should be fired on indexed records deletion.
/// \details Copy outs are used to copy data from deleted record to the event.
///          Should be attached to indexed record type.
struct kan_repository_meta_automatic_on_delete_event_t
{
    const char *event_type;
    uint64_t copy_outs_count;
    struct kan_repository_copy_out_t *copy_outs;
};

/// \brief Describes link between types for automatic cascade deletion.
/// \details Should be attached to indexed record type which deletion triggers cascade.
///          Records are deleted if values in parent key path and child key path are equal.
///          Child type name could be equal to attached type name, this case results in hierarchy deletion.
struct kan_repository_meta_automatic_cascade_deletion_t
{
    struct kan_repository_field_path_t parent_key_path;
    const char *child_type_name;
    struct kan_repository_field_path_t child_key_path;
};

KAN_C_HEADER_END
