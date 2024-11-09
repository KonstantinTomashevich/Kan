#pragma once

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>

/// \file
/// \brief Contains utility inline functions for working with alignment.

KAN_C_HEADER_BEGIN

/// \brief Makes given address or size aligned by adding offset or padding if necessary.
static inline kan_memory_size_t kan_apply_alignment (kan_memory_size_t address_or_size, kan_memory_size_t alignment)
{
    const kan_memory_size_t modulo = address_or_size % alignment;
    if (modulo != 0u)
    {
        address_or_size += alignment - modulo;
    }

    return address_or_size;
}

KAN_C_HEADER_END
