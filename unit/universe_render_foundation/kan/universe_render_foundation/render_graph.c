#include <string.h>

#include <kan/context/all_system_names.h>
#include <kan/log/logging.h>
#include <kan/precise_time/precise_time.h>
#include <kan/resource_pipeline/resource_pipeline.h>
#include <kan/universe/macro.h>
#include <kan/universe_render_foundation/render_graph.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

KAN_LOG_DEFINE_CATEGORY (render_foundation_graph);
KAN_USE_STATIC_INTERNED_IDS

KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP (render_foundation_pass_management_planning)
KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP (render_foundation_pass_management_execution)
KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP (render_foundation_frame_execution)
UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_GROUP_META (render_foundation_root_routine,
                                                          KAN_RENDER_FOUNDATION_ROOT_ROUTINE_MUTATOR_GROUP);

struct render_foundation_pass_loading_state_t
{
    kan_interned_string_t name;
    kan_resource_request_id_t request_id;
    kan_time_size_t inspection_time_ns;
};

struct render_foundation_pass_variant_loading_state_t
{
    kan_interned_string_t pass_name;
    kan_interned_string_t resource_name;
    kan_instance_size_t variant_index;
    kan_resource_request_id_t request_id;
};

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

struct render_foundation_pass_management_planning_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (render_foundation_pass_management_planning)
    KAN_UM_BIND_STATE (render_foundation_pass_management_planning, state)
};

UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_DEPLOY (render_foundation_pass_management_planning)
{
    kan_static_interned_ids_ensure_initialized ();
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_PASS_MANAGEMENT_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RESOURCE_PROVIDER_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RENDER_FOUNDATION_FRAME_BEGIN);
}

static void destroy_pass_frame_buffers (struct kan_render_graph_resource_management_singleton_t *render_graph,
                                        kan_render_pass_t pass)
{
    struct render_foundation_graph_frame_buffer_cache_node_t *frame_buffer =
        (struct render_foundation_graph_frame_buffer_cache_node_t *) render_graph->frame_buffer_cache.items.first;

    while (frame_buffer)
    {
        struct render_foundation_graph_frame_buffer_cache_node_t *next =
            (struct render_foundation_graph_frame_buffer_cache_node_t *) frame_buffer->node.list_node.next;

        if (KAN_HANDLE_IS_EQUAL (frame_buffer->pass, pass))
        {
            kan_hash_storage_remove (&render_graph->frame_buffer_cache, &frame_buffer->node);
            render_foundation_graph_frame_buffer_cache_node_destroy (render_graph, frame_buffer);
        }

        frame_buffer = next;
    }
}

#define HELPER_DELETE_LOADED_PASS(PASS_NAME)                                                                           \
    KAN_UML_VALUE_DELETE (pass, kan_render_graph_pass_t, name, &PASS_NAME)                                             \
    {                                                                                                                  \
        destroy_pass_frame_buffers (render_graph, pass->pass);                                                         \
        KAN_UMO_EVENT_INSERT (event, kan_render_graph_pass_deleted_event_t) { event->name = PASS_NAME; }               \
        KAN_UM_ACCESS_DELETE (pass);                                                                                   \
    }

#define HELPER_DELETE_PASS_VARIANT_LOADING_STATES(PASS_NAME)                                                           \
    KAN_UML_VALUE_DELETE (variant_loading, render_foundation_pass_variant_loading_state_t, pass_name, &PASS_NAME)      \
    {                                                                                                                  \
        if (KAN_TYPED_ID_32_IS_VALID (variant_loading->request_id))                                                    \
        {                                                                                                              \
            KAN_UMO_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)                                    \
            {                                                                                                          \
                event->request_id = variant_loading->request_id;                                                       \
            }                                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        KAN_UM_ACCESS_DELETE (variant_loading);                                                                        \
    }

UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_EXECUTE (render_foundation_pass_management_planning)
{
    KAN_UMI_SINGLETON_READ (resource_provider, kan_resource_provider_singleton_t)
    KAN_UMI_SINGLETON_WRITE (render_graph, kan_render_graph_resource_management_singleton_t)

    KAN_UML_EVENT_FETCH (insert_event, kan_resource_native_entry_on_insert_event_t)
    {
        if (insert_event->type == KAN_STATIC_INTERNED_ID_GET (kan_resource_render_pass_t) ||
            insert_event->type == KAN_STATIC_INTERNED_ID_GET (kan_resource_render_pass_compiled_t))
        {
            KAN_UMO_INDEXED_INSERT (loading, render_foundation_pass_loading_state_t)
            {
                loading->name = insert_event->name;
                loading->inspection_time_ns = 0u;

                KAN_UMO_INDEXED_INSERT (request, kan_resource_request_t)
                {
                    request->request_id = kan_next_resource_request_id (resource_provider);
                    loading->request_id = request->request_id;

                    request->name = insert_event->name;
                    request->type = KAN_STATIC_INTERNED_ID_GET (kan_resource_render_pass_compiled_t);

                    // Passes are very important and their count is small, therefore we load them with max priority.
                    request->priority = KAN_INT_MAX (kan_instance_size_t);
                }
            }
        }
    }

    KAN_UML_EVENT_FETCH (delete_event, kan_resource_native_entry_on_delete_event_t)
    {
        if (delete_event->type == KAN_STATIC_INTERNED_ID_GET (kan_resource_render_pass_t) ||
            delete_event->type == KAN_STATIC_INTERNED_ID_GET (kan_resource_render_pass_compiled_t))
        {
            KAN_UMI_VALUE_DELETE_OPTIONAL (loading, render_foundation_pass_loading_state_t, name, &delete_event->name)
            if (loading)
            {
                HELPER_DELETE_LOADED_PASS (loading->name)
                if (KAN_TYPED_ID_32_IS_VALID (loading->request_id))
                {
                    KAN_UMO_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
                    {
                        event->request_id = loading->request_id;
                    }
                }

                HELPER_DELETE_PASS_VARIANT_LOADING_STATES (loading->name)
                KAN_UM_ACCESS_DELETE (loading);
            }
        }
    }

    KAN_UML_SIGNAL_UPDATE (loading, render_foundation_pass_loading_state_t, request_id, KAN_TYPED_ID_32_INVALID_LITERAL)
    {
        KAN_UMO_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (resource_provider);
            loading->request_id = request->request_id;

            request->name = loading->name;
            request->type = KAN_STATIC_INTERNED_ID_GET (kan_resource_render_pass_compiled_t);

            // Passes are very important and their count is small, therefore we load them with max priority.
            request->priority = KAN_INT_MAX (kan_instance_size_t);
        }
    }

    KAN_UML_SIGNAL_UPDATE (variant_loading, render_foundation_pass_variant_loading_state_t, request_id,
                           KAN_TYPED_ID_32_INVALID_LITERAL)
    {
        KAN_UMO_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (resource_provider);
            variant_loading->request_id = request->request_id;

            request->name = variant_loading->resource_name;
            request->type = KAN_STATIC_INTERNED_ID_GET (kan_resource_render_pass_variant_compiled_t);

            // Passes are very important and their count is small, therefore we load them with max priority.
            request->priority = KAN_INT_MAX (kan_instance_size_t);
        }
    }
}

struct render_foundation_pass_management_execution_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (render_foundation_pass_management_execution)
    KAN_UM_BIND_STATE (render_foundation_pass_management_execution, state)

    kan_allocation_group_t temporary_allocation_group;
};

UNIVERSE_RENDER_FOUNDATION_API void render_foundation_pass_management_execution_state_init (
    struct render_foundation_pass_management_execution_state_t *instance)
{
    instance->temporary_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_stack_get (), "temporary");
}

UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_DEPLOY (render_foundation_pass_management_execution)
{
    kan_static_interned_ids_ensure_initialized ();
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_FRAME_END);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RENDER_FOUNDATION_PASS_MANAGEMENT_END_CHECKPOINT);
}

