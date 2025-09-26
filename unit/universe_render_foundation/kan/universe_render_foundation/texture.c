#include <kan/cpu_profiler/markup.h>
#include <kan/log/logging.h>
#include <kan/render_foundation/resource_texture.h>
#include <kan/universe/macro.h>
#include <kan/universe_render_foundation/render_graph.h>
#include <kan/universe_render_foundation/texture.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

KAN_LOG_DEFINE_CATEGORY (render_foundation_texture);
KAN_USE_STATIC_INTERNED_IDS
KAN_USE_STATIC_CPU_SECTIONS

KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP (render_foundation_texture_management)
UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_GROUP_META (render_foundation_texture_management,
                                                          KAN_RENDER_FOUNDATION_TEXTURE_MANAGEMENT_MUTATOR_GROUP);

struct render_foundation_texture_usage_on_insert_event_t
{
    kan_interned_string_t texture_name;
};

KAN_REFLECTION_STRUCT_META (kan_render_texture_usage_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_on_insert_event_t
    render_foundation_texture_usage_on_insert_event = {
        .event_type = "render_foundation_texture_usage_on_insert_event_t",
        .copy_outs_count = 1u,
        .copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                    .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"texture_name"}},
                },
            },
};

struct render_foundation_texture_usage_on_delete_event_t
{
    kan_interned_string_t texture_name;
};

KAN_REFLECTION_STRUCT_META (kan_render_texture_usage_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_on_delete_event_t
    render_foundation_texture_usage_on_delete_event = {
        .event_type = "render_foundation_texture_usage_on_delete_event_t",
        .copy_outs_count = 1u,
        .copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                    .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"texture_name"}},
                },
            },
};

struct render_foundation_texture_data_usage_t
{
    kan_interned_string_t texture_name;
    kan_interned_string_t data_name;
    kan_resource_usage_id_t usage_id;
    uint8_t mip;
};

KAN_REFLECTION_STRUCT_META (render_foundation_texture_data_usage_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_cascade_deletion_t
    render_foundation_texture_data_usage_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
        .child_type_name = "kan_resource_usage_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
};

enum render_foundation_texture_state_t
{
    RENDER_FOUNDATION_TEXTURE_STATE_INITIAL = 0u,
    RENDER_FOUNDATION_TEXTURE_STATE_WAITING_MAIN,
    RENDER_FOUNDATION_TEXTURE_STATE_WAITING_DATA,
    RENDER_FOUNDATION_TEXTURE_STATE_READY,
};

struct render_foundation_texture_t
{
    kan_interned_string_t name;
    kan_instance_size_t reference_count;

    kan_resource_usage_id_t usage_id;
    kan_instance_size_t selected_format_item_index;

    enum render_foundation_texture_state_t state;
    kan_instance_size_t state_frame_id;

    kan_instance_size_t usages_mip_frame_id;
    uint8_t usages_best_mip;
    uint8_t usages_worst_mip;

    uint8_t requested_best_mip;
    uint8_t requested_worst_mip;

    uint8_t loaded_best_mip;
    uint8_t loaded_worst_mip;
};

KAN_REFLECTION_STRUCT_META (render_foundation_texture_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_cascade_deletion_t
    render_foundation_texture_usage_id_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
        .child_type_name = "kan_resource_usage_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
};

KAN_REFLECTION_STRUCT_META (render_foundation_texture_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_cascade_deletion_t
    render_foundation_texture_data_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
        .child_type_name = "render_foundation_texture_data_usage_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"texture_name"}},
};

KAN_REFLECTION_STRUCT_META (render_foundation_texture_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_cascade_deletion_t
    render_foundation_texture_loaded_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
        .child_type_name = "kan_render_texture_loaded_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
};

struct render_foundation_texture_management_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (render_foundation_texture_management)
    KAN_UM_BIND_STATE (render_foundation_texture_management, state)
};

UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_DEPLOY (render_foundation_texture_management)
{
    kan_static_interned_ids_ensure_initialized ();
    kan_cpu_static_sections_ensure_initialized ();

    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_FRAME_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_TEXTURE_MANAGEMENT_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RENDER_FOUNDATION_TEXTURE_MANAGEMENT_END_CHECKPOINT);

    // TODO: Temporary for debugging CI.
    kan_log_category_set_verbosity (kan_log_category_get ("render_foundation_texture"), KAN_LOG_VERBOSE);
}

static void recalculate_usages_mip (struct render_foundation_texture_management_state_t *state,
                                    struct render_foundation_texture_t *texture)
{
    texture->usages_best_mip = KAN_INT_MAX (typeof (texture->usages_best_mip));
    texture->usages_worst_mip = 0u;

    KAN_UML_VALUE_READ (usage, kan_render_texture_usage_t, name, &texture->name)
    {
        texture->usages_best_mip = KAN_MIN (texture->usages_best_mip, usage->best_advised_mip);
        texture->usages_worst_mip = KAN_MAX (texture->usages_worst_mip, usage->worst_advised_mip);
    }
}

static void advance_from_initial_state (struct render_foundation_texture_management_state_t *state,
                                        const struct kan_resource_provider_singleton_t *provider,
                                        struct render_foundation_texture_t *texture);

static void advance_from_waiting_main_state (struct render_foundation_texture_management_state_t *state,
                                             const struct kan_resource_provider_singleton_t *provider,
                                             struct render_foundation_texture_t *texture);

static void advance_from_waiting_data_state (struct render_foundation_texture_management_state_t *state,
                                             const struct kan_resource_provider_singleton_t *provider,
                                             struct render_foundation_texture_t *texture);

static void on_main_resource_updated (struct render_foundation_texture_management_state_t *state,
                                      const struct kan_resource_provider_singleton_t *provider,
                                      kan_interned_string_t texture_name)
{
    KAN_UMI_VALUE_UPDATE_OPTIONAL (texture, render_foundation_texture_t, name, &texture_name)
    if (!texture)
    {
        return;
    }

    // Reset everything connected to current loading state.
    // Nevertheless, we do not need to remove main resource usage as it does not change at all anyway.

    KAN_UML_VALUE_DETACH (data_usage, render_foundation_texture_data_usage_t, texture_name, &texture_name)
    {
        KAN_UM_ACCESS_DELETE (data_usage);
    }

    texture->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);
    texture->state = RENDER_FOUNDATION_TEXTURE_STATE_INITIAL;
    texture->state_frame_id = provider->logic_deduplication_frame_id;
    texture->requested_best_mip = 0u;
    texture->requested_worst_mip = 0u;

    // Start advancing, having some data loaded is very likely here.
    advance_from_initial_state (state, provider, texture);
}

static void add_mip_usage (struct render_foundation_texture_management_state_t *state,
                           const struct kan_resource_provider_singleton_t *provider,
                           kan_interned_string_t texture_name,
                           const struct kan_resource_texture_format_item_t *format_item,
                           kan_loop_size_t mip)
{
    KAN_UMO_INDEXED_INSERT (usage, render_foundation_texture_data_usage_t)
    {
        usage->texture_name = texture_name;
        usage->data_name = ((kan_interned_string_t *) format_item->data_per_mip.data)[mip];
        usage->usage_id = kan_next_resource_usage_id (provider);
        usage->mip = (uint8_t) mip;

        KAN_UMO_INDEXED_INSERT (resource_usage, kan_resource_usage_t)
        {
            resource_usage->usage_id = usage->usage_id;
            resource_usage->type = KAN_STATIC_INTERNED_ID_GET (kan_resource_texture_data_t);
            resource_usage->name = usage->data_name;
            resource_usage->priority = KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_DATA_PRIORITY;
        }
    }
}

