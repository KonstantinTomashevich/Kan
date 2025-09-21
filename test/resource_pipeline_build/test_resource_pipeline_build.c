#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

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
#include <kan/resource_pipeline/index.h>
#include <kan/resource_pipeline/meta.h>
#include <kan/resource_pipeline/platform_configuration.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>
#include <kan/testing/testing.h>
#include <kan/virtual_file_system/virtual_file_system.h>

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

struct sum_parse_platform_configuration_t
{
    kan_instance_size_t max_parsed_value;
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
    .flags = KAN_RESOURCE_REFERENCE_META_PLATFORM_OPTIONAL,
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

struct secondary_producer_resource_raw_t
{
    kan_instance_size_t count_to_produce;
};

KAN_REFLECTION_STRUCT_META (secondary_producer_resource_raw_t)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_type_meta_t secondary_producer_resource_raw_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

struct secondary_resource_raw_t
{
    kan_instance_size_t index_in_producer;
};

KAN_REFLECTION_STRUCT_META (secondary_resource_raw_t)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_type_meta_t secondary_resource_raw_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

struct secondary_resource_t
{
    kan_instance_size_t multiplied_index_in_producer;
};

KAN_REFLECTION_STRUCT_META (secondary_resource_t)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_type_meta_t secondary_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

struct secondary_producer_resource_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t produced;
};

KAN_REFLECTION_STRUCT_META (secondary_producer_resource_t)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_type_meta_t secondary_producer_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

KAN_REFLECTION_STRUCT_FIELD_META (secondary_producer_resource_t, produced)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_reference_meta_t secondary_producer_resource_reference_produced = {
    .type_name = "secondary_resource_t",
    .flags = 0u,
};

