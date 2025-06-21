#include <test_universe_api.h>

#include <stddef.h>

#include <kan/context/all_system_names.h>
#include <kan/context/reflection_system.h>
#include <kan/context/universe_system.h>
#include <kan/context/update_system.h>
#include <kan/reflection/generated_reflection.h>
#include <kan/testing/testing.h>
#include <kan/universe/macro.h>
#include <kan/universe/universe.h>

#define WORLD_CHILD_UPDATE_TEST_DEPTH 3u

struct counters_singleton_t
{
    kan_instance_size_t update_only_scheduler_executions;
    kan_instance_size_t update_only_mutator_executions;

    kan_instance_size_t double_update_scheduler_executions;
    kan_instance_size_t double_update_second_mutator_executions;

    kan_instance_size_t world_update_counters[WORLD_CHILD_UPDATE_TEST_DEPTH];
};

TEST_UNIVERSE_API void counters_singleton_init (struct counters_singleton_t *data)
{
    data->update_only_scheduler_executions = 0u;
    data->update_only_mutator_executions = 0u;
    data->double_update_scheduler_executions = 0u;
    data->double_update_second_mutator_executions = 0u;

    for (kan_loop_size_t index = 0u; index < WORLD_CHILD_UPDATE_TEST_DEPTH; ++index)
    {
        data->world_update_counters[index] = 0u;
    }
}

struct object_record_t
{
    kan_instance_size_t object_id;
    kan_instance_size_t parent_object_id;
    kan_instance_size_t data_x;
    kan_instance_size_t data_y;
};

#define INVALID_PARENT_OBJECT_ID KAN_INT_MAX (kan_instance_size_t)

TEST_UNIVERSE_API void object_record_init (struct object_record_t *data)
{
    data->object_id = 0u;
    data->parent_object_id = INVALID_PARENT_OBJECT_ID;
    data->data_x = 0u;
    data->data_y = 0u;
}

KAN_REFLECTION_STRUCT_META (object_record_t)
TEST_UNIVERSE_API struct kan_repository_meta_automatic_cascade_deletion_t object_record_hierarchy_cascade_deletion = {
    .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
    .child_type_name = "object_record_t",
    .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"parent_object_id"}},
};

struct status_record_t
{
    kan_instance_size_t object_id;
    kan_bool_t alive;
    kan_bool_t poisoned;
    kan_bool_t stunned;
    kan_bool_t boosted;
};

TEST_UNIVERSE_API void status_record_init (struct status_record_t *data)
{
    data->object_id = 0u;
    data->alive = KAN_FALSE;
    data->poisoned = KAN_FALSE;
    data->stunned = KAN_FALSE;
    data->boosted = KAN_FALSE;
}

KAN_REFLECTION_STRUCT_META (object_record_t)
TEST_UNIVERSE_API struct kan_repository_meta_automatic_cascade_deletion_t status_record_object_cascade_deletion = {
    .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
    .child_type_name = "status_record_t",
    .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
};

struct manual_event_t
{
    kan_instance_size_t some_data;
};

TEST_UNIVERSE_API void manual_event_init (struct manual_event_t *data) { data->some_data = 0u; }

struct world_configuration_counter_index_t
{
    kan_instance_size_t index;
};

struct run_only_update_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (run_only_update)
    KAN_UM_BIND_STATE (run_only_update, state)
};

TEST_UNIVERSE_API void kan_universe_scheduler_execute_run_only_update (kan_universe_scheduler_interface_t interface,
                                                                       struct run_only_update_state_t *state)
{
    {
        KAN_UMI_SINGLETON_WRITE (counters, counters_singleton_t)
        KAN_TEST_CHECK (counters->update_only_scheduler_executions == counters->update_only_mutator_executions)
        ++counters->update_only_scheduler_executions;
    }

    // We need to close all accesses before running pipelines.
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern ("update"));

    {
        KAN_UMI_SINGLETON_WRITE (counters, counters_singleton_t)
        KAN_TEST_CHECK (counters->update_only_scheduler_executions == counters->update_only_mutator_executions)
    }
}

