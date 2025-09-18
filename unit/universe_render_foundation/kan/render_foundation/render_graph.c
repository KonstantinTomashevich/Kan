#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <string.h>

#include <kan/context/all_system_names.h>
#include <kan/context/render_backend_system.h>
#include <kan/log/logging.h>
#include <kan/render_foundation/render_graph.h>
#include <kan/universe/macro.h>

KAN_LOG_DEFINE_CATEGORY (render_foundation_graph);
KAN_USE_STATIC_INTERNED_IDS
KAN_USE_STATIC_CPU_SECTIONS

KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP (render_foundation_frame_schedule)
UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_GROUP_META (render_foundation_frame_schedule,
                                                          KAN_RENDER_FOUNDATION_FRAME_MUTATOR_GROUP);

KAN_REFLECTION_IGNORE
struct render_foundation_graph_image_usage_t
{
    struct render_foundation_graph_image_usage_t *next;
    struct kan_render_graph_resource_response_t *producer_response;
    kan_instance_size_t user_responses_count;
    struct kan_render_graph_resource_response_t *user_responses[];
};

KAN_REFLECTION_IGNORE
struct render_foundation_graph_image_cache_node_t
{
    struct kan_hash_storage_node_t node;
    kan_render_image_t image;
    struct kan_render_image_description_t description;
    struct render_foundation_graph_image_usage_t *first_usage;
};

static inline kan_hash_t calculate_image_description_hash (struct kan_render_image_description_t *description)
{
    static_assert (KAN_RENDER_IMAGE_FORMAT_COUNT <= UINT8_MAX, "Possible to pack format into byte.");
    const kan_hash_t attributes_hash = (((uint8_t) description->format) << 0u) | (description->layers << 1u);

    const kan_hash_t sizes_hash =
        kan_hash_combine ((kan_hash_t) description->width,
                          kan_hash_combine ((kan_hash_t) description->height, (kan_hash_t) description->depth));

    return kan_hash_combine (attributes_hash, sizes_hash);
}

static struct render_foundation_graph_image_cache_node_t *render_foundation_graph_image_cache_node_create (
    const struct kan_render_graph_resource_management_singleton_t *singleton,
    kan_render_context_t context,
    kan_hash_t precalculated_description_hash,
    const struct kan_render_image_description_t *description)
{
    struct kan_render_image_description_t creation_description = *description;
    creation_description.tracking_name = KAN_STATIC_INTERNED_ID_GET (render_graph_cached_image);
    kan_render_image_t image = kan_render_image_create (context, &creation_description);

    if (!KAN_HANDLE_IS_VALID (image))
    {
        KAN_LOG (render_foundation_graph, KAN_LOG_ERROR, "Failed to create new image for render graph.")
        return NULL;
    }

    struct render_foundation_graph_image_cache_node_t *node =
        kan_allocate_batched (singleton->cache_group, sizeof (struct render_foundation_graph_image_cache_node_t));

    node->image = image;
    node->description = creation_description;
    node->first_usage = NULL;
    node->node.hash = precalculated_description_hash;
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
    bool used_in_current_frame;

    kan_instance_size_t attachments_count;
    struct kan_render_frame_buffer_attachment_description_t attachments[];
};

static kan_hash_t calculate_cached_frame_buffer_hash (
    kan_render_pass_t pass,
    kan_instance_size_t attachments_count,
    struct kan_render_frame_buffer_attachment_description_t *attachments)
{
    kan_hash_t hash = (kan_hash_t) KAN_HANDLE_GET (pass);
    for (kan_loop_size_t index = 0u; index < attachments_count; ++index)
    {
        hash = kan_hash_combine (hash, kan_hash_combine ((kan_hash_t) KAN_HANDLE_GET (attachments[index].image),
                                                         (kan_hash_t) attachments[index].layer));
    }

    return hash;
}

static struct render_foundation_graph_frame_buffer_cache_node_t *
render_foundation_graph_frame_buffer_cache_node_create (
    const struct kan_render_graph_resource_management_singleton_t *singleton,
    kan_render_context_t context,
    kan_hash_t precalculated_hash,
    kan_render_pass_t pass,
    kan_instance_size_t attachments_count,
    struct kan_render_frame_buffer_attachment_description_t *attachments)
{
    struct kan_render_frame_buffer_description_t description = {
        .associated_pass = pass,
        .attachments_count = attachments_count,
        .attachments = attachments,
        .tracking_name = KAN_STATIC_INTERNED_ID_GET (render_graph_cached_frame_buffer),
    };

