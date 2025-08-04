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
    instance->version.last_modification_time = 0u;
    instance->deployed = false;

    kan_dynamic_array_init (&instance->references, 0u, sizeof (struct kan_resource_log_reference_t),
                            alignof (struct kan_resource_log_reference_t), allocation_group);
}

void kan_resource_log_raw_entry_init_copy (struct kan_resource_log_raw_entry_t *instance,
                                           const struct kan_resource_log_raw_entry_t *copy_from)
{
    instance->type = copy_from->type;
    instance->name = copy_from->name;
    instance->version = copy_from->version;
    instance->deployed = copy_from->deployed;

    kan_dynamic_array_init (&instance->references, copy_from->references.size,
                            sizeof (struct kan_resource_log_reference_t), alignof (struct kan_resource_log_reference_t),
                            allocation_group);

    instance->references.size = copy_from->references.size;
    memcpy (instance->references.data, copy_from->references.data,
            sizeof (struct kan_resource_log_reference_t) * instance->references.size);
}

void kan_resource_log_raw_entry_shutdown (struct kan_resource_log_raw_entry_t *instance)
{
    kan_dynamic_array_shutdown (&instance->references);
}

void kan_resource_log_built_entry_init (struct kan_resource_log_built_entry_t *instance)
{
    instance->type = NULL;
    instance->name = NULL;
    instance->version.type_version = 0u;
    instance->version.last_modification_time = 0u;
    instance->platform_configuration_time = 0u;
    instance->rule_version = 0u;
    instance->primary_input_version.type_version = 0u;
    instance->primary_input_version.last_modification_time = 0u;
    instance->saved_directory = KAN_RESOURCE_LOG_SAVED_DIRECTORY_CACHE;

    kan_dynamic_array_init (&instance->references, 0u, sizeof (struct kan_resource_log_reference_t),
                            alignof (struct kan_resource_log_reference_t), allocation_group);

    kan_dynamic_array_init (&instance->secondary_inputs, 0u, sizeof (struct kan_resource_log_secondary_input_t),
                            alignof (struct kan_resource_log_secondary_input_t), allocation_group);
}

void kan_resource_log_built_entry_init_copy (struct kan_resource_log_built_entry_t *instance,
                                             const struct kan_resource_log_built_entry_t *copy_from)
{
    instance->type = copy_from->type;
    instance->name = copy_from->name;
    instance->version = copy_from->version;
    instance->platform_configuration_time = copy_from->platform_configuration_time;
    instance->rule_version = copy_from->rule_version;
    instance->primary_input_version = copy_from->primary_input_version;
    instance->saved_directory = copy_from->saved_directory;

    kan_dynamic_array_init (&instance->references, copy_from->references.size,
                            sizeof (struct kan_resource_log_reference_t), alignof (struct kan_resource_log_reference_t),
                            allocation_group);

    instance->references.size = copy_from->references.size;
    memcpy (instance->references.data, copy_from->references.data,
            sizeof (struct kan_resource_log_reference_t) * instance->references.size);

    kan_dynamic_array_init (&instance->secondary_inputs, copy_from->secondary_inputs.size,
                            sizeof (struct kan_resource_log_secondary_input_t),
                            alignof (struct kan_resource_log_secondary_input_t), allocation_group);

    instance->secondary_inputs.size = copy_from->secondary_inputs.size;
    memcpy (instance->secondary_inputs.data, copy_from->secondary_inputs.data,
            sizeof (struct kan_resource_log_secondary_input_t) * instance->secondary_inputs.size);
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
    instance->version.last_modification_time = 0u;
    instance->saved_directory = KAN_RESOURCE_LOG_SAVED_DIRECTORY_DEPLOY;

    instance->producer_type = NULL;
    instance->producer_name = NULL;
    instance->producer_version.type_version = 0u;
    instance->producer_version.last_modification_time = 0u;

    kan_dynamic_array_init (&instance->references, 0u, sizeof (struct kan_resource_log_reference_t),
                            alignof (struct kan_resource_log_reference_t), allocation_group);
}

