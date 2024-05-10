#define _CRT_SECURE_NO_WARNINGS

#include <test_universe_resource_provider_api.h>

#include <stddef.h>
#include <stdio.h>

#include <kan/context/reflection_system.h>
#include <kan/context/universe_system.h>
#include <kan/context/update_system.h>
#include <kan/context/virtual_file_system.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/stream.h>
#include <kan/memory_profiler/capture.h>
#include <kan/platform/precise_time.h>
#include <kan/reflection/generated_reflection.h>
#include <kan/resource_index/resource_index.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/testing/testing.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

#define WORKSPACE_SUB_DIRECTORY "workspace"
#define WORKSPACE_MOUNT_PATH "resources"

static kan_bool_t global_test_finished = KAN_FALSE;

struct first_resource_type_t
{
    uint64_t some_integer;
    kan_bool_t flag_1;
    kan_bool_t flag_2;
    kan_bool_t flag_3;
    kan_bool_t flag_4;
};

_Static_assert (_Alignof (struct first_resource_type_t) == _Alignof (uint64_t),
                "Alignment does not require additional offset calculations.");

// \meta reflection_struct_meta = "first_resource_type_t"
TEST_UNIVERSE_RESOURCE_PROVIDER_API struct kan_resource_provider_type_meta_t first_resource_type_meta = {0u};

struct second_resource_type_t
{
    kan_interned_string_t first_id;
    kan_interned_string_t second_id;
};

_Static_assert (_Alignof (struct second_resource_type_t) == _Alignof (uint64_t),
                "Alignment does not require additional offset calculations.");

// \meta reflection_struct_meta = "second_resource_type_t"
TEST_UNIVERSE_RESOURCE_PROVIDER_API struct kan_resource_provider_type_meta_t second_resource_type_meta = {0u};

static struct first_resource_type_t resource_alpha = {
    64u, KAN_TRUE, KAN_FALSE, KAN_TRUE, KAN_FALSE,
};

static struct first_resource_type_t resource_beta = {
    129u, KAN_TRUE, KAN_TRUE, KAN_TRUE, KAN_TRUE,
};

static struct second_resource_type_t resource_players;
static struct second_resource_type_t resource_characters;

static uint64_t resource_test_third_party[] = {
    129u, 12647u, 1827346u, 222u, 99u, 11u, 98u, 12746u,
};

static uint64_t resource_test_third_party_changed[] = {
    129u, 12617u, 1827336u, 222u, 99u, 11u, 98u, 12746u, 179u, 221u, 14u, 12u,
};

static void initialize_resources (void)
{
    resource_players = (struct second_resource_type_t) {
        .first_id = kan_string_intern ("konrad"),
        .second_id = kan_string_intern ("alfred"),
    };

    resource_characters = (struct second_resource_type_t) {
        .first_id = kan_string_intern ("warrior"),
        .second_id = kan_string_intern ("trader"),
    };
}

struct run_update_state_t
{
    uint64_t stub;
};

TEST_UNIVERSE_RESOURCE_PROVIDER_API void kan_universe_scheduler_execute_run_update (
    kan_universe_scheduler_interface_t interface, struct run_update_state_t *state)
{
    // We need to close all accesses before running pipelines.
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern ("update"));
}

struct request_resources_and_check_singleton_t
{
    kan_bool_t requests_created;
    uint64_t alpha_request_id;
    uint64_t beta_request_id_first;
    uint64_t beta_request_id_second;
    uint64_t players_request_id;
    uint64_t characters_request_id;
    uint64_t test_third_party_id;

    kan_bool_t alpha_ready;
    kan_bool_t beta_ready;
    kan_bool_t players_ready;
    kan_bool_t characters_ready;
    kan_bool_t test_third_party_ready;
};

TEST_UNIVERSE_RESOURCE_PROVIDER_API void request_resources_and_check_singleton_init (
    struct request_resources_and_check_singleton_t *instance)
{
    instance->requests_created = KAN_FALSE;
    instance->alpha_ready = KAN_FALSE;
    instance->beta_ready = KAN_FALSE;
    instance->players_ready = KAN_FALSE;
    instance->characters_ready = KAN_FALSE;
    instance->test_third_party_ready = KAN_FALSE;
}

struct request_resources_and_check_state_t
{
    struct kan_repository_singleton_write_query_t write__request_resources_and_check_singleton;
    struct kan_repository_singleton_read_query_t read__kan_resource_provider_singleton;
    struct kan_repository_indexed_insert_query_t insert__kan_resource_request;
    struct kan_repository_indexed_value_read_query_t read_value__kan_resource_request__request_id;
    struct kan_repository_indexed_value_read_query_t
        read_value__resource_provider_container_first_resource_type__container_id;
    struct kan_repository_indexed_value_read_query_t
        read_value__resource_provider_container_second_resource_type__container_id;
};

TEST_UNIVERSE_RESOURCE_PROVIDER_API void kan_universe_mutator_deploy_request_resources_and_check (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct request_resources_and_check_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
}

