#include <test_repository_api.h>

#include <stddef.h>

#include <kan/container/stack_group_allocator.h>
#include <kan/cpu_dispatch/job.h>
#include <kan/reflection/generated_reflection.h>
#include <kan/repository/repository.h>
#include <kan/testing/testing.h>

KAN_REFLECTION_EXPECT_UNIT_REGISTRAR (repository);
KAN_REFLECTION_EXPECT_UNIT_REGISTRAR (test_repository);

struct manual_event_t
{
    int32_t x;
    int32_t y;
};

struct manual_event_second_t
{
    uint64_t id;
    float a;
    float b;
};

struct first_singleton_t
{
    uint32_t x;
    uint32_t y;
    uint32_t observable_z;

    kan_interned_string_t some_string;
    float observable_a;
    float observable_b;
};

TEST_REPOSITORY_API void first_singleton_init (uint64_t reflection_user_data, void *data)
{
    *(struct first_singleton_t *) data = (struct first_singleton_t) {
        .x = 0u,
        .y = 0u,
        .observable_z = 0u,

        .some_string = kan_string_intern ("some_value"),
        .observable_a = 0.0f,
        .observable_b = 0.0f,
    };
}

struct first_singleton_z_changed_event_t
{
    uint32_t old_z;
    uint32_t new_z;
};

// \meta reflection_struct_meta = "first_singleton_t"
TEST_REPOSITORY_API struct kan_repository_meta_automatic_on_change_event_t first_singleton_z_changed_event = {
    .event_type = "first_singleton_z_changed_event_t",
    .observed_fields_count = 1u,
    .observed_fields =
        (struct kan_repository_field_path_t[]) {
            {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_z"}},
        },
    .unchanged_copy_outs_count = 1u,
    .unchanged_copy_outs =
        (struct kan_repository_copy_out_t[]) {
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_z"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"old_z"}},
            },
        },
    .changed_copy_outs_count = 1u,
    .changed_copy_outs =
        (struct kan_repository_copy_out_t[]) {
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_z"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"new_z"}},
            },
        },
};

struct first_singleton_a_b_changed_event_t
{
    float old_a;
    float old_b;
    float new_a;
    float new_b;
};

// \meta reflection_struct_meta = "first_singleton_t"
TEST_REPOSITORY_API struct kan_repository_meta_automatic_on_change_event_t first_singleton_a_b_changed_event = {
    .event_type = "first_singleton_a_b_changed_event_t",
    .observed_fields_count = 2u,
    .observed_fields =
        (struct kan_repository_field_path_t[]) {
            {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_a"}},
            {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_b"}},
        },
    .unchanged_copy_outs_count = 2u,
    .unchanged_copy_outs =
        (struct kan_repository_copy_out_t[]) {
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_a"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"old_a"}},
            },
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_b"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"old_b"}},
            },
        },
    .changed_copy_outs_count = 2u,
    .changed_copy_outs =
        (struct kan_repository_copy_out_t[]) {
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_a"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"new_a"}},
            },
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_b"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"new_b"}},
            },
        },
};

struct second_singleton_t
{
    uint32_t a;
    uint32_t b;
};

TEST_REPOSITORY_API void second_singleton_init (uint64_t reflection_user_data, void *data)
{
    *(struct second_singleton_t *) data = (struct second_singleton_t) {
        .a = 0u,
        .b = 0u,
    };
}

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

struct check_manual_event_from_task_user_data_t
{
    struct kan_repository_event_read_access_t access;
    struct manual_event_t data;
};

static void check_manual_event_from_task_executor (uint64_t user_data_int)
{
    struct check_manual_event_from_task_user_data_t *user_data =
        (struct check_manual_event_from_task_user_data_t *) user_data_int;

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
    struct check_manual_event_from_task_user_data_t *user_data = kan_stack_group_allocator_allocate (
        temporary_allocator, sizeof (struct check_manual_event_from_task_user_data_t),
        _Alignof (struct check_manual_event_from_task_user_data_t));

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

static void check_z_changed_event (struct kan_repository_event_fetch_query_t *query,
                                   struct first_singleton_z_changed_event_t data)
{
    struct kan_repository_event_read_access_t access = kan_repository_event_fetch_query_next (query);
    struct first_singleton_z_changed_event_t *event =
        (struct first_singleton_z_changed_event_t *) kan_repository_event_read_access_resolve (&access);

    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->old_z == data.old_z)
    KAN_TEST_CHECK (event->new_z == data.new_z)
    kan_repository_event_read_access_close (&access);
}

static void check_a_b_changed_event (struct kan_repository_event_fetch_query_t *query,
                                     struct first_singleton_a_b_changed_event_t data)
{
    struct kan_repository_event_read_access_t access = kan_repository_event_fetch_query_next (query);
    struct first_singleton_a_b_changed_event_t *event =
        (struct first_singleton_a_b_changed_event_t *) kan_repository_event_read_access_resolve (&access);

    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->old_a == data.old_a)
    KAN_TEST_CHECK (event->old_b == data.old_b)
    KAN_TEST_CHECK (event->new_a == data.new_a)
    KAN_TEST_CHECK (event->new_b == data.new_b)
    kan_repository_event_read_access_close (&access);
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

