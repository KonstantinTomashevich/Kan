#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <string.h>

#include <kan/context/all_system_names.h>
#include <kan/context/hot_reload_coordination_system.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/log/logging.h>
#include <kan/render_foundation/program.h>
#include <kan/render_foundation/render_graph.h>
#include <kan/render_foundation/resource_material.h>
#include <kan/render_foundation/resource_material_instance.h>
#include <kan/render_foundation/resource_render_pass.h>
#include <kan/render_foundation/texture.h>
#include <kan/universe/macro.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>

KAN_LOG_DEFINE_CATEGORY (render_foundation_program);
KAN_USE_STATIC_INTERNED_IDS
KAN_USE_STATIC_CPU_SECTIONS

KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP (render_foundation_program_core_management)
KAN_UM_ADD_MUTATOR_TO_FOLLOWING_GROUP (render_foundation_material_instance_management)
UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_GROUP_META (render_foundation_program_management,
                                                          KAN_RENDER_FOUNDATION_PROGRAM_MANAGEMENT_MUTATOR_GROUP);

/// \details Material instances depend on materials and materials depend on passes, therefore we need to synchronize
///          whole hot reload routine for these resources to avoid broken frames (where some of the resources are
///          unavailable due to dependency-driven resource initialization).
enum render_foundation_program_hot_reload_state_t
{
    RENDER_FOUNDATION_HOT_RELOAD_STATE_NONE = 0u,
    RENDER_FOUNDATION_HOT_RELOAD_STATE_SETUP_FRAME,
    RENDER_FOUNDATION_HOT_RELOAD_STATE_LOADING_SCOPE,
    RENDER_FOUNDATION_HOT_RELOAD_STATE_APPLICATION_FRAME,
};

struct render_foundation_program_management_singleton_t
{
    enum render_foundation_program_hot_reload_state_t hot_reload_state;
    kan_instance_size_t hot_reload_blocks;
};

UNIVERSE_RENDER_FOUNDATION_API void render_foundation_program_management_singleton_init (
    struct render_foundation_program_management_singleton_t *instance)
{
    instance->hot_reload_state = RENDER_FOUNDATION_HOT_RELOAD_STATE_NONE;
    instance->hot_reload_blocks = 0u;
}

static kan_render_pipeline_parameter_set_layout_t construct_parameter_set_layout_from_meta (
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

    CUSHION_DEFER
    {
        if (bindings != bindings_static)
        {
            kan_free_general (temporary_allocation_group, bindings,
                              sizeof (struct kan_render_parameter_binding_description_t) * bindings_count);
        }
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
        .bindings_count = binding_output_index,
        .bindings = bindings,
        .tracking_name = tracking_name,
    };

    return kan_render_pipeline_parameter_set_layout_create (render_context, &description);
}

enum render_foundation_pass_state_t
{
    RENDER_FOUNDATION_PASS_STATE_INITIAL,
    RENDER_FOUNDATION_PASS_STATE_WAITING,
    RENDER_FOUNDATION_PASS_STATE_READY,
};

struct render_foundation_pass_t
{
    kan_interned_string_t name;
    kan_resource_usage_id_t usage_id;
    enum render_foundation_pass_state_t state;
    kan_instance_size_t state_frame_id;
    bool hot_reload_mark;
    bool hot_reload_ready_mark;
};

KAN_REFLECTION_STRUCT_META (render_foundation_pass_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_cascade_deletion_t
    render_foundation_pass_id_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
        .child_type_name = "kan_resource_usage_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
};

KAN_REFLECTION_STRUCT_META (render_foundation_pass_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_cascade_deletion_t
    render_foundation_pass_loaded_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
        .child_type_name = "kan_render_foundation_pass_loaded_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
};

enum render_foundation_material_state_t
{
    RENDER_FOUNDATION_MATERIAL_STATE_INITIAL,
    RENDER_FOUNDATION_MATERIAL_STATE_WAITING,
    RENDER_FOUNDATION_MATERIAL_STATE_READY,
};

struct render_foundation_material_t
{
    kan_interned_string_t name;
    kan_resource_usage_id_t usage_id;
    kan_instance_size_t instance_references;
    enum render_foundation_material_state_t state;
    kan_instance_size_t state_frame_id;
    bool hot_reload_mark;
    bool hot_reload_ready_mark;
};

KAN_REFLECTION_STRUCT_META (render_foundation_material_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_cascade_deletion_t
    render_foundation_material_usage_id_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
        .child_type_name = "kan_resource_usage_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
};

KAN_REFLECTION_STRUCT_META (render_foundation_material_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_cascade_deletion_t
    render_foundation_material_loaded_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
        .child_type_name = "kan_render_material_loaded_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
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

struct render_foundation_material_instance_texture_usage_t
{
    kan_interned_string_t material_instance_name;
    kan_interned_string_t texture_name;
    kan_rpl_size_t binding;
    bool bound;
    kan_render_texture_usage_id_t usage_id;
};

KAN_REFLECTION_STRUCT_META (render_foundation_material_instance_texture_usage_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_cascade_deletion_t
    render_foundation_material_instance_texture_usage_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
        .child_type_name = "kan_render_texture_usage_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
};

enum render_foundation_material_instance_state_t
{
    RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_INITIAL = 0u,
    RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_WAITING_RESOURCE,
    RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_WAITING_DEPENDENCIES,
    RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_READY,
};

struct render_foundation_material_instance_t
{
    kan_interned_string_t name;
    kan_interned_string_t loading_material_name;
    kan_instance_size_t reference_count;
    kan_resource_usage_id_t usage_id;

    enum render_foundation_material_instance_state_t state;
    kan_instance_size_t state_frame_id;

    kan_instance_size_t usages_mip_frame_id;
    uint8_t usages_best_mip;
    uint8_t usages_worst_mip;

    bool hot_reload_mark;
    bool hot_reload_ready_mark;
};

KAN_REFLECTION_STRUCT_META (render_foundation_material_instance_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_cascade_deletion_t
    render_foundation_material_instance_usage_id_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
        .child_type_name = "kan_resource_usage_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"usage_id"}},
};

KAN_REFLECTION_STRUCT_META (render_foundation_material_instance_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_cascade_deletion_t
    render_foundation_material_instance_texture_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
        .child_type_name = "render_foundation_material_instance_texture_usage_t",
        .child_key_path = {.reflection_path_length = 1u,
                           .reflection_path = (const char *[]) {"material_instance_name"}},
};

KAN_REFLECTION_STRUCT_META (render_foundation_material_instance_t)
UNIVERSE_RENDER_FOUNDATION_API struct kan_repository_meta_automatic_cascade_deletion_t
    render_foundation_material_instance_loaded_cascade_deletion = {
        .parent_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
        .child_type_name = "kan_render_material_instance_loaded_t",
        .child_key_path = {.reflection_path_length = 1u, .reflection_path = (const char *[]) {"name"}},
};

struct render_foundation_program_core_management_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (render_foundation_program_core_management)
    KAN_UM_BIND_STATE (render_foundation_program_core_management, state)

    kan_context_system_t hot_reload_coordination_system;
    kan_allocation_group_t temporary_allocation_group;
};

UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_DEPLOY (render_foundation_program_core_management)
{
    kan_static_interned_ids_ensure_initialized ();
    kan_cpu_static_sections_ensure_initialized ();

    state->hot_reload_coordination_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_HOT_RELOAD_COORDINATION_SYSTEM_NAME);
    state->temporary_allocation_group = kan_allocation_group_get_child (kan_allocation_group_stack_get (), "temporary");

    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_PROGRAM_MANAGEMENT_BEGIN_CHECKPOINT);
}

static void advance_pass_from_initial_state (struct render_foundation_program_core_management_state_t *state,
                                             struct kan_render_program_singleton_t *public,
                                             struct render_foundation_program_management_singleton_t *private,
                                             const struct kan_resource_provider_singleton_t *provider,
                                             struct render_foundation_pass_t *pass);

static void advance_pass_from_waiting_state (struct render_foundation_program_core_management_state_t *state,
                                             struct kan_render_program_singleton_t *public,
                                             struct render_foundation_program_management_singleton_t *private,
                                             const struct kan_resource_provider_singleton_t *provider,
                                             struct render_foundation_pass_t *pass);

static inline void on_any_resource_update_received (struct render_foundation_program_management_singleton_t *private)
{
    switch (private->hot_reload_state)
    {
    case RENDER_FOUNDATION_HOT_RELOAD_STATE_NONE:
        private->hot_reload_state = RENDER_FOUNDATION_HOT_RELOAD_STATE_SETUP_FRAME;
        break;

    case RENDER_FOUNDATION_HOT_RELOAD_STATE_SETUP_FRAME:
        break;

    case RENDER_FOUNDATION_HOT_RELOAD_STATE_LOADING_SCOPE:
    case RENDER_FOUNDATION_HOT_RELOAD_STATE_APPLICATION_FRAME:
        // Mismanaged hot reload routine, should not be possible.
        KAN_ASSERT (false)
        break;
    }

    ++private->hot_reload_blocks;
}

static void on_pass_resource_updated (struct render_foundation_program_core_management_state_t *state,
                                      struct kan_render_program_singleton_t *public,
                                      struct render_foundation_program_management_singleton_t *private,
                                      const struct kan_resource_provider_singleton_t *provider,
                                      kan_interned_string_t pass_name)
{
    KAN_UMI_VALUE_UPDATE_OPTIONAL (pass, render_foundation_pass_t, name, &pass_name)
    if (!pass)
    {
        return;
    }

    on_any_resource_update_received (private);
    if (KAN_TYPED_ID_32_IS_VALID (pass->usage_id))
    {
        KAN_UMI_VALUE_DETACH_REQUIRED (usage, kan_resource_usage_t, usage_id, &pass->usage_id)
        KAN_UM_ACCESS_DELETE (usage);
        pass->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);
    }

    pass->state = RENDER_FOUNDATION_PASS_STATE_INITIAL;
    pass->state_frame_id = provider->logic_deduplication_frame_id;
    pass->hot_reload_mark = true;
    pass->hot_reload_ready_mark = false;

    ++public->pass_loading_counter;
    advance_pass_from_initial_state (state, public, private, provider, pass);
}

static inline void on_pass_registered (struct render_foundation_program_core_management_state_t *state,
                                       struct kan_render_program_singleton_t *public,
                                       struct render_foundation_program_management_singleton_t *private,
                                       const struct kan_resource_provider_singleton_t *provider,
                                       kan_interned_string_t pass_name)
{
    KAN_UMO_INDEXED_INSERT (pass, render_foundation_pass_t)
    {
        pass->name = pass_name;
        pass->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);
        pass->state = RENDER_FOUNDATION_PASS_STATE_INITIAL;
        pass->state_frame_id = provider->logic_deduplication_frame_id;
        pass->hot_reload_mark = false;
        pass->hot_reload_ready_mark = false;

        ++public->pass_loading_counter;
        advance_pass_from_initial_state (state, public, private, provider, pass);
    }
}