void kan_resource_log_secondary_entry_init_copy (struct kan_resource_log_secondary_entry_t *instance,
                                                 const struct kan_resource_log_secondary_entry_t *copy_from)
{
    instance->type = copy_from->type;
    instance->name = copy_from->name;
    instance->version = copy_from->version;
    instance->saved_directory = copy_from->saved_directory;

    instance->producer_type = copy_from->producer_type;
    instance->producer_name = copy_from->producer_name;
    instance->producer_version = copy_from->producer_version;

    kan_dynamic_array_init (&instance->references, copy_from->references.size,
                            sizeof (struct kan_resource_log_reference_t), alignof (struct kan_resource_log_reference_t),
                            allocation_group);

    instance->references.size = copy_from->references.size;
    memcpy (instance->references.data, copy_from->references.data,
            sizeof (struct kan_resource_log_reference_t) * instance->references.size);
}

void kan_resource_log_secondary_entry_shutdown (struct kan_resource_log_secondary_entry_t *instance)
{
    kan_dynamic_array_shutdown (&instance->references);
}

void kan_resource_log_target_init (struct kan_resource_log_target_t *instance)
{
    instance->name = NULL;
    kan_dynamic_array_init (&instance->raw, 0u, sizeof (struct kan_resource_log_raw_entry_t),
                            alignof (struct kan_resource_log_raw_entry_t), allocation_group);

    kan_dynamic_array_init (&instance->built, 0u, sizeof (struct kan_resource_log_built_entry_t),
                            alignof (struct kan_resource_log_built_entry_t), allocation_group);

    kan_dynamic_array_init (&instance->secondary, 0u, sizeof (struct kan_resource_log_secondary_entry_t),
                            alignof (struct kan_resource_log_secondary_entry_t), allocation_group);
}

void kan_resource_log_target_init_copy (struct kan_resource_log_target_t *instance,
                                        const struct kan_resource_log_target_t *copy_from)
{
    instance->name = copy_from->name;
    kan_dynamic_array_init (&instance->raw, copy_from->raw.size, sizeof (struct kan_resource_log_raw_entry_t),
                            alignof (struct kan_resource_log_raw_entry_t), allocation_group);

    for (kan_loop_size_t index = 0u; index < copy_from->raw.size; ++index)
    {
        const struct kan_resource_log_raw_entry_t *input =
            &((struct kan_resource_log_raw_entry_t *) copy_from->raw.data)[index];
        struct kan_resource_log_raw_entry_t *output = kan_dynamic_array_add_last (&instance->raw);
        kan_resource_log_raw_entry_init_copy (output, input);
    }

    kan_dynamic_array_init (&instance->built, copy_from->built.size, sizeof (struct kan_resource_log_built_entry_t),
                            alignof (struct kan_resource_log_built_entry_t), allocation_group);

    for (kan_loop_size_t index = 0u; index < copy_from->built.size; ++index)
    {
        const struct kan_resource_log_built_entry_t *input =
            &((struct kan_resource_log_built_entry_t *) copy_from->built.data)[index];
        struct kan_resource_log_built_entry_t *output = kan_dynamic_array_add_last (&instance->built);
        kan_resource_log_built_entry_init_copy (output, input);
    }

    kan_dynamic_array_init (&instance->secondary, copy_from->secondary.size,
                            sizeof (struct kan_resource_log_secondary_entry_t),
                            alignof (struct kan_resource_log_secondary_entry_t), allocation_group);

    for (kan_loop_size_t index = 0u; index < copy_from->secondary.size; ++index)
    {
        const struct kan_resource_log_secondary_entry_t *input =
            &((struct kan_resource_log_secondary_entry_t *) copy_from->secondary.data)[index];
        struct kan_resource_log_secondary_entry_t *output = kan_dynamic_array_add_last (&instance->secondary);
        kan_resource_log_secondary_entry_init_copy (output, input);
    }
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