KAN_TEST_CASE (singleton_access)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (repository) (registry);
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_repository) (registry);

    kan_repository_t root_repository = kan_repository_create_root (KAN_ALLOCATION_GROUP_IGNORE, registry);
    kan_repository_t child_repository = kan_repository_create_child (root_repository, "child");

    kan_repository_singleton_storage_t first_storage_root =
        kan_repository_singleton_storage_open (root_repository, "first_singleton_t");

    kan_repository_singleton_storage_t first_storage_child =
        kan_repository_singleton_storage_open (child_repository, "first_singleton_t");

    kan_repository_singleton_storage_t second_storage_child =
        kan_repository_singleton_storage_open (child_repository, "second_singleton_t");

    struct kan_repository_singleton_read_query_t first_read_from_root;
    kan_repository_singleton_read_query_init (&first_read_from_root, first_storage_root);

    struct kan_repository_singleton_write_query_t first_write_from_child;
    kan_repository_singleton_write_query_init (&first_write_from_child, first_storage_child);

    struct kan_repository_singleton_write_query_t second_write_from_child;
    kan_repository_singleton_write_query_init (&second_write_from_child, second_storage_child);

    kan_repository_enter_serving_mode (root_repository);

    {
        kan_repository_singleton_read_access_t first_access =
            kan_repository_singleton_read_query_execute (&first_read_from_root);

        const struct first_singleton_t *singleton =
            (struct first_singleton_t *) kan_repository_singleton_read_access_resolve (first_access);

        KAN_TEST_ASSERT (singleton)
        KAN_TEST_CHECK (singleton->x == 0u)
        KAN_TEST_CHECK (singleton->y == 0u)
        KAN_TEST_CHECK (singleton->observable_z == 0u)

        KAN_TEST_CHECK (singleton->some_string == kan_string_intern ("some_value"))
        KAN_TEST_CHECK (singleton->observable_a == 0.0f)
        KAN_TEST_CHECK (singleton->observable_b == 0.0f)

        kan_repository_singleton_read_access_close (first_access);
    }

    {
        kan_repository_singleton_write_access_t first_access =
            kan_repository_singleton_write_query_execute (&first_write_from_child);
        kan_repository_singleton_write_access_t second_access =
            kan_repository_singleton_write_query_execute (&second_write_from_child);

        struct first_singleton_t *first_singleton =
            (struct first_singleton_t *) kan_repository_singleton_write_access_resolve (first_access);
        KAN_TEST_ASSERT (first_singleton)

        struct second_singleton_t *second_singleton =
            (struct second_singleton_t *) kan_repository_singleton_write_access_resolve (second_access);
        KAN_TEST_ASSERT (second_singleton)

        first_singleton->x = 1u;
        first_singleton->y = 2u;
        first_singleton->observable_z = 3u;

        second_singleton->a = 42u;
        second_singleton->b = 13u;

        kan_repository_singleton_write_access_close (first_access);
        kan_repository_singleton_write_access_close (second_access);
    }

    {
        kan_repository_singleton_read_access_t first_access =
            kan_repository_singleton_read_query_execute (&first_read_from_root);

        const struct first_singleton_t *singleton =
            (struct first_singleton_t *) kan_repository_singleton_read_access_resolve (first_access);

        KAN_TEST_ASSERT (singleton)
        KAN_TEST_CHECK (singleton->x == 1u)
        KAN_TEST_CHECK (singleton->y == 2u)
        KAN_TEST_CHECK (singleton->observable_z == 3u)

        kan_repository_singleton_read_access_close (first_access);
    }

    {
        kan_repository_singleton_write_access_t second_access =
            kan_repository_singleton_write_query_execute (&second_write_from_child);

        struct second_singleton_t *singleton =
            (struct second_singleton_t *) kan_repository_singleton_write_access_resolve (second_access);

        KAN_TEST_ASSERT (singleton)
        KAN_TEST_CHECK (singleton->a == 42u)
        KAN_TEST_CHECK (singleton->b == 13u)

        kan_repository_singleton_write_access_close (second_access);
    }

    kan_repository_enter_planning_mode (root_repository);
    kan_repository_singleton_read_query_shutdown (&first_read_from_root);
    kan_repository_singleton_write_query_shutdown (&first_write_from_child);
    kan_repository_singleton_write_query_shutdown (&second_write_from_child);

    kan_repository_destroy (root_repository);
    kan_reflection_registry_destroy (registry);
}

