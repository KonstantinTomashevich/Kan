#define _CRT_SECURE_NO_WARNINGS

#include <test_universe_resource_reference_api.h>

#include <stddef.h>

#include <kan/context/reflection_system.h>
#include <kan/context/universe_system.h>
#include <kan/context/update_system.h>
#include <kan/context/virtual_file_system.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/stream.h>
#include <kan/reflection/generated_reflection.h>
#include <kan/reflection/patch.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/serialization/readable_data.h>
#include <kan/testing/testing.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>
#include <kan/universe_resource_reference/universe_resource_reference.h>

#define WORKSPACE_RESOURCES_SUB_DIRECTORY "workspace_resources"
#define WORKSPACE_RESOURCES_MOUNT_PATH "resources"

#define WORKSPACE_REFERENCE_CACHE_SUB_DIRECTORY "workspace_reference_cache"
#define WORKSPACE_REFERENCE_CACHE_MOUNT_PATH "resource_reference_cache"

static kan_bool_t global_test_finished = KAN_FALSE;

struct resource_prototype_t
{
    /// \meta reflection_dynamic_array_type = "kan_reflection_patch_t"
    struct kan_dynamic_array_t components;
};

TEST_UNIVERSE_RESOURCE_REFERENCE_API void resource_prototype_init (struct resource_prototype_t *prototype)
{
    kan_dynamic_array_init (&prototype->components, 0u, sizeof (kan_reflection_patch_t),
                            _Alignof (kan_reflection_patch_t), KAN_ALLOCATION_GROUP_IGNORE);
}

TEST_UNIVERSE_RESOURCE_REFERENCE_API void resource_prototype_shutdown (struct resource_prototype_t *prototype)
{
    for (uint64_t index = 0u; index < prototype->components.size; ++index)
    {
        kan_reflection_patch_t patch = ((kan_reflection_patch_t *) prototype->components.data)[index];
        if (patch != KAN_INVALID_REFLECTION_PATCH)
        {
            kan_reflection_patch_destroy (patch);
        }
    }

    kan_dynamic_array_shutdown (&prototype->components);
}

// \meta reflection_struct_meta = "resource_prototype_t"
TEST_UNIVERSE_RESOURCE_REFERENCE_API struct kan_resource_pipeline_resource_type_meta_t resource_prototype_type_meta = {
    .root = KAN_TRUE};

struct prototype_component_t
{
    kan_interned_string_t inner_prototype;
};

TEST_UNIVERSE_RESOURCE_REFERENCE_API void prototype_component_init (struct prototype_component_t *instance)
{
    instance->inner_prototype = NULL;
}