    kan_render_frame_buffer_t frame_buffer = kan_render_frame_buffer_create (context, &description);
    if (!KAN_HANDLE_IS_VALID (frame_buffer))
    {
        KAN_LOG (render_foundation_graph, KAN_LOG_ERROR, "Failed to create new frame buffer for render graph.")
        return NULL;
    }

    struct render_foundation_graph_frame_buffer_cache_node_t *node =
        kan_allocate_general (singleton->cache_group,
                              sizeof (struct render_foundation_graph_frame_buffer_cache_node_t) +
                                  attachments_count * sizeof (struct kan_render_frame_buffer_attachment_description_t),
                              alignof (struct render_foundation_graph_frame_buffer_cache_node_t));

    node->frame_buffer = frame_buffer;
    node->pass = pass;
    node->used_in_current_frame = false;

    node->attachments_count = attachments_count;
    memcpy (node->attachments, attachments,
            attachments_count * sizeof (struct kan_render_frame_buffer_attachment_description_t));

    node->node.hash = precalculated_hash;
    return node;
}

static void render_foundation_graph_frame_buffer_cache_node_destroy (
    const struct kan_render_graph_resource_management_singleton_t *singleton,
    struct render_foundation_graph_frame_buffer_cache_node_t *instance)
{
    kan_render_frame_buffer_destroy (instance->frame_buffer);
    kan_free_general (
        singleton->cache_group, instance,
        sizeof (struct render_foundation_graph_frame_buffer_cache_node_t) +
            sizeof (struct kan_render_frame_buffer_attachment_description_t) * instance->attachments_count);
}

struct render_foundation_frame_schedule_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (render_foundation_frame_schedule)
    KAN_UM_BIND_STATE (render_foundation_frame_schedule, state)
    kan_context_system_t render_backend_system;
};

UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_DEPLOY (render_foundation_frame_schedule)
{
    kan_static_interned_ids_ensure_initialized ();
    kan_cpu_static_sections_ensure_initialized ();

    state->render_backend_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME);

    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_FRAME_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RENDER_FOUNDATION_FRAME_END_CHECKPOINT);
}

static void schedule_frame (struct render_foundation_frame_schedule_state_t *state)
{
    KAN_CPU_SCOPED_STATIC_SECTION (schedule_frame)
    KAN_UMI_SINGLETON_WRITE (render_context, kan_render_context_singleton_t)

    render_context->selected_device_info =
        kan_render_backend_system_get_selected_device_info (state->render_backend_system);

    if (!render_context->selected_device_info)
    {
        KAN_LOG (render_foundation_graph, KAN_LOG_WARNING,
                 "Device selection wasn't done prior to render foundation update. Automatically selecting device, "
                 "prioritizing discrete one.")

        const char *selected_device_name = NULL;
        kan_render_device_t selected_device = KAN_HANDLE_INITIALIZE_INVALID;

        struct kan_render_supported_devices_t *supported_devices =
            kan_render_backend_system_get_devices (state->render_backend_system);

        for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) supported_devices->supported_device_count; ++index)
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

        render_context->selected_device_info =
            kan_render_backend_system_get_selected_device_info (state->render_backend_system);
    }

    render_context->render_context = kan_render_backend_system_get_render_context (state->render_backend_system);
    render_context->frame_scheduled = kan_render_backend_system_next_frame (state->render_backend_system);
}

static void free_unused_resources_from_graph (struct render_foundation_frame_schedule_state_t *state)
{
    KAN_CPU_SCOPED_STATIC_SECTION (free_unused_resources_from_graph)
    KAN_UMI_SINGLETON_WRITE (render_graph, kan_render_graph_resource_management_singleton_t)
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
        else
        {
            image->first_usage = NULL;
        }

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
        else
        {
            frame_buffer->used_in_current_frame = false;
        }

        frame_buffer = next;
    }

    kan_hash_storage_update_bucket_count_default (&render_graph->image_cache,
                                                  KAN_UNIVERSE_RENDER_FOUNDATION_GIC_BUCKETS);

    kan_hash_storage_update_bucket_count_default (&render_graph->frame_buffer_cache,
                                                  KAN_UNIVERSE_RENDER_FOUNDATION_GFBC_BUCKETS);

    kan_stack_group_allocator_shrink (&render_graph->temporary_allocator);
    kan_stack_group_allocator_reset (&render_graph->temporary_allocator);
}

