#include <string.h>

#include <kan/context/all_system_names.h>
#include <kan/context/hot_reload_coordination_system.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/log/logging.h>
#include <kan/precise_time/precise_time.h>
#include <kan/resource_material/resource_material.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe_render_foundation/material.h>
#include <kan/universe_render_foundation/material_instance.h>
#include <kan/universe_render_foundation/render_graph.h>
#include <kan/universe_render_foundation/texture.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

KAN_LOG_DEFINE_CATEGORY (render_foundation_material_instance);

KAN_REFLECTION_FUNCTION_META (kan_universe_mutator_execute_render_foundation_material_instance_management_planning)
KAN_REFLECTION_FUNCTION_META (kan_universe_mutator_execute_render_foundation_material_instance_management_execution)
UNIVERSE_RENDER_FOUNDATION_API struct kan_universe_mutator_group_meta_t
    render_foundation_material_instance_management_group_meta = {
        .group_name = KAN_RENDER_FOUNDATION_MATERIAL_INSTANCE_MANAGEMENT_MUTATOR_GROUP,
};

struct render_foundation_material_instance_usage_on_insert_event_t
{
    kan_interned_string_t material_instance_name;
};

KAN_REFLECTION_STRUCT_META (kan_render_material_instance_usage_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_on_insert_event_t
    render_foundation_material_instance_usage_on_insert_event = {
        .event_type = "render_foundation_material_instance_usage_on_insert_event_t",
        .copy_outs_count = 1u,
        .copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                    .target_path = {.reflection_path_length = 1u,
                                    .reflection_path = (const char *[]) {"material_instance_name"}},
                },
            },
};

struct render_foundation_material_instance_usage_on_change_event_t
{
    kan_interned_string_t old_material_instance_name;
    kan_interned_string_t new_material_instance_name;
};

KAN_REFLECTION_STRUCT_META (kan_render_material_instance_usage_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_on_change_event_t
    render_foundation_material_instance_usage_on_change_event = {
        .event_type = "render_foundation_material_instance_usage_on_change_event_t",
        .observed_fields_count = 3u,
        .observed_fields =
            (struct kan_repository_field_path_t[]) {
                {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"image_best_advised_mip"}},
                {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"image_worst_advised_mip"}},
            },
        .unchanged_copy_outs_count = 1u,
        .unchanged_copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                    .target_path = {.reflection_path_length = 1u,
                                    .reflection_path = (const char *[]) {"old_material_instance_name"}},
                },
            },
        .changed_copy_outs_count = 1u,
        .changed_copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                    .target_path = {.reflection_path_length = 1u,
                                    .reflection_path = (const char *[]) {"new_material_instance_name"}},
                },
            },
};

struct render_foundation_material_instance_usage_on_delete_event_t
{
    kan_interned_string_t material_instance_name;
};

KAN_REFLECTION_STRUCT_META (kan_render_material_instance_usage_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_on_delete_event_t
    render_foundation_material_instance_usage_on_delete_event = {
        .event_type = "render_foundation_material_instance_usage_on_delete_event_t",
        .copy_outs_count = 1u,
        .copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                    .target_path = {.reflection_path_length = 1u,
                                    .reflection_path = (const char *[]) {"material_instance_name"}},
                },
            },
};

KAN_REFLECTION_STRUCT_META (kan_render_material_instance_usage_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_cascade_deletion_t
    kan_render_material_instance_usage_custom_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
        .child_type_name = "kan_render_material_instance_custom_loaded_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
};

struct render_foundation_material_instance_custom_on_change_event_t
{
    kan_render_material_instance_usage_id_t usage_id;
};

KAN_REFLECTION_STRUCT_META (kan_render_material_instance_custom_instanced_parameter_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_on_insert_event_t
    kan_render_material_instance_custom_instanced_parameter_on_insert_event = {
        .event_type = "render_foundation_material_instance_custom_on_change_event_t",
        .copy_outs_count = 1u,
        .copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
                    .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
                },
            },
};

KAN_REFLECTION_STRUCT_META (kan_render_material_instance_custom_instanced_parameter_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_on_change_event_t
    kan_render_material_instance_custom_instanced_parameter_on_change_event = {
        .event_type = "render_foundation_material_instance_custom_on_change_event_t",
        .observed_fields_count = 1u,
        .observed_fields =
            (struct kan_repository_field_path_t[]) {
                {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"parameter"}},
            },
        .unchanged_copy_outs_count = 0u,
        .unchanged_copy_outs = NULL,
        .changed_copy_outs_count = 1u,
        .changed_copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
                    .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
                },
            },
};

KAN_REFLECTION_STRUCT_META (kan_render_material_instance_custom_instanced_parameter_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_on_delete_event_t
    kan_render_material_instance_custom_instanced_parameter_on_delete_event = {
        .event_type = "render_foundation_material_instance_custom_on_change_event_t",
        .copy_outs_count = 1u,
        .copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
                    .target_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
                },
            },
};

struct render_foundation_material_instance_state_t
{
    kan_interned_string_t name;
    kan_resource_request_id_t request_id;
    kan_instance_size_t reference_count;

    kan_interned_string_t static_name;
    kan_interned_string_t loaded_static_name;

    kan_time_size_t last_usage_inspection_time_ns;
    uint8_t image_best_mip;
    uint8_t image_worst_mip;
};

struct render_foundation_material_instance_static_state_t
{
    kan_interned_string_t name;
    kan_resource_request_id_t request_id;
    kan_instance_size_t reference_count;

    kan_time_size_t last_loading_inspection_time_ns;
    kan_time_size_t last_applied_inspection_time_ns;

    kan_render_material_usage_id_t current_material_usage_id;
    kan_render_material_usage_id_t kept_material_usage_id;
    kan_interned_string_t loaded_material_name;
    kan_interned_string_t loading_material_name;

    kan_bool_t mip_update_needed;
    uint8_t image_best_mip;
    uint8_t image_worst_mip;

    kan_render_pipeline_parameter_set_t parameter_set;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_render_buffer_t)
    struct kan_dynamic_array_t parameter_buffers;

    /// \details Needed to easily update images in parameter set even in packaged mode (where there is no hot reload
    ///          and we unload static data after instantiation).
    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (struct kan_resource_material_image_t)
    struct kan_dynamic_array_t last_load_images;
};

UNIVERSE_RENDER_FOUNDATION_API void render_foundation_material_instance_static_state_init (
    struct render_foundation_material_instance_static_state_t *instance)
{
    instance->name = NULL;
    instance->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
    instance->reference_count = 0u;

    instance->last_loading_inspection_time_ns = KAN_INT_MAX (kan_time_size_t);
    instance->last_applied_inspection_time_ns = KAN_INT_MAX (kan_time_size_t);

    instance->current_material_usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_render_material_usage_id_t);
    instance->kept_material_usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_render_material_usage_id_t);
    instance->loaded_material_name = NULL;
    instance->loading_material_name = NULL;

    instance->mip_update_needed = KAN_FALSE;
    instance->image_best_mip = 0u;
    instance->image_worst_mip = KAN_INT_MAX (uint8_t);

    instance->parameter_set = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_t);
    kan_dynamic_array_init (&instance->parameter_buffers, 0u, sizeof (kan_render_buffer_t),
                            _Alignof (kan_render_buffer_t), kan_allocation_group_stack_get ());

    kan_dynamic_array_init (&instance->last_load_images, 0u, sizeof (struct kan_resource_material_image_t),
                            _Alignof (struct kan_resource_material_image_t), kan_allocation_group_stack_get ());
}

UNIVERSE_RENDER_FOUNDATION_API void render_foundation_material_instance_static_state_shutdown (
    struct render_foundation_material_instance_static_state_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->parameter_set))
    {
        kan_render_pipeline_parameter_set_destroy (instance->parameter_set);
    }

    for (kan_loop_size_t index = 0u; index < instance->parameter_buffers.size; ++index)
    {
        kan_render_buffer_t buffer = ((kan_render_buffer_t *) instance->parameter_buffers.data)[index];
        if (KAN_HANDLE_IS_VALID (buffer))
        {
            kan_render_buffer_destroy (buffer);
        }
    }

    kan_dynamic_array_shutdown (&instance->parameter_buffers);
    kan_dynamic_array_shutdown (&instance->last_load_images);
}

struct render_foundation_material_instance_static_image_t
{
    kan_interned_string_t static_name;
    kan_interned_string_t texture_name;
    kan_render_texture_usage_id_t usage_id;
};

struct render_foundation_material_instance_management_planning_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (render_foundation_material_instance_management_planning)
    KAN_UP_BIND_STATE (render_foundation_material_instance_management_planning, state)

    kan_interned_string_t interned_kan_resource_material_instance_static_compiled_t;
    kan_interned_string_t interned_kan_resource_material_instance_compiled_t;
};

UNIVERSE_RENDER_FOUNDATION_API void render_foundation_material_instance_management_planning_state_init (
    struct render_foundation_material_instance_management_planning_state_t *instance)
{
    instance->interned_kan_resource_material_instance_static_compiled_t =
        kan_string_intern ("kan_resource_material_instance_static_compiled_t");
    instance->interned_kan_resource_material_instance_compiled_t =
        kan_string_intern ("kan_resource_material_instance_compiled_t");
}

UNIVERSE_RENDER_FOUNDATION_API void
kan_universe_mutator_deploy_render_foundation_material_instance_management_planning (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct render_foundation_material_instance_management_planning_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node,
                                       KAN_RENDER_FOUNDATION_MATERIAL_INSTANCE_MANAGEMENT_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node,
                                                KAN_RENDER_FOUNDATION_MATERIAL_MANAGEMENT_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node,
                                                KAN_RENDER_FOUNDATION_TEXTURE_MANAGEMENT_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RESOURCE_PROVIDER_BEGIN_CHECKPOINT);
}

static void create_new_usage_state_if_needed (
    struct render_foundation_material_instance_management_planning_state_t *state,
    const struct kan_resource_provider_singleton_t *resource_provider,
    kan_interned_string_t material_instance_name)
{
    KAN_UP_VALUE_UPDATE (referencer_state, render_foundation_material_instance_state_t, name, &material_instance_name)
    {
        ++referencer_state->reference_count;
        KAN_UP_QUERY_RETURN_VOID;
    }

    KAN_UP_INDEXED_INSERT (new_state, render_foundation_material_instance_state_t)
    {
        new_state->name = material_instance_name;
        new_state->reference_count = 1u;
        new_state->static_name = NULL;
        new_state->loaded_static_name = NULL;

        new_state->last_usage_inspection_time_ns = KAN_INT_MAX (kan_time_size_t);
        new_state->image_best_mip = 0u;
        new_state->image_worst_mip = KAN_INT_MAX (uint8_t);

        KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (resource_provider);
            new_state->request_id = request->request_id;

            request->name = material_instance_name;
            request->type = state->interned_kan_resource_material_instance_compiled_t;
            request->priority = KAN_UNIVERSE_RENDER_FOUNDATION_MI_PRIORITY;
        }
    }
}

