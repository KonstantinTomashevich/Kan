#include <kan/memory/allocation.h>
#include <kan/resource_pipeline/platform_configuration.h>

static kan_allocation_group_t allocation_group;
static bool statics_initialized = false;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "resource_pipeline_platform_configuration");
        statics_initialized = true;
    }
}

kan_allocation_group_t kan_resource_platform_configuration_get_allocation_group (void)
{
    ensure_statics_initialized ();
    return allocation_group;
}

void kan_resource_platform_configuration_setup_init (struct kan_resource_platform_configuration_setup_t *instance)
{
    ensure_statics_initialized ();
    kan_dynamic_array_init (&instance->layers, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            allocation_group);
}

void kan_resource_platform_configuration_setup_shutdown (struct kan_resource_platform_configuration_setup_t *instance)
{
    kan_dynamic_array_shutdown (&instance->layers);
}

void kan_resource_platform_configuration_entry_init (struct kan_resource_platform_configuration_entry_t *instance)
{
    kan_dynamic_array_init (&instance->required_tags, 0u, sizeof (kan_interned_string_t),
                            alignof (kan_interned_string_t), allocation_group);
    instance->layer = NULL;
    instance->data = KAN_HANDLE_SET_INVALID (kan_reflection_patch_t);
}

void kan_resource_platform_configuration_entry_shutdown (struct kan_resource_platform_configuration_entry_t *instance)
{
    kan_dynamic_array_shutdown (&instance->required_tags);
    if (KAN_HANDLE_IS_VALID (instance->data))
    {
        kan_reflection_patch_destroy (instance->data);
    }
}
