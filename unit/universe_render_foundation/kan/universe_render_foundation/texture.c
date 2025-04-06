#include <kan/context/all_system_names.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/log/logging.h>
#include <kan/precise_time/precise_time.h>
#include <kan/resource_texture/resource_texture.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe_render_foundation/render_graph.h>
#include <kan/universe_render_foundation/texture.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

KAN_LOG_DEFINE_CATEGORY (render_foundation_texture);

KAN_REFLECTION_FUNCTION_META (kan_universe_mutator_execute_render_foundation_texture_management_planning)
KAN_REFLECTION_FUNCTION_META (kan_universe_mutator_execute_render_foundation_texture_management_execution)
UNIVERSE_RENDER_FOUNDATION_API struct kan_universe_mutator_group_meta_t
    render_foundation_texture_management_group_meta = {
        .group_name = KAN_RENDER_FOUNDATION_TEXTURE_MANAGEMENT_MUTATOR_GROUP,
};

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

struct render_foundation_texture_usage_on_change_event_t
{
    kan_interned_string_t old_texture_name;
    kan_interned_string_t new_texture_name;
};

KAN_REFLECTION_STRUCT_META (kan_render_texture_usage_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_on_change_event_t
    render_foundation_texture_usage_on_change_event = {
        .event_type = "render_foundation_texture_usage_on_change_event_t",
        .observed_fields_count = 3u,
        .observed_fields =
            (struct kan_repository_field_path_t[]) {
                {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"best_advised_mip"}},
                {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"worst_advised_mip"}},
            },
        .unchanged_copy_outs_count = 1u,
        .unchanged_copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                    .target_path = {.reflection_path_length = 1u,
                                    .reflection_path = (const char *[]) {"old_texture_name"}},
                },
            },
        .changed_copy_outs_count = 1u,
        .changed_copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                    .target_path = {.reflection_path_length = 1u,
                                    .reflection_path = (const char *[]) {"new_texture_name"}},
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

struct render_foundation_texture_raw_data_usage_t
{
    kan_interned_string_t texture_name;
    kan_interned_string_t raw_data_name;
    kan_resource_request_id_t request_id;
};

struct render_foundation_texture_compiled_data_usage_t
{
    kan_interned_string_t texture_name;
    kan_interned_string_t compiled_data_name;
    uint8_t mip;
    kan_resource_request_id_t request_id;
};

enum render_foundation_texture_usage_flags_t
{
    RENDER_FOUNDATION_TEXTURE_USAGE_FLAGS_LOADING_COMPILED = 1u << 0u,
    RENDER_FOUNDATION_TEXTURE_USAGE_FLAGS_HAS_REQUESTED_MIPS = 1u << 1u,
    RENDER_FOUNDATION_TEXTURE_USAGE_FLAGS_HAS_LOADED_MIPS = 1u << 2u,
};

struct render_foundation_texture_usage_state_t
{
    kan_interned_string_t name;
    kan_instance_size_t reference_count;

    uint8_t flags;
    uint8_t usage_best_mip;
    uint8_t usage_worst_mip;
    uint8_t requested_best_mip;
    uint8_t requested_worst_mip;
    uint8_t loaded_best_mip;
    uint8_t loaded_worst_mip;
    kan_instance_size_t selected_compiled_format_index;

    kan_resource_request_id_t texture_request_id;
    kan_time_size_t last_usage_inspection_time_ns;
    kan_time_size_t last_loading_inspection_time_ns;
};

struct render_foundation_texture_management_planning_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (render_foundation_texture_management_planning)
    KAN_UP_BIND_STATE (render_foundation_texture_management_planning, state)

    kan_interned_string_t interned_kan_resource_texture_raw_data_t;
    kan_interned_string_t interned_kan_resource_texture_t;
    kan_interned_string_t interned_kan_resource_texture_compiled_data_t;
    kan_interned_string_t interned_kan_resource_texture_compiled_t;
};

UNIVERSE_RENDER_FOUNDATION_API void render_foundation_texture_management_planning_state_init (
    struct render_foundation_texture_management_planning_state_t *instance)
{
    instance->interned_kan_resource_texture_raw_data_t = kan_string_intern ("kan_resource_texture_raw_data_t");
    instance->interned_kan_resource_texture_t = kan_string_intern ("kan_resource_texture_t");
    instance->interned_kan_resource_texture_compiled_data_t =
        kan_string_intern ("kan_resource_texture_compiled_data_t");
    instance->interned_kan_resource_texture_compiled_t = kan_string_intern ("kan_resource_texture_compiled_t");
}

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_deploy_render_foundation_texture_management_planning (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct render_foundation_texture_management_planning_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_TEXTURE_MANAGEMENT_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RESOURCE_PROVIDER_BEGIN_CHECKPOINT);
}

