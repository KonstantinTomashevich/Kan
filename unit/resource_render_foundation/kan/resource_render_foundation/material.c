#include <kan/resource_pipeline/meta.h>
#include <kan/resource_render_foundation/material.h>

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_pipeline_t, pass_name)
RESOURCE_RENDER_FOUNDATION_API struct kan_resource_reference_meta_t kan_resource_material_pipeline_reference_pass_name =
    {
        .type_name = "kan_resource_render_pass_t",
        .flags = 0u,
};

void kan_resource_material_pipeline_init (struct kan_resource_material_pipeline_t *instance)
{
    instance->pass_name = NULL;
    instance->variant_name = NULL;

    kan_dynamic_array_init (&instance->entry_points, 0u, sizeof (struct kan_rpl_entry_point_t),
                            alignof (struct kan_rpl_entry_point_t), kan_allocation_group_stack_get ());

    instance->code_format = KAN_RENDER_CODE_FORMAT_SPIRV;
    instance->pipeline_settings = kan_rpl_graphics_classic_pipeline_settings_default ();

    kan_dynamic_array_init (&instance->color_outputs, 0u, sizeof (struct kan_rpl_meta_color_output_t),
                            alignof (struct kan_rpl_meta_color_output_t), kan_allocation_group_stack_get ());

    instance->color_blend_constants = (struct kan_rpl_color_blend_constants_t) {
        .r = 0.0f,
        .g = 0.0f,
        .b = 0.0f,
        .a = 0.0f,
    };

    kan_dynamic_array_init (&instance->code, 0u, sizeof (uint8_t), alignof (uint32_t),
                            kan_allocation_group_stack_get ());
}

void kan_resource_material_pipeline_shutdown (struct kan_resource_material_pipeline_t *instance)
{
    kan_dynamic_array_shutdown (&instance->entry_points);
    kan_dynamic_array_shutdown (&instance->color_outputs);
    kan_dynamic_array_shutdown (&instance->code);
}

KAN_REFLECTION_STRUCT_META (kan_resource_material_t)
RESOURCE_RENDER_FOUNDATION_API struct kan_resource_type_meta_t kan_resource_material_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

void kan_resource_material_init (struct kan_resource_material_t *instance)
{
    kan_dynamic_array_init (&instance->vertex_attribute_sources, 0u, sizeof (struct kan_rpl_meta_attribute_source_t),
                            alignof (struct kan_rpl_meta_attribute_source_t), kan_allocation_group_stack_get ());

    instance->has_instanced_attribute_source = false;
    kan_rpl_meta_attribute_source_init (&instance->instanced_attribute_source);

    instance->push_constant_size = 0u;
    kan_rpl_meta_set_bindings_init (&instance->set_material);
    kan_rpl_meta_set_bindings_init (&instance->set_object);
    kan_rpl_meta_set_bindings_init (&instance->set_shared);

    kan_dynamic_array_init (&instance->pipelines, 0u, sizeof (struct kan_resource_material_pipeline_t),
                            alignof (struct kan_resource_material_pipeline_t), kan_allocation_group_stack_get ());
}

void kan_resource_material_shutdown (struct kan_resource_material_t *instance)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->vertex_attribute_sources, kan_rpl_meta_attribute_source)
    kan_rpl_meta_attribute_source_shutdown (&instance->instanced_attribute_source);
    kan_rpl_meta_set_bindings_shutdown (&instance->set_material);
    kan_rpl_meta_set_bindings_shutdown (&instance->set_object);
    kan_rpl_meta_set_bindings_shutdown (&instance->set_shared);
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->pipelines, kan_resource_material_pipeline)
}
