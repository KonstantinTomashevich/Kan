#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <string.h>

#include <kan/memory/allocation.h>
#include <kan/resource_pipeline/meta.h>
#include <kan/resource_text/font.h>

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_font_style_t, font_data_file)
RESOURCE_TEXT_API struct kan_resource_reference_meta_t kan_resource_font_style_reference_font_data_file = {
    .type_name = NULL,
    .flags = 0u,
};

void kan_resource_font_style_init (struct kan_resource_font_style_t *instance)
{
    instance->style = NULL;
    instance->font_data_file = NULL;
    kan_dynamic_array_init (&instance->variable_font_axes, 0u, sizeof (float), alignof (float),
                            kan_allocation_group_stack_get ());
}

void kan_resource_font_style_shutdown (struct kan_resource_font_style_t *instance)
{
    kan_dynamic_array_shutdown (&instance->variable_font_axes);
}

void kan_resource_font_category_init (struct kan_resource_font_category_t *instance)
{
    instance->script = NULL;
    kan_dynamic_array_init (&instance->used_for_languages, 0u, sizeof (kan_interned_string_t),
                            alignof (kan_interned_string_t), kan_allocation_group_stack_get ());
    instance->precache_utf8_horizontal = NULL;
    instance->precache_utf8_vertical = NULL;
    kan_dynamic_array_init (&instance->styles, 0u, sizeof (struct kan_resource_font_style_t),
                            alignof (struct kan_resource_font_style_t), kan_allocation_group_stack_get ());
}

void kan_resource_font_category_shutdown (struct kan_resource_font_category_t *instance)
{
    if (instance->precache_utf8_horizontal)
    {
        kan_instance_size_t length = strlen (instance->precache_utf8_horizontal);
        kan_free_general (instance->used_for_languages.allocation_group, instance->precache_utf8_horizontal,
                          length + 1u);
    }

    if (instance->precache_utf8_vertical)
    {
        kan_instance_size_t length = strlen (instance->precache_utf8_vertical);
        kan_free_general (instance->used_for_languages.allocation_group, instance->precache_utf8_vertical, length + 1u);
    }

    kan_dynamic_array_shutdown (&instance->used_for_languages);
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->styles, kan_resource_font_style);
}

KAN_REFLECTION_STRUCT_META (kan_resource_font_library_t)
RESOURCE_TEXT_API struct kan_resource_type_meta_t kan_resource_font_library_resource_type = {
    .flags = KAN_RESOURCE_TYPE_ROOT,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

void kan_resource_font_library_init (struct kan_resource_font_library_t *instance)
{
    instance->usage_class = NULL;
    kan_dynamic_array_init (&instance->categories, 0u, sizeof (struct kan_resource_font_category_t),
                            alignof (struct kan_resource_font_category_t), kan_allocation_group_stack_get ());
}

void kan_resource_font_library_shutdown (struct kan_resource_font_library_t *instance)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->categories, kan_resource_font_category);
}
