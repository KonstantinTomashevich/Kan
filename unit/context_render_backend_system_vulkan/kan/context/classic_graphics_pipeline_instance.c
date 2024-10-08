#include <kan/context/render_backend_implementation_interface.h>

kan_render_classic_graphics_pipeline_instance_t kan_render_classic_graphics_pipeline_instance_create (
    kan_render_context_t context,
    kan_render_classic_graphics_pipeline_t pipeline,
    uint64_t initial_bindings_count,
    struct kan_render_layout_update_description_t *initial_bindings,
    kan_interned_string_t tracking_name)
{
    // TODO: Implement.
    return KAN_INVALID_RENDER_CLASSIC_GRAPHICS_PIPELINE_INSTANCE;
}

void kan_render_classic_graphics_pipeline_instance_update_layout (
    kan_render_classic_graphics_pipeline_instance_t instance,
    uint64_t bindings_count,
    struct kan_render_layout_update_description_t *bindings)
{
    // TODO: Implement.
}

CONTEXT_RENDER_BACKEND_SYSTEM_API void kan_render_classic_graphics_pipeline_instance_destroy (
    kan_render_classic_graphics_pipeline_instance_t instance)
{
    // TODO: Implement.
}