struct update_only_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (update_only)
    KAN_UM_BIND_STATE (update_only, state)
};

TEST_UNIVERSE_API void kan_universe_mutator_execute_update_only (kan_cpu_job_t job, struct update_only_state_t *state)
{
    KAN_UM_MUTATOR_RELEASE_JOB_ON_RETURN
    KAN_UMI_SINGLETON_WRITE (counters, counters_singleton_t)
    ++counters->update_only_mutator_executions;
}

struct double_update_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (double_update)
    KAN_UM_BIND_STATE (double_update, state)
};

TEST_UNIVERSE_API void kan_universe_scheduler_execute_double_update (kan_universe_scheduler_interface_t interface,
                                                                     struct double_update_state_t *state)
{
    {
        KAN_UMI_SINGLETON_WRITE (counters, counters_singleton_t)
        ++counters->double_update_scheduler_executions;
    }

    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern ("update"));

    KAN_UML_EVENT_FETCH (event, manual_event_t)
    {
        kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern ("second_update"));
    }

    {
        KAN_UMI_SINGLETON_WRITE (counters, counters_singleton_t)
        KAN_TEST_CHECK (counters->double_update_scheduler_executions ==
                        counters->double_update_second_mutator_executions + 1u)
    }
}

struct second_update_stub_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (second_update_stub)
    KAN_UM_BIND_STATE (second_update_stub, state)
};

TEST_UNIVERSE_API void kan_universe_mutator_execute_second_update_stub (kan_cpu_job_t job,
                                                                        struct second_update_stub_state_t *state)
{
    KAN_UM_MUTATOR_RELEASE_JOB_ON_RETURN
    KAN_UMI_SINGLETON_WRITE (counters, counters_singleton_t)
    ++counters->double_update_second_mutator_executions;
}

struct spawn_objects_if_not_exist_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (spawn_objects_if_not_exist)
    KAN_UM_BIND_STATE (spawn_objects_if_not_exist, state)
};

TEST_UNIVERSE_API void kan_universe_mutator_execute_spawn_objects_if_not_exist (
    kan_cpu_job_t job, struct spawn_objects_if_not_exist_state_t *state)
{
    KAN_UM_MUTATOR_RELEASE_JOB_ON_RETURN
    kan_bool_t should_insert = KAN_TRUE;

    KAN_UML_SEQUENCE_READ (should_insert_check, status_record_t) { should_insert = KAN_FALSE; }

    if (should_insert)
    {
        {
            KAN_UMO_INDEXED_INSERT (object, object_record_t)
            {
                object->object_id = 1u;
                object->parent_object_id = INVALID_PARENT_OBJECT_ID;
                object->data_x = 1u;
                object->data_y = 2u;
            }
        }

        {
            KAN_UMO_INDEXED_INSERT (object, object_record_t)
            {
                object->object_id = 2u;
                object->parent_object_id = INVALID_PARENT_OBJECT_ID;
                object->data_x = 1u;
                object->data_y = 2u;
            }

            KAN_UMO_INDEXED_INSERT (status, status_record_t)
            {
                status->object_id = 2u;
                status->alive = KAN_TRUE;
                status->poisoned = KAN_FALSE;
                status->stunned = KAN_FALSE;
                status->boosted = KAN_FALSE;
            }
        }
    }
}

struct delete_objects_and_fire_event_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (delete_objects_and_fire_event)
    KAN_UM_BIND_STATE (delete_objects_and_fire_event, state)
};

TEST_UNIVERSE_API void kan_universe_mutator_deploy_delete_objects_and_fire_event (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct delete_objects_and_fire_event_state_t *state)
{
    // Intentionally execute delete BEFORE insert.
    kan_workflow_graph_node_make_dependency_of (workflow_node, "spawn_objects_if_not_exist");
}

