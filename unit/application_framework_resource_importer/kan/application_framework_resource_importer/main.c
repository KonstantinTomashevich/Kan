#include <kan/api_common/min_max.h>
#include <kan/application_framework_resource_tool/context.h>
#include <kan/application_framework_resource_tool/project.h>
#include <kan/checksum/checksum.h>
#include <kan/context/context.h>
#include <kan/context/plugin_system.h>
#include <kan/context/reflection_system.h>
#include <kan/cpu_dispatch/job.h>
#include <kan/cpu_dispatch/task.h>
#include <kan/error/critical.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/path_container.h>
#include <kan/file_system/stream.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/platform/hardware.h>
#include <kan/reflection/generated_reflection.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>
#include <kan/threading/atomic.h>
#include <kan/threading/conditional_variable.h>
#include <kan/threading/mutex.h>

KAN_LOG_DEFINE_CATEGORY (application_framework_resource_importer);

#define ERROR_CODE_INCORRECT_ARGUMENTS -1
#define ERROR_CODE_FAILED_TO_READ_PROJECT -2
#define ERROR_CODE_FAILED_TO_READ_IMPORT_RULES -3
#define ERROR_CODE_ERRORS_DURING_IMPORT -4

static struct
{
    const char *executable_path;
    const char *project_path;

    kan_bool_t keep_resources;
    kan_bool_t check_is_data_new;

    kan_instance_size_t requests_count;
    char **requests;

} arguments = {
    .project_path = NULL,
    .keep_resources = KAN_FALSE,
    .check_is_data_new = KAN_FALSE,

    .requests_count = 0u,
    .requests = NULL,
};

struct rule_t
{
    struct rule_t *next_in_list;
    struct kan_resource_import_rule_t import_rule;

    kan_instance_size_t directory_part_length;
    char *path;

    struct kan_atomic_int_t inputs_left_to_process;
    struct kan_atomic_int_t input_errors;

    void *configuration_instance;
    const struct kan_resource_import_configuration_type_meta_t *meta;

    struct kan_dynamic_array_t new_import_inputs;

    const char *external_source_path_root;
    kan_instance_size_t source_path_root_length;
    const char *external_owner_resource_directory_path;
};

struct rule_start_request_t
{
    struct rule_start_request_t *next;
    struct rule_t *rule;
};

struct rule_process_request_t
{
    struct rule_process_request_t *next;
    struct rule_t *rule;
    struct kan_resource_import_input_t *input;
};

struct rule_finish_request_t
{
    struct rule_finish_request_t *next;
    struct rule_t *rule;
};

static struct
{
    struct kan_application_resource_project_t project;
    kan_reflection_registry_t registry;
    kan_serialization_binary_script_storage_t binary_script_storage;

    struct kan_atomic_int_t import_tasks_left;
    struct kan_atomic_int_t errors_count;

    struct kan_atomic_int_t rule_registration_lock;
    struct rule_t *first_rule;

    kan_mutex_t request_management_mutex;
    kan_conditional_variable_t on_request_served;
    kan_instance_size_t requests_in_serving;

    struct rule_start_request_t *first_start_request;
    struct rule_process_request_t *first_process_request;
    struct rule_finish_request_t *first_finish_request;

    struct kan_stack_group_allocator_t temporary_allocator;

    kan_interned_string_t interned_kan_resource_import_rule_t;
    kan_interned_string_t interned_kan_resource_import_configuration_type_meta_t;

    kan_allocation_group_t temporary_allocation_group;
    kan_allocation_group_t configuration_allocation_group;

} global = {
    .registry = KAN_HANDLE_INITIALIZE_INVALID,
    .binary_script_storage = KAN_HANDLE_INITIALIZE_INVALID,
    .first_rule = NULL,
    .first_start_request = NULL,
    .first_process_request = NULL,
    .first_finish_request = NULL,
};

static inline void rule_init (struct rule_t *rule)
{
    kan_resource_import_rule_init (&rule->import_rule);
    rule->path = NULL;

    rule->inputs_left_to_process = kan_atomic_int_init (0);
    rule->input_errors = kan_atomic_int_init (0);

    rule->configuration_instance = NULL;
    rule->meta = NULL;

    kan_dynamic_array_init (&rule->new_import_inputs, 0u, sizeof (struct kan_resource_import_input_t),
                            _Alignof (struct kan_resource_import_input_t),
                            kan_resource_import_rule_get_allocation_group ());

    rule->external_source_path_root = NULL;
    rule->source_path_root_length = 0u;
    rule->external_owner_resource_directory_path = NULL;
}

static inline void rule_shutdown (struct rule_t *rule)
{
    kan_allocation_group_t allocation_group = kan_resource_import_rule_get_allocation_group ();
    kan_resource_import_rule_shutdown (&rule->import_rule);

    if (rule->path)
    {
        kan_free_general (allocation_group, rule->path, strlen (rule->path) + 1u);
    }

    for (kan_loop_size_t index = 0u; index < rule->new_import_inputs.size; ++index)
    {
        kan_resource_import_input_shutdown (
            &((struct kan_resource_import_input_t *) rule->new_import_inputs.data)[index]);
    }

    kan_dynamic_array_shutdown (&rule->new_import_inputs);
}

static inline void destroy_all_rules (void)
{
    kan_allocation_group_t allocation_group = kan_resource_import_rule_get_allocation_group ();
    struct rule_t *rule = global.first_rule;
    global.first_rule = NULL;

    while (rule)
    {
        struct rule_t *next = rule->next_in_list;
        rule_shutdown (rule);
        kan_free_batched (allocation_group, rule);
        rule = next;
    }
}