TEST_UNIVERSE_RESOURCE_PROVIDER_API void kan_universe_mutator_execute_request_resources_and_check (
    kan_cpu_job_t job, struct request_resources_and_check_state_t *state)
{
    kan_repository_singleton_write_access_t singleton_access =
        kan_repository_singleton_write_query_execute (&state->write__request_resources_and_check_singleton);

    struct request_resources_and_check_singleton_t *singleton =
        (struct request_resources_and_check_singleton_t *) kan_repository_singleton_write_access_resolve (
            singleton_access);

    if (!singleton->requests_created)
    {
        kan_repository_singleton_read_access_t provider_access =
            kan_repository_singleton_read_query_execute (&state->read__kan_resource_provider_singleton);

        const struct kan_resource_provider_singleton_t *provider =
            kan_repository_singleton_read_access_resolve (provider_access);

        struct kan_repository_indexed_insertion_package_t package =
            kan_repository_indexed_insert_query_execute (&state->insert__kan_resource_request);
        struct kan_resource_request_t *request = kan_repository_indexed_insertion_package_get (&package);

        request->request_id = kan_next_resource_request_id (provider);
        request->type = kan_string_intern ("first_resource_type_t");
        request->name = kan_string_intern ("alpha");
        request->priority = 0u;
        singleton->alpha_request_id = request->request_id;
        kan_repository_indexed_insertion_package_submit (&package);

        package = kan_repository_indexed_insert_query_execute (&state->insert__kan_resource_request);
        request = kan_repository_indexed_insertion_package_get (&package);
        request->request_id = kan_next_resource_request_id (provider);
        request->type = kan_string_intern ("first_resource_type_t");
        request->name = kan_string_intern ("beta");
        request->priority = 0u;
        singleton->beta_request_id_first = request->request_id;
        kan_repository_indexed_insertion_package_submit (&package);

        package = kan_repository_indexed_insert_query_execute (&state->insert__kan_resource_request);
        request = kan_repository_indexed_insertion_package_get (&package);
        request->request_id = kan_next_resource_request_id (provider);
        request->type = kan_string_intern ("first_resource_type_t");
        request->name = kan_string_intern ("beta");
        request->priority = 100u;
        singleton->beta_request_id_second = request->request_id;
        kan_repository_indexed_insertion_package_submit (&package);

        package = kan_repository_indexed_insert_query_execute (&state->insert__kan_resource_request);
        request = kan_repository_indexed_insertion_package_get (&package);
        request->request_id = kan_next_resource_request_id (provider);
        request->type = kan_string_intern ("second_resource_type_t");
        request->name = kan_string_intern ("players");
        request->priority = 0u;
        singleton->players_request_id = request->request_id;
        kan_repository_indexed_insertion_package_submit (&package);

        package = kan_repository_indexed_insert_query_execute (&state->insert__kan_resource_request);
        request = kan_repository_indexed_insertion_package_get (&package);
        request->request_id = kan_next_resource_request_id (provider);
        request->type = kan_string_intern ("second_resource_type_t");
        request->name = kan_string_intern ("characters");
        request->priority = 0u;
        singleton->characters_request_id = request->request_id;
        kan_repository_indexed_insertion_package_submit (&package);

        package = kan_repository_indexed_insert_query_execute (&state->insert__kan_resource_request);
        request = kan_repository_indexed_insertion_package_get (&package);
        request->request_id = kan_next_resource_request_id (provider);
        request->type = NULL;
        request->name = kan_string_intern ("test_third_party");
        request->priority = 200u;
        singleton->test_third_party_id = request->request_id;
        kan_repository_indexed_insertion_package_submit (&package);

        kan_repository_singleton_read_access_close (provider_access);
        singleton->requests_created = KAN_TRUE;
    }

    if (!singleton->alpha_ready)
    {
        struct kan_repository_indexed_value_read_cursor_t request_cursor =
            kan_repository_indexed_value_read_query_execute (&state->read_value__kan_resource_request__request_id,
                                                             &singleton->alpha_request_id);

        struct kan_repository_indexed_value_read_access_t request_access =
            kan_repository_indexed_value_read_cursor_next (&request_cursor);

        const struct kan_resource_request_t *request =
            kan_repository_indexed_value_read_access_resolve (&request_access);
        KAN_TEST_ASSERT (request)

        if (request->provided_container_id != KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)
        {
            singleton->alpha_ready = KAN_TRUE;

            struct kan_repository_indexed_value_read_cursor_t container_cursor =
                kan_repository_indexed_value_read_query_execute (
                    &state->read_value__resource_provider_container_first_resource_type__container_id,
                    &request->provided_container_id);

            struct kan_repository_indexed_value_read_access_t container_access =
                kan_repository_indexed_value_read_cursor_next (&container_cursor);

            const struct kan_resource_container_view_t *view =
                kan_repository_indexed_value_read_access_resolve (&container_access);
            KAN_TEST_CHECK (view)

            if (view)
            {
                struct first_resource_type_t *loaded_resource = (struct first_resource_type_t *) view->data_begin;
                KAN_TEST_CHECK (loaded_resource->some_integer == resource_alpha.some_integer)
                KAN_TEST_CHECK (loaded_resource->flag_1 == resource_alpha.flag_1)
                KAN_TEST_CHECK (loaded_resource->flag_2 == resource_alpha.flag_2)
                KAN_TEST_CHECK (loaded_resource->flag_3 == resource_alpha.flag_3)
                KAN_TEST_CHECK (loaded_resource->flag_4 == resource_alpha.flag_4)
                kan_repository_indexed_value_read_access_close (&container_access);
            }

            kan_repository_indexed_value_read_cursor_close (&container_cursor);
        }

        kan_repository_indexed_value_read_access_close (&request_access);
        kan_repository_indexed_value_read_cursor_close (&request_cursor);
    }

    if (!singleton->beta_ready)
    {
        struct kan_repository_indexed_value_read_cursor_t request_cursor =
            kan_repository_indexed_value_read_query_execute (&state->read_value__kan_resource_request__request_id,
                                                             &singleton->beta_request_id_first);

        struct kan_repository_indexed_value_read_access_t request_access =
            kan_repository_indexed_value_read_cursor_next (&request_cursor);

        const struct kan_resource_request_t *request =
            kan_repository_indexed_value_read_access_resolve (&request_access);
        KAN_TEST_ASSERT (request)

        if (request->provided_container_id != KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)
        {
            singleton->beta_ready = KAN_TRUE;

            struct kan_repository_indexed_value_read_cursor_t container_cursor =
                kan_repository_indexed_value_read_query_execute (
                    &state->read_value__resource_provider_container_first_resource_type__container_id,
                    &request->provided_container_id);

            struct kan_repository_indexed_value_read_access_t container_access =
                kan_repository_indexed_value_read_cursor_next (&container_cursor);

            const struct kan_resource_container_view_t *view =
                kan_repository_indexed_value_read_access_resolve (&container_access);
            KAN_TEST_CHECK (view)

            if (view)
            {
                struct first_resource_type_t *loaded_resource = (struct first_resource_type_t *) view->data_begin;
                KAN_TEST_CHECK (loaded_resource->some_integer == resource_beta.some_integer)
                KAN_TEST_CHECK (loaded_resource->flag_1 == resource_beta.flag_1)
                KAN_TEST_CHECK (loaded_resource->flag_2 == resource_beta.flag_2)
                KAN_TEST_CHECK (loaded_resource->flag_3 == resource_beta.flag_3)
                KAN_TEST_CHECK (loaded_resource->flag_4 == resource_beta.flag_4)
                kan_repository_indexed_value_read_access_close (&container_access);
            }

            kan_repository_indexed_value_read_cursor_close (&container_cursor);

            struct kan_repository_indexed_value_read_cursor_t second_request_cursor =
                kan_repository_indexed_value_read_query_execute (&state->read_value__kan_resource_request__request_id,
                                                                 &singleton->beta_request_id_second);

            struct kan_repository_indexed_value_read_access_t second_request_access =
                kan_repository_indexed_value_read_cursor_next (&second_request_cursor);

            struct kan_resource_request_t *second_request =
                (struct kan_resource_request_t *) kan_repository_indexed_value_read_access_resolve (
                    &second_request_access);
            KAN_TEST_ASSERT (second_request)
            KAN_TEST_CHECK (second_request->provided_container_id == request->provided_container_id)

            kan_repository_indexed_value_read_access_close (&second_request_access);
            kan_repository_indexed_value_read_cursor_close (&second_request_cursor);
        }

        kan_repository_indexed_value_read_access_close (&request_access);
        kan_repository_indexed_value_read_cursor_close (&request_cursor);
    }

    if (!singleton->players_ready)
    {
        struct kan_repository_indexed_value_read_cursor_t request_cursor =
            kan_repository_indexed_value_read_query_execute (&state->read_value__kan_resource_request__request_id,
                                                             &singleton->players_request_id);

        struct kan_repository_indexed_value_read_access_t request_access =
            kan_repository_indexed_value_read_cursor_next (&request_cursor);

        const struct kan_resource_request_t *request =
            kan_repository_indexed_value_read_access_resolve (&request_access);
        KAN_TEST_ASSERT (request)

        if (request->provided_container_id != KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)
        {
            singleton->players_ready = KAN_TRUE;

            struct kan_repository_indexed_value_read_cursor_t container_cursor =
                kan_repository_indexed_value_read_query_execute (
                    &state->read_value__resource_provider_container_second_resource_type__container_id,
                    &request->provided_container_id);

            struct kan_repository_indexed_value_read_access_t container_access =
                kan_repository_indexed_value_read_cursor_next (&container_cursor);

            const struct kan_resource_container_view_t *view =
                kan_repository_indexed_value_read_access_resolve (&container_access);
            KAN_TEST_CHECK (view)

            if (view)
            {
                struct second_resource_type_t *loaded_resource = (struct second_resource_type_t *) view->data_begin;
                KAN_TEST_CHECK (loaded_resource->first_id == resource_players.first_id)
                KAN_TEST_CHECK (loaded_resource->second_id == resource_players.second_id)
                kan_repository_indexed_value_read_access_close (&container_access);
            }

            kan_repository_indexed_value_read_cursor_close (&container_cursor);
        }

        kan_repository_indexed_value_read_access_close (&request_access);
        kan_repository_indexed_value_read_cursor_close (&request_cursor);
    }

    if (!singleton->characters_ready)
    {
        struct kan_repository_indexed_value_read_cursor_t request_cursor =
            kan_repository_indexed_value_read_query_execute (&state->read_value__kan_resource_request__request_id,
                                                             &singleton->characters_request_id);

        struct kan_repository_indexed_value_read_access_t request_access =
            kan_repository_indexed_value_read_cursor_next (&request_cursor);

        const struct kan_resource_request_t *request =
            kan_repository_indexed_value_read_access_resolve (&request_access);
        KAN_TEST_ASSERT (request)

        if (request->provided_container_id != KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)
        {
            singleton->characters_ready = KAN_TRUE;

            struct kan_repository_indexed_value_read_cursor_t container_cursor =
                kan_repository_indexed_value_read_query_execute (
                    &state->read_value__resource_provider_container_second_resource_type__container_id,
                    &request->provided_container_id);

            struct kan_repository_indexed_value_read_access_t container_access =
                kan_repository_indexed_value_read_cursor_next (&container_cursor);

            const struct kan_resource_container_view_t *view =
                kan_repository_indexed_value_read_access_resolve (&container_access);
            KAN_TEST_CHECK (view)

            if (view)
            {
                struct second_resource_type_t *loaded_resource = (struct second_resource_type_t *) view->data_begin;
                KAN_TEST_CHECK (loaded_resource->first_id == resource_characters.first_id)
                KAN_TEST_CHECK (loaded_resource->second_id == resource_characters.second_id)
                kan_repository_indexed_value_read_access_close (&container_access);
            }

            kan_repository_indexed_value_read_cursor_close (&container_cursor);
        }

        kan_repository_indexed_value_read_access_close (&request_access);
        kan_repository_indexed_value_read_cursor_close (&request_cursor);
    }

    if (!singleton->test_third_party_ready)
    {
        struct kan_repository_indexed_value_read_cursor_t request_cursor =
            kan_repository_indexed_value_read_query_execute (&state->read_value__kan_resource_request__request_id,
                                                             &singleton->test_third_party_id);

        struct kan_repository_indexed_value_read_access_t request_access =
            kan_repository_indexed_value_read_cursor_next (&request_cursor);

        const struct kan_resource_request_t *request =
            kan_repository_indexed_value_read_access_resolve (&request_access);
        KAN_TEST_ASSERT (request)

        if (request->provided_third_party.data)
        {
            singleton->test_third_party_ready = KAN_TRUE;
            KAN_TEST_CHECK (request->provided_third_party.size == sizeof (resource_test_third_party))
            KAN_TEST_CHECK (memcmp (request->provided_third_party.data, resource_test_third_party,
                                    sizeof (resource_test_third_party)) == 0)
        }

        kan_repository_indexed_value_read_access_close (&request_access);
        kan_repository_indexed_value_read_cursor_close (&request_cursor);
    }

    if (singleton->alpha_ready && singleton->beta_ready && singleton->players_ready && singleton->characters_ready &&
        singleton->test_third_party_ready)
    {
        global_test_finished = KAN_TRUE;
    }

    kan_repository_singleton_write_access_close (singleton_access);
    kan_cpu_job_release (job);
}

