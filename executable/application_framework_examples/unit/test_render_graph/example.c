#include <application_framework_examples_test_render_graph_api.h>

#include <stdio.h>
#include <string.h>

#include <kan/context/all_system_names.h>
#include <kan/context/application_framework_system.h>
#include <kan/context/application_system.h>
#include <kan/log/logging.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe/universe.h>
#include <kan/universe_render_foundation/render_graph.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>
#include <kan/universe_time/universe_time.h>

// TODO: This is a temporary test-only example which is used until enough
//       render functionality is implemented in order to create proper examples.

KAN_LOG_DEFINE_CATEGORY (application_framework_example_test_render_graph);

struct example_test_render_graph_singleton_t
{
    kan_application_system_window_t window_handle;
    kan_render_surface_t window_surface;
    kan_instance_size_t test_frames_count;
    kan_bool_t frame_checked;
};

APPLICATION_FRAMEWORK_EXAMPLES_TEST_RENDER_GRAPH_API void example_test_render_graph_singleton_init (
    struct example_test_render_graph_singleton_t *instance)
{
    instance->window_handle = KAN_HANDLE_SET_INVALID (kan_application_system_window_t);
    instance->window_surface = KAN_HANDLE_SET_INVALID (kan_render_surface_t);
    instance->test_frames_count = 0u;
    instance->frame_checked = KAN_FALSE;
}

struct test_render_graph_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (test_render_graph)
    KAN_UP_BIND_STATE (test_render_graph, state)

    kan_context_system_t application_system_handle;
    kan_context_system_t application_framework_system_handle;
    kan_context_system_t render_backend_system_handle;
    kan_bool_t test_mode;

    kan_interned_string_t scene_pass_name;
    kan_interned_string_t shadow_pass_name;
};

APPLICATION_FRAMEWORK_EXAMPLES_TEST_RENDER_GRAPH_API void kan_universe_mutator_deploy_test_render_graph (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct test_render_graph_state_t *state)
{
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
    state->shadow_pass_name = kan_string_intern ("test_shadow_pass");
}

