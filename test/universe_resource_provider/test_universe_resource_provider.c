#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <test_universe_resource_provider_api.h>

#include <stddef.h>
#include <stdio.h>

#include <kan/context/all_system_names.h>
#include <kan/context/hot_reload_coordination_system.h>
#include <kan/context/reflection_system.h>
#include <kan/context/universe_system.h>
#include <kan/context/update_system.h>
#include <kan/context/virtual_file_system.h>
#include <kan/file_system/entry.h>
#include <kan/file_system/stream.h>
#include <kan/precise_time/precise_time.h>
#include <kan/reflection/generated_reflection.h>
#include <kan/resource_pipeline/build.h>
#include <kan/resource_pipeline/common_meta.h>
#include <kan/resource_pipeline/platform_configuration.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/testing/testing.h>
#include <kan/universe/macro.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

#define WORKSPACE_DIRECTORY "workspace"
#define RAW_DIRECTORY "raw"
#define PLATFORM_CONFIGURATION_DIRECTORY "platform_configuration"
#define RESOURCE_MOUNT_PATH "resources"

KAN_LOG_DEFINE_CATEGORY (test_universe_resource_provider);
KAN_USE_STATIC_INTERNED_IDS
static bool global_test_finished = false;

struct first_resource_type_t
{
    uint64_t some_integer;
    bool flag_1;
    bool flag_2;
    bool flag_3;
    bool flag_4;
};

