#include <test_serialization_api.h>

#include <stddef.h>

#include <kan/api_common/min_max.h>
#include <kan/container/dynamic_array.h>
#include <kan/file_system/stream.h>
#include <kan/reflection/generated_reflection.h>
#include <kan/reflection/patch.h>
#include <kan/serialization/binary.h>
#include <kan/serialization/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>
#include <kan/testing/testing.h>

struct float_vector_3_t
{
    float x;
    float y;
    float z;
};

TEST_SERIALIZATION_API void float_vector_3_init (struct float_vector_3_t *instance)
{
    instance->x = 0.0f;
    instance->y = 0.0f;
    instance->z = 0.0f;
}

struct quaternion_t
{
    float x;
    float y;
    float z;
    float w;
};

TEST_SERIALIZATION_API void quaternion_init (struct quaternion_t *instance)
{
    instance->x = 0.0f;
    instance->y = 0.0f;
    instance->z = 0.0f;
    instance->w = 1.0f;
}

struct map_object_t
{
    uint64_t id;

    struct float_vector_3_t position;
    struct quaternion_t rotation;
    struct float_vector_3_t scale;

    kan_interned_string_t object_prototype;

    /// \meta reflection_dynamic_array_type = "kan_reflection_patch_t"
    struct kan_dynamic_array_t components;
};

TEST_SERIALIZATION_API void map_object_init (struct map_object_t *instance)
{
    instance->id = 0u;

    float_vector_3_init (&instance->position);
    quaternion_init (&instance->rotation);
    float_vector_3_init (&instance->scale);

    instance->object_prototype = NULL;

    kan_dynamic_array_init (&instance->components, 2u, sizeof (kan_reflection_patch_t),
                            _Alignof (kan_reflection_patch_t), KAN_ALLOCATION_GROUP_IGNORE);
}

