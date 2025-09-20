#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <string.h>

#include <kan/file_system/stream.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/render_foundation/resource_render_pass_build.h>
#include <kan/render_foundation/resource_rpl_build.h>
#include <kan/resource_pipeline/meta.h>
#include <kan/stream/random_access_stream_buffer.h>

KAN_LOG_DEFINE_CATEGORY (resource_render_foundation_rpl);

void kan_resource_render_code_platform_configuration_init (
    struct kan_resource_render_code_platform_configuration_t *instance)
{
    instance->code_format = KAN_RENDER_CODE_FORMAT_SPIRV;
    kan_dynamic_array_init (&instance->supported_pass_tags, 0u, sizeof (kan_interned_string_t),
                            alignof (kan_interned_string_t), kan_allocation_group_stack_get ());
}

bool kan_resource_render_code_platform_configuration_is_pass_supported (
    const struct kan_resource_render_code_platform_configuration_t *instance,
    const struct kan_resource_render_pass_header_t *pass)
{
    for (kan_loop_size_t required_index = 0u; required_index < pass->required_tags.size; ++required_index)
    {
        bool is_supported = false;
        kan_interned_string_t required = ((kan_interned_string_t *) pass->required_tags.data)[required_index];

        for (kan_loop_size_t supported_index = 0u; supported_index < instance->supported_pass_tags.size;
             ++supported_index)
        {
            kan_interned_string_t supported =
                ((kan_interned_string_t *) instance->supported_pass_tags.data)[supported_index];

            if (supported == required)
            {
                is_supported = true;
                break;
            }
        }

        if (!is_supported)
        {
            return false;
        }
    }

    return true;
}

void kan_resource_render_code_platform_configuration_shutdown (
    struct kan_resource_render_code_platform_configuration_t *instance)
{
    kan_dynamic_array_shutdown (&instance->supported_pass_tags);
}

KAN_REFLECTION_STRUCT_META (kan_resource_rpl_source_t)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_type_meta_t kan_resource_rpl_source_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

void kan_resource_rpl_source_init (struct kan_resource_rpl_source_t *instance)
{
    kan_rpl_intermediate_init (&instance->intermediate);
}

void kan_resource_rpl_source_shutdown (struct kan_resource_rpl_source_t *instance)
{
    kan_rpl_intermediate_shutdown (&instance->intermediate);
}

