#pragma once

#include <cpu_profiler_api.h>

#include <stdint.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>

/// \file
/// \brief Contains functions for marking events for CPU profiling.
///
/// \par Usage
/// \parblock
/// To report CPU usage you firstly need to get appropriate section instance. Sections are global and identified by
/// unique names. You can get section through kan_cpu_section_get, but be careful: it has average execution time of
/// two hash lookups, may allocate new section and may trigger hash storage rehash. Therefore, it is advised to get
/// section instance once and cache it.
///
/// After you have section instance, you can report CPU usage like that:
/// ```c
/// struct kan_cpu_section_execution_t current_execution;
/// // Report that section is entered by initializing execution.
/// kan_cpu_section_execution_init (&current_execution, section);
/// // ... Do your stuff here ...
/// // Then report that section is exited by shutting down the execution.
/// kan_cpu_section_execution_shutdown (&current_execution);
/// ```
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// Getting sections is fully thread safe: you can do it from multiple threads at any time. Keep in mind that setting
/// section color from different threads naturally results in race condition.
///
/// But section execution structure is single threaded: you need to exit section in the same thread you have
/// entered it. But it is allowed to have several section executions of one section in different threads simultaneously.
/// \endparblock

KAN_C_HEADER_BEGIN

/// \brief Separates program execution into stages. In game development it is usually frames.
CPU_PROFILER_API void kan_cpu_stage_separator (void);

KAN_HANDLE_DEFINE (kan_cpu_section_t);

/// \brief Returns section instance with given name. Creates it if it does not exist.
CPU_PROFILER_API kan_cpu_section_t kan_cpu_section_get (const char *name);

/// \brief Sets color for given section instance.
CPU_PROFILER_API void kan_cpu_section_set_color (kan_cpu_section_t section, uint32_t rgba_color);

struct kan_cpu_section_execution_t
{
    void *implementation_data[2u];
};

/// \brief Informs profiler that section is entered and initializes execution context.
CPU_PROFILER_API void kan_cpu_section_execution_init (struct kan_cpu_section_execution_t *execution,
                                                      kan_cpu_section_t section);

/// \brief Informs profiler that section is exited and cleans execution context.
CPU_PROFILER_API void kan_cpu_section_execution_shutdown (struct kan_cpu_section_execution_t *execution);

KAN_C_HEADER_END
