#include <test_repository_api.h>

#include <stddef.h>

#include <kan/container/stack_group_allocator.h>
#include <kan/cpu_dispatch/job.h>
#include <kan/reflection/generated_reflection.h>
#include <kan/reflection/markup.h>
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

TEST_REPOSITORY_API void first_singleton_init (struct first_singleton_t *data)
{
    data->x = 0u;
    data->y = 0u;
    data->observable_z = 0u;

    data->some_string = kan_string_intern ("some_value");
    data->observable_a = 0.0f;
    data->observable_b = 0.0f;
}

struct first_singleton_z_changed_event_t
{
    uint32_t old_z;
    uint32_t new_z;
};

KAN_REFLECTION_STRUCT_META (first_singleton_t)
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

KAN_REFLECTION_STRUCT_META (first_singleton_t)
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

TEST_REPOSITORY_API void second_singleton_init (struct second_singleton_t *data)
{
    data->a = 0u;
    data->b = 0u;
}

struct object_record_t
{
    uint32_t object_id;
    uint32_t parent_object_id;
    uint32_t data_x;
    uint32_t data_y;
};

#define INVALID_PARENT_OBJECT_ID KAN_INT_MAX (uint32_t)

TEST_REPOSITORY_API void object_record_init (struct object_record_t *data)
{
    data->object_id = 0u;
    data->parent_object_id = INVALID_PARENT_OBJECT_ID;
    data->data_x = 0u;
    data->data_y = 0u;
}

KAN_REFLECTION_STRUCT_META (object_record_t)
TEST_REPOSITORY_API struct kan_repository_meta_automatic_cascade_deletion_t object_record_hierarchy_cascade_deletion = {
    .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
    .child_type_name = "object_record_t",
    .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"parent_object_id"}},
};

struct status_record_t
{
    uint32_t object_id;
    bool observable_alive;
    bool observable_poisoned;
    bool observable_stunned;
    bool observable_boosted;
};

TEST_REPOSITORY_API void status_record_init (struct status_record_t *data)
{
    data->object_id = 0u;
    data->observable_alive = false;
    data->observable_poisoned = false;
    data->observable_stunned = false;
    data->observable_boosted = false;
}

KAN_REFLECTION_STRUCT_META (object_record_t)
TEST_REPOSITORY_API struct kan_repository_meta_automatic_cascade_deletion_t status_record_object_cascade_deletion = {
    .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
    .child_type_name = "status_record_t",
    .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
};

struct status_record_on_insert_event_t
{
    uint32_t object_id;
    bool initially_alive;
};

KAN_REFLECTION_STRUCT_META (status_record_t)
TEST_REPOSITORY_API struct kan_repository_meta_automatic_on_insert_event_t status_record_on_insert = {
    .event_type = "status_record_on_insert_event_t",
    .copy_outs_count = 2u,
    .copy_outs =
        (struct kan_repository_copy_out_t[]) {
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
            },
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_alive"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"initially_alive"}},
            },
        },
};

struct status_record_on_change_event_t
{
    uint32_t object_id;
    bool was_alive;
    bool was_poisoned;
    bool was_stunned;
    bool was_boosted;
    bool now_alive;
    bool now_poisoned;
    bool now_stunned;
    bool now_boosted;
};

KAN_REFLECTION_STRUCT_META (status_record_t)
TEST_REPOSITORY_API struct kan_repository_meta_automatic_on_change_event_t status_record_on_change = {
    .event_type = "status_record_on_change_event_t",
    .observed_fields_count = 4u,
    .observed_fields =
        (struct kan_repository_field_path_t[]) {
            {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_alive"}},
            {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_poisoned"}},
            {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_stunned"}},
            {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_boosted"}},
        },
    .unchanged_copy_outs_count = 4u,
    .unchanged_copy_outs =
        (struct kan_repository_copy_out_t[]) {
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_alive"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"was_alive"}},
            },
            {
                .source_path = {.reflection_path_length = 1u,
                                .reflection_path = (const char *[]) {"observable_poisoned"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"was_poisoned"}},
            },
            {
                .source_path = {.reflection_path_length = 1u,
                                .reflection_path = (const char *[]) {"observable_stunned"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"was_stunned"}},
            },
            {
                .source_path = {.reflection_path_length = 1u,
                                .reflection_path = (const char *[]) {"observable_boosted"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"was_boosted"}},
            },
        },
    .changed_copy_outs_count = 5u,
    .changed_copy_outs =
        (struct kan_repository_copy_out_t[]) {
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
            },
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_alive"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"now_alive"}},
            },
            {
                .source_path = {.reflection_path_length = 1u,
                                .reflection_path = (const char *[]) {"observable_poisoned"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"now_poisoned"}},
            },
            {
                .source_path = {.reflection_path_length = 1u,
                                .reflection_path = (const char *[]) {"observable_stunned"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"now_stunned"}},
            },
            {
                .source_path = {.reflection_path_length = 1u,
                                .reflection_path = (const char *[]) {"observable_boosted"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"now_boosted"}},
            },
        },
};

struct status_record_on_delete_event_t
{
    uint32_t object_id;
    bool was_alive;
};

KAN_REFLECTION_STRUCT_META (status_record_t)
TEST_REPOSITORY_API struct kan_repository_meta_automatic_on_delete_event_t status_record_on_delete = {
    .event_type = "status_record_on_delete_event_t",
    .copy_outs_count = 2u,
    .copy_outs =
        (struct kan_repository_copy_out_t[]) {
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
            },
            {
                .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"observable_alive"}},
                .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"was_alive"}},
            },
        },
};

struct multi_component_record_t
{
    uint32_t object_id;
    uint64_t some_data;
};

TEST_REPOSITORY_API void multi_component_record_init (struct multi_component_record_t *data)
{
    data->object_id = 0u;
    data->some_data = 0u;
}

KAN_REFLECTION_STRUCT_META (object_record_t)
TEST_REPOSITORY_API struct kan_repository_meta_automatic_cascade_deletion_t
    multi_component_record_object_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
        .child_type_name = "multi_component_record_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
};

struct bounding_box_component_record_t
{
    uint32_t object_id;
    float min[3u];
    float max[3u];
};

KAN_REFLECTION_STRUCT_META (object_record_t)
TEST_REPOSITORY_API struct kan_repository_meta_automatic_cascade_deletion_t
    bounding_box_component_record_object_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
        .child_type_name = "bounding_box_component_record_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"object_id"}},
};

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

static void check_manual_event_from_task_executor (kan_functor_user_data_t user_data_int)
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
    KAN_CPU_TASK_LIST_USER_STRUCT (node, temporary_allocator, check_manual_event_from_task_executor,
                                   kan_cpu_section_get ("check_manual_event_from_task"),
                                   struct check_manual_event_from_task_user_data_t,
                                   {
                                       .access = access,
                                       .data = data,
                                   })
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

static void check_status_insert_event (struct kan_repository_event_fetch_query_t *query,
                                       struct status_record_on_insert_event_t data)
{
    struct kan_repository_event_read_access_t access = kan_repository_event_fetch_query_next (query);
    struct status_record_on_insert_event_t *event =
        (struct status_record_on_insert_event_t *) kan_repository_event_read_access_resolve (&access);

    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->object_id == data.object_id)
    KAN_TEST_CHECK (event->initially_alive == data.initially_alive)
    kan_repository_event_read_access_close (&access);
}

static void check_status_change_event (struct kan_repository_event_fetch_query_t *query,
                                       struct status_record_on_change_event_t data)
{
    struct kan_repository_event_read_access_t access = kan_repository_event_fetch_query_next (query);
    struct status_record_on_change_event_t *event =
        (struct status_record_on_change_event_t *) kan_repository_event_read_access_resolve (&access);

    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->object_id == data.object_id)
    KAN_TEST_CHECK (event->was_alive == data.was_alive)
    KAN_TEST_CHECK (event->was_poisoned == data.was_poisoned)
    KAN_TEST_CHECK (event->was_stunned == data.was_stunned)
    KAN_TEST_CHECK (event->was_boosted == data.was_boosted)
    KAN_TEST_CHECK (event->now_alive == data.now_alive)
    KAN_TEST_CHECK (event->now_poisoned == data.now_poisoned)
    KAN_TEST_CHECK (event->now_stunned == data.now_stunned)
    KAN_TEST_CHECK (event->now_boosted == data.now_boosted)
    kan_repository_event_read_access_close (&access);
}

static void check_status_delete_event (struct kan_repository_event_fetch_query_t *query,
                                       struct status_record_on_delete_event_t data)
{
    struct kan_repository_event_read_access_t access = kan_repository_event_fetch_query_next (query);
    struct status_record_on_delete_event_t *event =
        (struct status_record_on_delete_event_t *) kan_repository_event_read_access_resolve (&access);

    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->object_id == data.object_id)
    KAN_TEST_CHECK (event->was_alive == data.was_alive)
    kan_repository_event_read_access_close (&access);
}

static void insert_object_record_and_undo (struct kan_repository_indexed_insert_query_t *query)
{
    struct kan_repository_indexed_insertion_package_t package = kan_repository_indexed_insert_query_execute (query);
    KAN_TEST_CHECK (kan_repository_indexed_insertion_package_get (&package));
    kan_repository_indexed_insertion_package_undo (&package);
}

static void insert_object_record (struct kan_repository_indexed_insert_query_t *query, struct object_record_t data)
{
    struct kan_repository_indexed_insertion_package_t package = kan_repository_indexed_insert_query_execute (query);
    struct object_record_t *record = (struct object_record_t *) kan_repository_indexed_insertion_package_get (&package);
    KAN_TEST_ASSERT (record);
    *record = data;
    kan_repository_indexed_insertion_package_submit (&package);
}

static void insert_status_record (struct kan_repository_indexed_insert_query_t *query, struct status_record_t data)
{
    struct kan_repository_indexed_insertion_package_t package = kan_repository_indexed_insert_query_execute (query);
    struct status_record_t *record = (struct status_record_t *) kan_repository_indexed_insertion_package_get (&package);
    KAN_TEST_ASSERT (record);
    *record = data;
    kan_repository_indexed_insertion_package_submit (&package);
}

static void insert_multi_component_record (struct kan_repository_indexed_insert_query_t *query,
                                           struct multi_component_record_t data)
{
    struct kan_repository_indexed_insertion_package_t package = kan_repository_indexed_insert_query_execute (query);
    struct multi_component_record_t *record =
        (struct multi_component_record_t *) kan_repository_indexed_insertion_package_get (&package);

    KAN_TEST_ASSERT (record);
    *record = data;
    kan_repository_indexed_insertion_package_submit (&package);
}

