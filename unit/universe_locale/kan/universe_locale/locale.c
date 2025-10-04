#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <string.h>

#include <kan/log/logging.h>
#include <kan/universe/macro.h>
#include <kan/universe_locale/locale.h>
#include <kan/universe_resource_provider/provider.h>

KAN_LOG_DEFINE_CATEGORY (locale);
KAN_USE_STATIC_INTERNED_IDS
KAN_USE_STATIC_CPU_SECTIONS

KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP (locale_management)
UNIVERSE_LOCALE_API KAN_UM_MUTATOR_GROUP_META (locale_management, KAN_RENDER_LOCALE_MANAGEMENT_MUTATOR_GROUP);

enum universe_locale_loading_state_t
{
    UNIVERSE_LOCALE_LOADING_STATE_INITIAL = 0u,
    UNIVERSE_LOCALE_LOADING_STATE_WAITING,

    /// \details We'd like to update locale loaded data only when all the loading states are available and no longer
    ///          waiting to provide the synchronized view on data in case of hot reload.
    UNIVERSE_LOCALE_LOADING_STATE_APPLICABLE,

    UNIVERSE_LOCALE_LOADING_STATE_READY,
};

struct universe_locale_t
{
    kan_interned_string_t name;
    kan_resource_usage_id_t usage_id;
    enum universe_locale_loading_state_t state;
    kan_instance_size_t state_frame_id;
};

KAN_REFLECTION_STRUCT_META (universe_locale_t)
UNIVERSE_LOCALE_API struct kan_repository_meta_automatic_cascade_deletion_t universe_locale_usage_cascade_deletion = {
    .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
    .child_type_name = "kan_resource_usage_t",
    .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
};

struct universe_language_t
{
    kan_interned_string_t name;
    kan_resource_usage_id_t usage_id;
    enum universe_locale_loading_state_t state;
    kan_instance_size_t state_frame_id;
};

KAN_REFLECTION_STRUCT_META (universe_language_t)
UNIVERSE_LOCALE_API struct kan_repository_meta_automatic_cascade_deletion_t universe_language_usage_cascade_deletion = {
    .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
    .child_type_name = "kan_resource_usage_t",
    .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
};

struct locale_management_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (locale_management)
    KAN_UM_BIND_STATE (locale_management, state)
};

KAN_UM_MUTATOR_DEPLOY (locale_management)
{
    kan_static_interned_ids_ensure_initialized ();
    kan_cpu_static_sections_ensure_initialized ();

    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_LOCALE_MANAGEMENT_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RENDER_LOCALE_MANAGEMENT_END_CHECKPOINT);
}

static bool advance_locale_from_initial_state (struct locale_management_state_t *state,
                                               const struct kan_resource_provider_singleton_t *provider,
                                               struct universe_locale_t *locale);

static bool advance_locale_from_waiting_state (struct locale_management_state_t *state,
                                               const struct kan_resource_provider_singleton_t *provider,
                                               struct universe_locale_t *locale);

static void advance_locale_from_applicable_state (struct locale_management_state_t *state,
                                                  const struct kan_resource_provider_singleton_t *provider,
                                                  struct universe_locale_t *locale);

static bool advance_language_from_initial_state (struct locale_management_state_t *state,
                                                 const struct kan_resource_provider_singleton_t *provider,
                                                 struct universe_language_t *language);

static bool advance_language_from_waiting_state (struct locale_management_state_t *state,
                                                 const struct kan_resource_provider_singleton_t *provider,
                                                 struct universe_language_t *language);

static void advance_language_from_applicable_state (struct locale_management_state_t *state,
                                                    const struct kan_resource_provider_singleton_t *provider,
                                                    struct universe_language_t *language);

static bool on_locale_resource_updated (struct locale_management_state_t *state,
                                        const struct kan_resource_provider_singleton_t *provider,
                                        kan_interned_string_t name)
{
    KAN_UMI_VALUE_UPDATE_OPTIONAL (locale, universe_locale_t, name, &name)
    if (!locale)
    {
        return false;
    }

    locale->state = UNIVERSE_LOCALE_LOADING_STATE_INITIAL;
    locale->state_frame_id = provider->logic_deduplication_frame_id;
    return advance_locale_from_initial_state (state, provider, locale);
}