static void create_import_rule_using_stream (struct kan_stream_t *stream,
                                             const char *rule_path,
                                             const char *owner_resource_directory_path)
{
    kan_allocation_group_t allocation_group = kan_resource_import_rule_get_allocation_group ();
    struct rule_t *rule = kan_allocate_batched (allocation_group, sizeof (struct rule_t));
    rule_init (rule);
    rule->external_owner_resource_directory_path = owner_resource_directory_path;

    kan_serialization_rd_reader_t reader = kan_serialization_rd_reader_create (
        stream, &rule->import_rule, global.interned_kan_resource_import_rule_t, global.registry, allocation_group);

    enum kan_serialization_state_t serialization_state;
    while ((serialization_state = kan_serialization_rd_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    kan_serialization_rd_reader_destroy (reader);
    if (serialization_state == KAN_SERIALIZATION_FINISHED)
    {
        KAN_LOG (application_framework_resource_importer, KAN_LOG_INFO, "Using import rule from \"%s\".", rule_path)
        rule->inputs_left_to_process = kan_atomic_int_init (0);

        const char *last_separator = strrchr (rule_path, '/');
        if (!last_separator)
        {
            KAN_ASSERT (serialization_state == KAN_SERIALIZATION_FAILED)
            KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR,
                     "Unable to get directory path part from \"%s\".", rule_path)

            kan_atomic_int_add (&global.errors_count, 1);
            rule_shutdown (rule);
            kan_free_batched (allocation_group, rule);
            return;
        }

        rule->directory_part_length = (kan_instance_size_t) (last_separator - rule_path);
        kan_instance_size_t path_length = strlen (rule_path);
        rule->path = kan_allocate_general (allocation_group, path_length + 1u, _Alignof (char));
        memcpy (rule->path, rule_path, path_length + 1u);

        kan_atomic_int_lock (&global.rule_registration_lock);
        rule->next_in_list = global.first_rule;
        global.first_rule = rule;
        kan_atomic_int_unlock (&global.rule_registration_lock);
    }
    else
    {
        KAN_ASSERT (serialization_state == KAN_SERIALIZATION_FAILED)
        KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR, "Failed to read import rule at \"%s\".",
                 rule_path)

        kan_atomic_int_add (&global.errors_count, 1);
        rule_shutdown (rule);
        kan_free_batched (allocation_group, rule);
    }
}

static void create_import_rule_from_path (const char *rule_path)
{
    struct kan_stream_t *input_stream = kan_direct_file_stream_open_for_read (rule_path, KAN_TRUE);
    if (!input_stream)
    {
        KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR, "Failed to open import rule at \"%s\".",
                 rule_path)
        kan_atomic_int_add (&global.errors_count, 1);
        return;
    }

    input_stream = kan_random_access_stream_buffer_open_for_read (input_stream, KAN_RESOURCE_IMPORTER_IO_BUFFER);
    create_import_rule_using_stream (input_stream, rule_path, NULL);
    input_stream->operations->close (input_stream);
}

static void scan_file_as_potential_rule (struct kan_file_system_path_container_t *path_container,
                                         const char *resource_directory)
{
    // We expect rules only in readable data format.
    if (path_container->length >= 3u && path_container->path[path_container->length - 3u] == '.' &&
        path_container->path[path_container->length - 2u] == 'r' &&
        path_container->path[path_container->length - 1u] == 'd')
    {
        struct kan_stream_t *input_stream = kan_direct_file_stream_open_for_read (path_container->path, KAN_TRUE);
        if (!input_stream)
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                                 KAN_LOG_ERROR, "Failed to open resource file at \"%s\".", path_container->path)
            kan_atomic_int_add (&global.errors_count, 1);
            return;
        }

        input_stream = kan_random_access_stream_buffer_open_for_read (input_stream, KAN_RESOURCE_IMPORTER_IO_BUFFER);
        kan_interned_string_t type_name;

        if (!kan_serialization_rd_read_type_header (input_stream, &type_name))
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                                 KAN_LOG_ERROR, "Failed read type header of \"%s\".", path_container->path)
            kan_atomic_int_add (&global.errors_count, 1);
            input_stream->operations->close (input_stream);
            return;
        }

        if (type_name == global.interned_kan_resource_import_rule_t)
        {
            create_import_rule_using_stream (input_stream, path_container->path, resource_directory);
        }

        input_stream->operations->close (input_stream);
    }
}

static void scan_directory_for_rules (struct kan_file_system_path_container_t *path_container,
                                      const char *resource_directory)
{
    kan_file_system_directory_iterator_t iterator = kan_file_system_directory_iterator_create (path_container->path);
    const char *item_name;

    while ((item_name = kan_file_system_directory_iterator_advance (iterator)))
    {
        if ((item_name[0u] == '.' && item_name[1u] == '\0') ||
            (item_name[0u] == '.' && item_name[1u] == '.' && item_name[2u] == '\0'))
        {
            // Skip special entries.
            continue;
        }

        const kan_instance_size_t old_length = path_container->length;
        kan_file_system_path_container_append (path_container, item_name);
        struct kan_file_system_entry_status_t status;

        if (kan_file_system_query_entry (path_container->path, &status))
        {
            switch (status.type)
            {
            case KAN_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN:
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                                     KAN_LOG_ERROR, "Entry \"%s\" has unknown type.", path_container->path)
                kan_atomic_int_add (&global.errors_count, 1);
                break;

            case KAN_FILE_SYSTEM_ENTRY_TYPE_FILE:
                scan_file_as_potential_rule (path_container, resource_directory);
                break;

            case KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY:
                scan_directory_for_rules (path_container, resource_directory);
                break;
            }
        }
        else
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                                 KAN_LOG_ERROR, "Failed to query status of entry \"%s\".", path_container->path)
            kan_atomic_int_add (&global.errors_count, 1);
        }

        kan_file_system_path_container_reset_length (path_container, old_length);
    }

    kan_file_system_directory_iterator_destroy (iterator);
}

