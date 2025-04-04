#include <application_framework_examples_test_render_material_api.h>

#include <stdio.h>
#include <string.h>

#include <kan/context/all_system_names.h>
#include <kan/context/application_framework_system.h>
#include <kan/context/application_system.h>
#include <kan/log/logging.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe/universe.h>
#include <kan/universe_render_foundation/material.h>
#include <kan/universe_render_foundation/material_instance.h>
#include <kan/universe_render_foundation/render_graph.h>
#include <kan/universe_render_foundation/texture.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>
#include <kan/universe_time/universe_time.h>

// TODO: This is a temporary test-only example which is used until enough
//       render functionality is implemented in order to create proper examples.

KAN_LOG_DEFINE_CATEGORY (application_framework_example_test_render_material);

struct test_render_material_config_t
{
    kan_interned_string_t material_instance_name;
};

KAN_REFLECTION_STRUCT_META (test_render_material_config_t)
APPLICATION_FRAMEWORK_EXAMPLES_TEST_RENDER_MATERIAL_API struct kan_resource_resource_type_meta_t
    test_render_material_config_resource_type_meta = {
        .root = KAN_TRUE,
};

KAN_REFLECTION_STRUCT_FIELD_META (test_render_material_config_t, material_instance_name)
APPLICATION_FRAMEWORK_EXAMPLES_TEST_RENDER_MATERIAL_API struct kan_resource_reference_meta_t
    root_config_required_sum_reference_meta = {
        .type = "kan_resource_material_instance_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

struct test_render_material_vertex_t
{
    struct kan_float_vector_3_t position;
    struct kan_float_vector_2_t uv;
};

struct example_test_render_material_singleton_t
{
    kan_application_system_window_t window_handle;
    kan_render_surface_t window_surface;
    kan_instance_size_t test_frames_count;
    kan_resource_request_id_t config_request_id;
    kan_render_material_instance_usage_id_t material_instance_usage_id;
    kan_bool_t frame_checked;

    kan_bool_t object_buffers_initialized;
    kan_render_buffer_t vertex_buffer;
    kan_render_buffer_t index_buffer;
    kan_render_size_t index_count;
    kan_render_frame_lifetime_buffer_allocator_t instanced_data_allocator;

    kan_render_pipeline_parameter_set_t pass_parameter_set;
    kan_render_buffer_t pass_buffer;
};

APPLICATION_FRAMEWORK_EXAMPLES_TEST_RENDER_MATERIAL_API void example_test_render_material_singleton_init (
    struct example_test_render_material_singleton_t *instance)
{
    instance->window_handle = KAN_HANDLE_SET_INVALID (kan_application_system_window_t);
    instance->window_surface = KAN_HANDLE_SET_INVALID (kan_render_surface_t);
    instance->test_frames_count = 0u;
    instance->config_request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
    instance->material_instance_usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_render_material_instance_usage_id_t);
    instance->frame_checked = KAN_FALSE;

    instance->object_buffers_initialized = KAN_FALSE;
    instance->vertex_buffer = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
    instance->index_buffer = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
    instance->index_count = 0u;
    instance->instanced_data_allocator = KAN_HANDLE_SET_INVALID (kan_render_frame_lifetime_buffer_allocator_t);

    instance->pass_parameter_set = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_t);
    instance->pass_buffer = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
}

static void example_test_render_material_singleton_initialize_object_buffers (
    struct example_test_render_material_singleton_t *instance, kan_render_context_t render_context)
{
    KAN_ASSERT (!instance->object_buffers_initialized)

    struct test_render_material_vertex_t vertices[] = {
        {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f}},   {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f}},
        {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}},     {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f}}, {{-0.5f, 0.5f, -0.5f}, {-1.0f, 1.0f}},
        {{0.5f, -0.5f, -0.5f}, {2.0f, 0.0f}},   {{0.5f, 0.5f, -0.5f}, {2.0f, 1.0f}},
        {{0.5f, 0.5f, -0.5f}, {1.0f, 2.0f}},    {{-0.5f, 0.5f, -0.5f}, {0.0f, 2.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f}}, {{0.5f, -0.5f, -0.5f}, {1.0f, -1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {0.0f, -2.0f}},  {{0.5f, 0.5f, -0.5f}, {1.0f, -2.0f}},
    };

    instance->vertex_buffer = kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_ATTRIBUTE,
                                                        sizeof (vertices), vertices, kan_string_intern ("vertices"));

    uint16_t indices[] = {
        0u, 1u, 2u, 2u, 3u, 0u, 0u,  3u,  5u, 5u, 4u, 0u,  1u,  6u,  7u,  7u,  2u,  1u,
        2u, 8u, 9u, 9u, 3u, 2u, 10u, 11u, 1u, 1u, 0u, 10u, 12u, 13u, 11u, 11u, 10u, 12u,
    };

    instance->index_count = sizeof (indices) / sizeof (indices[0u]);
    instance->index_buffer = kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_INDEX_16,
                                                       sizeof (indices), indices, kan_string_intern ("indices"));

    instance->instanced_data_allocator =
        kan_render_frame_lifetime_buffer_allocator_create (render_context, KAN_RENDER_BUFFER_TYPE_ATTRIBUTE, 4096u,
                                                           KAN_FALSE, kan_string_intern ("instanced_data_allocator"));
    instance->object_buffers_initialized = KAN_TRUE;
}