APPLICATION_FRAMEWORK_EXAMPLES_TEST_RENDER_GRAPH_API void kan_universe_mutator_execute_test_render_graph (
    kan_cpu_job_t job, struct test_render_graph_state_t *state)
{
    KAN_UP_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    KAN_UP_SINGLETON_READ (render_graph, kan_render_graph_resource_management_singleton_t)
    KAN_UP_SINGLETON_WRITE (singleton, example_test_render_graph_singleton_t)
    {
        if (!KAN_HANDLE_IS_VALID (singleton->window_handle))
        {
            singleton->window_handle =
                kan_application_system_window_create (state->application_system_handle, "Title placeholder", 600u, 400u,
                                                      KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_VULKAN);

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
                KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_ERROR, "Failed to create surface.")
                kan_application_framework_system_request_exit (state->application_framework_system_handle, 1);
                KAN_UP_MUTATOR_RETURN;
            }

            kan_application_system_window_raise (state->application_system_handle, singleton->window_handle);
        }

        if (KAN_HANDLE_IS_VALID (render_context->render_context) && render_context->frame_scheduled)
        {
            KAN_UP_VALUE_READ (scene_pass, kan_render_graph_pass_t, name, &state->scene_pass_name)
            {
                KAN_UP_VALUE_READ (shadow_pass, kan_render_graph_pass_t, name, &state->shadow_pass_name)
                {
                    const struct kan_render_graph_resource_response_t *scene_pass_1_response = NULL;
                    const struct kan_render_graph_resource_response_t *scene_pass_2_response = NULL;

                    const struct kan_render_graph_resource_response_t *shadow_pass_1_response = NULL;
                    const struct kan_render_graph_resource_response_t *shadow_pass_2_response = NULL;
                    const struct kan_render_graph_resource_response_t *shadow_pass_3_response = NULL;

                    KAN_ASSERT (scene_pass->attachments.size == 2u)
                    enum kan_render_image_format_t scene_depth_format =
                        ((struct kan_render_graph_pass_attachment_t *) scene_pass->attachments.data)[1u].format;

                    struct kan_render_graph_resource_image_request_t scene_pass_images[] = {
                        {
                            .description =
                                {
                                    .format = scene_depth_format,
                                    .width = 600u,
                                    .height = 400u,
                                    .depth = 1u,
                                    .layers = 1u,
                                    .mips = 1u,
                                    .render_target = KAN_TRUE,
                                    .supports_sampling = KAN_FALSE,
                                    .tracking_name = NULL,
                                },
                            .internal = KAN_TRUE,
                        },
                    };

                    struct kan_render_graph_resource_frame_buffer_request_t scene_pass_frame_buffers[] = {
                        {
                            .pass = scene_pass->pass,
                            .attachments_count = 2u,
                            .attachments =
                                (struct kan_render_graph_resource_frame_buffer_request_attachment_t[]) {
                                    {
                                        .surface_attachment = KAN_TRUE,
                                        .surface = singleton->window_surface,
                                    },
                                    {
                                        .surface_attachment = KAN_FALSE,
                                        .image =
                                            {
                                                .index = 0u,
                                                .layer = 0u,
                                            },
                                    },
                                },
                        },
                    };

                    struct kan_render_graph_resource_request_t scene_pass_request = {
                        .context = render_context->render_context,
                        .dependant_count = 0u,
                        .dependant = NULL,
                        .images_count = sizeof (scene_pass_images) / sizeof (scene_pass_images[0u]),
                        .images = scene_pass_images,
                        .frame_buffers_count =
                            sizeof (scene_pass_frame_buffers) / sizeof (scene_pass_frame_buffers[0u]),
                        .frame_buffers = scene_pass_frame_buffers,
                    };

                    if (!(scene_pass_1_response = kan_render_graph_resource_management_singleton_request (
                              render_graph, &scene_pass_request)))
                    {
                        KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_ERROR,
                                 "Failed to create scene pass 1.")
                        kan_application_framework_system_request_exit (state->application_framework_system_handle, 1);
                        KAN_UP_MUTATOR_RETURN;
                    }

                    if (!(scene_pass_2_response = kan_render_graph_resource_management_singleton_request (
                              render_graph, &scene_pass_request)))
                    {
                        KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_ERROR,
                                 "Failed to create scene pass 2.")
                        kan_application_framework_system_request_exit (state->application_framework_system_handle, 1);
                        KAN_UP_MUTATOR_RETURN;
                    }

                    KAN_ASSERT (shadow_pass->attachments.size == 1u)
                    enum kan_render_image_format_t shadow_depth_format =
                        ((struct kan_render_graph_pass_attachment_t *) shadow_pass->attachments.data)[0u].format;

                    struct kan_render_graph_resource_image_request_t shadow_pass_images[] = {
                        {
                            .description =
                                {
                                    .format = shadow_depth_format,
                                    .width = 256u,
                                    .height = 256u,
                                    .depth = 1u,
                                    .layers = 1u,
                                    .mips = 1u,
                                    .render_target = KAN_TRUE,
                                    .supports_sampling = KAN_TRUE,
                                    .tracking_name = NULL,
                                },
                            .internal = KAN_FALSE,
                        },
                    };

                    struct kan_render_graph_resource_frame_buffer_request_t shadow_pass_frame_buffers[] = {
                        {
                            .pass = shadow_pass->pass,
                            .attachments_count = 1u,
                            .attachments =
                                (struct kan_render_graph_resource_frame_buffer_request_attachment_t[]) {
                                    {
                                        .surface_attachment = KAN_FALSE,
                                        .image =
                                            {
                                                .index = 0u,
                                                .layer = 0u,
                                            },
                                    },
                                },
                        },
                    };

                    {
                        struct kan_render_graph_resource_request_t request = {
                            .context = render_context->render_context,
                            .dependant_count = 1u,
                            .dependant =
                                (const struct kan_render_graph_resource_response_t *[]) {
                                    scene_pass_1_response,
                                },
                            .images_count = sizeof (shadow_pass_images) / sizeof (shadow_pass_images[0u]),
                            .images = shadow_pass_images,
                            .frame_buffers_count =
                                sizeof (shadow_pass_frame_buffers) / sizeof (shadow_pass_frame_buffers[0u]),
                            .frame_buffers = shadow_pass_frame_buffers,
                        };

                        if (!(shadow_pass_1_response =
                                  kan_render_graph_resource_management_singleton_request (render_graph, &request)))
                        {
                            KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_ERROR,
                                     "Failed to create shadow pass 1.")
                            kan_application_framework_system_request_exit (state->application_framework_system_handle,
                                                                           1);
                            KAN_UP_MUTATOR_RETURN;
                        }
                    }

                    {
                        struct kan_render_graph_resource_request_t request = {
                            .context = render_context->render_context,
                            .dependant_count = 2u,
                            .dependant =
                                (const struct kan_render_graph_resource_response_t *[]) {
                                    scene_pass_1_response,
                                    scene_pass_2_response,
                                },
                            .images_count = sizeof (shadow_pass_images) / sizeof (shadow_pass_images[0u]),
                            .images = shadow_pass_images,
                            .frame_buffers_count =
                                sizeof (shadow_pass_frame_buffers) / sizeof (shadow_pass_frame_buffers[0u]),
                            .frame_buffers = shadow_pass_frame_buffers,
                        };

                        if (!(shadow_pass_2_response =
                                  kan_render_graph_resource_management_singleton_request (render_graph, &request)))
                        {
                            KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_ERROR,
                                     "Failed to create shadow pass 2.")
                            kan_application_framework_system_request_exit (state->application_framework_system_handle,
                                                                           1);
                            KAN_UP_MUTATOR_RETURN;
                        }
                    }

                    {
                        struct kan_render_graph_resource_request_t request = {
                            .context = render_context->render_context,
                            .dependant_count = 1u,
                            .dependant =
                                (const struct kan_render_graph_resource_response_t *[]) {
                                    scene_pass_2_response,
                                },
                            .images_count = sizeof (shadow_pass_images) / sizeof (shadow_pass_images[0u]),
                            .images = shadow_pass_images,
                            .frame_buffers_count =
                                sizeof (shadow_pass_frame_buffers) / sizeof (shadow_pass_frame_buffers[0u]),
                            .frame_buffers = shadow_pass_frame_buffers,
                        };

                        if (!(shadow_pass_3_response =
                                  kan_render_graph_resource_management_singleton_request (render_graph, &request)))
                        {
                            KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_ERROR,
                                     "Failed to create shadow pass 3.")
                            kan_application_framework_system_request_exit (state->application_framework_system_handle,
                                                                           1);
                            KAN_UP_MUTATOR_RETURN;
                        }
                    }

                    kan_bool_t check_successful = KAN_TRUE;
                    if (!KAN_HANDLE_IS_EQUAL (scene_pass_1_response->images[0u], scene_pass_2_response->images[0u]))
                    {
                        KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_ERROR,
                                 "Scene passes expected to reuse the same depth image.")
                        check_successful = KAN_FALSE;
                    }

                    if (!KAN_HANDLE_IS_EQUAL (scene_pass_1_response->frame_buffers[0u],
                                              scene_pass_2_response->frame_buffers[0u]))
                    {
                        KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_ERROR,
                                 "Scene passes expected to reuse the same frame buffer.")
                        check_successful = KAN_FALSE;
                    }

                    if (KAN_HANDLE_IS_EQUAL (shadow_pass_1_response->images[0u], shadow_pass_2_response->images[0u]))
                    {
                        KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_ERROR,
                                 "Shadow pass 1 and 2 must have different images.")
                        check_successful = KAN_FALSE;
                    }

                    if (KAN_HANDLE_IS_EQUAL (shadow_pass_1_response->frame_buffers[0u],
                                             shadow_pass_2_response->frame_buffers[0u]))
                    {
                        KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_ERROR,
                                 "Shadow pass 1 and 2 must have different frame buffers.")
                        check_successful = KAN_FALSE;
                    }

                    if (KAN_HANDLE_IS_EQUAL (shadow_pass_2_response->images[0u], shadow_pass_3_response->images[0u]))
                    {
                        KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_ERROR,
                                 "Shadow pass 1 and 2 must have different images.")
                        check_successful = KAN_FALSE;
                    }

                    if (KAN_HANDLE_IS_EQUAL (shadow_pass_2_response->frame_buffers[0u],
                                             shadow_pass_3_response->frame_buffers[0u]))
                    {
                        KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_ERROR,
                                 "Shadow pass 1 and 2 must have different frame buffers.")
                        check_successful = KAN_FALSE;
                    }

                    if (!KAN_HANDLE_IS_EQUAL (shadow_pass_1_response->images[0u], shadow_pass_3_response->images[0u]))
                    {
                        KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_ERROR,
                                 "Shadow pass 1 and 3 must share image.")
                        check_successful = KAN_FALSE;
                    }

                    if (!KAN_HANDLE_IS_EQUAL (shadow_pass_1_response->frame_buffers[0u],
                                              shadow_pass_3_response->frame_buffers[0u]))
                    {
                        KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_ERROR,
                                 "Shadow pass 1 and 3 must share frame buffer.")
                        check_successful = KAN_FALSE;
                    }

                    struct kan_render_viewport_bounds_t scene_pass_viewport_bounds = {
                        .x = 0.0f,
                        .y = 0.0f,
                        .width = 300.0f,
                        .height = 400.0f,
                        .depth_min = 0.0f,
                        .depth_max = 1.0f,
                    };

                    struct kan_render_integer_region_t scene_pass_scissor = {
                        .x = 0,
                        .y = 0,
                        .width = 300u,
                        .height = 400u,
                    };

                    struct kan_render_clear_value_t scene_pass_clear_values[] = {
                        {
                            .color = {0.29f, 0.64f, 0.88f, 1.0f},
                        },
                        {
                            .depth_stencil = {1.0f, 0u},
                        },
                    };

                    kan_render_pass_instance_t scene_pass_1 = kan_render_pass_instantiate (
                        scene_pass->pass, scene_pass_1_response->frame_buffers[0u], &scene_pass_viewport_bounds,
                        &scene_pass_scissor, scene_pass_clear_values);

                    kan_render_pass_instance_add_checkpoint_dependency (scene_pass_1,
                                                                        scene_pass_1_response->usage_begin_checkpoint);

                    kan_render_pass_instance_checkpoint_add_instance_dependancy (
                        scene_pass_1_response->usage_end_checkpoint, scene_pass_1);

                    scene_pass_viewport_bounds.x = 300.0f;
                    scene_pass_scissor.x = 300;
                    scene_pass_clear_values[0u].color.r = 0.32f;
                    scene_pass_clear_values[0u].color.g = 0.29f;
                    scene_pass_clear_values[0u].color.b = 0.88f;

                    kan_render_pass_instance_t scene_pass_2 = kan_render_pass_instantiate (
                        scene_pass->pass, scene_pass_2_response->frame_buffers[0u], &scene_pass_viewport_bounds,
                        &scene_pass_scissor, scene_pass_clear_values);

                    kan_render_pass_instance_add_checkpoint_dependency (scene_pass_2,
                                                                        scene_pass_2_response->usage_begin_checkpoint);

                    kan_render_pass_instance_checkpoint_add_instance_dependancy (
                        scene_pass_2_response->usage_end_checkpoint, scene_pass_2);

                    struct kan_render_viewport_bounds_t shadow_pass_viewport_bounds = {
                        .x = 0.0f,
                        .y = 0.0f,
                        .width = 256.0f,
                        .height = 256.0f,
                        .depth_min = 0.0f,
                        .depth_max = 1.0f,
                    };

                    struct kan_render_integer_region_t shadow_pass_scissor = {
                        .x = 0,
                        .y = 0,
                        .width = 256u,
                        .height = 256u,
                    };

                    struct kan_render_clear_value_t shadow_pass_clear_values[] = {
                        {
                            .depth_stencil = {1.0f, 0u},
                        },
                    };

                    kan_render_pass_instance_t shadow_pass_1 = kan_render_pass_instantiate (
                        shadow_pass->pass, shadow_pass_1_response->frame_buffers[0u], &shadow_pass_viewport_bounds,
                        &shadow_pass_scissor, shadow_pass_clear_values);

                    kan_render_pass_instance_add_checkpoint_dependency (shadow_pass_1,
                                                                        shadow_pass_1_response->usage_begin_checkpoint);

                    kan_render_pass_instance_checkpoint_add_instance_dependancy (
                        shadow_pass_1_response->usage_end_checkpoint, shadow_pass_1);

                    kan_render_pass_instance_t shadow_pass_2 = kan_render_pass_instantiate (
                        shadow_pass->pass, shadow_pass_2_response->frame_buffers[0u], &shadow_pass_viewport_bounds,
                        &shadow_pass_scissor, shadow_pass_clear_values);

                    kan_render_pass_instance_add_checkpoint_dependency (shadow_pass_2,
                                                                        shadow_pass_2_response->usage_begin_checkpoint);

                    kan_render_pass_instance_checkpoint_add_instance_dependancy (
                        shadow_pass_2_response->usage_end_checkpoint, shadow_pass_2);

                    kan_render_pass_instance_t shadow_pass_3 = kan_render_pass_instantiate (
                        shadow_pass->pass, shadow_pass_3_response->frame_buffers[0u], &shadow_pass_viewport_bounds,
                        &shadow_pass_scissor, shadow_pass_clear_values);

                    kan_render_pass_instance_add_checkpoint_dependency (shadow_pass_3,
                                                                        shadow_pass_3_response->usage_begin_checkpoint);

                    kan_render_pass_instance_checkpoint_add_instance_dependancy (
                        shadow_pass_3_response->usage_end_checkpoint, shadow_pass_3);

                    if (check_successful)
                    {
                        singleton->frame_checked = KAN_TRUE;
                    }
                    else
                    {
                        kan_application_framework_system_request_exit (state->application_framework_system_handle, 1);
                    }
                }
            }
        }

        if (state->test_mode)
        {
            if (30u < ++singleton->test_frames_count)
            {
                KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_INFO, "Shutting down in test mode...")
                if (!singleton->frame_checked)
                {
                    KAN_LOG (application_framework_example_test_render_graph, KAN_LOG_ERROR,
                             "Wasn't able to check render graph frame even once.")
                }

                kan_application_framework_system_request_exit (state->application_framework_system_handle,
                                                               singleton->frame_checked ? 0 : 1);
            }
        }
    }

    KAN_UP_MUTATOR_RETURN;
}