static void create_new_usage_state_if_needed (struct render_foundation_texture_management_planning_state_t *state,
                                              const struct kan_resource_provider_singleton_t *resource_provider,
                                              kan_interned_string_t texture_name)
{
    KAN_UP_VALUE_UPDATE (usage_state, render_foundation_texture_usage_state_t, name, &texture_name)
    {
        ++usage_state->reference_count;
        KAN_UP_QUERY_RETURN_VOID;
    }

    KAN_UP_INDEXED_INSERT (new_usage_state, render_foundation_texture_usage_state_t)
    {
        new_usage_state->name = texture_name;
        new_usage_state->reference_count = 1u;

        new_usage_state->flags = 0u;
        new_usage_state->usage_best_mip = 0u;
        new_usage_state->usage_worst_mip = KAN_INT_MAX (uint8_t);
        new_usage_state->requested_best_mip = 0u;
        new_usage_state->requested_worst_mip = KAN_INT_MAX (uint8_t);
        new_usage_state->loaded_best_mip = 0u;
        new_usage_state->loaded_worst_mip = KAN_INT_MAX (uint8_t);
        new_usage_state->selected_compiled_format_index = KAN_INT_MAX (kan_instance_size_t);

        new_usage_state->last_usage_inspection_time_ns = KAN_INT_MAX (kan_time_size_t);
        new_usage_state->last_loading_inspection_time_ns = KAN_INT_MAX (kan_time_size_t);

        KAN_UP_VALUE_READ (entry, kan_resource_native_entry_t, name, &texture_name)
        {
            if (entry->type == state->interned_kan_resource_texture_compiled_t)
            {
                new_usage_state->flags |= RENDER_FOUNDATION_TEXTURE_USAGE_FLAGS_LOADING_COMPILED;
                KAN_UP_QUERY_BREAK;
            }
        }

        KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (resource_provider);
            new_usage_state->texture_request_id = request->request_id;

            request->name = texture_name;
            request->type = (new_usage_state->flags & RENDER_FOUNDATION_TEXTURE_USAGE_FLAGS_LOADING_COMPILED) != 0u ?
                                state->interned_kan_resource_texture_compiled_t :
                                state->interned_kan_resource_texture_t;
            request->priority = KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_INFO_PRIORITY;
        }
    }
}

static void destroy_old_usage_state_if_not_referenced (
    struct render_foundation_texture_management_planning_state_t *state, kan_interned_string_t texture_name)
{
    KAN_UP_VALUE_WRITE (usage_state, render_foundation_texture_usage_state_t, name, &texture_name)
    {
        KAN_ASSERT (usage_state->reference_count > 0u)
        --usage_state->reference_count;

        if (usage_state->reference_count > 0u)
        {
            KAN_UP_QUERY_RETURN_VOID;
        }

        if (KAN_TYPED_ID_32_IS_VALID (usage_state->texture_request_id))
        {
            KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
            {
                event->request_id = usage_state->texture_request_id;
            }
        }

        KAN_UP_VALUE_DELETE (raw_data_usage, render_foundation_texture_raw_data_usage_t, texture_name, &texture_name)
        {
            if (KAN_TYPED_ID_32_IS_VALID (raw_data_usage->request_id))
            {
                KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
                {
                    event->request_id = raw_data_usage->request_id;
                }
            }

            KAN_UP_ACCESS_DELETE (raw_data_usage);
        }

        KAN_UP_VALUE_DELETE (compiled_data_usage, render_foundation_texture_compiled_data_usage_t, texture_name,
                             &texture_name)
        {
            if (KAN_TYPED_ID_32_IS_VALID (compiled_data_usage->request_id))
            {
                KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
                {
                    event->request_id = compiled_data_usage->request_id;
                }
            }

            KAN_UP_ACCESS_DELETE (compiled_data_usage);
        }

        KAN_UP_VALUE_DELETE (loaded, kan_render_texture_loaded_t, name, &texture_name)
        {
            KAN_UP_ACCESS_DELETE (loaded);
        }

        KAN_UP_ACCESS_DELETE (usage_state);
    }
}

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_execute_render_foundation_texture_management_planning (
    kan_cpu_job_t job, struct render_foundation_texture_management_planning_state_t *state)
{
    KAN_UP_SINGLETON_READ (resource_provider, kan_resource_provider_singleton_t)
    {
        if (!resource_provider->scan_done)
        {
            KAN_UP_MUTATOR_RETURN;
        }

        // This mutator only processes changes that result in new request insertion or in deferred request deletion.

        KAN_UP_EVENT_FETCH (on_insert_event, render_foundation_texture_usage_on_insert_event_t)
        {
            create_new_usage_state_if_needed (state, resource_provider, on_insert_event->texture_name);
        }

        KAN_UP_EVENT_FETCH (on_change_event, render_foundation_texture_usage_on_change_event_t)
        {
            if (on_change_event->new_texture_name != on_change_event->old_texture_name)
            {
                create_new_usage_state_if_needed (state, resource_provider, on_change_event->new_texture_name);
                destroy_old_usage_state_if_not_referenced (state, on_change_event->old_texture_name);
            }
        }

        KAN_UP_EVENT_FETCH (on_delete_event, render_foundation_texture_usage_on_delete_event_t)
        {
            destroy_old_usage_state_if_not_referenced (state, on_delete_event->texture_name);
        }

        // TODO: In future, data loading priority should depend on mips and we should implement proper streaming.
        //       It is too early to properly implement this right now as streaming is too far away in plans.

        KAN_UP_SIGNAL_UPDATE (raw_data_usage, render_foundation_texture_raw_data_usage_t, request_id,
                              KAN_TYPED_ID_32_INVALID_LITERAL)
        {
            KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
            {
                request->request_id = kan_next_resource_request_id (resource_provider);
                raw_data_usage->request_id = request->request_id;
                request->name = raw_data_usage->raw_data_name;
                request->type = state->interned_kan_resource_texture_raw_data_t;
                request->priority = KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_DATA_PRIORITY;
            }
        }

        KAN_UP_SIGNAL_UPDATE (compiled_data_usage, render_foundation_texture_compiled_data_usage_t, request_id,
                              KAN_TYPED_ID_32_INVALID_LITERAL)
        {
            KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
            {
                request->request_id = kan_next_resource_request_id (resource_provider);
                compiled_data_usage->request_id = request->request_id;
                request->name = compiled_data_usage->compiled_data_name;
                request->type = state->interned_kan_resource_texture_compiled_data_t;
                request->priority = KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_DATA_PRIORITY;
            }
        }
    }

    KAN_UP_MUTATOR_RETURN;
}

