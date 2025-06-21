#include <qsort.h>

#include <kan/api_common/mute_warnings.h>
#include <kan/context/all_system_names.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/log/logging.h>
#include <kan/precise_time/precise_time.h>
#include <kan/resource_material/resource_material.h>
#include <kan/universe/preprocessor_markup.h>
#include <kan/universe_render_foundation/material.h>
#include <kan/universe_render_foundation/render_graph.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

KAN_LOG_DEFINE_CATEGORY (render_foundation_material);

KAN_REFLECTION_FUNCTION_META (kan_universe_mutator_execute_render_foundation_material_management_planning)
KAN_REFLECTION_FUNCTION_META (kan_universe_mutator_execute_render_foundation_material_management_execution)
UNIVERSE_RENDER_FOUNDATION_API struct kan_universe_mutator_group_meta_t
    render_foundation_material_management_group_meta = {
        .group_name = KAN_RENDER_FOUNDATION_MATERIAL_MANAGEMENT_MUTATOR_GROUP,
};

struct render_foundation_material_usage_on_insert_event_t
{
    kan_interned_string_t material_name;
};

KAN_REFLECTION_STRUCT_META (kan_render_material_usage_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_on_insert_event_t
    render_foundation_material_usage_on_insert_event = {
        .event_type = "render_foundation_material_usage_on_insert_event_t",
        .copy_outs_count = 1u,
        .copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                    .target_path = {.reflection_path_length = 1u,
                                    .reflection_path = (const char *[]) {"material_name"}},
                },
            },
};

struct render_foundation_material_usage_on_change_event_t
{
    kan_interned_string_t old_material_name;
    kan_interned_string_t new_material_name;
};

KAN_REFLECTION_STRUCT_META (kan_render_material_usage_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_on_change_event_t
    render_foundation_material_usage_on_change_event = {
        .event_type = "render_foundation_material_usage_on_change_event_t",
        .observed_fields_count = 1u,
        .observed_fields =
            (struct kan_repository_field_path_t[]) {
                {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
            },
        .unchanged_copy_outs_count = 1u,
        .unchanged_copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                    .target_path = {.reflection_path_length = 1u,
                                    .reflection_path = (const char *[]) {"old_material_name"}},
                },
            },
        .changed_copy_outs_count = 1u,
        .changed_copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                    .target_path = {.reflection_path_length = 1u,
                                    .reflection_path = (const char *[]) {"new_material_name"}},
                },
            },
};

struct render_foundation_material_usage_on_delete_event_t
{
    kan_interned_string_t material_name;
};

KAN_REFLECTION_STRUCT_META (kan_render_material_usage_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_on_delete_event_t
    render_foundation_material_usage_on_delete_event = {
        .event_type = "render_foundation_material_usage_on_delete_event_t",
        .copy_outs_count = 1u,
        .copy_outs =
            (struct kan_repository_copy_out_t[]) {
                {
                    .source_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
                    .target_path = {.reflection_path_length = 1u,
                                    .reflection_path = (const char *[]) {"material_name"}},
                },
            },
};

struct render_foundation_material_state_t
{
    kan_interned_string_t name;
    kan_resource_request_id_t request_id;
    kan_instance_size_t reference_count;
    kan_interned_string_t pipeline_family_name;
    kan_interned_string_t last_loaded_pipeline_family_name;
};

enum render_foundation_material_pass_variant_flags_t
{
    RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_PASSED_TO_LOADED_DATA = 1u << 0u,
    RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_FOUND_IN_NEW_DATA = 1u << 1u,
    RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_HAS_ATTACHED_PIPELINE_STATE = 1u << 2u,
    RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_HAS_ATTACHED_VARIANT_STATE = 1u << 3u,
};

struct render_foundation_material_pass_variant_state_t
{
    kan_interned_string_t material_name;
    kan_interned_string_t pass_name;
    kan_instance_size_t pass_variant_index;
    enum render_foundation_material_pass_variant_flags_t usage_flags;
    kan_interned_string_t pipeline_name;
};

struct render_foundation_pipeline_family_state_t
{
    kan_interned_string_t name;
    kan_resource_request_id_t request_id;
    kan_render_pipeline_parameter_set_layout_t set_material;
    kan_render_pipeline_parameter_set_layout_t set_object;
    kan_render_pipeline_parameter_set_layout_t set_shared;
    kan_instance_size_t reference_count;
    kan_time_size_t inspection_time_ns;
};

UNIVERSE_RENDER_FOUNDATION_API void render_foundation_pipeline_family_state_shutdown (
    struct render_foundation_pipeline_family_state_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->set_material))
    {
        kan_render_pipeline_parameter_set_layout_destroy (instance->set_material);
    }

    if (KAN_HANDLE_IS_VALID (instance->set_object))
    {
        kan_render_pipeline_parameter_set_layout_destroy (instance->set_object);
    }

    if (KAN_HANDLE_IS_VALID (instance->set_shared))
    {
        kan_render_pipeline_parameter_set_layout_destroy (instance->set_shared);
    }
}

/// \brief Pipeline state is used to manage pipeline code loading, it is not bound to any render state,
struct render_foundation_pipeline_state_t
{
    kan_interned_string_t pipeline_name;
    kan_interned_string_t family_name;
    kan_resource_request_id_t request_id;
    kan_instance_size_t reference_count;
};

/// \brief Pipeline pass variant is used to manage instanced render pipelines.
/// \details It is entirely possible to still have `render_foundation_pipeline_pass_variant_state_t`, but do not have
///          `render_foundation_pipeline_state_t` for that pipeline during hot reload. The reason is that pipeline
///          states must directly reflect what needs to be loaded and therefore should be destroyed right away when
///          new data does not reference them. But pass variant states must be kept around until new pipelines are
///          successfully created. Keeping pipeline states like pass variant states would result in errors during hot
///          reload, because pipeline states might reference byproducts that no longer exist in memory unless pipeline
///          state is deleted right away when it is not needed.
struct render_foundation_pipeline_pass_variant_state_t
{
    kan_interned_string_t pipeline_name;
    kan_interned_string_t pass_name;
    kan_instance_size_t pass_variant_index;
    kan_instance_size_t reference_count;
    kan_render_graphics_pipeline_t pipeline;
};

UNIVERSE_RENDER_FOUNDATION_API void render_foundation_pipeline_pass_variant_state_shutdown (
    struct render_foundation_pipeline_pass_variant_state_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->pipeline))
    {
        kan_render_graphics_pipeline_destroy (instance->pipeline);
    }
}

struct render_foundation_material_management_planning_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (render_foundation_material_management_planning)
    KAN_UP_BIND_STATE (render_foundation_material_management_planning, state)

    kan_interned_string_t interned_kan_resource_material_pipeline_family_compiled_t;
    kan_interned_string_t interned_kan_resource_material_pipeline_compiled_t;
    kan_interned_string_t interned_kan_resource_material_compiled_t;
    kan_interned_string_t interned_kan_resource_material_t;

    kan_bool_t preload_materials;
};

UNIVERSE_RENDER_FOUNDATION_API void render_foundation_material_management_planning_state_init (
    struct render_foundation_material_management_planning_state_t *instance)
{
    instance->interned_kan_resource_material_pipeline_family_compiled_t =
        kan_string_intern ("kan_resource_material_pipeline_family_compiled_t");
    instance->interned_kan_resource_material_pipeline_compiled_t =
        kan_string_intern ("kan_resource_material_pipeline_compiled_t");
    instance->interned_kan_resource_material_compiled_t = kan_string_intern ("kan_resource_material_compiled_t");
    instance->interned_kan_resource_material_t = kan_string_intern ("kan_resource_material_t");
}

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_deploy_render_foundation_material_management_planning (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct render_foundation_material_management_planning_state_t *state)
{
    const struct kan_render_material_configuration_t *configuration = kan_universe_world_query_configuration (
        world, kan_string_intern (KAN_RENDER_FOUNDATION_MATERIAL_MANAGEMENT_CONFIGURATION));

    KAN_ASSERT (configuration)
    state->preload_materials = configuration->preload_materials;

    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_MATERIAL_MANAGEMENT_BEGIN_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RESOURCE_PROVIDER_BEGIN_CHECKPOINT);
}

static inline void create_material_state (struct render_foundation_material_management_planning_state_t *state,
                                          const struct kan_resource_provider_singleton_t *resource_provider,
                                          kan_interned_string_t material_name,
                                          kan_instance_size_t initial_references)
{
    KAN_UP_INDEXED_INSERT (new_state, render_foundation_material_state_t)
    {
        new_state->name = material_name;
        new_state->reference_count = initial_references;
        new_state->pipeline_family_name = NULL;
        new_state->last_loaded_pipeline_family_name = NULL;

        KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (resource_provider);
            new_state->request_id = request->request_id;

            request->name = material_name;
            request->type = state->interned_kan_resource_material_compiled_t;
            request->priority = KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_INFO_PRIORITY;
        }
    }
}

static inline void update_loaded_compilation_priority (
    struct render_foundation_material_management_planning_state_t *state,
    kan_interned_string_t material_name,
    enum kan_render_pipeline_compilation_priority_t priority)
{
    KAN_ASSERT (state->preload_materials)
    KAN_UP_VALUE_READ (loaded, kan_render_material_loaded_t, name, &material_name)
    {
        for (kan_loop_size_t index = 0u; index < loaded->pipelines.size; ++index)
        {
            struct kan_render_material_loaded_pipeline_t *pipeline =
                &((struct kan_render_material_loaded_pipeline_t *) loaded->pipelines.data)[index];
            kan_render_graphics_pipeline_change_compilation_priority (pipeline->pipeline, priority);
        }
    }
}