static void check_value_exists_unique (struct kan_repository_indexed_value_read_query_t *query, uint64_t value)
{
    struct kan_repository_indexed_value_read_cursor_t cursor =
        kan_repository_indexed_value_read_query_execute (query, &value);

    struct kan_repository_indexed_value_read_access_t access = kan_repository_indexed_value_read_cursor_next (&cursor);
    KAN_TEST_CHECK (kan_repository_indexed_value_read_access_resolve (&access))
    kan_repository_indexed_value_read_access_close (&access);

    access = kan_repository_indexed_value_read_cursor_next (&cursor);
    KAN_TEST_CHECK (!kan_repository_indexed_value_read_access_resolve (&access))
    kan_repository_indexed_value_read_cursor_close (&cursor);
}

static void check_value_not_exists (struct kan_repository_indexed_value_read_query_t *query, uint64_t value)
{
    struct kan_repository_indexed_value_read_cursor_t cursor =
        kan_repository_indexed_value_read_query_execute (query, &value);

    struct kan_repository_indexed_value_read_access_t access = kan_repository_indexed_value_read_cursor_next (&cursor);
    KAN_TEST_CHECK (!kan_repository_indexed_value_read_access_resolve (&access))
    kan_repository_indexed_value_read_cursor_close (&cursor);
}

static void insert_bounding_box_component_record (struct kan_repository_indexed_insert_query_t *query,
                                                  struct bounding_box_component_record_t data)
{
    struct kan_repository_indexed_insertion_package_t package = kan_repository_indexed_insert_query_execute (query);
    struct bounding_box_component_record_t *record =
        (struct bounding_box_component_record_t *) kan_repository_indexed_insertion_package_get (&package);

    KAN_TEST_ASSERT (record);
    *record = data;
    kan_repository_indexed_insertion_package_submit (&package);
}

static uint64_t query_bounding_box (struct kan_repository_indexed_space_read_query_t *query,
                                    kan_coordinate_floating_t min_x,
                                    kan_coordinate_floating_t min_y,
                                    kan_coordinate_floating_t min_z,
                                    kan_coordinate_floating_t max_x,
                                    kan_coordinate_floating_t max_y,
                                    kan_coordinate_floating_t max_z)
{
    uint64_t flags = 0u;
    const kan_coordinate_floating_t min[] = {min_x, min_y, min_z};
    const kan_coordinate_floating_t max[] = {max_x, max_y, max_z};

    struct kan_repository_indexed_space_shape_read_cursor_t cursor =
        kan_repository_indexed_space_read_query_execute_shape (query, min, max);

    while (true)
    {
        struct kan_repository_indexed_space_read_access_t access =
            kan_repository_indexed_space_shape_read_cursor_next (&cursor);

        const struct bounding_box_component_record_t *record =
            (struct bounding_box_component_record_t *) kan_repository_indexed_space_read_access_resolve (&access);

        if (!record)
        {
            break;
        }

        const uint64_t record_flag = ((uint64_t) 1u) << record->object_id;
        // Check that there are no duplicate visits.
        KAN_TEST_CHECK ((flags & record_flag) == 0u)
        flags |= record_flag;
        kan_repository_indexed_space_read_access_close (&access);
    }

    kan_repository_indexed_space_shape_read_cursor_close (&cursor);
    return flags;
}

static uint64_t query_ray (struct kan_repository_indexed_space_read_query_t *query,
                           kan_coordinate_floating_t origin_x,
                           kan_coordinate_floating_t origin_y,
                           kan_coordinate_floating_t origin_z,
                           kan_coordinate_floating_t direction_x,
                           kan_coordinate_floating_t direction_y,
                           kan_coordinate_floating_t direction_z,
                           kan_coordinate_floating_t max_time)
{
    uint64_t flags = 0u;
    const kan_coordinate_floating_t origin[] = {origin_x, origin_y, origin_z};
    const kan_coordinate_floating_t direction[] = {direction_x, direction_y, direction_z};

    struct kan_repository_indexed_space_ray_read_cursor_t cursor =
        kan_repository_indexed_space_read_query_execute_ray (query, origin, direction, max_time);

    while (true)
    {
        struct kan_repository_indexed_space_read_access_t access =
            kan_repository_indexed_space_ray_read_cursor_next (&cursor);

        const struct bounding_box_component_record_t *record =
            (struct bounding_box_component_record_t *) kan_repository_indexed_space_read_access_resolve (&access);

        if (!record)
        {
            break;
        }

        const uint64_t record_flag = ((uint64_t) 1u) << record->object_id;
        // Check that there are no duplicate visits.
        KAN_TEST_CHECK ((flags & record_flag) == 0u)
        flags |= record_flag;
        kan_repository_indexed_space_read_access_close (&access);
    }

    kan_repository_indexed_space_ray_read_cursor_close (&cursor);
    return flags;
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
        struct kan_repository_singleton_read_access_t first_access =
            kan_repository_singleton_read_query_execute (&first_read_from_root);

        const struct first_singleton_t *singleton =
            (struct first_singleton_t *) kan_repository_singleton_read_access_resolve (&first_access);

        KAN_TEST_ASSERT (singleton)
        KAN_TEST_CHECK (singleton->x == 0u)
        KAN_TEST_CHECK (singleton->y == 0u)
        KAN_TEST_CHECK (singleton->observable_z == 0u)

        KAN_TEST_CHECK (singleton->some_string == kan_string_intern ("some_value"))
        KAN_TEST_CHECK (singleton->observable_a == 0.0f)
        KAN_TEST_CHECK (singleton->observable_b == 0.0f)

        kan_repository_singleton_read_access_close (&first_access);
    }

    {
        struct kan_repository_singleton_write_access_t first_access =
            kan_repository_singleton_write_query_execute (&first_write_from_child);
        struct kan_repository_singleton_write_access_t second_access =
            kan_repository_singleton_write_query_execute (&second_write_from_child);

        struct first_singleton_t *first_singleton =
            (struct first_singleton_t *) kan_repository_singleton_write_access_resolve (&first_access);
        KAN_TEST_ASSERT (first_singleton)

        struct second_singleton_t *second_singleton =
            (struct second_singleton_t *) kan_repository_singleton_write_access_resolve (&second_access);
        KAN_TEST_ASSERT (second_singleton)

        first_singleton->x = 1u;
        first_singleton->y = 2u;
        first_singleton->observable_z = 3u;

        second_singleton->a = 42u;
        second_singleton->b = 13u;

        kan_repository_singleton_write_access_close (&first_access);
        kan_repository_singleton_write_access_close (&second_access);
    }

    {
        struct kan_repository_singleton_read_access_t first_access =
            kan_repository_singleton_read_query_execute (&first_read_from_root);

        const struct first_singleton_t *singleton =
            (struct first_singleton_t *) kan_repository_singleton_read_access_resolve (&first_access);

        KAN_TEST_ASSERT (singleton)
        KAN_TEST_CHECK (singleton->x == 1u)
        KAN_TEST_CHECK (singleton->y == 2u)
        KAN_TEST_CHECK (singleton->observable_z == 3u)

        kan_repository_singleton_read_access_close (&first_access);
    }

    {
        struct kan_repository_singleton_write_access_t second_access =
            kan_repository_singleton_write_query_execute (&second_write_from_child);

        struct second_singleton_t *singleton =
            (struct second_singleton_t *) kan_repository_singleton_write_access_resolve (&second_access);

        KAN_TEST_ASSERT (singleton)
        KAN_TEST_CHECK (singleton->a == 42u)
        KAN_TEST_CHECK (singleton->b == 13u)

        kan_repository_singleton_write_access_close (&second_access);
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
        struct kan_repository_singleton_write_access_t access =
            kan_repository_singleton_write_query_execute (&write_singleton);

        struct first_singleton_t *singleton =
            (struct first_singleton_t *) kan_repository_singleton_write_access_resolve (&access);

        KAN_TEST_ASSERT (singleton)
        singleton->observable_z = 42u;
        kan_repository_singleton_write_access_close (&access);
    }

    check_z_changed_event (&fetch_z_changed, (struct first_singleton_z_changed_event_t) {.old_z = 0u, .new_z = 42u});
    check_no_event (&fetch_a_b_changed);

    // Set same value to Z and check that there is no event.
    {
        struct kan_repository_singleton_write_access_t access =
            kan_repository_singleton_write_query_execute (&write_singleton);

        struct first_singleton_t *singleton =
            (struct first_singleton_t *) kan_repository_singleton_write_access_resolve (&access);

        KAN_TEST_ASSERT (singleton)
        singleton->observable_z = 13u;
        singleton->observable_z = 42u;
        kan_repository_singleton_write_access_close (&access);
    }

    check_no_event (&fetch_z_changed);
    check_no_event (&fetch_a_b_changed);

    // Set value to only A and check event.
    {
        struct kan_repository_singleton_write_access_t access =
            kan_repository_singleton_write_query_execute (&write_singleton);

        struct first_singleton_t *singleton =
            (struct first_singleton_t *) kan_repository_singleton_write_access_resolve (&access);

        KAN_TEST_ASSERT (singleton)
        singleton->observable_a = 1.0f;
        kan_repository_singleton_write_access_close (&access);
    }

    check_no_event (&fetch_z_changed);
    check_a_b_changed_event (&fetch_a_b_changed, (struct first_singleton_a_b_changed_event_t) {
                                                     .old_a = 0.0f, .old_b = 0.0f, .new_a = 1.0f, .new_b = 0.0f});

    // Set value to only B and check event.
    {
        struct kan_repository_singleton_write_access_t access =
            kan_repository_singleton_write_query_execute (&write_singleton);

        struct first_singleton_t *singleton =
            (struct first_singleton_t *) kan_repository_singleton_write_access_resolve (&access);

        KAN_TEST_ASSERT (singleton)
        singleton->observable_b = 2.0f;
        kan_repository_singleton_write_access_close (&access);
    }

    check_no_event (&fetch_z_changed);
    check_a_b_changed_event (&fetch_a_b_changed, (struct first_singleton_a_b_changed_event_t) {
                                                     .old_a = 1.0f, .old_b = 0.0f, .new_a = 1.0f, .new_b = 2.0f});

    // Set value to both A and B and check event.
    {
        struct kan_repository_singleton_write_access_t access =
            kan_repository_singleton_write_query_execute (&write_singleton);

        struct first_singleton_t *singleton =
            (struct first_singleton_t *) kan_repository_singleton_write_access_resolve (&access);

        KAN_TEST_ASSERT (singleton)
        singleton->observable_a = 4.0f;
        singleton->observable_b = 3.0f;
        kan_repository_singleton_write_access_close (&access);
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

KAN_TEST_CASE (indexed_sequence_operations)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (repository) (registry);
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_repository) (registry);

    kan_repository_t root_repository = kan_repository_create_root (KAN_ALLOCATION_GROUP_IGNORE, registry);
    kan_repository_t child_repository = kan_repository_create_child (root_repository, "child");

    kan_repository_indexed_storage_t storage_root =
        kan_repository_indexed_storage_open (root_repository, "object_record_t");
    kan_repository_indexed_storage_t storage_child =
        kan_repository_indexed_storage_open (child_repository, "object_record_t");

    struct kan_repository_indexed_insert_query_t insert_child;
    kan_repository_indexed_insert_query_init (&insert_child, storage_child);

    struct kan_repository_indexed_sequence_read_query_t read_root;
    kan_repository_indexed_sequence_read_query_init (&read_root, storage_root);

    struct kan_repository_indexed_sequence_update_query_t update_child;
    kan_repository_indexed_sequence_update_query_init (&update_child, storage_child);

    struct kan_repository_indexed_sequence_delete_query_t delete_child;
    kan_repository_indexed_sequence_delete_query_init (&delete_child, storage_child);

    struct kan_repository_indexed_sequence_write_query_t write_root;
    kan_repository_indexed_sequence_write_query_init (&write_root, storage_root);

    kan_repository_enter_serving_mode (root_repository);

    {
        struct kan_repository_indexed_sequence_read_cursor_t cursor =
            kan_repository_indexed_sequence_read_query_execute (&read_root);

        struct kan_repository_indexed_sequence_read_access_t access =
            kan_repository_indexed_sequence_read_cursor_next (&cursor);
        KAN_TEST_CHECK (!kan_repository_indexed_sequence_read_access_resolve (&access));

        kan_repository_indexed_sequence_read_cursor_close (&cursor);
    }

    insert_object_record (
        &insert_child,
        (struct object_record_t) {
            .object_id = 1u, .parent_object_id = INVALID_PARENT_OBJECT_ID, .data_x = 42u, .data_y = 13u});

    insert_object_record (
        &insert_child,
        (struct object_record_t) {
            .object_id = 2u, .parent_object_id = INVALID_PARENT_OBJECT_ID, .data_x = 11u, .data_y = 19u});

    insert_object_record_and_undo (&insert_child);

    {
        struct kan_repository_indexed_sequence_update_cursor_t cursor =
            kan_repository_indexed_sequence_update_query_execute (&update_child);
        kan_loop_size_t objects_found = 0u;

        while (true)
        {
            struct kan_repository_indexed_sequence_update_access_t access =
                kan_repository_indexed_sequence_update_cursor_next (&cursor);

            struct object_record_t *object =
                (struct object_record_t *) kan_repository_indexed_sequence_update_access_resolve (&access);

            if (!object)
            {
                break;
            }

            ++objects_found;
            if (object->object_id == 1u)
            {
                KAN_TEST_CHECK (object->parent_object_id == INVALID_PARENT_OBJECT_ID)
                KAN_TEST_CHECK (object->data_x == 42u)
                KAN_TEST_CHECK (object->data_y == 13u)
                object->object_id = 10u;
            }
            else
            {
                KAN_TEST_CHECK (object->object_id == 2u)
                KAN_TEST_CHECK (object->parent_object_id == INVALID_PARENT_OBJECT_ID)
                KAN_TEST_CHECK (object->data_x == 11u)
                KAN_TEST_CHECK (object->data_y == 19u)
            }

            kan_repository_indexed_sequence_update_access_close (&access);
        }

        KAN_TEST_CHECK (objects_found == 2u)
        kan_repository_indexed_sequence_update_cursor_close (&cursor);
    }

    {
        struct kan_repository_indexed_sequence_delete_cursor_t cursor =
            kan_repository_indexed_sequence_delete_query_execute (&delete_child);
        kan_loop_size_t objects_found = 0u;

        while (true)
        {
            struct kan_repository_indexed_sequence_delete_access_t access =
                kan_repository_indexed_sequence_delete_cursor_next (&cursor);

            struct object_record_t *object =
                (struct object_record_t *) kan_repository_indexed_sequence_delete_access_resolve (&access);

            if (!object)
            {
                break;
            }

            ++objects_found;
            if (object->object_id == 10u)
            {
                KAN_TEST_CHECK (object->parent_object_id == INVALID_PARENT_OBJECT_ID)
                KAN_TEST_CHECK (object->data_x == 42u)
                KAN_TEST_CHECK (object->data_y == 13u)
                kan_repository_indexed_sequence_delete_access_close (&access);
            }
            else
            {
                KAN_TEST_CHECK (object->object_id == 2u)
                KAN_TEST_CHECK (object->parent_object_id == INVALID_PARENT_OBJECT_ID)
                KAN_TEST_CHECK (object->data_x == 11u)
                KAN_TEST_CHECK (object->data_y == 19u)
                kan_repository_indexed_sequence_delete_access_delete (&access);
            }
        }

        KAN_TEST_CHECK (objects_found == 2u)
        kan_repository_indexed_sequence_delete_cursor_close (&cursor);
    }

    {
        struct kan_repository_indexed_sequence_write_cursor_t cursor =
            kan_repository_indexed_sequence_write_query_execute (&write_root);
        kan_loop_size_t objects_found = 0u;

        while (true)
        {
            struct kan_repository_indexed_sequence_write_access_t access =
                kan_repository_indexed_sequence_write_cursor_next (&cursor);

            struct object_record_t *object =
                (struct object_record_t *) kan_repository_indexed_sequence_write_access_resolve (&access);

            if (!object)
            {
                break;
            }

            ++objects_found;
            if (object->object_id == 10u)
            {
                KAN_TEST_CHECK (object->parent_object_id == INVALID_PARENT_OBJECT_ID)
                KAN_TEST_CHECK (object->data_x == 42u)
                KAN_TEST_CHECK (object->data_y == 13u)
                kan_repository_indexed_sequence_write_access_delete (&access);
            }
            else
            {
                KAN_TEST_CHECK (false)
                kan_repository_indexed_sequence_write_access_close (&access);
            }
        }

        KAN_TEST_CHECK (objects_found == 1u)
        kan_repository_indexed_sequence_write_cursor_close (&cursor);
    }

    {
        struct kan_repository_indexed_sequence_read_cursor_t cursor =
            kan_repository_indexed_sequence_read_query_execute (&read_root);

        struct kan_repository_indexed_sequence_read_access_t access =
            kan_repository_indexed_sequence_read_cursor_next (&cursor);
        KAN_TEST_CHECK (!kan_repository_indexed_sequence_read_access_resolve (&access));

        kan_repository_indexed_sequence_read_cursor_close (&cursor);
    }

    kan_repository_enter_planning_mode (root_repository);
    kan_repository_indexed_insert_query_shutdown (&insert_child);
    kan_repository_indexed_sequence_read_query_shutdown (&read_root);
    kan_repository_indexed_sequence_update_query_shutdown (&update_child);
    kan_repository_indexed_sequence_delete_query_shutdown (&delete_child);
    kan_repository_indexed_sequence_write_query_shutdown (&write_root);

    kan_repository_destroy (root_repository);
    kan_reflection_registry_destroy (registry);
}