static void advance_pass_from_initial_state (struct render_foundation_program_core_management_state_t *state,
                                             struct kan_render_program_singleton_t *public,
                                             struct render_foundation_program_management_singleton_t *private,
                                             const struct kan_resource_provider_singleton_t *provider,
                                             struct render_foundation_pass_t *pass)
{
    KAN_LOG (render_foundation_program, KAN_LOG_DEBUG,
             "Attempting to advance pass \"%s\" state from initial to waiting.", pass->name)

    pass->state_frame_id = provider->logic_deduplication_frame_id;
    pass->state = RENDER_FOUNDATION_PASS_STATE_WAITING; // We will always advance from initial state.

    KAN_ASSERT (!KAN_TYPED_ID_32_IS_VALID (pass->usage_id))
    pass->usage_id = kan_next_resource_usage_id (provider);

    KAN_UMO_INDEXED_INSERT (usage, kan_resource_usage_t)
    {
        usage->usage_id = pass->usage_id;
        usage->type = KAN_STATIC_INTERNED_ID_GET (kan_resource_render_pass_t);
        usage->name = pass->name;
        usage->priority = KAN_UNIVERSE_RENDER_FOUNDATION_PASS_PRIORITY;
    }

    advance_pass_from_waiting_state (state, public, private, provider, pass);
}

static void load_pass (struct render_foundation_program_core_management_state_t *state,
                       const struct kan_resource_render_pass_t *resource,
                       struct kan_render_foundation_pass_loaded_t *pass)
{
    KAN_CPU_SCOPED_STATIC_SECTION (load_pass)
    if (KAN_HANDLE_IS_VALID (pass->pass))
    {
        kan_render_pass_destroy (pass->pass);
        pass->pass = KAN_HANDLE_SET_INVALID (kan_render_pass_t);
    }

    for (kan_loop_size_t variant_index = 0u; variant_index < pass->variants.size; ++variant_index)
    {
        kan_render_foundation_pass_variant_shutdown (
            &((struct kan_render_foundation_pass_variant_t *) pass->variants.data)[variant_index]);
    }

    KAN_UMI_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    pass->attachments.size = 0u;
    pass->variants.size = 0u;

    pass->type = resource->type;
    struct kan_render_pass_description_t description = {
        .type = resource->type,
        .attachments_count = resource->attachments.size,
        .attachments = (struct kan_render_pass_attachment_t *) resource->attachments.data,
        .tracking_name = pass->name,
    };

    pass->pass = kan_render_pass_create (render_context->render_context, &description);
    if (!KAN_HANDLE_IS_VALID (pass->pass))
    {
        KAN_LOG (render_foundation_program, KAN_LOG_ERROR, "Failed to create render pass from resources \"%s\".",
                 pass->name)
        return;
    }

    kan_dynamic_array_set_capacity (&pass->attachments, resource->attachments.size);
    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) resource->attachments.size; ++index)
    {
        struct kan_render_foundation_pass_attachment_t *output = kan_dynamic_array_add_last (&pass->attachments);
        KAN_ASSERT (output)

        const struct kan_render_pass_attachment_t *input =
            &((struct kan_render_pass_attachment_t *) resource->attachments.data)[index];

        output->type = input->type;
        output->format = input->format;
    }

    kan_dynamic_array_set_capacity (&pass->variants, resource->variants.size);
    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) resource->variants.size; ++index)
    {
        struct kan_render_foundation_pass_variant_t *output = kan_dynamic_array_add_last (&pass->variants);
        KAN_ASSERT (output)

        kan_allocation_group_stack_push (pass->variants.allocation_group);
        kan_render_foundation_pass_variant_init (output);
        kan_allocation_group_stack_pop ();

        const struct kan_resource_render_pass_variant_t *input =
            &((struct kan_resource_render_pass_variant_t *) resource->variants.data)[index];

        output->name = input->name;
        kan_rpl_meta_set_bindings_shutdown (&output->pass_parameter_set_bindings);
        kan_rpl_meta_set_bindings_init_copy (&output->pass_parameter_set_bindings, &input->pass_set_bindings);

        char tracking_name_buffer[KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH];
        snprintf (tracking_name_buffer, sizeof (tracking_name_buffer), "%s::pass_set::%s", pass->name, output->name);

        output->pass_parameter_set_layout = construct_parameter_set_layout_from_meta (
            render_context->render_context, &output->pass_parameter_set_bindings,
            kan_string_intern (tracking_name_buffer), state->temporary_allocation_group);

        if (!KAN_HANDLE_IS_VALID (output->pass_parameter_set_layout))
        {
            KAN_LOG (render_foundation_program, KAN_LOG_ERROR,
                     "Failed to create render pass \"%s\" parameter set layout for variant \"%s\".", pass->name,
                     output->name)
        }
    }

    KAN_UMO_EVENT_INSERT (updated_event, kan_render_foundation_pass_updated_event_t)
    {
        updated_event->name = pass->name;
    }
}

#define HELPER_ACKNOWLEDGE_POSSIBLE_BLOCK_DURING_HOT_RELOAD(RESOURCE)                                                  \
    switch (private->hot_reload_state)                                                                                 \
    {                                                                                                                  \
    case RENDER_FOUNDATION_HOT_RELOAD_STATE_NONE:                                                                      \
    case RENDER_FOUNDATION_HOT_RELOAD_STATE_APPLICATION_FRAME:                                                         \
        break;                                                                                                         \
                                                                                                                       \
    case RENDER_FOUNDATION_HOT_RELOAD_STATE_SETUP_FRAME:                                                               \
    case RENDER_FOUNDATION_HOT_RELOAD_STATE_LOADING_SCOPE:                                                             \
    {                                                                                                                  \
        if ((RESOURCE)->hot_reload_mark)                                                                               \
        {                                                                                                              \
            if (!(RESOURCE)->hot_reload_ready_mark)                                                                    \
            {                                                                                                          \
                KAN_ASSERT (private->hot_reload_blocks > 0u)                                                           \
                --private->hot_reload_blocks;                                                                          \
                (RESOURCE)->hot_reload_ready_mark = true;                                                              \
            }                                                                                                          \
                                                                                                                       \
            return;                                                                                                    \
        }                                                                                                              \
                                                                                                                       \
        break;                                                                                                         \
    }                                                                                                                  \
    }

static void advance_pass_from_waiting_state (struct render_foundation_program_core_management_state_t *state,
                                             struct kan_render_program_singleton_t *public,
                                             struct render_foundation_program_management_singleton_t *private,
                                             const struct kan_resource_provider_singleton_t *provider,
                                             struct render_foundation_pass_t *pass)
{
    KAN_LOG (render_foundation_program, KAN_LOG_DEBUG, "Attempting to advance pass \"%s\" state from waiting to ready.",
             pass->name)

    pass->state_frame_id = provider->logic_deduplication_frame_id;
    KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (resource, kan_resource_render_pass_t, &pass->name)

    if (!resource)
    {
        // Still loading.
        return;
    }

    HELPER_ACKNOWLEDGE_POSSIBLE_BLOCK_DURING_HOT_RELOAD (pass)
    pass->state = RENDER_FOUNDATION_PASS_STATE_READY;
    KAN_UMI_VALUE_UPDATE_OPTIONAL (existing_loaded, kan_render_foundation_pass_loaded_t, name, &pass->name)

    if (existing_loaded)
    {
        load_pass (state, resource, existing_loaded);
    }
    else
    {
        KAN_UMO_INDEXED_INSERT (new_loaded, kan_render_foundation_pass_loaded_t)
        {
            new_loaded->name = pass->name;
            load_pass (state, resource, new_loaded);
        }
    }

    KAN_UMI_VALUE_DETACH_REQUIRED (usage, kan_resource_usage_t, usage_id, &pass->usage_id)
    KAN_UM_ACCESS_DELETE (usage);
    pass->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);

    KAN_ASSERT (public->pass_loading_counter > 0u)
    --public->pass_loading_counter;
    KAN_LOG (render_foundation_program, KAN_LOG_DEBUG, "Advanced pass \"%s\" state to ready.", pass->name)
}

static void advance_material_from_initial_state (struct render_foundation_program_core_management_state_t *state,
                                                 struct kan_render_program_singleton_t *public,
                                                 struct render_foundation_program_management_singleton_t *private,
                                                 const struct kan_resource_provider_singleton_t *provider,
                                                 struct render_foundation_material_t *material);

static void advance_material_from_waiting_state (struct render_foundation_program_core_management_state_t *state,
                                                 struct kan_render_program_singleton_t *public,
                                                 struct render_foundation_program_management_singleton_t *private,
                                                 const struct kan_resource_provider_singleton_t *provider,
                                                 struct render_foundation_material_t *material);

static void on_material_resource_updated (struct render_foundation_program_core_management_state_t *state,
                                          struct kan_render_program_singleton_t *public,
                                          struct render_foundation_program_management_singleton_t *private,
                                          const struct kan_resource_provider_singleton_t *provider,
                                          kan_interned_string_t material_name)
{
    KAN_UMI_VALUE_UPDATE_OPTIONAL (material, render_foundation_material_t, name, &material_name)
    if (!material)
    {
        return;
    }

    on_any_resource_update_received (private);
    if (KAN_TYPED_ID_32_IS_VALID (material->usage_id))
    {
        KAN_UMI_VALUE_DETACH_REQUIRED (usage, kan_resource_usage_t, usage_id, &material->usage_id)
        KAN_UM_ACCESS_DELETE (usage);
        material->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);
    }

    material->state = RENDER_FOUNDATION_MATERIAL_STATE_INITIAL;
    material->state_frame_id = provider->logic_deduplication_frame_id;
    material->hot_reload_mark = true;
    material->hot_reload_ready_mark = false;

    ++public->material_loading_counter;
    advance_material_from_initial_state (state, public, private, provider, material);
}

static inline void on_material_registered (struct render_foundation_program_core_management_state_t *state,
                                           struct kan_render_program_singleton_t *public,
                                           struct render_foundation_program_management_singleton_t *private,
                                           const struct kan_resource_provider_singleton_t *provider,
                                           kan_interned_string_t material_name)
{
    KAN_UMO_INDEXED_INSERT (material, render_foundation_material_t)
    {
        material->name = material_name;
        material->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);
        material->instance_references = 0u;
        material->state = RENDER_FOUNDATION_MATERIAL_STATE_INITIAL;
        material->state_frame_id = provider->logic_deduplication_frame_id;
        material->hot_reload_mark = false;
        material->hot_reload_ready_mark = false;

        ++public->material_loading_counter;
        advance_material_from_initial_state (state, public, private, provider, material);
    }
}

static void advance_material_from_initial_state (struct render_foundation_program_core_management_state_t *state,
                                                 struct kan_render_program_singleton_t *public,
                                                 struct render_foundation_program_management_singleton_t *private,
                                                 const struct kan_resource_provider_singleton_t *provider,
                                                 struct render_foundation_material_t *material)
{
    KAN_LOG (render_foundation_program, KAN_LOG_DEBUG,
             "Attempting to advance material \"%s\" state from initial to waiting.", material->name)

