#include <application_framework_examples_deferred_render_api.h>

#include <stdio.h>
#include <string.h>

#include <kan/context/all_system_names.h>
#include <kan/context/application_framework_system.h>
#include <kan/context/application_system.h>
#include <kan/context/virtual_file_system.h>
#include <kan/file_system/stream.h>
#include <kan/image/image.h>
#include <kan/log/logging.h>
#include <kan/precise_time/precise_time.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe/universe.h>
#include <kan/universe_render_foundation/material.h>
#include <kan/universe_render_foundation/material_instance.h>
#include <kan/universe_render_foundation/render_graph.h>
#include <kan/universe_render_foundation/texture.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>
#include <kan/universe_time/universe_time.h>

KAN_LOG_DEFINE_CATEGORY (application_framework_example_deferred_render);

struct deferred_render_config_t
{
    kan_interned_string_t ground_material_instance_name;
    kan_interned_string_t cube_material_instance_name;
    kan_interned_string_t ambient_light_material_name;
    kan_interned_string_t directional_light_material_name;
    kan_interned_string_t point_light_material_name;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t test_expectations;
};

APPLICATION_FRAMEWORK_EXAMPLES_DEFERRED_RENDER_API void deferred_render_config_init (
    struct deferred_render_config_t *instance)
{
    instance->ground_material_instance_name = NULL;
    instance->cube_material_instance_name = NULL;
    instance->ambient_light_material_name = NULL;
    instance->directional_light_material_name = NULL;
    instance->point_light_material_name = NULL;

    kan_dynamic_array_init (&instance->test_expectations, 0u, sizeof (kan_interned_string_t),
                            _Alignof (kan_interned_string_t), kan_allocation_group_stack_get ());
}

APPLICATION_FRAMEWORK_EXAMPLES_DEFERRED_RENDER_API void deferred_render_config_shutdown (
    struct deferred_render_config_t *instance)
{
    kan_dynamic_array_shutdown (&instance->test_expectations);
}

KAN_REFLECTION_STRUCT_META (deferred_render_config_t)
APPLICATION_FRAMEWORK_EXAMPLES_DEFERRED_RENDER_API struct kan_resource_resource_type_meta_t
    deferred_render_config_resource_type_meta = {
        .root = KAN_TRUE,
};

KAN_REFLECTION_STRUCT_FIELD_META (deferred_render_config_t, ground_material_instance_name)
APPLICATION_FRAMEWORK_EXAMPLES_DEFERRED_RENDER_API struct kan_resource_reference_meta_t
    deferred_render_config_ground_material_instance_name_reference_meta = {
        .type = "kan_resource_material_instance_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

KAN_REFLECTION_STRUCT_FIELD_META (deferred_render_config_t, cube_material_instance_name)
APPLICATION_FRAMEWORK_EXAMPLES_DEFERRED_RENDER_API struct kan_resource_reference_meta_t
    deferred_render_config_cube_material_instance_name_reference_meta = {
        .type = "kan_resource_material_instance_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

KAN_REFLECTION_STRUCT_FIELD_META (deferred_render_config_t, ambient_light_material_name)
APPLICATION_FRAMEWORK_EXAMPLES_DEFERRED_RENDER_API struct kan_resource_reference_meta_t
    deferred_render_config_ambient_light_material_instance_name_reference_meta = {
        .type = "kan_resource_material_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

KAN_REFLECTION_STRUCT_FIELD_META (deferred_render_config_t, directional_light_material_name)
APPLICATION_FRAMEWORK_EXAMPLES_DEFERRED_RENDER_API struct kan_resource_reference_meta_t
    deferred_render_config_directional_light_material_instance_name_reference_meta = {
        .type = "kan_resource_material_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

KAN_REFLECTION_STRUCT_FIELD_META (deferred_render_config_t, point_light_material_name)
APPLICATION_FRAMEWORK_EXAMPLES_DEFERRED_RENDER_API struct kan_resource_reference_meta_t
    deferred_render_config_point_light_material_instance_name_reference_meta = {
        .type = "kan_resource_material_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

KAN_REFLECTION_STRUCT_FIELD_META (deferred_render_config_t, test_expectations)
APPLICATION_FRAMEWORK_EXAMPLES_DEFERRED_RENDER_API struct kan_resource_reference_meta_t
    deferred_render_config_test_expectations_reference_meta = {
        .type = NULL,
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

struct deferred_render_full_screen_quad_vertex_t
{
    struct kan_float_vector_2_t position;
};

struct deferred_render_vertex_t
{
    struct kan_float_vector_3_t position;
    struct kan_float_vector_3_t normal;
    struct kan_float_vector_2_t uv;
};

struct deferred_scene_view_parameters_t
{
    struct kan_float_matrix_4x4_t projection_view;
    struct kan_float_vector_4_t camera_position;
    struct kan_float_vector_4_t camera_forward;
    struct kan_float_vector_4_t camera_right;
    struct kan_float_vector_4_t camera_up;
};

struct deferred_scene_directional_light_push_constant_t
{
    struct kan_float_matrix_4x4_t shadow_map_projection_view;
    struct kan_float_vector_4_t color;
    struct kan_float_vector_4_t direction;
};

struct deferred_scene_point_light_instanced_t
{
    struct kan_float_vector_4_t position_and_distance;
    struct kan_float_vector_3_t color;
    int32_t shadow_map_index;
};

struct deferred_render_scene_view_data_t
{
    kan_render_pipeline_parameter_set_t g_buffer_parameter_set;
    kan_render_pipeline_parameter_set_t lighting_parameter_set;
    kan_render_buffer_t view_parameters_buffer;
};

void deferred_render_scene_view_data_init (struct deferred_render_scene_view_data_t *instance)
{
    instance->g_buffer_parameter_set = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_t);
    instance->lighting_parameter_set = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_t);
    instance->view_parameters_buffer = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
}

void deferred_render_scene_view_data_shutdown (struct deferred_render_scene_view_data_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->g_buffer_parameter_set))
    {
        kan_render_pipeline_parameter_set_destroy (instance->g_buffer_parameter_set);
    }

    if (KAN_HANDLE_IS_VALID (instance->lighting_parameter_set))
    {
        kan_render_pipeline_parameter_set_destroy (instance->lighting_parameter_set);
    }

    if (KAN_HANDLE_IS_VALID (instance->view_parameters_buffer))
    {
        kan_render_buffer_destroy (instance->view_parameters_buffer);
    }
}

struct deferred_render_shadow_pass_data_t
{
    kan_render_pipeline_parameter_set_t parameter_set;
    kan_render_buffer_t parameters_buffer;
};

void deferred_render_shadow_pass_data_init (struct deferred_render_shadow_pass_data_t *instance)
{
    instance->parameter_set = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_t);
    instance->parameters_buffer = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
}

void deferred_render_shadow_pass_data_shutdown (struct deferred_render_shadow_pass_data_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->parameter_set))
    {
        kan_render_pipeline_parameter_set_destroy (instance->parameter_set);
    }

    if (KAN_HANDLE_IS_VALID (instance->parameters_buffer))
    {
        kan_render_buffer_destroy (instance->parameters_buffer);
    }
}

#define SPLIT_SCREEN_VIEWS 2u
#define POINT_LIGHTS_WITH_SHADOWS 3u
#define POINT_LIGHTS_WITH_SHADOWS_DISTANCE 10.0f
#define POINT_LIGHTS_SHADOW_PASS_COUNT (POINT_LIGHTS_WITH_SHADOWS * DEFERRED_RENDER_CUBE_SHADOW_FRAME_BUFFER_COUNT)

#define DIRECTIONAL_SHADOW_MAP_WIDTH 1024u
#define DIRECTIONAL_SHADOW_MAP_HEIGHT 1024u
#define POINT_SHADOW_MAP_WIDTH 256u
#define POINT_SHADOW_MAP_HEIGHT 256u

#define BOXES_X 40
#define BOXES_Y 40

#define WORLD_HALF_WIDTH 40.0f
#define WORLD_HALF_HEIGHT 40.0f

#define FIXED_TEST_WIDTH 1600u
#define FIXED_TEST_HEIGHT 800u

enum deferred_render_scene_image_t
{
    DEFERRED_RENDER_SCENE_IMAGE_POSITION = 0u,
    DEFERRED_RENDER_SCENE_IMAGE_NORMAL_SPECULAR,
    DEFERRED_RENDER_SCENE_IMAGE_ALBEDO,
    DEFERRED_RENDER_SCENE_IMAGE_DEPTH,
    DEFERRED_RENDER_SCENE_IMAGE_VIEW_COLOR,
    DEFERRED_RENDER_SCENE_IMAGE_COUNT,
};

enum deferred_render_scene_frame_buffer_t
{
    DEFERRED_RENDER_SCENE_FRAME_BUFFER_G_BUFFER = 0u,
    DEFERRED_RENDER_SCENE_FRAME_BUFFER_LIGHTING,
    DEFERRED_RENDER_SCENE_FRAME_BUFFER_COUNT,
};

enum deferred_render_shadow_image_t
{
    DEFERRED_RENDER_SHADOW_IMAGE_DEPTH = 0u,
    DEFERRED_RENDER_SHADOW_IMAGE_COUNT,
};

enum deferred_render_flat_shadow_frame_buffer_t
{
    DEFERRED_RENDER_FLAT_SHADOW_FRAME_BUFFER_DEPTH = 0u,
    DEFERRED_RENDER_FLAT_SHADOW_FRAME_BUFFER_COUNT,
};

enum deferred_render_cube_shadow_frame_buffer_t
{
    DEFERRED_RENDER_CUBE_SHADOW_FRAME_BUFFER_RIGHT = 0u,
    DEFERRED_RENDER_CUBE_SHADOW_FRAME_BUFFER_LEFT,
    DEFERRED_RENDER_CUBE_SHADOW_FRAME_BUFFER_UP,
    DEFERRED_RENDER_CUBE_SHADOW_FRAME_BUFFER_DOWN,
    DEFERRED_RENDER_CUBE_SHADOW_FRAME_BUFFER_FORWARD,
    DEFERRED_RENDER_CUBE_SHADOW_FRAME_BUFFER_BACK,
    DEFERRED_RENDER_CUBE_SHADOW_FRAME_BUFFER_COUNT,
};

struct example_deferred_render_singleton_t
{
    kan_application_system_window_t window_handle;
    kan_render_surface_t window_surface;
    kan_instance_size_t test_frames_count;
    kan_resource_request_id_t config_request_id;
    kan_render_material_instance_usage_id_t ground_material_instance_usage_id;
    kan_render_material_instance_usage_id_t cube_material_instance_usage_id;
    kan_render_material_usage_id_t ambient_light_material_usage_id;
    kan_render_material_usage_id_t directional_light_material_usage_id;
    kan_render_material_usage_id_t point_light_material_usage_id;
    kan_resource_request_id_t test_expectation_requests[SPLIT_SCREEN_VIEWS];
    kan_bool_t frame_checked;

    kan_render_buffer_t full_screen_quad_vertex_buffer;
    kan_render_buffer_t full_screen_quad_index_buffer;
    kan_render_size_t full_screen_quad_index_count;

    kan_bool_t object_buffers_initialized;
    kan_render_buffer_t ground_vertex_buffer;
    kan_render_buffer_t ground_index_buffer;
    kan_render_size_t ground_index_count;

    kan_render_buffer_t cube_vertex_buffer;
    kan_render_buffer_t cube_index_buffer;
    kan_render_size_t cube_index_count;

    kan_render_buffer_t test_read_back_buffer;
    kan_render_read_back_status_t test_read_back_statuses[SPLIT_SCREEN_VIEWS];

    kan_render_frame_lifetime_buffer_allocator_t instanced_data_allocator;
    struct deferred_render_scene_view_data_t scene_view[SPLIT_SCREEN_VIEWS];

    struct deferred_render_shadow_pass_data_t directional_light_shadow_pass;
    struct deferred_render_shadow_pass_data_t point_light_shadow_passes[POINT_LIGHTS_SHADOW_PASS_COUNT];

    kan_render_pipeline_parameter_set_t directional_light_object_parameter_set;
    kan_render_pipeline_parameter_set_t point_light_shared_parameter_set;
    struct kan_float_matrix_4x4_t box_transform_matrices[BOXES_X * BOXES_Y];

    struct kan_float_vector_3_t directional_light_direction;
    struct kan_float_matrix_4x4_t directional_light_shadow_projection_view;

    struct kan_float_vector_3_t point_lights_with_shadows_positions[POINT_LIGHTS_WITH_SHADOWS];
};

