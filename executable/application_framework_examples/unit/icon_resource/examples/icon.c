#include <kan/resource_pipeline/resource_pipeline.h>

#include <examples/icon.h>

KAN_REFLECTION_STRUCT_META (icon_t)
APPLICATION_FRAMEWORK_EXAMPLES_ICON_RESOURCE_API struct kan_resource_resource_type_meta_t icon_resource_type = {
    .root = KAN_TRUE,
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