static bool on_language_resource_updated (struct locale_management_state_t *state,
                                          const struct kan_resource_provider_singleton_t *provider,
                                          kan_interned_string_t name)
{
    KAN_UMI_VALUE_UPDATE_OPTIONAL (language, universe_language_t, name, &name)
    if (!language)
    {
        return false;
    }

    language->state = UNIVERSE_LOCALE_LOADING_STATE_INITIAL;
    language->state_frame_id = provider->logic_deduplication_frame_id;
    return advance_language_from_initial_state (state, provider, language);
}

static bool on_locale_registered (struct locale_management_state_t *state,
                                  const struct kan_resource_provider_singleton_t *provider,
                                  kan_interned_string_t name)
{
    KAN_UMO_INDEXED_INSERT (locale, universe_locale_t)
    {
        locale->name = name;
        locale->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);
        locale->state = UNIVERSE_LOCALE_LOADING_STATE_INITIAL;
        locale->state_frame_id = provider->logic_deduplication_frame_id;
        return advance_locale_from_initial_state (state, provider, locale);
    }

    return false;
}

static bool on_language_registered (struct locale_management_state_t *state,
                                    const struct kan_resource_provider_singleton_t *provider,
                                    kan_interned_string_t name)
{
    KAN_UMO_INDEXED_INSERT (language, universe_language_t)
    {
        language->name = name;
        language->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);
        language->state = UNIVERSE_LOCALE_LOADING_STATE_INITIAL;
        language->state_frame_id = provider->logic_deduplication_frame_id;
        return advance_language_from_initial_state (state, provider, language);
    }

    return false;
}

static bool advance_locale_from_initial_state (struct locale_management_state_t *state,
                                               const struct kan_resource_provider_singleton_t *provider,
                                               struct universe_locale_t *locale)
{
    KAN_LOG (locale, KAN_LOG_DEBUG, "Attempting to locale \"%s\" state from initial to waiting.", locale->name)
    locale->state_frame_id = provider->logic_deduplication_frame_id;
    locale->state = UNIVERSE_LOCALE_LOADING_STATE_WAITING; // We will always advance from initial state.

    if (!KAN_TYPED_ID_32_IS_VALID (locale->usage_id))
    {
        locale->usage_id = kan_next_resource_usage_id (provider);
        KAN_UMO_INDEXED_INSERT (usage, kan_resource_usage_t)
        {
            usage->usage_id = usage->usage_id;
            usage->type = KAN_STATIC_INTERNED_ID_GET (kan_resource_locale_t);
            usage->name = locale->name;
            usage->priority = KAN_UNIVERSE_LOCALE_LOADING_PRIORITY;
        }
    }

    return advance_locale_from_waiting_state (state, provider, locale);
}

static bool advance_locale_from_waiting_state (struct locale_management_state_t *state,
                                               const struct kan_resource_provider_singleton_t *provider,
                                               struct universe_locale_t *locale)
{
    KAN_LOG (locale, KAN_LOG_DEBUG, "Attempting to locale \"%s\" state from waiting to applicable.", locale->name)
    locale->state_frame_id = provider->logic_deduplication_frame_id;
    KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (resource, kan_resource_locale_t, &locale->name)

    if (!locale)
    {
        // Still waiting.
        return false;
    }

    locale->state = UNIVERSE_LOCALE_LOADING_STATE_APPLICABLE;
    // Applicable is advanced by separate routine.
    return true;
}

static void load_locale (const struct kan_resource_locale_t *resource, struct kan_locale_t *locale)
{
    locale->resource.preferred_orientation = resource->preferred_orientation;
    locale->resource.font_language_filter.size = 0u;
    kan_dynamic_array_set_capacity (&locale->resource.font_language_filter, resource->font_language_filter.size);
    locale->resource.font_language_filter.size = resource->font_language_filter.size;
    memcpy (locale->resource.font_language_filter.data, resource->font_language_filter.data,
            sizeof (kan_interned_string_t) * resource->font_language_filter.size);
}

static void advance_locale_from_applicable_state (struct locale_management_state_t *state,
                                                  const struct kan_resource_provider_singleton_t *provider,
                                                  struct universe_locale_t *locale)
{
    KAN_LOG (locale, KAN_LOG_DEBUG, "Attempting to locale \"%s\" state from applicable to ready.", locale->name)
    KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (resource, kan_resource_locale_t, &locale->name)