TEST_RESOURCE_PIPELINE_BUILD_API void secondary_producer_resource_init (struct secondary_producer_resource_t *instance)
{
    kan_dynamic_array_init (&instance->produced, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
}

TEST_RESOURCE_PIPELINE_BUILD_API void secondary_producer_resource_shutdown (
    struct secondary_producer_resource_t *instance)
{
    kan_dynamic_array_shutdown (&instance->produced);
}

struct root_resource_t
{
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t needed_sums;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t needed_secondary_producers;
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
    .flags = KAN_RESOURCE_REFERENCE_META_NULLABLE,
};

KAN_REFLECTION_STRUCT_FIELD_META (root_resource_t, needed_secondary_producers)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_reference_meta_t
    root_resource_reference_needed_secondary_producers = {
        .type_name = "secondary_producer_resource_t",
        .flags = KAN_RESOURCE_REFERENCE_META_NULLABLE,
};

TEST_RESOURCE_PIPELINE_BUILD_API void root_resource_init (struct root_resource_t *instance)
{
    kan_dynamic_array_init (&instance->needed_sums, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->needed_secondary_producers, 0u, sizeof (kan_interned_string_t),
                            alignof (kan_interned_string_t), kan_allocation_group_stack_get ());
}

TEST_RESOURCE_PIPELINE_BUILD_API void root_resource_shutdown (struct root_resource_t *instance)
{
    kan_dynamic_array_shutdown (&instance->needed_sums);
    kan_dynamic_array_shutdown (&instance->needed_secondary_producers);
}

static enum kan_resource_build_rule_result_t sum_parsed_source_build (
    struct kan_resource_build_rule_context_t *context);

KAN_REFLECTION_STRUCT_META (sum_parsed_source_t)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_build_rule_t sum_parsed_source_build_rule = {
    .primary_input_type = NULL,
    .platform_configuration_type = "sum_parse_platform_configuration_t",
    .secondary_types_count = 0u,
    .secondary_types = NULL,
    .functor = sum_parsed_source_build,
    .version = CUSHION_START_NS_X64,
};

static enum kan_resource_build_rule_result_t sum_parsed_source_build (struct kan_resource_build_rule_context_t *context)
{
    struct sum_parsed_source_t *output = context->primary_output;
    output->source_number = 0u;

    struct kan_stream_t *stream = kan_direct_file_stream_open_for_read (context->primary_third_party_path, true);
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

    const struct sum_parse_platform_configuration_t *configuration = context->platform_configuration;
    if (output->source_number > configuration->max_parsed_value)
    {
        return KAN_RESOURCE_BUILD_RULE_UNSUPPORTED;
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
        KAN_LOG (test_resource_pipeline_build, KAN_LOG_INFO,
                 "While building \"%s\", expected %u secondary inputs, but got %u. Unless it is a platform unsupported "
                 "dependency test, you should expect failure down the road and you could start debug from here.",
                 context->primary_name, (unsigned int) input->sources.size, (unsigned int) secondary_received)
    }

    return KAN_RESOURCE_BUILD_RULE_SUCCESS;
}

static enum kan_resource_build_rule_result_t secondary_resource_build (
    struct kan_resource_build_rule_context_t *context);

KAN_REFLECTION_STRUCT_META (secondary_resource_t)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_build_rule_t secondary_resource_build_rule = {
    .primary_input_type = "secondary_resource_raw_t",
    .platform_configuration_type = NULL,
    .secondary_types_count = 0u,
    .secondary_types = NULL,
    .functor = secondary_resource_build,
    .version = CUSHION_START_NS_X64,
};

static enum kan_resource_build_rule_result_t secondary_resource_build (
    struct kan_resource_build_rule_context_t *context)
{
    const struct secondary_resource_raw_t *input = context->primary_input;
    struct secondary_resource_t *output = context->primary_output;
    output->multiplied_index_in_producer = input->index_in_producer * 2u;
    return KAN_RESOURCE_BUILD_RULE_SUCCESS;
}

static enum kan_resource_build_rule_result_t secondary_producer_resource_build (
    struct kan_resource_build_rule_context_t *context);

KAN_REFLECTION_STRUCT_META (secondary_producer_resource_t)
TEST_RESOURCE_PIPELINE_BUILD_API struct kan_resource_build_rule_t secondary_producer_resource_build_rule = {
    .primary_input_type = "secondary_producer_resource_raw_t",
    .platform_configuration_type = NULL,
    .secondary_types_count = 0u,
    .secondary_types = NULL,
    .functor = secondary_producer_resource_build,
    .version = CUSHION_START_NS_X64,
};

static enum kan_resource_build_rule_result_t secondary_producer_resource_build (
    struct kan_resource_build_rule_context_t *context)
{
    const struct secondary_producer_resource_raw_t *input = context->primary_input;
    struct secondary_producer_resource_t *output = context->primary_output;

    kan_dynamic_array_set_capacity (&output->produced, input->count_to_produce);
    struct secondary_resource_raw_t produced;

    for (kan_loop_size_t index = 0u; index < input->count_to_produce; ++index)
    {
        produced.index_in_producer = index;
        char name_buffer[256u];
        snprintf (name_buffer, sizeof (name_buffer), "%s_child_%u", context->primary_name, (unsigned int) index);

        kan_interned_string_t produced_name = kan_string_intern (name_buffer);
        if (!context->produce_secondary_output (
                context->interface, KAN_STATIC_INTERNED_ID_GET (secondary_resource_raw_t), produced_name, &produced))
        {
            KAN_LOG (test_resource_pipeline_build, KAN_LOG_ERROR, "Failed to produce \"%s\".", name_buffer)
            return KAN_RESOURCE_BUILD_RULE_FAILURE;
        }

        *(kan_interned_string_t *) kan_dynamic_array_add_last (&output->produced) = produced_name;
    }

    return KAN_RESOURCE_BUILD_RULE_SUCCESS;
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
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (path, true);
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

static void load_binary_extended (kan_serialization_binary_script_storage_t script_storage,
                                  struct kan_stream_t *stream,
                                  kan_interned_string_t type,
                                  void *data,
                                  kan_serialization_interned_string_registry_t string_registry)
{
    kan_interned_string_t actual_type;
    KAN_TEST_ASSERT (kan_serialization_binary_read_type_header (stream, &actual_type, string_registry))
    KAN_TEST_ASSERT (type == actual_type)

    kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
        stream, data, type, script_storage, string_registry, KAN_ALLOCATION_GROUP_IGNORE);

    CUSHION_DEFER { kan_serialization_binary_reader_destroy (reader); }
    enum kan_serialization_state_t state;

    while ((state = kan_serialization_binary_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
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

    load_binary_extended (script_storage, stream, type, data,
                          KAN_HANDLE_SET_INVALID (kan_serialization_interned_string_registry_t));
}

static void load_binary_from_vfs (kan_serialization_binary_script_storage_t script_storage,
                                  kan_virtual_file_system_volume_t volume,
                                  const char *path,
                                  kan_interned_string_t type,
                                  void *data,
                                  kan_serialization_interned_string_registry_t string_registry)
{
    struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (volume, path);
    KAN_TEST_ASSERT (stream)

    stream = kan_random_access_stream_buffer_open_for_read (stream, 4096u);
    CUSHION_DEFER { stream->operations->close (stream); }
    load_binary_extended (script_storage, stream, type, data, string_registry);
}

#define WORKSPACE_DIRECTORY "workspace"
#define PLATFORM_CONFIGURATION_DIRECTORY "platform_configuration"

static void initialize_platform_configuration (kan_reflection_registry_t registry)
{
    struct kan_file_system_path_container_t path;
    kan_file_system_path_container_copy_string (&path, PLATFORM_CONFIGURATION_DIRECTORY);
    kan_file_system_path_container_append (&path, KAN_RESOURCE_PLATFORM_CONFIGURATION_SETUP_FILE);

    struct kan_resource_platform_configuration_setup_t setup;
    kan_resource_platform_configuration_setup_init (&setup);
    CUSHION_DEFER { kan_resource_platform_configuration_setup_shutdown (&setup); }

    kan_dynamic_array_set_capacity (&setup.layers, 2u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&setup.layers) = KAN_STATIC_INTERNED_ID_GET (base);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&setup.layers) = KAN_STATIC_INTERNED_ID_GET (additional);

    save_rd_to (registry, path.path, KAN_STATIC_INTERNED_ID_GET (kan_resource_platform_configuration_setup_t), &setup);
    kan_reflection_patch_builder_t builder = kan_reflection_patch_builder_create ();
    CUSHION_DEFER { kan_reflection_patch_builder_destroy (builder); }

    const struct kan_reflection_struct_t *configuration_type = kan_reflection_registry_query_struct (
        registry, KAN_STATIC_INTERNED_ID_GET (sum_parse_platform_configuration_t));

    {
        struct sum_parse_platform_configuration_t configuration = {
            .max_parsed_value = 500u,
        };

        struct kan_resource_platform_configuration_entry_t entry;
        kan_resource_platform_configuration_entry_init (&entry);

        entry.layer = KAN_STATIC_INTERNED_ID_GET (base);
        kan_reflection_patch_builder_add_chunk (builder, KAN_REFLECTION_PATCH_BUILDER_SECTION_ROOT, 0u,
                                                sizeof (configuration), &configuration);
        entry.data = kan_reflection_patch_builder_build (builder, registry, configuration_type);

        KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (entry.data))
        kan_file_system_path_container_copy_string (&path, PLATFORM_CONFIGURATION_DIRECTORY);
        kan_file_system_path_container_append (&path, "base.rd");

        save_rd_to (registry, path.path, KAN_STATIC_INTERNED_ID_GET (kan_resource_platform_configuration_entry_t),
                    &entry);
        kan_resource_platform_configuration_entry_shutdown (&entry);
    }

    {
        struct sum_parse_platform_configuration_t configuration = {
            .max_parsed_value = 100u,
        };

        struct kan_resource_platform_configuration_entry_t entry;
        kan_resource_platform_configuration_entry_init (&entry);

        kan_dynamic_array_set_capacity (&entry.required_tags, 1u);
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&entry.required_tags) =
            KAN_STATIC_INTERNED_ID_GET (no_such_tag);

        entry.layer = KAN_STATIC_INTERNED_ID_GET (additional);
        kan_reflection_patch_builder_add_chunk (builder, KAN_REFLECTION_PATCH_BUILDER_SECTION_ROOT, 0u,
                                                sizeof (configuration), &configuration);
        entry.data = kan_reflection_patch_builder_build (builder, registry, configuration_type);

        KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (entry.data))
        kan_file_system_path_container_copy_string (&path, PLATFORM_CONFIGURATION_DIRECTORY);
        kan_file_system_path_container_append (&path, "child_1.rd");

        save_rd_to (registry, path.path, KAN_STATIC_INTERNED_ID_GET (kan_resource_platform_configuration_entry_t),
                    &entry);
        kan_resource_platform_configuration_entry_shutdown (&entry);
    }

    {
        struct sum_parse_platform_configuration_t configuration = {
            .max_parsed_value = 2000u,
        };

        struct kan_resource_platform_configuration_entry_t entry;
        kan_resource_platform_configuration_entry_init (&entry);

        kan_dynamic_array_set_capacity (&entry.required_tags, 1u);
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&entry.required_tags) =
            KAN_STATIC_INTERNED_ID_GET (additional_precision_tag);

        entry.layer = KAN_STATIC_INTERNED_ID_GET (additional);
        kan_reflection_patch_builder_add_chunk (builder, KAN_REFLECTION_PATCH_BUILDER_SECTION_ROOT, 0u,
                                                sizeof (configuration), &configuration);
        entry.data = kan_reflection_patch_builder_build (builder, registry, configuration_type);

        KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (entry.data))
        kan_file_system_path_container_copy_string (&path, PLATFORM_CONFIGURATION_DIRECTORY);
        kan_file_system_path_container_append (&path, "child_2.rd");

        save_rd_to (registry, path.path, KAN_STATIC_INTERNED_ID_GET (kan_resource_platform_configuration_entry_t),
                    &entry);
        kan_resource_platform_configuration_entry_shutdown (&entry);
    }
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
    initialize_platform_configuration (registry);                                                                      \
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
    setup.pack_mode = KAN_RESOURCE_BUILD_PACK_MODE_NONE;                                                               \
    setup.log_verbosity = KAN_LOG_VERBOSE;                                                                             \
                                                                                                                       \
    kan_dynamic_array_set_capacity (&setup.targets, 1u);                                                               \
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&setup.targets) = kan_string_intern (TEST_TARGET_NAME)

