#include <kan/render_foundation/resource_texture.h>
#include <kan/resource_pipeline/meta.h>

KAN_REFLECTION_STRUCT_META (kan_resource_texture_data_t)
RESOURCE_RENDER_FOUNDATION_API struct kan_resource_type_meta_t kan_resource_texture_data_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

void kan_resource_texture_data_init (struct kan_resource_texture_data_t *instance)
{
    kan_dynamic_array_init (&instance->data, 0u, sizeof (uint8_t), alignof (int), kan_allocation_group_stack_get ());
}

void kan_resource_texture_data_shutdown (struct kan_resource_texture_data_t *instance)
{
    kan_dynamic_array_shutdown (&instance->data);
}

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_texture_format_item_t, data_per_mip)
RESOURCE_RENDER_FOUNDATION_API struct kan_resource_reference_meta_t
    kan_resource_texture_format_item_reference_data_per_mip = {
        .type_name = "kan_resource_texture_data_t",
        .flags = 0u,
};

void kan_resource_texture_format_item_init (struct kan_resource_texture_format_item_t *instance)
{
    instance->format = KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_R8_SRGB;
    kan_dynamic_array_init (&instance->data_per_mip, 0u, sizeof (kan_interned_string_t),
                            alignof (kan_interned_string_t), kan_allocation_group_stack_get ());
}

void kan_resource_texture_format_item_shutdown (struct kan_resource_texture_format_item_t *instance)
{
    kan_dynamic_array_shutdown (&instance->data_per_mip);
}

KAN_REFLECTION_STRUCT_META (kan_resource_texture_t)
RESOURCE_RENDER_FOUNDATION_API struct kan_resource_type_meta_t kan_resource_texture_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

void kan_resource_texture_init (struct kan_resource_texture_t *instance)
{
    instance->width = 1u;
    instance->height = 1u;
    instance->depth = 1u;
    instance->mips = 1u;

    kan_dynamic_array_init (&instance->formats, 0u, sizeof (struct kan_resource_texture_format_item_t),
                            alignof (struct kan_resource_texture_format_item_t), kan_allocation_group_stack_get ());
}

void kan_resource_texture_shutdown (struct kan_resource_texture_t *instance)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->formats, kan_resource_texture_format_item)
}
