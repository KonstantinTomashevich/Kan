#include <string.h>

#include <kan/context/all_system_names.h>
#include <kan/log/logging.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe_render_foundation/render_graph.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

KAN_LOG_DEFINE_CATEGORY (render_foundation_graph);

KAN_REFLECTION_STRUCT_META (kan_render_graph_pass_resource_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_resource_resource_type_meta_t
    kan_render_graph_pass_resource_resource_type_meta = {
        .root = KAN_TRUE,
};

KAN_REFLECTION_FUNCTION_META (kan_universe_mutator_execute_render_foundation_pass_management_planning)
KAN_REFLECTION_FUNCTION_META (kan_universe_mutator_execute_render_foundation_pass_management_execution)
KAN_REFLECTION_FUNCTION_META (kan_universe_mutator_execute_render_foundation_frame_execution)
UNIVERSE_RENDER_FOUNDATION_API struct kan_universe_mutator_group_meta_t render_foundation_root_routine_group_meta = {
    .group_name = KAN_RENDER_FOUNDATION_ROOT_ROUTINE_MUTATOR_GROUP,
};

struct render_foundation_pass_loading_state_t
{
    kan_interned_string_t name;
    kan_resource_request_id_t request_id;
};

KAN_REFLECTION_STRUCT_META (render_foundation_pass_loading_state_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_cascade_deletion_t
    resource_request_pass_loading_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"request_id"}},
        .child_type_name = "kan_resource_request_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"request_id"}},
};

KAN_REFLECTION_STRUCT_META (render_foundation_pass_loading_state_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_cascade_deletion_t
    render_graph_pass_pass_loading_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
        .child_type_name = "kan_render_graph_pass_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
};

KAN_REFLECTION_IGNORE
struct render_foundation_graph_image_usage_t
{
    struct render_foundation_graph_image_usage_t *next;
    kan_render_pass_instance_t producer_pass;
    kan_instance_size_t user_passes_count;
    kan_render_pass_instance_t user_passes[];
};

KAN_REFLECTION_IGNORE
struct render_foundation_graph_image_cache_node_t
{
    struct kan_hash_storage_node_t node;

    kan_render_image_t image;
    enum kan_render_image_format_t format;
    kan_render_size_t width;
    kan_render_size_t height;
    kan_bool_t supports_sampling;

    struct render_foundation_graph_image_usage_t *first_usage;
};

static inline kan_hash_t calculate_cached_image_hash (enum kan_render_image_format_t format,
                                                      kan_render_size_t width,
                                                      kan_render_size_t height)
{
    return kan_hash_combine ((kan_hash_t) format, kan_hash_combine ((kan_hash_t) width, (kan_hash_t) height));
}

static struct render_foundation_graph_image_cache_node_t *render_foundation_graph_image_cache_node_create (
    const struct kan_render_graph_resource_management_singleton_t *singleton,
    kan_render_context_t context,
    enum kan_render_image_format_t format,
    kan_render_size_t width,
    kan_render_size_t height,
    kan_bool_t supports_sampling)
{
    struct kan_render_image_description_t description = {
        .format = format,
        .width = width,
        .height = height,
        .depth = 1u,
        .mips = 1u,
        .render_target = KAN_TRUE,
        .supports_sampling = supports_sampling,
        .tracking_name = singleton->cached_image_name,
    };

    kan_render_image_t image = kan_render_image_create (context, &description);
    if (!KAN_HANDLE_IS_VALID (image))
    {
        KAN_LOG (render_foundation_graph, KAN_LOG_ERROR, "Failed to create new image for render graph.")
        return NULL;
    }

    struct render_foundation_graph_image_cache_node_t *node =
        kan_allocate_batched (singleton->cache_group, sizeof (struct render_foundation_graph_image_cache_node_t));

    node->image = image;
    node->format = format;
    node->width = width;
    node->height = height;
    node->supports_sampling = supports_sampling;
    node->first_usage = NULL;

    node->node.hash = calculate_cached_image_hash (node->format, node->width, node->height);
    return node;
}

static void render_foundation_graph_image_cache_node_destroy (
    const struct kan_render_graph_resource_management_singleton_t *singleton,
    struct render_foundation_graph_image_cache_node_t *instance)
{
    kan_render_image_destroy (instance->image);
    kan_free_batched (singleton->cache_group, instance);
}

KAN_REFLECTION_IGNORE
struct render_foundation_graph_frame_buffer_cache_node_t
{
    struct kan_hash_storage_node_t node;

