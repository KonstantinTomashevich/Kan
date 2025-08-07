#include <test_resource_pipeline_build_api.h>

#include <string.h>

#include <kan/context/all_system_names.h>
#include <kan/context/context.h>
#include <kan/context/reflection_system.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/path_container.h>
#include <kan/file_system/stream.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/precise_time/precise_time.h>
#include <kan/resource_pipeline/build.h>
#include <kan/resource_pipeline/platform_configuration.h>
#include <kan/resource_pipeline/tooling_meta.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>
#include <kan/testing/testing.h>

KAN_LOG_DEFINE_CATEGORY (test_resource_pipeline_build);
KAN_USE_STATIC_INTERNED_IDS

static bool statics_initialized = false;

static void ensure_statics_initialized ()
{
    if (!statics_initialized)
    {
        kan_static_interned_ids_ensure_initialized ();
        statics_initialized = true;
    }
}

struct sum_parsed_source_t
{
    kan_instance_size_t source_number;
};

KAN_REFLECTION_STRUCT_META (sum_parsed_source_t)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_type_meta_t sum_parsed_source_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

struct sum_resource_raw_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t sources;
};

KAN_REFLECTION_STRUCT_META (sum_resource_raw_t)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_type_meta_t sum_resource_raw_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

KAN_REFLECTION_STRUCT_FIELD_META (sum_resource_raw_t, sources)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_reference_meta_t sum_resource_raw_reference_sources = {
    .type_name = "sum_parsed_source_t",
    .flags = 0u,
};

TEST_RESOURCE_PIPELINE_BUILD_API void sum_resource_raw_init (struct sum_resource_raw_t *instance)
{
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
}

TEST_RESOURCE_PIPELINE_BUILD_API void sum_resource_raw_shutdown (struct sum_resource_raw_t *instance)
{
    kan_dynamic_array_shutdown (&instance->sources);
}

struct sum_resource_t
{
    kan_instance_size_t sum;
};

KAN_REFLECTION_STRUCT_META (sum_resource_t)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_type_meta_t sum_resource_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

struct root_resource_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t needed_sums;
};

