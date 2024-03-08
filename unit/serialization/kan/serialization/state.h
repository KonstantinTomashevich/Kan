#pragma once

#include <serialization_api.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Describes common serialization state enum for different serializers.

KAN_C_HEADER_BEGIN

/// \brief Describes state of serializer after executing step.
enum kan_serialization_state_t
{
    /// \brief There is more data to process, therefore serialization is in progress.
    KAN_SERIALIZATION_IN_PROGRESS = 0,

    /// \brief Serialization is fully complete and serializer should be destroyed.
    KAN_SERIALIZATION_FINISHED,

    /// \brief Serialization resulted in error and serializer should be destroyed.
    KAN_SERIALIZATION_FAILED,
};

KAN_C_HEADER_END