TEST_SERIALIZATION_API void map_object_shutdown (struct map_object_t *instance)
{
    for (uint64_t index = 0u; index < instance->components.size; ++index)
    {
        kan_reflection_patch_destroy (((kan_reflection_patch_t *) instance->components.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->components);
}

struct first_component_t
{
    struct float_vector_3_t vector_1;
    struct float_vector_3_t vector_2;
};

TEST_SERIALIZATION_API void first_component_init (struct first_component_t *instance)
{
    float_vector_3_init (&instance->vector_1);
    float_vector_3_init (&instance->vector_2);
}

struct second_component_t
{
    kan_interned_string_t id_1;
    struct float_vector_3_t vector;
    kan_interned_string_t id_2;
};

TEST_SERIALIZATION_API void second_component_init (struct second_component_t *instance)
{
    instance->id_1 = NULL;
    float_vector_3_init (&instance->vector);
    instance->id_2 = NULL;
}

struct map_t
{
    kan_interned_string_t name;

    /// \meta reflection_dynamic_array_type = "struct map_object_t"
    struct kan_dynamic_array_t objects;
};

TEST_SERIALIZATION_API void map_init (struct map_t *instance)
{
    instance->name = NULL;
    kan_dynamic_array_init (&instance->objects, 16u, sizeof (struct map_object_t), _Alignof (struct map_object_t),
                            KAN_ALLOCATION_GROUP_IGNORE);
}

TEST_SERIALIZATION_API void map_shutdown (struct map_t *instance)
{
    for (uint64_t index = 0u; index < instance->objects.size; ++index)
    {
        map_object_shutdown (&((struct map_object_t *) instance->objects.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->objects);
}

static void fill_test_map (struct map_t *map, kan_reflection_registry_t registry)
{
    kan_reflection_patch_builder_t patch_builder = kan_reflection_patch_builder_create ();
    const kan_interned_string_t object_a = kan_string_intern ("ObjectA");
    const kan_interned_string_t object_b = kan_string_intern ("ObjectB");
    const kan_interned_string_t first_component = kan_string_intern ("first_component_t");
    const kan_interned_string_t second_component = kan_string_intern ("second_component_t");
    const kan_interned_string_t data_1 = kan_string_intern ("data_1");
    const kan_interned_string_t data_2 = kan_string_intern ("data_2");
    const kan_interned_string_t data_a = kan_string_intern ("data_a");
    const kan_interned_string_t data_b = kan_string_intern ("data_b");

#define TEST_MAP_OBJECTS 64u
    map->name = kan_string_intern ("HelloWorldMap");
    kan_dynamic_array_set_capacity (&map->objects, TEST_MAP_OBJECTS);

    for (uint64_t index = 0u; index < TEST_MAP_OBJECTS; ++index)
    {
        struct map_object_t *object = kan_dynamic_array_add_last (&map->objects);
        KAN_TEST_ASSERT (object)
        map_object_init (object);

        object->id = index + 1u;
        object->position.x = (float) (index % 10u);
        object->position.y = (float) (index / 10u);
        object->position.z = 1.0f;

        object->scale.x = 1.0f;
        object->scale.y = 1.0f;
        object->scale.z = 1.0f;

        object->object_prototype = index % 2u ? object_a : object_b;

        switch (index % 3u)
        {
        case 0u:
        {
            struct first_component_t component = {
                .vector_1 = {2.0f, 1.0f, 10.0f},
                .vector_2 = {3.0f, 4.0f, 5.0f},
            };

            kan_reflection_patch_builder_add_chunk (patch_builder, 0u, sizeof (struct first_component_t), &component);
            kan_reflection_patch_t *patch = kan_dynamic_array_add_last (&object->components);
            KAN_TEST_ASSERT (patch)

            *patch = kan_reflection_patch_builder_build (
                patch_builder, registry, kan_reflection_registry_query_struct (registry, first_component));
            break;
        }

        case 1u:
        {
            struct second_component_t component = {
                .id_1 = data_1,
                .vector = {110.0f, 111.0f, 112.0f},
                .id_2 = data_2,
            };

            kan_reflection_patch_builder_add_chunk (patch_builder, 0u, sizeof (struct second_component_t), &component);
            kan_reflection_patch_t *patch = kan_dynamic_array_add_last (&object->components);
            KAN_TEST_ASSERT (patch)

            *patch = kan_reflection_patch_builder_build (
                patch_builder, registry, kan_reflection_registry_query_struct (registry, second_component));
            break;
        }

        case 2u:
        {
            struct first_component_t component_1 = {
                .vector_1 = {17.0f, -5.0f, -3.0f},
                .vector_2 = {44.0f, 13.0f, 19.0f},
            };

            kan_reflection_patch_builder_add_chunk (patch_builder, 0u, sizeof (struct first_component_t), &component_1);
            kan_reflection_patch_t *patch = kan_dynamic_array_add_last (&object->components);
            KAN_TEST_ASSERT (patch)

            *patch = kan_reflection_patch_builder_build (
                patch_builder, registry, kan_reflection_registry_query_struct (registry, first_component));

            struct second_component_t component_2 = {
                .id_1 = data_a,
                .vector = {89.0f, 1116.0f, 21.0f},
                .id_2 = data_b,
            };

            kan_reflection_patch_builder_add_chunk (patch_builder, 0u, sizeof (struct second_component_t),
                                                    &component_2);
            patch = kan_dynamic_array_add_last (&object->components);
            KAN_TEST_ASSERT (patch)

            *patch = kan_reflection_patch_builder_build (
                patch_builder, registry, kan_reflection_registry_query_struct (registry, second_component));
            break;
        }
        }
    }

#undef TEST_MAP_OBJECTS
    kan_reflection_patch_builder_destroy (patch_builder);
}

static void check_map_equality (struct map_t *source_map, struct map_t *deserialized_map)
{
    const kan_interned_string_t first_component_t = kan_string_intern ("first_component_t");
    const kan_interned_string_t second_component_t = kan_string_intern ("second_component_t");

    KAN_TEST_CHECK (source_map->name == deserialized_map->name)
    KAN_TEST_CHECK (source_map->objects.size == deserialized_map->objects.size)
    const uint64_t objects_to_check = KAN_MIN (source_map->objects.size, deserialized_map->objects.size);

    for (uint64_t index = 0u; index < objects_to_check; ++index)
    {
        struct map_object_t *source_object = ((struct map_object_t *) source_map->objects.data) + index;
        struct map_object_t *deserialized_object = ((struct map_object_t *) deserialized_map->objects.data) + index;

        KAN_TEST_CHECK (source_object->id == deserialized_object->id)
        KAN_TEST_CHECK (source_object->position.x == deserialized_object->position.x)
        KAN_TEST_CHECK (source_object->position.y == deserialized_object->position.y)
        KAN_TEST_CHECK (source_object->position.z == deserialized_object->position.z)

        KAN_TEST_CHECK (source_object->rotation.x == deserialized_object->rotation.x)
        KAN_TEST_CHECK (source_object->rotation.y == deserialized_object->rotation.y)
        KAN_TEST_CHECK (source_object->rotation.z == deserialized_object->rotation.z)
        KAN_TEST_CHECK (source_object->rotation.w == deserialized_object->rotation.w)

        KAN_TEST_CHECK (source_object->scale.x == deserialized_object->scale.x)
        KAN_TEST_CHECK (source_object->scale.y == deserialized_object->scale.y)
        KAN_TEST_CHECK (source_object->scale.z == deserialized_object->scale.z)

        KAN_TEST_CHECK (source_object->object_prototype == deserialized_object->object_prototype)
        KAN_TEST_CHECK (source_object->components.size == deserialized_object->components.size)
        const uint64_t components_to_check =
            KAN_MIN (source_object->components.size, deserialized_object->components.size);

        for (uint64_t component_index = 0u; component_index < components_to_check; ++component_index)
        {
            kan_reflection_patch_t source_component =
                ((kan_reflection_patch_t *) source_object->components.data)[component_index];
            kan_reflection_patch_t deserialized_component =
                ((kan_reflection_patch_t *) deserialized_object->components.data)[component_index];

            KAN_TEST_CHECK (kan_reflection_patch_get_type (source_component) ==
                            kan_reflection_patch_get_type (deserialized_component))

            if (kan_reflection_patch_get_type (source_component)->name == first_component_t)
            {
                struct first_component_t source_component_data;
                first_component_init (&source_component_data);
                kan_reflection_patch_apply (source_component, &source_component_data);

                struct first_component_t deserialized_component_data;
                first_component_init (&deserialized_component_data);
                kan_reflection_patch_apply (deserialized_component, &deserialized_component_data);

                KAN_TEST_CHECK (source_component_data.vector_1.x == deserialized_component_data.vector_1.x)
                KAN_TEST_CHECK (source_component_data.vector_1.y == deserialized_component_data.vector_1.y)
                KAN_TEST_CHECK (source_component_data.vector_1.z == deserialized_component_data.vector_1.z)

                KAN_TEST_CHECK (source_component_data.vector_2.x == deserialized_component_data.vector_2.x)
                KAN_TEST_CHECK (source_component_data.vector_2.y == deserialized_component_data.vector_2.y)
                KAN_TEST_CHECK (source_component_data.vector_2.z == deserialized_component_data.vector_2.z)
            }
            else if (kan_reflection_patch_get_type (source_component)->name == second_component_t)
            {
                struct second_component_t source_component_data;
                second_component_init (&source_component_data);
                kan_reflection_patch_apply (source_component, &source_component_data);

                struct second_component_t deserialized_component_data;
                second_component_init (&deserialized_component_data);
                kan_reflection_patch_apply (deserialized_component, &deserialized_component_data);

                KAN_TEST_CHECK (source_component_data.id_1 == deserialized_component_data.id_1)
                KAN_TEST_CHECK (source_component_data.vector.x == deserialized_component_data.vector.x)
                KAN_TEST_CHECK (source_component_data.vector.y == deserialized_component_data.vector.y)
                KAN_TEST_CHECK (source_component_data.vector.z == deserialized_component_data.vector.z)
                KAN_TEST_CHECK (source_component_data.id_2 == deserialized_component_data.id_2)
            }
            else
            {
                KAN_TEST_CHECK (KAN_FALSE)
            }
        }
    }
}

static void save_map_binary (struct map_t *map,
                             kan_serialization_binary_script_storage_t script_storage,
                             kan_serialization_interned_string_registry_t string_registry)
{
    const kan_interned_string_t map_t = kan_string_intern ("map_t");
    struct kan_stream_t *direct_file_stream = kan_direct_file_stream_open_for_write ("map.bin", KAN_TRUE);
    struct kan_stream_t *buffered_file_stream =
        kan_random_access_stream_buffer_open_for_write (direct_file_stream, 1024u);

    KAN_TEST_CHECK (kan_serialization_binary_write_type_header (buffered_file_stream, map_t, string_registry))
    kan_serialization_binary_writer_t writer =
        kan_serialization_binary_writer_create (buffered_file_stream, map, map_t, script_storage, string_registry);

    while (KAN_TRUE)
    {
        enum kan_serialization_state_t state = kan_serialization_binary_writer_step (writer);
        KAN_TEST_ASSERT (state != KAN_SERIALIZATION_FAILED)

        if (state == KAN_SERIALIZATION_FINISHED)
        {
            break;
        }
    }

    kan_serialization_binary_writer_destroy (writer);
    kan_random_access_stream_buffer_close (buffered_file_stream);
    kan_direct_file_stream_close (direct_file_stream);
}

static void load_map_binary (struct map_t *map,
                             kan_serialization_binary_script_storage_t script_storage,
                             kan_serialization_interned_string_registry_t string_registry)
{
    const kan_interned_string_t map_t = kan_string_intern ("map_t");
    struct kan_stream_t *direct_file_stream = kan_direct_file_stream_open_for_read ("map.bin", KAN_TRUE);
    struct kan_stream_t *buffered_file_stream =
        kan_random_access_stream_buffer_open_for_read (direct_file_stream, 1024u);

    kan_interned_string_t header_type;
    KAN_TEST_CHECK (kan_serialization_binary_read_type_header (buffered_file_stream, &header_type, string_registry))

    kan_serialization_binary_reader_t reader = kan_serialization_binary_reader_create (
        buffered_file_stream, map, map_t, script_storage, string_registry, KAN_ALLOCATION_GROUP_IGNORE);

    while (KAN_TRUE)
    {
        enum kan_serialization_state_t state = kan_serialization_binary_reader_step (reader);
        KAN_TEST_ASSERT (state != KAN_SERIALIZATION_FAILED)

        if (state == KAN_SERIALIZATION_FINISHED)
        {
            break;
        }
    }

    kan_serialization_binary_reader_destroy (reader);
    kan_random_access_stream_buffer_close (buffered_file_stream);
    kan_direct_file_stream_close (direct_file_stream);
}

static void save_map_rd (struct map_t *map, kan_reflection_registry_t registry)
{
    const kan_interned_string_t map_t = kan_string_intern ("map_t");
    struct kan_stream_t *direct_file_stream = kan_direct_file_stream_open_for_write ("map.rd", KAN_TRUE);
    struct kan_stream_t *buffered_file_stream =
        kan_random_access_stream_buffer_open_for_write (direct_file_stream, 1024u);

    KAN_TEST_CHECK (kan_serialization_rd_write_type_header (buffered_file_stream, map_t))
    kan_serialization_rd_writer_t writer =
        kan_serialization_rd_writer_create (buffered_file_stream, map, map_t, registry);

    while (KAN_TRUE)
    {
        enum kan_serialization_state_t state = kan_serialization_rd_writer_step (writer);
        buffered_file_stream->operations->flush (buffered_file_stream);
        direct_file_stream->operations->flush (direct_file_stream);
        KAN_TEST_ASSERT (state != KAN_SERIALIZATION_FAILED)

        if (state == KAN_SERIALIZATION_FINISHED)
        {
            break;
        }
    }

    kan_serialization_rd_writer_destroy (writer);
    kan_random_access_stream_buffer_close (buffered_file_stream);
    kan_direct_file_stream_close (direct_file_stream);
}

static void load_map_rd (struct map_t *map, kan_reflection_registry_t registry)
{
    const kan_interned_string_t map_t = kan_string_intern ("map_t");
    struct kan_stream_t *direct_file_stream = kan_direct_file_stream_open_for_read ("map.rd", KAN_TRUE);
    struct kan_stream_t *buffered_file_stream =
        kan_random_access_stream_buffer_open_for_read (direct_file_stream, 1024u);

    kan_interned_string_t header_type;
    KAN_TEST_CHECK (kan_serialization_rd_read_type_header (buffered_file_stream, &header_type))

    kan_serialization_rd_reader_t reader =
        kan_serialization_rd_reader_create (buffered_file_stream, map, map_t, registry, KAN_ALLOCATION_GROUP_IGNORE);

    while (KAN_TRUE)
    {
        enum kan_serialization_state_t state = kan_serialization_rd_reader_step (reader);
        KAN_TEST_ASSERT (state != KAN_SERIALIZATION_FAILED)

        if (state == KAN_SERIALIZATION_FINISHED)
        {
            break;
        }
    }

    kan_serialization_rd_reader_destroy (reader);
    kan_random_access_stream_buffer_close (buffered_file_stream);
    kan_direct_file_stream_close (direct_file_stream);
}

// \c_interface_scanner_disable
KAN_REFLECTION_EXPECT_UNIT_REGISTRAR (test_serialization);
// \c_interface_scanner_enable

KAN_TEST_CASE (binary_no_interned_string_registry)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_serialization) (registry);
    kan_serialization_binary_script_storage_t script_storage =
        kan_serialization_binary_script_storage_create (registry);

    struct map_t initial_map;
    map_init (&initial_map);
    fill_test_map (&initial_map, registry);
    save_map_binary (&initial_map, script_storage, KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY);

    struct map_t deserialized_map;
    map_init (&deserialized_map);
    load_map_binary (&deserialized_map, script_storage, KAN_INVALID_SERIALIZATION_INTERNED_STRING_REGISTRY);

    check_map_equality (&initial_map, &deserialized_map);
    map_shutdown (&initial_map);
    map_shutdown (&deserialized_map);

    kan_serialization_binary_script_storage_destroy (script_storage);
    kan_reflection_registry_destroy (registry);
}

KAN_TEST_CASE (binary_with_interned_string_registry)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_serialization) (registry);
    kan_serialization_binary_script_storage_t script_storage =
        kan_serialization_binary_script_storage_create (registry);

    kan_serialization_interned_string_registry_t interned_string_registry_write =
        kan_serialization_interned_string_registry_create_empty ();

    struct map_t initial_map;
    map_init (&initial_map);
    fill_test_map (&initial_map, registry);
    save_map_binary (&initial_map, script_storage, interned_string_registry_write);

    struct kan_stream_t *direct_file_stream = kan_direct_file_stream_open_for_write ("string_registry.bin", KAN_TRUE);
    struct kan_stream_t *buffered_file_stream =
        kan_random_access_stream_buffer_open_for_write (direct_file_stream, 1024u);

    kan_serialization_interned_string_registry_writer_t registry_writer =
        kan_serialization_interned_string_registry_writer_create (buffered_file_stream, interned_string_registry_write);

    while (KAN_TRUE)
    {
        enum kan_serialization_state_t state = kan_serialization_interned_string_registry_writer_step (registry_writer);
        KAN_TEST_ASSERT (state != KAN_SERIALIZATION_FAILED)

        if (state == KAN_SERIALIZATION_FINISHED)
        {
            break;
        }
    }

    kan_serialization_interned_string_registry_writer_destroy (registry_writer);
    kan_random_access_stream_buffer_close (buffered_file_stream);
    kan_direct_file_stream_close (direct_file_stream);

    direct_file_stream = kan_direct_file_stream_open_for_read ("string_registry.bin", KAN_TRUE);
    buffered_file_stream = kan_random_access_stream_buffer_open_for_read (direct_file_stream, 1024u);

    kan_serialization_interned_string_registry_reader_t registry_reader =
        kan_serialization_interned_string_registry_reader_create (buffered_file_stream, KAN_TRUE);

    while (KAN_TRUE)
    {
        enum kan_serialization_state_t state = kan_serialization_interned_string_registry_reader_step (registry_reader);
        KAN_TEST_ASSERT (state != KAN_SERIALIZATION_FAILED)

        if (state == KAN_SERIALIZATION_FINISHED)
        {
            break;
        }
    }

    kan_serialization_interned_string_registry_t interned_string_registry_read =
        kan_serialization_interned_string_registry_reader_get (registry_reader);

    kan_serialization_interned_string_registry_reader_destroy (registry_reader);
    kan_random_access_stream_buffer_close (buffered_file_stream);
    kan_direct_file_stream_close (direct_file_stream);

    struct map_t deserialized_map;
    map_init (&deserialized_map);
    load_map_binary (&deserialized_map, script_storage, interned_string_registry_read);

    check_map_equality (&initial_map, &deserialized_map);
    map_shutdown (&initial_map);
    map_shutdown (&deserialized_map);

    kan_serialization_binary_script_storage_destroy (script_storage);
    kan_reflection_registry_destroy (registry);

    kan_serialization_interned_string_registry_destroy (interned_string_registry_write);
    kan_serialization_interned_string_registry_destroy (interned_string_registry_read);
}

KAN_TEST_CASE (readable_data)
{
    kan_reflection_registry_t registry = kan_reflection_registry_create ();
    KAN_REFLECTION_UNIT_REGISTRAR_NAME (test_serialization) (registry);

    struct map_t initial_map;
    map_init (&initial_map);
    fill_test_map (&initial_map, registry);
    save_map_rd (&initial_map, registry);

    struct map_t deserialized_map;
    map_init (&deserialized_map);
    load_map_rd (&deserialized_map, registry);

    check_map_equality (&initial_map, &deserialized_map);
    map_shutdown (&initial_map);
    map_shutdown (&deserialized_map);
    kan_reflection_registry_destroy (registry);
}