static void create_new_usage_state_if_needed (struct render_foundation_material_management_planning_state_t *state,
                                              const struct kan_resource_provider_singleton_t *resource_provider,
                                              kan_interned_string_t material_name)
{
    KAN_UP_VALUE_UPDATE (referencer_state, render_foundation_material_state_t, name, &material_name)
    {
        ++referencer_state->reference_count;
        if (referencer_state->reference_count == 1u)
        {
            // We've switched material from preload mode to used mode. We need to update pipeline compilation priority.
            update_loaded_compilation_priority (state, material_name, KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_ACTIVE);
        }

        return;
    }

    create_material_state (state, resource_provider, material_name, 1u);
}

#define MATERIAL_HELPER_DETACH_MATERIAL_PASS_VARIANT(VARIABLE_NAME)                                                    \
    if (VARIABLE_NAME->usage_flags & RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_HAS_ATTACHED_VARIANT_STATE)               \
    {                                                                                                                  \
        VARIABLE_NAME->usage_flags &= ~RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_HAS_ATTACHED_VARIANT_STATE;             \
        KAN_UP_VALUE_WRITE (pipeline_pass, render_foundation_pipeline_pass_variant_state_t, pipeline_name,             \
                            &VARIABLE_NAME->pipeline_name)                                                             \
        {                                                                                                              \
            if (pipeline_pass->pass_name == VARIABLE_NAME->pass_name &&                                                \
                pipeline_pass->pass_variant_index == VARIABLE_NAME->pass_variant_index)                                \
            {                                                                                                          \
                KAN_ASSERT (pipeline_pass->reference_count > 0u)                                                       \
                --pipeline_pass->reference_count;                                                                      \
                                                                                                                       \
                if (pipeline_pass->reference_count == 0u)                                                              \
                {                                                                                                      \
                    KAN_UP_ACCESS_DELETE (pipeline_pass);                                                              \
                }                                                                                                      \
            }                                                                                                          \
        }                                                                                                              \
    }

#define MATERIAL_HELPER_DETACH_MATERIAL_PASS_PIPELINE(VARIABLE_NAME)                                                   \
    if (VARIABLE_NAME->usage_flags & RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_HAS_ATTACHED_PIPELINE_STATE)              \
    {                                                                                                                  \
        VARIABLE_NAME->usage_flags &= ~RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_HAS_ATTACHED_PIPELINE_STATE;            \
        KAN_UP_VALUE_WRITE (pipeline, render_foundation_pipeline_state_t, pipeline_name,                               \
                            &VARIABLE_NAME->pipeline_name)                                                             \
        {                                                                                                              \
            KAN_ASSERT (pipeline->reference_count > 0u)                                                                \
            --pipeline->reference_count;                                                                               \
                                                                                                                       \
            if (pipeline->reference_count == 0u)                                                                       \
            {                                                                                                          \
                if (KAN_TYPED_ID_32_IS_VALID (pipeline->request_id))                                                   \
                {                                                                                                      \
                    KAN_UP_EVENT_INSERT (delete_event, kan_resource_request_defer_delete_event_t)                      \
                    {                                                                                                  \
                        delete_event->request_id = pipeline->request_id;                                               \
                    }                                                                                                  \
                }                                                                                                      \
                                                                                                                       \
                KAN_UP_ACCESS_DELETE (pipeline);                                                                       \
            }                                                                                                          \
        }                                                                                                              \
    }

#define MATERIAL_HELPER_DETACH_FAMILY                                                                                  \
    KAN_ASSERT (family_state->reference_count > 0u)                                                                    \
    --family_state->reference_count;                                                                                   \
                                                                                                                       \
    if (family_state->reference_count == 0u)                                                                           \
    {                                                                                                                  \
        if (KAN_TYPED_ID_32_IS_VALID (family_state->request_id))                                                       \
        {                                                                                                              \
            KAN_UP_EVENT_INSERT (delete_event, kan_resource_request_defer_delete_event_t)                              \
            {                                                                                                          \
                delete_event->request_id = family_state->request_id;                                                   \
            }                                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        KAN_UP_ACCESS_DELETE (family_state);                                                                           \
    }

static void destroy_old_usage_state_if_not_referenced (
    struct render_foundation_material_management_planning_state_t *state, kan_interned_string_t material_name)
{
    KAN_UP_VALUE_WRITE (material, render_foundation_material_state_t, name, &material_name)
    {
        KAN_ASSERT (material->reference_count > 0u)
        --material->reference_count;
        kan_bool_t should_delete = material->reference_count == 0u;

        if (state->preload_materials && should_delete)
        {
            kan_bool_t still_exists_as_a_resource = KAN_FALSE;
            KAN_UP_VALUE_READ (native_entry, kan_resource_native_entry_t, name, &material_name)
            {
                if (native_entry->type == state->interned_kan_resource_material_compiled_t ||
                    native_entry->type == state->interned_kan_resource_material_t)
                {
                    still_exists_as_a_resource = KAN_TRUE;
                    break;
                }
            }

            should_delete = !still_exists_as_a_resource;
            if (!should_delete)
            {
                // We've switched material from used mode to preload. We need to update pipeline compilation priority.
                update_loaded_compilation_priority (state, material_name,
                                                    KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CACHE);
            }
        }

        if (!should_delete)
        {
            return;
        }

        if (KAN_TYPED_ID_32_IS_VALID (material->request_id))
        {
            KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
            {
                event->request_id = material->request_id;
            }
        }

        KAN_UP_VALUE_DELETE (loaded, kan_render_material_loaded_t, name, &material->name)
        {
            KAN_UP_ACCESS_DELETE (loaded);
        }

        KAN_UP_VALUE_WRITE (material_pass, render_foundation_material_pass_variant_state_t, material_name,
                            &material->name)
        {
            MATERIAL_HELPER_DETACH_MATERIAL_PASS_PIPELINE (material_pass)
            MATERIAL_HELPER_DETACH_MATERIAL_PASS_VARIANT (material_pass)
            KAN_UP_ACCESS_DELETE (material_pass);
        }

        KAN_UP_VALUE_WRITE (family_state, render_foundation_pipeline_family_state_t, name,
                            &material->pipeline_family_name)
        {
            MATERIAL_HELPER_DETACH_FAMILY;
        }

        KAN_UP_ACCESS_DELETE (material);
    }
}

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_execute_render_foundation_material_management_planning (
    kan_cpu_job_t job, struct render_foundation_material_management_planning_state_t *state)
{
    KAN_UP_MUTATOR_RELEASE_JOB_ON_RETURN
    KAN_UP_SINGLETON_READ (resource_provider, kan_resource_provider_singleton_t)

    if (!resource_provider->scan_done)
    {
        return;
    }

    // This mutator only processes changes that result in new request insertion or in deferred request deletion.

    KAN_UP_EVENT_FETCH (native_entry_event, kan_resource_native_entry_on_insert_event_t)
    {
        if (state->preload_materials)
        {
            if (native_entry_event->type == state->interned_kan_resource_material_compiled_t ||
                native_entry_event->type == state->interned_kan_resource_material_t)
            {
                kan_bool_t already_has_state = KAN_FALSE;
                KAN_UP_VALUE_READ (referencer_state, render_foundation_material_state_t, name,
                                   &native_entry_event->name)
                {
                    already_has_state = KAN_TRUE;
                }

                if (!already_has_state)
                {
                    // Zero references, as it is unreferenced preload state.
                    create_material_state (state, resource_provider, native_entry_event->name, 0u);
                }
            }
        }
    }

    // We ignore native entry removal for several reasons:
    // - Entry removal only happens in development and preload is designed for packaged only.
    // - Material can be still be used somewhere and we shouldn't unload it until it is properly replaced by user.

    KAN_UP_EVENT_FETCH (on_insert_event, render_foundation_material_usage_on_insert_event_t)
    {
        create_new_usage_state_if_needed (state, resource_provider, on_insert_event->material_name);
    }

    KAN_UP_EVENT_FETCH (on_change_event, render_foundation_material_usage_on_change_event_t)
    {
        if (on_change_event->new_material_name != on_change_event->old_material_name)
        {
            create_new_usage_state_if_needed (state, resource_provider, on_change_event->new_material_name);
            destroy_old_usage_state_if_not_referenced (state, on_change_event->old_material_name);
        }
    }

    KAN_UP_EVENT_FETCH (on_delete_event, render_foundation_material_usage_on_delete_event_t)
    {
        destroy_old_usage_state_if_not_referenced (state, on_delete_event->material_name);
    }

    KAN_UP_SIGNAL_UPDATE (pipeline_family, render_foundation_pipeline_family_state_t, request_id,
                          KAN_TYPED_ID_32_INVALID_LITERAL)
    {
        KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (resource_provider);
            pipeline_family->request_id = request->request_id;
            request->name = pipeline_family->name;
            request->type = state->interned_kan_resource_material_pipeline_family_compiled_t;
            request->priority = KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_DATA_PRIORITY;
        }
    }

    KAN_UP_SIGNAL_UPDATE (pipeline, render_foundation_pipeline_state_t, request_id, KAN_TYPED_ID_32_INVALID_LITERAL)
    {
        KAN_UP_INDEXED_INSERT (request, kan_resource_request_t)
        {
            request->request_id = kan_next_resource_request_id (resource_provider);
            pipeline->request_id = request->request_id;
            request->name = pipeline->pipeline_name;
            request->type = state->interned_kan_resource_material_pipeline_compiled_t;
            request->priority = KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_DATA_PRIORITY;
        }
    }
}

struct render_foundation_material_management_execution_state_t
{
    KAN_UP_GENERATE_STATE_QUERIES (render_foundation_material_management_execution)
    KAN_UP_BIND_STATE (render_foundation_material_management_execution, state)

    kan_context_system_t render_backend_system;

    kan_interned_string_t interned_kan_resource_material_pipeline_family_compiled_t;
    kan_interned_string_t interned_kan_resource_material_pipeline_compiled_t;
    kan_interned_string_t interned_kan_resource_material_compiled_t;

    kan_bool_t preload_materials;

    kan_allocation_group_t description_allocation_group;
};

