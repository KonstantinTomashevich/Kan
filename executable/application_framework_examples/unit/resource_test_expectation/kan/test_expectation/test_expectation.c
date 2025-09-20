#include <kan/resource_pipeline/meta.h>
#include <kan/test_expectation/test_expectation.h>

KAN_REFLECTION_STRUCT_META (test_expectation_t)
RESOURCE_TEST_EXPECTATION_API struct kan_resource_type_meta_t test_expectation_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

void test_expectation_init (struct test_expectation_t *instance)
{
    instance->width = 0u;
    instance->height = 0u;
    kan_dynamic_array_init (&instance->rgba_data, 0u, sizeof (uint32_t), alignof (uint32_t),
                            kan_allocation_group_stack_get ());
}

void test_expectation_shutdown (struct test_expectation_t *instance)
{
    kan_dynamic_array_shutdown (&instance->rgba_data);
}