KAN_TEST_CASE (indexed_value_operations)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (repository) (registry);
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_repository) (registry);

    kan_repository_t root_repository = kan_repository_create_root (KAN_ALLOCATION_GROUP_IGNORE, registry);
    kan_repository_t child_repository = kan_repository_create_child (root_repository, "child");

    kan_repository_indexed_storage_t storage_root =
        kan_repository_indexed_storage_open (root_repository, "object_record_t");
    kan_repository_indexed_storage_t storage_child =
        kan_repository_indexed_storage_open (child_repository, "object_record_t");
    kan_repository_indexed_storage_t storage_multi_component =
        kan_repository_indexed_storage_open (root_repository, "multi_component_record_t");

    struct kan_repository_indexed_insert_query_t insert_child;
    kan_repository_indexed_insert_query_init (&insert_child, storage_child);

    struct kan_repository_indexed_value_read_query_t read_root;
    kan_repository_indexed_value_read_query_init (
        &read_root, storage_root,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"object_id"}});

    struct kan_repository_indexed_value_update_query_t update_child;
    kan_repository_indexed_value_update_query_init (
        &update_child, storage_child,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"object_id"}});

    struct kan_repository_indexed_value_delete_query_t delete_child;
    kan_repository_indexed_value_delete_query_init (
        &delete_child, storage_child,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"object_id"}});

    struct kan_repository_indexed_insert_query_t insert_multi_component;
    kan_repository_indexed_insert_query_init (&insert_multi_component, storage_multi_component);

    struct kan_repository_indexed_value_write_query_t write_multi_component;
    kan_repository_indexed_value_write_query_init (
        &write_multi_component, storage_multi_component,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"object_id"}});

    kan_repository_enter_serving_mode (root_repository);

    struct object_record_t first_record_value = {
        .object_id = 1u, .parent_object_id = INVALID_PARENT_OBJECT_ID, .data_x = 42u, .data_y = 13u};

    struct object_record_t second_record_value = {
        .object_id = 2u, .parent_object_id = INVALID_PARENT_OBJECT_ID, .data_x = 11u, .data_y = 19u};

    struct object_record_t third_record_value = {
        .object_id = 3u, .parent_object_id = INVALID_PARENT_OBJECT_ID, .data_x = 14u, .data_y = 21u};

    insert_object_record (&insert_child, first_record_value);
    insert_object_record (&insert_child, second_record_value);
    insert_object_record (&insert_child, third_record_value);

    {
        struct kan_repository_indexed_value_read_cursor_t cursor =
            kan_repository_indexed_value_read_query_execute (&read_root, &first_record_value.object_id);

        struct kan_repository_indexed_value_read_access_t access =
            kan_repository_indexed_value_read_cursor_next (&cursor);

        struct object_record_t *record =
            (struct object_record_t *) kan_repository_indexed_value_read_access_resolve (&access);

        KAN_TEST_ASSERT (record)
        KAN_TEST_CHECK (record->object_id == first_record_value.object_id)
        KAN_TEST_CHECK (record->parent_object_id == first_record_value.parent_object_id)
        KAN_TEST_CHECK (record->data_x == first_record_value.data_x)
        KAN_TEST_CHECK (record->data_y == first_record_value.data_y)

        kan_repository_indexed_value_read_access_close (&access);
        access = kan_repository_indexed_value_read_cursor_next (&cursor);
        KAN_TEST_CHECK (!kan_repository_indexed_value_read_access_resolve (&access))
        kan_repository_indexed_value_read_cursor_close (&cursor);
    }

    const uint32_t id_4u = 4u;

    {
        struct kan_repository_indexed_value_read_cursor_t cursor =
            kan_repository_indexed_value_read_query_execute (&read_root, &id_4u);

        struct kan_repository_indexed_value_read_access_t access =
            kan_repository_indexed_value_read_cursor_next (&cursor);

        struct object_record_t *record =
            (struct object_record_t *) kan_repository_indexed_value_read_access_resolve (&access);

        KAN_TEST_CHECK (!record)
        kan_repository_indexed_value_read_cursor_close (&cursor);
    }

    {
        struct kan_repository_indexed_value_update_cursor_t cursor =
            kan_repository_indexed_value_update_query_execute (&update_child, &second_record_value.object_id);

        struct kan_repository_indexed_value_update_access_t access =
            kan_repository_indexed_value_update_cursor_next (&cursor);

        struct object_record_t *record =
            (struct object_record_t *) kan_repository_indexed_value_update_access_resolve (&access);

        KAN_TEST_ASSERT (record)
        KAN_TEST_CHECK (record->object_id == second_record_value.object_id)
        KAN_TEST_CHECK (record->parent_object_id == second_record_value.parent_object_id)
        KAN_TEST_CHECK (record->data_x == second_record_value.data_x)
        KAN_TEST_CHECK (record->data_y == second_record_value.data_y)

        record->object_id = id_4u;
        kan_repository_indexed_value_update_access_close (&access);
        access = kan_repository_indexed_value_update_cursor_next (&cursor);
        KAN_TEST_CHECK (!kan_repository_indexed_value_update_access_resolve (&access))
        kan_repository_indexed_value_update_cursor_close (&cursor);
    }

    {
        struct kan_repository_indexed_value_read_cursor_t cursor =
            kan_repository_indexed_value_read_query_execute (&read_root, &id_4u);

        struct kan_repository_indexed_value_read_access_t access =
            kan_repository_indexed_value_read_cursor_next (&cursor);

        KAN_TEST_CHECK (kan_repository_indexed_value_read_access_resolve (&access))
        kan_repository_indexed_value_read_access_close (&access);
        kan_repository_indexed_value_read_cursor_close (&cursor);
    }

    {
        struct kan_repository_indexed_value_delete_cursor_t cursor =
            kan_repository_indexed_value_delete_query_execute (&delete_child, &id_4u);

        struct kan_repository_indexed_value_delete_access_t access =
            kan_repository_indexed_value_delete_cursor_next (&cursor);

        KAN_TEST_CHECK (kan_repository_indexed_value_delete_access_resolve (&access))
        kan_repository_indexed_value_delete_access_delete (&access);
        kan_repository_indexed_value_delete_cursor_close (&cursor);
    }

    {
        struct kan_repository_indexed_value_read_cursor_t cursor =
            kan_repository_indexed_value_read_query_execute (&read_root, &id_4u);

        struct kan_repository_indexed_value_read_access_t access =
            kan_repository_indexed_value_read_cursor_next (&cursor);

        struct object_record_t *record =
            (struct object_record_t *) kan_repository_indexed_value_read_access_resolve (&access);

        KAN_TEST_CHECK (!record)
        kan_repository_indexed_value_read_cursor_close (&cursor);
    }

    insert_multi_component_record (&insert_multi_component,
                                   (struct multi_component_record_t) {.object_id = 1u, .some_data = 11u});
    insert_multi_component_record (&insert_multi_component,
                                   (struct multi_component_record_t) {.object_id = 1u, .some_data = 12u});
    insert_multi_component_record (&insert_multi_component,
                                   (struct multi_component_record_t) {.object_id = 1u, .some_data = 13u});

    {
        struct kan_repository_indexed_value_write_cursor_t cursor =
            kan_repository_indexed_value_write_query_execute (&write_multi_component, &first_record_value.object_id);

        struct kan_repository_indexed_value_write_access_t accesses[3u];
        for (kan_loop_size_t index = 0u; index < 3u; ++index)
        {
            accesses[index] = kan_repository_indexed_value_write_cursor_next (&cursor);
        }

        struct kan_repository_indexed_value_write_access_t last_access =
            kan_repository_indexed_value_write_cursor_next (&cursor);
        KAN_TEST_ASSERT (!kan_repository_indexed_value_write_access_resolve (&last_access))

        kan_repository_indexed_value_write_cursor_close (&cursor);
        uint8_t found_11u = 0u;
        uint8_t found_12u = 0u;
        uint8_t found_13u = 0u;

        for (kan_loop_size_t index = 0u; index < 3u; ++index)
        {
            struct multi_component_record_t *record =
                (struct multi_component_record_t *) kan_repository_indexed_value_write_access_resolve (
                    &accesses[index]);

            switch (record->some_data)
            {
            case 11u:
                ++found_11u;
                kan_repository_indexed_value_write_access_delete (&accesses[index]);
                break;

            case 12u:
                ++found_12u;
                record->some_data = 42u;
                kan_repository_indexed_value_write_access_close (&accesses[index]);
                break;

            case 13u:
                ++found_13u;
                kan_repository_indexed_value_write_access_close (&accesses[index]);
                break;

            default:
                KAN_TEST_CHECK (false);
                kan_repository_indexed_value_write_access_close (&accesses[index]);
                break;
            }
        }

        KAN_TEST_CHECK (found_11u == 1u)
        KAN_TEST_CHECK (found_12u == 1u)
        KAN_TEST_CHECK (found_13u == 1u)
    }

    {
        struct kan_repository_indexed_value_write_cursor_t cursor =
            kan_repository_indexed_value_write_query_execute (&write_multi_component, &first_record_value.object_id);

        struct kan_repository_indexed_value_write_access_t accesses[2u];
        for (kan_loop_size_t index = 0u; index < 2u; ++index)
        {
            accesses[index] = kan_repository_indexed_value_write_cursor_next (&cursor);
        }

        struct kan_repository_indexed_value_write_access_t last_access =
            kan_repository_indexed_value_write_cursor_next (&cursor);
        KAN_TEST_ASSERT (!kan_repository_indexed_value_write_access_resolve (&last_access))

        kan_repository_indexed_value_write_cursor_close (&cursor);
        uint8_t found_42u = 0u;
        uint8_t found_13u = 0u;

        for (kan_loop_size_t index = 0u; index < 2u; ++index)
        {
            struct multi_component_record_t *record =
                (struct multi_component_record_t *) kan_repository_indexed_value_write_access_resolve (
                    &accesses[index]);

            switch (record->some_data)
            {
            case 42u:
                ++found_42u;
                break;

            case 13u:
                ++found_13u;
                break;

            default:
                KAN_TEST_CHECK (false);
                break;
            }

            kan_repository_indexed_value_write_access_close (&accesses[index]);
        }

        KAN_TEST_CHECK (found_42u == 1u)
        KAN_TEST_CHECK (found_13u == 1u)
    }

    kan_repository_enter_planning_mode (root_repository);
    kan_repository_indexed_insert_query_shutdown (&insert_child);
    kan_repository_indexed_value_read_query_shutdown (&read_root);
    kan_repository_indexed_value_update_query_shutdown (&update_child);
    kan_repository_indexed_value_delete_query_shutdown (&delete_child);
    kan_repository_indexed_insert_query_shutdown (&insert_multi_component);
    kan_repository_indexed_value_write_query_shutdown (&write_multi_component);

    kan_repository_destroy (root_repository);
    kan_reflection_registry_destroy (registry);
}