static void configure_render_pass (struct render_foundation_pass_management_execution_state_t *state,
                                   const struct kan_render_context_singleton_t *render_context,
                                   struct kan_render_graph_pass_t *pass,
                                   const struct render_foundation_pass_loading_state_t *loading)
{
    if (KAN_HANDLE_IS_VALID (pass->pass))
    {
        kan_render_pass_destroy (pass->pass);
        pass->pass = KAN_HANDLE_SET_INVALID (kan_render_pass_t);
    }

    for (kan_loop_size_t variant_index = 0u; variant_index < pass->variants.size; ++variant_index)
    {
        kan_render_graph_pass_variant_shutdown (
            &((struct kan_render_graph_pass_variant_t *) pass->variants.data)[variant_index]);
    }

    pass->variants.size = 0u;
    KAN_UMI_VALUE_READ_REQUIRED (pass_request, kan_resource_request_t, request_id, &loading->request_id)

    if (KAN_TYPED_ID_32_IS_VALID (pass_request->provided_container_id))
    {
        KAN_UMI_VALUE_READ_REQUIRED (container,
                                     KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_render_pass_compiled_t),
                                     container_id, &pass_request->provided_container_id)

        const struct kan_resource_render_pass_compiled_t *pass_resource =
            KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_render_pass_compiled_t, container);

        // If not supported, should've been cancelled earlier on loading routine.
        KAN_ASSERT (pass_resource->supported)

        struct kan_render_pass_description_t description = {
            .type = pass_resource->type,
            .attachments_count = pass_resource->attachments.size,
            .attachments = (struct kan_render_pass_attachment_t *) pass_resource->attachments.data,
            .tracking_name = loading->name,
        };

        pass->pass = kan_render_pass_create (render_context->render_context, &description);
        if (!KAN_HANDLE_IS_VALID (pass->pass))
        {
            KAN_LOG (render_foundation_graph, KAN_LOG_ERROR, "Failed to create render pass from resources \"%s\".",
                     pass_request->name)
            return;
        }

        pass->attachments.size = 0u;
        kan_dynamic_array_set_capacity (&pass->attachments, pass_resource->attachments.size);

        for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) pass_resource->attachments.size; ++index)
        {
            struct kan_render_graph_pass_attachment_t *output = kan_dynamic_array_add_last (&pass->attachments);
            KAN_ASSERT (output)

            struct kan_render_pass_attachment_t *input =
                &((struct kan_render_pass_attachment_t *) pass_resource->attachments.data)[index];

            output->type = input->type;
            output->format = input->format;
        }

        kan_dynamic_array_set_capacity (&pass->variants, 1u);
        pass->variants.size = 1u;

        for (kan_loop_size_t variant_index = 0u; variant_index < pass->variants.size; ++variant_index)
        {
            kan_render_graph_pass_variant_init (
                &((struct kan_render_graph_pass_variant_t *) pass->variants.data)[variant_index]);
        }

        KAN_UML_VALUE_READ (variant_loading, render_foundation_pass_variant_loading_state_t, pass_name, &loading->name)
        {
            KAN_ASSERT (variant_loading->variant_index < pass->variants.size)
            KAN_UMI_VALUE_READ_REQUIRED (variant_request, kan_resource_request_t, request_id,
                                         &variant_loading->request_id)

            if (KAN_TYPED_ID_32_IS_VALID (variant_request->provided_container_id))
            {
                KAN_UMI_VALUE_READ_REQUIRED (
                    variant_container,
                    KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_render_pass_variant_compiled_t),
                    container_id, &variant_request->provided_container_id)

                const struct kan_resource_render_pass_variant_compiled_t *variant_resource =
                    KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_render_pass_variant_compiled_t,
                                                         variant_container);

                if (variant_resource->pass_set_bindings.buffers.size > 0u ||
                    variant_resource->pass_set_bindings.samplers.size > 0u)
                {
                    kan_render_pipeline_parameter_set_layout_t layout =
                        kan_render_construct_parameter_set_layout_from_meta (
                            render_context->render_context, &variant_resource->pass_set_bindings, loading->name,
                            state->temporary_allocation_group);

                    if (!KAN_HANDLE_IS_VALID (layout))
                    {
                        KAN_LOG (render_foundation_graph, KAN_LOG_ERROR,
                                 "Failed to create render pass \"%s\" set layout for variant %lu.", loading->name,
                                 (unsigned long) variant_loading->variant_index)
                        break;
                    }

                    struct kan_render_graph_pass_variant_t *variant = &(
                        (struct kan_render_graph_pass_variant_t *) pass->variants.data)[variant_loading->variant_index];

                    variant->pass_parameter_set_layout = layout;
                    kan_rpl_meta_set_bindings_shutdown (&variant->pass_parameter_set_bindings);
                    kan_rpl_meta_set_bindings_init_copy (&variant->pass_parameter_set_bindings,
                                                         &variant_resource->pass_set_bindings);
                }
            }
        }
    }
}

