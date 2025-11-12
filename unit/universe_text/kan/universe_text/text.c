#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <string.h>

#include <kan/log/logging.h>
#include <kan/resource_text/font.h>
#include <kan/universe/macro.h>
#include <kan/universe_locale/locale.h>
#include <kan/universe_render_foundation/render_graph.h>
#include <kan/universe_resource_provider/provider.h>
#include <kan/universe_text/text.h>

KAN_LOG_DEFINE_CATEGORY (text_management);
KAN_LOG_DEFINE_CATEGORY (text_shaping);
KAN_USE_STATIC_INTERNED_IDS
KAN_USE_STATIC_CPU_SECTIONS

KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP (text_management)
UNIVERSE_TEXT_API KAN_UM_MUTATOR_GROUP_META (text_management, KAN_TEXT_MANAGEMENT_MUTATOR_GROUP);

KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP (text_shaping)
UNIVERSE_TEXT_API KAN_UM_MUTATOR_GROUP_META (text_shaping, KAN_TEXT_SHAPING_MUTATOR_GROUP);

enum font_library_loading_state_t
{
    FONT_LIBRARY_LOADING_STATE_INITIAL = 0u,
    FONT_LIBRARY_LOADING_STATE_WAITING_MAIN,
    FONT_LIBRARY_LOADING_STATE_WAITING_BLOBS,
    FONT_LIBRARY_LOADING_STATE_READY,
};

struct text_management_singleton_t
{
    enum font_library_loading_state_t font_library_loading_state;
    kan_instance_size_t font_library_loading_state_frame_id;
};

UNIVERSE_TEXT_API void text_management_singleton_init (struct text_management_singleton_t *instance)
{
    instance->font_library_loading_state = FONT_LIBRARY_LOADING_STATE_INITIAL;
    instance->font_library_loading_state_frame_id = 0u;
}

struct font_library_t
{
    kan_interned_string_t name;
    kan_font_library_t library;
    kan_interned_string_t usage_class;
    kan_resource_usage_id_t usage_id;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_instance_size_t)
    struct kan_dynamic_array_t selected_categories;
};

KAN_REFLECTION_STRUCT_META (font_library_t)
UNIVERSE_TEXT_API struct kan_repository_meta_automatic_cascade_deletion_t font_library_usage_id_cascade_deletion = {
    .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
    .child_type_name = "kan_resource_usage_t",
    .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
};

UNIVERSE_TEXT_API void font_library_init (struct font_library_t *instance)
{
    instance->name = NULL;
    instance->usage_class = NULL;
    instance->library = KAN_HANDLE_SET_INVALID (kan_font_library_t);
    instance->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);
    kan_dynamic_array_init (&instance->selected_categories, 0u, sizeof (kan_instance_size_t),
                            alignof (kan_instance_size_t), kan_allocation_group_stack_get ());
}

UNIVERSE_TEXT_API void font_library_shutdown (struct font_library_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->library))
    {
        kan_font_library_destroy (instance->library);
    }

    kan_dynamic_array_shutdown (&instance->selected_categories);
}

struct font_blob_t
{
    kan_interned_string_t name;
    kan_resource_third_party_blob_id_t current;
    kan_resource_third_party_blob_id_t loading;
    bool used_in_current;
    bool used_for_loading;
};

KAN_REFLECTION_STRUCT_META (font_blob_t)
UNIVERSE_TEXT_API struct kan_repository_meta_automatic_cascade_deletion_t font_blob_current_cascade_deletion = {
    .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"current"}},
    .child_type_name = "kan_resource_third_party_blob_t",
    .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"blob_id"}},
};

KAN_REFLECTION_STRUCT_META (font_blob_t)
UNIVERSE_TEXT_API struct kan_repository_meta_automatic_cascade_deletion_t font_blob_current_loading_deletion = {
    .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"loading"}},
    .child_type_name = "kan_resource_third_party_blob_t",
    .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"blob_id"}},
};

struct font_libraries_loaded_event_t
{
    kan_instance_size_t stub;
};

struct text_management_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (text_management)
    KAN_UM_BIND_STATE (text_management, state)

    kan_allocation_group_t temporary_group;
};

UNIVERSE_TEXT_API void text_management_state_init (struct text_management_state_t *instance)
{
    instance->temporary_group = kan_allocation_group_get_child (kan_allocation_group_stack_get (), "temporary");
}

UNIVERSE_TEXT_API KAN_UM_MUTATOR_DEPLOY (text_management)
{
    kan_static_interned_ids_ensure_initialized ();
    kan_cpu_static_sections_ensure_initialized ();

    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_FRAME_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_LOCALE_MANAGEMENT_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_TEXT_MANAGEMENT_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_TEXT_MANAGEMENT_END_CHECKPOINT);
}

static void cancel_font_library_blob_loading (struct text_management_state_t *state)
{
    KAN_UML_SEQUENCE_UPDATE (blob, font_blob_t)
    {
        if (KAN_TYPED_ID_32_IS_VALID (blob->loading))
        {
            KAN_UMI_VALUE_DETACH_REQUIRED (request, kan_resource_third_party_blob_t, blob_id, &blob->loading)
            KAN_UM_ACCESS_DELETE (request);
        }

        blob->loading = KAN_TYPED_ID_32_SET_INVALID (kan_resource_third_party_blob_id_t);
    }
}