void kan_resource_rpl_options_init (struct kan_resource_rpl_options_t *instance)
{
    kan_dynamic_array_init (&instance->flags, 0u, sizeof (struct kan_resource_rpl_flag_option_t),
                            alignof (struct kan_resource_rpl_flag_option_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->uints, 0u, sizeof (struct kan_resource_rpl_uint_option_t),
                            alignof (struct kan_resource_rpl_uint_option_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->sints, 0u, sizeof (struct kan_resource_rpl_sint_option_t),
                            alignof (struct kan_resource_rpl_sint_option_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->floats, 0u, sizeof (struct kan_resource_rpl_float_option_t),
                            alignof (struct kan_resource_rpl_float_option_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->enums, 0u, sizeof (struct kan_resource_rpl_enum_option_t),
                            alignof (struct kan_resource_rpl_enum_option_t), kan_allocation_group_stack_get ());
}

void kan_resource_rpl_options_init_copy (struct kan_resource_rpl_options_t *instance,
                                         const struct kan_resource_rpl_options_t *copy_from)
{
    kan_dynamic_array_init (&instance->flags, copy_from->flags.size, sizeof (struct kan_resource_rpl_flag_option_t),
                            alignof (struct kan_resource_rpl_flag_option_t), kan_allocation_group_stack_get ());
    instance->flags.size = copy_from->flags.size;
    memcpy (instance->flags.data, copy_from->flags.data,
            sizeof (struct kan_resource_rpl_flag_option_t) * copy_from->flags.size);

    kan_dynamic_array_init (&instance->uints, copy_from->uints.size, sizeof (struct kan_resource_rpl_uint_option_t),
                            alignof (struct kan_resource_rpl_uint_option_t), kan_allocation_group_stack_get ());
    instance->uints.size = copy_from->uints.size;
    memcpy (instance->uints.data, copy_from->uints.data,
            sizeof (struct kan_resource_rpl_uint_option_t) * copy_from->uints.size);

    kan_dynamic_array_init (&instance->sints, copy_from->sints.size, sizeof (struct kan_resource_rpl_sint_option_t),
                            alignof (struct kan_resource_rpl_sint_option_t), kan_allocation_group_stack_get ());
    instance->sints.size = copy_from->sints.size;
    memcpy (instance->sints.data, copy_from->sints.data,
            sizeof (struct kan_resource_rpl_sint_option_t) * copy_from->sints.size);

    kan_dynamic_array_init (&instance->floats, copy_from->floats.size, sizeof (struct kan_resource_rpl_float_option_t),
                            alignof (struct kan_resource_rpl_float_option_t), kan_allocation_group_stack_get ());
    instance->floats.size = copy_from->floats.size;
    memcpy (instance->floats.data, copy_from->floats.data,
            sizeof (struct kan_resource_rpl_float_option_t) * copy_from->floats.size);

    kan_dynamic_array_init (&instance->enums, copy_from->enums.size, sizeof (struct kan_resource_rpl_enum_option_t),
                            alignof (struct kan_resource_rpl_enum_option_t), kan_allocation_group_stack_get ());
    instance->enums.size = copy_from->enums.size;
    memcpy (instance->enums.data, copy_from->enums.data,
            sizeof (struct kan_resource_rpl_enum_option_t) * copy_from->enums.size);
}

void kan_resource_rpl_options_append (struct kan_resource_rpl_options_t *instance,
                                      const struct kan_resource_rpl_options_t *override)
{
    kan_dynamic_array_set_capacity (&instance->flags, instance->flags.size + override->flags.size);
    kan_dynamic_array_set_capacity (&instance->uints, instance->uints.size + override->uints.size);
    kan_dynamic_array_set_capacity (&instance->sints, instance->sints.size + override->sints.size);
    kan_dynamic_array_set_capacity (&instance->floats, instance->floats.size + override->floats.size);
    kan_dynamic_array_set_capacity (&instance->enums, instance->enums.size + override->enums.size);

#define APPEND_OPTIONS_OF_TYPE(TYPE)                                                                                   \
    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) override->TYPE##s.size; ++index)                        \
    {                                                                                                                  \
        const struct kan_resource_rpl_##TYPE##_option_t *input =                                                       \
            &((struct kan_resource_rpl_##TYPE##_option_t *) override->TYPE##s.data)[index];                            \
        struct kan_resource_rpl_##TYPE##_option_t *output = NULL;                                                      \
                                                                                                                       \
        for (kan_loop_size_t target_index = 0u; target_index < (kan_loop_size_t) instance->TYPE##s.size;               \
             ++target_index)                                                                                           \
        {                                                                                                              \
            struct kan_resource_rpl_##TYPE##_option_t *other =                                                         \
                &((struct kan_resource_rpl_##TYPE##_option_t *) instance->TYPE##s.data)[target_index];                 \
                                                                                                                       \
            if (other->name == input->name)                                                                            \
            {                                                                                                          \
                output = other;                                                                                        \
                break;                                                                                                 \
            }                                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        if (output)                                                                                                    \
        {                                                                                                              \
            output->value = input->value;                                                                              \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            output = kan_dynamic_array_add_last (&instance->TYPE##s);                                                  \
            KAN_ASSERT (output)                                                                                        \
            output->name = input->name;                                                                                \
            output->value = input->value;                                                                              \
        }                                                                                                              \
    }

    APPEND_OPTIONS_OF_TYPE (flag)
    APPEND_OPTIONS_OF_TYPE (uint)
    APPEND_OPTIONS_OF_TYPE (sint)
    APPEND_OPTIONS_OF_TYPE (float)
    APPEND_OPTIONS_OF_TYPE (enum)
#undef APPEND_OPTIONS_OF_TYPE
}

bool kan_resource_rpl_options_apply (const struct kan_resource_rpl_options_t *options,
                                     kan_rpl_compiler_context_t compiler_context,
                                     enum kan_rpl_option_target_scope_t target_scope)
{
    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) options->flags.size; ++index)
    {
        struct kan_resource_rpl_flag_option_t *option =
            &((struct kan_resource_rpl_flag_option_t *) options->flags.data)[index];

        if (!kan_rpl_compiler_context_set_option_flag (compiler_context, target_scope, option->name, option->value))
        {
            return false;
        }
    }

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) options->uints.size; ++index)
    {
        struct kan_resource_rpl_uint_option_t *option =
            &((struct kan_resource_rpl_uint_option_t *) options->uints.data)[index];

        if (!kan_rpl_compiler_context_set_option_uint (compiler_context, target_scope, option->name, option->value))
        {
            return false;
        }
    }

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) options->sints.size; ++index)
    {
        struct kan_resource_rpl_sint_option_t *option =
            &((struct kan_resource_rpl_sint_option_t *) options->sints.data)[index];

        if (!kan_rpl_compiler_context_set_option_sint (compiler_context, target_scope, option->name, option->value))
        {
            return false;
        }
    }

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) options->floats.size; ++index)
    {
        struct kan_resource_rpl_float_option_t *option =
            &((struct kan_resource_rpl_float_option_t *) options->floats.data)[index];

        if (!kan_rpl_compiler_context_set_option_float (compiler_context, target_scope, option->name, option->value))
        {
            return false;
        }
    }

    for (kan_loop_size_t index = 0u; index < (kan_loop_size_t) options->enums.size; ++index)
    {
        struct kan_resource_rpl_enum_option_t *option =
            &((struct kan_resource_rpl_enum_option_t *) options->enums.data)[index];

        if (!kan_rpl_compiler_context_set_option_enum (compiler_context, target_scope, option->name, option->value))
        {
            return false;
        }
    }

    return true;
}

void kan_resource_rpl_options_shutdown (struct kan_resource_rpl_options_t *instance)
{
    kan_dynamic_array_shutdown (&instance->flags);
    kan_dynamic_array_shutdown (&instance->uints);
    kan_dynamic_array_shutdown (&instance->sints);
    kan_dynamic_array_shutdown (&instance->floats);
    kan_dynamic_array_shutdown (&instance->enums);
}

KAN_REFLECTION_STRUCT_META (kan_resource_rpl_pipeline_header_t)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_type_meta_t kan_resource_rpl_pipeline_header_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_rpl_pipeline_header_t, sources)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_reference_meta_t
    kan_resource_rpl_pipeline_header_reference_sources = {
        .type_name = "kan_resource_rpl_source_t",
        .flags = 0u,
};