KAN_TEST_CASE (indexed_cascade_deletion)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (repository) (registry);
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_repository) (registry);

    kan_repository_t repository = kan_repository_create_root (KAN_ALLOCATION_GROUP_IGNORE, registry);
    kan_repository_indexed_storage_t storage_object =
        kan_repository_indexed_storage_open (repository, "object_record_t");
    kan_repository_indexed_storage_t storage_status =
        kan_repository_indexed_storage_open (repository, "status_record_t");
    kan_repository_indexed_storage_t storage_multi_component =
        kan_repository_indexed_storage_open (repository, "multi_component_record_t");

    struct kan_repository_indexed_insert_query_t insert_object;
    kan_repository_indexed_insert_query_init (&insert_object, storage_object);

    struct kan_repository_indexed_value_read_query_t read_object;
    kan_repository_indexed_value_read_query_init (
        &read_object, storage_object,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"object_id"}});

    struct kan_repository_indexed_value_delete_query_t delete_object;
    kan_repository_indexed_value_delete_query_init (
        &delete_object, storage_object,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"object_id"}});

    struct kan_repository_indexed_insert_query_t insert_status;
    kan_repository_indexed_insert_query_init (&insert_status, storage_status);

    struct kan_repository_indexed_value_read_query_t read_status;
    kan_repository_indexed_value_read_query_init (
        &read_status, storage_status,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"object_id"}});

    struct kan_repository_indexed_insert_query_t insert_multi_component;
    kan_repository_indexed_insert_query_init (&insert_multi_component, storage_multi_component);

    struct kan_repository_indexed_value_read_query_t read_multi_component;
    kan_repository_indexed_value_read_query_init (
        &read_multi_component, storage_multi_component,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"object_id"}});

    kan_repository_enter_serving_mode (repository);

    insert_object_record (
        &insert_object,
        (struct object_record_t) {
            .object_id = 1u, .parent_object_id = INVALID_PARENT_OBJECT_ID, .data_x = 42u, .data_y = 13u});
    insert_status_record (&insert_status, (struct status_record_t) {.object_id = 1u,
                                                                    .observable_alive = true,
                                                                    .observable_poisoned = false,
                                                                    .observable_stunned = false,
                                                                    .observable_boosted = false});

    insert_object_record (&insert_object, (struct object_record_t) {
                                              .object_id = 2u, .parent_object_id = 1u, .data_x = 11u, .data_y = 19u});
    insert_multi_component_record (&insert_multi_component,
                                   (struct multi_component_record_t) {.object_id = 2u, .some_data = 1u});
    insert_multi_component_record (&insert_multi_component,
                                   (struct multi_component_record_t) {.object_id = 2u, .some_data = 1u});

    insert_object_record (&insert_object, (struct object_record_t) {
                                              .object_id = 3u, .parent_object_id = 2u, .data_x = 14u, .data_y = 21u});
    insert_status_record (&insert_status, (struct status_record_t) {.object_id = 3u,
                                                                    .observable_alive = true,
                                                                    .observable_poisoned = false,
                                                                    .observable_stunned = false,
                                                                    .observable_boosted = false});
    insert_multi_component_record (&insert_multi_component,
                                   (struct multi_component_record_t) {.object_id = 3u, .some_data = 1u});

    insert_object_record (&insert_object, (struct object_record_t) {
                                              .object_id = 4u, .parent_object_id = 1u, .data_x = 14u, .data_y = 21u});
    insert_multi_component_record (&insert_multi_component,
                                   (struct multi_component_record_t) {.object_id = 4u, .some_data = 1u});
    insert_multi_component_record (&insert_multi_component,
                                   (struct multi_component_record_t) {.object_id = 4u, .some_data = 1u});

    insert_object_record (
        &insert_object,
        (struct object_record_t) {
            .object_id = 5u, .parent_object_id = INVALID_PARENT_OBJECT_ID, .data_x = 14u, .data_y = 21u});
    insert_status_record (&insert_status, (struct status_record_t) {.object_id = 5u,
                                                                    .observable_alive = true,
                                                                    .observable_poisoned = false,
                                                                    .observable_stunned = false,
                                                                    .observable_boosted = false});
    insert_multi_component_record (&insert_multi_component,
                                   (struct multi_component_record_t) {.object_id = 5u, .some_data = 1u});

    {
        const uint32_t id_1u = 1u;
        struct kan_repository_indexed_value_delete_cursor_t cursor =
            kan_repository_indexed_value_delete_query_execute (&delete_object, &id_1u);

        struct kan_repository_indexed_value_delete_access_t access =
            kan_repository_indexed_value_delete_cursor_next (&cursor);
        kan_repository_indexed_value_delete_cursor_close (&cursor);

        KAN_TEST_CHECK (kan_repository_indexed_value_delete_access_resolve (&access))
        kan_repository_indexed_value_delete_access_delete (&access);
    }

    check_value_not_exists (&read_object, 1u);
    check_value_not_exists (&read_status, 1u);
    check_value_not_exists (&read_multi_component, 1u);

    check_value_not_exists (&read_object, 2u);
    check_value_not_exists (&read_status, 2u);
    check_value_not_exists (&read_multi_component, 2u);

    check_value_not_exists (&read_object, 3u);
    check_value_not_exists (&read_status, 3u);
    check_value_not_exists (&read_multi_component, 3u);

    check_value_not_exists (&read_object, 4u);
    check_value_not_exists (&read_status, 4u);
    check_value_not_exists (&read_multi_component, 4u);

    check_value_exists_unique (&read_object, 5u);
    check_value_exists_unique (&read_status, 5u);
    check_value_exists_unique (&read_multi_component, 5u);

    kan_repository_enter_planning_mode (repository);
    kan_repository_indexed_insert_query_shutdown (&insert_object);
    kan_repository_indexed_value_read_query_shutdown (&read_object);
    kan_repository_indexed_value_delete_query_shutdown (&delete_object);
    kan_repository_indexed_insert_query_shutdown (&insert_status);
    kan_repository_indexed_value_read_query_shutdown (&read_status);
    kan_repository_indexed_insert_query_shutdown (&insert_multi_component);
    kan_repository_indexed_value_read_query_shutdown (&read_multi_component);

    kan_repository_destroy (repository);
    kan_reflection_registry_destroy (registry);
}

