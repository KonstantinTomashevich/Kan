#include <string.h>

#include <kan/application_framework/application_framework.h>
#include <kan/context/application_framework_system.h>
#include <kan/context/application_system.h>
#include <kan/context/plugin_system.h>
#include <kan/context/reflection_system.h>
#include <kan/context/universe_system.h>
#include <kan/context/universe_world_definition_system.h>
#include <kan/context/update_system.h>
#include <kan/context/virtual_file_system.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/file_system/stream.h>
#include <kan/log/logging.h>
#include <kan/platform/application.h>
#include <kan/platform/precise_time.h>
#include <kan/reflection/generated_reflection.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>

KAN_LOG_DEFINE_CATEGORY (application_framework);

#define UNIVERSE_WORLD_DEFINITIONS_MOUNT_PATH "universe_world_definitions"

static kan_bool_t statics_initialized = KAN_FALSE;
static kan_allocation_group_t config_allocation_group;
static kan_allocation_group_t context_allocation_group;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        config_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "application_framework_config");
        context_allocation_group =
            kan_allocation_group_get_child (kan_allocation_group_root (), "application_framework_context");
        statics_initialized = KAN_TRUE;
    }
}

kan_allocation_group_t kan_application_framework_get_configuration_allocation_group (void)
{
    ensure_statics_initialized ();
    return config_allocation_group;
}

void kan_application_framework_resource_directory_init (struct kan_application_framework_resource_directory_t *instance)
{
    instance->path = NULL;
    instance->mount_path = NULL;
}

void kan_application_framework_resource_directory_shutdown (
    struct kan_application_framework_resource_directory_t *instance)
{
    if (instance->path)
    {
        kan_free_general (config_allocation_group, instance->path, strlen (instance->path) + 1u);
    }

    if (instance->mount_path)
    {
        kan_free_general (config_allocation_group, instance->mount_path, strlen (instance->mount_path) + 1u);
    }
}

void kan_application_framework_resource_pack_init (struct kan_application_framework_resource_pack_t *instance)
{
    instance->path = NULL;
    instance->mount_path = NULL;
}

void kan_application_framework_resource_pack_shutdown (struct kan_application_framework_resource_pack_t *instance)
{
    if (instance->path)
    {
        kan_free_general (config_allocation_group, instance->path, strlen (instance->path) + 1u);
    }

    if (instance->mount_path)
    {
        kan_free_general (config_allocation_group, instance->mount_path, strlen (instance->mount_path) + 1u);
    }
}

void kan_application_framework_core_configuration_init (struct kan_application_framework_core_configuration_t *instance)
{
    ensure_statics_initialized ();
    kan_dynamic_array_init (&instance->systems, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            config_allocation_group);
    kan_dynamic_array_init (&instance->plugins, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            config_allocation_group);
    kan_dynamic_array_init (&instance->resource_directories, 0u,
                            sizeof (struct kan_application_framework_resource_directory_t),
                            _Alignof (struct kan_application_framework_resource_directory_t), config_allocation_group);
    kan_dynamic_array_init (&instance->resource_packs, 0u, sizeof (struct kan_application_framework_resource_pack_t),
                            _Alignof (struct kan_application_framework_resource_pack_t), config_allocation_group);
    kan_dynamic_array_init (&instance->environment_tags, 0u, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), config_allocation_group);
    instance->root_world = NULL;
    instance->plugin_directory_path = NULL;
    instance->world_directory_path = NULL;
    instance->observe_world_definitions = KAN_FALSE;
    instance->world_definition_rescan_delay_ns = 100000000u;
    instance->enable_code_hot_reload = KAN_FALSE;
    instance->code_hot_reload_delay_ns = 200000000u;
    instance->auto_build_and_hot_reload_command = NULL;
}

void kan_application_framework_core_configuration_shutdown (
    struct kan_application_framework_core_configuration_t *instance)
{
    kan_dynamic_array_shutdown (&instance->systems);
    kan_dynamic_array_shutdown (&instance->plugins);