APPLICATION_FRAMEWORK_EXAMPLES_TEST_RENDER_MATERIAL_API void example_test_render_material_singleton_shutdown (
    struct example_test_render_material_singleton_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->vertex_buffer))
    {
        kan_render_buffer_destroy (instance->vertex_buffer);
    }

    if (KAN_HANDLE_IS_VALID (instance->index_buffer))
    {
        kan_render_buffer_destroy (instance->index_buffer);
    }

    if (KAN_HANDLE_IS_VALID (instance->instanced_data_allocator))
    {
        kan_render_frame_lifetime_buffer_allocator_destroy (instance->instanced_data_allocator);
    }

    if (KAN_HANDLE_IS_VALID (instance->pass_parameter_set))
    {
        kan_render_pipeline_parameter_set_destroy (instance->pass_parameter_set);
    }

    if (KAN_HANDLE_IS_VALID (instance->pass_buffer))
    {
        kan_render_buffer_destroy (instance->pass_buffer);
    }
}

struct test_render_material_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (test_render_material)
    KAN_UP_BIND_STATE (test_render_material, state)

    kan_context_system_t application_system_handle;
    kan_context_system_t application_framework_system_handle;
    kan_context_system_t render_backend_system_handle;
    kan_bool_t test_mode;

    kan_interned_string_t scene_pass_name;
};

APPLICATION_FRAMEWORK_EXAMPLES_TEST_RENDER_MATERIAL_API void kan_universe_mutator_deploy_test_render_material (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct test_render_material_state_t *state)
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

    state->scene_pass_name = kan_string_intern ("test_scene_pass");
}

#define TEST_WIDTH 512u
#define TEST_HEIGHT 512u
#define TEST_WIDTH_FLOAT 512.0f
#define TEST_HEIGHT_FLOAT 512.0f

/// \brief As this example is temporary and is used for manual launches,
///        check for custom material logic was hidden under this define.
// #define TEST_USE_CUSTOM_MATERIAL