static void scan_target_for_rules (kan_functor_user_data_t user_data)
{
    struct kan_application_resource_target_t *target = (struct kan_application_resource_target_t *) user_data;
    struct kan_file_system_path_container_t path_container;

    for (kan_loop_size_t index = 0u; index < target->directories.size; ++index)
    {
        kan_file_system_path_container_copy_string (&path_container, ((char **) target->directories.data)[index]);
        scan_directory_for_rules (&path_container, ((char **) target->directories.data)[index]);
    }

    KAN_LOG (application_framework_resource_importer, KAN_LOG_INFO, "Done scanning for import rules in target \"%s\".",
             target->name)
}

static inline void serve_start_request_end_by_error (void)
{
    kan_atomic_int_add (&global.errors_count, 1);
    kan_mutex_lock (global.request_management_mutex);
    --global.requests_in_serving;
    kan_mutex_unlock (global.request_management_mutex);
    kan_conditional_variable_signal_one (global.on_request_served);
}

static inline void rule_add_new_import (struct rule_t *rule, const char *relative_source_path)
{
    kan_allocation_group_t import_group = kan_resource_import_rule_get_allocation_group ();
    struct kan_resource_import_input_t *input = kan_dynamic_array_add_last (&rule->new_import_inputs);

    if (!input)
    {
        kan_dynamic_array_set_capacity (&rule->new_import_inputs, KAN_MAX (KAN_RESOURCE_IMPORTER_INPUT_CAPACITY,
                                                                           rule->new_import_inputs.size * 2u));

        input = kan_dynamic_array_add_last (&rule->new_import_inputs);
        KAN_ASSERT (input)
    }

    kan_resource_import_input_init (input);
    const kan_instance_size_t source_path_length = (kan_instance_size_t) strlen (relative_source_path);
    input->source_path = kan_allocate_general (import_group, source_path_length + 1u, _Alignof (char));
    memcpy (input->source_path, relative_source_path, source_path_length + 1u);
}

static inline void scan_file_as_potential_import (struct rule_t *rule,
                                                  struct kan_file_system_path_container_t *path_container,
                                                  const char *relative_path_start)
{
    if (rule->import_rule.extension_filter)
    {
        kan_bool_t filter_result = KAN_FALSE;
        kan_instance_size_t extension_length = 0u;

        while (extension_length + 1u < path_container->length)
        {
            if (path_container->path[path_container->length - extension_length - 1u] == '.')
            {
                const kan_instance_size_t filter_length =
                    (kan_instance_size_t) strlen (rule->import_rule.extension_filter);

                if (filter_length != extension_length)
                {
                    break;
                }

                filter_result = strcmp (&path_container->path[path_container->length - extension_length],
                                        rule->import_rule.extension_filter) == 0;
                break;
            }
            else if (path_container->path[path_container->length - extension_length - 1u] == '/')
            {
                break;
            }

            ++extension_length;
        }

        if (!filter_result)
        {
            return;
        }
    }

    rule_add_new_import (rule, relative_path_start);
}

static void scan_directory_for_import (struct rule_t *rule,
                                       struct kan_file_system_path_container_t *path_container,
                                       const char *relative_path_start)
{
    kan_file_system_directory_iterator_t iterator = kan_file_system_directory_iterator_create (path_container->path);
    const char *item_name;

    while ((item_name = kan_file_system_directory_iterator_advance (iterator)))
    {
        if ((item_name[0u] == '.' && item_name[1u] == '\0') ||
            (item_name[0u] == '.' && item_name[1u] == '.' && item_name[2u] == '\0'))
        {
            // Skip special entries.
            continue;
        }

        const kan_instance_size_t old_length = path_container->length;
        kan_file_system_path_container_append (path_container, item_name);
        struct kan_file_system_entry_status_t status;

        if (kan_file_system_query_entry (path_container->path, &status))
        {
            switch (status.type)
            {
            case KAN_FILE_SYSTEM_ENTRY_TYPE_UNKNOWN:
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                                     KAN_LOG_ERROR, "Entry \"%s\" has unknown type.", path_container->path)
                kan_atomic_int_add (&global.errors_count, 1);
                break;

            case KAN_FILE_SYSTEM_ENTRY_TYPE_FILE:
                scan_file_as_potential_import (rule, path_container, relative_path_start);
                break;

            case KAN_FILE_SYSTEM_ENTRY_TYPE_DIRECTORY:
                scan_directory_for_import (rule, path_container, relative_path_start);
                break;
            }
        }
        else
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                                 KAN_LOG_ERROR, "Failed to query status of entry \"%s\".", path_container->path)
            kan_atomic_int_add (&global.errors_count, 1);
        }

        kan_file_system_path_container_reset_length (path_container, old_length);
    }

    kan_file_system_directory_iterator_destroy (iterator);
}

static inline void add_finish_request_unsafe (struct rule_t *rule)
{
    struct rule_finish_request_t *request =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&global.temporary_allocator, struct rule_finish_request_t);
    request->next = global.first_finish_request;
    global.first_finish_request = request;
    request->rule = rule;
}

