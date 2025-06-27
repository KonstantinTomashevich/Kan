#pragma once

#include <universe_resource_reference_api.h>

#include <kan/api_common/c_header.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

/// \file
/// \brief Provides public API for Resource Reference extension for Universe unit.
///
/// \par Definition
/// \parblock
/// Resource reference extension is built on top of resource provider extension and provides API for retrieving
/// references to resources. It is able to attach all outer references of particular resource to its native entry,
/// also it supports scan-all-resources-that-can-reference-requested-type query, which is useful when you need to
/// get inner references to particular resource. It also utilizes resource pipeline unit and caching for references.
///
/// Disambiguation. Outer references are references that go from its resource to other resources. For example,
/// material instance may reference texture and it is registered as outer reference. Inner references are references
/// that go from other resources to its resource. For example, when material instances references texture, it is an
/// inner reference to texture.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Group, used to add all the mutators that implement resource reference extension.
#define KAN_RESOURCE_REFERENCE_MUTATOR_GROUP "resource_reference"

/// \brief Name for resource reference configuration object in universe world.
#define KAN_RESOURCE_REFERENCE_CONFIGURATION "resource_reference"

/// \brief Checkpoint, after which resource reference mutators are executed.
#define KAN_RESOURCE_REFERENCE_BEGIN_CHECKPOINT "resource_reference_begin"

/// \brief Checkpoint, that is hit after all resource reference mutators finished execution.
#define KAN_RESOURCE_REFERENCE_END_CHECKPOINT "resource_reference_end"

/// \brief Attachment for `kan_resource_native_entry_t` that describes one outer reference.
struct kan_resource_native_entry_outer_reference_t
{
    kan_resource_entry_id_t attachment_id;
    kan_interned_string_t reference_type;
    kan_interned_string_t reference_name;
};

/// \brief Requests outer reference attachments on native entry with given type and name to be updated.
struct kan_resource_update_outer_references_request_event_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
};

/// \brief Sent as a response to `kan_resource_update_outer_references_request_event_t` when operation is finished.
struct kan_resource_update_outer_references_response_event_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    bool successful;

    /// \brief Entry attachment id is added to make parsing response result more convenient.
    kan_resource_entry_id_t entry_attachment_id;
};

/// \brief Requests outer reference attachments to be update on all native entries that can reference given type.
struct kan_resource_update_all_references_to_type_request_event_t
{
    kan_interned_string_t type;
};

/// \brief Sent as a response to `kan_resource_update_all_references_to_type_request_event_t` when operation is
///        finished.
struct kan_resource_update_all_references_to_type_response_event_t
{
    kan_interned_string_t type;
    bool successful;
};

/// \brief Structure that contains configuration for resource reference manager.
struct kan_resource_reference_configuration_t
{
    /// \brief Maximum time per frame for processing resource reference operations.
    kan_time_offset_t budget_ns;

    /// \brief Virtual directory that can be used as workspace for storing outer reference caches.
    kan_interned_string_t workspace_directory_path;
};

KAN_C_HEADER_END
