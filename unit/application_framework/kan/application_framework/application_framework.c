#include <string.h>

#include <kan/application_framework/application_framework.h>
#include <kan/context/all_system_names.h>
#include <kan/context/application_framework_system.h>
#include <kan/context/application_system.h>
#include <kan/context/plugin_system.h>
#include <kan/context/reflection_system.h>
#include <kan/context/universe_system.h>
#include <kan/context/universe_world_definition_system.h>
#include <kan/context/update_system.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/path_container.h>
#include <kan/file_system/stream.h>
#include <kan/log/logging.h>
#include <kan/log/observation.h>
#include <kan/platform/application.h>
#include <kan/precise_time/precise_time.h>
#include <kan/reflection/generated_reflection.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>

KAN_LOG_DEFINE_CATEGORY (application_framework);

static kan_bool_t statics_initialized = KAN_FALSE;
static kan_allocation_group_t config_allocation_group;
static kan_allocation_group_t context_allocation_group;
static FILE *logging_file = NULL;

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

void kan_application_framework_system_configuration_init (
    struct kan_application_framework_system_configuration_t *instance)
{
    instance->name = NULL;
    instance->configuration = KAN_HANDLE_SET_INVALID (kan_reflection_patch_t);
}

void kan_application_framework_system_configuration_shutdown (
    struct kan_application_framework_system_configuration_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->configuration))
    {
        kan_reflection_patch_destroy (instance->configuration);
    }
}

void kan_application_framework_core_configuration_init (struct kan_application_framework_core_configuration_t *instance)
{
    ensure_statics_initialized ();
    kan_dynamic_array_init (
        &instance->enabled_systems, 0u, sizeof (struct kan_application_framework_system_configuration_t),
        _Alignof (struct kan_application_framework_system_configuration_t), config_allocation_group);
    instance->root_world = NULL;
    instance->enable_auto_build = KAN_FALSE;
    instance->auto_build_command = NULL;
}