struct render_foundation_texture_management_execution_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (render_foundation_texture_management_execution)
    KAN_UP_BIND_STATE (render_foundation_texture_management_execution, state)

    kan_context_system_t render_backend_system;

    kan_interned_string_t interned_kan_resource_texture_raw_data_t;
    kan_interned_string_t interned_kan_resource_texture_t;
    kan_interned_string_t interned_kan_resource_texture_compiled_data_t;
    kan_interned_string_t interned_kan_resource_texture_compiled_t;

    kan_cpu_section_t section_inspect_texture_usages_internal;
    kan_cpu_section_t section_inspect_texture_usages;
    kan_cpu_section_t section_process_loading;
};

UNIVERSE_RENDER_FOUNDATION_API void render_foundation_texture_management_execution_state_init (
    struct render_foundation_texture_management_execution_state_t *instance)
{
    instance->interned_kan_resource_texture_raw_data_t = kan_string_intern ("kan_resource_texture_raw_data_t");
    instance->interned_kan_resource_texture_t = kan_string_intern ("kan_resource_texture_t");
    instance->interned_kan_resource_texture_compiled_data_t =
        kan_string_intern ("kan_resource_texture_compiled_data_t");
    instance->interned_kan_resource_texture_compiled_t = kan_string_intern ("kan_resource_texture_compiled_t");

    instance->section_inspect_texture_usages_internal = kan_cpu_section_get ("inspect_texture_usages_internal");
    instance->section_inspect_texture_usages = kan_cpu_section_get ("inspect_texture_usages");
    instance->section_process_loading = kan_cpu_section_get ("process_loading");
}

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_deploy_render_foundation_texture_management_execution (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct render_foundation_texture_management_execution_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RENDER_FOUNDATION_TEXTURE_MANAGEMENT_END_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RENDER_FOUNDATION_FRAME_BEGIN);

    state->render_backend_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME);
}