    material->state_frame_id = provider->logic_deduplication_frame_id;
    material->state = RENDER_FOUNDATION_MATERIAL_STATE_WAITING; // We will always advance from initial state.

    KAN_ASSERT (!KAN_TYPED_ID_32_IS_VALID (material->usage_id))
    material->usage_id = kan_next_resource_usage_id (provider);

    KAN_UMO_INDEXED_INSERT (usage, kan_resource_usage_t)
    {
        usage->usage_id = material->usage_id;
        usage->type = KAN_STATIC_INTERNED_ID_GET (kan_resource_material_t);
        usage->name = material->name;
        usage->priority = material->instance_references > 0u ? KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_USED_PRIORITY :
                                                               KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_BASE_PRIORITY;
    }

    advance_material_from_waiting_state (state, public, private, provider, material);
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

    KAN_ASSERT (false)
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

    KAN_ASSERT (false)
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

    KAN_ASSERT (false)
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

    KAN_ASSERT (false)
    return KAN_RENDER_BLEND_OPERATION_ADD;
}

static void load_material (struct render_foundation_program_core_management_state_t *state,
                           struct render_foundation_material_t *material,
                           const struct kan_resource_material_t *resource,
                           struct kan_render_material_loaded_t *loaded)
{
    KAN_CPU_SCOPED_STATIC_SECTION (load_material)
    // Not the most effective way to reset material content technically,
    // but should be rare enough for us to not care about it at all.
    kan_render_material_loaded_shutdown (loaded);
    kan_render_material_loaded_init (loaded);

    KAN_UMI_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    loaded->name = material->name;

    char tracking_name_buffer[KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH];
    if (resource->set_material.buffers.size > 0u || resource->set_material.samplers.size > 0u ||
        resource->set_material.images.size > 0u)
    {
        snprintf (tracking_name_buffer, sizeof (tracking_name_buffer), "%s::material_set", material->name);
        loaded->set_material = construct_parameter_set_layout_from_meta (
            render_context->render_context, &resource->set_material, kan_string_intern (tracking_name_buffer),
            state->temporary_allocation_group);

        if (!KAN_HANDLE_IS_VALID (loaded->set_material))
        {
            KAN_LOG (render_foundation_program, KAN_LOG_ERROR,
                     "Failed to create material parameter set layout for material \"%s\".", material->name)
        }
    }

    if (resource->set_object.buffers.size > 0u || resource->set_object.samplers.size > 0u ||
        resource->set_object.images.size > 0u)
    {
        snprintf (tracking_name_buffer, sizeof (tracking_name_buffer), "%s::material_set", material->name);
        loaded->set_object = construct_parameter_set_layout_from_meta (
            render_context->render_context, &resource->set_object, kan_string_intern (tracking_name_buffer),
            state->temporary_allocation_group);

        if (!KAN_HANDLE_IS_VALID (loaded->set_object))
        {
            KAN_LOG (render_foundation_program, KAN_LOG_ERROR,
                     "Failed to create object parameter set layout for material \"%s\".", material->name)
        }
    }

    if (resource->set_shared.buffers.size > 0u || resource->set_shared.samplers.size > 0u ||
        resource->set_shared.images.size > 0u)
    {
        snprintf (tracking_name_buffer, sizeof (tracking_name_buffer), "%s::material_set", material->name);
        loaded->set_shared = construct_parameter_set_layout_from_meta (
            render_context->render_context, &resource->set_shared, kan_string_intern (tracking_name_buffer),
            state->temporary_allocation_group);

        if (!KAN_HANDLE_IS_VALID (loaded->set_shared))
        {
            KAN_LOG (render_foundation_program, KAN_LOG_ERROR,
                     "Failed to create shared parameter set layout for material \"%s\".", material->name)
        }
    }

    struct kan_render_attribute_source_description_t
        attribute_sources_static[KAN_UNIVERSE_RENDER_FOUNDATION_BINDINGS_MAX_STATIC];
    struct kan_render_attribute_source_description_t *attribute_sources = NULL;
    kan_instance_size_t attributes_sources_count = 0u;

    CUSHION_DEFER
    {
        if (attribute_sources && attribute_sources != attribute_sources_static)
        {
            kan_free_general (state->temporary_allocation_group, attribute_sources,
                              sizeof (struct kan_render_attribute_source_description_t) * attributes_sources_count);
        }
    }

    struct kan_render_attribute_description_t attributes_static[KAN_UNIVERSE_RENDER_FOUNDATION_BINDINGS_MAX_STATIC];
    struct kan_render_attribute_description_t *attributes = NULL;
    kan_instance_size_t attributes_count = 0u;

    CUSHION_DEFER
    {
        if (attributes && attributes != attributes_static)
        {
            kan_free_general (state->temporary_allocation_group, attributes,
                              sizeof (struct kan_render_attribute_description_t) * attributes_count);
        }
    }

    if (resource->vertex_attribute_sources.size > 0u || resource->has_instanced_attribute_source)
    {
        attribute_sources = attribute_sources_static;
        attributes_sources_count = resource->vertex_attribute_sources.size;

        if (resource->has_instanced_attribute_source)
        {
            ++attributes_sources_count;
        }

        if (attributes_sources_count > KAN_UNIVERSE_RENDER_FOUNDATION_BINDINGS_MAX_STATIC)
        {
            attribute_sources = kan_allocate_general (
                state->temporary_allocation_group,
                sizeof (struct kan_render_attribute_source_description_t) * attributes_sources_count,
                alignof (struct kan_render_attribute_source_description_t));
        }

        for (kan_loop_size_t index = 0u; index < resource->vertex_attribute_sources.size; ++index)
        {
            struct kan_rpl_meta_attribute_source_t *source =
                &((struct kan_rpl_meta_attribute_source_t *) resource->vertex_attribute_sources.data)[index];
            attributes_count += source->attributes.size;

            attribute_sources[index] = (struct kan_render_attribute_source_description_t) {
                .binding = source->binding,
                .stride = source->block_size,
                .rate = KAN_RENDER_ATTRIBUTE_RATE_PER_VERTEX,
            };
        }

        if (resource->has_instanced_attribute_source)
        {
            attributes_count += resource->instanced_attribute_source.attributes.size;
            attribute_sources[resource->vertex_attribute_sources.size] =
                (struct kan_render_attribute_source_description_t) {
                    .binding = resource->instanced_attribute_source.binding,
                    .stride = resource->instanced_attribute_source.block_size,
                    .rate = KAN_RENDER_ATTRIBUTE_RATE_PER_INSTANCE,
                };
        }
    }

    if (attributes_count > 0u)
    {
        attributes = attributes_static;
        if (attributes_count > KAN_UNIVERSE_RENDER_FOUNDATION_BINDINGS_MAX_STATIC)
        {
            attributes = kan_allocate_general (state->temporary_allocation_group,
                                               sizeof (struct kan_render_attribute_description_t) * attributes_count,
                                               alignof (struct kan_render_attribute_description_t));
        }

        kan_instance_size_t attribute_output_index = 0u;
        for (kan_loop_size_t source_index = 0u; source_index < resource->vertex_attribute_sources.size; ++source_index)
        {
            struct kan_rpl_meta_attribute_source_t *source =
                &((struct kan_rpl_meta_attribute_source_t *) resource->vertex_attribute_sources.data)[source_index];
            add_attributes_from_source (source, attributes, &attribute_output_index);
        }

        if (resource->has_instanced_attribute_source)
        {
            add_attributes_from_source (&resource->instanced_attribute_source, attributes, &attribute_output_index);
        }
    }

    const enum kan_render_pipeline_compilation_priority_t pipeline_priority =
        material->instance_references > 0u ? KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_ACTIVE :
                                             KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CACHE;
    kan_dynamic_array_set_capacity (&loaded->pipelines, resource->pipelines.size);

