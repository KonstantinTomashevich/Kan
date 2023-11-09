#pragma once

#include <c_interface_api.h>

#include <kan/api_common/c_header.h>
#include <kan/c_interface/interface.h>

/// \file
/// \brief Describes file format that should be followed by c interface scanners and scan users.

KAN_C_HEADER_BEGIN

/// \brief Contents of file with scanned c interface.
struct kan_c_interface_file_t
{
    struct kan_c_interface_t *interface;
    char *source_file_path;

    /// \brief Optional data for non-includable objects (.c files).
    ///        Contains "includable" version of file -- everything related to implementation is stripped out.
    char *optional_includable_object;
};

/// \brief Initializes structure for storing c interface file.
C_INTERFACE_API void kan_c_interface_file_init (struct kan_c_interface_file_t *file);

/// \brief Returns whether c interface file, described by this structure,
///        should have kan_c_interface_file_t::optional_includable_object.
C_INTERFACE_API kan_bool_t kan_c_interface_file_should_have_includable_object (struct kan_c_interface_file_t *file);

/// \brief Serializes c interface file content to given stream.
C_INTERFACE_API kan_bool_t kan_c_interface_file_serialize (struct kan_c_interface_file_t *file,
                                                           struct kan_stream_t *stream);

/// \brief Deserializes c interface file content from given stream.
C_INTERFACE_API kan_bool_t kan_c_interface_file_deserialize (struct kan_c_interface_file_t *file,
                                                             struct kan_stream_t *stream);

/// \brief Shuts down structure for storing c interface file and frees its resources.
C_INTERFACE_API void kan_c_interface_file_shutdown (struct kan_c_interface_file_t *file);

KAN_C_HEADER_END