KAN_TEST_CASE (indexed_automatic_events)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (repository) (registry);
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_repository) (registry);

    kan_repository_t repository = kan_repository_create_root (KAN_ALLOCATION_GROUP_IGNORE, registry);
    kan_repository_indexed_storage_t status_storage =
        kan_repository_indexed_storage_open (repository, "status_record_t");

    kan_repository_event_storage_t on_insert_storage =
        kan_repository_event_storage_open (repository, "status_record_on_insert_event_t");

    kan_repository_event_storage_t on_change_storage =
        kan_repository_event_storage_open (repository, "status_record_on_change_event_t");

    kan_repository_event_storage_t on_delete_storage =
        kan_repository_event_storage_open (repository, "status_record_on_delete_event_t");

    struct kan_repository_indexed_insert_query_t insert_status;
    kan_repository_indexed_insert_query_init (&insert_status, status_storage);

    struct kan_repository_indexed_value_write_query_t write_status;
    kan_repository_indexed_value_write_query_init (
        &write_status, status_storage,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"object_id"}});

    struct kan_repository_event_fetch_query_t fetch_on_insert;
    kan_repository_event_fetch_query_init (&fetch_on_insert, on_insert_storage);

    struct kan_repository_event_fetch_query_t fetch_on_change;
    kan_repository_event_fetch_query_init (&fetch_on_change, on_change_storage);

    struct kan_repository_event_fetch_query_t fetch_on_delete;
    kan_repository_event_fetch_query_init (&fetch_on_delete, on_delete_storage);

    kan_repository_enter_serving_mode (repository);
    check_no_event (&fetch_on_insert);
    check_no_event (&fetch_on_change);
    check_no_event (&fetch_on_delete);

    insert_status_record (&insert_status, (struct status_record_t) {.object_id = 1u,
                                                                    .observable_alive = true,
                                                                    .observable_poisoned = false,
                                                                    .observable_stunned = false,
                                                                    .observable_boosted = false});

    insert_status_record (&insert_status, (struct status_record_t) {.object_id = 2u,
                                                                    .observable_alive = false,
                                                                    .observable_poisoned = false,
                                                                    .observable_stunned = false,
                                                                    .observable_boosted = false});

    insert_status_record (&insert_status, (struct status_record_t) {.object_id = 3u,
                                                                    .observable_alive = true,
                                                                    .observable_poisoned = false,
                                                                    .observable_stunned = false,
                                                                    .observable_boosted = false});

    check_status_insert_event (&fetch_on_insert,
                               (struct status_record_on_insert_event_t) {.object_id = 1u, .initially_alive = true});

    check_status_insert_event (&fetch_on_insert,
                               (struct status_record_on_insert_event_t) {.object_id = 2u, .initially_alive = false});

    check_status_insert_event (&fetch_on_insert,
                               (struct status_record_on_insert_event_t) {.object_id = 3u, .initially_alive = true});

    check_no_event (&fetch_on_change);
    check_no_event (&fetch_on_delete);

    const uint32_t id_1u = 1u;

    {
        struct kan_repository_indexed_value_write_cursor_t cursor =
            kan_repository_indexed_value_write_query_execute (&write_status, &id_1u);

        struct kan_repository_indexed_value_write_access_t access =
            kan_repository_indexed_value_write_cursor_next (&cursor);
        kan_repository_indexed_value_write_cursor_close (&cursor);

        struct status_record_t *record =
            (struct status_record_t *) kan_repository_indexed_value_write_access_resolve (&access);
        record->observable_stunned = true;
        record->observable_poisoned = true;
        record->observable_boosted = true;
        record->observable_boosted = false;
        kan_repository_indexed_value_write_access_close (&access);
    }

    check_status_change_event (&fetch_on_change, (struct status_record_on_change_event_t) {.object_id = 1u,
                                                                                           .was_alive = true,
                                                                                           .was_poisoned = false,
                                                                                           .was_stunned = false,
                                                                                           .was_boosted = false,
                                                                                           .now_alive = true,
                                                                                           .now_poisoned = true,
                                                                                           .now_stunned = true,
                                                                                           .now_boosted = false});

    check_no_event (&fetch_on_insert);
    check_no_event (&fetch_on_delete);

    const uint32_t id_2u = 2u;

    {
        struct kan_repository_indexed_value_write_cursor_t cursor =
            kan_repository_indexed_value_write_query_execute (&write_status, &id_2u);

        struct kan_repository_indexed_value_write_access_t access =
            kan_repository_indexed_value_write_cursor_next (&cursor);
        kan_repository_indexed_value_write_cursor_close (&cursor);

        KAN_TEST_ASSERT (kan_repository_indexed_value_write_access_resolve (&access));
        kan_repository_indexed_value_write_access_delete (&access);
    }

    check_no_event (&fetch_on_insert);
    check_no_event (&fetch_on_change);

    check_status_delete_event (&fetch_on_delete,
                               (struct status_record_on_delete_event_t) {.object_id = 2u, .was_alive = false});

    kan_repository_enter_planning_mode (repository);
    kan_repository_indexed_insert_query_shutdown (&insert_status);
    kan_repository_indexed_value_write_query_shutdown (&write_status);
    kan_repository_event_fetch_query_shutdown (&fetch_on_insert);
    kan_repository_event_fetch_query_shutdown (&fetch_on_change);
    kan_repository_event_fetch_query_shutdown (&fetch_on_delete);

    kan_repository_destroy (repository);
    kan_reflection_registry_destroy (registry);
}

KAN_TEST_CASE (on_delete_event_after_destroy)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (repository) (registry);
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_repository) (registry);

    kan_repository_t repository = kan_repository_create_root (KAN_ALLOCATION_GROUP_IGNORE, registry);
    kan_repository_t child_repository = kan_repository_create_child (repository, "child");

    kan_repository_indexed_storage_t status_storage =
        kan_repository_indexed_storage_open (child_repository, "status_record_t");

    kan_repository_event_storage_t on_delete_storage =
        kan_repository_event_storage_open (repository, "status_record_on_delete_event_t");

    struct kan_repository_indexed_insert_query_t insert_status;
    kan_repository_indexed_insert_query_init (&insert_status, status_storage);

    struct kan_repository_event_fetch_query_t fetch_on_delete;
    kan_repository_event_fetch_query_init (&fetch_on_delete, on_delete_storage);

    kan_repository_enter_serving_mode (repository);
    check_no_event (&fetch_on_delete);

    insert_status_record (&insert_status, (struct status_record_t) {.object_id = 1u,
                                                                    .observable_alive = true,
                                                                    .observable_poisoned = false,
                                                                    .observable_stunned = false,
                                                                    .observable_boosted = false});

    insert_status_record (&insert_status, (struct status_record_t) {.object_id = 2u,
                                                                    .observable_alive = false,
                                                                    .observable_poisoned = false,
                                                                    .observable_stunned = false,
                                                                    .observable_boosted = false});

    insert_status_record (&insert_status, (struct status_record_t) {.object_id = 3u,
                                                                    .observable_alive = true,
                                                                    .observable_poisoned = false,
                                                                    .observable_stunned = false,
                                                                    .observable_boosted = false});

    check_no_event (&fetch_on_delete);
    kan_repository_schedule_child_destroy (child_repository);
    kan_repository_enter_planning_mode (repository);
    kan_repository_indexed_insert_query_shutdown (&insert_status);
    kan_repository_enter_serving_mode (repository);

    check_status_delete_event (&fetch_on_delete,
                               (struct status_record_on_delete_event_t) {.object_id = 1u, .was_alive = true});

    check_status_delete_event (&fetch_on_delete,
                               (struct status_record_on_delete_event_t) {.object_id = 2u, .was_alive = false});

    check_status_delete_event (&fetch_on_delete,
                               (struct status_record_on_delete_event_t) {.object_id = 3u, .was_alive = true});

    kan_repository_enter_planning_mode (repository);
    kan_repository_event_fetch_query_shutdown (&fetch_on_delete);

    kan_repository_destroy (repository);
    kan_reflection_registry_destroy (registry);
}