    for (kan_loop_size_t index = 0u; index < resource->pipelines.size; ++index)
    {
        const struct kan_resource_material_pipeline_t *input =
            &((struct kan_resource_material_pipeline_t *) resource->pipelines.data)[index];

        snprintf (tracking_name_buffer, sizeof (tracking_name_buffer), "%s::%s::%s", material->name, input->pass_name,
                  input->variant_name ? input->variant_name : "<base>");
        const kan_interned_string_t tracking_name = kan_string_intern (tracking_name_buffer);

        KAN_UMI_VALUE_READ_OPTIONAL (pass, kan_render_foundation_pass_loaded_t, name, &input->pass_name)
        if (!pass || !KAN_HANDLE_IS_VALID (pass->pass))
        {
            KAN_LOG (render_foundation_program, KAN_LOG_ERROR,
                     "Failed to create pipeline for material \"%s\" for pass \"%s\" for variant \"%s\" as pass is not "
                     "available in runtime for some reason.",
                     material->name, input->pass_name, input->variant_name ? input->variant_name : "<base>")
            continue;
        }

        if ((kan_render_get_supported_code_format_flags () & (kan_memory_size_t) (1u << input->code_format)) == 0u)
        {
            KAN_LOG (render_foundation_program, KAN_LOG_ERROR,
                     "Failed to create pipeline for material \"%s\" for pass \"%s\" for variant \"%s\" as its code "
                     "format is not supported.",
                     material->name, input->pass_name, input->variant_name ? input->variant_name : "<base>")
            continue;
        }

        kan_render_code_module_t code_module = kan_render_code_module_create (
            render_context->render_context, input->code.size, input->code.data, tracking_name);

        if (!KAN_HANDLE_IS_VALID (code_module))
        {
            KAN_LOG (render_foundation_program, KAN_LOG_ERROR,
                     "Failed to create pipeline for material \"%s\" for pass \"%s\" for variant \"%s\" as code module "
                     "creation has failed.",
                     material->name, input->pass_name, input->variant_name ? input->variant_name : "<base>")
            continue;
        }

        CUSHION_DEFER { kan_render_code_module_destroy (code_module); }
        kan_render_pipeline_parameter_set_layout_t pass_layout = KAN_HANDLE_INITIALIZE_INVALID;

        for (kan_loop_size_t variant_index = 0u; variant_index < pass->variants.size; ++variant_index)
        {
            const struct kan_render_foundation_pass_variant_t *variant =
                &((struct kan_render_foundation_pass_variant_t *) pass->variants.data)[variant_index];

            if (variant->name == input->variant_name)
            {
                pass_layout = variant->pass_parameter_set_layout;
                break;
            }
        }

        enum kan_render_polygon_mode_t polygon_mode = KAN_RENDER_POLYGON_MODE_FILL;
        switch (input->pipeline_settings.polygon_mode)
        {
        case KAN_RPL_POLYGON_MODE_FILL:
            polygon_mode = KAN_RENDER_POLYGON_MODE_FILL;
            break;

        case KAN_RPL_POLYGON_MODE_WIREFRAME:
            polygon_mode = KAN_RENDER_POLYGON_MODE_WIREFRAME;
            break;
        }

        enum kan_render_cull_mode_t cull_mode = KAN_RENDER_CULL_MODE_BACK;
        switch (input->pipeline_settings.cull_mode)
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

        struct kan_render_color_output_setup_description_t color_outputs_static[COLOR_OUTPUTS_STATIC_COUNT];
        struct kan_render_color_output_setup_description_t *color_outputs = NULL;

        if (input->color_outputs.size > 0u)
        {
            color_outputs = color_outputs_static;
            if (input->color_outputs.size > COLOR_OUTPUTS_STATIC_COUNT)
            {
                color_outputs = kan_allocate_general (
                    state->temporary_allocation_group,
                    sizeof (struct kan_render_color_output_setup_description_t) * input->color_outputs.size,
                    alignof (struct kan_render_color_output_setup_description_t));
            }

            for (kan_loop_size_t color_index = 0u; color_index < input->color_outputs.size; ++color_index)
            {
                struct kan_rpl_meta_color_output_t *color_output =
                    &((struct kan_rpl_meta_color_output_t *) input->color_outputs.data)[color_index];

                color_outputs[color_index] = (struct kan_render_color_output_setup_description_t) {
                    .use_blend = color_output->use_blend,
                    .write_r = color_output->write_r,
                    .write_g = color_output->write_g,
                    .write_b = color_output->write_b,
                    .write_a = color_output->write_a,
                    .source_color_blend_factor = convert_blend_factor (color_output->source_color_blend_factor),
                    .destination_color_blend_factor =
                        convert_blend_factor (color_output->destination_color_blend_factor),
                    .color_blend_operation = convert_blend_operation (color_output->color_blend_operation),
                    .source_alpha_blend_factor = convert_blend_factor (color_output->source_alpha_blend_factor),
                    .destination_alpha_blend_factor =
                        convert_blend_factor (color_output->destination_alpha_blend_factor),
                    .alpha_blend_operation = convert_blend_operation (color_output->alpha_blend_operation),
                };
            }
        }

        CUSHION_DEFER
        {
            if (color_outputs && color_outputs != color_outputs_static)
            {
                kan_free_general (
                    state->temporary_allocation_group, color_outputs,
                    sizeof (struct kan_render_color_output_setup_description_t) * input->color_outputs.size);
            }
        }

        struct kan_render_pipeline_code_entry_point_t entry_points_static[ENTRY_POINTS_STATIC_COUNT];
        struct kan_render_pipeline_code_module_usage_t code_module_usage = {
            .code_module = code_module,
            .entry_points_count = input->entry_points.size,
            .entry_points = NULL,
        };

        if (code_module_usage.entry_points_count > 0u)
        {
            code_module_usage.entry_points = entry_points_static;
            if (code_module_usage.entry_points_count > ENTRY_POINTS_STATIC_COUNT)
            {
                code_module_usage.entry_points = kan_allocate_general (
                    state->temporary_allocation_group,
                    sizeof (struct kan_render_pipeline_code_entry_point_t) * code_module_usage.entry_points_count,
                    alignof (struct kan_render_pipeline_code_entry_point_t));
            }

            for (kan_loop_size_t point_index = 0u; point_index < code_module_usage.entry_points_count; ++point_index)
            {
                struct kan_rpl_entry_point_t *entry_point =
                    &((struct kan_rpl_entry_point_t *) input->entry_points.data)[point_index];
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

                code_module_usage.entry_points[point_index] = (struct kan_render_pipeline_code_entry_point_t) {
                    .stage = stage,
                    .function_name = entry_point->function_name,
                };
            }
        }

        CUSHION_DEFER
        {
            if (code_module_usage.entry_points && code_module_usage.entry_points != entry_points_static)
            {
                kan_free_general (
                    state->temporary_allocation_group, code_module_usage.entry_points,
                    sizeof (struct kan_render_pipeline_code_entry_point_t) * code_module_usage.entry_points_count);
            }
        }

        kan_render_pipeline_parameter_set_layout_t parameter_sets[4u] = {
            pass_layout,
            loaded->set_material,
            loaded->set_object,
            loaded->set_shared,
        };

        struct kan_render_graphics_pipeline_description_t description = {
            .pass = pass->pass,
            .topology = KAN_RENDER_GRAPHICS_TOPOLOGY_TRIANGLE_LIST,
            .attribute_sources_count = attributes_sources_count,
            .attribute_sources = attribute_sources,
            .attributes_count = attributes_count,
            .attributes = attributes,
            .push_constant_size = resource->push_constant_size,
            .parameter_set_layouts_count = 4u,
            .parameter_set_layouts = parameter_sets,

            .polygon_mode = polygon_mode,
            .cull_mode = cull_mode,
            .use_depth_clamp = false,

            .output_setups_count = input->color_outputs.size,
            .output_setups = color_outputs,

            .blend_constant_r = input->color_blend_constants.r,
            .blend_constant_g = input->color_blend_constants.g,
            .blend_constant_b = input->color_blend_constants.b,
            .blend_constant_a = input->color_blend_constants.a,

            .depth_test_enabled = input->pipeline_settings.depth_test,
            .depth_write_enabled = input->pipeline_settings.depth_write,
            .depth_bounds_test_enabled = input->pipeline_settings.depth_bounds_test,
            .depth_compare_operation = convert_compare_operation (input->pipeline_settings.depth_compare_operation),
            .min_depth = input->pipeline_settings.depth_min,
            .max_depth = input->pipeline_settings.depth_max,

            .stencil_test_enabled = input->pipeline_settings.stencil_test,
            .stencil_front =
                {
                    .on_fail = convert_stencil_operation (input->pipeline_settings.stencil_front_on_fail),
                    .on_depth_fail = convert_stencil_operation (input->pipeline_settings.stencil_front_on_depth_fail),
                    .on_pass = convert_stencil_operation (input->pipeline_settings.stencil_front_on_pass),
                    .compare = convert_compare_operation (input->pipeline_settings.stencil_front_compare),
                    .compare_mask = input->pipeline_settings.stencil_front_compare_mask,
                    .write_mask = input->pipeline_settings.stencil_front_write_mask,
                    .reference = input->pipeline_settings.stencil_front_reference,
                },
            .stencil_back =
                {
                    .on_fail = convert_stencil_operation (input->pipeline_settings.stencil_back_on_fail),
                    .on_depth_fail = convert_stencil_operation (input->pipeline_settings.stencil_back_on_depth_fail),
                    .on_pass = convert_stencil_operation (input->pipeline_settings.stencil_back_on_pass),
                    .compare = convert_compare_operation (input->pipeline_settings.stencil_back_compare),
                    .compare_mask = input->pipeline_settings.stencil_back_compare_mask,
                    .write_mask = input->pipeline_settings.stencil_back_write_mask,
                    .reference = input->pipeline_settings.stencil_back_reference,
                },

            .code_modules_count = 1u,
            .code_modules = &code_module_usage,
            .tracking_name = tracking_name,
        };

        kan_render_graphics_pipeline_t pipeline =
            kan_render_graphics_pipeline_create (render_context->render_context, &description, pipeline_priority);

        if (!KAN_HANDLE_IS_VALID (pipeline))
        {
            KAN_LOG (render_foundation_program, KAN_LOG_ERROR,
                     "Failed to create pipeline for material \"%s\" for pass \"%s\" for variant \"%s\" as pipeline "
                     "creation function has failed.",
                     material->name, input->pass_name, input->variant_name ? input->variant_name : "<base>")
            continue;
        }

#undef COLOR_OUTPUTS_STATIC_COUNT
#undef ENTRY_POINTS_STATIC_COUNT

        struct kan_render_material_pipeline_t *output = kan_dynamic_array_add_last (&loaded->pipelines);
        output->pass_name = input->pass_name;
        output->variant_name = input->variant_name;
        output->pipeline = pipeline;
    }

    kan_dynamic_array_set_capacity (&loaded->vertex_attribute_sources, resource->vertex_attribute_sources.size);
    for (kan_loop_size_t index = 0u; index < resource->vertex_attribute_sources.size; ++index)
    {
        const struct kan_rpl_meta_attribute_source_t *input =
            &((struct kan_rpl_meta_attribute_source_t *) resource->vertex_attribute_sources.data)[index];

        struct kan_rpl_meta_attribute_source_t *output = kan_dynamic_array_add_last (&loaded->vertex_attribute_sources);
        kan_rpl_meta_attribute_source_init_copy (output, input);
    }

    loaded->push_constant_size = resource->push_constant_size;
    if ((loaded->has_instanced_attribute_source = resource->has_instanced_attribute_source))
    {
        kan_rpl_meta_attribute_source_shutdown (&loaded->instanced_attribute_source);
        kan_rpl_meta_attribute_source_init_copy (&loaded->instanced_attribute_source,
                                                 &resource->instanced_attribute_source);
    }

    kan_rpl_meta_set_bindings_shutdown (&loaded->set_material_bindings);
    kan_rpl_meta_set_bindings_init_copy (&loaded->set_material_bindings, &resource->set_material);

    kan_rpl_meta_set_bindings_shutdown (&loaded->set_object_bindings);
    kan_rpl_meta_set_bindings_init_copy (&loaded->set_object_bindings, &resource->set_object);

    kan_rpl_meta_set_bindings_shutdown (&loaded->set_shared_bindings);
    kan_rpl_meta_set_bindings_init_copy (&loaded->set_shared_bindings, &resource->set_shared);
    KAN_UMO_EVENT_INSERT (updated_event, kan_render_material_updated_event_t) { updated_event->name = material->name; }
}

static void advance_material_from_waiting_state (struct render_foundation_program_core_management_state_t *state,
                                                 struct kan_render_program_singleton_t *public,
                                                 struct render_foundation_program_management_singleton_t *private,
                                                 const struct kan_resource_provider_singleton_t *provider,
                                                 struct render_foundation_material_t *material)
{
    KAN_LOG (render_foundation_program, KAN_LOG_DEBUG,
             "Attempting to advance material \"%s\" state from waiting to ready.", material->name)

    material->state_frame_id = provider->logic_deduplication_frame_id;
    KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (resource, kan_resource_material_t, &material->name)

    if (!resource)
    {
        // Still loading.
        return;
    }

    HELPER_ACKNOWLEDGE_POSSIBLE_BLOCK_DURING_HOT_RELOAD (material)
    material->state = RENDER_FOUNDATION_MATERIAL_STATE_READY;
    KAN_UMI_VALUE_UPDATE_OPTIONAL (existing_loaded, kan_render_material_loaded_t, name, &material->name)

    if (existing_loaded)
    {
        load_material (state, material, resource, existing_loaded);
    }
    else
    {
        KAN_UMO_INDEXED_INSERT (new_loaded, kan_render_material_loaded_t)
        {
            new_loaded->name = material->name;
            load_material (state, material, resource, new_loaded);
        }
    }