static bool on_font_library_updated (struct text_management_state_t *state,
                                     struct text_management_singleton_t *private,
                                     const struct kan_resource_provider_singleton_t *provider,
                                     kan_interned_string_t name)
{
    KAN_UMI_VALUE_UPDATE_OPTIONAL (font_library, font_library_t, name, &name)
    if (!font_library)
    {
        return false;
    }

    // Usage should already exist.
    KAN_ASSERT (KAN_TYPED_ID_32_IS_VALID (font_library->usage_id))

    if (private->font_library_loading_state == FONT_LIBRARY_LOADING_STATE_WAITING_BLOBS)
    {
        cancel_font_library_blob_loading (state);
    }

    private->font_library_loading_state = FONT_LIBRARY_LOADING_STATE_WAITING_MAIN;
    private->font_library_loading_state_frame_id = provider->logic_deduplication_frame_id;
    return true;
}

static void on_font_library_registered (struct text_management_state_t *state,
                                        struct text_management_singleton_t *private,
                                        const struct kan_resource_provider_singleton_t *provider,
                                        kan_interned_string_t name)
{
    // Reset loading state if we've suddenly found new font library.
    if (private->font_library_loading_state == FONT_LIBRARY_LOADING_STATE_WAITING_BLOBS)
    {
        cancel_font_library_blob_loading (state);
    }

    private->font_library_loading_state = FONT_LIBRARY_LOADING_STATE_WAITING_MAIN;
    private->font_library_loading_state_frame_id = provider->logic_deduplication_frame_id;

    KAN_UMO_INDEXED_INSERT (library, font_library_t)
    {
        library->name = name;
        library->library = KAN_HANDLE_SET_INVALID (kan_font_library_t);
        library->usage_class = NULL;
        library->usage_id = kan_next_resource_usage_id (provider);

        KAN_UMO_INDEXED_INSERT (usage, kan_resource_usage_t)
        {
            usage->usage_id = library->usage_id;
            usage->type = KAN_STATIC_INTERNED_ID_GET (kan_resource_font_library_t);
            usage->name = name;
            usage->priority = KAN_UNIVERSE_TEXT_FONT_LIBRARY_PRIORITY;
        }
    }
}

static void advance_font_libraries_from_waiting_main (struct text_management_state_t *state,
                                                      struct text_management_singleton_t *private,
                                                      const struct kan_resource_provider_singleton_t *provider,
                                                      const struct kan_locale_singleton_t *locale);

static void advance_font_libraries_from_waiting_blobs (struct text_management_state_t *state,
                                                       struct text_management_singleton_t *private,
                                                       const struct kan_resource_provider_singleton_t *provider,
                                                       const struct kan_locale_singleton_t *locale);

static void font_blob_start_new_loading (struct text_management_state_t *state,
                                         const struct kan_resource_provider_singleton_t *provider,
                                         struct font_blob_t *blob)
{
    if (KAN_TYPED_ID_32_IS_VALID (blob->loading))
    {
        KAN_UMI_VALUE_DETACH_REQUIRED (request, kan_resource_third_party_blob_t, blob_id, &blob->loading)
        KAN_UM_ACCESS_DELETE (request);
    }

    blob->loading = kan_next_resource_third_party_blob_id (provider);
    KAN_UMO_INDEXED_INSERT (request, kan_resource_third_party_blob_t)
    {
        request->blob_id = blob->loading;
        request->name = blob->name;
        request->priority = KAN_UNIVERSE_TEXT_FONT_BLOB_PRIORITY;
    }
}

