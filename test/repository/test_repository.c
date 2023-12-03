#include <kan/container/stack_group_allocator.h>
#include <kan/cpu_dispatch/job.h>
#include <kan/reflection/generated_reflection.h>
#include <kan/repository/repository.h>
#include <kan/testing/testing.h>

#include "test_repository.h"

KAN_REFLECTION_EXPECT_UNIT_REGISTRAR (repository);
KAN_REFLECTION_EXPECT_UNIT_REGISTRAR (test_repository);

static void check_no_event (struct kan_repository_event_fetch_query_t *query)
{
    struct kan_repository_event_read_access_t access = kan_repository_event_fetch_query_next (query);
    struct manual_event_t *event = (struct manual_event_t *) kan_repository_event_read_access_resolve (&access);
    KAN_TEST_CHECK (!event)
}

static void insert_manual_event (struct kan_repository_event_insert_query_t *query, struct manual_event_t data)
{
    struct kan_repository_event_insertion_package_t insertion_package =
        kan_repository_event_insert_query_execute (query);
    struct manual_event_t *event =
        (struct manual_event_t *) kan_repository_event_insertion_package_get (&insertion_package);

    KAN_TEST_ASSERT (event)
    *event = data;
    kan_repository_event_insertion_package_submit (&insertion_package);
}

static void insert_manual_event_second (struct kan_repository_event_insert_query_t *query,
                                        struct manual_event_second_t data)
{
    struct kan_repository_event_insertion_package_t insertion_package =
        kan_repository_event_insert_query_execute (query);

    struct manual_event_second_t *event =
        (struct manual_event_second_t *) kan_repository_event_insertion_package_get (&insertion_package);

    KAN_TEST_ASSERT (event)
    *event = data;
    kan_repository_event_insertion_package_submit (&insertion_package);
}

static void insert_and_undo_event (struct kan_repository_event_insert_query_t *query)
{
    struct kan_repository_event_insertion_package_t insertion_package =
        kan_repository_event_insert_query_execute (query);
    void *event = kan_repository_event_insertion_package_get (&insertion_package);
    KAN_TEST_ASSERT (event)
    kan_repository_event_insertion_package_undo (&insertion_package);
}

static void check_manual_event (struct kan_repository_event_fetch_query_t *query, struct manual_event_t data)
{
    struct kan_repository_event_read_access_t access = kan_repository_event_fetch_query_next (query);
    struct manual_event_t *event = (struct manual_event_t *) kan_repository_event_read_access_resolve (&access);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->x == data.x)
    KAN_TEST_CHECK (event->y == data.y)
    kan_repository_event_read_access_close (&access);
}

static void check_manual_event_second (struct kan_repository_event_fetch_query_t *query,
                                       struct manual_event_second_t data)
{
    struct kan_repository_event_read_access_t access = kan_repository_event_fetch_query_next (query);
    struct manual_event_second_t *event =
        (struct manual_event_second_t *) kan_repository_event_read_access_resolve (&access);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->id == data.id)
    KAN_TEST_CHECK (event->a == data.a)
    KAN_TEST_CHECK (event->b == data.b)
    kan_repository_event_read_access_close (&access);
}

struct check_manual_event_from_task_user_data
{
    struct kan_repository_event_read_access_t access;
    struct manual_event_t data;
};

static void check_manual_event_from_task_executor (uint64_t user_data_int)
{
    struct check_manual_event_from_task_user_data *user_data =
        (struct check_manual_event_from_task_user_data *) user_data_int;

    struct manual_event_t *event =
        (struct manual_event_t *) kan_repository_event_read_access_resolve (&user_data->access);

    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->x == user_data->data.x)
    KAN_TEST_CHECK (event->y == user_data->data.y)
    kan_repository_event_read_access_close (&user_data->access);
}

static void check_manual_event_from_task (struct kan_repository_event_fetch_query_t *query,
                                          struct kan_cpu_task_list_node_t **node,
                                          struct kan_stack_group_allocator_t *temporary_allocator,
                                          struct manual_event_t data)
{
    struct kan_repository_event_read_access_t access = kan_repository_event_fetch_query_next (query);
    struct check_manual_event_from_task_user_data *user_data =
        kan_stack_group_allocator_allocate (temporary_allocator, sizeof (struct check_manual_event_from_task_user_data),
                                            _Alignof (struct check_manual_event_from_task_user_data));

    user_data->access = access;
    user_data->data = data;

    struct kan_cpu_task_list_node_t *task_node = kan_stack_group_allocator_allocate (
        temporary_allocator, sizeof (struct kan_cpu_task_list_node_t), _Alignof (struct kan_cpu_task_list_node_t));
    task_node->next = *node;
    *node = task_node;

    task_node->task = (struct kan_cpu_task_t) {
        .name = kan_string_intern ("check_manual_event_from_task"),
        .user_data = (uint64_t) user_data,
        .function = check_manual_event_from_task_executor,
    };