    for (uint64_t index = 0u; index < instance->resource_directories.size; ++index)
    {
        kan_application_framework_resource_directory_shutdown (
            &((struct kan_application_framework_resource_directory_t *) instance->resource_directories.data)[index]);
    }

    for (uint64_t index = 0u; index < instance->resource_packs.size; ++index)
    {
        kan_application_framework_resource_pack_shutdown (
            &((struct kan_application_framework_resource_pack_t *) instance->resource_packs.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->resource_directories);
    kan_dynamic_array_shutdown (&instance->resource_packs);
    kan_dynamic_array_shutdown (&instance->environment_tags);

    if (instance->plugin_directory_path)
    {
        kan_free_general (config_allocation_group, instance->plugin_directory_path,
                          strlen (instance->plugin_directory_path) + 1u);
    }

    if (instance->world_directory_path)
    {
        kan_free_general (config_allocation_group, instance->world_directory_path,
                          strlen (instance->world_directory_path) + 1u);
    }

    if (instance->auto_build_and_hot_reload_command)
    {
        kan_free_general (config_allocation_group, instance->auto_build_and_hot_reload_command,
                          strlen (instance->auto_build_and_hot_reload_command) + 1u);
    }
}

void kan_application_framework_program_configuration_init (
    struct kan_application_framework_program_configuration_t *instance)
{
    ensure_statics_initialized ();
    kan_dynamic_array_init (&instance->plugins, 0u, sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t),
                            config_allocation_group);
    kan_dynamic_array_init (&instance->resource_directories, 0u,
                            sizeof (struct kan_application_framework_resource_directory_t),
                            _Alignof (struct kan_application_framework_resource_directory_t), config_allocation_group);
    kan_dynamic_array_init (&instance->resource_packs, 0u, sizeof (struct kan_application_framework_resource_pack_t),
                            _Alignof (struct kan_application_framework_resource_pack_t), config_allocation_group);
    instance->program_world = NULL;
}

void kan_application_framework_program_configuration_shutdown (
    struct kan_application_framework_program_configuration_t *instance)
{
    kan_dynamic_array_shutdown (&instance->plugins);
    for (uint64_t index = 0u; index < instance->resource_directories.size; ++index)
    {
        kan_application_framework_resource_directory_shutdown (
            &((struct kan_application_framework_resource_directory_t *) instance->resource_directories.data)[index]);
    }