static void update_font_blob_usage (struct text_management_state_t *state,
                                    struct text_management_singleton_t *private,
                                    const struct kan_resource_provider_singleton_t *provider,
                                    kan_interned_string_t locale_name)
{
    KAN_UMI_VALUE_READ_OPTIONAL (locale, kan_locale_t, name, &locale_name)
    if (!locale)
    {
        KAN_LOG (text_management, KAN_LOG_ERROR,
                 "Cannot properly update font blob loading state as locale \"%s\" cannot be found!", locale_name)
        return;
    }

    KAN_ASSERT (private->font_library_loading_state == FONT_LIBRARY_LOADING_STATE_WAITING_BLOBS)
    KAN_UML_SEQUENCE_UPDATE (blob_to_clear, font_blob_t) { blob_to_clear->used_for_loading = false; }

    KAN_UML_SEQUENCE_UPDATE (loaded_library, font_library_t)
    {
        KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (resource, kan_resource_font_library_t, &loaded_library->name)
        KAN_ASSERT (resource)
        loaded_library->selected_categories.size = 0u;

        for (kan_loop_size_t category_index = 0u; category_index < resource->categories.size; ++category_index)
        {
            const struct kan_resource_font_category_t *category =
                &((struct kan_resource_font_category_t *) resource->categories.data)[category_index];
            bool filtered_in = false;

            for (kan_loop_size_t category_language_index = 0u;
                 category_language_index < category->used_for_languages.size; ++category_language_index)
            {
                for (kan_loop_size_t locale_language_index = 0u;
                     locale_language_index < locale->resource.font_languages.size; ++locale_language_index)
                {
                    if (((kan_interned_string_t *) category->used_for_languages.data)[category_language_index] ==
                        ((kan_interned_string_t *) locale->resource.font_languages.data)[locale_language_index])
                    {
                        filtered_in = true;
                        break;
                    }
                }

                if (filtered_in)
                {
                    break;
                }
            }

            if (!filtered_in)
            {
                continue;
            }

            kan_instance_size_t *spot = kan_dynamic_array_add_last (&loaded_library->selected_categories);
            if (!spot)
            {
                kan_dynamic_array_set_capacity (&loaded_library->selected_categories,
                                                KAN_MAX (1u, loaded_library->selected_categories.size * 2u));
                spot = kan_dynamic_array_add_last (&loaded_library->selected_categories);
            }

            *spot = category_index;
            for (kan_loop_size_t style_index = 0u; style_index < category->styles.size; ++style_index)
            {
                const struct kan_resource_font_style_t *style =
                    &((struct kan_resource_font_style_t *) category->styles.data)[style_index];

                KAN_UMI_VALUE_UPDATE_OPTIONAL (existing_blob, font_blob_t, name, &style->font_data_file)
                if (existing_blob)
                {
                    existing_blob->used_for_loading = true;
                    if (!KAN_TYPED_ID_32_IS_VALID (existing_blob->loading) &&
                        !KAN_TYPED_ID_32_IS_VALID (existing_blob->current))
                    {
                        font_blob_start_new_loading (state, provider, existing_blob);
                    }

                    continue;
                }

                KAN_UMO_INDEXED_INSERT (new_blob, font_blob_t)
                {
                    new_blob->name = style->font_data_file;
                    new_blob->current = KAN_TYPED_ID_32_SET_INVALID (kan_resource_third_party_blob_id_t);
                    new_blob->loading = KAN_TYPED_ID_32_SET_INVALID (kan_resource_third_party_blob_id_t);
                    new_blob->used_in_current = false;
                    new_blob->used_for_loading = true;
                    font_blob_start_new_loading (state, provider, new_blob);
                }
            }
        }
    }

    KAN_UML_SEQUENCE_DELETE (blob_to_check, font_blob_t)
    {
        if (!blob_to_check->used_in_current && !blob_to_check->used_for_loading)
        {
            KAN_UM_ACCESS_DELETE (blob_to_check);
        }
    }
}

static void advance_font_libraries_from_waiting_main (struct text_management_state_t *state,
                                                      struct text_management_singleton_t *private,
                                                      const struct kan_resource_provider_singleton_t *provider,
                                                      const struct kan_locale_singleton_t *locale)
{
    KAN_LOG (text_management, KAN_LOG_DEBUG,
             "Attempting to advance font library loading from waiting main to waiting blobs state.")
    private->font_library_loading_state_frame_id = provider->logic_deduplication_frame_id;

    KAN_UML_SEQUENCE_READ (library, font_library_t)
    {
        KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (resource, kan_resource_font_library_t, &library->name)
        if (!resource)
        {
            // Not all resources loaded.
            return;
        }
    }

    // Update usage classes.
    KAN_UML_SEQUENCE_UPDATE (loaded_library, font_library_t)
    {
        KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (resource, kan_resource_font_library_t, &loaded_library->name)
        KAN_ASSERT (resource)
        loaded_library->usage_class = resource->usage_class;
    }

    private->font_library_loading_state = FONT_LIBRARY_LOADING_STATE_WAITING_BLOBS;
    update_font_blob_usage (state, private, provider, locale->selected_locale);
    advance_font_libraries_from_waiting_blobs (state, private, provider, locale);
}

