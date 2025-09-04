#include <kan/render_foundation/resource_material_instance.h>
#include <kan/render_foundation/resource_texture.h>
#include <kan/resource_pipeline/meta.h>

void kan_resource_buffer_binding_init (struct kan_resource_buffer_binding_t *instance)
{
    instance->binding = 0u;
    kan_dynamic_array_init (&instance->data, 0u, 1u, KAN_RESOURCE_RENDER_FOUNDATION_BUFFER_ALIGNMENT,
                            kan_allocation_group_stack_get ());
}

void kan_resource_buffer_binding_shutdown (struct kan_resource_buffer_binding_t *instance)
{
    kan_dynamic_array_shutdown (&instance->data);
}

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_image_binding_t, texture)
RESOURCE_RENDER_FOUNDATION_API struct kan_resource_reference_meta_t kan_resource_image_binding_reference_texture = {
    .type_name = "kan_resource_texture_t",
    .flags = 0u,
};

void kan_resource_material_variant_init (struct kan_resource_material_variant_t *instance)
{
    instance->name = 0u;
    kan_dynamic_array_init (&instance->instanced_data, 0u, 1u, KAN_RESOURCE_RENDER_FOUNDATION_BUFFER_ALIGNMENT,
                            kan_allocation_group_stack_get ());
}

void kan_resource_material_variant_shutdown (struct kan_resource_material_variant_t *instance)
{
    kan_dynamic_array_shutdown (&instance->instanced_data);
}

KAN_REFLECTION_STRUCT_META (kan_resource_material_instance_t)
RESOURCE_RENDER_FOUNDATION_API struct kan_resource_type_meta_t kan_resource_material_instance_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_instance_t, material)
RESOURCE_RENDER_FOUNDATION_API struct kan_resource_reference_meta_t kan_resource_material_instance_reference_material =
    {
        .type_name = "kan_resource_material_t",
        .flags = 0u,
};

void kan_resource_material_instance_init (struct kan_resource_material_instance_t *instance)
{
    instance->material = NULL;
    kan_dynamic_array_init (&instance->buffers, 0u, sizeof (struct kan_resource_buffer_binding_t),
                            alignof (struct kan_resource_buffer_binding_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->samplers, 0u, sizeof (struct kan_resource_sampler_binding_t),
                            alignof (struct kan_resource_sampler_binding_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->images, 0u, sizeof (struct kan_resource_image_binding_t),
                            alignof (struct kan_resource_image_binding_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->variants, 0u, sizeof (struct kan_resource_material_variant_t),
                            alignof (struct kan_resource_material_variant_t), kan_allocation_group_stack_get ());
}

void kan_resource_material_instance_shutdown (struct kan_resource_material_instance_t *instance)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->buffers, kan_resource_buffer_binding)
    kan_dynamic_array_shutdown (&instance->samplers);
    kan_dynamic_array_shutdown (&instance->images);
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->variants, kan_resource_material_variant)
}