TEST_UNIVERSE_API void kan_universe_mutator_execute_delete_objects_and_fire_event (
    kan_cpu_job_t job, struct delete_objects_and_fire_event_state_t *state)
{
    KAN_UM_MUTATOR_RELEASE_JOB_ON_RETURN
    kan_instance_size_t deleted_count = 0u;

    KAN_UML_SEQUENCE_DELETE (object, object_record_t)
    {
        KAN_UM_ACCESS_DELETE (object);
        ++deleted_count;
    }

    if (deleted_count > 0u)
    {
        KAN_UMO_EVENT_INSERT (event, manual_event_t) { event->some_data = 1u; }
    }
}

struct insert_from_multiple_threads_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (insert_from_multiple_threads)
    KAN_UM_BIND_STATE (insert_from_multiple_threads, state)

    KAN_REFLECTION_IGNORE
    struct kan_stack_group_allocator_t task_data_allocator;

    kan_cpu_section_t task_section;
};

TEST_UNIVERSE_API void kan_universe_mutator_deploy_insert_from_multiple_threads (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct insert_from_multiple_threads_state_t *state)
{
    kan_stack_group_allocator_init (&state->task_data_allocator, KAN_ALLOCATION_GROUP_IGNORE, 1024u);
    state->task_section = kan_cpu_section_get ("insert_task");
    kan_workflow_graph_node_make_dependency_of (workflow_node, "validate_insert_from_multiple_threads");
}

struct insert_task_user_data_t
{
    struct insert_from_multiple_threads_state_t *state;
    kan_instance_size_t index;
};

static void insert_task_execute (kan_functor_user_data_t user_data)
{
    struct insert_task_user_data_t *data = (struct insert_task_user_data_t *) user_data;
    struct insert_from_multiple_threads_state_t *state = data->state;

    KAN_UMO_INDEXED_INSERT (object, object_record_t)
    {
        object->object_id = data->index;
        object->parent_object_id = INVALID_PARENT_OBJECT_ID;
        object->data_x = data->index + 3u;
        object->data_y = data->index * 2u;
    }
}

KAN_REFLECTION_FUNCTION_META (kan_universe_mutator_execute_insert_from_multiple_threads)
KAN_REFLECTION_FUNCTION_META (kan_universe_mutator_execute_validate_insert_from_multiple_threads)
TEST_UNIVERSE_API struct kan_universe_mutator_group_meta_t multiple_threads_test_group = {
    .group_name = "multiple_threads_test",
};

TEST_UNIVERSE_API void kan_universe_mutator_execute_insert_from_multiple_threads (
    kan_cpu_job_t job, struct insert_from_multiple_threads_state_t *state)
{
    KAN_UM_MUTATOR_RELEASE_JOB_ON_RETURN
    kan_stack_group_allocator_reset (&state->task_data_allocator);
    struct kan_cpu_task_list_node_t *tasks_head = NULL;

    for (kan_loop_size_t index = 0u; index < 16u; ++index)
    {
        KAN_CPU_TASK_LIST_USER_STRUCT (&tasks_head, &state->task_data_allocator, insert_task_execute,
                                       state->task_section, struct insert_task_user_data_t,
                                       {
                                           .state = state,
                                           .index = index,
                                       });
    }

    kan_cpu_job_dispatch_and_detach_task_list (job, tasks_head);
}

TEST_UNIVERSE_API void kan_universe_mutator_undeploy_insert_from_multiple_threads (
    struct insert_from_multiple_threads_state_t *state)
{
    kan_stack_group_allocator_shutdown (&state->task_data_allocator);
}

struct validate_insert_from_multiple_threads_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (validate_insert_from_multiple_threads)
    KAN_UM_BIND_STATE (validate_insert_from_multiple_threads, state)
};

TEST_UNIVERSE_API void kan_universe_mutator_execute_validate_insert_from_multiple_threads (
    kan_cpu_job_t job, struct validate_insert_from_multiple_threads_state_t *state)
{
    KAN_UM_MUTATOR_RELEASE_JOB_ON_RETURN
    kan_instance_size_t count = 0u;

    KAN_UML_INTERVAL_ASCENDING_READ (object, object_record_t, object_id, NULL, NULL)
    {
        KAN_TEST_CHECK (object->object_id == count)
        KAN_TEST_CHECK (object->parent_object_id == INVALID_PARENT_OBJECT_ID)
        KAN_TEST_CHECK (object->data_x == count + 3u)
        KAN_TEST_CHECK (object->data_y == count * 2u)
        ++count;
    }

    KAN_TEST_CHECK (count == 16u)
}