static void advance_font_libraries_from_waiting_blobs (struct text_management_state_t *state,
                                                       struct text_management_singleton_t *private,
                                                       const struct kan_resource_provider_singleton_t *provider,
                                                       const struct kan_locale_singleton_t *locale)
{
    KAN_LOG (text_management, KAN_LOG_DEBUG,
             "Attempting to advance font library loading from waiting blobs to ready state.")
    private->font_library_loading_state_frame_id = provider->logic_deduplication_frame_id;

    KAN_UML_SEQUENCE_READ (blob_to_check, font_blob_t)
    {
        if (!blob_to_check->used_for_loading)
        {
            continue;
        }

        if (KAN_TYPED_ID_32_IS_VALID (blob_to_check->loading))
        {
            KAN_UMI_VALUE_READ_REQUIRED (data, kan_resource_third_party_blob_t, blob_id, &blob_to_check->loading)
            if (!data->available)
            {
                return;
            }
        }
        else
        {
            KAN_ASSERT (KAN_TYPED_ID_32_IS_VALID (blob_to_check->current))
#if defined(KAN_WITH_ASSERT)
            KAN_UMI_VALUE_READ_REQUIRED (data, kan_resource_third_party_blob_t, blob_id, &blob_to_check->current)
            KAN_ASSERT (data->available)
#endif
        }
    }

    KAN_UMI_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    KAN_ASSERT (KAN_HANDLE_IS_VALID (render_context->render_context))

    struct kan_font_library_category_t categories_static[KAN_UNIVERSE_TEXT_FONT_CATEGORY_INIT_STACK];
    kan_instance_size_t categories_size = KAN_UNIVERSE_TEXT_FONT_CATEGORY_INIT_STACK;
    struct kan_font_library_category_t *categories = categories_static;

    CUSHION_DEFER
    {
        if (categories_static != categories)
        {
            kan_free_general (state->temporary_group, categories,
                              sizeof (struct kan_font_library_category_t) * categories_size);
        }
    }

    KAN_UML_SEQUENCE_UPDATE (library, font_library_t)
    {
        KAN_CPU_SCOPED_STATIC_SECTION (font_library_create)
        if (KAN_HANDLE_IS_VALID (library->library))
        {
            kan_font_library_destroy (library->library);
        }

        KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (resource, kan_resource_font_library_t, &library->name)
        KAN_ASSERT (resource)
        library->usage_class = resource->usage_class;
        kan_instance_size_t selected_categories_count = 0u;

        for (kan_loop_size_t selection_index = 0u; selection_index < library->selected_categories.size;
             ++selection_index)
        {
            kan_instance_size_t selected_index =
                ((kan_instance_size_t *) library->selected_categories.data)[selection_index];
            const struct kan_resource_font_category_t *category =
                &((struct kan_resource_font_category_t *) resource->categories.data)[selected_index];

            if (selected_categories_count + category->styles.size > categories_size)
            {
                const kan_instance_size_t new_categories_size =
                    KAN_MAX (categories_size * 2u, categories_size + category->styles.size);

                struct kan_font_library_category_t *new_categories = kan_allocate_general (
                    state->temporary_group, sizeof (struct kan_font_library_category_t) * new_categories_size,
                    alignof (struct kan_font_library_category_t));

                memcpy (new_categories, categories,
                        sizeof (struct kan_font_library_category_t) * selected_categories_count);

                if (categories_static != categories)
                {
                    kan_free_general (state->temporary_group, categories,
                                      sizeof (struct kan_font_library_category_t) * categories_size);
                }

                categories_size = new_categories_size;
                categories = new_categories;
            }

            for (kan_loop_size_t style_index = 0u; style_index < category->styles.size; ++style_index)
            {
                const struct kan_resource_font_style_t *style =
                    &((struct kan_resource_font_style_t *) category->styles.data)[style_index];

                struct kan_font_library_category_t *setup = &categories[selected_categories_count];
                ++selected_categories_count;
                KAN_ASSERT (selected_categories_count <= categories_size)

                setup->script = category->script;
                setup->style = style->style;
                setup->variable_axis_count = style->variable_font_axes.size;
                setup->variable_axis = (float *) style->variable_font_axes.data;

                KAN_UMI_VALUE_READ_REQUIRED (font_blob, font_blob_t, name, &style->font_data_file)
                KAN_ASSERT (font_blob->used_for_loading)

                kan_resource_third_party_blob_id_t blob_id =
                    KAN_TYPED_ID_32_IS_VALID (font_blob->loading) ? font_blob->loading : font_blob->current;

                KAN_ASSERT (KAN_TYPED_ID_32_IS_VALID (blob_id))
                KAN_UMI_VALUE_READ_REQUIRED (blob, kan_resource_third_party_blob_t, blob_id, &blob_id)
                KAN_ASSERT (blob->available)

                setup->data_size = blob->available_size;
                setup->data = blob->available_data;
            }
        }

        library->library =
            kan_font_library_create (render_context->render_context, selected_categories_count, categories);

        if (!KAN_HANDLE_IS_VALID (library->library))
        {
            KAN_LOG (text_management, KAN_LOG_ERROR, "Failed to create font library \"%s\".", library->name)
            continue;
        }

        {
            KAN_CPU_SCOPED_STATIC_SECTION (font_library_precache)
            for (kan_loop_size_t selection_index = 0u; selection_index < library->selected_categories.size;
                 ++selection_index)
            {
                kan_instance_size_t selected_index =
                    ((kan_instance_size_t *) library->selected_categories.data)[selection_index];
                const struct kan_resource_font_category_t *category =
                    &((struct kan_resource_font_category_t *) resource->categories.data)[selected_index];

                for (kan_loop_size_t style_index = 0u; style_index < category->styles.size; ++style_index)
                {
                    const struct kan_resource_font_style_t *style =
                        &((struct kan_resource_font_style_t *) category->styles.data)[style_index];

                    struct kan_text_precache_request_t precache_request = {
                        .script = category->script,
                        .style = style->style,
                        .render_format = KAN_FONT_GLYPH_RENDER_FORMAT_SDF,
                        .orientation = KAN_TEXT_ORIENTATION_HORIZONTAL,
                        .utf8 = NULL,
                    };

                    if (category->precache_utf8_horizontal)
                    {
                        precache_request.utf8 = category->precache_utf8_horizontal;
                        if (!kan_font_library_precache (library->library, &precache_request))
                        {
                            KAN_LOG (text_management, KAN_LOG_ERROR,
                                     "Failed to execute precache for library \"%s\" script \"%s\" style \"%s\".",
                                     library->name, category->script, style->style ? style->style : "<default>")
                        }
                    }

                    if (category->precache_utf8_vertical)
                    {
                        precache_request.orientation = KAN_TEXT_ORIENTATION_VERTICAL;
                        precache_request.utf8 = category->precache_utf8_vertical;

                        if (!kan_font_library_precache (library->library, &precache_request))
                        {
                            KAN_LOG (text_management, KAN_LOG_ERROR,
                                     "Failed to execute precache for library \"%s\" script \"%s\" style \"%s\".",
                                     library->name, category->script, style->style ? style->style : "<default>")
                        }
                    }
                }
            }
        }
    }

    KAN_UML_SEQUENCE_WRITE (blob, font_blob_t)
    {
        if (blob->used_for_loading)
        {
            if (KAN_TYPED_ID_32_IS_VALID (blob->loading))
            {
                if (KAN_TYPED_ID_32_IS_VALID (blob->current))
                {
                    KAN_UMI_VALUE_DETACH_REQUIRED (request, kan_resource_third_party_blob_t, blob_id, &blob->current)
                    KAN_UM_ACCESS_DELETE (request);
                }

                blob->current = blob->loading;
                blob->loading = KAN_TYPED_ID_32_SET_INVALID (kan_resource_third_party_blob_id_t);
            }

            blob->used_in_current = true;
            blob->used_for_loading = false;
        }
        else
        {
            KAN_UM_ACCESS_DELETE (blob);
        }
    }

    KAN_UMO_EVENT_INSERT (event, font_libraries_loaded_event_t) { event->stub = 0u; }
    private->font_library_loading_state = FONT_LIBRARY_LOADING_STATE_READY;
    KAN_LOG (text_management, KAN_LOG_DEBUG, "Advanced font library loading to ready state.")
}