    for (uint64_t index = 0u; index < instance->resource_packs.size; ++index)
    {
        kan_application_framework_resource_pack_shutdown (
            &((struct kan_application_framework_resource_pack_t *) instance->resource_packs.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->resource_directories);
    kan_dynamic_array_shutdown (&instance->resource_packs);
}

KAN_REFLECTION_EXPECT_UNIT_REGISTRAR_LOCAL (application_framework);

static inline kan_bool_t is_path_to_binary (const char *path)
{
    const uint64_t length = strlen (path);
    return length > 4u && path[length - 4u] == '.' && path[length - 3u] == 'b' && path[length - 2u] == 'i' &&
           path[length - 1u] == 'n';
}

int kan_application_framework_run (const char *core_configuration_path,
                                   const char *program_configuration_path,
                                   uint64_t arguments_count,
                                   char **arguments)
{
    int result = 0;
    ensure_statics_initialized ();
    struct kan_application_framework_core_configuration_t core_config;
    kan_application_framework_core_configuration_init (&core_config);

    struct kan_application_framework_program_configuration_t program_config;
    kan_application_framework_program_configuration_init (&program_config);

    kan_reflection_registry_t temporary_registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (application_framework) (temporary_registry);
    kan_serialization_binary_script_storage_t temporary_script_storage =
        kan_serialization_binary_script_storage_create (temporary_registry);

    struct kan_stream_t *configuration_stream =
        kan_direct_file_stream_open_for_read (core_configuration_path, KAN_TRUE);

    if (configuration_stream)
    {
        configuration_stream = kan_random_access_stream_buffer_open_for_read (
            configuration_stream, KAN_APPLICATION_FRAMEWORK_CONFIGURATION_BUFFER);

        if (is_path_to_binary (core_configuration_path))
        {
            KAN_LOG (application_framework, KAN_LOG_INFO, "Reading core configuration from \"%s\" as binary.",
                     core_configuration_path)

            kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
                configuration_stream, &core_config,
                kan_string_intern ("kan_application_framework_core_configuration_t"), temporary_script_storage,
                KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t), config_allocation_group);

            enum kan_serialization_state_t state;
            while ((state = kan_serialization_binary_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
            {
            }

            if (state == KAN_SERIALIZATION_FAILED)
            {
                KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to read core configuration from \"%s\".",
                         core_configuration_path)
                result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_READ_CONFIGURATION;
            }

            kan_serialization_binary_reader_destroy (reader);
        }
        else
        {
            KAN_LOG (application_framework, KAN_LOG_INFO, "Reading core configuration from \"%s\" as readable data.",
                     core_configuration_path)

            kan_serialization_rd_reader_t reader = kan_serialization_rd_reader_create (
                configuration_stream, &core_config,
                kan_string_intern ("kan_application_framework_core_configuration_t"), temporary_registry,
                config_allocation_group);

            enum kan_serialization_state_t state;
            while ((state = kan_serialization_rd_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
            {
            }

            if (state == KAN_SERIALIZATION_FAILED)
            {
                KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to read core configuration from \"%s\".",
                         core_configuration_path)
                result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_READ_CONFIGURATION;
            }

            kan_serialization_rd_reader_destroy (reader);
        }

        configuration_stream->operations->close (configuration_stream);
    }
    else
    {
        KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to open configuration file \"%s\".",
                 core_configuration_path)
        result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_READ_CONFIGURATION;
    }

    configuration_stream = kan_direct_file_stream_open_for_read (program_configuration_path, KAN_TRUE);
    if (configuration_stream)
    {
        configuration_stream = kan_random_access_stream_buffer_open_for_read (
            configuration_stream, KAN_APPLICATION_FRAMEWORK_CONFIGURATION_BUFFER);

        if (is_path_to_binary (program_configuration_path))
        {
            KAN_LOG (application_framework, KAN_LOG_INFO, "Reading program configuration from \"%s\" as binary.",
                     program_configuration_path)

            kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
                configuration_stream, &program_config,
                kan_string_intern ("kan_application_framework_program_configuration_t"), temporary_script_storage,
                KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t), config_allocation_group);

            enum kan_serialization_state_t state;
            while ((state = kan_serialization_binary_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
            {
            }

            if (state == KAN_SERIALIZATION_FAILED)
            {
                KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to read program configuration from \"%s\".",
                         program_configuration_path)
                result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_READ_CONFIGURATION;
            }

            kan_serialization_binary_reader_destroy (reader);
        }
        else
        {
            KAN_LOG (application_framework, KAN_LOG_INFO, "Reading program configuration from \"%s\" as readable data.",
                     program_configuration_path)

            kan_serialization_rd_reader_t reader = kan_serialization_rd_reader_create (
                configuration_stream, &program_config,
                kan_string_intern ("kan_application_framework_program_configuration_t"), temporary_registry,
                config_allocation_group);

            enum kan_serialization_state_t state;
            while ((state = kan_serialization_rd_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
            {
            }

            if (state == KAN_SERIALIZATION_FAILED)
            {
                KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to read program configuration from \"%s\".",
                         program_configuration_path)
                result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_READ_CONFIGURATION;
            }

            kan_serialization_rd_reader_destroy (reader);
        }

        configuration_stream->operations->close (configuration_stream);
    }
    else
    {
        KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to open configuration file \"%s\".",
                 program_configuration_path)
        result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_READ_CONFIGURATION;
    }

    kan_serialization_binary_script_storage_destroy (temporary_script_storage);
    kan_reflection_registry_destroy (temporary_registry);

    if (result == 0)
    {
        result = kan_application_framework_run_with_configuration (&core_config, &program_config, arguments_count,
                                                                   arguments);
    }