    if (!locale)
    {
        KAN_LOG (locale, KAN_LOG_DEBUG,
                 "Failed to advance locale \"%s\" from applicable state as resource is no longer available, seems "
                 "like internal error.",
                 locale->name)
        return;
    }

    KAN_UMI_VALUE_UPDATE_OPTIONAL (existing_loaded, kan_locale_t, name, &locale->name)
    if (existing_loaded)
    {
        load_locale (resource, existing_loaded);
    }
    else
    {
        KAN_UMO_INDEXED_INSERT (new_loaded, kan_locale_t)
        {
            new_loaded->name = locale->name;
            load_locale (resource, new_loaded);
        }
    }

    locale->state = UNIVERSE_LOCALE_LOADING_STATE_READY;
    locale->state_frame_id = provider->logic_deduplication_frame_id;

    KAN_UMI_VALUE_DETACH_REQUIRED (usage, kan_resource_usage_t, usage_id, &locale->usage_id)
    KAN_UM_ACCESS_DELETE (usage);
    locale->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);

    KAN_LOG (locale, KAN_LOG_DEBUG, "Advanced locale \"%s\" to state ready.", locale->name)
}

static bool advance_language_from_initial_state (struct locale_management_state_t *state,
                                                 const struct kan_resource_provider_singleton_t *provider,
                                                 struct universe_language_t *language)
{
    KAN_LOG (locale, KAN_LOG_DEBUG, "Attempting to language \"%s\" state from initial to waiting.", language->name)
    language->state_frame_id = provider->logic_deduplication_frame_id;
    language->state = UNIVERSE_LOCALE_LOADING_STATE_WAITING; // We will always advance from initial state.

    if (!KAN_TYPED_ID_32_IS_VALID (language->usage_id))
    {
        language->usage_id = kan_next_resource_usage_id (provider);
        KAN_UMO_INDEXED_INSERT (usage, kan_resource_usage_t)
        {
            usage->usage_id = usage->usage_id;
            usage->type = KAN_STATIC_INTERNED_ID_GET (kan_resource_language_t);
            usage->name = language->name;
            usage->priority = KAN_UNIVERSE_LOCALE_LOADING_PRIORITY;
        }
    }

    return advance_language_from_waiting_state (state, provider, language);
}

static bool advance_language_from_waiting_state (struct locale_management_state_t *state,
                                                 const struct kan_resource_provider_singleton_t *provider,
                                                 struct universe_language_t *language)
{
    KAN_LOG (locale, KAN_LOG_DEBUG, "Attempting to language \"%s\" state from waiting to applicable.", language->name)
    language->state_frame_id = provider->logic_deduplication_frame_id;
    KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (resource, kan_resource_language_t, &language->name)

    if (!language)
    {
        // Still waiting.
        return false;
    }

    language->state = UNIVERSE_LOCALE_LOADING_STATE_APPLICABLE;
    // Applicable is advanced by separate routine.
    return true;
}

static void update_is_language_allowed_to_be_used_for_fonts (struct locale_management_state_t *state,
                                                             struct kan_language_t *language)
{
    KAN_UMI_SINGLETON_READ (locale_singleton, kan_locale_singleton_t)
    KAN_UMI_VALUE_READ_OPTIONAL (locale, kan_locale_t, name, &locale_singleton->selected_locale)

    if (!locale || locale->resource.font_language_filter.size == 0u)
    {
        language->allowed_to_be_used_for_fonts = true;
        return;
    }

    language->allowed_to_be_used_for_fonts = false;
    for (kan_loop_size_t index = 0u; index < locale->resource.font_language_filter.size; ++index)
    {
        if (language->name == ((kan_interned_string_t *) locale->resource.font_language_filter.data)[index])
        {
            language->allowed_to_be_used_for_fonts = true;
            break;
        }
    }
}

static inline void load_language (struct locale_management_state_t *state,
                                  const struct kan_resource_language_t *resource,
                                  struct kan_language_t *language)
{
    language->resource = *resource;
    update_is_language_allowed_to_be_used_for_fonts (state, language);
}

static void advance_language_from_applicable_state (struct locale_management_state_t *state,
                                                    const struct kan_resource_provider_singleton_t *provider,
                                                    struct universe_language_t *language)
{
    KAN_LOG (locale, KAN_LOG_DEBUG, "Attempting to language \"%s\" state from applicable to ready.", language->name)
    KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (resource, kan_resource_language_t, &language->name)

