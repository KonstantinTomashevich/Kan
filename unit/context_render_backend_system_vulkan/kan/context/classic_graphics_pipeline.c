#include <kan/context/render_backend_implementation_interface.h>

kan_render_classic_graphics_pipeline_t kan_render_classic_graphics_pipeline_create (
    kan_render_context_t context,
    struct kan_render_classic_graphics_pipeline_description_t *description,
    enum kan_render_pipeline_compilation_priority_t compilation_priority)
{
    // TODO: Implement.
    return KAN_INVALID_RENDER_CLASSIC_GRAPHICS_PIPELINE;
}

void kan_render_classic_graphics_pipeline_change_compilation_priority (
    kan_render_classic_graphics_pipeline_t pipeline,
    enum kan_render_pipeline_compilation_priority_t compilation_priority)
{
    // TODO: Implement.
}

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_classic_graphics_pipeline_destroy (
    kan_render_classic_graphics_pipeline_t pipeline)
{
    // TODO: Implement.
}