KAN_TEST_CASE (correctness)
{
    SETUP_TRIVIAL_TEST_ENVIRONMENT;
    kan_dynamic_array_set_capacity (&project.platform_configuration_tags, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&project.platform_configuration_tags) =
        KAN_STATIC_INTERNED_ID_GET (additional_precision_tag);

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
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

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
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

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
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

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
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

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
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

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
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

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
    SETUP_TRIVIAL_TEST_ENVIRONMENT;

    struct kan_file_system_path_container_t write_path;
    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, "1.txt");
    save_text_to (write_path.path, "1");

    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, "2.txt");
    save_text_to (write_path.path, "2");

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "test_1.rd");

        struct sum_resource_raw_t raw;
        sum_resource_raw_init (&raw);
        kan_dynamic_array_set_capacity (&raw.sources, 1u);
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern ("1.txt");
        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_raw_t), &raw);
        sum_resource_raw_shutdown (&raw);
    }

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "test_2.rd");

        struct sum_resource_raw_t raw;
        sum_resource_raw_init (&raw);
        kan_dynamic_array_set_capacity (&raw.sources, 1u);
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern ("2.txt");
        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_raw_t), &raw);
        sum_resource_raw_shutdown (&raw);
    }

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "root.rd");

        struct root_resource_t root;
        root_resource_init (&root);

        kan_dynamic_array_set_capacity (&root.needed_sums, 1u);
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&root.needed_sums) = KAN_STATIC_INTERNED_ID_GET (test_2);

        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (root_resource_t), &root);
        root_resource_shutdown (&root);
    }

    enum kan_resource_build_result_t result = kan_resource_build (&setup);
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

    struct kan_file_system_path_container_t read_path;
    kan_file_system_path_container_copy_string (&read_path, WORKSPACE_DIRECTORY);
    const kan_instance_size_t read_path_base_length = read_path.length;

    kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
    kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", "test_1");
    KAN_TEST_CHECK (!kan_file_system_check_existence (read_path.path))

    kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
    kan_resource_build_append_cache_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", "test_1");
    KAN_TEST_CHECK (!kan_file_system_check_existence (read_path.path))

    kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
    kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", "test_2");
    KAN_TEST_CHECK (kan_file_system_check_existence (read_path.path))

    kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
    kan_resource_build_append_cache_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_parsed_source_t", "1.txt");
    KAN_TEST_CHECK (!kan_file_system_check_existence (read_path.path))

    kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
    kan_resource_build_append_cache_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_parsed_source_t", "2.txt");
    KAN_TEST_CHECK (kan_file_system_check_existence (read_path.path))
}

