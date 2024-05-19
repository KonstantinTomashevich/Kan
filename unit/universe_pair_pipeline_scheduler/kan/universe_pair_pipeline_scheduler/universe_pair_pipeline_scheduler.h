#pragma once

#include <universe_pair_pipeline_scheduler_api.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Provides information about pair pipeline scheduler.
///
/// \par
/// \parblock
/// Pair pipeline scheduler should be selected when user needs two different update pipelines: logical pipeline with
/// guaranteed fixed time step for stable simulation and visual pipeline with variable time step for different visual
/// stuff.
///
/// It works on top of kan_time_singleton_t in order to ensure that pipelines operate in a correct and synchronized
/// time. In this case, time is a resource, logical pipeline is a resource producer and visual pipeline is a resource
/// consumer. It means that logical pipeline is executed when visual pipeline "consumed" enough time to reach logical
/// time and "new time needs to be produced" by executing logical pipeline with fixed time step.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Name of the pair pipeline scheduler.
#define KAN_UNIVERSE_PAIR_PIPELINE_SCHEDULER_NAME "pair_pipeline"

/// \brief Name of the pipeline that is considered logical pipeline for pair pipeline scheduler.
#define KAN_UNIVERSE_PAIR_PIPELINE_SCHEDULER_LOGICAL_PIPELINE_NAME "logical_update"

/// \brief Name of the pipeline that is considered visual pipeline for pair pipeline scheduler.
#define KAN_UNIVERSE_PAIR_PIPELINE_SCHEDULER_VISUAL_PIPELINE_NAME "visual_update"

/// \brief Default value for kan_pair_pipeline_settings_singleton_t::logical_time_step_ns.
#define KAN_UNIVERSE_PAIR_PIPELINE_SCHEDULER_DEFAULT_LOGICAL_TIME_STEP_NS 8000000u

/// \brief Default value for kan_pair_pipeline_settings_singleton_t::max_logical_advance_time_ns.
#define KAN_UNIVERSE_PAIR_PIPELINE_SCHEDULER_DEFAULT_MAX_LOGICAL_ADVANCE_TIME_NS 25000000u

/// \brief Singleton that contains settings for pair pipeline scheduler. Expected to be edited by user.
struct kan_pair_pipeline_settings_singleton_t
{
    /// \brief Length of fixed time step for logical pipeline.
    uint64_t logical_time_step_ns;

    /// \brief If logical pipeline advance exceeds this time limit, game will slow down to avoid death spiral.
    /// \details Logical pipeline advance is a procedure of calling logical pipeline until its time steps produce
    ///          enough time for new visual pipeline update.
    uint64_t max_logical_advance_time_ns;
};

UNIVERSE_PAIR_PIPELINE_SCHEDULER_API void kan_pair_pipeline_settings_singleton_init (
    struct kan_pair_pipeline_settings_singleton_t *instance);

KAN_C_HEADER_END
