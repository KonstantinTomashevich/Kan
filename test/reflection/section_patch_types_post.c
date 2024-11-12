#include <section_patch_types_post.h>

void most_inner_type_post_init (struct most_inner_type_post_t *instance)
{
    instance->x = 0u;
    instance->y = 0u;
}

void middle_type_post_init (struct middle_type_post_t *instance)
{
    kan_dynamic_array_init (&instance->inner_structs, 0u, sizeof (struct most_inner_type_post_t),
                            _Alignof (struct most_inner_type_post_t), KAN_ALLOCATION_GROUP_IGNORE);

    kan_dynamic_array_init (&instance->enums, 0u, sizeof (enum enum_to_adapt_post_t),
                            _Alignof (enum enum_to_adapt_post_t), KAN_ALLOCATION_GROUP_IGNORE);
}

void middle_type_post_shutdown (struct middle_type_post_t *instance)
{
    kan_dynamic_array_shutdown (&instance->inner_structs);
    kan_dynamic_array_shutdown (&instance->enums);
}

void root_type_post_init (struct root_type_post_t *instance)
{
    kan_dynamic_array_init (&instance->middle_structs, 0u, sizeof (struct middle_type_post_t),
                            _Alignof (struct middle_type_post_t), KAN_ALLOCATION_GROUP_IGNORE);
    instance->data_after = 0u;
}

void root_type_post_shutdown (struct root_type_post_t *instance)
{
    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) instance->middle_structs.size; ++index)
    {
        middle_type_post_shutdown (&((struct middle_type_post_t *) instance->middle_structs.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->middle_structs);
}