UNIVERSE_RENDER_FOUNDATION_API void render_foundation_material_management_execution_state_init (
    struct render_foundation_material_management_execution_state_t *instance)
{
    instance->interned_kan_resource_material_pipeline_family_compiled_t =
        kan_string_intern ("kan_resource_material_pipeline_family_compiled_t");
    instance->interned_kan_resource_material_pipeline_compiled_t =
        kan_string_intern ("kan_resource_material_pipeline_compiled_t");
    instance->interned_kan_resource_material_compiled_t = kan_string_intern ("kan_resource_material_compiled_t");

    instance->description_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_stack_get (), "description");
}

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_deploy_render_foundation_material_management_execution (
    kan_universe_t universe,
    kan_universe_world_t world,
    kan_repository_t world_repository,
    kan_workflow_graph_node_t workflow_node,
    struct render_foundation_material_management_execution_state_t *state)
{
    const struct kan_render_material_configuration_t *configuration = kan_universe_world_query_configuration (
        world, kan_string_intern (KAN_RENDER_FOUNDATION_MATERIAL_MANAGEMENT_CONFIGURATION));

    KAN_ASSERT (configuration)
    state->preload_materials = configuration->preload_materials;

    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_FRAME_END);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_PASS_MANAGEMENT_END_CHECKPOINT);

    kan_workflow_graph_node_make_dependency_of (workflow_node,
                                                KAN_RENDER_FOUNDATION_MATERIAL_MANAGEMENT_END_CHECKPOINT);

    state->render_backend_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME);
}

static void add_attributes_from_source (const struct kan_rpl_meta_attribute_source_t *source,
                                        struct kan_render_attribute_description_t *attributes,
                                        kan_instance_size_t *attribute_output_index_pointer)
{
    for (kan_loop_size_t attribute_index = 0u; attribute_index < source->attributes.size;
         ++attribute_index, ++*attribute_output_index_pointer)
    {
        struct kan_rpl_meta_attribute_t *attribute =
            &((struct kan_rpl_meta_attribute_t *) source->attributes.data)[attribute_index];

        enum kan_render_attribute_class_t class = KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_1;
        enum kan_render_attribute_item_format_t format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_32;

        switch (attribute->class)
        {
        case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_1:
            class = KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_1;
            break;

        case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_2:
            class = KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_2;
            break;

        case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_3:
            class = KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_3;
            break;

        case KAN_RPL_META_ATTRIBUTE_CLASS_VECTOR_4:
            class = KAN_RENDER_ATTRIBUTE_CLASS_VECTOR_4;
            break;

        case KAN_RPL_META_ATTRIBUTE_CLASS_MATRIX_3X3:
            class = KAN_RENDER_ATTRIBUTE_CLASS_MATRIX_3_3;
            break;

        case KAN_RPL_META_ATTRIBUTE_CLASS_MATRIX_4X4:
            class = KAN_RENDER_ATTRIBUTE_CLASS_MATRIX_4_4;
            break;
        }

        switch (attribute->item_format)
        {
        case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_16:
            format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_16;
            break;

        case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_FLOAT_32:
            format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_32;
            break;

        case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_8:
            format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_UNORM_8;
            break;

        case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UNORM_16:
            format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_UNORM_16;
            break;

        case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_8:
            format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_SNORM_8;
            break;

        case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SNORM_16:
            format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_SNORM_16;
            break;

        case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_8:
            format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_UINT_8;
            break;

        case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_16:
            format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_UINT_16;
            break;

        case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_UINT_32:
            format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_UINT_32;
            break;

        case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_8:
            format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_SINT_8;
            break;

        case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_16:
            format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_SINT_16;
            break;

        case KAN_RPL_META_ATTRIBUTE_ITEM_FORMAT_SINT_32:
            format = KAN_RENDER_ATTRIBUTE_ITEM_FORMAT_FLOAT_SINT_32;
            break;
        }

        attributes[*attribute_output_index_pointer] = (struct kan_render_attribute_description_t) {
            .binding = source->binding,
            .location = attribute->location,
            .offset = attribute->offset,
            .class = class,
            .item_format = format,
        };
    }
}