KAN_TEST_CASE (secondary)
{
    SETUP_TRIVIAL_TEST_ENVIRONMENT;
    struct kan_file_system_path_container_t write_path;

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "test.rd");

        struct secondary_producer_resource_raw_t raw;
        raw.count_to_produce = 3u;
        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (secondary_producer_resource_raw_t), &raw);
    }

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "root.rd");

        struct root_resource_t root;
        root_resource_init (&root);

        kan_dynamic_array_set_capacity (&root.needed_secondary_producers, 1u);
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&root.needed_secondary_producers) =
            KAN_STATIC_INTERNED_ID_GET (test);

        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (root_resource_t), &root);
        root_resource_shutdown (&root);
    }

    enum kan_resource_build_result_t result = kan_resource_build (&setup);
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

    kan_time_size_t test_child_0_initial_time;
    kan_time_size_t test_initial_time;

    struct kan_file_system_path_container_t read_path;
    kan_file_system_path_container_copy_string (&read_path, WORKSPACE_DIRECTORY);
    const kan_instance_size_t read_path_base_length = read_path.length;

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_cache_path_in_workspace (&read_path, TEST_TARGET_NAME, "secondary_resource_raw_t",
                                                           "test_child_0");

        struct secondary_resource_raw_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (secondary_resource_raw_t),
                          &resource);
        KAN_TEST_CHECK (resource.index_in_producer == 0u)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_cache_path_in_workspace (&read_path, TEST_TARGET_NAME, "secondary_resource_raw_t",
                                                           "test_child_1");

        struct secondary_resource_raw_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (secondary_resource_raw_t),
                          &resource);
        KAN_TEST_CHECK (resource.index_in_producer == 1u)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_cache_path_in_workspace (&read_path, TEST_TARGET_NAME, "secondary_resource_raw_t",
                                                           "test_child_2");

        struct secondary_resource_raw_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (secondary_resource_raw_t),
                          &resource);
        KAN_TEST_CHECK (resource.index_in_producer == 2u)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "secondary_resource_t",
                                                            "test_child_0");

        struct secondary_resource_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (secondary_resource_t), &resource);
        KAN_TEST_CHECK (resource.multiplied_index_in_producer == 0u)

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        test_child_0_initial_time = status.last_modification_time_ns;
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "secondary_resource_t",
                                                            "test_child_1");

        struct secondary_resource_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (secondary_resource_t), &resource);
        KAN_TEST_CHECK (resource.multiplied_index_in_producer == 2u)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "secondary_resource_t",
                                                            "test_child_2");

        struct secondary_resource_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (secondary_resource_t), &resource);
        KAN_TEST_CHECK (resource.multiplied_index_in_producer == 4u)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME,
                                                            "secondary_producer_resource_t", "test");

        struct secondary_producer_resource_t resource;
        secondary_producer_resource_init (&resource);
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (secondary_producer_resource_t),
                          &resource);

        KAN_TEST_ASSERT (resource.produced.size == 3u)
        KAN_TEST_CHECK (((kan_interned_string_t *) resource.produced.data)[0u] =
                            KAN_STATIC_INTERNED_ID_GET (test_child_0))

        KAN_TEST_CHECK (((kan_interned_string_t *) resource.produced.data)[1u] =
                            KAN_STATIC_INTERNED_ID_GET (test_child_1))

        KAN_TEST_CHECK (((kan_interned_string_t *) resource.produced.data)[1u] =
                            KAN_STATIC_INTERNED_ID_GET (test_child_2))
        secondary_producer_resource_shutdown (&resource);

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        test_initial_time = status.last_modification_time_ns;
    }

    // Sleep some time before doing next build to avoid error with unchanged last modification time because
    // changes were too close to each to other for filesystem to change modification time.
    kan_precise_time_sleep (10000000u);

    result = kan_resource_build (&setup);
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_cache_path_in_workspace (&read_path, TEST_TARGET_NAME, "secondary_resource_raw_t",
                                                           "test_child_0");
        KAN_TEST_CHECK (kan_file_system_check_existence (read_path.path))
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "secondary_resource_t",
                                                            "test_child_0");

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        KAN_TEST_CHECK (test_child_0_initial_time == status.last_modification_time_ns)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME,
                                                            "secondary_producer_resource_t", "test");

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        KAN_TEST_CHECK (test_initial_time == status.last_modification_time_ns)
    }

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "test.rd");

        struct secondary_producer_resource_raw_t raw;
        raw.count_to_produce = 2u;
        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (secondary_producer_resource_raw_t), &raw);
    }

    // Sleep some time before doing next build to avoid error with unchanged last modification time because
    // changes were too close to each to other for filesystem to change modification time.
    kan_precise_time_sleep (10000000u);

    result = kan_resource_build (&setup);
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_cache_path_in_workspace (&read_path, TEST_TARGET_NAME, "secondary_resource_raw_t",
                                                           "test_child_0");
        KAN_TEST_CHECK (kan_file_system_check_existence (read_path.path))
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_cache_path_in_workspace (&read_path, TEST_TARGET_NAME, "secondary_resource_raw_t",
                                                           "test_child_2");
        KAN_TEST_CHECK (!kan_file_system_check_existence (read_path.path))
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "secondary_resource_t",
                                                            "test_child_0");

        struct secondary_resource_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (secondary_resource_t), &resource);
        KAN_TEST_CHECK (resource.multiplied_index_in_producer == 0u)

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        KAN_TEST_CHECK (test_child_0_initial_time != status.last_modification_time_ns)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "secondary_resource_t",
                                                            "test_child_1");

        struct secondary_resource_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (secondary_resource_t), &resource);
        KAN_TEST_CHECK (resource.multiplied_index_in_producer == 2u)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "secondary_resource_t",
                                                            "test_child_2");
        KAN_TEST_CHECK (!kan_file_system_check_existence (read_path.path))
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME,
                                                            "secondary_producer_resource_t", "test");

        struct secondary_producer_resource_t resource;
        secondary_producer_resource_init (&resource);
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (secondary_producer_resource_t),
                          &resource);

        KAN_TEST_ASSERT (resource.produced.size == 2u)
        KAN_TEST_CHECK (((kan_interned_string_t *) resource.produced.data)[0u] =
                            KAN_STATIC_INTERNED_ID_GET (test_child_0))

        KAN_TEST_CHECK (((kan_interned_string_t *) resource.produced.data)[1u] =
                            KAN_STATIC_INTERNED_ID_GET (test_child_1))
        secondary_producer_resource_shutdown (&resource);

        struct kan_file_system_entry_status_t status;
        KAN_TEST_ASSERT (kan_file_system_query_entry (read_path.path, &status))
        KAN_TEST_CHECK (test_initial_time != status.last_modification_time_ns)
    }
}

#define TEST_BASE_TARGET_NAME "test_base_target"
#define TEST_BASE_TARGET_RESOURCE_DIRECTORY "base_resources"

#define TEST_CHILD_TARGET_NAME "test_child_target"
#define TEST_CHILD_TARGET_RESOURCE_DIRECTORY "child_resources"