static void on_usage_insert (struct render_foundation_texture_management_state_t *state,
                             const struct kan_resource_provider_singleton_t *provider,
                             kan_interned_string_t texture_name)
{
    KAN_UMI_VALUE_UPDATE_OPTIONAL (existent, render_foundation_texture_t, name, &texture_name)
    if (existent)
    {
        ++existent->reference_count;
        if (existent->usages_mip_frame_id == provider->logic_deduplication_frame_id)
        {
            return;
        }

        recalculate_usages_mip (state, existent);
        existent->usages_mip_frame_id = provider->logic_deduplication_frame_id;

        switch (existent->state)
        {
        case RENDER_FOUNDATION_TEXTURE_STATE_INITIAL:
        case RENDER_FOUNDATION_TEXTURE_STATE_WAITING_MAIN:
            break;

        case RENDER_FOUNDATION_TEXTURE_STATE_WAITING_DATA:
        {
            if (existent->usages_best_mip == existent->requested_best_mip &&
                existent->usages_worst_mip == existent->requested_worst_mip)
            {
                // No changes, quick exit.
                break;
            }

            KAN_LOG (render_foundation_texture, KAN_LOG_DEBUG,
                     "Rebuilding mip usages for texture \"%s\" (still in loading).", texture_name)

            // Delete requested values that are no longer used.
            KAN_UML_VALUE_DETACH (data_usage, render_foundation_texture_data_usage_t, texture_name, &texture_name)
            {
                if (data_usage->mip < existent->usages_best_mip || data_usage->mip > existent->usages_worst_mip)
                {
                    KAN_UM_ACCESS_DELETE (data_usage);
                }
            }

            KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (main_resource, kan_resource_texture_t, &texture_name)
            KAN_ASSERT (main_resource) // We shouldn't be in this state if this fails.

            const struct kan_resource_texture_format_item_t *format_item =
                &((struct kan_resource_texture_format_item_t *)
                      main_resource->formats.data)[existent->selected_format_item_index];

            // Add new mips that were not requested.
            for (kan_loop_size_t mip = existent->usages_best_mip;
                 mip <= existent->usages_worst_mip && mip < main_resource->mips; ++mip)
            {
                if (mip >= existent->requested_best_mip && mip <= existent->requested_worst_mip)
                {
                    // Should already exist.
                    continue;
                }

                add_mip_usage (state, provider, texture_name, format_item, mip);
            }

            existent->requested_best_mip = existent->usages_best_mip;
            existent->requested_worst_mip = existent->usages_worst_mip;
            advance_from_waiting_data_state (state, provider, existent);
            break;
        }

        case RENDER_FOUNDATION_TEXTURE_STATE_READY:
        {
            if (existent->usages_best_mip == existent->loaded_best_mip &&
                existent->usages_worst_mip == existent->loaded_worst_mip)
            {
                // No changes, quick exit.
                break;
            }

            KAN_LOG (render_foundation_texture, KAN_LOG_DEBUG,
                     "Rebuilding mip usages for texture \"%s\" (currently loaded).", texture_name)

            KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (main_resource, kan_resource_texture_t, &texture_name)
            KAN_ASSERT (main_resource) // We shouldn't be in this state if this fails.

            const struct kan_resource_texture_format_item_t *format_item =
                &((struct kan_resource_texture_format_item_t *)
                      main_resource->formats.data)[existent->selected_format_item_index];

            // Add new mips that were not requested.
            for (kan_loop_size_t mip = existent->usages_best_mip;
                 mip <= existent->usages_worst_mip && mip < main_resource->mips; ++mip)
            {
                add_mip_usage (state, provider, texture_name, format_item, mip);
            }

            existent->requested_best_mip = existent->usages_best_mip;
            existent->requested_worst_mip = existent->usages_worst_mip;
            advance_from_waiting_data_state (state, provider, existent);
            break;
        }
        }

        return;
    }

    KAN_UMO_INDEXED_INSERT (texture, render_foundation_texture_t)
    {
        texture->name = texture_name;
        texture->reference_count = 1u;

        texture->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);
        texture->selected_format_item_index = 0u;

        texture->state = RENDER_FOUNDATION_TEXTURE_STATE_INITIAL;
        texture->state_frame_id = provider->logic_deduplication_frame_id;

        texture->usages_mip_frame_id = provider->logic_deduplication_frame_id;
        recalculate_usages_mip (state, texture);

        texture->requested_best_mip = 0u;
        texture->requested_worst_mip = 0u;

        texture->loaded_best_mip = 0u;
        texture->loaded_worst_mip = 0u;
        advance_from_initial_state (state, provider, texture);
    }
}