static void serve_start_request (kan_functor_user_data_t user_data)
{
    struct rule_t *rule = (struct rule_t *) user_data;
    if (!KAN_HANDLE_IS_VALID (rule->import_rule.configuration) ||
        !kan_reflection_patch_get_type (rule->import_rule.configuration))
    {
        KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR,
                 "Unable to unpack configuration from patch as patch is invalid for the rule at \"%s\".", rule->path)
        serve_start_request_end_by_error ();
        return;
    }

    const struct kan_reflection_struct_t *patch_type = kan_reflection_patch_get_type (rule->import_rule.configuration);
    struct kan_reflection_struct_meta_iterator_t iterator = kan_reflection_registry_query_struct_meta (
        global.registry, patch_type->name, global.interned_kan_resource_import_configuration_type_meta_t);
    rule->meta = kan_reflection_struct_meta_iterator_get (&iterator);

    if (!rule->meta)
    {
        KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR,
                 "Configuration type \"%s\" has no \"kan_resource_import_configuration_type_meta_t\" meta.",
                 patch_type->name)
        serve_start_request_end_by_error ();
        return;
    }

    kan_reflection_struct_meta_iterator_next (&iterator);
    if (kan_reflection_struct_meta_iterator_get (&iterator))
    {
        KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR,
                 "Configuration type \"%s\" has several \"kan_resource_import_configuration_type_meta_t\" metas.",
                 patch_type->name)
        serve_start_request_end_by_error ();
        return;
    }

    rule->configuration_instance = kan_allocate_batched (global.configuration_allocation_group, patch_type->size);
    if (patch_type->init)
    {
        patch_type->init (patch_type->functor_user_data, rule->configuration_instance);
    }

    kan_reflection_patch_apply (rule->import_rule.configuration, rule->configuration_instance);

    switch (rule->import_rule.source_path_root)
    {
    case KAN_RESOURCE_IMPORT_SOURCE_PATH_ROOT_FILE_DIRECTORY:
        rule->source_path_root_length = rule->directory_part_length;
        rule->external_source_path_root = rule->path;
        break;

    case KAN_RESOURCE_IMPORT_SOURCE_PATH_ROOT_RESOURCE_DIRECTORY:
        if (rule->external_owner_resource_directory_path)
        {
            rule->external_source_path_root = rule->external_owner_resource_directory_path;
            rule->source_path_root_length = (kan_instance_size_t) strlen (rule->external_source_path_root);
        }
        else
        {
            // Received rule without provided external resource path (directly as argument, perhaps).
            // We need to find resource directory path ourselves.
            const kan_instance_size_t path_length = (kan_instance_size_t) strlen (rule->path);

            for (kan_loop_size_t target_index = 0u; target_index < global.project.targets.size; ++target_index)
            {
                struct kan_application_resource_target_t *target =
                    &((struct kan_application_resource_target_t *) global.project.targets.data)[target_index];

                for (kan_loop_size_t directory_index = 0u; directory_index < target->directories.size;
                     ++directory_index)
                {
                    const char *directory_path = ((char **) target->directories.data)[directory_index];
                    const kan_instance_size_t directory_path_length = (kan_instance_size_t) strlen (directory_path);

                    if (directory_path_length < path_length &&
                        strncmp (rule->path, directory_path, directory_path_length) == 0)
                    {
                        rule->source_path_root_length = directory_path_length;
                        rule->external_source_path_root = directory_path;
                        break;
                    }
                }

                if (rule->external_source_path_root)
                {
                    break;
                }
            }

            if (!rule->external_source_path_root)
            {
                KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR,
                         "Unable to parent resource directory for the rule \"%s\".", rule->path)
                serve_start_request_end_by_error ();
                return;
            }
        }

        break;

    case KAN_RESOURCE_IMPORT_SOURCE_PATH_ROOT_APPLICATION_SOURCE:
        rule->external_source_path_root = global.project.application_source_directory;
        rule->source_path_root_length = (kan_instance_size_t) strlen (rule->external_source_path_root);
        break;

    case KAN_RESOURCE_IMPORT_SOURCE_PATH_ROOT_PROJECT_SOURCE:
        rule->external_source_path_root = global.project.project_source_directory;
        rule->source_path_root_length = (kan_instance_size_t) strlen (rule->external_source_path_root);
        break;

    case KAN_RESOURCE_IMPORT_SOURCE_PATH_ROOT_CMAKE_SOURCE:
        rule->external_source_path_root = global.project.source_directory;
        rule->source_path_root_length = (kan_instance_size_t) strlen (rule->external_source_path_root);
        break;
    }

    KAN_ASSERT (rule->external_source_path_root)
    switch (rule->import_rule.source_path_rule)
    {
    case KAN_RESOURCE_IMPORT_SOURCE_PATH_RULE_EXACT:
    {
        rule_add_new_import (rule, rule->import_rule.source_path);
        break;
    }

    case KAN_RESOURCE_IMPORT_SOURCE_PATH_RULE_HIERARCHY:
    {
        struct kan_file_system_path_container_t path_container;
        kan_file_system_path_container_copy_char_sequence (
            &path_container, rule->external_source_path_root,
            rule->external_source_path_root + rule->source_path_root_length);
        kan_file_system_path_container_append (&path_container, rule->import_rule.source_path);
        scan_directory_for_import (rule, &path_container, path_container.path + rule->source_path_root_length + 1u);
        break;
    }
    }

    if (rule->new_import_inputs.size == 0u)
    {
        KAN_LOG (application_framework_resource_importer, KAN_LOG_WARNING,
                 "Unable to find any inputs for the rule \"%s\".", rule->path)
    }

    kan_atomic_int_set (&rule->inputs_left_to_process, (int) rule->new_import_inputs.size);
    kan_mutex_lock (global.request_management_mutex);

    for (kan_loop_size_t index = 0u; index < rule->new_import_inputs.size; ++index)
    {
        struct kan_resource_import_input_t *input =
            &((struct kan_resource_import_input_t *) rule->new_import_inputs.data)[index];

        struct rule_process_request_t *request =
            KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&global.temporary_allocator, struct rule_process_request_t);

        request->next = global.first_process_request;
        global.first_process_request = request;
        request->rule = rule;
        request->input = input;
    }

    if (rule->new_import_inputs.size == 0u)
    {
        // Finish instead. We might need to clear old data.
        add_finish_request_unsafe (rule);
    }

    --global.requests_in_serving;
    kan_mutex_unlock (global.request_management_mutex);
    kan_conditional_variable_signal_one (global.on_request_served);
}