KAN_TEST_CASE (target_visibility)
{
    ensure_statics_initialized ();
    SETUP_CONTEXT_AND_GET_REFLECTION

    kan_file_system_remove_directory_with_content (TEST_BASE_TARGET_RESOURCE_DIRECTORY);
    KAN_TEST_CHECK (kan_file_system_make_directory (TEST_BASE_TARGET_RESOURCE_DIRECTORY))

    kan_file_system_remove_directory_with_content (TEST_CHILD_TARGET_RESOURCE_DIRECTORY);
    KAN_TEST_CHECK (kan_file_system_make_directory (TEST_CHILD_TARGET_RESOURCE_DIRECTORY))

    kan_file_system_remove_directory_with_content (WORKSPACE_DIRECTORY);
    KAN_TEST_CHECK (kan_file_system_make_directory (WORKSPACE_DIRECTORY))

    kan_file_system_remove_directory_with_content (PLATFORM_CONFIGURATION_DIRECTORY);
    KAN_TEST_CHECK (kan_file_system_make_directory (PLATFORM_CONFIGURATION_DIRECTORY))
    initialize_platform_configuration (registry);

    struct kan_resource_project_t project;
    kan_resource_project_init (&project);
    CUSHION_DEFER { kan_resource_project_shutdown (&project); }
    set_base_directories_to_project (&project);

    struct kan_resource_project_target_t *base_target = kan_dynamic_array_add_last (&project.targets);
    if (!base_target)
    {
        kan_dynamic_array_set_capacity (&project.targets, KAN_MAX (1u, project.targets.size * 2u));
        base_target = kan_dynamic_array_add_last (&project.targets);
    }

    kan_resource_project_target_init (base_target);
    base_target->name = kan_string_intern (TEST_BASE_TARGET_NAME);

    char *directory = kan_allocate_general (kan_resource_project_get_allocation_group (),
                                            sizeof (TEST_BASE_TARGET_RESOURCE_DIRECTORY), alignof (char));
    strcpy (directory, TEST_BASE_TARGET_RESOURCE_DIRECTORY);

    kan_dynamic_array_set_capacity (&base_target->directories, 1u);
    *(char **) kan_dynamic_array_add_last (&base_target->directories) = directory;

    struct kan_resource_project_target_t *child_target = kan_dynamic_array_add_last (&project.targets);
    if (!child_target)
    {
        kan_dynamic_array_set_capacity (&project.targets, KAN_MAX (1u, project.targets.size * 2u));
        child_target = kan_dynamic_array_add_last (&project.targets);
    }

    kan_resource_project_target_init (child_target);
    child_target->name = kan_string_intern (TEST_CHILD_TARGET_NAME);

    directory = kan_allocate_general (kan_resource_project_get_allocation_group (),
                                      sizeof (TEST_CHILD_TARGET_RESOURCE_DIRECTORY), alignof (char));
    strcpy (directory, TEST_CHILD_TARGET_RESOURCE_DIRECTORY);

    kan_dynamic_array_set_capacity (&child_target->directories, 1u);
    *(char **) kan_dynamic_array_add_last (&child_target->directories) = directory;

    kan_dynamic_array_set_capacity (&child_target->visible_targets, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&child_target->visible_targets) =
        kan_string_intern (TEST_BASE_TARGET_NAME);

    struct kan_resource_reflected_data_storage_t reflected_data;
    kan_resource_reflected_data_storage_build (&reflected_data, registry);
    CUSHION_DEFER { kan_resource_reflected_data_storage_shutdown (&reflected_data); }

    struct kan_resource_build_setup_t setup;
    kan_resource_build_setup_init (&setup);
    CUSHION_DEFER { kan_resource_build_setup_shutdown (&setup); };

    setup.project = &project;
    setup.reflected_data = &reflected_data;
    setup.pack_mode = KAN_RESOURCE_BUILD_PACK_MODE_NONE;
    setup.log_verbosity = KAN_LOG_VERBOSE;

    kan_dynamic_array_set_capacity (&setup.targets, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&setup.targets) = kan_string_intern (TEST_CHILD_TARGET_NAME);

    struct kan_file_system_path_container_t write_path;
    kan_file_system_path_container_copy_string (&write_path, TEST_BASE_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, "1.txt");
    save_text_to (write_path.path, "123");

    kan_file_system_path_container_copy_string (&write_path, TEST_BASE_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, "2.txt");
    save_text_to (write_path.path, "456");

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_CHILD_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "test.rd");

        struct sum_resource_raw_t raw;
        sum_resource_raw_init (&raw);
        kan_dynamic_array_set_capacity (&raw.sources, 4u);

        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern ("1.txt");
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern ("2.txt");

        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_raw_t), &raw);
        sum_resource_raw_shutdown (&raw);
    }

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_CHILD_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "root.rd");

        struct root_resource_t root;
        root_resource_init (&root);

        kan_dynamic_array_set_capacity (&root.needed_sums, 1u);
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&root.needed_sums) = KAN_STATIC_INTERNED_ID_GET (test);

        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (root_resource_t), &root);
        root_resource_shutdown (&root);
    }

    const enum kan_resource_build_result_t result = kan_resource_build (&setup);
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

    struct kan_file_system_path_container_t read_path;
    kan_file_system_path_container_copy_string (&read_path, WORKSPACE_DIRECTORY);
    const kan_instance_size_t read_path_base_length = read_path.length;

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_cache_path_in_workspace (&read_path, TEST_BASE_TARGET_NAME, "sum_parsed_source_t",
                                                           "1.txt");

        struct sum_parsed_source_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_parsed_source_t), &resource);
        KAN_TEST_CHECK (resource.source_number == 123u)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_CHILD_TARGET_NAME, "sum_resource_t",
                                                            "test");

        struct sum_resource_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_t), &resource);
        KAN_TEST_CHECK (resource.sum == 579u)
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_CHILD_TARGET_NAME, "root_resource_t",
                                                            "root");

        struct root_resource_t resource;
        root_resource_init (&resource);
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (root_resource_t), &resource);
        KAN_TEST_ASSERT (resource.needed_sums.size == 1u)
        KAN_TEST_ASSERT (((kan_interned_string_t *) resource.needed_sums.data)[0u] == KAN_STATIC_INTERNED_ID_GET (test))
        root_resource_shutdown (&resource);
    }
}

KAN_TEST_CASE (platform_unsupported_dependency)
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

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "test.rd");

        struct sum_resource_raw_t raw;
        sum_resource_raw_init (&raw);
        kan_dynamic_array_set_capacity (&raw.sources, 3u);

        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern ("1.txt");
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

        kan_dynamic_array_set_capacity (&root.needed_sums, 1u);
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&root.needed_sums) = KAN_STATIC_INTERNED_ID_GET (test);

        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (root_resource_t), &root);
        root_resource_shutdown (&root);
    }

    const enum kan_resource_build_result_t result = kan_resource_build (&setup);
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

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
        KAN_TEST_CHECK (!kan_file_system_check_existence (read_path.path))
    }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", "test");

        struct sum_resource_t resource;
        load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_t), &resource);
        KAN_TEST_CHECK (resource.sum == 579u)
    }
}