static inline enum kan_render_image_format_t compiled_texture_format_to_render_format (
    enum kan_resource_texture_compiled_format_t format)
{
    switch (format)
    {
    case KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_R8:
        return KAN_RENDER_IMAGE_FORMAT_R8_SRGB;

    case KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_RG16:
        return KAN_RENDER_IMAGE_FORMAT_RG16_SRGB;

    case KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_RGB24:
        return KAN_RENDER_IMAGE_FORMAT_RGB24_SRGB;

    case KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_RGBA32:
        return KAN_RENDER_IMAGE_FORMAT_RGBA32_SRGB;

    case KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_D16:
        return KAN_RENDER_IMAGE_FORMAT_D16_UNORM;

    case KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_D32:
        return KAN_RENDER_IMAGE_FORMAT_D32_SFLOAT;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_RENDER_IMAGE_FORMAT_RGBA32_SRGB;
}

static inline kan_render_image_t create_image_for_compiled_texture (
    struct render_foundation_texture_management_execution_state_t *state,
    kan_interned_string_t texture_name,
    const struct kan_resource_texture_compiled_t *texture,
    struct kan_resource_texture_compiled_format_item_t *format_item,
    uint8_t best_mip,
    uint8_t worst_mip)
{
    struct kan_render_image_description_t description = {
        .format = compiled_texture_format_to_render_format (format_item->format),
        .width = KAN_MAX (1u, texture->width >> best_mip),
        .height = KAN_MAX (1u, texture->height >> best_mip),
        .depth = KAN_MAX (1u, texture->depth >> best_mip),
        .layers = 1u,
        .mips = worst_mip - best_mip + 1u,

        .render_target = KAN_FALSE,
        .supports_sampling = KAN_TRUE,
        .tracking_name = texture_name,
    };

    return kan_render_image_create (kan_render_backend_system_get_render_context (state->render_backend_system),
                                    &description);
}

static void inspect_texture_usages_internal (struct render_foundation_texture_management_execution_state_t *state,
                                             struct kan_render_supported_device_info_t *device_info,
                                             struct render_foundation_texture_usage_state_t *usage_state,
                                             const struct kan_resource_texture_compiled_t *loaded_texture)
{
    struct kan_cpu_section_execution_t section_execution;
    kan_cpu_section_execution_init (&section_execution, state->section_inspect_texture_usages_internal);

    KAN_ASSERT (usage_state->flags & RENDER_FOUNDATION_TEXTURE_USAGE_FLAGS_LOADING_COMPILED)
    if (usage_state->selected_compiled_format_index == KAN_INT_MAX (kan_instance_size_t))
    {
        for (kan_loop_size_t index = 0u; index < loaded_texture->compiled_formats.size; ++index)
        {
            struct kan_resource_texture_compiled_format_item_t *item =
                &((struct kan_resource_texture_compiled_format_item_t *) loaded_texture->compiled_formats.data)[index];

            KAN_ASSERT (item->compiled_data_per_mip.size == loaded_texture->mips)
            const enum kan_render_image_format_t render_format =
                compiled_texture_format_to_render_format (item->format);
            const uint8_t required_flags =
                KAN_RENDER_IMAGE_FORMAT_SUPPORT_FLAG_TRANSFER | KAN_RENDER_IMAGE_FORMAT_SUPPORT_FLAG_SAMPLED;

            if ((device_info->image_format_support[render_format] & required_flags) == required_flags)
            {
                usage_state->selected_compiled_format_index = (uint8_t) index;
                break;
            }
        }

        if (usage_state->selected_compiled_format_index == KAN_INT_MAX (kan_instance_size_t))
        {
            // Unable to select compiled format.
            KAN_LOG (render_foundation_texture, KAN_LOG_ERROR,
                     "There is no compiled data in supported format for texture \"%s\".", usage_state->name)
            kan_cpu_section_execution_shutdown (&section_execution);
            KAN_UP_QUERY_RETURN_VOID;
        }
    }

    usage_state->usage_best_mip = KAN_INT_MAX (uint8_t);
    usage_state->usage_worst_mip = 0u;

    KAN_UP_VALUE_READ (usage, kan_render_texture_usage_t, name, &usage_state->name)
    {
        KAN_ASSERT (usage->best_advised_mip <= usage->worst_advised_mip)
        usage_state->usage_best_mip = KAN_MIN (usage_state->usage_best_mip, usage->best_advised_mip);
        usage_state->usage_worst_mip = KAN_MAX (usage_state->usage_worst_mip, usage->worst_advised_mip);
    }

    KAN_ASSERT (usage_state->usage_best_mip <= usage_state->usage_worst_mip);
    usage_state->usage_best_mip = KAN_MIN (usage_state->usage_best_mip, (uint8_t) loaded_texture->mips - 1u);
    usage_state->usage_worst_mip = KAN_MIN (usage_state->usage_worst_mip, (uint8_t) loaded_texture->mips - 1u);

    // TODO: In the future, it would be good to implement texture memory budget and automatically unload
    //       best mips when we don't have enough memory. Skipped for now as we're prototyping texture
    //       loading.

    if ((usage_state->flags & RENDER_FOUNDATION_TEXTURE_USAGE_FLAGS_HAS_REQUESTED_MIPS) &&
        usage_state->usage_best_mip == usage_state->requested_best_mip &&
        usage_state->usage_worst_mip == usage_state->requested_worst_mip)
    {
        // No changes, just exit.
        kan_cpu_section_execution_shutdown (&section_execution);
        KAN_UP_QUERY_RETURN_VOID;
    }

    // Delete compiled data usages that are no longer relevant.
    KAN_UP_VALUE_DELETE (compiled_data_usage, render_foundation_texture_compiled_data_usage_t, texture_name,
                         &usage_state->name)
    {
        if (compiled_data_usage->mip < usage_state->usage_best_mip ||
            compiled_data_usage->mip > usage_state->usage_worst_mip)
        {
            if (KAN_TYPED_ID_32_IS_VALID (compiled_data_usage->request_id))
            {
                KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
                {
                    event->request_id = compiled_data_usage->request_id;
                }
            }

            KAN_UP_ACCESS_DELETE (compiled_data_usage);
        }
    }

    kan_bool_t any_new_request = KAN_FALSE;
    struct kan_resource_texture_compiled_format_item_t *format_item =
        &((struct kan_resource_texture_compiled_format_item_t *)
              loaded_texture->compiled_formats.data)[usage_state->selected_compiled_format_index];

    for (uint8_t mip = usage_state->usage_best_mip; mip <= usage_state->usage_worst_mip; ++mip)
    {
        if ((usage_state->flags & RENDER_FOUNDATION_TEXTURE_USAGE_FLAGS_HAS_REQUESTED_MIPS) == 0u ||
            mip < usage_state->requested_best_mip || mip > usage_state->requested_worst_mip)
        {
            KAN_UP_INDEXED_INSERT (data_usage, render_foundation_texture_compiled_data_usage_t)
            {
                data_usage->texture_name = usage_state->name;
                data_usage->compiled_data_name =
                    ((kan_interned_string_t *) format_item->compiled_data_per_mip.data)[mip];
                data_usage->mip = mip;
                data_usage->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
                any_new_request = KAN_TRUE;
            }
        }
    }

    usage_state->flags |= RENDER_FOUNDATION_TEXTURE_USAGE_FLAGS_HAS_REQUESTED_MIPS;
    usage_state->requested_best_mip = usage_state->usage_best_mip;
    usage_state->requested_worst_mip = usage_state->usage_worst_mip;

    if (!any_new_request && (usage_state->flags & RENDER_FOUNDATION_TEXTURE_USAGE_FLAGS_HAS_LOADED_MIPS) != 0u)
    {
        // No new requests: we're just using less mips than before.
        // We can plainly recreate texture with less mips.

        KAN_UP_VALUE_UPDATE (loaded, kan_render_texture_loaded_t, name, &usage_state->name)
        {
            kan_render_image_t new_image =
                create_image_for_compiled_texture (state, usage_state->name, loaded_texture, format_item,
                                                   usage_state->requested_best_mip, usage_state->requested_worst_mip);

            if (KAN_HANDLE_IS_VALID (new_image))
            {
                for (uint8_t mip = usage_state->requested_best_mip; mip <= usage_state->requested_worst_mip; ++mip)
                {
                    kan_render_image_copy_data (loaded->image, 0u, mip - usage_state->loaded_best_mip, new_image, 0u,
                                                mip - usage_state->requested_best_mip);
                }

                usage_state->flags |= RENDER_FOUNDATION_TEXTURE_USAGE_FLAGS_HAS_LOADED_MIPS;
                usage_state->loaded_best_mip = usage_state->requested_best_mip;
                usage_state->loaded_worst_mip = usage_state->requested_worst_mip;
                kan_render_image_destroy (loaded->image);
                loaded->image = new_image;

                KAN_UP_EVENT_INSERT (event, kan_render_texture_updated_event_t)
                {
                    event->name = usage_state->name;
                }
            }
            else
            {
                KAN_LOG (render_foundation_texture, KAN_LOG_ERROR, "Failed to create new image for texture \"%s\".",
                         usage_state->name)
            }
        }
    }

    kan_cpu_section_execution_shutdown (&section_execution);
}

static inline void inspect_texture_usages (struct render_foundation_texture_management_execution_state_t *state,
                                           struct kan_render_supported_device_info_t *device_info,
                                           kan_interned_string_t texture_name,
                                           kan_time_size_t inspection_time_ns)
{
    struct kan_cpu_section_execution_t section_execution;
    kan_cpu_section_execution_init (&section_execution, state->section_inspect_texture_usages);

    KAN_UP_VALUE_UPDATE (usage_state, render_foundation_texture_usage_state_t, name, &texture_name)
    {
        if (usage_state->last_usage_inspection_time_ns == inspection_time_ns)
        {
            kan_cpu_section_execution_shutdown (&section_execution);
            KAN_UP_QUERY_RETURN_VOID;
        }

        usage_state->last_usage_inspection_time_ns = inspection_time_ns;
        if ((usage_state->flags & RENDER_FOUNDATION_TEXTURE_USAGE_FLAGS_LOADING_COMPILED) == 0u)
        {
            // Mip management is only relevant for compiled textures.
            kan_cpu_section_execution_shutdown (&section_execution);
            KAN_UP_QUERY_RETURN_VOID;
        }

        KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &usage_state->texture_request_id)
        {
            if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
            {
                KAN_UP_VALUE_READ (container,
                                   KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_texture_compiled_t),
                                   container_id, &request->provided_container_id)
                {
                    const struct kan_resource_texture_compiled_t *loaded_texture =
                        KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_texture_compiled_t, container);
                    inspect_texture_usages_internal (state, device_info, usage_state, loaded_texture);
                }
            }
        }
    }

    kan_cpu_section_execution_shutdown (&section_execution);
}