KAN_REFLECTION_STRUCT_META (first_resource_type_t)
TEST_UNIVERSE_RESOURCE_PROVIDER_API struct kan_resource_type_meta_t first_resource_type_meta = {
    .flags = KAN_RESOURCE_TYPE_ROOT,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

struct second_resource_type_t
{
    kan_interned_string_t first_id;
    kan_interned_string_t second_id;
};

KAN_REFLECTION_STRUCT_META (second_resource_type_t)
TEST_UNIVERSE_RESOURCE_PROVIDER_API struct kan_resource_type_meta_t second_resource_type_meta = {
    .flags = KAN_RESOURCE_TYPE_ROOT,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

static struct first_resource_type_t resource_alpha = {
    64u, true, false, true, false,
};

static struct first_resource_type_t resource_beta = {
    129u, true, true, true, true,
};

static struct second_resource_type_t resource_players;
static struct second_resource_type_t resource_characters;

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

struct run_update_scheduler_state_t
{
    kan_instance_size_t stub;
};

TEST_UNIVERSE_RESOURCE_PROVIDER_API KAN_UM_SCHEDULER_EXECUTE (run_update)
{
    kan_universe_scheduler_interface_run_pipeline (interface, KAN_STATIC_INTERNED_ID_GET (update));
}

static void save_rd (const char *path, void *instance, kan_interned_string_t type, kan_reflection_registry_t registry)
{
    struct kan_stream_t *stream = kan_direct_file_stream_open_for_write (path, true);
    KAN_TEST_ASSERT (stream)
    KAN_TEST_ASSERT (kan_serialization_rd_write_type_header (stream, type))
    CUSHION_DEFER { stream->operations->close (stream); }

    kan_serialization_rd_writer_t writer = kan_serialization_rd_writer_create (stream, instance, type, registry);
    CUSHION_DEFER { kan_serialization_rd_writer_destroy (writer); }
    enum kan_serialization_state_t state;

    while ((state = kan_serialization_rd_writer_step (writer)) == KAN_SERIALIZATION_IN_PROGRESS)
    {
    }

    KAN_TEST_ASSERT (state == KAN_SERIALIZATION_FINISHED);
}

static void initialize_platform_configuration (kan_reflection_registry_t registry)
{
    kan_file_system_remove_directory_with_content (PLATFORM_CONFIGURATION_DIRECTORY);
    KAN_TEST_CHECK (kan_file_system_make_directory (PLATFORM_CONFIGURATION_DIRECTORY))

    struct kan_file_system_path_container_t path;
    kan_file_system_path_container_copy_string (&path, PLATFORM_CONFIGURATION_DIRECTORY);
    kan_file_system_path_container_append (&path, KAN_RESOURCE_PLATFORM_CONFIGURATION_SETUP_FILE);

    struct kan_resource_platform_configuration_setup_t setup;
    kan_resource_platform_configuration_setup_init (&setup);
    CUSHION_DEFER { kan_resource_platform_configuration_setup_shutdown (&setup); }

    kan_dynamic_array_set_capacity (&setup.layers, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&setup.layers) = KAN_STATIC_INTERNED_ID_GET (base);
    save_rd (path.path, &setup, KAN_STATIC_INTERNED_ID_GET (kan_resource_platform_configuration_setup_t), registry);
}

static void setup_trivial_raw_resources (kan_reflection_registry_t registry)
{
    initialize_resources ();
    kan_file_system_make_directory (RAW_DIRECTORY);

    kan_file_system_make_directory (RAW_DIRECTORY "/bulk");
    save_rd (RAW_DIRECTORY "/bulk/alpha.rd", &resource_alpha, kan_string_intern ("first_resource_type_t"), registry);
    save_rd (RAW_DIRECTORY "/bulk/beta.rd", &resource_beta, kan_string_intern ("first_resource_type_t"), registry);

    kan_file_system_make_directory (RAW_DIRECTORY "/config");
    kan_file_system_make_directory (RAW_DIRECTORY "/config/common");
    save_rd (RAW_DIRECTORY "/config/common/players.rd", &resource_players, kan_string_intern ("second_resource_type_t"),
             registry);

    kan_file_system_make_directory (RAW_DIRECTORY "/config/map_1");
    save_rd (RAW_DIRECTORY "/config/map_1/characters.rd", &resource_characters,
             kan_string_intern ("second_resource_type_t"), registry);
}

#define TEST_TARGET_NAME "test_target"

KAN_REFLECTION_IGNORE
enum setup_context_flags_t
{
    SETUP_CONTEXT_WITH_HOT_RELOAD = 1u << 0u,
    SETUP_CONTEXT_MOUNT_DEPLOY = 1u << 1u,
};

static kan_context_t setup_context (enum setup_context_flags_t flags)
{
    kan_context_t context =
        kan_context_create (kan_allocation_group_get_child (kan_allocation_group_root (), "context"));

    struct kan_hot_reload_coordination_system_config_t hot_reload_config;
    kan_hot_reload_coordination_system_config_init (&hot_reload_config);

    if (flags & SETUP_CONTEXT_WITH_HOT_RELOAD)
    {
        KAN_TEST_CHECK (
            kan_context_request_system (context, KAN_CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_NAME, &hot_reload_config))
    }

    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME, NULL))

    struct kan_virtual_file_system_config_t virtual_file_system_config;
    kan_virtual_file_system_config_init (&virtual_file_system_config);
    CUSHION_DEFER { kan_virtual_file_system_config_shutdown (&virtual_file_system_config); }

    struct kan_file_system_path_container_t path_container;
    kan_file_system_path_container_copy_string (&path_container, WORKSPACE_DIRECTORY);

    if (flags & SETUP_CONTEXT_MOUNT_DEPLOY)
    {
        kan_dynamic_array_set_capacity (&virtual_file_system_config.mount_real, 1u);

        struct kan_virtual_file_system_config_mount_real_t *workspace =
            kan_dynamic_array_add_last (&virtual_file_system_config.mount_real);

        KAN_ASSERT (workspace)
        workspace->mount_path = kan_string_intern (RESOURCE_MOUNT_PATH);
        kan_file_system_path_container_append (&path_container, KAN_RESOURCE_PROJECT_WORKSPACE_DEPLOY_DIRECTORY);
        workspace->real_path = kan_string_intern (path_container.path);
    }

    KAN_TEST_CHECK (
        kan_context_request_system (context, KAN_CONTEXT_VIRTUAL_FILE_SYSTEM_NAME, &virtual_file_system_config))

    kan_context_assembly (context);
    return context;
}