#define SCALE_TXT_DIR "txt"
#define SCALE_SUM_DIR "sum"
#define SCALE_SECONDARY_DIR "secondary"
#define SCALE_SIZE_SUM 800u
#define SCALE_REFERENCED_SUM (SCALE_SIZE_SUM * 4u / 5u)
#define SCALE_SIZE_SECONDARY 100u
#define SCALE_REFERENCED_SECONDARY (SCALE_SIZE_SECONDARY * 4u / 5u)

KAN_TEST_CASE (scale)
{
    SETUP_TRIVIAL_TEST_ENVIRONMENT;
    struct kan_file_system_path_container_t write_path;

    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, SCALE_TXT_DIR);
    KAN_TEST_CHECK (kan_file_system_make_directory (write_path.path))

    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, SCALE_SUM_DIR);
    KAN_TEST_CHECK (kan_file_system_make_directory (write_path.path))

    kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
    kan_file_system_path_container_append (&write_path, SCALE_SECONDARY_DIR);
    KAN_TEST_CHECK (kan_file_system_make_directory (write_path.path))

    for (kan_loop_size_t index = 0u; index < SCALE_SIZE_SUM; ++index)
    {
        char buffer[128u];

        // Generate text.
        snprintf (buffer, sizeof (buffer), "%u.txt", (unsigned int) index);
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, SCALE_TXT_DIR);
        kan_file_system_path_container_append (&write_path, buffer);
        save_text_to (write_path.path, index % 2u ? "42" : "17");

        // Generate sum.
        snprintf (buffer, sizeof (buffer), "%u.rd", (unsigned int) index);
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, SCALE_SUM_DIR);
        kan_file_system_path_container_append (&write_path, buffer);

        struct sum_resource_raw_t raw;
        sum_resource_raw_init (&raw);
        kan_dynamic_array_set_capacity (&raw.sources, 2u);

        snprintf (buffer, sizeof (buffer), "%u.txt", (unsigned int) index);
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern (buffer);

        snprintf (buffer, sizeof (buffer), "%u.txt", (unsigned int) (index + 1u) % SCALE_SIZE_SUM);
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern (buffer);

        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_raw_t), &raw);
        sum_resource_raw_shutdown (&raw);
    }

    for (kan_loop_size_t index = 0u; index < SCALE_SIZE_SECONDARY; ++index)
    {
        char buffer[128u];
        snprintf (buffer, sizeof (buffer), "%u.rd", (unsigned int) index);
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, SCALE_SECONDARY_DIR);
        kan_file_system_path_container_append (&write_path, buffer);

        struct secondary_producer_resource_raw_t raw;
        raw.count_to_produce = 3u;
        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (secondary_producer_resource_raw_t), &raw);
    }

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "root.rd");

        struct root_resource_t root;
        root_resource_init (&root);

        kan_dynamic_array_set_capacity (&root.needed_sums, SCALE_REFERENCED_SUM);
        kan_dynamic_array_set_capacity (&root.needed_secondary_producers, SCALE_REFERENCED_SECONDARY);
        char buffer[128u];

        for (kan_loop_size_t index = 0u; index < SCALE_REFERENCED_SUM; ++index)
        {
            snprintf (buffer, sizeof (buffer), "%u", (unsigned int) index);
            *(kan_interned_string_t *) kan_dynamic_array_add_last (&root.needed_sums) = kan_string_intern (buffer);
        }

        for (kan_loop_size_t index = 0u; index < SCALE_REFERENCED_SECONDARY; ++index)
        {
            snprintf (buffer, sizeof (buffer), "%u", (unsigned int) index);
            *(kan_interned_string_t *) kan_dynamic_array_add_last (&root.needed_secondary_producers) =
                kan_string_intern (buffer);
        }

        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (root_resource_t), &root);
        root_resource_shutdown (&root);
    }

    enum kan_resource_build_result_t result = kan_resource_build (&setup);
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

    // Do trivial correctness check.
    struct kan_file_system_path_container_t read_path;
    kan_file_system_path_container_copy_string (&read_path, WORKSPACE_DIRECTORY);
    const kan_instance_size_t read_path_base_length = read_path.length;

    for (kan_loop_size_t index = 0u; index < SCALE_SIZE_SUM; ++index)
    {
        char buffer[128u];
        snprintf (buffer, sizeof (buffer), "%u", (unsigned int) index);

        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME, "sum_resource_t", buffer);

        if (index < SCALE_REFERENCED_SUM)
        {
            struct sum_resource_t resource;
            load_binary_from (script_storage, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_t), &resource);
            KAN_TEST_CHECK (resource.sum == 59u)
        }
        else
        {
            KAN_TEST_CHECK (!kan_file_system_check_existence (read_path.path))
        }
    }

    for (kan_loop_size_t index = 0u; index < SCALE_SIZE_SECONDARY; ++index)
    {
        char buffer[128u];
        snprintf (buffer, sizeof (buffer), "%u", (unsigned int) index);

        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_resource_build_append_deploy_path_in_workspace (&read_path, TEST_TARGET_NAME,
                                                            "secondary_producer_resource_t", buffer);

        if (index < SCALE_REFERENCED_SECONDARY)
        {
            struct secondary_producer_resource_t resource;
            secondary_producer_resource_init (&resource);

            load_binary_from (script_storage, read_path.path,
                              KAN_STATIC_INTERNED_ID_GET (secondary_producer_resource_t), &resource);

            KAN_TEST_CHECK (resource.produced.size == 3u)
            secondary_producer_resource_shutdown (&resource);
        }
        else
        {
            KAN_TEST_CHECK (!kan_file_system_check_existence (read_path.path))
        }
    }

    // Second build to measure up-to-date check.
    result = kan_resource_build (&setup);
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)
}

#define PACK_SIZE_SUM 100u
#define PACK_SIZE_SECONDARY 50u
#define PACK_MOUNT_PATH "mounted"