    task_node->queue = KAN_CPU_DISPATCH_QUEUE_FOREGROUND;
}

KAN_TEST_CASE (manual_event)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (repository) (registry);
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_repository) (registry);

    kan_repository_t repository = kan_repository_create_root (KAN_ALLOCATION_GROUP_IGNORE, registry);
    kan_repository_event_storage_t storage = kan_repository_event_storage_open (repository, "manual_event_t");

    struct kan_repository_event_insert_query_t event_insert;
    kan_repository_event_insert_query_init (&event_insert, storage);

    struct kan_repository_event_fetch_query_t event_fetch_first;
    kan_repository_event_fetch_query_init (&event_fetch_first, storage);

    struct kan_repository_event_fetch_query_t event_fetch_second;
    kan_repository_event_fetch_query_init (&event_fetch_second, storage);

    kan_repository_enter_serving_mode (repository);
    check_no_event (&event_fetch_first);
    check_no_event (&event_fetch_second);

    insert_manual_event (&event_insert, (struct manual_event_t) {.x = 42u, .y = 2u});
    check_manual_event (&event_fetch_first, (struct manual_event_t) {.x = 42u, .y = 2u});
    check_no_event (&event_fetch_first);

    insert_manual_event (&event_insert, (struct manual_event_t) {.x = 13u, .y = 19u});
    check_manual_event (&event_fetch_second, (struct manual_event_t) {.x = 42u, .y = 2u});
    check_manual_event (&event_fetch_first, (struct manual_event_t) {.x = 13u, .y = 19u});
    check_manual_event (&event_fetch_second, (struct manual_event_t) {.x = 13u, .y = 19u});
    check_no_event (&event_fetch_first);
    check_no_event (&event_fetch_second);

    insert_and_undo_event (&event_insert);
    check_no_event (&event_fetch_first);
    check_no_event (&event_fetch_second);

    kan_repository_enter_planning_mode (repository);
    kan_repository_event_insert_query_shutdown (&event_insert);
    kan_repository_event_fetch_query_shutdown (&event_fetch_first);
    kan_repository_event_fetch_query_shutdown (&event_fetch_second);

    kan_repository_destroy (repository);
    kan_reflection_registry_destroy (registry);
}

KAN_TEST_CASE (manual_event_no_insert_if_no_fetch)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (repository) (registry);
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_repository) (registry);

    kan_repository_t repository = kan_repository_create_root (KAN_ALLOCATION_GROUP_IGNORE, registry);
    kan_repository_event_storage_t storage = kan_repository_event_storage_open (repository, "manual_event_t");

    struct kan_repository_event_insert_query_t event_insert;
    kan_repository_event_insert_query_init (&event_insert, storage);

    kan_repository_enter_serving_mode (repository);

    {
        struct kan_repository_event_insertion_package_t insertion_package =
            kan_repository_event_insert_query_execute (&event_insert);
        struct manual_event_t *new_event = kan_repository_event_insertion_package_get (&insertion_package);
        KAN_TEST_CHECK (!new_event)
    }

    kan_repository_enter_planning_mode (repository);
    kan_repository_event_insert_query_shutdown (&event_insert);

    kan_repository_destroy (repository);
    kan_reflection_registry_destroy (registry);
}

KAN_TEST_CASE (manual_event_insert_from_child_to_root)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (repository) (registry);
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_repository) (registry);

    kan_repository_t root_repository = kan_repository_create_root (KAN_ALLOCATION_GROUP_IGNORE, registry);
    kan_repository_t child_repository = kan_repository_create_child (root_repository, "child_repository");

    kan_repository_event_storage_t root_storage = kan_repository_event_storage_open (root_repository, "manual_event_t");

    kan_repository_event_storage_t child_storage =
        kan_repository_event_storage_open (child_repository, "manual_event_t");

    struct kan_repository_event_fetch_query_t event_fetch;
    kan_repository_event_fetch_query_init (&event_fetch, root_storage);

    struct kan_repository_event_insert_query_t event_insert;
    kan_repository_event_insert_query_init (&event_insert, child_storage);

    kan_repository_enter_serving_mode (root_repository);
    check_no_event (&event_fetch);
    insert_manual_event (&event_insert, (struct manual_event_t) {.x = 42u, .y = 2u});
    check_manual_event (&event_fetch, (struct manual_event_t) {.x = 42u, .y = 2u});
    check_no_event (&event_fetch);

    kan_repository_enter_planning_mode (root_repository);
    kan_repository_event_fetch_query_shutdown (&event_fetch);
    kan_repository_event_insert_query_shutdown (&event_insert);

    kan_repository_destroy (root_repository);
    kan_reflection_registry_destroy (registry);
}