KAN_UM_MUTATOR_EXECUTE (render_foundation_frame_schedule)
{
    if (!KAN_HANDLE_IS_VALID (state->render_backend_system))
    {
        return;
    }

    schedule_frame (state);
    free_unused_resources_from_graph (state);
}

void kan_render_context_singleton_init (struct kan_render_context_singleton_t *instance)
{
    instance->render_context = KAN_HANDLE_SET_INVALID (kan_render_context_t);
    instance->selected_device_info = NULL;
    instance->frame_scheduled = false;
}

void kan_render_graph_resource_management_singleton_init (
    struct kan_render_graph_resource_management_singleton_t *instance)
{
    instance->request_lock = kan_atomic_int_init (0);
    instance->allocation_group = kan_allocation_group_stack_get ();
    instance->cache_group = kan_allocation_group_get_child (instance->allocation_group, "cache");

    kan_hash_storage_init (&instance->image_cache, instance->cache_group, KAN_UNIVERSE_RENDER_FOUNDATION_GIC_BUCKETS);
    kan_hash_storage_init (&instance->frame_buffer_cache, instance->cache_group,
                           KAN_UNIVERSE_RENDER_FOUNDATION_GFBC_BUCKETS);

    kan_stack_group_allocator_init (&instance->temporary_allocator,
                                    kan_allocation_group_get_child (instance->allocation_group, "temporary"),
                                    KAN_UNIVERSE_RENDER_FOUNDATION_GTA_INITIAL_STACK);
}