void kan_application_framework_core_configuration_shutdown (
    struct kan_application_framework_core_configuration_t *instance)
{
    for (kan_loop_size_t index = 0u; index < instance->enabled_systems.size; ++index)
    {
        kan_application_framework_system_configuration_shutdown (
            &((struct kan_application_framework_system_configuration_t *) instance->enabled_systems.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->enabled_systems);
    if (instance->auto_build_command)
    {
        kan_free_general (config_allocation_group, instance->auto_build_command,
                          strlen (instance->auto_build_command) + 1u);
    }
}

void kan_application_framework_program_configuration_init (
    struct kan_application_framework_program_configuration_t *instance)
{
    ensure_statics_initialized ();
    kan_dynamic_array_init (
        &instance->enabled_systems, 0u, sizeof (struct kan_application_framework_system_configuration_t),
        _Alignof (struct kan_application_framework_system_configuration_t), config_allocation_group);

    instance->log_name = NULL;
    instance->program_world = NULL;
}

void kan_application_framework_program_configuration_shutdown (
    struct kan_application_framework_program_configuration_t *instance)
{
    for (kan_loop_size_t index = 0u; index < instance->enabled_systems.size; ++index)
    {
        kan_application_framework_system_configuration_shutdown (
            &((struct kan_application_framework_system_configuration_t *) instance->enabled_systems.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->enabled_systems);
}

KAN_REFLECTION_EXPECT_UNIT_REGISTRAR_LOCAL (application_framework);

static inline kan_bool_t is_path_to_binary (const char *path)
{
    const kan_instance_size_t length = (kan_instance_size_t) strlen (path);
    return length > 4u && path[length - 4u] == '.' && path[length - 3u] == 'b' && path[length - 2u] == 'i' &&
           path[length - 1u] == 'n';
}

int kan_application_framework_run (const char *core_configuration_path,
                                   const char *program_configuration_path,
                                   kan_instance_size_t arguments_count,
                                   char **arguments)
{
    int result = 0;
    ensure_statics_initialized ();
    struct kan_application_framework_core_configuration_t core_config;
    kan_application_framework_core_configuration_init (&core_config);

    struct kan_application_framework_program_configuration_t program_config;
    kan_application_framework_program_configuration_init (&program_config);

    kan_reflection_registry_t temporary_registry = kan_reflection_registry_create ();
    // All context system configs belong to static as context systems belong to static.
    kan_reflection_system_register_static_reflection (temporary_registry);

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
    if (result == 0)
    {
        result = kan_application_framework_run_with_configuration (&core_config, &program_config, arguments_count,
                                                                   arguments, temporary_registry);

        if (result == KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_INITIALIZE_PLATFORM)
        {
            kan_application_framework_core_configuration_shutdown (&core_config);
            kan_application_framework_program_configuration_shutdown (&program_config);
            kan_reflection_registry_destroy (temporary_registry);
        }
    }
    else
    {
        kan_application_framework_core_configuration_shutdown (&core_config);
        kan_application_framework_program_configuration_shutdown (&program_config);
        kan_reflection_registry_destroy (temporary_registry);
    }

    return result;
}

struct config_instance_t
{
    const struct kan_reflection_struct_t *type;
    void *data;
};

static inline struct kan_application_framework_system_configuration_t *find_system_configuration (
    const struct kan_dynamic_array_t *array, kan_interned_string_t name)
{
    for (kan_loop_size_t index = 0u; index < array->size; ++index)
    {
        struct kan_application_framework_system_configuration_t *configuration =
            &((struct kan_application_framework_system_configuration_t *) array->data)[index];

        if (configuration->name == name)
        {
            return configuration;
        }
    }

    return NULL;
}

static void start_logging_to_file (const char *executable_path_argument, const char *log_name)
{
    KAN_ASSERT (!logging_file)
    if (!log_name)
    {
        log_name = "program_without_log_name";
    }

    struct kan_file_system_path_container_t path_container;
    struct kan_file_system_path_container_t rename_container;
    kan_file_system_path_container_copy_string (&path_container, executable_path_argument);

    while (path_container.length > 0u && path_container.path[path_container.length - 1u] != '/')
    {
        --path_container.length;
    }

    kan_file_system_path_container_add_suffix (&path_container, "logs");
    if (!kan_file_system_check_existence (path_container.path))
    {
        if (!kan_file_system_make_directory (path_container.path))
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework, KAN_LOG_ERROR,
                                 "Failed to initialize logging to file: unable to create directory \"%s\".",
                                 path_container.path)
            return;
        }
    }

    const kan_instance_size_t log_name_length = strlen (log_name);
    kan_file_system_path_container_append_char_sequence (&path_container, log_name, log_name + log_name_length);
    const kan_instance_size_t preserved_length = path_container.length;
    kan_file_system_path_container_add_suffix (&path_container, "_X.log");

    // Offset old log names and delete last one if there are too many.
    // We check all files instead of using directory iterator, because we need to move log files in order.

    for (char index_char = '9'; index_char >= '0'; --index_char)
    {
        path_container.path[path_container.length - 5u] = index_char;
        if (kan_file_system_check_existence (path_container.path))
        {
            if (index_char == '9')
            {
                if (!kan_file_system_remove_file (path_container.path))
                {
                    KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework, KAN_LOG_ERROR,
                                         "Failed to remove log file \"%s\".", path_container.path)
                }
            }
            else
            {
                kan_file_system_path_container_copy (&rename_container, &path_container);
                rename_container.path[path_container.length - 5u] = index_char + 1u;

                if (!kan_file_system_move_file (path_container.path, rename_container.path))
                {
                    KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework, KAN_LOG_ERROR,
                                         "Failed to move log file from \"%s\" to \"%s\".", path_container.path,
                                         rename_container.path)
                }
            }
        }
    }

    kan_file_system_path_container_reset_length (&path_container, preserved_length);
    kan_file_system_path_container_copy (&rename_container, &path_container);
    kan_file_system_path_container_add_suffix (&path_container, ".log");

    if (kan_file_system_check_existence (path_container.path))
    {
        kan_file_system_path_container_add_suffix (&rename_container, "_0.log");
        if (!kan_file_system_move_file (path_container.path, rename_container.path))
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework, KAN_LOG_ERROR,
                                 "Failed to move log file from \"%s\" to \"%s\".", path_container.path,
                                 rename_container.path)
        }
    }

    logging_file = fopen (path_container.path, "w");
    if (!logging_file)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework, KAN_LOG_ERROR,
                             "Failed to initialize logging to file: unable to open log file \"%s\".",
                             path_container.path)
        return;
    }

    kan_log_callback_add (kan_log_default_callback, (kan_functor_user_data_t) logging_file);
    KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework, KAN_LOG_INFO,
                         "Initialized logging to file \"%s\".", path_container.path)
}

static void stop_logging_to_file (void)
{
    if (logging_file)
    {
        kan_log_callback_remove (kan_log_default_callback, (kan_functor_user_data_t) logging_file);
        fclose (logging_file);
    }
}