KAN_TEST_CASE (indexed_signal_operations)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (repository) (registry);
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_repository) (registry);

    kan_repository_t root_repository = kan_repository_create_root (KAN_ALLOCATION_GROUP_IGNORE, registry);
    kan_repository_t child_repository = kan_repository_create_child (root_repository, "child");

    kan_repository_indexed_storage_t storage_root =
        kan_repository_indexed_storage_open (root_repository, "status_record_t");
    kan_repository_indexed_storage_t storage_child =
        kan_repository_indexed_storage_open (child_repository, "status_record_t");

    struct kan_repository_indexed_insert_query_t insert_child;
    kan_repository_indexed_insert_query_init (&insert_child, storage_child);

    struct kan_repository_indexed_signal_read_query_t read_alive_root;
    kan_repository_indexed_signal_read_query_init (
        &read_alive_root, storage_root,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"observable_alive"}},
        true);

    struct kan_repository_indexed_signal_update_query_t update_poisoned_child;
    kan_repository_indexed_signal_update_query_init (
        &update_poisoned_child, storage_child,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"observable_poisoned"}},
        true);

    struct kan_repository_indexed_signal_delete_query_t delete_dead_child;
    kan_repository_indexed_signal_delete_query_init (
        &delete_dead_child, storage_child,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"observable_alive"}},
        false);

    struct kan_repository_indexed_signal_write_query_t write_boosted_root;
    kan_repository_indexed_signal_write_query_init (
        &write_boosted_root, storage_child,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"observable_boosted"}},
        true);

    kan_repository_enter_serving_mode (root_repository);

    insert_status_record (&insert_child, (struct status_record_t) {.object_id = 1u,
                                                                   .observable_alive = true,
                                                                   .observable_poisoned = false,
                                                                   .observable_stunned = false,
                                                                   .observable_boosted = true});

    insert_status_record (&insert_child, (struct status_record_t) {.object_id = 2u,
                                                                   .observable_alive = true,
                                                                   .observable_poisoned = true,
                                                                   .observable_stunned = false,
                                                                   .observable_boosted = false});

    insert_status_record (&insert_child, (struct status_record_t) {.object_id = 3u,
                                                                   .observable_alive = false,
                                                                   .observable_poisoned = false,
                                                                   .observable_stunned = false,
                                                                   .observable_boosted = true});

    {
        struct kan_repository_indexed_signal_read_cursor_t cursor =
            kan_repository_indexed_signal_read_query_execute (&read_alive_root);

        struct kan_repository_indexed_signal_read_access_t access =
            kan_repository_indexed_signal_read_cursor_next (&cursor);

        struct status_record_t *record =
            (struct status_record_t *) kan_repository_indexed_signal_read_access_resolve (&access);

        kan_loop_size_t records_found = 0u;
        bool one_found = false;
        bool two_found = false;

        while (record)
        {
            ++records_found;
            if (record->object_id == 1u)
            {
                one_found = true;
                KAN_TEST_CHECK (record->observable_alive)
                KAN_TEST_CHECK (!record->observable_poisoned)
                KAN_TEST_CHECK (!record->observable_stunned)
                KAN_TEST_CHECK (record->observable_boosted)
            }
            else if (record->object_id == 2u)
            {
                two_found = true;
                KAN_TEST_CHECK (record->observable_alive)
                KAN_TEST_CHECK (record->observable_poisoned)
                KAN_TEST_CHECK (!record->observable_stunned)
                KAN_TEST_CHECK (!record->observable_boosted)
            }

            kan_repository_indexed_signal_read_access_close (&access);
            access = kan_repository_indexed_signal_read_cursor_next (&cursor);
            record = (struct status_record_t *) kan_repository_indexed_signal_read_access_resolve (&access);
        }

        kan_repository_indexed_signal_read_cursor_close (&cursor);
        KAN_TEST_CHECK (records_found == 2u)
        KAN_TEST_CHECK (one_found)
        KAN_TEST_CHECK (two_found)
    }

    {
        struct kan_repository_indexed_signal_update_cursor_t cursor =
            kan_repository_indexed_signal_update_query_execute (&update_poisoned_child);

        struct kan_repository_indexed_signal_update_access_t access =
            kan_repository_indexed_signal_update_cursor_next (&cursor);

        struct status_record_t *record =
            (struct status_record_t *) kan_repository_indexed_signal_update_access_resolve (&access);

        KAN_TEST_ASSERT (record)
        KAN_TEST_CHECK (record->object_id == 2u)
        record->observable_alive = false;
        record->observable_poisoned = false;

        kan_repository_indexed_signal_update_access_close (&access);
        access = kan_repository_indexed_signal_update_cursor_next (&cursor);
        KAN_TEST_ASSERT (!kan_repository_indexed_signal_update_access_resolve (&access))
        kan_repository_indexed_signal_update_cursor_close (&cursor);
    }

    {
        struct kan_repository_indexed_signal_delete_cursor_t cursor =
            kan_repository_indexed_signal_delete_query_execute (&delete_dead_child);

        struct kan_repository_indexed_signal_delete_access_t access =
            kan_repository_indexed_signal_delete_cursor_next (&cursor);

        struct status_record_t *record =
            (struct status_record_t *) kan_repository_indexed_signal_delete_access_resolve (&access);

        kan_loop_size_t records_found = 0u;
        bool two_found = false;
        bool three_found = false;

        while (record)
        {
            ++records_found;
            if (record->object_id == 2u)
            {
                two_found = true;
            }
            else if (record->object_id == 3u)
            {
                three_found = true;
            }

            kan_repository_indexed_signal_delete_access_delete (&access);
            access = kan_repository_indexed_signal_delete_cursor_next (&cursor);
            record = (struct status_record_t *) kan_repository_indexed_signal_delete_access_resolve (&access);
        }

        kan_repository_indexed_signal_delete_cursor_close (&cursor);
        KAN_TEST_CHECK (records_found == 2u)
        KAN_TEST_CHECK (two_found)
        KAN_TEST_CHECK (three_found)
    }

    insert_status_record (&insert_child, (struct status_record_t) {.object_id = 4u,
                                                                   .observable_alive = false,
                                                                   .observable_poisoned = false,
                                                                   .observable_stunned = false,
                                                                   .observable_boosted = true});

    {
        struct kan_repository_indexed_signal_write_cursor_t cursor =
            kan_repository_indexed_signal_write_query_execute (&write_boosted_root);

        struct kan_repository_indexed_signal_write_access_t access =
            kan_repository_indexed_signal_write_cursor_next (&cursor);

        struct status_record_t *record =
            (struct status_record_t *) kan_repository_indexed_signal_write_access_resolve (&access);

        kan_loop_size_t records_found = 0u;
        bool one_found = false;
        bool four_found = false;

        while (record)
        {
            ++records_found;
            if (record->object_id == 1u)
            {
                one_found = true;
            }
            else if (record->object_id == 4u)
            {
                four_found = true;
            }

            if (record->observable_alive)
            {
                kan_repository_indexed_signal_write_access_delete (&access);
            }
            else
            {
                record->observable_alive = true;
                kan_repository_indexed_signal_write_access_close (&access);
            }

            access = kan_repository_indexed_signal_write_cursor_next (&cursor);
            record = (struct status_record_t *) kan_repository_indexed_signal_write_access_resolve (&access);
        }

        kan_repository_indexed_signal_write_cursor_close (&cursor);
        KAN_TEST_CHECK (records_found == 2u)
        KAN_TEST_CHECK (one_found)
        KAN_TEST_CHECK (four_found)
    }

    {
        struct kan_repository_indexed_signal_read_cursor_t cursor =
            kan_repository_indexed_signal_read_query_execute (&read_alive_root);

        struct kan_repository_indexed_signal_read_access_t access =
            kan_repository_indexed_signal_read_cursor_next (&cursor);

        struct status_record_t *record =
            (struct status_record_t *) kan_repository_indexed_signal_read_access_resolve (&access);

        KAN_TEST_ASSERT (record)
        KAN_TEST_CHECK (record->object_id == 4u)

        kan_repository_indexed_signal_read_access_close (&access);
        access = kan_repository_indexed_signal_read_cursor_next (&cursor);
        KAN_TEST_ASSERT (!kan_repository_indexed_signal_read_access_resolve (&access))
        kan_repository_indexed_signal_read_cursor_close (&cursor);
    }

    kan_repository_enter_planning_mode (root_repository);
    kan_repository_indexed_insert_query_shutdown (&insert_child);
    kan_repository_indexed_signal_read_query_shutdown (&read_alive_root);
    kan_repository_indexed_signal_update_query_shutdown (&update_poisoned_child);
    kan_repository_indexed_signal_delete_query_shutdown (&delete_dead_child);
    kan_repository_indexed_signal_write_query_shutdown (&write_boosted_root);

    kan_repository_destroy (root_repository);
    kan_reflection_registry_destroy (registry);
}