static inline void serve_process_request_end_by_error (struct rule_t *rule)
{
    kan_atomic_int_add (&global.errors_count, 1);
    kan_atomic_int_add (&rule->input_errors, 1);
    const kan_bool_t last_input = kan_atomic_int_add (&rule->inputs_left_to_process, -1) == 1;

    kan_mutex_lock (global.request_management_mutex);
    if (last_input)
    {
        add_finish_request_unsafe (rule);
    }

    --global.requests_in_serving;
    kan_mutex_unlock (global.request_management_mutex);
    kan_conditional_variable_signal_one (global.on_request_served);
}

static kan_bool_t import_interface_produce (kan_functor_user_data_t user_data,
                                            const char *relative_path,
                                            kan_interned_string_t type_name,
                                            void *data)
{
    struct rule_process_request_t *request = (struct rule_process_request_t *) user_data;
    struct rule_t *rule = request->rule;
    struct kan_resource_import_input_t *input = request->input;

    const kan_instance_size_t relative_path_length = (kan_instance_size_t) strlen (relative_path);
    char *copied_relative_path = kan_allocate_general (kan_resource_import_rule_get_allocation_group (),
                                                       relative_path_length + 1u, _Alignof (char));
    memcpy (copied_relative_path, relative_path, relative_path_length + 1u);

    void *spot = kan_dynamic_array_add_last (&input->outputs);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (&input->outputs,
                                        KAN_MAX (KAN_RESOURCE_IMPORTER_OUTPUT_CAPACITY, input->outputs.size * 2u));
        spot = kan_dynamic_array_add_last (&input->outputs);
        KAN_ASSERT (spot)
    }

    *(char **) spot = copied_relative_path;
    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_char_sequence (&path_container, rule->path,
                                                       rule->path + rule->directory_part_length);
    kan_file_system_path_container_append (&path_container, relative_path);

    struct kan_stream_t *output_stream = kan_direct_file_stream_open_for_write (path_container.path, KAN_TRUE);
    if (!output_stream)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                             KAN_LOG_ERROR, "Failed to open output stream to import result \"%s\".",
                             path_container.path)
        kan_atomic_int_add (&global.errors_count, 1);
        kan_atomic_int_add (&rule->input_errors, 1);
        return KAN_FALSE;
    }

    output_stream = kan_random_access_stream_buffer_open_for_write (output_stream, KAN_RESOURCE_IMPORTER_IO_BUFFER);
    kan_bool_t result = KAN_TRUE;

    if (path_container.length >= 3u && path_container.path[path_container.length - 3u] == '.' &&
        path_container.path[path_container.length - 2u] == 'r' &&
        path_container.path[path_container.length - 1u] == 'd')
    {
        if (kan_serialization_rd_write_type_header (output_stream, type_name))
        {
            kan_serialization_rd_writer_t writer =
                kan_serialization_rd_writer_create (output_stream, data, type_name, global.registry);

            enum kan_serialization_state_t state;
            while ((state = kan_serialization_rd_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
            {
            }

            kan_serialization_rd_writer_destroy (writer);
            if (state != KAN_SERIALIZATION_FINISHED)
            {
                KAN_ASSERT (state == KAN_SERIALIZATION_FAILED)
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                                     KAN_LOG_ERROR, "Failed to write readable data for import result \"%s\".",
                                     path_container.path)
                kan_atomic_int_add (&global.errors_count, 1);
                kan_atomic_int_add (&rule->input_errors, 1);
                result = KAN_FALSE;
            }
        }
        else
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                                 KAN_LOG_ERROR, "Failed to write type header for import result \"%s\".",
                                 path_container.path)
            kan_atomic_int_add (&global.errors_count, 1);
            kan_atomic_int_add (&rule->input_errors, 1);
            result = KAN_FALSE;
        }
    }
    else if (path_container.length >= 4u && path_container.path[path_container.length - 4u] == '.' &&
             path_container.path[path_container.length - 3u] == 'b' &&
             path_container.path[path_container.length - 2u] == 'i' &&
             path_container.path[path_container.length - 1u] == 'n')
    {
        if (kan_serialization_binary_write_type_header (
                output_stream, type_name, KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t)))
        {
            kan_serialization_binary_writer_t writer = kan_serialization_binary_writer_create (
                output_stream, data, type_name, global.binary_script_storage,
                KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));

            enum kan_serialization_state_t state;
            while ((state = kan_serialization_binary_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
            {
            }

            kan_serialization_binary_writer_destroy (writer);
            if (state != KAN_SERIALIZATION_FINISHED)
            {
                KAN_ASSERT (state == KAN_SERIALIZATION_FAILED)
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                                     KAN_LOG_ERROR, "Failed to write binary for import result \"%s\".",
                                     path_container.path)
                kan_atomic_int_add (&global.errors_count, 1);
                kan_atomic_int_add (&rule->input_errors, 1);
                result = KAN_FALSE;
            }
        }
        else
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                                 KAN_LOG_ERROR, "Failed to write type header for import result \"%s\".",
                                 path_container.path)
            kan_atomic_int_add (&global.errors_count, 1);
            kan_atomic_int_add (&rule->input_errors, 1);
            result = KAN_FALSE;
        }
    }
    else
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                             KAN_LOG_ERROR, "Failed to open output stream to import results \"%s\".",
                             path_container.path)
        kan_atomic_int_add (&global.errors_count, 1);
        kan_atomic_int_add (&rule->input_errors, 1);
        result = KAN_FALSE;
    }

    output_stream->operations->close (output_stream);
    return result;
}