    kan_render_frame_buffer_t frame_buffer;
    kan_render_pass_t pass;
    kan_bool_t used_in_current_frame;

    kan_instance_size_t attachments_count;
    struct kan_render_frame_buffer_attachment_description_t *attachments;
};

static kan_hash_t calculate_cached_frame_buffer_hash (
    kan_render_pass_t pass,
    kan_instance_size_t attachments_count,
    struct kan_render_frame_buffer_attachment_description_t *attachments)
{
    kan_hash_t hash = (kan_hash_t) KAN_HANDLE_GET (pass);
    for (kan_loop_size_t index = 0u; index < attachments_count; ++index)
    {
        switch (attachments[index].type)
        {
        case KAN_FRAME_BUFFER_ATTACHMENT_IMAGE:
            hash = kan_hash_combine (hash, (kan_hash_t) KAN_HANDLE_GET (attachments[index].image));
            break;

        case KAN_FRAME_BUFFER_ATTACHMENT_SURFACE:
            hash = kan_hash_combine (hash, (kan_hash_t) KAN_HANDLE_GET (attachments[index].surface));
            break;
        }
    }

    return hash;
}

static struct render_foundation_graph_frame_buffer_cache_node_t *
render_foundation_graph_frame_buffer_cache_node_create (
    const struct kan_render_graph_resource_management_singleton_t *singleton,
    kan_render_context_t context,
    kan_render_pass_t pass,
    kan_instance_size_t attachments_count,
    struct kan_render_frame_buffer_attachment_description_t *attachments)
{
    struct kan_render_frame_buffer_description_t description = {
        .associated_pass = pass,
        .attachment_count = attachments_count,
        .attachments = attachments,
        .tracking_name = singleton->cached_frame_buffer_name,
    };

    kan_render_frame_buffer_t frame_buffer = kan_render_frame_buffer_create (context, &description);
    if (!KAN_HANDLE_IS_VALID (frame_buffer))
    {
        KAN_LOG (render_foundation_graph, KAN_LOG_ERROR, "Failed to create new frame buffer for render graph.")
        return NULL;
    }

    struct render_foundation_graph_frame_buffer_cache_node_t *node = kan_allocate_batched (
        singleton->cache_group, sizeof (struct render_foundation_graph_frame_buffer_cache_node_t));

    node->frame_buffer = frame_buffer;
    node->pass = pass;
    node->used_in_current_frame = KAN_FALSE;

    node->attachments_count = attachments_count;
    node->attachments = attachments;

    node->node.hash = calculate_cached_frame_buffer_hash (pass, attachments_count, attachments);
    return node;
}

static void render_foundation_graph_frame_buffer_cache_node_destroy (
    const struct kan_render_graph_resource_management_singleton_t *singleton,
    struct render_foundation_graph_frame_buffer_cache_node_t *instance)
{
    kan_render_frame_buffer_destroy (instance->frame_buffer);
    kan_free_general (singleton->cache_group, instance->attachments,
                      sizeof (struct kan_render_frame_buffer_attachment_description_t) * instance->attachments_count);
    kan_free_batched (singleton->cache_group, instance);
}

struct render_foundation_pass_management_planning_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (render_foundation_pass_management_planning)
    KAN_UP_BIND_STATE (render_foundation_pass_management_planning, state)

    kan_interned_string_t interned_kan_render_graph_pass_resource_t;
};