    if (!language)
    {
        KAN_LOG (locale, KAN_LOG_DEBUG,
                 "Failed to advance language \"%s\" from applicable state as resource is no longer available, seems "
                 "like internal error.",
                 language->name)
        return;
    }

    KAN_UMI_VALUE_UPDATE_OPTIONAL (existing_loaded, kan_language_t, name, &language->name)
    if (existing_loaded)
    {
        load_language (state, resource, existing_loaded);
    }
    else
    {
        KAN_UMO_INDEXED_INSERT (new_loaded, kan_language_t)
        {
            new_loaded->name = language->name;
            load_language (state, resource, new_loaded);
        }
    }

    language->state = UNIVERSE_LOCALE_LOADING_STATE_READY;
    language->state_frame_id = provider->logic_deduplication_frame_id;

    KAN_UMI_VALUE_DETACH_REQUIRED (usage, kan_resource_usage_t, usage_id, &language->usage_id)
    KAN_UM_ACCESS_DELETE (usage);
    language->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);

    KAN_LOG (locale, KAN_LOG_DEBUG, "Advanced language \"%s\" to state ready.", language->name)
}

KAN_UM_MUTATOR_EXECUTE (locale_management)
{
    KAN_UMI_SINGLETON_READ (provider, kan_resource_provider_singleton_t)
    if (!provider->scan_done)
    {
        return;
    }

    bool any_state_changed = false;
    KAN_UML_RESOURCE_UPDATED_EVENT_FETCH (locale_updated_event, kan_resource_locale_t)
    {
        any_state_changed |= on_locale_resource_updated (state, provider, locale_updated_event->name);
    }

    KAN_UML_RESOURCE_UPDATED_EVENT_FETCH (language_updated_event, kan_resource_language_t)
    {
        any_state_changed |= on_language_resource_updated (state, provider, language_updated_event->name);
    }

    KAN_UML_RESOURCE_REGISTERED_EVENT_FETCH (locale_registered_event, kan_resource_locale_t)
    {
        any_state_changed |= on_locale_registered (state, provider, locale_registered_event->name);
    }

    KAN_UML_RESOURCE_REGISTERED_EVENT_FETCH (language_registered_event, kan_resource_language_t)
    {
        any_state_changed |= on_language_registered (state, provider, language_registered_event->name);
    }

    KAN_UML_RESOURCE_LOADED_EVENT_FETCH (locale_loaded_event, kan_resource_locale_t)
    {
        KAN_UMI_VALUE_UPDATE_REQUIRED (locale, universe_locale_t, name, &locale_loaded_event->name)
        if (locale->state_frame_id != provider->logic_deduplication_frame_id)
        {
            switch (locale->state)
            {
            case UNIVERSE_LOCALE_LOADING_STATE_INITIAL:
            case UNIVERSE_LOCALE_LOADING_STATE_APPLICABLE:
            case UNIVERSE_LOCALE_LOADING_STATE_READY:
                KAN_ASSERT_FORMATTED (false,
                                      "Locale \"%s\" in state %u received resource loaded event, which is totally "
                                      "unexpected in this state.",
                                      locale->name, (unsigned int) locale->state)
                break;

            case UNIVERSE_LOCALE_LOADING_STATE_WAITING:
                any_state_changed |= advance_locale_from_waiting_state (state, provider, locale);
                break;
            }
        }
    }

    KAN_UML_RESOURCE_LOADED_EVENT_FETCH (language_loaded_event, kan_resource_language_t)
    {
        KAN_UMI_VALUE_UPDATE_REQUIRED (language, universe_language_t, name, &language_loaded_event->name)
        if (language->state_frame_id != provider->logic_deduplication_frame_id)
        {
            switch (language->state)
            {
            case UNIVERSE_LOCALE_LOADING_STATE_INITIAL:
            case UNIVERSE_LOCALE_LOADING_STATE_APPLICABLE:
            case UNIVERSE_LOCALE_LOADING_STATE_READY:
                KAN_ASSERT_FORMATTED (false,
                                      "Language \"%s\" in state %u received resource loaded event, which is totally "
                                      "unexpected in this state.",
                                      language->name, (unsigned int) language->state)
                break;

            case UNIVERSE_LOCALE_LOADING_STATE_WAITING:
                any_state_changed |= advance_language_from_waiting_state (state, provider, language);
                break;
            }
        }
    }

    bool selection_changed = false;
    KAN_UML_EVENT_FETCH (selection_changed_event, kan_locale_selection_updated_t) { selection_changed = true; }

    if (!any_state_changed && !selection_changed)
    {
        // Nothing to update, can early out.
        return;
    }

    KAN_CPU_SCOPED_STATIC_SECTION (apply_update)
    bool new_data_applied = false;

    if (any_state_changed)
    {
        bool all_applicable = true;
        KAN_UML_SEQUENCE_READ (locale_to_check, universe_locale_t)
        {
            switch (locale_to_check->state)
            {
            case UNIVERSE_LOCALE_LOADING_STATE_INITIAL:
            case UNIVERSE_LOCALE_LOADING_STATE_WAITING:
                all_applicable = false;
                break;

            case UNIVERSE_LOCALE_LOADING_STATE_APPLICABLE:
            case UNIVERSE_LOCALE_LOADING_STATE_READY:
                break;
            }
        }

        KAN_UML_SEQUENCE_READ (language_to_check, universe_language_t)
        {
            switch (language_to_check->state)
            {
            case UNIVERSE_LOCALE_LOADING_STATE_INITIAL:
            case UNIVERSE_LOCALE_LOADING_STATE_WAITING:
                all_applicable = false;
                break;

            case UNIVERSE_LOCALE_LOADING_STATE_APPLICABLE:
            case UNIVERSE_LOCALE_LOADING_STATE_READY:
                break;
            }
        }

        if (all_applicable)
        {
            // Locales must be updated before languages as language info depends on selected locale.
            KAN_UML_SEQUENCE_UPDATE (locale, universe_locale_t)
            {
                if (locale->state == UNIVERSE_LOCALE_LOADING_STATE_APPLICABLE)
                {
                    advance_locale_from_applicable_state (state, provider, locale);
                }
            }

            KAN_UML_SEQUENCE_UPDATE (language, universe_language_t)
            {
                if (language->state == UNIVERSE_LOCALE_LOADING_STATE_APPLICABLE)
                {
                    advance_language_from_applicable_state (state, provider, language);
                }
            }

            new_data_applied = true;
            {
                KAN_UMI_SINGLETON_WRITE (locale_singleton, kan_locale_singleton_t)
                locale_singleton->initial_loading_complete = true;
            }
        }
    }

    if (selection_changed)
    {
        KAN_UML_SEQUENCE_UPDATE (language, kan_language_t)
        {
            update_is_language_allowed_to_be_used_for_fonts (state, language);
        }
    }

    if (new_data_applied || selection_changed)
    {
        KAN_UMO_EVENT_INSERT (applied_event, kan_locale_update_applied_t) { applied_event->stub = 0u; }
    }
}