KAN_TEST_CASE (interval_operations)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (repository) (registry);
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_repository) (registry);

    kan_repository_t root_repository = kan_repository_create_root (KAN_ALLOCATION_GROUP_IGNORE, registry);
    kan_repository_t child_repository = kan_repository_create_child (root_repository, "child");

    kan_repository_indexed_storage_t root_storage =
        kan_repository_indexed_storage_open (root_repository, "object_record_t");
    kan_repository_indexed_storage_t child_storage =
        kan_repository_indexed_storage_open (child_repository, "object_record_t");

    struct kan_repository_indexed_insert_query_t insert_child;
    kan_repository_indexed_insert_query_init (&insert_child, child_storage);

    struct kan_repository_indexed_interval_read_query_t read_x_root;
    kan_repository_indexed_interval_read_query_init (
        &read_x_root, root_storage,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"data_x"}});

    struct kan_repository_indexed_interval_update_query_t update_x_child;
    kan_repository_indexed_interval_update_query_init (
        &update_x_child, child_storage,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"data_x"}});

    struct kan_repository_indexed_interval_delete_query_t delete_y_child;
    kan_repository_indexed_interval_delete_query_init (
        &delete_y_child, child_storage,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"data_y"}});

    struct kan_repository_indexed_interval_write_query_t write_y_root;
    kan_repository_indexed_interval_write_query_init (
        &write_y_root, root_storage,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"data_y"}});

    kan_repository_enter_serving_mode (root_repository);

    for (kan_loop_size_t index = 0u; index < 100u; ++index)
    {
        insert_object_record (
            &insert_child,
            (struct object_record_t) {
                .object_id = index, .parent_object_id = INVALID_PARENT_OBJECT_ID, .data_x = index, .data_y = index});
    }

    // Read all ascending.
    {
        struct kan_repository_indexed_interval_ascending_read_cursor_t read_all_cursor =
            kan_repository_indexed_interval_read_query_execute_ascending (&read_x_root, NULL, NULL);

        for (kan_loop_size_t index = 0u; index < 100u; ++index)
        {
            struct kan_repository_indexed_interval_read_access_t access =
                kan_repository_indexed_interval_ascending_read_cursor_next (&read_all_cursor);

            const struct object_record_t *record =
                (const struct object_record_t *) kan_repository_indexed_interval_read_access_resolve (&access);
            KAN_TEST_ASSERT (record)
            KAN_TEST_CHECK (record->data_x == index)
            kan_repository_indexed_interval_read_access_close (&access);
        }

        struct kan_repository_indexed_interval_read_access_t access =
            kan_repository_indexed_interval_ascending_read_cursor_next (&read_all_cursor);
        KAN_TEST_ASSERT (!kan_repository_indexed_interval_read_access_resolve (&access))
        kan_repository_indexed_interval_ascending_read_cursor_close (&read_all_cursor);
    }

    // Read all descending.
    {
        struct kan_repository_indexed_interval_descending_read_cursor_t read_all_cursor =
            kan_repository_indexed_interval_read_query_execute_descending (&read_x_root, NULL, NULL);

        for (kan_loop_size_t index = 0u; index < 100u; ++index)
        {
            struct kan_repository_indexed_interval_read_access_t access =
                kan_repository_indexed_interval_descending_read_cursor_next (&read_all_cursor);

            const struct object_record_t *record =
                (const struct object_record_t *) kan_repository_indexed_interval_read_access_resolve (&access);
            KAN_TEST_ASSERT (record)
            KAN_TEST_CHECK (record->data_x == 100u - index - 1u)
            kan_repository_indexed_interval_read_access_close (&access);
        }

        struct kan_repository_indexed_interval_read_access_t access =
            kan_repository_indexed_interval_descending_read_cursor_next (&read_all_cursor);
        KAN_TEST_ASSERT (!kan_repository_indexed_interval_read_access_resolve (&access))
        kan_repository_indexed_interval_descending_read_cursor_close (&read_all_cursor);
    }

    // Read interval.
    {
        const uint32_t including_start = 42u;
        const uint32_t including_end = 69u;

        struct kan_repository_indexed_interval_ascending_read_cursor_t read_cursor =
            kan_repository_indexed_interval_read_query_execute_ascending (&read_x_root, &including_start,
                                                                          &including_end);

        for (kan_loop_size_t index = including_start; index <= including_end; ++index)
        {
            struct kan_repository_indexed_interval_read_access_t access =
                kan_repository_indexed_interval_ascending_read_cursor_next (&read_cursor);

            const struct object_record_t *record =
                (const struct object_record_t *) kan_repository_indexed_interval_read_access_resolve (&access);
            KAN_TEST_ASSERT (record)
            KAN_TEST_CHECK (record->data_x == index)
            kan_repository_indexed_interval_read_access_close (&access);
        }

        struct kan_repository_indexed_interval_read_access_t access =
            kan_repository_indexed_interval_ascending_read_cursor_next (&read_cursor);
        KAN_TEST_ASSERT (!kan_repository_indexed_interval_read_access_resolve (&access))
        kan_repository_indexed_interval_ascending_read_cursor_close (&read_cursor);
    }

    // Update everything above 49 to 0..49 to make duplicates.
    {
        const uint32_t including_start = 50u;
        struct kan_repository_indexed_interval_descending_update_cursor_t update_cursor =
            kan_repository_indexed_interval_update_query_execute_descending (&update_x_child, &including_start, NULL);

        for (kan_loop_size_t index = including_start; index < 100u; ++index)
        {
            struct kan_repository_indexed_interval_update_access_t access =
                kan_repository_indexed_interval_descending_update_cursor_next (&update_cursor);

            struct object_record_t *record =
                (struct object_record_t *) kan_repository_indexed_interval_update_access_resolve (&access);
            KAN_TEST_ASSERT (record)
            record->data_x -= 50u;
            kan_repository_indexed_interval_update_access_close (&access);
        }

        struct kan_repository_indexed_interval_update_access_t access =
            kan_repository_indexed_interval_descending_update_cursor_next (&update_cursor);
        KAN_TEST_ASSERT (!kan_repository_indexed_interval_update_access_resolve (&access))
        kan_repository_indexed_interval_descending_update_cursor_close (&update_cursor);
    }

    // Read interval again.
    {
        const uint32_t including_start = 17u;
        const uint32_t including_end = 42u;

        struct kan_repository_indexed_interval_ascending_read_cursor_t read_cursor =
            kan_repository_indexed_interval_read_query_execute_ascending (&read_x_root, &including_start,
                                                                          &including_end);

        for (kan_loop_size_t index = including_start; index <= including_end; ++index)
        {
            struct kan_repository_indexed_interval_read_access_t access =
                kan_repository_indexed_interval_ascending_read_cursor_next (&read_cursor);

            const struct object_record_t *record =
                (const struct object_record_t *) kan_repository_indexed_interval_read_access_resolve (&access);
            KAN_TEST_ASSERT (record)
            KAN_TEST_CHECK (record->data_x == index)
            kan_repository_indexed_interval_read_access_close (&access);

            // We expect two records with the same data.
            access = kan_repository_indexed_interval_ascending_read_cursor_next (&read_cursor);
            record = (const struct object_record_t *) kan_repository_indexed_interval_read_access_resolve (&access);
            KAN_TEST_ASSERT (record)
            KAN_TEST_CHECK (record->data_x == index)
            kan_repository_indexed_interval_read_access_close (&access);
        }

        struct kan_repository_indexed_interval_read_access_t access =
            kan_repository_indexed_interval_ascending_read_cursor_next (&read_cursor);
        KAN_TEST_ASSERT (!kan_repository_indexed_interval_read_access_resolve (&access))
        kan_repository_indexed_interval_ascending_read_cursor_close (&read_cursor);
    }

    // Delete all records with odd value of data_y.
    {
        struct kan_repository_indexed_interval_ascending_delete_cursor_t delete_cursor =
            kan_repository_indexed_interval_delete_query_execute_ascending (&delete_y_child, NULL, NULL);

        while (true)
        {
            struct kan_repository_indexed_interval_delete_access_t access =
                kan_repository_indexed_interval_ascending_delete_cursor_next (&delete_cursor);

            const struct object_record_t *record =
                (const struct object_record_t *) kan_repository_indexed_interval_delete_access_resolve (&access);

            if (!record)
            {
                break;
            }

            if (record->data_y % 2u == 0u)
            {
                kan_repository_indexed_interval_delete_access_close (&access);
            }
            else
            {
                kan_repository_indexed_interval_delete_access_delete (&access);
            }
        }

        kan_repository_indexed_interval_ascending_delete_cursor_close (&delete_cursor);
    }

    // Make all even data_y records odd.
    {
        struct kan_repository_indexed_interval_ascending_write_cursor_t write_cursor =
            kan_repository_indexed_interval_write_query_execute_ascending (&write_y_root, NULL, NULL);

        while (true)
        {
            struct kan_repository_indexed_interval_write_access_t access =
                kan_repository_indexed_interval_ascending_write_cursor_next (&write_cursor);

            struct object_record_t *record =
                (struct object_record_t *) kan_repository_indexed_interval_write_access_resolve (&access);

            if (!record)
            {
                break;
            }

            ++record->data_y;
            KAN_TEST_CHECK (record->data_y % 2u == 1u)
            kan_repository_indexed_interval_write_access_close (&access);
        }

        kan_repository_indexed_interval_ascending_write_cursor_close (&write_cursor);
    }

    // Delete everything with data_y inside [20, 60].
    {
        const uint32_t value_20u = 20u;
        const uint32_t value_60u = 60u;

        struct kan_repository_indexed_interval_descending_write_cursor_t write_cursor =
            kan_repository_indexed_interval_write_query_execute_descending (&write_y_root, &value_20u, &value_60u);

        while (true)
        {
            struct kan_repository_indexed_interval_write_access_t access =
                kan_repository_indexed_interval_descending_write_cursor_next (&write_cursor);

            struct object_record_t *record =
                (struct object_record_t *) kan_repository_indexed_interval_write_access_resolve (&access);

            if (!record)
            {
                break;
            }

            KAN_TEST_CHECK (record->data_y % 2u == 1u)
            KAN_TEST_CHECK (record->data_y > 20u)
            KAN_TEST_CHECK (record->data_y < 60u)
            kan_repository_indexed_interval_write_access_delete (&access);
        }

        kan_repository_indexed_interval_descending_write_cursor_close (&write_cursor);
    }

    // Read all and check that deletion above is done.
    {
        struct kan_repository_indexed_interval_ascending_read_cursor_t read_all_cursor =
            kan_repository_indexed_interval_read_query_execute_ascending (&read_x_root, NULL, NULL);

        while (true)
        {
            struct kan_repository_indexed_interval_read_access_t access =
                kan_repository_indexed_interval_ascending_read_cursor_next (&read_all_cursor);

            struct object_record_t *record =
                (struct object_record_t *) kan_repository_indexed_interval_read_access_resolve (&access);

            if (!record)
            {
                break;
            }

            KAN_TEST_CHECK (record->data_y < 20u || record->data_y > 60u)
            kan_repository_indexed_interval_read_access_close (&access);
        }

        kan_repository_indexed_interval_ascending_read_cursor_close (&read_all_cursor);
    }

    kan_repository_enter_planning_mode (root_repository);
    kan_repository_indexed_insert_query_shutdown (&insert_child);
    kan_repository_indexed_interval_read_query_shutdown (&read_x_root);
    kan_repository_indexed_interval_update_query_shutdown (&update_x_child);
    kan_repository_indexed_interval_delete_query_shutdown (&delete_y_child);
    kan_repository_indexed_interval_write_query_shutdown (&write_y_root);

    kan_repository_destroy (root_repository);
    kan_reflection_registry_destroy (registry);
}