static inline enum kan_render_compare_operation_t convert_compare_operation (enum kan_rpl_compare_operation_t operation)
{
    switch (operation)
    {
    case KAN_RPL_COMPARE_OPERATION_NEVER:
        return KAN_RENDER_COMPARE_OPERATION_NEVER;

    case KAN_RPL_COMPARE_OPERATION_ALWAYS:
        return KAN_RENDER_COMPARE_OPERATION_ALWAYS;

    case KAN_RPL_COMPARE_OPERATION_EQUAL:
        return KAN_RENDER_COMPARE_OPERATION_EQUAL;

    case KAN_RPL_COMPARE_OPERATION_NOT_EQUAL:
        return KAN_RENDER_COMPARE_OPERATION_NOT_EQUAL;

    case KAN_RPL_COMPARE_OPERATION_LESS:
        return KAN_RENDER_COMPARE_OPERATION_LESS;

    case KAN_RPL_COMPARE_OPERATION_LESS_OR_EQUAL:
        return KAN_RENDER_COMPARE_OPERATION_LESS_OR_EQUAL;

    case KAN_RPL_COMPARE_OPERATION_GREATER:
        return KAN_RENDER_COMPARE_OPERATION_GREATER;

    case KAN_RPL_COMPARE_OPERATION_GREATER_OR_EQUAL:
        return KAN_RENDER_COMPARE_OPERATION_GREATER_OR_EQUAL;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_RENDER_COMPARE_OPERATION_NEVER;
}

static inline enum kan_render_stencil_operation_t convert_stencil_operation (enum kan_rpl_stencil_operation_t operation)
{
    switch (operation)
    {
    case KAN_RPL_STENCIL_OPERATION_KEEP:
        return KAN_RENDER_STENCIL_OPERATION_KEEP;

    case KAN_RPL_STENCIL_OPERATION_ZERO:
        return KAN_RENDER_STENCIL_OPERATION_ZERO;

    case KAN_RPL_STENCIL_OPERATION_REPLACE:
        return KAN_RENDER_STENCIL_OPERATION_REPLACE;

    case KAN_RPL_STENCIL_OPERATION_INCREMENT_AND_CLAMP:
        return KAN_RENDER_STENCIL_OPERATION_INCREMENT_AND_CLAMP;

    case KAN_RPL_STENCIL_OPERATION_DECREMENT_AND_CLAMP:
        return KAN_RENDER_STENCIL_OPERATION_DECREMENT_AND_CLAMP;

    case KAN_RPL_STENCIL_OPERATION_INVERT:
        return KAN_RENDER_STENCIL_OPERATION_INVERT;

    case KAN_RPL_STENCIL_OPERATION_INCREMENT_AND_WRAP:
        return KAN_RENDER_STENCIL_OPERATION_INCREMENT_AND_WRAP;

    case KAN_RPL_STENCIL_OPERATION_DECREMENT_AND_WRAP:
        return KAN_RENDER_STENCIL_OPERATION_DECREMENT_AND_WRAP;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_RENDER_STENCIL_OPERATION_KEEP;
}

static inline enum kan_render_blend_factor_t convert_blend_factor (enum kan_rpl_blend_factor_t blend_factor)
{
    switch (blend_factor)
    {
    case KAN_RPL_BLEND_FACTOR_ZERO:
        return KAN_RENDER_BLEND_FACTOR_ZERO;

    case KAN_RPL_BLEND_FACTOR_ONE:
        return KAN_RENDER_BLEND_FACTOR_ONE;

    case KAN_RPL_BLEND_FACTOR_SOURCE_COLOR:
        return KAN_RENDER_BLEND_FACTOR_SOURCE_COLOR;

    case KAN_RPL_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR:
        return KAN_RENDER_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR;

    case KAN_RPL_BLEND_FACTOR_DESTINATION_COLOR:
        return KAN_RENDER_BLEND_FACTOR_DESTINATION_COLOR;

    case KAN_RPL_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR:
        return KAN_RENDER_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR;

    case KAN_RPL_BLEND_FACTOR_SOURCE_ALPHA:
        return KAN_RENDER_BLEND_FACTOR_SOURCE_ALPHA;

    case KAN_RPL_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA:
        return KAN_RENDER_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA;

    case KAN_RPL_BLEND_FACTOR_DESTINATION_ALPHA:
        return KAN_RENDER_BLEND_FACTOR_DESTINATION_ALPHA;

    case KAN_RPL_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA:
        return KAN_RENDER_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA;

    case KAN_RPL_BLEND_FACTOR_CONSTANT_COLOR:
        return KAN_RENDER_BLEND_FACTOR_CONSTANT_COLOR;

    case KAN_RPL_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
        return KAN_RENDER_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;

    case KAN_RPL_BLEND_FACTOR_CONSTANT_ALPHA:
        return KAN_RENDER_BLEND_FACTOR_CONSTANT_ALPHA;

    case KAN_RPL_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
        return KAN_RENDER_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;

    case KAN_RPL_BLEND_FACTOR_SOURCE_ALPHA_SATURATE:
        return KAN_RENDER_BLEND_FACTOR_SOURCE_ALPHA_SATURATE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_RENDER_BLEND_FACTOR_ZERO;
}

static inline enum kan_render_blend_operation_t convert_blend_operation (enum kan_rpl_blend_operation_t blend_operation)
{
    switch (blend_operation)
    {
    case KAN_RPL_BLEND_OPERATION_ADD:
        return KAN_RENDER_BLEND_OPERATION_ADD;

    case KAN_RPL_BLEND_OPERATION_SUBTRACT:
        return KAN_RENDER_BLEND_OPERATION_SUBTRACT;

    case KAN_RPL_BLEND_OPERATION_REVERSE_SUBTRACT:
        return KAN_RENDER_BLEND_OPERATION_REVERSE_SUBTRACT;

    case KAN_RPL_BLEND_OPERATION_MIN:
        return KAN_RENDER_BLEND_OPERATION_MIN;

    case KAN_RPL_BLEND_OPERATION_MAX:
        return KAN_RENDER_BLEND_OPERATION_MAX;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_RENDER_BLEND_OPERATION_ADD;
}

static void recreate_family (struct render_foundation_material_management_execution_state_t *state,
                             struct render_foundation_pipeline_family_state_t *family)
{
    if (KAN_HANDLE_IS_VALID (family->set_material))
    {
        kan_render_pipeline_parameter_set_layout_destroy (family->set_material);
        family->set_material = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);
    }

    if (KAN_HANDLE_IS_VALID (family->set_object))
    {
        kan_render_pipeline_parameter_set_layout_destroy (family->set_object);
        family->set_object = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);
    }

    if (KAN_HANDLE_IS_VALID (family->set_shared))
    {
        kan_render_pipeline_parameter_set_layout_destroy (family->set_shared);
        family->set_shared = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);
    }

    struct kan_render_attribute_source_description_t
        attribute_sources_static[KAN_UNIVERSE_RENDER_FOUNDATION_BINDINGS_MAX_STATIC];
    struct kan_render_attribute_source_description_t *attribute_sources = NULL;
    kan_instance_size_t attributes_sources_count = 0u;
    kan_instance_size_t attributes_count = 0u;

    struct kan_render_attribute_description_t attributes_static[KAN_UNIVERSE_RENDER_FOUNDATION_BINDINGS_MAX_STATIC];
    struct kan_render_attribute_description_t *attributes = NULL;

    kan_instance_size_t push_constant_size = 0u;
    kan_bool_t set_layouts_created = KAN_TRUE;

    KAN_UP_VALUE_READ (family_request, kan_resource_request_t, request_id, &family->request_id)
    {
        KAN_ASSERT (KAN_TYPED_ID_32_IS_VALID (family_request->provided_container_id))
        KAN_UP_VALUE_READ (container,
                           KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_material_pipeline_family_compiled_t),
                           container_id, &family_request->provided_container_id)
        {
            const struct kan_resource_material_pipeline_family_compiled_t *loaded =
                KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_material_pipeline_family_compiled_t, container);

            if (loaded->vertex_attribute_sources.size > 0u || loaded->has_instanced_attribute_source)
            {
                attribute_sources = attribute_sources_static;
                attributes_sources_count = loaded->vertex_attribute_sources.size;

                if (loaded->has_instanced_attribute_source)
                {
                    ++attributes_sources_count;
                }

                if (attributes_sources_count > KAN_UNIVERSE_RENDER_FOUNDATION_BINDINGS_MAX_STATIC)
                {
                    attribute_sources = kan_allocate_general (
                        state->description_allocation_group,
                        sizeof (struct kan_render_attribute_source_description_t) * attributes_sources_count,
                        _Alignof (struct kan_render_attribute_source_description_t));
                }

                for (kan_loop_size_t index = 0u; index < loaded->vertex_attribute_sources.size; ++index)
                {
                    struct kan_rpl_meta_attribute_source_t *source =
                        &((struct kan_rpl_meta_attribute_source_t *) loaded->vertex_attribute_sources.data)[index];
                    attributes_count += source->attributes.size;

                    attribute_sources[index] = (struct kan_render_attribute_source_description_t) {
                        .binding = source->binding,
                        .stride = source->block_size,
                        .rate = KAN_RENDER_ATTRIBUTE_RATE_PER_VERTEX,
                    };
                }

                if (loaded->has_instanced_attribute_source)
                {
                    attributes_count += loaded->instanced_attribute_source.attributes.size;
                    attribute_sources[loaded->vertex_attribute_sources.size] =
                        (struct kan_render_attribute_source_description_t) {
                            .binding = loaded->instanced_attribute_source.binding,
                            .stride = loaded->instanced_attribute_source.block_size,
                            .rate = KAN_RENDER_ATTRIBUTE_RATE_PER_INSTANCE,
                        };
                }
            }

            if (attributes_count > 0u)
            {
                attributes = attributes_static;
                if (attributes_count > KAN_UNIVERSE_RENDER_FOUNDATION_BINDINGS_MAX_STATIC)
                {
                    attributes =
                        kan_allocate_general (state->description_allocation_group,
                                              sizeof (struct kan_render_attribute_description_t) * attributes_count,
                                              _Alignof (struct kan_render_attribute_description_t));
                }

                kan_instance_size_t attribute_output_index = 0u;
                for (kan_loop_size_t source_index = 0u; source_index < loaded->vertex_attribute_sources.size;
                     ++source_index)
                {
                    struct kan_rpl_meta_attribute_source_t *source = &(
                        (struct kan_rpl_meta_attribute_source_t *) loaded->vertex_attribute_sources.data)[source_index];
                    add_attributes_from_source (source, attributes, &attribute_output_index);
                }

                if (loaded->has_instanced_attribute_source)
                {
                    add_attributes_from_source (&loaded->instanced_attribute_source, attributes,
                                                &attribute_output_index);
                }
            }

            push_constant_size = loaded->push_constant_size;
            if (loaded->set_material.buffers.size > 0u || loaded->set_material.samplers.size > 0u ||
                loaded->set_material.images.size > 0u)
            {
                char name_buffer[KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH];
                snprintf (name_buffer, KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH, "%s[set_material]",
                          family->name);

                family->set_material = kan_render_construct_parameter_set_layout_from_meta (
                    kan_render_backend_system_get_render_context (state->render_backend_system), &loaded->set_material,
                    name_buffer, state->description_allocation_group);

                if (!KAN_HANDLE_IS_VALID (family->set_material))
                {
                    KAN_LOG (render_foundation_material, KAN_LOG_ERROR,
                             "Failed to create material set layout for family \"%s\".", family->name)
                    set_layouts_created = KAN_FALSE;
                }
            }

            if (loaded->set_object.buffers.size > 0u || loaded->set_object.samplers.size > 0u ||
                loaded->set_object.images.size > 0u)
            {
                char name_buffer[KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH];
                snprintf (name_buffer, KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH, "%s[set_object]",
                          family->name);

                family->set_object = kan_render_construct_parameter_set_layout_from_meta (
                    kan_render_backend_system_get_render_context (state->render_backend_system), &loaded->set_object,
                    name_buffer, state->description_allocation_group);

                if (!KAN_HANDLE_IS_VALID (family->set_object))
                {
                    KAN_LOG (render_foundation_material, KAN_LOG_ERROR,
                             "Failed to create object set layout for family \"%s\".", family->name)
                    set_layouts_created = KAN_FALSE;
                }
            }

            if (loaded->set_shared.buffers.size > 0u || loaded->set_shared.samplers.size > 0u ||
                loaded->set_shared.images.size > 0u)
            {
                char name_buffer[KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH];
                snprintf (name_buffer, KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH, "%s[set_shared]",
                          family->name);

                family->set_shared = kan_render_construct_parameter_set_layout_from_meta (
                    kan_render_backend_system_get_render_context (state->render_backend_system), &loaded->set_shared,
                    name_buffer, state->description_allocation_group);

                if (!KAN_HANDLE_IS_VALID (family->set_shared))
                {
                    KAN_LOG (render_foundation_material, KAN_LOG_ERROR,
                             "Failed to create shared set layout for family \"%s\".", family->name)
                    set_layouts_created = KAN_FALSE;
                }
            }
        }

        // Request will be put to sleep later after materials copy out family meta.
    }

    KAN_UP_VALUE_READ (pipeline_state, render_foundation_pipeline_state_t, family_name, &family->name)
    {
        KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &pipeline_state->request_id)
        {
            KAN_ASSERT (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
            KAN_UP_VALUE_READ (container,
                               KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_material_pipeline_compiled_t),
                               container_id, &request->provided_container_id)
            {
                const struct kan_resource_material_pipeline_compiled_t *loaded =
                    KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_material_pipeline_compiled_t, container);

                kan_render_code_module_t code_module = KAN_HANDLE_INITIALIZE_INVALID;
                if (kan_render_get_supported_code_format_flags () & (kan_memory_size_t) (1u << loaded->code_format))
                {
                    code_module = kan_render_code_module_create (
                        kan_render_backend_system_get_render_context (state->render_backend_system), loaded->code.size,
                        loaded->code.data, pipeline_state->pipeline_name);

                    if (!KAN_HANDLE_IS_VALID (code_module))
                    {
                        KAN_LOG (render_foundation_material, KAN_LOG_ERROR,
                                 "Cannot build code module for pipeline \"%s\".", pipeline_state->pipeline_name)
                    }
                }
                else
                {
                    KAN_LOG (render_foundation_material, KAN_LOG_ERROR,
                             "Pipeline \"%s\" is compiled to unsupported code format.", pipeline_state->pipeline_name)
                }

                KAN_UP_VALUE_UPDATE (pipeline_pass_variant, render_foundation_pipeline_pass_variant_state_t,
                                     pipeline_name, &pipeline_state->pipeline_name)
                {
                    if (KAN_HANDLE_IS_VALID (pipeline_pass_variant->pipeline))
                    {
                        kan_render_graphics_pipeline_destroy (pipeline_pass_variant->pipeline);
                        pipeline_pass_variant->pipeline = KAN_HANDLE_SET_INVALID (kan_render_graphics_pipeline_t);
                    }

                    if (!set_layouts_created || !KAN_HANDLE_IS_VALID (code_module))
                    {
                        continue;
                    }

                    KAN_UP_VALUE_READ (pass, kan_render_graph_pass_t, name, &pipeline_pass_variant->pass_name)
                    {
                        if (pipeline_pass_variant->pass_variant_index >= pass->variants.size &&
                            // Corner case: variant-less passes that have no pass customization.
                            (pipeline_pass_variant->pass_variant_index != 0u || pass->variants.size != 0u))
                        {
                            KAN_LOG (render_foundation_material, KAN_LOG_ERROR,
                                     "Failed to create pipeline \"%s\" for pass \"%s\" variant %lu -- variant index "
                                     "overflow, internal error?.",
                                     pipeline_pass_variant->pipeline_name, pipeline_pass_variant->pass_name,
                                     (unsigned long) pipeline_pass_variant->pass_variant_index)
                            continue;
                        }

                        enum kan_render_polygon_mode_t polygon_mode = KAN_RENDER_POLYGON_MODE_FILL;
                        switch (loaded->pipeline_settings.polygon_mode)
                        {
                        case KAN_RPL_POLYGON_MODE_FILL:
                            polygon_mode = KAN_RENDER_POLYGON_MODE_FILL;
                            break;

                        case KAN_RPL_POLYGON_MODE_WIREFRAME:
                            polygon_mode = KAN_RENDER_POLYGON_MODE_WIREFRAME;
                            break;
                        }

                        enum kan_render_cull_mode_t cull_mode = KAN_RENDER_CULL_MODE_BACK;
                        switch (loaded->pipeline_settings.cull_mode)
                        {
                        case KAN_RPL_CULL_MODE_NONE:
                            cull_mode = KAN_RENDER_CULL_MODE_NONE;
                            break;

                        case KAN_RPL_CULL_MODE_BACK:
                            cull_mode = KAN_RENDER_CULL_MODE_BACK;
                            break;

                        case KAN_RPL_CULL_MODE_FRONT:
                            cull_mode = KAN_RENDER_CULL_MODE_FRONT;
                            break;
                        }

#define COLOR_OUTPUTS_STATIC_COUNT 4u
#define ENTRY_POINTS_STATIC_COUNT 4u

                        struct kan_render_color_output_setup_description_t
                            color_outputs_static[COLOR_OUTPUTS_STATIC_COUNT];
                        struct kan_render_color_output_setup_description_t *color_outputs = NULL;

                        if (loaded->color_outputs.size > 0u)
                        {
                            color_outputs = color_outputs_static;
                            if (loaded->color_outputs.size > COLOR_OUTPUTS_STATIC_COUNT)
                            {
                                color_outputs = kan_allocate_general (
                                    state->description_allocation_group,
                                    sizeof (struct kan_render_color_output_setup_description_t) *
                                        loaded->color_outputs.size,
                                    _Alignof (struct kan_render_color_output_setup_description_t));
                            }

                            for (kan_loop_size_t index = 0u; index < loaded->color_outputs.size; ++index)
                            {
                                struct kan_rpl_meta_color_output_t *color_output =
                                    &((struct kan_rpl_meta_color_output_t *) loaded->color_outputs.data)[index];

                                color_outputs[index] = (struct kan_render_color_output_setup_description_t) {
                                    .use_blend = color_output->use_blend,
                                    .write_r = color_output->write_r,
                                    .write_g = color_output->write_g,
                                    .write_b = color_output->write_b,
                                    .write_a = color_output->write_a,
                                    .source_color_blend_factor =
                                        convert_blend_factor (color_output->source_color_blend_factor),
                                    .destination_color_blend_factor =
                                        convert_blend_factor (color_output->destination_color_blend_factor),
                                    .color_blend_operation =
                                        convert_blend_operation (color_output->color_blend_operation),
                                    .source_alpha_blend_factor =
                                        convert_blend_factor (color_output->source_alpha_blend_factor),
                                    .destination_alpha_blend_factor =
                                        convert_blend_factor (color_output->destination_alpha_blend_factor),
                                    .alpha_blend_operation =
                                        convert_blend_operation (color_output->alpha_blend_operation),
                                };
                            }
                        }

                        struct kan_render_pipeline_code_entry_point_t entry_points_static[ENTRY_POINTS_STATIC_COUNT];
                        struct kan_render_pipeline_code_module_usage_t code_module_usage = {
                            .code_module = code_module,
                            .entry_points_count = loaded->entry_points.size,
                            .entry_points = NULL,
                        };

                        if (code_module_usage.entry_points_count > 0u)
                        {
                            code_module_usage.entry_points = entry_points_static;
                            if (code_module_usage.entry_points_count > ENTRY_POINTS_STATIC_COUNT)
                            {
                                code_module_usage.entry_points =
                                    kan_allocate_general (state->description_allocation_group,
                                                          sizeof (struct kan_render_pipeline_code_entry_point_t) *
                                                              code_module_usage.entry_points_count,
                                                          _Alignof (struct kan_render_pipeline_code_entry_point_t));
                            }

                            for (kan_loop_size_t index = 0u; index < code_module_usage.entry_points_count; ++index)
                            {
                                struct kan_rpl_entry_point_t *entry_point =
                                    &((struct kan_rpl_entry_point_t *) loaded->entry_points.data)[index];
                                enum kan_render_stage_t stage = KAN_RENDER_STAGE_GRAPHICS_FRAGMENT;

                                switch (entry_point->stage)
                                {
                                case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX:
                                    stage = KAN_RENDER_STAGE_GRAPHICS_VERTEX;
                                    break;

                                case KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT:
                                    stage = KAN_RENDER_STAGE_GRAPHICS_FRAGMENT;
                                    break;
                                }

                                code_module_usage.entry_points[index] =
                                    (struct kan_render_pipeline_code_entry_point_t) {
                                        .stage = stage,
                                        .function_name = entry_point->function_name,
                                    };
                            }
                        }

                        kan_render_pipeline_parameter_set_layout_t pass_parameter_set_layout =
                            KAN_HANDLE_INITIALIZE_INVALID;

                        if (pipeline_pass_variant->pass_variant_index < pass->variants.size)
                        {
                            struct kan_render_graph_pass_variant_t *pass_variant =
                                &((struct kan_render_graph_pass_variant_t *)
                                      pass->variants.data)[pipeline_pass_variant->pass_variant_index];
                            pass_parameter_set_layout = pass_variant->pass_parameter_set_layout;
                        }

                        kan_render_pipeline_parameter_set_layout_t parameter_sets[4u] = {
                            pass_parameter_set_layout,
                            family->set_material,
                            family->set_object,
                            family->set_shared,
                        };

                        struct kan_render_graphics_pipeline_description_t description = {
                            .pass = pass->pass,
                            .topology = KAN_RENDER_GRAPHICS_TOPOLOGY_TRIANGLE_LIST,
                            .attribute_sources_count = attributes_sources_count,
                            .attribute_sources = attribute_sources,
                            .attributes_count = attributes_count,
                            .attributes = attributes,
                            .push_constant_size = push_constant_size,
                            .parameter_set_layouts_count = 4u,
                            .parameter_set_layouts = parameter_sets,

                            .polygon_mode = polygon_mode,
                            .cull_mode = cull_mode,
                            .use_depth_clamp = KAN_FALSE,

                            .output_setups_count = loaded->color_outputs.size,
                            .output_setups = color_outputs,

                            .blend_constant_r = loaded->color_blend_constants.r,
                            .blend_constant_g = loaded->color_blend_constants.g,
                            .blend_constant_b = loaded->color_blend_constants.b,
                            .blend_constant_a = loaded->color_blend_constants.a,

                            .depth_test_enabled = loaded->pipeline_settings.depth_test,
                            .depth_write_enabled = loaded->pipeline_settings.depth_write,
                            .depth_bounds_test_enabled = loaded->pipeline_settings.depth_bounds_test,
                            .depth_compare_operation =
                                convert_compare_operation (loaded->pipeline_settings.depth_compare_operation),
                            .min_depth = loaded->pipeline_settings.depth_min,
                            .max_depth = loaded->pipeline_settings.depth_max,

                            .stencil_test_enabled = loaded->pipeline_settings.stencil_test,
                            .stencil_front =
                                {
                                    .on_fail =
                                        convert_stencil_operation (loaded->pipeline_settings.stencil_front_on_fail),
                                    .on_depth_fail = convert_stencil_operation (
                                        loaded->pipeline_settings.stencil_front_on_depth_fail),
                                    .on_pass =
                                        convert_stencil_operation (loaded->pipeline_settings.stencil_front_on_pass),
                                    .compare =
                                        convert_compare_operation (loaded->pipeline_settings.stencil_front_compare),
                                    .compare_mask = loaded->pipeline_settings.stencil_front_compare_mask,
                                    .write_mask = loaded->pipeline_settings.stencil_front_write_mask,
                                    .reference = loaded->pipeline_settings.stencil_front_reference,
                                },
                            .stencil_back =
                                {
                                    .on_fail =
                                        convert_stencil_operation (loaded->pipeline_settings.stencil_back_on_fail),
                                    .on_depth_fail = convert_stencil_operation (
                                        loaded->pipeline_settings.stencil_back_on_depth_fail),
                                    .on_pass =
                                        convert_stencil_operation (loaded->pipeline_settings.stencil_back_on_pass),
                                    .compare =
                                        convert_compare_operation (loaded->pipeline_settings.stencil_back_compare),
                                    .compare_mask = loaded->pipeline_settings.stencil_back_compare_mask,
                                    .write_mask = loaded->pipeline_settings.stencil_back_write_mask,
                                    .reference = loaded->pipeline_settings.stencil_back_reference,
                                },

                            .code_modules_count = 1u,
                            .code_modules = &code_module_usage,

                            .tracking_name = pipeline_pass_variant->pipeline_name,
                        };

                        pipeline_pass_variant->pipeline = kan_render_graphics_pipeline_create (
                            kan_render_backend_system_get_render_context (state->render_backend_system), &description,
                            // Priority will be automatically changed to active when active material uses it.
                            KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CACHE);

                        if (color_outputs && color_outputs != color_outputs_static)
                        {
                            kan_free_general (state->description_allocation_group, color_outputs,
                                              sizeof (struct kan_render_color_output_setup_description_t) *
                                                  loaded->color_outputs.size);
                        }

                        if (code_module_usage.entry_points && code_module_usage.entry_points != entry_points_static)
                        {
                            kan_free_general (state->description_allocation_group, code_module_usage.entry_points,
                                              sizeof (struct kan_render_pipeline_code_entry_point_t) *
                                                  code_module_usage.entry_points_count);
                        }

#undef COLOR_OUTPUTS_STATIC_COUNT
#undef ENTRY_POINTS_STATIC_COUNT

                        if (!KAN_HANDLE_IS_VALID (pipeline_pass_variant->pipeline))
                        {
                            KAN_LOG (render_foundation_material, KAN_LOG_ERROR,
                                     "Failed to create pipeline \"%s\" for pass \"%s\" variant %lu.",
                                     pipeline_pass_variant->pipeline_name, pipeline_pass_variant->pass_name,
                                     (unsigned long) pipeline_pass_variant->pass_variant_index)
                        }
                    }
                }

                if (!KAN_HANDLE_IS_VALID (code_module))
                {
                    kan_render_code_module_destroy (code_module);
                }

                KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_sleep_event_t)
                {
                    event->request_id = pipeline_state->request_id;
                }
            }
        }
    }

    if (attribute_sources && attribute_sources != attribute_sources_static)
    {
        kan_free_general (state->description_allocation_group, attribute_sources,
                          sizeof (struct kan_render_attribute_source_description_t) * attributes_sources_count);
    }

    if (attributes && attributes != attributes_static)
    {
        kan_free_general (state->description_allocation_group, attributes,
                          sizeof (struct kan_render_attribute_description_t) * attributes_count);
    }
}