static void on_third_party_updated (struct text_management_state_t *state,
                                    struct text_management_singleton_t *private,
                                    const struct kan_resource_provider_singleton_t *provider,
                                    const struct kan_locale_singleton_t *locale,
                                    kan_interned_string_t name)
{
    switch (private->font_library_loading_state)
    {
    case FONT_LIBRARY_LOADING_STATE_INITIAL:
    case FONT_LIBRARY_LOADING_STATE_WAITING_MAIN:
        // Don't care yet.
        return;

    case FONT_LIBRARY_LOADING_STATE_WAITING_BLOBS:
        // No additional logic needed.
        break;

    case FONT_LIBRARY_LOADING_STATE_READY:
    {
        // If it is a font blob, we need to do a reset.
        {
            KAN_UMI_VALUE_READ_OPTIONAL (font_blob, font_blob_t, name, &name)
            if (!font_blob)
            {
                return;
            }
        }

        private->font_library_loading_state = FONT_LIBRARY_LOADING_STATE_WAITING_BLOBS;
        private->font_library_loading_state_frame_id = provider->logic_deduplication_frame_id;
        update_font_blob_usage (state, private, provider, locale->selected_locale);
        break;
    }
    }

    KAN_UMI_VALUE_UPDATE_OPTIONAL (font_blob, font_blob_t, name, &name)
    if (!font_blob || !font_blob->used_for_loading)
    {
        return;
    }

    // Blob updated, load new version.
    font_blob_start_new_loading (state, provider, font_blob);
}

