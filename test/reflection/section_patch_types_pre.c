#include <section_patch_types_pre.h>

void most_inner_type_pre_init (struct most_inner_type_t *instance)
{
    instance->x = 0u;
    instance->y = 0u;
    instance->z = 0u;
}

void middle_type_pre_init (struct middle_type_t *instance)
{
    kan_dynamic_array_init (&instance->inner_structs, 0u, sizeof (struct most_inner_type_t),
                            alignof (struct most_inner_type_t), KAN_ALLOCATION_GROUP_IGNORE);

    kan_dynamic_array_init (&instance->structs_to_delete, 0u, sizeof (struct type_to_delete_t),
                            alignof (struct type_to_delete_t), KAN_ALLOCATION_GROUP_IGNORE);

    kan_dynamic_array_init (&instance->enums, 0u, sizeof (enum enum_to_adapt_t), alignof (enum enum_to_adapt_t),
                            KAN_ALLOCATION_GROUP_IGNORE);
}

void middle_type_pre_shutdown (struct middle_type_t *instance)
{
    kan_dynamic_array_shutdown (&instance->inner_structs);
    kan_dynamic_array_shutdown (&instance->structs_to_delete);
    kan_dynamic_array_shutdown (&instance->enums);
}

void root_type_pre_init (struct root_type_t *instance)
{
    instance->data_before = 0u;
    kan_dynamic_array_init (&instance->middle_structs, 0u, sizeof (struct middle_type_t),
                            alignof (struct middle_type_t), KAN_ALLOCATION_GROUP_IGNORE);
    instance->data_after = 0u;
}

void root_type_pre_shutdown (struct root_type_t *instance)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS (instance->middle_structs, struct middle_type_t)
    {
        middle_type_pre_shutdown (value);
    }
}