    KAN_UMI_VALUE_DETACH_REQUIRED (usage, kan_resource_usage_t, usage_id, &material->usage_id)
    KAN_UM_ACCESS_DELETE (usage);
    material->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);

    KAN_ASSERT (public->material_loading_counter > 0u)
    --public->material_loading_counter;

    KAN_LOG (render_foundation_program, KAN_LOG_DEBUG, "Advanced material \"%s\" state to ready.", material->name)
}

UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_EXECUTE (render_foundation_program_core_management)
{
    KAN_UMI_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    if (!KAN_HANDLE_IS_VALID (render_context->render_context))
    {
        return;
    }

    KAN_UMI_SINGLETON_READ (provider, kan_resource_provider_singleton_t)
    if (!provider->scan_done)
    {
        return;
    }

    KAN_UMI_SINGLETON_WRITE (public, kan_render_program_singleton_t)
    KAN_UMI_SINGLETON_WRITE (private, render_foundation_program_management_singleton_t)

    if (private->hot_reload_state == RENDER_FOUNDATION_HOT_RELOAD_STATE_SETUP_FRAME)
    {
        private->hot_reload_state = RENDER_FOUNDATION_HOT_RELOAD_STATE_LOADING_SCOPE;
    }

    if (private->hot_reload_state == RENDER_FOUNDATION_HOT_RELOAD_STATE_APPLICATION_FRAME)
    {
        private->hot_reload_state = RENDER_FOUNDATION_HOT_RELOAD_STATE_NONE;
    }

    if (private->hot_reload_state == RENDER_FOUNDATION_HOT_RELOAD_STATE_LOADING_SCOPE &&
        private->hot_reload_blocks == 0u)
    {
        private->hot_reload_state = RENDER_FOUNDATION_HOT_RELOAD_STATE_APPLICATION_FRAME;
        KAN_UML_SIGNAL_UPDATE (pass, render_foundation_pass_t, hot_reload_mark, true)
        {
            KAN_ASSERT (pass->hot_reload_ready_mark)
            KAN_ASSERT (pass->state == RENDER_FOUNDATION_PASS_STATE_WAITING)
            advance_pass_from_waiting_state (state, public, private, provider, pass);
            pass->hot_reload_mark = false;
            pass->hot_reload_ready_mark = false;
        }

        KAN_UML_SIGNAL_UPDATE (material, render_foundation_material_t, hot_reload_mark, true)
        {
            KAN_ASSERT (material->hot_reload_ready_mark)
            KAN_ASSERT (material->state == RENDER_FOUNDATION_MATERIAL_STATE_WAITING)
            advance_material_from_waiting_state (state, public, private, provider, material);
            material->hot_reload_mark = false;
            material->hot_reload_ready_mark = false;
        }
    }

    // Delay hot reload rebuilds until our segmented synchronized hot reload is done.
    if (KAN_HANDLE_IS_VALID (state->hot_reload_coordination_system) &&
        kan_hot_reload_coordination_system_is_scheduled (state->hot_reload_coordination_system) &&
        private->hot_reload_state != RENDER_FOUNDATION_HOT_RELOAD_STATE_NONE)
    {
        kan_hot_reload_coordination_system_delay (state->hot_reload_coordination_system);
    }

    KAN_UML_RESOURCE_UPDATED_EVENT_FETCH (pass_updated_event, kan_resource_render_pass_t)
    {
        on_pass_resource_updated (state, public, private, provider, pass_updated_event->name);
    }

    KAN_UML_RESOURCE_UPDATED_EVENT_FETCH (material_updated_event, kan_resource_material_t)
    {
        on_material_resource_updated (state, public, private, provider, material_updated_event->name);
    }

    KAN_UML_RESOURCE_REGISTERED_EVENT_FETCH (pass_registered, kan_resource_render_pass_t)
    {
        on_pass_registered (state, public, private, provider, pass_registered->name);
    }

    KAN_UML_RESOURCE_LOADED_EVENT_FETCH (pass_loaded_event, kan_resource_render_pass_t)
    {
        KAN_UMI_VALUE_UPDATE_REQUIRED (pass, render_foundation_pass_t, name, &pass_loaded_event->name)
        if (pass->state_frame_id != provider->logic_deduplication_frame_id)
        {
            switch (pass->state)
            {
            case RENDER_FOUNDATION_PASS_STATE_INITIAL:
            case RENDER_FOUNDATION_PASS_STATE_READY:
                KAN_ASSERT_FORMATTED (false,
                                      "Render pass \"%s\" in state %u received resource loaded event, which is totally "
                                      "unexpected in this state.",
                                      pass->name, (unsigned int) pass->state)
                break;

            case RENDER_FOUNDATION_PASS_STATE_WAITING:
                advance_pass_from_waiting_state (state, public, private, provider, pass);
                break;
            }
        }
    }

    if (public->pass_loading_counter > 0u && private->hot_reload_state == RENDER_FOUNDATION_HOT_RELOAD_STATE_NONE)
    {
        // We do not start loading materials unless all passes are loaded.
        // Hot reload is a special case as we need to schedule reload of all the materials too.
        return;
    }

    KAN_UML_RESOURCE_REGISTERED_EVENT_FETCH (material_registered, kan_resource_material_t)
    {
        on_material_registered (state, public, private, provider, material_registered->name);
    }

    KAN_UML_RESOURCE_LOADED_EVENT_FETCH (material_loaded_event, kan_resource_material_t)
    {
        KAN_UMI_VALUE_UPDATE_REQUIRED (material, render_foundation_material_t, name, &material_loaded_event->name)
        if (material->state_frame_id != provider->logic_deduplication_frame_id)
        {
            switch (material->state)
            {
            case RENDER_FOUNDATION_PASS_STATE_INITIAL:
            case RENDER_FOUNDATION_PASS_STATE_READY:
                KAN_ASSERT_FORMATTED (false,
                                      "Render material \"%s\" in state %u received resource loaded event, which is "
                                      "totally unexpected in this state.",
                                      material->name, (unsigned int) material->state)
                break;

            case RENDER_FOUNDATION_PASS_STATE_WAITING:
                advance_material_from_waiting_state (state, public, private, provider, material);
                break;
            }
        }
    }
}

struct render_foundation_material_instance_management_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (render_foundation_material_instance_management)
    KAN_UM_BIND_STATE (render_foundation_material_instance_management, state)

    kan_context_system_t render_backend_system;
    kan_allocation_group_t temporary_allocation_group;
};

UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_DEPLOY (render_foundation_material_instance_management)
{
    kan_static_interned_ids_ensure_initialized ();
    kan_cpu_static_sections_ensure_initialized ();

    state->render_backend_system =
        kan_context_query (kan_universe_get_context (universe), KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME);
    state->temporary_allocation_group = kan_allocation_group_get_child (kan_allocation_group_stack_get (), "temporary");

    kan_workflow_graph_node_depend_on (workflow_node, KAN_RESOURCE_PROVIDER_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, KAN_RENDER_FOUNDATION_TEXTURE_MANAGEMENT_END_CHECKPOINT);
    kan_workflow_graph_node_depend_on (workflow_node, "render_foundation_program_core_management");
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RENDER_FOUNDATION_PROGRAM_MANAGEMENT_END_CHECKPOINT);
    kan_workflow_graph_node_make_dependency_of (workflow_node, KAN_RENDER_FOUNDATION_FRAME_BEGIN_CHECKPOINT);
}

static void recalculate_usages_mip (struct render_foundation_material_instance_management_state_t *state,
                                    struct render_foundation_material_instance_t *material_instance)
{
    material_instance->usages_best_mip = KAN_INT_MAX (typeof (material_instance->usages_best_mip));
    material_instance->usages_worst_mip = 0u;

    KAN_UML_VALUE_READ (usage, kan_render_material_instance_usage_t, name, &material_instance->name)
    {
        material_instance->usages_best_mip = KAN_MIN (material_instance->usages_best_mip, usage->best_advised_mip);
        material_instance->usages_worst_mip = KAN_MAX (material_instance->usages_worst_mip, usage->worst_advised_mip);
    }
}

static void advance_material_instance_from_initial_state (
    struct render_foundation_material_instance_management_state_t *state,
    struct kan_render_program_singleton_t *public,
    struct render_foundation_program_management_singleton_t *private,
    const struct kan_resource_provider_singleton_t *provider,
    struct render_foundation_material_instance_t *material_instance);

static void advance_material_instance_from_waiting_resource_state (
    struct render_foundation_material_instance_management_state_t *state,
    struct kan_render_program_singleton_t *public,
    struct render_foundation_program_management_singleton_t *private,
    const struct kan_resource_provider_singleton_t *provider,
    struct render_foundation_material_instance_t *material_instance);

static void advance_material_instance_from_waiting_dependencies_state (
    struct render_foundation_material_instance_management_state_t *state,
    struct kan_render_program_singleton_t *public,
    struct render_foundation_program_management_singleton_t *private,
    const struct kan_resource_provider_singleton_t *provider,
    struct render_foundation_material_instance_t *material_instance);

static void remap_material_link_priorities (struct render_foundation_material_instance_management_state_t *state,
                                            const struct kan_resource_provider_singleton_t *provider,
                                            struct render_foundation_material_t *material)
{
    KAN_CPU_SCOPED_STATIC_SECTION (remap_material_link_priorities)
    if (KAN_TYPED_ID_32_IS_VALID (material->usage_id))
    {
        KAN_UMI_VALUE_DELETE_REQUIRED (usage_to_detach, kan_resource_usage_t, usage_id, &material->usage_id)
        KAN_UM_ACCESS_DELETE (usage_to_detach);
        material->usage_id = kan_next_resource_usage_id (provider);

        KAN_UMO_INDEXED_INSERT (usage, kan_resource_usage_t)
        {
            usage->usage_id = material->usage_id;
            usage->type = KAN_STATIC_INTERNED_ID_GET (kan_resource_render_material_t);
            usage->name = material->name;
            usage->priority = material->instance_references > 0u ?
                                  KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_USED_PRIORITY :
                                  KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_BASE_PRIORITY;
        }
    }

    KAN_UMI_VALUE_READ_OPTIONAL (loaded, kan_render_material_loaded_t, name, &material->name)
    if (!loaded)
    {
        return;
    }

    const enum kan_render_pipeline_compilation_priority_t pipeline_priority =
        material->instance_references > 0u ? KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_ACTIVE :
                                             KAN_RENDER_PIPELINE_COMPILATION_PRIORITY_CACHE;

    for (kan_loop_size_t index = 0u; index < loaded->pipelines.size; ++index)
    {
        const struct kan_render_material_pipeline_t *pipeline =
            &((struct kan_render_material_pipeline_t *) loaded->pipelines.data)[index];
        kan_render_graphics_pipeline_change_compilation_priority (pipeline->pipeline, pipeline_priority);
    }
}

static void add_material_link_from_instance (struct render_foundation_material_instance_management_state_t *state,
                                             const struct kan_resource_provider_singleton_t *provider,
                                             kan_interned_string_t material_name)
{
    KAN_UMI_VALUE_UPDATE_OPTIONAL (material, render_foundation_material_t, name, &material_name)
    if (!material)
    {
        return;
    }

    ++material->instance_references;
    if (material->instance_references == 1u)
    {
        remap_material_link_priorities (state, provider, material);
    }
}

