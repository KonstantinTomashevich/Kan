#pragma once

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Contains utility inline functions for working with alignment.

KAN_C_HEADER_BEGIN

/// \brief Makes given address or size aligned by adding offset or padding if necessary.
static inline uint64_t kan_apply_alignment (uint64_t address_or_size, uint64_t alignment)
{
    const uint64_t modulo = address_or_size % alignment;
    if (modulo != 0u)
    {
        address_or_size += alignment - modulo;
    }

    return address_or_size;
}

KAN_C_HEADER_END