KAN_REFLECTION_STRUCT_META (root_resource_t)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_type_meta_t root_resource_resource_type = {
    .flags = KAN_RESOURCE_TYPE_ROOT,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

KAN_REFLECTION_STRUCT_FIELD_META (root_resource_t, needed_sums)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_reference_meta_t root_resource_reference_needed_sums = {
    .type_name = "sum_resource_t",
    .flags = 0u,
};

static enum kan_resource_build_rule_result_t sum_parsed_source_build (
    struct kan_resource_build_rule_context_t *context);

KAN_REFLECTION_STRUCT_META (sum_parsed_source_t)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_build_rule_t sum_parsed_source_build_rule = {
    .primary_input_type = NULL,
    .platform_configuration_type = NULL,
    .secondary_types_count = 0u,
    .secondary_types = NULL,
    .functor = sum_parsed_source_build,
    .version = CUSHION_START_NS_X64,
};

static enum kan_resource_build_rule_result_t sum_parsed_source_build (struct kan_resource_build_rule_context_t *context)
{
    struct sum_parsed_source_t *output = context->primary_output;
    output->source_number = 0u;

    struct kan_stream_t *stream = kan_direct_file_stream_open_for_read (context->primary_third_party_path, false);
    if (!stream)
    {
        KAN_LOG (test_resource_pipeline_build, KAN_LOG_ERROR, "Failed to open sum source file for \"%s\".",
                 context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    stream = kan_random_access_stream_buffer_open_for_read (stream, 4096u);
    CUSHION_DEFER { stream->operations->close (stream); };
    char symbol;
    bool reading = true;

    while (reading)
    {
        switch (stream->operations->read (stream, sizeof (symbol), &symbol))
        {
        case sizeof (symbol):
        {
            if (symbol < '0' || symbol > '9')
            {
                KAN_LOG (test_resource_pipeline_build, KAN_LOG_ERROR,
                         "Got symbol with code %u instead of digit while parsing \"%s\".", (unsigned int) symbol,
                         context->primary_name)
                return KAN_RESOURCE_BUILD_RULE_FAILURE;
            }

            output->source_number = output->source_number * 10u + (kan_instance_size_t) (symbol - '0');
            break;
        }

        case 0u:
        {
            reading = false;
            break;
        }

        default:
        {
            KAN_LOG (test_resource_pipeline_build, KAN_LOG_ERROR, "Failed to read next digit while parsing \"%s\".",
                     context->primary_name)
            return KAN_RESOURCE_BUILD_RULE_FAILURE;
        }
        }
    }

    return KAN_RESOURCE_BUILD_RULE_SUCCESS;
}

static enum kan_resource_build_rule_result_t sum_resource_build (struct kan_resource_build_rule_context_t *context);

KAN_REFLECTION_STRUCT_META (sum_resource_t)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_build_rule_t sum_resource_build_rule = {
    .primary_input_type = "sum_resource_raw_t",
    .platform_configuration_type = NULL,
    .secondary_types_count = 1u,
    .secondary_types = (const char *[]) {"sum_parsed_source_t"},
    .functor = sum_resource_build,
    .version = CUSHION_START_NS_X64,
};

static enum kan_resource_build_rule_result_t sum_resource_build (struct kan_resource_build_rule_context_t *context)
{
    const struct sum_resource_raw_t *input = context->primary_input;
    struct sum_resource_t *output = context->primary_output;
    output->sum = 0u;

    const struct kan_resource_build_rule_secondary_node_t *secondary = context->secondary_input_first;
    kan_loop_size_t secondary_received = 0u;

    while (secondary)
    {
        const struct sum_parsed_source_t *source = secondary->data;
        output->sum += source->source_number;
        ++secondary_received;
        secondary = secondary->next;
    }

    if (input->sources.size != secondary_received)
    {
        KAN_LOG (test_resource_pipeline_build, KAN_LOG_ERROR,
                 "While building \"%s\", expected %u secondary inputs, but got %u.", context->primary_name,
                 (unsigned int) input->sources.size, (unsigned int) secondary_received)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    return KAN_RESOURCE_BUILD_RULE_SUCCESS;
}

TEST_RESOURCE_PIPELINE_BUILD_API void root_resource_init (struct root_resource_t *instance)
{
    kan_dynamic_array_init (&instance->needed_sums, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
}

TEST_RESOURCE_PIPELINE_BUILD_API void root_resource_shutdown (struct root_resource_t *instance)
{
    kan_dynamic_array_shutdown (&instance->needed_sums);
}

static void save_text_to (const char *path, const char *text)
{
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (path, false);
    KAN_TEST_ASSERT (stream)

    stream = kan_random_access_stream_buffer_open_for_write (stream, 4096u);
    CUSHION_DEFER { stream->operations->close (stream); }

    const kan_instance_size_t size = (kan_instance_size_t) strlen (text);
    KAN_TEST_ASSERT (stream->operations->write (stream, size, text) == size)
}

static void save_rd_to (kan_reflection_registry_t registry,
                        const char *path,
                        kan_interned_string_t type,
                        const void *data)
{
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (path, false);
    KAN_TEST_ASSERT (stream)

    stream = kan_random_access_stream_buffer_open_for_write (stream, 4096u);
    CUSHION_DEFER { stream->operations->close (stream); }

    KAN_TEST_ASSERT (kan_serialization_rd_write_type_header (stream, type))
    kan_serialization_rd_writer_t writer = kan_serialization_rd_writer_create (stream, data, type, registry);
    CUSHION_DEFER { kan_serialization_rd_writer_destroy (writer); }
    enum kan_serialization_state_t state;

    while ((state = kan_serialization_rd_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    KAN_TEST_ASSERT (state == KAN_SERIALIZATION_FINISHED)
}

static void load_binary_from (kan_serialization_binary_script_storage_t script_storage,
                              const char *path,
                              kan_interned_string_t type,
                              void *data)
{
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_read (path, true);
    KAN_TEST_ASSERT (stream)

    stream = kan_random_access_stream_buffer_open_for_read (stream, 4096u);
    CUSHION_DEFER { stream->operations->close (stream); }

    kan_interned_string_t actual_type;
    KAN_TEST_ASSERT (kan_serialization_binary_read_type_header (
        stream, &actual_type, KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t)))
    KAN_TEST_ASSERT (type == actual_type)

    kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
        stream, data, type, script_storage, KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t),
        KAN_ALLOCATION_GROUP_IGNORE);

    CUSHION_DEFER { kan_serialization_binary_reader_destroy (reader); }
    enum kan_serialization_state_t state;

    while ((state = kan_serialization_binary_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    KAN_TEST_ASSERT (state == KAN_SERIALIZATION_FINISHED)
}

#define WORKSPACE_DIRECTORY "workspace"
#define PLATFORM_CONFIGURATION_DIRECTORY "platform_configuration"
#define PLATFORM_CONFIGURATION_BASE_LAYER "base"
#define PLATFORM_CONFIGURATION_ADDITIONAL_LAYER "additional"

static void initialize_platform_configuration_with_empty_stub (kan_reflection_registry_t registry)
{
    struct kan_file_system_path_container_t path;
    kan_file_system_path_container_copy_string (&path, PLATFORM_CONFIGURATION_DIRECTORY);
    kan_file_system_path_container_append (&path, KAN_RESOURCE_PLATFORM_CONFIGURATION_SETUP_FILE);

    struct kan_resource_platform_configuration_setup_t setup;
    kan_resource_platform_configuration_setup_init (&setup);
    CUSHION_DEFER { kan_resource_platform_configuration_setup_shutdown (&setup); }

    kan_dynamic_array_set_capacity (&setup.layers, 2u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&setup.layers) =
        kan_string_intern (PLATFORM_CONFIGURATION_BASE_LAYER);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&setup.layers) =
        kan_string_intern (PLATFORM_CONFIGURATION_ADDITIONAL_LAYER);

    save_rd_to (registry, path.path, KAN_STATIC_INTERNED_ID_GET (kan_resource_platform_configuration_setup_t), &setup);
}

static void set_base_directories_to_project (struct kan_resource_project_t *project)
{
    project->workspace_directory = kan_allocate_general (kan_resource_project_get_allocation_group (),
                                                         sizeof (WORKSPACE_DIRECTORY), alignof (char));
    strcpy (project->workspace_directory, WORKSPACE_DIRECTORY);

    project->platform_configuration_directory = kan_allocate_general (
        kan_resource_project_get_allocation_group (), sizeof (PLATFORM_CONFIGURATION_DIRECTORY), alignof (char));
    strcpy (project->platform_configuration_directory, PLATFORM_CONFIGURATION_DIRECTORY);
}

#define TEST_TARGET_NAME "test_target"
#define TEST_TARGET_RESOURCE_DIRECTORY "resources"

static void add_common_basic_test_target_to_project (struct kan_resource_project_t *project)
{
    struct kan_resource_project_target_t *target = kan_dynamic_array_add_last (&project->targets);
    if (!target)
    {
        kan_dynamic_array_set_capacity (&project->targets, KAN_MAX (1u, project->targets.size * 2u));
        target = kan_dynamic_array_add_last (&project->targets);
    }

    kan_resource_project_target_init (target);
    target->name = kan_string_intern (TEST_TARGET_NAME);

    char *directory = kan_allocate_general (kan_resource_project_get_allocation_group (),
                                            sizeof (TEST_TARGET_RESOURCE_DIRECTORY), alignof (char));
    strcpy (directory, TEST_TARGET_RESOURCE_DIRECTORY);

    kan_dynamic_array_set_capacity (&target->directories, 1u);
    *(char **) kan_dynamic_array_add_last (&target->directories) = directory;
}

#define SETUP_CONTEXT_AND_GET_REFLECTION                                                                               \
    kan_context_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);                                          \
    CUSHION_DEFER { kan_context_destroy (context); }                                                                   \
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))                    \
    kan_context_assembly (context);                                                                                    \
                                                                                                                       \
    kan_context_system_t reflection_system_handle = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);   \
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (reflection_system_handle))                                                   \
                                                                                                                       \
    kan_reflection_registry_t registry = kan_reflection_system_get_registry (reflection_system_handle);                \
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (registry))                                                                   \
                                                                                                                       \
    kan_serialization_binary_script_storage_t script_storage =                                                         \
        kan_serialization_binary_script_storage_create (registry);                                                     \
    CUSHION_DEFER { kan_serialization_binary_script_storage_destroy (script_storage); }