static void remove_material_link_from_instance (struct render_foundation_material_instance_management_state_t *state,
                                                const struct kan_resource_provider_singleton_t *provider,
                                                kan_interned_string_t material_name)
{
    KAN_UMI_VALUE_UPDATE_OPTIONAL (material, render_foundation_material_t, name, &material_name)
    if (!material)
    {
        return;
    }

    KAN_ASSERT (material->instance_references > 0u)
    --material->instance_references;

    if (material->instance_references == 0u)
    {
        remap_material_link_priorities (state, provider, material);
    }
}

static void on_material_instance_resource_updated (struct render_foundation_material_instance_management_state_t *state,
                                                   struct kan_render_program_singleton_t *public,
                                                   struct render_foundation_program_management_singleton_t *private,
                                                   const struct kan_resource_provider_singleton_t *provider,
                                                   kan_interned_string_t material_instance_name)
{
    KAN_UMI_VALUE_UPDATE_OPTIONAL (material_instance, render_foundation_material_instance_t, name,
                                   &material_instance_name)
    if (!material_instance)
    {
        return;
    }

    on_any_resource_update_received (private);
    if (material_instance->loading_material_name)
    {
        remove_material_link_from_instance (state, provider, material_instance->loading_material_name);
        material_instance->loading_material_name = NULL;
    }

    if (KAN_TYPED_ID_32_IS_VALID (material_instance->usage_id))
    {
        KAN_UMI_VALUE_DETACH_REQUIRED (usage, kan_resource_usage_t, usage_id, &material_instance->usage_id)
        KAN_UM_ACCESS_DELETE (usage);
        material_instance->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);
    }

    KAN_UML_VALUE_DELETE (texture_usage, render_foundation_material_instance_texture_usage_t, material_instance_name,
                          &material_instance_name)
    {
        if (!texture_usage->bound)
        {
            KAN_UM_ACCESS_DELETE (texture_usage);
        }
    }

    material_instance->state = RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_INITIAL;
    material_instance->state_frame_id = provider->logic_deduplication_frame_id;
    material_instance->hot_reload_mark = true;
    material_instance->hot_reload_ready_mark = false;

    ++public->material_instance_loading_counter;
    advance_material_instance_from_initial_state (state, public, private, provider, material_instance);
}

static void on_material_instance_usage_insert (struct render_foundation_material_instance_management_state_t *state,
                                               struct kan_render_program_singleton_t *public,
                                               struct render_foundation_program_management_singleton_t *private,
                                               const struct kan_resource_provider_singleton_t *provider,
                                               kan_interned_string_t material_instance_name)
{
    KAN_UMI_VALUE_UPDATE_OPTIONAL (existent, render_foundation_material_instance_t, name, &material_instance_name)
    if (existent)
    {
        ++existent->reference_count;
        if (existent->usages_mip_frame_id == provider->logic_deduplication_frame_id)
        {
            return;
        }

        const uint8_t previous_best_mip = existent->usages_best_mip;
        const uint8_t previous_worst_mip = existent->usages_best_mip;

        recalculate_usages_mip (state, existent);
        if (previous_best_mip == existent->usages_best_mip && previous_worst_mip == existent->usages_worst_mip)
        {
            return;
        }

        KAN_UMI_SINGLETON_READ (texture_singleton, kan_render_texture_singleton_t)
        KAN_UML_VALUE_UPDATE (texture_usage, render_foundation_material_instance_texture_usage_t,
                              material_instance_name, &material_instance_name)
        {
            KAN_UMI_VALUE_DETACH_REQUIRED (usage_to_detach, kan_render_texture_usage_t, usage_id,
                                           &texture_usage->usage_id)
            KAN_UM_ACCESS_DELETE (usage_to_detach);

            texture_usage->usage_id = kan_next_texture_usage_id (texture_singleton);
            KAN_UMO_INDEXED_INSERT (usage_to_insert, kan_render_texture_usage_t)
            {
                usage_to_insert->usage_id = texture_usage->usage_id;
                usage_to_insert->name = texture_usage->texture_name;
                usage_to_insert->best_advised_mip = existent->usages_best_mip;
                usage_to_insert->worst_advised_mip = existent->usages_worst_mip;
            }
        }

        return;
    }

    KAN_UMO_INDEXED_INSERT (material_instance, render_foundation_material_instance_t)
    {
        material_instance->name = material_instance_name;
        material_instance->loading_material_name = NULL;
        material_instance->reference_count = 1u;
        material_instance->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);

        material_instance->state = RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_INITIAL;
        material_instance->state_frame_id = provider->logic_deduplication_frame_id;

        material_instance->usages_mip_frame_id = provider->logic_deduplication_frame_id;
        recalculate_usages_mip (state, material_instance);

        material_instance->hot_reload_mark = false;
        material_instance->hot_reload_ready_mark = false;
        ++public->material_instance_loading_counter;
        advance_material_instance_from_initial_state (state, public, private, provider, material_instance);
    }
}

static void on_material_instance_usage_delete (struct render_foundation_material_instance_management_state_t *state,
                                               struct kan_render_program_singleton_t *public,
                                               const struct kan_resource_provider_singleton_t *provider,
                                               kan_interned_string_t material_instance_name)
{
    KAN_UMI_VALUE_WRITE_OPTIONAL (existent, render_foundation_material_instance_t, name, &material_instance_name)
    if (existent)
    {
        --existent->reference_count;
        if (existent->reference_count == 0u)
        {
            if (existent->loading_material_name)
            {
                remove_material_link_from_instance (state, provider, existent->loading_material_name);
            }

            {
                KAN_UMI_VALUE_DETACH_OPTIONAL (loaded, kan_render_material_instance_loaded_t, name,
                                               &material_instance_name)
                remove_material_link_from_instance (state, provider, loaded->material_name);
                KAN_UM_ACCESS_DELETE (loaded);
            }

            switch (existent->state)
            {
            case RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_INITIAL:
            case RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_WAITING_RESOURCE:
            case RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_WAITING_DEPENDENCIES:
                KAN_ASSERT (public->material_instance_loading_counter > 0u)
                --public->material_instance_loading_counter;
                break;

            case RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_READY:
                break;
            }

            // Cascade deletion should handle everything.
            KAN_UM_ACCESS_DELETE (existent);
        }
    }
}

static void advance_material_instance_from_initial_state (
    struct render_foundation_material_instance_management_state_t *state,
    struct kan_render_program_singleton_t *public,
    struct render_foundation_program_management_singleton_t *private,
    const struct kan_resource_provider_singleton_t *provider,
    struct render_foundation_material_instance_t *material_instance)
{
    KAN_LOG (render_foundation_program, KAN_LOG_DEBUG,
             "Attempting to advance material instance \"%s\" state from initial to waiting resource.",
             material_instance->name)

    material_instance->state_frame_id = provider->logic_deduplication_frame_id;
    material_instance->state = RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_WAITING_RESOURCE;

    KAN_ASSERT (!KAN_TYPED_ID_32_IS_VALID (material_instance->usage_id))
    material_instance->usage_id = kan_next_resource_usage_id (provider);

    KAN_UMO_INDEXED_INSERT (usage, kan_resource_usage_t)
    {
        usage->usage_id = material_instance->usage_id;
        usage->type = KAN_STATIC_INTERNED_ID_GET (kan_resource_material_instance_t);
        usage->name = material_instance->name;
        usage->priority = KAN_UNIVERSE_RENDER_FOUNDATION_MI_PRIORITY;
    }

    advance_material_instance_from_waiting_resource_state (state, public, private, provider, material_instance);
}

static void advance_material_instance_from_waiting_resource_state (
    struct render_foundation_material_instance_management_state_t *state,
    struct kan_render_program_singleton_t *public,
    struct render_foundation_program_management_singleton_t *private,
    const struct kan_resource_provider_singleton_t *provider,
    struct render_foundation_material_instance_t *material_instance)
{
    KAN_LOG (
        render_foundation_program, KAN_LOG_DEBUG,
        "Attempting to advance material instance \"%s\" state from waiting resource to waiting dependencies state.",
        material_instance->name)

    material_instance->state_frame_id = provider->logic_deduplication_frame_id;
    KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (resource, kan_resource_material_instance_t, &material_instance->name)

    if (!resource)
    {
        return;
    }

    // Add link to material, so it will be loaded faster.
    material_instance->loading_material_name = resource->material;
    add_material_link_from_instance (state, provider, resource->material);

    // Add usages for all textures mentioned in the resource.
    KAN_UMI_SINGLETON_READ (texture_singleton, kan_render_texture_singleton_t)

    for (kan_loop_size_t index = 0u; index < resource->images.size; ++index)
    {
        const struct kan_resource_image_binding_t *binding =
            &((struct kan_resource_image_binding_t *) resource->images.data)[index];

        // We just insert all the references here as not bound usages.
        // When everything is loaded, we'll delete previously bound usage and bind new ones.
        // Not the most effective way,

        KAN_UMO_INDEXED_INSERT (usage, render_foundation_material_instance_texture_usage_t)
        {
            usage->material_instance_name = material_instance->name;
            usage->texture_name = binding->texture;
            usage->binding = binding->binding;
            usage->bound = false;
            usage->usage_id = kan_next_texture_usage_id (texture_singleton);

            KAN_UMO_INDEXED_INSERT (usage_to_insert, kan_render_texture_usage_t)
            {
                usage_to_insert->usage_id = usage->usage_id;
                usage_to_insert->name = usage->texture_name;
                usage_to_insert->best_advised_mip = material_instance->usages_best_mip;
                usage_to_insert->worst_advised_mip = material_instance->usages_worst_mip;
            }
        }
    }

    material_instance->state = RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_WAITING_DEPENDENCIES;
    advance_material_instance_from_waiting_dependencies_state (state, public, private, provider, material_instance);
}

static void load_material_instance (struct render_foundation_material_instance_management_state_t *state,
                                    const struct kan_resource_material_instance_t *resource,
                                    struct kan_render_material_instance_loaded_t *loaded)
{
    KAN_CPU_SCOPED_STATIC_SECTION (load_material)
    // Not the most effective way to reset material instance content technically,
    // but should be rare enough for us to not care about it at all.
    kan_interned_string_t name = loaded->name;
    kan_render_material_instance_loaded_shutdown (loaded);
    kan_render_material_instance_loaded_init (loaded);

    loaded->name = name;
    loaded->material_name = resource->material;

    kan_render_context_t render_context = kan_render_backend_system_get_render_context (state->render_backend_system);
    KAN_ASSERT (KAN_HANDLE_IS_VALID (render_context))

    KAN_UMI_VALUE_READ_REQUIRED (material_loaded, kan_render_material_loaded_t, name, &resource->material)
    if (!KAN_HANDLE_IS_VALID (material_loaded->set_material))
    {
        KAN_LOG (render_foundation_program, KAN_LOG_ERROR,
                 "Failed to load material instance \"%s\" as material \"%s\" parameter set layout is not available.",
                 loaded->name, resource->material)
        return;
    }