void kan_resource_rpl_pipeline_header_init (struct kan_resource_rpl_pipeline_header_t *instance)
{
    instance->type = KAN_RPL_PIPELINE_TYPE_GRAPHICS_CLASSIC;
    kan_dynamic_array_init (&instance->entry_points, 0u, sizeof (struct kan_rpl_entry_point_t),
                            alignof (struct kan_rpl_entry_point_t), kan_allocation_group_stack_get ());
    kan_dynamic_array_init (&instance->sources, 0u, sizeof (kan_interned_string_t), alignof (kan_interned_string_t),
                            kan_allocation_group_stack_get ());
    kan_resource_rpl_options_init (&instance->global_options);
    kan_resource_rpl_options_init (&instance->instance_options);
}

void kan_resource_rpl_pipeline_header_shutdown (struct kan_resource_rpl_pipeline_header_t *instance)
{
    kan_dynamic_array_shutdown (&instance->entry_points);
    kan_dynamic_array_shutdown (&instance->sources);
    kan_resource_rpl_options_shutdown (&instance->global_options);
    kan_resource_rpl_options_shutdown (&instance->instance_options);
}

KAN_REFLECTION_STRUCT_META (kan_resource_rpl_pipeline_t)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_type_meta_t kan_resource_rpl_pipeline_resource_type = {
    .flags = 0u,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

void kan_resource_rpl_pipeline_init (struct kan_resource_rpl_pipeline_t *instance)
{
    kan_dynamic_array_init (&instance->entry_points, 0u, sizeof (struct kan_rpl_entry_point_t),
                            alignof (struct kan_rpl_entry_point_t), kan_allocation_group_stack_get ());
    kan_rpl_meta_init (&instance->meta);
    kan_dynamic_array_init (&instance->code, 0u, sizeof (uint8_t), alignof (uint32_t),
                            kan_allocation_group_stack_get ());
}

void kan_resource_rpl_pipeline_shutdown (struct kan_resource_rpl_pipeline_t *instance)
{
    kan_dynamic_array_shutdown (&instance->entry_points);
    kan_rpl_meta_shutdown (&instance->meta);
    kan_dynamic_array_shutdown (&instance->code);
}

static enum kan_resource_build_rule_result_t rpl_source_build (struct kan_resource_build_rule_context_t *context);

KAN_REFLECTION_STRUCT_META (kan_resource_rpl_source_t)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_build_rule_t kan_resource_rpl_source_build_rule = {
    .primary_input_type = NULL,
    .platform_configuration_type = NULL,
    .secondary_types_count = 0u,
    .secondary_types = NULL,
    .functor = rpl_source_build,
    .version = CUSHION_START_NS_X64,
};

static enum kan_resource_build_rule_result_t rpl_source_build (struct kan_resource_build_rule_context_t *context)
{
    const char *input_path = context->primary_third_party_path;
    struct kan_resource_rpl_source_t *output = context->primary_output;
    struct kan_stream_t *input_stream = kan_direct_file_stream_open_for_read (input_path, true);

    if (!input_stream)
    {
        KAN_LOG (resource_render_foundation_rpl, KAN_LOG_ERROR,
                 "Failed to open file \"%s\" for read in order to produce rpl source \"%s\".", input_path,
                 context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    CUSHION_DEFER { input_stream->operations->close (input_stream); }

    // Currently, RPL parser expects full strings, not streams, therefore we read file as a whole first.

    kan_file_size_t file_size = 0u;
    const bool read_file_size = input_stream->operations->seek (input_stream, KAN_STREAM_SEEK_END, 0) &&
                                (file_size = input_stream->operations->tell (input_stream),
                                 input_stream->operations->seek (input_stream, KAN_STREAM_SEEK_START, 0));

    if (!read_file_size)
    {
        KAN_LOG (resource_render_foundation_rpl, KAN_LOG_ERROR,
                 "Failed to get size of file \"%s\" in order to produce rpl source \"%s\".", input_path,
                 context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    kan_allocation_group_t temporary_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "resource_render_foundation_rpl_parse");

    char *file_data = kan_allocate_general (temporary_allocation_group, file_size + 1u, alignof (char));
    CUSHION_DEFER { kan_free_general (temporary_allocation_group, file_data, file_size + 1u); }

    if (input_stream->operations->read (input_stream, file_size, file_data) != file_size)
    {
        KAN_LOG (resource_render_foundation_rpl, KAN_LOG_ERROR,
                 "Failed to read content from file \"%s\" in order to produce rpl source \"%s\".", input_path,
                 context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    file_data[file_size] = '\0';
    kan_rpl_parser_t parser = kan_rpl_parser_create (context->primary_name);
    CUSHION_DEFER { kan_rpl_parser_destroy (parser); }

    if (!kan_rpl_parser_add_source (parser, file_data, context->primary_name))
    {
        KAN_LOG (resource_render_foundation_rpl, KAN_LOG_ERROR,
                 "Failed to parse rpl code from file \"%s\" in order to produce rpl source \"%s\".", input_path,
                 context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    if (!kan_rpl_parser_build_intermediate (parser, &output->intermediate))
    {
        KAN_LOG (resource_render_foundation_rpl, KAN_LOG_ERROR,
                 "Failed to produce intermediate data for rpl source \"%s\".", input_path, context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    return KAN_RESOURCE_BUILD_RULE_SUCCESS;
}

static enum kan_resource_build_rule_result_t rpl_pipeline_build (struct kan_resource_build_rule_context_t *context);

KAN_REFLECTION_STRUCT_META (kan_resource_rpl_pipeline_t)
RESOURCE_RENDER_FOUNDATION_BUILD_API struct kan_resource_build_rule_t kan_resource_rpl_pipeline_build_rule = {
    .primary_input_type = "kan_resource_rpl_pipeline_header_t",
    .platform_configuration_type = "kan_resource_render_code_platform_configuration_t",
    .secondary_types_count = 1u,
    .secondary_types = (const char *[]) {"kan_resource_rpl_source_t"},
    .functor = rpl_pipeline_build,
    .version = CUSHION_START_NS_X64,
};

static enum kan_resource_build_rule_result_t rpl_pipeline_build (struct kan_resource_build_rule_context_t *context)
{
    const struct kan_resource_rpl_pipeline_header_t *input = context->primary_input;
    struct kan_resource_rpl_pipeline_t *output = context->primary_output;
    const struct kan_resource_render_code_platform_configuration_t *configuration = context->platform_configuration;

    kan_dynamic_array_set_capacity (&output->entry_points, input->entry_points.size);
    output->entry_points.size = output->entry_points.capacity;
    memcpy (output->entry_points.data, input->entry_points.data,
            sizeof (struct kan_rpl_entry_point_t) * input->entry_points.size);

    kan_rpl_compiler_context_t compiler_context = kan_rpl_compiler_context_create (input->type, context->primary_name);
    CUSHION_DEFER { kan_rpl_compiler_context_destroy (compiler_context); }

    struct kan_resource_build_rule_secondary_node_t *secondary = context->secondary_input_first;
    bool modules_used = true;

    while (secondary)
    {
        const struct kan_resource_rpl_source_t *source = secondary->data;
        if (!kan_rpl_compiler_context_use_module (compiler_context, &source->intermediate))
        {
            KAN_LOG (resource_render_foundation_rpl, KAN_LOG_ERROR,
                     "Failed to use source \"%s\" while trying to compile pipeline \"%s\".", secondary->name,
                     context->primary_name)
            modules_used = false;
        }

        secondary = secondary->next;
    }

    if (!modules_used)
    {
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    if (!kan_resource_rpl_options_apply (&input->global_options, compiler_context, KAN_RPL_OPTION_TARGET_SCOPE_GLOBAL))
    {
        KAN_LOG (resource_render_foundation_rpl, KAN_LOG_ERROR,
                 "Failed to apply global options while trying to compile pipeline \"%s\".", context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    if (!kan_resource_rpl_options_apply (&input->instance_options, compiler_context,
                                         KAN_RPL_OPTION_TARGET_SCOPE_INSTANCE))
    {
        KAN_LOG (resource_render_foundation_rpl, KAN_LOG_ERROR,
                 "Failed to apply instance options while trying to compile pipeline \"%s\".", context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    kan_rpl_compiler_instance_t compiler_instance = kan_rpl_compiler_context_resolve (
        compiler_context, input->entry_points.size, (struct kan_rpl_entry_point_t *) input->entry_points.data);

    if (!KAN_HANDLE_IS_VALID (compiler_instance))
    {
        KAN_LOG (resource_render_foundation_rpl, KAN_LOG_ERROR,
                 "Failed to resolve compile context while trying to compile pipeline \"%s\".", context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    CUSHION_DEFER { kan_rpl_compiler_instance_destroy (compiler_instance); }
    if (!kan_rpl_compiler_instance_emit_meta (compiler_instance, &output->meta, KAN_RPL_META_EMISSION_FULL))
    {
        KAN_LOG (resource_render_foundation_rpl, KAN_LOG_ERROR,
                 "Failed to emit meta while trying to compile pipeline \"%s\".", context->primary_name)
        return KAN_RESOURCE_BUILD_RULE_FAILURE;
    }

    switch (configuration->code_format)
    {
    case KAN_RENDER_CODE_FORMAT_SPIRV:
    {
        kan_allocation_group_t allocation_group = output->code.allocation_group;
        kan_dynamic_array_shutdown (&output->code);

        if (!kan_rpl_compiler_instance_emit_spirv (compiler_instance, &output->code, allocation_group))
        {
            KAN_LOG (resource_render_foundation_rpl, KAN_LOG_ERROR,
                     "Failed to emit code while trying to compile pipeline \"%s\".", context->primary_name)
            return KAN_RESOURCE_BUILD_RULE_FAILURE;
        }

        break;
    }
    }

    return KAN_RESOURCE_BUILD_RULE_SUCCESS;
}