struct update_with_children_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (update_with_children)
    KAN_UM_BIND_STATE (update_with_children, state)
};

TEST_UNIVERSE_API void kan_universe_scheduler_execute_update_with_children (
    kan_universe_scheduler_interface_t interface, struct update_with_children_state_t *state)
{
    kan_universe_scheduler_interface_run_pipeline (interface, kan_string_intern ("update"));
    kan_universe_scheduler_interface_update_all_children (interface);

    KAN_UMI_SINGLETON_READ (counters, counters_singleton_t)
    for (kan_loop_size_t index = 1u; index < WORLD_CHILD_UPDATE_TEST_DEPTH; ++index)
    {
        KAN_TEST_CHECK (counters->world_update_counters[index - 1u] == counters->world_update_counters[index])
    }
}

struct world_update_counter_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (world_update_counter)
    KAN_UM_BIND_STATE (world_update_counter, state)

    kan_instance_size_t world_counter_index;
};

TEST_UNIVERSE_API void kan_universe_mutator_deploy_world_update_counter (kan_universe_t universe,
                                                                         kan_universe_world_t world,
                                                                         kan_repository_t world_repository,
                                                                         kan_workflow_graph_node_t workflow_node,
                                                                         struct world_update_counter_state_t *state)
{
    const struct world_configuration_counter_index_t *configuration =
        kan_universe_world_query_configuration (world, kan_string_intern ("counter"));
    KAN_TEST_ASSERT (configuration)
    state->world_counter_index = configuration->index;
}

TEST_UNIVERSE_API void kan_universe_mutator_execute_world_update_counter (kan_cpu_job_t job,
                                                                          struct world_update_counter_state_t *state)
{
    KAN_UM_MUTATOR_RELEASE_JOB_ON_RETURN
    KAN_UMI_SINGLETON_WRITE (counters, counters_singleton_t)
    ++counters->world_update_counters[state->world_counter_index];
}

KAN_REFLECTION_EXPECT_UNIT_REGISTRAR (test_universe_pre_migration);
KAN_REFLECTION_EXPECT_UNIT_REGISTRAR (test_universe_post_migration);

struct migration_reflection_population_system_t
{
    kan_context_t context;
    kan_allocation_group_t group;
    kan_bool_t select_post;
};

kan_context_system_t migration_reflection_population_system_create (kan_allocation_group_t group, void *user_config)
{
    struct migration_reflection_population_system_t *system =
        kan_allocate_general (group, sizeof (struct migration_reflection_population_system_t),
                              _Alignof (struct migration_reflection_population_system_t));
    system->group = group;
    system->select_post = KAN_FALSE;
    return KAN_HANDLE_SET (kan_context_system_t, system);
}

static void migration_populate_reflection (kan_context_system_t handle, kan_reflection_registry_t registry)
{
    struct migration_reflection_population_system_t *system = KAN_HANDLE_GET (handle);
    if (system->select_post)
    {
        KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_universe_post_migration) (registry);
    }
    else
    {
        KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_universe_pre_migration) (registry);
        system->select_post = KAN_TRUE;
    }
}

void migration_reflection_population_system_connect (kan_context_system_t handle, kan_context_t context)
{
    struct migration_reflection_population_system_t *system = KAN_HANDLE_GET (handle);
    system->context = context;

    kan_context_system_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    if (KAN_HANDLE_IS_VALID (reflection_system))
    {
        kan_reflection_system_connect_on_populate (reflection_system, handle, migration_populate_reflection);
    }
}

void migration_reflection_population_system_init (kan_context_system_t handle) {}

void migration_reflection_population_system_shutdown (kan_context_system_t handle) {}

