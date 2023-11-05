#include <stddef.h>

#include <kan/cpu_profiler/markup.h>

#include <tracy/TracyC.h>

void kan_cpu_stage_separator (void)
{
    ___tracy_emit_frame_mark (NULL);
}

_Static_assert (sizeof (struct kan_cpu_section_t) >= sizeof (struct ___tracy_source_location_data),
                "Check that kan_cpu_section_t can hold tracy location data.");

void kan_cpu_section_init (struct kan_cpu_section_t *section, const char *name, uint32_t rgba_color)
{
    struct ___tracy_source_location_data *data = (struct ___tracy_source_location_data *) section;
    data->name = name;
    data->function = NULL;
    data->file = NULL;
    data->line = 0u;
    data->color = rgba_color;
}

void kan_cpu_section_destroy (struct kan_cpu_section_t *section)
{
}

_Static_assert (sizeof (struct kan_cpu_section_execution_t) >= sizeof (struct ___tracy_c_zone_context),
                "Check that kan_cpu_section_execution_t can hold tracy zone data.");

void kan_cpu_section_execution_init (struct kan_cpu_section_execution_t *execution, struct kan_cpu_section_t *section)
{
    *(struct ___tracy_c_zone_context *) execution =
        ___tracy_emit_zone_begin ((struct ___tracy_source_location_data *) section, 1);
}

void kan_cpu_section_execution_shutdown (struct kan_cpu_section_execution_t *execution)
{
    ___tracy_emit_zone_end (*(struct ___tracy_c_zone_context *) execution);
}
