#pragma once

#include <cpu_profiler_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>

/// \file
/// \brief Contains functions for marking events for CPU profiling.
///
/// \par Usage
/// \parblock
/// To report execution section you need to start by creating section information using `kan_cpu_section_t` and
/// `kan_cpu_section_init`. Then, when you actually enter the section, you need to allocate
/// `kan_cpu_section_execution_t` on stack and call `kan_cpu_section_execution_init`. To exit section, you must call
/// `kan_cpu_section_execution_shutdown`.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// Addressing sections is fully thread safe: after initializing section, you can access it from multiple threads at any
/// time. But section execution structure is single threaded: you need to exit section in the same thread you have
/// entered it. But it is allowed to have several section executions of one section in different threads simultaneously.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Separates program execution into stages. In game development it is usually frames.
CPU_PROFILER_API void kan_cpu_stage_separator ();

struct kan_cpu_section_t
{
    uint64_t implementation_data[4u];
};

/// \brief Initializes CPU section with given meta information.
CPU_PROFILER_API void kan_cpu_section_init (struct kan_cpu_section_t *section, const char *name, uint32_t rgba_color);

/// \brief Cleans up CPU section resources.
CPU_PROFILER_API void kan_cpu_section_shutdown (struct kan_cpu_section_t *section);

struct kan_cpu_section_execution_t
{
    uint64_t implementation_data[2u];
};

/// \brief Informs profiler that section is entered and initializes execution context.
CPU_PROFILER_API void kan_cpu_section_execution_init (struct kan_cpu_section_execution_t *execution,
                                                      struct kan_cpu_section_t *section);

/// \brief Informs profiler that section is exited and cleans execution context.
CPU_PROFILER_API void kan_cpu_section_execution_shutdown (struct kan_cpu_section_execution_t *execution);

KAN_C_HEADER_END