void migration_reflection_population_system_disconnect (kan_context_system_t handle)
{
    struct migration_reflection_population_system_t *system = KAN_HANDLE_GET (handle);
    kan_context_system_t reflection_system = kan_context_query (system->context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (reflection_system))
    {
        kan_reflection_system_disconnect_on_populate (reflection_system, handle);
    }
}

void migration_reflection_population_system_destroy (kan_context_system_t handle)
{
    struct migration_reflection_population_system_t *system = KAN_HANDLE_GET (handle);
    kan_free_general (system->group, system, sizeof (struct migration_reflection_population_system_t));
}

TEST_UNIVERSE_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (
    migration_reflection_population_system_t) = {
    .name = "migration_reflection_population_system_t",
    .create = migration_reflection_population_system_create,
    .connect = migration_reflection_population_system_connect,
    .connected_init = migration_reflection_population_system_init,
    .connected_shutdown = migration_reflection_population_system_shutdown,
    .disconnect = migration_reflection_population_system_disconnect,
    .destroy = migration_reflection_population_system_destroy,
};

KAN_TEST_CASE (update_only)
{
    kan_context_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME, NULL))
    kan_context_assembly (context);

    kan_context_system_t universe_system_handle = kan_context_query (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe_system_handle))

    kan_universe_t universe = kan_universe_system_get_universe (universe_system_handle);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe))

    kan_context_system_t update_system = kan_context_query (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (update_system))

    struct kan_universe_world_definition_t definition;
    kan_universe_world_definition_init (&definition);
    definition.world_name = kan_string_intern ("root_world");
    definition.scheduler_name = kan_string_intern ("run_only_update");

    kan_dynamic_array_set_capacity (&definition.pipelines, 1u);
    struct kan_universe_world_pipeline_definition_t *update_pipeline =
        kan_dynamic_array_add_last (&definition.pipelines);

    kan_universe_world_pipeline_definition_init (update_pipeline);
    update_pipeline->name = kan_string_intern ("update");

    kan_dynamic_array_set_capacity (&update_pipeline->mutators, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutators) =
        kan_string_intern ("update_only");

    kan_universe_deploy_root (universe, &definition);
    kan_universe_world_definition_shutdown (&definition);

    kan_update_system_run (update_system);
    kan_update_system_run (update_system);
    kan_update_system_run (update_system);
    kan_context_destroy (context);
}

KAN_TEST_CASE (update_several_pipelines)
{
    kan_context_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME, NULL))
    kan_context_assembly (context);

    kan_context_system_t universe_system_handle = kan_context_query (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe_system_handle))

    kan_universe_t universe = kan_universe_system_get_universe (universe_system_handle);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe))

    kan_context_system_t update_system = kan_context_query (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (update_system))

    struct kan_universe_world_definition_t definition;
    kan_universe_world_definition_init (&definition);
    definition.world_name = kan_string_intern ("root_world");
    definition.scheduler_name = kan_string_intern ("double_update");

    kan_dynamic_array_set_capacity (&definition.pipelines, 2u);
    struct kan_universe_world_pipeline_definition_t *update_pipeline =
        kan_dynamic_array_add_last (&definition.pipelines);

    kan_universe_world_pipeline_definition_init (update_pipeline);
    update_pipeline->name = kan_string_intern ("update");

    kan_dynamic_array_set_capacity (&update_pipeline->mutators, 2u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutators) =
        kan_string_intern ("spawn_objects_if_not_exist");
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutators) =
        kan_string_intern ("delete_objects_and_fire_event");

    struct kan_universe_world_pipeline_definition_t *second_update_pipeline =
        kan_dynamic_array_add_last (&definition.pipelines);

    kan_universe_world_pipeline_definition_init (second_update_pipeline);
    second_update_pipeline->name = kan_string_intern ("second_update");

    kan_dynamic_array_set_capacity (&second_update_pipeline->mutators, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&second_update_pipeline->mutators) =
        kan_string_intern ("second_update_stub");

    kan_universe_deploy_root (universe, &definition);
    kan_universe_world_definition_shutdown (&definition);

    kan_update_system_run (update_system);
    kan_update_system_run (update_system);
    kan_update_system_run (update_system);
    kan_context_destroy (context);
}