static void execute_resource_build (kan_reflection_registry_t registry, enum kan_resource_build_pack_mode_t pack_mode)
{
    struct kan_resource_project_t project;
    kan_resource_project_init (&project);
    CUSHION_DEFER { kan_resource_project_shutdown (&project); }

    struct kan_resource_project_target_t *target = kan_dynamic_array_add_last (&project.targets);
    if (!target)
    {
        kan_dynamic_array_set_capacity (&project.targets, KAN_MAX (1u, project.targets.size * 2u));
        target = kan_dynamic_array_add_last (&project.targets);
    }

    kan_resource_project_target_init (target);
    target->name = kan_string_intern (TEST_TARGET_NAME);

    char *directory =
        kan_allocate_general (kan_resource_project_get_allocation_group (), sizeof (RAW_DIRECTORY), alignof (char));
    strcpy (directory, RAW_DIRECTORY);

    kan_dynamic_array_set_capacity (&target->directories, 1u);
    *(char **) kan_dynamic_array_add_last (&target->directories) = directory;

    project.workspace_directory = kan_allocate_general (kan_resource_project_get_allocation_group (),
                                                        sizeof (WORKSPACE_DIRECTORY), alignof (char));
    strcpy (project.workspace_directory, WORKSPACE_DIRECTORY);

    project.platform_configuration_directory = kan_allocate_general (
        kan_resource_project_get_allocation_group (), sizeof (PLATFORM_CONFIGURATION_DIRECTORY), alignof (char));
    strcpy (project.platform_configuration_directory, PLATFORM_CONFIGURATION_DIRECTORY);

    struct kan_resource_reflected_data_storage_t reflected_data;
    kan_resource_reflected_data_storage_build (&reflected_data, registry);
    CUSHION_DEFER { kan_resource_reflected_data_storage_shutdown (&reflected_data); }

    struct kan_resource_build_setup_t setup;
    kan_resource_build_setup_init (&setup);
    CUSHION_DEFER { kan_resource_build_setup_shutdown (&setup); };

    setup.project = &project;
    setup.reflected_data = &reflected_data;
    setup.pack_mode = pack_mode;
    setup.log_verbosity = KAN_LOG_VERBOSE;

    kan_dynamic_array_set_capacity (&setup.targets, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&setup.targets) = kan_string_intern (TEST_TARGET_NAME);

    const enum kan_resource_build_result_t result = kan_resource_build (&setup);
    KAN_TEST_ASSERT (result == KAN_RESOURCE_BUILD_RESULT_SUCCESS)
}

