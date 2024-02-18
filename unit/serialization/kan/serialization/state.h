#pragma once

#include <serialization_api.h>

#include <kan/api_common/c_header.h>

KAN_C_HEADER_BEGIN

enum kan_serialization_state_t
{
    KAN_SERIALIZATION_IN_PROGRESS = 0,
    KAN_SERIALIZATION_FINISHED,
    KAN_SERIALIZATION_FAILED,
};

KAN_C_HEADER_END