UNIVERSE_RENDER_FOUNDATION_API void render_foundation_pass_management_planning_state_init (
    struct render_foundation_pass_management_planning_state_t *instance)
{
    instance->interned_kan_render_graph_pass_resource_t = kan_string_intern ("kan_render_graph_pass_resource_t");
}

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_deploy_render_foundation_pass_management_planning (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct render_foundation_pass_management_planning_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_PASS_MANAGEMENT_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RESOURCE_PROVIDER_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RENDER_FOUNDATION_FRAME_BEGIN);
}

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_execute_render_foundation_pass_management_planning (
    kan_cpu_job_t job, struct render_foundation_pass_management_planning_state_t *state)
{
    KAN_UP_SINGLETON_READ (resource_provider, kan_resource_provider_singleton_t)
    KAN_UP_SINGLETON_WRITE (render_graph, kan_render_graph_resource_management_singleton_t)
    {
        KAN_UP_EVENT_FETCH (insert_event, kan_resource_native_entry_on_insert_event_t)
        {
            if (insert_event->type == state->interned_kan_render_graph_pass_resource_t)
            {
                KAN_UP_INDEXED_INSERT (loading, render_foundation_pass_loading_state_t)
                {
                    loading->name = insert_event->name;
                    KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
                    {
                        request->request_id = kan_next_resource_request_id (resource_provider);
                        loading->request_id = request->request_id;

                        request->name = insert_event->name;
                        request->type = state->interned_kan_render_graph_pass_resource_t;

                        // Passes are very important and their count is small, therefore we load them with max priority.
                        request->priority = KAN_RESOURCE_PROVIDER_USER_PRIORITY_MAX;
                    }
                }
            }
        }

        KAN_UP_EVENT_FETCH (delete_event, kan_resource_native_entry_on_delete_event_t)
        {
            if (delete_event->type == state->interned_kan_render_graph_pass_resource_t)
            {
                KAN_UP_VALUE_DELETE (loading, render_foundation_pass_loading_state_t, name, &delete_event->name)
                {
                    // If we've loaded and used render pass, we need to delete it associated frame buffers.
                    KAN_UP_VALUE_READ (pass, kan_render_graph_pass_t, name, &loading->name)
                    {
                        struct render_foundation_graph_frame_buffer_cache_node_t *frame_buffer =
                            (struct render_foundation_graph_frame_buffer_cache_node_t *)
                                render_graph->frame_buffer_cache.items.first;

                        while (frame_buffer)
                        {
                            struct render_foundation_graph_frame_buffer_cache_node_t *next =
                                (struct render_foundation_graph_frame_buffer_cache_node_t *)
                                    frame_buffer->node.list_node.next;

                            if (KAN_HANDLE_IS_EQUAL (frame_buffer->pass, pass->pass))
                            {
                                kan_hash_storage_remove (&render_graph->frame_buffer_cache, &frame_buffer->node);
                                render_foundation_graph_frame_buffer_cache_node_destroy (render_graph, frame_buffer);
                            }

                            frame_buffer = next;
                        }
                    }

                    KAN_UP_ACCESS_DELETE (loading);
                }
            }
        }
    }

    KAN_UP_MUTATOR_RETURN;
}

struct render_foundation_pass_management_execution_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (render_foundation_pass_management_execution)
    KAN_UP_BIND_STATE (render_foundation_pass_management_execution, state)

    kan_interned_string_t interned_kan_render_graph_pass_resource_t;
};

UNIVERSE_RENDER_FOUNDATION_API void render_foundation_pass_management_execution_state_init (
    struct render_foundation_pass_management_execution_state_t *instance)
{
    instance->interned_kan_render_graph_pass_resource_t = kan_string_intern ("kan_render_graph_pass_resource_t");
}

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_deploy_render_foundation_pass_management_execution (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct render_foundation_pass_management_execution_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RENDER_FOUNDATION_PASS_MANAGEMENT_END_CHECKPOINT);
}