    struct kan_render_parameter_update_description_t bindings_static[KAN_UNIVERSE_RENDER_FOUNDATION_MI_UPDATES_COUNT];
    struct kan_render_parameter_update_description_t *bindings = bindings_static;
    const kan_instance_size_t bindings_count = resource->buffers.size + resource->samplers.size + resource->images.size;

    if (bindings_count > KAN_UNIVERSE_RENDER_FOUNDATION_MI_UPDATES_COUNT)
    {
        bindings = kan_allocate_general (state->temporary_allocation_group,
                                         sizeof (struct kan_render_parameter_update_description_t) * bindings_count,
                                         alignof (struct kan_render_parameter_update_description_t));
    }

    CUSHION_DEFER
    {
        if (bindings != bindings_static)
        {
            kan_free_general (state->temporary_allocation_group, bindings,
                              sizeof (struct kan_render_parameter_update_description_t) * bindings_count);
        }
    }

    char tracking_name_buffer[KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH];
    struct kan_render_parameter_update_description_t *bindings_output = bindings;
    kan_dynamic_array_set_capacity (&loaded->bound_buffers, resource->buffers.size);

    for (kan_loop_size_t index = 0u; index < resource->buffers.size; ++index)
    {
        const struct kan_resource_buffer_binding_t *buffer_binding =
            &((struct kan_resource_buffer_binding_t *) resource->buffers.data)[index];

        if (buffer_binding->data.size == 0u)
        {
            // Zero size buffers are possible for buffers with zero count of tails and empty main part.
            continue;
        }

        enum kan_render_buffer_type_t buffer_type = KAN_RENDER_BUFFER_TYPE_UNIFORM;
        switch (buffer_binding->type)
        {
        case KAN_RPL_BUFFER_TYPE_UNIFORM:
            buffer_type = KAN_RENDER_BUFFER_TYPE_UNIFORM;
            break;

        case KAN_RPL_BUFFER_TYPE_READ_ONLY_STORAGE:
            buffer_type = KAN_RENDER_BUFFER_TYPE_STORAGE;
            break;

        case KAN_RPL_BUFFER_TYPE_PUSH_CONSTANT:
            // Should not be here in the meta.
            KAN_ASSERT (false)
            break;
        }

        snprintf (tracking_name_buffer, sizeof (tracking_name_buffer), "%s::buffer%u", loaded->name,
                  (unsigned int) buffer_binding->binding);

        kan_render_buffer_t new_buffer =
            kan_render_buffer_create (render_context, buffer_type, buffer_binding->data.size, buffer_binding->data.data,
                                      kan_string_intern (tracking_name_buffer));

        if (!KAN_HANDLE_IS_VALID (new_buffer))
        {
            KAN_LOG (render_foundation_program, KAN_LOG_ERROR,
                     "Failed to create buffer for material instance \"%s\" at binding %u.", loaded->name,
                     (unsigned int) buffer_binding->binding)
            continue;
        }

        struct kan_render_material_instance_bound_buffer_t *bound_buffer =
            kan_dynamic_array_add_last (&loaded->bound_buffers);
        bound_buffer->binding = buffer_binding->binding;
        bound_buffer->buffer = new_buffer;

        bindings_output->binding = buffer_binding->binding;
        bindings_output->buffer_binding.buffer = new_buffer;
        bindings_output->buffer_binding.offset = 0u;
        bindings_output->buffer_binding.range = buffer_binding->data.size;
        ++bindings_output;
    }

    for (kan_loop_size_t index = 0u; index < resource->samplers.size; ++index)
    {
        const struct kan_resource_sampler_binding_t *sampler_binding =
            &((struct kan_resource_sampler_binding_t *) resource->samplers.data)[index];

        bindings_output->binding = sampler_binding->binding;
        bindings_output->sampler_binding.sampler = sampler_binding->sampler;
        ++bindings_output;
    }

    KAN_UML_VALUE_WRITE (texture_usage, render_foundation_material_instance_texture_usage_t, material_instance_name,
                         &loaded->name)
    {
        if (texture_usage->bound)
        {
            // Already bound usage from previous loading (happens only with hot reload), delete it now.
            KAN_UM_ACCESS_DELETE (texture_usage);
            continue;
        }

        texture_usage->bound = true;
        KAN_UMI_VALUE_READ_REQUIRED (texture_loaded, kan_render_texture_loaded_t, name, &texture_usage->texture_name)

        bindings_output->binding = texture_usage->binding;
        bindings_output->image_binding.image = texture_loaded->image;
        bindings_output->image_binding.array_index = 0u;
        bindings_output->image_binding.layer_offset = 0u;
        bindings_output->image_binding.layer_count = 1u;
        ++bindings_output;
    }

    struct kan_render_pipeline_parameter_set_description_t description = {
        .layout = material_loaded->set_material,
        .stable_binding = true,
        .tracking_name = loaded->name,
        .initial_bindings_count = bindings_output - bindings,
        .initial_bindings = bindings,
    };

    loaded->parameter_set = kan_render_pipeline_parameter_set_create (render_context, &description);
    if (!KAN_HANDLE_IS_VALID (loaded->parameter_set))
    {
        KAN_LOG (render_foundation_program, KAN_LOG_ERROR,
                 "Failed to create parameter set for material instance \"%s\".", loaded->name)
    }

    kan_dynamic_array_set_capacity (&loaded->variants, resource->variants.size);
    for (kan_loop_size_t index = 0u; index < resource->variants.size; ++index)
    {
        const struct kan_resource_material_variant_t *source =
            &((struct kan_resource_material_variant_t *) resource->variants.data)[index];
        struct kan_resource_material_variant_t *target = kan_dynamic_array_add_last (&loaded->variants);

        kan_allocation_group_stack_push (loaded->variants.allocation_group);
        kan_resource_material_variant_init (target);
        kan_allocation_group_stack_pop ();

        target->name = source->name;
        kan_dynamic_array_set_capacity (&target->instanced_data, source->instanced_data.size);
        target->instanced_data.size = source->instanced_data.size;
        memcpy (target->instanced_data.data, source->instanced_data.data, source->instanced_data.size);
    }
}

static void advance_material_instance_from_waiting_dependencies_state (
    struct render_foundation_material_instance_management_state_t *state,
    struct kan_render_program_singleton_t *public,
    struct render_foundation_program_management_singleton_t *private,
    const struct kan_resource_provider_singleton_t *provider,
    struct render_foundation_material_instance_t *material_instance)
{
    KAN_LOG (render_foundation_program, KAN_LOG_DEBUG,
             "Attempting to advance material instance \"%s\" state from waiting dependencies to ready.",
             material_instance->name)
    material_instance->state_frame_id = provider->logic_deduplication_frame_id;

    KAN_UMI_VALUE_READ_OPTIONAL (material_loaded, kan_render_material_loaded_t, name,
                                 &material_instance->loading_material_name)

    if (!material_loaded)
    {
        return;
    }

    KAN_UML_VALUE_READ (texture_usage, render_foundation_material_instance_texture_usage_t, material_instance_name,
                        &material_instance->name)
    {
        if (!texture_usage->bound)
        {
            KAN_UMI_VALUE_READ_OPTIONAL (texture_loaded, kan_render_texture_loaded_t, name,
                                         &texture_usage->texture_name)

            if (!texture_loaded)
            {
                return;
            }
        }
    }

    HELPER_ACKNOWLEDGE_POSSIBLE_BLOCK_DURING_HOT_RELOAD (material_instance)

    // Everything seems to be ready for the loading.
    material_instance->state = RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_READY;
    KAN_UMI_VALUE_UPDATE_OPTIONAL (existing_loaded, kan_render_material_instance_loaded_t, name,
                                   &material_instance->name)

    KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED_AND_FRESH (resource, kan_resource_material_instance_t, &material_instance->name)
    KAN_ASSERT (resource)

    if (existing_loaded)
    {
        // Remove previous material link.
        // If material didn't change, we still have loading-time link.
        // If material has changed, we need to unlink it either way.
        if (existing_loaded->material_name)
        {
            remove_material_link_from_instance (state, provider, existing_loaded->material_name);
        }

        load_material_instance (state, resource, existing_loaded);
    }
    else
    {
        KAN_UMO_INDEXED_INSERT (new_loaded, kan_render_material_instance_loaded_t)
        {
            new_loaded->name = material_instance->name;
            load_material_instance (state, resource, new_loaded);
        }
    }

    KAN_UMI_VALUE_DETACH_REQUIRED (usage, kan_resource_usage_t, usage_id, &material_instance->usage_id)
    KAN_UM_ACCESS_DELETE (usage);
    material_instance->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_resource_usage_id_t);

    material_instance->loading_material_name = NULL;
    // Material is no longer used for loading purposes, but we should not unlink it
    // as pipelines might still be compiling, so priority still matters.
    // However, link should be deleted when hot reload is started.

    KAN_ASSERT (public->material_instance_loading_counter > 0u)
    --public->material_instance_loading_counter;

    KAN_LOG (render_foundation_program, KAN_LOG_DEBUG, "Advanced material instance \"%s\" state to ready.",
             material_instance->name)
}