KAN_TEST_CASE (pack)
{
    SETUP_TRIVIAL_TEST_ENVIRONMENT;
    setup.pack_mode = KAN_RESOURCE_BUILD_PACK_MODE_INTERNED;
    struct kan_file_system_path_container_t write_path;

    for (kan_loop_size_t index = 0u; index < PACK_SIZE_SUM; ++index)
    {
        char buffer[128u];

        // Generate text.
        snprintf (buffer, sizeof (buffer), "%u.txt", (unsigned int) index);
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, buffer);
        save_text_to (write_path.path, index % 2u ? "42" : "17");

        // Generate sum.
        snprintf (buffer, sizeof (buffer), "sum_%u.rd", (unsigned int) index);
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, buffer);

        struct sum_resource_raw_t raw;
        sum_resource_raw_init (&raw);
        kan_dynamic_array_set_capacity (&raw.sources, 2u);

        snprintf (buffer, sizeof (buffer), "%u.txt", (unsigned int) index);
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern (buffer);

        snprintf (buffer, sizeof (buffer), "%u.txt", (unsigned int) (index + 1u) % PACK_SIZE_SUM);
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&raw.sources) = kan_string_intern (buffer);

        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_raw_t), &raw);
        sum_resource_raw_shutdown (&raw);
    }

    for (kan_loop_size_t index = 0u; index < PACK_SIZE_SECONDARY; ++index)
    {
        char buffer[128u];
        snprintf (buffer, sizeof (buffer), "secondary_%u.rd", (unsigned int) index);
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, buffer);

        struct secondary_producer_resource_raw_t raw;
        raw.count_to_produce = 3u;
        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (secondary_producer_resource_raw_t), &raw);
    }

    {
        kan_file_system_path_container_copy_string (&write_path, TEST_TARGET_RESOURCE_DIRECTORY);
        kan_file_system_path_container_append (&write_path, "root.rd");

        struct root_resource_t root;
        root_resource_init (&root);

        kan_dynamic_array_set_capacity (&root.needed_sums, PACK_SIZE_SUM);
        kan_dynamic_array_set_capacity (&root.needed_secondary_producers, PACK_SIZE_SECONDARY);
        char buffer[128u];

        for (kan_loop_size_t index = 0u; index < PACK_SIZE_SUM; ++index)
        {
            snprintf (buffer, sizeof (buffer), "sum_%u", (unsigned int) index);
            *(kan_interned_string_t *) kan_dynamic_array_add_last (&root.needed_sums) = kan_string_intern (buffer);
        }

        for (kan_loop_size_t index = 0u; index < PACK_SIZE_SECONDARY; ++index)
        {
            snprintf (buffer, sizeof (buffer), "secondary_%u", (unsigned int) index);
            *(kan_interned_string_t *) kan_dynamic_array_add_last (&root.needed_secondary_producers) =
                kan_string_intern (buffer);
        }

        save_rd_to (registry, write_path.path, KAN_STATIC_INTERNED_ID_GET (root_resource_t), &root);
        root_resource_shutdown (&root);
    }

    const enum kan_resource_build_result_t result = kan_resource_build (&setup);
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)

    kan_virtual_file_system_volume_t volume = kan_virtual_file_system_volume_create ();
    CUSHION_DEFER { kan_virtual_file_system_volume_destroy (volume); }

    struct kan_file_system_path_container_t pack_path;
    kan_file_system_path_container_copy_string (&pack_path, WORKSPACE_DIRECTORY);
    kan_resource_build_append_pack_path_in_workspace (&pack_path, TEST_TARGET_NAME);
    KAN_TEST_ASSERT (kan_virtual_file_system_volume_mount_read_only_pack (volume, PACK_MOUNT_PATH, pack_path.path))

    struct kan_file_system_path_container_t read_path;
    kan_file_system_path_container_copy_string (&read_path, PACK_MOUNT_PATH);
    const kan_instance_size_t read_path_base_length = read_path.length;

    kan_serialization_interned_string_registry_t interned_string_registry = KAN_HANDLE_INITIALIZE_INVALID;
    CUSHION_DEFER { kan_serialization_interned_string_registry_destroy (interned_string_registry); }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_file_system_path_container_append (&read_path,
                                               KAN_RESOURCE_INDEX_ACCOMPANYING_STRING_REGISTRY_DEFAULT_NAME);

        struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (volume, read_path.path);
        KAN_TEST_ASSERT (stream)

        stream = kan_random_access_stream_buffer_open_for_read (stream, 4096u);
        CUSHION_DEFER { stream->operations->close (stream); }

        struct handle_struct_for_kan_serialization_interned_string_registry_reader_t reader =
            kan_serialization_interned_string_registry_reader_create (stream, true);
        CUSHION_DEFER { kan_serialization_interned_string_registry_reader_destroy (reader); }

        enum kan_serialization_state_t state;
        while ((state = kan_serialization_interned_string_registry_reader_step (reader)) ==
               KAN_SERIALIZATION_IN_PROGRESS)
        {
        }

        KAN_TEST_ASSERT (state == KAN_SERIALIZATION_FINISHED)
        interned_string_registry = kan_serialization_interned_string_registry_reader_get (reader);
    }

    struct kan_resource_index_t resource_index;
    kan_resource_index_init (&resource_index);
    CUSHION_DEFER { kan_resource_index_shutdown (&resource_index); }

    {
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_file_system_path_container_append (&read_path, KAN_RESOURCE_INDEX_DEFAULT_NAME);

        struct kan_stream_t *stream = kan_virtual_file_stream_open_for_read (volume, read_path.path);
        KAN_TEST_ASSERT (stream)

        stream = kan_random_access_stream_buffer_open_for_read (stream, 4096u);
        CUSHION_DEFER { stream->operations->close (stream); }

        kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
            stream, &resource_index, KAN_STATIC_INTERNED_ID_GET (kan_resource_index_t), script_storage,
            interned_string_registry, kan_resource_index_get_allocation_group ());

        CUSHION_DEFER { kan_serialization_binary_reader_destroy (reader); }
        enum kan_serialization_state_t state;

        while ((state = kan_serialization_binary_reader_step (reader)) == KAN_SERIALIZATION_IN_PROGRESS)
        {
        }

        KAN_TEST_ASSERT (state == KAN_SERIALIZATION_FINISHED)
    }

    struct kan_resource_index_container_t *root_container = NULL;
    struct kan_resource_index_container_t *producer_container = NULL;
    struct kan_resource_index_container_t *secondary_container = NULL;
    struct kan_resource_index_container_t *sum_container = NULL;
    KAN_TEST_ASSERT (resource_index.containers.size == 4u)

    for (kan_loop_size_t index = 0u; index < resource_index.containers.size; ++index)
    {
        struct kan_resource_index_container_t *container =
            &((struct kan_resource_index_container_t *) resource_index.containers.data)[index];
        if (container->type == KAN_STATIC_INTERNED_ID_GET (root_resource_t))
        {
            KAN_TEST_ASSERT (!root_container)
            root_container = container;
        }
        else if (container->type == KAN_STATIC_INTERNED_ID_GET (secondary_producer_resource_t))
        {
            KAN_TEST_ASSERT (!producer_container)
            producer_container = container;
        }
        else if (container->type == KAN_STATIC_INTERNED_ID_GET (secondary_resource_t))
        {
            KAN_TEST_ASSERT (!secondary_container)
            secondary_container = container;
        }
        else if (container->type == KAN_STATIC_INTERNED_ID_GET (sum_resource_t))
        {
            KAN_TEST_ASSERT (!sum_container)
            sum_container = container;
        }
    }

    KAN_TEST_ASSERT (root_container)
    KAN_TEST_ASSERT (producer_container)
    KAN_TEST_ASSERT (secondary_container)
    KAN_TEST_ASSERT (sum_container)

    KAN_TEST_ASSERT (root_container->items.size == 1u)
    KAN_TEST_ASSERT (producer_container->items.size == PACK_SIZE_SECONDARY)
    KAN_TEST_ASSERT (secondary_container->items.size == PACK_SIZE_SECONDARY * 3u)
    KAN_TEST_ASSERT (sum_container->items.size == PACK_SIZE_SUM)

    struct kan_resource_index_item_t *root_index_item =
        &((struct kan_resource_index_item_t *) root_container->items.data)[0u];
    KAN_TEST_ASSERT (root_index_item->name == KAN_STATIC_INTERNED_ID_GET (root))

    kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
    kan_file_system_path_container_append (&read_path, root_index_item->path);

    struct root_resource_t root_resource;
    root_resource_init (&root_resource);
    CUSHION_DEFER { root_resource_shutdown (&root_resource); }
    load_binary_from_vfs (script_storage, volume, read_path.path, KAN_STATIC_INTERNED_ID_GET (root_resource_t),
                          &root_resource, interned_string_registry);

    KAN_TEST_CHECK (root_resource.needed_sums.size == PACK_SIZE_SUM)
    KAN_TEST_CHECK (root_resource.needed_secondary_producers.size == PACK_SIZE_SECONDARY)

    for (kan_loop_size_t index = 0u; index < root_resource.needed_sums.size; ++index)
    {
        kan_interned_string_t name = ((kan_interned_string_t *) root_resource.needed_sums.data)[index];
        struct kan_resource_index_item_t *index_item = NULL;

        for (kan_loop_size_t search_index = 0u; search_index < sum_container->items.size; ++search_index)
        {
            struct kan_resource_index_item_t *item =
                &((struct kan_resource_index_item_t *) sum_container->items.data)[search_index];

            if (item->name == name)
            {
                index_item = item;
                break;
            }
        }

        KAN_TEST_ASSERT (index_item)
        kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
        kan_file_system_path_container_append (&read_path, index_item->path);

        struct sum_resource_t resource;
        load_binary_from_vfs (script_storage, volume, read_path.path, KAN_STATIC_INTERNED_ID_GET (sum_resource_t),
                              &resource, interned_string_registry);
        KAN_TEST_CHECK (resource.sum == 59u)
    }

    for (kan_loop_size_t producer_index = 0u; producer_index < root_resource.needed_secondary_producers.size;
         ++producer_index)
    {
        {
            kan_interned_string_t name =
                ((kan_interned_string_t *) root_resource.needed_secondary_producers.data)[producer_index];
            struct kan_resource_index_item_t *index_item = NULL;

            for (kan_loop_size_t search_index = 0u; search_index < producer_container->items.size; ++search_index)
            {
                struct kan_resource_index_item_t *item =
                    &((struct kan_resource_index_item_t *) producer_container->items.data)[search_index];

                if (item->name == name)
                {
                    index_item = item;
                    break;
                }
            }

            KAN_TEST_ASSERT (index_item)
            kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
            kan_file_system_path_container_append (&read_path, index_item->path);
        }

        struct secondary_producer_resource_t producer_resource;
        secondary_producer_resource_init (&producer_resource);
        CUSHION_DEFER { secondary_producer_resource_shutdown (&producer_resource); }

        load_binary_from_vfs (script_storage, volume, read_path.path,
                              KAN_STATIC_INTERNED_ID_GET (secondary_producer_resource_t), &producer_resource,
                              interned_string_registry);
        KAN_TEST_CHECK (producer_resource.produced.size == 3u)

        for (kan_loop_size_t secondary_index = 0u; secondary_index < producer_resource.produced.size; ++secondary_index)
        {
            kan_interned_string_t name = ((kan_interned_string_t *) producer_resource.produced.data)[secondary_index];
            struct kan_resource_index_item_t *index_item = NULL;

            for (kan_loop_size_t search_index = 0u; search_index < secondary_container->items.size; ++search_index)
            {
                struct kan_resource_index_item_t *item =
                    &((struct kan_resource_index_item_t *) secondary_container->items.data)[search_index];

                if (item->name == name)
                {
                    index_item = item;
                    break;
                }
            }

            KAN_TEST_ASSERT (index_item)
            kan_file_system_path_container_reset_length (&read_path, read_path_base_length);
            kan_file_system_path_container_append (&read_path, index_item->path);

            struct secondary_resource_t resource;
            load_binary_from_vfs (script_storage, volume, read_path.path,
                                  KAN_STATIC_INTERNED_ID_GET (secondary_resource_t), &resource,
                                  interned_string_registry);
            KAN_TEST_CHECK (resource.multiplied_index_in_producer == (kan_instance_size_t) secondary_index * 2u)
        }
    }
}