static void run_trivial_test (kan_context_t context)
{
    kan_context_system_t universe_system_handle = kan_context_query (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe_system_handle))

    kan_universe_t universe = kan_universe_system_get_universe (universe_system_handle);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe))

    kan_context_system_t update_system = kan_context_query (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (update_system))

    kan_context_system_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (reflection_system))
    kan_reflection_registry_t registry = kan_reflection_system_get_registry (reflection_system);

    struct kan_universe_world_definition_t definition;
    kan_universe_world_definition_init (&definition);
    definition.world_name = KAN_STATIC_INTERNED_ID_GET (root_world);
    definition.scheduler_name = KAN_STATIC_INTERNED_ID_GET (run_update);

    struct kan_resource_provider_configuration_t resource_provider_configuration = {
        .serve_budget_ns = 2000000u,
        .resource_directory_path = kan_string_intern (RESOURCE_MOUNT_PATH),
    };

    kan_reflection_patch_builder_t patch_builder = kan_reflection_patch_builder_create ();
    kan_reflection_patch_builder_add_chunk (patch_builder, KAN_REFLECTION_PATCH_BUILDER_SECTION_ROOT, 0u,
                                            sizeof (struct kan_resource_provider_configuration_t),
                                            &resource_provider_configuration);
    kan_reflection_patch_t resource_provider_configuration_patch = kan_reflection_patch_builder_build (
        patch_builder, registry,
        kan_reflection_registry_query_struct (registry,
                                              KAN_STATIC_INTERNED_ID_GET (kan_resource_provider_configuration_t)));
    kan_reflection_patch_builder_destroy (patch_builder);

    kan_dynamic_array_set_capacity (&definition.configuration, 1u);
    struct kan_universe_world_configuration_t *configuration = kan_dynamic_array_add_last (&definition.configuration);
    kan_universe_world_configuration_init (configuration);
    configuration->name = kan_string_intern (KAN_RESOURCE_PROVIDER_CONFIGURATION);
    kan_dynamic_array_set_capacity (&configuration->layers, 1u);

    struct kan_universe_world_configuration_layer_t *variant = kan_dynamic_array_add_last (&configuration->layers);
    kan_universe_world_configuration_layer_init (variant);
    variant->data = resource_provider_configuration_patch;

    kan_dynamic_array_set_capacity (&definition.pipelines, 1u);
    struct kan_universe_world_pipeline_definition_t *update_pipeline =
        kan_dynamic_array_add_last (&definition.pipelines);

    kan_universe_world_pipeline_definition_init (update_pipeline);
    update_pipeline->name = KAN_STATIC_INTERNED_ID_GET (update);

    kan_dynamic_array_set_capacity (&update_pipeline->mutators, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutators) =
        KAN_STATIC_INTERNED_ID_GET (trivial_test);

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

struct trivial_test_singleton_t
{
    bool registration_checked;
    bool usages_created;

    bool alpha_registered;
    bool alpha_loaded;

    bool beta_registered;
    bool beta_loaded;

    bool players_registered;
    bool players_loaded;

    bool characters_registered;
    bool characters_loaded;
};

TEST_UNIVERSE_RESOURCE_PROVIDER_API void trivial_test_singleton_init (struct trivial_test_singleton_t *instance)
{
    instance->registration_checked = false;
    instance->usages_created = false;

    instance->alpha_registered = false;
    instance->alpha_loaded = false;

    instance->beta_registered = false;
    instance->beta_loaded = false;

    instance->players_registered = false;
    instance->players_loaded = false;

    instance->characters_registered = false;
    instance->characters_loaded = false;
}

struct trivial_test_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (trivial_test_state)
    KAN_UM_BIND_STATE (trivial_test_state, state)
};

TEST_UNIVERSE_RESOURCE_PROVIDER_API KAN_UM_MUTATOR_DEPLOY (trivial_test)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
}

