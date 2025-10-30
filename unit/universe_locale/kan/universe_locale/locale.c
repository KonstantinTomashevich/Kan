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

static void advance_locale_from_initial_state (struct locale_management_state_t *state,
                                               struct kan_locale_singleton_t *public,
                                               const struct kan_resource_provider_singleton_t *provider,
                                               struct universe_locale_t *locale);

static void advance_locale_from_waiting_state (struct locale_management_state_t *state,
                                               struct kan_locale_singleton_t *public,
                                               const struct kan_resource_provider_singleton_t *provider,
                                               struct universe_locale_t *locale);

static void on_locale_resource_updated (struct locale_management_state_t *state,
                                        struct kan_locale_singleton_t *public,
                                        const struct kan_resource_provider_singleton_t *provider,
                                        kan_interned_string_t name)
{
    KAN_UMI_VALUE_UPDATE_OPTIONAL (locale, universe_locale_t, name, &name)
    if (!locale)
    {
        return;
    }

    locale->state = UNIVERSE_LOCALE_LOADING_STATE_INITIAL;
    locale->state_frame_id = provider->logic_deduplication_frame_id;

    ++public->locale_counter;
    advance_locale_from_initial_state (state, public, provider, locale);
}

static void on_locale_registered (struct locale_management_state_t *state,
                                  struct kan_locale_singleton_t *public,
                                  const struct kan_resource_provider_singleton_t *provider,
                                  kan_interned_string_t name)
{
    KAN_UMO_INDEXED_INSERT (locale, universe_locale_t)
    {
        locale->name = name;
        locale->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);
        locale->state = UNIVERSE_LOCALE_LOADING_STATE_INITIAL;
        locale->state_frame_id = provider->logic_deduplication_frame_id;

        ++public->locale_counter;
        advance_locale_from_initial_state (state, public, provider, locale);
    }
}

static void advance_locale_from_initial_state (struct locale_management_state_t *state,
                                               struct kan_locale_singleton_t *public,
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

    advance_locale_from_waiting_state (state, public, provider, locale);
}

static void load_locale (const struct kan_resource_locale_t *resource, struct kan_locale_t *locale)
{
    locale->resource.preferred_direction = resource->preferred_direction;
    locale->resource.font_languages.size = 0u;
    kan_dynamic_array_set_capacity (&locale->resource.font_languages, resource->font_languages.size);
    locale->resource.font_languages.size = resource->font_languages.size;
    memcpy (locale->resource.font_languages.data, resource->font_languages.data,
            sizeof (kan_interned_string_t) * resource->font_languages.size);
}

static void advance_locale_from_waiting_state (struct locale_management_state_t *state,
                                               struct kan_locale_singleton_t *public,
                                               const struct kan_resource_provider_singleton_t *provider,
                                               struct universe_locale_t *locale)
{
    KAN_LOG (locale, KAN_LOG_DEBUG, "Attempting to locale \"%s\" state from waiting to ready.", locale->name)
    locale->state_frame_id = provider->logic_deduplication_frame_id;
    KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (resource, kan_resource_locale_t, &locale->name)

    if (!locale)
    {
        // Still waiting.
        return;
    }

    locale->state = UNIVERSE_LOCALE_LOADING_STATE_READY;
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

    KAN_UMO_EVENT_INSERT (updated_event, kan_locale_updated_t) { updated_event->name = locale->name; }
    KAN_ASSERT (public->locale_counter > 0u)
    --public->locale_counter;
    KAN_LOG (locale, KAN_LOG_DEBUG, "Advanced locale \"%s\" state to ready.", locale->name)
}

KAN_UM_MUTATOR_EXECUTE (locale_management)
{
    KAN_UMI_SINGLETON_READ (provider, kan_resource_provider_singleton_t)
    if (!provider->scan_done)
    {
        return;
    }

    KAN_UMI_SINGLETON_WRITE (public, kan_locale_singleton_t)
    KAN_UML_RESOURCE_UPDATED_EVENT_FETCH (locale_updated_event, kan_resource_locale_t)
    {
        on_locale_resource_updated (state, public, provider, locale_updated_event->name);
    }

    KAN_UML_RESOURCE_REGISTERED_EVENT_FETCH (locale_registered_event, kan_resource_locale_t)
    {
        on_locale_registered (state, public, provider, locale_registered_event->name);
    }

    KAN_UML_RESOURCE_LOADED_EVENT_FETCH (locale_loaded_event, kan_resource_locale_t)
    {
        KAN_UMI_VALUE_UPDATE_REQUIRED (locale, universe_locale_t, name, &locale_loaded_event->name)
        if (locale->state_frame_id != provider->logic_deduplication_frame_id)
        {
            switch (locale->state)
            {
            case UNIVERSE_LOCALE_LOADING_STATE_INITIAL:
            case UNIVERSE_LOCALE_LOADING_STATE_READY:
                KAN_ASSERT_FORMATTED (false,
                                      "Locale \"%s\" in state %u received resource loaded event, which is totally "
                                      "unexpected in this state.",
                                      locale->name, (unsigned int) locale->state)
                break;

            case UNIVERSE_LOCALE_LOADING_STATE_WAITING:
                advance_locale_from_waiting_state (state, public, provider, locale);
                break;
            }
        }
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
    instance->selected_locale = NULL;
    instance->locale_counter = 0u;
}

void kan_locale_init (struct kan_locale_t *instance)
{
    instance->name = NULL;
    kan_resource_locale_init (&instance->resource);
}

void kan_locale_shutdown (struct kan_locale_t *instance) { kan_resource_locale_shutdown (&instance->resource); }
