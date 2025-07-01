#include <string.h>

#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/resource_material/resource_rpl_source.h>
#include <kan/resource_pipeline/resource_pipeline.h>

KAN_LOG_DEFINE_CATEGORY (resource_rpl_compilation);

KAN_REFLECTION_STRUCT_META (kan_resource_rpl_source_t)
RESOURCE_MATERIAL_API struct kan_resource_byproduct_type_meta_t kan_resource_rpl_source_byproduct_type_meta = {
    .hash = NULL,
    .is_equal = NULL,
    .move = NULL,
    .reset = NULL,
};

static enum kan_resource_compile_result_t kan_resource_rpl_source_compile (struct kan_resource_compile_state_t *state);

KAN_REFLECTION_STRUCT_META (kan_resource_rpl_source_t)
RESOURCE_MATERIAL_API struct kan_resource_compilable_meta_t kan_resource_rpl_source_compilable_meta = {
    .output_type_name = "kan_resource_rpl_source_compiled_t",
    .configuration_type_name = NULL,
    // No state as render pipeline language does not support step by step parsing right now.
    .state_type_name = NULL,
    .functor = kan_resource_rpl_source_compile,
};

KAN_REFLECTION_STRUCT_FIELD_META (kan_resource_rpl_source_t, source)
RESOURCE_MATERIAL_API struct kan_resource_reference_meta_t kan_resource_rpl_source_source_reference_meta = {
    .type = NULL, // Null means third party.
    .compilation_usage = KAN_RESOURCE_REFERENCE_COMPILATION_USAGE_TYPE_NEEDED_RAW,
};

KAN_REFLECTION_STRUCT_META (kan_resource_rpl_source_compiled_t)
RESOURCE_MATERIAL_API struct kan_resource_resource_type_meta_t kan_resource_rpl_source_compiled_resource_type_meta = {
    .root = false,
};

static enum kan_resource_compile_result_t kan_resource_rpl_source_compile (struct kan_resource_compile_state_t *state)
{
    const struct kan_resource_rpl_source_t *input = state->input_instance;
    struct kan_resource_rpl_source_compiled_t *output = state->output_instance;

    KAN_ASSERT (state->dependencies_count == 1u)
    KAN_ASSERT (state->dependencies->type == NULL)

    if (state->dependencies->data_size_if_third_party == 0u)
    {
        KAN_LOG (resource_rpl_compilation, KAN_LOG_ERROR,
                 "Failed to compile \"%s\" as source \"%s\" loaded data is empty.", state->name,
                 state->dependencies->name)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    kan_allocation_group_t temporary_allocation_group =
        kan_allocation_group_get_child (kan_allocation_group_root (), "material_rpl_source_compilation");

    // Unfortunately, due to how underlying parser is implemented (and how re2c parses are implemented in general),
    // it is better for performance to process input with sentinel characters (for example, nulls in null-terminated
    // strings). Third party data has no sentinel character in the end, as it is just an array of bytes.
    // Therefore, we just copy it and add null terminator -- it should still be faster than checking the limit through
    // custom API in re2c in parser implementation.

    char *string_to_parse = kan_allocate_general (temporary_allocation_group,
                                                  state->dependencies->data_size_if_third_party + 1u, alignof (char));
    memcpy (string_to_parse, state->dependencies->data, state->dependencies->data_size_if_third_party);
    string_to_parse[state->dependencies->data_size_if_third_party] = '\0';

    kan_rpl_parser_t parser = kan_rpl_parser_create (state->name);
    if (!kan_rpl_parser_add_source (parser, string_to_parse, input->source))
    {
        kan_free_general (temporary_allocation_group, string_to_parse,
                          state->dependencies->data_size_if_third_party + 1u);
        kan_rpl_parser_destroy (parser);

        KAN_LOG (resource_rpl_compilation, KAN_LOG_ERROR,
                 "Failed to compile \"%s\" as source \"%s\" cannot be properly parsed.", state->name,
                 state->dependencies->name)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    kan_free_general (temporary_allocation_group, string_to_parse, state->dependencies->data_size_if_third_party + 1u);
    if (!kan_rpl_parser_build_intermediate (parser, &output->intermediate))
    {
        kan_rpl_parser_destroy (parser);
        KAN_LOG (resource_rpl_compilation, KAN_LOG_ERROR,
                 "Failed to compile \"%s\" as source \"%s\" export to intermediate format failed.", state->name,
                 state->dependencies->name)
        return KAN_RESOURCE_PIPELINE_COMPILE_FAILED;
    }

    kan_rpl_parser_destroy (parser);
    return KAN_RESOURCE_PIPELINE_COMPILE_FINISHED;
}

void kan_resource_rpl_source_compiled_init (struct kan_resource_rpl_source_compiled_t *instance)
{
    kan_rpl_intermediate_init (&instance->intermediate);
}

void kan_resource_rpl_source_compiled_shutdown (struct kan_resource_rpl_source_compiled_t *instance)
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

void kan_resource_rpl_options_shutdown (struct kan_resource_rpl_options_t *instance)
{
    kan_dynamic_array_shutdown (&instance->flags);
    kan_dynamic_array_shutdown (&instance->uints);
    kan_dynamic_array_shutdown (&instance->sints);
    kan_dynamic_array_shutdown (&instance->floats);
    kan_dynamic_array_shutdown (&instance->enums);
}