TEST_UNIVERSE_RESOURCE_PROVIDER_API KAN_UM_MUTATOR_EXECUTE (trivial_test)
{
    KAN_UMI_SINGLETON_WRITE (singleton, trivial_test_singleton_t)
    KAN_UMI_SINGLETON_READ (provider, kan_resource_provider_singleton_t)

    if (!provider->scan_done)
    {
        return;
    }

    if (!singleton->registration_checked)
    {
        KAN_UML_RESOURCE_REGISTERED_EVENT_FETCH (first_registered, first_resource_type_t)
        {
            if (first_registered->name == KAN_STATIC_INTERNED_ID_GET (alpha))
            {
                KAN_TEST_CHECK (!singleton->alpha_registered)
                singleton->alpha_registered = true;
            }
            else if (first_registered->name == KAN_STATIC_INTERNED_ID_GET (beta))
            {
                KAN_TEST_CHECK (!singleton->beta_registered)
                singleton->beta_registered = true;
            }
            else
            {
                KAN_TEST_CHECK (false)
            }
        }

        KAN_UML_RESOURCE_REGISTERED_EVENT_FETCH (second_registered, second_resource_type_t)
        {
            if (second_registered->name == KAN_STATIC_INTERNED_ID_GET (players))
            {
                KAN_TEST_CHECK (!singleton->players_registered)
                singleton->players_registered = true;
            }
            else if (second_registered->name == KAN_STATIC_INTERNED_ID_GET (characters))
            {
                KAN_TEST_CHECK (!singleton->characters_registered)
                singleton->characters_registered = true;
            }
            else
            {
                KAN_TEST_CHECK (false)
            }
        }

        KAN_TEST_CHECK (singleton->alpha_registered)
        KAN_TEST_CHECK (singleton->beta_registered)
        KAN_TEST_CHECK (singleton->characters_registered)
        KAN_TEST_CHECK (singleton->players_registered)
        singleton->registration_checked = true;
    }

    if (!singleton->usages_created)
    {
        KAN_UMO_INDEXED_INSERT (alpha_usage, kan_resource_usage_t)
        {
            alpha_usage->usage_id = kan_next_resource_usage_id (provider);
            alpha_usage->type = KAN_STATIC_INTERNED_ID_GET (first_resource_type_t);
            alpha_usage->name = KAN_STATIC_INTERNED_ID_GET (alpha);
            alpha_usage->priority = 0u;
        }

        KAN_UMO_INDEXED_INSERT (beta_usage, kan_resource_usage_t)
        {
            beta_usage->usage_id = kan_next_resource_usage_id (provider);
            beta_usage->type = KAN_STATIC_INTERNED_ID_GET (first_resource_type_t);
            beta_usage->name = KAN_STATIC_INTERNED_ID_GET (beta);
            beta_usage->priority = 0u;
        }

        KAN_UMO_INDEXED_INSERT (players_usage, kan_resource_usage_t)
        {
            players_usage->usage_id = kan_next_resource_usage_id (provider);
            players_usage->type = KAN_STATIC_INTERNED_ID_GET (second_resource_type_t);
            players_usage->name = KAN_STATIC_INTERNED_ID_GET (players);
            players_usage->priority = 0u;
        }

        KAN_UMO_INDEXED_INSERT (characters_usage, kan_resource_usage_t)
        {
            characters_usage->usage_id = kan_next_resource_usage_id (provider);
            characters_usage->type = KAN_STATIC_INTERNED_ID_GET (second_resource_type_t);
            characters_usage->name = KAN_STATIC_INTERNED_ID_GET (characters);
            characters_usage->priority = 0u;
        }

        singleton->usages_created = true;
    }

    KAN_UML_RESOURCE_LOADED_EVENT_FETCH (first_loaded, first_resource_type_t)
    {
        KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED (loaded, first_resource_type_t, &first_loaded->name)
        KAN_TEST_ASSERT (loaded)

        if (first_loaded->name == KAN_STATIC_INTERNED_ID_GET (alpha))
        {
            KAN_TEST_CHECK (!singleton->alpha_loaded)
            KAN_TEST_CHECK (loaded->some_integer == resource_alpha.some_integer)
            KAN_TEST_CHECK (loaded->flag_1 == resource_alpha.flag_1)
            KAN_TEST_CHECK (loaded->flag_2 == resource_alpha.flag_2)
            KAN_TEST_CHECK (loaded->flag_3 == resource_alpha.flag_3)
            KAN_TEST_CHECK (loaded->flag_4 == resource_alpha.flag_4)
            singleton->alpha_loaded = true;
        }
        else if (first_loaded->name == KAN_STATIC_INTERNED_ID_GET (beta))
        {
            KAN_TEST_CHECK (!singleton->beta_loaded)
            KAN_TEST_CHECK (loaded->some_integer == resource_beta.some_integer)
            KAN_TEST_CHECK (loaded->flag_1 == resource_beta.flag_1)
            KAN_TEST_CHECK (loaded->flag_2 == resource_beta.flag_2)
            KAN_TEST_CHECK (loaded->flag_3 == resource_beta.flag_3)
            KAN_TEST_CHECK (loaded->flag_4 == resource_beta.flag_4)
            singleton->beta_loaded = true;
        }
        else
        {
            KAN_TEST_CHECK (false)
        }
    }

    KAN_UML_RESOURCE_LOADED_EVENT_FETCH (second_loaded, second_resource_type_t)
    {
        KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED (loaded, second_resource_type_t, &second_loaded->name)
        KAN_TEST_ASSERT (loaded)

        if (second_loaded->name == KAN_STATIC_INTERNED_ID_GET (players))
        {
            KAN_TEST_CHECK (!singleton->players_loaded)
            KAN_TEST_CHECK (loaded->first_id == resource_players.first_id)
            KAN_TEST_CHECK (loaded->second_id == resource_players.second_id)
            singleton->players_loaded = true;
        }
        else if (second_loaded->name == KAN_STATIC_INTERNED_ID_GET (characters))
        {
            KAN_TEST_CHECK (!singleton->characters_loaded)
            KAN_TEST_CHECK (loaded->first_id == resource_characters.first_id)
            KAN_TEST_CHECK (loaded->second_id == resource_characters.second_id)
            singleton->characters_loaded = true;
        }
        else
        {
            KAN_TEST_CHECK (false)
        }
    }

    if (singleton->alpha_loaded && singleton->beta_loaded && singleton->characters_loaded && singleton->players_loaded)
    {
        global_test_finished = true;
    }
}