static void configure_render_pass (struct kan_render_graph_pass_t *pass,
                                   kan_render_pass_t backend_pass,
                                   const struct kan_render_graph_pass_resource_t *pass_resource)
{
    pass->type = pass_resource->type;
    pass->pass = backend_pass;

    kan_dynamic_array_set_capacity (&pass->attachments, pass_resource->attachments.size);
    pass->attachments.size = 0u;

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) pass_resource->attachments.size; ++index)
    {
        struct kan_render_graph_pass_attachment_t *output = kan_dynamic_array_add_last (&pass->attachments);
        KAN_ASSERT (output)

        struct kan_render_pass_attachment_t *input =
            &((struct kan_render_pass_attachment_t *) pass_resource->attachments.data)[index];

        output->type = input->type;
        output->format = input->format;
    }
}

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_execute_render_foundation_pass_management_execution (
    kan_cpu_job_t job, struct render_foundation_pass_management_execution_state_t *state)
{
    KAN_UP_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    {
        if (!KAN_HANDLE_IS_VALID (render_context->render_context))
        {
            KAN_UP_MUTATOR_RETURN;
        }

        KAN_UP_EVENT_FETCH (updated_event, kan_resource_request_updated_event_t)
        {
            if (updated_event->type == state->interned_kan_render_graph_pass_resource_t)
            {
                KAN_UP_VALUE_UPDATE (loading, render_foundation_pass_loading_state_t, request_id,
                                     &updated_event->request_id)
                {
                    KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &updated_event->request_id)
                    {
                        if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
                        {
                            KAN_UP_VALUE_READ (
                                container, KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_render_graph_pass_resource_t),
                                container_id, &request->provided_container_id)
                            {
                                const struct kan_render_graph_pass_resource_t *pass_resource =
                                    KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_render_graph_pass_resource_t, container);

                                struct kan_render_pass_description_t description = {
                                    .type = pass_resource->type,
                                    .attachments_count = pass_resource->attachments.size,
                                    .attachments =
                                        (struct kan_render_pass_attachment_t *) pass_resource->attachments.data,
                                    .tracking_name = loading->name,
                                };

                                kan_render_pass_t backend_pass =
                                    kan_render_pass_create (render_context->render_context, &description);

                                if (KAN_HANDLE_IS_VALID (backend_pass))
                                {
                                    kan_bool_t loaded = KAN_FALSE;
                                    KAN_UP_VALUE_UPDATE (pass, kan_render_graph_pass_t, name, &loading->name)
                                    {
                                        configure_render_pass (pass, backend_pass, pass_resource);
                                        loaded = KAN_TRUE;
                                    }

                                    if (!loaded)
                                    {
                                        KAN_UP_INDEXED_INSERT (new_pass, kan_render_graph_pass_t)
                                        {
                                            new_pass->name = loading->name;
                                            configure_render_pass (new_pass, backend_pass, pass_resource);
                                        }
                                    }
                                }
                                else
                                {
                                    KAN_LOG (render_foundation_graph, KAN_LOG_ERROR,
                                             "Failed to create render pass from resources \"%s\".", request->name)
                                }

                                KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_sleep_event_t)
                                {
                                    event->request_id = request->request_id;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    KAN_UP_MUTATOR_RETURN;
}

struct render_foundation_frame_execution_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (render_foundation_frame_execution)
    KAN_UP_BIND_STATE (render_foundation_frame_execution, state)

    kan_context_system_t render_backend_system;
};

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_deploy_render_foundation_frame_execution (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct render_foundation_frame_execution_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_PASS_MANAGEMENT_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_FRAME_BEGIN);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RENDER_FOUNDATION_FRAME_END);

    state->render_backend_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME);
}

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_execute_render_foundation_frame_execution (
    kan_cpu_job_t job, struct render_foundation_frame_execution_state_t *state)
{
    if (!KAN_HANDLE_IS_VALID (state->render_backend_system))
    {
        KAN_UP_MUTATOR_RETURN;
    }

    KAN_UP_SINGLETON_WRITE (render_context, kan_render_context_singleton_t)
    {
        if (!kan_render_backend_system_get_selected_device_info (state->render_backend_system))
        {
            KAN_LOG (render_foundation_graph, KAN_LOG_WARNING,
                     "Device selection wasn't done prior to render foundation update. Automatically selecting device, "
                     "prioritizing discrete one.")

            const char *selected_device_name = NULL;
            kan_render_device_t selected_device = KAN_HANDLE_INITIALIZE_INVALID;

            struct kan_render_supported_devices_t *supported_devices =
                kan_render_backend_system_get_devices (state->render_backend_system);

            for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) supported_devices->supported_device_count;
                 ++index)
            {
                if (supported_devices->devices[index].device_type == KAN_RENDER_DEVICE_TYPE_DISCRETE_GPU)
                {
                    selected_device_name = supported_devices->devices[index].name;
                    selected_device = supported_devices->devices[index].id;
                    break;
                }
                else if (!KAN_HANDLE_IS_VALID (selected_device))
                {
                    selected_device_name = supported_devices->devices[index].name;
                    selected_device = supported_devices->devices[index].id;
                }
            }

            if (!KAN_HANDLE_IS_VALID (selected_device))
            {
                kan_error_critical ("Unable to find suitable device for render foundation.", __FILE__, __LINE__);
            }

            KAN_LOG (render_foundation_graph, KAN_LOG_WARNING, "Selecting device \"%s\".", selected_device_name)
            if (!kan_render_backend_system_select_device (state->render_backend_system, selected_device))
            {
                kan_error_critical ("Failed to select appropriate device for render.", __FILE__, __LINE__);
            }
        }

        render_context->render_context = kan_render_backend_system_get_render_context (state->render_backend_system);
        render_context->frame_scheduled = kan_render_backend_system_next_frame (state->render_backend_system);
    }

    KAN_UP_SINGLETON_WRITE (render_graph, kan_render_graph_resource_management_singleton_t)
    {
        struct render_foundation_graph_image_cache_node_t *image =
            (struct render_foundation_graph_image_cache_node_t *) render_graph->image_cache.items.first;

        while (image)
        {
            struct render_foundation_graph_image_cache_node_t *next =
                (struct render_foundation_graph_image_cache_node_t *) image->node.list_node.next;

            if (!image->first_usage)
            {
                kan_hash_storage_remove (&render_graph->image_cache, &image->node);
                render_foundation_graph_image_cache_node_destroy (render_graph, image);
            }

            image->first_usage = NULL;
            image = next;
        }

        struct render_foundation_graph_frame_buffer_cache_node_t *frame_buffer =
            (struct render_foundation_graph_frame_buffer_cache_node_t *) render_graph->frame_buffer_cache.items.first;

        while (frame_buffer)
        {
            struct render_foundation_graph_frame_buffer_cache_node_t *next =
                (struct render_foundation_graph_frame_buffer_cache_node_t *) frame_buffer->node.list_node.next;

            if (!frame_buffer->used_in_current_frame)
            {
                kan_hash_storage_remove (&render_graph->frame_buffer_cache, &frame_buffer->node);
                render_foundation_graph_frame_buffer_cache_node_destroy (render_graph, frame_buffer);
            }

            frame_buffer->used_in_current_frame = KAN_FALSE;
            frame_buffer = next;
        }

        kan_hash_storage_update_bucket_count_default (&render_graph->image_cache,
                                                      KAN_UNIVERSE_RENDER_FOUNDATION_GIC_BUCKETS);

        kan_hash_storage_update_bucket_count_default (&render_graph->frame_buffer_cache,
                                                      KAN_UNIVERSE_RENDER_FOUNDATION_GFBC_BUCKETS);

        kan_stack_group_allocator_shrink (&render_graph->temporary_allocator);
        kan_stack_group_allocator_reset (&render_graph->temporary_allocator);
    }

    KAN_UP_MUTATOR_RETURN;
}