static inline enum kan_render_image_format_t raw_texture_format_to_render_format (
    enum kan_resource_texture_raw_format_t format)
{
    switch (format)
    {
    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_R8:
        return KAN_RENDER_IMAGE_FORMAT_R8_SRGB;

    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RG16:
        return KAN_RENDER_IMAGE_FORMAT_RG16_SRGB;

    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGB24:
        return KAN_RENDER_IMAGE_FORMAT_RGB24_SRGB;

    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_RGBA32:
        return KAN_RENDER_IMAGE_FORMAT_RGBA32_SRGB;

    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH16:
        return KAN_RENDER_IMAGE_FORMAT_D16_UNORM;

    case KAN_RESOURCE_TEXTURE_RAW_FORMAT_DEPTH32:
        return KAN_RENDER_IMAGE_FORMAT_D32_SFLOAT;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_RENDER_IMAGE_FORMAT_RGBA32_SRGB;
}

static void raw_texture_load (struct render_foundation_texture_management_execution_state_t *state,
                              struct kan_render_supported_device_info_t *device_info,
                              struct render_foundation_texture_usage_state_t *usage_state,
                              const struct kan_resource_texture_t *loaded_texture,
                              const struct kan_resource_texture_raw_data_t *raw_data)
{
    struct kan_render_image_description_t description = {
        .format = raw_texture_format_to_render_format (raw_data->format),
        .width = raw_data->width,
        .height = raw_data->height,
        .depth = raw_data->depth,
        .layers = 1u,
        .mips = (uint8_t) loaded_texture->mips,

        .render_target = KAN_FALSE,
        .supports_sampling = KAN_TRUE,
        .tracking_name = usage_state->name,
    };

    const uint8_t required_flags =
        KAN_RENDER_IMAGE_FORMAT_SUPPORT_FLAG_TRANSFER | KAN_RENDER_IMAGE_FORMAT_SUPPORT_FLAG_SAMPLED;

    if ((device_info->image_format_support[description.format] & required_flags) != required_flags)
    {
        KAN_LOG (render_foundation_texture, KAN_LOG_ERROR,
                 "Failed to load raw data for texture \"%s\" as its raw format is not supported by GPU itself.",
                 usage_state->name)
        return;
    }

    kan_render_image_t new_image = kan_render_image_create (
        kan_render_backend_system_get_render_context (state->render_backend_system), &description);

    if (!KAN_HANDLE_IS_VALID (new_image))
    {
        KAN_LOG (render_foundation_texture, KAN_LOG_ERROR, "Failed to create new image for texture \"%s\".",
                 usage_state->name)
        return;
    }

    kan_render_image_upload_data (new_image, 0u, 0u, raw_data->data.size, raw_data->data.data);
    kan_render_image_request_mip_generation (new_image, 0u, 0u, (uint8_t) (loaded_texture->mips - 1u));

    KAN_UP_EVENT_INSERT (event, kan_render_texture_updated_event_t)
    {
        event->name = usage_state->name;
    }

    KAN_UP_VALUE_UPDATE (loaded, kan_render_texture_loaded_t, name, &usage_state->name)
    {
        if (KAN_HANDLE_IS_VALID (loaded->image))
        {
            kan_render_image_destroy (loaded->image);
        }

        loaded->image = new_image;
        KAN_UP_QUERY_RETURN_VOID;
    }

    KAN_UP_INDEXED_INSERT (new_loaded, kan_render_texture_loaded_t)
    {
        new_loaded->name = usage_state->name;
        new_loaded->image = new_image;
    }
}

static void on_raw_texture_data_request_updated (struct render_foundation_texture_management_execution_state_t *state,
                                                 struct kan_render_supported_device_info_t *device_info,
                                                 const struct kan_resource_request_updated_event_t *updated_event)
{
    KAN_UP_VALUE_UPDATE (raw_data_usage, render_foundation_texture_raw_data_usage_t, request_id,
                         &updated_event->request_id)
    {
        KAN_UP_VALUE_READ (raw_data_request, kan_resource_request_t, request_id, &raw_data_usage->request_id)
        {
            if (KAN_TYPED_ID_32_IS_VALID (raw_data_request->provided_container_id))
            {
                KAN_UP_VALUE_READ (raw_data_container,
                                   KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_texture_raw_data_t),
                                   container_id, &raw_data_request->provided_container_id)
                {
                    const struct kan_resource_texture_raw_data_t *raw_data =
                        KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_texture_raw_data_t, raw_data_container);

                    KAN_UP_VALUE_UPDATE (usage_state, render_foundation_texture_usage_state_t, name,
                                         &raw_data_usage->texture_name)
                    {
                        KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id,
                                           &usage_state->texture_request_id)
                        {
                            if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
                            {
                                KAN_UP_VALUE_READ (texture_container,
                                                   KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_texture_t),
                                                   container_id, &request->provided_container_id)
                                {
                                    const struct kan_resource_texture_t *loaded_texture =
                                        KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_texture_t, texture_container);

                                    raw_texture_load (state, device_info, usage_state, loaded_texture, raw_data);
                                    KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_sleep_event_t)
                                    {
                                        // Raw data is loaded and uploaded, put request to sleep now.
                                        event->request_id = updated_event->request_id;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

static void on_raw_texture_request_updated (struct render_foundation_texture_management_execution_state_t *state,
                                            const struct kan_resource_request_updated_event_t *updated_event)
{
    KAN_UP_VALUE_UPDATE (usage_state, render_foundation_texture_usage_state_t, texture_request_id,
                         &updated_event->request_id)
    {
        KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &usage_state->texture_request_id)
        {
            if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
            {
                KAN_UP_VALUE_READ (container, KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_texture_t),
                                   container_id, &request->provided_container_id)
                {
                    const struct kan_resource_texture_t *loaded_texture =
                        KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_texture_t, container);

                    // Delete old raw data usage if it was present.
                    KAN_UP_VALUE_DELETE (raw_data_usage, render_foundation_texture_raw_data_usage_t, texture_name,
                                         &usage_state->name)
                    {
                        if (KAN_TYPED_ID_32_IS_VALID (raw_data_usage->request_id))
                        {
                            KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
                            {
                                event->request_id = raw_data_usage->request_id;
                            }
                        }

                        KAN_UP_ACCESS_DELETE (raw_data_usage);
                    }

                    KAN_UP_INDEXED_INSERT (new_raw_data_usage, render_foundation_texture_raw_data_usage_t)
                    {
                        new_raw_data_usage->texture_name = usage_state->name;
                        new_raw_data_usage->raw_data_name = loaded_texture->raw_data;
                        new_raw_data_usage->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
                    }
                }
            }
        }
    }
}

static void on_compiled_texture_request_updated (struct render_foundation_texture_management_execution_state_t *state,
                                                 struct kan_render_supported_device_info_t *device_info,
                                                 const struct kan_resource_request_updated_event_t *updated_event,
                                                 kan_time_size_t inspection_time_ns)
{
    KAN_UP_VALUE_UPDATE (usage_state, render_foundation_texture_usage_state_t, texture_request_id,
                         &updated_event->request_id)
    {
        KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &usage_state->texture_request_id)
        {
            if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
            {
                KAN_UP_VALUE_READ (container,
                                   KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_texture_compiled_t),
                                   container_id, &request->provided_container_id)
                {
                    const struct kan_resource_texture_compiled_t *loaded_texture =
                        KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_texture_compiled_t, container);

                    // Delete any old data usage if it was present.
                    KAN_UP_VALUE_DELETE (compiled_data_usage, render_foundation_texture_compiled_data_usage_t,
                                         texture_name, &usage_state->name)
                    {
                        if (KAN_TYPED_ID_32_IS_VALID (compiled_data_usage->request_id))
                        {
                            KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
                            {
                                event->request_id = compiled_data_usage->request_id;
                            }
                        }

                        KAN_UP_ACCESS_DELETE (compiled_data_usage);
                    }

                    usage_state->flags &= ~0u ^ (RENDER_FOUNDATION_TEXTURE_USAGE_FLAGS_HAS_REQUESTED_MIPS |
                                                 RENDER_FOUNDATION_TEXTURE_USAGE_FLAGS_HAS_LOADED_MIPS);

                    usage_state->requested_best_mip = 0u;
                    usage_state->requested_worst_mip = KAN_INT_MAX (uint8_t);
                    usage_state->loaded_best_mip = 0u;
                    usage_state->loaded_worst_mip = KAN_INT_MAX (uint8_t);
                    usage_state->selected_compiled_format_index = KAN_INT_MAX (kan_instance_size_t);
                    usage_state->last_usage_inspection_time_ns = inspection_time_ns;
                    inspect_texture_usages_internal (state, device_info, usage_state, loaded_texture);
                }
            }
        }
    }
}