#define HELPER_UNLINK_STATIC_STATE_DATA(STATIC_NAME)                                                                   \
    KAN_UP_VALUE_WRITE (static_data, render_foundation_material_instance_static_state_t, name, STATIC_NAME)            \
    {                                                                                                                  \
        KAN_ASSERT (static_data->reference_count > 0u)                                                                 \
        --static_data->reference_count;                                                                                \
                                                                                                                       \
        if (static_data->reference_count == 0u)                                                                        \
        {                                                                                                              \
            if (KAN_TYPED_ID_32_IS_VALID (static_data->request_id))                                                    \
            {                                                                                                          \
                KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)                                 \
                {                                                                                                      \
                    event->request_id = static_data->request_id;                                                       \
                }                                                                                                      \
            }                                                                                                          \
                                                                                                                       \
            if (KAN_TYPED_ID_32_IS_VALID (static_data->current_material_usage_id))                                     \
            {                                                                                                          \
                KAN_UP_VALUE_DELETE (usage, kan_render_material_usage_t, usage_id,                                     \
                                     &static_data->current_material_usage_id)                                          \
                {                                                                                                      \
                    KAN_UP_ACCESS_DELETE (usage);                                                                      \
                }                                                                                                      \
            }                                                                                                          \
                                                                                                                       \
            if (KAN_TYPED_ID_32_IS_VALID (static_data->kept_material_usage_id))                                        \
            {                                                                                                          \
                KAN_UP_VALUE_DELETE (usage, kan_render_material_usage_t, usage_id,                                     \
                                     &static_data->kept_material_usage_id)                                             \
                {                                                                                                      \
                    KAN_UP_ACCESS_DELETE (usage);                                                                      \
                }                                                                                                      \
            }                                                                                                          \
                                                                                                                       \
            KAN_UP_VALUE_DELETE (static_image, render_foundation_material_instance_static_image_t, static_name,        \
                                 &static_data->name)                                                                   \
            {                                                                                                          \
                if (KAN_TYPED_ID_32_IS_VALID (static_image->usage_id))                                                 \
                {                                                                                                      \
                    KAN_UP_VALUE_DELETE (usage, kan_render_texture_usage_t, usage_id, &static_image->usage_id)         \
                    {                                                                                                  \
                        KAN_UP_ACCESS_DELETE (usage);                                                                  \
                    }                                                                                                  \
                }                                                                                                      \
                                                                                                                       \
                KAN_UP_ACCESS_DELETE (static_image);                                                                   \
            }                                                                                                          \
                                                                                                                       \
            KAN_UP_ACCESS_DELETE (static_data);                                                                        \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            static_data->mip_update_needed = KAN_TRUE;                                                                 \
        }                                                                                                              \
                                                                                                                       \
        KAN_UP_QUERY_BREAK;                                                                                            \
    }

static void destroy_old_usage_state_if_not_referenced (
    struct render_foundation_material_instance_management_planning_state_t *state,
    kan_interned_string_t material_instance_name)
{
    KAN_UP_VALUE_WRITE (instance, render_foundation_material_instance_state_t, name, &material_instance_name)
    {
        KAN_ASSERT (instance->reference_count > 0u)
        --instance->reference_count;

        if (instance->reference_count > 0u)
        {
            KAN_UP_QUERY_RETURN_VOID;
        }

        if (KAN_TYPED_ID_32_IS_VALID (instance->request_id))
        {
            KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
            {
                event->request_id = instance->request_id;
            }
        }

        KAN_UP_VALUE_DELETE (loaded, kan_render_material_instance_loaded_t, name, &instance->name)
        {
            KAN_UP_ACCESS_DELETE (loaded);
        }

        HELPER_UNLINK_STATIC_STATE_DATA (&instance->static_name)
        KAN_UP_ACCESS_DELETE (instance);
    }
}

UNIVERSE_RENDER_FOUNDATION_API void
kan_universe_mutator_execute_render_foundation_material_instance_management_planning (
    kan_cpu_job_t job, struct render_foundation_material_instance_management_planning_state_t *state)
{
    KAN_UP_SINGLETON_READ (resource_provider, kan_resource_provider_singleton_t)
    {
        if (!resource_provider->scan_done)
        {
            KAN_UP_MUTATOR_RETURN;
        }

        // This mutator only processes changes that result in new request insertion or in deferred request deletion.

        KAN_UP_EVENT_FETCH (on_insert_event, render_foundation_material_instance_usage_on_insert_event_t)
        {
            create_new_usage_state_if_needed (state, resource_provider, on_insert_event->material_instance_name);
        }

        KAN_UP_EVENT_FETCH (on_change_event, render_foundation_material_instance_usage_on_change_event_t)
        {
            if (on_change_event->new_material_instance_name != on_change_event->old_material_instance_name)
            {
                create_new_usage_state_if_needed (state, resource_provider,
                                                  on_change_event->new_material_instance_name);
                destroy_old_usage_state_if_not_referenced (state, on_change_event->old_material_instance_name);
            }
        }

        KAN_UP_EVENT_FETCH (on_delete_event, render_foundation_material_instance_usage_on_delete_event_t)
        {
            destroy_old_usage_state_if_not_referenced (state, on_delete_event->material_instance_name);
        }

        KAN_UP_SIGNAL_UPDATE (data, render_foundation_material_instance_static_state_t, request_id,
                              KAN_TYPED_ID_32_INVALID_LITERAL)
        {
            KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
            {
                request->request_id = kan_next_resource_request_id (resource_provider);
                data->request_id = request->request_id;
                request->name = data->name;
                request->type = state->interned_kan_resource_material_instance_static_compiled_t;
                request->priority = KAN_UNIVERSE_RENDER_FOUNDATION_MI_PRIORITY;
            }
        }
    }

    KAN_UP_MUTATOR_RETURN;
}

struct render_foundation_material_instance_management_execution_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (render_foundation_material_instance_management_execution)
    KAN_UP_BIND_STATE (render_foundation_material_instance_management_execution, state)

    kan_context_system_t render_backend_system;

    kan_interned_string_t interned_kan_resource_material_instance_static_compiled_t;
    kan_interned_string_t interned_kan_resource_material_instance_compiled_t;

    kan_cpu_section_t section_inspect_usage_changes;
    kan_cpu_section_t section_process_material_updates;
    kan_cpu_section_t section_process_texture_updates;
    kan_cpu_section_t section_process_loading;
    kan_cpu_section_t section_update_static_state_mips;

    kan_bool_t hot_reload_possible;

    kan_allocation_group_t temporary_allocation_group;
};

UNIVERSE_RENDER_FOUNDATION_API void render_foundation_material_instance_management_execution_state_init (
    struct render_foundation_material_instance_management_execution_state_t *instance)
{
    instance->interned_kan_resource_material_instance_static_compiled_t =
        kan_string_intern ("kan_resource_material_instance_static_compiled_t");
    instance->interned_kan_resource_material_instance_compiled_t =
        kan_string_intern ("kan_resource_material_instance_compiled_t");

    instance->section_inspect_usage_changes = kan_cpu_section_get ("inspect_usage_changes");
    instance->section_process_material_updates = kan_cpu_section_get ("process_material_updates");
    instance->section_process_texture_updates = kan_cpu_section_get ("process_texture_updates");
    instance->section_process_loading = kan_cpu_section_get ("process_loading");
    instance->section_update_static_state_mips = kan_cpu_section_get ("update_static_state_mips");

    instance->temporary_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_stack_get (), "temporary");
}

UNIVERSE_RENDER_FOUNDATION_API void
kan_universe_mutator_deploy_render_foundation_material_instance_management_execution (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct render_foundation_material_instance_management_execution_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_MATERIAL_MANAGEMENT_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_TEXTURE_MANAGEMENT_END_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node,
                                                KAN_RENDER_FOUNDATION_MATERIAL_INSTANCE_MANAGEMENT_END_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RENDER_FOUNDATION_FRAME_BEGIN);

    state->render_backend_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME);

    kan_context_system_t hot_reload_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (hot_reload_system) &&
        kan_hot_reload_coordination_system_get_current_mode (hot_reload_system) != KAN_HOT_RELOAD_MODE_DISABLED)
    {
        state->hot_reload_possible = KAN_TRUE;
    }
    else
    {
        state->hot_reload_possible = KAN_FALSE;
    }
}

static inline void inspect_material_instance_usages (
    struct render_foundation_material_instance_management_execution_state_t *state,
    kan_interned_string_t material_instance_name,
    kan_time_size_t inspection_time_ns)
{
    KAN_UP_VALUE_UPDATE (instance, render_foundation_material_instance_state_t, name, &material_instance_name)
    {
        if (instance->last_usage_inspection_time_ns == inspection_time_ns)
        {
            KAN_UP_QUERY_RETURN_VOID;
        }

        instance->last_usage_inspection_time_ns = inspection_time_ns;
        uint8_t best_mip = KAN_INT_MAX (uint8_t);
        uint8_t worst_mip = 0u;

        KAN_UP_VALUE_READ (usage, kan_render_material_instance_usage_t, name, &instance->name)
        {
            KAN_ASSERT (usage->image_best_advised_mip <= usage->image_worst_advised_mip)
            best_mip = KAN_MIN (best_mip, usage->image_best_advised_mip);
            worst_mip = KAN_MAX (worst_mip, usage->image_worst_advised_mip);
        }

        if (instance->image_best_mip == best_mip && instance->image_worst_mip == worst_mip)
        {
            KAN_UP_QUERY_RETURN_VOID;
        }

        instance->image_best_mip = best_mip;
        instance->image_worst_mip = worst_mip;

        // Mark that static data mips usage should be updated.
        KAN_UP_VALUE_UPDATE (static_data, render_foundation_material_instance_static_state_t, name,
                             &instance->static_name)
        {
            static_data->mip_update_needed = KAN_TRUE;
        }
    }
}

static void inspect_material_instance_static (
    struct render_foundation_material_instance_management_execution_state_t *state,
    struct render_foundation_material_instance_static_state_t *static_state,
    kan_time_size_t inspection_time_ns);

static void remove_material_instance_loaded_data (
    struct render_foundation_material_instance_management_execution_state_t *state, kan_interned_string_t name)
{
    KAN_UP_VALUE_DELETE (instance_loaded, kan_render_material_instance_loaded_t, name, &name)
    {
        KAN_UP_ACCESS_DELETE (instance_loaded);
    }

    KAN_UP_VALUE_READ (usage, kan_render_material_instance_usage_t, name, &name)
    {
        KAN_UP_VALUE_DELETE (custom_loaded, kan_render_material_instance_custom_loaded_t, usage_id, &usage->usage_id)
        {
            KAN_UP_ACCESS_DELETE (custom_loaded);
        }
    }
}