static void on_usage_delete (struct render_foundation_texture_management_state_t *state,
                             kan_interned_string_t texture_name)
{
    KAN_UMI_VALUE_WRITE_OPTIONAL (existent, render_foundation_texture_t, name, &texture_name)
    if (existent)
    {
        --existent->reference_count;
        if (existent->reference_count == 0u)
        {
            // Cascade deletion should handle everything.
            KAN_UM_ACCESS_DELETE (existent);
        }
    }
}

static void advance_from_initial_state (struct render_foundation_texture_management_state_t *state,
                                        const struct kan_resource_provider_singleton_t *provider,
                                        struct render_foundation_texture_t *texture)
{
    KAN_LOG (render_foundation_texture, KAN_LOG_DEBUG,
             "Attempting to advance texture \"%s\" state from initial to waiting main.", texture->name)

    texture->state_frame_id = provider->logic_deduplication_frame_id;
    texture->state = RENDER_FOUNDATION_TEXTURE_STATE_WAITING_MAIN; // We will always advance from initial state.

    // Usage will already be present if we're doing hot reload.
    if (!KAN_TYPED_ID_32_IS_VALID (texture->usage_id))
    {
        texture->usage_id = kan_next_resource_usage_id (provider);
        KAN_UMO_INDEXED_INSERT (usage, kan_resource_usage_t)
        {
            usage->usage_id = usage->usage_id;
            usage->type = KAN_STATIC_INTERNED_ID_GET (kan_resource_texture_t);
            usage->name = texture->name;
            usage->priority = KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_INFO_PRIORITY;
        }
    }

    advance_from_waiting_main_state (state, provider, texture);
}

static inline enum kan_render_image_format_t texture_format_to_render_format (enum kan_resource_texture_format_t format)
{
    switch (format)
    {
    case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_R8_SRGB:
        return KAN_RENDER_IMAGE_FORMAT_R8_SRGB;

    case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RG16_SRGB:
        return KAN_RENDER_IMAGE_FORMAT_RG16_SRGB;

    case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RGBA32_SRGB:
        return KAN_RENDER_IMAGE_FORMAT_RGBA32_SRGB;

    case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_R8_UNORM:
        return KAN_RENDER_IMAGE_FORMAT_R8_UNORM;

    case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RG16_UNORM:
        return KAN_RENDER_IMAGE_FORMAT_RG16_UNORM;

    case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_RGBA32_UNORM:
        return KAN_RENDER_IMAGE_FORMAT_RGBA32_UNORM;

    case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_D16:
        return KAN_RENDER_IMAGE_FORMAT_D16_UNORM;

    case KAN_RESOURCE_TEXTURE_FORMAT_UNCOMPRESSED_D32:
        return KAN_RENDER_IMAGE_FORMAT_D32_SFLOAT;
    }

    KAN_ASSERT (false)
    return KAN_RENDER_IMAGE_FORMAT_RGBA32_SRGB;
}

void advance_from_waiting_main_state (struct render_foundation_texture_management_state_t *state,
                                      const struct kan_resource_provider_singleton_t *provider,
                                      struct render_foundation_texture_t *texture)
{
    KAN_LOG (render_foundation_texture, KAN_LOG_DEBUG,
             "Attempting to advance texture \"%s\" state from waiting main to waiting data.", texture->name)

    texture->state_frame_id = provider->logic_deduplication_frame_id;
    KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (main_resource, kan_resource_texture_t, &texture->name)

