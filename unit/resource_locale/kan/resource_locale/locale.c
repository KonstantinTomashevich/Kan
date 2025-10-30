#include <kan/resource_locale/locale.h>
#include <kan/resource_pipeline/meta.h>

KAN_REFLECTION_STRUCT_META (kan_resource_locale_t)
RESOURCE_LOCALE_API struct kan_resource_type_meta_t kan_resource_locale_resource_type = {
    .flags = KAN_RESOURCE_TYPE_ROOT,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

void kan_resource_locale_init (struct kan_resource_locale_t *instance)
{
    instance->preferred_direction= KAN_LOCALE_PREFERRED_TEXT_DIRECTION_LEFT_TO_RIGHT;
    kan_dynamic_array_init (&instance->font_languages, 0u, sizeof (kan_interned_string_t),
                            alignof (kan_interned_string_t), kan_allocation_group_stack_get ());
}

void kan_resource_locale_shutdown (struct kan_resource_locale_t *instance)
{
    kan_dynamic_array_shutdown (&instance->font_languages);
}