static void reload_material_from_family (struct render_foundation_material_management_execution_state_t *state,
                                         struct render_foundation_material_state_t *material,
                                         const struct render_foundation_pipeline_family_state_t *family,
                                         struct kan_render_material_loaded_t *loaded)
{
    KAN_UP_EVENT_INSERT (event, kan_render_material_updated_event_t) { event->name = material->name; }

    loaded->set_material = family->set_material;
    loaded->set_object = family->set_object;
    loaded->set_shared = family->set_shared;
    loaded->pipelines.size = 0u;
    kan_dynamic_array_set_capacity (&loaded->pipelines, KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_PSC);

    KAN_UP_VALUE_WRITE (material_pass, render_foundation_material_pass_variant_state_t, material_name, &material->name)
    {
        if (material_pass->usage_flags & RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_FOUND_IN_NEW_DATA)
        {
            material_pass->usage_flags |= RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_PASSED_TO_LOADED_DATA;
            KAN_UP_VALUE_READ (pipeline_pass_variant, render_foundation_pipeline_pass_variant_state_t, pipeline_name,
                               &material_pass->pipeline_name)
            {
                if (pipeline_pass_variant->pass_name == material_pass->pass_name &&
                    pipeline_pass_variant->pass_variant_index == material_pass->pass_variant_index)
                {
                    if (KAN_HANDLE_IS_VALID (pipeline_pass_variant->pipeline))
                    {
                        struct kan_render_material_loaded_pipeline_t *spot =
                            kan_dynamic_array_add_last (&loaded->pipelines);

                        if (!spot)
                        {
                            kan_dynamic_array_set_capacity (&loaded->pipelines, loaded->pipelines.size * 2u);
                            spot = kan_dynamic_array_add_last (&loaded->pipelines);
                        }

                        spot->pass_name = material_pass->pass_name;
                        spot->variant_index = pipeline_pass_variant->pass_variant_index;
                        spot->pipeline = pipeline_pass_variant->pipeline;

                        if (state->preload_materials && material->reference_count > 0u)
                        {
                            kan_render_graphics_pipeline_change_compilation_priority (
                                spot->pipeline, KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_ACTIVE);
                        }
                    }

                    break;
                }
            }
        }
        else
        {
            // Should not have pipeline state already.
            KAN_ASSERT ((material_pass->usage_flags &
                         RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_HAS_ATTACHED_PIPELINE_STATE) == 0u)

            MATERIAL_HELPER_DETACH_MATERIAL_PASS_VARIANT (material_pass)
            KAN_UP_ACCESS_DELETE (material_pass);
        }
    }