    if (!main_resource)
    {
        // Still waiting.
        return;
    }

    KAN_UMI_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    bool format_selected = false;

    for (kan_loop_size_t index = 0u; index < main_resource->formats.size; ++index)
    {
        const struct kan_resource_texture_format_item_t *format_item =
            &((struct kan_resource_texture_format_item_t *) main_resource->formats.data)[index];

        const enum kan_render_image_format_t render_format = texture_format_to_render_format (format_item->format);
        const uint8_t required_flags =
            KAN_RENDER_IMAGE_FORMAT_SUPPORT_FLAG_TRANSFER | KAN_RENDER_IMAGE_FORMAT_SUPPORT_FLAG_SAMPLED;

        if ((render_context->selected_device_info->image_format_support[render_format] & required_flags) ==
            required_flags)
        {
            texture->selected_format_item_index = (kan_instance_size_t) index;
            format_selected = true;
            break;
        }
    }

    if (!format_selected)
    {
        KAN_LOG (render_foundation_texture, KAN_LOG_ERROR,
                 "Failed to load texture \"%s\" as there is no format with proper support flags.")
        return;
    }

    // Create data usages.
    // Technically, creating usage would be unnecessary if all the data is already here somehow, but it is rarely
    // possible and checking for that should not provide a lot of benefits, therefore we simplify code here.

    const struct kan_resource_texture_format_item_t *format_item = &(
        (struct kan_resource_texture_format_item_t *) main_resource->formats.data)[texture->selected_format_item_index];

    // Add new mips that were not requested.
    for (kan_loop_size_t mip = texture->usages_best_mip; mip <= texture->usages_worst_mip && mip < main_resource->mips;
         ++mip)
    {
        add_mip_usage (state, provider, texture->name, format_item, mip);
    }

    texture->requested_best_mip = texture->usages_best_mip;
    texture->requested_worst_mip = texture->usages_worst_mip;
    texture->state = RENDER_FOUNDATION_TEXTURE_STATE_WAITING_DATA;
    advance_from_waiting_data_state (state, provider, texture);
}

static void load_texture_into_image (struct render_foundation_texture_management_state_t *state,
                                     struct render_foundation_texture_t *texture,
                                     struct kan_render_texture_loaded_t *loaded)
{
    KAN_CPU_SCOPED_STATIC_SECTION (load_texture_into_image)
    KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (main_resource, kan_resource_texture_t, &texture->name)
    KAN_ASSERT (main_resource) // We shouldn't be in this state if this fails.

    const struct kan_resource_texture_format_item_t *format_item = &(
        (struct kan_resource_texture_format_item_t *) main_resource->formats.data)[texture->selected_format_item_index];

    const uint8_t best_mip = texture->requested_best_mip;
    const uint8_t worst_mip = KAN_MIN (texture->requested_worst_mip, (uint8_t) (format_item->data_per_mip.size - 1u));

    struct kan_render_image_description_t description = {
        .format = texture_format_to_render_format (format_item->format),
        .width = KAN_MAX (1u, main_resource->width >> best_mip),
        .height = KAN_MAX (1u, main_resource->height >> best_mip),
        .depth = KAN_MAX (1u, main_resource->depth >> best_mip),
        .layers = 1u,
        .mips = worst_mip - best_mip + 1u,

        .render_target = false,
        .supports_sampling = true,
        .tracking_name = texture->name,
    };

    KAN_UMI_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    kan_render_image_t new_image = kan_render_image_create (render_context->render_context, &description);

    if (!KAN_HANDLE_IS_VALID (new_image))
    {
        KAN_LOG (render_foundation_texture, KAN_LOG_ERROR,
                 "Failed to finish loading of texture \"%s\" as GPU image creation has failed.", texture->name)
        return;
    }

    kan_render_image_t old_image = loaded->image;
    loaded->image = new_image;