// \meta reflection_struct_field_meta = "prototype_component_t.inner_prototype"
TEST_UNIVERSE_RESOURCE_REFERENCE_API struct kan_resource_pipeline_reference_meta_t
    prototype_component_inner_prototype_meta = {
        .type = "resource_prototype_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

struct config_component_t
{
    kan_interned_string_t optional_config_a;
    kan_interned_string_t optional_config_b;
};

TEST_UNIVERSE_RESOURCE_REFERENCE_API void config_component_init (struct config_component_t *instance)
{
    instance->optional_config_a = NULL;
    instance->optional_config_b = NULL;
}

// \meta reflection_struct_field_meta = "config_component_t.optional_config_a"
TEST_UNIVERSE_RESOURCE_REFERENCE_API struct kan_resource_pipeline_reference_meta_t
    config_component_optional_config_a_meta = {
        .type = "config_a_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

// \meta reflection_struct_field_meta = "config_component_t.optional_config_b"
TEST_UNIVERSE_RESOURCE_REFERENCE_API struct kan_resource_pipeline_reference_meta_t
    config_component_optional_config_b_meta = {
        .type = "config_b_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

struct config_a_t
{
    uint64_t x;
    uint64_t y;
};

TEST_UNIVERSE_RESOURCE_REFERENCE_API void config_a_init (struct config_a_t *instance)
{
    instance->x = 0u;
    instance->y = 0u;
}

// \meta reflection_struct_meta = "config_a_t"
TEST_UNIVERSE_RESOURCE_REFERENCE_API struct kan_resource_pipeline_resource_type_meta_t config_a_type_meta = {
    .root = KAN_TRUE};

struct config_b_t
{
    uint64_t data;
    kan_interned_string_t optional_config_a;
};

TEST_UNIVERSE_RESOURCE_REFERENCE_API void config_b_init (struct config_b_t *instance)
{
    instance->data = 0u;
    instance->optional_config_a = NULL;
}

// \meta reflection_struct_meta = "config_b_t"
TEST_UNIVERSE_RESOURCE_REFERENCE_API struct kan_resource_pipeline_resource_type_meta_t config_b_type_meta = {
    .root = KAN_TRUE};

// \meta reflection_struct_field_meta = "config_b_t.optional_config_a"
TEST_UNIVERSE_RESOURCE_REFERENCE_API struct kan_resource_pipeline_reference_meta_t config_b_optional_config_a_meta = {
    .type = "config_a_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

static void save_rd (const char *path, void *instance, kan_interned_string_t type, kan_reflection_registry_t registry)
{
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (path, KAN_TRUE);
    KAN_TEST_ASSERT (stream)
    KAN_TEST_ASSERT (kan_serialization_rd_write_type_header (stream, type))

    kan_serialization_rd_writer_t writer = kan_serialization_rd_writer_create (stream, instance, type, registry);
    enum kan_serialization_state_t state;

    while ((state = kan_serialization_rd_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    KAN_TEST_ASSERT (state == KAN_SERIALIZATION_FINISHED)
    kan_serialization_rd_writer_destroy (writer);
    stream->operations->close (stream);
}

static void save_prototype_1 (kan_reflection_registry_t registry, const char *path)
{
    const struct kan_reflection_struct_t *prototype_component_type =
        kan_reflection_registry_query_struct (registry, kan_string_intern ("prototype_component_t"));
    const struct kan_reflection_struct_t *config_component_type =
        kan_reflection_registry_query_struct (registry, kan_string_intern ("config_component_t"));

    kan_reflection_patch_builder_t patch_builder = kan_reflection_patch_builder_create ();
    struct resource_prototype_t prototype_1;
    resource_prototype_init (&prototype_1);
    kan_dynamic_array_set_capacity (&prototype_1.components, 3u);

    struct prototype_component_t prototype_component = {.inner_prototype = kan_string_intern ("prototype_2")};
    kan_reflection_patch_builder_add_chunk (patch_builder, 0u, sizeof (prototype_component), &prototype_component);
    *(kan_reflection_patch_t *) kan_dynamic_array_add_last (&prototype_1.components) =
        kan_reflection_patch_builder_build (patch_builder, registry, prototype_component_type);

    struct config_component_t config_component = {.optional_config_a = NULL,
                                                  .optional_config_b = kan_string_intern ("config_b_2")};
    kan_reflection_patch_builder_add_chunk (patch_builder, 0u, sizeof (config_component), &config_component);
    *(kan_reflection_patch_t *) kan_dynamic_array_add_last (&prototype_1.components) =
        kan_reflection_patch_builder_build (patch_builder, registry, config_component_type);

    // Just to ensure that serialization is able to deal with it.
    *(kan_reflection_patch_t *) kan_dynamic_array_add_last (&prototype_1.components) = KAN_INVALID_REFLECTION_PATCH;

    save_rd (path, &prototype_1, kan_string_intern ("resource_prototype_t"), registry);
    resource_prototype_shutdown (&prototype_1);

    kan_reflection_patch_builder_destroy (patch_builder);
}

static void save_prototype_2 (kan_reflection_registry_t registry, const char *path)
{
    const struct kan_reflection_struct_t *config_component_type =
        kan_reflection_registry_query_struct (registry, kan_string_intern ("config_component_t"));

    kan_reflection_patch_builder_t patch_builder = kan_reflection_patch_builder_create ();
    struct resource_prototype_t prototype_2;
    resource_prototype_init (&prototype_2);
    kan_dynamic_array_set_capacity (&prototype_2.components, 2u);

    struct config_component_t config_component_1 = {.optional_config_a = NULL,
                                                    .optional_config_b = kan_string_intern ("config_b_1")};
    kan_reflection_patch_builder_add_chunk (patch_builder, 0u, sizeof (config_component_1), &config_component_1);
    *(kan_reflection_patch_t *) kan_dynamic_array_add_last (&prototype_2.components) =
        kan_reflection_patch_builder_build (patch_builder, registry, config_component_type);

    struct config_component_t config_component_2 = {.optional_config_a = kan_string_intern ("config_a"),
                                                    .optional_config_b = kan_string_intern ("config_b_1")};
    kan_reflection_patch_builder_add_chunk (patch_builder, 0u, sizeof (config_component_2), &config_component_2);
    *(kan_reflection_patch_t *) kan_dynamic_array_add_last (&prototype_2.components) =
        kan_reflection_patch_builder_build (patch_builder, registry, config_component_type);

    save_rd (path, &prototype_2, kan_string_intern ("resource_prototype_t"), registry);
    resource_prototype_shutdown (&prototype_2);

    kan_reflection_patch_builder_destroy (patch_builder);
}

static void setup_workspace (kan_context_handle_t context)
{
    kan_file_system_remove_directory_with_content (WORKSPACE_RESOURCES_SUB_DIRECTORY);
    kan_file_system_remove_directory_with_content (WORKSPACE_REFERENCE_CACHE_SUB_DIRECTORY);
    kan_file_system_make_directory (WORKSPACE_RESOURCES_SUB_DIRECTORY);
    kan_file_system_make_directory (WORKSPACE_REFERENCE_CACHE_SUB_DIRECTORY);

    kan_context_system_handle_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_ASSERT (reflection_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    kan_reflection_registry_t registry = kan_reflection_system_get_registry (reflection_system);

    struct config_a_t config_a = {.x = 11u, .y = 12u};
    save_rd (WORKSPACE_RESOURCES_SUB_DIRECTORY "/config_a.rd", &config_a, kan_string_intern ("config_a_t"), registry);

    struct config_b_t config_b_1 = {.data = 42u, .optional_config_a = kan_string_intern ("config_a")};
    save_rd (WORKSPACE_RESOURCES_SUB_DIRECTORY "/config_b_1.rd", &config_b_1, kan_string_intern ("config_b_t"),
             registry);

    struct config_b_t config_b_2 = {.data = 42u, .optional_config_a = NULL};
    save_rd (WORKSPACE_RESOURCES_SUB_DIRECTORY "/config_b_2.rd", &config_b_2, kan_string_intern ("config_b_t"),
             registry);

    save_prototype_1 (registry, WORKSPACE_RESOURCES_SUB_DIRECTORY "/prototype_1.rd");
    save_prototype_2 (registry, WORKSPACE_RESOURCES_SUB_DIRECTORY "/prototype_2.rd");
}

struct outer_reference_query_result_t
{
    kan_interned_string_t type;
    kan_interned_string_t name;
    uint64_t times_hit;
};

static void test_outer_reference_query (struct kan_repository_indexed_value_read_cursor_t *cursor,
                                        uint64_t expected_results_count,
                                        struct outer_reference_query_result_t *expected_results)
{
    while (KAN_TRUE)
    {
        struct kan_repository_indexed_value_read_access_t access =
            kan_repository_indexed_value_read_cursor_next (cursor);
        const struct kan_resource_native_entry_outer_reference_t *reference =
            kan_repository_indexed_value_read_access_resolve (&access);

        if (reference)
        {
            kan_bool_t expected = KAN_FALSE;
            for (uint64_t index = 0u; index < expected_results_count; ++index)
            {
                if (expected_results[index].type == reference->reference_type &&
                    expected_results[index].name == reference->reference_name)
                {
                    ++expected_results[index].times_hit;
                    expected = KAN_TRUE;
                    break;
                }
            }

            KAN_TEST_CHECK (expected)
            kan_repository_indexed_value_read_access_close (&access);
        }
        else
        {
            break;
        }
    }

    for (uint64_t index = 0u; index < expected_results_count; ++index)
    {
        KAN_TEST_CHECK (expected_results[index].times_hit == 1u)
    }
}

static inline void request_outer_references (struct kan_repository_event_insert_query_t *insert_event,
                                             kan_interned_string_t type,
                                             kan_interned_string_t name)
{
    struct kan_repository_event_insertion_package_t package = kan_repository_event_insert_query_execute (insert_event);
    struct kan_resource_update_outer_references_request_event_t *event =
        kan_repository_event_insertion_package_get (&package);

    if (event)
    {
        event->type = type;
        event->name = name;
        kan_repository_event_insertion_package_submit (&package);
    }
}

struct outer_reference_detection_test_state_t
{
    struct kan_repository_event_insert_query_t insert__kan_resource_update_outer_references_request_event;
    struct kan_repository_event_fetch_query_t fetch__kan_resource_update_outer_references_response_event;
    struct kan_repository_indexed_value_read_query_t
        read_value__kan_resource_native_entry_outer_reference__attachment_id;

    // Test internal state is saved in mutator for simplicity as we're not planning hot reloading in tests.
    kan_bool_t test_needs_to_request_detection;
    kan_bool_t config_a_scanned;
    kan_bool_t config_b_1_scanned;
    kan_bool_t config_b_2_scanned;
    kan_bool_t prototype_1_scanned;
    kan_bool_t prototype_2_scanned;
};

TEST_UNIVERSE_RESOURCE_REFERENCE_API void kan_universe_mutator_deploy_outer_reference_detection_test (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct outer_reference_detection_test_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_REFERENCE_END_CHECKPOINT);
    state->test_needs_to_request_detection = KAN_TRUE;
    state->config_a_scanned = KAN_FALSE;
    state->config_b_1_scanned = KAN_FALSE;
    state->config_b_2_scanned = KAN_FALSE;
    state->prototype_1_scanned = KAN_FALSE;
    state->prototype_2_scanned = KAN_FALSE;
}

TEST_UNIVERSE_RESOURCE_REFERENCE_API void kan_universe_mutator_execute_outer_reference_detection_test (
    kan_cpu_job_t job, struct outer_reference_detection_test_state_t *state)
{
    if (state->test_needs_to_request_detection)
    {
        state->test_needs_to_request_detection = KAN_FALSE;
        request_outer_references (&state->insert__kan_resource_update_outer_references_request_event,
                                  kan_string_intern ("config_a_t"), kan_string_intern ("config_a"));
        request_outer_references (&state->insert__kan_resource_update_outer_references_request_event,
                                  kan_string_intern ("config_b_t"), kan_string_intern ("config_b_1"));
        request_outer_references (&state->insert__kan_resource_update_outer_references_request_event,
                                  kan_string_intern ("config_b_t"), kan_string_intern ("config_b_2"));
        request_outer_references (&state->insert__kan_resource_update_outer_references_request_event,
                                  kan_string_intern ("resource_prototype_t"), kan_string_intern ("prototype_1"));
        request_outer_references (&state->insert__kan_resource_update_outer_references_request_event,
                                  kan_string_intern ("resource_prototype_t"), kan_string_intern ("prototype_2"));
    }

    const struct kan_resource_update_outer_references_response_event_t *response;
    struct kan_repository_event_read_access_t response_access =
        kan_repository_event_fetch_query_next (&state->fetch__kan_resource_update_outer_references_response_event);

    while ((response = kan_repository_event_read_access_resolve (&response_access)))
    {
        KAN_TEST_CHECK (response->successful)
        if (response->type == kan_string_intern ("config_a_t") && response->name == kan_string_intern ("config_a"))
        {
            KAN_TEST_CHECK (!state->config_a_scanned)
            state->config_a_scanned = KAN_TRUE;

            struct kan_repository_indexed_value_read_cursor_t cursor = kan_repository_indexed_value_read_query_execute (
                &state->read_value__kan_resource_native_entry_outer_reference__attachment_id,
                &response->entry_attachment_id);

            test_outer_reference_query (&cursor, 0u, NULL);
            kan_repository_indexed_value_read_cursor_close (&cursor);
        }
        else if (response->type == kan_string_intern ("config_b_t") &&
                 response->name == kan_string_intern ("config_b_1"))
        {
            KAN_TEST_CHECK (!state->config_b_1_scanned)
            state->config_b_1_scanned = KAN_TRUE;

            struct outer_reference_query_result_t result[] = {
                {.type = kan_string_intern ("config_a_t"), .name = kan_string_intern ("config_a"), .times_hit = 0u},
            };

            struct kan_repository_indexed_value_read_cursor_t cursor = kan_repository_indexed_value_read_query_execute (
                &state->read_value__kan_resource_native_entry_outer_reference__attachment_id,
                &response->entry_attachment_id);

            test_outer_reference_query (&cursor, sizeof (result) / (sizeof (result[0u])), result);
            kan_repository_indexed_value_read_cursor_close (&cursor);
        }
        else if (response->type == kan_string_intern ("config_b_t") &&
                 response->name == kan_string_intern ("config_b_2"))
        {
            KAN_TEST_CHECK (!state->config_b_2_scanned)
            state->config_b_2_scanned = KAN_TRUE;

            struct kan_repository_indexed_value_read_cursor_t cursor = kan_repository_indexed_value_read_query_execute (
                &state->read_value__kan_resource_native_entry_outer_reference__attachment_id,
                &response->entry_attachment_id);

            test_outer_reference_query (&cursor, 0u, NULL);
            kan_repository_indexed_value_read_cursor_close (&cursor);
        }
        else if (response->type == kan_string_intern ("resource_prototype_t") &&
                 response->name == kan_string_intern ("prototype_1"))
        {
            KAN_TEST_CHECK (!state->prototype_1_scanned)
            state->prototype_1_scanned = KAN_TRUE;

            struct outer_reference_query_result_t result[] = {
                {.type = kan_string_intern ("resource_prototype_t"),
                 .name = kan_string_intern ("prototype_2"),
                 .times_hit = 0u},
                {.type = kan_string_intern ("config_b_t"), .name = kan_string_intern ("config_b_2"), .times_hit = 0u},
            };

            struct kan_repository_indexed_value_read_cursor_t cursor = kan_repository_indexed_value_read_query_execute (
                &state->read_value__kan_resource_native_entry_outer_reference__attachment_id,
                &response->entry_attachment_id);

            test_outer_reference_query (&cursor, sizeof (result) / (sizeof (result[0u])), result);
            kan_repository_indexed_value_read_cursor_close (&cursor);
        }
        else if (response->type == kan_string_intern ("resource_prototype_t") &&
                 response->name == kan_string_intern ("prototype_2"))
        {
            KAN_TEST_CHECK (!state->prototype_2_scanned)
            state->prototype_2_scanned = KAN_TRUE;

            struct outer_reference_query_result_t result[] = {
                {.type = kan_string_intern ("config_a_t"), .name = kan_string_intern ("config_a"), .times_hit = 0u},
                {.type = kan_string_intern ("config_b_t"), .name = kan_string_intern ("config_b_1"), .times_hit = 0u},
            };

            struct kan_repository_indexed_value_read_cursor_t cursor = kan_repository_indexed_value_read_query_execute (
                &state->read_value__kan_resource_native_entry_outer_reference__attachment_id,
                &response->entry_attachment_id);

            test_outer_reference_query (&cursor, sizeof (result) / (sizeof (result[0u])), result);
            kan_repository_indexed_value_read_cursor_close (&cursor);
        }
        else
        {
            // Unknown event.
            KAN_TEST_CHECK (KAN_FALSE)
        }

        kan_repository_event_read_access_close (&response_access);
        response_access =
            kan_repository_event_fetch_query_next (&state->fetch__kan_resource_update_outer_references_response_event);
    }

    if (state->config_a_scanned && state->config_b_1_scanned && state->config_b_2_scanned &&
        state->prototype_1_scanned && state->prototype_2_scanned)
    {
        global_test_finished = KAN_TRUE;
    }

    kan_cpu_job_release (job);
}

struct all_references_to_type_detection_test_state_t
{
    struct kan_repository_event_insert_query_t insert__kan_resource_update_all_references_to_type_request_event;
    struct kan_repository_event_fetch_query_t fetch__kan_resource_update_all_references_to_type_response_event;
    struct kan_repository_indexed_value_read_query_t
        read_value__kan_resource_native_entry_outer_reference__reference_type;
    struct kan_repository_indexed_value_read_query_t read_value__kan_resource_native_entry__attachment_id;

    // Test internal state is saved in mutator for simplicity as we're not planning hot reloading in tests.
    kan_bool_t test_needs_to_request_detection;
};

TEST_UNIVERSE_RESOURCE_REFERENCE_API void kan_universe_mutator_deploy_all_references_to_type_detection_test (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct all_references_to_type_detection_test_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_REFERENCE_END_CHECKPOINT);
    state->test_needs_to_request_detection = KAN_TRUE;
}

TEST_UNIVERSE_RESOURCE_REFERENCE_API void kan_universe_mutator_execute_all_references_to_type_detection_test (
    kan_cpu_job_t job, struct all_references_to_type_detection_test_state_t *state)
{
    if (state->test_needs_to_request_detection)
    {
        state->test_needs_to_request_detection = KAN_FALSE;
        struct kan_repository_event_insertion_package_t package = kan_repository_event_insert_query_execute (
            &state->insert__kan_resource_update_all_references_to_type_request_event);
        struct kan_resource_update_all_references_to_type_request_event_t *event =
            kan_repository_event_insertion_package_get (&package);

        if (event)
        {
            event->type = kan_string_intern ("config_a_t");
            kan_repository_event_insertion_package_submit (&package);
        }
    }

    const struct kan_resource_update_all_references_to_type_response_event_t *response;
    struct kan_repository_event_read_access_t response_access = kan_repository_event_fetch_query_next (
        &state->fetch__kan_resource_update_all_references_to_type_response_event);

    while ((response = kan_repository_event_read_access_resolve (&response_access)))
    {
        KAN_TEST_CHECK (response->successful)
        KAN_TEST_CHECK (response->type == kan_string_intern ("config_a_t"))
        KAN_TEST_CHECK (!global_test_finished)
        global_test_finished = KAN_TRUE;

        kan_bool_t config_b_1_found = KAN_FALSE;
        kan_bool_t prototype_2_found = KAN_FALSE;

        struct kan_repository_indexed_value_read_cursor_t cursor = kan_repository_indexed_value_read_query_execute (
            &state->read_value__kan_resource_native_entry_outer_reference__reference_type, &response->type);

        while (KAN_TRUE)
        {
            struct kan_repository_indexed_value_read_access_t access =
                kan_repository_indexed_value_read_cursor_next (&cursor);

            const struct kan_resource_native_entry_outer_reference_t *reference =
                kan_repository_indexed_value_read_access_resolve (&access);

            if (reference)
            {
                struct kan_repository_indexed_value_read_cursor_t entry_cursor =
                    kan_repository_indexed_value_read_query_execute (
                        &state->read_value__kan_resource_native_entry__attachment_id, &reference->attachment_id);

                struct kan_repository_indexed_value_read_access_t entry_access =
                    kan_repository_indexed_value_read_cursor_next (&entry_cursor);
                kan_repository_indexed_value_read_cursor_close (&entry_cursor);

                const struct kan_resource_native_entry_t *entry =
                    kan_repository_indexed_value_read_access_resolve (&entry_access);
                KAN_TEST_ASSERT (entry)

                if (entry->type == kan_string_intern ("config_b_t") && entry->name == kan_string_intern ("config_b_1"))
                {
                    KAN_TEST_CHECK (!config_b_1_found)
                    config_b_1_found = KAN_TRUE;
                }
                else if (entry->type == kan_string_intern ("resource_prototype_t") &&
                         entry->name == kan_string_intern ("prototype_2"))
                {
                    KAN_TEST_CHECK (!prototype_2_found)
                    prototype_2_found = KAN_TRUE;
                }
                else
                {
                    KAN_TEST_CHECK (KAN_FALSE)
                }

                kan_repository_indexed_value_read_access_close (&entry_access);
                kan_repository_indexed_value_read_access_close (&access);
            }
            else
            {
                kan_repository_indexed_value_read_cursor_close (&cursor);
                break;
            }
        }

        KAN_TEST_CHECK (config_b_1_found)
        KAN_TEST_CHECK (prototype_2_found)

        kan_repository_event_read_access_close (&response_access);
        response_access = kan_repository_event_fetch_query_next (
            &state->fetch__kan_resource_update_all_references_to_type_response_event);
    }

    kan_cpu_job_release (job);
}

enum outer_reference_caching_test_stage_t
{
    OUTER_REFERENCE_CACHING_TEST_STAGE_INIT,
    OUTER_REFERENCE_CACHING_TEST_STAGE_FIRST_SCAN_REQUESTED,
    OUTER_REFERENCE_CACHING_TEST_STAGE_FIRST_SCAN_DONE,
    OUTER_REFERENCE_CACHING_TEST_STAGE_SECOND_SCAN_REQUESTED,
    OUTER_REFERENCE_CACHING_TEST_STAGE_SECOND_SCAN_DONE,
    OUTER_REFERENCE_CACHING_TEST_STAGE_CHANGED_SCAN_REQUESTED,
    OUTER_REFERENCE_CACHING_TEST_STAGE_CHANGED_SCAN_DONE,
};

struct outer_reference_caching_test_state_t
{
    struct kan_repository_event_insert_query_t insert__kan_resource_update_outer_references_request_event;
    struct kan_repository_event_fetch_query_t fetch__kan_resource_update_outer_references_response_event;
    struct kan_repository_indexed_value_read_query_t
        read_value__kan_resource_native_entry_outer_reference__attachment_id;

    kan_reflection_registry_t registry;

    // Test internal state is saved in mutator for simplicity as we're not planning hot reloading in tests.
    enum outer_reference_caching_test_stage_t stage;
};

TEST_UNIVERSE_RESOURCE_REFERENCE_API void kan_universe_mutator_deploy_outer_reference_caching_test (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct outer_reference_caching_test_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_REFERENCE_END_CHECKPOINT);
    state->registry = kan_universe_get_reflection_registry (universe);
    state->stage = OUTER_REFERENCE_CACHING_TEST_STAGE_INIT;
}

TEST_UNIVERSE_RESOURCE_REFERENCE_API void kan_universe_mutator_execute_outer_reference_caching_test (
    kan_cpu_job_t job, struct outer_reference_caching_test_state_t *state)
{
    switch (state->stage)
    {
    case OUTER_REFERENCE_CACHING_TEST_STAGE_INIT:
        request_outer_references (&state->insert__kan_resource_update_outer_references_request_event,
                                  kan_string_intern ("resource_prototype_t"), kan_string_intern ("prototype_1"));
        state->stage = OUTER_REFERENCE_CACHING_TEST_STAGE_FIRST_SCAN_REQUESTED;
        break;

    case OUTER_REFERENCE_CACHING_TEST_STAGE_FIRST_SCAN_REQUESTED:
    case OUTER_REFERENCE_CACHING_TEST_STAGE_SECOND_SCAN_REQUESTED:
    case OUTER_REFERENCE_CACHING_TEST_STAGE_CHANGED_SCAN_REQUESTED:
        break;

    case OUTER_REFERENCE_CACHING_TEST_STAGE_FIRST_SCAN_DONE:
        request_outer_references (&state->insert__kan_resource_update_outer_references_request_event,
                                  kan_string_intern ("resource_prototype_t"), kan_string_intern ("prototype_1"));
        state->stage = OUTER_REFERENCE_CACHING_TEST_STAGE_SECOND_SCAN_REQUESTED;
        break;

    case OUTER_REFERENCE_CACHING_TEST_STAGE_SECOND_SCAN_DONE:
    {
        // Overwrite prototype to change file and expect cache to be invalidated.
        // One ms sleeps are added to make sure that there is no error due to missed cache invalidation.
        save_prototype_2 (state->registry, WORKSPACE_RESOURCES_SUB_DIRECTORY "/prototype_1.rd");

        request_outer_references (&state->insert__kan_resource_update_outer_references_request_event,
                                  kan_string_intern ("resource_prototype_t"), kan_string_intern ("prototype_1"));
        state->stage = OUTER_REFERENCE_CACHING_TEST_STAGE_CHANGED_SCAN_REQUESTED;
        break;
    }

    case OUTER_REFERENCE_CACHING_TEST_STAGE_CHANGED_SCAN_DONE:
        break;
    }

    if (state->stage == OUTER_REFERENCE_CACHING_TEST_STAGE_INIT)
    {
    }

    const struct kan_resource_update_outer_references_response_event_t *response;
    struct kan_repository_event_read_access_t response_access =
        kan_repository_event_fetch_query_next (&state->fetch__kan_resource_update_outer_references_response_event);

    while ((response = kan_repository_event_read_access_resolve (&response_access)))
    {
        KAN_TEST_CHECK (response->successful)
        if (response->type == kan_string_intern ("resource_prototype_t") &&
            response->name == kan_string_intern ("prototype_1"))
        {
            struct outer_reference_query_result_t result_before_change[] = {
                {.type = kan_string_intern ("resource_prototype_t"),
                 .name = kan_string_intern ("prototype_2"),
                 .times_hit = 0u},
                {.type = kan_string_intern ("config_b_t"), .name = kan_string_intern ("config_b_2"), .times_hit = 0u},
            };

            struct outer_reference_query_result_t result_after_change[] = {
                {.type = kan_string_intern ("config_a_t"), .name = kan_string_intern ("config_a"), .times_hit = 0u},
                {.type = kan_string_intern ("config_b_t"), .name = kan_string_intern ("config_b_1"), .times_hit = 0u},
            };

            switch (state->stage)
            {
            case OUTER_REFERENCE_CACHING_TEST_STAGE_FIRST_SCAN_REQUESTED:
            {
                struct kan_repository_indexed_value_read_cursor_t cursor =
                    kan_repository_indexed_value_read_query_execute (
                        &state->read_value__kan_resource_native_entry_outer_reference__attachment_id,
                        &response->entry_attachment_id);

                test_outer_reference_query (
                    &cursor, sizeof (result_before_change) / (sizeof (result_before_change[0u])), result_before_change);
                kan_repository_indexed_value_read_cursor_close (&cursor);

                state->stage = OUTER_REFERENCE_CACHING_TEST_STAGE_FIRST_SCAN_DONE;
                break;
            }

            case OUTER_REFERENCE_CACHING_TEST_STAGE_SECOND_SCAN_REQUESTED:
            {
                struct kan_repository_indexed_value_read_cursor_t cursor =
                    kan_repository_indexed_value_read_query_execute (
                        &state->read_value__kan_resource_native_entry_outer_reference__attachment_id,
                        &response->entry_attachment_id);

                test_outer_reference_query (
                    &cursor, sizeof (result_before_change) / (sizeof (result_before_change[0u])), result_before_change);
                kan_repository_indexed_value_read_cursor_close (&cursor);

                state->stage = OUTER_REFERENCE_CACHING_TEST_STAGE_SECOND_SCAN_DONE;
                break;
            }

            case OUTER_REFERENCE_CACHING_TEST_STAGE_CHANGED_SCAN_REQUESTED:
            {
                struct kan_repository_indexed_value_read_cursor_t cursor =
                    kan_repository_indexed_value_read_query_execute (
                        &state->read_value__kan_resource_native_entry_outer_reference__attachment_id,
                        &response->entry_attachment_id);

                test_outer_reference_query (&cursor, sizeof (result_after_change) / (sizeof (result_after_change[0u])),
                                            result_after_change);
                kan_repository_indexed_value_read_cursor_close (&cursor);

                state->stage = OUTER_REFERENCE_CACHING_TEST_STAGE_CHANGED_SCAN_DONE;
                break;
            }

            case OUTER_REFERENCE_CACHING_TEST_STAGE_INIT:
            case OUTER_REFERENCE_CACHING_TEST_STAGE_FIRST_SCAN_DONE:
            case OUTER_REFERENCE_CACHING_TEST_STAGE_SECOND_SCAN_DONE:
            case OUTER_REFERENCE_CACHING_TEST_STAGE_CHANGED_SCAN_DONE:
                KAN_TEST_CHECK (KAN_FALSE)
                break;
            }
        }
        else
        {
            KAN_TEST_CHECK (KAN_FALSE)
        }

        kan_repository_event_read_access_close (&response_access);
        response_access =
            kan_repository_event_fetch_query_next (&state->fetch__kan_resource_update_outer_references_response_event);
    }

    if (state->stage == OUTER_REFERENCE_CACHING_TEST_STAGE_CHANGED_SCAN_DONE)
    {
        global_test_finished = KAN_TRUE;
    }

    kan_cpu_job_release (job);
}

static kan_context_handle_t setup_context (void)
{
    kan_context_handle_t context =
        kan_context_create (kan_allocation_group_get_child (kan_allocation_group_root (), "context"));
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME, NULL))

    struct kan_virtual_file_system_config_mount_real_t mount_point_reference_cache = {
        .next = NULL,
        .mount_path = WORKSPACE_REFERENCE_CACHE_MOUNT_PATH,
        .real_path = WORKSPACE_REFERENCE_CACHE_SUB_DIRECTORY,
    };

    struct kan_virtual_file_system_config_mount_real_t mount_point_resources = {
        .next = &mount_point_reference_cache,
        .mount_path = WORKSPACE_RESOURCES_MOUNT_PATH,
        .real_path = WORKSPACE_RESOURCES_SUB_DIRECTORY,
    };

    struct kan_virtual_file_system_config_t virtual_file_system_config = {
        .first_mount_real = &mount_point_resources,
        .first_mount_read_only_pack = NULL,
    };

    KAN_TEST_CHECK (
        kan_context_request_system (context, KAN_CONTEXT_VIRTUAL_FILE_SYSTEM_NAME, &virtual_file_system_config))
    kan_context_assembly (context);
    return context;
}

struct run_update_state_t
{
    uint64_t stub;
};

TEST_UNIVERSE_RESOURCE_REFERENCE_API void kan_universe_scheduler_execute_run_update (
    kan_universe_scheduler_interface_t interface, struct run_update_state_t *state)
{
    // We need to close all accesses before running pipelines.
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern ("update"));
}

static void run_test (kan_context_handle_t context, kan_interned_string_t test_mutator)
{
    kan_context_system_handle_t universe_system_handle = kan_context_query (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME);
    KAN_TEST_ASSERT (universe_system_handle != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)

    kan_universe_t universe = kan_universe_system_get_universe (universe_system_handle);
    KAN_TEST_ASSERT (universe != KAN_INVALID_UNIVERSE)

    kan_context_system_handle_t update_system = kan_context_query (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
    KAN_TEST_ASSERT (update_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)

    kan_context_system_handle_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_TEST_ASSERT (reflection_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    kan_reflection_registry_t registry = kan_reflection_system_get_registry (reflection_system);

    struct kan_universe_world_definition_t definition;
    kan_universe_world_definition_init (&definition);
    definition.world_name = kan_string_intern ("root_world");
    definition.scheduler_name = kan_string_intern ("run_update");

    kan_reflection_patch_builder_t patch_builder = kan_reflection_patch_builder_create ();
    struct kan_resource_provider_configuration_t resource_provider_configuration = {
        .scan_budget_ns = 2000000u,
        .load_budget_ns = 2000000u,
        .add_wait_time_ns = 100000000u,
        .modify_wait_time_ns = 100000000u,
        .use_load_only_string_registry = KAN_TRUE,
        .observe_file_system = KAN_FALSE,
        .resource_directory_path = kan_string_intern (WORKSPACE_RESOURCES_MOUNT_PATH),
    };

    kan_reflection_patch_builder_add_chunk (patch_builder, 0u, sizeof (struct kan_resource_provider_configuration_t),
                                            &resource_provider_configuration);
    kan_reflection_patch_t resource_provider_configuration_patch = kan_reflection_patch_builder_build (
        patch_builder, registry,
        kan_reflection_registry_query_struct (registry, kan_string_intern ("kan_resource_provider_configuration_t")));

    struct kan_resource_reference_configuration_t resource_reference_configuration = {
        .budget_ns = 2000000u,
        .workspace_directory_path = kan_string_intern (WORKSPACE_REFERENCE_CACHE_MOUNT_PATH),
    };

    kan_reflection_patch_builder_add_chunk (patch_builder, 0u, sizeof (struct kan_resource_reference_configuration_t),
                                            &resource_reference_configuration);
    kan_reflection_patch_t resource_reference_configuration_patch = kan_reflection_patch_builder_build (
        patch_builder, registry,
        kan_reflection_registry_query_struct (registry, kan_string_intern ("kan_resource_reference_configuration_t")));
    kan_reflection_patch_builder_destroy (patch_builder);

    kan_dynamic_array_set_capacity (&definition.configuration, 2u);
    struct kan_universe_world_configuration_t *provider_configuration =
        kan_dynamic_array_add_last (&definition.configuration);
    kan_universe_world_configuration_init (provider_configuration);
    provider_configuration->name = kan_string_intern (KAN_RESOURCE_PROVIDER_CONFIGURATION);
    kan_dynamic_array_set_capacity (&provider_configuration->variants, 1u);

    struct kan_universe_world_configuration_variant_t *provider_variant =
        kan_dynamic_array_add_last (&provider_configuration->variants);
    kan_universe_world_configuration_variant_init (provider_variant);
    provider_variant->data = resource_provider_configuration_patch;

    struct kan_universe_world_configuration_t *reference_configuration =
        kan_dynamic_array_add_last (&definition.configuration);
    kan_universe_world_configuration_init (reference_configuration);
    reference_configuration->name = kan_string_intern (KAN_RESOURCE_REFERENCE_CONFIGURATION);
    kan_dynamic_array_set_capacity (&reference_configuration->variants, 1u);

    struct kan_universe_world_configuration_variant_t *reference_variant =
        kan_dynamic_array_add_last (&reference_configuration->variants);
    kan_universe_world_configuration_variant_init (reference_variant);
    reference_variant->data = resource_reference_configuration_patch;

    kan_dynamic_array_set_capacity (&definition.pipelines, 1u);
    struct kan_universe_world_pipeline_definition_t *update_pipeline =
        kan_dynamic_array_add_last (&definition.pipelines);

    kan_universe_world_pipeline_definition_init (update_pipeline);
    update_pipeline->name = kan_string_intern ("update");

    kan_dynamic_array_set_capacity (&update_pipeline->mutators, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutators) = test_mutator;

    kan_dynamic_array_set_capacity (&update_pipeline->mutator_groups, 2u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutator_groups) =
        kan_string_intern (KAN_RESOURCE_PROVIDER_MUTATOR_GROUP);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutator_groups) =
        kan_string_intern (KAN_RESOURCE_REFERENCE_MUTATOR_GROUP);

    kan_universe_deploy_root (universe, &definition);
    kan_universe_world_definition_shutdown (&definition);

    while (!global_test_finished)
    {
        kan_update_system_run (update_system);
    }
}

KAN_TEST_CASE (outer_reference_detection)
{
    kan_context_handle_t context = setup_context ();
    setup_workspace (context);
    run_test (context, kan_string_intern ("outer_reference_detection_test"));
    kan_context_destroy (context);
}

KAN_TEST_CASE (all_references_to_type_detection)
{
    kan_context_handle_t context = setup_context ();
    setup_workspace (context);
    run_test (context, kan_string_intern ("all_references_to_type_detection_test"));
    kan_context_destroy (context);
}

KAN_TEST_CASE (outer_reference_caching)
{
    kan_context_handle_t context = setup_context ();
    setup_workspace (context);
    run_test (context, kan_string_intern ("outer_reference_caching_test"));
    kan_context_destroy (context);
}