APPLICATION_FRAMEWORK_EXAMPLES_DEFERRED_RENDER_API void example_deferred_render_singleton_init (
    struct example_deferred_render_singleton_t *instance)
{
    instance->window_handle = KAN_HANDLE_SET_INVALID (kan_application_system_window_t);
    instance->window_surface = KAN_HANDLE_SET_INVALID (kan_render_surface_t);
    instance->test_frames_count = 0u;
    instance->config_request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
    instance->ground_material_instance_usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_render_material_instance_usage_id_t);
    instance->cube_material_instance_usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_render_material_instance_usage_id_t);
    instance->ambient_light_material_usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_render_material_usage_id_t);
    instance->directional_light_material_usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_render_material_usage_id_t);
    instance->point_light_material_usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_render_material_usage_id_t);
    instance->frame_checked = KAN_FALSE;

    instance->object_buffers_initialized = KAN_FALSE;
    instance->ground_vertex_buffer = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
    instance->ground_index_buffer = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
    instance->ground_index_count = 0u;

    instance->cube_vertex_buffer = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
    instance->cube_index_buffer = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
    instance->cube_index_count = 0u;

    instance->test_read_back_buffer = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
    instance->instanced_data_allocator = KAN_HANDLE_SET_INVALID (kan_render_frame_lifetime_buffer_allocator_t);

    for (kan_loop_size_t index = 0u; index < SPLIT_SCREEN_VIEWS; ++index)
    {
        instance->test_expectation_requests[index] = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
        instance->test_read_back_statuses[index] = KAN_HANDLE_SET_INVALID (kan_render_read_back_status_t);
        deferred_render_scene_view_data_init (&instance->scene_view[index]);
    }

    deferred_render_shadow_pass_data_init (&instance->directional_light_shadow_pass);
    for (kan_loop_size_t index = 0u; index < POINT_LIGHTS_SHADOW_PASS_COUNT; ++index)
    {
        deferred_render_shadow_pass_data_init (&instance->point_light_shadow_passes[index]);
    }

    instance->directional_light_object_parameter_set = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_t);
    instance->point_light_shared_parameter_set = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_t);

    for (kan_instance_size_t box_x = 0u; box_x < BOXES_X; ++box_x)
    {
        for (kan_instance_size_t box_y = 0u; box_y < BOXES_Y; ++box_y)
        {
            const kan_instance_size_t box_index = box_x * BOXES_Y + box_y;
            const float box_scale = 0.7f + 0.3f * (sinf ((float) box_index) + 1.0f);
            const float box_offset = 0.25f * cosf ((float) (box_index * box_index));

            const float box_step_x = 2.0f * WORLD_HALF_WIDTH / (float) BOXES_X;
            const float box_step_y = 2.0f * WORLD_HALF_HEIGHT / (float) BOXES_Y;
            const float box_min_x = -0.5f * box_step_x * (float) BOXES_X;
            const float box_min_y = -0.5f * box_step_y * (float) BOXES_Y;

            struct kan_transform_3_t box_transform = kan_transform_3_get_identity ();
            box_transform.location.x = box_min_x + box_step_x * (float) box_x + box_offset;
            box_transform.location.z = box_min_y + box_step_y * (float) box_y + box_offset;
            box_transform.scale.x = box_scale;
            box_transform.scale.y = box_scale;
            box_transform.scale.z = box_scale;

            box_transform.rotation =
                kan_make_quaternion_from_euler (0.0f, (float) (box_index * box_index * box_index), 0.0f);

            instance->box_transform_matrices[box_index] = kan_transform_3_to_float_matrix_4x4 (&box_transform);
        }
    }

    instance->directional_light_direction =
        kan_float_vector_3_normalized (kan_make_float_vector_3_t (0.0f, -4.0f, 5.0f));
    {
        const float border_adjustment = 1.1f;
        // We use reversed depth everywhere in this example.
        struct kan_float_matrix_4x4_t projection = kan_orthographic_projection (
            -border_adjustment * WORLD_HALF_WIDTH, border_adjustment * WORLD_HALF_WIDTH,
            -border_adjustment * WORLD_HALF_HEIGHT, border_adjustment * WORLD_HALF_HEIGHT, 100.0f, 0.01f);

        struct kan_transform_3_t view_transform = kan_transform_3_get_identity ();
        view_transform.location.z = -border_adjustment * WORLD_HALF_HEIGHT;
        view_transform.location.y = instance->directional_light_direction.y * -border_adjustment * WORLD_HALF_HEIGHT /
                                    instance->directional_light_direction.z;

        const struct kan_float_vector_3_t forward = {0.0f, 0.0f, 1.0f};
        view_transform.rotation =
            kan_make_quaternion_from_vector_difference (&forward, &instance->directional_light_direction);

        struct kan_float_matrix_4x4_t view_transform_matrix = kan_transform_3_to_float_matrix_4x4 (&view_transform);
        struct kan_float_matrix_4x4_t view = kan_float_matrix_4x4_inverse (&view_transform_matrix);
        instance->directional_light_shadow_projection_view = kan_float_matrix_4x4_multiply (&projection, &view);
    }

    _Static_assert (POINT_LIGHTS_WITH_SHADOWS == 3u, "Update this setup if number of lights changes.");
    instance->point_lights_with_shadows_positions[0u] = kan_make_float_vector_3_t (-3.0f, 3.0f, -3.0f);
    instance->point_lights_with_shadows_positions[1u] = kan_make_float_vector_3_t (3.0f, 3.0f, -3.0f);
    instance->point_lights_with_shadows_positions[2u] = kan_make_float_vector_3_t (0.0f, 3.0f, 3.0f);
}

static void example_deferred_render_singleton_initialize_object_buffers (
    struct example_deferred_render_singleton_t *instance, kan_render_context_t render_context)
{
    KAN_ASSERT (!instance->object_buffers_initialized)
    struct deferred_render_full_screen_quad_vertex_t full_screen_quad_vertices[] = {
        {{-1.0f, -1.0f}},
        {{-1.0f, 1.0f}},
        {{1.0f, 1.0f}},
        {{1.0f, -1.0f}},
    };