UNIVERSE_RENDER_FOUNDATION_API KAN_UM_MUTATOR_EXECUTE (render_foundation_material_instance_management)
{
    KAN_UMI_SINGLETON_READ (render_context, kan_render_context_singleton_t)
    if (!KAN_HANDLE_IS_VALID (render_context->render_context))
    {
        return;
    }

    KAN_UMI_SINGLETON_READ (provider, kan_resource_provider_singleton_t)
    if (!provider->scan_done)
    {
        return;
    }

    KAN_UMI_SINGLETON_WRITE (public, kan_render_program_singleton_t)
    KAN_UMI_SINGLETON_WRITE (private, render_foundation_program_management_singleton_t)

    if (public->pass_loading_counter > 0u && private->hot_reload_state == RENDER_FOUNDATION_HOT_RELOAD_STATE_NONE)
    {
        // We do not start loading materials unless all passes are loaded, so we also cannot start loading instances.
        // Hot reload is a special case as we need to schedule reload of all the materials too.
        return;
    }

    KAN_UML_RESOURCE_UPDATED_EVENT_FETCH (material_instance_updated_event, kan_resource_material_instance_t)
    {
        on_material_instance_resource_updated (state, public, private, provider, material_instance_updated_event->name);
    }

    if (private->hot_reload_state == RENDER_FOUNDATION_HOT_RELOAD_STATE_APPLICATION_FRAME)
    {
        KAN_UML_SIGNAL_UPDATE (material_instance, render_foundation_material_instance_t, hot_reload_mark, true)
        {
            KAN_ASSERT (material_instance->hot_reload_ready_mark)
            KAN_ASSERT (material_instance->state == RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_WAITING_DEPENDENCIES)
            advance_material_instance_from_waiting_dependencies_state (state, public, private, provider,
                                                                       material_instance);
            material_instance->hot_reload_mark = false;
            material_instance->hot_reload_ready_mark = false;
        }
    }

    KAN_UML_EVENT_FETCH (on_insert_event, render_foundation_material_instance_usage_on_insert_event_t)
    {
        on_material_instance_usage_insert (state, public, private, provider, on_insert_event->material_instance_name);
    }

    KAN_UML_EVENT_FETCH (on_delete_event, render_foundation_material_instance_usage_on_delete_event_t)
    {
        on_material_instance_usage_delete (state, public, provider, on_delete_event->material_instance_name);
    }

    KAN_UML_RESOURCE_LOADED_EVENT_FETCH (resource_loaded_event, kan_resource_material_instance_t)
    {
        KAN_UMI_VALUE_UPDATE_OPTIONAL (material_instance, render_foundation_material_instance_t, name,
                                       &resource_loaded_event->name)

        if (material_instance && material_instance->state_frame_id != provider->logic_deduplication_frame_id)
        {
            switch (material_instance->state)
            {
            case RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_INITIAL:
            case RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_WAITING_DEPENDENCIES:
            case RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_READY:
                KAN_ASSERT_FORMATTED (false,
                                      "Texture \"%s\" in state %u received main resource loaded event, which is "
                                      "totally unexpected in this state.",
                                      material_instance->name, (unsigned int) material_instance->state)
                break;

            case RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_WAITING_RESOURCE:
                advance_material_instance_from_waiting_resource_state (state, public, private, provider,
                                                                       material_instance);
                break;
            }
        }
    }

    KAN_UML_EVENT_FETCH (material_loaded_event, kan_render_material_updated_event_t)
    {
        KAN_UML_VALUE_UPDATE (material_instance, render_foundation_material_instance_t, loading_material_name,
                              &material_loaded_event->name)
        {
            if (material_instance->state_frame_id != provider->logic_deduplication_frame_id)
            {
                switch (material_instance->state)
                {
                case RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_INITIAL:
                case RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_WAITING_RESOURCE:
                case RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_READY:
                    // Technically possible as material is a shared base resource.
                    break;

                case RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_WAITING_DEPENDENCIES:
                    advance_material_instance_from_waiting_dependencies_state (state, public, private, provider,
                                                                               material_instance);
                    break;
                }
            }
        }
    }

    KAN_UML_EVENT_FETCH (texture_loaded_event, kan_render_texture_updated_event_t)
    {
        KAN_UML_VALUE_READ (usage, render_foundation_material_instance_texture_usage_t, texture_name,
                            &texture_loaded_event->name)
        {
            KAN_UMI_VALUE_UPDATE_REQUIRED (material_instance, render_foundation_material_instance_t, name,
                                           &usage->material_instance_name)

            if (usage->bound)
            {
                KAN_CPU_SCOPED_STATIC_SECTION (rebind_texture)
                KAN_UMI_VALUE_READ_REQUIRED (instance_loaded, kan_render_material_instance_loaded_t, name,
                                             &material_instance->name)

                if (!KAN_HANDLE_IS_VALID (instance_loaded->parameter_set))
                {
                    break;
                }

                KAN_UMI_VALUE_READ_REQUIRED (texture_loaded, kan_render_texture_loaded_t, name,
                                             &texture_loaded_event->name)

                struct kan_render_parameter_update_description_t update = {
                    .binding = usage->binding,
                    .image_binding =
                        {
                            .image = texture_loaded->image,
                            .array_index = 0u,
                            .layer_offset = 0u,
                            .layer_count = 1u,
                        },
                };

                kan_render_pipeline_parameter_set_update (instance_loaded->parameter_set, 1u, &update);
            }
            else if (material_instance->state == RENDER_FOUNDATION_MATERIAL_INSTANCE_STATE_WAITING_DEPENDENCIES &&
                     material_instance->state_frame_id != provider->logic_deduplication_frame_id)
            {
                // Hack for advancing material instances from inside this loop.
                // To advance material instance loading, we need to manage their usages, which would be impossible
                // while this usage access is still open. Therefore, we escape it to close it early.
                struct kan_repository_indexed_value_read_access_t stolen_access;
                KAN_UM_ACCESS_ESCAPE (stolen_access, usage);
                kan_repository_indexed_value_read_access_close (&stolen_access);

                advance_material_instance_from_waiting_dependencies_state (state, public, private, provider,
                                                                           material_instance);
            }
        }
    }
}

void kan_render_program_singleton_init (struct kan_render_program_singleton_t *instance)
{
    instance->material_instance_usage_id_counter = kan_atomic_int_init (1);
    instance->pass_loading_counter = 0u;
    instance->material_loading_counter = 0u;
    instance->material_instance_loading_counter = 0u;
}

void kan_render_foundation_pass_variant_init (struct kan_render_foundation_pass_variant_t *instance)
{
    instance->name = NULL;
    instance->pass_parameter_set_layout = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);
    kan_rpl_meta_set_bindings_init (&instance->pass_parameter_set_bindings);
}

void kan_render_foundation_pass_variant_shutdown (struct kan_render_foundation_pass_variant_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->pass_parameter_set_layout))
    {
        kan_render_pipeline_parameter_set_layout_destroy (instance->pass_parameter_set_layout);
    }

    kan_rpl_meta_set_bindings_shutdown (&instance->pass_parameter_set_bindings);
}

void kan_render_foundation_pass_loaded_init (struct kan_render_foundation_pass_loaded_t *instance)
{
    instance->name = NULL;
    instance->type = KAN_RENDER_PASS_GRAPHICS;
    instance->pass = KAN_HANDLE_SET_INVALID (kan_render_pass_t);

    kan_dynamic_array_init (&instance->attachments, 0u, sizeof (struct kan_render_foundation_pass_attachment_t),
                            alignof (struct kan_render_foundation_pass_attachment_t),
                            kan_allocation_group_stack_get ());

    kan_dynamic_array_init (&instance->variants, 0u, sizeof (struct kan_render_foundation_pass_variant_t),
                            alignof (struct kan_render_foundation_pass_variant_t), kan_allocation_group_stack_get ());
}

void kan_render_foundation_pass_loaded_shutdown (struct kan_render_foundation_pass_loaded_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->pass))
    {
        kan_render_pass_destroy (instance->pass);
    }

    kan_dynamic_array_shutdown (&instance->attachments);
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->variants, kan_render_foundation_pass_variant)
}

void kan_render_material_pipeline_init (struct kan_render_material_pipeline_t *instance)
{
    instance->pass_name = NULL;
    instance->variant_name = NULL;
    instance->pipeline = KAN_HANDLE_SET_INVALID (kan_render_graphics_pipeline_t);
}

void kan_render_material_pipeline_shutdown (struct kan_render_material_pipeline_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->pipeline))
    {
        kan_render_graphics_pipeline_destroy (instance->pipeline);
    }
}

void kan_render_material_loaded_init (struct kan_render_material_loaded_t *instance)
{
    instance->name = NULL;
    kan_dynamic_array_init (&instance->pipelines, 0u, sizeof (struct kan_render_material_pipeline_t),
                            alignof (struct kan_render_material_pipeline_t), kan_allocation_group_stack_get ());

    instance->set_material = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);
    instance->set_object = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);
    instance->set_shared = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_layout_t);

    kan_dynamic_array_init (&instance->vertex_attribute_sources, 0u, sizeof (struct kan_rpl_meta_attribute_source_t),
                            alignof (struct kan_rpl_meta_attribute_source_t), kan_allocation_group_stack_get ());
    instance->push_constant_size = 0u;

    instance->has_instanced_attribute_source = false;
    kan_rpl_meta_attribute_source_init (&instance->instanced_attribute_source);

    kan_rpl_meta_set_bindings_init (&instance->set_material_bindings);
    kan_rpl_meta_set_bindings_init (&instance->set_object_bindings);
    kan_rpl_meta_set_bindings_init (&instance->set_shared_bindings);
}

void kan_render_material_loaded_shutdown (struct kan_render_material_loaded_t *instance)
{
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->pipelines, kan_render_material_pipeline)
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

    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->vertex_attribute_sources, kan_rpl_meta_attribute_source)
    kan_rpl_meta_attribute_source_shutdown (&instance->instanced_attribute_source);

    kan_rpl_meta_set_bindings_shutdown (&instance->set_material_bindings);
    kan_rpl_meta_set_bindings_shutdown (&instance->set_object_bindings);
    kan_rpl_meta_set_bindings_shutdown (&instance->set_shared_bindings);
}

void kan_render_material_instance_usage_init (struct kan_render_material_instance_usage_t *instance)
{
    instance->usage_id = KAN_TYPED_ID_32_SET_INVALID (kan_render_material_instance_usage_id_t);
    instance->name = NULL;
    instance->best_advised_mip = 0u;
    instance->worst_advised_mip = KAN_INT_MAX (uint8_t);
}

void kan_render_material_instance_variant_init (struct kan_render_material_instance_variant_t *instance)
{
    instance->name = NULL;
    kan_dynamic_array_init (&instance->instanced_data, 0u, sizeof (uint8_t),
                            KAN_RESOURCE_RENDER_FOUNDATION_BUFFER_ALIGNMENT, kan_allocation_group_stack_get ());
}

void kan_render_material_instance_variant_shutdown (struct kan_render_material_instance_variant_t *instance)
{
    kan_dynamic_array_shutdown (&instance->instanced_data);
}

void kan_render_material_instance_bound_buffer_init (struct kan_render_material_instance_bound_buffer_t *instance)
{
    instance->binding = 0u;
    instance->buffer = KAN_HANDLE_SET_INVALID (kan_render_buffer_t);
}

void kan_render_material_instance_bound_buffer_shutdown (struct kan_render_material_instance_bound_buffer_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->buffer))
    {
        kan_render_buffer_destroy (instance->buffer);
    }
}

void kan_render_material_instance_loaded_init (struct kan_render_material_instance_loaded_t *instance)
{
    instance->name = NULL;
    instance->material_name = NULL;
    instance->parameter_set = KAN_HANDLE_SET_INVALID (kan_render_pipeline_parameter_set_t);
    kan_dynamic_array_init (&instance->variants, 0u, sizeof (struct kan_render_material_instance_variant_t),
                            alignof (struct kan_render_material_instance_variant_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->bound_buffers, 0u, sizeof (struct kan_render_material_instance_bound_buffer_t),
                            alignof (struct kan_render_material_instance_bound_buffer_t),
                            kan_allocation_group_stack_get ());
}

void kan_render_material_instance_loaded_shutdown (struct kan_render_material_instance_loaded_t *instance)
{
    if (KAN_HANDLE_IS_VALID (instance->parameter_set))
    {
        kan_render_pipeline_parameter_set_destroy (instance->parameter_set);
    }

    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->variants, kan_render_material_instance_variant)
    KAN_DYNAMIC_ARRAY_SHUTDOWN_WITH_ITEMS_AUTO (instance->bound_buffers, kan_render_material_instance_bound_buffer)
}
