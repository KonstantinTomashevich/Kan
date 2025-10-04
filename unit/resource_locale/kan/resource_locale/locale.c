#include <kan/resource_locale/locale.h>
#include <kan/resource_pipeline/meta.h>

KAN_REFLECTION_STRUCT_META (kan_resource_locale_t)
RESOURCE_LOCALE_API struct kan_resource_type_meta_t kan_resource_locale_resource_type = {
    .flags = KAN_RESOURCE_TYPE_ROOT,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_locale_t, font_language_filter)
RESOURCE_LOCALE_API struct kan_resource_reference_meta_t kan_resource_locale_field_font_language_filter_reference_meta =
    {
        .type_name = "kan_resource_language_t",
        .flags = 0u,
};

void kan_resource_locale_init (struct kan_resource_locale_t *instance)
{
    instance->preferred_orientation = KAN_LOCALE_PREFERRED_ORIENTATION_HORIZONTAL;
    kan_dynamic_array_init (&instance->font_language_filter, 0u, sizeof (kan_interned_string_t),
                            alignof (kan_interned_string_t), kan_allocation_group_stack_get ());
}

void kan_resource_locale_shutdown (struct kan_resource_locale_t *instance)
{
    kan_dynamic_array_shutdown (&instance->font_language_filter);
}

KAN_REFLECTION_STRUCT_META (kan_resource_language_t)
RESOURCE_LOCALE_API struct kan_resource_type_meta_t kan_resource_language_resource_type = {
    .flags = KAN_RESOURCE_TYPE_ROOT,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

void kan_resource_language_init (struct kan_resource_language_t *instance)
{
    instance->unicode_language_id = NULL;
    instance->unicode_script_id = NULL;
    instance->horizontal_direction = KAN_LANGUAGE_HORIZONTAL_DIRECTION_LEFT_TO_RIGHT;
    instance->vertical_direction = KAN_LANGUAGE_VERTICAL_DIRECTION_TOP_TO_BOTTOM;
}