KAN_TEST_CASE (singleton_write_events)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (repository) (registry);
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_repository) (registry);
    kan_repository_t repository = kan_repository_create_root (KAN_ALLOCATION_GROUP_IGNORE, registry);

    kan_repository_singleton_storage_t first_storage =
        kan_repository_singleton_storage_open (repository, "first_singleton_t");

    struct kan_repository_singleton_write_query_t write_singleton;
    kan_repository_singleton_write_query_init (&write_singleton, first_storage);

    kan_repository_event_storage_t event_z_storage =
        kan_repository_event_storage_open (repository, "first_singleton_z_changed_event_t");

    kan_repository_event_storage_t event_a_b_storage =
        kan_repository_event_storage_open (repository, "first_singleton_a_b_changed_event_t");

    struct kan_repository_event_fetch_query_t fetch_z_changed;
    kan_repository_event_fetch_query_init (&fetch_z_changed, event_z_storage);

    struct kan_repository_event_fetch_query_t fetch_a_b_changed;
    kan_repository_event_fetch_query_init (&fetch_a_b_changed, event_a_b_storage);

    kan_repository_enter_serving_mode (repository);
    check_no_event (&fetch_z_changed);
    check_no_event (&fetch_a_b_changed);

    // Change Z and check events.
    {
        kan_repository_singleton_write_access_t access =
            kan_repository_singleton_write_query_execute (&write_singleton);

        struct first_singleton_t *singleton =
            (struct first_singleton_t *) kan_repository_singleton_write_access_resolve (access);

        KAN_TEST_ASSERT (singleton)
        singleton->observable_z = 42u;
        kan_repository_singleton_write_access_close (access);
    }

    check_z_changed_event (&fetch_z_changed, (struct first_singleton_z_changed_event_t) {.old_z = 0u, .new_z = 42u});
    check_no_event (&fetch_a_b_changed);

    // Set same value to Z and check that there is no event.
    {
        kan_repository_singleton_write_access_t access =
            kan_repository_singleton_write_query_execute (&write_singleton);

        struct first_singleton_t *singleton =
            (struct first_singleton_t *) kan_repository_singleton_write_access_resolve (access);

        KAN_TEST_ASSERT (singleton)
        singleton->observable_z = 13u;
        singleton->observable_z = 42u;
        kan_repository_singleton_write_access_close (access);
    }

    check_no_event (&fetch_z_changed);
    check_no_event (&fetch_a_b_changed);

    // Set value to only A and check event.
    {
        kan_repository_singleton_write_access_t access =
            kan_repository_singleton_write_query_execute (&write_singleton);

        struct first_singleton_t *singleton =
            (struct first_singleton_t *) kan_repository_singleton_write_access_resolve (access);

        KAN_TEST_ASSERT (singleton)
        singleton->observable_a = 1.0f;
        kan_repository_singleton_write_access_close (access);
    }

    check_no_event (&fetch_z_changed);
    check_a_b_changed_event (&fetch_a_b_changed, (struct first_singleton_a_b_changed_event_t) {
                                                     .old_a = 0.0f, .old_b = 0.0f, .new_a = 1.0f, .new_b = 0.0f});

    // Set value to only B and check event.
    {
        kan_repository_singleton_write_access_t access =
            kan_repository_singleton_write_query_execute (&write_singleton);

        struct first_singleton_t *singleton =
            (struct first_singleton_t *) kan_repository_singleton_write_access_resolve (access);

        KAN_TEST_ASSERT (singleton)
        singleton->observable_b = 2.0f;
        kan_repository_singleton_write_access_close (access);
    }

    check_no_event (&fetch_z_changed);
    check_a_b_changed_event (&fetch_a_b_changed, (struct first_singleton_a_b_changed_event_t) {
                                                     .old_a = 1.0f, .old_b = 0.0f, .new_a = 1.0f, .new_b = 2.0f});

    // Set value to both A and B and check event.
    {
        kan_repository_singleton_write_access_t access =
            kan_repository_singleton_write_query_execute (&write_singleton);

        struct first_singleton_t *singleton =
            (struct first_singleton_t *) kan_repository_singleton_write_access_resolve (access);

        KAN_TEST_ASSERT (singleton)
        singleton->observable_a = 4.0f;
        singleton->observable_b = 3.0f;
        kan_repository_singleton_write_access_close (access);
    }

    check_no_event (&fetch_z_changed);
    check_a_b_changed_event (&fetch_a_b_changed, (struct first_singleton_a_b_changed_event_t) {
                                                     .old_a = 1.0f, .old_b = 2.0f, .new_a = 4.0f, .new_b = 3.0f});

    kan_repository_enter_planning_mode (repository);
    kan_repository_singleton_write_query_shutdown (&write_singleton);
    kan_repository_event_fetch_query_shutdown (&fetch_z_changed);
    kan_repository_event_fetch_query_shutdown (&fetch_a_b_changed);

    kan_repository_destroy (repository);
    kan_reflection_registry_destroy (registry);
}