int kan_application_framework_run_with_configuration (
    struct kan_application_framework_core_configuration_t *core_configuration,
    struct kan_application_framework_program_configuration_t *program_configuration,
    kan_instance_size_t arguments_count,
    char **arguments,
    kan_reflection_registry_t configuration_loading_registry)
{
    ensure_statics_initialized ();
    start_logging_to_file (arguments[0u], program_configuration->log_name);

    if (!kan_platform_application_init ())
    {
        stop_logging_to_file ();
        return KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_INITIALIZE_PLATFORM;
    }

    int result = 0;
    kan_context_t context = kan_context_create (context_allocation_group);

    struct kan_application_framework_system_config_t application_framework_system_config;
    application_framework_system_config.arguments_count = arguments_count;
    application_framework_system_config.arguments = arguments;
    application_framework_system_config.auto_build_command =
        core_configuration->enable_auto_build ? core_configuration->auto_build_command : NULL;

    if (!kan_context_request_system (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME,
                                     &application_framework_system_config))
    {
        KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to request application framework system.")
        result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_ASSEMBLE_CONTEXT;
    }

    struct kan_dynamic_array_t config_instances;
    kan_dynamic_array_init (
        &config_instances, core_configuration->enabled_systems.size + program_configuration->enabled_systems.size,
        sizeof (struct config_instance_t), _Alignof (struct config_instance_t), config_allocation_group);

    for (kan_loop_size_t index = 0u; index < core_configuration->enabled_systems.size; ++index)
    {
        struct kan_application_framework_system_configuration_t *system_core =
            &((struct kan_application_framework_system_configuration_t *)
                  core_configuration->enabled_systems.data)[index];

        struct kan_application_framework_system_configuration_t *system_program =
            find_system_configuration (&program_configuration->enabled_systems, system_core->name);

        const struct kan_reflection_struct_t *type_core =
            KAN_HANDLE_IS_VALID (system_core->configuration) ?
                kan_reflection_patch_get_type (system_core->configuration) :
                NULL;

        const struct kan_reflection_struct_t *type_program =
            system_program && KAN_HANDLE_IS_VALID (system_program->configuration) ?
                kan_reflection_patch_get_type (system_program->configuration) :
                NULL;

        if (type_core && type_program && type_core != type_program)
        {
            KAN_LOG (application_framework, KAN_LOG_ERROR,
                     "Skipped context system \"%s\" as its core config has type \"%s\" and program config has type "
                     "\"%s\", while configs must have the same type.",
                     system_core->name, type_core->name, type_program->name)
            continue;
        }

        const struct kan_reflection_struct_t *config_type = type_core ? type_core : type_program;
        void *config = NULL;

        if (config_type)
        {
#define INSTANTIATE_CONFIG                                                                                             \
    struct config_instance_t *instance = kan_dynamic_array_add_last (&config_instances);                               \
    KAN_ASSERT (instance)                                                                                              \
    instance->type = config_type;                                                                                      \
    instance->data = kan_allocate_general (config_allocation_group, config_type->size, config_type->alignment);        \
                                                                                                                       \
    if (config_type->init)                                                                                             \
    {                                                                                                                  \
        config_type->init (config_type->functor_user_data, instance->data);                                            \
    }

            INSTANTIATE_CONFIG
            if (KAN_HANDLE_IS_VALID (system_core->configuration))
            {
                kan_reflection_patch_apply (system_core->configuration, instance->data);
            }

            if (system_program && KAN_HANDLE_IS_VALID (system_program->configuration))
            {
                kan_reflection_patch_apply (system_program->configuration, instance->data);
            }

            config = instance->data;
        }

        if (!kan_context_request_system (context, system_core->name, config))
        {
            KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to request \"%s\" context system.",
                     system_core->name)
            result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_ASSEMBLE_CONTEXT;
        }
    }

    for (kan_loop_size_t index = 0u; index < program_configuration->enabled_systems.size; ++index)
    {
        struct kan_application_framework_system_configuration_t *system_program =
            &((struct kan_application_framework_system_configuration_t *)
                  program_configuration->enabled_systems.data)[index];

        if (find_system_configuration (&core_configuration->enabled_systems, system_program->name))
        {
            // Already registered through core.
            continue;
        }

        const struct kan_reflection_struct_t *config_type =
            KAN_HANDLE_IS_VALID (system_program->configuration) ?
                kan_reflection_patch_get_type (system_program->configuration) :
                NULL;
        void *config = NULL;

        if (config_type)
        {
            INSTANTIATE_CONFIG
#undef INSTANTIATE_CONFIG

            if (KAN_HANDLE_IS_VALID (system_program->configuration))
            {
                kan_reflection_patch_apply (system_program->configuration, instance->data);
            }

            config = instance->data;
        }

        if (!kan_context_request_system (context, system_program->name, config))
        {
            KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to request \"%s\" context system.",
                     system_program->name)
            result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_ASSEMBLE_CONTEXT;
        }
    }

    kan_context_assembly (context);
    for (kan_loop_size_t index = 0u; index < config_instances.size; ++index)
    {
        struct config_instance_t *instance = &((struct config_instance_t *) config_instances.data)[index];
        if (instance->type->shutdown)
        {
            instance->type->shutdown (instance->type->functor_user_data, instance->data);
        }

        kan_free_general (config_allocation_group, instance->data, instance->type->size);
    }

    kan_dynamic_array_shutdown (&config_instances);
    kan_application_framework_core_configuration_shutdown (core_configuration);
    kan_application_framework_program_configuration_shutdown (program_configuration);
    kan_reflection_registry_destroy (configuration_loading_registry);
    kan_context_system_t application_system = kan_context_query (context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);

    if (result == 0)
    {
        kan_context_system_t application_framework_system =
            kan_context_query (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME);
        kan_context_system_t universe_system = kan_context_query (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME);
        kan_context_system_t universe_world_definition_system =
            kan_context_query (context, KAN_CONTEXT_UNIVERSE_WORLD_DEFINITION_SYSTEM_NAME);
        kan_context_system_t update_system = kan_context_query (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);

        const struct kan_universe_world_definition_t *root_definition = NULL;
        const struct kan_universe_world_definition_t *child_definition = NULL;

        if (!KAN_HANDLE_IS_VALID (application_system) || !KAN_HANDLE_IS_VALID (application_framework_system) ||
            !KAN_HANDLE_IS_VALID (universe_system) || !KAN_HANDLE_IS_VALID (universe_world_definition_system) ||
            !KAN_HANDLE_IS_VALID (update_system))
        {
            KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to create required systems.")
            result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_CREATE_REQUIRED_SYSTEMS;
        }
        else if (!(root_definition = kan_universe_world_definition_system_query (universe_world_definition_system,
                                                                                 core_configuration->root_world)))
        {
            KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to find root world definition \"%s\".",
                     core_configuration->root_world)
            result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_FIND_WORLD_DEFINITIONS;
        }
        else if (!(child_definition = kan_universe_world_definition_system_query (
                       universe_world_definition_system, program_configuration->program_world)))
        {
            KAN_LOG (application_framework, KAN_LOG_ERROR, "Failed to find program world definition \"%s\".",
                     program_configuration->program_world)
            result = KAN_APPLICATION_FRAMEWORK_EXIT_CODE_FAILED_TO_FIND_WORLD_DEFINITIONS;
        }
        else
        {
            kan_universe_t universe = kan_universe_system_get_universe (universe_system);
            kan_universe_world_t root_world = kan_universe_deploy_root (universe, root_definition);
            kan_universe_deploy_child (universe, root_world, child_definition);
            kan_cpu_section_t cooling_section = kan_cpu_section_get ("frame_cooling");

            while (!kan_application_framework_system_is_exit_requested (application_framework_system, &result))
            {
                kan_cpu_stage_separator ();
                kan_cpu_reset_task_dispatch_counter ();

                const kan_time_size_t frame_start_ns = kan_precise_time_get_elapsed_nanoseconds ();
                kan_application_system_sync_in_main_thread (application_system);
                kan_update_system_run (update_system);
                const kan_time_size_t frame_end_ns = kan_precise_time_get_elapsed_nanoseconds ();

                const kan_time_offset_t frame_time_ns = (kan_time_offset_t) (frame_end_ns - frame_start_ns);
                const kan_time_offset_t min_frame_time_ns =
                    kan_application_framework_get_min_frame_time_ns (application_framework_system);

#if defined(KAN_APPLICATION_FRAMEWORK_PRINT_FRAME_TIMES)
                KAN_LOG (application_framework, KAN_LOG_INFO, "CPU frame took %lu ns.", (unsigned long) frame_time_ns)
#endif

                if (min_frame_time_ns != 0u && frame_time_ns < min_frame_time_ns)
                {
                    struct kan_cpu_section_execution_t cooling_execution;
                    kan_cpu_section_execution_init (&cooling_execution, cooling_section);
                    kan_precise_time_sleep (min_frame_time_ns - frame_time_ns);
                    kan_cpu_section_execution_shutdown (&cooling_execution);
                }
            }

            kan_cpu_stage_separator ();
            kan_cpu_reset_task_dispatch_counter ();
        }
    }

    if (KAN_HANDLE_IS_VALID (application_system))
    {
        kan_application_system_prepare_for_destroy_in_main_thread (application_system);
    }

    kan_context_destroy (context);
    kan_platform_application_shutdown ();
    stop_logging_to_file ();
    return result;
}