static inline void process_material_updates (
    struct render_foundation_material_instance_management_execution_state_t *state, kan_time_size_t inspection_time_ns)
{
    KAN_UP_EVENT_FETCH (event, kan_render_material_updated_event_t)
    {
        KAN_UP_VALUE_UPDATE (static_state, render_foundation_material_instance_static_state_t, loaded_material_name,
                             &event->name)
        {
            inspect_material_instance_static (state, static_state, inspection_time_ns);
            if (static_state->last_applied_inspection_time_ns != inspection_time_ns &&
                KAN_HANDLE_IS_VALID (static_state->parameter_set))
            {
                // Material was updated and material instance is loaded, but we weren't able to update material instance
                // right away, therefore loaded data is kind of out of date and must be destroyed until proper update
                // is executed.

                static_state->last_applied_inspection_time_ns = KAN_INT_MAX (kan_time_size_t);
                static_state->loaded_material_name = NULL;
                kan_render_pipeline_parameter_set_destroy (static_state->parameter_set);
                static_state->parameter_set = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_t);

                for (kan_loop_size_t index = 0u; index < static_state->parameter_buffers.size; ++index)
                {
                    kan_render_buffer_t buffer = ((kan_render_buffer_t *) static_state->parameter_buffers.data)[index];
                    if (KAN_HANDLE_IS_VALID (buffer))
                    {
                        kan_render_buffer_destroy (buffer);
                    }
                }

                static_state->parameter_buffers.size = 0u;
                kan_dynamic_array_set_capacity (&static_state->parameter_buffers, 0u);
                static_state->last_load_images.size = 0u;
                kan_dynamic_array_set_capacity (&static_state->last_load_images, 0u);

                KAN_UP_VALUE_READ (instance, render_foundation_material_instance_state_t, static_name,
                                   &static_state->name)
                {
                    remove_material_instance_loaded_data (state, instance->name);
                }
            }
        }

        KAN_UP_VALUE_UPDATE (loading_static_state, render_foundation_material_instance_static_state_t,
                             loading_material_name, &event->name)
        {
            inspect_material_instance_static (state, loading_static_state, inspection_time_ns);
        }
    }
}

static inline void process_texture_updates (
    struct render_foundation_material_instance_management_execution_state_t *state, kan_time_size_t inspection_time_ns)
{
    KAN_UP_EVENT_FETCH (event, kan_render_texture_updated_event_t)
    {
        KAN_UP_VALUE_READ (texture_loaded, kan_render_texture_loaded_t, name, &event->name)
        {
            KAN_UP_VALUE_READ (static_image, render_foundation_material_instance_static_image_t, texture_name,
                               &event->name)
            {
                KAN_UP_VALUE_READ (static_state, render_foundation_material_instance_static_state_t, name,
                                   &static_image->static_name)
                {
                    if (!KAN_HANDLE_IS_VALID (static_state->parameter_set) || !static_state->loaded_material_name ||
                        // No need for extra applies as all the new data is already applied.
                        static_state->last_applied_inspection_time_ns == inspection_time_ns)
                    {
                        KAN_UP_QUERY_BREAK;
                    }

                    KAN_UP_VALUE_READ (material_loaded, kan_render_material_loaded_t, name,
                                       &static_state->loaded_material_name)
                    {
                        struct kan_render_parameter_update_description_t
                            updates[KAN_UNIVERSE_RENDER_FOUNDATION_MI_UPDATES_COUNT];
                        kan_instance_size_t update_output_index = 0u;

                        for (kan_loop_size_t image_index = 0u; image_index < static_state->last_load_images.size;
                             ++image_index)
                        {
                            struct kan_resource_material_image_t *image =
                                &((struct kan_resource_material_image_t *)
                                      static_state->last_load_images.data)[image_index];

                            if (image->texture == static_image->texture_name)
                            {
                                for (kan_loop_size_t image_binding_index = 0u;
                                     image_binding_index < material_loaded->set_material_bindings.images.size;
                                     ++image_binding_index)
                                {
                                    struct kan_rpl_meta_image_t *image_binding =
                                        &((struct kan_rpl_meta_image_t *)
                                              material_loaded->set_material_bindings.images.data)[image_binding_index];

                                    if (image_binding->name == image->name)
                                    {
                                        if (update_output_index >= KAN_UNIVERSE_RENDER_FOUNDATION_MI_UPDATES_COUNT)
                                        {
                                            kan_render_pipeline_parameter_set_update (static_state->parameter_set,
                                                                                      update_output_index, updates);
                                            update_output_index = 0u;
                                        }

                                        updates[update_output_index] =
                                            (struct kan_render_parameter_update_description_t) {
                                                .binding = image_binding->binding,
                                                .image_binding =
                                                    {
                                                        .image = texture_loaded->image,
                                                        .array_index = 0u,
                                                        .layer_offset = 0u,
                                                        .layer_count = 1u,
                                                    },
                                            };

                                        ++update_output_index;
                                        break;
                                    }
                                }
                            }
                        }

                        if (update_output_index > 0u)
                        {
                            kan_render_pipeline_parameter_set_update (static_state->parameter_set, update_output_index,
                                                                      updates);
                        }
                    }
                }
            }
        }
    }
}

static void create_or_update_material_instance_loaded_data (
    struct render_foundation_material_instance_management_execution_state_t *state,
    struct render_foundation_material_instance_static_state_t *static_state,
    struct render_foundation_material_instance_state_t *instance,
    const struct kan_resource_material_instance_compiled_t *instance_data,
    const struct kan_render_material_loaded_t *material_loaded);

static void on_material_instance_updated (
    struct render_foundation_material_instance_management_execution_state_t *state,
    kan_resource_request_id_t request_id,
    kan_time_size_t inspection_time_ns)
{
    kan_interned_string_t new_static_name = NULL;
    KAN_UP_VALUE_UPDATE (instance, render_foundation_material_instance_state_t, request_id, &request_id)
    {
        KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &request_id)
        {
            if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
            {
                KAN_UP_VALUE_READ (
                    container, KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_material_instance_compiled_t),
                    container_id, &request->provided_container_id)
                {
                    const struct kan_resource_material_instance_compiled_t *instance_data =
                        KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_material_instance_compiled_t, container);
                    new_static_name = instance->static_name;

                    if (instance->static_name != instance_data->static_data)
                    {
                        // If current static name is not used for loaded data, we can unlink it right away.
                        if (instance->static_name != instance->loaded_static_name)
                        {
                            HELPER_UNLINK_STATIC_STATE_DATA (&instance->static_name)
                        }

                        instance->static_name = instance_data->static_data;
                        kan_bool_t new_static_exists = KAN_FALSE;

                        KAN_UP_VALUE_UPDATE (static_state, render_foundation_material_instance_static_state_t, name,
                                             &instance->static_name)
                        {
                            new_static_exists = KAN_TRUE;
                            ++static_state->reference_count;
                            static_state->mip_update_needed = KAN_TRUE;
                        }

                        if (!new_static_exists)
                        {
                            KAN_UP_INDEXED_INSERT (new_static_state, render_foundation_material_instance_static_state_t)
                            {
                                new_static_state->name = instance->static_name;
                                new_static_state->reference_count = 1u;
                                new_static_state->mip_update_needed = KAN_TRUE;
                            }
                        }
                    }
                }
            }
        }
    }

    // Material instances static state might've waited for this request to be updated.
    // We need to do inspection in this case.
    KAN_UP_VALUE_UPDATE (static_state, render_foundation_material_instance_static_state_t, name, &new_static_name)
    {
        inspect_material_instance_static (state, static_state, inspection_time_ns);
    }
}

static void delete_dangling_usages_from_static (
    struct render_foundation_material_instance_management_execution_state_t *state,
    struct render_foundation_material_instance_static_state_t *static_state,
    const struct kan_resource_material_instance_static_compiled_t *data)
{
    if (KAN_TYPED_ID_32_IS_VALID (static_state->kept_material_usage_id))
    {
        KAN_UP_VALUE_DELETE (usage, kan_render_material_usage_t, usage_id, &static_state->kept_material_usage_id)
        {
            KAN_UP_ACCESS_DELETE (usage);
        }

        static_state->kept_material_usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_render_material_usage_id_t);
    }

    KAN_UP_VALUE_DELETE (static_image, render_foundation_material_instance_static_image_t, static_name,
                         &static_state->name)
    {
        kan_bool_t unused = KAN_TRUE;
        for (kan_loop_size_t index = 0u; index < data->images.size; ++index)
        {
            const struct kan_resource_material_image_t *image =
                &((struct kan_resource_material_image_t *) data->images.data)[index];

            if (image->texture == static_image->texture_name)
            {
                unused = KAN_FALSE;
                break;
            }
        }

        if (unused)
        {
            KAN_UP_VALUE_DELETE (usage, kan_render_texture_usage_t, usage_id, &static_image->usage_id)
            {
                KAN_UP_ACCESS_DELETE (usage);
            }

            KAN_UP_ACCESS_DELETE (static_image);
        }
    }
}

static inline void apply_parameter_to_memory (kan_interned_string_t instance_name,
                                              kan_interned_string_t tail_name,
                                              kan_bool_t custom,
                                              uint8_t *memory,
                                              kan_instance_size_t offset,
                                              const struct kan_dynamic_array_t *parameters_meta,
                                              const struct kan_resource_material_parameter_t *parameter)
{
    for (kan_loop_size_t meta_index = 0u; meta_index < parameters_meta->size; ++meta_index)
    {
        struct kan_rpl_meta_parameter_t *meta =
            &((struct kan_rpl_meta_parameter_t *) parameters_meta->data)[meta_index];

        if (meta->name == parameter->name)
        {
            if (meta->type != parameter->type)
            {
                if (tail_name)
                {
                    KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                             "Material instance \"%s\" %s has parameter \"%s\" (tail \"%s\") which type %s does not "
                             "match meta type %s.",
                             instance_name, custom ? "(custom)" : "", parameter->name, tail_name,
                             kan_rpl_meta_variable_type_to_string (parameter->type),
                             kan_rpl_meta_variable_type_to_string (meta->type))
                }
                else
                {
                    KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                             "Material instance \"%s\" %s has parameter \"%s\" (main parameters) which type %s does "
                             "not match meta type %s.",
                             instance_name, custom ? "(custom)" : "", parameter->name,
                             kan_rpl_meta_variable_type_to_string (parameter->type),
                             kan_rpl_meta_variable_type_to_string (meta->type))
                }

                break;
            }

            if (meta->total_item_count > 1u)
            {
                if (tail_name)
                {
                    KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                             "Material instance \"%s\" %s has parameter \"%s\" (tail \"%s\") which is in array and "
                             "arrays are not yet supported.",
                             instance_name, custom ? "(custom)" : "", parameter->name, tail_name)
                }
                else
                {
                    KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                             "Material instance \"%s\" %s has parameter \"%s\" (main parameters) which is in array "
                             "and arrays are not yet supported.",
                             instance_name, custom ? "(custom)" : "", parameter->name)
                }

                break;
            }

            uint8_t *address = memory + offset + meta->offset;
            switch (parameter->type)
            {
            case KAN_RPL_META_VARIABLE_TYPE_F1:
                *(float *) address = parameter->value_f1;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_F2:
                *(struct kan_float_vector_2_t *) address = parameter->value_f2;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_F3:
                *(struct kan_float_vector_3_t *) address = parameter->value_f3;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_F4:
                *(struct kan_float_vector_4_t *) address = parameter->value_f4;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_U1:
                *(kan_serialized_size_t *) address = parameter->value_u1;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_U2:
                *(struct kan_unsigned_integer_vector_2_t *) address = parameter->value_u2;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_U3:
                *(struct kan_unsigned_integer_vector_3_t *) address = parameter->value_u3;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_U4:
                *(struct kan_unsigned_integer_vector_4_t *) address = parameter->value_u4;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_S1:
                *(kan_serialized_offset_t *) address = parameter->value_s1;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_S2:
                *(struct kan_integer_vector_2_t *) address = parameter->value_s2;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_S3:
                *(struct kan_integer_vector_3_t *) address = parameter->value_s3;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_S4:
                *(struct kan_integer_vector_4_t *) address = parameter->value_s4;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_F3X3:
                *(struct kan_float_matrix_3x3_t *) address = parameter->value_f3x3;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_F4X4:
                *(struct kan_float_matrix_4x4_t *) address = parameter->value_f4x4;
                break;
            }

            break;
        }
    }
}

