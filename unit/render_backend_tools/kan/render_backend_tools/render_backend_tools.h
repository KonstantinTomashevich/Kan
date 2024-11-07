#pragma once

#include <render_backend_tools_api.h>

#include <kan/api_common/c_header.h>
#include <kan/render_pipeline_language/compiler.h>

/// \file
/// \brief Contains functions for preparing data for context_render_backend_system with the same implementation.

KAN_C_HEADER_BEGIN

/// \brief Calls kan_rpl_compiler_instance_emit_* to emit code that is readable by implementation.
RENDER_BACKEND_TOOLS_API kan_bool_t
kan_render_backend_tools_emit_platform_code (kan_rpl_compiler_instance_t compiler_instance,
                                             struct kan_dynamic_array_t *output,
                                             kan_allocation_group_t output_allocation_group);

KAN_C_HEADER_END