static void inspect_render_pass_loading (struct render_foundation_pass_management_execution_state_t *state,
                                         const struct kan_render_context_singleton_t *render_context,
                                         struct kan_render_graph_resource_management_singleton_t *render_graph,
                                         struct render_foundation_pass_loading_state_t *loading,
                                         kan_time_size_t inspection_time_ns)
{
    if (loading->inspection_time_ns == inspection_time_ns)
    {
        // Already checked this frame.
        return;
    }

    loading->inspection_time_ns = inspection_time_ns;
    KAN_UMI_VALUE_READ_OPTIONAL (pass_request, kan_resource_request_t, request_id, &loading->request_id)

    if (pass_request)
    {
        if (pass_request->sleeping)
        {
            // If we've got into inspection and pass request is sleeping, then some pass resources were updated,
            // but pass resource was not. We need to recreate request in order to force retrieval of the last
            // proper pass resource.
            KAN_UMO_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
            {
                event->request_id = loading->request_id;
            }

            loading->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
            return;
        }
        else if (pass_request->expecting_new_data || !KAN_TYPED_ID_32_IS_VALID (pass_request->provided_container_id))
        {
            return;
        }
    }
    else
    {
        loading->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
        return;
    }

    KAN_UML_VALUE_UPDATE (variant_loading, render_foundation_pass_variant_loading_state_t, pass_name, &loading->name)
    {
        KAN_UMI_VALUE_READ_OPTIONAL (variant_request, kan_resource_request_t, request_id, &variant_loading->request_id)
        if (variant_request)
        {
            if (variant_request->sleeping)
            {
                // If we've got into inspection and variant request is sleeping, then some pass resources were updated,
                // but this variant resource was not. We need to recreate request in order to force retrieval of the
                // last proper variant resource.

                KAN_UMO_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
                {
                    event->request_id = variant_loading->request_id;
                }

                variant_loading->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
                return;
            }
            else if (variant_request->expecting_new_data ||
                     !KAN_TYPED_ID_32_IS_VALID (variant_request->provided_container_id))
            {
                return;
            }
        }
        else
        {
            variant_loading->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
            return;
        }
    }

    KAN_UMO_EVENT_INSERT (update_event, kan_render_graph_pass_updated_event_t) { update_event->name = loading->name; }

    KAN_UMI_VALUE_UPDATE_OPTIONAL (pass, kan_render_graph_pass_t, name, &loading->name)
    if (pass)
    {
        destroy_pass_frame_buffers (render_graph, pass->pass);
        configure_render_pass (state, render_context, pass, loading);
    }
    else
    {
        KAN_UMO_INDEXED_INSERT (new_pass, kan_render_graph_pass_t)
        {
            new_pass->name = loading->name;
            configure_render_pass (state, render_context, new_pass, loading);
        }
    }

    KAN_UMO_EVENT_INSERT (pass_sleep_event, kan_resource_request_defer_sleep_event_t)
    {
        pass_sleep_event->request_id = loading->request_id;
    }

    KAN_UML_VALUE_READ (variant_loading_to_unload_request, render_foundation_pass_variant_loading_state_t, pass_name,
                        &loading->name)
    {
        KAN_UMO_EVENT_INSERT (variant_sleep_event, kan_resource_request_defer_sleep_event_t)
        {
            variant_sleep_event->request_id = variant_loading_to_unload_request->request_id;
        }
    }
}

static void on_render_pass_loaded (struct render_foundation_pass_management_execution_state_t *state,
                                   const struct kan_render_context_singleton_t *render_context,
                                   struct kan_render_graph_resource_management_singleton_t *render_graph,
                                   kan_resource_request_id_t request_id,
                                   kan_time_size_t inspection_time_ns)
{
    KAN_UMI_VALUE_UPDATE_OPTIONAL (loading, render_foundation_pass_loading_state_t, request_id, &request_id)
    if (!loading)
    {
        return;
    }

    HELPER_DELETE_PASS_VARIANT_LOADING_STATES (loading->name)

    // Start by creating new loading states for variants.
    KAN_UMI_VALUE_READ_REQUIRED (request, kan_resource_request_t, request_id, &request_id)

    if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
    {
        KAN_UMI_VALUE_READ_REQUIRED (container,
                                     KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_render_pass_compiled_t),
                                     container_id, &request->provided_container_id)

        const struct kan_resource_render_pass_compiled_t *pass_resource =
            KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_render_pass_compiled_t, container);

        if (!pass_resource->supported)
        {
            // Pass is not supported, no need to do further loading.
            HELPER_DELETE_LOADED_PASS (loading->name)
            return;
        }

        for (kan_loop_size_t variant_index = 0u; variant_index < pass_resource->variants.size; ++variant_index)
        {
            KAN_UMO_INDEXED_INSERT (new_variant_loading, render_foundation_pass_variant_loading_state_t)
            {
                new_variant_loading->pass_name = loading->name;
                new_variant_loading->resource_name =
                    ((kan_interned_string_t *) pass_resource->variants.data)[variant_index];
                new_variant_loading->variant_index = (kan_instance_size_t) variant_index;
                new_variant_loading->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
            }
        }
    }

    inspect_render_pass_loading (state, render_context, render_graph, loading, inspection_time_ns);
}