#if defined(KAN_UNIVERSE_RENDER_FOUNDATION_VALIDATION_ENABLED)
static inline kan_bool_t is_parameter_found_in_buffer (const struct kan_dynamic_array_t *parameters_meta,
                                                       const struct kan_resource_material_parameter_t *parameter)
{
    for (kan_loop_size_t meta_index = 0u; meta_index < parameters_meta->size; ++meta_index)
    {
        struct kan_rpl_meta_parameter_t *meta =
            &((struct kan_rpl_meta_parameter_t *) parameters_meta->data)[meta_index];

        if (meta->name == parameter->name)
        {
            return KAN_TRUE;
        }
    }

    return KAN_FALSE;
}
#endif

static void instantiate_material_static_data (
    struct render_foundation_material_instance_management_execution_state_t *state,
    struct render_foundation_material_instance_static_state_t *static_state,
    const struct kan_resource_material_instance_static_compiled_t *data,
    const struct kan_render_material_loaded_t *material_loaded)
{
    // Delete old data if it exists.
    if (KAN_HANDLE_IS_VALID (static_state->parameter_set))
    {
        kan_render_pipeline_parameter_set_destroy (static_state->parameter_set);
        static_state->parameter_set = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_t);
    }

    for (kan_loop_size_t index = 0u; index < static_state->parameter_buffers.size; ++index)
    {
        kan_render_buffer_t buffer = ((kan_render_buffer_t *) static_state->parameter_buffers.data)[index];
        if (KAN_HANDLE_IS_VALID (buffer))
        {
            kan_render_buffer_destroy (buffer);
        }
    }

    static_state->parameter_buffers.size = 0u;
    kan_dynamic_array_set_capacity (&static_state->parameter_buffers,
                                    material_loaded->set_material_bindings.buffers.size);
    kan_render_context_t render_context = kan_render_backend_system_get_render_context (state->render_backend_system);

    // Align as the biggest possible alignment of parameter value.
    _Alignas (struct kan_resource_material_parameter_t)
        uint8_t temporary_data_static[KAN_UNIVERSE_RENDER_FOUNDATION_MI_TEMPORARY_DATA_SIZE];
    kan_instance_size_t temporary_data_size = KAN_UNIVERSE_RENDER_FOUNDATION_MI_TEMPORARY_DATA_SIZE;
    uint8_t *temporary_data = temporary_data_static;

#if defined(KAN_UNIVERSE_RENDER_FOUNDATION_VALIDATION_ENABLED)
    // Detect and log unknown parameters and tails.
    for (kan_loop_size_t parameter_index = 0u; parameter_index < data->parameters.size; ++parameter_index)
    {
        struct kan_resource_material_parameter_t *parameter =
            &((struct kan_resource_material_parameter_t *) data->parameters.data)[parameter_index];
        kan_bool_t found = KAN_FALSE;

        for (kan_loop_size_t buffer_index = 0u; buffer_index < material_loaded->set_material_bindings.buffers.size;
             ++buffer_index)
        {
            struct kan_rpl_meta_buffer_t *meta_buffer =
                &((struct kan_rpl_meta_buffer_t *) material_loaded->set_material_bindings.buffers.data)[buffer_index];

            if ((found |= is_parameter_found_in_buffer (&meta_buffer->main_parameters, parameter)))
            {
                break;
            }
        }

        if (!found)
        {
            KAN_LOG (
                render_foundation_material_instance, KAN_LOG_ERROR,
                "Material instance \"%s\" has parameter \"%s\", but there is no such parameter in any meta buffer.",
                static_state->name, parameter->name)
        }
    }

    for (kan_loop_size_t tail_index = 0u; tail_index < data->tail_set.size; ++tail_index)
    {
        struct kan_resource_material_tail_set_t *tail_set =
            &((struct kan_resource_material_tail_set_t *) data->tail_set.data)[tail_index];
        kan_bool_t tail_found = KAN_FALSE;

        for (kan_loop_size_t buffer_index = 0u; buffer_index < material_loaded->set_material_bindings.buffers.size;
             ++buffer_index)
        {
            struct kan_rpl_meta_buffer_t *meta_buffer =
                &((struct kan_rpl_meta_buffer_t *) material_loaded->set_material_bindings.buffers.data)[buffer_index];

            if (meta_buffer->tail_name == tail_set->tail_name)
            {
                for (kan_loop_size_t parameter_index = 0u; parameter_index < tail_set->parameters.size;
                     ++parameter_index)
                {
                    struct kan_resource_material_parameter_t *parameter =
                        &((struct kan_resource_material_parameter_t *) tail_set->parameters.data)[parameter_index];

                    if (!is_parameter_found_in_buffer (&meta_buffer->tail_item_parameters, parameter))
                    {
                        KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                                 "Material instance \"%s\" has parameter \"%s\" (tail \"%s\"), but there is no such "
                                 "parameter in this tail meta.",
                                 static_state->name, parameter->name, tail_set->tail_name)
                    }
                }

                tail_found = KAN_TRUE;
                break;
            }
        }

        if (!tail_found)
        {
            KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                     "Material instance \"%s\" has parameter tail \"%s\", but there is no such tail in meta.",
                     static_state->name, tail_set->tail_name)
        }
    }

    for (kan_loop_size_t tail_index = 0u; tail_index < data->tail_append.size; ++tail_index)
    {
        struct kan_resource_material_tail_append_t *tail_append =
            &((struct kan_resource_material_tail_append_t *) data->tail_append.data)[tail_index];
        kan_bool_t tail_found = KAN_FALSE;

        for (kan_loop_size_t buffer_index = 0u; buffer_index < material_loaded->set_material_bindings.buffers.size;
             ++buffer_index)
        {
            struct kan_rpl_meta_buffer_t *meta_buffer =
                &((struct kan_rpl_meta_buffer_t *) material_loaded->set_material_bindings.buffers.data)[buffer_index];

            if (meta_buffer->tail_name == tail_append->tail_name)
            {
                for (kan_loop_size_t parameter_index = 0u; parameter_index < tail_append->parameters.size;
                     ++parameter_index)
                {
                    struct kan_resource_material_parameter_t *parameter =
                        &((struct kan_resource_material_parameter_t *) tail_append->parameters.data)[parameter_index];

                    if (!is_parameter_found_in_buffer (&meta_buffer->tail_item_parameters, parameter))
                    {
                        KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                                 "Material instance \"%s\" has parameter \"%s\" (tail \"%s\"), but there is no such "
                                 "parameter in this tail meta.",
                                 static_state->name, parameter->name, tail_append->tail_name)
                    }
                }

                tail_found = KAN_TRUE;
                break;
            }
        }

        if (!tail_found)
        {
            KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                     "Material instance \"%s\" has parameter tail \"%s\", but there is no such tail in meta.",
                     static_state->name, tail_append->tail_name)
        }
    }