static void serve_process_request (kan_functor_user_data_t user_data)
{
    struct rule_process_request_t *request = (struct rule_process_request_t *) user_data;
    struct rule_t *rule = request->rule;
    struct kan_resource_import_input_t *input = request->input;

    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_char_sequence (&path_container, rule->external_source_path_root,
                                                       rule->external_source_path_root + rule->source_path_root_length);
    kan_file_system_path_container_append (&path_container, input->source_path);

    struct kan_stream_t *input_stream = kan_direct_file_stream_open_for_read (path_container.path, KAN_TRUE);
    if (!input_stream)
    {
        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                             KAN_LOG_ERROR, "Unable to open input \"%s\" for the rule \"%s\".", path_container.path,
                             rule->path)
        serve_process_request_end_by_error (rule);
        return;
    }

    input_stream = kan_random_access_stream_buffer_open_for_read (input_stream, KAN_RESOURCE_IMPORTER_IO_BUFFER);
    if (rule->meta->allow_checksum)
    {
        kan_checksum_state_t state = kan_checksum_create ();
        uint8_t checksum_buffer[KAN_RESOURCE_IMPORTER_IO_BUFFER];
        kan_file_size_t read;

        while ((read = input_stream->operations->read (input_stream, KAN_RESOURCE_IMPORTER_IO_BUFFER,
                                                       checksum_buffer)) > 0)
        {
            kan_checksum_append (state, read, checksum_buffer);
        }

        input->checksum = kan_checksum_finalize (state);
        if (!input_stream->operations->seek (input_stream, KAN_STREAM_SEEK_START, 0))
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                                 KAN_LOG_ERROR, "Failed to get size of \"%s\" for the rule \"%s\".",
                                 path_container.path, rule->path)
            serve_process_request_end_by_error (rule);
            return;
        }
    }

    kan_bool_t use_old_import_data = KAN_FALSE;
    if (arguments.check_is_data_new && rule->meta->allow_checksum)
    {
        for (kan_loop_size_t index = 0u; index < rule->import_rule.last_import.size; ++index)
        {
            struct kan_resource_import_input_t *old_input =
                &((struct kan_resource_import_input_t *) rule->import_rule.last_import.data)[index];

            if (strcmp (old_input->source_path, input->source_path) == 0 && old_input->checksum == input->checksum)
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                                     KAN_LOG_INFO,
                                     "Skipping import of \"%s\" for the rule \"%s\" as its data seems old.",
                                     path_container.path, rule->path)
                use_old_import_data = KAN_TRUE;
                break;
            }
        }
    }

    if (!use_old_import_data)
    {
        struct kan_resource_import_interface_t interface = {
            .user_data = (kan_functor_user_data_t) request,
            .produce = import_interface_produce,
        };

        if (!rule->meta->functor (input_stream, input->source_path, global.registry, rule->configuration_instance,
                                  &interface))
        {
            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                                 KAN_LOG_ERROR, "Failure during import of \"%s\" for the rule \"%s\".",
                                 path_container.path, rule->path)
            kan_atomic_int_add (&global.errors_count, 1);
            kan_atomic_int_add (&rule->input_errors, 1);
        }
    }

    input_stream->operations->close (input_stream);
    const kan_bool_t last_input = kan_atomic_int_add (&rule->inputs_left_to_process, -1) == 1;
    kan_mutex_lock (global.request_management_mutex);

    if (last_input)
    {
        add_finish_request_unsafe (rule);
    }

    --global.requests_in_serving;
    kan_mutex_unlock (global.request_management_mutex);
    kan_conditional_variable_signal_one (global.on_request_served);
}

