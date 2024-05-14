#pragma once

#include <universe_single_pipeline_scheduler_api.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Provides information about single pipeline scheduler.
///
/// \par Definition
/// \parblock
/// Single pipeline scheduler represents most basic scheduling type: it just runs pipeline with name
/// KAN_UNIVERSE_SINGLE_PIPELINE_SCHEDULER_PIPELINE_NAME and then updates all child worlds.
///
/// There are two variants of single pipeline scheduler:
/// - Version with time also updates kan_time_singleton_t by scaled delta time, that affects both logical and visual
///   time.
/// - No-time version just runs pipeline without accessing time.
///
/// No-time version is advised for root worlds as it avoids creating time singleton in root. Version with time is good
/// for leaf worlds that have time-related logic.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Name of the scheduler variant with time.
#define KAN_UNIVERSE_SINGLE_PIPELINE_SCHEDULER_NAME "single_pipeline"

/// \brief Name of the scheduler no-time variant.
#define KAN_UNIVERSE_SINGLE_PIPELINE_NO_TIME_SCHEDULER_NAME "single_pipeline_no_time"

/// \brief Name of the pipeline that is executed by single pipeline scheduler.
#define KAN_UNIVERSE_SINGLE_PIPELINE_SCHEDULER_PIPELINE_NAME "update"

KAN_C_HEADER_END