#endif

    for (kan_loop_size_t index = 0u; index < material_loaded->set_material_bindings.buffers.size; ++index)
    {
        struct kan_rpl_meta_buffer_t *meta_buffer =
            &((struct kan_rpl_meta_buffer_t *) material_loaded->set_material_bindings.buffers.data)[index];

        if (meta_buffer->main_size == 0u && meta_buffer->tail_item_size == 0u)
        {
            KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                     "Buffer \"%s\" of material \"%s\" will always have zero size. Code module error?",
                     meta_buffer->name, data->material)

            kan_render_buffer_t *spot = kan_dynamic_array_add_last (&static_state->parameter_buffers);
            KAN_ASSERT (spot)
            *spot = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
            continue;
        }

        kan_instance_size_t buffer_size = meta_buffer->main_size;
        if (meta_buffer->tail_item_size > 0u)
        {
            kan_bool_t has_tail_set = KAN_FALSE;
            kan_instance_size_t highest_tail_set_index = 0u;

            for (kan_loop_size_t tail_index = 0u; tail_index < data->tail_set.size; ++tail_index)
            {
                struct kan_resource_material_tail_set_t *tail_set =
                    &((struct kan_resource_material_tail_set_t *) data->tail_set.data)[tail_index];

                if (tail_set->tail_name == meta_buffer->tail_name)
                {
                    has_tail_set = KAN_TRUE;
                    highest_tail_set_index = KAN_MAX (highest_tail_set_index, tail_set->index);
                }
            }

            if (has_tail_set)
            {
                buffer_size += (highest_tail_set_index + 1u) * meta_buffer->tail_item_size;
            }

            for (kan_loop_size_t tail_index = 0u; tail_index < data->tail_append.size; ++tail_index)
            {
                struct kan_resource_material_tail_append_t *tail_append =
                    &((struct kan_resource_material_tail_append_t *) data->tail_append.data)[tail_index];

                if (tail_append->tail_name == meta_buffer->tail_name)
                {
                    buffer_size += meta_buffer->tail_item_size;
                }
            }
        }

        if (temporary_data_size < buffer_size)
        {
            if (temporary_data)
            {
                kan_free_general (state->temporary_allocation_group, temporary_data, temporary_data_size);
            }

            temporary_data_size =
                (kan_instance_size_t) kan_apply_alignment (buffer_size, _Alignof (struct kan_float_matrix_4x4_t));
            temporary_data = kan_allocate_general (state->temporary_allocation_group, temporary_data_size,
                                                   _Alignof (struct kan_float_matrix_4x4_t));
        }

        for (kan_loop_size_t parameter_index = 0u; parameter_index < data->parameters.size; ++parameter_index)
        {
            struct kan_resource_material_parameter_t *parameter =
                &((struct kan_resource_material_parameter_t *) data->parameters.data)[parameter_index];
            apply_parameter_to_memory (static_state->name, NULL, KAN_FALSE, temporary_data, 0u,
                                       &meta_buffer->main_parameters, parameter);
        }

        kan_instance_size_t tail_append_offset = 0u;
        for (kan_loop_size_t tail_index = 0u; tail_index < data->tail_set.size; ++tail_index)
        {
            struct kan_resource_material_tail_set_t *tail_set =
                &((struct kan_resource_material_tail_set_t *) data->tail_set.data)[tail_index];

            if (tail_set->tail_name == meta_buffer->tail_name)
            {
                kan_instance_size_t offset = tail_set->index * meta_buffer->tail_item_size;
                tail_append_offset = KAN_MAX (tail_append_offset, offset + meta_buffer->tail_item_size);

                for (kan_loop_size_t parameter_index = 0u; parameter_index < tail_set->parameters.size;
                     ++parameter_index)
                {
                    struct kan_resource_material_parameter_t *parameter =
                        &((struct kan_resource_material_parameter_t *) tail_set->parameters.data)[parameter_index];
                    apply_parameter_to_memory (static_state->name, tail_set->tail_name, KAN_FALSE, temporary_data,
                                               offset, &meta_buffer->tail_item_parameters, parameter);
                }
            }
        }

        for (kan_loop_size_t tail_index = 0u; tail_index < data->tail_append.size; ++tail_index)
        {
            struct kan_resource_material_tail_append_t *tail_append =
                &((struct kan_resource_material_tail_append_t *) data->tail_append.data)[tail_index];

            if (tail_append->tail_name == meta_buffer->tail_name)
            {
                kan_instance_size_t offset = tail_append_offset;
                tail_append_offset += meta_buffer->tail_item_size;

                for (kan_loop_size_t parameter_index = 0u; parameter_index < tail_append->parameters.size;
                     ++parameter_index)
                {
                    struct kan_resource_material_parameter_t *parameter =
                        &((struct kan_resource_material_parameter_t *) tail_append->parameters.data)[parameter_index];
                    apply_parameter_to_memory (static_state->name, tail_append->tail_name, KAN_FALSE, temporary_data,
                                               offset, &meta_buffer->tail_item_parameters, parameter);
                }
            }
        }

        enum kan_render_buffer_type_t buffer_type = KAN_RENDER_BUFFER_TYPE_UNIFORM;
        switch (meta_buffer->type)
        {
        case KAN_RPL_BUFFER_TYPE_UNIFORM:
            buffer_type = KAN_RENDER_BUFFER_TYPE_UNIFORM;
            break;

        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
            buffer_type = KAN_RENDER_BUFFER_TYPE_STORAGE;
            break;
        }

        kan_render_buffer_t buffer = KAN_HANDLE_INITIALIZE_INVALID;

        // Zero size buffers are possible for buffers with zero count of tails and empty main part.
        if (buffer_size > 0u)
        {
            char buffer_name[KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH];
            snprintf (buffer_name, KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH, "%s::%s", static_state->name,
                      meta_buffer->name);

            buffer = kan_render_buffer_create (render_context, buffer_type, buffer_size, temporary_data,
                                               kan_string_intern (buffer_name));

            if (!KAN_HANDLE_IS_VALID (buffer))
            {
                KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                         "Failed to create buffer \"%s\" of material instance \"%s\".", meta_buffer->name,
                         static_state->name)
            }
        }

        kan_render_buffer_t *spot = kan_dynamic_array_add_last (&static_state->parameter_buffers);
        KAN_ASSERT (spot)
        *spot = buffer;
    }

    if (temporary_data != temporary_data_static)
    {
        kan_free_general (state->temporary_allocation_group, temporary_data, temporary_data_size);
    }

    struct kan_render_parameter_update_description_t updates_static[KAN_UNIVERSE_RENDER_FOUNDATION_MI_UPDATES_COUNT];
    const kan_instance_size_t updates_total = material_loaded->set_material_bindings.buffers.size +
                                              material_loaded->set_material_bindings.samplers.size +
                                              material_loaded->set_material_bindings.images.size;
    struct kan_render_parameter_update_description_t *updates = updates_static;

    if (updates_total > KAN_UNIVERSE_RENDER_FOUNDATION_MI_UPDATES_COUNT)
    {
        updates = kan_allocate_general (state->temporary_allocation_group,
                                        updates_total * sizeof (struct kan_render_parameter_update_description_t),
                                        _Alignof (struct kan_render_parameter_update_description_t));
    }

    kan_instance_size_t update_output_index = 0u;
    for (kan_loop_size_t index = 0u; index < material_loaded->set_material_bindings.buffers.size; ++index)
    {
        struct kan_rpl_meta_buffer_t *meta_buffer =
            &((struct kan_rpl_meta_buffer_t *) material_loaded->set_material_bindings.buffers.data)[index];
        kan_render_buffer_t render_buffer = ((kan_render_buffer_t *) static_state->parameter_buffers.data)[index];

        if (KAN_HANDLE_IS_VALID (render_buffer))
        {
            KAN_ASSERT (update_output_index < updates_total)
            updates[update_output_index] = (struct kan_render_parameter_update_description_t) {
                .binding = meta_buffer->binding,
                .buffer_binding =
                    {
                        .buffer = render_buffer,
                        .offset = 0u,
                        .range = kan_render_buffer_get_full_size (render_buffer),
                    },
            };

            ++update_output_index;
        }
    }

    for (kan_loop_size_t sampler_index = 0u; sampler_index < data->samplers.size; ++sampler_index)
    {
        struct kan_resource_material_sampler_t *sampler =
            &((struct kan_resource_material_sampler_t *) data->samplers.data)[sampler_index];

        for (kan_loop_size_t sampler_binding_index = 0u;
             sampler_binding_index < material_loaded->set_material_bindings.samplers.size; ++sampler_binding_index)
        {
            struct kan_rpl_meta_sampler_t *sampler_binding =
                &((struct kan_rpl_meta_sampler_t *)
                      material_loaded->set_material_bindings.samplers.data)[sampler_binding_index];

            if (sampler_binding->name == sampler->name)
            {
                KAN_ASSERT (update_output_index < updates_total)
                updates[update_output_index] = (struct kan_render_parameter_update_description_t) {
                    .binding = sampler_binding->binding,
                    .sampler_binding =
                        {
                            .sampler = sampler->sampler,
                        },
                };

                ++update_output_index;
                break;
            }
        }
    }

    KAN_UP_VALUE_READ (static_image, render_foundation_material_instance_static_image_t, static_name,
                       &static_state->name)
    {
        KAN_UP_VALUE_READ (texture_loaded, kan_render_texture_loaded_t, name, &static_image->texture_name)
        {
            for (kan_loop_size_t image_index = 0u; image_index < data->images.size; ++image_index)
            {
                struct kan_resource_material_image_t *image =
                    &((struct kan_resource_material_image_t *) data->images.data)[image_index];

                if (image->texture == static_image->texture_name)
                {
                    for (kan_loop_size_t image_binding_index = 0u;
                         image_binding_index < material_loaded->set_material_bindings.images.size;
                         ++image_binding_index)
                    {
                        struct kan_rpl_meta_image_t *image_binding =
                            &((struct kan_rpl_meta_image_t *)
                                  material_loaded->set_material_bindings.images.data)[image_binding_index];

                        if (image_binding->name == image->name)
                        {
                            KAN_ASSERT (update_output_index < updates_total)
                            updates[update_output_index] = (struct kan_render_parameter_update_description_t) {
                                .binding = image_binding->binding,
                                .image_binding =
                                    {
                                        .image = texture_loaded->image,
                                        .array_index = 0u,
                                        .layer_offset = 0u,
                                        .layer_count = 1u,
                                    },
                            };

                            ++update_output_index;
                            break;
                        }
                    }
                }
            }
        }
    }

    struct kan_render_pipeline_parameter_set_description_t set_description = {
        .layout = material_loaded->set_material,
        .stable_binding = KAN_TRUE,
        .tracking_name = static_state->name,
        .initial_bindings_count = update_output_index,
        .initial_bindings = updates,
    };

    static_state->parameter_set = kan_render_pipeline_parameter_set_create (render_context, &set_description);
    if (updates != updates_static)
    {
        kan_free_general (state->temporary_allocation_group, updates,
                          updates_total * sizeof (struct kan_render_parameter_update_description_t));
    }

    if (!KAN_HANDLE_IS_VALID (static_state->parameter_set))
    {
        KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                 "Failed to create material instance \"%s\" parameter set.", static_state->name)
    }

    static_state->last_load_images.size = 0u;
    kan_dynamic_array_set_capacity (&static_state->last_load_images, data->images.size);
    static_state->last_load_images.size = data->images.size;
    memcpy (static_state->last_load_images.data, data->images.data,
            static_state->last_load_images.size * sizeof (struct kan_resource_material_image_t));
}

static void update_material_instance_custom_inherit_data (
    const struct kan_render_material_instance_loaded_t *instance_loaded,
    struct kan_render_material_instance_custom_loaded_t *custom_loaded)
{
    custom_loaded->data.material_name = instance_loaded->data.material_name;
    custom_loaded->data.parameter_set = instance_loaded->data.parameter_set;

    custom_loaded->data.instanced_data.size = 0u;
    kan_dynamic_array_set_capacity (&custom_loaded->data.instanced_data, instance_loaded->data.instanced_data.size);
    custom_loaded->data.instanced_data.size = custom_loaded->data.instanced_data.capacity;
    memcpy (custom_loaded->data.instanced_data.data, instance_loaded->data.instanced_data.data,
            custom_loaded->data.instanced_data.size);
}

#if defined(KAN_UNIVERSE_RENDER_FOUNDATION_VALIDATION_ENABLED)
static inline kan_bool_t is_instanced_attribute_found_in_source (
    const struct kan_rpl_meta_attribute_source_t *source, const struct kan_resource_material_parameter_t *parameter)
{
    for (kan_loop_size_t meta_index = 0u; meta_index < source->attributes.size; ++meta_index)
    {
        struct kan_rpl_meta_attribute_t *meta =
            &((struct kan_rpl_meta_attribute_t *) source->attributes.data)[meta_index];

        if (meta->name == parameter->name)
        {
            return KAN_TRUE;
        }
    }

    return KAN_FALSE;
}
#endif