static void save_third_party (const char *path, uint8_t *data, uint64_t data_size)
{
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (path, KAN_TRUE);
    KAN_TEST_ASSERT (stream)
    KAN_TEST_ASSERT (stream->operations->write (stream, data_size, data) == data_size)
    stream->operations->close (stream);
}

static void save_binary (const char *path,
                         void *instance,
                         kan_interned_string_t type,
                         kan_serialization_binary_script_storage_t script_storage,
                         kan_serialization_interned_string_registry_t string_registry,
                         kan_bool_t write_header)
{
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (path, KAN_TRUE);
    KAN_TEST_ASSERT (stream)

    if (write_header)
    {
        KAN_TEST_ASSERT (kan_serialization_binary_write_type_header (stream, type, string_registry))
    }

    kan_serialization_binary_writer_t writer =
        kan_serialization_binary_writer_create (stream, instance, type, script_storage, string_registry);

    enum kan_serialization_state_t state;
    while ((state = kan_serialization_binary_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    KAN_TEST_ASSERT (state == KAN_SERIALIZATION_FINISHED)
    kan_serialization_binary_writer_destroy (writer);
    stream->operations->close (stream);
}

static void setup_binary_workspace (kan_reflection_registry_t registry,
                                    kan_bool_t make_index,
                                    kan_bool_t use_string_registry)
{
    initialize_resources ();
    kan_file_system_make_directory (WORKSPACE_SUB_DIRECTORY);

    kan_serialization_binary_script_storage_t storage = kan_serialization_binary_script_storage_create (registry);
    kan_serialization_interned_string_registry_t string_registry = KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY;

    if (use_string_registry)
    {
        string_registry = kan_serialization_interned_string_registry_create_empty ();
    }

    kan_file_system_make_directory (WORKSPACE_SUB_DIRECTORY "/bulk");
    save_binary (WORKSPACE_SUB_DIRECTORY "/bulk/alpha.bin", &resource_alpha,
                 kan_string_intern ("first_resource_type_t"), storage, string_registry, KAN_TRUE);
    save_binary (WORKSPACE_SUB_DIRECTORY "/bulk/beta.bin", &resource_beta, kan_string_intern ("first_resource_type_t"),
                 storage, string_registry, KAN_TRUE);

    kan_file_system_make_directory (WORKSPACE_SUB_DIRECTORY "/config");
    kan_file_system_make_directory (WORKSPACE_SUB_DIRECTORY "/config/common");
    save_binary (WORKSPACE_SUB_DIRECTORY "/config/common/players.bin", &resource_players,
                 kan_string_intern ("second_resource_type_t"), storage, string_registry, KAN_TRUE);

    kan_file_system_make_directory (WORKSPACE_SUB_DIRECTORY "/config/map_1");
    save_binary (WORKSPACE_SUB_DIRECTORY "/config/map_1/characters.bin", &resource_characters,
                 kan_string_intern ("second_resource_type_t"), storage, string_registry, KAN_TRUE);

    kan_file_system_make_directory (WORKSPACE_SUB_DIRECTORY "/images");
    save_third_party (WORKSPACE_SUB_DIRECTORY "/images/test_third_party", (uint8_t *) resource_test_third_party,
                      sizeof (resource_test_third_party));

    if (make_index)
    {
        struct kan_resource_index_t resource_index;
        kan_resource_index_init (&resource_index);

        kan_resource_index_add_native_entry (&resource_index, kan_string_intern ("first_resource_type_t"),
                                             kan_string_intern ("alpha"), KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY,
                                             "bulk/alpha.bin");

        kan_resource_index_add_native_entry (&resource_index, kan_string_intern ("first_resource_type_t"),
                                             kan_string_intern ("beta"), KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY,
                                             "bulk/beta.bin");

        kan_resource_index_add_native_entry (&resource_index, kan_string_intern ("second_resource_type_t"),
                                             kan_string_intern ("players"),
                                             KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY, "config/common/players.bin");

        kan_resource_index_add_native_entry (
            &resource_index, kan_string_intern ("second_resource_type_t"), kan_string_intern ("characters"),
            KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY, "config/map_1/characters.bin");

        kan_resource_index_add_third_party_entry (&resource_index, kan_string_intern ("test_third_party"),
                                                  "images/test_third_party", sizeof (resource_test_third_party));

        save_binary (WORKSPACE_SUB_DIRECTORY "/" KAN_RESOURCE_INDEX_DEFAULT_NAME, &resource_index,
                     kan_string_intern ("kan_resource_index_t"), storage, string_registry, KAN_FALSE);
        kan_resource_index_shutdown (&resource_index);
    }

    if (string_registry != KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY)
    {
        struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (
            WORKSPACE_SUB_DIRECTORY "/" KAN_RESOURCE_INDEX_ACCOMPANYING_STRING_REGISTRY_DEFAULT_NAME, KAN_TRUE);
        KAN_TEST_ASSERT (stream)

        kan_serialization_interned_string_registry_writer_t writer =
            kan_serialization_interned_string_registry_writer_create (stream, string_registry);

        enum kan_serialization_state_t state;
        while ((state = kan_serialization_interned_string_registry_writer_step (writer)) ==
               KAN_SERIALIZATION_IN_PROGRESS)
        {
        }

        KAN_TEST_ASSERT (state == KAN_SERIALIZATION_FINISHED)
        kan_serialization_interned_string_registry_writer_destroy (writer);
        kan_serialization_interned_string_registry_destroy (string_registry);
        stream->operations->close (stream);
    }

    kan_serialization_binary_script_storage_destroy (storage);
}

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

static void setup_rd_workspace (kan_reflection_registry_t registry, kan_bool_t make_index)
{
    initialize_resources ();
    kan_file_system_make_directory (WORKSPACE_SUB_DIRECTORY);

    kan_file_system_make_directory (WORKSPACE_SUB_DIRECTORY "/bulk");
    save_rd (WORKSPACE_SUB_DIRECTORY "/bulk/alpha.rd", &resource_alpha, kan_string_intern ("first_resource_type_t"),
             registry);
    save_rd (WORKSPACE_SUB_DIRECTORY "/bulk/beta.rd", &resource_beta, kan_string_intern ("first_resource_type_t"),
             registry);

    kan_file_system_make_directory (WORKSPACE_SUB_DIRECTORY "/config");
    kan_file_system_make_directory (WORKSPACE_SUB_DIRECTORY "/config/common");
    save_rd (WORKSPACE_SUB_DIRECTORY "/config/common/players.rd", &resource_players,
             kan_string_intern ("second_resource_type_t"), registry);

    kan_file_system_make_directory (WORKSPACE_SUB_DIRECTORY "/config/map_1");
    save_rd (WORKSPACE_SUB_DIRECTORY "/config/map_1/characters.rd", &resource_characters,
             kan_string_intern ("second_resource_type_t"), registry);

    kan_file_system_make_directory (WORKSPACE_SUB_DIRECTORY "/images");
    save_third_party (WORKSPACE_SUB_DIRECTORY "/images/test_third_party", (uint8_t *) resource_test_third_party,
                      sizeof (resource_test_third_party));

    if (make_index)
    {
        struct kan_resource_index_t resource_index;
        kan_resource_index_init (&resource_index);

        kan_resource_index_add_native_entry (&resource_index, kan_string_intern ("first_resource_type_t"),
                                             kan_string_intern ("alpha"),
                                             KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA, "bulk/alpha.rd");

        kan_resource_index_add_native_entry (&resource_index, kan_string_intern ("first_resource_type_t"),
                                             kan_string_intern ("beta"),
                                             KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA, "bulk/beta.rd");

        kan_resource_index_add_native_entry (
            &resource_index, kan_string_intern ("second_resource_type_t"), kan_string_intern ("players"),
            KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA, "config/common/players.rd");

        kan_resource_index_add_native_entry (
            &resource_index, kan_string_intern ("second_resource_type_t"), kan_string_intern ("characters"),
            KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_READABLE_DATA, "config/map_1/characters.rd");

        kan_resource_index_add_third_party_entry (&resource_index, kan_string_intern ("test_third_party"),
                                                  "images/test_third_party", sizeof (resource_test_third_party));

        kan_serialization_binary_script_storage_t storage = kan_serialization_binary_script_storage_create (registry);
        save_binary (WORKSPACE_SUB_DIRECTORY "/" KAN_RESOURCE_INDEX_DEFAULT_NAME, &resource_index,
                     kan_string_intern ("kan_resource_index_t"), storage,
                     KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY, KAN_FALSE);
        kan_serialization_binary_script_storage_destroy (storage);
        kan_resource_index_shutdown (&resource_index);
    }
}

static void setup_indexing_stress_test_workspace (kan_reflection_registry_t registry)
{
    initialize_resources ();
    kan_file_system_make_directory (WORKSPACE_SUB_DIRECTORY);

    kan_serialization_binary_script_storage_t storage = kan_serialization_binary_script_storage_create (registry);
    kan_serialization_interned_string_registry_t string_registry = KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY;
    string_registry = kan_serialization_interned_string_registry_create_empty ();

    struct kan_resource_index_t resource_index;
    kan_resource_index_init (&resource_index);

    for (uint64_t index = 0u; index < 20000u; ++index)
    {
        char path_buffer[KAN_FILE_SYSTEM_MAX_PATH_LENGTH];
        char indexed_path_buffer[KAN_FILE_SYSTEM_MAX_PATH_LENGTH];

        snprintf (path_buffer, KAN_FILE_SYSTEM_MAX_PATH_LENGTH, "%s/dir%llu", WORKSPACE_SUB_DIRECTORY,
                  (unsigned long long) (index % 20u));

        if (!kan_file_system_check_existence (path_buffer))
        {
            kan_file_system_make_directory (path_buffer);
        }

        char name_buffer[256u];
        snprintf (name_buffer, 256u, "file%llu", (unsigned long long) index);

        snprintf (path_buffer, KAN_FILE_SYSTEM_MAX_PATH_LENGTH, "%s/dir%llu/%s.bin", WORKSPACE_SUB_DIRECTORY,
                  (unsigned long long) (index % 20u), name_buffer);

        snprintf (indexed_path_buffer, KAN_FILE_SYSTEM_MAX_PATH_LENGTH, "dir%llu/%s.bin",
                  (unsigned long long) (index % 20u), name_buffer);

        switch (index % 3u)
        {
        case 0u:
            save_binary (path_buffer, &resource_alpha, kan_string_intern ("first_resource_type_t"), storage,
                         string_registry, KAN_TRUE);
            kan_resource_index_add_native_entry (&resource_index, kan_string_intern ("first_resource_type_t"),
                                                 kan_string_intern (name_buffer),
                                                 KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY, indexed_path_buffer);
            break;

        case 1u:
            save_binary (path_buffer, &resource_beta, kan_string_intern ("first_resource_type_t"), storage,
                         string_registry, KAN_TRUE);
            kan_resource_index_add_native_entry (&resource_index, kan_string_intern ("first_resource_type_t"),
                                                 kan_string_intern (name_buffer),
                                                 KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY, indexed_path_buffer);
            break;

        case 2u:
            save_binary (path_buffer, &resource_players, kan_string_intern ("second_resource_type_t"), storage,
                         string_registry, KAN_TRUE);
            kan_resource_index_add_native_entry (&resource_index, kan_string_intern ("second_resource_type_t"),
                                                 kan_string_intern (name_buffer),
                                                 KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY, indexed_path_buffer);
            break;

        case 3u:
            save_binary (path_buffer, &resource_characters, kan_string_intern ("second_resource_type_t"), storage,
                         string_registry, KAN_TRUE);
            kan_resource_index_add_native_entry (&resource_index, kan_string_intern ("second_resource_type_t"),
                                                 kan_string_intern (name_buffer),
                                                 KAN_RESOURCE_INDEX_NATIVE_ITEM_FORMAT_BINARY, indexed_path_buffer);
            break;
        }
    }

    save_binary (WORKSPACE_SUB_DIRECTORY "/" KAN_RESOURCE_INDEX_DEFAULT_NAME, &resource_index,
                 kan_string_intern ("kan_resource_index_t"), storage, string_registry, KAN_FALSE);
    kan_resource_index_shutdown (&resource_index);

    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (
        WORKSPACE_SUB_DIRECTORY "/" KAN_RESOURCE_INDEX_ACCOMPANYING_STRING_REGISTRY_DEFAULT_NAME, KAN_TRUE);
    KAN_TEST_ASSERT (stream)

    kan_serialization_interned_string_registry_writer_t writer =
        kan_serialization_interned_string_registry_writer_create (stream, string_registry);

    enum kan_serialization_state_t state;
    while ((state = kan_serialization_interned_string_registry_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    KAN_TEST_ASSERT (state == KAN_SERIALIZATION_FINISHED)
    kan_serialization_interned_string_registry_writer_destroy (writer);
    kan_serialization_interned_string_registry_destroy (string_registry);
    stream->operations->close (stream);
    kan_serialization_binary_script_storage_destroy (storage);
}

enum check_observation_and_reload_stage_t
{
    CHECK_OBSERVATION_AND_RELOAD_STAGE_INIT = 0,
    CHECK_OBSERVATION_AND_RELOAD_STAGE_ADD_ALPHA,
    CHECK_OBSERVATION_AND_RELOAD_STAGE_CHANGE_TO_BETA,
    CHECK_OBSERVATION_AND_RELOAD_STAGE_CHANGE_TO_CHARACTERS,
    CHECK_OBSERVATION_AND_RELOAD_STAGE_REMOVE_CHARACTERS,
    CHECK_OBSERVATION_AND_RELOAD_STAGE_ADD_THIRD_PARTY,
    CHECK_OBSERVATION_AND_RELOAD_STAGE_CHANGE_THIRD_PARTY,
    CHECK_OBSERVATION_AND_RELOAD_STAGE_REMOVE_THIRD_PARTY,
};

struct check_observation_and_reload_singleton_t
{
    enum check_observation_and_reload_stage_t stage;
    uint64_t request_id;
};

TEST_UNIVERSE_RESOURCE_PROVIDER_API void check_observation_and_reload_singleton_init (
    struct check_observation_and_reload_singleton_t *instance)
{
    instance->stage = CHECK_OBSERVATION_AND_RELOAD_STAGE_INIT;
}

struct check_observation_and_reload_state_t
{
    struct kan_repository_singleton_write_query_t write__check_observation_and_reload_singleton;
    struct kan_repository_singleton_read_query_t read__kan_resource_provider_singleton;
    struct kan_repository_indexed_insert_query_t insert__kan_resource_request;
    struct kan_repository_indexed_value_update_query_t update_value__kan_resource_request__request_id;

    struct kan_repository_indexed_value_read_query_t
        read_value__resource_provider_container_first_resource_type__container_id;
    struct kan_repository_indexed_value_read_query_t
        read_value__resource_provider_container_second_resource_type__container_id;

    struct kan_repository_event_fetch_query_t fetch__kan_resource_request_updated_event;

    kan_reflection_registry_t current_registry;
};

TEST_UNIVERSE_RESOURCE_PROVIDER_API void kan_universe_mutator_deploy_check_observation_and_reload (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct check_observation_and_reload_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
    state->current_registry = kan_universe_get_reflection_registry (universe);
}

TEST_UNIVERSE_RESOURCE_PROVIDER_API void kan_universe_mutator_execute_check_observation_and_reload (
    kan_cpu_job_t job, struct check_observation_and_reload_state_t *state)
{
    kan_repository_singleton_write_access_t singleton_access =
        kan_repository_singleton_write_query_execute (&state->write__check_observation_and_reload_singleton);

    struct check_observation_and_reload_singleton_t *singleton =
        kan_repository_singleton_write_access_resolve (singleton_access);

    switch (singleton->stage)
    {
    case CHECK_OBSERVATION_AND_RELOAD_STAGE_INIT:
    {
        kan_repository_singleton_read_access_t provider_access =
            kan_repository_singleton_read_query_execute (&state->read__kan_resource_provider_singleton);

        const struct kan_resource_provider_singleton_t *provider =
            kan_repository_singleton_read_access_resolve (provider_access);

        struct kan_repository_indexed_insertion_package_t package =
            kan_repository_indexed_insert_query_execute (&state->insert__kan_resource_request);
        struct kan_resource_request_t *request = kan_repository_indexed_insertion_package_get (&package);

        request->request_id = kan_next_resource_request_id (provider);
        request->type = kan_string_intern ("first_resource_type_t");
        request->name = kan_string_intern ("alpha");
        request->priority = 0u;
        singleton->request_id = request->request_id;
        kan_repository_indexed_insertion_package_submit (&package);

        kan_repository_singleton_read_access_close (provider_access);
        singleton->stage = CHECK_OBSERVATION_AND_RELOAD_STAGE_ADD_ALPHA;

        save_rd (WORKSPACE_SUB_DIRECTORY "/alpha.rd", &resource_alpha, kan_string_intern ("first_resource_type_t"),
                 state->current_registry);
        break;
    }

    case CHECK_OBSERVATION_AND_RELOAD_STAGE_ADD_ALPHA:
    {
        while (KAN_TRUE)
        {
            struct kan_repository_event_read_access_t event_access =
                kan_repository_event_fetch_query_next (&state->fetch__kan_resource_request_updated_event);

            const struct kan_resource_request_updated_event_t *event =
                kan_repository_event_read_access_resolve (&event_access);

            if (event)
            {
                if (event->request_id == singleton->request_id)
                {
                    KAN_TEST_CHECK (event->type == kan_string_intern ("first_resource_type_t"))
                    struct kan_repository_indexed_value_update_cursor_t request_cursor =
                        kan_repository_indexed_value_update_query_execute (
                            &state->update_value__kan_resource_request__request_id, &singleton->request_id);

                    struct kan_repository_indexed_value_update_access_t request_access =
                        kan_repository_indexed_value_update_cursor_next (&request_cursor);

                    struct kan_resource_request_t *request =
                        kan_repository_indexed_value_update_access_resolve (&request_access);
                    KAN_TEST_ASSERT (request)
                    KAN_TEST_ASSERT (request->provided_container_id != KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)

                    struct kan_repository_indexed_value_read_cursor_t container_cursor =
                        kan_repository_indexed_value_read_query_execute (
                            &state->read_value__resource_provider_container_first_resource_type__container_id,
                            &request->provided_container_id);

                    struct kan_repository_indexed_value_read_access_t container_access =
                        kan_repository_indexed_value_read_cursor_next (&container_cursor);

                    struct kan_resource_container_view_t *view =
                        (struct kan_resource_container_view_t *) kan_repository_indexed_value_read_access_resolve (
                            &container_access);
                    KAN_TEST_CHECK (view)

                    if (view)
                    {
                        struct first_resource_type_t *loaded_resource =
                            (struct first_resource_type_t *) view->data_begin;
                        KAN_TEST_CHECK (loaded_resource->some_integer == resource_alpha.some_integer)
                        KAN_TEST_CHECK (loaded_resource->flag_1 == resource_alpha.flag_1)
                        KAN_TEST_CHECK (loaded_resource->flag_2 == resource_alpha.flag_2)
                        KAN_TEST_CHECK (loaded_resource->flag_3 == resource_alpha.flag_3)
                        KAN_TEST_CHECK (loaded_resource->flag_4 == resource_alpha.flag_4)
                        kan_repository_indexed_value_read_access_close (&container_access);
                    }

                    kan_repository_indexed_value_read_cursor_close (&container_cursor);
                    kan_repository_indexed_value_update_access_close (&request_access);
                    kan_repository_indexed_value_update_cursor_close (&request_cursor);

                    // Then we can rewrite alpha and wait for reload.
                    save_rd (WORKSPACE_SUB_DIRECTORY "/alpha.rd", &resource_beta,
                             kan_string_intern ("first_resource_type_t"), state->current_registry);
                }

                kan_repository_event_read_access_close (&event_access);
                singleton->stage = CHECK_OBSERVATION_AND_RELOAD_STAGE_CHANGE_TO_BETA;
            }
            else
            {
                break;
            }
        }

        break;
    }

    case CHECK_OBSERVATION_AND_RELOAD_STAGE_CHANGE_TO_BETA:
    {
        while (KAN_TRUE)
        {
            struct kan_repository_event_read_access_t event_access =
                kan_repository_event_fetch_query_next (&state->fetch__kan_resource_request_updated_event);

            const struct kan_resource_request_updated_event_t *event =
                kan_repository_event_read_access_resolve (&event_access);

            if (event)
            {
                if (event->request_id == singleton->request_id)
                {
                    KAN_TEST_CHECK (event->type == kan_string_intern ("first_resource_type_t"))
                    struct kan_repository_indexed_value_update_cursor_t request_cursor =
                        kan_repository_indexed_value_update_query_execute (
                            &state->update_value__kan_resource_request__request_id, &singleton->request_id);

                    struct kan_repository_indexed_value_update_access_t request_access =
                        kan_repository_indexed_value_update_cursor_next (&request_cursor);

                    struct kan_resource_request_t *request =
                        kan_repository_indexed_value_update_access_resolve (&request_access);
                    KAN_TEST_ASSERT (request)
                    KAN_TEST_ASSERT (request->provided_container_id != KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)

                    struct kan_repository_indexed_value_read_cursor_t container_cursor =
                        kan_repository_indexed_value_read_query_execute (
                            &state->read_value__resource_provider_container_first_resource_type__container_id,
                            &request->provided_container_id);

                    struct kan_repository_indexed_value_read_access_t container_access =
                        kan_repository_indexed_value_read_cursor_next (&container_cursor);

                    struct kan_resource_container_view_t *view =
                        (struct kan_resource_container_view_t *) kan_repository_indexed_value_read_access_resolve (
                            &container_access);
                    KAN_TEST_CHECK (view)

                    if (view)
                    {
                        struct first_resource_type_t *loaded_resource =
                            (struct first_resource_type_t *) view->data_begin;
                        KAN_TEST_CHECK (loaded_resource->some_integer == resource_beta.some_integer)
                        KAN_TEST_CHECK (loaded_resource->flag_1 == resource_beta.flag_1)
                        KAN_TEST_CHECK (loaded_resource->flag_2 == resource_beta.flag_2)
                        KAN_TEST_CHECK (loaded_resource->flag_3 == resource_beta.flag_3)
                        KAN_TEST_CHECK (loaded_resource->flag_4 == resource_beta.flag_4)
                        kan_repository_indexed_value_read_access_close (&container_access);
                    }

                    kan_repository_indexed_value_read_cursor_close (&container_cursor);
                    kan_repository_indexed_value_update_access_close (&request_access);
                    kan_repository_indexed_value_update_cursor_close (&request_cursor);

                    // Then we can rewrite alpha and wait for reload again.
                    save_rd (WORKSPACE_SUB_DIRECTORY "/alpha.rd", &resource_characters,
                             kan_string_intern ("second_resource_type_t"), state->current_registry);

                    singleton->stage = CHECK_OBSERVATION_AND_RELOAD_STAGE_CHANGE_TO_CHARACTERS;
                }

                kan_repository_event_read_access_close (&event_access);
            }
            else
            {
                break;
            }
        }

        break;
    }

    case CHECK_OBSERVATION_AND_RELOAD_STAGE_CHANGE_TO_CHARACTERS:
    {
        while (KAN_TRUE)
        {
            struct kan_repository_event_read_access_t event_access =
                kan_repository_event_fetch_query_next (&state->fetch__kan_resource_request_updated_event);

            const struct kan_resource_request_updated_event_t *event =
                kan_repository_event_read_access_resolve (&event_access);

            if (event)
            {
                if (event->request_id == singleton->request_id)
                {
                    struct kan_repository_indexed_value_update_cursor_t request_cursor =
                        kan_repository_indexed_value_update_query_execute (
                            &state->update_value__kan_resource_request__request_id, &singleton->request_id);

                    struct kan_repository_indexed_value_update_access_t request_access =
                        kan_repository_indexed_value_update_cursor_next (&request_cursor);

                    struct kan_resource_request_t *request =
                        kan_repository_indexed_value_update_access_resolve (&request_access);
                    KAN_TEST_ASSERT (request)

                    if (request->type == kan_string_intern ("first_resource_type_t"))
                    {
                        KAN_TEST_CHECK (request->provided_container_id == KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)
                        request->type = kan_string_intern ("second_resource_type_t");
                    }
                    else
                    {
                        KAN_TEST_ASSERT (request->type == kan_string_intern ("second_resource_type_t"))
                        KAN_TEST_ASSERT (request->provided_container_id != KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)

                        struct kan_repository_indexed_value_read_cursor_t container_cursor =
                            kan_repository_indexed_value_read_query_execute (
                                &state->read_value__resource_provider_container_second_resource_type__container_id,
                                &request->provided_container_id);

                        struct kan_repository_indexed_value_read_access_t container_access =
                            kan_repository_indexed_value_read_cursor_next (&container_cursor);

                        struct kan_resource_container_view_t *view =
                            (struct kan_resource_container_view_t *) kan_repository_indexed_value_read_access_resolve (
                                &container_access);
                        KAN_TEST_CHECK (view)

                        if (view)
                        {
                            struct second_resource_type_t *loaded_resource =
                                (struct second_resource_type_t *) view->data_begin;
                            KAN_TEST_CHECK (loaded_resource->first_id == resource_characters.first_id)
                            KAN_TEST_CHECK (loaded_resource->second_id == resource_characters.second_id)
                            kan_repository_indexed_value_read_access_close (&container_access);
                        }

                        kan_repository_indexed_value_read_cursor_close (&container_cursor);

                        // Then we can remove alpha.
                        kan_file_system_remove_file (WORKSPACE_SUB_DIRECTORY "/alpha.rd");

                        singleton->stage = CHECK_OBSERVATION_AND_RELOAD_STAGE_REMOVE_CHARACTERS;
                    }

                    kan_repository_indexed_value_update_access_close (&request_access);
                    kan_repository_indexed_value_update_cursor_close (&request_cursor);
                }

                kan_repository_event_read_access_close (&event_access);
            }
            else
            {
                break;
            }
        }

        break;
    }

    case CHECK_OBSERVATION_AND_RELOAD_STAGE_REMOVE_CHARACTERS:
    {
        while (KAN_TRUE)
        {
            struct kan_repository_event_read_access_t event_access =
                kan_repository_event_fetch_query_next (&state->fetch__kan_resource_request_updated_event);

            const struct kan_resource_request_updated_event_t *event =
                kan_repository_event_read_access_resolve (&event_access);

            if (event)
            {
                if (event->request_id == singleton->request_id)
                {
                    KAN_TEST_CHECK (event->type == kan_string_intern ("second_resource_type_t"))
                    struct kan_repository_indexed_value_update_cursor_t request_cursor =
                        kan_repository_indexed_value_update_query_execute (
                            &state->update_value__kan_resource_request__request_id, &singleton->request_id);

                    struct kan_repository_indexed_value_update_access_t request_access =
                        kan_repository_indexed_value_update_cursor_next (&request_cursor);

                    struct kan_resource_request_t *request =
                        kan_repository_indexed_value_update_access_resolve (&request_access);
                    KAN_TEST_ASSERT (request)
                    KAN_TEST_CHECK (request->provided_container_id == KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)

                    request->type = NULL;
                    request->name = kan_string_intern ("test_third_party.data");

                    kan_repository_indexed_value_update_access_close (&request_access);
                    kan_repository_indexed_value_update_cursor_close (&request_cursor);

                    singleton->stage = CHECK_OBSERVATION_AND_RELOAD_STAGE_ADD_THIRD_PARTY;
                    save_third_party (WORKSPACE_SUB_DIRECTORY "/test_third_party.data",
                                      (uint8_t *) resource_test_third_party, sizeof (resource_test_third_party));
                }

                kan_repository_event_read_access_close (&event_access);
            }
            else
            {
                break;
            }
        }

        break;
    }

    case CHECK_OBSERVATION_AND_RELOAD_STAGE_ADD_THIRD_PARTY:
    {
        while (KAN_TRUE)
        {
            struct kan_repository_event_read_access_t event_access =
                kan_repository_event_fetch_query_next (&state->fetch__kan_resource_request_updated_event);

            const struct kan_resource_request_updated_event_t *event =
                kan_repository_event_read_access_resolve (&event_access);

            if (event)
            {
                if (event->request_id == singleton->request_id)
                {
                    KAN_TEST_CHECK (!event->type)
                    struct kan_repository_indexed_value_update_cursor_t request_cursor =
                        kan_repository_indexed_value_update_query_execute (
                            &state->update_value__kan_resource_request__request_id, &singleton->request_id);

                    struct kan_repository_indexed_value_update_access_t request_access =
                        kan_repository_indexed_value_update_cursor_next (&request_cursor);

                    struct kan_resource_request_t *request =
                        kan_repository_indexed_value_update_access_resolve (&request_access);
                    KAN_TEST_ASSERT (request)
                    KAN_TEST_ASSERT (request->provided_third_party.data)
                    KAN_TEST_CHECK (request->provided_third_party.size == sizeof (resource_test_third_party))
                    KAN_TEST_CHECK (memcmp (request->provided_third_party.data, resource_test_third_party,
                                            sizeof (resource_test_third_party)) == 0)

                    kan_repository_indexed_value_update_access_close (&request_access);
                    kan_repository_indexed_value_update_cursor_close (&request_cursor);

                    singleton->stage = CHECK_OBSERVATION_AND_RELOAD_STAGE_CHANGE_THIRD_PARTY;
                    save_third_party (WORKSPACE_SUB_DIRECTORY "/test_third_party.data",
                                      (uint8_t *) resource_test_third_party_changed,
                                      sizeof (resource_test_third_party_changed));
                }

                kan_repository_event_read_access_close (&event_access);
            }
            else
            {
                break;
            }
        }

        break;
    }

    case CHECK_OBSERVATION_AND_RELOAD_STAGE_CHANGE_THIRD_PARTY:
    {
        while (KAN_TRUE)
        {
            struct kan_repository_event_read_access_t event_access =
                kan_repository_event_fetch_query_next (&state->fetch__kan_resource_request_updated_event);

            const struct kan_resource_request_updated_event_t *event =
                kan_repository_event_read_access_resolve (&event_access);

            if (event)
            {
                if (event->request_id == singleton->request_id)
                {
                    KAN_TEST_CHECK (!event->type)
                    struct kan_repository_indexed_value_update_cursor_t request_cursor =
                        kan_repository_indexed_value_update_query_execute (
                            &state->update_value__kan_resource_request__request_id, &singleton->request_id);

                    struct kan_repository_indexed_value_update_access_t request_access =
                        kan_repository_indexed_value_update_cursor_next (&request_cursor);

                    struct kan_resource_request_t *request =
                        kan_repository_indexed_value_update_access_resolve (&request_access);
                    KAN_TEST_ASSERT (request)
                    KAN_TEST_ASSERT (request->provided_third_party.data)
                    KAN_TEST_CHECK (request->provided_third_party.size == sizeof (resource_test_third_party_changed))
                    KAN_TEST_CHECK (memcmp (request->provided_third_party.data, resource_test_third_party_changed,
                                            sizeof (resource_test_third_party_changed)) == 0)

                    kan_repository_indexed_value_update_access_close (&request_access);
                    kan_repository_indexed_value_update_cursor_close (&request_cursor);

                    singleton->stage = CHECK_OBSERVATION_AND_RELOAD_STAGE_REMOVE_THIRD_PARTY;
                    kan_file_system_remove_file (WORKSPACE_SUB_DIRECTORY "/test_third_party.data");
                }

                kan_repository_event_read_access_close (&event_access);
            }
            else
            {
                break;
            }
        }

        break;
    }

    case CHECK_OBSERVATION_AND_RELOAD_STAGE_REMOVE_THIRD_PARTY:
    {
        while (KAN_TRUE)
        {
            struct kan_repository_event_read_access_t event_access =
                kan_repository_event_fetch_query_next (&state->fetch__kan_resource_request_updated_event);

            const struct kan_resource_request_updated_event_t *event =
                kan_repository_event_read_access_resolve (&event_access);

            if (event)
            {
                if (event->request_id == singleton->request_id)
                {
                    KAN_TEST_CHECK (!event->type)
                    struct kan_repository_indexed_value_update_cursor_t request_cursor =
                        kan_repository_indexed_value_update_query_execute (
                            &state->update_value__kan_resource_request__request_id, &singleton->request_id);

                    struct kan_repository_indexed_value_update_access_t request_access =
                        kan_repository_indexed_value_update_cursor_next (&request_cursor);

                    struct kan_resource_request_t *request =
                        kan_repository_indexed_value_update_access_resolve (&request_access);
                    KAN_TEST_ASSERT (request)
                    KAN_TEST_CHECK (!request->provided_third_party.data)

                    kan_repository_indexed_value_update_access_close (&request_access);
                    kan_repository_indexed_value_update_cursor_close (&request_cursor);

                    global_test_finished = KAN_TRUE;
                }

                kan_repository_event_read_access_close (&event_access);
            }
            else
            {
                break;
            }
        }

        break;
    }
    }

    kan_repository_singleton_write_access_close (singleton_access);
    kan_cpu_job_release (job);
}

struct indexed_stress_test_singleton_t
{
    kan_bool_t requests_created;
    uint64_t request_id;
};

TEST_UNIVERSE_RESOURCE_PROVIDER_API void indexed_stress_test_singleton_init (
    struct indexed_stress_test_singleton_t *instance)
{
    instance->requests_created = KAN_FALSE;
}

struct indexed_stress_test_state_t
{
    struct kan_repository_singleton_write_query_t write__indexed_stress_test_singleton;
    struct kan_repository_singleton_read_query_t read__kan_resource_provider_singleton;
    struct kan_repository_indexed_insert_query_t insert__kan_resource_request;
    struct kan_repository_indexed_value_read_query_t read_value__kan_resource_request__request_id;
    struct kan_repository_indexed_value_read_query_t
        read_value__resource_provider_container_first_resource_type__container_id;
};

TEST_UNIVERSE_RESOURCE_PROVIDER_API void kan_universe_mutator_deploy_indexed_stress_test (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct indexed_stress_test_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
}

TEST_UNIVERSE_RESOURCE_PROVIDER_API void kan_universe_mutator_execute_indexed_stress_test (
    kan_cpu_job_t job, struct indexed_stress_test_state_t *state)
{
    kan_repository_singleton_write_access_t singleton_access =
        kan_repository_singleton_write_query_execute (&state->write__indexed_stress_test_singleton);

    struct indexed_stress_test_singleton_t *singleton =
        (struct indexed_stress_test_singleton_t *) kan_repository_singleton_write_access_resolve (singleton_access);

    if (!singleton->requests_created)
    {
        kan_repository_singleton_read_access_t provider_access =
            kan_repository_singleton_read_query_execute (&state->read__kan_resource_provider_singleton);

        const struct kan_resource_provider_singleton_t *provider =
            kan_repository_singleton_read_access_resolve (provider_access);

        struct kan_repository_indexed_insertion_package_t package =
            kan_repository_indexed_insert_query_execute (&state->insert__kan_resource_request);
        struct kan_resource_request_t *request = kan_repository_indexed_insertion_package_get (&package);

        request->request_id = kan_next_resource_request_id (provider);
        request->type = kan_string_intern ("first_resource_type_t");
        request->name = kan_string_intern ("file0");
        request->priority = 0u;
        singleton->request_id = request->request_id;
        kan_repository_indexed_insertion_package_submit (&package);

        kan_repository_singleton_read_access_close (provider_access);
        singleton->requests_created = KAN_TRUE;
    }

    struct kan_repository_indexed_value_read_cursor_t request_cursor = kan_repository_indexed_value_read_query_execute (
        &state->read_value__kan_resource_request__request_id, &singleton->request_id);

    struct kan_repository_indexed_value_read_access_t request_access =
        kan_repository_indexed_value_read_cursor_next (&request_cursor);

    const struct kan_resource_request_t *request = kan_repository_indexed_value_read_access_resolve (&request_access);
    KAN_TEST_ASSERT (request)

    if (request->provided_container_id != KAN_RESOURCE_PROVIDER_CONTAINER_ID_NONE)
    {
        global_test_finished = KAN_TRUE;
    }

    kan_repository_indexed_value_read_access_close (&request_access);
    kan_repository_indexed_value_read_cursor_close (&request_cursor);

    kan_repository_singleton_write_access_close (singleton_access);
    kan_cpu_job_release (job);
}

static kan_context_handle_t setup_context (void)
{
    kan_context_handle_t context =
        kan_context_create (kan_allocation_group_get_child (kan_allocation_group_root (), "context"));
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME, NULL))

    struct kan_virtual_file_system_config_mount_real_t mount_point = {
        .next = NULL,
        .mount_path = WORKSPACE_MOUNT_PATH,
        .real_path = WORKSPACE_SUB_DIRECTORY,
    };

    struct kan_virtual_file_system_config_t virtual_file_system_config = {
        .first_mount_real = &mount_point,
        .first_mount_read_only_pack = NULL,
    };

    KAN_TEST_CHECK (
        kan_context_request_system (context, KAN_CONTEXT_VIRTUAL_FILE_SYSTEM_NAME, &virtual_file_system_config))
    kan_context_assembly (context);
    return context;
}

static void run_request_resources_and_check_test (kan_context_handle_t context)
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

    struct kan_resource_provider_configuration_t resource_provider_configuration = {
        .scan_budget_ns = 2000000u,
        .load_budget_ns = 2000000u,
        .add_wait_time_ns = 100000000u,
        .modify_wait_time_ns = 100000000u,
        .use_load_only_string_registry = KAN_TRUE,
        .observe_file_system = KAN_FALSE,
        .resource_directory_path = kan_string_intern (WORKSPACE_MOUNT_PATH),
    };

    kan_reflection_patch_builder_t patch_builder = kan_reflection_patch_builder_create ();
    kan_reflection_patch_builder_add_chunk (patch_builder, 0u, sizeof (struct kan_resource_provider_configuration_t),
                                            &resource_provider_configuration);
    kan_reflection_patch_t resource_provider_configuration_patch = kan_reflection_patch_builder_build (
        patch_builder, registry,
        kan_reflection_registry_query_struct (registry, kan_string_intern ("kan_resource_provider_configuration_t")));
    kan_reflection_patch_builder_destroy (patch_builder);

    kan_dynamic_array_set_capacity (&definition.configuration, 1u);
    *(struct kan_universe_world_configuration_t *) kan_dynamic_array_add_last (&definition.configuration) =
        (struct kan_universe_world_configuration_t) {
            .name = kan_string_intern (KAN_RESOURCE_PROVIDER_CONFIGURATION),
            .data = resource_provider_configuration_patch,
        };

    kan_dynamic_array_set_capacity (&definition.pipelines, 1u);
    struct kan_universe_world_pipeline_definition_t *update_pipeline =
        kan_dynamic_array_add_last (&definition.pipelines);

    kan_universe_world_pipeline_definition_init (update_pipeline);
    update_pipeline->name = kan_string_intern ("update");

    kan_dynamic_array_set_capacity (&update_pipeline->mutators, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutators) =
        kan_string_intern ("request_resources_and_check");

    kan_dynamic_array_set_capacity (&update_pipeline->mutator_groups, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutator_groups) =
        kan_string_intern (KAN_RESOURCE_PROVIDER_MUTATOR_GROUP);

    kan_universe_deploy_root (universe, &definition);
    kan_universe_world_definition_shutdown (&definition);

    while (!global_test_finished)
    {
        kan_update_system_run (update_system);
    }
}

KAN_TEST_CASE (binary)
{
    kan_file_system_remove_directory_with_content (WORKSPACE_SUB_DIRECTORY);
    kan_context_handle_t context = setup_context ();
    kan_context_system_handle_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_TEST_ASSERT (reflection_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)

    setup_binary_workspace (kan_reflection_system_get_registry (reflection_system), KAN_FALSE, KAN_FALSE);
    run_request_resources_and_check_test (context);
    kan_context_destroy (context);
}

KAN_TEST_CASE (binary_with_index)
{
    kan_file_system_remove_directory_with_content (WORKSPACE_SUB_DIRECTORY);
    kan_context_handle_t context = setup_context ();
    kan_context_system_handle_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_TEST_ASSERT (reflection_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)

    setup_binary_workspace (kan_reflection_system_get_registry (reflection_system), KAN_TRUE, KAN_FALSE);
    run_request_resources_and_check_test (context);
    kan_context_destroy (context);
}

KAN_TEST_CASE (binary_with_index_and_string_registry)
{
    kan_file_system_remove_directory_with_content (WORKSPACE_SUB_DIRECTORY);
    kan_context_handle_t context = setup_context ();
    kan_context_system_handle_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_TEST_ASSERT (reflection_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)

    setup_binary_workspace (kan_reflection_system_get_registry (reflection_system), KAN_TRUE, KAN_TRUE);
    run_request_resources_and_check_test (context);
    kan_context_destroy (context);
}

KAN_TEST_CASE (readable_data)
{
    kan_file_system_remove_directory_with_content (WORKSPACE_SUB_DIRECTORY);
    kan_context_handle_t context = setup_context ();
    kan_context_system_handle_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_TEST_ASSERT (reflection_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)

    setup_rd_workspace (kan_reflection_system_get_registry (reflection_system), KAN_FALSE);
    run_request_resources_and_check_test (context);
    kan_context_destroy (context);
}

KAN_TEST_CASE (readable_data_with_index)
{
    kan_file_system_remove_directory_with_content (WORKSPACE_SUB_DIRECTORY);
    kan_context_handle_t context = setup_context ();
    kan_context_system_handle_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_TEST_ASSERT (reflection_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)

    setup_rd_workspace (kan_reflection_system_get_registry (reflection_system), KAN_TRUE);
    run_request_resources_and_check_test (context);
    kan_context_destroy (context);
}

KAN_TEST_CASE (file_system_observation)
{
    initialize_resources ();
    kan_file_system_remove_directory_with_content (WORKSPACE_SUB_DIRECTORY);
    kan_file_system_make_directory (WORKSPACE_SUB_DIRECTORY);
    kan_context_handle_t context = setup_context ();

    kan_context_system_handle_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_TEST_ASSERT (reflection_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    kan_reflection_registry_t registry = kan_reflection_system_get_registry (reflection_system);

    kan_context_system_handle_t universe_system_handle = kan_context_query (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME);
    KAN_TEST_ASSERT (universe_system_handle != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)

    kan_universe_t universe = kan_universe_system_get_universe (universe_system_handle);
    KAN_TEST_ASSERT (universe != KAN_INVALID_UNIVERSE)

    kan_context_system_handle_t update_system = kan_context_query (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
    KAN_TEST_ASSERT (update_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)

    struct kan_universe_world_definition_t definition;
    kan_universe_world_definition_init (&definition);
    definition.world_name = kan_string_intern ("root_world");
    definition.scheduler_name = kan_string_intern ("run_update");

    struct kan_resource_provider_configuration_t resource_provider_configuration = {
        .scan_budget_ns = 2000000u,
        .load_budget_ns = 2000000u,
        .add_wait_time_ns = 100000000u,
        .modify_wait_time_ns = 100000000u,
        .use_load_only_string_registry = KAN_TRUE,
        .observe_file_system = KAN_TRUE,
        .resource_directory_path = kan_string_intern (WORKSPACE_MOUNT_PATH),
    };

    kan_reflection_patch_builder_t patch_builder = kan_reflection_patch_builder_create ();
    kan_reflection_patch_builder_add_chunk (patch_builder, 0u, sizeof (struct kan_resource_provider_configuration_t),
                                            &resource_provider_configuration);
    kan_reflection_patch_t resource_provider_configuration_patch = kan_reflection_patch_builder_build (
        patch_builder, registry,
        kan_reflection_registry_query_struct (registry, kan_string_intern ("kan_resource_provider_configuration_t")));
    kan_reflection_patch_builder_destroy (patch_builder);

    kan_dynamic_array_set_capacity (&definition.configuration, 1u);
    *(struct kan_universe_world_configuration_t *) kan_dynamic_array_add_last (&definition.configuration) =
        (struct kan_universe_world_configuration_t) {
            .name = kan_string_intern (KAN_RESOURCE_PROVIDER_CONFIGURATION),
            .data = resource_provider_configuration_patch,
        };

    kan_dynamic_array_set_capacity (&definition.pipelines, 1u);
    struct kan_universe_world_pipeline_definition_t *update_pipeline =
        kan_dynamic_array_add_last (&definition.pipelines);

    kan_universe_world_pipeline_definition_init (update_pipeline);
    update_pipeline->name = kan_string_intern ("update");

    kan_dynamic_array_set_capacity (&update_pipeline->mutators, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutators) =
        kan_string_intern ("check_observation_and_reload");

    kan_dynamic_array_set_capacity (&update_pipeline->mutator_groups, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutator_groups) =
        kan_string_intern (KAN_RESOURCE_PROVIDER_MUTATOR_GROUP);

    kan_universe_deploy_root (universe, &definition);
    kan_universe_world_definition_shutdown (&definition);

    while (!global_test_finished)
    {
        kan_update_system_run (update_system);
    }

    kan_context_destroy (context);
}

KAN_TEST_CASE (indexing_stress_test)
{
    initialize_resources ();
    kan_file_system_remove_directory_with_content (WORKSPACE_SUB_DIRECTORY);
    kan_file_system_make_directory (WORKSPACE_SUB_DIRECTORY);
    kan_context_handle_t context = setup_context ();

    kan_context_system_handle_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_TEST_ASSERT (reflection_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    kan_reflection_registry_t registry = kan_reflection_system_get_registry (reflection_system);

    kan_context_system_handle_t universe_system_handle = kan_context_query (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME);
    KAN_TEST_ASSERT (universe_system_handle != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)

    kan_universe_t universe = kan_universe_system_get_universe (universe_system_handle);
    KAN_TEST_ASSERT (universe != KAN_INVALID_UNIVERSE)

    kan_context_system_handle_t update_system = kan_context_query (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
    KAN_TEST_ASSERT (update_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    setup_indexing_stress_test_workspace (registry);

    struct kan_universe_world_definition_t definition;
    kan_universe_world_definition_init (&definition);
    definition.world_name = kan_string_intern ("root_world");
    definition.scheduler_name = kan_string_intern ("run_update");

    struct kan_resource_provider_configuration_t resource_provider_configuration = {
        .scan_budget_ns = 2000000u,
        .load_budget_ns = 2000000u,
        .add_wait_time_ns = 100000000u,
        .modify_wait_time_ns = 100000000u,
        .use_load_only_string_registry = KAN_TRUE,
        .observe_file_system = KAN_TRUE,
        .resource_directory_path = kan_string_intern (WORKSPACE_MOUNT_PATH),
    };

    kan_reflection_patch_builder_t patch_builder = kan_reflection_patch_builder_create ();
    kan_reflection_patch_builder_add_chunk (patch_builder, 0u, sizeof (struct kan_resource_provider_configuration_t),
                                            &resource_provider_configuration);
    kan_reflection_patch_t resource_provider_configuration_patch = kan_reflection_patch_builder_build (
        patch_builder, registry,
        kan_reflection_registry_query_struct (registry, kan_string_intern ("kan_resource_provider_configuration_t")));
    kan_reflection_patch_builder_destroy (patch_builder);

    kan_dynamic_array_set_capacity (&definition.configuration, 1u);
    *(struct kan_universe_world_configuration_t *) kan_dynamic_array_add_last (&definition.configuration) =
        (struct kan_universe_world_configuration_t) {
            .name = kan_string_intern (KAN_RESOURCE_PROVIDER_CONFIGURATION),
            .data = resource_provider_configuration_patch,
        };

    kan_dynamic_array_set_capacity (&definition.pipelines, 1u);
    struct kan_universe_world_pipeline_definition_t *update_pipeline =
        kan_dynamic_array_add_last (&definition.pipelines);

    kan_universe_world_pipeline_definition_init (update_pipeline);
    update_pipeline->name = kan_string_intern ("update");

    kan_dynamic_array_set_capacity (&update_pipeline->mutators, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutators) =
        kan_string_intern ("indexed_stress_test");

    kan_dynamic_array_set_capacity (&update_pipeline->mutator_groups, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutator_groups) =
        kan_string_intern (KAN_RESOURCE_PROVIDER_MUTATOR_GROUP);

    kan_universe_deploy_root (universe, &definition);
    kan_universe_world_definition_shutdown (&definition);

    const uint64_t time_begin = kan_platform_get_elapsed_nanoseconds ();
    while (!global_test_finished)
    {
        kan_update_system_run (update_system);
    }

    const uint64_t time_end = kan_platform_get_elapsed_nanoseconds ();
    printf ("Indexed stress test raw time: %lluns\n", (unsigned long long) (time_end - time_begin));
    kan_context_destroy (context);
}
