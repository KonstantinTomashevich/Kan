#pragma once

#include <application_framework_resource_tool_api.h>

#include <kan/application_framework_resource_tool/project.h>
#include <kan/context/context.h>

/// \file
/// \brief Utility functions for working with context from application framework tools.

KAN_C_HEADER_BEGIN

/// \brief Creates and assembles minimalistic context with plugins and reflection, which is enough for most tools.
APPLICATION_FRAMEWORK_RESOURCE_TOOL_API kan_context_t kan_application_create_resource_tool_context (
    const struct kan_application_resource_project_t *project, const char *executable_path);

KAN_C_HEADER_END
