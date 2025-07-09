#include <section_patch_types_post.h>

void most_inner_type_post_init (struct most_inner_type_t *instance)
{
    instance->x = 0u;
    instance->y = 0u;
}

void middle_type_post_init (struct middle_type_t *instance)
{
    kan_dynamic_array_init (&instance->inner_structs, 0u, sizeof (struct most_inner_type_t),
                            alignof (struct most_inner_type_t), KAN_ALLOCATION_GROUP_IGNORE);

    kan_dynamic_array_init (&instance->enums, 0u, sizeof (enum enum_to_adapt_t), alignof (enum enum_to_adapt_t),
                            KAN_ALLOCATION_GROUP_IGNORE);
}

void middle_type_post_shutdown (struct middle_type_t *instance)
{
    kan_dynamic_array_shutdown (&instance->inner_structs);
    kan_dynamic_array_shutdown (&instance->enums);
}

void root_type_post_init (struct root_type_t *instance)
{
    kan_dynamic_array_init (&instance->middle_structs, 0u, sizeof (struct middle_type_t),
                            alignof (struct middle_type_t), KAN_ALLOCATION_GROUP_IGNORE);
    instance->data_after = 0u;
}

void root_type_post_shutdown (struct root_type_t *instance)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS (instance->middle_structs, struct middle_type_t)
    {
        middle_type_post_shutdown (value);
    }
}