static void serve_finish_request (kan_functor_user_data_t user_data)
{
    struct rule_t *rule = (struct rule_t *) user_data;

    if (kan_atomic_int_get (&rule->input_errors) == 0)
    {
        kan_bool_t has_errors = KAN_FALSE;
        for (kan_loop_size_t old_index = 0u; old_index < rule->import_rule.last_import.size; ++old_index)
        {
            struct kan_resource_import_input_t *old_input =
                &((struct kan_resource_import_input_t *) rule->import_rule.last_import.data)[old_index];

            for (kan_loop_size_t old_output_index = 0u; old_output_index < old_input->outputs.size; ++old_output_index)
            {
                const char *old_output = ((char **) old_input->outputs.data)[old_output_index];
                kan_bool_t found_in_new = KAN_FALSE;

                for (kan_loop_size_t new_index = 0u; new_index < rule->new_import_inputs.size; ++new_index)
                {
                    struct kan_resource_import_input_t *new_input =
                        &((struct kan_resource_import_input_t *) rule->new_import_inputs.data)[new_index];

                    for (kan_loop_size_t new_output_index = 0u; new_output_index < new_input->outputs.size;
                         ++new_output_index)
                    {
                        const char *new_output = ((char **) new_input->outputs.data)[new_output_index];
                        if (strcmp (old_output, new_output) == 0)
                        {
                            found_in_new = KAN_TRUE;
                            break;
                        }
                    }

                    if (found_in_new)
                    {
                        break;
                    }
                }

                if (!found_in_new)
                {
                    struct kan_file_system_path_container_t path_container;
                    kan_file_system_path_container_copy_char_sequence (&path_container, rule->path,
                                                                       rule->path + rule->directory_part_length);
                    kan_file_system_path_container_append (&path_container, old_output);

                    if (arguments.keep_resources)
                    {
                        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u,
                                             application_framework_resource_importer, KAN_LOG_WARNING,
                                             "Output \"%s\" of rule \"%s\" is no longer produced and is dangling.",
                                             path_container.path, rule->path)
                    }
                    else
                    {
                        KAN_LOG_WITH_BUFFER (
                            KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer, KAN_LOG_INFO,
                            "Deleting dangling output \"%s\" of rule \"%s\".", path_container.path, rule->path)

                        if (!kan_file_system_remove_file (path_container.path))
                        {
                            KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u,
                                                 application_framework_resource_importer, KAN_LOG_ERROR,
                                                 "Failed to delete dangling output \"%s\" of rule \"%s\".",
                                                 path_container.path, rule->path)
                            kan_atomic_int_add (&global.errors_count, 1);
                            has_errors = KAN_TRUE;
                        }
                    }
                }
            }
        }

        if (!has_errors)
        {
            // Clear last import data first.
            for (kan_loop_size_t index = 0u; index < rule->import_rule.last_import.size; ++index)
            {
                kan_resource_import_input_shutdown (
                    &((struct kan_resource_import_input_t *) rule->import_rule.last_import.data)[index]);
            }

            kan_dynamic_array_shutdown (&rule->import_rule.last_import);
            // Override import rule array.
            rule->import_rule.last_import = rule->new_import_inputs;

            // Clear new import rule array to avoid double free.
            rule->new_import_inputs.capacity = 0u;
            rule->new_import_inputs.size = 0u;
            rule->new_import_inputs.data = NULL;

            // Save updated import rule.
            struct kan_stream_t *output_stream = kan_direct_file_stream_open_for_write (rule->path, KAN_TRUE);
            if (output_stream)
            {
                output_stream =
                    kan_random_access_stream_buffer_open_for_write (output_stream, KAN_RESOURCE_IMPORTER_IO_BUFFER);

                if (kan_serialization_rd_write_type_header (output_stream, global.interned_kan_resource_import_rule_t))
                {
                    kan_serialization_rd_writer_t writer = kan_serialization_rd_writer_create (
                        output_stream, &rule->import_rule, global.interned_kan_resource_import_rule_t, global.registry);

                    enum kan_serialization_state_t state;
                    while ((state = kan_serialization_rd_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
                    {
                    }

                    kan_serialization_rd_writer_destroy (writer);
                    if (state != KAN_SERIALIZATION_FINISHED)
                    {
                        KAN_ASSERT (state == KAN_SERIALIZATION_FAILED)
                        KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u,
                                             application_framework_resource_importer, KAN_LOG_ERROR,
                                             "Failed to write readable data to save import rule \"%s\".", rule->path)
                        kan_atomic_int_add (&global.errors_count, 1);
                    }
                }
                else
                {
                    KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                                         KAN_LOG_ERROR, "Failed to write type header to save import rule \"%s\".",
                                         rule->path)
                    kan_atomic_int_add (&global.errors_count, 1);
                }

                output_stream->operations->close (output_stream);
            }
            else
            {
                KAN_LOG_WITH_BUFFER (KAN_FILE_SYSTEM_MAX_PATH_LENGTH * 2u, application_framework_resource_importer,
                                     KAN_LOG_ERROR, "Failed to open output stream to save import rule \"%s\".",
                                     rule->path)
                kan_atomic_int_add (&global.errors_count, 1);
            }
        }
    }

    if (rule->configuration_instance)
    {
        const struct kan_reflection_struct_t *patch_type =
            kan_reflection_patch_get_type (rule->import_rule.configuration);
        if (patch_type->shutdown)
        {
            patch_type->shutdown (patch_type->functor_user_data, rule->configuration_instance);
        }

        kan_free_batched (global.configuration_allocation_group, rule->configuration_instance);
    }

    kan_mutex_lock (global.request_management_mutex);
    --global.requests_in_serving;
    kan_mutex_unlock (global.request_management_mutex);
    kan_conditional_variable_signal_one (global.on_request_served);
}