static void on_render_pass_variant_loaded (struct render_foundation_pass_management_execution_state_t *state,
                                           const struct kan_render_context_singleton_t *render_context,
                                           struct kan_render_graph_resource_management_singleton_t *render_graph,
                                           kan_resource_request_id_t request_id,
                                           kan_time_size_t inspection_time_ns)
{
    kan_interned_string_t pass_name;
    {
        KAN_UMI_VALUE_READ_OPTIONAL (variant_loading, render_foundation_pass_variant_loading_state_t, request_id,
                                     &request_id)

        if (!variant_loading)
        {
            return;
        }

        pass_name = variant_loading->pass_name;
    }

    KAN_UMI_VALUE_UPDATE_REQUIRED (loading, render_foundation_pass_loading_state_t, name, &pass_name)
    inspect_render_pass_loading (state, render_context, render_graph, loading, inspection_time_ns);
}

UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_EXECUTE (render_foundation_pass_management_execution)
{
    KAN_UMI_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    KAN_UMI_SINGLETON_WRITE (render_graph, kan_render_graph_resource_management_singleton_t)

    if (!KAN_HANDLE_IS_VALID (render_context->render_context))
    {
        return;
    }

    const kan_time_size_t inspection_time_ns = kan_precise_time_get_elapsed_nanoseconds ();
    KAN_UML_EVENT_FETCH (updated_event, kan_resource_request_updated_event_t)
    {
        if (updated_event->type == KAN_STATIC_INTERNED_ID_GET (kan_resource_render_pass_compiled_t))
        {
            on_render_pass_loaded (state, render_context, render_graph, updated_event->request_id, inspection_time_ns);
        }
        else if (updated_event->type == KAN_STATIC_INTERNED_ID_GET (kan_resource_render_pass_variant_compiled_t))
        {
            on_render_pass_variant_loaded (state, render_context, render_graph, updated_event->request_id,
                                           inspection_time_ns);
        }
    }
}

struct render_foundation_frame_execution_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (render_foundation_frame_execution)
    KAN_UM_BIND_STATE (render_foundation_frame_execution, state)

    kan_context_system_t render_backend_system;
};

UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_DEPLOY (render_foundation_frame_execution)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_FRAME_BEGIN);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RENDER_FOUNDATION_FRAME_END);

    state->render_backend_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME);
}

UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_EXECUTE (render_foundation_frame_execution)
{
    if (!KAN_HANDLE_IS_VALID (state->render_backend_system))
    {
        return;
    }

    {
        KAN_UMI_SINGLETON_WRITE (render_context, kan_render_context_singleton_t)
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

    {
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
}

void kan_render_graph_pass_variant_init (struct kan_render_graph_pass_variant_t *instance)
{
    instance->pass_parameter_set_layout = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);
    kan_rpl_meta_set_bindings_init (&instance->pass_parameter_set_bindings);
}

void kan_render_graph_pass_variant_shutdown (struct kan_render_graph_pass_variant_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->pass_parameter_set_layout))
    {
        kan_render_pipeline_parameter_set_layout_destroy (instance->pass_parameter_set_layout);
    }

    kan_rpl_meta_set_bindings_shutdown (&instance->pass_parameter_set_bindings);
}

void kan_render_graph_pass_init (struct kan_render_graph_pass_t *instance)
{
    instance->name = NULL;
    instance->type = KAN_RENDER_PASS_GRAPHICS;
    instance->pass = KAN_HANDLE_SET_INVALID (kan_render_pass_t);

    kan_dynamic_array_init (&instance->attachments, 0u, sizeof (struct kan_render_graph_pass_attachment_t),
                            alignof (struct kan_render_graph_pass_attachment_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->variants, 0u, sizeof (struct kan_render_graph_pass_variant_t),
                            alignof (struct kan_render_graph_pass_variant_t), kan_allocation_group_stack_get ());
}

void kan_render_graph_pass_shutdown (struct kan_render_graph_pass_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->pass))
    {
        kan_render_pass_destroy (instance->pass);
    }

    kan_dynamic_array_shutdown (&instance->attachments);
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->variants, kan_render_graph_pass_variant)
}