void kan_render_graph_pass_resource_init (struct kan_render_graph_pass_resource_t *instance)
{
    instance->type = KAN_RENDER_PASS_GRAPHICS;
    kan_dynamic_array_init (&instance->attachments, 0u, sizeof (struct kan_render_pass_attachment_t),
                            _Alignof (struct kan_render_pass_attachment_t), kan_allocation_group_stack_get ());
}

void kan_render_graph_pass_resource_shutdown (struct kan_render_graph_pass_resource_t *instance)
{
    kan_dynamic_array_shutdown (&instance->attachments);
}

void kan_render_graph_pass_init (struct kan_render_graph_pass_t *instance)
{
    instance->name = NULL;
    instance->type = KAN_RENDER_PASS_GRAPHICS;
    instance->pass = KAN_HANDLE_SET_INVALID (kan_render_pass_t);
    kan_dynamic_array_init (&instance->attachments, 0u, sizeof (struct kan_render_graph_pass_attachment_t),
                            _Alignof (struct kan_render_graph_pass_attachment_t), kan_allocation_group_stack_get ());
}

void kan_render_graph_pass_shutdown (struct kan_render_graph_pass_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->pass))
    {
        kan_render_pass_destroy (instance->pass);
    }

    kan_dynamic_array_shutdown (&instance->attachments);
}

void kan_render_context_singleton_init (struct kan_render_context_singleton_t *instance)
{
    instance->render_context = KAN_HANDLE_SET_INVALID (kan_render_context_t);
    instance->frame_scheduled = KAN_FALSE;
}

void kan_render_graph_resource_management_singleton_init (
    struct kan_render_graph_resource_management_singleton_t *instance)
{
    instance->request_lock = kan_atomic_int_init (0);
    instance->allocation_group = kan_allocation_group_stack_get ();
    instance->cache_group = kan_allocation_group_get_child (instance->allocation_group, "cache");

    instance->cached_image_name = kan_string_intern ("render_graph_cached_image");
    instance->cached_frame_buffer_name = kan_string_intern ("render_graph_cached_frame_buffer");

    kan_hash_storage_init (&instance->image_cache, instance->cache_group, KAN_UNIVERSE_RENDER_FOUNDATION_GIC_BUCKETS);
    kan_hash_storage_init (&instance->frame_buffer_cache, instance->cache_group,
                           KAN_UNIVERSE_RENDER_FOUNDATION_GFBC_BUCKETS);