    if (material->pipeline_family_name != material->last_loaded_pipeline_family_name)
    {
        KAN_UP_VALUE_WRITE (family_state, render_foundation_pipeline_family_state_t, name,
                            &material->last_loaded_pipeline_family_name)
        {
            MATERIAL_HELPER_DETACH_FAMILY;
        }
    }

    material->last_loaded_pipeline_family_name = material->pipeline_family_name;
    kan_dynamic_array_set_capacity (&loaded->pipelines, loaded->pipelines.size);

    {
        // Now we need to properly sort the pipelines array for usability.
        struct kan_render_material_loaded_pipeline_t temporary;

        KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#define AT_INDEX(INDEX) (((struct kan_render_material_loaded_pipeline_t *) loaded->pipelines.data)[INDEX])
#define LESS(first_index, second_index)                                                                                \
    __CUSHION_PRESERVE__ (AT_INDEX (first_index).pass_name == AT_INDEX (second_index).pass_name &&                     \
                          AT_INDEX (first_index).variant_index < AT_INDEX (second_index).variant_index) ||             \
        AT_INDEX (first_index).pass_name < AT_INDEX (second_index).pass_name
#define SWAP(first_index, second_index)                                                                                \
    __CUSHION_PRESERVE__                                                                                               \
    temporary = AT_INDEX (first_index), AT_INDEX (first_index) = AT_INDEX (second_index),                              \
    AT_INDEX (second_index) = temporary
        QSORT (loaded->pipelines.size, LESS, SWAP);
#undef LESS
#undef SWAP
#undef AT_INDEX
        KAN_MUTE_THIRD_PARTY_WARNINGS_END
    }

    KAN_UP_VALUE_READ (family_request, kan_resource_request_t, request_id, &family->request_id)
    {
        KAN_ASSERT (KAN_TYPED_ID_32_IS_VALID (family_request->provided_container_id))
        KAN_UP_VALUE_READ (container,
                           KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_material_pipeline_family_compiled_t),
                           container_id, &family_request->provided_container_id)
        {
            const struct kan_resource_material_pipeline_family_compiled_t *family_data =
                KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_material_pipeline_family_compiled_t, container);

            for (kan_loop_size_t index = 0u; index < loaded->vertex_attribute_sources.size; ++index)
            {
                kan_rpl_meta_attribute_source_shutdown (
                    &((struct kan_rpl_meta_attribute_source_t *) loaded->vertex_attribute_sources.data)[index]);
            }

            loaded->vertex_attribute_sources.size = 0u;
            kan_rpl_meta_attribute_source_shutdown (&loaded->instanced_attribute_source);
            kan_rpl_meta_set_bindings_shutdown (&loaded->set_material_bindings);
            kan_rpl_meta_set_bindings_shutdown (&loaded->set_object_bindings);
            kan_rpl_meta_set_bindings_shutdown (&loaded->set_shared_bindings);

            kan_dynamic_array_set_capacity (&loaded->vertex_attribute_sources,
                                            family_data->vertex_attribute_sources.size);

            for (kan_loop_size_t index = 0u; index < family_data->vertex_attribute_sources.size; ++index)
            {
                const struct kan_rpl_meta_attribute_source_t *source =
                    &((struct kan_rpl_meta_attribute_source_t *) family_data->vertex_attribute_sources.data)[index];

                struct kan_rpl_meta_attribute_source_t *target =
                    kan_dynamic_array_add_last (&loaded->vertex_attribute_sources);
                KAN_ASSERT (target)
                kan_rpl_meta_attribute_source_init_copy (target, source);
            }

            loaded->has_instanced_attribute_source = family_data->has_instanced_attribute_source;
            kan_rpl_meta_attribute_source_init_copy (&loaded->instanced_attribute_source,
                                                     &family_data->instanced_attribute_source);

            loaded->push_constant_size = family_data->push_constant_size;
            kan_rpl_meta_set_bindings_init_copy (&loaded->set_material_bindings, &family_data->set_material);
            kan_rpl_meta_set_bindings_init_copy (&loaded->set_object_bindings, &family_data->set_object);
            kan_rpl_meta_set_bindings_init_copy (&loaded->set_shared_bindings, &family_data->set_shared);
            return;
        }
    }

    // If we're here, then something is broken with loaded data.
    KAN_ASSERT (KAN_FALSE)
}

static void inspect_family (struct render_foundation_material_management_execution_state_t *state,
                            struct render_foundation_pipeline_family_state_t *family,
                            kan_time_size_t inspection_time_ns)
{
    if (family->inspection_time_ns == inspection_time_ns)
    {
        // Already inspected this frame, nothing has changed.
        return;
    }

    family->inspection_time_ns = inspection_time_ns;
    kan_bool_t family_loaded = KAN_FALSE;
    kan_bool_t family_request_valid = KAN_FALSE;

    KAN_UP_VALUE_READ (family_request, kan_resource_request_t, request_id, &family->request_id)
    {
        if (family_request->sleeping)
        {
            // If we've got into inspection and family request is sleeping, then some family resources were updated,
            // but family resource was not. We need to recreate request in order to force retrieval of the last
            // proper family resource.
            KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
            {
                event->request_id = family->request_id;
            }
        }
        else
        {
            family_loaded =
                !family_request->expecting_new_data && KAN_TYPED_ID_32_IS_VALID (family_request->provided_container_id);
            family_request_valid = KAN_TRUE;
        }
    }

    if (!family_request_valid)
    {
        family->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
    }

    if (!family_loaded)
    {
        // Family not yet loaded, nothing to do.
        return;
    }

    KAN_UP_VALUE_UPDATE (pipeline, render_foundation_pipeline_state_t, family_name, &family->name)
    {
        kan_bool_t pipeline_loaded = KAN_FALSE;
        kan_bool_t pipeline_request_valid = KAN_FALSE;

        KAN_UP_VALUE_READ (pipeline_request, kan_resource_request_t, request_id, &pipeline->request_id)
        {
            if (pipeline_request->sleeping)
            {
                // If we've got into inspection and pipeline request is sleeping, then some family resources were
                // updated, but pipeline resource was not. We need to recreate request in order to force retrieval of
                // the last proper family resource.
                KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
                {
                    event->request_id = pipeline->request_id;
                }
            }
            else
            {
                pipeline_loaded = !pipeline_request->expecting_new_data &&
                                  KAN_TYPED_ID_32_IS_VALID (pipeline_request->provided_container_id);
                pipeline_request_valid = KAN_TRUE;
            }
        }

        if (!pipeline_request_valid)
        {
            pipeline->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
        }

        if (!pipeline_loaded)
        {
            // Pipeline not yet loaded, cannot reload full family and its materials until every pipeline is loaded.
            return;
        }
    }

    recreate_family (state, family);
    KAN_UP_VALUE_UPDATE (material, render_foundation_material_state_t, pipeline_family_name, &family->name)
    {
        kan_bool_t updated = KAN_FALSE;
        KAN_UP_VALUE_UPDATE (loaded, kan_render_material_loaded_t, name, &material->name)
        {
            reload_material_from_family (state, material, family, loaded);
            updated = KAN_TRUE;
        }

        if (!updated)
        {
            KAN_UP_INDEXED_INSERT (new_loaded, kan_render_material_loaded_t)
            {
                new_loaded->name = material->name;
                reload_material_from_family (state, material, family, new_loaded);
            }
        }
    }

    KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_sleep_event_t) { event->request_id = family->request_id; }
}