static void compiled_texture_load_mips (struct render_foundation_texture_management_execution_state_t *state,
                                        struct render_foundation_texture_usage_state_t *usage_state,
                                        kan_render_image_t new_image,
                                        kan_render_image_t old_image)
{
    KAN_UP_VALUE_READ (data_usage, render_foundation_texture_compiled_data_usage_t, texture_name, &usage_state->name)
    {
        if (KAN_TYPED_ID_32_IS_VALID (data_usage->request_id))
        {
            KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &data_usage->request_id)
            {
                if (request->sleeping)
                {
                    // If there are sleeping requests, then their data must already be inside loaded image.
                    KAN_ASSERT (KAN_HANDLE_IS_VALID (old_image))
                    KAN_ASSERT (data_usage->mip >= usage_state->loaded_best_mip &&
                                data_usage->mip <= usage_state->loaded_worst_mip)

                    kan_render_image_copy_data (old_image, 0u, data_usage->mip - usage_state->loaded_best_mip,
                                                new_image, 0u, data_usage->mip - usage_state->requested_best_mip);
                }
                else
                {
                    // We shouldn't go there if not all mips are loaded.
                    KAN_ASSERT (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))

                    KAN_UP_VALUE_READ (compiled_data_container,
                                       KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_texture_compiled_data_t),
                                       container_id, &request->provided_container_id)
                    {
                        const struct kan_resource_texture_compiled_data_t *compiled_data =
                            KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_texture_compiled_data_t,
                                                                 compiled_data_container);

                        kan_render_image_upload_data (new_image, 0u, data_usage->mip - usage_state->requested_best_mip,
                                                      compiled_data->data.size, compiled_data->data.data);
                    }

                    KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_sleep_event_t)
                    {
                        // Raw data is loaded and uploaded, put request to sleep now.
                        event->request_id = data_usage->request_id;
                    }
                }
            }
        }
    }
}