KAN_TEST_CASE (trivial)
{
    kan_static_interned_ids_ensure_initialized ();
    kan_file_system_remove_directory_with_content (WORKSPACE_DIRECTORY);
    kan_file_system_remove_directory_with_content (RAW_DIRECTORY);

    kan_context_t context = setup_context (SETUP_CONTEXT_MOUNT_DEPLOY);
    CUSHION_DEFER { kan_context_destroy (context); }

    kan_context_system_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (reflection_system))

    kan_reflection_registry_t registry = kan_reflection_system_get_registry (reflection_system);
    initialize_platform_configuration (registry);
    setup_trivial_raw_resources (registry);
    execute_resource_build (registry, KAN_RESOURCE_BUILD_PACK_MODE_NONE);
    run_trivial_test (context);
}

KAN_TEST_CASE (trivial_pack)
{
    kan_static_interned_ids_ensure_initialized ();
    kan_file_system_remove_directory_with_content (WORKSPACE_DIRECTORY);
    kan_file_system_remove_directory_with_content (RAW_DIRECTORY);

    kan_context_t context = setup_context (0u);
    CUSHION_DEFER { kan_context_destroy (context); }

    kan_context_system_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (reflection_system))

    kan_reflection_registry_t registry = kan_reflection_system_get_registry (reflection_system);
    initialize_platform_configuration (registry);
    setup_trivial_raw_resources (registry);
    execute_resource_build (registry, KAN_RESOURCE_BUILD_PACK_MODE_INTERNED);

    // And only now we have a pack that we can mount into VFS.
    kan_context_system_t virtual_file_system = kan_context_query (context, KAN_CONTEXT_VIRTUAL_FILE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (virtual_file_system))

    {
        kan_virtual_file_system_volume_t volume =
            kan_virtual_file_system_get_context_volume_for_write (virtual_file_system);
        CUSHION_DEFER { kan_virtual_file_system_close_context_write_access (virtual_file_system); }

        struct kan_file_system_path_container_t path_container;
        kan_file_system_path_container_copy_string (&path_container, WORKSPACE_DIRECTORY);
        kan_resource_build_append_pack_path_in_workspace (&path_container, TEST_TARGET_NAME);
        kan_virtual_file_system_volume_mount_read_only_pack (volume, RESOURCE_MOUNT_PATH, path_container.path);
    }

    run_trivial_test (context);
}

KAN_TEST_CASE (hot_reload)
{
    kan_static_interned_ids_ensure_initialized ();
    kan_file_system_remove_directory_with_content (WORKSPACE_DIRECTORY);
    kan_file_system_remove_directory_with_content (RAW_DIRECTORY);

    // TODO: Test.
}

// TODO: Do we want old stress test?
