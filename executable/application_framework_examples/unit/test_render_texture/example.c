#include <application_framework_examples_test_render_texture_api.h>

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
#include <kan/universe_render_foundation/texture.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>
#include <kan/universe_time/universe_time.h>

// TODO: This is a temporary test-only example which is used until enough
//       render functionality is implemented in order to create proper examples.

struct test_render_texture_config_t
{
    kan_interned_string_t texture_name;
};

KAN_REFLECTION_STRUCT_META (test_render_texture_config_t)
APPLICATION_FRAMEWORK_EXAMPLES_TEST_RENDER_TEXTURE_API struct kan_resource_resource_type_meta_t
    test_render_texture_config_resource_type_meta = {
        .root = KAN_TRUE,
};

KAN_REFLECTION_STRUCT_FIELD_META (test_render_texture_config_t, texture_name)
APPLICATION_FRAMEWORK_EXAMPLES_TEST_RENDER_TEXTURE_API struct kan_resource_reference_meta_t
    root_config_required_sum_reference_meta = {
        .type = "kan_resource_texture_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

KAN_LOG_DEFINE_CATEGORY (application_framework_example_test_render_texture);

struct example_test_render_texture_singleton_t
{
    kan_application_system_window_t window_handle;
    kan_render_surface_t window_surface;
    kan_instance_size_t test_frames_count;
    kan_resource_request_id_t config_request_id;
    kan_render_texture_usage_id_t texture_usage_id;
    kan_bool_t frame_checked;
};

APPLICATION_FRAMEWORK_EXAMPLES_TEST_RENDER_TEXTURE_API void example_test_render_texture_singleton_init (
    struct example_test_render_texture_singleton_t *instance)
{
    instance->window_handle = KAN_HANDLE_SET_INVALID (kan_application_system_window_t);
    instance->window_surface = KAN_HANDLE_SET_INVALID (kan_render_surface_t);
    instance->test_frames_count = 0u;
    instance->config_request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
    instance->texture_usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_render_texture_usage_id_t);
    instance->frame_checked = KAN_FALSE;
}

struct test_render_texture_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (test_render_texture)
    KAN_UP_BIND_STATE (test_render_texture, state)

    kan_context_system_t application_system_handle;
    kan_context_system_t application_framework_system_handle;
    kan_context_system_t render_backend_system_handle;
    kan_bool_t test_mode;
};

APPLICATION_FRAMEWORK_EXAMPLES_TEST_RENDER_TEXTURE_API void kan_universe_mutator_deploy_test_render_texture (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct test_render_texture_state_t *state)
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
}

APPLICATION_FRAMEWORK_EXAMPLES_TEST_RENDER_TEXTURE_API void kan_universe_mutator_execute_test_render_texture (
    kan_cpu_job_t job, struct test_render_texture_state_t *state)
{
    KAN_UP_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    KAN_UP_SINGLETON_READ (render_texture_singleton, kan_render_texture_singleton_t)
    KAN_UP_SINGLETON_READ (resource_provider, kan_resource_provider_singleton_t)
    KAN_UP_SINGLETON_WRITE (singleton, example_test_render_texture_singleton_t)
    {
        if (!KAN_HANDLE_IS_VALID (singleton->window_handle))
        {
            singleton->window_handle =
                kan_application_system_window_create (state->application_system_handle, "Title placeholder", 256u, 256u,
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
                KAN_LOG (application_framework_example_test_render_texture, KAN_LOG_ERROR, "Failed to create surface.")
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
                request->type = kan_string_intern ("test_render_texture_config_t");
                request->name = kan_string_intern ("root_config");
            }
        }

        KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &singleton->config_request_id)
        {
            if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
            {
                KAN_UP_VALUE_READ (container, KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (test_render_texture_config_t),
                                   container_id, &request->provided_container_id)
                {
                    const struct test_render_texture_config_t *test_config =
                        KAN_RESOURCE_PROVIDER_CONTAINER_GET (test_render_texture_config_t, container);

                    if (!KAN_TYPED_ID_32_IS_VALID (singleton->texture_usage_id))
                    {
                        KAN_UP_INDEXED_INSERT (usage, kan_render_texture_usage_t)
                        {
                            usage->usage_id = kan_next_texture_usage_id (render_texture_singleton);
                            singleton->texture_usage_id = usage->usage_id;
                            usage->name = test_config->texture_name;
                            // We intentionally request low resolution mips so compilation will be visible
                            // in packaged example version.
                            usage->best_advised_mip = 2u;
                            usage->worst_advised_mip = 3u;
                        }
                    }

                    KAN_UP_VALUE_READ (loaded, kan_render_texture_loaded_t, name, &test_config->texture_name)
                    {
                        if (KAN_HANDLE_IS_VALID (render_context->render_context) && render_context->frame_scheduled)
                        {
                            struct kan_render_integer_region_t surface_region = {
                                .x = 0u,
                                .y = 0u,
                                .width = 256u,
                                .height = 256u,
                            };

                            struct kan_render_integer_region_t texture_region = {
                                .x = 0u,
                                .y = 0u,
                                // Size is divided by 4 as we're expecting mip 2.
                                // Actually, expecting mip is a bad pattern as loaded is allowed to select mips as it
                                // wishes, and we'll actually get all 4 mips when loading raw texture.
                                // But it is kind of okay for temporary example.
                                .width = 256u / 4u,
                                .height = 256u / 4u,
                            };

                            kan_render_backend_system_present_image_on_surface (
                                singleton->window_surface, loaded->image, surface_region, texture_region);
                            singleton->frame_checked = KAN_TRUE;
                        }
                    }
                }
            }
        }

        if (state->test_mode)
        {
            if (30u < ++singleton->test_frames_count)
            {
                KAN_LOG (application_framework_example_test_render_texture, KAN_LOG_INFO,
                         "Shutting down in test mode...")
                if (!singleton->frame_checked)
                {
                    KAN_LOG (application_framework_example_test_render_texture, KAN_LOG_ERROR,
                             "Wasn't able to check render frame at least once.")
                }

                kan_application_framework_system_request_exit (state->application_framework_system_handle,
                                                               singleton->frame_checked ? 0 : 1);
            }
        }
    }

    KAN_UP_MUTATOR_RETURN;
}