static inline void on_pipeline_family_request_updated (
    struct render_foundation_material_management_execution_state_t *state,
    kan_resource_request_id_t request_id,
    kan_time_size_t inspection_time_ns)
{
    KAN_UP_VALUE_UPDATE (family, render_foundation_pipeline_family_state_t, request_id, &request_id)
    {
        inspect_family (state, family, inspection_time_ns);
    }
}

static inline void on_pipeline_request_updated (struct render_foundation_material_management_execution_state_t *state,
                                                kan_resource_request_id_t request_id,
                                                kan_time_size_t inspection_time_ns)
{
    kan_interned_string_t family_name = NULL;
    KAN_UP_VALUE_READ (pipeline_state, render_foundation_pipeline_state_t, request_id, &request_id)
    {
        family_name = pipeline_state->family_name;
    }

    KAN_UP_VALUE_UPDATE (family, render_foundation_pipeline_family_state_t, name, &family_name)
    {
        inspect_family (state, family, inspection_time_ns);
    }
}

static inline void create_material_pass_variant_attachments (
    struct render_foundation_material_management_execution_state_t *state,
    const struct render_foundation_material_state_t *material,
    struct render_foundation_material_pass_variant_state_t *material_pass)
{
    KAN_ASSERT ((material_pass->usage_flags & (RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_HAS_ATTACHED_PIPELINE_STATE |
                                               RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_HAS_ATTACHED_VARIANT_STATE)) ==
                0u)

    material_pass->usage_flags |= RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_HAS_ATTACHED_PIPELINE_STATE |
                                  RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_HAS_ATTACHED_VARIANT_STATE;
    kan_bool_t pipeline_pass_exists = KAN_FALSE;

    KAN_UP_VALUE_UPDATE (pipeline_pass, render_foundation_pipeline_pass_variant_state_t, pipeline_name,
                         &material_pass->pipeline_name)
    {
        if (pipeline_pass->pass_name == material_pass->pass_name &&
            pipeline_pass->pass_variant_index == material_pass->pass_variant_index)
        {
            ++pipeline_pass->reference_count;
            pipeline_pass_exists = KAN_TRUE;
            break;
        }
    }

    if (!pipeline_pass_exists)
    {
        KAN_UP_INDEXED_INSERT (new_pipeline_pass, render_foundation_pipeline_pass_variant_state_t)
        {
            new_pipeline_pass->pipeline_name = material_pass->pipeline_name;
            new_pipeline_pass->pass_name = material_pass->pass_name;
            new_pipeline_pass->reference_count = 1u;
            new_pipeline_pass->pass_variant_index = material_pass->pass_variant_index;
            new_pipeline_pass->pipeline = KAN_HANDLE_SET_INVALID (kan_render_graphics_pipeline_t);
        }
    }

    kan_bool_t pipeline_exists = KAN_FALSE;
    KAN_UP_VALUE_UPDATE (pipeline, render_foundation_pipeline_state_t, pipeline_name, &material_pass->pipeline_name)
    {
        if (!pipeline_pass_exists)
        {
            // Request pipeline bytecode to be loaded again as we need to compile new pass.
            pipeline->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
        }

        ++pipeline->reference_count;
        pipeline_exists = KAN_TRUE;
    }

    if (!pipeline_exists)
    {
        KAN_ASSERT (!pipeline_pass_exists)
        KAN_UP_INDEXED_INSERT (new_pipeline, render_foundation_pipeline_state_t)
        {
            new_pipeline->pipeline_name = material_pass->pipeline_name;
            new_pipeline->family_name = material->pipeline_family_name;
            new_pipeline->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
            new_pipeline->reference_count = 1u;
        }
    }
}

static inline void on_material_updated (struct render_foundation_material_management_execution_state_t *state,
                                        kan_resource_request_id_t request_id)
{
    KAN_UP_VALUE_UPDATE (material, render_foundation_material_state_t, request_id, &request_id)
    {
        KAN_UP_VALUE_READ (request, kan_resource_request_t, request_id, &request_id)
        {
            if (KAN_TYPED_ID_32_IS_VALID (request->provided_container_id))
            {
                KAN_UP_VALUE_READ (container,
                                   KAN_RESOURCE_PROVIDER_MAKE_CONTAINER_TYPE (kan_resource_material_compiled_t),
                                   container_id, &request->provided_container_id)
                {
                    const struct kan_resource_material_compiled_t *material_data =
                        KAN_RESOURCE_PROVIDER_CONTAINER_GET (kan_resource_material_compiled_t, container);

                    // We cannot actually put material data to sleep, because putting it to sleep means that all
                    // byproducts that are not referenced as resources or by runtime compilation would be deleted
                    // right away. We reference them, so it kind of seems normal and expected.
                    //
                    // But there is a corner case: byproducts for passes that are not loaded right now, for example
                    // due to compilation error, would be deleted due to sleep. But material should be recompiled with
                    // pass change, right? Yes, but material and pass compilation would most certainly happen on frame
                    // N and then pass variant compilation would happen on frame N + 1, because pass variant is not yet
                    // referenced right after its compilation. Therefore, pass would be available on frame N + 1, but
                    // material callbacks would already finish working and defer sleep on frame N. So, on frame N + 1
                    // we would not have the byproducts to enable the pass.
                    //
                    // Thankfully, material and its raw byproducts are pretty lightweight, therefore we do not lose
                    // much memory here.

                    if (material_data->pipeline_family != material->pipeline_family_name)
                    {
                        if (material->pipeline_family_name != material->last_loaded_pipeline_family_name)
                        {
                            // Current pipeline family is not referenced in loaded, delete reference right away.
                            KAN_UP_VALUE_WRITE (family_state, render_foundation_pipeline_family_state_t, name,
                                                &material->pipeline_family_name)
                            {
                                MATERIAL_HELPER_DETACH_FAMILY;
                            }
                        }

                        material->pipeline_family_name = material_data->pipeline_family;
                        if (material->pipeline_family_name != material->last_loaded_pipeline_family_name)
                        {
                            // New family is different from loaded. Reference it or create it.
                            kan_bool_t new_family_exists = KAN_FALSE;
                            KAN_UP_VALUE_UPDATE (family_to_add_reference, render_foundation_pipeline_family_state_t,
                                                 name, &material_data->pipeline_family)
                            {
                                ++family_to_add_reference->reference_count;
                                // We need to reset family meta loading because
                                // we'll need meta resource in for loaded material data.
                                family_to_add_reference->request_id =
                                    KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
                                new_family_exists = KAN_TRUE;
                            }

                            if (!new_family_exists)
                            {
                                KAN_UP_INDEXED_INSERT (new_family, render_foundation_pipeline_family_state_t)
                                {
                                    new_family->name = material_data->pipeline_family;
                                    new_family->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
                                    new_family->set_material =
                                        KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);
                                    new_family->set_object =
                                        KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);
                                    new_family->set_shared =
                                        KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);
                                    new_family->reference_count = 1u;
                                    new_family->inspection_time_ns = 0u;
                                }
                            }
                        }
                    }

                    // Clear new data flag from all existent pass variants.
                    KAN_UP_VALUE_UPDATE (material_pass_to_unmark, render_foundation_material_pass_variant_state_t,
                                         material_name, &material->name)
                    {
                        material_pass_to_unmark->usage_flags &=
                            ~RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_FOUND_IN_NEW_DATA;
                    }

                    // Mark pass variants that exists in new data or create new ones if they do no exist.
                    for (kan_loop_size_t variant_index = 0u; variant_index < material_data->pass_variants.size;
                         ++variant_index)
                    {
                        const struct kan_resource_material_pass_variant_compiled_t *variant =
                            &((struct kan_resource_material_pass_variant_compiled_t *)
                                  material_data->pass_variants.data)[variant_index];

                        kan_bool_t found = KAN_FALSE;
                        KAN_UP_VALUE_UPDATE (material_pass_to_check, render_foundation_material_pass_variant_state_t,
                                             material_name, &material->name)
                        {
                            if (material_pass_to_check->pass_name == variant->name &&
                                material_pass_to_check->pass_variant_index == variant->variant_index &&
                                material_pass_to_check->pipeline_name == variant->pipeline)
                            {
                                material_pass_to_check->usage_flags |=
                                    RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_FOUND_IN_NEW_DATA;
                                found = KAN_TRUE;
                                break;
                            }
                        }

                        if (!found)
                        {
                            KAN_UP_INDEXED_INSERT (new_material_pass, render_foundation_material_pass_variant_state_t)
                            {
                                new_material_pass->material_name = material->name;
                                new_material_pass->pass_name = variant->name;
                                new_material_pass->pass_variant_index = variant->variant_index;
                                new_material_pass->usage_flags =
                                    RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_FOUND_IN_NEW_DATA;
                                new_material_pass->pipeline_name = variant->pipeline;

                                // We instantiate pipeline and pipeline pass only
                                // if pass is available in current context.
                                KAN_UP_VALUE_READ (pass, kan_render_graph_pass_t, name, &variant->name)
                                {
                                    create_material_pass_variant_attachments (state, material, new_material_pass);
                                }
                            }
                        }
                    }

                    // Process pass attachments according to their flags.
                    const enum render_foundation_material_pass_variant_flags_t flags_mask =
                        RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_FOUND_IN_NEW_DATA |
                        RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_PASSED_TO_LOADED_DATA;

                    KAN_UP_VALUE_WRITE (material_pass_to_check, render_foundation_material_pass_variant_state_t,
                                        material_name, &material->name)
                    {
                        switch (material_pass_to_check->usage_flags & flags_mask)
                        {
                        case RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_FOUND_IN_NEW_DATA:
                            // Only used in new data, nothing to do.
                            break;

                        case RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_PASSED_TO_LOADED_DATA:
                        {
                            // Only used in loaded data, should detach pipeline state unless it is already detached.
                            MATERIAL_HELPER_DETACH_MATERIAL_PASS_PIPELINE (material_pass_to_check)
                            break;
                        }

                        case RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_FOUND_IN_NEW_DATA |
                            RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_PASSED_TO_LOADED_DATA:
                            // Used in both new data and loaded data, nothing to do.
                            break;

                        case 0u:
                        {
                            // Not used anywhere anymore, delete everything right away.
                            MATERIAL_HELPER_DETACH_MATERIAL_PASS_PIPELINE (material_pass_to_check)
                            MATERIAL_HELPER_DETACH_MATERIAL_PASS_VARIANT (material_pass_to_check)
                            KAN_UP_ACCESS_DELETE (material_pass_to_check);
                            break;
                        }
                        }
                    }

                    // It is never possible to update material right away, as we need to ensure that family meta
                    // resource is loaded in order to properly update material loaded data.
                }
            }
        }
    }
}

