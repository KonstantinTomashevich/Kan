#include <kan/application_framework_resource_builder/target_byproduct_state.h>

void kan_resource_target_byproduct_state_init (struct kan_resource_target_byproduct_state_t *instance)
{
    kan_dynamic_array_init (
        &instance->production, 0u, sizeof (struct kan_resource_target_byproduct_production_t),
        alignof (struct kan_resource_target_byproduct_production_t),
        kan_allocation_group_get_child (kan_allocation_group_root (), "resource_target_byproduct_state"));
}

APPLICATION_FRAMEWORK_RESOURCE_BUILDER_API void kan_resource_target_byproduct_state_shutdown (
    struct kan_resource_target_byproduct_state_t *instance)
{
    kan_dynamic_array_shutdown (&instance->production);
}