    // We use bitmask to check which mips are provided from loading and
    // which should be copied from old image if present.
    KAN_ASSERT (main_resource->mips < 32u)
    uint32_t found_data_for_mips = 0u;

    KAN_UML_VALUE_DETACH (data_usage, render_foundation_texture_data_usage_t, texture_name, &texture->name)
    {
        KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (data, kan_resource_texture_data_t, &data_usage->data_name)
        KAN_ASSERT (data) // Should be available if we got into this function.

        kan_render_image_upload_data (new_image, 0u, data_usage->mip, data->data.size, data->data.data);
        found_data_for_mips |= (uint32_t) (1u << data_usage->mip);
        KAN_UM_ACCESS_DELETE (data_usage);
    }

    if (KAN_HANDLE_IS_VALID (old_image))
    {
        for (kan_loop_size_t mip = (kan_loop_size_t) best_mip;
             mip <= texture->requested_worst_mip && mip < main_resource->mips; ++mip)
        {
            if (found_data_for_mips & (1u << mip))
            {
                continue;
            }

            if (mip < texture->loaded_best_mip || mip > texture->loaded_worst_mip)
            {
                KAN_LOG (render_foundation_texture, KAN_LOG_ERROR,
                         "Unable to upload mip %u of texture \"%s\" as it is neither in resource data nor in loaded "
                         "image. Looks like internal error.",
                         (unsigned int) mip, texture->name)
                continue;
            }

            kan_render_image_copy_data (old_image, 0u, (uint8_t) mip - texture->loaded_best_mip, new_image, 0u,
                                        (uint8_t) mip - best_mip);
        }

        kan_render_image_destroy (old_image);
    }

    texture->loaded_best_mip = texture->requested_best_mip;
    texture->loaded_worst_mip = texture->requested_worst_mip;
    KAN_UMO_EVENT_INSERT (updated_event, kan_render_texture_updated_event_t) { updated_event->name = texture->name; }
}

static void advance_from_waiting_data_state (struct render_foundation_texture_management_state_t *state,
                                             const struct kan_resource_provider_singleton_t *provider,
                                             struct render_foundation_texture_t *texture)
{
    KAN_LOG (render_foundation_texture, KAN_LOG_DEBUG,
             "Attempting to advance texture \"%s\" state from waiting data to ready.", texture->name)
    texture->state_frame_id = provider->logic_deduplication_frame_id;

    // For now, we're just probing data like that. Might be not the most efficient path, but should be okay.
    KAN_UML_VALUE_READ (data_to_probe, render_foundation_texture_data_usage_t, texture_name, &texture->name)
    {
        KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (data, kan_resource_texture_data_t, &data_to_probe->data_name)
        if (!data)
        {
            // Not yet loaded, cannot advance this frame.
            return;
        }
    }

    // All data should be loaded, so we're good to go.
    KAN_UMI_VALUE_UPDATE_OPTIONAL (existing_loaded, kan_render_texture_loaded_t, name, &texture->name)

    if (existing_loaded)
    {
        load_texture_into_image (state, texture, existing_loaded);
    }
    else
    {
        KAN_UMO_INDEXED_INSERT (new_loaded, kan_render_texture_loaded_t)
        {
            new_loaded->name = texture->name;
            load_texture_into_image (state, texture, new_loaded);
        }
    }

    texture->requested_best_mip = 0u;
    texture->requested_worst_mip = 0u;
    texture->state = RENDER_FOUNDATION_TEXTURE_STATE_READY;

    KAN_LOG (render_foundation_texture, KAN_LOG_DEBUG, "Advanced texture \"%s\" state to ready.", texture->name)
}

UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_EXECUTE (render_foundation_texture_management)
{
    KAN_UMI_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    if (!KAN_HANDLE_IS_VALID (render_context->render_context))
    {
        return;
    }

    KAN_UMI_SINGLETON_READ (resource_provider, kan_resource_provider_singleton_t)
    if (!resource_provider->scan_done)
    {
        return;
    }

    // We check only main resource updates as data is only updated when main resource is updated.
    KAN_UML_RESOURCE_UPDATED_EVENT_FETCH (updated_event, kan_resource_texture_t)
    {
        on_main_resource_updated (state, resource_provider, updated_event->name);
    }

    KAN_UML_EVENT_FETCH (on_insert_event, render_foundation_texture_usage_on_insert_event_t)
    {
        on_usage_insert (state, resource_provider, on_insert_event->texture_name);
    }

    KAN_UML_EVENT_FETCH (on_delete_event, render_foundation_texture_usage_on_delete_event_t)
    {
        on_usage_delete (state, on_delete_event->texture_name);
    }

    KAN_UML_RESOURCE_LOADED_EVENT_FETCH (main_loaded_event, kan_resource_texture_t)
    {
        KAN_UMI_VALUE_UPDATE_OPTIONAL (texture, render_foundation_texture_t, name, &main_loaded_event->name)
        if (texture && texture->state_frame_id != resource_provider->logic_deduplication_frame_id)
        {
            switch (texture->state)
            {
            case RENDER_FOUNDATION_TEXTURE_STATE_INITIAL:
            case RENDER_FOUNDATION_TEXTURE_STATE_WAITING_DATA:
            case RENDER_FOUNDATION_TEXTURE_STATE_READY:
                KAN_ASSERT_FORMATTED (false,
                                      "Texture \"%s\" in state %u received main resource loaded event, which is "
                                      "totally unexpected in this state.",
                                      texture->name, (unsigned int) texture->state)
                break;

            case RENDER_FOUNDATION_TEXTURE_STATE_WAITING_MAIN:
                advance_from_waiting_main_state (state, resource_provider, texture);
                break;
            }
        }
    }

    KAN_UML_RESOURCE_LOADED_EVENT_FETCH (data_loaded_event, kan_resource_texture_data_t)
    {
        kan_interned_string_t texture_name = NULL;
        {
            KAN_UMI_VALUE_UPDATE_OPTIONAL (data_usage, render_foundation_texture_data_usage_t, data_name,
                                           &data_loaded_event->name)

            if (data_usage)
            {
                texture_name = data_usage->texture_name;
            }
        }

        if (texture_name)
        {
            KAN_UMI_VALUE_UPDATE_OPTIONAL (texture, render_foundation_texture_t, name, &texture_name)
            if (texture && texture->state_frame_id != resource_provider->logic_deduplication_frame_id)
            {
                switch (texture->state)
                {
                case RENDER_FOUNDATION_TEXTURE_STATE_INITIAL:
                case RENDER_FOUNDATION_TEXTURE_STATE_WAITING_MAIN:
                case RENDER_FOUNDATION_TEXTURE_STATE_READY:
                    KAN_ASSERT_FORMATTED (false,
                                          "Texture \"%s\" in state %u received data resource loaded event, which is "
                                          "totally unexpected in this state.",
                                          texture->name, (unsigned int) texture->state)
                    break;

                case RENDER_FOUNDATION_TEXTURE_STATE_WAITING_DATA:
                    advance_from_waiting_data_state (state, resource_provider, texture);
                    break;
                }
            }
        }
    }
}

void kan_render_texture_usage_init (struct kan_render_texture_usage_t *instance)
{
    instance->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_render_texture_usage_id_t);
    instance->name = NULL;
    instance->best_advised_mip = 0u;
    instance->worst_advised_mip = KAN_INT_MAX (uint8_t);
}

void kan_render_texture_singleton_init (struct kan_render_texture_singleton_t *instance)
{
    instance->usage_id_counter = kan_atomic_int_init (1);
}

void kan_render_texture_loaded_init (struct kan_render_texture_loaded_t *instance)
{
    instance->name = NULL;
    instance->image = KAN_HANDLE_SET_INVALID (kan_render_image_t);
}

void kan_render_texture_loaded_shutdown (struct kan_render_texture_loaded_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->image))
    {
        kan_render_image_destroy (instance->image);
    }
}