const struct kan_render_graph_resource_response_t *kan_render_graph_resource_management_singleton_request (
    const struct kan_render_graph_resource_management_singleton_t *instance,
    const struct kan_render_graph_resource_request_t *request)
{
    struct kan_render_graph_resource_management_singleton_t *mutable_instance =
        (struct kan_render_graph_resource_management_singleton_t *) instance;

    KAN_ATOMIC_INT_SCOPED_LOCK (&mutable_instance->request_lock);
    bool successful = true;

    // Start by allocating response.
    struct kan_render_graph_resource_response_t *response = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
        &mutable_instance->temporary_allocator, struct kan_render_graph_resource_response_t);

    response->usage_begin_checkpoint = kan_render_pass_instance_checkpoint_create (request->context);
    response->usage_end_checkpoint = kan_render_pass_instance_checkpoint_create (request->context);
    kan_render_pass_instance_checkpoint_add_checkpoint_dependency (response->usage_end_checkpoint,
                                                                   response->usage_begin_checkpoint);

    for (kan_loop_size_t index = 0u; index < request->dependant_count; ++index)
    {
        kan_render_pass_instance_checkpoint_add_checkpoint_dependency (
            request->dependant[index]->usage_begin_checkpoint, response->usage_end_checkpoint);
    }

    response->images_count = request->images_count;
    if (response->images_count > 0u)
    {
        response->images = kan_stack_group_allocator_allocate (&mutable_instance->temporary_allocator,
                                                               sizeof (kan_render_image_t) * request->images_count,
                                                               alignof (kan_render_image_t));
    }
    else
    {
        response->images = NULL;
    }

    response->frame_buffers_count = request->frame_buffers_count;
    if (response->frame_buffers_count > 0u)
    {
        response->frame_buffers = kan_stack_group_allocator_allocate (
            &mutable_instance->temporary_allocator, sizeof (kan_render_frame_buffer_t) * request->frame_buffers_count,
            alignof (kan_render_frame_buffer_t));
    }
    else
    {
        response->frame_buffers = NULL;
    }

    // Try to retrieve or create proper images.
    for (kan_loop_size_t index = 0u; index < request->images_count; ++index)
    {
        struct kan_render_graph_resource_image_request_t *image_request = &request->images[index];
        if (image_request->description.mips > 1u)
        {
            KAN_LOG (render_foundation_graph, KAN_LOG_ERROR,
                     "Received image requests with mips, makes no sense for render target allocation.")
            successful = false;
            break;
        }

        if (!image_request->description.render_target)
        {
            KAN_LOG (render_foundation_graph, KAN_LOG_ERROR,
                     "Received image requests without render target flag, makes no sense for render target allocation.")
            successful = false;
            break;
        }

        const kan_hash_t request_hash = calculate_image_description_hash (&image_request->description);
        const struct kan_hash_storage_bucket_t *bucket =
            kan_hash_storage_query (&mutable_instance->image_cache, request_hash);

        struct render_foundation_graph_image_cache_node_t *node =
            (struct render_foundation_graph_image_cache_node_t *) bucket->first;
        const struct render_foundation_graph_image_cache_node_t *node_end =
            (struct render_foundation_graph_image_cache_node_t *) (bucket->last ? bucket->last->next : NULL);

        while (node != node_end)
        {
            if (node->node.hash == request_hash &&
                // Catch special case: one request can have several images with the same description, for example two
                // textures for G buffer that have literally equal descriptions. But if request contains them as two
                // distinct images, then we need to return two distinct images.
                // If image is requested for this response during this request resolution, it is always stored in its
                // first usage. Therefore, checking first usage is enough.
                (!node->first_usage || node->first_usage->producer_response != response) &&
                // Check that description is the same.
                node->description.format == image_request->description.format &&
                node->description.width == image_request->description.width &&
                node->description.height == image_request->description.height &&
                node->description.depth == image_request->description.depth &&
                node->description.layers == image_request->description.layers &&
                (!image_request->description.supports_sampling || node->description.supports_sampling))
            {
                // Found suitable image. Let's check if we can use it.
                bool found_collision = false;

                if (!image_request->internal)
                {
                    struct render_foundation_graph_image_usage_t *usage = node->first_usage;
                    while (usage && !found_collision)
                    {
                        for (kan_loop_size_t dependant_index = 0u;
                             dependant_index < (kan_loop_size_t) request->dependant_count && !found_collision;
                             ++dependant_index)
                        {
                            if (request->dependant[index] == usage->producer_response)
                            {
                                found_collision = true;
                                break;
                            }

                            for (kan_loop_size_t user_index = 0u; user_index < usage->user_responses_count;
                                 ++user_index)
                            {
                                if (request->dependant[index] == usage->user_responses[user_index])
                                {
                                    found_collision = true;
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

        if (node == node_end)
        {
            node = render_foundation_graph_image_cache_node_create (instance, request->context, request_hash,
                                                                    &image_request->description);

            if (!node)
            {
                successful = false;
                break;
            }

            kan_hash_storage_update_bucket_count_default (&mutable_instance->image_cache,
                                                          KAN_UNIVERSE_RENDER_FOUNDATION_GIC_BUCKETS);
            kan_hash_storage_add (&mutable_instance->image_cache, &node->node);
        }

        response->images[index] = node->image;

        // Technically, adding usage right now might be considered too early as allocation might fail later.
        // But in reality, this usage would not cause any harm as it would be just an empty usage with empty checkpoints
        // and no dependencies (as user will not receive response due to error).
        // Therefore, it is safe to add usage right now.
        //
        // One of the cornerstones of current render graph implementation is that we need to reduce parallelism
        // in order to use less resources. For example, when we have split screen, in most cases we would have
        // shadow maps that are visible in one viewport, but are not visible in other viewport. First though in
        // this case it to let every shadow map be rendered independently as there is no dependencies in graph.
        // However, chance that GPU will benefit from such large parallelism is slim, but this parallelism would
        // require much more memory. Instead, we can inject additional dependencies between pass instances to make
        // it possible to reuse more textures by reducing parallelism. It should reduce memory usage while also
        // reducing parallelism, but, as said before, lost parallelism might actually not be beneficial at all.

        const kan_instance_size_t dependant_to_register = image_request->internal ? 0u : request->dependant_count;
        struct render_foundation_graph_image_usage_t *usage = kan_stack_group_allocator_allocate (
            &mutable_instance->temporary_allocator,
            sizeof (struct render_foundation_graph_image_usage_t) +
                sizeof (struct kan_render_graph_resource_response_t *) * dependant_to_register,
            alignof (struct render_foundation_graph_image_usage_t));

        usage->next = node->first_usage;
        node->first_usage = usage;
        usage->producer_response = response;
        usage->user_responses_count = dependant_to_register;

        if (dependant_to_register > 0u)
        {
            memcpy (usage->user_responses, request->dependant,
                    sizeof (struct kan_render_graph_resource_response_t *) * dependant_to_register);
        }

        if (usage->next)
        {
            // Just in case. Should never happen, but better to catch it early.
            KAN_ASSERT (usage->next->producer_response != response)

            if (dependant_to_register > 0u)
            {
                for (kan_loop_size_t dependant_index = 0u; dependant_index < (kan_loop_size_t) dependant_to_register;
                     ++dependant_index)
                {
                    kan_render_pass_instance_checkpoint_add_checkpoint_dependency (
                        usage->next->producer_response->usage_begin_checkpoint,
                        usage->user_responses[dependant_index]->usage_end_checkpoint);
                }
            }
            else
            {
                kan_render_pass_instance_checkpoint_add_checkpoint_dependency (
                    usage->next->producer_response->usage_begin_checkpoint, response->usage_end_checkpoint);
            }
        }
    }

    if (successful)
    {
        for (kan_loop_size_t frame_buffer_index = 0u; frame_buffer_index < request->frame_buffers_count;
             ++frame_buffer_index)
        {
            struct kan_render_graph_resource_frame_buffer_request_t *frame_buffer_request =
                &request->frame_buffers[frame_buffer_index];

            struct kan_render_frame_buffer_attachment_description_t *attachments =
                kan_stack_group_allocator_allocate (&mutable_instance->temporary_allocator,
                                                    sizeof (struct kan_render_frame_buffer_attachment_description_t) *
                                                        frame_buffer_request->attachments_count,
                                                    alignof (struct kan_render_frame_buffer_attachment_description_t));

            for (kan_loop_size_t attachment_index = 0u; attachment_index < frame_buffer_request->attachments_count;
                 ++attachment_index)
            {
                struct kan_render_graph_resource_frame_buffer_request_attachment_t *attachment_request =
                    &frame_buffer_request->attachments[attachment_index];

                attachments[attachment_index] = (struct kan_render_frame_buffer_attachment_description_t) {
                    .image = response->images[attachment_request->image_index],
                    .layer = attachment_request->image_layer,
                };
            }

            const kan_hash_t request_hash = calculate_cached_frame_buffer_hash (
                frame_buffer_request->pass, frame_buffer_request->attachments_count, attachments);

            const struct kan_hash_storage_bucket_t *bucket =
                kan_hash_storage_query (&mutable_instance->frame_buffer_cache, request_hash);

            struct render_foundation_graph_frame_buffer_cache_node_t *node =
                (struct render_foundation_graph_frame_buffer_cache_node_t *) bucket->first;

            const struct render_foundation_graph_frame_buffer_cache_node_t *node_end =
                (struct render_foundation_graph_frame_buffer_cache_node_t *) (bucket->last ? bucket->last->next : NULL);

            while (node != node_end)
            {
                if (node->node.hash == request_hash && KAN_HANDLE_IS_EQUAL (node->pass, frame_buffer_request->pass) &&
                    node->attachments_count == frame_buffer_request->attachments_count)
                {
                    bool attachments_equal = true;
                    for (kan_loop_size_t index = 0u; index < node->attachments_count; ++index)
                    {
                        if (!KAN_HANDLE_IS_EQUAL (attachments[index].image, node->attachments[index].image) ||
                            attachments[index].layer != node->attachments[index].layer)
                        {
                            attachments_equal = false;
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

            if (node == node_end)
            {
                node = render_foundation_graph_frame_buffer_cache_node_create (
                    instance, request->context, request_hash, frame_buffer_request->pass,
                    frame_buffer_request->attachments_count, attachments);

                if (node)
                {
                    kan_hash_storage_update_bucket_count_default (&mutable_instance->frame_buffer_cache,
                                                                  KAN_UNIVERSE_RENDER_FOUNDATION_GFBC_BUCKETS);
                    kan_hash_storage_add (&mutable_instance->frame_buffer_cache, &node->node);
                }
                else
                {
                    successful = false;
                    break;
                }
            }

            node->used_in_current_frame = true;
            response->frame_buffers[frame_buffer_index] = node->frame_buffer;
        }
    }

    return successful ? response : NULL;
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