    kan_stack_group_allocator_init (&instance->temporary_allocator,
                                    kan_allocation_group_get_child (instance->allocation_group, "temporary"),
                                    KAN_UNIVERSE_RENDER_FOUNDATION_GTA_INITIAL_STACK);
}

kan_bool_t kan_render_graph_resource_management_singleton_request_pass (
    const struct kan_render_graph_resource_management_singleton_t *instance,
    const struct kan_render_graph_pass_instance_request_t *request,
    struct kan_render_graph_pass_instance_allocation_t *output)
{
    struct kan_render_graph_resource_management_singleton_t *mutable_instance =
        (struct kan_render_graph_resource_management_singleton_t *) instance;

    struct kan_render_frame_buffer_attachment_description_t *attachment_descriptions = kan_allocate_general (
        instance->cache_group,
        sizeof (struct kan_render_frame_buffer_attachment_description_t) * request->pass->attachments.size,
        _Alignof (struct kan_render_frame_buffer_attachment_description_t));

    struct render_foundation_graph_image_cache_node_t
        *images_static[KAN_UNIVERSE_RENDER_FOUNDATION_ATTACHMENTS_MAX_STATIC];
    struct render_foundation_graph_image_cache_node_t **images = images_static;
    kan_bool_t successful = KAN_TRUE;

    if (KAN_UNIVERSE_RENDER_FOUNDATION_ATTACHMENTS_MAX_STATIC < request->pass->attachments.size)
    {
        images = kan_allocate_general (
            instance->allocation_group,
            sizeof (struct render_foundation_graph_image_cache_node_t *) * request->pass->attachments.size,
            _Alignof (struct render_foundation_graph_image_cache_node_t *));
    }