KAN_REFLECTION_STRUCT_META (kan_locale_singleton_t)
UNIVERSE_LOCALE_API struct kan_repository_meta_automatic_on_change_event_t
    kan_locale_singleton_field_selected_locale_changed = {
        .event_type = "kan_locale_selection_updated_t",
        .observed_fields_count = 1u,
        .observed_fields =
            (struct kan_repository_field_path_t[]) {
                {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"selected_locale"}},
            },
        .unchanged_copy_outs_count = 1u,
        .unchanged_copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u,
                                    .reflection_path = (const char *[]) {"selected_locale"}},
                    .target_path = {.reflection_path_length = 1u,
                                    .reflection_path = (const char *[]) {"old_selection"}},
                },
            },
        .changed_copy_outs_count = 1u,
        .changed_copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u,
                                    .reflection_path = (const char *[]) {"selected_locale"}},
                    .target_path = {.reflection_path_length = 1u,
                                    .reflection_path = (const char *[]) {"new_selection"}},
                },
            },
};

void kan_locale_singleton_init (struct kan_locale_singleton_t *instance)
{
    instance->initial_loading_complete = false;
    instance->selected_locale = NULL;
}

void kan_locale_init (struct kan_locale_t *instance)
{
    instance->name = NULL;
    kan_resource_locale_init (&instance->resource);
}

void kan_locale_shutdown (struct kan_locale_t *instance) { kan_resource_locale_shutdown (&instance->resource); }

void kan_language_init (struct kan_language_t *instance)
{
    instance->name = NULL;
    instance->allowed_to_be_used_for_fonts = false;
    kan_resource_language_init (&instance->resource);
}
