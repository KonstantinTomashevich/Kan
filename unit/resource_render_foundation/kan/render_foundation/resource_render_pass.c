#include <kan/render_foundation/resource_render_pass.h>
#include <kan/resource_pipeline/meta.h>

void kan_resource_render_pass_variant_init (struct kan_resource_render_pass_variant_t *instance)
{
    instance->name = NULL;
    kan_rpl_meta_set_bindings_init (&instance->pass_set_bindings);
}

void kan_resource_render_pass_variant_shutdown (struct kan_resource_render_pass_variant_t *instance)
{
    kan_rpl_meta_set_bindings_shutdown (&instance->pass_set_bindings);
}

KAN_REFLECTION_STRUCT_META (kan_resource_render_pass_t)
RESOURCE_RENDER_FOUNDATION_API struct kan_resource_type_meta_t kan_resource_render_pass_resource_type = {
    .flags = KAN_RESOURCE_TYPE_ROOT,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

void kan_resource_render_pass_init (struct kan_resource_render_pass_t *instance)
{
    instance->type = KAN_RENDER_PASS_GRAPHICS;
    kan_dynamic_array_init (&instance->attachments, 0u, sizeof (struct kan_render_pass_attachment_t),
                            alignof (struct kan_render_pass_attachment_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->variants, 0u, sizeof (struct kan_resource_render_pass_variant_t),
                            alignof (struct kan_resource_render_pass_variant_t), kan_allocation_group_stack_get ());
}

void kan_resource_render_pass_shutdown (struct kan_resource_render_pass_t *instance)
{
    kan_dynamic_array_shutdown (&instance->attachments);
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->variants, kan_resource_render_pass_variant)
}