    kan_atomic_int_lock (&mutable_instance->request_lock);
    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) request->pass->attachments.size; ++index)
    {
        struct kan_render_graph_pass_attachment_t *pass_attachment =
            &((struct kan_render_graph_pass_attachment_t *) request->pass->attachments.data)[index];
        struct kan_render_graph_pass_instance_request_attachment_info_t *info = &request->attachment_info[index];
        struct kan_render_frame_buffer_attachment_description_t *attachment_output = &attachment_descriptions[index];

        if (KAN_HANDLE_IS_VALID (info->use_surface))
        {
            attachment_output->type = KAN_FRAME_BUFFER_ATTACHMENT_SURFACE;
            attachment_output->surface = info->use_surface;
            images[index] = NULL;
            continue;
        }

        const kan_hash_t attachment_hash = calculate_cached_image_hash (
            pass_attachment->format, request->frame_buffer_width, request->frame_buffer_height);

        const struct kan_hash_storage_bucket_t *bucket =
            kan_hash_storage_query (&mutable_instance->image_cache, attachment_hash);

        struct render_foundation_graph_image_cache_node_t *node =
            (struct render_foundation_graph_image_cache_node_t *) bucket->first;

        const struct render_foundation_graph_image_cache_node_t *node_end =
            (struct render_foundation_graph_image_cache_node_t *) (bucket->last ? bucket->last->next : NULL);

        while (node != node_end)
        {
            if (node->node.hash == attachment_hash && node->format == pass_attachment->format &&
                node->width == request->frame_buffer_width && node->height == request->frame_buffer_height &&
                // Technically, we can use images with sampling even if sampling is not used, but it shouldn't matter
                // in real world scenarios as no-sampling mode is usually only selected for scene depth textures,
                // that are rarely shared to other passes except for passes with scene depth rendering.
                node->supports_sampling == info->used_by_dependant_instances)
            {
                kan_bool_t found_collision = KAN_FALSE;
                if (info->used_by_dependant_instances)
                {
                    struct render_foundation_graph_image_usage_t *usage = node->first_usage;
                    while (usage && !found_collision)
                    {
                        for (kan_loop_size_t dependant_index = 0u;
                             dependant_index < (kan_loop_size_t) request->dependant_count && !found_collision;
                             ++dependant_index)
                        {
                            if (KAN_HANDLE_IS_EQUAL (request->dependant[index], usage->producer_pass))
                            {
                                found_collision = KAN_TRUE;
                                break;
                            }

                            for (kan_loop_size_t user_index = 0u; user_index < usage->user_passes_count; ++user_index)
                            {
                                if (KAN_HANDLE_IS_EQUAL (request->dependant[index], usage->user_passes[user_index]))
                                {
                                    found_collision = KAN_TRUE;
                                    break;
                                }
                            }
                        }

                        usage = usage->next;
                    }
                }

                if (!found_collision)
                {
                    break;
                }
            }

            node = (struct render_foundation_graph_image_cache_node_t *) node->node.list_node.next;
        }

        if (!node)
        {
            node = render_foundation_graph_image_cache_node_create (
                instance, request->context, pass_attachment->format, request->frame_buffer_width,
                request->frame_buffer_height, info->used_by_dependant_instances);

            if (!node)
            {
                successful = KAN_FALSE;
                break;
            }

            kan_hash_storage_update_bucket_count_default (&mutable_instance->image_cache,
                                                          KAN_UNIVERSE_RENDER_FOUNDATION_GIC_BUCKETS);
            kan_hash_storage_add (&mutable_instance->image_cache, &node->node);
        }

        attachment_output->type = KAN_FRAME_BUFFER_ATTACHMENT_IMAGE;
        attachment_output->image = node->image;
        images[index] = node;
    }

    struct render_foundation_graph_frame_buffer_cache_node_t *frame_buffer_node = NULL;
    if (successful)
    {
        const kan_hash_t frame_buffer_hash = calculate_cached_frame_buffer_hash (
            request->pass->pass, request->pass->attachments.size, attachment_descriptions);

        const struct kan_hash_storage_bucket_t *bucket =
            kan_hash_storage_query (&mutable_instance->frame_buffer_cache, (kan_hash_t) frame_buffer_hash);

        struct render_foundation_graph_frame_buffer_cache_node_t *node =
            (struct render_foundation_graph_frame_buffer_cache_node_t *) bucket->first;

        const struct render_foundation_graph_frame_buffer_cache_node_t *node_end =
            (struct render_foundation_graph_frame_buffer_cache_node_t *) (bucket->last ? bucket->last->next : NULL);

        while (node != node_end)
        {
            if (node->node.hash == frame_buffer_hash && KAN_HANDLE_IS_EQUAL (node->pass, request->pass->pass) &&
                node->attachments_count == request->pass->attachments.size)
            {
                kan_bool_t attachments_equal = KAN_TRUE;
                for (kan_loop_size_t index = 0u; index < node->attachments_count && attachments_equal; ++index)
                {
                    if (attachment_descriptions[index].type != node->attachments[index].type)
                    {
                        attachments_equal = KAN_FALSE;
                        break;
                    }

                    switch (attachment_descriptions[index].type)
                    {
                    case KAN_FRAME_BUFFER_ATTACHMENT_IMAGE:
                        if (!KAN_HANDLE_IS_EQUAL (attachment_descriptions[index].image, node->attachments[index].image))
                        {
                            attachments_equal = KAN_FALSE;
                            break;
                        }

                        break;

                    case KAN_FRAME_BUFFER_ATTACHMENT_SURFACE:
                        if (!KAN_HANDLE_IS_EQUAL (attachment_descriptions[index].surface,
                                                  node->attachments[index].surface))
                        {
                            attachments_equal = KAN_FALSE;
                            break;
                        }

                        break;
                    }
                }

                if (attachments_equal)
                {
                    break;
                }
            }

            node = (struct render_foundation_graph_frame_buffer_cache_node_t *) node->node.list_node.next;
        }

        if (!node)
        {
            node = render_foundation_graph_frame_buffer_cache_node_create (
                instance, request->context, request->pass->pass, request->pass->attachments.size,
                attachment_descriptions);

            if (node)
            {
                kan_hash_storage_update_bucket_count_default (&mutable_instance->frame_buffer_cache,
                                                              KAN_UNIVERSE_RENDER_FOUNDATION_GFBC_BUCKETS);
                kan_hash_storage_add (&mutable_instance->frame_buffer_cache, &node->node);
            }
            else
            {
                successful = KAN_FALSE;
            }
        }

        frame_buffer_node = node;
    }

    if (successful)
    {
        output->pass_instance =
            kan_render_pass_instantiate (request->pass->pass, frame_buffer_node->frame_buffer, request->viewport_bounds,
                                         request->scissor, request->attachment_clear_values);

        output->attachments_count = request->pass->attachments.size;
        output->attachments = attachment_descriptions;

        if (!KAN_HANDLE_IS_VALID (output->pass_instance))
        {
            KAN_LOG (render_foundation_graph, KAN_LOG_ERROR, "Failed to create new frame buffer for render graph.")
            successful = KAN_FALSE;
        }
    }

    if (successful)
    {
        frame_buffer_node->used_in_current_frame = KAN_TRUE;
        for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) request->pass->attachments.size; ++index)
        {
            struct kan_render_graph_pass_instance_request_attachment_info_t *info = &request->attachment_info[index];
            struct render_foundation_graph_image_cache_node_t *node = images[index];

            if (!node)
            {
                KAN_ASSERT (KAN_HANDLE_IS_VALID (info->use_surface))
                continue;
            }

            // One of the cornerstones of current render graph implementation is that we need to reduce parallelism
            // in order to use less resources. For example, when we have split screen, in most cases we would have
            // shadow maps that are visible in one viewport, but are not visible in other viewport. First though in
            // this case it to let every shadow map be rendered independently as there is no dependencies in graph.
            // However, chance that GPU will benefit from such large parallelism is slim, but this parallelism would
            // require much more memory. Instead, we can inject additional dependencies between pass instances to make
            // it possible to reuse more textures by reducing parallelism. It should reduce memory usage while also
            // reducing parallelism, but, as said before, lost parallelism might actually not be beneficial at all.

            const kan_instance_size_t dependant_to_register =
                info->used_by_dependant_instances ? request->dependant_count : 0u;

            struct render_foundation_graph_image_usage_t *usage =
                kan_stack_group_allocator_allocate (&mutable_instance->temporary_allocator,
                                                    sizeof (struct render_foundation_graph_image_usage_t) +
                                                        sizeof (kan_render_pass_instance_t) * dependant_to_register,
                                                    _Alignof (struct render_foundation_graph_image_usage_t));

            usage->next = node->first_usage;
            node->first_usage = usage;
            usage->producer_pass = output->pass_instance;
            usage->user_passes_count = dependant_to_register;

            if (dependant_to_register > 0u)
            {
                memcpy (usage->user_passes, request->dependant,
                        sizeof (kan_render_pass_instance_t) * dependant_to_register);
            }

            if (usage->next)
            {
                if (dependant_to_register > 0u)
                {
                    for (kan_loop_size_t dependant_index = 0u;
                         dependant_index < (kan_loop_size_t) dependant_to_register; ++dependant_index)
                    {
                        kan_render_pass_instance_add_dynamic_dependency (usage->next->producer_pass,
                                                                         usage->user_passes[dependant_index]);
                    }
                }
                else
                {
                    kan_render_pass_instance_add_dynamic_dependency (usage->next->producer_pass, usage->producer_pass);
                }
            }
        }
    }

    kan_atomic_int_unlock (&mutable_instance->request_lock);
    if (images != images_static)
    {
        kan_free_general (
            instance->allocation_group, images,
            sizeof (struct render_foundation_graph_image_cache_node_t *) * request->pass->attachments.size);
    }

    if (!successful)
    {
        kan_free_general (
            instance->cache_group, attachment_descriptions,
            sizeof (struct kan_render_frame_buffer_attachment_description_t) * request->pass->attachments.size);
    }

    return successful;
}