    instance->full_screen_quad_vertex_buffer =
        kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_ATTRIBUTE, sizeof (full_screen_quad_vertices),
                                  full_screen_quad_vertices, kan_string_intern ("full_screen_quad_vertices"));

    uint16_t full_screen_quad_indices[] = {
        0u, 1u, 2u, 2u, 3u, 0u,
    };

    instance->full_screen_quad_index_count = sizeof (full_screen_quad_indices) / sizeof (full_screen_quad_indices[0u]);
    instance->full_screen_quad_index_buffer =
        kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_INDEX_16, sizeof (full_screen_quad_indices),
                                  full_screen_quad_indices, kan_string_intern ("full_screen_quad_indices"));

    struct deferred_render_vertex_t ground_vertices[] = {
        {{-WORLD_HALF_WIDTH, 0.0f, -WORLD_HALF_HEIGHT}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{-WORLD_HALF_WIDTH, 0.0f, WORLD_HALF_HEIGHT}, {0.0f, 1.0f, 0.0f}, {0.0f, 2.0f * WORLD_HALF_HEIGHT}},
        {{WORLD_HALF_WIDTH, 0.0f, WORLD_HALF_HEIGHT},
         {0.0f, 1.0f, 0.0f},
         {2.0f * WORLD_HALF_WIDTH, 2.0f * WORLD_HALF_HEIGHT}},
        {{WORLD_HALF_WIDTH, 0.0f, -WORLD_HALF_HEIGHT}, {0.0f, 1.0f, 0.0f}, {2.0f * WORLD_HALF_WIDTH, 0.0f}},
    };

    instance->ground_vertex_buffer =
        kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_ATTRIBUTE, sizeof (ground_vertices),
                                  ground_vertices, kan_string_intern ("ground_vertices"));

    uint16_t ground_indices[] = {
        0u, 1u, 2u, 2u, 3u, 0u,
    };

    instance->ground_index_count = sizeof (ground_indices) / sizeof (ground_indices[0u]);
    instance->ground_index_buffer =
        kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_INDEX_16, sizeof (ground_indices),
                                  ground_indices, kan_string_intern ("ground_indices"));

    struct deferred_render_vertex_t cube_vertices[] = {
        // Up.
        {{-0.5f, 1.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{-0.5f, 1.0f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        {{0.5f, 1.0f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{0.5f, 1.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        // Down.
        {{0.5f, 0.0f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f, 0.0f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        {{-0.5f, 0.0f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, 0.0f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
        // Front.
        {{-0.5f, 1.0f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
        {{0.5f, 1.0f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        {{0.5f, 0.0f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
        {{-0.5f, 0.0f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
        // Back.
        {{0.5f, 0.0f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.5f, 1.0f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{-0.5f, 1.0f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{-0.5f, 0.0f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        // Right.
        {{0.5f, 1.0f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, 1.0f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, 0.0f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{0.5f, 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        // Left.
        {{-0.5f, 0.0f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f, 1.0f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f, 1.0f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-0.5f, 0.0f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    };

    instance->cube_vertex_buffer =
        kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_ATTRIBUTE, sizeof (cube_vertices),
                                  cube_vertices, kan_string_intern ("cube_vertices"));

    uint16_t cube_indices[] = {
        // Up.
        0u,
        1u,
        2u,
        2u,
        3u,
        0u,
        // Down.
        4u,
        5u,
        6u,
        6u,
        7u,
        4u,
        // Front.
        8u,
        9u,
        10u,
        10u,
        11u,
        8u,
        // Back.
        12u,
        13u,
        14u,
        14u,
        15u,
        12u,
        // Right.
        16u,
        17u,
        18u,
        18u,
        19u,
        16u,
        // Left.
        20u,
        21u,
        22u,
        22u,
        23u,
        20u,
    };

    instance->cube_index_count = sizeof (cube_indices) / sizeof (cube_indices[0u]);
    instance->cube_index_buffer =
        kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_INDEX_16, sizeof (cube_indices), cube_indices,
                                  kan_string_intern ("cube_indices"));

    instance->instanced_data_allocator =
        kan_render_frame_lifetime_buffer_allocator_create (render_context, KAN_RENDER_BUFFER_TYPE_ATTRIBUTE, 1048576u,
                                                           KAN_FALSE, kan_string_intern ("instanced_data_allocator"));
    instance->object_buffers_initialized = KAN_TRUE;
}

APPLICATION_FRAMEWORK_EXAMPLES_DEFERRED_RENDER_API void example_deferred_render_singleton_shutdown (
    struct example_deferred_render_singleton_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->full_screen_quad_vertex_buffer))
    {
        kan_render_buffer_destroy (instance->full_screen_quad_vertex_buffer);
    }

    if (KAN_HANDLE_IS_VALID (instance->full_screen_quad_index_buffer))
    {
        kan_render_buffer_destroy (instance->full_screen_quad_index_buffer);
    }

    if (KAN_HANDLE_IS_VALID (instance->ground_vertex_buffer))
    {
        kan_render_buffer_destroy (instance->ground_vertex_buffer);
    }

    if (KAN_HANDLE_IS_VALID (instance->ground_index_buffer))
    {
        kan_render_buffer_destroy (instance->ground_index_buffer);
    }

    if (KAN_HANDLE_IS_VALID (instance->cube_vertex_buffer))
    {
        kan_render_buffer_destroy (instance->cube_vertex_buffer);
    }

    if (KAN_HANDLE_IS_VALID (instance->cube_index_buffer))
    {
        kan_render_buffer_destroy (instance->cube_index_buffer);
    }

    if (KAN_HANDLE_IS_VALID (instance->test_read_back_buffer))
    {
        kan_render_buffer_destroy (instance->test_read_back_buffer);
    }

    if (KAN_HANDLE_IS_VALID (instance->instanced_data_allocator))
    {
        kan_render_frame_lifetime_buffer_allocator_destroy (instance->instanced_data_allocator);
    }

    for (kan_loop_size_t index = 0u; index < SPLIT_SCREEN_VIEWS; ++index)
    {
        if (KAN_HANDLE_IS_VALID (instance->test_read_back_statuses[index]))
        {
            kan_render_read_back_status_destroy (instance->test_read_back_statuses[index]);
        }

        deferred_render_scene_view_data_shutdown (&instance->scene_view[index]);
    }

    deferred_render_shadow_pass_data_shutdown (&instance->directional_light_shadow_pass);
    for (kan_loop_size_t index = 0u; index < POINT_LIGHTS_SHADOW_PASS_COUNT; ++index)
    {
        deferred_render_shadow_pass_data_shutdown (&instance->point_light_shadow_passes[index]);
    }

    if (KAN_HANDLE_IS_VALID (instance->directional_light_object_parameter_set))
    {
        kan_render_pipeline_parameter_set_destroy (instance->directional_light_object_parameter_set);
    }

    if (KAN_HANDLE_IS_VALID (instance->point_light_shared_parameter_set))
    {
        kan_render_pipeline_parameter_set_destroy (instance->point_light_shared_parameter_set);
    }
}

struct deferred_render_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (deferred_render)
    KAN_UP_BIND_STATE (deferred_render, state)

    kan_context_system_t application_system_handle;
    kan_context_system_t application_framework_system_handle;
    kan_context_system_t render_backend_system_handle;
    kan_bool_t test_mode;

    kan_interned_string_t g_buffer_pass_name;
    kan_interned_string_t lighting_pass_name;
    kan_interned_string_t shadow_pass_name;
};

APPLICATION_FRAMEWORK_EXAMPLES_DEFERRED_RENDER_API void kan_universe_mutator_deploy_deferred_render (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct deferred_render_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node,
                                       KAN_RENDER_FOUNDATION_MATERIAL_INSTANCE_CUSTOM_SYNC_END_CHECKPOINT);

    kan_context_t context = kan_universe_get_context (universe);
    state->application_system_handle = kan_context_query (context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);
    state->application_framework_system_handle =
        kan_context_query (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME);
    state->render_backend_system_handle = kan_context_query (context, KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
    {
        state->test_mode =
            kan_application_framework_system_get_arguments_count (state->application_framework_system_handle) == 2 &&
            strcmp (kan_application_framework_system_get_arguments (state->application_framework_system_handle)[1],
                    "--test") == 0;
    }
    else
    {
        state->test_mode = KAN_FALSE;
    }

    state->g_buffer_pass_name = kan_string_intern ("g_buffer");
    state->lighting_pass_name = kan_string_intern ("lighting");
    state->shadow_pass_name = kan_string_intern ("shadow");
}

static inline kan_render_graphics_pipeline_t find_pipeline (const struct kan_render_material_loaded_t *material,
                                                            kan_interned_string_t pass_name,
                                                            kan_instance_size_t variant_index)
{
    for (kan_loop_size_t index = 0u; index < material->pipelines.size; ++index)
    {
        const struct kan_render_material_loaded_pipeline_t *loaded =
            &((struct kan_render_material_loaded_pipeline_t *) material->pipelines.data)[index];

        if (loaded->pass_name == pass_name && loaded->variant_index == variant_index)
        {
            return loaded->pipeline;
        }
    }

    return KAN_HANDLE_SET_INVALID (kan_render_graphics_pipeline_t);
}

static kan_bool_t try_render_opaque_objects (struct deferred_render_state_t *state,
                                             struct example_deferred_render_singleton_t *singleton,
                                             kan_render_pass_instance_t pass_instance,
                                             kan_render_pipeline_parameter_set_t pass_parameter_set,
                                             kan_interned_string_t pass_name,
                                             const struct deferred_render_config_t *config)
{
    if (!singleton->object_buffers_initialized)
    {
        return KAN_FALSE;
    }

    kan_bool_t ground_rendered = KAN_FALSE;
    KAN_UP_VALUE_READ (ground_material_instance, kan_render_material_instance_loaded_t, name,
                       &config->ground_material_instance_name)
    {
        KAN_UP_VALUE_READ (material, kan_render_material_loaded_t, name, &ground_material_instance->data.material_name)
        {
            kan_render_graphics_pipeline_t pipeline = find_pipeline (material, pass_name, 0u);
            if (!KAN_HANDLE_IS_VALID (pipeline))
            {
                KAN_UP_QUERY_BREAK;
            }

            if (!kan_render_pass_instance_graphics_pipeline (pass_instance, pipeline))
            {
                // Pipeline is not yet ready, skip draw.
                KAN_UP_QUERY_BREAK;
            }

            kan_render_pipeline_parameter_set_t sets[] = {
                pass_parameter_set,
                ground_material_instance->data.parameter_set,
            };

            kan_render_pass_instance_pipeline_parameter_sets (pass_instance, KAN_RPL_SET_PASS,
                                                              sizeof (sets) / sizeof (sets[0u]), sets);

            // Only one vertex attribute buffer for the sake of simplicity.
            KAN_ASSERT (material->vertex_attribute_sources.size == 1u)
            KAN_ASSERT (material->has_instanced_attribute_source)

            struct kan_render_allocated_slice_t allocation = kan_render_frame_lifetime_buffer_allocator_allocate (
                singleton->instanced_data_allocator, material->instanced_attribute_source.block_size,
                _Alignof (struct kan_float_matrix_4x4_t));
            KAN_ASSERT (KAN_HANDLE_IS_VALID (allocation.buffer))

            struct kan_transform_3_t ground_transform = kan_transform_3_get_identity ();
            struct kan_float_matrix_4x4_t ground_transform_matrix =
                kan_transform_3_to_float_matrix_4x4 (&ground_transform);

            void *instanced_data = kan_render_buffer_patch (allocation.buffer, allocation.slice_offset,
                                                            material->instanced_attribute_source.block_size);
            memcpy (instanced_data, ground_material_instance->data.instanced_data.data,
                    material->instanced_attribute_source.block_size);
            // For the sake of the simple example, we just assume that model matrix is the first field.
            memcpy (instanced_data, &ground_transform_matrix, sizeof (ground_transform_matrix));

            kan_render_buffer_t attribute_buffers[] = {singleton->ground_vertex_buffer, allocation.buffer};
            kan_render_size_t attribute_buffers_offsets[] = {0u, allocation.slice_offset};

            struct kan_rpl_meta_attribute_source_t *expected_attribute_source =
                &((struct kan_rpl_meta_attribute_source_t *) material->vertex_attribute_sources.data)[0u];

            kan_render_pass_instance_attributes (pass_instance, expected_attribute_source->binding,
                                                 sizeof (attribute_buffers) / sizeof (attribute_buffers[0u]),
                                                 attribute_buffers, attribute_buffers_offsets);

            kan_render_pass_instance_indices (pass_instance, singleton->ground_index_buffer);
            kan_render_pass_instance_draw (pass_instance, 0u, singleton->ground_index_count, 0u, 0u, 1u);
            ground_rendered = KAN_TRUE;
        }
    }

    kan_bool_t cube_rendered = KAN_FALSE;
    KAN_UP_VALUE_READ (cube_material_instance, kan_render_material_instance_loaded_t, name,
                       &config->cube_material_instance_name)
    {
        KAN_UP_VALUE_READ (material, kan_render_material_loaded_t, name, &cube_material_instance->data.material_name)
        {
            kan_render_graphics_pipeline_t pipeline = find_pipeline (material, pass_name, 0u);
            if (!KAN_HANDLE_IS_VALID (pipeline))
            {
                KAN_UP_QUERY_BREAK;
            }

            if (!kan_render_pass_instance_graphics_pipeline (pass_instance, pipeline))
            {
                // Pipeline is not yet ready, skip draw.
                KAN_UP_QUERY_BREAK;
            }

            kan_render_pipeline_parameter_set_t sets[] = {
                pass_parameter_set,
                cube_material_instance->data.parameter_set,
            };

            kan_render_pass_instance_pipeline_parameter_sets (pass_instance, KAN_RPL_SET_PASS,
                                                              sizeof (sets) / sizeof (sets[0u]), sets);

            // Only one vertex attribute buffer for the sake of simplicity.
            KAN_ASSERT (material->vertex_attribute_sources.size == 1u)
            KAN_ASSERT (material->has_instanced_attribute_source)

            struct kan_render_allocated_slice_t allocation = kan_render_frame_lifetime_buffer_allocator_allocate (
                singleton->instanced_data_allocator,
                material->instanced_attribute_source.block_size * BOXES_X * BOXES_Y,
                _Alignof (struct kan_float_matrix_4x4_t));
            KAN_ASSERT (KAN_HANDLE_IS_VALID (allocation.buffer))

            void *instanced_data =
                kan_render_buffer_patch (allocation.buffer, allocation.slice_offset,
                                         material->instanced_attribute_source.block_size * BOXES_X * BOXES_Y);

            memcpy (instanced_data, singleton->box_transform_matrices,
                    material->instanced_attribute_source.block_size * BOXES_X * BOXES_Y);

            kan_render_buffer_t attribute_buffers[] = {singleton->cube_vertex_buffer, allocation.buffer};
            kan_render_size_t attribute_buffers_offsets[] = {0u, allocation.slice_offset};

            struct kan_rpl_meta_attribute_source_t *expected_attribute_source =
                &((struct kan_rpl_meta_attribute_source_t *) material->vertex_attribute_sources.data)[0u];

            kan_render_pass_instance_attributes (pass_instance, expected_attribute_source->binding,
                                                 sizeof (attribute_buffers) / sizeof (attribute_buffers[0u]),
                                                 attribute_buffers, attribute_buffers_offsets);

            kan_render_pass_instance_indices (pass_instance, singleton->cube_index_buffer);
            kan_render_pass_instance_draw (pass_instance, 0u, singleton->cube_index_count, 0u, 0u, BOXES_X * BOXES_Y);

            cube_rendered = KAN_TRUE;
        }
    }

    return ground_rendered && cube_rendered;
}

static kan_bool_t try_render_lighting (struct deferred_render_state_t *state,
                                       const struct kan_render_context_singleton_t *render_context,
                                       struct example_deferred_render_singleton_t *singleton,
                                       kan_render_pass_instance_t pass_instance,
                                       struct deferred_render_scene_view_data_t *scene_view_data,
                                       const struct deferred_render_config_t *config)
{
    if (!singleton->object_buffers_initialized)
    {
        return KAN_FALSE;
    }

    kan_bool_t ambient_rendered = KAN_FALSE;
    KAN_UP_VALUE_READ (ambient_material, kan_render_material_loaded_t, name, &config->ambient_light_material_name)
    {
        kan_render_graphics_pipeline_t pipeline = find_pipeline (ambient_material, state->lighting_pass_name, 0u);
        if (!KAN_HANDLE_IS_VALID (pipeline))
        {
            KAN_UP_QUERY_BREAK;
        }

        if (!kan_render_pass_instance_graphics_pipeline (pass_instance, pipeline))
        {
            // Pipeline is not yet ready, skip draw.
            KAN_UP_QUERY_BREAK;
        }

        kan_render_pass_instance_pipeline_parameter_sets (pass_instance, KAN_RPL_SET_PASS, 1u,
                                                          &scene_view_data->lighting_parameter_set);

        // Only one vertex attribute buffer for the sake of simplicity.
        KAN_ASSERT (ambient_material->vertex_attribute_sources.size == 1u)
        KAN_ASSERT (!ambient_material->has_instanced_attribute_source)

        const struct kan_float_vector_4_t ambient_modifier = {kan_color_transfer_rgb_to_srgb_approximate (0.05f),
                                                              kan_color_transfer_rgb_to_srgb_approximate (0.05f),
                                                              kan_color_transfer_rgb_to_srgb_approximate (0.05f), 1.0f};
        kan_render_pass_instance_push_constant (pass_instance, &ambient_modifier);

        kan_render_buffer_t attribute_buffers[] = {singleton->full_screen_quad_vertex_buffer};
        kan_render_size_t attribute_buffers_offsets[] = {0u};

        struct kan_rpl_meta_attribute_source_t *expected_attribute_source =
            &((struct kan_rpl_meta_attribute_source_t *) ambient_material->vertex_attribute_sources.data)[0u];

        kan_render_pass_instance_attributes (pass_instance, expected_attribute_source->binding,
                                             sizeof (attribute_buffers) / sizeof (attribute_buffers[0u]),
                                             attribute_buffers, attribute_buffers_offsets);

        kan_render_pass_instance_indices (pass_instance, singleton->full_screen_quad_index_buffer);
        kan_render_pass_instance_draw (pass_instance, 0u, singleton->full_screen_quad_index_count, 0u, 0u, 1u);
        ambient_rendered = KAN_TRUE;
    }

    kan_bool_t directional_rendered = KAN_FALSE;
    KAN_UP_VALUE_READ (directional_material, kan_render_material_loaded_t, name,
                       &config->directional_light_material_name)
    {
        kan_render_graphics_pipeline_t pipeline = find_pipeline (directional_material, state->lighting_pass_name, 0u);
        if (!KAN_HANDLE_IS_VALID (pipeline))
        {
            KAN_UP_QUERY_BREAK;
        }

        if (!kan_render_pass_instance_graphics_pipeline (pass_instance, pipeline))
        {
            // Pipeline is not yet ready, skip draw.
            KAN_UP_QUERY_BREAK;
        }

        kan_render_pipeline_parameter_set_t parameter_sets[] = {
            scene_view_data->lighting_parameter_set,
            KAN_HANDLE_INITIALIZE_INVALID,
            singleton->directional_light_object_parameter_set,
        };

        kan_render_pass_instance_pipeline_parameter_sets (
            pass_instance, KAN_RPL_SET_PASS, sizeof (parameter_sets) / sizeof (parameter_sets[0u]), parameter_sets);

        // Only one vertex attribute buffer for the sake of simplicity.
        KAN_ASSERT (directional_material->vertex_attribute_sources.size == 1u)
        KAN_ASSERT (!directional_material->has_instanced_attribute_source)

        struct deferred_scene_directional_light_push_constant_t push_constant = {
            .shadow_map_projection_view = singleton->directional_light_shadow_projection_view,
            .color =
                {
                    .x = kan_color_transfer_rgb_to_srgb_approximate (0.2f),
                    .y = kan_color_transfer_rgb_to_srgb_approximate (0.2f),
                    .z = kan_color_transfer_rgb_to_srgb_approximate (0.4f),
                    .w = 0.0f,
                },
            .direction =
                {
                    .x = singleton->directional_light_direction.x,
                    .y = singleton->directional_light_direction.y,
                    .z = singleton->directional_light_direction.z,
                    0.0f,
                },
        };

        kan_render_pass_instance_push_constant (pass_instance, &push_constant);
        kan_render_buffer_t attribute_buffers[] = {singleton->full_screen_quad_vertex_buffer};
        kan_render_size_t attribute_buffers_offsets[] = {0u};

        struct kan_rpl_meta_attribute_source_t *expected_attribute_source =
            &((struct kan_rpl_meta_attribute_source_t *) directional_material->vertex_attribute_sources.data)[0u];

        kan_render_pass_instance_attributes (pass_instance, expected_attribute_source->binding,
                                             sizeof (attribute_buffers) / sizeof (attribute_buffers[0u]),
                                             attribute_buffers, attribute_buffers_offsets);

        kan_render_pass_instance_indices (pass_instance, singleton->full_screen_quad_index_buffer);
        kan_render_pass_instance_draw (pass_instance, 0u, singleton->full_screen_quad_index_count, 0u, 0u, 1u);
        directional_rendered = KAN_TRUE;
    }

    kan_bool_t point_rendered = KAN_FALSE;
    KAN_UP_VALUE_READ (point_material, kan_render_material_loaded_t, name, &config->point_light_material_name)
    {
        kan_render_graphics_pipeline_t pipeline = find_pipeline (point_material, state->lighting_pass_name, 0u);
        if (!KAN_HANDLE_IS_VALID (pipeline))
        {
            KAN_UP_QUERY_BREAK;
        }

        if (!kan_render_pass_instance_graphics_pipeline (pass_instance, pipeline))
        {
            // Pipeline is not yet ready, skip draw.
            KAN_UP_QUERY_BREAK;
        }

        kan_render_pipeline_parameter_set_t parameter_sets[] = {
            scene_view_data->lighting_parameter_set,
            KAN_HANDLE_INITIALIZE_INVALID,
            KAN_HANDLE_INITIALIZE_INVALID,
            singleton->point_light_shared_parameter_set,
        };

        kan_render_pass_instance_pipeline_parameter_sets (
            pass_instance, KAN_RPL_SET_PASS, sizeof (parameter_sets) / sizeof (parameter_sets[0u]), parameter_sets);

        // Only one vertex attribute buffer for the sake of simplicity.
        KAN_ASSERT (point_material->vertex_attribute_sources.size == 1u)
        KAN_ASSERT (point_material->has_instanced_attribute_source)

#define SHADOWLESS_POINT_LIGHTS_X 31u
#define SHADOWLESS_POINT_LIGHTS_Y 31u

        const kan_instance_size_t total_lights =
            POINT_LIGHTS_WITH_SHADOWS + SHADOWLESS_POINT_LIGHTS_X * SHADOWLESS_POINT_LIGHTS_Y;
        const kan_instance_size_t allocation_size =
            point_material->instanced_attribute_source.block_size * total_lights;

        struct kan_render_allocated_slice_t allocation = kan_render_frame_lifetime_buffer_allocator_allocate (
            singleton->instanced_data_allocator, allocation_size,
            _Alignof (struct deferred_scene_point_light_instanced_t));
        KAN_ASSERT (KAN_HANDLE_IS_VALID (allocation.buffer))

        struct deferred_scene_point_light_instanced_t *instanced_data =
            kan_render_buffer_patch (allocation.buffer, allocation.slice_offset, allocation_size);
        struct deferred_scene_point_light_instanced_t *instanced_data_output = instanced_data;

        for (kan_instance_size_t point_light_index = 0u; point_light_index < POINT_LIGHTS_WITH_SHADOWS;
             ++point_light_index)
        {
            instanced_data_output->position_and_distance = kan_extend_float_vector_3_t (
                singleton->point_lights_with_shadows_positions[point_light_index], POINT_LIGHTS_WITH_SHADOWS_DISTANCE);
            instanced_data_output->color = kan_make_float_vector_3_t (
                kan_color_transfer_rgb_to_srgb_approximate (1.0f), kan_color_transfer_rgb_to_srgb_approximate (1.0f),
                kan_color_transfer_rgb_to_srgb_approximate (1.0f));
            instanced_data_output->shadow_map_index = (int32_t) point_light_index;
            ++instanced_data_output;
        }

        for (kan_instance_size_t light_x = 0u; light_x < SHADOWLESS_POINT_LIGHTS_X; ++light_x)
        {
            for (kan_instance_size_t light_y = 0u; light_y < SHADOWLESS_POINT_LIGHTS_Y; ++light_y)
            {
                const kan_instance_size_t light_index = light_x * SHADOWLESS_POINT_LIGHTS_Y + light_y;
                const float light_distance = 1.5f + 1.5f * (1.0f + sinf ((float) light_index));
                const float light_offset = 0.5f * cosf ((float) (light_index * light_index));

                const float light_step_x = 2.0f * WORLD_HALF_WIDTH / (float) SHADOWLESS_POINT_LIGHTS_X;
                const float light_step_y = 2.0f * WORLD_HALF_HEIGHT / (float) SHADOWLESS_POINT_LIGHTS_Y;
                const float light_min_x = -0.5f * light_step_x * (float) SHADOWLESS_POINT_LIGHTS_X;
                const float light_min_y = -0.5f * light_step_y * (float) SHADOWLESS_POINT_LIGHTS_Y;

                instanced_data_output->position_and_distance = kan_make_float_vector_4_t (
                    light_min_x + light_step_x * (float) light_x + light_offset, 0.5f,
                    light_min_y + light_step_y * (float) light_y + light_offset, light_distance);

                switch (light_index % 6u)
                {
                case 0u:
                    instanced_data_output->color =
                        kan_make_float_vector_3_t (kan_color_transfer_rgb_to_srgb_approximate (1.0f), 0.0f, 0.0f);
                    break;
                case 1u:
                    instanced_data_output->color =
                        kan_make_float_vector_3_t (0.0f, kan_color_transfer_rgb_to_srgb_approximate (1.0f), 0.0f);
                    break;
                case 2u:
                    instanced_data_output->color =
                        kan_make_float_vector_3_t (0.0f, 0.0f, kan_color_transfer_rgb_to_srgb_approximate (1.0f));
                    break;
                case 3u:
                    instanced_data_output->color =
                        kan_make_float_vector_3_t (kan_color_transfer_rgb_to_srgb_approximate (1.0f),
                                                   kan_color_transfer_rgb_to_srgb_approximate (1.0f), 0.0f);
                    break;
                case 4u:
                    instanced_data_output->color =
                        kan_make_float_vector_3_t (0.0f, kan_color_transfer_rgb_to_srgb_approximate (1.0f),
                                                   kan_color_transfer_rgb_to_srgb_approximate (1.0f));
                    break;
                case 5u:
                    instanced_data_output->color =
                        kan_make_float_vector_3_t (kan_color_transfer_rgb_to_srgb_approximate (1.0f), 0.0f,
                                                   kan_color_transfer_rgb_to_srgb_approximate (1.0f));
                    break;
                }

                instanced_data_output->shadow_map_index = -1;
                ++instanced_data_output;
            }
        }

        kan_render_buffer_t attribute_buffers[] = {singleton->full_screen_quad_vertex_buffer, allocation.buffer};
        kan_render_size_t attribute_buffers_offsets[] = {0u, allocation.slice_offset};

        struct kan_rpl_meta_attribute_source_t *expected_attribute_source =
            &((struct kan_rpl_meta_attribute_source_t *) point_material->vertex_attribute_sources.data)[0u];

        kan_render_pass_instance_attributes (pass_instance, expected_attribute_source->binding,
                                             sizeof (attribute_buffers) / sizeof (attribute_buffers[0u]),
                                             attribute_buffers, attribute_buffers_offsets);

        kan_render_pass_instance_indices (pass_instance, singleton->full_screen_quad_index_buffer);
        kan_render_pass_instance_draw (pass_instance, 0u, singleton->full_screen_quad_index_count, 0u, 0u,
                                       total_lights);
        point_rendered = KAN_TRUE;
    }

    return ambient_rendered && directional_rendered && point_rendered;
}

static inline void initialize_scene_view_buffer_if_needed (struct example_deferred_render_singleton_t *singleton,
                                                           const struct kan_render_context_singleton_t *render_context,
                                                           kan_instance_size_t index,
                                                           kan_instance_size_t size)
{
    if (!KAN_HANDLE_IS_VALID (singleton->scene_view[index].view_parameters_buffer))
    {
        KAN_ASSERT (!KAN_HANDLE_IS_VALID (singleton->scene_view[index].g_buffer_parameter_set))
        KAN_ASSERT (!KAN_HANDLE_IS_VALID (singleton->scene_view[index].lighting_parameter_set))
        singleton->scene_view[index].view_parameters_buffer =
            kan_render_buffer_create (render_context->render_context, KAN_RENDER_BUFFER_TYPE_UNIFORM, size, NULL,
                                      kan_string_intern ("scene_view_buffer_data"));
    }
    else
    {
        KAN_ASSERT (kan_render_buffer_get_full_size (singleton->scene_view[index].view_parameters_buffer) == size)
    }
}

static inline void initialize_shadow_pass_data_if_needed (const struct kan_render_context_singleton_t *render_context,
                                                          struct deferred_render_shadow_pass_data_t *data,
                                                          struct kan_render_graph_pass_variant_t *shadow_pass_variant,
                                                          struct kan_rpl_meta_buffer_t *shadow_pass_buffer_meta)
{
    if (!KAN_HANDLE_IS_VALID (data->parameters_buffer))
    {
        KAN_ASSERT (!KAN_HANDLE_IS_VALID (data->parameter_set))
        data->parameters_buffer = kan_render_buffer_create (
            render_context->render_context, KAN_RENDER_BUFFER_TYPE_UNIFORM, shadow_pass_buffer_meta->main_size, NULL,
            kan_string_intern ("shadow_buffer_data"));
    }

    if (!KAN_HANDLE_IS_VALID (data->parameter_set))
    {
        struct kan_render_parameter_update_description_t bindings[] = {{
            .binding = shadow_pass_buffer_meta->binding,
            .buffer_binding =
                {
                    .buffer = data->parameters_buffer,
                    .offset = 0u,
                    .range = shadow_pass_buffer_meta->main_size,
                },
        }};

        struct kan_render_pipeline_parameter_set_description_t description = {
            .layout = shadow_pass_variant->pass_parameter_set_layout,
            .stable_binding = KAN_TRUE,
            .initial_bindings_count = sizeof (bindings) / sizeof (bindings[0u]),
            .initial_bindings = bindings,
            .tracking_name = kan_string_intern ("shadow_pass_set"),
        };

        data->parameter_set = kan_render_pipeline_parameter_set_create (render_context->render_context, &description);
    }
}

static void try_render_frame (struct deferred_render_state_t *state,
                              const struct kan_render_context_singleton_t *render_context,
                              const struct kan_render_graph_resource_management_singleton_t *render_resource_management,
                              struct example_deferred_render_singleton_t *singleton,
                              const struct deferred_render_config_t *config)
{
    const struct kan_application_system_window_info_t *window_info =
        kan_application_system_get_window_info_from_handle (state->application_system_handle, singleton->window_handle);

    kan_render_size_t viewport_width;
    kan_render_size_t viewport_height;
    kan_render_size_t viewport_step_x;
    kan_render_size_t viewport_step_y;

    if (window_info->width_for_render > window_info->height_for_render)
    {
        viewport_width = window_info->width_for_render / SPLIT_SCREEN_VIEWS;
        viewport_height = window_info->height_for_render;
        viewport_step_x = viewport_width;
        viewport_step_y = 0u;
    }
    else
    {
        viewport_width = window_info->width_for_render;
        viewport_height = window_info->height_for_render / SPLIT_SCREEN_VIEWS;
        viewport_step_x = 0u;
        viewport_step_y = viewport_height;
    }

#define MAX_EXPECTED_SCENE_IMAGE_REQUESTS 16u
    _Static_assert (MAX_EXPECTED_SCENE_IMAGE_REQUESTS >= DEFERRED_RENDER_SCENE_IMAGE_COUNT &&
                        MAX_EXPECTED_SCENE_IMAGE_REQUESTS >= DEFERRED_RENDER_SHADOW_IMAGE_COUNT,
                    "Static request allocation for frame buffers is big enough.");
    struct kan_render_graph_resource_image_request_t image_requests[MAX_EXPECTED_SCENE_IMAGE_REQUESTS];

#define MAX_EXPECTED_SCENE_FRAME_BUFFER_REQUESTS 16u
    _Static_assert (MAX_EXPECTED_SCENE_FRAME_BUFFER_REQUESTS >= DEFERRED_RENDER_SCENE_FRAME_BUFFER_COUNT &&
                        MAX_EXPECTED_SCENE_FRAME_BUFFER_REQUESTS >= DEFERRED_RENDER_FLAT_SHADOW_FRAME_BUFFER_COUNT &&
                        MAX_EXPECTED_SCENE_FRAME_BUFFER_REQUESTS >= DEFERRED_RENDER_CUBE_SHADOW_FRAME_BUFFER_COUNT,
                    "Static request allocation for frame buffers is big enough.");
    struct kan_render_graph_resource_frame_buffer_request_t
        frame_buffer_requests[MAX_EXPECTED_SCENE_FRAME_BUFFER_REQUESTS];

    kan_render_pass_t g_buffer_pass_handle = KAN_HANDLE_INITIALIZE_INVALID;
    KAN_UP_VALUE_READ (g_buffer_pass, kan_render_graph_pass_t, name, &state->g_buffer_pass_name)
    {
        if (g_buffer_pass->attachments.size != 4u)
        {
            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                     "G buffer pass has unexpected count of attachments.")
            KAN_UP_QUERY_RETURN_VOID;
        }

        if (g_buffer_pass->variants.size != 1u)
        {
            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                     "G buffer pass has unexpected count of variants.")
            KAN_UP_QUERY_RETURN_VOID;
        }

        struct kan_render_graph_pass_variant_t *pass_variant =
            &((struct kan_render_graph_pass_variant_t *) g_buffer_pass->variants.data)[0u];

        if (pass_variant->pass_parameter_set_bindings.buffers.size != 1u)
        {
            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                     "G buffer pass has unexpected parameter set bindings.")
            KAN_UP_QUERY_RETURN_VOID;
        }

        g_buffer_pass_handle = g_buffer_pass->pass;
        struct kan_render_graph_pass_attachment_t *position_attachment =
            &((struct kan_render_graph_pass_attachment_t *) g_buffer_pass->attachments.data)[0u];
        struct kan_render_graph_pass_attachment_t *normal_shininess_attachment =
            &((struct kan_render_graph_pass_attachment_t *) g_buffer_pass->attachments.data)[1u];
        struct kan_render_graph_pass_attachment_t *diffuse_attachment =
            &((struct kan_render_graph_pass_attachment_t *) g_buffer_pass->attachments.data)[2u];
        struct kan_render_graph_pass_attachment_t *depth_attachment =
            &((struct kan_render_graph_pass_attachment_t *) g_buffer_pass->attachments.data)[3u];

        image_requests[DEFERRED_RENDER_SCENE_IMAGE_POSITION] = (struct kan_render_graph_resource_image_request_t) {
            .description =
                {
                    .format = position_attachment->format,
                    .width = viewport_width,
                    .height = viewport_height,
                    .depth = 1u,
                    .layers = 1u,
                    .mips = 1u,
                    .render_target = KAN_TRUE,
                    .supports_sampling = KAN_TRUE,
                    .tracking_name = NULL,
                },
            .internal = KAN_TRUE,
        };

        image_requests[DEFERRED_RENDER_SCENE_IMAGE_NORMAL_SPECULAR] =
            (struct kan_render_graph_resource_image_request_t) {
                .description =
                    {
                        .format = normal_shininess_attachment->format,
                        .width = viewport_width,
                        .height = viewport_height,
                        .depth = 1u,
                        .layers = 1u,
                        .mips = 1u,
                        .render_target = KAN_TRUE,
                        .supports_sampling = KAN_TRUE,
                        .tracking_name = NULL,
                    },
                .internal = KAN_TRUE,
            };

        image_requests[DEFERRED_RENDER_SCENE_IMAGE_ALBEDO] = (struct kan_render_graph_resource_image_request_t) {
            .description =
                {
                    .format = diffuse_attachment->format,
                    .width = viewport_width,
                    .height = viewport_height,
                    .depth = 1u,
                    .layers = 1u,
                    .mips = 1u,
                    .render_target = KAN_TRUE,
                    .supports_sampling = KAN_TRUE,
                    .tracking_name = NULL,
                },
            .internal = KAN_TRUE,
        };

        image_requests[DEFERRED_RENDER_SCENE_IMAGE_DEPTH] = (struct kan_render_graph_resource_image_request_t) {
            .description =
                {
                    .format = depth_attachment->format,
                    .width = viewport_width,
                    .height = viewport_height,
                    .depth = 1u,
                    .layers = 1u,
                    .mips = 1u,
                    .render_target = KAN_TRUE,
                    .supports_sampling = KAN_TRUE,
                    .tracking_name = NULL,
                },
            .internal = KAN_TRUE,
        };

        struct kan_rpl_meta_buffer_t *scene_view_buffer_meta =
            &((struct kan_rpl_meta_buffer_t *) pass_variant->pass_parameter_set_bindings.buffers.data)[0u];

        for (kan_loop_size_t index = 0u; index < SPLIT_SCREEN_VIEWS; ++index)
        {
            if (!KAN_HANDLE_IS_VALID (singleton->scene_view[index].g_buffer_parameter_set))
            {
                initialize_scene_view_buffer_if_needed (singleton, render_context, index,
                                                        scene_view_buffer_meta->main_size);

                struct kan_render_parameter_update_description_t bindings[] = {{
                    .binding = scene_view_buffer_meta->binding,
                    .buffer_binding =
                        {
                            .buffer = singleton->scene_view[index].view_parameters_buffer,
                            .offset = 0u,
                            .range = scene_view_buffer_meta->main_size,
                        },
                }};

                struct kan_render_pipeline_parameter_set_description_t description = {
                    .layout = pass_variant->pass_parameter_set_layout,
                    .stable_binding = KAN_TRUE,
                    .initial_bindings_count = sizeof (bindings) / sizeof (bindings[0u]),
                    .initial_bindings = bindings,
                    .tracking_name = kan_string_intern ("g_buffer_pass_set"),
                };

                singleton->scene_view[index].g_buffer_parameter_set =
                    kan_render_pipeline_parameter_set_create (render_context->render_context, &description);
            }
        }
    }

    kan_render_pass_t lighting_pass_handle = KAN_HANDLE_INITIALIZE_INVALID;
    kan_instance_size_t lighting_g_buffer_world_position_binding = 0u;
    kan_instance_size_t lighting_g_buffer_normal_shininess_binding = 0u;
    kan_instance_size_t lighting_g_buffer_diffuse_binding = 0u;

    KAN_UP_VALUE_READ (lighting_pass, kan_render_graph_pass_t, name, &state->lighting_pass_name)
    {
        if (lighting_pass->attachments.size != 2u)
        {
            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                     "Lighting pass has unexpected count of attachments.")
            KAN_UP_QUERY_RETURN_VOID;
        }

        if (lighting_pass->variants.size != 1u)
        {
            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                     "Lighting pass has unexpected count of variants.")
            KAN_UP_QUERY_RETURN_VOID;
        }

        struct kan_render_graph_pass_variant_t *pass_variant =
            &((struct kan_render_graph_pass_variant_t *) lighting_pass->variants.data)[0u];

        if (pass_variant->pass_parameter_set_bindings.buffers.size != 1u ||
            pass_variant->pass_parameter_set_bindings.samplers.size != 2u ||
            pass_variant->pass_parameter_set_bindings.images.size != 3u)
        {
            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                     "Lighting pass has unexpected parameter set bindings.")
            KAN_UP_QUERY_RETURN_VOID;
        }

        lighting_pass_handle = lighting_pass->pass;
        struct kan_render_graph_pass_attachment_t *color_attachment =
            &((struct kan_render_graph_pass_attachment_t *) lighting_pass->attachments.data)[0u];

        if (color_attachment->format != KAN_RENDER_IMAGE_FORMAT_RGBA32_SRGB)
        {
            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                     "Lighting pass has unexpected color format which breaks test.")
            KAN_UP_QUERY_RETURN_VOID;
        }

        image_requests[DEFERRED_RENDER_SCENE_IMAGE_VIEW_COLOR] = (struct kan_render_graph_resource_image_request_t) {
            .description =
                {
                    .format = color_attachment->format,
                    .width = viewport_width,
                    .height = viewport_height,
                    .depth = 1u,
                    .layers = 1u,
                    .mips = 1u,
                    .render_target = KAN_TRUE,
                    .supports_sampling = KAN_FALSE,
                    .tracking_name = NULL,
                },
            .internal = KAN_TRUE,
        };

        struct kan_rpl_meta_buffer_t *scene_view_buffer_meta =
            &((struct kan_rpl_meta_buffer_t *) pass_variant->pass_parameter_set_bindings.buffers.data)[0u];
        struct kan_rpl_meta_sampler_t *g_buffer_color_sampler_meta =
            &((struct kan_rpl_meta_sampler_t *) pass_variant->pass_parameter_set_bindings.samplers.data)[0u];
        struct kan_rpl_meta_sampler_t *g_buffer_depth_sampler_meta =
            &((struct kan_rpl_meta_sampler_t *) pass_variant->pass_parameter_set_bindings.samplers.data)[1u];

        struct kan_rpl_meta_image_t *g_buffer_world_position_image_meta =
            &((struct kan_rpl_meta_image_t *) pass_variant->pass_parameter_set_bindings.images.data)[0u];
        lighting_g_buffer_world_position_binding = g_buffer_world_position_image_meta->binding;

        struct kan_rpl_meta_image_t *g_buffer_normal_shininess_image_meta =
            &((struct kan_rpl_meta_image_t *) pass_variant->pass_parameter_set_bindings.images.data)[1u];
        lighting_g_buffer_normal_shininess_binding = g_buffer_normal_shininess_image_meta->binding;

        struct kan_rpl_meta_image_t *g_buffer_diffuse_image_meta =
            &((struct kan_rpl_meta_image_t *) pass_variant->pass_parameter_set_bindings.images.data)[2u];
        lighting_g_buffer_diffuse_binding = g_buffer_diffuse_image_meta->binding;

        for (kan_loop_size_t index = 0u; index < SPLIT_SCREEN_VIEWS; ++index)
        {
            if (!KAN_HANDLE_IS_VALID (singleton->scene_view[index].lighting_parameter_set))
            {
                initialize_scene_view_buffer_if_needed (singleton, render_context, index,
                                                        scene_view_buffer_meta->main_size);

                struct kan_render_parameter_update_description_t bindings[] = {
                    {
                        .binding = scene_view_buffer_meta->binding,
                        .buffer_binding =
                            {
                                .buffer = singleton->scene_view[index].view_parameters_buffer,
                                .offset = 0u,
                                .range = scene_view_buffer_meta->main_size,
                            },
                    },
                    {
                        .binding = g_buffer_color_sampler_meta->binding,
                        .sampler_binding =
                            {
                                .sampler =
                                    {
                                        .mag_filter = KAN_RENDER_FILTER_MODE_NEAREST,
                                        .min_filter = KAN_RENDER_FILTER_MODE_NEAREST,
                                        .mip_map_mode = KAN_RENDER_MIP_MAP_MODE_NEAREST,
                                        .address_mode_u = KAN_RENDER_ADDRESS_MODE_REPEAT,
                                        .address_mode_v = KAN_RENDER_ADDRESS_MODE_REPEAT,
                                        .address_mode_w = KAN_RENDER_ADDRESS_MODE_REPEAT,
                                        .depth_compare_enabled = KAN_FALSE,
                                        .depth_compare = KAN_RENDER_COMPARE_OPERATION_GREATER,
                                        .anisotropy_enabled = KAN_FALSE,
                                        .anisotropy_max = 1.0f,
                                    },
                            },
                    },
                    {
                        .binding = g_buffer_depth_sampler_meta->binding,
                        .sampler_binding =
                            {
                                .sampler =
                                    {
                                        .mag_filter = KAN_RENDER_FILTER_MODE_LINEAR,
                                        .min_filter = KAN_RENDER_FILTER_MODE_LINEAR,
                                        .mip_map_mode = KAN_RENDER_MIP_MAP_MODE_NEAREST,
                                        .address_mode_u = KAN_RENDER_ADDRESS_MODE_REPEAT,
                                        .address_mode_v = KAN_RENDER_ADDRESS_MODE_REPEAT,
                                        .address_mode_w = KAN_RENDER_ADDRESS_MODE_REPEAT,
                                        .depth_compare_enabled = KAN_TRUE,
                                        .depth_compare = KAN_RENDER_COMPARE_OPERATION_GREATER_OR_EQUAL,
                                        .anisotropy_enabled = KAN_FALSE,
                                        .anisotropy_max = 1.0f,
                                    },
                            },
                    },
                };

                struct kan_render_pipeline_parameter_set_description_t description = {
                    .layout = pass_variant->pass_parameter_set_layout,
                    .stable_binding = KAN_FALSE,
                    .initial_bindings_count = sizeof (bindings) / sizeof (bindings[0u]),
                    .initial_bindings = bindings,
                    .tracking_name = kan_string_intern ("lighting_pass_set"),
                };

                singleton->scene_view[index].lighting_parameter_set =
                    kan_render_pipeline_parameter_set_create (render_context->render_context, &description);
            }
        }
    }

    if (!KAN_HANDLE_IS_VALID (g_buffer_pass_handle) || !KAN_HANDLE_IS_VALID (lighting_pass_handle))
    {
        return;
    }

    struct kan_render_graph_resource_request_t scene_request = {
        .context = render_context->render_context,
        .dependant_count = 0u,
        .dependant = NULL,
        .images_count = DEFERRED_RENDER_SCENE_IMAGE_COUNT,
        .images = image_requests,
        .frame_buffers_count = DEFERRED_RENDER_SCENE_FRAME_BUFFER_COUNT,
        .frame_buffers = frame_buffer_requests,
    };

    const struct kan_render_graph_resource_response_t *scene_responses[SPLIT_SCREEN_VIEWS];
    for (kan_loop_size_t index = 0u; index < SPLIT_SCREEN_VIEWS; ++index)
    {
        // Frame buffer requests must be filled in the same scope as
        // request function if we want to utilize anonymous arrays.

        frame_buffer_requests[DEFERRED_RENDER_SCENE_FRAME_BUFFER_G_BUFFER] =
            (struct kan_render_graph_resource_frame_buffer_request_t) {
                .pass = g_buffer_pass_handle,
                .attachments_count = 4u,
                .attachments =
                    (struct kan_render_graph_resource_frame_buffer_request_attachment_t[]) {
                        {
                            .image_index = DEFERRED_RENDER_SCENE_IMAGE_POSITION,
                            .image_layer = 0u,
                        },
                        {
                            .image_index = DEFERRED_RENDER_SCENE_IMAGE_NORMAL_SPECULAR,
                            .image_layer = 0u,
                        },
                        {
                            .image_index = DEFERRED_RENDER_SCENE_IMAGE_ALBEDO,
                            .image_layer = 0u,
                        },
                        {
                            .image_index = DEFERRED_RENDER_SCENE_IMAGE_DEPTH,
                            .image_layer = 0u,
                        },
                    },
            };

        frame_buffer_requests[DEFERRED_RENDER_SCENE_FRAME_BUFFER_LIGHTING] =
            (struct kan_render_graph_resource_frame_buffer_request_t) {
                .pass = lighting_pass_handle,
                .attachments_count = 2u,
                .attachments =
                    (struct kan_render_graph_resource_frame_buffer_request_attachment_t[]) {
                        {
                            .image_index = DEFERRED_RENDER_SCENE_IMAGE_VIEW_COLOR,
                            .image_layer = 0u,
                        },
                        {
                            .image_index = DEFERRED_RENDER_SCENE_IMAGE_DEPTH,
                            .image_layer = 0u,
                        },
                    },
            };

        scene_responses[index] =
            kan_render_graph_resource_management_singleton_request (render_resource_management, &scene_request);

        if (!scene_responses[index])
        {
            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                     "Failed to allocate scene view resources.")
            return;
        }
    }

    kan_render_pass_t shadow_pass_handle = KAN_HANDLE_INITIALIZE_INVALID;
    const struct kan_render_graph_resource_response_t *directional_light_shadow_response = NULL;
    const struct kan_render_graph_resource_response_t *point_light_shadow_responses[POINT_LIGHTS_WITH_SHADOWS] = {NULL};

    KAN_UP_VALUE_READ (shadow_pass, kan_render_graph_pass_t, name, &state->shadow_pass_name)
    {
        if (shadow_pass->attachments.size != 1u)
        {
            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                     "Shadow pass has unexpected count of attachments.")
            KAN_UP_QUERY_RETURN_VOID;
        }

        if (shadow_pass->variants.size != 1u)
        {
            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                     "Shadow pass has unexpected count of variants.")
            KAN_UP_QUERY_RETURN_VOID;
        }

        struct kan_render_graph_pass_variant_t *pass_variant =
            &((struct kan_render_graph_pass_variant_t *) shadow_pass->variants.data)[0u];

        if (pass_variant->pass_parameter_set_bindings.buffers.size != 1u)
        {
            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                     "Shadow pass has unexpected parameter set bindings.")
            KAN_UP_QUERY_RETURN_VOID;
        }

        shadow_pass_handle = shadow_pass->pass;
        struct kan_render_graph_pass_attachment_t *depth_attachment =
            &((struct kan_render_graph_pass_attachment_t *) shadow_pass->attachments.data)[0u];

        image_requests[DEFERRED_RENDER_SHADOW_IMAGE_DEPTH] = (struct kan_render_graph_resource_image_request_t) {
            .description =
                {
                    .format = depth_attachment->format,
                    .width = DIRECTIONAL_SHADOW_MAP_WIDTH,
                    .height = DIRECTIONAL_SHADOW_MAP_HEIGHT,
                    .depth = 1u,
                    .layers = 1u,
                    .mips = 1u,
                    .render_target = KAN_TRUE,
                    .supports_sampling = KAN_TRUE,
                    .tracking_name = NULL,
                },
            .internal = KAN_FALSE,
        };

        frame_buffer_requests[DEFERRED_RENDER_FLAT_SHADOW_FRAME_BUFFER_DEPTH] =
            (struct kan_render_graph_resource_frame_buffer_request_t) {
                .pass = shadow_pass->pass,
                .attachments_count = 1u,
                .attachments =
                    (struct kan_render_graph_resource_frame_buffer_request_attachment_t[]) {
                        {
                            .image_index = DEFERRED_RENDER_SHADOW_IMAGE_DEPTH,
                            .image_layer = 0u,
                        },
                    },
            };

        struct kan_render_graph_resource_request_t request = {
            .context = render_context->render_context,
            .dependant_count = SPLIT_SCREEN_VIEWS,
            .dependant = scene_responses,
            .images_count = DEFERRED_RENDER_SHADOW_IMAGE_COUNT,
            .images = image_requests,
            .frame_buffers_count = DEFERRED_RENDER_FLAT_SHADOW_FRAME_BUFFER_COUNT,
            .frame_buffers = frame_buffer_requests,
        };

        if (!(directional_light_shadow_response =
                  kan_render_graph_resource_management_singleton_request (render_resource_management, &request)))
        {
            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                     "Failed to allocate directional shadow map resources.")
            KAN_UP_QUERY_RETURN_VOID;
        }

        for (kan_loop_size_t light_index = 0u; light_index < POINT_LIGHTS_WITH_SHADOWS; ++light_index)
        {
            request.frame_buffers_count = DEFERRED_RENDER_CUBE_SHADOW_FRAME_BUFFER_COUNT;
            image_requests[DEFERRED_RENDER_SHADOW_IMAGE_DEPTH] = (struct kan_render_graph_resource_image_request_t) {
                .description =
                    {
                        .format = depth_attachment->format,
                        .width = POINT_SHADOW_MAP_WIDTH,
                        .height = POINT_SHADOW_MAP_HEIGHT,
                        .depth = 1u,
                        .layers = 6u,
                        .mips = 1u,
                        .render_target = KAN_TRUE,
                        .supports_sampling = KAN_TRUE,
                        .tracking_name = NULL,
                    },
                .internal = KAN_FALSE,
            };

            struct kan_render_graph_resource_frame_buffer_request_attachment_t
                attachments[DEFERRED_RENDER_CUBE_SHADOW_FRAME_BUFFER_COUNT];

            for (kan_loop_size_t frame_buffer_index = 0u;
                 frame_buffer_index < DEFERRED_RENDER_CUBE_SHADOW_FRAME_BUFFER_COUNT; ++frame_buffer_index)
            {
                attachments[frame_buffer_index] = (struct kan_render_graph_resource_frame_buffer_request_attachment_t) {
                    .image_index = DEFERRED_RENDER_SHADOW_IMAGE_DEPTH,
                    .image_layer = (kan_instance_size_t) frame_buffer_index,
                };

                frame_buffer_requests[frame_buffer_index] = (struct kan_render_graph_resource_frame_buffer_request_t) {
                    .pass = shadow_pass->pass,
                    .attachments_count = 1u,
                    .attachments = &attachments[frame_buffer_index],
                };
            }

            if (!(point_light_shadow_responses[light_index] =
                      kan_render_graph_resource_management_singleton_request (render_resource_management, &request)))
            {
                KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                         "Failed to allocate directional shadow map resources.")
                KAN_UP_QUERY_RETURN_VOID;
            }
        }

        struct kan_rpl_meta_buffer_t *shadow_pass_buffer_meta =
            &((struct kan_rpl_meta_buffer_t *) pass_variant->pass_parameter_set_bindings.buffers.data)[0u];
        initialize_shadow_pass_data_if_needed (render_context, &singleton->directional_light_shadow_pass, pass_variant,
                                               shadow_pass_buffer_meta);

        for (kan_loop_size_t light_index = 0u; light_index < POINT_LIGHTS_SHADOW_PASS_COUNT; ++light_index)
        {
            initialize_shadow_pass_data_if_needed (render_context, &singleton->point_light_shadow_passes[light_index],
                                                   pass_variant, shadow_pass_buffer_meta);
        }
    }

    if (!KAN_HANDLE_IS_VALID (shadow_pass_handle))
    {
        return;
    }

    kan_bool_t everything_rendered = KAN_TRUE;

    // Update directional light object set if possible.
    KAN_UP_VALUE_READ (directional_material, kan_render_material_loaded_t, name,
                       &config->directional_light_material_name)
    {
        if (!KAN_HANDLE_IS_VALID (singleton->directional_light_object_parameter_set))
        {
            struct kan_render_pipeline_parameter_set_description_t description = {
                .layout = directional_material->set_object,
                .stable_binding = KAN_FALSE,
                .initial_bindings_count = 0u,
                .initial_bindings = NULL,
                .tracking_name = kan_string_intern ("directional_light_set"),
            };

            singleton->directional_light_object_parameter_set =
                kan_render_pipeline_parameter_set_create (render_context->render_context, &description);
        }

        struct kan_render_parameter_update_description_t object_bindings[] = {
            {
                .binding = 0u,
                .image_binding =
                    {
                        .image = directional_light_shadow_response->images[DEFERRED_RENDER_SHADOW_IMAGE_DEPTH],
                        .array_index = 0u,
                        .layer_offset = 0u,
                        .layer_count = 1u,
                    },
            },
        };

        kan_render_pipeline_parameter_set_update (singleton->directional_light_object_parameter_set,
                                                  sizeof (object_bindings) / sizeof (object_bindings[0u]),
                                                  object_bindings);
    }

    // Update point light shared set if possible.
    KAN_UP_VALUE_READ (point_material, kan_render_material_loaded_t, name, &config->point_light_material_name)
    {
        if (point_material->set_shared_bindings.images.size != 1u)
        {
            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                     "Point light material has unexpected shared set bindings.")
            KAN_UP_QUERY_RETURN_VOID;
        }

        if (!KAN_HANDLE_IS_VALID (singleton->point_light_shared_parameter_set))
        {
            struct kan_render_pipeline_parameter_set_description_t description = {
                .layout = point_material->set_shared,
                .stable_binding = KAN_FALSE,
                .initial_bindings_count = 0u,
                .initial_bindings = NULL,
                .tracking_name = kan_string_intern ("point_light_set"),
            };

            singleton->point_light_shared_parameter_set =
                kan_render_pipeline_parameter_set_create (render_context->render_context, &description);
        }

        struct kan_render_parameter_update_description_t bindings[POINT_LIGHTS_WITH_SHADOWS];
        for (kan_loop_size_t light_index = 0u; light_index < POINT_LIGHTS_WITH_SHADOWS; ++light_index)
        {
            bindings[light_index] = (struct kan_render_parameter_update_description_t) {
                .binding = 0u,
                .image_binding =
                    {
                        .image = point_light_shadow_responses[light_index]->images[DEFERRED_RENDER_SHADOW_IMAGE_DEPTH],
                        .array_index = light_index,
                        .layer_offset = 0u,
                        // Cube map, therefore 6 layers.
                        .layer_count = 6u,
                    },

            };
        }

        kan_render_pipeline_parameter_set_update (singleton->point_light_shared_parameter_set,
                                                  sizeof (bindings) / sizeof (bindings[0u]), bindings);
    }

    // Scene view passes.

    const kan_time_size_t current_time = kan_precise_time_get_elapsed_nanoseconds ();
    const float current_phase =
        state->test_mode && !singleton->frame_checked ? 0.45f : 0.1f * 1e-9f * (float) (current_time % 10000000000u);

    struct kan_transform_3_t scene_camera_base_transform = kan_transform_3_get_identity ();
    scene_camera_base_transform.location.y = 5.0f;
    scene_camera_base_transform.location.z = -5.0f;
    scene_camera_base_transform.rotation = kan_make_quaternion_from_euler (KAN_PI / 6.0f, 0.0f, 0.0f);

    struct kan_float_matrix_4x4_t scene_camera_base_transform_matrix =
        kan_transform_3_to_float_matrix_4x4 (&scene_camera_base_transform);

    for (kan_loop_size_t index = 0u; index < SPLIT_SCREEN_VIEWS; ++index)
    {
        // We use reversed depth everywhere in this example.
        struct kan_float_matrix_4x4_t projection =
            kan_perspective_projection (KAN_PI_2, (float) viewport_width / (float) viewport_height, 100.0f, 0.01f);

        struct kan_transform_3_t camera_view_transform = kan_transform_3_get_identity ();
        camera_view_transform.rotation = kan_make_quaternion_from_euler (
            0.0f, 2.0f * current_phase * KAN_PI + 2.0f * KAN_PI * index / SPLIT_SCREEN_VIEWS, 0.0f);

        struct kan_float_matrix_4x4_t camera_view_transform_matrix =
            kan_transform_3_to_float_matrix_4x4 (&camera_view_transform);

        struct kan_float_matrix_4x4_t camera_global_transform_matrix = kan_float_matrix_4x4_multiply_for_transform (
            &camera_view_transform_matrix, &scene_camera_base_transform_matrix);

        struct kan_transform_3_t camera_global_transform =
            kan_float_matrix_4x4_to_transform_3 (&camera_global_transform_matrix);
        struct kan_float_matrix_4x4_t view = kan_float_matrix_4x4_inverse (&camera_global_transform_matrix);

        struct deferred_scene_view_parameters_t *pass_buffer_data = kan_render_buffer_patch (
            singleton->scene_view[index].view_parameters_buffer, 0u,
            kan_render_buffer_get_full_size (singleton->scene_view[index].view_parameters_buffer));

        pass_buffer_data->projection_view = kan_float_matrix_4x4_multiply (&projection, &view);
        pass_buffer_data->camera_position.x = camera_global_transform.location.x;
        pass_buffer_data->camera_position.y = camera_global_transform.location.y;
        pass_buffer_data->camera_position.z = camera_global_transform.location.z;
        pass_buffer_data->camera_position.z = 1.0f;

        pass_buffer_data->camera_forward = kan_extend_float_vector_3_t (
            kan_float_vector_3_normalized (kan_float_vector_3_rotate (kan_make_float_vector_3_t (0.0f, 0.0f, 1.0f),
                                                                      camera_global_transform.rotation)),
            1.0f);

        pass_buffer_data->camera_right = kan_extend_float_vector_3_t (
            kan_float_vector_3_normalized (kan_float_vector_3_rotate (kan_make_float_vector_3_t (1.0f, 0.0f, 0.0f),
                                                                      camera_global_transform.rotation)),
            1.0f);

        pass_buffer_data->camera_up = kan_extend_float_vector_3_t (
            kan_float_vector_3_normalized (kan_float_vector_3_rotate (kan_make_float_vector_3_t (0.0f, 1.0f, 0.0f),
                                                                      camera_global_transform.rotation)),
            1.0f);

        struct kan_render_parameter_update_description_t lighting_bindings[] = {
            {
                .binding = lighting_g_buffer_world_position_binding,
                .image_binding =
                    {
                        .image = scene_responses[index]->images[DEFERRED_RENDER_SCENE_IMAGE_POSITION],
                        .array_index = 0u,
                        .layer_offset = 0u,
                        .layer_count = 1u,
                    },
            },
            {
                .binding = lighting_g_buffer_normal_shininess_binding,
                .image_binding =
                    {
                        .image = scene_responses[index]->images[DEFERRED_RENDER_SCENE_IMAGE_NORMAL_SPECULAR],
                        .array_index = 0u,
                        .layer_offset = 0u,
                        .layer_count = 1u,
                    },
            },
            {
                .binding = lighting_g_buffer_diffuse_binding,
                .image_binding =
                    {
                        .image = scene_responses[index]->images[DEFERRED_RENDER_SCENE_IMAGE_ALBEDO],
                        .array_index = 0u,
                        .layer_offset = 0u,
                        .layer_count = 1u,
                    },
            },
        };

        kan_render_pipeline_parameter_set_update (singleton->scene_view[index].lighting_parameter_set,
                                                  sizeof (lighting_bindings) / sizeof (lighting_bindings[0u]),
                                                  lighting_bindings);

        struct kan_render_viewport_bounds_t viewport_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float) viewport_width,
            .height = (float) viewport_height,
            .depth_min = 0.0f,
            .depth_max = 1.0f,
        };

        struct kan_render_integer_region_t scissor = {
            .x = 0,
            .y = 0,
            .width = viewport_width,
            .height = viewport_height,
        };

        struct kan_render_clear_value_t g_buffer_clear_values[] = {
            {
                .color = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            {
                .color = {0.0f, 0.0f, 0.0f, 0.0f},
            },
            {
                .color = {0.0f, 0.0f, 0.0f, 1.0f},
            },
            {
                // We use reversed depth everywhere in this example.
                .depth_stencil = {0.0f, 0u},
            },
        };

        kan_render_pass_instance_t g_buffer_pass_instance = kan_render_pass_instantiate (
            g_buffer_pass_handle, scene_responses[index]->frame_buffers[DEFERRED_RENDER_SCENE_FRAME_BUFFER_G_BUFFER],
            &viewport_bounds, &scissor, g_buffer_clear_values);

        kan_render_pass_instance_add_checkpoint_dependency (g_buffer_pass_instance,
                                                            scene_responses[index]->usage_begin_checkpoint);

        everything_rendered &= try_render_opaque_objects (state, singleton, g_buffer_pass_instance,
                                                          singleton->scene_view[index].g_buffer_parameter_set,
                                                          state->g_buffer_pass_name, config);

        struct kan_render_clear_value_t lighting_clear_values[] = {
            {
                .color =
                    {
                        kan_color_transfer_rgb_to_srgb_approximate (0.1f),
                        kan_color_transfer_rgb_to_srgb_approximate (0.1f),
                        kan_color_transfer_rgb_to_srgb_approximate (0.2f),
                        1.0f,
                    },
            },
            {
                // Should not be cleared, actually.
                .depth_stencil = {0.0f, 0u},
            },
        };

        kan_render_pass_instance_t lighting_pass_instance = kan_render_pass_instantiate (
            lighting_pass_handle, scene_responses[index]->frame_buffers[DEFERRED_RENDER_SCENE_FRAME_BUFFER_LIGHTING],
            &viewport_bounds, &scissor, lighting_clear_values);

        kan_render_pass_instance_add_instance_dependency (lighting_pass_instance, g_buffer_pass_instance);
        kan_render_pass_instance_checkpoint_add_instance_dependency (scene_responses[index]->usage_end_checkpoint,
                                                                     lighting_pass_instance);

        everything_rendered &= try_render_lighting (state, render_context, singleton, lighting_pass_instance,
                                                    &singleton->scene_view[index], config);

        if (everything_rendered)
        {
            struct kan_render_integer_region_t image_region = scissor;
            struct kan_render_integer_region_t surface_region = scissor;
            surface_region.x += viewport_step_x * index;
            surface_region.y += viewport_step_y * index;

            kan_render_backend_system_present_image_on_surface (
                singleton->window_surface, scene_responses[index]->images[DEFERRED_RENDER_SCENE_IMAGE_VIEW_COLOR], 0u,
                surface_region, image_region, lighting_pass_instance);

            if (state->test_mode && !singleton->frame_checked)
            {
                if (KAN_HANDLE_IS_VALID (singleton->test_read_back_statuses[index]))
                {
                    kan_render_read_back_status_destroy (singleton->test_read_back_statuses[index]);
                }

                singleton->test_read_back_statuses[index] = kan_render_request_read_back_from_image (
                    scene_responses[index]->images[DEFERRED_RENDER_SCENE_IMAGE_VIEW_COLOR], 0u, 0u,
                    singleton->test_read_back_buffer, index * viewport_width * viewport_height * 4u,
                    lighting_pass_instance);
            }
        }
    }

    // Directional light shadow pass.
    {
        struct kan_float_matrix_4x4_t *pass_buffer_data = kan_render_buffer_patch (
            singleton->directional_light_shadow_pass.parameters_buffer, 0u,
            kan_render_buffer_get_full_size (singleton->directional_light_shadow_pass.parameters_buffer));
        *pass_buffer_data = singleton->directional_light_shadow_projection_view;

        struct kan_render_viewport_bounds_t viewport_bounds = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float) DIRECTIONAL_SHADOW_MAP_WIDTH,
            .height = (float) DIRECTIONAL_SHADOW_MAP_HEIGHT,
            .depth_min = 0.0f,
            .depth_max = 1.0f,
        };

        struct kan_render_integer_region_t scissor = {
            .x = 0,
            .y = 0,
            .width = DIRECTIONAL_SHADOW_MAP_WIDTH,
            .height = DIRECTIONAL_SHADOW_MAP_HEIGHT,
        };

        struct kan_render_clear_value_t clear_values[] = {
            {
                // We use reversed depth everywhere in this example.
                .depth_stencil = {0.0f, 0u},
            },
        };

        kan_render_pass_instance_t pass_instance = kan_render_pass_instantiate (
            shadow_pass_handle,
            directional_light_shadow_response->frame_buffers[DEFERRED_RENDER_FLAT_SHADOW_FRAME_BUFFER_DEPTH],
            &viewport_bounds, &scissor, clear_values);

        kan_render_pass_instance_add_checkpoint_dependency (pass_instance,
                                                            directional_light_shadow_response->usage_begin_checkpoint);

        kan_render_pass_instance_checkpoint_add_instance_dependency (
            directional_light_shadow_response->usage_end_checkpoint, pass_instance);

        everything_rendered &= try_render_opaque_objects (state, singleton, pass_instance,
                                                          singleton->directional_light_shadow_pass.parameter_set,
                                                          state->shadow_pass_name, config);
    }

    // Point light shadow passes.
    for (kan_loop_size_t point_light_index = 0u; point_light_index < POINT_LIGHTS_WITH_SHADOWS; ++point_light_index)
    {
        const struct kan_render_graph_resource_response_t *light_response =
            point_light_shadow_responses[point_light_index];

        for (kan_loop_size_t side = 0u; side < DEFERRED_RENDER_CUBE_SHADOW_FRAME_BUFFER_COUNT; ++side)
        {
            const kan_instance_size_t pass_index =
                point_light_index * DEFERRED_RENDER_CUBE_SHADOW_FRAME_BUFFER_COUNT + side;
            const struct deferred_render_shadow_pass_data_t *pass_data =
                &singleton->point_light_shadow_passes[pass_index];

            static const struct kan_float_vector_3_t euler_rotations[DEFERRED_RENDER_CUBE_SHADOW_FRAME_BUFFER_COUNT] = {
                // Right.
                {0.0f, (float) M_PI_2, 0.0f},
                // Left.
                {0.0f, (float) -M_PI_2, 0.0f},
                // Up.
                {(float) -M_PI_2, 0.0f, 0.0f},
                // Down.
                {(float) M_PI_2, 0.0f, 0.0f},
                // Forward.
                {0.0f, 0.0f, 0.0f},
                // Back.
                {0.0f, (float) M_PI, 0.0f},
            };

            struct kan_float_matrix_4x4_t projection =
                kan_perspective_projection ((float) M_PI_2, 1.0f, POINT_LIGHTS_WITH_SHADOWS_DISTANCE, 0.01f);

            struct kan_transform_3_t light_transform = kan_transform_3_get_identity ();
            light_transform.location = singleton->point_lights_with_shadows_positions[point_light_index];
            light_transform.rotation = kan_make_quaternion_from_euler_vector (euler_rotations[side]);

            const struct kan_float_matrix_4x4_t light_view_transform_matrix =
                kan_transform_3_to_float_matrix_4x4 (&light_transform);
            const struct kan_float_matrix_4x4_t view = kan_float_matrix_4x4_inverse (&light_view_transform_matrix);

            struct kan_float_matrix_4x4_t *pass_buffer_data = kan_render_buffer_patch (
                pass_data->parameters_buffer, 0u, kan_render_buffer_get_full_size (pass_data->parameters_buffer));
            *pass_buffer_data = kan_float_matrix_4x4_multiply (&projection, &view);

            struct kan_render_viewport_bounds_t viewport_bounds = {
                .x = 0.0f,
                .y = 0.0f,
                .width = (float) POINT_SHADOW_MAP_WIDTH,
                .height = (float) POINT_SHADOW_MAP_HEIGHT,
                .depth_min = 0.0f,
                .depth_max = 1.0f,
            };

            struct kan_render_integer_region_t scissor = {
                .x = 0,
                .y = 0,
                .width = POINT_SHADOW_MAP_WIDTH,
                .height = POINT_SHADOW_MAP_HEIGHT,
            };

            struct kan_render_clear_value_t clear_values[] = {
                {
                    // We use reversed depth everywhere in this example.
                    .depth_stencil = {0.0f, 0u},
                },
            };

            kan_render_pass_instance_t pass_instance = kan_render_pass_instantiate (
                shadow_pass_handle, light_response->frame_buffers[side], &viewport_bounds, &scissor, clear_values);

            kan_render_pass_instance_add_checkpoint_dependency (pass_instance, light_response->usage_begin_checkpoint);

            kan_render_pass_instance_checkpoint_add_instance_dependency (light_response->usage_end_checkpoint,
                                                                         pass_instance);

            everything_rendered &= try_render_opaque_objects (state, singleton, pass_instance, pass_data->parameter_set,
                                                              state->shadow_pass_name, config);
        }
    }

    if (everything_rendered)
    {
        if (!singleton->frame_checked)
        {
            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_INFO, "First frame to render index %u.",
                     (unsigned) singleton->test_frames_count)
        }

        singleton->frame_checked = KAN_TRUE;
    }
}

APPLICATION_FRAMEWORK_EXAMPLES_DEFERRED_RENDER_API void kan_universe_mutator_execute_deferred_render (
    kan_cpu_job_t job, struct deferred_render_state_t *state)
{
    KAN_UP_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    KAN_UP_SINGLETON_READ (render_graph, kan_render_graph_resource_management_singleton_t)
    KAN_UP_SINGLETON_READ (render_material_singleton, kan_render_material_singleton_t)
    KAN_UP_SINGLETON_READ (render_material_instance_singleton, kan_render_material_instance_singleton_t)
    KAN_UP_SINGLETON_READ (resource_provider, kan_resource_provider_singleton_t)
    KAN_UP_SINGLETON_WRITE (singleton, example_deferred_render_singleton_t)
    {
        if (!KAN_HANDLE_IS_VALID (render_context->render_context))
        {
            KAN_UP_MUTATOR_RETURN;
        }

        if (!KAN_HANDLE_IS_VALID (singleton->window_handle))
        {
            enum kan_platform_window_flag_t flags = kan_render_get_required_window_flags ();
            if (!state->test_mode)
            {
                flags |= KAN_PLATFORM_WINDOW_FLAG_RESIZABLE;
            }

            // We create window with simple initial size and support resizing after that.
            singleton->window_handle = kan_application_system_window_create (
                state->application_system_handle, "application_framework_example_deferred_render", FIXED_TEST_WIDTH,
                FIXED_TEST_HEIGHT, flags);

            enum kan_render_surface_present_mode_t present_modes[] = {
                KAN_RENDER_SURFACE_PRESENT_MODE_MAILBOX,
                KAN_RENDER_SURFACE_PRESENT_MODE_IMMEDIATE,
                KAN_RENDER_SURFACE_PRESENT_MODE_INVALID,
            };

            singleton->window_surface =
                kan_render_backend_system_create_surface (state->render_backend_system_handle, singleton->window_handle,
                                                          present_modes, kan_string_intern ("window_surface"));

            if (!KAN_HANDLE_IS_VALID (singleton->window_surface))
            {
                KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR, "Failed to create surface.")
                kan_application_framework_system_request_exit (state->application_framework_system_handle, 1);
                KAN_UP_MUTATOR_RETURN;
            }

            kan_application_system_window_raise (state->application_system_handle, singleton->window_handle);
        }

        if (state->test_mode && !KAN_HANDLE_IS_VALID (singleton->test_read_back_buffer))
        {
            singleton->test_read_back_buffer = kan_render_buffer_create (
                render_context->render_context, KAN_RENDER_BUFFER_TYPE_READ_BACK_STORAGE,
                FIXED_TEST_WIDTH * FIXED_TEST_HEIGHT * 4u, NULL, kan_string_intern ("test_read_back_buffer"));
        }

        if (!KAN_TYPED_ID_32_IS_VALID (singleton->config_request_id))
        {
            KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
            {
                request->request_id = kan_next_resource_request_id (resource_provider);
                singleton->config_request_id = request->request_id;
                request->type = kan_string_intern ("deferred_render_config_t");
                request->name = kan_string_intern ("root_config");
            }
        }

        if (!singleton->object_buffers_initialized && KAN_HANDLE_IS_VALID (render_context->render_context))
        {
            example_deferred_render_singleton_initialize_object_buffers (singleton, render_context->render_context);
        }

        KAN_UP_EVENT_FETCH (pass_updated, kan_render_graph_pass_updated_event_t)
        {
            // Reset pass data in order to rebuild it in render function.
            for (kan_loop_size_t index = 0u; index < SPLIT_SCREEN_VIEWS; ++index)
            {
                deferred_render_scene_view_data_shutdown (&singleton->scene_view[index]);
                deferred_render_scene_view_data_init (&singleton->scene_view[index]);
            }

            deferred_render_shadow_pass_data_shutdown (&singleton->directional_light_shadow_pass);
            deferred_render_shadow_pass_data_init (&singleton->directional_light_shadow_pass);

            for (kan_loop_size_t index = 0u; index < POINT_LIGHTS_SHADOW_PASS_COUNT; ++index)
            {
                deferred_render_shadow_pass_data_shutdown (&singleton->point_light_shadow_passes[index]);
                deferred_render_shadow_pass_data_init (&singleton->point_light_shadow_passes[index]);
            }
        }

        KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &singleton->config_request_id)
        {
            if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
            {
                KAN_UP_VALUE_READ (container, KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (deferred_render_config_t),
                                   container_id, &request->provided_container_id)
                {
                    const struct deferred_render_config_t *test_config =
                        KAN_RESOURCE_PROVIDER_CONTAINER_GET (deferred_render_config_t, container);

                    if (!KAN_TYPED_ID_32_IS_VALID (singleton->ground_material_instance_usage_id))
                    {
                        KAN_UP_INDEXED_INSERT (usage, kan_render_material_instance_usage_t)
                        {
                            usage->usage_id = kan_next_material_instance_usage_id (render_material_instance_singleton);
                            singleton->ground_material_instance_usage_id = usage->usage_id;
                            usage->name = test_config->ground_material_instance_name;
                        }
                    }

                    if (!KAN_TYPED_ID_32_IS_VALID (singleton->cube_material_instance_usage_id))
                    {
                        KAN_UP_INDEXED_INSERT (usage, kan_render_material_instance_usage_t)
                        {
                            usage->usage_id = kan_next_material_instance_usage_id (render_material_instance_singleton);
                            singleton->cube_material_instance_usage_id = usage->usage_id;
                            usage->name = test_config->cube_material_instance_name;
                        }
                    }

                    if (!KAN_TYPED_ID_32_IS_VALID (singleton->ambient_light_material_usage_id))
                    {
                        KAN_UP_INDEXED_INSERT (usage, kan_render_material_usage_t)
                        {
                            usage->usage_id = kan_next_material_usage_id (render_material_singleton);
                            singleton->ambient_light_material_usage_id = usage->usage_id;
                            usage->name = test_config->ambient_light_material_name;
                        }
                    }

                    if (!KAN_TYPED_ID_32_IS_VALID (singleton->directional_light_material_usage_id))
                    {
                        KAN_UP_INDEXED_INSERT (usage, kan_render_material_usage_t)
                        {
                            usage->usage_id = kan_next_material_usage_id (render_material_singleton);
                            singleton->directional_light_material_usage_id = usage->usage_id;
                            usage->name = test_config->directional_light_material_name;
                        }
                    }

                    if (!KAN_TYPED_ID_32_IS_VALID (singleton->point_light_material_usage_id))
                    {
                        KAN_UP_INDEXED_INSERT (usage, kan_render_material_usage_t)
                        {
                            usage->usage_id = kan_next_material_usage_id (render_material_singleton);
                            singleton->point_light_material_usage_id = usage->usage_id;
                            usage->name = test_config->point_light_material_name;
                        }
                    }

                    if (state->test_mode)
                    {
                        if (test_config->test_expectations.size != SPLIT_SCREEN_VIEWS)
                        {
                            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                                     "Count of test expectations is not equal to the split screen views count.")
                            kan_application_framework_system_request_exit (state->application_framework_system_handle,
                                                                           1);
                            KAN_UP_MUTATOR_RETURN;
                        }

                        for (kan_loop_size_t index = 0u; index < SPLIT_SCREEN_VIEWS; ++index)
                        {
                            if (!KAN_TYPED_ID_32_IS_VALID (singleton->test_expectation_requests[index]))
                            {
                                KAN_UP_INDEXED_INSERT (expectation_request, kan_resource_request_t)
                                {
                                    expectation_request->request_id = kan_next_resource_request_id (resource_provider);
                                    singleton->test_expectation_requests[index] = expectation_request->request_id;
                                    expectation_request->type = NULL;
                                    expectation_request->name =
                                        ((kan_interned_string_t *) test_config->test_expectations.data)[index];
                                }
                            }
                        }
                    }

                    KAN_UP_EVENT_FETCH (material_updated, kan_render_material_updated_event_t)
                    {
                        // Destroy parameter sets on hot reload in order to create new ones during next render.
                        if (material_updated->name == test_config->directional_light_material_name)
                        {
                            if (KAN_HANDLE_IS_VALID (singleton->directional_light_object_parameter_set))
                            {
                                kan_render_pipeline_parameter_set_destroy (
                                    singleton->directional_light_object_parameter_set);
                                singleton->directional_light_object_parameter_set =
                                    KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_t);
                            }
                        }
                        else if (material_updated->name == test_config->point_light_material_name)
                        {
                            if (KAN_HANDLE_IS_VALID (singleton->point_light_shared_parameter_set))
                            {
                                kan_render_pipeline_parameter_set_destroy (singleton->point_light_shared_parameter_set);
                                singleton->point_light_shared_parameter_set =
                                    KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_t);
                            }
                        }
                    }

                    if (KAN_HANDLE_IS_VALID (render_context->render_context) && render_context->frame_scheduled)
                    {
                        try_render_frame (state, render_context, render_graph, singleton, test_config);
                    }
                }
            }
        }

        ++singleton->test_frames_count;
        if (state->test_mode)
        {
            if (singleton->frame_checked)
            {
                kan_bool_t ready_for_testing = KAN_TRUE;
                for (kan_loop_size_t index = 0u; index < SPLIT_SCREEN_VIEWS; ++index)
                {
                    if (kan_read_read_back_status_get (singleton->test_read_back_statuses[index]) !=
                        KAN_RENDER_READ_BACK_STATE_FINISHED)
                    {
                        ready_for_testing = KAN_FALSE;
                    }

                    KAN_UP_VALUE_READ (expectation_request, kan_resource_request_t, request_id,
                                       &singleton->test_expectation_requests[index])
                    {
                        if (!expectation_request->provided_third_party.data)
                        {
                            ready_for_testing = KAN_FALSE;
                        }
                    }

                    if (!ready_for_testing)
                    {
                        break;
                    }
                }

                if (ready_for_testing)
                {
                    int exit_code = 0;
                    KAN_LOG (application_framework_example_deferred_render, KAN_LOG_INFO,
                             "Shutting down in test mode...")

                    const uint8_t *read_back_data = kan_render_buffer_read (singleton->test_read_back_buffer);
                    struct kan_file_system_path_container_t output_path_container;
                    kan_file_system_path_container_copy_string (&output_path_container,
                                                                "deferred_render_test_result_0.png");
                    _Static_assert (SPLIT_SCREEN_VIEWS <= 9u, "Not too many splits for testing.");

                    for (kan_loop_size_t index = 0u; index < SPLIT_SCREEN_VIEWS;
                         ++index, ++output_path_container.path[28u])
                    {
                        struct kan_image_raw_data_t frame_raw_data;
                        frame_raw_data.width = FIXED_TEST_WIDTH / SPLIT_SCREEN_VIEWS;
                        frame_raw_data.height = FIXED_TEST_HEIGHT;
                        frame_raw_data.data =
                            (uint8_t *) read_back_data + index * frame_raw_data.width * frame_raw_data.height * 4u;

                        struct kan_stream_t *output_stream =
                            kan_direct_file_stream_open_for_write (output_path_container.path, KAN_TRUE);

                        if (output_stream)
                        {
                            if (!kan_image_save (output_stream, KAN_IMAGE_SAVE_FORMAT_PNG, &frame_raw_data))
                            {
                                KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                                         "Failed to write result %lu.", (unsigned long) index)
                                exit_code = 1;
                            }

                            output_stream->operations->close (output_stream);
                        }
                        else
                        {
                            KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                                     "Failed to open file to write result %lu.", (unsigned long) index)
                            exit_code = 1;
                        }

                        KAN_UP_VALUE_READ (expectation_request, kan_resource_request_t, request_id,
                                           &singleton->test_expectation_requests[index])
                        {
                            struct kan_image_raw_data_t expectation_data;
                            if (kan_image_load_from_buffer (expectation_request->provided_third_party.data,
                                                            expectation_request->provided_third_party.size,
                                                            &expectation_data))
                            {
                                if (expectation_data.width != frame_raw_data.width ||
                                    expectation_data.height != frame_raw_data.height)
                                {
                                    KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                                             "Expectation %lu size doesn't match with frame size.",
                                             (unsigned long) index)
                                    exit_code = 1;
                                }
                                else
                                {
                                    const uint32_t *frame = (const uint32_t *) frame_raw_data.data;
                                    const uint32_t *expectation = (const uint32_t *) expectation_data.data;

                                    const kan_loop_size_t pixel_count = frame_raw_data.width * frame_raw_data.height;
                                    kan_loop_size_t error_count = 0u;
                                    // Not more than 1% of errors.
                                    kan_loop_size_t max_error_count = pixel_count / 100u;

                                    for (kan_loop_size_t pixel_index = 0u; pixel_index < pixel_count; ++pixel_index)
                                    {
                                        if (kan_are_colors_different (frame[pixel_index], expectation[pixel_index], 3u))
                                        {
                                            ++error_count;
                                        }
                                    }

                                    if (error_count > max_error_count)
                                    {
                                        KAN_LOG (
                                            application_framework_example_deferred_render, KAN_LOG_ERROR,
                                            "Frame and expectation have different data at view %lu: different %.3f%%.",
                                            (unsigned long) index, 100.0f * (float) error_count / (float) pixel_count)
                                        exit_code = 1;
                                    }
                                }

                                kan_image_raw_data_shutdown (&expectation_data);
                            }
                            else
                            {
                                KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                                         "Failed to decode expectation %lu.", (unsigned long) index)
                                exit_code = 1;
                            }
                        }
                    }

                    kan_application_framework_system_request_exit (state->application_framework_system_handle,
                                                                   exit_code);
                    KAN_UP_MUTATOR_RETURN;
                }
            }

            if (120u < singleton->test_frames_count)
            {
                KAN_LOG (application_framework_example_deferred_render, KAN_LOG_INFO, "Shutting down in test mode...")
                KAN_LOG (application_framework_example_deferred_render, KAN_LOG_ERROR,
                         "Time elapsed, but wasn't able to do any testing.")
                kan_application_framework_system_request_exit (state->application_framework_system_handle, 1);
                KAN_UP_MUTATOR_RETURN;
            }
        }
    }

    KAN_UP_MUTATOR_RETURN;
}
