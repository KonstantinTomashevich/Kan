#include <kan/resource_pipeline/resource_pipeline.h>

#include <example_import/icon.h>

/// \meta reflection_struct_meta = "icon_t"
APPLICATION_FRAMEWORK_EXAMPLE_IMPORT_ICON_API struct kan_resource_pipeline_resource_type_meta_t icon_resource_type = {
    .root = KAN_TRUE,
    .compilation_output_type_name = NULL,
    .compile = NULL,
};

void icon_init (struct icon_t *icon)
{
    icon->width = 0u;
    icon->height = 0u;
    kan_dynamic_array_init (&icon->pixels, 0u, sizeof (rgba_pixel_t), _Alignof (rgba_pixel_t),
                            kan_allocation_group_stack_get ());
}

void icon_shutdown (struct icon_t *icon)
{
    kan_dynamic_array_shutdown (&icon->pixels);
}