void kan_render_graph_resource_management_singleton_shutdown (
    struct kan_render_graph_resource_management_singleton_t *instance)
{
    struct render_foundation_graph_image_cache_node_t *image_node =
        (struct render_foundation_graph_image_cache_node_t *) instance->image_cache.items.first;

    while (image_node)
    {
        struct render_foundation_graph_image_cache_node_t *next =
            (struct render_foundation_graph_image_cache_node_t *) image_node->node.list_node.next;
        render_foundation_graph_image_cache_node_destroy (instance, image_node);
        image_node = next;
    }

    struct render_foundation_graph_frame_buffer_cache_node_t *frame_buffer_node =
        (struct render_foundation_graph_frame_buffer_cache_node_t *) instance->frame_buffer_cache.items.first;

    while (frame_buffer_node)
    {
        struct render_foundation_graph_frame_buffer_cache_node_t *next =
            (struct render_foundation_graph_frame_buffer_cache_node_t *) frame_buffer_node->node.list_node.next;
        render_foundation_graph_frame_buffer_cache_node_destroy (instance, frame_buffer_node);
        frame_buffer_node = next;
    }

    kan_hash_storage_shutdown (&instance->image_cache);
    kan_hash_storage_shutdown (&instance->frame_buffer_cache);
    kan_stack_group_allocator_shutdown (&instance->temporary_allocator);
}