    kan_application_framework_core_configuration_shutdown (&core_config);
    kan_application_framework_program_configuration_shutdown (&program_config);
    return result;
}

static inline void setup_plugin_system_config (
    struct kan_plugin_system_config_t *plugin_system_config,
    const struct kan_application_framework_core_configuration_t *core_configuration,
    const struct kan_application_framework_program_configuration_t *program_configuration)
{
    plugin_system_config->plugin_directory_path = core_configuration->plugin_directory_path;
    kan_plugin_system_config_init (plugin_system_config);

    KAN_ASSERT (core_configuration->plugin_directory_path)
    const uint64_t plugin_directory_name_length = strlen (core_configuration->plugin_directory_path);
    plugin_system_config->plugin_directory_path = kan_allocate_general (
        kan_plugin_system_config_get_allocation_group (), plugin_directory_name_length + 1u, _Alignof (char));
    memcpy (plugin_system_config->plugin_directory_path, core_configuration->plugin_directory_path,
            plugin_directory_name_length + 1u);

    kan_dynamic_array_set_capacity (&plugin_system_config->plugins,
                                    core_configuration->plugins.size + program_configuration->plugins.size);

    for (uint64_t index = 0u; index < core_configuration->plugins.size; ++index)
    {
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&plugin_system_config->plugins) =
            ((kan_interned_string_t *) core_configuration->plugins.data)[index];
    }

    for (uint64_t index = 0u; index < program_configuration->plugins.size; ++index)
    {
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&plugin_system_config->plugins) =
            ((kan_interned_string_t *) program_configuration->plugins.data)[index];
    }

    plugin_system_config->enable_hot_reload = core_configuration->enable_code_hot_reload;
    plugin_system_config->hot_reload_update_delay_ns = core_configuration->code_hot_reload_delay_ns;
}

static inline void add_resource_directories_to_virtual_file_system_config (
    struct kan_virtual_file_system_config_t *virtual_file_system_config,
    const struct kan_dynamic_array_t *resource_directories)
{
    for (uint64_t index = 0u; index < resource_directories->size; ++index)
    {
        struct kan_application_framework_resource_directory_t *item =
            &((struct kan_application_framework_resource_directory_t *) resource_directories->data)[index];

        KAN_ASSERT (item->path)
        KAN_ASSERT (item->mount_path)

        struct kan_virtual_file_system_config_mount_real_t *config_item =
            kan_allocate_batched (config_allocation_group, sizeof (struct kan_virtual_file_system_config_mount_real_t));

        config_item->mount_path = item->mount_path;
        config_item->real_path = item->path;
        config_item->next = virtual_file_system_config->first_mount_real;
        virtual_file_system_config->first_mount_real = config_item;
    }
}

static inline void add_resource_packs_to_virtual_file_system_config (
    struct kan_virtual_file_system_config_t *virtual_file_system_config,
    const struct kan_dynamic_array_t *resource_packs)
{
    for (uint64_t index = 0u; index < resource_packs->size; ++index)
    {
        struct kan_application_framework_resource_pack_t *item =
            &((struct kan_application_framework_resource_pack_t *) resource_packs->data)[index];

        KAN_ASSERT (item->path)
        KAN_ASSERT (item->mount_path)

        struct kan_virtual_file_system_config_mount_read_only_pack_t *config_item = kan_allocate_batched (
            config_allocation_group, sizeof (struct kan_virtual_file_system_config_mount_read_only_pack_t));

        config_item->mount_path = item->mount_path;
        config_item->pack_real_path = item->path;
        config_item->next = virtual_file_system_config->first_mount_read_only_pack;
        virtual_file_system_config->first_mount_read_only_pack = config_item;
    }
}

