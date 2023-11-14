#include <kan/cpu_profiler/markup.h>

void kan_cpu_stage_separator (void)
{
}

kan_cpu_section_t kan_cpu_section_get (const char *name)
{
    return 0u;
}

void kan_cpu_section_set_color (kan_cpu_section_t section, uint32_t rgba_color)
{
}

void kan_cpu_section_execution_init (struct kan_cpu_section_execution_t *execution, kan_cpu_section_t section)
{
}

void kan_cpu_section_execution_shutdown (struct kan_cpu_section_execution_t *execution)
{
}
