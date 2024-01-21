#pragma once

#include <universe_api.h>

#include <kan/api_common/c_header.h>
#include <kan/context/context.h>
#include <kan/universe/universe.h>

KAN_C_HEADER_BEGIN

UNIVERSE_API kan_universe_t kan_universe_system_get_universe (kan_context_system_handle_t universe_system);

KAN_C_HEADER_END