KAN_TEST_CASE (manual_event_two_types)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (repository) (registry);
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_repository) (registry);

    kan_repository_t repository = kan_repository_create_root (KAN_ALLOCATION_GROUP_IGNORE, registry);
    kan_repository_event_storage_t first_storage = kan_repository_event_storage_open (repository, "manual_event_t");
    kan_repository_event_storage_t second_storage =
        kan_repository_event_storage_open (repository, "manual_event_second_t");

    struct kan_repository_event_insert_query_t event_insert_first;
    kan_repository_event_insert_query_init (&event_insert_first, first_storage);

    struct kan_repository_event_insert_query_t event_insert_second;
    kan_repository_event_insert_query_init (&event_insert_second, second_storage);

    struct kan_repository_event_fetch_query_t event_fetch_first;
    kan_repository_event_fetch_query_init (&event_fetch_first, first_storage);

    struct kan_repository_event_fetch_query_t event_fetch_second;
    kan_repository_event_fetch_query_init (&event_fetch_second, second_storage);

    kan_repository_enter_serving_mode (repository);
    check_no_event (&event_fetch_first);
    check_no_event (&event_fetch_second);

    insert_manual_event (&event_insert_first, (struct manual_event_t) {.x = 42u, .y = 2u});
    insert_manual_event_second (&event_insert_second, (struct manual_event_second_t) {.id = 3u, .a = 1.0f, .b = 2.0f});

    check_manual_event (&event_fetch_first, (struct manual_event_t) {.x = 42u, .y = 2u});
    check_manual_event_second (&event_fetch_second, (struct manual_event_second_t) {.id = 3u, .a = 1.0f, .b = 2.0f});

    check_no_event (&event_fetch_first);
    check_no_event (&event_fetch_second);

    kan_repository_enter_planning_mode (repository);
    kan_repository_event_insert_query_shutdown (&event_insert_first);
    kan_repository_event_insert_query_shutdown (&event_insert_second);
    kan_repository_event_fetch_query_shutdown (&event_fetch_first);
    kan_repository_event_fetch_query_shutdown (&event_fetch_second);

    kan_repository_destroy (repository);
    kan_reflection_registry_destroy (registry);
}

KAN_TEST_CASE (manual_event_access_from_tasks)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (repository) (registry);
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_repository) (registry);

    kan_repository_t repository = kan_repository_create_root (KAN_ALLOCATION_GROUP_IGNORE, registry);
    kan_repository_event_storage_t storage = kan_repository_event_storage_open (repository, "manual_event_t");

    struct kan_repository_event_insert_query_t event_insert;
    kan_repository_event_insert_query_init (&event_insert, storage);

    struct kan_repository_event_fetch_query_t event_fetch_first;
    kan_repository_event_fetch_query_init (&event_fetch_first, storage);

    struct kan_repository_event_fetch_query_t event_fetch_second;
    kan_repository_event_fetch_query_init (&event_fetch_second, storage);

    kan_repository_enter_serving_mode (repository);
    insert_manual_event (&event_insert, (struct manual_event_t) {.x = 42u, .y = 2u});
    insert_manual_event (&event_insert, (struct manual_event_t) {.x = 13u, .y = 19u});
    insert_manual_event (&event_insert, (struct manual_event_t) {.x = 111u, .y = 222u});

    struct kan_stack_group_allocator_t temporary_allocator;
    kan_stack_group_allocator_init (&temporary_allocator, KAN_ALLOCATION_GROUP_IGNORE, 8196u);

    kan_cpu_job_t job = kan_cpu_job_create ();
    struct kan_cpu_task_list_node_t *node = NULL;

    check_manual_event_from_task (&event_fetch_first, &node, &temporary_allocator,
                                  (struct manual_event_t) {.x = 42u, .y = 2u});
    check_manual_event_from_task (&event_fetch_first, &node, &temporary_allocator,
                                  (struct manual_event_t) {.x = 13u, .y = 19u});
    check_manual_event_from_task (&event_fetch_first, &node, &temporary_allocator,
                                  (struct manual_event_t) {.x = 111u, .y = 222u});

    check_manual_event_from_task (&event_fetch_second, &node, &temporary_allocator,
                                  (struct manual_event_t) {.x = 42u, .y = 2u});
    check_manual_event_from_task (&event_fetch_second, &node, &temporary_allocator,
                                  (struct manual_event_t) {.x = 13u, .y = 19u});
    check_manual_event_from_task (&event_fetch_second, &node, &temporary_allocator,
                                  (struct manual_event_t) {.x = 111u, .y = 222u});

    kan_cpu_job_dispatch_and_detach_task_list (job, node);
    kan_cpu_job_release (job);
    kan_cpu_job_wait (job);
    kan_stack_group_allocator_shutdown (&temporary_allocator);

    kan_repository_enter_planning_mode (repository);
    kan_repository_event_insert_query_shutdown (&event_insert);
    kan_repository_event_fetch_query_shutdown (&event_fetch_first);
    kan_repository_event_fetch_query_shutdown (&event_fetch_second);

    kan_repository_destroy (repository);
    kan_reflection_registry_destroy (registry);
}
