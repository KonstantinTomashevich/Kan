#pragma once

#include <application_framework_tool_common_api.h>

#include <kan/api_common/c_header.h>
#include <kan/context/context.h>

/// \file
/// \brief Contains common utilities for tools to be used with application framework.

KAN_C_HEADER_BEGIN

/// \brief Path to plugins directory. Generated and statically linked in by application framework to its tools.
extern char *kan_application_framework_tool_plugins_directory;

/// \brief Count of application plugins. Generated and statically linked in by application framework to its tools.
extern uint64_t kan_application_framework_tool_plugins_count;

/// \brief All application plugin names, Generated and statically linked in by application framework to its tools.
extern char *kan_application_framework_tool_plugins[];

/// \brief Creates and assembles minimal tool context with only plugin system and reflection system.
APPLICATION_FRAMEWORK_TOOL_COMMON_API kan_context_handle_t kan_application_framework_tool_create_context (void);

KAN_C_HEADER_END