UNIVERSE_TEXT_API KAN_UM_MUTATOR_EXECUTE (text_management)
{
    KAN_UMI_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    if (!KAN_HANDLE_IS_VALID (render_context->render_context))
    {
        // Won't be able to render glyphs until render context is available.
        return;
    }

    KAN_UMI_SINGLETON_READ (provider, kan_resource_provider_singleton_t)
    if (!provider->scan_done)
    {
        return;
    }

    KAN_UMI_SINGLETON_READ (locale, kan_locale_singleton_t)
    if (locale->locale_counter > 0u)
    {
        // Cannot properly load fonts until locale are loaded.
        return;
    }

    if (!locale->selected_locale)
    {
        KAN_LOG (text_management, KAN_LOG_DEBUG,
                 "Skipping text management mutator execution as locale is not yet selected.")
        return;
    }

    KAN_UMI_SINGLETON_WRITE (private, text_management_singleton_t)
    bool need_advance = false;

    KAN_UML_RESOURCE_UPDATED_EVENT_FETCH (main_updated_event, kan_resource_font_library_t)
    {
        need_advance |= on_font_library_updated (state, private, provider, main_updated_event->name);
    }

    KAN_UML_RESOURCE_REGISTERED_EVENT_FETCH (main_registered_event, kan_resource_font_library_t)
    {
        on_font_library_registered (state, private, provider, main_registered_event->name);
        need_advance = true;
    }

    bool process_locale_change = false;
    KAN_UML_EVENT_FETCH (locale_selection_event, kan_locale_selection_updated_t) { process_locale_change = true; }

    KAN_UML_EVENT_FETCH (locale_updated_event, kan_locale_updated_t)
    {
        if (locale_updated_event->name == locale->selected_locale)
        {
            process_locale_change = true;
        }
    }

    if (process_locale_change)
    {
        switch (private->font_library_loading_state)
        {
        case FONT_LIBRARY_LOADING_STATE_INITIAL:
        case FONT_LIBRARY_LOADING_STATE_WAITING_MAIN:
            // Main data is not ready, cannot (and no need to) update blob usage.
            break;

        case FONT_LIBRARY_LOADING_STATE_WAITING_BLOBS:
            update_font_blob_usage (state, private, provider, locale->selected_locale);
            break;

        case FONT_LIBRARY_LOADING_STATE_READY:
            // Format disabled due to strange behavior on Windows.
            // clang-format off
            private->font_library_loading_state = FONT_LIBRARY_LOADING_STATE_WAITING_BLOBS;
            // clang-format on
            private->font_library_loading_state_frame_id = provider->logic_deduplication_frame_id;
            // We do not need any additional logic when falling back to loading blobs,
            // as we're calling usage update right away.
            update_font_blob_usage (state, private, provider, locale->selected_locale);
            break;
        }

        need_advance = true;
    }

    KAN_UML_EVENT_FETCH (third_party_updated_event, kan_resource_third_party_updated_event_t)
    {
        on_third_party_updated (state, private, provider, locale, third_party_updated_event->name);
    }

    if (need_advance)
    {
        switch (private->font_library_loading_state)
        {
        case FONT_LIBRARY_LOADING_STATE_INITIAL:
        case FONT_LIBRARY_LOADING_STATE_READY:
            KAN_ASSERT (false)
            break;

        case FONT_LIBRARY_LOADING_STATE_WAITING_MAIN:
            advance_font_libraries_from_waiting_main (state, private, provider, locale);
            break;

        case FONT_LIBRARY_LOADING_STATE_WAITING_BLOBS:
            advance_font_libraries_from_waiting_blobs (state, private, provider, locale);
            break;
        }
    }

    KAN_UML_RESOURCE_LOADED_EVENT_FETCH (library_loaded_event, kan_resource_font_library_t)
    {
        if (private->font_library_loading_state_frame_id != provider->logic_deduplication_frame_id)
        {
            switch (private->font_library_loading_state)
            {
            case FONT_LIBRARY_LOADING_STATE_INITIAL:
            case FONT_LIBRARY_LOADING_STATE_WAITING_BLOBS:
            case FONT_LIBRARY_LOADING_STATE_READY:
                KAN_ASSERT_FORMATTED (
                    false, "Font library \"%s\" loaded event received while not expecting it due to state %d.",
                    library_loaded_event->name, (unsigned int) private->font_library_loading_state)
                break;

            case FONT_LIBRARY_LOADING_STATE_WAITING_MAIN:
                advance_font_libraries_from_waiting_main (state, private, provider, locale);
                break;
            }
        }
    }

    KAN_UML_EVENT_FETCH (blob_loaded_event, kan_resource_third_party_blob_available_t)
    {
#if defined(KAN_WITH_ASSERT)
        kan_interned_string_t blob_name_for_log = NULL;
#endif

        {
            KAN_UMI_VALUE_READ_REQUIRED (data_blob, kan_resource_third_party_blob_t, blob_id,
                                         &blob_loaded_event->blob_id)

            KAN_UMI_VALUE_READ_OPTIONAL (font_blob, font_blob_t, name, &data_blob->name)
#if defined(KAN_WITH_ASSERT)
            blob_name_for_log = data_blob->name;
#endif

            if (!font_blob)
            {
                continue;
            }
        }

        if (private->font_library_loading_state_frame_id != provider->logic_deduplication_frame_id)
        {
            switch (private->font_library_loading_state)
            {
            case FONT_LIBRARY_LOADING_STATE_INITIAL:
            case FONT_LIBRARY_LOADING_STATE_WAITING_MAIN:
            case FONT_LIBRARY_LOADING_STATE_READY:
                KAN_ASSERT_FORMATTED (false,
                                      "Font blob \"%s\" loaded event received while not expecting it due to state %d.",
                                      blob_name_for_log, (unsigned int) private->font_library_loading_state)
                break;

            case FONT_LIBRARY_LOADING_STATE_WAITING_BLOBS:
                advance_font_libraries_from_waiting_blobs (state, private, provider, locale);
                break;
            }
        }
    }
}

struct text_shaping_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (text_shaping)
    KAN_UM_BIND_STATE (text_shaping, state)
};

UNIVERSE_TEXT_API KAN_UM_MUTATOR_DEPLOY (text_shaping)
{
    kan_static_interned_ids_ensure_initialized ();
    kan_cpu_static_sections_ensure_initialized ();

    // Should normally be in different worlds, but add dependency just in case.
    kan_workflow_graph_node_depend_on (workflow_node, KAN_TEXT_MANAGEMENT_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_TEXT_SHAPING_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_TEXT_SHAPING_END_CHECKPOINT);
}