static inline void remove_pass_from_loaded_material (
    struct render_foundation_material_management_execution_state_t *state,
    kan_interned_string_t material_name,
    kan_interned_string_t pass_name)
{
    KAN_UP_VALUE_UPDATE (loaded, kan_render_material_loaded_t, name, &material_name)
    {
        kan_loop_size_t pass_index = 0u;
        while (pass_index < loaded->pipelines.size)
        {
            struct kan_render_material_loaded_pipeline_t *loaded_pipeline =
                &((struct kan_render_material_loaded_pipeline_t *) loaded->pipelines.data)[pass_index];

            if (loaded_pipeline->pass_name == pass_name)
            {
                // We do not use remove swap, as we need to maintain convenient order of pipelines in array.
                kan_dynamic_array_remove_at (&loaded->pipelines, pass_index);
            }
            else
            {
                ++pass_index;
            }
        }

        KAN_UP_EVENT_INSERT (event, kan_render_material_updated_event_t) { event->name = material_name; }
    }
}

UNIVERSE_RENDER_FOUNDATION_API void kan_universe_mutator_execute_render_foundation_material_management_execution (
    kan_cpu_job_t job, struct render_foundation_material_management_execution_state_t *state)
{
    KAN_UP_MUTATOR_RELEASE_JOB_ON_RETURN
    if (!KAN_HANDLE_IS_VALID (state->render_backend_system))
    {
        return;
    }

    struct kan_render_supported_device_info_t *device_info =
        kan_render_backend_system_get_selected_device_info (state->render_backend_system);

    if (!device_info)
    {
        return;
    }

    KAN_UP_SINGLETON_READ (resource_provider, kan_resource_provider_singleton_t)
    if (!resource_provider->scan_done)
    {
        return;
    }

    KAN_UP_EVENT_FETCH (pass_updated_event, kan_render_graph_pass_updated_event_t)
    {
        KAN_UP_VALUE_UPDATE (pipeline_pass, render_foundation_pipeline_pass_variant_state_t, pass_name,
                             &pass_updated_event->name)
        {
            // We need to also delete pipeline as it depends on pass set layout from pass.
            if (KAN_HANDLE_IS_VALID (pipeline_pass->pipeline))
            {
                kan_render_graphics_pipeline_destroy (pipeline_pass->pipeline);
                pipeline_pass->pipeline = KAN_HANDLE_SET_INVALID (kan_render_graphics_pipeline_t);
            }

            // Reset pipeline loading in order to recompile pipelines with new pass.
            KAN_UP_VALUE_UPDATE (pipeline, render_foundation_pipeline_state_t, pipeline_name,
                                 &pipeline_pass->pipeline_name)
            {
                pipeline->request_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_request_id_t);
            }
        }

        KAN_UP_VALUE_UPDATE (material_pass, render_foundation_material_pass_variant_state_t, pass_name,
                             &pass_updated_event->name)
        {
            // We do to temporary remove pipeline from loaded materials as its pass was destroyed during update.
            remove_pass_from_loaded_material (state, material_pass->material_name, material_pass->pass_name);

            // We need to update material pass variant attachments if
            // they were skipped because pass was not available at that time.

            const enum render_foundation_material_pass_variant_flags_t flags_to_check =
                RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_FOUND_IN_NEW_DATA |
                RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_HAS_ATTACHED_PIPELINE_STATE |
                RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_HAS_ATTACHED_VARIANT_STATE;

            const enum render_foundation_material_pass_variant_flags_t flags_filter =
                RENDER_FOUNDATION_MATERIAL_PASS_VARIANT_FOUND_IN_NEW_DATA;

            if ((material_pass->usage_flags & flags_to_check) == flags_filter)
            {
                KAN_UP_VALUE_READ (material, render_foundation_material_state_t, name, &material_pass->material_name)
                {
                    create_material_pass_variant_attachments (state, material, material_pass);
                }
            }
        }
    }

    KAN_UP_EVENT_FETCH (pass_deleted_event, kan_render_graph_pass_deleted_event_t)
    {
        // Pass deletion is rare hot reload related event, therefore we do not optimize for cases like multiple
        // pass deletions in one frame.

        KAN_UP_VALUE_DELETE (pipeline_pass, render_foundation_pipeline_pass_variant_state_t, pass_name,
                             &pass_deleted_event->name)
        {
            KAN_UP_ACCESS_DELETE (pipeline_pass);
        }

        // Rationally, there can be several instances of one pass in material,
        // therefore we can just iterate materials and update them.

        KAN_UP_VALUE_READ (material_pass, render_foundation_material_pass_variant_state_t, pass_name,
                           pass_deleted_event->name)
        {
            // Destroy pipeline if necessary.
            KAN_UP_VALUE_WRITE (pipeline, render_foundation_pipeline_state_t, pipeline_name,
                                &material_pass->pipeline_name)
            {
                KAN_ASSERT (pipeline->reference_count > 0u)
                --pipeline->reference_count;

                if (pipeline->reference_count == 0u)
                {
                    if (KAN_TYPED_ID_32_IS_VALID (pipeline->request_id))
                    {
                        KAN_UP_EVENT_INSERT (event, kan_resource_request_defer_delete_event_t)
                        {
                            event->request_id = pipeline->request_id;
                        }
                    }

                    KAN_UP_ACCESS_DELETE (pipeline);
                }
            }

            // We do not actually need to update everything, we can just remove obsolete passed from loaded data.
            remove_pass_from_loaded_material (state, material_pass->material_name, material_pass->pass_name);
        }
    }

    const kan_time_size_t inspection_time_ns = kan_precise_time_get_elapsed_nanoseconds ();
    KAN_UP_EVENT_FETCH (updated_event, kan_resource_request_updated_event_t)
    {
        if (updated_event->type == state->interned_kan_resource_material_pipeline_family_compiled_t)
        {
            on_pipeline_family_request_updated (state, updated_event->request_id, inspection_time_ns);
        }
        else if (updated_event->type == state->interned_kan_resource_material_pipeline_compiled_t)
        {
            on_pipeline_request_updated (state, updated_event->request_id, inspection_time_ns);
        }
        else if (updated_event->type == state->interned_kan_resource_material_compiled_t)
        {
            on_material_updated (state, updated_event->request_id);
        }
    }
}

void kan_render_material_singleton_init (struct kan_render_material_singleton_t *instance)
{
    instance->usage_id_counter = kan_atomic_int_init (1);
}

void kan_render_material_loaded_init (struct kan_render_material_loaded_t *instance)
{
    instance->name = NULL;
    instance->set_material = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);
    instance->set_object = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);
    instance->set_shared = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);
    kan_dynamic_array_init (&instance->pipelines, 0u, sizeof (struct kan_render_material_loaded_pipeline_t),
                            _Alignof (struct kan_render_material_loaded_pipeline_t), kan_allocation_group_stack_get ());

    kan_dynamic_array_init (&instance->vertex_attribute_sources, 0u, sizeof (struct kan_rpl_meta_attribute_source_t),
                            _Alignof (struct kan_rpl_meta_attribute_source_t), kan_allocation_group_stack_get ());

    instance->push_constant_size = 0u;
    instance->has_instanced_attribute_source = KAN_FALSE;
    kan_rpl_meta_attribute_source_init (&instance->instanced_attribute_source);

    kan_rpl_meta_set_bindings_init (&instance->set_material_bindings);
    kan_rpl_meta_set_bindings_init (&instance->set_object_bindings);
    kan_rpl_meta_set_bindings_init (&instance->set_shared_bindings);
}

void kan_render_material_loaded_shutdown (struct kan_render_material_loaded_t *instance)
{
    for (kan_loop_size_t index = 0u; index < instance->vertex_attribute_sources.size; ++index)
    {
        kan_rpl_meta_attribute_source_shutdown (
            &((struct kan_rpl_meta_attribute_source_t *) instance->vertex_attribute_sources.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->pipelines);
    kan_dynamic_array_shutdown (&instance->vertex_attribute_sources);
    kan_rpl_meta_attribute_source_shutdown (&instance->instanced_attribute_source);
    kan_rpl_meta_set_bindings_shutdown (&instance->set_material_bindings);
    kan_rpl_meta_set_bindings_shutdown (&instance->set_object_bindings);
    kan_rpl_meta_set_bindings_shutdown (&instance->set_shared_bindings);
}