KAN_TEST_CASE (update_mutator_multiple_threads)
{
    kan_context_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME, NULL))
    kan_context_assembly (context);

    kan_context_system_t universe_system_handle = kan_context_query (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe_system_handle))

    kan_universe_t universe = kan_universe_system_get_universe (universe_system_handle);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe))

    kan_context_system_t update_system = kan_context_query (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (update_system))

    struct kan_universe_world_definition_t definition;
    kan_universe_world_definition_init (&definition);
    definition.world_name = kan_string_intern ("root_world");
    definition.scheduler_name = kan_string_intern ("run_only_update");

    kan_dynamic_array_set_capacity (&definition.pipelines, 1u);
    struct kan_universe_world_pipeline_definition_t *update_pipeline =
        kan_dynamic_array_add_last (&definition.pipelines);

    kan_universe_world_pipeline_definition_init (update_pipeline);
    update_pipeline->name = kan_string_intern ("update");

    kan_dynamic_array_set_capacity (&update_pipeline->mutators, 1u);
    kan_dynamic_array_set_capacity (&update_pipeline->mutator_groups, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutators) =
        kan_string_intern ("update_only");
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutator_groups) =
        kan_string_intern ("multiple_threads_test");

    kan_universe_deploy_root (universe, &definition);
    kan_universe_world_definition_shutdown (&definition);

    kan_update_system_run (update_system);
    kan_context_destroy (context);
}

KAN_TEST_CASE (update_hierarchy)
{
    kan_context_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME, NULL))
    kan_context_assembly (context);

    kan_context_system_t universe_system_handle = kan_context_query (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe_system_handle))

    kan_universe_t universe = kan_universe_system_get_universe (universe_system_handle);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe))

    kan_context_system_t update_system = kan_context_query (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (update_system))

    kan_context_system_t reflection_system_handle = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (reflection_system_handle))

    kan_reflection_registry_t registry = kan_reflection_system_get_registry (reflection_system_handle);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (registry))

    kan_reflection_patch_builder_t patch_builder = kan_reflection_patch_builder_create ();
    const struct kan_reflection_struct_t *config_type =
        kan_reflection_registry_query_struct (registry, kan_string_intern ("world_configuration_counter_index_t"));
    KAN_ASSERT (config_type != NULL)

    const kan_instance_size_t _0u = 0u;
    const kan_instance_size_t _1u = 1u;
    const kan_instance_size_t _2u = 2u;

    struct kan_universe_world_definition_t definition;
    kan_universe_world_definition_init (&definition);
    definition.world_name = kan_string_intern ("root_world");
    definition.scheduler_name = kan_string_intern ("update_with_children");

