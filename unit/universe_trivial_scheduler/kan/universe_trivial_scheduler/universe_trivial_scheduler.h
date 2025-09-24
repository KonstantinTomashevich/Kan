#pragma once

#include <universe_trivial_scheduler_api.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Provides trivial drop-in scheduler for simplistic update behavior.
///
/// \par Definition
/// \parblock
/// This unit provides simplistic scheduler that always calls `KAN_UNIVERSE_TRIVIAL_SCHEDULER_PIPELINE_NAME` pipeline
/// and then calls schedulers of all child worlds. Useful for basic cases when no additional logic is needed.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Name of the trivial scheduler that can be used in configuration.
#define KAN_UNIVERSE_TRIVIAL_SCHEDULER_NAME "trivial"

/// \brief Name of the pipeline that is executed by trivial scheduler.
#define KAN_UNIVERSE_TRIVIAL_SCHEDULER_PIPELINE_NAME "update"

KAN_C_HEADER_END