static inline void setup_virtual_file_system_config (
    struct kan_virtual_file_system_config_t *virtual_file_system_config,
    const struct kan_application_framework_core_configuration_t *core_configuration,
    const struct kan_application_framework_program_configuration_t *program_configuration)
{
    virtual_file_system_config->first_mount_real = NULL;
    virtual_file_system_config->first_mount_read_only_pack = NULL;

    add_resource_directories_to_virtual_file_system_config (virtual_file_system_config,
                                                            &core_configuration->resource_directories);

    add_resource_directories_to_virtual_file_system_config (virtual_file_system_config,
                                                            &program_configuration->resource_directories);

    add_resource_packs_to_virtual_file_system_config (virtual_file_system_config, &core_configuration->resource_packs);

    add_resource_packs_to_virtual_file_system_config (virtual_file_system_config,
                                                      &program_configuration->resource_packs);

    if (core_configuration->world_directory_path)
    {
        struct kan_virtual_file_system_config_mount_real_t *config_item =
            kan_allocate_batched (config_allocation_group, sizeof (struct kan_virtual_file_system_config_mount_real_t));

        config_item->mount_path = UNIVERSE_WORLD_DEFINITIONS_MOUNT_PATH;
        config_item->real_path = core_configuration->world_directory_path;
        config_item->next = virtual_file_system_config->first_mount_real;
        virtual_file_system_config->first_mount_real = config_item;
    }
}

static inline void shutdown_virtual_file_system_config (
    struct kan_virtual_file_system_config_t *virtual_file_system_config)
{
    while (virtual_file_system_config->first_mount_real)
    {
        struct kan_virtual_file_system_config_mount_real_t *item = virtual_file_system_config->first_mount_real;
        virtual_file_system_config->first_mount_real = item->next;
        kan_free_batched (config_allocation_group, item);
    }

    while (virtual_file_system_config->first_mount_read_only_pack)
    {
        struct kan_virtual_file_system_config_mount_read_only_pack_t *item =
            virtual_file_system_config->first_mount_read_only_pack;
        virtual_file_system_config->first_mount_read_only_pack = item->next;
        kan_free_batched (config_allocation_group, item);
    }
}