#define SETUP_TRIVIAL_TEST_ENVIRONMENT                                                                                 \
    ensure_statics_initialized ();                                                                                     \
    SETUP_CONTEXT_AND_GET_REFLECTION                                                                                   \
    kan_file_system_remove_directory_with_content (TEST_TARGET_RESOURCE_DIRECTORY);                                    \
    KAN_TEST_CHECK (kan_file_system_make_directory (TEST_TARGET_RESOURCE_DIRECTORY))                                   \
                                                                                                                       \
    kan_file_system_remove_directory_with_content (WORKSPACE_DIRECTORY);                                               \
    KAN_TEST_CHECK (kan_file_system_make_directory (WORKSPACE_DIRECTORY))                                              \
                                                                                                                       \
    kan_file_system_remove_directory_with_content (PLATFORM_CONFIGURATION_DIRECTORY);                                  \
    KAN_TEST_CHECK (kan_file_system_make_directory (PLATFORM_CONFIGURATION_DIRECTORY))                                 \
    initialize_platform_configuration_with_empty_stub (registry);                                                      \
                                                                                                                       \
    struct kan_resource_project_t project;                                                                             \
    kan_resource_project_init (&project);                                                                              \
    CUSHION_DEFER { kan_resource_project_shutdown (&project); }                                                        \
                                                                                                                       \
    add_common_basic_test_target_to_project (&project);                                                                \
    set_base_directories_to_project (&project);                                                                        \
                                                                                                                       \
    struct kan_resource_reflected_data_storage_t reflected_data;                                                       \
    kan_resource_reflected_data_storage_build (&reflected_data, registry);                                             \
    CUSHION_DEFER { kan_resource_reflected_data_storage_shutdown (&reflected_data); }                                  \
                                                                                                                       \
    struct kan_resource_build_setup_t setup;                                                                           \
    kan_resource_build_setup_init (&setup);                                                                            \
    CUSHION_DEFER { kan_resource_build_setup_shutdown (&setup); };                                                     \
                                                                                                                       \
    setup.project = &project;                                                                                          \
    setup.reflected_data = &reflected_data;                                                                            \
    setup.pack = false;                                                                                                \
    setup.log_verbosity = KAN_LOG_VERBOSE;                                                                             \
                                                                                                                       \
    kan_dynamic_array_set_capacity (&setup.targets, 1u);                                                               \
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&setup.targets) = kan_string_intern (TEST_TARGET_NAME)