static void try_render_frame (struct test_render_material_state_t *state,
                              const struct kan_render_context_singleton_t *render_context,
                              struct example_test_render_material_singleton_t *singleton,
                              const struct kan_render_graph_pass_t *pass,
                              struct kan_render_graph_pass_instance_allocation_t *pass_allocation,
                              kan_interned_string_t material_instance_name)
{
    if (!singleton->object_buffers_initialized)
    {
        return;
    }

    struct kan_float_matrix_4x4_t projection;
    kan_perspective_projection (&projection, KAN_PI_2, TEST_WIDTH_FLOAT / TEST_HEIGHT_FLOAT, 0.01f, 1000.0f);

    struct kan_transform_3_t camera_transform = kan_transform_3_get_identity ();
    camera_transform.location.y = 1.0f;
    camera_transform.location.z = -1.0f;
    camera_transform.rotation = kan_make_quaternion_from_euler (KAN_PI / 6.0f, 0.0f, 0.0f);

    struct kan_float_matrix_4x4_t camera_transform_matrix;
    kan_transform_3_to_float_matrix_4x4 (&camera_transform, &camera_transform_matrix);

    struct kan_float_matrix_4x4_t view;
    kan_float_matrix_4x4_inverse (&camera_transform_matrix, &view);

    struct kan_float_matrix_4x4_t projection_view;
    kan_float_matrix_4x4_multiply (&projection, &view, &projection_view);

    // Only one buffer in pass parameter set in one variant for the sake of simplicity.
    KAN_ASSERT (pass->variants.size == 1u)
    struct kan_render_graph_pass_variant_t *pass_variant =
        &((struct kan_render_graph_pass_variant_t *) pass->variants.data)[0u];

    KAN_ASSERT (pass_variant->pass_parameter_set_bindings.buffers.size == 1u)
    struct kan_rpl_meta_buffer_t *pass_meta_buffer =
        &((struct kan_rpl_meta_buffer_t *) pass_variant->pass_parameter_set_bindings.buffers.data)[0u];

    if (!KAN_HANDLE_IS_VALID (singleton->pass_buffer))
    {
        KAN_ASSERT (!KAN_HANDLE_IS_VALID (singleton->pass_parameter_set))
        singleton->pass_buffer =
            kan_render_buffer_create (render_context->render_context, KAN_RENDER_BUFFER_TYPE_UNIFORM,
                                      pass_meta_buffer->main_size, NULL, kan_string_intern ("pass_data"));
    }

    if (!KAN_HANDLE_IS_VALID (singleton->pass_parameter_set))
    {
        KAN_ASSERT (KAN_HANDLE_IS_VALID (pass_variant->pass_parameter_set_layout))
        struct kan_render_parameter_update_description_t bindings[] = {{
            .binding = pass_meta_buffer->binding,
            .buffer_binding =
                {
                    .buffer = singleton->pass_buffer,
                    .offset = 0u,
                    .range = pass_meta_buffer->main_size,
                },
        }};

        struct kan_render_pipeline_parameter_set_description_t description = {
            .layout = pass_variant->pass_parameter_set_layout,
            .stable_binding = KAN_TRUE,
            .initial_bindings_count = sizeof (bindings) / sizeof (bindings[0u]),
            .initial_bindings = bindings,
            .tracking_name = kan_string_intern ("pass"),
        };

        singleton->pass_parameter_set =
            kan_render_pipeline_parameter_set_create (render_context->render_context, &description);
    }

    void *pass_buffer_data = kan_render_buffer_patch (singleton->pass_buffer, 0u, pass_meta_buffer->main_size);
    // For the sake of the simple example, we just assume that projection view matrix is at the beginning of the buffer.
    memcpy (pass_buffer_data, &projection_view, sizeof (projection_view));

#if defined(TEST_USE_CUSTOM_MATERIAL)
    KAN_UP_VALUE_READ (material_instance, kan_render_material_instance_custom_loaded_t, usage_id,
                       &singleton->material_instance_usage_id)
#else
    KAN_UP_VALUE_READ (material_instance, kan_render_material_instance_loaded_t, name, &material_instance_name)
#endif
    {
#if defined(TEST_USE_CUSTOM_MATERIAL)
        KAN_UP_VALUE_UPDATE (parameter, kan_render_material_instance_custom_instanced_parameter_t, usage_id,
                             &singleton->material_instance_usage_id)
        {
            if (parameter->parameter.name == kan_string_intern ("preset_index"))
            {
                parameter->parameter.value_i1 = (parameter->parameter.value_i1 + 1) % 3;
                KAN_UP_QUERY_BREAK;
            }
        }
#endif

        KAN_UP_VALUE_READ (material, kan_render_material_loaded_t, name, &material_instance->data.material_name)
        {
            kan_render_graphics_pipeline_t pipeline = KAN_HANDLE_INITIALIZE_INVALID;
            for (kan_loop_size_t index = 0u; index < material->pipelines.size; ++index)
            {
                const struct kan_render_material_loaded_pipeline_t *loaded =
                    &((struct kan_render_material_loaded_pipeline_t *) material->pipelines.data)[index];

                if (loaded->pass_name == state->scene_pass_name && loaded->variant_index == 0u)
                {
                    pipeline = loaded->pipeline;
                    break;
                }
            }

            if (!KAN_HANDLE_IS_VALID (pipeline))
            {
                KAN_UP_QUERY_BREAK;
            }

            if (!kan_render_pass_instance_graphics_pipeline (pass_allocation->pass_instance, pipeline))
            {
                // Pipeline is not yet ready, skip draw.
                KAN_UP_QUERY_BREAK;
            }

            kan_render_pass_instance_pipeline_parameter_sets (pass_allocation->pass_instance, KAN_RPL_SET_PASS, 1u,
                                                              &singleton->pass_parameter_set);
            kan_render_pass_instance_pipeline_parameter_sets (pass_allocation->pass_instance, KAN_RPL_SET_MATERIAL, 1u,
                                                              &material_instance->data.parameter_set);

            // Only one vertex attribute buffer for the sake of simplicity.
            KAN_ASSERT (material->vertex_attribute_sources.size == 1u)
            KAN_ASSERT (material->has_instanced_attribute_source)

            struct kan_rpl_meta_attribute_source_t *expected_attribute_source =
                &((struct kan_rpl_meta_attribute_source_t *) material->vertex_attribute_sources.data)[0u];

            struct kan_render_allocated_slice_t allocation = kan_render_frame_lifetime_buffer_allocator_allocate (
                singleton->instanced_data_allocator, material->instanced_attribute_source.block_size,
                _Alignof (struct kan_float_matrix_4x4_t));
            KAN_ASSERT (KAN_HANDLE_IS_VALID (allocation.buffer))

            struct kan_transform_3_t box_transform = kan_transform_3_get_identity ();
            struct kan_float_matrix_4x4_t box_transform_matrix;
            kan_transform_3_to_float_matrix_4x4 (&box_transform, &box_transform_matrix);

            void *instanced_data = kan_render_buffer_patch (allocation.buffer, allocation.slice_offset,
                                                            material->instanced_attribute_source.block_size);
            memcpy (instanced_data, material_instance->data.instanced_data.data,
                    material->instanced_attribute_source.block_size);
            // For the sake of the simple example, we just assume that model matrix is the first field.
            memcpy (instanced_data, &box_transform_matrix, sizeof (box_transform_matrix));

            kan_render_buffer_t attribute_buffers[] = {singleton->vertex_buffer, allocation.buffer};
            kan_render_size_t attribute_buffers_offsets[] = {0u, allocation.slice_offset};

            kan_render_pass_instance_attributes (pass_allocation->pass_instance, expected_attribute_source->binding,
                                                 sizeof (attribute_buffers) / sizeof (attribute_buffers[0u]),
                                                 attribute_buffers, attribute_buffers_offsets);

            kan_render_pass_instance_indices (pass_allocation->pass_instance, singleton->index_buffer);
            kan_render_pass_instance_draw (pass_allocation->pass_instance, 0u, singleton->index_count, 0u);

            if (!singleton->frame_checked)
            {
                KAN_LOG (application_framework_example_test_render_material, KAN_LOG_INFO,
                         "First frame to render index %u.", (unsigned) singleton->test_frames_count)
            }

            singleton->frame_checked = KAN_TRUE;
        }
    }
}