static void on_compiled_texture_data_request_updated (
    struct render_foundation_texture_management_execution_state_t *state,
    const struct kan_resource_request_updated_event_t *updated_event,
    kan_time_size_t inspection_time_ns)
{
    kan_interned_string_t texture_name = NULL;
    KAN_UP_VALUE_READ (source_data_usage, render_foundation_texture_compiled_data_usage_t, request_id,
                       &updated_event->request_id)
    {
        texture_name = source_data_usage->texture_name;
    }

    KAN_UP_VALUE_UPDATE (usage_state, render_foundation_texture_usage_state_t, name, &texture_name)
    {
        if (usage_state->last_loading_inspection_time_ns == inspection_time_ns)
        {
            // Loading was already processed this frame.
            KAN_UP_QUERY_RETURN_VOID;
        }

        usage_state->last_loading_inspection_time_ns = inspection_time_ns;
        // Currently, we only update loaded image when all requested mips are loaded.
        // We might change the strategy later for the cases when we need texture with any mip as soon as possible.
        kan_bool_t all_mips_loaded = KAN_TRUE;

        KAN_UP_VALUE_READ (data_usage_to_check, render_foundation_texture_compiled_data_usage_t, texture_name,
                           &usage_state->name)
        {
            if (KAN_TYPED_ID_32_IS_VALID (data_usage_to_check->request_id))
            {
                KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &data_usage_to_check->request_id)
                {
                    if (!request->sleeping && !KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
                    {
                        all_mips_loaded = KAN_FALSE;
                        KAN_UP_QUERY_BREAK;
                    }
                }
            }
        }

        if (!all_mips_loaded)
        {
            // Not all mips loaded, wait for them in order to create loaded image.
            KAN_UP_QUERY_RETURN_VOID;
        }

        KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &usage_state->texture_request_id)
        {
            if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
            {
                KAN_UP_VALUE_READ (container,
                                   KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_texture_compiled_t),
                                   container_id, &request->provided_container_id)
                {
                    const struct kan_resource_texture_compiled_t *loaded_texture =
                        KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_texture_compiled_t, container);

                    struct kan_resource_texture_compiled_format_item_t *format_item =
                        &((struct kan_resource_texture_compiled_format_item_t *)
                              loaded_texture->compiled_formats.data)[usage_state->selected_compiled_format_index];

                    kan_render_image_t new_image = create_image_for_compiled_texture (
                        state, usage_state->name, loaded_texture, format_item, usage_state->requested_best_mip,
                        usage_state->requested_worst_mip);

                    if (!KAN_HANDLE_IS_VALID (new_image))
                    {
                        KAN_LOG (render_foundation_texture, KAN_LOG_ERROR,
                                 "Failed to create new image for texture \"%s\".", usage_state->name)
                        KAN_UP_QUERY_RETURN_VOID;
                    }

                    KAN_UP_EVENT_INSERT (event, kan_render_texture_updated_event_t)
                    {
                        event->name = usage_state->name;
                    }

                    kan_bool_t updated = KAN_FALSE;
                    KAN_UP_VALUE_UPDATE (loaded, kan_render_texture_loaded_t, name, &usage_state->name)
                    {
                        compiled_texture_load_mips (state, usage_state, new_image, loaded->image);
                        if (KAN_HANDLE_IS_VALID (loaded->image))
                        {
                            kan_render_image_destroy (loaded->image);
                        }

                        loaded->image = new_image;
                        updated = KAN_TRUE;
                    }

                    if (!updated)
                    {
                        KAN_UP_INDEXED_INSERT (new_loaded, kan_render_texture_loaded_t)
                        {
                            compiled_texture_load_mips (state, usage_state, new_image,
                                                        KAN_HANDLE_SET_INVALID (kan_render_image_t));
                            new_loaded->name = usage_state->name;
                            new_loaded->image = new_image;
                        }
                    }

                    usage_state->flags |= RENDER_FOUNDATION_TEXTURE_USAGE_FLAGS_HAS_LOADED_MIPS;
                    usage_state->loaded_best_mip = usage_state->requested_best_mip;
                    usage_state->loaded_worst_mip = usage_state->requested_worst_mip;
                }
            }
        }
    }
}

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_execute_render_foundation_texture_management_execution (
    kan_cpu_job_t job, struct render_foundation_texture_management_execution_state_t *state)
{
    if (!KAN_HANDLE_IS_VALID (state->render_backend_system))
    {
        KAN_UP_MUTATOR_RETURN;
    }

    struct kan_render_supported_device_info_t *device_info =
        kan_render_backend_system_get_selected_device_info (state->render_backend_system);

    if (!device_info)
    {
        KAN_UP_MUTATOR_RETURN;
    }

    KAN_UP_SINGLETON_READ (resource_provider, kan_resource_provider_singleton_t)
    {
        if (!resource_provider->scan_done)
        {
            KAN_UP_MUTATOR_RETURN;
        }

        kan_time_size_t inspection_time_ns = kan_precise_time_get_elapsed_nanoseconds ();
        KAN_UP_EVENT_FETCH (on_insert_event, render_foundation_texture_usage_on_insert_event_t)
        {
            inspect_texture_usages (state, device_info, on_insert_event->texture_name, inspection_time_ns);
        }

        KAN_UP_EVENT_FETCH (on_change_event, render_foundation_texture_usage_on_change_event_t)
        {
            inspect_texture_usages (state, device_info, on_change_event->old_texture_name, inspection_time_ns);
            inspect_texture_usages (state, device_info, on_change_event->new_texture_name, inspection_time_ns);
        }

        KAN_UP_EVENT_FETCH (on_delete_event, render_foundation_texture_usage_on_delete_event_t)
        {
            inspect_texture_usages (state, device_info, on_delete_event->texture_name, inspection_time_ns);
        }

        struct kan_cpu_section_execution_t section_execution;
        kan_cpu_section_execution_init (&section_execution, state->section_process_loading);

        KAN_UP_EVENT_FETCH (updated_event, kan_resource_request_updated_event_t)
        {
            if (updated_event->type == state->interned_kan_resource_texture_raw_data_t)
            {
                on_raw_texture_data_request_updated (state, device_info, updated_event);
            }
            else if (updated_event->type == state->interned_kan_resource_texture_t)
            {
                on_raw_texture_request_updated (state, updated_event);
            }
            else if (updated_event->type == state->interned_kan_resource_texture_compiled_data_t)
            {
                on_compiled_texture_data_request_updated (state, updated_event, inspection_time_ns);
            }
            else if (updated_event->type == state->interned_kan_resource_texture_compiled_t)
            {
                on_compiled_texture_request_updated (state, device_info, updated_event, inspection_time_ns);
            }
        }

        kan_cpu_section_execution_shutdown (&section_execution);
    }

    KAN_UP_MUTATOR_RETURN;
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

void kan_render_texture_loaded_shutdown (struct kan_render_texture_loaded_t *instance)
{
    kan_render_image_destroy (instance->image);
}