KAN_TEST_CASE (correctness)
{
    SETUP_TRIVIAL_TEST_ENVIRONMENT;

    struct kan_file_system_path_container_t write_path;
    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, "1.txt");
    save_text_to (write_path.path, "123");

    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, "2.txt");
    save_text_to (write_path.path, "456");

    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, "3.txt");
    save_text_to (write_path.path, "789");

    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, "4.txt");
    save_text_to (write_path.path, "0");

    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, "5.txt");
    save_text_to (write_path.path, "1111");

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "test.rd");

        struct sum_resource_raw_t raw;
        sum_resource_raw_init (&raw);
        kan_dynamic_array_set_capacity (&raw.sources, 4u);

        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern ("1.txt");
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern ("2.txt");
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern ("3.txt");
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern ("4.txt");

        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_raw_t), &raw);
        sum_resource_raw_shutdown (&raw);
    }

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "root.rd");

        struct root_resource_t root;
        root_resource_init (&root);

        kan_dynamic_array_set_capacity (&root.needed_sums, 1u);
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&root.needed_sums) = KAN_STATIC_INTERNED_ID_GET (test);

        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (root_resource_t), &root);
        root_resource_shutdown (&root);
    }

    const enum kan_resource_build_result_t result = kan_resource_build (&setup);
    KAN_TEST_CHECK (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

    struct kan_file_system_path_container_t read_path;
    kan_file_system_path_container_copy_string (&read_path, WORKSPACE_DIRECTORY);
    const kan_instance_size_t read_path_base_length = read_path.length;

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_cache_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_parsed_source_t",
                                                           "1.txt");

        struct sum_parsed_source_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_parsed_source_t), &resource);
        KAN_TEST_CHECK (resource.source_number == 123u)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_cache_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_parsed_source_t",
                                                           "2.txt");

        struct sum_parsed_source_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_parsed_source_t), &resource);
        KAN_TEST_CHECK (resource.source_number == 456u)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_cache_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_parsed_source_t",
                                                           "3.txt");

        struct sum_parsed_source_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_parsed_source_t), &resource);
        KAN_TEST_CHECK (resource.source_number == 789u)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_cache_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_parsed_source_t",
                                                           "4.txt");

        struct sum_parsed_source_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_parsed_source_t), &resource);
        KAN_TEST_CHECK (resource.source_number == 0u)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_cache_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_parsed_source_t",
                                                           "5.txt");
        KAN_TEST_CHECK (!kan_file_system_check_existence (read_path.path))
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", "test");

        struct sum_resource_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_t), &resource);
        KAN_TEST_CHECK (resource.sum == 1368u)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "root_resource_t", "root");

        struct root_resource_t resource;
        root_resource_init (&resource);
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (root_resource_t), &resource);
        KAN_TEST_ASSERT (resource.needed_sums.size == 1u)
        KAN_TEST_ASSERT (((kan_interned_string_t *) resource.needed_sums.data)[0u] == KAN_STATIC_INTERNED_ID_GET (test))
        root_resource_shutdown (&resource);
    }
}