int main (int argument_count, char **argument_values)
{
    if (argument_count < 2)
    {
        KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR,
                 "Incorrect number of arguments. Expected arguments: <path_to_resource_project_file> flags...? "
                 "rules_or_targets_to_check...")
        KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR, "Where flags are:")
        KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR,
                 "    --keep_resources: avoid deleting resources that were previously imported but are no longer "
                 "produced from import.")
        KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR,
                 "    --check_is_data_new: check source files size and checksum and skip import if they're the same.")
        KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR,
                 "It is possible to provide either paths to rules or target names.")
        KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR,
                 "All provided paths must be absolute in order to make parent resource directory search easier.")
        KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR,
                 "When target is provided, all rules from it are executed.")
        return ERROR_CODE_INCORRECT_ARGUMENTS;
    }

    arguments.executable_path = argument_values[0u];
    arguments.project_path = argument_values[1u];
    kan_instance_size_t argument_index = 2u;

    while (argument_index < (kan_instance_size_t) argument_count)
    {
        if (strcmp (argument_values[argument_index], "--keep_resources") == 0)
        {
            arguments.keep_resources = KAN_TRUE;
        }
        else if (strcmp (argument_values[argument_index], "--check_is_data_new") == 0)
        {
            arguments.check_is_data_new = KAN_TRUE;
        }
        else
        {
            arguments.requests_count = (kan_instance_size_t) argument_count - argument_index;
            arguments.requests = argument_values + argument_index;
        }

        ++argument_index;
    }

    if (arguments.requests_count == 0u)
    {
        KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR, "No rules or targets provided.")
        return ERROR_CODE_INCORRECT_ARGUMENTS;
    }

    int result = 0;
    KAN_LOG (application_framework_resource_importer, KAN_LOG_INFO, "Reading project...")
    kan_application_resource_project_init (&global.project);

    if (!kan_application_resource_project_read (argument_values[1u], &global.project))
    {
        KAN_LOG (application_framework_resource_importer, KAN_LOG_ERROR, "Failed to read project from \"%s\".",
                 argument_values[1u])
        kan_application_resource_project_shutdown (&global.project);
        return ERROR_CODE_FAILED_TO_READ_PROJECT;
    }

    global.interned_kan_resource_import_rule_t = kan_string_intern ("kan_resource_import_rule_t");
    global.interned_kan_resource_import_configuration_type_meta_t =
        kan_string_intern ("kan_resource_import_configuration_type_meta_t");

    global.temporary_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "temporary_allocation");
    global.configuration_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "import_configuration");

    global.import_tasks_left = kan_atomic_int_init (0);
    global.errors_count = kan_atomic_int_init (0);
    global.rule_registration_lock = kan_atomic_int_init (0);

    kan_context_t context = kan_application_create_resource_tool_context (&global.project, argument_values[0u]);
    kan_context_system_t plugin_system = kan_context_query (context, KAN_CONTEXT_PLUGIN_SYSTEM_NAME);
    KAN_ASSERT (KAN_HANDLE_IS_VALID (plugin_system))

    kan_context_system_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_ASSERT (KAN_HANDLE_IS_VALID (reflection_system))

    global.registry = kan_reflection_system_get_registry (reflection_system);
    global.binary_script_storage = kan_serialization_binary_script_storage_create (global.registry);

    kan_stack_group_allocator_init (&global.temporary_allocator, global.temporary_allocation_group,
                                    KAN_RESOURCE_IMPORTER_TEMPORARY_STACK);
    KAN_LOG (application_framework_resource_importer, KAN_LOG_INFO, "Reading import rules...")

    struct kan_cpu_task_list_node_t *task_list = NULL;
    const kan_interned_string_t task_name = kan_string_intern ("scan_target_for_rules");

    for (kan_loop_size_t request_index = 0u; request_index < (kan_loop_size_t) arguments.requests_count;
         ++request_index)
    {
        kan_bool_t processed_as_target = KAN_FALSE;
        for (kan_loop_size_t target_index = 0u; target_index < global.project.targets.size; ++target_index)
        {
            struct kan_application_resource_target_t *target =
                &((struct kan_application_resource_target_t *) global.project.targets.data)[target_index];

            if (strcmp (target->name, arguments.requests[request_index]) == 0)
            {
                processed_as_target = KAN_TRUE;
                KAN_CPU_TASK_LIST_USER_VALUE (&task_list, &global.temporary_allocator, task_name, scan_target_for_rules,
                                              target)
            }
        }

        if (!processed_as_target)
        {
            create_import_rule_from_path (arguments.requests[request_index]);
        }
    }

    if (task_list)
    {
        kan_cpu_job_t job = kan_cpu_job_create ();
        kan_cpu_job_dispatch_and_detach_task_list (job, task_list);
        kan_cpu_job_release (job);
        kan_cpu_job_wait (job);
    }

    kan_stack_group_allocator_reset (&global.temporary_allocator);
    if (kan_atomic_int_get (&global.errors_count) > 0)
    {
        result = ERROR_CODE_FAILED_TO_READ_IMPORT_RULES;
    }

    if (result == 0)
    {
        global.request_management_mutex = kan_mutex_create ();
        global.on_request_served = kan_conditional_variable_create ();

        struct rule_t *rule = global.first_rule;
        while (rule)
        {
            struct rule_start_request_t *request =
                KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (&global.temporary_allocator, struct rule_start_request_t);

            request->next = global.first_start_request;
            global.first_start_request = request;
            request->rule = rule;
            rule = rule->next_in_list;
        }

        // We need to limit count of max requests in serving in order to avoid using too much memory.
        const kan_instance_size_t max_requests_in_serving = kan_platform_get_cpu_count ();
        kan_mutex_lock (global.request_management_mutex);

        const kan_interned_string_t task_start = kan_string_intern ("rule_start");
        const kan_interned_string_t task_process = kan_string_intern ("rule_process");
        const kan_interned_string_t task_finish = kan_string_intern ("rule_finish");

        while (KAN_TRUE)
        {
            while (global.requests_in_serving < max_requests_in_serving)
            {
                if (global.first_start_request)
                {
                    struct kan_cpu_task_t task = {
                        .name = task_start,
                        .function = serve_start_request,
                        .user_data = (kan_functor_user_data_t) global.first_start_request->rule,
                    };

                    kan_cpu_task_t handle = kan_cpu_task_dispatch (task);
                    KAN_ASSERT (KAN_HANDLE_IS_VALID (handle))
                    kan_cpu_task_detach (handle);

                    ++global.requests_in_serving;
                    global.first_start_request = global.first_start_request->next;
                }
                else if (global.first_process_request)
                {
                    struct kan_cpu_task_t task = {
                        .name = task_process,
                        .function = serve_process_request,
                        .user_data = (kan_functor_user_data_t) global.first_process_request,
                    };

                    kan_cpu_task_t handle = kan_cpu_task_dispatch (task);
                    KAN_ASSERT (KAN_HANDLE_IS_VALID (handle))
                    kan_cpu_task_detach (handle);

                    ++global.requests_in_serving;
                    global.first_process_request = global.first_process_request->next;
                }
                else if (global.first_finish_request)
                {
                    struct kan_cpu_task_t task = {
                        .name = task_finish,
                        .function = serve_finish_request,
                        .user_data = (kan_functor_user_data_t) global.first_finish_request->rule,
                    };

                    kan_cpu_task_t handle = kan_cpu_task_dispatch (task);
                    KAN_ASSERT (KAN_HANDLE_IS_VALID (handle))
                    kan_cpu_task_detach (handle);

                    ++global.requests_in_serving;
                    global.first_finish_request = global.first_finish_request->next;
                }
                else
                {
                    break;
                }
            }

            if (global.requests_in_serving == 0u)
            {
                // Nothing in serving and no requests -- everything is finished.
                kan_mutex_unlock (global.request_management_mutex);
                break;
            }

            kan_conditional_variable_wait (global.on_request_served, global.request_management_mutex);
        }

        kan_conditional_variable_destroy (global.on_request_served);
        kan_mutex_destroy (global.request_management_mutex);

        if (kan_atomic_int_get (&global.errors_count) > 0)
        {
            result = ERROR_CODE_ERRORS_DURING_IMPORT;
        }
    }

    kan_stack_group_allocator_shutdown (&global.temporary_allocator);
    destroy_all_rules ();
    kan_serialization_binary_script_storage_destroy (global.binary_script_storage);
    kan_context_destroy (context);
    kan_application_resource_project_shutdown (&global.project);
    return result;
}