static inline void apply_instanced_attribute_to_memory (kan_interned_string_t instance_name,
                                                        kan_bool_t custom,
                                                        uint8_t *memory,
                                                        kan_instance_size_t offset,
                                                        const struct kan_rpl_meta_attribute_source_t *source,
                                                        const struct kan_resource_material_parameter_t *parameter)
{
    for (kan_loop_size_t meta_index = 0u; meta_index < source->attributes.size; ++meta_index)
    {
        struct kan_rpl_meta_attribute_t *meta =
            &((struct kan_rpl_meta_attribute_t *) source->attributes.data)[meta_index];

        if (meta->name == parameter->name)
        {
            enum kan_rpl_meta_variable_type_t expected_variable_type = KAN_RPL_META_VARIABLE_TYPE_F1;
            kan_bool_t variable_type_valid = KAN_TRUE;

            switch (meta->item_format)
            {
            case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_16:
            case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_8:
            case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_16:
            case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_8:
            case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_16:
            case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_8:
            case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_16:
            case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_8:
            case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_16:
                // These formats shouldn't be allowed during material compilation for instanced attributes.
                KAN_ASSERT (KAN_FALSE)
                break;

            case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32:
                switch (meta->class)
                {
                case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_1:
                    expected_variable_type = KAN_RPL_META_VARIABLE_TYPE_F1;
                    break;

                case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_2:
                    expected_variable_type = KAN_RPL_META_VARIABLE_TYPE_F2;
                    break;

                case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_3:
                    expected_variable_type = KAN_RPL_META_VARIABLE_TYPE_F3;
                    break;

                case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_4:
                    expected_variable_type = KAN_RPL_META_VARIABLE_TYPE_F4;
                    break;

                case KAN_RPL_META_ATTRIBUTE_CLASS_MATRIX_3X3:
                    expected_variable_type = KAN_RPL_META_VARIABLE_TYPE_F3X3;
                    break;

                case KAN_RPL_META_ATTRIBUTE_CLASS_MATRIX_4X4:
                    expected_variable_type = KAN_RPL_META_VARIABLE_TYPE_F4X4;
                    break;
                }

                break;

            case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_32:
                switch (meta->class)
                {
                case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_1:
                    expected_variable_type = KAN_RPL_META_VARIABLE_TYPE_U1;
                    break;

                case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_2:
                    expected_variable_type = KAN_RPL_META_VARIABLE_TYPE_U2;
                    break;

                case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_3:
                    expected_variable_type = KAN_RPL_META_VARIABLE_TYPE_U3;
                    break;

                case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_4:
                    expected_variable_type = KAN_RPL_META_VARIABLE_TYPE_U4;
                    break;

                case KAN_RPL_META_ATTRIBUTE_CLASS_MATRIX_3X3:
                case KAN_RPL_META_ATTRIBUTE_CLASS_MATRIX_4X4:
                    KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                             "Material instance \"%s\" %s has parameter \"%s\" which is expected to be integer matrix "
                             "by the pipeline, but integer matrices are not supported.",
                             instance_name, custom ? "(custom)" : "", parameter->name)
                    variable_type_valid = KAN_FALSE;
                    break;
                }

                break;

            case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_32:
                switch (meta->class)
                {
                case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_1:
                    expected_variable_type = KAN_RPL_META_VARIABLE_TYPE_S1;
                    break;

                case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_2:
                    expected_variable_type = KAN_RPL_META_VARIABLE_TYPE_S2;
                    break;

                case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_3:
                    expected_variable_type = KAN_RPL_META_VARIABLE_TYPE_S3;
                    break;

                case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_4:
                    expected_variable_type = KAN_RPL_META_VARIABLE_TYPE_S4;
                    break;

                case KAN_RPL_META_ATTRIBUTE_CLASS_MATRIX_3X3:
                case KAN_RPL_META_ATTRIBUTE_CLASS_MATRIX_4X4:
                    KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                             "Material instance \"%s\" %s has parameter \"%s\" which is expected to be integer matrix "
                             "by the pipeline, but integer matrices are not supported.",
                             instance_name, custom ? "(custom)" : "", parameter->name)
                    variable_type_valid = KAN_FALSE;
                    break;
                }

                break;
            }

            if (!variable_type_valid)
            {
                break;
            }

            if (expected_variable_type != parameter->type)
            {
                KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                         "Material instance \"%s\" %s has parameter \"%s\" (main parameters) which type %s does "
                         "not match expected type %s.",
                         instance_name, custom ? "(custom)" : "", parameter->name,
                         kan_rpl_meta_variable_type_to_string (parameter->type),
                         kan_rpl_meta_variable_type_to_string (expected_variable_type))
                break;
            }

            uint8_t *address = memory + offset + meta->offset;
            switch (parameter->type)
            {
            case KAN_RPL_META_VARIABLE_TYPE_F1:
                *(float *) address = parameter->value_f1;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_F2:
                *(struct kan_float_vector_2_t *) address = parameter->value_f2;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_F3:
                *(struct kan_float_vector_3_t *) address = parameter->value_f3;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_F4:
                *(struct kan_float_vector_4_t *) address = parameter->value_f4;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_U1:
                *(kan_serialized_size_t *) address = parameter->value_u1;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_U2:
                *(struct kan_unsigned_integer_vector_2_t *) address = parameter->value_u2;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_U3:
                *(struct kan_unsigned_integer_vector_3_t *) address = parameter->value_u3;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_U4:
                *(struct kan_unsigned_integer_vector_4_t *) address = parameter->value_u4;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_S1:
                *(kan_serialized_offset_t *) address = parameter->value_s1;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_S2:
                *(struct kan_integer_vector_2_t *) address = parameter->value_s2;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_S3:
                *(struct kan_integer_vector_3_t *) address = parameter->value_s3;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_S4:
                *(struct kan_integer_vector_4_t *) address = parameter->value_s4;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_F3X3:
                *(struct kan_float_matrix_3x3_t *) address = parameter->value_f3x3;
                break;

            case KAN_RPL_META_VARIABLE_TYPE_F4X4:
                *(struct kan_float_matrix_4x4_t *) address = parameter->value_f4x4;
                break;
            }

            break;
        }
    }
}

static void update_material_instance_custom_apply_parameter (
    const struct kan_render_material_loaded_t *material_loaded,
    const struct kan_render_material_instance_loaded_t *instance_loaded,
    struct kan_render_material_instance_custom_loaded_t *custom_loaded,
    const struct kan_render_material_instance_custom_instanced_parameter_t *parameter)
{
#if defined(KAN_UNIVERSE_RENDER_FOUNDATION_VALIDATION_ENABLED)
    kan_bool_t found = KAN_FALSE;
    if (material_loaded->has_instanced_attribute_source)
    {
        found = is_instanced_attribute_found_in_source (&material_loaded->instanced_attribute_source,
                                                        &parameter->parameter);
    }

    if (!found)
    {
        KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                 "Material instance \"%s\" custom usage with id %lu has instanced parameter \"%s\", but there is no "
                 "such parameter in any meta attribute buffer.",
                 instance_loaded->name, (unsigned long) KAN_TYPED_ID_32_GET (custom_loaded->usage_id),
                 parameter->parameter.name)
    }
#endif

    apply_instanced_attribute_to_memory (instance_loaded->name, KAN_TRUE, custom_loaded->data.instanced_data.data, 0u,
                                         &material_loaded->instanced_attribute_source, &parameter->parameter);
}

/// \details Macro as it can be used from different mutators.
#define UPDATE_MATERIAL_INSTANCE_CUSTOM_LOADED_DATA(MATERIAL_LOADED, INSTANCE_LOADED, CUSTOM_LOADED)                   \
    update_material_instance_custom_inherit_data (INSTANCE_LOADED, CUSTOM_LOADED);                                     \
    KAN_UP_VALUE_READ (custom_parameter, kan_render_material_instance_custom_instanced_parameter_t, usage_id,          \
                       &CUSTOM_LOADED->usage_id)                                                                       \
    {                                                                                                                  \
        update_material_instance_custom_apply_parameter (MATERIAL_LOADED, INSTANCE_LOADED, CUSTOM_LOADED,              \
                                                         custom_parameter);                                            \
    }

static void update_material_instance_loaded_data (
    struct render_foundation_material_instance_management_execution_state_t *state,
    struct render_foundation_material_instance_static_state_t *static_state,
    const struct kan_resource_material_instance_compiled_t *instance_data,
    const struct kan_render_material_loaded_t *material_loaded,
    struct kan_render_material_instance_loaded_t *instance_loaded)
{
    instance_loaded->data.material_name = static_state->loaded_material_name;
    instance_loaded->data.parameter_set = static_state->parameter_set;

#if defined(KAN_UNIVERSE_RENDER_FOUNDATION_VALIDATION_ENABLED)
    // Detect and log unknown parameters and tails.
    if (material_loaded->has_instanced_attribute_source)
    {
        for (kan_loop_size_t parameter_index = 0u; parameter_index < instance_data->instanced_parameters.size;
             ++parameter_index)
        {
            struct kan_resource_material_parameter_t *parameter = &(
                (struct kan_resource_material_parameter_t *) instance_data->instanced_parameters.data)[parameter_index];

            if (!is_instanced_attribute_found_in_source (&material_loaded->instanced_attribute_source, parameter))
            {
                KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                         "Material instance \"%s\" has instanced parameter \"%s\", but there is no such parameter in "
                         "any meta attribute buffer.",
                         instance_loaded->name, parameter->name)
            }
        }
    }
    else if (instance_data->instanced_parameters.size > 0u)
    {
        KAN_LOG (render_foundation_material_instance, KAN_LOG_ERROR,
                 "Material instance \"%s\" has instanced parameters, but there is no instanced attribute buffer in "
                 "material.",
                 instance_loaded->name)
    }
#endif

    instance_loaded->data.instanced_data.size = 0u;
    if (material_loaded->has_instanced_attribute_source)
    {
        kan_dynamic_array_set_capacity (&instance_loaded->data.instanced_data,
                                        material_loaded->instanced_attribute_source.block_size);
        instance_loaded->data.instanced_data.size = material_loaded->instanced_attribute_source.block_size;

        for (kan_loop_size_t parameter_index = 0u; parameter_index < instance_data->instanced_parameters.size;
             ++parameter_index)
        {
            struct kan_resource_material_parameter_t *parameter = &(
                (struct kan_resource_material_parameter_t *) instance_data->instanced_parameters.data)[parameter_index];

            apply_instanced_attribute_to_memory (instance_loaded->name, KAN_FALSE,
                                                 instance_loaded->data.instanced_data.data, 0u,
                                                 &material_loaded->instanced_attribute_source, parameter);
        }
    }
    else
    {
        kan_dynamic_array_set_capacity (&instance_loaded->data.instanced_data, 0u);
    }

    // Update custom instances.
    KAN_UP_VALUE_READ (usage, kan_render_material_instance_usage_t, name, &instance_loaded->name)
    {
        kan_bool_t has_parameters = KAN_FALSE;
        KAN_UP_VALUE_READ (parameter, kan_render_material_instance_custom_instanced_parameter_t, usage_id,
                           &usage->usage_id)
        {
            has_parameters = KAN_TRUE;
            KAN_UP_QUERY_BREAK;
        }

        if (has_parameters)
        {
            kan_bool_t existing = KAN_FALSE;
            KAN_UP_VALUE_UPDATE (custom_loaded, kan_render_material_instance_custom_loaded_t, usage_id,
                                 &usage->usage_id)
            {
                existing = KAN_TRUE;
                custom_loaded->last_inspection_time_ns = static_state->last_applied_inspection_time_ns;
                UPDATE_MATERIAL_INSTANCE_CUSTOM_LOADED_DATA (material_loaded, instance_loaded, custom_loaded)
            }

            if (!existing)
            {
                KAN_UP_INDEXED_INSERT (new_custom_loaded, kan_render_material_instance_custom_loaded_t)
                {
                    new_custom_loaded->usage_id = usage->usage_id;
                    new_custom_loaded->last_inspection_time_ns = static_state->last_applied_inspection_time_ns;
                    UPDATE_MATERIAL_INSTANCE_CUSTOM_LOADED_DATA (material_loaded, instance_loaded, new_custom_loaded)
                }
            }
        }
    }
}