KAN_TEST_CASE (rebuild)
{
    SETUP_TRIVIAL_TEST_ENVIRONMENT;

    struct kan_file_system_path_container_t write_path;
    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, "1.txt");
    save_text_to (write_path.path, "1");

    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, "2.txt");
    save_text_to (write_path.path, "2");

    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, "3.txt");
    save_text_to (write_path.path, "3");

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "test_1_2.rd");

        struct sum_resource_raw_t raw;
        sum_resource_raw_init (&raw);
        kan_dynamic_array_set_capacity (&raw.sources, 2u);

        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern ("1.txt");
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern ("2.txt");

        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_raw_t), &raw);
        sum_resource_raw_shutdown (&raw);
    }

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "test_2_3.rd");

        struct sum_resource_raw_t raw;
        sum_resource_raw_init (&raw);
        kan_dynamic_array_set_capacity (&raw.sources, 2u);

        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern ("2.txt");
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern ("3.txt");

        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_raw_t), &raw);
        sum_resource_raw_shutdown (&raw);
    }

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "root.rd");

        struct root_resource_t root;
        root_resource_init (&root);

        kan_dynamic_array_set_capacity (&root.needed_sums, 2u);
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&root.needed_sums) =
            KAN_STATIC_INTERNED_ID_GET (test_1_2);
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&root.needed_sums) =
            KAN_STATIC_INTERNED_ID_GET (test_2_3);

        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (root_resource_t), &root);
        root_resource_shutdown (&root);
    }

    enum kan_resource_build_result_t result = kan_resource_build (&setup);
    KAN_TEST_CHECK (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

    kan_time_size_t last_build_time_test_1_2;
    kan_time_size_t last_build_time_test_2_3;

    struct kan_file_system_path_container_t read_path;
    kan_file_system_path_container_copy_string (&read_path, WORKSPACE_DIRECTORY);
    const kan_instance_size_t read_path_base_length = read_path.length;

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", "test_1_2");

        struct sum_resource_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_t), &resource);
        KAN_TEST_CHECK (resource.sum == 3u)

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        last_build_time_test_1_2 = status.last_modification_time_ns;
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", "test_2_3");

        struct sum_resource_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_t), &resource);
        KAN_TEST_CHECK (resource.sum == 5u)

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        last_build_time_test_2_3 = status.last_modification_time_ns;
    }

    result = kan_resource_build (&setup);
    KAN_TEST_CHECK (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", "test_1_2");

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        KAN_TEST_CHECK (last_build_time_test_1_2 == status.last_modification_time_ns)
        last_build_time_test_1_2 = status.last_modification_time_ns;
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", "test_2_3");

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        KAN_TEST_CHECK (last_build_time_test_2_3 == status.last_modification_time_ns)
        last_build_time_test_2_3 = status.last_modification_time_ns;
    }

    // Sleep some time before doing next build to avoid error with unchanged last modification time because
    // changes were too close to each to other for filesystem to change modification time.
    kan_precise_time_sleep (10000000u);

    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, "1.txt");
    save_text_to (write_path.path, "10");

    result = kan_resource_build (&setup);
    KAN_TEST_CHECK (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", "test_1_2");

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        KAN_TEST_CHECK (last_build_time_test_1_2 != status.last_modification_time_ns)
        last_build_time_test_1_2 = status.last_modification_time_ns;

        struct sum_resource_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_t), &resource);
        KAN_TEST_CHECK (resource.sum == 12u)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", "test_2_3");

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        KAN_TEST_CHECK (last_build_time_test_2_3 == status.last_modification_time_ns)
        last_build_time_test_2_3 = status.last_modification_time_ns;
    }

    // Sleep some time before doing next build to avoid error with unchanged last modification time because
    // changes were too close to each to other for filesystem to change modification time.
    kan_precise_time_sleep (10000000u);

    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, "2.txt");
    save_text_to (write_path.path, "20");

    result = kan_resource_build (&setup);
    KAN_TEST_CHECK (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", "test_1_2");

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        KAN_TEST_CHECK (last_build_time_test_1_2 != status.last_modification_time_ns)
        last_build_time_test_1_2 = status.last_modification_time_ns;

        struct sum_resource_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_t), &resource);
        KAN_TEST_CHECK (resource.sum == 30u)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", "test_2_3");

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        KAN_TEST_CHECK (last_build_time_test_2_3 != status.last_modification_time_ns)
        last_build_time_test_2_3 = status.last_modification_time_ns;

        struct sum_resource_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_t), &resource);
        KAN_TEST_CHECK (resource.sum == 23u)
    }

    // Sleep some time before doing next build to avoid error with unchanged last modification time because
    // changes were too close to each to other for filesystem to change modification time.
    kan_precise_time_sleep (10000000u);

    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, "3.txt");
    save_text_to (write_path.path, "30");

    result = kan_resource_build (&setup);
    KAN_TEST_CHECK (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", "test_1_2");

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        KAN_TEST_CHECK (last_build_time_test_1_2 == status.last_modification_time_ns)
        last_build_time_test_1_2 = status.last_modification_time_ns;
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", "test_2_3");

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        KAN_TEST_CHECK (last_build_time_test_2_3 != status.last_modification_time_ns)
        last_build_time_test_2_3 = status.last_modification_time_ns;

        struct sum_resource_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_t), &resource);
        KAN_TEST_CHECK (resource.sum == 50u)
    }
}

KAN_TEST_CASE (references)
{
    // TODO: Test.
}

KAN_TEST_CASE (secondary)
{
    // TODO: Test.
}

KAN_TEST_CASE (target_visibility)
{
    // TODO: Test.
}

KAN_TEST_CASE (platform_unsupported_dependency)
{
    // TODO: Test.
}

KAN_TEST_CASE (platform_configuration)
{
    // TODO: Test.
}

KAN_TEST_CASE (scale)
{
    // TODO: Test.
}