static inline void shaping_unit_clean_shaped_data (struct kan_text_shaping_unit_t *unit)
{
    if (unit->shaped_as_stable)
    {
        if (KAN_HANDLE_IS_VALID (unit->shaped_stable.glyphs))
        {
            kan_render_buffer_destroy (unit->shaped_stable.glyphs);
        }

        if (KAN_HANDLE_IS_VALID (unit->shaped_stable.icons))
        {
            kan_render_buffer_destroy (unit->shaped_stable.icons);
        }
    }
    else
    {
        kan_dynamic_array_shutdown (&unit->shaped_unstable.glyphs);
        kan_dynamic_array_shutdown (&unit->shaped_unstable.icons);
    }
}

static void shaping_unit_on_failed (struct kan_text_shaping_unit_t *unit)
{
    shaping_unit_clean_shaped_data (unit);
    unit->shaped = false;
    unit->shaped_as_stable = false;
    unit->shaped_with_library = KAN_HANDLE_SET_INVALID (kan_font_library_t);
}

static void shape_unit (struct text_shaping_state_t *state,
                        struct kan_text_shaping_unit_t *unit,
                        const struct kan_locale_t *locale,
                        kan_render_context_t render_context)
{
    unit->dirty = false;
    bool shaped_successfully = false;

    CUSHION_DEFER
    {
        if (shaped_successfully)
        {
            unit->shaped = true;
            unit->shaped_as_stable = unit->stable;
        }
        else
        {
            shaping_unit_on_failed (unit);
        }
    }

    unit->shaped_with_library = KAN_HANDLE_SET_INVALID (kan_font_library_t);
    KAN_UML_SEQUENCE_READ (font_library, font_library_t)
    {
        if (font_library->usage_class == unit->library_usage_class)
        {
            unit->shaped_with_library = font_library->library;
            break;
        }
    }

    if (!KAN_HANDLE_IS_VALID (unit->shaped_with_library))
    {
        KAN_LOG (text_shaping, KAN_LOG_ERROR,
                 "Failed to execute text shaping as font library with usage class \"%s\" is not found.",
                 unit->library_usage_class)
        return;
    }

    switch (locale->resource.preferred_direction)
    {
    case KAN_LOCALE_PREFERRED_TEXT_DIRECTION_LEFT_TO_RIGHT:
        unit->request.reading_direction = KAN_TEXT_READING_DIRECTION_LEFT_TO_RIGHT;
        break;

    case KAN_LOCALE_PREFERRED_TEXT_DIRECTION_RIGHT_TO_LEFT:
        unit->request.reading_direction = KAN_TEXT_READING_DIRECTION_RIGHT_TO_LEFT;
        break;
    }

    struct kan_text_shaped_data_t shaped_data;
    kan_text_shaped_data_init (&shaped_data);

    if (!kan_font_library_shape (unit->shaped_with_library, &unit->request, &shaped_data))
    {
        kan_text_shaped_data_shutdown (&shaped_data);
        KAN_LOG (text_shaping, KAN_LOG_ERROR, "Failed to execute text shaping due to errors in backend.",
                 unit->library_usage_class)
        return;
    }

    unit->shaped_min = shaped_data.min;
    unit->shaped_max = shaped_data.max;

    if (unit->stable)
    {
        const kan_instance_size_t glyphs_data_size =
            sizeof (struct kan_text_shaped_glyph_instance_data_t) * shaped_data.glyphs.size;

        const kan_instance_size_t icons_data_size =
            sizeof (struct kan_text_shaped_icon_instance_data_t) * shaped_data.icons.size;

        if (!unit->shaped || !unit->shaped_as_stable)
        {
            shaping_unit_clean_shaped_data (unit);
            unit->shaped_stable.glyphs = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
            unit->shaped_stable.icons = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
        }

        unit->shaped_stable.glyphs_count = shaped_data.glyphs.size;
        unit->shaped_stable.icons_count = shaped_data.icons.size;

        if (KAN_HANDLE_IS_VALID (unit->shaped_stable.glyphs) && glyphs_data_size > 0u &&
            kan_render_buffer_get_full_size (unit->shaped_stable.glyphs) >= glyphs_data_size)
        {
            void *data = kan_render_buffer_patch (unit->shaped_stable.glyphs, 0u, glyphs_data_size);
            memcpy (data, shaped_data.glyphs.data, glyphs_data_size);
        }
        else
        {
            if (KAN_HANDLE_IS_VALID (unit->shaped_stable.glyphs))
            {
                kan_render_buffer_destroy (unit->shaped_stable.glyphs);
                unit->shaped_stable.glyphs = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
            }

            if (glyphs_data_size > 0u)
            {
                unit->shaped_stable.glyphs =
                    kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_ATTRIBUTE, glyphs_data_size,
                                              shaped_data.glyphs.data, KAN_STATIC_INTERNED_ID_GET (shaped_glyphs));
                KAN_ASSERT (KAN_HANDLE_IS_VALID (unit->shaped_stable.glyphs))
            }
        }

        if (KAN_HANDLE_IS_VALID (unit->shaped_stable.icons) && icons_data_size > 0u &&
            kan_render_buffer_get_full_size (unit->shaped_stable.icons) >= icons_data_size)
        {
            void *data = kan_render_buffer_patch (unit->shaped_stable.icons, 0u, icons_data_size);
            memcpy (data, shaped_data.icons.data, icons_data_size);
        }
        else
        {
            if (KAN_HANDLE_IS_VALID (unit->shaped_stable.icons))
            {
                kan_render_buffer_destroy (unit->shaped_stable.icons);
                unit->shaped_stable.icons = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
            }

            if (icons_data_size > 0u)
            {
                unit->shaped_stable.icons =
                    kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_ATTRIBUTE, icons_data_size,
                                              shaped_data.icons.data, KAN_STATIC_INTERNED_ID_GET (shaped_icons));
                KAN_ASSERT (KAN_HANDLE_IS_VALID (unit->shaped_stable.icons))
            }
        }

        kan_text_shaped_data_shutdown (&shaped_data);
        shaped_successfully = true;
    }
    else
    {
        shaping_unit_clean_shaped_data (unit);
        // On purpose: just copy pointers and go, do not shutdown shaped data on stack.
        unit->shaped_unstable.glyphs = shaped_data.glyphs;
        unit->shaped_unstable.icons = shaped_data.icons;
        shaped_successfully = true;
    }
}

