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

/// \brief Prepares utilities needed to properly register sections stored as static cpu profiler sections.
#define KAN_USE_STATIC_CPU_SECTIONS                                                                                    \
    static bool kan_static_profiler_sections_initialized = false;                                                      \
    CUSHION_STATEMENT_ACCUMULATOR (kan_static_profiler_sections_variables)                                             \
                                                                                                                       \
    static void kan_cpu_static_sections_ensure_initialized (void)                                                      \
    {                                                                                                                  \
        if (!kan_static_profiler_sections_initialized)                                                                 \
        {                                                                                                              \
            CUSHION_STATEMENT_ACCUMULATOR (kan_static_profiler_sections_initialization)                                \
            kan_static_profiler_sections_initialized = true;                                                           \
        }                                                                                                              \
    }

/// \def KAN_CPU_SCOPED_SECTION
/// \brief Uses cpu profiler section at given path in current scope with current scope lifetime.

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_CPU_SCOPED_SECTION(PATH) /* No highlight-only content. */
#else
#    define KAN_CPU_SCOPED_SECTION(PATH)                                                                               \
        struct kan_cpu_section_execution_t cpu_profiler_execution_##__CUSHION_REPLACEMENT_INDEX__;                     \
        kan_cpu_section_execution_init (&cpu_profiler_execution_##__CUSHION_REPLACEMENT_INDEX__, PATH);                \
        CUSHION_DEFER { kan_cpu_section_execution_shutdown (&cpu_profiler_execution_##__CUSHION_REPLACEMENT_INDEX__); }
#endif

/// \brief Internal macro for properly registering static cpu profiler sections.
#define KAN_CPU_STATIC_SECTION_REGISTER_INTERNAL(NAME)                                                                 \
    CUSHION_STATEMENT_ACCUMULATOR_PUSH (kan_static_profiler_sections_variables, unique)                                \
    {                                                                                                                  \
        kan_cpu_section_t cpu_section_##NAME;                                                                          \
    }                                                                                                                  \
                                                                                                                       \
    CUSHION_STATEMENT_ACCUMULATOR_PUSH (kan_static_profiler_sections_initialization, unique)                           \
    {                                                                                                                  \
        cpu_section_##NAME = kan_cpu_section_get (#NAME);                                                              \
    }

/// \def KAN_CPU_STATIC_SECTION_GET
/// \brief Returns static cpu profiler section with given name.
/// \details To make sure that static cpu profiler sections are initialized, kan_cpu_static_sections_ensure_initialized
///          should be called. In order for static ids to be usable inside file, use need to paste
///          KAN_USE_STATIC_CPU_SECTIONS in global scope prior to using any static cpu profiler scope.

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_CPU_STATIC_SECTION_GET(NAME) KAN_HANDLE_SET_INVALID (kan_cpu_section_t)
#else
#    define KAN_CPU_STATIC_SECTION_GET(NAME)                                                                           \
        KAN_CPU_STATIC_SECTION_REGISTER_INTERNAL (NAME)                                                                \
        cpu_section_##NAME
#endif

/// \def KAN_CPU_SCOPED_STATIC_SECTION
/// \brief Uses static cpu profiler section with given name in current scope with current scope lifetime.
/// \details To make sure that static cpu profiler sections are initialized, kan_cpu_static_sections_ensure_initialized
///          should be called. In order for static ids to be usable inside file, use need to paste
///          KAN_USE_STATIC_CPU_SECTIONS in global scope prior to using any static cpu profiler scope.

#if defined(CMAKE_UNIT_FRAMEWORK_HIGHLIGHT)
#    define KAN_CPU_SCOPED_STATIC_SECTION(NAME) /* No highlight-only content. */
#else
#    define KAN_CPU_SCOPED_STATIC_SECTION(NAME)                                                                        \
        KAN_CPU_STATIC_SECTION_REGISTER_INTERNAL (NAME)                                                                \
        struct kan_cpu_section_execution_t cpu_profiler_execution_##__CUSHION_REPLACEMENT_INDEX__;                     \
        kan_cpu_section_execution_init (&cpu_profiler_execution_##__CUSHION_REPLACEMENT_INDEX__, cpu_section_##NAME);  \
        CUSHION_DEFER { kan_cpu_section_execution_shutdown (&cpu_profiler_execution_##__CUSHION_REPLACEMENT_INDEX__); }
#endif

KAN_C_HEADER_END