static void create_or_update_material_instance_loaded_data (
    struct render_foundation_material_instance_management_execution_state_t *state,
    struct render_foundation_material_instance_static_state_t *static_state,
    struct render_foundation_material_instance_state_t *instance,
    const struct kan_resource_material_instance_compiled_t *instance_data,
    const struct kan_render_material_loaded_t *material_loaded)
{
    kan_bool_t existing = KAN_FALSE;
    KAN_UP_VALUE_UPDATE (instance_loaded, kan_render_material_instance_loaded_t, name, &instance->name)
    {
        existing = KAN_TRUE;
        update_material_instance_loaded_data (state, static_state, instance_data, material_loaded, instance_loaded);
        KAN_UP_QUERY_BREAK;
    }

    if (!existing)
    {
        KAN_UP_INDEXED_INSERT (new_instance_loaded, kan_render_material_instance_loaded_t)
        {
            new_instance_loaded->name = instance->name;
            update_material_instance_loaded_data (state, static_state, instance_data, material_loaded,
                                                  new_instance_loaded);
        }
    }
}

static void update_linked_material_instances (
    struct render_foundation_material_instance_management_execution_state_t *state,
    struct render_foundation_material_instance_static_state_t *static_state,
    const struct kan_render_material_loaded_t *material_loaded)
{
    if (state->hot_reload_possible)
    {
        // When hot reload is enabled, there is a peculiar case when:
        //
        // - Instance plans to use new static data after loading (new static data name).
        // - Loaded static data is also updated 1 or more prior to the new static data.
        // - Loaded instance ends up with invalid data as its loaded static reloaded its data, but normal routine
        //   didn't update loaded instance as it already has other static name selected for the future.
        //
        // Therefore, to solve this, we additionally updated instances that reference this state as loaded: if they've
        // already received new data, they wouldn't have this static data in loaded fields.
        KAN_UP_VALUE_UPDATE (instance, render_foundation_material_instance_state_t, loaded_static_name,
                             &static_state->name)
        {
            KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &instance->request_id)
            {
                KAN_ASSERT (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
                KAN_UP_VALUE_READ (
                    container, KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_material_instance_compiled_t),
                    container_id, &request->provided_container_id)
                {
                    const struct kan_resource_material_instance_compiled_t *instance_data =
                        KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_material_instance_compiled_t, container);
                    create_or_update_material_instance_loaded_data (state, static_state, instance, instance_data,
                                                                    material_loaded);
                }
            }
        }
    }

    KAN_UP_VALUE_UPDATE (instance, render_foundation_material_instance_state_t, static_name, &static_state->name)
    {
        KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &instance->request_id)
        {
            KAN_ASSERT (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
            KAN_UP_VALUE_READ (container,
                               KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_material_instance_compiled_t),
                               container_id, &request->provided_container_id)
            {
                const struct kan_resource_material_instance_compiled_t *instance_data =
                    KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_material_instance_compiled_t, container);
                create_or_update_material_instance_loaded_data (state, static_state, instance, instance_data,
                                                                material_loaded);

                if (instance->loaded_static_name != instance->static_name)
                {
                    HELPER_UNLINK_STATIC_STATE_DATA (&instance->loaded_static_name)
                    instance->loaded_static_name = instance->static_name;
                }
            }
        }

        // If hot reload is not possible, delete resource request to free a little bit of memory.
        if (!state->hot_reload_possible)
        {
            if (KAN_TYPED_ID_32_IS_VALID (instance->request_id))
            {
                KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
                {
                    event->request_id = instance->request_id;
                }
            }
        }
    }
}

static void inspect_material_instance_static (
    struct render_foundation_material_instance_management_execution_state_t *state,
    struct render_foundation_material_instance_static_state_t *static_state,
    kan_time_size_t inspection_time_ns)
{
    if (static_state->last_loading_inspection_time_ns == inspection_time_ns)
    {
        return;
    }

    static_state->last_loading_inspection_time_ns = inspection_time_ns;
    kan_bool_t static_data_ready = KAN_FALSE;
    kan_bool_t material_ready = KAN_FALSE;

    KAN_UP_VALUE_READ (static_request, kan_resource_request_t, request_id, &static_state->request_id)
    {
        if ((static_data_ready = !static_request->expecting_new_data &&
                                 KAN_TYPED_ID_32_IS_VALID (static_request->provided_container_id)))
        {
            KAN_UP_VALUE_READ (
                container, KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_material_instance_static_compiled_t),
                container_id, &static_request->provided_container_id)
            {
                const struct kan_resource_material_instance_static_compiled_t *data =
                    KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_material_instance_static_compiled_t, container);

                KAN_UP_VALUE_READ (material_loaded, kan_render_material_loaded_t, name, &data->material)
                {
                    material_ready = KAN_HANDLE_IS_VALID (material_loaded->set_material);
                }
            }
        }
    }

    if (!static_data_ready || !material_ready)
    {
        return;
    }

    KAN_UP_VALUE_READ (instance_state, render_foundation_material_instance_state_t, static_name, &static_state->name)
    {
        kan_bool_t instance_ready = KAN_FALSE;
        KAN_UP_VALUE_READ (instance_request, kan_resource_request_t, request_id, &instance_state->request_id)
        {
            instance_ready = !instance_request->expecting_new_data &&
                             KAN_TYPED_ID_32_IS_VALID (instance_request->provided_container_id);
        }

        if (!instance_ready)
        {
            KAN_UP_QUERY_RETURN_VOID;
        }
    }

    KAN_UP_VALUE_READ (static_image, render_foundation_material_instance_static_image_t, static_name,
                       &static_state->name)
    {
        kan_bool_t image_ready = KAN_FALSE;
        KAN_UP_VALUE_READ (texture_loaded, kan_render_texture_loaded_t, name, &static_image->texture_name)
        {
            image_ready = KAN_HANDLE_IS_VALID (texture_loaded->image);
        }

        if (!image_ready)
        {
            KAN_UP_QUERY_RETURN_VOID;
        }
    }

    KAN_UP_VALUE_READ (data_request, kan_resource_request_t, request_id, &static_state->request_id)
    {
        if ((static_data_ready = KAN_TYPED_ID_32_IS_VALID (data_request->provided_container_id)))
        {
            KAN_UP_VALUE_READ (
                container, KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_material_instance_static_compiled_t),
                container_id, &data_request->provided_container_id)
            {
                const struct kan_resource_material_instance_static_compiled_t *data =
                    KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_material_instance_static_compiled_t, container);

                KAN_UP_VALUE_READ (material_loaded, kan_render_material_loaded_t, name, &data->material)
                {
                    delete_dangling_usages_from_static (state, static_state, data);
                    static_state->loaded_material_name = data->material;
                    static_state->loading_material_name = NULL;
                    static_state->last_applied_inspection_time_ns = inspection_time_ns;
                    instantiate_material_static_data (state, static_state, data, material_loaded);
                    update_linked_material_instances (state, static_state, material_loaded);

                    // If hot reload is not possible, delete resource request to free a little bit of memory.
                    if (!state->hot_reload_possible)
                    {
                        if (KAN_TYPED_ID_32_IS_VALID (static_state->request_id))
                        {
                            KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
                            {
                                event->request_id = static_state->request_id;
                            }
                        }
                    }
                }
            }
        }
    }
}