UNIVERSE_TEXT_API KAN_UM_MUTATOR_EXECUTE (text_shaping)
{
    KAN_UMI_SINGLETON_READ (private, text_management_singleton_t)
    KAN_UMI_SINGLETON_READ (locale_singleton, kan_locale_singleton_t)

    if (private->font_library_loading_state != FONT_LIBRARY_LOADING_STATE_READY)
    {
        // Libraries are not ready, no shaping is allowed.
        return;
    }

    KAN_UMI_VALUE_READ_OPTIONAL (locale, kan_locale_t, name, &locale_singleton->selected_locale)
    if (!locale)
    {
        // Can't shape while locale is not available.
        return;
    }

    KAN_UMI_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    KAN_ASSERT (KAN_HANDLE_IS_VALID (render_context->render_context))

    bool after_loading_reshape = false;
    KAN_UML_EVENT_FETCH (loaded_event, font_libraries_loaded_event_t) { after_loading_reshape = true; }

    if (after_loading_reshape)
    {
        KAN_CPU_SCOPED_STATIC_SECTION (after_loading_reshape)
        KAN_UML_SEQUENCE_UPDATE (unit, kan_text_shaping_unit_t)
        {
            if (!unit->stable)
            {
                // Will be reshaped below anyway.
                continue;
            }

            shape_unit (state, unit, locale, render_context->render_context);
        }
    }

    {
        KAN_CPU_SCOPED_STATIC_SECTION (shape_unstable)
        KAN_UML_SIGNAL_UPDATE (unit, kan_text_shaping_unit_t, stable, false)
        {
            shape_unit (state, unit, locale, render_context->render_context);
        }
    }

    {
        KAN_CPU_SCOPED_STATIC_SECTION (shape_dirty)
        KAN_UML_SIGNAL_UPDATE (unit, kan_text_shaping_unit_t, dirty, true)
        {
            if (!unit->stable)
            {
                // Not stable unit, reset flag and skip it.
                unit->dirty = false;
                continue;
            }

            shape_unit (state, unit, locale, render_context->render_context);
        }
    }
}

void kan_text_shaping_singleton_init (struct kan_text_shaping_singleton_t *instance)
{
    instance->unit_id_counter = kan_atomic_int_init (1);
}

void kan_text_shaping_unit_init (struct kan_text_shaping_unit_t *instance)
{
    instance->id = KAN_TYPED_ID_32_SET_INVALID (kan_text_shaping_unit_id_t);
    instance->library_usage_class = NULL;

    instance->request.font_size = 24u;
    instance->request.render_format = KAN_FONT_GLYPH_RENDER_FORMAT_SDF;
    instance->request.orientation = KAN_TEXT_ORIENTATION_HORIZONTAL;
    instance->request.reading_direction = KAN_TEXT_READING_DIRECTION_LEFT_TO_RIGHT;
    instance->request.alignment = KAN_TEXT_SHAPING_ALIGNMENT_LEFT;
    instance->request.primary_axis_limit = 200u;
    instance->request.text = KAN_HANDLE_SET_INVALID (kan_text_t);

    instance->stable = true;
    instance->dirty = true;
    instance->shaped = false;
    instance->shaped_as_stable = true;

    instance->shaped_with_library = KAN_HANDLE_SET_INVALID (kan_font_library_t);
    instance->shaped_min.x = 0;
    instance->shaped_min.y = 0;
    instance->shaped_max.x = 0;
    instance->shaped_max.y = 0;

    instance->shaped_stable.glyphs_count = 0u;
    instance->shaped_stable.icons_count = 0u;
    instance->shaped_stable.glyphs = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
    instance->shaped_stable.icons = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
}

void kan_text_shaping_unit_shutdown (struct kan_text_shaping_unit_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->request.text))
    {
        kan_text_destroy (instance->request.text);
    }

    shaping_unit_clean_shaped_data (instance);
}