#define BUILD_WORLD_DEFINITION(DEFINITION, INDEX)                                                                      \
    {                                                                                                                  \
        (DEFINITION)->scheduler_name = kan_string_intern ("update_with_children");                                     \
                                                                                                                       \
        kan_dynamic_array_set_capacity (&(DEFINITION)->configuration, 1u);                                             \
        struct kan_universe_world_configuration_t *configuration =                                                     \
            kan_dynamic_array_add_last (&(DEFINITION)->configuration);                                                 \
        kan_universe_world_configuration_init (configuration);                                                         \
        configuration->name = kan_string_intern ("counter");                                                           \
                                                                                                                       \
        kan_dynamic_array_set_capacity (&configuration->layers, 1u);                                                   \
        struct kan_universe_world_configuration_layer_t *variant =                                                     \
            kan_dynamic_array_add_last (&configuration->layers);                                                       \
        kan_universe_world_configuration_layer_init (variant);                                                         \
                                                                                                                       \
        kan_reflection_patch_builder_add_chunk (patch_builder, KAN_REFLECTION_PATCH_BUILDER_SECTION_ROOT, 0u,          \
                                                sizeof (kan_instance_size_t), INDEX);                                  \
        variant->data = kan_reflection_patch_builder_build (patch_builder, registry, config_type);                     \
                                                                                                                       \
        kan_dynamic_array_set_capacity (&(DEFINITION)->pipelines, 1u);                                                 \
        struct kan_universe_world_pipeline_definition_t *update_pipeline =                                             \
            kan_dynamic_array_add_last (&(DEFINITION)->pipelines);                                                     \
                                                                                                                       \
        kan_universe_world_pipeline_definition_init (update_pipeline);                                                 \
        update_pipeline->name = kan_string_intern ("update");                                                          \
                                                                                                                       \
        kan_dynamic_array_set_capacity (&update_pipeline->mutators, 1u);                                               \
        *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutators) =                           \
            kan_string_intern ("world_update_counter");                                                                \
    }

    BUILD_WORLD_DEFINITION (&definition, &_0u)

    kan_dynamic_array_set_capacity (&definition.children, 1u);
    struct kan_universe_world_definition_t *child_level_1_definition =
        kan_dynamic_array_add_last (&definition.children);

    kan_universe_world_definition_init (child_level_1_definition);
    child_level_1_definition->world_name = kan_string_intern ("world_level_1");
    BUILD_WORLD_DEFINITION (child_level_1_definition, &_1u)

    kan_dynamic_array_set_capacity (&child_level_1_definition->children, 1u);
    struct kan_universe_world_definition_t *child_level_2_definition =
        kan_dynamic_array_add_last (&child_level_1_definition->children);

    kan_universe_world_definition_init (child_level_2_definition);
    child_level_2_definition->world_name = kan_string_intern ("world_level_2");
    BUILD_WORLD_DEFINITION (child_level_2_definition, &_2u)

#undef BUILD_WORLD_DEFINITION

    kan_universe_deploy_root (universe, &definition);
    kan_universe_world_definition_shutdown (&definition);
    kan_reflection_patch_builder_destroy (patch_builder);

    kan_update_system_run (update_system);
    kan_update_system_run (update_system);
    kan_update_system_run (update_system);
    kan_context_destroy (context);
}

KAN_TEST_CASE (migration)
{
    kan_context_t context = kan_context_create (KAN_ALLOCATION_GROUP_IGNORE);
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (kan_context_request_system (context, "migration_reflection_population_system_t", NULL))
    kan_context_assembly (context);

    kan_context_system_t universe_system_handle = kan_context_query (context, KAN_CONTEXT_UNIVERSE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe_system_handle))

    kan_universe_t universe = kan_universe_system_get_universe (universe_system_handle);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (universe))

    kan_context_system_t update_system = kan_context_query (context, KAN_CONTEXT_UPDATE_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (update_system))

    kan_context_system_t reflection_system_handle = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    KAN_TEST_ASSERT (KAN_HANDLE_IS_VALID (reflection_system_handle))

    struct kan_universe_world_definition_t definition;
    kan_universe_world_definition_init (&definition);
    definition.world_name = kan_string_intern ("root_world");
    definition.scheduler_name = kan_string_intern ("migration_scheduler");

    kan_dynamic_array_set_capacity (&definition.pipelines, 1u);
    struct kan_universe_world_pipeline_definition_t *update_pipeline =
        kan_dynamic_array_add_last (&definition.pipelines);

    kan_universe_world_pipeline_definition_init (update_pipeline);
    update_pipeline->name = kan_string_intern ("update");

    kan_dynamic_array_set_capacity (&update_pipeline->mutators, 1u);
    *(kan_interned_string_t *) kan_dynamic_array_add_last (&update_pipeline->mutators) =
        kan_string_intern ("migration_mutator");

    kan_universe_deploy_root (universe, &definition);
    kan_universe_world_definition_shutdown (&definition);

    kan_update_system_run (update_system);
    kan_update_system_run (update_system);
    kan_reflection_system_invalidate (reflection_system_handle);
    kan_update_system_run (update_system);
    kan_update_system_run (update_system);
    kan_context_destroy (context);
}
