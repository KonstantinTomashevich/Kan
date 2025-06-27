#include <string.h>

#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/resource_material/resource_material.h>
#include <kan/resource_material/resource_material_instance.h>
#include <kan/resource_pipeline/resource_pipeline.h>

KAN_LOG_DEFINE_CATEGORY (resource_material_instance_compilation);

KAN_REFLECTION_STRUCT_META (kan_resource_material_instance_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t kan_resource_material_instance_resource_type_meta = {
    .root = false,
};

static enum kan_resource_compile_result_t kan_resource_material_instance_compile (
    struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (kan_resource_material_instance_t)
RESOURCE_MATERIAL_API struct kan_resource_compilable_meta_t kan_resource_material_instance_compilable_meta = {
    .output_type_name = "kan_resource_material_instance_compiled_t",
    .configuration_type_name = NULL,
    // No state as material instance compilation is pretty simple and should be fast.
    .state_type_name = NULL,
    .functor = kan_resource_material_instance_compile,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_instance_t, material)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_instance_material_reference_meta = {
    .type = "kan_resource_material_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_instance_t, parent)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_instance_parent_reference_meta = {
    .type = "kan_resource_material_instance_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_image_t, texture)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_material_image_texture_reference_meta = {
    .type = "kan_resource_texture_t",
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

KAN_REFLECTION_STRUCT_META (kan_resource_material_instance_static_compiled_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t
    kan_resource_material_instance_static_compiled_resource_type_meta = {
        .root = false,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_instance_static_compiled_t, material)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t
    kan_resource_material_instance_static_compiled_material_reference_meta = {
        .type = "kan_resource_material_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

KAN_REFLECTION_STRUCT_META (kan_resource_material_instance_compiled_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t
    kan_resource_material_instance_compiled_resource_type_meta = {
        .root = false,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_instance_compiled_t, static_data)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t
    kan_resource_material_instance_compiled_static_data_reference_meta = {
        .type = "kan_resource_material_instance_static_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NOT_NEEDED,
};

struct kan_resource_material_instance_static_t
{
    kan_interned_string_t base_static;
    kan_interned_string_t append_raw_instance;
};

KAN_REFLECTION_STRUCT_META (kan_resource_material_instance_static_t)
RESOURCE_MATERIAL_API struct kan_resource_byproduct_type_meta_t
    kan_resource_material_instance_static_byproduct_type_meta = {
        .hash = NULL,
        .is_equal = NULL,
        .move = NULL,
        .reset = NULL,
};

static enum kan_resource_compile_result_t kan_resource_material_instance_static_compile (
    struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (kan_resource_material_instance_static_t)
RESOURCE_MATERIAL_API struct kan_resource_compilable_meta_t kan_resource_material_instance_static_compilable_meta = {
    .output_type_name = "kan_resource_material_instance_static_compiled_t",
    .configuration_type_name = NULL,
    // No state as material instance static compilation is pretty simple and should be fast.
    .state_type_name = NULL,
    .functor = kan_resource_material_instance_static_compile,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_instance_static_t, base_static)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t
    kan_resource_material_instance_static_base_static_reference_meta = {
        .type = "kan_resource_material_instance_static_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_COMPILED,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_material_instance_static_t, append_raw_instance)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t
    kan_resource_material_instance_static_append_raw_instance_reference_meta = {
        .type = "kan_resource_material_instance_t",
        .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW,
};

static inline bool merge_parameters_into (struct kan_dynamic_array_t *output,
                                          const struct kan_dynamic_array_t *overlay,
                                          const char *instance_name)
{
    for (kan_loop_size_t overlay_index = 0u; overlay_index < overlay->size; ++overlay_index)
    {
        bool overridden = false;
        struct kan_resource_material_parameter_t *overlay_parameter =
            &((struct kan_resource_material_parameter_t *) overlay->data)[overlay_index];

        for (kan_loop_size_t output_index = 0u; output_index < output->size; ++output_index)
        {
            struct kan_resource_material_parameter_t *output_parameter =
                &((struct kan_resource_material_parameter_t *) output->data)[output_index];

            if (output_parameter->name == overlay_parameter->name)
            {
                if (output_parameter->type != overlay_parameter->type)
                {
                    KAN_LOG (resource_material_instance_compilation, KAN_LOG_ERROR,
                             "Material instance \"%s\": failure during parameter merge. Encountered attempt to "
                             "override parameter \"%s\" with different type.",
                             instance_name, output_parameter->name)
                    return false;
                }

                switch (output_parameter->type)
                {
                case KAN_RPL_META_VARIABLE_TYPE_F1:
                    output_parameter->value_f1 = overlay_parameter->value_f1;
                    break;

                case KAN_RPL_META_VARIABLE_TYPE_F2:
                    output_parameter->value_f2 = overlay_parameter->value_f2;
                    break;

                case KAN_RPL_META_VARIABLE_TYPE_F3:
                    output_parameter->value_f3 = overlay_parameter->value_f3;
                    break;

                case KAN_RPL_META_VARIABLE_TYPE_F4:
                    output_parameter->value_f4 = overlay_parameter->value_f4;
                    break;

                case KAN_RPL_META_VARIABLE_TYPE_U1:
                    output_parameter->value_u1 = overlay_parameter->value_u1;
                    break;

                case KAN_RPL_META_VARIABLE_TYPE_U2:
                    output_parameter->value_u2 = overlay_parameter->value_u2;
                    break;

                case KAN_RPL_META_VARIABLE_TYPE_U3:
                    output_parameter->value_u3 = overlay_parameter->value_u3;
                    break;

                case KAN_RPL_META_VARIABLE_TYPE_U4:
                    output_parameter->value_u4 = overlay_parameter->value_u4;
                    break;

                case KAN_RPL_META_VARIABLE_TYPE_S1:
                    output_parameter->value_s1 = overlay_parameter->value_s1;
                    break;

                case KAN_RPL_META_VARIABLE_TYPE_S2:
                    output_parameter->value_s2 = overlay_parameter->value_s2;
                    break;

                case KAN_RPL_META_VARIABLE_TYPE_S3:
                    output_parameter->value_s3 = overlay_parameter->value_s3;
                    break;

                case KAN_RPL_META_VARIABLE_TYPE_S4:
                    output_parameter->value_s4 = overlay_parameter->value_s4;
                    break;

                case KAN_RPL_META_VARIABLE_TYPE_F3X3:
                    output_parameter->value_f3x3 = overlay_parameter->value_f3x3;
                    break;

                case KAN_RPL_META_VARIABLE_TYPE_F4X4:
                    output_parameter->value_f4x4 = overlay_parameter->value_f4x4;
                    break;
                }

                overridden = true;
                break;
            }
        }

        if (!overridden)
        {
            void *spot = kan_dynamic_array_add_last (output);
            KAN_ASSERT (spot);
            memcpy (spot, overlay_parameter, sizeof (struct kan_resource_material_parameter_t));
        }
    }

    return true;
}

static inline bool merge_parameter_arrays (struct kan_dynamic_array_t *output,
                                           const struct kan_dynamic_array_t *base,
                                           const struct kan_dynamic_array_t *overlay,
                                           const char *instance_name)
{
    kan_dynamic_array_set_capacity (output, base->size + overlay->size);
    output->size = base->size;

    if (base->size > 0u)
    {
        memcpy (output->data, base->data, sizeof (struct kan_resource_material_parameter_t) * base->size);
    }

    return merge_parameters_into (output, overlay, instance_name);
}

static enum kan_resource_compile_result_t kan_resource_material_instance_compile (
    struct kan_resource_compile_state_t *state)
{
    const struct kan_resource_material_instance_t *input = state->input_instance;
    struct kan_resource_material_instance_compiled_t *output = state->output_instance;
    const struct kan_resource_material_instance_compiled_t *parent = NULL;

    if (state->dependencies_count > 0u)
    {
        KAN_ASSERT (state->dependencies_count == 1u)
        KAN_ASSERT (state->dependencies[0u].type == kan_string_intern ("kan_resource_material_instance_compiled_t"))
        parent = state->dependencies[0u].data;
    }

    if (input->parameters.size > 0u || input->tail_set.size > 0u || input->tail_append.size > 0u ||
        input->images.size > 0u || !parent)
    {
        // Instance has non-instanced parameters, therefore it needs new static data.
        struct kan_resource_material_instance_static_t static_byproduct;
        static_byproduct.base_static = parent ? parent->static_data : NULL;
        static_byproduct.append_raw_instance = state->name;

        output->static_data = state->register_unique_byproduct (
            state->interface_user_data, kan_string_intern ("kan_resource_material_instance_static_t"), state->name,
            &static_byproduct);

        if (!output->static_data)
        {
            KAN_LOG (resource_material_instance_compilation, KAN_LOG_ERROR,
                     "Material instance \"%s\" failed to register static data byproduct.", state->name)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }
    }
    else if (parent)
    {
        output->static_data = parent->static_data;
    }

    if (parent)
    {
        if (!merge_parameter_arrays (&output->instanced_parameters, &parent->instanced_parameters,
                                     &input->instanced_parameters, state->name))
        {
            KAN_LOG (resource_material_instance_compilation, KAN_LOG_ERROR,
                     "Material instance \"%s\" failed to merge instanced parameter arrays.", state->name)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }
    }
    else if (input->instanced_parameters.size > 0u)
    {
        kan_dynamic_array_set_capacity (&output->instanced_parameters, input->instanced_parameters.size);
        output->instanced_parameters.size = output->instanced_parameters.capacity;
        memcpy (output->instanced_parameters.data, input->instanced_parameters.data,
                sizeof (struct kan_resource_material_parameter_t) * input->instanced_parameters.size);
    }

    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

static void append_tail_set_copy (struct kan_dynamic_array_t *output,
                                  const struct kan_resource_material_tail_set_t *source)
{
    struct kan_resource_material_tail_set_t *target = kan_dynamic_array_add_last (output);
    KAN_ASSERT (target)
    kan_resource_material_tail_set_init (target);
    target->tail_name = source->tail_name;
    target->index = source->index;

    kan_dynamic_array_set_capacity (&target->parameters, source->parameters.size);
    target->parameters.size = target->parameters.capacity;
    memcpy (target->parameters.data, source->parameters.data,
            sizeof (struct kan_resource_material_parameter_t) * source->parameters.size);
}

static void append_tail_append_copy (struct kan_dynamic_array_t *output,
                                     const struct kan_resource_material_tail_append_t *source)
{
    struct kan_resource_material_tail_append_t *target = kan_dynamic_array_add_last (output);
    KAN_ASSERT (target)
    kan_resource_material_tail_append_init (target);
    target->tail_name = source->tail_name;

    kan_dynamic_array_set_capacity (&target->parameters, source->parameters.size);
    target->parameters.size = target->parameters.capacity;
    memcpy (target->parameters.data, source->parameters.data,
            sizeof (struct kan_resource_material_parameter_t) * source->parameters.size);
}

static enum kan_resource_compile_result_t kan_resource_material_instance_static_compile (
    struct kan_resource_compile_state_t *state)
{
    const struct kan_resource_material_instance_static_t *input = state->input_instance;
    struct kan_resource_material_instance_static_compiled_t *output = state->output_instance;
    const struct kan_resource_material_instance_static_compiled_t *base = NULL;
    const struct kan_resource_material_instance_t *append_raw = NULL;

    for (kan_loop_size_t index = 0u; index < state->dependencies_count; ++index)
    {
        if (state->dependencies[index].name == input->base_static)
        {
            KAN_ASSERT (state->dependencies[index].type ==
                        kan_string_intern ("kan_resource_material_instance_static_compiled_t"))
            base = state->dependencies[index].data;
        }
        else if (state->dependencies[index].name == input->append_raw_instance)
        {
            KAN_ASSERT (state->dependencies[index].type == kan_string_intern ("kan_resource_material_instance_t"))
            append_raw = state->dependencies[index].data;
        }
        else
        {
            KAN_ASSERT (false)
        }
    }

    output->material = append_raw->material;
    if (base && base->material != append_raw->material)
    {
        KAN_LOG (resource_material_instance_compilation, KAN_LOG_ERROR,
                 "Material instance \"%s\" uses material \"%s\" while its parent uses different material \"%s\".",
                 input->append_raw_instance, append_raw->material, base->material)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    KAN_ASSERT (append_raw)
    if (base)
    {
        if (!merge_parameter_arrays (&output->parameters, &base->parameters, &append_raw->parameters, state->name))
        {
            KAN_LOG (resource_material_instance_compilation, KAN_LOG_ERROR,
                     "Material instance \"%s\" failed to merge parameter arrays.", state->name)
            return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
        }
    }
    else if (append_raw->parameters.size > 0u)
    {
        kan_dynamic_array_set_capacity (&output->parameters, append_raw->parameters.size);
        output->parameters.size = output->parameters.capacity;
        memcpy (output->parameters.data, append_raw->parameters.data,
                sizeof (struct kan_resource_material_parameter_t) * append_raw->parameters.size);
    }

    kan_dynamic_array_set_capacity (&output->tail_set, (base ? base->tail_set.size : 0u) + append_raw->tail_set.size);
    if (base)
    {
        for (kan_loop_size_t index = 0; index < base->tail_set.size; ++index)
        {
            append_tail_set_copy (&output->tail_set,
                                  &((struct kan_resource_material_tail_set_t *) base->tail_set.data)[index]);
        }
    }

    for (kan_loop_size_t index = 0; index < append_raw->tail_set.size; ++index)
    {
        struct kan_resource_material_tail_set_t *append_set =
            &((struct kan_resource_material_tail_set_t *) append_raw->tail_set.data)[index];
        bool overridden = false;

        for (kan_loop_size_t scan_index = 0u; scan_index < output->tail_set.size; ++scan_index)
        {
            struct kan_resource_material_tail_set_t *output_set =
                &((struct kan_resource_material_tail_set_t *) output->tail_set.data)[scan_index];

            if (output_set->tail_name == append_set->tail_name && output_set->index == append_set->index)
            {
                if (!merge_parameters_into (&output_set->parameters, &append_set->parameters, state->name))
                {
                    KAN_LOG (
                        resource_material_instance_compilation, KAN_LOG_ERROR,
                        "Material instance \"%s\" failed to merge tail parameter arrays of tail \"%s\" at index %lu.",
                        state->name, append_set->tail_name, (unsigned long) append_set->index)
                    return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
                }

                overridden = true;
                break;
            }
        }

        if (!overridden)
        {
            append_tail_set_copy (&output->tail_set, append_set);
        }
    }

    kan_dynamic_array_set_capacity (&output->tail_append,
                                    (base ? base->tail_append.size : 0u) + append_raw->tail_append.size);

    if (base)
    {
        for (kan_loop_size_t index = 0; index < base->tail_append.size; ++index)
        {
            append_tail_append_copy (&output->tail_append,
                                     &((struct kan_resource_material_tail_append_t *) base->tail_append.data)[index]);
        }
    }

    for (kan_loop_size_t index = 0; index < append_raw->tail_append.size; ++index)
    {
        append_tail_append_copy (&output->tail_append,
                                 &((struct kan_resource_material_tail_append_t *) append_raw->tail_append.data)[index]);
    }

    kan_dynamic_array_set_capacity (&output->samplers, (base ? base->samplers.size : 0u) + append_raw->samplers.size);
    if (base && base->samplers.size > 0u)
    {
        output->samplers.size = base->samplers.size;
        memcpy (output->samplers.data, base->samplers.data,
                sizeof (struct kan_resource_material_sampler_t) * base->samplers.size);
    }

    for (kan_loop_size_t append_index = 0u; append_index < append_raw->samplers.size; ++append_index)
    {
        bool overridden = false;
        const struct kan_resource_material_sampler_t *to_append =
            &((struct kan_resource_material_sampler_t *) append_raw->samplers.data)[append_index];

        for (kan_loop_size_t existent_index = 0u; existent_index < output->samplers.size; ++existent_index)
        {
            struct kan_resource_material_sampler_t *existent =
                &((struct kan_resource_material_sampler_t *) output->samplers.data)[existent_index];

            if (to_append->name == existent->name)
            {
                existent->sampler = to_append->sampler;
                overridden = true;
                break;
            }
        }

        if (!overridden)
        {
            struct kan_resource_material_sampler_t *spot = kan_dynamic_array_add_last (&output->samplers);
            KAN_ASSERT (spot);
            *spot = *to_append;
        }
    }

    kan_dynamic_array_set_capacity (&output->images, (base ? base->images.size : 0u) + append_raw->images.size);
    if (base && base->images.size > 0u)
    {
        output->images.size = base->images.size;
        memcpy (output->images.data, base->images.data,
                sizeof (struct kan_resource_material_image_t) * base->images.size);
    }

    for (kan_loop_size_t append_index = 0u; append_index < append_raw->images.size; ++append_index)
    {
        bool overridden = false;
        const struct kan_resource_material_image_t *to_append =
            &((struct kan_resource_material_image_t *) append_raw->images.data)[append_index];

        for (kan_loop_size_t existent_index = 0u; existent_index < output->images.size; ++existent_index)
        {
            struct kan_resource_material_image_t *existent =
                &((struct kan_resource_material_image_t *) output->images.data)[existent_index];

            if (to_append->name == existent->name)
            {
                existent->texture = to_append->texture;
                overridden = true;
                break;
            }
        }

        if (!overridden)
        {
            struct kan_resource_material_image_t *spot = kan_dynamic_array_add_last (&output->images);
            KAN_ASSERT (spot);
            *spot = *to_append;
        }
    }

    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

void kan_resource_material_tail_set_init (struct kan_resource_material_tail_set_t *instance)
{
    instance->tail_name = NULL;
    instance->index = 0u;
    kan_dynamic_array_init (&instance->parameters, 0u, sizeof (struct kan_resource_material_parameter_t),
                            _Alignof (struct kan_resource_material_parameter_t), kan_allocation_group_stack_get ());
}

void kan_resource_material_tail_set_shutdown (struct kan_resource_material_tail_set_t *instance)
{
    kan_dynamic_array_shutdown (&instance->parameters);
}

void kan_resource_material_tail_append_init (struct kan_resource_material_tail_append_t *instance)
{
    instance->tail_name = NULL;
    kan_dynamic_array_init (&instance->parameters, 0u, sizeof (struct kan_resource_material_parameter_t),
                            _Alignof (struct kan_resource_material_parameter_t), kan_allocation_group_stack_get ());
}

void kan_resource_material_tail_append_shutdown (struct kan_resource_material_tail_append_t *instance)
{
    kan_dynamic_array_shutdown (&instance->parameters);
}

void kan_resource_material_sampler_init (struct kan_resource_material_sampler_t *instance)
{
    instance->name = NULL;
    instance->sampler.min_filter = KAN_RENDER_FILTER_MODE_NEAREST;
    instance->sampler.mag_filter = KAN_RENDER_FILTER_MODE_NEAREST;
    instance->sampler.mip_map_mode = KAN_RENDER_MIP_MAP_MODE_NEAREST;
    instance->sampler.address_mode_u = KAN_RENDER_ADDRESS_MODE_REPEAT;
    instance->sampler.address_mode_v = KAN_RENDER_ADDRESS_MODE_REPEAT;
    instance->sampler.address_mode_w = KAN_RENDER_ADDRESS_MODE_REPEAT;
    instance->sampler.depth_compare_enabled = false;
    instance->sampler.depth_compare = KAN_RENDER_COMPARE_OPERATION_NEVER;
    instance->sampler.anisotropy_enabled = false;
    instance->sampler.anisotropy_max = 1.0f;
}

void kan_resource_material_image_init (struct kan_resource_material_image_t *instance)
{
    instance->name = NULL;
    instance->texture = NULL;
}

void kan_resource_material_instance_init (struct kan_resource_material_instance_t *instance)
{
    instance->material = NULL;
    instance->parent = NULL;
    kan_dynamic_array_init (&instance->instanced_parameters, 0u, sizeof (struct kan_resource_material_parameter_t),
                            _Alignof (struct kan_resource_material_parameter_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->parameters, 0u, sizeof (struct kan_resource_material_parameter_t),
                            _Alignof (struct kan_resource_material_parameter_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->tail_set, 0u, sizeof (struct kan_resource_material_tail_set_t),
                            _Alignof (struct kan_resource_material_tail_set_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->tail_append, 0u, sizeof (struct kan_resource_material_tail_append_t),
                            _Alignof (struct kan_resource_material_tail_append_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->samplers, 0u, sizeof (struct kan_resource_material_sampler_t),
                            _Alignof (struct kan_resource_material_sampler_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->images, 0u, sizeof (struct kan_resource_material_image_t),
                            _Alignof (struct kan_resource_material_image_t), kan_allocation_group_stack_get ());
}

void kan_resource_material_instance_shutdown (struct kan_resource_material_instance_t *instance)
{
    for (kan_loop_size_t index = 0u; index < instance->tail_set.size; ++index)
    {
        kan_resource_material_tail_set_shutdown (
            &((struct kan_resource_material_tail_set_t *) instance->tail_set.data)[index]);
    }

    for (kan_loop_size_t index = 0u; index < instance->tail_append.size; ++index)
    {
        kan_resource_material_tail_append_shutdown (
            &((struct kan_resource_material_tail_append_t *) instance->tail_append.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->instanced_parameters);
    kan_dynamic_array_shutdown (&instance->parameters);
    kan_dynamic_array_shutdown (&instance->tail_set);
    kan_dynamic_array_shutdown (&instance->tail_append);
    kan_dynamic_array_shutdown (&instance->samplers);
    kan_dynamic_array_shutdown (&instance->images);
}

void kan_resource_material_instance_static_compiled_init (
    struct kan_resource_material_instance_static_compiled_t *instance)
{
    instance->material = NULL;
    kan_dynamic_array_init (&instance->parameters, 0u, sizeof (struct kan_resource_material_parameter_t),
                            _Alignof (struct kan_resource_material_parameter_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->tail_set, 0u, sizeof (struct kan_resource_material_tail_set_t),
                            _Alignof (struct kan_resource_material_tail_set_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->tail_append, 0u, sizeof (struct kan_resource_material_tail_append_t),
                            _Alignof (struct kan_resource_material_tail_append_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->samplers, 0u, sizeof (struct kan_resource_material_sampler_t),
                            _Alignof (struct kan_resource_material_sampler_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->images, 0u, sizeof (struct kan_resource_material_image_t),
                            _Alignof (struct kan_resource_material_image_t), kan_allocation_group_stack_get ());
}

void kan_resource_material_instance_static_compiled_shutdown (
    struct kan_resource_material_instance_static_compiled_t *instance)
{
    for (kan_loop_size_t index = 0u; index < instance->tail_set.size; ++index)
    {
        kan_resource_material_tail_set_shutdown (
            &((struct kan_resource_material_tail_set_t *) instance->tail_set.data)[index]);
    }

    for (kan_loop_size_t index = 0u; index < instance->tail_append.size; ++index)
    {
        kan_resource_material_tail_append_shutdown (
            &((struct kan_resource_material_tail_append_t *) instance->tail_append.data)[index]);
    }

    kan_dynamic_array_shutdown (&instance->parameters);
    kan_dynamic_array_shutdown (&instance->tail_set);
    kan_dynamic_array_shutdown (&instance->tail_append);
    kan_dynamic_array_shutdown (&instance->samplers);
    kan_dynamic_array_shutdown (&instance->images);
}

void kan_resource_material_instance_compiled_init (struct kan_resource_material_instance_compiled_t *instance)
{
    instance->static_data = NULL;
    kan_dynamic_array_init (&instance->instanced_parameters, 0u, sizeof (struct kan_resource_material_parameter_t),
                            _Alignof (struct kan_resource_material_parameter_t), kan_allocation_group_stack_get ());
}

void kan_resource_material_instance_compiled_shutdown (struct kan_resource_material_instance_compiled_t *instance)
{
    kan_dynamic_array_shutdown (&instance->instanced_parameters);
}