kan_render_pipeline_parameter_set_layout_t kan_render_construct_parameter_set_layout_from_meta (
    kan_render_context_t render_context,
    const struct kan_rpl_meta_set_bindings_t *meta,
    kan_interned_string_t tracking_name,
    kan_allocation_group_t temporary_allocation_group)
{
    struct kan_render_parameter_binding_description_t
        bindings_static[KAN_UNIVERSE_RENDER_FOUNDATION_BINDINGS_MAX_STATIC];
    struct kan_render_parameter_binding_description_t *bindings = bindings_static;
    const kan_instance_size_t bindings_count = meta->buffers.size + meta->samplers.size + meta->images.size;

    if (bindings_count > KAN_UNIVERSE_RENDER_FOUNDATION_BINDINGS_MAX_STATIC)
    {
        bindings = kan_allocate_general (temporary_allocation_group,
                                         sizeof (struct kan_render_parameter_binding_description_t) * bindings_count,
                                         alignof (struct kan_render_parameter_binding_description_t));
    }

    kan_instance_size_t binding_output_index = 0u;
    for (kan_loop_size_t index = 0u; index < meta->buffers.size; ++index, ++binding_output_index)
    {
        struct kan_rpl_meta_buffer_t *buffer = &((struct kan_rpl_meta_buffer_t *) meta->buffers.data)[index];
        enum kan_render_parameter_binding_type_t binding_type = KAN_RENDER_PARAMETER_BINDING_TYPE_UNIFORM_BUFFER;

        switch (buffer->type)
        {
        case KAN_RPL_BUFFER_TYPE_UNIFORM:
            binding_type = KAN_RENDER_PARAMETER_BINDING_TYPE_UNIFORM_BUFFER;
            break;

        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
            binding_type = KAN_RENDER_PARAMETER_BINDING_TYPE_STORAGE_BUFFER;
            break;

        case KAN_RPL_BUFFER_TYPE_PUSH_CONSTANT:
            // Should not be here.
            KAN_ASSERT (false)
            break;
        }

        bindings[binding_output_index] = (struct kan_render_parameter_binding_description_t) {
            .binding = buffer->binding,
            .type = binding_type,
            .descriptor_count = 1u,
            .used_stage_mask = (1u << KAN_RENDER_STAGE_GRAPHICS_VERTEX) | (1u << KAN_RENDER_STAGE_GRAPHICS_FRAGMENT),
        };
    }

    for (kan_loop_size_t index = 0u; index < meta->samplers.size; ++index, ++binding_output_index)
    {
        struct kan_rpl_meta_sampler_t *sampler = &((struct kan_rpl_meta_sampler_t *) meta->samplers.data)[index];

        bindings[binding_output_index] = (struct kan_render_parameter_binding_description_t) {
            .binding = sampler->binding,
            .type = KAN_RENDER_PARAMETER_BINDING_TYPE_SAMPLER,
            .descriptor_count = 1u,
            .used_stage_mask = (1u << KAN_RENDER_STAGE_GRAPHICS_VERTEX) | (1u << KAN_RENDER_STAGE_GRAPHICS_FRAGMENT),
        };
    }

    for (kan_loop_size_t index = 0u; index < meta->images.size; ++index, ++binding_output_index)
    {
        struct kan_rpl_meta_image_t *image = &((struct kan_rpl_meta_image_t *) meta->images.data)[index];

        bindings[binding_output_index] = (struct kan_render_parameter_binding_description_t) {
            .binding = image->binding,
            .type = KAN_RENDER_PARAMETER_BINDING_TYPE_IMAGE,
            .descriptor_count = image->image_array_size,
            .used_stage_mask = (1u << KAN_RENDER_STAGE_GRAPHICS_VERTEX) | (1u << KAN_RENDER_STAGE_GRAPHICS_FRAGMENT),
        };
    }

    struct kan_render_pipeline_parameter_set_layout_description_t description = {
        .bindings_count = bindings_count,
        .bindings = bindings,
        .tracking_name = tracking_name,
    };

    kan_render_pipeline_parameter_set_layout_t layout =
        kan_render_pipeline_parameter_set_layout_create (render_context, &description);

    if (bindings != bindings_static)
    {
        kan_free_general (temporary_allocation_group, bindings,
                          sizeof (struct kan_render_parameter_binding_description_t) * bindings_count);
    }

    return layout;
}

void kan_render_context_singleton_init (struct kan_render_context_singleton_t *instance)
{
    instance->render_context = KAN_HANDLE_SET_INVALID (kan_render_context_t);
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