int kan_application_framework_run_with_configuration (
    const struct kan_application_framework_core_configuration_t *core_configuration,
    const struct kan_application_framework_program_configuration_t *program_configuration,
    uint64_t arguments_count,
    char **arguments)
{
    ensure_statics_initialized ();
    if (!kan_platform_application_init ())
    {
        return KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_INITIALIZE_PLATFORM;
    }

    int result = 0;
    kan_context_t context = kan_context_create (context_allocation_group);

    struct kan_application_framework_system_config_t application_framework_system_config;
    application_framework_system_config.arguments_count = arguments_count;
    application_framework_system_config.arguments = arguments;
    application_framework_system_config.auto_build_and_hot_reload_command =
        core_configuration->enable_code_hot_reload ? core_configuration->auto_build_and_hot_reload_command : NULL;

    if (!kan_context_request_system (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME,
                                     &application_framework_system_config))
    {
        KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to request application framework system.")
        result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_ASSEMBLE_CONTEXT;
    }

    if (!kan_context_request_system (context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME, NULL))
    {
        KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to request application system.")
        result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_ASSEMBLE_CONTEXT;
    }

    struct kan_plugin_system_config_t plugin_system_config;
    setup_plugin_system_config (&plugin_system_config, core_configuration, program_configuration);

    if (!kan_context_request_system (context, KAN_CONTEXT_PLUGIN_SYSTEM_NAME, &plugin_system_config))
    {
        KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to request plugin system.")
        result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_ASSEMBLE_CONTEXT;
    }

    if (!kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    {
        KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to request reflection system.")
        result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_ASSEMBLE_CONTEXT;
    }

    if (!kan_context_request_system (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME, NULL))
    {
        KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to request universe system.")
        result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_ASSEMBLE_CONTEXT;
    }

    struct kan_universe_world_definition_system_config_t universe_world_definition_system_config;
    universe_world_definition_system_config.definitions_mount_path = UNIVERSE_WORLD_DEFINITIONS_MOUNT_PATH;
    universe_world_definition_system_config.observe_definitions = core_configuration->observe_world_definitions;
    universe_world_definition_system_config.observation_rescan_delay_ns =
        core_configuration->world_definition_rescan_delay_ns;

    if (!kan_context_request_system (context, KAN_CONTEXT_UNIVERSE_WORLD_DEFINITION_SYSTEM_NAME,
                                     &universe_world_definition_system_config))
    {
        KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to request universe world definition system.")
        result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_ASSEMBLE_CONTEXT;
    }

    if (!kan_context_request_system (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME, NULL))
    {
        KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to request update system.")
        result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_ASSEMBLE_CONTEXT;
    }

    struct kan_virtual_file_system_config_t virtual_file_system_config;
    setup_virtual_file_system_config (&virtual_file_system_config, core_configuration, program_configuration);

    if (!kan_context_request_system (context, KAN_CONTEXT_VIRTUAL_FILE_SYSTEM_NAME, &virtual_file_system_config))
    {
        KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to request virtual file system.")
        result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_ASSEMBLE_CONTEXT;
    }

    for (uint64_t index = 0u; index < core_configuration->systems.size; ++index)
    {
        kan_interned_string_t name = ((kan_interned_string_t *) core_configuration->systems.data)[index];
        if (!kan_context_request_system (context, name, NULL))
        {
            KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to request custom system \"%s\".", name)
            result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_ASSEMBLE_CONTEXT;
        }
    }

    kan_context_assembly (context);
    kan_plugin_system_config_shutdown (&plugin_system_config);
    shutdown_virtual_file_system_config (&virtual_file_system_config);
    kan_context_system_t application_system = kan_context_query (context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);

    if (result == 0)
    {
        kan_context_system_t application_framework_system =
            kan_context_query (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME);
        kan_context_system_t universe_system = kan_context_query (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME);
        kan_context_system_t universe_world_definition_system =
            kan_context_query (context, KAN_CONTEXT_UNIVERSE_WORLD_DEFINITION_SYSTEM_NAME);
        kan_context_system_t update_system = kan_context_query (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);

        const struct kan_universe_world_definition_t *root_definition = kan_universe_world_definition_system_query (
            universe_world_definition_system, core_configuration->root_world);

        const struct kan_universe_world_definition_t *child_definition = kan_universe_world_definition_system_query (
            universe_world_definition_system, program_configuration->program_world);

        if (!root_definition)
        {
            KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to find root world definition \"%s\".",
                     core_configuration->root_world)
            result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_FIND_WORLD_DEFINITIONS;
        }
        else if (!child_definition)
        {
            KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to find program world definition \"%s\".",
                     program_configuration->program_world)
            result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_FIND_WORLD_DEFINITIONS;
        }
        else
        {
            kan_universe_t universe = kan_universe_system_get_universe (universe_system);
            for (uint64_t tag_index = 0u; tag_index < core_configuration->environment_tags.size; ++tag_index)
            {
                kan_universe_add_environment_tag (
                    universe, ((kan_interned_string_t *) core_configuration->environment_tags.data)[tag_index]);
            }

            kan_universe_world_t root_world = kan_universe_deploy_root (universe, root_definition);
            kan_universe_deploy_child (universe, root_world, child_definition);
            kan_cpu_section_t cooling_section = kan_cpu_section_get ("frame_cooling");

            while (!kan_application_framework_system_is_exit_requested (application_framework_system, &result))
            {
                kan_cpu_stage_separator ();
                const uint64_t frame_start_ns = kan_platform_get_elapsed_nanoseconds ();
                kan_application_system_sync_in_main_thread (application_system);
                kan_update_system_run (update_system);
                const uint64_t frame_end_ns = kan_platform_get_elapsed_nanoseconds ();

                const uint64_t frame_time_ns = frame_end_ns - frame_start_ns;
                const uint64_t min_frame_time_ns =
                    kan_application_framework_get_min_frame_time_ns (application_framework_system);

                if (min_frame_time_ns != 0u && frame_time_ns < min_frame_time_ns)
                {
                    struct kan_cpu_section_execution_t cooling_execution;
                    kan_cpu_section_execution_init (&cooling_execution, cooling_section);
                    kan_platform_sleep (min_frame_time_ns - frame_time_ns);
                    kan_cpu_section_execution_shutdown (&cooling_execution);
                }
            }

            kan_cpu_stage_separator ();
        }
    }

    if (KAN_HANDLE_IS_VALID (application_system))
    {
        kan_application_system_prepare_for_destroy_in_main_thread (application_system);
    }

    kan_context_destroy (context);
    kan_platform_application_shutdown ();
    return result;
}