static inline void on_material_instance_static_updated (
    struct render_foundation_material_instance_management_execution_state_t *state,
    kan_resource_request_id_t request_id,
    kan_time_size_t inspection_time_ns)
{
    KAN_UP_VALUE_UPDATE (static_state, render_foundation_material_instance_static_state_t, request_id, &request_id)
    {
        // We always need to insert new texture usages and update material usage.
        // We cannot delete old ones as they might still be used until full reload is complete.

        KAN_UP_SINGLETON_READ (material_singletion, kan_render_material_singleton_t)
        KAN_UP_SINGLETON_READ (texture_singletion, kan_render_texture_singleton_t)
        {
            KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &request_id)
            {
                if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
                {
                    KAN_UP_VALUE_READ (
                        container,
                        KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_material_instance_static_compiled_t),
                        container_id, &request->provided_container_id)
                    {
                        const struct kan_resource_material_instance_static_compiled_t *data =
                            KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_material_instance_static_compiled_t,
                                                                 container);
                        static_state->loading_material_name = data->material;

                        if (data->material != static_state->loaded_material_name)
                        {
                            if (KAN_TYPED_ID_32_IS_VALID (static_state->kept_material_usage_id))
                            {
                                // We already have kept usage, therefore current usage is not loaded and can be changed.
                                KAN_ASSERT (KAN_TYPED_ID_32_IS_VALID (static_state->current_material_usage_id))

                                KAN_UP_VALUE_UPDATE (usage, kan_render_material_usage_t, usage_id,
                                                     &static_state->current_material_usage_id)
                                {
                                    usage->name = data->material;
                                }
                            }
                            else
                            {
                                // We don't have kept usage, we need to keep current usage and create new one for
                                // loading.
                                static_state->kept_material_usage_id = static_state->current_material_usage_id;

                                KAN_UP_INDEXED_INSERT (usage, kan_render_material_usage_t)
                                {
                                    usage->usage_id = kan_next_material_usage_id (material_singletion);
                                    static_state->current_material_usage_id = usage->usage_id;
                                    usage->name = data->material;
                                }
                            }
                        }

                        for (kan_loop_size_t index = 0u; index < data->images.size; ++index)
                        {
                            const struct kan_resource_material_image_t *image =
                                &((struct kan_resource_material_image_t *) data->images.data)[index];
                            kan_bool_t already_here = KAN_FALSE;

                            KAN_UP_VALUE_READ (static_image, render_foundation_material_instance_static_image_t,
                                               static_name, &static_state->name)
                            {
                                if (static_image->texture_name == image->texture)
                                {
                                    already_here = KAN_TRUE;
                                    KAN_UP_QUERY_BREAK;
                                }
                            }

                            if (!already_here)
                            {
                                KAN_UP_INDEXED_INSERT (new_static_image,
                                                       render_foundation_material_instance_static_image_t)
                                {
                                    new_static_image->static_name = static_state->name;
                                    new_static_image->texture_name = image->texture;

                                    KAN_UP_INDEXED_INSERT (usage, kan_render_texture_usage_t)
                                    {
                                        usage->usage_id = kan_next_texture_usage_id (texture_singletion);
                                        new_static_image->usage_id = usage->usage_id;
                                        usage->name = image->texture;
                                        usage->best_advised_mip = static_state->image_best_mip;
                                        usage->worst_advised_mip = static_state->image_worst_mip;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        inspect_material_instance_static (state, static_state, inspection_time_ns);
    }
}

static inline void update_static_state_mips (
    struct render_foundation_material_instance_management_execution_state_t *state)
{
    KAN_UP_SIGNAL_UPDATE (static_state, render_foundation_material_instance_static_state_t, mip_update_needed, KAN_TRUE)
    {
        uint8_t new_best_mip = KAN_INT_MAX (uint8_t);
        uint8_t new_worst_mip = 0u;

        KAN_UP_VALUE_READ (instance_state, render_foundation_material_instance_state_t, static_name,
                           &static_state->name)
        {
            new_best_mip = KAN_MIN (new_best_mip, instance_state->image_best_mip);
            new_worst_mip = KAN_MAX (new_worst_mip, instance_state->image_worst_mip);
        }

        if (new_best_mip != static_state->image_best_mip || new_worst_mip != static_state->image_worst_mip)
        {
            static_state->image_best_mip = new_best_mip;
            static_state->image_worst_mip = new_worst_mip;

            KAN_UP_VALUE_READ (static_image, render_foundation_material_instance_static_image_t, static_name,
                               &static_state->name)
            {
                if (KAN_TYPED_ID_32_IS_VALID (static_image->usage_id))
                {
                    KAN_UP_VALUE_UPDATE (usage, kan_render_texture_usage_t, usage_id, &static_image->usage_id)
                    {
                        usage->best_advised_mip = new_best_mip;
                        usage->worst_advised_mip = new_worst_mip;
                    }
                }
            }
        }

        static_state->mip_update_needed = KAN_FALSE;
    }
}

UNIVERSE_RENDER_FOUNDATION_API void
kan_universe_mutator_execute_render_foundation_material_instance_management_execution (
    kan_cpu_job_t job, struct render_foundation_material_instance_management_execution_state_t *state)
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
    KAN_UP_SINGLETON_WRITE (material_instance_singleton, kan_render_material_instance_singleton_t)
    {
        if (!resource_provider->scan_done)
        {
            KAN_UP_MUTATOR_RETURN;
        }

        struct kan_cpu_section_execution_t section_execution;
        kan_cpu_section_execution_init (&section_execution, state->section_inspect_usage_changes);
        kan_time_size_t inspection_time_ns = kan_precise_time_get_elapsed_nanoseconds ();

        KAN_UP_EVENT_FETCH (on_insert_event, render_foundation_material_instance_usage_on_insert_event_t)
        {
            inspect_material_instance_usages (state, on_insert_event->material_instance_name, inspection_time_ns);
        }

        KAN_UP_EVENT_FETCH (on_change_event, render_foundation_material_instance_usage_on_change_event_t)
        {
            inspect_material_instance_usages (state, on_change_event->old_material_instance_name, inspection_time_ns);
            inspect_material_instance_usages (state, on_change_event->new_material_instance_name, inspection_time_ns);
        }

        KAN_UP_EVENT_FETCH (on_delete_event, render_foundation_material_instance_usage_on_delete_event_t)
        {
            inspect_material_instance_usages (state, on_delete_event->material_instance_name, inspection_time_ns);
        }

        kan_cpu_section_execution_shutdown (&section_execution);
        kan_cpu_section_execution_init (&section_execution, state->section_process_material_updates);
        process_material_updates (state, inspection_time_ns);

        kan_cpu_section_execution_shutdown (&section_execution);
        kan_cpu_section_execution_init (&section_execution, state->section_process_texture_updates);
        process_texture_updates (state, inspection_time_ns);

        kan_cpu_section_execution_shutdown (&section_execution);
        kan_cpu_section_execution_init (&section_execution, state->section_process_loading);

        KAN_UP_EVENT_FETCH (updated_event, kan_resource_request_updated_event_t)
        {
            if (updated_event->type == state->interned_kan_resource_material_instance_compiled_t)
            {
                on_material_instance_updated (state, updated_event->request_id, inspection_time_ns);
            }
            else if (updated_event->type == state->interned_kan_resource_material_instance_static_compiled_t)
            {
                on_material_instance_static_updated (state, updated_event->request_id, inspection_time_ns);
            }
        }

        kan_cpu_section_execution_shutdown (&section_execution);
        material_instance_singleton->custom_sync_inspection_marker_ns = inspection_time_ns;

        kan_cpu_section_execution_init (&section_execution, state->section_update_static_state_mips);
        // Done in the end, as might be affected by loading events.
        update_static_state_mips (state);
        kan_cpu_section_execution_shutdown (&section_execution);
    }

    KAN_UP_MUTATOR_RETURN;
}

KAN_REFLECTION_FUNCTION_META (kan_universe_mutator_execute_render_foundation_material_instance_custom_sync)
UNIVERSE_RENDER_FOUNDATION_API struct kan_universe_mutator_group_meta_t
    render_foundation_material_instance_custom_sync_group_meta = {
        .group_name = KAN_RENDER_FOUNDATION_MATERIAL_INSTANCE_CUSTOM_SYNC_MUTATOR_GROUP,
};

struct render_foundation_material_instance_custom_sync_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (render_foundation_material_instance_custom_sync)
    KAN_UP_BIND_STATE (render_foundation_material_instance_custom_sync, state)
};

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_deploy_render_foundation_material_instance_custom_sync (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct render_foundation_material_instance_custom_sync_state_t *state)
{
    kan_workflow_graph_node_depend_on (workflow_node,
                                       KAN_RENDER_FOUNDATION_MATERIAL_INSTANCE_CUSTOM_SYNC_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node,
                                                KAN_RENDER_FOUNDATION_MATERIAL_INSTANCE_CUSTOM_SYNC_END_CHECKPOINT);
}

static inline void update_usage_custom_parameters (
    struct render_foundation_material_instance_custom_sync_state_t *state,
    kan_render_material_instance_usage_id_t usage_id,
    kan_time_size_t inspection_time_ns)
{
    kan_bool_t has_parameters_now = KAN_FALSE;
    KAN_UP_VALUE_READ (parameter, kan_render_material_instance_custom_instanced_parameter_t, usage_id, &usage_id)
    {
        has_parameters_now = KAN_TRUE;
        KAN_UP_QUERY_BREAK;
    }

    if (has_parameters_now)
    {
        KAN_UP_VALUE_READ (usage, kan_render_material_instance_usage_t, usage_id, &usage_id)
        {
            KAN_UP_VALUE_READ (instance_loaded, kan_render_material_instance_loaded_t, name, &usage->name)
            {
                KAN_UP_VALUE_READ (material_loaded, kan_render_material_loaded_t, name,
                                   &instance_loaded->data.material_name)
                {
                    kan_bool_t existing = KAN_FALSE;
                    KAN_UP_VALUE_UPDATE (custom_loaded, kan_render_material_instance_custom_loaded_t, usage_id,
                                         &usage->usage_id)
                    {
                        existing = KAN_TRUE;
                        custom_loaded->last_inspection_time_ns = inspection_time_ns;
                        UPDATE_MATERIAL_INSTANCE_CUSTOM_LOADED_DATA (material_loaded, instance_loaded, custom_loaded)
                    }

                    if (!existing)
                    {
                        KAN_UP_INDEXED_INSERT (new_custom_loaded, kan_render_material_instance_custom_loaded_t)
                        {
                            new_custom_loaded->usage_id = usage->usage_id;
                            new_custom_loaded->last_inspection_time_ns = inspection_time_ns;
                            UPDATE_MATERIAL_INSTANCE_CUSTOM_LOADED_DATA (material_loaded, instance_loaded,
                                                                         new_custom_loaded)
                        }
                    }
                }
            }
        }
    }
    else
    {
        KAN_UP_VALUE_DELETE (custom_loaded, kan_render_material_instance_custom_loaded_t, usage_id, &usage_id)
        {
            KAN_UP_ACCESS_DELETE (custom_loaded);
        }
    }
}

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_execute_render_foundation_material_instance_custom_sync (
    kan_cpu_job_t job, struct render_foundation_material_instance_custom_sync_state_t *state)
{
    KAN_UP_SINGLETON_READ (material_instance_singleton, kan_render_material_instance_singleton_t)
    {
        KAN_UP_EVENT_FETCH (custom_event, render_foundation_material_instance_custom_on_change_event_t)
        {
            update_usage_custom_parameters (state, custom_event->usage_id,
                                            material_instance_singleton->custom_sync_inspection_marker_ns);
        }
    }

    KAN_UP_MUTATOR_RETURN;
}

void kan_render_material_instance_usage_init (struct kan_render_material_instance_usage_t *instance)
{
    instance->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_render_material_instance_usage_id_t);
    instance->name = NULL;
    instance->image_best_advised_mip = 0u;
    instance->image_worst_advised_mip = KAN_INT_MAX (uint8_t);
}

void kan_render_material_instance_singleton_init (struct kan_render_material_instance_singleton_t *instance)
{
    instance->usage_id_counter = kan_atomic_int_init (1);
    instance->custom_sync_inspection_marker_ns = 0u;
}

void kan_render_material_instance_loaded_data_init (struct kan_render_material_instance_loaded_data_t *instance)
{
    instance->parameter_set = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_t);
    kan_dynamic_array_init (&instance->instanced_data, 0u, sizeof (uint8_t),
                            KAN_RENDER_MATERIAL_INSTANCE_ATTRIBUTE_DATA_ALIGNMENT, kan_allocation_group_stack_get ());
}

void kan_render_material_instance_loaded_data_shutdown (struct kan_render_material_instance_loaded_data_t *instance)
{
    kan_dynamic_array_shutdown (&instance->instanced_data);
}

void kan_render_material_instance_loaded_init (struct kan_render_material_instance_loaded_t *instance)
{
    instance->name = NULL;
    kan_render_material_instance_loaded_data_init (&instance->data);
}

void kan_render_material_instance_loaded_shutdown (struct kan_render_material_instance_loaded_t *instance)
{
    kan_render_material_instance_loaded_data_shutdown (&instance->data);
}

void kan_render_material_instance_custom_loaded_init (struct kan_render_material_instance_custom_loaded_t *instance)
{
    instance->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_render_material_instance_usage_id_t);
    instance->last_inspection_time_ns = KAN_INT_MAX (kan_time_size_t);
    kan_render_material_instance_loaded_data_init (&instance->data);
}

void kan_render_material_instance_custom_loaded_shutdown (struct kan_render_material_instance_custom_loaded_t *instance)
{
    kan_render_material_instance_loaded_data_shutdown (&instance->data);
}