APPLICATION_FRAMEWORK_EXAMPLES_TEST_RENDER_MATERIAL_API void kan_universe_mutator_execute_test_render_material (
    kan_cpu_job_t job, struct test_render_material_state_t *state)
{
    KAN_UP_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    KAN_UP_SINGLETON_READ (render_graph, kan_render_graph_resource_management_singleton_t)
    KAN_UP_SINGLETON_READ (render_material_instance_singleton, kan_render_material_instance_singleton_t)
    KAN_UP_SINGLETON_READ (resource_provider, kan_resource_provider_singleton_t)
    KAN_UP_SINGLETON_WRITE (singleton, example_test_render_material_singleton_t)
    {
        if (!KAN_HANDLE_IS_VALID (singleton->window_handle))
        {
            singleton->window_handle =
                kan_application_system_window_create (state->application_system_handle, "Title placeholder", TEST_WIDTH,
                                                      TEST_HEIGHT, KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_VULKAN);

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
                KAN_LOG (application_framework_example_test_render_material, KAN_LOG_ERROR, "Failed to create surface.")
                kan_application_framework_system_request_exit (state->application_framework_system_handle, 1);
                KAN_UP_MUTATOR_RETURN;
            }

            kan_application_system_window_raise (state->application_system_handle, singleton->window_handle);
        }

        if (!KAN_TYPED_ID_32_IS_VALID (singleton->config_request_id))
        {
            KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
            {
                request->request_id = kan_next_resource_request_id (resource_provider);
                singleton->config_request_id = request->request_id;
                request->type = kan_string_intern ("test_render_material_config_t");
                request->name = kan_string_intern ("root_config");
            }
        }

        if (!singleton->object_buffers_initialized && KAN_HANDLE_IS_VALID (render_context->render_context))
        {
            example_test_render_material_singleton_initialize_object_buffers (singleton,
                                                                              render_context->render_context);
        }

        KAN_UP_EVENT_FETCH (pass_updated, kan_render_graph_pass_updated_event_t)
        {
            if (pass_updated->name == state->scene_pass_name)
            {
                // Reset pass data in order to rebuild it in render function.
                if (KAN_HANDLE_IS_VALID (singleton->pass_parameter_set))
                {
                    kan_render_pipeline_parameter_set_destroy (singleton->pass_parameter_set);
                    singleton->pass_parameter_set = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_t);
                }

                if (KAN_HANDLE_IS_VALID (singleton->pass_buffer))
                {
                    kan_render_buffer_destroy (singleton->pass_buffer);
                    singleton->pass_buffer = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
                }
            }
        }

        KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &singleton->config_request_id)
        {
            if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
            {
                KAN_UP_VALUE_READ (container, KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (test_render_material_config_t),
                                   container_id, &request->provided_container_id)
                {
                    const struct test_render_material_config_t *test_config =
                        KAN_RESOURCE_PROVIDER_CONTAINER_GET (test_render_material_config_t, container);

                    if (!KAN_TYPED_ID_32_IS_VALID (singleton->material_instance_usage_id))
                    {
                        KAN_UP_INDEXED_INSERT (usage, kan_render_material_instance_usage_t)
                        {
                            usage->usage_id = kan_next_material_instance_usage_id (render_material_instance_singleton);
                            singleton->material_instance_usage_id = usage->usage_id;
                            usage->name = test_config->material_instance_name;
                        }

#if defined(TEST_USE_CUSTOM_MATERIAL)
                        KAN_UP_INDEXED_INSERT (custom_parameter,
                                               kan_render_material_instance_custom_instanced_parameter_t)
                        {
                            custom_parameter->usage_id = singleton->material_instance_usage_id;
                            custom_parameter->parameter.name = kan_string_intern ("preset_index");
                            custom_parameter->parameter.type = KAN_RPL_META_VARIABLE_TYPE_I1;
                            custom_parameter->parameter.value_i1 = 1;
                        }
#endif
                    }

                    if (KAN_HANDLE_IS_VALID (render_context->render_context) && render_context->frame_scheduled)
                    {
                        KAN_UP_VALUE_READ (scene_pass, kan_render_graph_pass_t, name, &state->scene_pass_name)
                        {
                            struct kan_render_graph_pass_instance_request_attachment_info_t
                                scene_pass_attachment_info[] = {
                                    {
                                        .use_surface = singleton->window_surface,
                                        .used_by_dependant_instances = KAN_FALSE,
                                    },
                                    {
                                        .use_surface = KAN_HANDLE_INITIALIZE_INVALID,
                                        .used_by_dependant_instances = KAN_FALSE,
                                    },
                                };

                            struct kan_render_viewport_bounds_t scene_pass_viewport_bounds = {
                                .x = 0.0f,
                                .y = 0.0f,
                                .width = TEST_WIDTH_FLOAT,
                                .height = TEST_HEIGHT_FLOAT,
                                .depth_min = 0.0f,
                                .depth_max = 1.0f,
                            };

                            struct kan_render_integer_region_t scene_pass_scissor = {
                                .x = 0,
                                .y = 0,
                                .width = TEST_WIDTH,
                                .height = TEST_HEIGHT,
                            };

                            struct kan_render_clear_value_t scene_pass_clear_values[] = {
                                {
                                    .color = {0.29f, 0.64f, 0.88f, 1.0f},
                                },
                                {
                                    .depth_stencil = {1.0f, 0u},
                                },
                            };

                            struct kan_render_graph_pass_instance_allocation_t scene_pass_allocation;
                            struct kan_render_graph_pass_instance_request_t scene_pass_request = {
                                .context = render_context->render_context,
                                .pass = scene_pass,

                                .frame_buffer_width = TEST_WIDTH,
                                .frame_buffer_height = TEST_HEIGHT,
                                .attachment_info = scene_pass_attachment_info,

                                .dependant_count = 0u,
                                .dependant = NULL,

                                .viewport_bounds = &scene_pass_viewport_bounds,
                                .scissor = &scene_pass_scissor,
                                .attachment_clear_values = scene_pass_clear_values,
                            };

                            if (!kan_render_graph_resource_management_singleton_request_pass (
                                    render_graph, &scene_pass_request, &scene_pass_allocation))
                            {
                                KAN_LOG (application_framework_example_test_render_material, KAN_LOG_ERROR,
                                         "Failed to create scene pass.")
                                kan_application_framework_system_request_exit (
                                    state->application_framework_system_handle, 1);
                                KAN_UP_MUTATOR_RETURN;
                            }

                            try_render_frame (state, render_context, singleton, scene_pass, &scene_pass_allocation,
                                              test_config->material_instance_name);
                        }
                    }
                }
            }
        }

        ++singleton->test_frames_count;
        if (state->test_mode)
        {
            if (30u < singleton->test_frames_count || singleton->frame_checked)
            {
                KAN_LOG (application_framework_example_test_render_material, KAN_LOG_INFO,
                         "Shutting down in test mode...")

                if (!singleton->frame_checked)
                {
                    KAN_LOG (application_framework_example_test_render_material, KAN_LOG_ERROR,
                             "Wasn't able to check render frame at least once.")
                }

                kan_application_framework_system_request_exit (state->application_framework_system_handle,
                                                               singleton->frame_checked ? 0 : 1);
            }
        }
    }

    KAN_UP_MUTATOR_RETURN;
}