KAN_TEST_CASE (space_operations)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (repository) (registry);
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_repository) (registry);

    kan_repository_t root_repository = kan_repository_create_root (KAN_ALLOCATION_GROUP_IGNORE, registry);
    kan_repository_t child_repository = kan_repository_create_child (root_repository, "child");

    kan_repository_indexed_storage_t root_storage =
        kan_repository_indexed_storage_open (root_repository, "bounding_box_component_record_t");
    kan_repository_indexed_storage_t child_storage =
        kan_repository_indexed_storage_open (child_repository, "bounding_box_component_record_t");

    struct kan_repository_indexed_insert_query_t insert_child;
    kan_repository_indexed_insert_query_init (&insert_child, child_storage);

    struct kan_repository_indexed_space_read_query_t read_root;
    kan_repository_indexed_space_read_query_init (
        &read_root, root_storage,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"min"}},
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"max"}}, -100.0, 100.0,
        2.0);

    struct kan_repository_indexed_space_update_query_t update_child;
    kan_repository_indexed_space_update_query_init (
        &update_child, child_storage,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"min"}},
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"max"}}, -100.0, 100.0,
        2.0);

    struct kan_repository_indexed_space_delete_query_t delete_child;
    kan_repository_indexed_space_delete_query_init (
        &delete_child, child_storage,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"min"}},
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"max"}}, -100.0, 100.0,
        2.0);

    struct kan_repository_indexed_space_write_query_t write_root;
    kan_repository_indexed_space_write_query_init (
        &write_root, root_storage,
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"min"}},
        (struct kan_repository_field_path_t) {.reflection_path_length = 1u, (const char *[]) {"max"}}, -100.0, 100.0,
        2.0);

    kan_repository_enter_serving_mode (root_repository);

    insert_bounding_box_component_record (&insert_child,
                                          (struct bounding_box_component_record_t) {
                                              .object_id = 0u, .min = {10.0f, 8.0f, 4.0f}, .max = {11.0f, 9.0f, 5.0f}});

    insert_bounding_box_component_record (&insert_child,
                                          (struct bounding_box_component_record_t) {
                                              .object_id = 1u, .min = {-2.0f, 1.0f, 0.0f}, .max = {0.0f, 4.0f, 2.0f}});

    insert_bounding_box_component_record (
        &insert_child, (struct bounding_box_component_record_t) {
                           .object_id = 2u, .min = {15.0f, 8.0f, 50.0f}, .max = {19.0f, 11.0f, 60.0f}});

    insert_bounding_box_component_record (
        &insert_child, (struct bounding_box_component_record_t) {
                           .object_id = 3u, .min = {10.0f, 8.0f, -90.0f}, .max = {11.0f, 9.0f, -89.0f}});

    const uint64_t flag_0 = 1u << 0u;
    const uint64_t flag_1 = 1u << 1u;
    const uint64_t flag_2 = 1u << 2u;
    const uint64_t flag_3 = 1u << 3u;

    KAN_TEST_CHECK (query_bounding_box (&read_root, -3.0, -3.0, -3.0, 50.0, 50.0, 50.0) == (flag_0 | flag_1 | flag_2))
    KAN_TEST_CHECK (query_bounding_box (&read_root, 9.0, 7.0, 0.0, 20.0, 10.5, 50.0) == (flag_0 | flag_2))
    KAN_TEST_CHECK (query_bounding_box (&read_root, 9.0, 7.0, 0.0, 20.0, 10.5, 20.0) == flag_0)
    KAN_TEST_CHECK (query_bounding_box (&read_root, 9.0, 7.0, 20.0, 20.0, 10.5, 50.0) == flag_2)
    KAN_TEST_CHECK (query_bounding_box (&read_root, 2.0, 0.0, 0.0, 7.0, 8.5, 7.0) == 0u)

    KAN_TEST_CHECK (query_bounding_box (&read_root, 8.0, 6.0, -90.0, 10.5, 9.0, -89.0) == flag_3)
    KAN_TEST_CHECK (query_bounding_box (&read_root, 8.0, 7.0, -90.0, 9.0, 9.0, -89.0) == 0u)
    KAN_TEST_CHECK (query_bounding_box (&read_root, 10.5, 9.0, -90.0, 10.5, 9.0, -89.0) == flag_3)
    KAN_TEST_CHECK (query_bounding_box (&read_root, 8.0, 7.0, -90.0, 10.5, 8.0, -89.0) == flag_3)
    KAN_TEST_CHECK (query_bounding_box (&read_root, 0.0, 0.0, -90.0, 50.0, 50.0, -89.0) == flag_3)
    KAN_TEST_CHECK (query_bounding_box (&read_root, -3.0, 0.0, -90.0, 00.0, 50.0, -89.0) == 0u)

    KAN_TEST_CHECK (query_ray (&read_root, 7.0, -100.0, -89.5, 2.0, 0.0, 0.0, 100.0) == 0u)
    KAN_TEST_CHECK (query_ray (&read_root, 7.0, 9.0, -89.5, 2.0, 0.0, 0.0, 1.0) == 0u)
    KAN_TEST_CHECK (query_ray (&read_root, 7.0, 9.0, -89.5, 2.0, 0.0, 0.0, 3.0) == flag_3)
    KAN_TEST_CHECK (query_ray (&read_root, 7.0, 8.5, -89.5, 2.0, 0.0, 0.0, 1000.0) == flag_3)
    KAN_TEST_CHECK (query_ray (&read_root, 7.0, 8.0, -89.5, 2.0, 0.0, 0.0, 1000.0) == flag_3)
    KAN_TEST_CHECK (query_ray (&read_root, 10.5, 8.0, -89.5, 2.0, 0.0, 0.0, 1000.0) == flag_3)
    KAN_TEST_CHECK (query_ray (&read_root, 7.0, 7.0, -89.5, 2.0, 0.0, 0.0, 1000.0) == 0u)
    KAN_TEST_CHECK (query_ray (&read_root, 10.5, 0.0, -89.5, 0.0, 2.0, 0.0, 1000.0) == flag_3)
    KAN_TEST_CHECK (query_ray (&read_root, 10.5, 50.0, -89.5, 0.0, 2.0, 0.0, 1000.0) == 0u)
    KAN_TEST_CHECK (query_ray (&read_root, 10.5, 0.0, -89.5, 0.0, -1.0, 0.0, 1000.0) == 0u)
    KAN_TEST_CHECK (query_ray (&read_root, 10.5, 50.0, -89.5, 0.0, -1.0, 0.0, 1000.0) == flag_3)
    KAN_TEST_CHECK (query_ray (&read_root, 7.0, 9.0, -89.5, 2.0, -1.0, 0.0, 1000.0) == 0u)
    KAN_TEST_CHECK (query_ray (&read_root, 9.0, 9.0, -89.5, 2.0, -1.0, 0.0, 1000.0) == flag_3)
    KAN_TEST_CHECK (query_ray (&read_root, 9.0, 9.0, -89.5, 2.0, -1.0, 0.0, 0.45) == 0u)
    KAN_TEST_CHECK (query_ray (&read_root, 9.0, 9.0, -89.5, 2.0, -1.0, 0.0, 0.55) == flag_3)

    KAN_TEST_CHECK (query_ray (&read_root, 7.0, 8.5, 4.5, 2.0, 0.0, 0.0, 1000.0) == flag_0)
    KAN_TEST_CHECK (query_ray (&read_root, 7.0, 8.5, 4.5, 2.0, 0.0, 9.0, 1000.0) == flag_2)
    KAN_TEST_CHECK (query_ray (&read_root, 10.5, 8.5, 4.5, 2.0, 0.0, 20.0, 1000.0) == (flag_0 | flag_2))
    KAN_TEST_CHECK (query_ray (&read_root, 10.5, 8.5, 4.5, 2.0, 0.0, 20.0, 2.0) == flag_0)

    // Delete box with id 2 and check that it no longer appears in queries.
    {
        kan_coordinate_floating_t min[] = {9.0, 7.0, 20.0};
        kan_coordinate_floating_t max[] = {20.0, 10.5, 50.0};

        struct kan_repository_indexed_space_shape_delete_cursor_t cursor =
            kan_repository_indexed_space_delete_query_execute_shape (&delete_child, min, max);

        struct kan_repository_indexed_space_delete_access_t access =
            kan_repository_indexed_space_shape_delete_cursor_next (&cursor);

        const struct bounding_box_component_record_t *record =
            (const struct bounding_box_component_record_t *) kan_repository_indexed_space_delete_access_resolve (
                &access);

        KAN_TEST_ASSERT (record)
        kan_repository_indexed_space_delete_access_delete (&access);
        kan_repository_indexed_space_shape_delete_cursor_close (&cursor);
    }

    KAN_TEST_CHECK (query_bounding_box (&read_root, -3.0, -3.0, -3.0, 50.0, 50.0, 50.0) == (flag_0 | flag_1))
    KAN_TEST_CHECK (query_bounding_box (&read_root, 9.0, 7.0, 0.0, 20.0, 10.5, 50.0) == flag_0)
    KAN_TEST_CHECK (query_bounding_box (&read_root, 9.0, 7.0, 20.0, 20.0, 10.5, 50.0) == 0u)
    KAN_TEST_CHECK (query_ray (&read_root, 7.0, 8.5, 4.5, 2.0, 0.0, 9.0, 1000.0) == 0u)
    KAN_TEST_CHECK (query_ray (&read_root, 10.5, 8.5, 4.5, 2.0, 0.0, 20.0, 1000.0) == flag_0)

    // Edit box with id 0 in order to make it equal to previously deleted box with id 2.
    {
        kan_coordinate_floating_t origin[] = {10.5, 8.5, 4.5};
        kan_coordinate_floating_t direction[] = {2.0, 0.0, 20.0};

        struct kan_repository_indexed_space_ray_write_cursor_t cursor =
            kan_repository_indexed_space_write_query_execute_ray (&write_root, origin, direction, 1000.0);

        struct kan_repository_indexed_space_write_access_t access =
            kan_repository_indexed_space_ray_write_cursor_next (&cursor);

        struct bounding_box_component_record_t *record =
            (struct bounding_box_component_record_t *) kan_repository_indexed_space_write_access_resolve (&access);

        KAN_TEST_ASSERT (record)
        record->object_id = 2u;
        record->min[0u] = 15.0f;
        record->min[1u] = 8.0f;
        record->min[2u] = 50.0f;
        record->max[0u] = 19.0f;
        record->max[1u] = 11.0f;
        record->max[2u] = 60.0f;

        kan_repository_indexed_space_write_access_close (&access);
        kan_repository_indexed_space_ray_write_cursor_close (&cursor);
    }

    KAN_TEST_CHECK (query_bounding_box (&read_root, -3.0, -3.0, -3.0, 50.0, 50.0, 50.0) == (flag_1 | flag_2))
    KAN_TEST_CHECK (query_bounding_box (&read_root, 9.0, 7.0, 0.0, 20.0, 10.5, 50.0) == flag_2)
    KAN_TEST_CHECK (query_bounding_box (&read_root, 9.0, 7.0, 20.0, 20.0, 10.5, 50.0) == flag_2)
    KAN_TEST_CHECK (query_ray (&read_root, 7.0, 8.5, 4.5, 2.0, 0.0, 9.0, 1000.0) == flag_2)
    KAN_TEST_CHECK (query_ray (&read_root, 10.5, 8.5, 4.5, 2.0, 0.0, 20.0, 1000.0) == flag_2)
    KAN_TEST_CHECK (query_ray (&read_root, 10.5, 8.5, 4.5, 2.0, 0.0, 20.0, 2.0) == 0u)

    kan_repository_enter_planning_mode (root_repository);
    kan_repository_indexed_insert_query_shutdown (&insert_child);
    kan_repository_indexed_space_read_query_shutdown (&read_root);
    kan_repository_indexed_space_update_query_shutdown (&update_child);
    kan_repository_indexed_space_delete_query_shutdown (&delete_child);
    kan_repository_indexed_space_write_query_shutdown (&write_root);

    kan_repository_destroy (root_repository);
    kan_reflection_registry_destroy (registry);
}
