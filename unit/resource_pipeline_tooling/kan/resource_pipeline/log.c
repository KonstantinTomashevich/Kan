#include <string.h>

#include <kan/resource_pipeline/log.h>

static kan_allocation_group_t allocation_group;
static bool statics_initialized = false;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "resource_pipeline_log");
        statics_initialized = true;
    }
}

kan_allocation_group_t kan_resource_log_get_allocation_group (void)
{
    ensure_statics_initialized ();
    return allocation_group;
}

void kan_resource_log_raw_entry_init (struct kan_resource_log_raw_entry_t *instance)
{
    instance->type = NULL;
    instance->name = NULL;
    instance->version.type_version = 0u;
    instance->version.last_access_modification_time = 0u;
    instance->saved_virtual_path = NULL;

    kan_dynamic_array_init (&instance->references, 0u, sizeof (struct kan_resource_log_reference_t),
                            alignof (struct kan_resource_log_reference_t), allocation_group);
}

void kan_resource_log_raw_entry_shutdown (struct kan_resource_log_raw_entry_t *instance)
{
    kan_dynamic_array_shutdown (&instance->references);
}

void kan_resource_log_built_entry_init (struct kan_resource_log_built_entry_t *instance)
{
    instance->type = NULL;
    instance->name = NULL;
    instance->platform_configuration_time = 0u;
    instance->primary_version.type_version = 0u;
    instance->primary_version.last_access_modification_time = 0u;
    instance->rule_version = 0u;
    instance->saved_virtual_path = NULL;

    kan_dynamic_array_init (&instance->references, 0u, sizeof (struct kan_resource_log_reference_t),
                            alignof (struct kan_resource_log_reference_t), allocation_group);

    kan_dynamic_array_init (&instance->secondary_inputs, 0u, sizeof (struct kan_resource_log_secondary_input_t),
                            alignof (struct kan_resource_log_secondary_input_t), allocation_group);
}

void kan_resource_log_built_entry_shutdown (struct kan_resource_log_built_entry_t *instance)
{
    kan_dynamic_array_shutdown (&instance->references);
    kan_dynamic_array_shutdown (&instance->secondary_inputs);
}

void kan_resource_log_secondary_entry_init (struct kan_resource_log_secondary_entry_t *instance)
{
    instance->type = NULL;
    instance->name = NULL;
    instance->version.type_version = 0u;
    instance->version.last_access_modification_time = 0u;
    instance->saved_virtual_path = NULL;
    instance->hash_if_mergeable = 0u;

    kan_dynamic_array_init (&instance->references, 0u, sizeof (struct kan_resource_log_reference_t),
                            alignof (struct kan_resource_log_reference_t), allocation_group);
}

void kan_resource_log_secondary_entry_shutdown (struct kan_resource_log_secondary_entry_t *instance)
{
    kan_dynamic_array_shutdown (&instance->references);
}

void kan_resource_log_target_init (struct kan_resource_log_target_t *instance)
{
    kan_dynamic_array_init (&instance->raw, 0u, sizeof (struct kan_resource_log_raw_entry_t),
                            alignof (struct kan_resource_log_raw_entry_t), allocation_group);

    kan_dynamic_array_init (&instance->built, 0u, sizeof (struct kan_resource_log_built_entry_t),
                            alignof (struct kan_resource_log_built_entry_t), allocation_group);

    kan_dynamic_array_init (&instance->secondary, 0u, sizeof (struct kan_resource_log_secondary_entry_t),
                            alignof (struct kan_resource_log_secondary_entry_t), allocation_group);
}

void kan_resource_log_target_shutdown (struct kan_resource_log_target_t *instance)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->raw, kan_resource_log_raw_entry)
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->built, kan_resource_log_built_entry)
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->secondary, kan_resource_log_secondary_entry)
}

void kan_resource_log_init (struct kan_resource_log_t *instance)
{
    kan_dynamic_array_init (&instance->targets, 0u, sizeof (struct kan_resource_log_target_t),
                            alignof (struct kan_resource_log_target_t), allocation_group);
}

void kan_resource_log_shutdown (struct kan_resource_log_t *instance)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->targets, kan_resource_log_target)
}
