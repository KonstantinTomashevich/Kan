#pragma once

#include <application_framework_resource_tool_api.h>

#include <kan/application_framework_resource_tool/project.h>
#include <kan/context/context.h>

/// \file
/// \brief Utility functions for working with context from application framework tools.

KAN_C_HEADER_BEGIN

/// \brief Additional optional capabilities for application tool context.
enum kan_application_tool_context_capability_t
{
    KAN_APPLICATION_TOOL_CONTEXT_CAPABILITY_PLATFORM_CONFIGURATION = 1u << 0u,
    KAN_APPLICATION_TOOL_CONTEXT_CAPABILITY_REFERENCE_TYPE_INFO_STORAGE = 1u << 1u,
};

/// \brief Creates and assembles minimalistic context with plugins, reflection and specified capabilities if any.
APPLICATION_FRAMEWORK_RESOURCE_TOOL_API kan_context_t
kan_application_create_resource_tool_context (const struct kan_application_resource_project_t *project,
                                              const char *executable_path,
                                              enum kan_application_tool_context_capability_t capability_flags);

KAN_C_HEADER_END
